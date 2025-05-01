#include "timeval.h"

static const suseconds_t oneMillion = 1000000;

// Normalize a timeval value
static __inline__ void tvNorm(struct timeval *t)
{
    if (t->tv_usec >= oneMillion) {
        t->tv_sec += (t->tv_usec / oneMillion);
        t->tv_usec = (t->tv_usec % oneMillion);
    }
}

// Add two timeval values: result = x - y
void tvAdd(struct timeval *result, const struct timeval *x, const struct timeval *y)
{
    struct timeval X = *x;
    struct timeval Y = *y;

    tvNorm(&X);
    tvNorm(&Y);

    result->tv_sec = X.tv_sec + Y.tv_sec;
    result->tv_usec = X.tv_usec + Y.tv_usec;
    tvNorm(result);
}

// Subtract two timeval values: result = x - y
void tvSub(struct timeval *result, const struct timeval *x, const struct timeval *y)
{
    struct timeval X = *x;
    struct timeval Y = *y;

    tvNorm(&X);
    tvNorm(&Y);

    if (X.tv_usec < Y.tv_usec) {
        X.tv_sec -= 1;
        X.tv_usec += oneMillion;
    }
    result->tv_sec = X.tv_sec - Y.tv_sec;
    result->tv_usec = X.tv_usec - Y.tv_usec;
}

// Compare two timeval values
int tvCmp(const struct timeval *x, const struct timeval *y)
{
    struct timeval X = *x;
    struct timeval Y = *y;

    tvNorm(&X);
    tvNorm(&Y);

    if (x->tv_sec > y->tv_sec) {
        return 1;
    } else if (x->tv_sec < y->tv_sec) {
        return -1;
    } else if (x->tv_usec > y->tv_usec) {
        return 1;
    } else if (x->tv_usec < y->tv_usec) {
        return -1;
    }

    return 0;
}
