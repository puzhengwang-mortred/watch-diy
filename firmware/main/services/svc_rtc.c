#include "svc_rtc.h"

#include "bsp_board.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "svc_rtc";

#define PCF85063_REG_CTRL1   0x00U
#define PCF85063_REG_SECONDS 0x04U

#define PCF85063_CTRL1_STOP  (1U << 5)
#define PCF85063_SEC_OS      (1U << 7)

#define RTC_I2C_TIMEOUT_MS   25

static bool s_have_chip;

static int bcd_to_int(uint8_t v, uint8_t mask)
{
    v &= mask;
    return (int)(v & 0x0FU) + (int)((v >> 4) & 0x0FU) * 10;
}

static esp_err_t rtc_read_reg(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_write_read_device(BSP_I2C_NUM, BSP_RTC_I2C_ADDR, &reg, 1, buf, len,
                                        pdMS_TO_TICKS(RTC_I2C_TIMEOUT_MS));
}

static esp_err_t rtc_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t w[2] = {reg, val};
    return i2c_master_write_to_device(BSP_I2C_NUM, BSP_RTC_I2C_ADDR, w, sizeof(w),
                                      pdMS_TO_TICKS(RTC_I2C_TIMEOUT_MS));
}

esp_err_t svc_rtc_init(void)
{
    s_have_chip = false;

    uint8_t ctrl1 = 0;
    esp_err_t err = rtc_read_reg(PCF85063_REG_CTRL1, &ctrl1, 1);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "PCF85063 not responding (%s); watch time will show fallback", esp_err_to_name(err));
        return err;
    }

    s_have_chip = true;

    if (ctrl1 & PCF85063_CTRL1_STOP) {
        uint8_t cleared = (uint8_t)(ctrl1 & (uint8_t)~PCF85063_CTRL1_STOP);
        err = rtc_write_reg(PCF85063_REG_CTRL1, cleared);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "clear STOP failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "cleared STOP; oscillator running");
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    } else {
        ESP_LOGI(TAG, "PCF85063 OK (CTRL1=0x%02x)", (unsigned)ctrl1);
    }

    /* OS (sec bit7): many boards keep it set until first good run; mask for display. Clear if stuck. */
    uint8_t sec0 = 0;
    if (rtc_read_reg(PCF85063_REG_SECONDS, &sec0, 1) == ESP_OK && (sec0 & PCF85063_SEC_OS) != 0) {
        err = rtc_write_reg(PCF85063_REG_SECONDS, (uint8_t)(sec0 & 0x7FU));
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "cleared OS on seconds reg (was 0x%02x)", (unsigned)sec0);
        }
    }

    return ESP_OK;
}

bool svc_rtc_have_chip(void)
{
    return s_have_chip;
}

esp_err_t svc_rtc_read_local(svc_rtc_datetime_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->valid = false;

    if (!s_have_chip) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t raw[7];
    esp_err_t err = rtc_read_reg(PCF85063_REG_SECONDS, raw, sizeof(raw));
    if (err != ESP_OK) {
        return err;
    }

    /* Bit 7 = OS (oscillator stop). NXP: mask for BCD; do not drop whole read (was causing permanent "RTC invalid"). */
    int sec = bcd_to_int(raw[0], 0x7FU);
    int min = bcd_to_int(raw[1], 0x7FU);
    int hour = bcd_to_int(raw[2], 0x3FU);
    int mday = bcd_to_int(raw[3], 0x3FU);
    /* raw[4] weekday 1–7 — unused for display */
    int mon = bcd_to_int(raw[5], 0x1FU);
    int year2 = bcd_to_int(raw[6], 0xFFU);

    if (sec < 0 || sec > 59 || min < 0 || min > 59 || hour < 0 || hour > 23 || mday < 1 || mday > 31 || mon < 1 ||
        mon > 12 || year2 < 0 || year2 > 99) {
        return ESP_OK;
    }

    out->year = 2000 + year2;
    out->mon = mon;
    out->mday = mday;
    out->hour = hour;
    out->min = min;
    out->sec = sec;
    out->valid = true;
    return ESP_OK;
}
