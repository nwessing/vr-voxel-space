cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_STANDARD 99)
project(DesktopVrVoxelSpace)

add_compile_definitions(INCLUDE_GLAD)
include_directories(${CMAKE_SOURCE_DIR}/../src ${CMAKE_SOURCE_DIR}/../vendor/cglm/include ${CMAKE_SOURCE_DIR}/../vendor/include)

if (MSVC)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
endif()

add_executable(game
            ${CMAKE_SOURCE_DIR}/main.c
            # ${CMAKE_SOURCE_DIR}/vr.c
            ${CMAKE_SOURCE_DIR}/../src/glad/glad.c
            ${CMAKE_SOURCE_DIR}/../src/file.c
            ${CMAKE_SOURCE_DIR}/../src/game.c
            ${CMAKE_SOURCE_DIR}/../src/image.c
            ${CMAKE_SOURCE_DIR}/../src/raycasting.c
            ${CMAKE_SOURCE_DIR}/../src/culling.c
            ${CMAKE_SOURCE_DIR}/../src/shader.c
            ${CMAKE_SOURCE_DIR}/../src/util.c
            )
if (MSVC)
  target_compile_definitions(game PRIVATE _CRT_SECURE_NO_WARNINGS)
	target_compile_options(game PRIVATE /W4)
  # copy the .dll file to the same folder as the executable
  add_custom_command(
      TARGET game POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
      $<TARGET_FILE_DIR:SDL2::SDL2>
      $<TARGET_FILE_DIR:game>)
else()
  target_compile_options(game PRIVATE -Wall -Wextra -pedantic -Werror -Wno-error=unused-function -O2)
endif()

add_subdirectory(../vendor/SDL ./sdl2)
target_link_libraries(game PRIVATE SDL2)
