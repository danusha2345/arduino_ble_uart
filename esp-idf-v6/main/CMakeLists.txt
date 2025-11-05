idf_component_register(
    SRCS
        "main.c"
        "ble_service.c"
        "wifi_service.c"
        "gps_parser.c"
        "display_manager.c"
    INCLUDE_DIRS "."
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
