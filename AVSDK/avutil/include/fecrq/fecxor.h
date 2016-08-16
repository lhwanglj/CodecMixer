
#ifndef _MEDIACLOUD_FECXOR_H
#define _MEDIACLOUD_FECXOR_H

#include <stdint.h>
#include <vector>

namespace MediaCloud
{
    namespace Common
    {
        class FECXOR
        {
        public:

            /// xor the input symbols with differ length, and returning the result heading with 16bit xored length
            static uint16_t Encoding(const std::pair<uint8_t*, uint16_t> sourceSymbols[], 
                                int symbolNum, 
                                uint8_t repairedSymbol[]);

            static uint16_t Decoding(const std::pair<uint8_t*, uint16_t> sourceSymbols[],
                                int symbolNum,
                                const std::pair<uint8_t*, uint16_t> repairedSymbol,
                                uint8_t recoveredSymbol[]);
        };
    }
}

#endif
