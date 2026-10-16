add_library(oe2core STATIC bar.c pmd.c moddata.c)
target_include_directories(oe2core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(oe2core PUBLIC PkgConfig::ZLIB PkgConfig::LIBAV)

add_executable(oe2tool tools/oe2tool.c)
target_link_libraries(oe2tool PRIVATE oe2core)

# The game: engine sources in game/, the SDL host in main.c + platform_sdl.c.
file(GLOB OE2_GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/game/*.c)
add_executable(oe2 main.c platform_sdl.c ${OE2_GAME_SOURCES})
target_include_directories(oe2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/game)
target_link_libraries(oe2 PRIVATE oe2core PkgConfig::SDL2 m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	# The original is ARM code: two's-complement wraparound in its fixed-point math (-fwrapv), and plain
	# char is unsigned in the ARM ABI (-funsigned-char).
	target_compile_options(oe2 PRIVATE -fwrapv -funsigned-char -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function)
endif()

if(OE2_SANITIZE)
	foreach(t oe2core oe2tool oe2)
		target_compile_options(${t} PRIVATE -fsanitize=address,undefined -fno-sanitize=shift-base -fno-omit-frame-pointer)
		target_link_options(${t} PRIVATE -fsanitize=address,undefined)
	endforeach()
endif()
