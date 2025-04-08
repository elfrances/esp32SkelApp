#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "esp32.h"
#include "led.h"
#include "mlog.h"

#define USE_MICROSECOND_RESOLUTION  0

#if !USE_MICROSECOND_RESOLUTION
static __inline__ unsigned int usToMs(unsigned int us)
{
    unsigned int ms = us / 1000;
    if ((us % 1000) >= 500)
        ms++;
    return ms;
}
#endif

static const char *logLevelName[] = {
    [none] = "",
    [info] = "INFO",
    [dump] = "DUMP",
    [trace] = GREEN_FGC "TRACE" RESET_FGC,
    [debug] = CYAN_FGC "DEBUG" RESET_FGC,
    [warning] = YELLOW_FGC "WARNING" RESET_FGC,
    [error] = RED_FGC "ERROR" RESET_FGC,
    [errNo] = RED_FGC "ERROR" RESET_FGC,
    [fatal] = RED_FGC "FATAL" RESET_FGC,
};

static bool usTimestamp = false;
static int8_t utcOffset;
static LogDest msgLogDest = console;
static LogLevel msgLogLevel = info;
static FILE *logFile = NULL;
static SemaphoreHandle_t mutexHandle;
static StaticSemaphore_t mutexSem;

static const char *fmtTimestamp(void)
{
    struct timeval now;
    struct tm brkDwnTime;
    static char tsBuf[32];  // YYYY-MM-DDTHH:MM:SS.xxxxxx
    size_t bufLen = sizeof (tsBuf);
    int n;

    gettimeofday(&now, NULL);
    now.tv_sec += utcOffset * 3600; // adjust based on UTC offset
    if (usTimestamp) {
        n = strftime(tsBuf, bufLen, "%Y-%m-%d %H:%M:%S", gmtime_r(&now.tv_sec, &brkDwnTime));    // %H means 24-hour time
        snprintf((tsBuf + n), (bufLen - n), ".%06u", (unsigned) now.tv_usec);
    } else {
        unsigned int ms = usToMs(now.tv_usec);
        if (ms >= 1000) {
            now.tv_sec += 1;
            ms -= 1000;
        }
        n = strftime(tsBuf, bufLen, "%Y-%m-%d %H:%M:%S", gmtime_r(&now.tv_sec, &brkDwnTime));    // %H means 24-hour time
        snprintf((tsBuf + n), (bufLen - n), ".%03u", ms);
    }

    return tsBuf;
}

static char msgLogBuf[1024];

void msgLog(LogLevel logLevel, const char *funcName, int lineNum, int errorNum, const char *fmt, ...)
{
    // Everything at or above "warning" is
    // always printed...
    if ((logLevel <= msgLogLevel) || (logLevel >= warning)) {
        char *p = msgLogBuf;
        int len = sizeof (msgLogBuf);
        int n = 0;
        va_list ap;

        xSemaphoreTake(mutexHandle, portMAX_DELAY);

        n += snprintf((p + n), (len - n), "%s %s ", fmtTimestamp(), logLevelName[logLevel]);

        if (logLevel >= trace) {
            n += snprintf((p + n), (len - n), "%s@%u:%s:%d", pcTaskGetName(NULL), esp_cpu_get_core_id(), funcName, lineNum);
        }
        va_start(ap, fmt);
        n += vsnprintf((p + n), (len - n), fmt, ap);
        va_end(ap);
        if (((logLevel == errNo) || (logLevel == fatal)) && (errorNum != 0)) {
            n += snprintf((p + n), (len - n), " errno=%d (%s)", errorNum, strerror(errorNum));
        }

        if ((msgLogDest == both) || (msgLogDest == console)) {
            fprintf(stdout, "%s\n", msgLogBuf);
        }
        if ((msgLogDest == both) || (msgLogDest == file)) {
            fprintf(logFile, "%s\n", msgLogBuf);
        }

        if (logLevel == fatal) {
            ledSet(on, red);
        }

        xSemaphoreGive(mutexHandle);
    }
}

int msgLogInit(bool usTimestamp)
{
    if ((mutexHandle = xSemaphoreCreateMutexStatic(&mutexSem)) == NULL) {
        // Hu?
        return -1;
    }

    // Reset the terminal's foreground color highlighting
    fprintf(stdout, "%s\n", RESET_FGC);

    mlog(info, "Message logging enabled");

    return 0;
}

LogDest msgLogSetDest(LogDest logDest)
{
    LogDest prevLogDest = msgLogDest;
    if ((msgLogDest = logDest) != prevLogDest) {
        if ((msgLogDest == console) && (logFile != NULL)) {
            fclose(logFile);
            logFile = NULL;
        } else if (prevLogDest == console) {
            time_t now = time(NULL);
            struct tm brkDwnTime;
            char logFileName[128];
            // Log file name: "YYYY-MM-DDTHH:MM:SS.txt"
            strftime(logFileName, sizeof (logFileName), "%Y-%m-%dT%H:%M:%S.txt", gmtime_r(&now, &brkDwnTime));
            logFile = fopen(logFileName, "w");
        }
    }
    return prevLogDest;
}

LogLevel msgLogSetLevel(LogLevel logLevel)
{
    LogLevel prevLogLevel = msgLogLevel;
    if (logLevel != prevLogLevel) {
        mlog(info, "New message logging level is %s", logLevelName[logLevel]);
        msgLogLevel = logLevel;
    }
    return prevLogLevel;
}

LogLevel msgLogGetLevel(void)
{
    return msgLogLevel;
}
