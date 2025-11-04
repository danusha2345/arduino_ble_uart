idf_component_register(
    SRCS
        "main.c"
        "ble_service.c"
        "wifi_service.c"
        "gps_parser.c"
        "display_manager.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        lwip
        bt
        driver
        esp_lcd
        spi_flash
        freertos
        lvgl
)

# Определения для компиляции
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    "-DLV_CONF_INCLUDE_SIMPLE"
)
