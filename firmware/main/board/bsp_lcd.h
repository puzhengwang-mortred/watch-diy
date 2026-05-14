#pragma once

#include "esp_err.h"

/** QSPI LCD + I2C FT3168 + LVGL port; call after `svc_system_early_init()` (NVS). */
esp_err_t bsp_watch_display_start(void);
