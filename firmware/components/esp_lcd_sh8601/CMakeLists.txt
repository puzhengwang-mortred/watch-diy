idf_component_register(
    SRCS "esp_lcd_sh8601.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd
    PRIV_REQUIRES driver
)
