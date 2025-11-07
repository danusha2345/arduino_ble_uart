idf_component_register(
    SRCS
        "main.c"
        "ble_service.c"
        "wifi_service.c"
        "gps_parser.c"
        "display_manager.c"
        "Vernon_ST7789T/Vernon_ST7789T.c"
    INCLUDE_DIRS "." "Vernon_ST7789T" "../components"
    PRIV_REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        lwip
        bt
        driver
        esp_driver_uart
        esp_driver_spi
        esp_driver_gpio
        esp_driver_i2c
        esp_lcd
        spi_flash
        freertos
        lvgl
        esp_event
)

# Определения для компиляции
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    "-DLV_CONF_INCLUDE_SIMPLE"
)

# Добавляем путь к lv_conf.h для LVGL
idf_component_get_property(lvgl_lib lvgl COMPONENT_LIB)
target_include_directories(${lvgl_lib} PUBLIC "../components")
