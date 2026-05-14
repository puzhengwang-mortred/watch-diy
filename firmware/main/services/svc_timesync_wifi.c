#include "svc_timesync_wifi.h"

#include "svc_rtc.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "svc_ts";

#if defined(CONFIG_WATCH_WIFI_STA_ENABLE) && CONFIG_WATCH_WIFI_STA_ENABLE

static bool s_sntp_started;

static void apply_tz_cst8(void)
{
    (void)setenv("TZ", "CST-8", 1);
    tzset();
}

static void time_sync_notification_cb(struct timeval *tv)
{
    (void)tv;
    apply_tz_cst8();

    time_t now = time(NULL);
    struct tm lt;
    if (localtime_r(&now, &lt) == NULL) {
        ESP_LOGW(TAG, "localtime_r failed after SNTP");
        return;
    }

    if (lt.tm_year < (2016 - 1900)) {
        ESP_LOGW(TAG, "system time still not plausible (tm_year=%d)", lt.tm_year);
        return;
    }

    svc_rtc_datetime_t dt = {
        .year = lt.tm_year + 1900,
        .mon = lt.tm_mon + 1,
        .mday = lt.tm_mday,
        .hour = lt.tm_hour,
        .min = lt.tm_min,
        .sec = lt.tm_sec,
        .valid = true,
    };

    ESP_LOGI(TAG, "SNTP sync, wall CST-8: %04d-%02d-%02d %02d:%02d:%02d", dt.year, dt.mon, dt.mday, dt.hour,
             dt.min, dt.sec);

    if (!svc_rtc_have_chip()) {
        ESP_LOGW(TAG, "no PCF85063; skip RTC write");
        return;
    }

    if (!lvgl_port_lock(5000)) {
        ESP_LOGW(TAG, "LVGL lock timeout; skip RTC write (I2C vs touch)");
        return;
    }
    esp_err_t w = svc_rtc_write_local(&dt);
    lvgl_port_unlock();
    if (w != ESP_OK) {
        ESP_LOGW(TAG, "RTC write failed: %s", esp_err_to_name(w));
    } else {
        ESP_LOGI(TAG, "RTC updated from NTP");
    }
}

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)id;
    (void)data;

    if (s_sntp_started) {
        return;
    }
    s_sntp_started = true;

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP started (pool.ntp.org)");
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)data;

    if (id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, retrying");
        if (s_sntp_started) {
            esp_sntp_stop();
            s_sntp_started = false;
        }
        esp_wifi_connect();
    }
}

#endif /* CONFIG_WATCH_WIFI_STA_ENABLE */

void svc_timesync_wifi_start(void)
{
#if !defined(CONFIG_WATCH_WIFI_STA_ENABLE) || !CONFIG_WATCH_WIFI_STA_ENABLE
    ESP_LOGI(TAG, "WiFi/SNTP disabled (menuconfig: WATCH_WIFI_STA_ENABLE)");
    return;
#else
    static bool s_inited;

    if (s_inited) {
        return;
    }

    if (CONFIG_WATCH_WIFI_STA_SSID[0] == '\0') {
        ESP_LOGW(TAG, "WATCH_WIFI_STA_SSID empty; skip WiFi/SNTP (set Component config → Watch WiFi / SNTP)");
        return;
    }

    apply_tz_cst8();

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init: %s", esp_err_to_name(err));
        return;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default: %s", esp_err_to_name(err));
        return;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wcfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init: %s", esp_err_to_name(err));
        return;
    }

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register WIFI_EVENT: %s", esp_err_to_name(err));
        return;
    }
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register IP_EVENT: %s", esp_err_to_name(err));
        return;
    }

    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, CONFIG_WATCH_WIFI_STA_SSID, sizeof(cfg.sta.ssid) - 1);
    cfg.sta.ssid[sizeof(cfg.sta.ssid) - 1] = '\0';
    strncpy((char *)cfg.sta.password, CONFIG_WATCH_WIFI_STA_PASSWORD, sizeof(cfg.sta.password) - 1);
    cfg.sta.password[sizeof(cfg.sta.password) - 1] = '\0';

    if (CONFIG_WATCH_WIFI_STA_PASSWORD[0] == '\0') {
        cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode: %s", esp_err_to_name(err));
        return;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config: %s", esp_err_to_name(err));
        return;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start: %s", esp_err_to_name(err));
        return;
    }

    s_inited = true;
    ESP_LOGI(TAG, "WiFi STA started (SSID=%s)", CONFIG_WATCH_WIFI_STA_SSID);
#endif
}
