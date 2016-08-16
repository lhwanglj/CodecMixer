
#ifndef _MEDIACLOUD_PUBLIC_API_H
#define _MEDIACLOUD_PUBLIC_API_H

#include <map>
#include <string>
#include <stdint.h>

namespace MComp {

	/// Media cloud use 32bit to identify a user and media room in the scope of developer GUID
	typedef uint32_t Identity;
#define InvalidIdentity (0)

	class PublicAPI {
	public:
		class IDelegate {
		public:
			virtual ~IDelegate() {}

            // for reader
			enum { // the value must be consistent with HPSPReaderMsgType
				SESSION_JOINING = 1,
				SESSION_JOINED,
				PEER_ALIVE,	// evtParam - peer identity
				PEER_LEFT,
                STREAM_END,
			};
			virtual void OnSessionEvent(int evtType, int64_t evtParam) = 0;
            virtual void OnSessionStream(void *sessionFrameInfo) = 0;

            // for writer
            virtual bool OnSessionStreamBitrateAdjust(bool increase, uint32_t targetBitrate) = 0;
		};

		enum {
			SUBSCRIBE_LIVE_VIDEO = 1, // identity > 0 - subscribe live video; identity == 0 - unsubscribe
			UPLOAD_CONTROL,	// allow video or audio upload, allowed by default; param - high 16bit for video; low 16bit for audio
		};
	};
}

#endif