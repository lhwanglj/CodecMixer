#include <map>
#include <sched.h>
#include "mqthread.h"
#include "sync.h"
#include "fmtbuffer.h"
#include "flist.h"
#include "stmassembler.h"
#include "stmrecver.h"
#include "fec.h"
#include "logging.h"

using namespace cppcmn;

using namespace MediaCloud::Common;

namespace hpsp {
    
    struct DecodingSegments : ListHead {
        uint32_t identity;
        int assemblerId;
        int stmtype;
        int frmtype;
        uint16_t frmlen;
        uint16_t srcNum;
        uint16_t symSize;
        uint16_t maxESI;    // the max of the next expecting esi
        uint16_t fid;
        ListHead segments;
        void *frmbuffer;    // has padding for fec
    };
    
    class StmRecverThread : public MQThread::IDelegate, StreamRecver::IDelegate {
    public:
        struct IDelegate {
            virtual ~IDelegate() {}
            virtual void HandleFecDecodingRequest(DecodingSegments *segments) = 0;
        };
        
    private:
        struct AssemblerInfo {
            int id;
            StmAssembler::IDelegate *delegate;
            StreamRecver *recver;
        };
        
        struct StreamHeader {
            int assemblerId;
            int length;
            Tick tick;
        };
        
        struct RegisterInfo {
            StmAssembler::IDelegate *delegate;
            int assemberId;
            bool registered;
            volatile bool finished;
        };
        
        struct DecodedResultInfo {
            DecodingSegments *segments;
            bool suc;
        };
        
        struct FrameBufferHeader {
            int size;
            void *pbuffer;
        };
        
        enum {
            CMD_ID_STREAM = 1,
            CMD_ID_REGISTER,
            CMD_ID_DECODED,
        };
        
        IDelegate *_delegate;
        MQThread *_thread;
        std::map<int, AssemblerInfo*> _assemblers;
        
        ListHead _decodingHdr;
        int _decodingNum;
        
        Mutex _stmAllocLock;
        FixedSizeAllocator _stmAllocator;
        
        FixedSizeAllocator _segmentAllocator;
        FixedSizeAllocator _decodingSegmentsAllocator;
        
        MTFixedSizeAllocator _smallFrmBufAllocator;
        MTFixedSizeAllocator _bigFrmBufAllocator;
        MTFixedSizeAllocator _hugeFrmBufAllocator;
        static const int _smallFrmBufSize = 1024;       // for audio
        static const int _bigFrmBufSize = 1024 * 10;    // 10KB, big enough for most of video frame
        static const int _hugeFrmBufSize = 1024 * 50;   // the max frame size
        
        void* AllocateFrameBuffer(int frmlen)
        {
            void *buffer = nullptr;
            int requestSize = frmlen + sizeof(FrameBufferHeader);
            
            if (requestSize <= _smallFrmBufSize) {
                buffer = _smallFrmBufAllocator.Alloc();
            }
            else if (requestSize <= _bigFrmBufSize) {
                buffer = _bigFrmBufAllocator.Alloc();
            }
            else {
                Assert(requestSize <= _hugeFrmBufSize);
                buffer = _hugeFrmBufAllocator.Alloc();
            }
            auto *hdr = reinterpret_cast<FrameBufferHeader*>(buffer);
            hdr->size = frmlen;
            hdr->pbuffer = hdr + 1;
            return hdr + 1;
        }
        
        void ReleaseFrameBuffer(const void *buffer)
        {
            auto *hdr = reinterpret_cast<const FrameBufferHeader*>(buffer) - 1;
            Assert(hdr->pbuffer == buffer);
            
            int requestSize = hdr->size + sizeof(FrameBufferHeader);
            Assert(requestSize <= _hugeFrmBufSize);
            
            if (requestSize <= _smallFrmBufSize) {
                _smallFrmBufAllocator.DecRef((void*)hdr);
            }
            else if (requestSize <= _bigFrmBufSize) {
                _bigFrmBufAllocator.DecRef((void*)hdr);
            }
            else {
                _hugeFrmBufAllocator.DecRef((void*)hdr);
            }
        }
        
