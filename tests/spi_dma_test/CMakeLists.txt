cmake_minimum_required(VERSION 3.24)
project(spi_dma_test)

# TEENSY_VERSION / CPU_CORE_SPEED / COMPILERPATH / COREPATH are supplied by the
# toolchain file (toolchain/rt1170-evkb.toolchain.cmake). Keep a fallback here
# so a bare `cmake -B build .` without the toolchain file still selects 117.
set(TEENSY_VERSION 117 CACHE STRING "")

# This gate now lives in the SPI library repo (SPI/tests/spi_dma_test/), so it
# compiles SPI.h/SPI.cpp from THIS repo (not the evkb core, whose own SPI
# driver was removed) while still borrowing the evkb checkout for the
# teensy-cmake-macros build machinery and the rest of the imxrt1176 core.
if(NOT DEFINED EVKB)
    set(EVKB $ENV{HOME}/Development/rt1170/evkb)
endif()

include(FetchContent)
# Use the LOCAL macros checkout (branch imxrt1176) rather than fetching from
# GitHub, so local edits to CMakeLists.include.txt are exercised.
FetchContent_Declare(teensy_cmake_macros SOURCE_DIR ${EVKB}/teensy-cmake-macros)
FetchContent_MakeAvailable(teensy_cmake_macros)
include(${teensy_cmake_macros_SOURCE_DIR}/CMakeLists.include.txt)

# Compile the whole imxrt1176 core as the `cores` library. No avr/util subdirs
# in the minimal Phase-0 core, so none are listed. The core's own SPI.{h,cpp}
# were removed when SPI moved here, so this contributes no SPI.h.
import_arduino_library(cores ${EVKB}/cores/imxrt1176)

# Compile SPI.h/SPI.cpp from THIS repo (the library under test). Its include
# dir is added so #include "SPI.h" in the firmware below resolves here.
import_arduino_library(SPI ${CMAKE_CURRENT_LIST_DIR}/../..)

teensy_add_executable(spi_dma_test spi_dma_test.cpp)
teensy_target_link_libraries(spi_dma_test cores SPI)

# spi_dma_test.cpp / main.cpp use C++; link the std C++ runtime (nano specs set in
# the toolchain file).
target_link_libraries(spi_dma_test.elf stdc++)
