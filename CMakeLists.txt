cmake_minimum_required(VERSION 3.14)
project(quantified-typing
    VERSION 0.1.3
    DESCRIPTION "Measuring typing speed over the day"
    HOMEPAGE_URL "https://github.com/fxkr/quantified-typing"
    LANGUAGES C)

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

set(CPACK_GENERATOR "RPM")
#set(CPACK_DEBIAN_PACKAGE_MAINTAINER "KK") #required
include(CPack)