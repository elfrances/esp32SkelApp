#pragma once

#include "app.h"

__BEGIN_DECLS

extern int nvramOpen(void);
extern int nvramClose(void);
extern int nvramRead(AppPersData *configInfo);
extern int nvramWrite(const AppPersData *configInfo);
extern int nvramClear(void);

__END_DECLS
