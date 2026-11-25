#
# CMakeLists.txt - HWLINK framing tests on Zephyr's native_sim board
#
# Author:  Sandesh Ghimire <sandesh@soccentric.com>
#
# Build with scripts/test-ztest.sh.  The test links the real application
# hwlink.c, so the CRC and COBS code under test is the code that goes into the
# firmware, not a copy.
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hwlink_tests LANGUAGES C)

target_include_directories(app PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

target_sources(app PRIVATE
    src/test_hwlink.c
    src/test_led_pattern.c
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/comms/hwlink.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/board/led_pattern.c")
