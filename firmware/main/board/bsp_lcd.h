#pragma once

#include <stdbool.h>
#include "esp_err.h"

/** QSPI LCD + I2C FT3168 + LVGL port; call after `svc_system_early_init()` (NVS). */
esp_err_t bsp_watch_display_start(void);

/**
 * Panel visible output via SH8601 DISPON/DISPOFF (does not toggle GPIO EN rail).
 * Uses lvgl_port recursive mutex — safe under svc_display_idle while lock held.
 */
esp_err_t bsp_lcd_display_set_awake(bool awake);
