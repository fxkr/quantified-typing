cmake_minimum_required(VERSION 3.14)
project(quantified-typing C)

set (CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(MY_PKG REQUIRED IMPORTED_TARGET libevdev)

add_executable(quantified-typing main.c inotify_thread.c device_thread.c dev_input_set.c)

target_link_libraries(quantified-typing
    PkgConfig::MY_PKG
    ${CMAKE_THREAD_LIBS_INIT})
