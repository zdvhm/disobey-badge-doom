# ESP32-S3 DOOM for Disobey Badge 2025/2026 (prboom port)
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/prboom
    ${CMAKE_CURRENT_SOURCE_DIR}/prboom-esp32-compat
    ${CMAKE_CURRENT_SOURCE_DIR}/prboom-wad-tables
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(badge-doom)
