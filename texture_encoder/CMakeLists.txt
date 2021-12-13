cmake_minimum_required(VERSION 3.10)

project(TextureEncoder)
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/../vendor/include)

add_executable(texture_encoder
  ${CMAKE_SOURCE_DIR}/main.cpp
  )

add_subdirectory(../vendor/astc-encoder ./astc-encoder)
target_link_libraries(texture_encoder PRIVATE astcenc-native-static)
set_property(TARGET texture_encoder PROPERTY CXX_STANDARD 17)