    public:
        StmRecverThread(IDelegate *delegate)
            : _delegate(delegate)
            , _thread(nullptr)
            , _decodingNum(0)
            , _stmAllocator(1500 + sizeof(StreamHeader), 50, 0)
            , _segmentAllocator(1500, 50, 0)
            , _decodingSegmentsAllocator(sizeof(DecodingSegments), 50, 0)
            , _smallFrmBufAllocator(_smallFrmBufSize, 50, 0)
            , _bigFrmBufAllocator(_bigFrmBufSize, 50, 0)
            , _hugeFrmBufAllocator(_hugeFrmBufSize, 10, 0)
        {
            ListInitHead(&_decodingHdr);
        }
        
        ~StmRecverThread()
        {
            Assert(false);
        }
        
        void SetThread(MQThread *thread)
        {
            _thread = thread;
        }
        
        void ReleaseFramePayload(const void *payload)
        {
            ReleaseFrameBuffer(payload);
        }
        
        void RegisterAssembler(int id, StmAssembler::IDelegate *delegate)
        {
            RegisterInfo reginfo;
            reginfo.assemberId = id;
            reginfo.delegate = delegate;
            reginfo.finished = false;
            reginfo.registered = true;
            
            _thread->SendCommand(CMD_ID_REGISTER, &reginfo, sizeof(RegisterInfo*));
            while(!reginfo.finished) {
                sched_yield();
            }
            //wlj//LogDebug("assembler registered, id %d, delegate %p\n", id, delegate);
        }
        
        void UnregisterAssembler(int id)
        {
            RegisterInfo reginfo;
            reginfo.assemberId = id;
            reginfo.delegate = nullptr;
            reginfo.finished = false;
            reginfo.registered = false;
            
            _thread->SendCommand(CMD_ID_REGISTER, &reginfo, sizeof(RegisterInfo*));
            while(!reginfo.finished) {
                sched_yield();
            }
            //wlj//LogDebug("assembler unregistered, id %d\n", id);
        }
        
        void HandleStream(int assemblerId, const uint8_t *data, int length, Tick tick)
        {
            // the stream packet is almost a copy of mep udp stream packet
            // so its length is limited by MTU
            Assert(length <= 1500);
            StreamHeader *stm;
            {
                ScopedMutexLock slock(_stmAllocLock);
                stm = reinterpret_cast<StreamHeader*>(_stmAllocator.Alloc());
            }
            
            stm->assemblerId = assemblerId;
            stm->tick = tick;
            stm->length = length;
            memcpy(stm + 1, data, length);
            _thread->SendCommand(CMD_ID_STREAM, &stm, sizeof(stm));
        }
        
        void HandleDecodedResult(DecodingSegments *segments, bool suc)
        {
            DecodedResultInfo result;
            result.segments = segments;
            result.suc = suc;
            
            _thread->SendCommand(CMD_ID_DECODED, &result, sizeof(result));
        }
        
