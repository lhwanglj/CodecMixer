//
//  thread.h
//  mediacloud
//
//  Created by xingyanjun on 14/12/14.
//  Copyright (c) 2014年 smyk. All rights reserved.
//

#ifndef mediacloud_thread_h
#define mediacloud_thread_h

#include <stdio.h>
#include <stdint.h>

namespace MediaCloud
{
    namespace Common
    {
        typedef bool(*ThreadRunFunction)(void*);
        
        enum ThreadPrior
        {
            lowPrio      = 1,
            normalPrio   = 2,
            highPrio     = 3,
            highestPrio  = 4,
            realtimePrio = 5
        };
        
        class GeneralThread
        {
        public:
            enum {threadMaxNameLen = 64};
            
            virtual ~GeneralThread(){};
            
            static GeneralThread* Create(ThreadRunFunction func, void* obj, bool selfdelete = false, ThreadPrior prio = normalPrio, const char* thread_name = 0);
            static int            Release(GeneralThread* mediathr);
            
            virtual bool Start() = 0;
            virtual bool Stop() = 0;
        };

        unsigned long GetThreadId();

        /// sleep current thread for a while.
        /// ms - zero : give up the current CPU slide. 
        void ThreadSleep(unsigned int ms);

        void SetThreadName(char* name);
        void GetThreadName(char* name, int bufsize);
    }
}
#endif
