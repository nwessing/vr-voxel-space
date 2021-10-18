#include "game.h"
#include "assert.h"
#include "cglm/affine.h"
#include "cglm/mat4.h"
#include "cglm/vec3.h"
#include "culling.h"
#include "file.h"
#include "image.h"
#include "math.h"
#include "platform.h"
#include "raycasting.h"
#include "shader.h"
#include "string.h"
#include "types.h"
#include "util.h"

static vec3 CAMERA_TO_TERRAIN = {BASE_MAP_SIZE, BASE_MAP_SIZE, BASE_MAP_SIZE};

static struct MapEntry maps[MAP_COUNT] = {
    {"maps/C1W.png", "maps/D1.png"},   {"maps/C2W.png", "maps/D2.png"},
    {"maps/C3.png", "maps/D3.png"},    {"maps/C4.png", "maps/D4.png"},
    {"maps/C5W.png", "maps/D5.png"},   {"maps/C6W.png", "maps/D6.png"},
    {"maps/C7W.png", "maps/D7.png"},   {"maps/C8.png", "maps/D6.png"},
    {"maps/C9W.png", "maps/D9.png"},   {"maps/C10W.png", "maps/D10.png"},
    {"maps/C11W.png", "maps/D11.png"}, {"maps/C12W.png", "maps/D11.png"},
    {"maps/C13.png", "maps/D13.png"},  {"maps/C14.png", "maps/D14.png"},
    {"maps/C14W.png", "maps/D14.png"}, {"maps/C15.png", "maps/D15.png"},
    {"maps/C16W.png", "maps/D16.png"}, {"maps/C17W.png", "maps/D17.png"},
    {"maps/C18W.png", "maps/D18.png"}, {"maps/C19W.png", "maps/D19.png"},
    {"maps/C20W.png", "maps/D20.png"}, {"maps/C21.png", "maps/D21.png"},
    {"maps/C22W.png", "maps/D22.png"}, {"maps/C23W.png", "maps/D21.png"},
    {"maps/C24W.png", "maps/D24.png"}, {"maps/C25W.png", "maps/D25.png"},
    {"maps/C26W.png", "maps/D18.png"}, {"maps/C27W.png", "maps/D15.png"},
    {"maps/C28W.png", "maps/D25.png"}, {"maps/C29W.png", "maps/D16.png"}};

static void hmd_position_to_world_position(struct Game *game, vec3 hmd_position,
                                           vec3 out) {

  float scale_factor = 1.0f / game->camera.terrain_scale;
  vec3 scaler = {scale_factor, scale_factor, scale_factor};
  glm_vec3_div(scaler, CAMERA_TO_TERRAIN, out);
  glm_vec3_mul(out, hmd_position, out);
}

// Find the direction vector indicating where the camera is pointing
static void get_forward_vector(struct Camera *camera, vec3 direction,
                               bool only_use_pitch, vec3 result) {
  versor user_rotation;
  glm_quat(user_rotation, camera->pitch, 0, 1, 0);

  mat4 rotate = GLM_MAT4_IDENTITY_INIT;
  glm_quat_rotate(rotate, user_rotation, rotate);

  if (only_use_pitch) {
    mat4 cam_rotation;
    glm_quat_mat4(camera->quat, cam_rotation);

    vec3 euler_angles;
    glm_euler_angles(cam_rotation, euler_angles);
    euler_angles[0] = 0;
    euler_angles[2] = 0;
    glm_euler(euler_angles, cam_rotation);

    glm_mat4_mul(rotate, cam_rotation, rotate);
  } else {
    glm_quat_rotate(rotate, camera->quat, rotate);
  }

  glm_mat4_mulv3(rotate, direction, 1, result);
}

/*!
 * Calculates projection and view matrices to be used for rendering based on
 * input matrices.
 *
 * @param[in]  game
 * @param[in]  matrices
 * @param[out]  out
 */
static void compute_matrices(struct Game *game, struct InputMatrices *matrices,
                             struct RenderingMatrices *out) {
  struct Camera *camera = &game->camera;
  vec3 camera_offset;
  glm_vec3_scale(camera->position, -1.0f, camera_offset);

  glm_vec3_mul(camera_offset, CAMERA_TO_TERRAIN, camera_offset);
  glm_vec3_scale(camera_offset, camera->terrain_scale, camera_offset);

  mat4 view_matrix = GLM_MAT4_IDENTITY_INIT;
  glm_rotate(view_matrix, -camera->pitch, (vec3){0.0, 1.0, 0.0});
  glm_translate(view_matrix, camera_offset);

  out->enable_stereo = matrices->enable_stereo;
  int num_matrices = matrices->enable_stereo ? 2 : 1;
  for (int32_t i = 0; i < num_matrices; ++i) {
    // HMD position is built into the view matrix but we already accounted
    // for it in the position of the camera.
    mat4 eye_view_matrix;
    glm_mat4_copy(matrices->view_matrices[i], eye_view_matrix);
    glm_translate(eye_view_matrix, camera->last_hmd_position);

    glm_mat4_mul(eye_view_matrix, view_matrix, eye_view_matrix);

    mat4 projection_view = GLM_MAT4_IDENTITY_INIT;
    glm_mat4_mul(matrices->projection_matrices[i], eye_view_matrix,
                 projection_view);

    glm_mat4_copy(matrices->projection_matrices[i],
                  out->projection_matrices[i]);
    glm_mat4_copy(eye_view_matrix, out->view_matrices[i]);
    glm_mat4_copy(projection_view, out->projection_view_matrices[i]);
  }
}