        virtual bool HandleMQThreadCommand(int cmd, const void *param, int paramlen, Tick tick) override
        {
            if (cmd == CMD_ID_STREAM) {
                Assert(paramlen == sizeof(StreamHeader*));
                StreamHeader *stm = *(StreamHeader**)(param);
                
                auto find = _assemblers.find(stm->assemblerId);
                if (find != _assemblers.end()) {
                    find->second->recver->HandleQPacket((uint8_t*)(stm + 1), stm->length, tick);
                }
                
                {
                    ScopedMutexLock slock(_stmAllocLock);
                    _stmAllocator.Free(stm);
                }
            }
            else if (cmd == CMD_ID_REGISTER) {
                Assert(paramlen == sizeof(RegisterInfo*));
                RegisterInfo *reginfo = *(RegisterInfo**)(param);
                
                if (reginfo->registered) {
                    Assert(reginfo->delegate != nullptr);

                    auto find = _assemblers.find(reginfo->assemberId);
                    Assert(find == _assemblers.end());
                    
                    AssemblerInfo *assembinfo = new AssemblerInfo();
                    assembinfo->delegate = reginfo->delegate;
                    assembinfo->id = reginfo->assemberId;
                    assembinfo->recver = new StreamRecver(this);
                    assembinfo->recver->SetTag(assembinfo);
                    
                    _assemblers[reginfo->assemberId] = assembinfo;
                }
                else {
                    auto find = _assemblers.find(reginfo->assemberId);
                    Assert(find != _assemblers.end());

                    delete find->second->recver;
                    _assemblers.erase(find);
                }
                reginfo->finished = true;
            }
            else if (cmd == CMD_ID_DECODED) {
                const DecodedResultInfo *result = reinterpret_cast<const DecodedResultInfo*>(param);
                Assert(paramlen == sizeof(DecodedResultInfo));
                
                Assert(_decodingNum > 0);
                _decodingNum--;
                ListRemove(result->segments); // remove from list
                
                if (result->suc == false) {
                    // terrible decoding fails
                }
                else {
                    auto find = _assemblers.find(result->segments->assemblerId);
                    if (find == _assemblers.end()) {
                        // the assembler was already destoryed
                    }
                    else {
                        StmAssembler::Frame stmfrm;
                        stmfrm.identity = result->segments->identity;
                        stmfrm.fid = result->segments->fid;
                        stmfrm.frmtype = result->segments->frmtype;
                        stmfrm.stmtype = result->segments->stmtype;
                        stmfrm.length = result->segments->frmlen;
                        stmfrm.payload = (const uint8_t*)result->segments->frmbuffer;
                        
                        // the frame buffer is transferred to client
                        result->segments->frmbuffer = nullptr;
                        
                        // callback the client
                        find->second->delegate->HandleRGridFrameRecved(stmfrm);
                    }
                }
                
                // release the decoding segments
                LIST_LOOP_BEGIN (result->segments->segments, RecvVideoFrame::Segment) {
                    _segmentAllocator.Free(litem);
                } LIST_LOOP_END;
                
                if (result->segments->frmbuffer != nullptr) {
                    ReleaseFrameBuffer(result->segments->frmbuffer);
                }
                
                _decodingSegmentsAllocator.Free(result->segments);
            }
            return true;
        }
        
        virtual bool HandleSocketReady(const SocketPoll::ReadyItem &readyItem, Tick tick) override
        {
            Assert(false);
            return true;
        }
        
        virtual bool HandlePreSocketPoll(int &timeout) override
        {
            return true;
        }
        
        virtual bool HandlePostSocketPoll(Tick tick) override
        {
            _smallFrmBufAllocator.Recycle();
            _bigFrmBufAllocator.Recycle();
            _hugeFrmBufAllocator.Recycle();
            return true;
        }
        
        virtual RecvVideoFrame::Segment* AllocateFrameSegment(int segdatalen) override
        {
            Assert(segdatalen <= 1500);
            return reinterpret_cast<RecvVideoFrame::Segment*>(_segmentAllocator.Alloc());
        }
        
        virtual void FreeFrameSegment(RecvVideoFrame::Segment* segment) override
        {
            if (segment != nullptr) {
                _segmentAllocator.Free(segment);
            }
        }
            
