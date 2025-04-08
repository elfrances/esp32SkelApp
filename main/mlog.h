#pragma once

#include <sys/cdefs.h>
#include <stdbool.h>
#include <errno.h>

// These macros are used to set the foreground color highlighting
#define BLACK_FGC   "\033[0m"
#define RED_FGC     "\033[0;31m"
#define GREEN_FGC   "\033[0;32m"
#define YELLOW_FGC  "\033[0;33m"
#define BLUE_FGC    "\033[0;34m"
#define MAGENTA_FGC "\033[0;35m"
#define CYAN_FGC    "\033[0;36m"
#define WHITE_FGC   "\033[0;37m"
#define ORANGE_FGC  "\033[38;5;208m"
#define RESET_FGC   "\033[0m"

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
