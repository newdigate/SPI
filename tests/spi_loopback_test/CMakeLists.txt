cmake_minimum_required(VERSION 3.24)
project(spi_loopback_test)

# TEENSY_VERSION / CPU_CORE_SPEED / COMPILERPATH / COREPATH are supplied by the
# toolchain file (toolchain/rt1170-evkb.toolchain.cmake). Keep a fallback here
# so a bare `cmake -B build .` without the toolchain file still selects 117.
set(TEENSY_VERSION 117 CACHE STRING "")

# This gate lives in the SPI library repo (SPI/tests/spi_loopback_test/), so it
# compiles SPI.h/SPI.cpp from THIS repo (not the core, whose own SPI driver was
# removed) and borrows the sibling teensy-cores checkout for the rest of the
# imxrt1176 core and teensy-cmake-macros for the build machinery (neither sits
# inside evkb any more).  TEENSY_LIB_ROOT mirrors evkb.cmake: -D wins, then the
# environment, else $HOME/Development.
if(NOT DEFINED TEENSY_LIB_ROOT)
    if(DEFINED ENV{TEENSY_LIB_ROOT})
        set(TEENSY_LIB_ROOT "$ENV{TEENSY_LIB_ROOT}")
    else()
        set(TEENSY_LIB_ROOT "$ENV{HOME}/Development")
    endif()
endif()

include(FetchContent)
# Use the LOCAL macros checkout rather than fetching from GitHub, so local
# edits to CMakeLists.include.txt are exercised.
FetchContent_Declare(teensy_cmake_macros SOURCE_DIR ${TEENSY_LIB_ROOT}/teensy-cmake-macros)
FetchContent_MakeAvailable(teensy_cmake_macros)
include(${teensy_cmake_macros_SOURCE_DIR}/CMakeLists.include.txt)

# Compile the whole imxrt1176 core as the `cores` library. No avr/util subdirs
# in the minimal Phase-0 core, so none are listed. The core's own SPI.{h,cpp}
# were removed when SPI moved here, so this contributes no SPI.h.
import_arduino_library(cores ${TEENSY_LIB_ROOT}/teensy-cores/imxrt1176)

# Compile SPI.h/SPI.cpp from THIS repo (the library under test). Its include
# dir is added so #include "SPI.h" in the firmware below resolves here.
import_arduino_library(SPI ${CMAKE_CURRENT_LIST_DIR}/../..)

teensy_add_executable(spi_loopback_test spi_loopback_test.cpp)
teensy_target_link_libraries(spi_loopback_test cores SPI)

# spi_loopback_test.cpp / main.cpp use C++; link the std C++ runtime (nano specs set in
# the toolchain file).
target_link_libraries(spi_loopback_test.elf stdc++)
