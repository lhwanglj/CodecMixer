
#ifndef _MEDIACLOUD_JITTER_IF_H
#define _MEDIACLOUD_JITTER_IF_H

#include <vector>
#include <stdint.h>
#include "public_api.h"
#include <avutil/include/common.h>
using namespace MediaCloud::Common;

namespace MComp {

	/* DON'T change the enum values */
	enum {
		SESSION_STREAM_AUDIO = 0,
		SESSION_STREAM_THUMBNAIL_VIDEO = 1,
		SESSION_STREAM_LIVE_VIDEO = 2,
		SESSION_STREAM_MAX = 3,
		SESSION_STREAM_ALL = 3
	};

    enum {
        VIDEO_I_FRAME,
        VIDEO_P_FRAME,
        VIDEO_B_FRAME,
        VIDEO_LP_FRAME,
    };

    struct FrameIDs {
        uint16_t idOfLiveVideo;
        uint16_t idOfThumbnailVideo;
        uint16_t idOfAudio;
    };

	// for uploading
	struct JitterFrameInfo {
        enum {
        //  FLAG_GOP_END = 1, // remove this, because it is never known for dynamic gop
            FLAG_GOP_BEGIN = 2,
			FLAG_SWITCH_SOURCE = 4	// set for the first I frame after switching camera 
        };

        int flag;
		int streamType;	// audio, thumbnail or live video
		int frameType;	// I/P/B frame
		BUF_ARG_REF uint8_t *payload;
		uint32_t payloadSize;

        Clock::Tick ts;
        void *fecGen;
	};

	// for receiving
	struct SessionFrameInfo {
		Identity identity;
		int streamType;
		int frameType;
		uint16_t frameId;
        BUF_ARG_MOVE const uint8_t *payload;
		uint32_t payloadSize;
	};
}

#endif