static void generate_draw_commands_for_map(struct Game *game, struct Map *map,
                                           vec4 frustum_planes[6],
                                           int32_t budget, int32_t *cost,
                                           int32_t x, int32_t z) {
  struct Camera *camera = &game->camera;
  vec3 translate = {x * (BASE_MAP_SIZE - map->modifier), 0.0f,
                    z * (BASE_MAP_SIZE - map->modifier)};
  mat4 model = GLM_MAT4_IDENTITY_INIT;
  vec3 map_scaler = {camera->terrain_scale, camera->terrain_scale,
                     camera->terrain_scale};
  glm_scale(model, map_scaler);
  glm_translate(model, translate);

  for (int32_t i_section = 0; i_section < MAP_SECTION_COUNT; ++i_section) {
    if (game->render_commands.num_commands == game->render_commands.capacity ||
        *cost >= budget) {
      return;
    }

    struct MapSection *section = &map->sections[i_section];

    vec3 cam_terrain_position;
    glm_vec3_mul(camera->position, CAMERA_TO_TERRAIN, cam_terrain_position);

    vec3 section_center;
    glm_vec3_add(section->center, translate, section_center);

    {
      vec3 scaled_section_center;
      glm_vec3_scale(section_center, camera->terrain_scale,
                     scaled_section_center);
      if (!is_sphere_in_frustum(frustum_planes, scaled_section_center,
                                section->bounding_sphere_radius *
                                    camera->terrain_scale)) {
        continue;
      }
    }

    float distance = glm_vec3_distance(cam_terrain_position, section_center);
    int32_t lod_index = 0;
    if (distance <= 256.0f) {
      lod_index = 0;
      *cost += 16;
    } else if (distance < 512.0) {
      lod_index = 1;
      *cost += 4;
    } else {
      lod_index = 2;
      *cost += 1;
    }

    struct Mesh *mesh = &section->lods[lod_index];
    struct DrawCommand *draw_command =
        &game->render_commands.commands[game->render_commands.num_commands];
    ++game->render_commands.num_commands;
    draw_command->mesh = *mesh;
    draw_command->lod = lod_index;
    glm_mat4_copy(model, draw_command->model_matrix);
  }
}

/*!
 * Calculates what the game should render. Performs LOD selection and frustum
 * culling
 *
 * @param[in]  game
 * @param[in]  matrices
 */
static void generate_draw_commands(struct Game *game,
                                   struct RenderingMatrices *matrices) {
  struct Map *map = &game->maps[game->map_index];
  /* struct Camera *camera = &game->camera; */

  vec4 frustum_planes[6];
  if (matrices->enable_stereo) {
    glm_frustum_planes(matrices->projection_view_matrices[0], frustum_planes);
    vec4 right_eye_frustum_planes[6];
    glm_frustum_planes(matrices->projection_view_matrices[1],
                       right_eye_frustum_planes);

    // Create combined frustum by assuming near, far, bottom, top planes
    // are the same for each eye. Use left plane from left eye, and right
    // plane from right eye.
    glm_vec4_copy(right_eye_frustum_planes[1], frustum_planes[1]);
  } else {
    glm_frustum_planes(matrices->projection_view_matrices[0], frustum_planes);
  }

  game->render_commands.num_commands = 0;

  int32_t budget = 200;
  int32_t cost = 0, total_cost_last_iteration = 0;
  int32_t map_count = 0;
  int32_t area_size = 1;
  int32_t x = 0, z = 0;
  int32_t initial_row = 0, initial_column = 0;
  while (cost < budget &&
         game->render_commands.num_commands < game->render_commands.capacity) {
    info("area_size = %d, x = %d, z = %d, cost = %d, commands = %d\n",
         area_size, x, z, cost, game->render_commands.num_commands);
    generate_draw_commands_for_map(game, map, frustum_planes, budget, &cost, x,
                                   z);
    ++map_count;
    if (map_count == area_size * area_size) {
      area_size += 2;
      x = -(area_size / 2);
      z = -(area_size / 2);
      initial_row = x;
      initial_column = z;
      if (total_cost_last_iteration == cost) {
        break;
      }
      total_cost_last_iteration = cost;
    } else {
      if (z == initial_column && x != initial_row + area_size - 1) {
        ++x;
      } else if (x == initial_row + area_size - 1 &&
                 z != initial_column + area_size - 1) {
        ++z;
      } else if (z == initial_column + area_size - 1 && x != initial_row) {
        --x;
      } else if (x == initial_row) {
        --z;
      }
    }
  }
}

