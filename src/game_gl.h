#pragma once

#ifdef INCLUDE_GLAD
#include "glad/glad.h"
#else
// clang-format off
#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>
// clang-format on
#endif
