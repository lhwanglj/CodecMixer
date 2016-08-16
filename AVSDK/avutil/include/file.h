
#ifndef _MEDIACLOUD_FILE_H
#define _MEDIACLOUD_FILE_H

#include <stdio.h>
#include <stdint.h>

namespace MediaCloud
{
    namespace Common
    {
        class SimpleFile
        {
        public:
            static SimpleFile* OpenForWrite(const char *filename);
            static SimpleFile* OpenForRead(const char *filename);
            static void Close(SimpleFile *file);
            void Flush();

            unsigned int GetSize() const;
            unsigned int GetOffset() const;
            int SetOffset(unsigned int offset);

            int Write(void *buffer, int length);
            int Read(void *buffer, int length);

            static int Rename(const char *srcFilename, const char *dstFilename);
            static int Delete(const char *filename);
            
            struct Stat
            {
                unsigned int size;
                unsigned long long createTime;
                unsigned long long lastModifiedTime;
            };
            static Stat GetStat(const char *filename);

            static int EnumFilesInFolder(const char *foldername, void(*filecb)(const char *filename));
            static const char* GetFilename(const char *path);
            static const char* GetExtensionName(const char *path);

        private:
            FILE *_file;
        };
    }
}

#endif  // WEBRTC_BASE_MD5_H_
