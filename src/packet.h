#ifndef _HPSP_PACKET_H
#define _HPSP_PACKET_H

#include <stdint.h>
#include <string>
#include <vector>
#include "userproto.h"
#include "netendian.h"
#include "clockex.h"

/*
quic packet format:
| 3bit version | 5bit packtype | 8bit flags | identity (optional) | pn (2,4,8 bytes) | N slices |

quic 8bit flags : 2bit for size of pn 2, 4, 8; 1bit for optional identity
*/

/*
* redefine the udp packet format:
*	| 32bit packet number | N frames |
*	frame: | 8bit type | 5bit flag | 11bit payload length | payload |
* 
*	8bit type:	1doossff	- stream frame (user data) | 8bit type | 32 bit identity (optional) | vlu payload len | payload |
*
*	stream type 1doossff meanings:
*		 d bit - always 0 for client -> server; for server -> client: 0 - following the previous stream frame identity; 1 - has identity
*		oo bit - layer : 0 - audio, 1 - thumbnail video, 2 - live video
*		ss bit - frame type of video : 0 - I frame, 1 - P frame, 2 - long P frame, 3 - B frame
*		ff bit - 0 - no fec; 1 - fec has pattern prefix; 2 - fec with symbol size = 8; 3 - fec with symbol size = 16;
*	audio/video non fec payload:
*		| 8bit header | 16bit stream frame len | 32bit identity (optional) | 16bit frame id | payload |
*	video fec stream frame:
*		| 8bit header | 16bit stream frame len | 32bit identity (optional) | 8bit symbol size (optional) |
*		| 16bit frame id | 16bit frame length | N * symbol segments (16bit offset + 16bit cnt + cnt * symbols) |
*/

namespace hpsp {
    
#define MAX_SLICES_NUM_OF_QPACKET   20
#define InvalidIdentity 0

    /* DON'T change the enum values */
    enum {
        SESSION_STREAM_AUDIO = 0,
        SESSION_STREAM_THUMB_VIDEO = 1,
        SESSION_STREAM_LIVE_VIDEO = 2,
    };

    enum {
        VIDEO_I_FRAME,
        VIDEO_P_FRAME,
        VIDEO_B_FRAME,
        VIDEO_LP_FRAME,
    };

    enum class UserPacketType { // 5bit packtype
        Quic = 0,
    };

    struct PacketPreviewInfo {
        UserPacketType type;
        int hdrlen; // for quic, end after pn; otherwise end after identity
        uint64_t pn;
        uint32_t identity;
    };

    // this is an info wrapper for a stream packet,
    // used to pass between stream handlers
    struct StreamPacketDesc {
        struct Slice {
            uint8_t type;
            uint8_t flag;           // copied of highest 5 flags of payloadlen
            uint16_t payloadLen;    // payload length
            uint16_t offset;        // slice start at 8bit type in packet
        };

        int hdrlen;
        uint64_t pn;
        uint32_t identity;

        Slice slices[MAX_SLICES_NUM_OF_QPACKET];
        int sliceNum;
    };

    struct StreamFrameSliceDetail {
        uint8_t stmType;
        uint8_t frmType;
        uint16_t fid;
        uint16_t frmLen;
        uint16_t symSize;    // 0 - non-fec
        uint16_t symNum;
        uint16_t symESI;
        uint16_t payloadLen;
        const uint8_t *payload;    // pointing to the segment raw data
    };

    struct PacketUtils {
        static bool PreVerify(const void *packet, int packlen, PacketPreviewInfo &previewInfo)
        {
            if ((*(uint8_t*)packet & 0xE0) != 0) {
                return false;   // 3bit version must be 0
            }
            
            int packtype = *(uint8_t*)packet & 0x1F;
            if (packtype != (int)UserPacketType::Quic) {
                return false;   // unknown packet type
            }
            previewInfo.type = (UserPacketType)packtype;

            uint8_t flag = *((uint8_t*)packet + 1);
            if ((flag & (1 << 2)) == 0) {
                return false;   // no identity after pn
            }
            
            previewInfo.identity = cppcmn::byte_to_u32((uint8_t*)packet + 2);
            if (previewInfo.identity == InvalidIdentity) {
                return false;
            }
            
            previewInfo.hdrlen = 6;

            // get pn
            uint8_t pnBytes = 2 << (flag & 3);
            if (pnBytes > 8) {
                return false;   // pn bytes flag error
            }

            if (pnBytes == 2) {
                previewInfo.pn = cppcmn::byte_to_u16((uint8_t*)packet + 6);
            }
            else if (pnBytes == 4) {
                previewInfo.pn = cppcmn::byte_to_u32((uint8_t*)packet + 6);
            }
            else if (pnBytes == 8) {
                previewInfo.pn = cppcmn::byte_to_u64((uint8_t*)packet + 6);
            }

            previewInfo.hdrlen += pnBytes;
            return true;
        }
        
