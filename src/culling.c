#include "types.h"

inline static float get_signed_distance(vec4 plane, vec3 point) {
  vec3 normal;
  glm_vec3(plane, normal);
  float distance = plane[3];
  return glm_dot(normal, point) + distance;
}

inline static bool is_sphere_in_front_of_plane(vec4 plane, vec3 center,
                                               float radius) {
  return get_signed_distance(plane, center) > -radius;
}

bool is_sphere_in_frustum(vec4 planes[6], vec3 center, float radius) {
  for (int i = 0; i < 6; ++i) {
    if (!is_sphere_in_front_of_plane(planes[i], center, radius)) {
      return false;
    }
  }
  return true;
}
