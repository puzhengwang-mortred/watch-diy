#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * Waveshare 1.43: BAT_ADC on GPIO4 → ADC1 channel 3 (see wiki 01_ADC_Test).
 * Cell mV = pin_mV * BSP_BAT_DIVIDER_NUM / BSP_BAT_DIVIDER_DEN (tune in bsp_board.h with DMM).
 */
esp_err_t svc_batt_init(void);

/** Linear SoC between BSP_BAT_CELL_MV_EMPTY..FULL; optional EMA in driver. */
esp_err_t svc_batt_read_percent(uint8_t *out_pct, bool *out_low);