static void render_real_3d(struct Game *game,
                           struct RenderingMatrices *matrices, int32_t eye) {
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

#ifdef GL_POLYGON_MODE
  if (game->options.show_wireframe) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
#endif

  glClearColor(0.529f, 0.808f, 0.98f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  mat4 birdseye_projection_view = GLM_MAT4_IDENTITY_INIT;
  if (game->options.visualize_frustum) {
    float middle = (1.0f / 2.0f) * BASE_MAP_SIZE;
    vec3 frustum_vis_eye = {middle, 4000.0f, middle};
    vec3 frustum_vis_up = {0.0f, 0.0f, -1.0f};
    vec3 frustum_vis_center = {middle, 0.0f, middle};
    mat4 birdseye_view_matrix = GLM_MAT4_IDENTITY_INIT;
    // TODO use orthographic projection?
    glm_lookat(frustum_vis_eye, frustum_vis_center, frustum_vis_up,
               birdseye_view_matrix);

    glm_mat4_mul(matrices->projection_matrices[eye], birdseye_view_matrix,
                 birdseye_projection_view);
  }

  struct OpenGLData *gl = &game->gl;
  struct Map *map = &game->maps[game->map_index];
  glUseProgram(gl->poly_shader_program);
  glBindVertexArray(map->map_vao);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, map->color_map_tex_id);

  mat4 *projection_view = &matrices->projection_view_matrices[eye];

  glUniform2i(glGetUniformLocation(gl->poly_shader_program, "heightMapSize"),
              BASE_MAP_SIZE, BASE_MAP_SIZE);

  for (int32_t i_command = 0; i_command < game->render_commands.num_commands;
       ++i_command) {
    struct DrawCommand *command = &game->render_commands.commands[i_command];
    vec4 blend_color = {1.0, 1.0, 1.0, 1.0};
    if (game->options.visualize_lod) {
      switch (command->lod) {
      case 0:
        glm_vec4_copy((vec4){1.0, 0.0, 0.0, 1.0}, blend_color);
        break;
      case 1:
        glm_vec4_copy((vec4){0.0, 1.0, 0.0, 1.0}, blend_color);
        break;
      case 2:
        glm_vec4_copy((vec4){0.0, 0.0, 1.0, 1.0}, blend_color);
        break;
      default:
        break;
      }
    }

    mat4 mvp = GLM_MAT4_IDENTITY_INIT;
    if (game->options.visualize_frustum) {
      glm_mat4_mul(birdseye_projection_view, command->model_matrix, mvp);
    } else {
      glm_mat4_mul(*projection_view, command->model_matrix, mvp);
    }
    glUniformMatrix4fv(glGetUniformLocation(gl->poly_shader_program, "mvp"), 1,
                       GL_FALSE, (float *)mvp);

    glUniform4fv(glGetUniformLocation(gl->poly_shader_program, "blendColor"), 1,
                 blend_color);
    glDrawElements(GL_TRIANGLES, command->mesh.num_indices, GL_UNSIGNED_INT,
                   (void *)(uintptr_t)(command->mesh.offset * sizeof(int32_t)));
  }

  glBindVertexArray(0);

  if (game->options.visualize_frustum) {
    vec4 frustum_verts[8];
    mat4 inv_projection_view;
    glm_mat4_inv(*projection_view, inv_projection_view);
    glm_frustum_corners(inv_projection_view, frustum_verts);

    // NOTE indices for each specific corner are not part of the API
    vec3 verts[6];
    // top left near
    glm_vec3(frustum_verts[1], verts[0]);
    // top right far
    glm_vec3(frustum_verts[6], verts[1]);
    // top left far
    glm_vec3(frustum_verts[5], verts[2]);
    // top right near
    glm_vec3(frustum_verts[2], verts[3]);
    // top right far
    glm_vec3(frustum_verts[6], verts[4]);
    // top left near
    glm_vec3(frustum_verts[1], verts[5]);

    glBindVertexArray(game->gl.frustum_vis_vao);
    glBindBuffer(GL_ARRAY_BUFFER, game->gl.frustum_vis_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

    glUseProgram(game->gl.poly_shader_program);

    vec4 blend_color = {1.0, 1.0, 0.0, 0.5};
    glUniform4fv(glGetUniformLocation(gl->poly_shader_program, "blendColor"), 1,
                 blend_color);

    glUniformMatrix4fv(glGetUniformLocation(gl->poly_shader_program, "mvp"), 1,
                       GL_FALSE, (float *)birdseye_projection_view);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, game->gl.white_tex_id);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_BLEND);
  }
}

static void check_opengl_error(char *name) {
  GLenum gl_error = glGetError();
  if (gl_error) {
    error("(%s) error: %i\n", name, gl_error);
  }
}

static void render_buffer_to_gl(struct FrameBuffer *frame,
                                struct OpenGLData *gl, int clip) {
  glEnable(GL_DEPTH_TEST);
  glClearColor(0.529f, 0.808f, 0.98f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glUseProgram(gl->shader_program);
  glBindVertexArray(gl->vao);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gl->tex_id);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->pitch / 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width - (clip * 2),
               frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame->pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  check_opengl_error("glTexImage2D");

  glDrawArrays(GL_TRIANGLES, 0, gl->vao_num_vertices);
}

void render_game(struct Game *game, struct InputMatrices *matrices) {
  struct RenderingMatrices rendering_matrices;
  compute_matrices(game, matrices, &rendering_matrices);

  generate_draw_commands(game, &rendering_matrices);

  int32_t view_count = rendering_matrices.enable_stereo ? 2 : 1;
  for (int32_t i = 0; i < view_count; ++i) {
    glViewport(0, 0, matrices->framebuffer_width[i],
               matrices->framebuffer_height[i]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, matrices->framebuffers[i]);
    render_real_3d(game, &rendering_matrices, i);
  }
#if 0
  if (game->options.do_raycasting) {
    memset(game->frame.pixels, 0, game->frame.height * game->frame.pitch);
    if (game->options.render_stereo) {
      for (int eye = 0; eye < 2; ++eye) {
        struct FrameBuffer eye_buffer;
        eye_buffer.width = game->frame.width / 2;
        eye_buffer.height = game->frame.height;
        eye_buffer.pitch = game->frame.pitch;
        eye_buffer.y_buffer = &game->frame.y_buffer[eye * (eye_buffer.width
  / 2)];
        eye_buffer.clip_left_x = eye == 0 ? 0 : game->camera.clip;
        eye_buffer.clip_right_x = eye_buffer.width - (eye == 0 ?
  game->camera.clip : 0);
        eye_buffer.pixels = eye == 0 ? game->frame.pixels :
  &game->frame.pixels[(eye_buffer.width - game->camera.clip * 2) * 4];

        int eye_mod = eye == 1 ? 1 : -1;
        int eye_dist = 3;
        struct Camera eye_cam = game->camera;
        eye_cam.position_x += (int)(eye_mod * eye_dist * sin(eye_cam.pitch
  + (M_PI / 2)));
        eye_cam.position_y += (int)(eye_mod * eye_dist * cos(eye_cam.pitch
  + (M_PI / 2)));

        render(&eye_buffer, &game->color_map, &game->height_map, &eye_cam);
      }
    } else {
      render(&game->frame, &game->color_map, &game->height_map,
  &game->camera);
    }

    render_buffer_to_gl(&game->frame, &game->gl, game->camera.clip);
  } else {
    //render_real was here
  }
#endif
}

