#pragma once

// App configuration info
typedef struct AppConfigInfo {
    char wifiSsid[64];
    char wifiPasswd[64];
    int8_t utcOffset;

} AppConfigInfo;


__BEGIN_DECLS

extern int nvramOpen(void);
extern int nvramClose(void);
extern int nvramRead(AppConfigInfo *configInfo);
extern int nvramWrite(const AppConfigInfo *configInfo);
extern int nvramClear(void);

__END_DECLS
