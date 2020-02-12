#pragma once
#include "stdint.h"

uint32_t compile_shader(int32_t shader_type, const char *shader_source);
uint32_t create_shader(const char *vertex_source, const char *fragment_source);