        /*
        *  if payload is not null, this is a non-fec frame, 
        *  and the payload is pointing to the data of HandleQPacket
        *  otherwise, StmAssembler assembly the frame from the segments
        */
        virtual void HandleFrameRecved(
            StreamRecver *recver, uint32_t identity, RecvFrame *frame, const uint8_t *payload) override
        {
            AssemblerInfo *assem = reinterpret_cast<AssemblerInfo*>(recver->GetTag());
            if (payload != nullptr) {
                // non-fec frame
                // callback to client
                StmAssembler::Frame stmfrm;
                stmfrm.identity = identity;
                stmfrm.fid = frame->fid;
                stmfrm.frmtype = frame->frmType;
                stmfrm.stmtype = frame->stmType;
                stmfrm.length = frame->length;
                
                void *frmbuf = AllocateFrameBuffer(frame->length);
                memcpy(frmbuf, payload, frame->length);
                stmfrm.payload = (uint8_t*)frmbuf;
                
                // callback the client
                assem->delegate->HandleRGridFrameRecved(stmfrm);
            }
            else {
                // fec frame
                Assert(frame->stmType != SESSION_STREAM_AUDIO);
                RecvVideoFrame *vfrm = static_cast<RecvVideoFrame*>(frame);
                Assert(vfrm->symSize > 0);
                
                int srcnum = vfrm->accumulator.SrcNum();
                int paddedLength = vfrm->symSize * srcnum;
                Assert(paddedLength >= frame->length);
                // we need padded length for frame buffer to handle the padded last src symbol
                void *frmbuf = AllocateFrameBuffer(paddedLength);

                if (vfrm->accumulator.IsMet()) {
                    int srccnt = 0;
                    uint16_t nextEsi = 0;
                    LIST_LOOP_BEGIN(vfrm->segments, RecvVideoFrame::Segment) {
                        // we need go through all list items to release all of them
                        if (srccnt < srcnum) {
                            Assert(litem->esi == nextEsi);
                            nextEsi = litem->esi + litem->cnt;

                            int cpnum = litem->cnt;
                            if (nextEsi > srcnum) {
                                cpnum = srcnum - litem->esi;
                            }

                            memcpy((char*)frmbuf + vfrm->symSize * litem->esi, litem + 1, cpnum * vfrm->symSize);
                            srccnt += cpnum;
                        }
                        
                        _segmentAllocator.Free(litem);
                    } LIST_LOOP_END;
                    
                    Assert(srccnt == srcnum);
                    // free those segments for StreamRecver
                    ListInitHead(&vfrm->segments);
                    
                    // callback to client
                    StmAssembler::Frame stmfrm;
                    stmfrm.identity = identity;
                    stmfrm.fid = frame->fid;
                    stmfrm.frmtype = frame->frmType;
                    stmfrm.stmtype = frame->stmType;
                    stmfrm.length = frame->length;
                    stmfrm.payload = (uint8_t*)frmbuf;
                    assem->delegate->HandleRGridFrameRecved(stmfrm);
                }
                else {
                    // move segments to decoding segments, then ask fec to decode
                    DecodingSegments *decoding = reinterpret_cast<DecodingSegments*>(_decodingSegmentsAllocator.Alloc());
                    decoding->identity = identity;
                    decoding->assemblerId = assem->id;
                    decoding->stmtype = frame->stmType;
                    decoding->frmtype = frame->frmType;
                    decoding->frmlen = frame->length;
                    decoding->srcNum = srcnum;
                    decoding->symSize = vfrm->symSize;
                    decoding->maxESI = vfrm->maxESI;
                    decoding->fid = vfrm->fid;
                    decoding->frmbuffer = frmbuf;
                    // the segments list is moved to decoding segments
                    ListReplaceHeader(&vfrm->segments, &decoding->segments);
                    
                    ListAddToEnd(&_decodingHdr, decoding);
                    _decodingNum++;
                    
                    _delegate->HandleFecDecodingRequest(decoding);
                }
            }
        }
    };
}

namespace hpsp {
    
    class StmFecThread : public MQThread::IDelegate {
    public:
        struct IDelegate {
            virtual ~IDelegate() {}
            virtual void HandleFecDecoded(DecodingSegments *segments, bool suc) = 0;
        };
        
    private:
        enum {
            CMD_ID_DECODE = 1,
        };
        
        MQThread *_thread;
        IDelegate *_delegate;
        FecBufferAllocator _fecAllocator;
        const uint8_t *_symbolPtrs[65535];
        
    public:
        StmFecThread(IDelegate *delegate) 
            : _thread(nullptr)
            , _delegate(delegate)
            , _fecAllocator()
        {
        }
        
        ~StmFecThread()
        {
            Assert(false);
        }
        
        void SetThread(MQThread *thread)
        {
            _thread = thread;
        }
        
        void RequestDecoding(DecodingSegments *segments)
        {
            _thread->SendCommand(CMD_ID_DECODE, &segments, sizeof(segments));
        }
    
