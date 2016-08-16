
#ifndef _MEDIACLOUD_RSA_H
#define _MEDIACLOUD_RSA_H

namespace MediaCloud
{
    namespace Common
    {
        class RSAHelper
        {
        public:
            static RSAHelper* GetSingleton();

            virtual unsigned int GetLengthOfN() const = 0;
            virtual unsigned int GetLengthOfE() const = 0;
            virtual int GetN(void *buffer, unsigned int length) const = 0;
            virtual int GetE(void *buffer, unsigned int length) const = 0;
            virtual int Decrypt(void *indata, unsigned int inLength, void *outData, unsigned int outLength) = 0;

        protected:
            virtual ~RSAHelper();
        };
    }
}


#endif