static inline bool is_key_pressed(struct KeyboardState *keyboard, int32_t key) {
  assert(key >= KEYBOAD_STATE_MIN_CHAR && key <= KEYBOARD_STATE_MAX_CHAR);
  return keyboard->down[key - KEYBOAD_STATE_MIN_CHAR];
}

static inline bool is_key_just_pressed(struct Game *game, int32_t key) {
  return is_key_pressed(&game->keyboard, key) &&
         !is_key_pressed(&game->prev_keyboard, key);
}

/* static inline bool is_button_just_pressed(struct Game *game */

static inline float read_axis(float joystick_axis, bool negative_key_pressed,
                              bool positive_key_pressed) {
  float result = 0.0f;
  if (joystick_axis) {
    result = joystick_axis;
  } else {
    if (positive_key_pressed) {
      result = 1.0f;
    } else if (negative_key_pressed) {
      result = -1.0f;
    }
  }

  return clamp(result, -1.0f, 1.0f);
}

static inline float get_ground_height(struct ImageBuffer *height_map, float x,
                                      float y) {
  return get_image_grey(height_map, x, y) / 255.0f;
}

static void update_camera(struct Camera *camera) {
  get_forward_vector(camera, (vec3){0, 0, -1.0f}, false, camera->front);

  vec3 up = {0.0f, 1.0f, 0.0f};
  glm_quat_rotatev(camera->quat, up, camera->up);

  versor pitch_adj;
  glm_quat(pitch_adj, camera->pitch, 0.0f, 1.0f, 0.0f);

  vec3 right = {1.0f, 0.0f, 0.0f};
  glm_vec3_copy(right, camera->right);
  glm_quat_rotatev(pitch_adj, camera->right, camera->right);
  glm_quat_rotatev(camera->quat, camera->right, camera->right);
}

