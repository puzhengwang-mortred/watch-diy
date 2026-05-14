#include "watch_app.h"

#include "bsp_lcd.h"
#include "svc_rtc.h"
#include "svc_system.h"
#include "svc_timesync_wifi.h"
#include "ui_demo.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "watch_app";

void watch_app_start(void)
{
    ESP_ERROR_CHECK(svc_system_early_init());
    ESP_ERROR_CHECK(bsp_watch_display_start());

    esp_err_t rtc = svc_rtc_init();
    if (rtc != ESP_OK) {
        ESP_LOGW(TAG, "RTC init: %s", esp_err_to_name(rtc));
    }

    if (lvgl_port_lock(0)) {
        ui_demo_show();
        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "LVGL UI started");

    svc_timesync_wifi_start();
}
