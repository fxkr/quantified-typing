cmake_minimum_required(VERSION 3.14)
project(quantified-typing C)

set (CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(MY_PKG REQUIRED IMPORTED_TARGET libevdev)

add_executable(quantified-typing main.c inotify_thread.c device_thread.c stats_flush_thread.c stats_thread.c dev_input_set.c journal.c util.c)

install(TARGETS quantified-typing RUNTIME DESTINATION bin)

install(FILES quantified-typing.service DESTINATION /usr/lib/systemd/system)

target_link_libraries(quantified-typing
    PkgConfig::MY_PKG
    ${CMAKE_THREAD_LIBS_INIT})
