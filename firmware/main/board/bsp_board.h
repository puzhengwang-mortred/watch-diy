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

#define BSP_I2C_NUM            I2C_NUM_0
#define BSP_I2C_SDA            GPIO_NUM_47
#define BSP_I2C_SCL            GPIO_NUM_48
#define BSP_TOUCH_I2C_ADDR     0x38
