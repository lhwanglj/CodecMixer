
#ifndef _MEDIACLOUD_BUFFER_H
#define _MEDIACLOUD_BUFFER_H
#include <list>
namespace MediaCloud
{
    namespace Common
    {
        void* AllocBuffer(unsigned int size, bool clear = false, int alignment = 8);
        void FreeBuffer(void *buffer);
        
        /// used to help fast allocate the fixed size buffer
        class BufferCache
        {
        public:
            static BufferCache* Create(int size);

            void *Allocate(bool clear = false, int alignment = 8);
            void Free(void *buffer);

            int BufferSize() const;
            ~BufferCache();
        protected:
             BufferCache(int size);
             BufferCache& operator=(const BufferCache& buf);
        private:
            BufferCache();
            int _bufSize;
        };

        class RefBuffer
        {
        public:
            RefBuffer(const RefBuffer &refbuf);
            ~RefBuffer();

            RefBuffer& operator=(const RefBuffer& refbuf);

            int AddRef();   /// return the previous ref count

            /// call this to release the RefBuffer instance
            /// return true if the buffer pointer can't be used anymore
            bool ReleaseRef();

            int Size() const;
            void *Buffer() const;

        private:
            RefBuffer();
        };

        /// a buffer with reference count
        class RefBufferCache
        {
        public:

            static RefBufferCache* Create(int size);

            /// create a ref buffer with any size
            static RefBuffer Allocate(int size, bool clear = false);

            /// the returned buffer have reference count 1
            RefBuffer Allocate(bool clear = false);
        };
    }
}

#endif