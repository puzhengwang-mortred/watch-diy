#pragma once

/**
 * Idle blanking: poll LVGL inactivity vs CONFIG_WATCH_DISPLAY_IDLE_TIMEOUT_SEC,
 * call bsp_lcd_display_set_awake from esp_timer while holding lvgl_port_lock.
 * Touch press resets LVGL activity → wakes panel when inactive_time drops below threshold.
 */
void svc_display_idle_init(void);
