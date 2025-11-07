# Минимальная версия CMake
cmake_minimum_required(VERSION 3.16)

# Путь к конфигурации LVGL (должен быть до include project.cmake)
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)

# Подключаем ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Определяем проект
project(gnss_ble_bridge)
