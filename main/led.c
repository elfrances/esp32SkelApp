#include "driver/gpio.h"
#include "led_strip.h"
#include "sdkconfig.h"

#include "esp32.h"
#include "led.h"
#include "mlog.h"

#ifdef CONFIG_RGB_LED

typedef struct LedMsg {
    uint8_t mode;
    uint8_t color;
} LedMsg;

static led_strip_handle_t ledStripHandle;

static bool ledOn = false;

// This table specifies the individual R-G-B values
// used to achieve the requested LED color.
static struct rgb {
    uint8_t R;
    uint8_t G;
    uint8_t B;
} rgbTbl[] = {
              //    R     G     B
    [black]   = { 0x00, 0x00, 0x00 },
    [blue]    = { 0x00, 0x00, 0xFF },
    [cyan]    = { 0x00, 0xFF, 0xFF },
    [green]   = { 0x00, 0xFF, 0x00 },
    [magenta] = { 0xFF, 0x00, 0xFF },
    [orange]  = { 0xFF, 0x70, 0x00 },
    [purple]  = { 0x80, 0x00, 0xFF },
    [red]     = { 0xFF, 0x00, 0x00 },
    [white]   = { 0x80, 0x80, 0x80 },
    [yellow]  = { 0xFF, 0xFF, 0x00 },
};

// This table specifies the period (in millisec) used
// to toggle the LED on/off to achieve the requested
// blink rate (in blinks per sec).
//
//         ON   OFF   ON    OFF
//      +-----+     +-----+     +-----
//      |     |     |     |     |
// -----+     +-----+     +-----+
//
//      |<--------->|
//        one blink
//
static struct period {
    TickType_t msTicks;
} periodTbl[] = {                       //   ON   |  OFF
    [off]    = { portMAX_DELAY },
    [on]     = { portMAX_DELAY },
    [blink4] = { pdMS_TO_TICKS(125) },  // 125 ms | 125 ms
    [blink2] = { pdMS_TO_TICKS(250) },  // 250 ms | 250 ms
    [blink1] = { pdMS_TO_TICKS(500) },  // 500 ms | 500 ms
};

static TaskHandle_t ledTaskHandle;
static QueueHandle_t ledMsgQHandle;
static LedMode ledMode = off;
static LedColor ledColor = black;
static uint8_t ledDimFactor = 2;
static uint8_t R = 0x00;
static uint8_t G = 0x00;
static uint8_t B = 0x00;

static void ledSetRGB(void)
{
#if (CONFIG_RGB_LED_GPIO == 21)
    led_strip_set_pixel(ledStripHandle, 0, G, R, B);    // on the S3-Zero the RGB LED WS2812 seems to have the RED and GREEN reversed
#else
    led_strip_set_pixel(ledStripHandle, 0, R, G, B);
#endif
    led_strip_refresh(ledStripHandle);
}

static void ledBlink(void)
{
    if (ledMode == off) {
        // Turn off the LED
        led_strip_clear(ledStripHandle);
        ledOn = false;
    } else if (ledMode == on) {
        // Turn on the LED
        ledSetRGB();
        ledOn = true;
    } else {
        if (ledOn) {
            // LED is currently on, so turn it off...
            led_strip_clear(ledStripHandle);
        } else {
            // LED is currently off, so turn it on...
            ledSetRGB();
        }

        // Toggle the "on" state
        ledOn = !ledOn;
    }
}

static void ledTask(void *parms)
{
    const int ledMsgQMaxItems = 8;

    //mlog(info, "Task %s started: core=%u prio=%u", __func__, esp_cpu_get_core_id(), uxTaskPriorityGet(NULL));

    ledMsgQHandle = xQueueCreate(ledMsgQMaxItems, sizeof (LedMsg));

    while (true) {
        LedMsg ledMsg;

        // Wait for a message or for the LED toggle
        // period to expire...
        if (xQueueReceive(ledMsgQHandle, &ledMsg, periodTbl[ledMode].msTicks) == pdTRUE) {
            ledMode = ledMsg.mode;
            if (ledMsg.color != ledColor) {
                ledColor = ledMsg.color;
                R = rgbTbl[ledColor].R >> ledDimFactor;
                G = rgbTbl[ledColor].G >> ledDimFactor;
                B = rgbTbl[ledColor].B >> ledDimFactor;
            }
        }

        ledBlink();
    }
}

int ledSet(LedMode mode, LedColor color)
{
    LedMsg ledMsg = { .mode = mode, .color = color };
    return (xQueueSend(ledMsgQHandle, &ledMsg, 0) == pdPASS) ? 0 : -1;
}

int ledInit(void)
{
    led_strip_config_t stripConfig = {
        .strip_gpio_num = CONFIG_RGB_LED_GPIO,
        .max_leds = 1, // at least one LED on board
    };

    led_strip_rmt_config_t rmtConfig = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&stripConfig, &rmtConfig, &ledStripHandle));

    led_strip_clear(ledStripHandle);

    // Spawn the ledTask that will manage the LED
    xTaskCreatePinnedToCore(ledTask, "ledMgr", 3072, NULL, configMAX_PRIORITIES, &ledTaskHandle, tskNO_AFFINITY);

#if 0
    {
        static const char *colorNameTbl[] = {
                [black] = "BLACK",
                [blue] = "BLUE",
                [cyan] = "CYAN",
                [green] = "GREEN",
                [magenta] = "MAGENTA",
                [orange] = "ORANGE",
                [purple] = "PURPLE",
                [red] = "RED",
                [white] = "WHITE",
                [yellow] = "YELLOW",
        };

        vTaskDelay(pdMS_TO_TICKS(1000));

        // Test all the colors at different brightness
        for (int dimFactor = 0; dimFactor < 3; dimFactor++) {
            ledDimFactor = dimFactor;
            for (int color = black; color <= yellow; color++) {
                printf("%s ...\n", colorNameTbl[color]);
                ledSet(on, color);
                vTaskDelay(pdMS_TO_TICKS(3000));
            }
        }

        ledDimFactor = 0;
        ledSet(off, black);
    }
#endif

    return 0;
}

#endif  // CONFIG_RGB_LED
