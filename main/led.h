#pragma once

typedef enum LedMode {
    off = 0,
    on,     // solid
    blink4, // 4 bps
    blink2, // 2 bps
    blink1, // 1 bps
} LedMode;

typedef enum LedColor {
    black = 0,
    blue,
    cyan,
    green,
    magenta,
    orange,
    purple,
    red,
    white,
    yellow,
} LedColor;

__BEGIN_DECLS

extern int ledInit(void);

extern int ledSet(LedMode mode, LedColor color);

__END_DECLS