        static bool GenStreamPacketDesc(const PacketPreviewInfo &preinfo,
            const void *packet, int packlen, StreamPacketDesc &desc) 
        {
            desc.hdrlen = preinfo.hdrlen;
            desc.pn = preinfo.pn;
            desc.identity = preinfo.identity;
            desc.sliceNum = 0;

            // go throught the slice
            uint8_t *ptr = (uint8_t*)packet + desc.hdrlen;
            uint8_t *end = (uint8_t*)packet + packlen;
            for (;;) {
                if (ptr + 3 > end) {
                    return false;
                }
                desc.slices[desc.sliceNum].offset = ptr - (uint8_t*)packet;
                desc.slices[desc.sliceNum].type = *ptr;
                desc.slices[desc.sliceNum].flag = ptr[1] & 0xF8;
                desc.slices[desc.sliceNum].payloadLen = (uint16_t)(ptr[1] & 7) << 8 | ptr[2];
                ptr += desc.slices[desc.sliceNum].payloadLen + 3;
                if (ptr > end) {
                    return false;
                }
                if (++desc.sliceNum >= MAX_SLICES_NUM_OF_QPACKET) {
                    if (ptr != end) {
                        return false;
                    }
                    break;
                }
                if (ptr == end) {
                    break;
                }
            }
            return true;
        }

        static bool GenStreamFrameSliceDetail(const uint8_t *packet, const StreamPacketDesc &stmDesc, int sliceIdx,
            StreamFrameSliceDetail &detail)
        {
            const uint8_t *ptr = packet + stmDesc.slices[sliceIdx].offset;
            const uint8_t *end = ptr + stmDesc.slices[sliceIdx].payloadLen + 3;
            
            detail.stmType = (ptr[0] >> 4) & 3;
            if (detail.stmType >= 3) {
                return false;
            }

            detail.frmType = (ptr[0] >> 2) & 3;
            
            uint8_t fecFlag = ptr[0] & 3;
            
            if ((ptr[0] & (1 << 6)) != 0) {
                return false;   // identity shouldn't be there
            }
            else {
                ptr += 3;
            }
            
            if (fecFlag == 0) { // non-fec
                if (end - ptr <= 2) {
                    return false;   // no payload
                }
            
                detail.symSize = 0;
                detail.symNum = 0;
                detail.symESI = 0xFFFF; // for detecting error
                detail.fid = cppcmn::byte_to_u16(ptr);  ptr += 2;
                detail.frmLen = end - ptr;
                detail.payload = ptr;
                detail.payloadLen = end - ptr;
                return true;
            }
            
            if (fecFlag != 1) {
                return false;   // not supported at first version
            }

            // | 8bit symbol size | 16bit frame id | 16bit frame length | 16bit offset + 16bit cnt + cnt * symbols |
            // from client to server, the segment must be only one
            if (end - ptr <= 9) {
                return false;
            }

            detail.symSize = *ptr;                      ptr += 1;
            detail.fid = cppcmn::byte_to_u16(ptr);      ptr += 2;
            detail.frmLen = cppcmn::byte_to_u16(ptr);   ptr += 2;
            detail.symESI = cppcmn::byte_to_u16(ptr);   ptr += 2;
            detail.symNum = cppcmn::byte_to_u16(ptr);   ptr += 2;
            detail.payload = ptr;
            detail.payloadLen = end - ptr;
            
            if (detail.payloadLen != detail.symSize * detail.symNum) {
                return false;
            }
            return true;
        }
        
        static inline bool IsStreamFrameSlice(uint8_t sliceType)
        {
            return (sliceType & (1 << 7)) != 0;
        }
    };
}

#endif // !_HPSP_PACKET_H