void update_game(struct Game *game, struct KeyboardState *keyboard,
                 struct ControllerState *left_controller,
                 struct ControllerState *right_controller, vec3 hmd_position,
                 float elapsed) {
  game->prev_keyboard = game->keyboard;
  game->keyboard = *keyboard;

  game->prev_controller[LEFT_CONTROLLER_INDEX] =
      game->controller[LEFT_CONTROLLER_INDEX];
  game->prev_controller[RIGHT_CONTROLLER_INDEX] =
      game->controller[RIGHT_CONTROLLER_INDEX];
  game->controller[LEFT_CONTROLLER_INDEX] = *left_controller;
  game->controller[RIGHT_CONTROLLER_INDEX] = *right_controller;

  {
    // Update position based on the the current HMD position

    // Use the different between current position and last reported position
    vec3 hmd_difference;
    glm_vec3_sub(hmd_position, game->camera.last_hmd_position, hmd_difference);

    // Scale the hmd position different to world units
    vec3 hmd_world_pos;
    hmd_position_to_world_position(game, hmd_difference, hmd_world_pos);

    // Rotate the position difference by the user specified rotation angled
    // so that walking forward moves the direction the camera points
    mat4 cam_rotation;
    versor manual_rotation_quat;
    glm_quat(manual_rotation_quat, game->camera.pitch, 0.0f, 1.0f, 0.0f);
    glm_quat_mat4(manual_rotation_quat, cam_rotation);
    glm_mat4_mulv3(cam_rotation, hmd_world_pos, 1.0f, hmd_world_pos);

    glm_vec3_add(game->camera.position, hmd_world_pos, game->camera.position);
    glm_vec3_copy(hmd_position, game->camera.last_hmd_position);
  }

  float forward_movement =
      read_axis(game->controller[LEFT_CONTROLLER_INDEX].joy_stick.y,
                is_key_pressed(&game->keyboard, 's'),
                is_key_pressed(&game->keyboard, 'w'));

  float horizontal_movement =
      read_axis(game->controller[LEFT_CONTROLLER_INDEX].joy_stick.x,
                is_key_pressed(&game->keyboard, 'a'),
                is_key_pressed(&game->keyboard, 'd'));

  float vertical_movement = 0.0f;
  if (game->controller[RIGHT_CONTROLLER_INDEX].primary_button ||
      is_key_pressed(&game->keyboard, 'f')) {
    vertical_movement -= 1.0f;
  }

  if (game->controller[RIGHT_CONTROLLER_INDEX].secondary_button ||
      is_key_pressed(&game->keyboard, 'r')) {
    vertical_movement += 1.0f;
  }

  float rotation_movement = read_axis(
      game->controller[RIGHT_CONTROLLER_INDEX].joy_stick.x, false, false);

  struct ControllerState *left_controller_prev =
      &game->prev_controller[LEFT_CONTROLLER_INDEX];

  bool change_map_mode = game->controller[RIGHT_CONTROLLER_INDEX].grip > 0.9f;
  if (is_key_just_pressed(game, 'j') ||
      (game->trigger_set[LEFT_CONTROLLER_INDEX] && change_map_mode &&
       game->controller[LEFT_CONTROLLER_INDEX].trigger > 0.9f)) {
    game->trigger_set[LEFT_CONTROLLER_INDEX] = false;
    --game->map_index;
    if (game->map_index < 0) {
      game->map_index = MAP_COUNT - 1;
    }
  } else if (is_key_just_pressed(game, 'k') ||
             (game->trigger_set[RIGHT_CONTROLLER_INDEX] && change_map_mode &&
              game->controller[RIGHT_CONTROLLER_INDEX].trigger > 0.9f)) {
    game->trigger_set[RIGHT_CONTROLLER_INDEX] = false;
    ++game->map_index;
    if (game->map_index >= MAP_COUNT) {
      game->map_index = 0;
    }
  }

  if (is_key_just_pressed(game, 'e') ||
      (left_controller->secondary_button &&
       !left_controller_prev->secondary_button)) {
    game->options.visualize_lod = !game->options.visualize_lod;
  }

  if (is_key_just_pressed(game, 'v')) {
    game->options.show_wireframe = !game->options.show_wireframe;
  }

  if (is_key_just_pressed(game, 'b') || (left_controller->primary_button &&
                                         left_controller_prev->primary_button !=
                                             left_controller->primary_button)) {
    game->options.visualize_frustum = !game->options.visualize_frustum;
  }

  for (int i = 0; i < 2; ++i) {
    if (game->controller[i].trigger < 0.1) {
      game->trigger_set[i] = true;
    }
  }

  vec3 movement;
  {

    vec3 direction_intensities = {horizontal_movement, vertical_movement,
                                  -forward_movement};
    glm_vec3_normalize(direction_intensities);

    float y_only_movement = direction_intensities[1];

    vec3 forward;
    direction_intensities[1] = .0f;
    get_forward_vector(&game->camera, direction_intensities, false, forward);

    float units_per_second = 0.25f;
    float delta_units = units_per_second * elapsed;
    glm_vec3_scale(forward, delta_units, movement);
    movement[1] += y_only_movement * delta_units;
  }

  glm_vec3_add(game->camera.position, movement, game->camera.position);

  game->camera.pitch +=
      -rotation_movement * M_PI *
      (right_controller->scale_rotation_by_time ? elapsed : 1.0f);

  while (game->camera.pitch >= 2 * M_PI) {
    game->camera.pitch -= 2 * M_PI;
  }

  while (game->camera.pitch < 0) {
    game->camera.pitch += 2 * M_PI;
  }

  while (game->camera.position[0] >= 1.0) {
    game->camera.position[0] -= 1.0;
  }

  while (game->camera.position[2] >= 1.0) {
    game->camera.position[2] -= 1.0;
  }

  while (game->camera.position[0] < 0) {
    game->camera.position[0] += 1.0;
  }

  while (game->camera.position[2] < 0) {
    game->camera.position[2] += 1.0;
  }

  update_camera(&game->camera);

  if (is_key_pressed(&game->keyboard, 'z') ||
      (!change_map_mode &&
       game->controller[LEFT_CONTROLLER_INDEX].trigger > 0.9f)) {
    // 50% per second
    float half = game->camera.terrain_scale / 2.0f;
    game->camera.terrain_scale -= half * elapsed;
  } else if (is_key_pressed(&game->keyboard, 'x') ||
             (!change_map_mode &&
              game->controller[RIGHT_CONTROLLER_INDEX].trigger > 0.9f)) {
    // 50% per second
    float half = game->camera.terrain_scale / 2.0f;
    game->camera.terrain_scale += half * elapsed;
  }

  /* printf("pos = %f, %f, %f, rot = %f\n", game->camera.position[0], */
  /*        game->camera.position[1], game->camera.position[2], */
  /*        game->camera.pitch); */
}