        virtual bool HandleMQThreadCommand(int cmd, const void *param, int paramlen, Tick tick) override
        {
            if (cmd == CMD_ID_DECODE) {
                DecodingSegments *segments = *(DecodingSegments**)param;
                Assert(paramlen == sizeof(DecodingSegments*));
                Assert(segments->maxESI <= 65535);
                
                int srcnum = segments->srcNum + StreamRecver::symbolDecodeOverheadNum;
                memset(_symbolPtrs, 0, sizeof(const uint8_t*) * segments->maxESI);
                
                int inputCnt = 0;
                uint16_t nextEsi = 0;
                uint16_t maxEsi = 0;
                LIST_LOOP_BEGIN(segments->segments, RecvVideoFrame::Segment) {
                    Assert(litem->esi >= nextEsi);
                    nextEsi = litem->esi + litem->cnt;
                    
                    for (int i = 0; i < litem->cnt && inputCnt < srcnum; i++) {
                        maxEsi = litem->esi + i;
                        Assert(maxEsi < segments->maxESI);
                        
                        _symbolPtrs[maxEsi] = (uint8_t*)(litem + 1) + i * segments->symSize;
                        inputCnt++;
                    }
                    
                    if (inputCnt >= srcnum) {
                        break;
                    }
                } LIST_LOOP_END;
                
                Assert(inputCnt >= srcnum);
                
                fec::Param fecparam;
                fec::Param::GetParam(segments->srcNum, segments->symSize, fecparam);
                fec::LTSymbols ltSymbols;
                
                bool decoderet = fec::FastFec::TryDecode(fecparam, _symbolPtrs, maxEsi + 1, 
                    inputCnt, &_fecAllocator, ltSymbols);
                
                if (decoderet) {
                    // filling the frm buffer
                    for (uint16_t esi = 0; esi < segments->srcNum; esi++) {
                        if (_symbolPtrs[esi] != nullptr) {
                            memcpy((char*)segments->frmbuffer + esi * segments->symSize, _symbolPtrs[esi], segments->symSize);
                        }
                        else {
                            __attribute__((aligned(16))) uint8_t tmp[256];
                            fec::FastFec::RecoverSymbol(ltSymbols, esi, tmp, sizeof(tmp));
                            memcpy((char*)segments->frmbuffer + esi * segments->symSize, tmp, segments->symSize);
                        }
                    }
                    _fecAllocator.FreeFecBuffer(ltSymbols.symbolVec, fec::IAllocator::PurposeLT);
                }
                _delegate->HandleFecDecoded(segments, decoderet);
            }
            return true;
        }
        
        virtual bool HandleSocketReady(const SocketPoll::ReadyItem &readyItem, Tick tick) override
        {
            Assert(false);
            return true;
        }
        
        virtual bool HandlePreSocketPoll(int &timeout) override
        {
            return true;
        }
        
        virtual bool HandlePostSocketPoll(Tick tick) override
        {
            return true;
        }
    };
}


using namespace hpsp;

static int _assemblerId = 0;
static bool _assemblerInited = false;
static Mutex _assemblerInitLock;
static StmRecverThread *_recverThread = nullptr;
static StmFecThread *_fecThread = nullptr;

class StmThreadsGlue : public StmRecverThread::IDelegate, public StmFecThread::IDelegate {
public:
    virtual void HandleFecDecodingRequest(DecodingSegments *segments) override
    {
        _fecThread->RequestDecoding(segments);
    }
    
    virtual void HandleFecDecoded(DecodingSegments *segments, bool suc) override
    {
        _recverThread->HandleDecodedResult(segments, suc);
    }
};
static StmThreadsGlue _stmThreadGlue;

static void InitializeStmThreads()
{
    if (false == _assemblerInited) {
        ScopedMutexLock slock(_assemblerInitLock);
        if (false == _assemblerInited) {
            StmRecverThread *recver = new StmRecverThread(&_stmThreadGlue);
            MQThread *recverthread = MQThread::Create("stmrecver", 0, recver, false);
            recver->SetThread(recverthread);
            
            StmFecThread *fec = new StmFecThread(&_stmThreadGlue);
            MQThread *fecthread = MQThread::Create("stmfec", 0, fec, false);
            fec->SetThread(fecthread);
            
            _assemblerInited = true;
        }
    }
}

/// StmAssembler
StmAssembler::StmAssembler(IDelegate *delegate)
    : _delegate(delegate)
{
    Assert(delegate != nullptr);
    InitializeStmThreads();
    
    _assemblerId = AtomicOperation::Increment(&_assemblerId, 1);
    _recverThread->RegisterAssembler(_assemblerId, delegate);
}

StmAssembler::~StmAssembler()
{
    _recverThread->UnregisterAssembler(_assemblerId);
}

/*
*  The data is the body part of the grid stream packet
*/ 
void StmAssembler::HandleRGridStream(const uint8_t *data, int length, Tick tick)
{
    if (data != nullptr && length > 0) {
        _recverThread->HandleStream(_assemblerId, data, length, tick);
    }
}

void StmAssembler::ReleaseFramePayload(const void *payload)
{
    if (payload != nullptr) {
        Assert(_recverThread != nullptr);
        _recverThread->ReleaseFramePayload(payload);
    }
}
