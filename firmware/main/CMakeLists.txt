idf_component_register(
    SRCS
        "app_main.c"
        "app/watch_app.c"
        "board/bsp_lcd.c"
        "ui/ui_demo.c"
        "services/svc_system.c"
        "services/svc_rtc.c"
    INCLUDE_DIRS
        "."
        "app"
        "board"
        "ui"
        "services"
    REQUIRES esp_lcd_sh8601 nvs_flash driver esp_system esp_app_format espressif__esp_lvgl_port
)

# UTF-8 string literals in ui_demo.c (Chinese); avoids mojibake on Windows toolchains.
target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-finput-charset=UTF-8>
    $<$<COMPILE_LANGUAGE:C>:-fexec-charset=UTF-8>)
