cmake_minimum_required(VERSION 3.13.1)

set(BOARD stm32f4_disco)
set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/overlays/stm32f4_disco.overlay")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sss)

# Include your source files
target_sources(app PRIVATE src/main.c)
