#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "app.h"
#include "esp32.h"
#include "fgc.h"
#include "led.h"
#include "mlog.h"
#include "timeval.h"

#ifdef CONFIG_MSG_LOG

static const char *logLevelName[] = {
    [none] = "NONE",
    [info] = "INFO",
    [trace] = GREEN_FGC "TRACE" RESET_FGC,
    [debug] = CYAN_FGC "DEBUG" RESET_FGC,
    [warning] = YELLOW_FGC "WARNING" RESET_FGC,
    [error] = RED_FGC "ERROR" RESET_FGC,
    [errNo] = RED_FGC "ERROR" RESET_FGC,
    [fatal] = RED_FGC "FATAL" RESET_FGC,
};

static const char *logDestName[] = {
    [undef] = "UNDEF",
    [both] = "BOTH",
    [console] = "CONSOLE",
    [file] = "FILE",
};

static LogDest msgLogDest = console;
static LogLevel msgLogLevel = trace;
static SemaphoreHandle_t mutexHandle;
static StaticSemaphore_t mutexSem;

typedef struct TsBuf {
    char buf[32]; // big enough for: "YYYY-MM-DD HH:MM:SS.xxxxxx"
} TsBuf;

#if CONFIG_MSG_LOG_TS_UPTIME_USEC
static const char *fmtTimestamp(TsBuf *tsBuf)
{
    struct timeval now, deltaT;
    unsigned dd, hh, mm, ss, us;
    size_t bufLen = sizeof (TsBuf);

    gettimeofday(&now, NULL);
    tvSub(&deltaT, &now, &baseTime);
    ss = deltaT.tv_sec;
    dd = ss / 86400;
    ss -= dd * 86400;
    hh = ss / 3600;
    ss -= hh * 3600;
    mm = ss / 60;
    ss -= mm * 60;
    us = deltaT.tv_usec;
    snprintf(tsBuf->buf, bufLen, "%02u %02u:%02u:%02u.%06u", dd, hh, mm, ss, us);

    return tsBuf->buf;
}
#elif CONFIG_MSG_LOG_TS_UPTIME_MSEC
static const char *fmtTimestamp(TsBuf *tsBuf)
{
    TickType_t now = xTaskGetTickCount();
    unsigned ticks = now - baseTicks;
    unsigned dd, hh, mm, ss, ms;
    size_t bufLen = sizeof (TsBuf);

    ss = pdTICKS_TO_MS(ticks) / 1000;
    dd = ss / 86400;
    ss -= dd * 86400;
    hh = ss / 3600;
    ss -= hh * 3600;
    mm = ss / 60;
    ss -= mm * 60;
    ms = pdTICKS_TO_MS(ticks) % 1000;
    snprintf(tsBuf->buf, bufLen, "%02u %02u:%02u:%02u.%03u", dd, hh, mm, ss, ms);

    return tsBuf->buf;
}
#else
static const char *fmtTimestamp(TsBuf *tsBuf)
{
    struct timeval now;
    struct tm brkDwnTime;
    size_t bufLen = sizeof (TsBuf);
    int n;

    gettimeofday(&now, NULL);
    now.tv_sec += appConfigInfo.utcOffset * 3600;   // adjust based on UTC offset
    n = strftime(tsBuf->buf, bufLen, "%Y-%m-%d %H:%M:%S", gmtime_r(&now.tv_sec, &brkDwnTime));    // %H means 24-hour time
#if CONFIG_MSG_LOG_TS_TOD_USEC
    snprintf((tsBuf->buf + n), (bufLen - n), ".%06u", (unsigned) now.tv_usec);
#else
   unsigned int ms = now.tv_usec / 1000;
   if (ms >= 1000) {
       now.tv_sec += 1;
       ms -= 1000;
    }
    snprintf((tsBuf->buf + n), (bufLen - n), ".%03u", ms);
#endif

    return tsBuf->buf;
}
#endif

static char msgLogBuf[CONFIG_MSG_LOG_MAX_LEN];

void msgLog(LogLevel logLevel, const char *funcName, int lineNum, int errorNum, const char *fmt, ...)
{
    if (msgLogLevel == none) {
        // Nothing to do!
    }

    // Everything at or above "warning" is
    // always printed...
    if ((logLevel <= msgLogLevel) || (logLevel >= warning)) {
        TsBuf tsBuf;
        char *p = msgLogBuf;
        int len = sizeof (msgLogBuf);
        int n = 0;
        va_list ap;

        xSemaphoreTake(mutexHandle, portMAX_DELAY);

        n += snprintf((p + n), (len - n), "%s %s ", fmtTimestamp(&tsBuf), logLevelName[logLevel]);

        if (logLevel >= trace) {
            n += snprintf((p + n), (len - n), "%s@%u:%s:%d ", pcTaskGetName(NULL), esp_cpu_get_core_id(), funcName, lineNum);
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
#ifdef CONFIG_FAT_FS
        if ((msgLogDest == both) || (msgLogDest == file)) {
            FILE *fp;
            if ((fp = fopen(mlogFilePath, "a")) == NULL) {
                fprintf(stderr, "SPONG! Failed to open log file! %s", strerror(errno));
                assert(0);
            }
            if (fprintf(fp, "%s\n", msgLogBuf) < 0) {
                if (errno == ENOSPC) {
                    // Running out of space on the FATFS is not fatal,
                    // but we need to switch the log message destination
                    // to the console, so as to avoid hitting our head
                    // against the wall over and over...
                    msgLogDest = console;
                } else {
                    fprintf(stderr, "SPONG! Failed to write to log file! %s", strerror(errno));
                    assert(0);
                }
            }
            fclose(fp);
        }
#endif

        if (logLevel == fatal) {
            ledSet(on, red);
            assert(false);
        }

        xSemaphoreGive(mutexHandle);
    }
}

int msgLogInit(LogLevel defLogLevel, LogDest defLogDest)
{
    msgLogLevel = defLogLevel;
    msgLogDest = defLogDest;

    // Semaphore used to serialize the calls to mlog()
    if ((mutexHandle = xSemaphoreCreateMutexStatic(&mutexSem)) == NULL) {
        // Hu?
        return -1;
    }

    // Reset the terminal's foreground color highlighting
    fprintf(stdout, "%s\n", RESET_FGC);

    mlog(info, "Message logging enabled: level=%s", logLevelName[defLogLevel]);

    return 0;
}

LogDest msgLogSetDest(LogDest logDest)
{
    LogDest prevLogDest = msgLogDest;
    if ((msgLogDest = logDest) != prevLogDest) {
        if (prevLogDest == console) {
            // Create the log file on the FATFS
            FILE *fp;
            if ((fp = fopen(mlogFilePath, "w+")) != NULL) {
                // Close it
                fclose(fp);
            } else {
                // Oops!
                msgLogDest = prevLogDest;
                mlog(errNo, "Failed to open log file: %s", mlogFilePath);
            }
        }
        mlog(info, "New message logging destination is %s", logDestName[msgLogDest]);
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
#else
int msgLogInit(LogLevel defLogLevel, LogDest defLogDest)
{
    return 0;
}

LogDest msgLogSetDest(LogDest logDest)
{
    return logDest;
}

LogLevel msgLogSetLevel(LogLevel logLevel)
{
    return none;
}

LogLevel msgLogGetLevel(void)
{
    return none;
}
#endif  // CONFIG_MSG_LOG
