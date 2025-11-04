# Минимальная версия CMake
cmake_minimum_required(VERSION 3.16)

# Подключаем ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Определяем проект
project(gnss_ble_bridge)
