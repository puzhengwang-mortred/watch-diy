/*
 * Waveshare ESP32-S3-Touch-AMOLED-1.43 pin map (wiki "接口介绍" table).
 */
#pragma once

#include "driver/gpio.h"
#include "hal/spi_types.h"

#define BSP_LCD_HOST           SPI2_HOST
#define BSP_LCD_PIN_CS         GPIO_NUM_9
#define BSP_LCD_PIN_PCLK       GPIO_NUM_10
#define BSP_LCD_PIN_DATA0      GPIO_NUM_11
#define BSP_LCD_PIN_DATA1      GPIO_NUM_12
#define BSP_LCD_PIN_DATA2      GPIO_NUM_13
#define BSP_LCD_PIN_DATA3      GPIO_NUM_14
#define BSP_LCD_PIN_RST        GPIO_NUM_21
#define BSP_LCD_PIN_EN         GPIO_NUM_42

#define BSP_LCD_H_RES          466
#define BSP_LCD_V_RES          466

/**
 * Waveshare 1.43" AMOLED ships with two controllers (SH8601 / CO5300) using the same
 * board layout. CO5300 has a fixed 6-pixel left padding before the visible area (visible
 * cols sit at panel columns 6..471), so LVGL must write to panel cols 6..(6+466-1)=471 —
 * not 0..465 — or the rightmost 6 visible cols never get touched and the AMOLED GRAM
 * default colour (often bright green) keeps showing through. See LovyanGFX Panel_CO5300
 * and thelastoutpostworkshop/waveshare_esp32s3_1.43_amoled_lvgl9 (controller_id==CO5300 ?
 * x+0x06 : x). On a true SH8601 this becomes a harmless 6-pixel right shift; for an
 * SH8601-only batch set this to 0.
 */
#define BSP_LCD_VIEW_OFFSET_X 6
/* Keep LVGL at full visible width — partial-width buffers caused the SPI mid-row split
 * artefacts we used to work around with shrinking LVGL_H_RES + a separate edge strip. */
#define BSP_LCD_LVGL_H_RES    BSP_LCD_H_RES

#define BSP_I2C_NUM            I2C_NUM_0
#define BSP_I2C_SDA            GPIO_NUM_47
#define BSP_I2C_SCL            GPIO_NUM_48
#define BSP_TOUCH_I2C_ADDR     0x38
/** NXP PCF85063A fixed 7-bit address; same I2C bus as touch (bsp_lcd.c). */
#define BSP_RTC_I2C_ADDR       0x51

/** Waveshare wiki: GPIO4 = BAT_ADC (ADC1 CH3). Cell mV = pin_mV * NUM / DEN — tune with DMM. */
#define BSP_BAT_ADC_GPIO         GPIO_NUM_4
#define BSP_BAT_DIVIDER_NUM      2
#define BSP_BAT_DIVIDER_DEN      1
#define BSP_BAT_CELL_MV_EMPTY    3500
#define BSP_BAT_CELL_MV_FULL     4200
/** DB_12 approx. full-scale at pin (mV); used with 12-bit raw for rough pin voltage. */
#define BSP_BAT_ADC_REF_PIN_MV   3100
#define BSP_BAT_ADC_BITS         12
