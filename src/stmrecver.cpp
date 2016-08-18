#include "accumu.h"
#include "frmqueue.h"
#include "fqueue.h"
#include "flist.h"
#include "fbuffer.h"
#include "netendian.h"
#include "misc.h"
#include "bits.h"
#include "fast_fec.h"
#include "userproto.h"
#include "stmrecver.h"
#include "fec.h"

using namespace cppcmn;
using namespace hpsp;

StreamRecver::StreamRecver(IDelegate *delegate) 
	: _delegate(delegate)
    , _tag(nullptr)
	, _audioAllocator(sizeof(RecvFrame), 20, 0)
	, _videoAllocator(sizeof(RecvVideoFrame), 20, 0)
{
}

StreamRecver::~StreamRecver() 
{
	// clear the recv identities
	for (auto &iter : _identities) {
		ReleaseRecvIdentity(iter.second, true);
	}
}

void StreamRecver::HandleQPacket(const uint8_t *data, int length, Tick tick) 
{
	PacketPreviewInfo preview;
	bool suc = PacketUtils::PreVerify(data, length, preview);
    if (!suc) {
        LogError("stmrecver", "stmrecver", "recver verify stm failed, length %d\n", length);
        return;
    }
	Assert(preview.type == UserPacketType::Quic);

	StreamPacketDesc stmdesc;
	suc = PacketUtils::GenStreamPacketDesc(preview, data, length, stmdesc);
	Assert(suc);

    LogVerbose("stmrecver", "stmrecver", "recving pack iden %u, pn %llu, length %d\n", stmdesc.identity, stmdesc.pn, length);
    
    // create identity info is not created
    RecvIdentity *recviden = GetRecvIdentity(stmdesc.identity, tick);

	for (int i = 0; i < stmdesc.sliceNum; i++) {
		StreamPacketDesc::Slice &slice = stmdesc.slices[i];
        Assert(PacketUtils::IsStreamFrameSlice(slice.type));
        
        StreamFrameSliceDetail frmDetail;
        suc = PacketUtils::GenStreamFrameSliceDetail(data, stmdesc, i, frmDetail);
        Assert(suc);

        if (frmDetail.symSize == 0) {
            HandleSliceNonFecPayload(recviden, frmDetail, tick);
        }
        else {
            HandleSliceFecSegment(recviden, frmDetail, tick);
        }
	}
}

void StreamRecver::HandleSliceFecSegment(RecvIdentity *recver, const StreamFrameSliceDetail &slice, Tick tick)
{
	ObsoleteFrames(recver, slice.stmType, tick);

	Assert(slice.symSize < 256);
	Assert(slice.stmType == SESSION_STREAM_LIVE_VIDEO || slice.stmType == SESSION_STREAM_THUMB_VIDEO);

	RecvIdentity::RecvFrameQueue &fqueue = 
		slice.stmType == SESSION_STREAM_LIVE_VIDEO ? recver->liveFrames : recver->thumbFrames;

	bool isnew = false;
	RecvVideoFrame* frm = static_cast<RecvVideoFrame*>(GetRecvFrame(recver, slice.stmType, fqueue, slice, isnew));
	if (frm == nullptr) {
		// the fid is too old for inserting, discarded
		LogWarning("stmrecver", "recving too late frame iden %u, fid %d by %d\n", recver->identity, slice.fid, fqueue.FrameIdBegin());
		return;
	}

	if (isnew) {
        frm->flag = 0;
        frm->fid = slice.fid;
        frm->frmType = slice.frmType;
        frm->stmType = slice.stmType;
        frm->length = slice.frmLen;
        frm->symSize = slice.symSize;
        frm->recvSymNum = 0;
        frm->maxESI = 0;
        ListInitHead(&frm->segments);
	}
	else {
		Assert(frm->length == slice.frmLen);
		Assert(frm->frmType == slice.frmType);
		Assert(frm->symSize == slice.symSize);
	}

	if (frm->flag & RecvFrame::FRAME_COMPLETED) {
		return;
	}

	frm->tick = tick;

	InsertFrameSegment(frm, slice);

	if (frm->accumulator.IsMet() || frm->recvSymNum >= frm->accumulator.SrcNum() + symbolDecodeOverheadNum) {
		LogDebug("stmrecver", "completing iden %u, fid %d, stmtype %d, srcmet %d, recvnum %d, srcnum %d\n",
			recver->identity, frm->fid, frm->stmType, frm->accumulator.IsMet(), frm->recvSymNum, frm->accumulator.SrcNum());

		frm->flag = RecvFrame::FRAME_COMPLETED;

        _delegate->HandleFrameRecved(this, recver->identity, frm, nullptr);
        Assert(ListIsEmpty(&frm->segments));
	}
}

