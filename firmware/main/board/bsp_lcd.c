#include "bsp_lcd.h"
#include "bsp_board.h"

#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

#include "esp_lcd_sh8601.h"

#include "lvgl.h"

static const char *TAG = "bsp_lcd";

/* 466x466: CASET/RASET end 0x1D1. Init seq from waveshare esp_lcd_sh8601 test, adapted for 1.43 panel. */
static const sh8601_lcd_init_cmd_t s_lcd_init_cmds[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 10},
    {0x63, (uint8_t[]){0xFF}, 1, 10},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0xD1}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xD1}, 4, 0},
    {0x29, (uint8_t[]){0x00}, 0, 10},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
};

static lv_disp_t *s_disp;
static esp_lcd_panel_io_handle_t s_lcd_io;
static lv_indev_drv_t s_touch_indev_drv;

/* Short timeout: long stalls here block LVGL indev and lose quick taps. */
#define TOUCH_I2C_TIMEOUT_MS 15

/* Runtime 7-bit address after probe (some FocalTech variants use 0x15). */
static uint8_t s_touch_i2c_addr = BSP_TOUCH_I2C_ADDR;

static esp_err_t touch_ft3168_write_reg(uint8_t dev_7bit, uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = i2c_master_start(cmd);
    if (err == ESP_OK) {
        err = i2c_master_write_byte(cmd, (dev_7bit << 1) | I2C_MASTER_WRITE, true);
    }
    if (err == ESP_OK) {
        err = i2c_master_write_byte(cmd, reg, true);
    }
    if (err == ESP_OK) {
        err = i2c_master_write_byte(cmd, val, true);
    }
    if (err == ESP_OK) {
        err = i2c_master_stop(cmd);
    }
    if (err == ESP_OK) {
        err = i2c_master_cmd_begin(BSP_I2C_NUM, cmd, pdMS_TO_TICKS(TOUCH_I2C_TIMEOUT_MS));
    }
    i2c_cmd_link_delete(cmd);
    return err;
}

/* FT3168 “normal mode”: reg 0, data 0. Non-fatal: display still comes up if bus NACKs. */
static void touch_ft3168_probe_normal_mode(void)
{
    static const uint8_t k_candidates[] = {BSP_TOUCH_I2C_ADDR, 0x15};
    esp_err_t last = ESP_ERR_NOT_FOUND;
    for (size_t i = 0; i < sizeof(k_candidates); i++) {
        last = touch_ft3168_write_reg(k_candidates[i], 0x00, 0x00);
        if (last == ESP_OK) {
            s_touch_i2c_addr = k_candidates[i];
            ESP_LOGI(TAG, "touch normal mode OK at 0x%02x", s_touch_i2c_addr);
            return;
        }
        ESP_LOGD(TAG, "touch normal mode 0x%02x: %s", k_candidates[i], esp_err_to_name(last));
    }
    s_touch_i2c_addr = BSP_TOUCH_I2C_ADDR;
    ESP_LOGW(TAG, "touch normal mode NACK (last %s); reads use 0x%02x", esp_err_to_name(last),
             (unsigned)s_touch_i2c_addr);
}

/* One transaction: TD_STATUS @0x02 + P1 X/Y @0x03..0x06 (FT6x06-style map). */
static esp_err_t touch_ft3168_read_status_xy(uint8_t *buf5)
{
    const uint8_t reg = 0x02;
    return i2c_master_write_read_device(BSP_I2C_NUM, s_touch_i2c_addr, &reg, 1, buf5, 5,
                                         pdMS_TO_TICKS(TOUCH_I2C_TIMEOUT_MS));
}

static lv_coord_t s_touch_last_x;
static lv_coord_t s_touch_last_y;

static void bsp_lv_touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    data->state = LV_INDEV_STATE_REL;

    uint8_t buf[5];
    if (touch_ft3168_read_status_xy(buf) != ESP_OK) {
        data->point.x = s_touch_last_x;
        data->point.y = s_touch_last_y;
        return;
    }

    const uint8_t npts = (uint8_t)(buf[0] & 0x0fU);
    if (npts == 0) {
        data->point.x = s_touch_last_x;
        data->point.y = s_touch_last_y;
        return;
    }

    uint16_t x = (((uint16_t)buf[1] & 0x0fU) << 8) | (uint16_t)buf[2];
    uint16_t y = (((uint16_t)buf[3] & 0x0fU) << 8) | (uint16_t)buf[4];
    if (x >= BSP_LCD_H_RES) {
        x = BSP_LCD_H_RES - 1;
    }
    if (y >= BSP_LCD_V_RES) {
        y = BSP_LCD_V_RES - 1;
    }

    s_touch_last_x = (lv_coord_t)x;
    s_touch_last_y = (lv_coord_t)y;
    data->point.x = s_touch_last_x;
    data->point.y = s_touch_last_y;
    data->state = LV_INDEV_STATE_PR;
}

