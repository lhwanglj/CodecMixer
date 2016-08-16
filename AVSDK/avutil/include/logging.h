
#ifndef _MEDIACLOUD_LOGGING_H
#define _MEDIACLOUD_LOGGING_H

#include <string>
#include <string.h>
#include <config.h>

namespace MediaCloud { namespace Common {
    enum LogLevel
    {
        LogLevelVerbose,
        LogLevelDebug,
        LogLevelInfo,
        LogLevelWarning,
        LogLevelError,
        LogLevelAssert,
    };
        
    typedef void (*LogFun)(LogLevel level, const char* tag, const char *text);
    void ConfigLog(LogFun logFun, int nLevel, const char *forbidModule = nullptr, const char *forceModule = nullptr);
    void LogHelper(LogLevel level, const char *module, const char *format, ...);
    void AssertHelper(bool condition, const char *file, const char *func, int line, const char *format, ...);

#ifndef LOGGING_IMPL
    extern char g_forbidModule[64];
    extern char g_forceModule[64];
    extern int g_nLogLevel;
    __inline bool CheckLogFilter(LogLevel level, const char *module) {
        bool output = level >= g_nLogLevel;
        if (output && g_forbidModule[0] != '0' && module != nullptr) {
            if (strcmp(g_forbidModule, module) == 0) {
                output = false;
            }
        }

        if (!output && g_forceModule[0] != '0' && module != nullptr) {
            if (strcmp(g_forceModule, module) == 0) {
                output = true;
            }
        }
        return output;
    }
#endif

#ifdef WIN32
#define Assert(e)  do { if (!(e)) { AssertHelper(e, __FILE__, __FUNCTION__, __LINE__, ""); } } while(0)
#define AssertMsg(e, format, ...)  do { if (!(e)) { AssertHelper(e, __FILE__, __FUNCTION__, __LINE__, format, __VA_ARGS__); } } while(0)
#define LogVerbose(module, format, ...) do { if (CheckLogFilter(LogLevelVerbose, module)) { LogHelper(LogLevelVerbose, module, format, __VA_ARGS__); } } while (0)
#define LogDebug(module, format, ...) do { if (CheckLogFilter(LogLevelDebug, module)) { LogHelper(LogLevelDebug, module, format, __VA_ARGS__); } } while (0)
#define LogInfo(module, format, ...) do { if (CheckLogFilter(LogLevelInfo, module)) { LogHelper(LogLevelInfo, module, format, __VA_ARGS__); } } while (0)
#define LogWarning(module, format, ...) do { if (CheckLogFilter(LogLevelWarning, module)) { LogHelper(LogLevelWarning, module, format, __VA_ARGS__); } } while (0)
#define LogError(module, format, ...) do { if (CheckLogFilter(LogLevelError, module)) { LogHelper(LogLevelError, module, format, __VA_ARGS__); } } while (0)
        
#else
#define Assert(e)  do { if (!(e)) { AssertHelper(e, __FILE__, __func__, __LINE__, ""); } } while(0)
#define AssertMsg(e, format, args...)  do { if (!(e)) { AssertHelper(e, __FILE__, __func__, __LINE__, format, ## args); } } while(0)
#define LogVerbose(module, format, args...) do { if (CheckLogFilter(LogLevelVerbose, module)) { LogHelper(LogLevelVerbose, module, format, ## args); } } while (0)
#define LogDebug(module, format, args...) do { if (CheckLogFilter(LogLevelDebug, module)) { LogHelper(LogLevelDebug, module, format, ## args); } } while (0)
#define LogInfo(module, format, args...) do { if (CheckLogFilter(LogLevelInfo, module)) { LogHelper(LogLevelInfo, module, format, ## args); } } while (0)
#define LogWarning(module, format, args...) do { if (CheckLogFilter(LogLevelWarning, module)) { LogHelper(LogLevelWarning, module, format, ## args); } } while (0)
#define LogError(module, format, args...) do { if (CheckLogFilter(LogLevelError, module)) { LogHelper(LogLevelError, module, format, ## args); } } while (0)
#endif

}}

#endif
