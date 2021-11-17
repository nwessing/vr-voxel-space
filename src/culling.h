#pragma once
#include "types.h"

bool is_sphere_in_frustum(vec4 planes[6], vec3 center, float radius);
