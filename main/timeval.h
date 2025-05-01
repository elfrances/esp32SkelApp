#pragma once

#include <sys/time.h>

// Add two timeval values: result = x + y
extern void tvAdd(struct timeval *result, const struct timeval *x, const struct timeval *y);

// Subtract two timeval values: result = x - y
extern void tvSub(struct timeval *result, const struct timeval *x, const struct timeval *y);

// Compare two timeval values: result = -1 if x < y, 0 if x == y, 1 if x > y
extern int tvCmp(const struct timeval *x, const struct timeval *y);
