#include "misc.h"
#include "logging.h"
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <signal.h>

using namespace MediaCloud::Common;

namespace cppcmn {
   
    bool StartProcess(const char *binaryPath, const char *cmdline[])
    {
        int fret = fork();
        if (fret == -1) {
            return false;
        }
        
        if (fret != 0) {
            // ignore the child process exit signal for avoding the child zombie status
            signal(SIGCHLD, SIG_IGN);
            // parent
            LogInfo("misc.c", "start process - child pid: %d, ppid: %d\n", fret, getpid());
            return true;
        }
        
        execv(binaryPath, (char* const *)cmdline);
        exit(0);
    }
    
    uint64_t GetProcessCpuAffinity()
    {
        cpu_set_t set;
        CPU_ZERO(&set);
        if (0 != sched_getaffinity(0, sizeof(set), &set)) {
            LogWarning("misc.c","failed to get process cpuaffinity err = %d\n", errno);
            return 0;
        }
        
        uint64_t mask = 0;
        for (int i = 0; i < 64; i++) {
            if (CPU_ISSET(i, &set)) {
                mask |= 1llu << i;
            }
        }
        LogInfo("misc.c","getting process cpuaffinity = %llu\n", mask);
        return mask;
    }
    
    struct StartThreadParam {
        void (*func) (void*);
        void *param;
    };
    
    static void* StartThreadEntrance(void *param)
    {
        StartThreadParam *p = reinterpret_cast<StartThreadParam*>(param);
        p->func(p->param);
        delete p;
        return nullptr;
    }
    
    void StartThread(void (*func) (void*), void* param)
    {
        StartThreadParam *p = new StartThreadParam();
        p->func = func;
        p->param = param;
        
        pthread_t tid;
        int ret = pthread_create(&tid, nullptr, StartThreadEntrance, p);
        Assert(ret == 0);
    }

    void SetCurrentThreadCpuAffinity(uint64_t cpumask)
    {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (int i = 0; i < 64; i++) {
            if (cpumask & (1llu << i)) {
                CPU_SET(i, &cpuset);
            }
        }
        
        pthread_t tid = pthread_self();
        if (pthread_setaffinity_np(tid, sizeof(cpuset), &cpuset) == -1) {
            LogWarning("misc.c","set curthread affinity failed, mask = %llu, tid = %u\n", cpumask, (uint32_t)tid);
        }
        else {
            LogDebug("misc.c","set curthread affinity mask = %llu, tid = %u\n", cpumask, (uint32_t)tid);
        }
    }
}


