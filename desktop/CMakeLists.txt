cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_STANDARD 99)
project(DesktopVrVoxelSpace)

add_compile_definitions(INCLUDE_GLAD)
include_directories(${CMAKE_SOURCE_DIR}/../src ${CMAKE_SOURCE_DIR}/../vendor/cglm/include)
add_executable(game
            ${CMAKE_SOURCE_DIR}/main.c
            # ${CMAKE_SOURCE_DIR}/vr.c
            ${CMAKE_SOURCE_DIR}/../src/glad/glad.c
            ${CMAKE_SOURCE_DIR}/../src/file.c
            ${CMAKE_SOURCE_DIR}/../src/game.c
            ${CMAKE_SOURCE_DIR}/../src/image.c
            ${CMAKE_SOURCE_DIR}/../src/raycasting.c
            ${CMAKE_SOURCE_DIR}/../src/shader.c
            ${CMAKE_SOURCE_DIR}/../src/util.c
            )
if (MSVC)
	target_compile_options(game PRIVATE /W4 /WX /O2)
else()
  target_compile_options(game PRIVATE -Wall -Wextra -pedantic -Werror -Wno-error=unused-function -O2)
endif()

add_subdirectory(../vendor/SDL ./sdl2)
target_link_libraries(game PRIVATE SDL2)