static void bsp_display_rounder_cb(lv_disp_drv_t *disp_drv, lv_area_t *area)
{
    (void)disp_drv;
    uint16_t x1 = area->x1;
    uint16_t y1 = area->y1;
    uint16_t x2 = area->x2;
    uint16_t y2 = area->y2;
    area->x1 = (x1 >> 1) << 1;
    area->y1 = (y1 >> 1) << 1;
    area->x2 = ((x2 >> 1) << 1) + 1;
    area->y2 = ((y2 >> 1) << 1) + 1;
}

static esp_err_t bsp_i2c_init(void)
{
    const i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(BSP_I2C_NUM, &cfg), TAG, "i2c_param_config");
    ESP_RETURN_ON_ERROR(i2c_driver_install(BSP_I2C_NUM, cfg.mode, 0, 0, 0), TAG, "i2c_driver_install");
    return ESP_OK;
}

esp_err_t bsp_watch_display_start(void)
{
    ESP_LOGI(TAG, "AMOLED power (GPIO%d)", (int)BSP_LCD_PIN_EN);
    const gpio_config_t en_cfg = {
        .pin_bit_mask = 1ULL << BSP_LCD_PIN_EN,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&en_cfg), TAG, "gpio EN");
    ESP_RETURN_ON_ERROR(gpio_set_level(BSP_LCD_PIN_EN, 1), TAG, "EN high");
    /* Touch shares rail with AMOLED: bring up I2C + FT3168 normal mode before QSPI flood. */
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "i2c");
    touch_ft3168_probe_normal_mode();
    vTaskDelay(pdMS_TO_TICKS(10));

    const size_t buf_lines = 48;
    const int max_tx = (int)(BSP_LCD_H_RES * buf_lines * sizeof(uint16_t));
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_PIN_PCLK,
        .data0_io_num = BSP_LCD_PIN_DATA0,
        .data1_io_num = BSP_LCD_PIN_DATA1,
        .data2_io_num = BSP_LCD_PIN_DATA2,
        .data3_io_num = BSP_LCD_PIN_DATA3,
        .max_transfer_sz = max_tx,
        .flags = SPICOMMON_BUSFLAG_QUAD,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "spi_bus_initialize");

    esp_lcd_panel_io_spi_config_t io_spi_cfg = SH8601_PANEL_IO_QSPI_CONFIG(BSP_LCD_PIN_CS, NULL, NULL);
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_HOST, &io_spi_cfg, &s_lcd_io),
                        TAG, "panel_io_spi install");

    sh8601_vendor_config_t vendor = {
        .init_cmds = s_lcd_init_cmds,
        .init_cmds_size = sizeof(s_lcd_init_cmds) / sizeof(s_lcd_init_cmds[0]),
        .flags = {.use_qspi_interface = 1},
    };
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BSP_LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor,
    };
    esp_lcd_panel_handle_t panel = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_sh8601(s_lcd_io, &panel_cfg, &panel), TAG, "panel sh8601");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), TAG, "disp on");

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "lvgl_port_init");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = s_lcd_io,
        .panel_handle = panel,
        .buffer_size = (uint32_t)(BSP_LCD_H_RES * buf_lines),
        .double_buffer = true,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .monochrome = false,
        .rotation = {.swap_xy = false, .mirror_x = false, .mirror_y = false},
        .flags = {.buff_dma = true},
    };
    s_disp = lvgl_port_add_disp(&disp_cfg);
    ESP_RETURN_ON_FALSE(s_disp != NULL, ESP_FAIL, TAG, "lvgl_port_add_disp");

#if LVGL_VERSION_MAJOR == 8
    if (s_disp && s_disp->driver) {
        s_disp->driver->rounder_cb = bsp_display_rounder_cb;
    }
#endif

    if (!lvgl_port_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }
    s_touch_last_x = (lv_coord_t)(BSP_LCD_H_RES / 2);
    s_touch_last_y = (lv_coord_t)(BSP_LCD_V_RES / 2);
    lv_indev_drv_init(&s_touch_indev_drv);
    s_touch_indev_drv.type = LV_INDEV_TYPE_POINTER;
    s_touch_indev_drv.read_cb = bsp_lv_touch_read_cb;
    s_touch_indev_drv.disp = s_disp;
    lv_indev_drv_register(&s_touch_indev_drv);
    lvgl_port_unlock();

    return ESP_OK;
}
