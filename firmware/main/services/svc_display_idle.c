#include "svc_display_idle.h"

#include "bsp_lcd.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "lvgl.h"
#include "sdkconfig.h"

static const char *TAG = "svc_display_idle";

#if CONFIG_WATCH_DISPLAY_IDLE_TIMEOUT_SEC > 0

static bool s_awake = true;
static esp_timer_handle_t s_poll_timer;

static void idle_poll_cb(void *arg)
{
    (void)arg;

    const uint32_t timeout_ms = (uint32_t)CONFIG_WATCH_DISPLAY_IDLE_TIMEOUT_SEC * 1000U;

    if (!lvgl_port_lock(pdMS_TO_TICKS(80))) {
        return;
    }

    lv_disp_t *disp = lv_disp_get_default();
    if (disp == NULL) {
        lvgl_port_unlock();
        return;
    }

    const uint32_t inactive_ms = lv_disp_get_inactive_time(disp);

    if (s_awake && inactive_ms >= timeout_ms) {
        esp_err_t err = bsp_lcd_display_set_awake(false);
        if (err == ESP_OK) {
            s_awake = false;
            ESP_LOGI(TAG, "sleep (inactive %lu ms)", (unsigned long)inactive_ms);
        } else {
            ESP_LOGW(TAG, "sleep failed: %s", esp_err_to_name(err));
        }
    } else if (!s_awake && inactive_ms < timeout_ms) {
        esp_err_t err = bsp_lcd_display_set_awake(true);
        if (err == ESP_OK) {
            s_awake = true;
            lv_obj_t *scr = lv_disp_get_scr_act(disp);
            if (scr != NULL) {
                lv_obj_invalidate(scr);
            }
            ESP_LOGI(TAG, "wake (inactive %lu ms)", (unsigned long)inactive_ms);
        } else {
            ESP_LOGW(TAG, "wake failed: %s", esp_err_to_name(err));
        }
    }

    lvgl_port_unlock();
}

void svc_display_idle_init(void)
{
    const esp_timer_create_args_t cfg = {
        .callback = &idle_poll_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "disp_idle",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&cfg, &s_poll_timer));
    /* Two polls within timeout avoid missing the wake edge after touch. */
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_poll_timer, 250 * 1000));
    ESP_LOGI(TAG, "idle blank after %d s", CONFIG_WATCH_DISPLAY_IDLE_TIMEOUT_SEC);
}

#else

void svc_display_idle_init(void)
{
    ESP_LOGI(TAG, "idle blank disabled (timeout 0)");
}

#endif
