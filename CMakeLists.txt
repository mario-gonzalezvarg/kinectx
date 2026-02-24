cmake_minimum_required(VERSION 4.1)
project(kinectx C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED)


include_directories(src)

add_executable(kinectx
        src/probe.c
        src/usb.c)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

target_link_libraries(kinectx PRIVATE PkgConfig::LIBUSB)
