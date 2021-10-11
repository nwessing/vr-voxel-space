#include "types.h"

void get_frustum(mat4 projection, struct Camera *camera,
                 struct Frustum *frustum);

bool is_sphere_in_frustum(struct Frustum *frustum, vec3 center, float radius);

void print_plane(struct Plane *plane);