struct Rect {
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

struct MapMeshExtents {
  int32_t width;
  int32_t height;
};

static int32_t generate_indices(int32_t *index_buffer, int32_t num_indices,
                                int32_t buffer_size, int32_t v_index,
                                int32_t sample_divisor, int32_t width) {
  assert(num_indices + 6 < buffer_size);
  index_buffer[num_indices++] = v_index;
  index_buffer[num_indices++] = v_index + (width * sample_divisor);
  index_buffer[num_indices++] = v_index + sample_divisor;

  index_buffer[num_indices++] = v_index + (width * sample_divisor);
  index_buffer[num_indices++] =
      v_index + (width * sample_divisor) + sample_divisor;
  index_buffer[num_indices++] = v_index + sample_divisor;
  return num_indices;
}

static int32_t generate_lod_indices(struct MapMeshExtents *map_mesh_extents,
                                    int32_t sample_divisor, struct Rect rect,
                                    int32_t *index_buffer,
                                    int32_t buffer_size) {
  int32_t width = map_mesh_extents->width;
  int32_t height = map_mesh_extents->height;

  int32_t num_indices = 0;

  int32_t sample_width = rect.width / sample_divisor;
  int32_t sample_height = rect.height / sample_divisor;

  for (int32_t sample_y = 0; sample_y < sample_height; ++sample_y) {
    for (int32_t sample_x = 0; sample_x < sample_width; ++sample_x) {
      int32_t x = rect.x + (sample_x * sample_divisor);
      int32_t y = rect.y + (sample_y * sample_divisor);
      if (x < 0 || y < 0 || x + sample_divisor >= width ||
          y + sample_divisor >= height) {
        continue;
      }

      int32_t v_index = ((y * width) + x);

      if (sample_divisor > 1 &&
          (sample_x == 0 || sample_y == 0 || sample_x == sample_width - 1 ||
           sample_y == sample_height - 1)) {

        if (sample_x == 0 && sample_y == 0) {
          // Top-left corner
          uint32_t pivot = v_index + sample_divisor + (width * sample_divisor);
          uint32_t v_begin = v_index;

          for (int32_t i = 1; i <= sample_divisor; ++i) {
            assert(num_indices + 3 < buffer_size);
            index_buffer[num_indices++] = v_begin + (i * width);
            index_buffer[num_indices++] = pivot;
            index_buffer[num_indices++] = v_begin + ((i - 1) * width);
          }

          for (int32_t i = 1; i <= sample_divisor; ++i) {
            assert(num_indices + 3 < buffer_size);
            index_buffer[num_indices++] = pivot;
            index_buffer[num_indices++] = v_begin + i;
            index_buffer[num_indices++] = v_begin + (i - 1);
          }

        } else if (sample_x == 0 && sample_y == sample_height - 1) {
          // Bottom-left corner
          uint32_t pivot = v_index + sample_divisor;
          uint32_t v_begin = v_index;

          for (int32_t i = 1; i <= sample_divisor; ++i) {
            assert(num_indices + 3 < buffer_size);
            index_buffer[num_indices++] = v_begin + (i * width);
            index_buffer[num_indices++] = pivot;
            index_buffer[num_indices++] = v_begin + ((i - 1) * width);
          }

          for (int32_t i = 1; i <= sample_divisor; ++i) {
            assert(num_indices + 3 < buffer_size);
            index_buffer[num_indices++] = pivot;
            index_buffer[num_indices++] =
                v_begin + (sample_divisor * width) + (i - 1);
            index_buffer[num_indices++] =
                v_begin + (sample_divisor * width) + i;
          }

        } else if (sample_x == sample_width - 1 && sample_y == 0) {
          // Top-right corner
          uint32_t pivot = v_index + (width * sample_divisor);
          uint32_t v_begin = v_index + sample_divisor;

          for (int32_t i = 1; i <= sample_divisor; ++i) {
            assert(num_indices + 3 < buffer_size);
            index_buffer[num_indices++] = v_begin - i;
            index_buffer[num_indices++] = pivot;
            index_buffer[num_indices++] = v_begin - (i - 1);
          }

          for (int32_t i = 1; i <= sample_divisor; ++i) {
            assert(num_indices + 3 < buffer_size);
            index_buffer[num_indices++] = pivot;
            index_buffer[num_indices++] = v_begin + (i * width);
            index_buffer[num_indices++] = v_begin + ((i - 1) * width);
          }
        } else if (sample_x == sample_width - 1 &&
                   sample_y == sample_height - 1) {
          // Bottom-right corner
          uint32_t pivot = v_index;
          uint32_t v_begin =
              v_index + (sample_divisor * width) + sample_divisor;

          for (int32_t i = 1; i <= sample_divisor; ++i) {
            assert(num_indices + 3 < buffer_size);
            index_buffer[num_indices++] = v_begin - (i - 1);
            index_buffer[num_indices++] = pivot;
            index_buffer[num_indices++] = v_begin - i;
          }

          for (int32_t i = 1; i <= sample_divisor; ++i) {
            assert(num_indices + 3 < buffer_size);
            index_buffer[num_indices++] = pivot;
            index_buffer[num_indices++] = v_begin - ((i - 1) * width);
            index_buffer[num_indices++] = v_begin - (i * width);
          }
        } else {
          // Strips on left and right sides
          if (sample_x == 0 || sample_x == sample_width - 1) {
            int32_t pivot = v_index;
            int32_t v_begin = v_index + sample_divisor;
            int32_t direction = 1;
            if (sample_x == 0) {
              pivot = v_index + sample_divisor;
              v_begin = v_index + (width * sample_divisor);
              direction = -1;
            }

            for (int32_t i = 1; i <= sample_divisor; ++i) {
              assert(num_indices + 3 < buffer_size);
              index_buffer[num_indices++] = pivot;
              index_buffer[num_indices++] = v_begin + (direction * i * width);
              index_buffer[num_indices++] =
                  v_begin + (direction * (i - 1) * width);
            }

            if (direction == 1) {
              assert(num_indices + 3 < buffer_size);
              index_buffer[num_indices++] = pivot;
              index_buffer[num_indices++] = pivot + (width * sample_divisor);
              index_buffer[num_indices++] =
                  pivot + (width * sample_divisor) + sample_divisor;
            } else {
              assert(num_indices + 3 < buffer_size);
              index_buffer[num_indices++] = pivot;
              index_buffer[num_indices++] =
                  pivot + (width * sample_divisor) - sample_divisor;
              index_buffer[num_indices++] = pivot + (width * sample_divisor);
            }
          }

          // Top and bottom strips
          if (sample_y == 0 || sample_y == sample_height - 1) {
            int32_t pivot = v_index;
            int32_t v_begin = v_index + (sample_divisor * width);
            int32_t direction = 1;
            if (sample_y == 0) {
              direction = -1;
              v_begin = v_index + sample_divisor;
              pivot = v_index + (sample_divisor * width);
            }

            for (int32_t i = 1; i <= sample_divisor; ++i) {
              assert(num_indices + 3 < buffer_size);
              index_buffer[num_indices++] = pivot;
              index_buffer[num_indices++] = v_begin + (direction * (i - 1));
              index_buffer[num_indices++] = v_begin + (direction * i);
            }

            if (direction == 1) {
              assert(num_indices + 3 < buffer_size);
              index_buffer[num_indices++] = pivot;
              index_buffer[num_indices++] =
                  pivot + sample_divisor + (width * sample_divisor);
              index_buffer[num_indices++] = pivot + sample_divisor;
            } else {
              index_buffer[num_indices++] = pivot;
              index_buffer[num_indices++] = pivot + sample_divisor;
              index_buffer[num_indices++] =
                  pivot + sample_divisor - (width * sample_divisor);
            }
          }
        }

      } else {
        num_indices = generate_indices(index_buffer, num_indices, buffer_size,
                                       v_index, sample_divisor, width);
      }
    }
  }

  return num_indices;
}

static void create_map_gl_data(struct Map *map) {
  glGenTextures(1, &map->color_map_tex_id);
  glBindTexture(GL_TEXTURE_2D, map->color_map_tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, map->color_map.width,
               map->color_map.height, 0, GL_RGB, GL_UNSIGNED_BYTE,
               map->color_map.pixels);

  // NOTE generate vertices for 1 past the width and height, so that maps
  // can be seamlessly tiled together
  struct MapMeshExtents extents = {
      .width = map->height_map.width + 1,
      .height = map->height_map.height + 1,
  };

  int32_t num_map_vertices = (extents.width * extents.height);
  V3 *map_vertices = malloc(sizeof(V3) * num_map_vertices);

  int32_t indices_per_vert = 6;

  float modifier = map->modifier;

  int32_t index_buffer_length = num_map_vertices * indices_per_vert * LOD_COUNT;
  int32_t *index_buffer = malloc(sizeof(int32_t) * index_buffer_length);

  for (int32_t y = 0; y < extents.height; ++y) {
    for (int32_t x = 0; x < extents.width; ++x) {
      int32_t v_index = ((y * extents.width) + x);

      assert(v_index >= 0);
      assert(v_index < num_map_vertices);

      map_vertices[v_index][0] = x * modifier;

      // NOTE When sampling 1 past the width or height, wrap around to get
      // the depth value. So that edges of the maps match up nice when
      // tiling.
      int32_t height_sample_x = x;
      if (height_sample_x == extents.width - 1) {
        height_sample_x = 0;
      }
      int32_t height_sample_y = y;
      if (height_sample_y == extents.height - 1) {
        height_sample_y = 0;
      }

      map_vertices[v_index][1] = (float)get_image_grey(
          &map->height_map, height_sample_x, height_sample_y);
      map_vertices[v_index][2] = y * modifier;
    }
  }

  int32_t num_indices = 0;
  int32_t section_width = extents.width / MAP_X_SEGMENTS;
  int32_t section_height = extents.height / MAP_Y_SEGMENTS;
  for (int32_t i_section = 0; i_section < MAP_SECTION_COUNT; ++i_section) {
    struct Rect rect = {.x = (i_section / MAP_X_SEGMENTS) * section_width,
                        .y = (i_section % MAP_Y_SEGMENTS) * section_height,
                        .width = section_width,
                        .height = section_height};
    struct MapSection *section = &map->sections[i_section];
    float half_section_width = section_width / 2.0f;
    float half_section_height = section_height / 2.0f;
    section->center[0] = (rect.x + half_section_width) * modifier;
    section->center[1] = 128.0f;
    section->center[2] = (rect.y + half_section_height) * modifier;
    section->bounding_sphere_radius = sqrtf(
        (half_section_width * half_section_width * modifier * modifier) +
        (half_section_height * half_section_height * modifier * modifier));

    int32_t divisor = 1;
    for (int32_t i_lod = 0; i_lod < LOD_COUNT; ++i_lod) {
      section->lods[i_lod].offset = num_indices;
      int32_t num_lod_indices = generate_lod_indices(
          &extents, divisor, rect, &index_buffer[num_indices],
          index_buffer_length - num_indices);
      section->lods[i_lod].num_indices = num_lod_indices;
      num_indices += num_lod_indices;
      divisor *= 2;
    }
  }

  glGenVertexArrays(1, &map->map_vao);
  glBindVertexArray(map->map_vao);

  glGenBuffers(1, &map->map_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, map->map_vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glBufferData(GL_ARRAY_BUFFER, num_map_vertices * sizeof(V3), map_vertices,
               GL_STATIC_DRAW);
  free(map_vertices);

  glGenBuffers(1, &map->map_vbo_indices);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, map->map_vbo_indices);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_indices * sizeof(int32_t),
               index_buffer, GL_STATIC_DRAW);
  free(index_buffer);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

static void create_gl_objects(struct Game *game) {
  float vertices[] = {
      -1.0, -1.0, 0.0, -1.0, 1.0,  0.0, 1.0, 1.0, 0.0,
      -1.0, -1.0, 0.0, 1.0,  -1.0, 0.0, 1.0, 1.0, 0.0,
  };
  struct OpenGLData *gl = &game->gl;

  glGenTextures(1, &gl->tex_id);
  glBindTexture(GL_TEXTURE_2D, gl->tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenFramebuffers(1, &gl->frame_buffer);

  glGenBuffers(1, &gl->vbo);

  glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
  gl->vao_num_vertices = sizeof(vertices) / 3;
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glGenVertexArrays(1, &gl->vao);
  glBindVertexArray(gl->vao);
  glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glGenVertexArrays(1, &gl->frustum_vis_vao);
  glGenBuffers(1, &gl->frustum_vis_vbo);
  glBindVertexArray(gl->frustum_vis_vao);
  glBindBuffer(GL_ARRAY_BUFFER, gl->frustum_vis_vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glGenTextures(1, &gl->white_tex_id);
  glBindTexture(GL_TEXTURE_2D, gl->white_tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  uint8_t white_tex_pixels[4] = {255, 255, 255, 0};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE,
               white_tex_pixels);

  char *vertex_shader_source = read_file("src/shaders/to_screen_space.vert");
  assert(vertex_shader_source != NULL);

  char *fragment_shader_source = read_file("src/shaders/blit.frag");
  assert(fragment_shader_source != NULL);

  gl->shader_program =
      create_shader(vertex_shader_source, fragment_shader_source);
  free(vertex_shader_source);
  free(fragment_shader_source);
  assert(gl->shader_program);

  char *model_vertex_shader_source = read_file("src/shaders/model_view.vert");
  assert(model_vertex_shader_source != NULL);

  char *get_color_fragment_shader_source =
      read_file("src/shaders/get_color.frag");
  assert(get_color_fragment_shader_source != NULL);

  gl->poly_shader_program = create_shader(model_vertex_shader_source,
                                          get_color_fragment_shader_source);
  free(model_vertex_shader_source);
  free(get_color_fragment_shader_source);
  assert(gl->poly_shader_program);

  for (int i = 0; i < MAP_COUNT; i++) {
    create_map_gl_data(&game->maps[i]);
  }
}

static int32_t load_map(struct Map *map, struct MapEntry *map_entry) {
  map->color_map.pixels =
      stbi_load(map_entry->color, &map->color_map.width, &map->color_map.height,
                &map->color_map.num_channels, 0);
  if (map->color_map.pixels == NULL) {
    error(stbi_failure_reason());
    error("Could not load color map");
    return GAME_ERROR;
  }

  map->height_map.pixels =
      stbi_load(map_entry->height, &map->height_map.width,
                &map->height_map.height, &map->height_map.num_channels, 0);
  if (map->height_map.pixels == NULL) {
    error(stbi_failure_reason());
    error("Could not load height map");
    return GAME_ERROR;
  }

  // Add on to the height map width to account for the extra column we add
  map->modifier = (float)BASE_MAP_SIZE / (map->height_map.width + 1);

  return GAME_SUCCESS;
}

static int32_t load_assets(struct Game *game) {
  for (int i = 0; i < MAP_COUNT; ++i) {
    struct Map *map = &game->maps[i];
    struct MapEntry *map_entry = &maps[i];
    if (load_map(map, map_entry) != GAME_SUCCESS) {
      error("Failed to load map %s : %s", map_entry->color, map_entry->height);
      return GAME_ERROR;
    }
  }

  return GAME_SUCCESS;
}

static void create_frame_buffer(struct Game *game, int32_t width,
                                int32_t height) {
  game->frame.width = width;
  game->frame.height = height;
  // frame.width = 2528;
  // frame.height = 1408;
  game->frame.clip_left_x = 0;
  game->frame.clip_right_x = game->frame.width,
  game->frame.y_buffer = malloc(game->frame.width * sizeof(int32_t));
  game->frame.pixels =
      malloc(game->frame.width * game->frame.height * sizeof(uint8_t) * 4);
  game->frame.pitch = game->frame.width * sizeof(uint32_t);
}

int32_t game_init(struct Game *game, int32_t width, int32_t height) {
  if (load_assets(game) == GAME_ERROR) {
    return GAME_ERROR;
  }

  memset(&game->keyboard, 0, sizeof(game->keyboard));
  memset(&game->prev_keyboard, 0, sizeof(game->keyboard));
  memset(&game->prev_controller[LEFT_CONTROLLER_INDEX], 0,
         sizeof(game->prev_controller[0]));
  memset(&game->prev_controller[RIGHT_CONTROLLER_INDEX], 0,
         sizeof(game->prev_controller[0]));
  memset(&game->controller[LEFT_CONTROLLER_INDEX], 0,
         sizeof(game->controller[0]));
  memset(&game->controller[RIGHT_CONTROLLER_INDEX], 0,
         sizeof(game->controller[0]));

  create_frame_buffer(game, width, height);

  game->camera = (struct Camera){
      .viewport_width = width,
      .viewport_height = height,
      .quat = GLM_QUAT_IDENTITY_INIT,
      .scale_height = game->frame.height * 0.35,
      .position =
          {
              436.0f / 1024.0f,
              200.0f / 1024.0f,
              54.0f / 1024.0f,
          },
      .pitch = M_PI,
      .last_hmd_position = {0.0f, 0.0f, 0.0f},
      .terrain_scale = 1.00f,
      .ray =
          {
              .horizon = game->frame.height / 2,
              .clip = .06f * game->frame.width,
              .distance = 800,
          },
  };

  create_gl_objects(game);
  game->map_index = 0;
  game->options.visualize_lod = false;
  game->options.visualize_frustum = false;
  game->options.show_wireframe = false;
  game->render_commands.capacity = sizeof(game->render_commands.commands) /
                                   sizeof(game->render_commands.commands[0]);
  for (int i = 0; i < 2; ++i) {
    game->trigger_set[i] = true;
  }

  return GAME_SUCCESS;
}

void game_free(struct Game *game) {
  for (int i = 0; i < MAP_COUNT; ++i) {
    struct Map *map = &game->maps[i];
    if (map->color_map.pixels != NULL) {
      stbi_image_free(map->color_map.pixels);
      map->color_map.pixels = NULL;
    }

    if (map->height_map.pixels != NULL) {
      stbi_image_free(map->height_map.pixels);
      map->height_map.pixels = NULL;
    }
  }

  if (game->frame.y_buffer != NULL) {
    free(game->frame.y_buffer);
    game->frame.y_buffer = NULL;
  }

  if (game->frame.pixels != NULL) {
    free(game->frame.pixels);
    game->frame.pixels = NULL;
  }
}
