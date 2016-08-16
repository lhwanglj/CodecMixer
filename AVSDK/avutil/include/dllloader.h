#ifndef _MEDIACLOUD_DLLLOADER_H
#define _MEDIACLOUD_DLLLOADER_H

#ifndef WIN32
#include <dlfcn.h>
#include <errno.h>
#include "logging.h"

namespace MediaCloud
{
    namespace Common
    {
        __inline void* LoadLibrary(char* fileName)
        {
            void* h = (void*)dlopen( fileName,  RTLD_LAZY );
            if (h != NULL)
                return h;
            
            LogError("Uitls","LoadLibrary %s err, errno %d", fileName, errno);
            return NULL;
        }
        
        __inline bool FreeLibrary(void* module)
        {
            bool ret = dlclose(module) == 0;
            if (ret)
                return true;
            
            return false;
        }
        
        __inline void* GetProcAddress(void* module, char* procName)
        {
            void* procAddr = (void*)dlsym(module, procName);
            if (procAddr != NULL)
                return procAddr;
            
            return NULL;
        }
        
    }
}
#endif

#endif//_DLLLOADER_H
