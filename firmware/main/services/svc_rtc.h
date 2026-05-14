#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * Wall-clock fields read from PCF85063 BCD registers.
 * Roadmap 1.2: treat chip as Asia/Shanghai civil time (no DST offset math).
 */
typedef struct {
    int year; /**< 2000–2099 */
    int mon;  /**< 1–12 */
    int mday; /**< 1–31 */
    int hour; /**< 0–23 */
    int min;  /**< 0–59 */
    int sec;  /**< 0–59 */
    bool valid;
} svc_rtc_datetime_t;

/** After `bsp_watch_display_start()` (I2C up). Clears STOP if present; non-fatal on NACK. */
esp_err_t svc_rtc_init(void);

bool svc_rtc_have_chip(void);

/** Read time/date; sets valid=false if bus/OS/out-of-range. */
esp_err_t svc_rtc_read_local(svc_rtc_datetime_t *out);
