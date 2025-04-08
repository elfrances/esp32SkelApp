#pragma once

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_vfs_dev.h"
#if (CONFIG_ESP_WIFI_ENABLED)
#include "esp_wifi.h"
#include "esp_wps.h"
#endif
#if (CONFIG_ETH_ENABLED)
#include "esp_eth.h"
#endif

#include "lwip/sockets.h"
#include "apps/esp_sntp.h"
#ifdef CONFIG_MDNS_MAX_INTERFACES
#include "mdns.h"
#endif

#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/ans/ble_svc_ans.h"

__BEGIN_DECLS

// Convert system ticks to milliseconds
static __inline__ float ticksToMs(float ticks) { return (ticks * 1000.0) / (float) configTICK_RATE_HZ; }

// Show main ESP32 system info
extern void esp32ShowSysInfo(void);

__END_DECLS

