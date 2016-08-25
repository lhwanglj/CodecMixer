#ifndef _CPPCMN_FRMQUEUE_H
#define _CPPCMN_FRMQUEUE_H

#include <stdint.h>
#include "logging.h"
#include "fqueue.h"

namespace cppcmn {

	struct FrameIdComparer {
		static inline bool IsLater(uint16_t later, uint16_t prev) {
			return later != prev && static_cast<uint16_t>(later - prev) < 0x8000u;
		}

		static inline bool IsEarlier(uint16_t earlier, uint16_t later) {
			return later != earlier && static_cast<uint16_t>(later - earlier) < 0x8000u;
		}

		static inline bool IsNotLater(uint16_t notlater, uint16_t prev) {
			return notlater == prev || static_cast<uint16_t>(prev - notlater) < 0x8000u;
		}

		static inline bool IsNotEarlier(uint16_t notEarlier, uint16_t prev) {
			return notEarlier == prev || static_cast<uint16_t>(notEarlier - prev) < 0x8000u;
		}

		static inline uint16_t LaterId(uint16_t id1, uint16_t id2) {
			return IsLater(id1, id2) ? id1 : id2;
		}

		static inline uint16_t Distance(uint16_t later, uint16_t prev) {
			return later - prev;
		}
	};

	/*
		For all frame cache in stream recver/sender, frame mgr. They should just handle finite frames in queue
		The queue has a discard threshold, all frames eariler than it will not be handled at all
		The queue's capability is fixed also for handling a specific time period of frames

		It should be a circle queue
		N - max frame number in this queue
		S - slot size
	*/
	template<int N, int S>
	class FrameQueue {
	public:
		struct Slot {
			char body[S];
		private:
			bool empty;
			uint16_t fid;
			friend class FrameQueue<N, S>;
		};
		static const int SlotSize = sizeof(Slot);
		static const int BodySize = S;
		static const int Capability = N;

		enum class VisitorRes {
			Continue,
			DeletedContinue,
			Stop,
			DeletedStop
		};
		typedef VisitorRes(*VisitorFunc) (Slot*, uint16_t, void*);

		FrameQueue() : _frmCnt(0) {}

		/*
			The frame queue is a N slot that has continous frame ids
			When inserting new frame, there are N first slot maybe erased if the queue is full
		*/
		Slot* Insert(uint16_t fid, bool &isnew, VisitorFunc efunc, void *tag) 
        {
			isnew = true;
			if (_frmCnt == 0) {
				Assert(_queue.Count() == 0);
				Slot *slot = reinterpret_cast<Slot*>(_queue.AppendSlot());
				slot->empty = false;
				slot->fid = fid;
				_fidBegin = _fidEnd = fid;
				++_frmCnt;
				return slot;
			}

			if (FrameIdComparer::IsEarlier(fid, _fidBegin)) {
				// too old frame
				isnew = false;
				return nullptr;
			}

			if (FrameIdComparer::IsNotLater(fid, _fidEnd)) {
                int idx = FrameIdComparer::Distance(fid, _fidBegin);
				Assert(idx >= 0 && idx < _queue.Count());

                Slot *slot = reinterpret_cast<Slot*>(_queue.At(idx));
                Assert(slot->fid == fid);
				if (slot->empty) {
					slot->empty = false;
					++_frmCnt;
				}
				else {
					isnew = false;
				}
				return slot;
			}

			FreeSlotForNewFrame(fid, efunc, tag);
			if (_frmCnt == 0) {
				return Insert(fid, isnew, efunc, tag);
			}
			else {
				uint16_t nextid = _fidEnd + 1;
				for (;;) {
					Assert(_queue.Count() < N);
					Slot *slot = reinterpret_cast<Slot*>(_queue.AppendSlot());
					if (nextid != fid) {
						slot->empty = true;
						slot->fid = nextid;
						++nextid;
						continue;
					}

					slot->empty = false;
					slot->fid = nextid;
					_fidEnd = fid;
					++_frmCnt;
					return slot;
				}
			}
			return nullptr;
		}