void StreamRecver::HandleSliceNonFecPayload(RecvIdentity *recver, const StreamFrameSliceDetail &slice, Tick tick)
{
	ObsoleteFrames(recver, slice.stmType, tick);

	RecvIdentity::RecvFrameQueue &fqueue = slice.stmType == SESSION_STREAM_AUDIO ?
		recver->audioFrames : (slice.stmType == SESSION_STREAM_LIVE_VIDEO ? recver->liveFrames : recver->thumbFrames);

	bool isnew = false;
	RecvFrame* frm = GetRecvFrame(recver, slice.stmType, fqueue, slice, isnew);
	if (frm == nullptr) {
		// the fid is too old for inserting, discarded
		LogWarning("stmrecver", "recving iden %u too late non-fec fid %d by %d, type %d\n", 
			recver->identity, slice.fid, fqueue.FrameIdBegin(), slice.stmType);
		return;
	}

	if (!isnew) {
		Assert(frm->flag & RecvFrame::FRAME_COMPLETED);
		Assert(frm->length == slice.frmLen);
		return;
	}

	LogDebug("stmrecver", "recving iden %u non-fec fid %d, stmtype %d, frmlen %d\n", 
        recver->identity, slice.fid, slice.stmType, slice.frmLen);

	Assert(slice.payloadLen == slice.frmLen && slice.frmLen > 0);

	frm->fid = slice.fid;
	frm->flag = RecvFrame::FRAME_COMPLETED;
	frm->frmType = slice.frmType;
	frm->stmType = slice.stmType;
	frm->length = slice.frmLen;
	frm->tick = tick;

    if (slice.stmType != SESSION_STREAM_AUDIO) {
		RecvVideoFrame *videofrm = static_cast<RecvVideoFrame*>(frm);
		videofrm->recvSymNum = 0;
		videofrm->symSize = 0;
        videofrm->maxESI = 0;
		ListInitHead(&videofrm->segments);
	}

	_delegate->HandleFrameRecved(this, recver->identity, frm, slice.payload);	
}

struct ObsoleteFrameTag {
	Tick now;
	StreamRecver *pthis;
	uint32_t identity;
	int stmType;
};

Tick _frameTimeout = TickFromSeconds(3);

RecvIdentity::RecvFrameQueue::VisitorRes
StreamRecver::ObsoleteFrameVisitor(RecvIdentity::RecvFrameQueue::Slot *slot, uint16_t fid, void *tag)
{
	auto *obsoleteTag = reinterpret_cast<ObsoleteFrameTag*>(tag);
	auto *frm = *reinterpret_cast<RecvFrame**>(slot->body);
    Assert(frm->fid == fid);
    
	if (frm->tick + _frameTimeout < obsoleteTag->now) {
        LogVerbose("stmrecver", "recver iden %u obsolete fid %u, stype %d, tick %llu\n", 
            obsoleteTag->identity, fid, obsoleteTag->stmType, frm->tick);
        
		if ((frm->flag & RecvFrame::FRAME_COMPLETED) == 0) {
			LogWarning("stmrecver", "recver iden %u obsolete uncompleted fid %d, type %d, frmlen %d\n",
				obsoleteTag->identity, frm->fid, frm->stmType, frm->length);
		}

		if (obsoleteTag->stmType != SESSION_STREAM_AUDIO) {
			obsoleteTag->pthis->ReleaseFrameSegment(static_cast<RecvVideoFrame*>(frm));
		}

		if (obsoleteTag->stmType == SESSION_STREAM_AUDIO) {
			obsoleteTag->pthis->_audioAllocator.Free(frm);
		}
		else {
			obsoleteTag->pthis->_videoAllocator.Free(frm);
		}
		return RecvIdentity::RecvFrameQueue::VisitorRes::DeletedContinue;
	}
	return RecvIdentity::RecvFrameQueue::VisitorRes::Stop;
}

