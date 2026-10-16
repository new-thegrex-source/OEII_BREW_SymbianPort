cmake_minimum_required(VERSION 3.25)
project(oe2 LANGUAGES C)

# The renderer is software and the game advances per tick; keep it optimized by default.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(OE2_SANITIZE "Build with ASan/UBSan" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)
# QCELP-13k (all O&E II sounds and music) is decoded with the system libavcodec (LGPL).
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET libavcodec libavutil)

add_subdirectory(src)