		/*
			returning non-null frame only
		*/
		inline Slot* At(uint16_t fid) 
        {
			if (_frmCnt > 0 &&
				FrameIdComparer::IsNotEarlier(fid, _fidBegin) &&
				FrameIdComparer::IsNotLater(fid, _fidEnd)) 
            {
                int idx = FrameIdComparer::Distance(fid, _fidBegin);
				Assert(idx >= 0 && idx < _queue.Count());

				Slot *pslot = reinterpret_cast<Slot*>(_queue.At(idx));
                Assert(pslot->fid == fid);
				if (!pslot->empty) {
					return pslot;
				}
			}
			return nullptr;
		}
        
        /*
            Remove a frame by its fid, which was inserted by Insert
            Returning true - if a frame is removed; If no such valid frame found, then returning false
            efunc is called to delete the found frame slot content
         */ 
        bool Remove(uint16_t fid, VisitorFunc efunc, void *tag)
        {
            Slot *slot = At(fid);
            if (slot == nullptr) {
                return false;
            }
            
            if (efunc != nullptr) {
                efunc(slot, slot->fid, tag);
            }

            Assert(_frmCnt > 0);
            slot->empty = true;
            
            if (_frmCnt == 1) {
                Assert(fid == _fidBegin && fid == _fidEnd);
                QuickReset();
                return true;
            }
            
            _frmCnt--;
            
            if (fid == _fidBegin || fid == _fidEnd) {
                // remove the all empty frames from the beginning
                int queueCnt = _queue.Count();
                bool fromBegin = fid == _fidBegin;
                void *qslot = fromBegin ? _queue.FirstSlot() : _queue.LastSlot();
                Assert(reinterpret_cast<Slot*>(qslot) == slot);
                Assert(reinterpret_cast<Slot*>(qslot)->fid == fid);
                
                for (int i = 0; i < queueCnt; i++) {
                    Slot *s = reinterpret_cast<Slot*>(qslot);
                    if (s->empty) {
                        qslot = fromBegin ? _queue.NextSlot(qslot) : _queue.PrevSlot(qslot);
                        continue;
                    }
                    
                    if (fromBegin) {
                        _queue.EraseFirstNSlot(i);
                        _fidBegin = s->fid;
                    }
                    else {
                        _queue.EraseLastNSlot(i);
                        _fidEnd = s->fid;
                    }
                    break;
                }
            }
            
            Assert(_frmCnt <= _queue.Count());
            return true;
        }

		void Reset(VisitorFunc efunc, void *tag) 
        {
			if (_frmCnt > 0) {
                int queuecnt = _queue.Count();
                Assert(queuecnt >= _frmCnt);
                
                void *qslot = _queue.FirstSlot();
                for (int i = 0; i < queuecnt; i++) {
                    Slot *slot = reinterpret_cast<Slot*>(qslot);
                    if (!slot->empty) {
                        if (efunc != nullptr) {
                            efunc(slot, slot->fid, tag);
                        }
                    }
                    qslot = _queue.NextSlot(qslot);
                }
			}
            else {
                Assert(_queue.Count() == 0);
            }
            
			_queue.Reset();
			_frmCnt = 0;
		}

        // used when the slot item doesn't need deletion
        inline void QuickReset() 
        {
            _queue.Reset();
            _frmCnt = 0;
        }