void StreamRecver::ObsoleteFrames(RecvIdentity *recver, int stmtype, Tick tick)
{
	/*
		remove those frame has tick 1 seconds ago
		but we don't remove the completed frame, we need them in queue for placeholder
	*/
	RecvIdentity::RecvFrameQueue &fqueue = stmtype == SESSION_STREAM_AUDIO ?
		recver->audioFrames : (stmtype == SESSION_STREAM_LIVE_VIDEO ? recver->liveFrames : recver->thumbFrames);

	ObsoleteFrameTag tag;
	tag.now = tick;
	tag.pthis = this;
	tag.identity = recver->identity;
	tag.stmType = stmtype;
	fqueue.LoopFrames(ObsoleteFrameVisitor, &tag);
}

Tick _identityTimeout = TickFromSeconds(3);
RecvIdentity* StreamRecver::GetRecvIdentity(uint32_t identity, Tick tick)
{
	Assert(identity != InvalidIdentity);
	RecvIdentityMap::iterator find = _identities.find(identity);
	if (find != _identities.end()) {
        if (find->second->lastTick + _identityTimeout < tick) {
            // this identity already timeout, reset it
            ReleaseRecvIdentity(find->second, false);
        }
        find->second->lastTick = tick;
		return find->second;
	}

	LogDebug("stmrecver", "stmrecver creating identity %u\n", identity);
	RecvIdentity *recver = new RecvIdentity();
	recver->identity = identity;
    recver->lastTick = tick;
	_identities[identity] = recver;
	return recver;
}

RecvIdentity::RecvFrameQueue::VisitorRes
StreamRecver::InsertFrameVisitor(RecvIdentity::RecvFrameQueue::Slot *slot, uint16_t fid, void *tag)
{
	auto *obsoleteTag = reinterpret_cast<ObsoleteFrameTag*>(tag);
	auto *frm = *reinterpret_cast<RecvFrame**>(slot->body);
    Assert(frm->fid == fid);
    
    LogVerbose("stmrecver", "recver %u dequeue fid %u, stype %d, tick %llu\n", 
        obsoleteTag->identity, fid, obsoleteTag->stmType, frm->tick);
	
	if ((frm->flag & RecvFrame::FRAME_COMPLETED) == 0) {
		LogWarning("stmrecver", "recver %u dequeue uncompleted fid %d, type %d, frmlen %d\n",
					obsoleteTag->identity, frm->fid, frm->stmType, frm->length);
	}

	if (obsoleteTag->stmType != SESSION_STREAM_AUDIO) {
		obsoleteTag->pthis->ReleaseFrameSegment(static_cast<RecvVideoFrame*>(frm));
	}

	if (obsoleteTag->stmType == SESSION_STREAM_AUDIO) {
		obsoleteTag->pthis->_audioAllocator.Free(frm);
	}
	else {
		obsoleteTag->pthis->_videoAllocator.Free(frm);
	}

	return RecvIdentity::RecvFrameQueue::VisitorRes::DeletedContinue;
}

RecvFrame* StreamRecver::GetRecvFrame(RecvIdentity *recver, int stmtype,
	RecvIdentity::RecvFrameQueue &frames, const StreamFrameSliceDetail &slice, bool &isnew)
{
	/*
		gen an ack info for dequeued incompleted frame to let server stopping sending
	*/
	ObsoleteFrameTag tag;
	tag.now = 0;
	tag.pthis = this;
	tag.identity = recver->identity;
	tag.stmType = stmtype;

	auto *slot = frames.Insert(slice.fid, isnew, InsertFrameVisitor, &tag);
	if (slot == nullptr) {
		return nullptr;
	}

	RecvFrame *frame = nullptr;
	if (isnew) {
		if (stmtype == SESSION_STREAM_AUDIO) {
			frame = reinterpret_cast<RecvFrame*>(_audioAllocator.Alloc());
		}
		else {
			void *buf = _videoAllocator.Alloc();
			frame = new (buf) RecvVideoFrame(slice.symSize == 0 ? 0 :
				(slice.frmLen / slice.symSize + (slice.frmLen % slice.symSize == 0 ? 0 : 1)));
		}
		*reinterpret_cast<RecvFrame**>(slot->body) = frame;
        
        LogVerbose("stmrecver", "recver iden %u get newfrm fid %u, stype %u, flen %u\n", 
                    recver->identity, slice.fid, slice.stmType, slice.frmLen);
	}
	else {
		frame = *reinterpret_cast<RecvFrame**>(slot->body);
        Assert(frame->fid == slice.fid);
	}
	return frame;
}

