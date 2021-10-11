#include "types.h"

static void
decompose_projection_matrix(mat4 proj,
                            struct DecomposedProjectionMatrix *decomp) {
  float near = proj[3][2] / (proj[2][2] - 1.0f);
  float far = proj[3][2] / (proj[2][2] + 1.0f);
  float bottom = near * (proj[2][1] - 1.0f) / proj[1][1];
  float top = near * (proj[2][1] + 1.0f) / proj[1][1];
  float left = near * (proj[2][0] - 1.0f) / proj[0][0];
  float right = near * (proj[2][0] + 1.0f) / proj[0][0];
  decomp->near = near;
  decomp->far = far;

  decomp->field_of_view = 2.0 * atan(1.0 / proj[1][1]); // * 180.0 / PI;

  decomp->aspect_ratio = proj[1][1] / proj[0][0];

  decomp->bottom = bottom;
  decomp->top = top;
  decomp->left = left;
  decomp->right = right;
}

void get_frustum(mat4 projection, struct Camera *camera,
                 struct Frustum *frustum) {
  struct DecomposedProjectionMatrix decomp = {0};
  decompose_projection_matrix(projection, &decomp);
  /* printf("near = %f, far = %f, bottom = %f, top = %f, left = %f, right = %f,
   * " */
  /*        "fov = %f, aspect_ratio = %f\n", */
  /*        decomp.near, decomp.far, decomp.bottom, decomp.top, decomp.left, */
  /*        decomp.right, decomp.field_of_view, decomp.aspect_ratio); */

  float cam_distance_from_origin = glm_vec3_norm(camera->position) *
                                   (float)BASE_MAP_SIZE * camera->terrain_scale;

  {
    // Near face
    vec3 front_scaled_by_near;
    glm_vec3_scale(camera->front, decomp.near, front_scaled_by_near);

    frustum->near.distance =
        cam_distance_from_origin + glm_vec3_norm(front_scaled_by_near);
    glm_vec3_copy(camera->front, frustum->near.normal);
    glm_normalize(frustum->near.normal);
  }

  vec3 front_scaled_far;
  glm_vec3_scale(camera->front, decomp.far, front_scaled_far);

  {
    // Far face
    frustum->far.distance =
        cam_distance_from_origin + glm_vec3_norm(front_scaled_far);

    glm_vec3_copy(camera->front, frustum->far.normal);
    glm_vec3_inv(frustum->far.normal);
    glm_normalize(frustum->far.normal);
  }

  float half_viewport_height = decomp.far * tanf(decomp.field_of_view * 0.5f);
  float half_viewport_width = half_viewport_height * decomp.aspect_ratio;

  {
    // Right face
    frustum->right.distance = cam_distance_from_origin;

    // cross(camera_up, front_scaled_far + camera_right * half_viewport_width)
    vec3 intermediate;
    glm_vec3_scale(camera->right, half_viewport_width, intermediate);
    glm_vec3_add(front_scaled_far, intermediate, intermediate);
    glm_cross(camera->up, intermediate, frustum->right.normal);
    glm_normalize(frustum->right.normal);
  }

  {
    // Left face
    frustum->left.distance = cam_distance_from_origin;

    // cross(front_scaled_far - camera_right * half_viewport_width, camera_up)
    vec3 intermediate;
    glm_vec3_scale(camera->right, half_viewport_width, intermediate);
    glm_vec3_sub(front_scaled_far, intermediate, intermediate);
    glm_cross(intermediate, camera->up, frustum->left.normal);
    glm_normalize(frustum->left.normal);
  }

  {
    // Top face
    frustum->top.distance = cam_distance_from_origin;

    // glm::cross(camera_right, front_scaled_far - camera_up *
    // half_viewport_height)
    vec3 intermediate;
    glm_vec3_scale(camera->up, half_viewport_height, intermediate);
    glm_vec3_sub(front_scaled_far, intermediate, intermediate);
    glm_cross(camera->right, intermediate, frustum->top.normal);
    glm_normalize(frustum->top.normal);
  }

  {
    // Bottom face
    frustum->bottom.distance = cam_distance_from_origin;

    // glm::cross(front_scaled_far + camera_up * half_viewport_height,
    // camera_right)
    vec3 intermediate;
    glm_vec3_scale(camera->up, half_viewport_height, intermediate);
    glm_vec3_add(front_scaled_far, intermediate, intermediate);
    glm_cross(intermediate, camera->right, frustum->bottom.normal);
    glm_normalize(frustum->bottom.normal);
  }
}

inline static float get_signed_distance(struct Plane *plane, vec3 point) {
  return glm_dot(plane->normal, point) + plane->distance;
}

inline static bool is_sphere_in_front_of_plane(struct Plane *plane, vec3 center,
                                               float radius) {
  return get_signed_distance(plane, center) > -radius;
}
bool is_sphere_in_frustum(struct Frustum *frustum, vec3 center, float radius) {
  return is_sphere_in_front_of_plane(&frustum->left, center, radius) &&
         is_sphere_in_front_of_plane(&frustum->right, center, radius) &&
         is_sphere_in_front_of_plane(&frustum->far, center, radius) &&
         is_sphere_in_front_of_plane(&frustum->near, center, radius) &&
         is_sphere_in_front_of_plane(&frustum->top, center, radius) &&
         is_sphere_in_front_of_plane(&frustum->bottom, center, radius);
}

void print_plane(struct Plane *plane) {
  printf("%f - (%f, %f, %f)", plane->distance, plane->normal[0],
         plane->normal[1], plane->normal[2]);
}
