#pragma once

#include <sys/cdefs.h>
#include <stdbool.h>
#include <errno.h>

typedef enum LogDest {
    undef = 0,
    both = 1,
    console = 2,
    file = 3,
} LogDest;

typedef enum LogLevel {
    none = 0,
    info,
    dump,
    trace,
    debug,
    warning,
    error,
    errNo,  // same as 'error' but includes the errno value and string as well
    fatal
} LogLevel;

// This macro is used to pick up the file name, line number,
// and errno value from where msgLog() is being called.
#define mlog(lvl, fmt, args...) msgLog((lvl), __func__, __LINE__, errno, (fmt), ##args)

// Shorthand for mlog(debug, ...) that can be easily compiled out
#define mlogDbg(fmt, args...) msgLog(debug, __func__, __LINE__, errno, (fmt), ##args)

__BEGIN_DECLS

extern void msgLog(LogLevel logLevel, const char *funcName, int lineNum, int errorNum, const char *fmt, ...)  __attribute__ ((__format__ (__printf__, 5, 6)));

extern int msgLogInit(bool usTimestamp);
extern LogDest msgLogSetDest(LogDest logDest);
extern LogLevel msgLogSetLevel(LogLevel logLevel);
extern LogLevel msgLogGetLevel(void);

__END_DECLS