		void LoopFrames(VisitorFunc vfunc, void *tag) 
        {
			if (_frmCnt == 0) {
				Assert(_queue.Count() == 0);
				return;
			}
            
			bool firstFrmMet = false;
			int emptyNumAtBegin = 0;
			uint16_t prefid = _fidBegin;
			int preIdx = 0;
            
            int queueCnt = _queue.Count();
            Assert(queueCnt >= _frmCnt);

			void *qslot = _queue.FirstSlot();
			Assert(reinterpret_cast<Slot*>(qslot)->fid == _fidBegin);
            Assert(reinterpret_cast<Slot*>(qslot)->empty == false);

			for (int i = 0; i < queueCnt; i++) {
				Slot *slot = reinterpret_cast<Slot*>(qslot);
				if (slot->empty) {
					if (!firstFrmMet) {
						++emptyNumAtBegin;
					}
				}
				else {
					auto vres = vfunc(slot, slot->fid, tag);
					if (vres == VisitorRes::DeletedContinue || vres == VisitorRes::DeletedStop) {
						slot->empty = true;
						if (!firstFrmMet) {
							++emptyNumAtBegin;
						}

						if (--_frmCnt == 0) {
							break;
						}
					}
					else {
						firstFrmMet = true;
						prefid = slot->fid;
						preIdx = i;
					}

					if (slot->fid == _fidEnd) {
						Assert(firstFrmMet);
						Assert(_frmCnt > 0);
						Assert(i + 1 == queueCnt);
						if (slot->empty) {
							// the last one get deleted
							// remove the empty slots at tail
							_queue.EraseLastNSlot(queueCnt - preIdx);
							_fidEnd = prefid;
						}
						break;
					}

					if (vres == VisitorRes::DeletedStop || vres == VisitorRes::Stop) {
						break;
					}
				}

				qslot = _queue.NextSlot(qslot);
			}

			if (_frmCnt == 0) {
				_queue.Reset();
			}
			else {
				if (emptyNumAtBegin > 0) {
                    Assert(emptyNumAtBegin < _queue.Count());
					_queue.EraseFirstNSlot(emptyNumAtBegin);
				}
                
                Slot* newFirst = reinterpret_cast<Slot*>(_queue.FirstSlot());
                Assert(newFirst->empty == false);
                
                _fidBegin = newFirst->fid;
                Assert(FrameIdComparer::IsNotLater(_fidBegin, _fidEnd));
                Assert(_frmCnt <= _queue.Count());
			}
		}

		/*
			Total slot count allocated for invalid/empty frames
		*/
		inline int SlotCount() const { return _queue.Count(); }
		/*
			Valid frame cnt
		*/
		inline int FrameCount() const { return _frmCnt; }

		/*
			Only valid when frame count > 0
		*/
		inline uint16_t FrameIdBegin() const { return _fidBegin; }
		inline uint16_t FrameIdEnd() const { return _fidEnd; }

	private:
		void FreeSlotForNewFrame(uint16_t fid, VisitorFunc efunc, void *tag) 
        {
			// before inserting the fid from tail, make sure the free slot for it
			// and dequeue/erase the first N frames if the queue is full
			Assert(_frmCnt > 0);
            Assert(_frmCnt <= _queue.Count());
			Assert(FrameIdComparer::IsLater(fid, _fidEnd));
			// eat from begin until there is room for new fid
			for (;;) {
				Slot *slot = reinterpret_cast<Slot*>(_queue.FirstSlot());
				if (slot == nullptr) {
					Assert(_frmCnt == 0);
                    _fidBegin = _fidEnd = 0;
					break;
				}

				if (slot->empty) {
					_queue.EraseFirstNSlot(1);
					continue;
				}

                if (FrameIdComparer::Distance(fid, slot->fid) >= N) {
					if (efunc != nullptr) {
						efunc(slot, slot->fid, tag);
					}

					_queue.EraseFirstNSlot(1);
                    if (--_frmCnt == 0) {
                        Assert(_queue.FirstSlot() == nullptr);
                    }
					continue;
				}

				_fidBegin = slot->fid;
				break;
			}
		}

		uint16_t _fidBegin;
		uint16_t _fidEnd;

		// the first slot in queue must be a valid frame if _frmCnt > 0 !!
		FixedSizeCircleQueue<N, SlotSize> _queue;
		int _frmCnt;
	};
}


#endif
