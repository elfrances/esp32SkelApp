#pragma once

#include "app.h"

__BEGIN_DECLS

extern int nvramOpen(void);
extern int nvramClose(void);
extern int nvramRead(AppConfigInfo *configInfo);
extern int nvramWrite(const AppConfigInfo *configInfo);
extern int nvramClear(void);

__END_DECLS
