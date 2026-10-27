cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(samu_monitor C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(samu_monitor
    SamuMonitor.cpp
)

target_link_libraries(samu_monitor pico_stdlib pico_stdio_usb pico_stdio_uart)

# 同时启用 USB CDC 和 UART：插 USB 就有 COM 口，也可用 USB-TTL 接 GP0/GP1
pico_enable_stdio_usb(samu_monitor 1)
pico_enable_stdio_uart(samu_monitor 1)

# 上电后最多等 3 秒让终端连上，避免开局信息丢失；没有 USB 主机则 3 秒后继续
target_compile_definitions(samu_monitor PRIVATE PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=3000)

pico_add_extra_outputs(samu_monitor)
