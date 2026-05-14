#include "svc_batt.h"

#include "bsp_board.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "svc_batt";

#define BSP_BAT_ADC_UNIT     ADC_UNIT_1
#define BSP_BAT_ADC_CHANNEL  ADC_CHANNEL_3

static adc_oneshot_unit_handle_t s_adc;
static bool s_ok;

#define SAMPLES 8

/** Exponential moving average of percent (0–100). */
static int s_pct_ema = -1;

esp_err_t svc_batt_init(void)
{
    s_ok = false;
    if (s_adc != NULL) {
        (void)adc_oneshot_del_unit(s_adc);
        s_adc = NULL;
    }

    const adc_oneshot_unit_init_cfg_t ucfg = {
        .unit_id = BSP_BAT_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&ucfg, &s_adc);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "adc unit: %s", esp_err_to_name(err));
        return err;
    }

    const adc_oneshot_chan_cfg_t ccfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    err = adc_oneshot_config_channel(s_adc, BSP_BAT_ADC_CHANNEL, &ccfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "adc channel: %s", esp_err_to_name(err));
        (void)adc_oneshot_del_unit(s_adc);
        s_adc = NULL;
        return err;
    }

    s_ok = true;
    ESP_LOGI(TAG, "BAT_ADC GPIO%d unit %d ch %d", (int)BSP_BAT_ADC_GPIO, (int)BSP_BAT_ADC_UNIT,
             (int)BSP_BAT_ADC_CHANNEL);
    return ESP_OK;
}

esp_err_t svc_batt_read_percent(uint8_t *out_pct, bool *out_low)
{
    if (out_pct == NULL || out_low == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_pct = 0;
    *out_low = true;

    if (!s_ok || s_adc == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    int sum = 0;
    int n = 0;
    for (int i = 0; i < SAMPLES; i++) {
        int raw = 0;
        esp_err_t e = adc_oneshot_read(s_adc, BSP_BAT_ADC_CHANNEL, &raw);
        if (e != ESP_OK) {
            continue;
        }
        sum += raw;
        n++;
    }
    if (n == 0) {
        return ESP_FAIL;
    }

    const int raw_avg = sum / n;
    const int pin_mv = (raw_avg * BSP_BAT_ADC_REF_PIN_MV) / ((1 << BSP_BAT_ADC_BITS) - 1);
    const int64_t num = (int64_t)pin_mv * (int64_t)BSP_BAT_DIVIDER_NUM;
    const int64_t den = (int64_t)BSP_BAT_DIVIDER_DEN;
    const int cell_mv = (int)(num / den);

    int pct = (int)(((int64_t)(cell_mv - BSP_BAT_CELL_MV_EMPTY) * 100LL) /
                    (int64_t)(BSP_BAT_CELL_MV_FULL - BSP_BAT_CELL_MV_EMPTY));
    if (pct < 0) {
        pct = 0;
    }
    if (pct > 100) {
        pct = 100;
    }

    if (s_pct_ema < 0) {
        s_pct_ema = pct;
    } else {
        s_pct_ema = (s_pct_ema * 3 + pct + 2) / 4;
    }

    *out_pct = (uint8_t)s_pct_ema;
    *out_low = (s_pct_ema < 20);
    ESP_LOGD(TAG, "raw_avg=%d pin_mV=%d cell_mV=%d pct=%u", raw_avg, pin_mv, cell_mv, (unsigned)*out_pct);
    return ESP_OK;
}
