cmake_minimum_required(VERSION 4.1)
project(kinectx C)

set(CMAKE_C_STANDARD 11)

include_directories(src)

add_executable(kinectx
        src/probe.c
        src/usb.c
        src/usb.h)