RecvIdentity::RecvFrameQueue::VisitorRes
StreamRecver::ReleaseFrameVisitor(RecvIdentity::RecvFrameQueue::Slot *slot, uint16_t fid, void *tag)
{
	auto *obsoleteTag = reinterpret_cast<ObsoleteFrameTag*>(tag);
	auto *frm = *reinterpret_cast<RecvFrame**>(slot->body);

	if (obsoleteTag->stmType != SESSION_STREAM_AUDIO) {
		obsoleteTag->pthis->ReleaseFrameSegment(static_cast<RecvVideoFrame*>(frm));
	}

	if (obsoleteTag->stmType == SESSION_STREAM_AUDIO) {
		obsoleteTag->pthis->_audioAllocator.Free(frm);
	}
	else {
		obsoleteTag->pthis->_videoAllocator.Free(frm);
	}

	return RecvIdentity::RecvFrameQueue::VisitorRes::DeletedContinue;
}

void StreamRecver::ReleaseRecvIdentity(RecvIdentity *recver, bool deleteRecv)
{
	ObsoleteFrameTag tag;
	tag.now = 0;
	tag.pthis = this;
	tag.identity = recver->identity;

	tag.stmType = SESSION_STREAM_AUDIO;
	recver->audioFrames.Reset(ReleaseFrameVisitor, &tag);

	tag.stmType = SESSION_STREAM_LIVE_VIDEO;
	recver->liveFrames.Reset(ReleaseFrameVisitor, &tag);

	tag.stmType = SESSION_STREAM_THUMB_VIDEO;
	recver->thumbFrames.Reset(ReleaseFrameVisitor, &tag);

    if (deleteRecv) {
        delete recver;
    }
}

void StreamRecver::ReleaseFrameSegment(RecvVideoFrame *vfrm)
{
	LIST_LOOP_BEGIN(vfrm->segments, RecvVideoFrame::Segment) {
		_delegate->FreeFrameSegment(litem);
	} LIST_LOOP_END;
	ListInitHead(&vfrm->segments);
}

int SegmentInsertComparer(ListHead *hdr, void *tag) 
{
	RecvVideoFrame::Segment *segment = reinterpret_cast<RecvVideoFrame::Segment*>(tag);
	RecvVideoFrame::Segment *prev = static_cast<RecvVideoFrame::Segment*>(hdr);
	if (segment->esi >= prev->esi + prev->cnt) {
		return 1;
	}
	if (segment->esi + segment->cnt <= prev->esi) {
		return -1;
	}

	return 0;
}

void StreamRecver::InsertFrameSegment(RecvVideoFrame *vfrm, const StreamFrameSliceDetail &slice)
{
	Assert(vfrm->symSize > 0);
    Assert(vfrm->symSize == slice.symSize);
	int segmentSize = sizeof(RecvVideoFrame::Segment) + slice.symNum * vfrm->symSize;
	auto *segment = _delegate->AllocateFrameSegment(segmentSize);
	segment->cnt = slice.symNum;
	segment->esi = slice.symESI;
	memcpy(segment + 1, slice.payload, segment->cnt * vfrm->symSize);
	
	// insert the segment in the esi orders
	// it will make decoding easier
	bool isdup = false;
	ListHead *insertAfter = ListInsertFromTail(&vfrm->segments, SegmentInsertComparer, segment, isdup);
    if (isdup) {
        LogWarning("stmrecver", "recving dup segment fid %u, esi %d\n", vfrm->fid, segment->cnt);
        _delegate->FreeFrameSegment(segment);
        return;
    }
    
	Assert(insertAfter != nullptr);
	ListInsertAfter(insertAfter, segment);

	vfrm->recvSymNum += segment->cnt;
	vfrm->accumulator.AddRange(segment->esi, segment->cnt);
    if (segment->esi + segment->cnt > vfrm->maxESI) {
        vfrm->maxESI = segment->esi + segment->cnt;
    }
}
