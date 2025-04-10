#pragma once

#include <sys/cdefs.h>
#include <stdint.h>

typedef struct SerialNumber {
    uint8_t digits[4];
} SerialNumber;

__BEGIN_DECLS

extern void appMainTask(void *parms);
extern void getSerialNumber(SerialNumber *sn);

__END_DECLS
