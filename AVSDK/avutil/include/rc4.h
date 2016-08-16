
#ifndef _MEDIACLOUD_RC4_H
#define _MEDIACLOUD_RC4_H

#include <stdint.h>

namespace MediaCloud
{
    namespace Common
    {
        class RC4
        {
        public:
            ~RC4() {}

            static RC4* Create(const uint8_t cipher[], int length);

            void Process(const uint8_t *indata, uint8_t *outdata, int length);
            void SetXY(uint8_t x, uint8_t y);

        private:
            RC4() {}
            
            uint8_t _data[256];
            uint8_t _x, _y;
        };
    }
}

#endif