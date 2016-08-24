#ifndef _HPSP_STMRECVER_H
#define _HPSP_STMRECVER_H

#include <map>
#include "misc.h"
#include "clockex.h"
#include "fqueue.h"
#include "frmqueue.h"
#include "fbuffer.h"
#include "accumu.h"
#include "packet.h"

namespace hpsp {

    struct RecvFrame {
        enum {
            FRAME_COMPLETED = 1,
        };
        int flag;
        uint8_t stmType;
        uint8_t frmType;
        uint16_t fid;
        uint16_t length;
        cppcmn::Tick tick;
    };

    struct RecvVideoFrame : RecvFrame {
        uint16_t symSize;
        uint16_t recvSymNum;
        uint16_t maxESI;    // the max of the next expecting esi
        cppcmn::IntAccumulator accumulator;

        struct Segment : cppcmn::ListHead {
            uint16_t esi;
            uint16_t cnt;
            // raw data right after it
        };
        cppcmn::ListHead segments;

        RecvVideoFrame(uint16_t srcSymNum) : accumulator(srcSymNum) {}
    };

    struct RecvIdentity {
        uint32_t identity;
        cppcmn::Tick lastTick;
        
        typedef cppcmn::FrameQueue<60, sizeof(RecvFrame*)> RecvFrameQueue;
        RecvFrameQueue audioFrames;
        RecvFrameQueue thumbFrames;
        RecvFrameQueue liveFrames;
    };

    /*
        get a frame, check its ack, recover it
        transfer to client
    */
    class StreamRecver {
    public:
        static const int symbolDecodeOverheadNum = 3;
        struct IDelegate {
            virtual ~IDelegate() {}
            virtual RecvVideoFrame::Segment* AllocateFrameSegment(int segdatalen) = 0;
            virtual void FreeFrameSegment(RecvVideoFrame::Segment* segment) = 0;
            /*
             *  if payload is not null, this is a non-fec frame, 
             *  and the payload is pointing to the data of HandleQPacket
             *  otherwise, StmAssembler assembly the frame from the segments
             */
            virtual void HandleFrameRecved(
                    StreamRecver *recver, uint32_t identity, RecvFrame *frame, const uint8_t *payload) = 0;
        };

        StreamRecver(IDelegate *delegate);
        virtual ~StreamRecver();

        void HandleQPacket(const uint8_t *data, int length, cppcmn::Tick tick);
        
        void  SetTag(void *tag) { _tag = tag; }
        void* GetTag() const { return _tag; }

    private:
        DISALLOW_COPY_AND_ASSIGN(StreamRecver);

        void HandleSliceFecSegment(RecvIdentity *recviden, const StreamFrameSliceDetail &slice, cppcmn::Tick tick);
        void HandleSliceNonFecPayload(RecvIdentity *recviden, const StreamFrameSliceDetail &slice, cppcmn::Tick tick);
        void ObsoleteFrames(RecvIdentity *recver, int stmtype, cppcmn::Tick tick);
        RecvIdentity* GetRecvIdentity(uint32_t identity, cppcmn::Tick tick);
        RecvFrame* GetRecvFrame(RecvIdentity *recver, int stmtype, RecvIdentity::RecvFrameQueue &frames, 
                                const StreamFrameSliceDetail &slice, bool &isnew);
        void ReleaseRecvIdentity(RecvIdentity *recv, bool deleteRecv);
        void ReleaseFrameSegment(RecvVideoFrame *vfrm);
        void InsertFrameSegment(RecvVideoFrame *vfrm, const StreamFrameSliceDetail &slice);
        
        static RecvIdentity::RecvFrameQueue::VisitorRes 
        ObsoleteFrameVisitor(RecvIdentity::RecvFrameQueue::Slot *slot, uint16_t fid, void *tag);
        
        static RecvIdentity::RecvFrameQueue::VisitorRes 
        InsertFrameVisitor(RecvIdentity::RecvFrameQueue::Slot *slot, uint16_t fid, void *tag);
        
        static RecvIdentity::RecvFrameQueue::VisitorRes 
        ReleaseFrameVisitor(RecvIdentity::RecvFrameQueue::Slot *slot, uint16_t fid, void *tag);

        IDelegate *_delegate;
        void *_tag;

        typedef std::map<uint32_t, RecvIdentity*> RecvIdentityMap;
        RecvIdentityMap _identities;

        cppcmn::FixedSizeAllocator _audioAllocator;
        cppcmn::FixedSizeAllocator _videoAllocator;
    };
}

#endif // !_HPSP_STMRECVER_H

