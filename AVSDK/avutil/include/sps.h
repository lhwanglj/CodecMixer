#ifndef __SPS_H_
#define __SPS_H_

namespace MediaCloud
{
    namespace Common
    {
        bool h264_decode_seq_parameter_set(unsigned char * buf, unsigned int nLen, unsigned int &Width, unsigned int &Height);

    }
}
#endif