#include <stdbool.h>

#include "sdkconfig.h"

#include "esp32.h"
#include "https.h"
#include "mlog.h"

#ifdef CONFIG_WEB_SERVER
static char helpText[] = "HELP\n";

static esp_err_t getData(httpd_req_t *req)
{
    char*  buf;
    size_t bufLen;
    bufLen = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (bufLen > 1) {
        buf = malloc(bufLen);
        if (httpd_req_get_hdr_value_str(req, "Host", buf, bufLen) == ESP_OK) {
            //printf("Found header => Host: %s\n", buf);
        }
        free(buf);
    }
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t helpURI = {
    .uri       = "/help",
    .method    = HTTP_GET,
    .handler   = getData,
    .user_ctx  = helpText,
};

int httpsInit(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    esp_err_t err;

    config.task_priority = CONFIG_WEB_SERVER_TASK_PRIO;
    config.stack_size = CONFIG_WEB_SERVER_TASK_STACK;
    config.core_id = CONFIG_WEB_SERVER_TASK_CPU;
    config.server_port = CONFIG_WEB_SERVER_TCP_PORT;
    config.max_uri_handlers = 5;
    config.lru_purge_enable = true;

    if ((err = httpd_start(&server, &config)) != ESP_OK) {
        mlog(error, "Failed to start HTTP server: err=%04X", err);
        return -1;
    }

    // Set URI handlers
    if ((err = httpd_register_uri_handler(server, &helpURI)) != ESP_OK) {
        mlog(error, "Failed to register helpURI: err=%04X", err);
        return -1;
    }

    return 0;
}
#endif  // CONFIG_WEB_SERVER
