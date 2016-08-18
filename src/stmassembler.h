#ifndef _HPSP_STMASSEMBLER_H
#define _HPSP_STMASSEMBLER_H

#include <stdint.h>
#include "clock.h"

namespace hpsp {
    
    /*
     *  StmAssembler handles the stream packet from rgrid, and assembly the frame segments into frames
     *  StmAssembler should be instanced for each session id
     *  It doesn't support concurrent calling for now
     */ 
    class StmAssembler {
    public:
        struct Frame {
            uint32_t identity;
            uint16_t fid;
            // 0 - audio stream, 2 - video stream
            int stmtype;
            // 0 - I frame, 1 - P frame, 2 - B frame
            int frmtype;
            int length;
            const uint8_t *payload;
        };

        struct IDelegate {
            virtual ~IDelegate() {}
            
            // the caller can save the pointer frame.payload, 
            // but it must be released by ReleaseFramePayload
            //
            // NOTICE:
            // the calling thread may deadlock if deleting StmAssembler in the callback of HandleRGridFrameRecved !!
            virtual void HandleRGridFrameRecved(const Frame &frame) = 0;
        };
        
        StmAssembler(IDelegate *delegate);
        virtual ~StmAssembler();

        /*
         *  The data is the body part of the grid stream packet
         */ 
        void HandleRGridStream(const uint8_t *data, int length, cppcmn::Tick tick);
        
        static void ReleaseFramePayload(const void *payload);
        
    private:
        int _assemblerId;
        IDelegate *_delegate;
    };
}

#endif
