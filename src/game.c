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
#include <stdlib.h>

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

void update_world_section_distances(struct Map *map,
                                    struct RenderState *render_state,
                                    vec3 camera_position) {
  for (int32_t i = 0; i < render_state->num_sections; ++i) {
    struct WorldSection *section = &render_state->sections_by_distance[i];
    vec3 section_center = {0, 0, 0};
    section_center[0] += section->map_x * (float)BASE_MAP_SIZE;
    section_center[2] += section->map_y * (float)BASE_MAP_SIZE;

    glm_vec3_add(section_center, map->sections[section->section_index].center,
                 section_center);
    section->camera_distance =
        glm_vec3_distance(camera_position, section_center);
  }
}

void sort_world_sections(struct WorldSection sections[], int32_t first,
                         int32_t last) {
  if (first < 0 || last < 0 || last <= first) {
    return;
  }

  float pivot = sections[(first + last) / 2].camera_distance;

  int32_t i = first - 1;
  int32_t j = last + 1;

  while (true) {
    do {
      ++i;
    } while (sections[i].camera_distance < pivot);

    do {
      --j;
    } while (sections[j].camera_distance > pivot);

    if (i >= j) {
      sort_world_sections(sections, first, j);
      sort_world_sections(sections, j + 1, last);
      return;
    }

    struct WorldSection temp = sections[i];
    sections[i] = sections[j];
    sections[j] = temp;
  }
}

static void hmd_position_to_world_position(struct Game *game, vec3 hmd_position,
                                           vec3 out) {
  // Allow hmd_position and out to be the same
  vec3 copy_hmd_position;
  glm_vec3_copy(hmd_position, copy_hmd_position);

  float scale_factor = 1.0f / game->camera.terrain_scale;
  vec3 scaler = {scale_factor, scale_factor, scale_factor};
  glm_vec3_div(scaler, CAMERA_TO_TERRAIN, out);
  glm_vec3_mul(out, copy_hmd_position, out);
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
                                           vec4 frustum_planes[6], int32_t x,
                                           int32_t z, int32_t i_section) {
  struct Camera *camera = &game->camera;
  vec3 translate = {x * (BASE_MAP_SIZE - map->modifier), 0.0f,
                    z * (BASE_MAP_SIZE - map->modifier)};
  mat4 model = GLM_MAT4_IDENTITY_INIT;
  vec3 map_scaler = {camera->terrain_scale, camera->terrain_scale,
                     camera->terrain_scale};
  glm_scale(model, map_scaler);
  glm_translate(model, translate);

  if (game->render_state.num_commands == game->render_state.capacity) {
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
      return;
    }
  }

  float distance = glm_vec3_distance(cam_terrain_position, section_center);
  int32_t lod_index = 0;
  if (distance <= 256.0f) {
    lod_index = 0;
  } else if (distance < 512.0) {
    lod_index = 1;
  } else {
    lod_index = 2;
  }

  struct Mesh *mesh = &section->lods[lod_index];
  struct DrawCommand *draw_command =
      &game->render_state.commands[game->render_state.num_commands];
  ++game->render_state.num_commands;
  draw_command->mesh = *mesh;
  draw_command->lod = lod_index;
  glm_mat4_copy(model, draw_command->model_matrix);
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

  vec3 camera_position;
  glm_vec3_mul(game->camera.position, CAMERA_TO_TERRAIN, camera_position);

  update_world_section_distances(&game->maps[game->map_index],
                                 &game->render_state, camera_position);
  sort_world_sections(game->render_state.sections_by_distance, 0,
                      game->render_state.num_sections - 1);

  // Through manual inspection, 70 sections appears to be the lower bound of
  // what we can draw at the current fog level to hide all pop-in
  game->render_state.num_commands = 0;
  for (int32_t i = 0; i < game->render_state.num_sections &&
                      game->render_state.num_commands < 70;
       ++i) {
    struct WorldSection *section = &game->render_state.sections_by_distance[i];
    generate_draw_commands_for_map(game, map, frustum_planes, section->map_x,
                                   section->map_y, section->section_index);
  }
}

static void render_hands(struct Game *game, struct Map *map,
                         struct RenderingMatrices *matrices) {
  struct OpenGLData *gl = &game->gl;
  glBindVertexArray(gl->cube_buffer.vao);
  glUseProgram(gl->hand_shader);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, map->color_map_tex_id);

  glUniform1i(gl->hand_shader_uniforms.color_map, 0);

  for (uint32_t i = 0; i < 2; ++i) {
    struct ControllerState *controller = &game->controller[i];
    if (!controller->is_connected) {
      continue;
    }

    vec3 camera_position;
    glm_vec3_mul(game->camera.position, CAMERA_TO_TERRAIN, camera_position);
    glm_vec3_scale(camera_position, game->camera.terrain_scale,
                   camera_position);

    vec3 inv_hmd_position;
    glm_vec3_scale(game->camera.last_hmd_position, -1.0f, inv_hmd_position);

    mat4 camera_transform = GLM_MAT4_IDENTITY_INIT;
    glm_translate(camera_transform, camera_position);
    glm_rotate(camera_transform, game->camera.pitch, (vec3){0.0, 1.0, 0.0});
    glm_translate(camera_transform, inv_hmd_position);

    mat4 model = GLM_MAT4_IDENTITY_INIT;
    glm_translate(model, controller->pose.position);
    glm_quat_rotate(model, controller->pose.orientation, model);
    glm_scale(model, (vec3){0.1f, 0.1f, 0.1f});
    glm_mat4_mul(camera_transform, model, model);

    for (uint32_t i = 0; i < (matrices->enable_stereo ? 2 : 1); ++i) {
      mat4 mvp;
      glm_mat4_mul(matrices->projection_view_matrices[i], model, mvp);
      glUniformMatrix4fv(gl->hand_shader_uniforms.mvp[i], 1, GL_FALSE,
                         (float *)mvp);
    }

    glDrawElements(GL_TRIANGLES, gl->cube_buffer.index_count, GL_UNSIGNED_INT,
                   gl->cube_buffer.index_offset);
  }
}

static void render_real_3d(struct Game *game,
                           struct RenderingMatrices *matrices) {
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

#ifdef GL_POLYGON_MODE
  if (game->options.show_wireframe) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
#endif

  int32_t eye_count = matrices->enable_stereo ? 2 : 1;

  vec4 *sky_color = &game->camera.sky_color;
  glClearColor((*sky_color)[0], (*sky_color)[1], (*sky_color)[2],
               (*sky_color)[3]);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  struct OpenGLData *gl = &game->gl;
  struct Map *map = &game->maps[game->map_index];

  mat4 birdseye_projection_view[2] = {GLM_MAT4_IDENTITY_INIT,
                                      GLM_MAT4_IDENTITY_INIT};
  if (game->options.visualize_frustum) {
    float middle = (1.0f / 2.0f) * BASE_MAP_SIZE;
    vec3 frustum_vis_eye = {middle, 4000.0f, middle};
    vec3 frustum_vis_up = {0.0f, 0.0f, -1.0f};
    vec3 frustum_vis_center = {middle, 0.0f, middle};
    mat4 birdseye_view_matrix = GLM_MAT4_IDENTITY_INIT;
    // TODO use orthographic projection?
    glm_lookat(frustum_vis_eye, frustum_vis_center, frustum_vis_up,
               birdseye_view_matrix);

    for (int32_t eye = 0; eye < eye_count; ++eye) {
      glm_mat4_mul(matrices->projection_matrices[eye], birdseye_view_matrix,
                   birdseye_projection_view[eye]);
    }
  } else {
    render_hands(game, map, matrices);
  }

  glUseProgram(gl->terrain_shader);
  glBindVertexArray(map->map_vao);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, map->color_map_tex_id);

  struct TerrainShaderUniforms *uniforms = &game->gl.terrain_shader_uniforms;
  glUniform2i(uniforms->height_map_size, BASE_MAP_SIZE, BASE_MAP_SIZE);
  glUniform4fv(uniforms->fog_color, 1, *sky_color);
  glUniform1f(uniforms->terrain_scale, game->camera.terrain_scale);
  glUniform1ui(uniforms->flags,
               game->options.visualize_frustum || !game->options.show_fog ? 0
                                                                          : 1);

  vec3 camera_world_position;
  glm_vec3_mul(game->camera.position, CAMERA_TO_TERRAIN, camera_world_position);
  glm_vec3_scale(camera_world_position, game->camera.terrain_scale,
                 camera_world_position);
  glUniform3fv(uniforms->camera_position, 1, camera_world_position);

  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, gl->draw_command_vbo);
  int32_t draw_indirect_buffer_size;
  glGetBufferParameteriv(GL_DRAW_INDIRECT_BUFFER, GL_BUFFER_SIZE,
                         &draw_indirect_buffer_size);
  int32_t required_buffer_size =
      game->render_state.num_commands *
      (int32_t)sizeof(struct DrawElementsIndirectCommand);

  if (draw_indirect_buffer_size < required_buffer_size) {
    info("Reallocating indirect draw buffer to %d bytes\n",
         required_buffer_size);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, required_buffer_size, NULL,
                 GL_DYNAMIC_DRAW);
    draw_indirect_buffer_size = required_buffer_size;
  }

  struct DrawElementsIndirectCommand *gl_commands =
      glMapBufferRange(GL_DRAW_INDIRECT_BUFFER, 0, draw_indirect_buffer_size,
                       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
  if (gl_commands == NULL) {
    GLenum gl_error = glGetError();
    error("gl_commands was NULL: %d\n", gl_error);
    assert(gl_commands != NULL);
  }

  // Populate indirect draw commands
  for (int32_t i_command = 0; i_command < game->render_state.num_commands;
       ++i_command) {
    struct DrawCommand *command = &game->render_state.commands[i_command];

    struct DrawElementsIndirectCommand *gl_command = &gl_commands[i_command];
    gl_command->count = command->mesh.num_indices;
    gl_command->instance_count = 1;
    gl_command->first_index = command->mesh.offset;
    gl_command->base_vertex = 0;
    gl_command->reserved_must_be_zero = 0;
  }
  assert(glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER) == GL_TRUE);

  for (int32_t i_command = 0; i_command < game->render_state.num_commands;
       ++i_command) {
    struct DrawCommand *command = &game->render_state.commands[i_command];
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

    mat4 mvp[2] = {GLM_MAT4_IDENTITY_INIT, GLM_MAT4_IDENTITY_INIT};
    for (int32_t eye = 0; eye < eye_count; ++eye) {
      if (game->options.visualize_frustum) {
        glm_mat4_mul(birdseye_projection_view[eye], command->model_matrix,
                     mvp[eye]);
      } else {
        glm_mat4_mul(matrices->projection_view_matrices[eye],
                     command->model_matrix, mvp[eye]);
      }
    }

    glUniformMatrix4fv(uniforms->projection_views[0], 1, GL_FALSE,
                       (float *)mvp[0]);
    if (matrices->enable_stereo) {
      glUniformMatrix4fv(uniforms->projection_views[1], 1, GL_FALSE,
                         (float *)mvp[1]);
    }
    glUniformMatrix4fv(uniforms->model, 1, GL_FALSE,
                       (float *)command->model_matrix);

    glUniform4fv(uniforms->blend_color, 1, blend_color);

    glDrawElementsIndirect(
        GL_TRIANGLES, GL_UNSIGNED_INT,
        (void *)(i_command * sizeof(struct DrawElementsIndirectCommand)));
  }

  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

  glBindVertexArray(0);

  if (game->options.visualize_frustum) {
    mat4 inv_projection_view;

    // NOTE indices for each specific corner are not part of the API
    vec3 verts[6];
    glm_mat4_inv(matrices->projection_view_matrices[0], inv_projection_view);
    vec4 frustum_verts[8];
    glm_frustum_corners(inv_projection_view, frustum_verts);
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

    if (matrices->enable_stereo) {
      glm_mat4_inv(matrices->projection_view_matrices[1], inv_projection_view);
      // top right far
      glm_vec3(frustum_verts[6], verts[1]);
      // top right near
      glm_vec3(frustum_verts[2], verts[3]);
      // top right far
      glm_vec3(frustum_verts[6], verts[4]);
    }

    glBindVertexArray(game->gl.frustum_vis_vao);
    glBindBuffer(GL_ARRAY_BUFFER, game->gl.frustum_vis_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

    glUseProgram(game->gl.terrain_shader);

    vec4 blend_color = {1.0, 1.0, 0.0, 0.5};
    glUniform4fv(gl->terrain_shader_uniforms.blend_color, 1, blend_color);

    glUniformMatrix4fv(gl->terrain_shader_uniforms.projection_views[0], 1,
                       GL_FALSE, (float *)birdseye_projection_view[0]);
    if (matrices->enable_stereo) {
      glUniformMatrix4fv(gl->terrain_shader_uniforms.projection_views[1], 1,
                         GL_FALSE, (float *)birdseye_projection_view[1]);
    }
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

  glViewport(0, 0, matrices->framebuffer_width, matrices->framebuffer_height);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, matrices->framebuffer);
  render_real_3d(game, &rendering_matrices);
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
                 struct ControllerState *right_controller, struct Pose hmd_pose,
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
    glm_quat_copy(hmd_pose.orientation, game->camera.quat);
    // Update position based on the the current HMD position

    // Use the different between current position and last reported position
    vec3 hmd_difference;
    glm_vec3_sub(hmd_pose.position, game->camera.last_hmd_position,
                 hmd_difference);

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
    glm_vec3_copy(hmd_pose.position, game->camera.last_hmd_position);
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

  game->options.show_fog = !is_key_pressed(&game->keyboard, 'h') &&
                           game->controller[LEFT_CONTROLLER_INDEX].grip < 0.5f;

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
    struct Rect rect = {.x = (i_section % MAP_X_SEGMENTS) * section_width,
                        .y = (i_section / MAP_Y_SEGMENTS) * section_height,
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

static void create_terrain_shader(struct OpenGLData *gl) {

  char *model_vertex_shader_source = read_file("src/shaders/model_view.vert");
  assert(model_vertex_shader_source != NULL);

  char *get_color_fragment_shader_source =
      read_file("src/shaders/get_color.frag");
  assert(get_color_fragment_shader_source != NULL);

  gl->terrain_shader = create_shader(model_vertex_shader_source,
                                     get_color_fragment_shader_source);

  free(model_vertex_shader_source);
  free(get_color_fragment_shader_source);
  assert(gl->terrain_shader);

  gl->terrain_shader_uniforms.height_map_size =
      glGetUniformLocation(gl->terrain_shader, "heightMapSize");
  gl->terrain_shader_uniforms.fog_color =
      glGetUniformLocation(gl->terrain_shader, "fogColor");
  gl->terrain_shader_uniforms.terrain_scale =
      glGetUniformLocation(gl->terrain_shader, "terrainScale");
  gl->terrain_shader_uniforms.flags =
      glGetUniformLocation(gl->terrain_shader, "flags");
  gl->terrain_shader_uniforms.camera_position =
      glGetUniformLocation(gl->terrain_shader, "cameraPosition");
  gl->terrain_shader_uniforms.projection_views[0] =
      glGetUniformLocation(gl->terrain_shader, "projectionViews[0]");
  gl->terrain_shader_uniforms.projection_views[1] =
      glGetUniformLocation(gl->terrain_shader, "projectionViews[1]");
  gl->terrain_shader_uniforms.model =
      glGetUniformLocation(gl->terrain_shader, "model");
  gl->terrain_shader_uniforms.blend_color =
      glGetUniformLocation(gl->terrain_shader, "blendColor");
}

static void create_hand_shader(struct OpenGLData *gl) {
  char *vertex_shader_source = read_file("src/shaders/hand.vert");
  assert(vertex_shader_source != NULL);

  char *fragment_shader_source = read_file("src/shaders/hand.frag");
  assert(fragment_shader_source != NULL);

  gl->hand_shader = create_shader(vertex_shader_source, fragment_shader_source);

  free(vertex_shader_source);
  free(fragment_shader_source);
  assert(gl->hand_shader);

  gl->hand_shader_uniforms.color_map =
      glGetUniformLocation(gl->hand_shader, "colorMap");
  gl->hand_shader_uniforms.mvp[0] =
      glGetUniformLocation(gl->hand_shader, "mvp[0]");
  gl->hand_shader_uniforms.mvp[1] =
      glGetUniformLocation(gl->hand_shader, "mvp[1]");
}

static void create_cube_buffer(struct CubeBuffer *buffer) {
  glGenVertexArrays(1, &buffer->vao);
  glGenBuffers(1, &buffer->vertex_vbo);

  glBindVertexArray(buffer->vao);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vertex_vbo);

  GLsizei stride = 5 * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        (void *)(3 * sizeof(float)));
  glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices,
               GL_STATIC_DRAW);

  glGenBuffers(1, &buffer->index_vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->index_vbo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices,
               GL_STATIC_DRAW);

  buffer->index_count = sizeof(cube_indices) / sizeof(cube_indices[0]);
  buffer->index_offset = 0;

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
  glBindVertexArray(0);

  glGenTextures(1, &gl->white_tex_id);
  glBindTexture(GL_TEXTURE_2D, gl->white_tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  uint8_t white_tex_pixels[4] = {255, 255, 255, 0};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE,
               white_tex_pixels);

  glGenBuffers(1, &gl->draw_command_vbo);
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, gl->draw_command_vbo);
  // Reserve space for 500 draw commands
  glBufferData(GL_DRAW_INDIRECT_BUFFER,
               500 * sizeof(struct DrawElementsIndirectCommand), NULL,
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

  char *vertex_shader_source = read_file("src/shaders/to_screen_space.vert");
  assert(vertex_shader_source != NULL);

  char *fragment_shader_source = read_file("src/shaders/blit.frag");
  assert(fragment_shader_source != NULL);

  gl->shader_program =
      create_shader(vertex_shader_source, fragment_shader_source);
  free(vertex_shader_source);
  free(fragment_shader_source);
  assert(gl->shader_program);

  create_terrain_shader(gl);
  create_hand_shader(gl);
  create_cube_buffer(&gl->cube_buffer);

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
      .sky_color = {0.529f, 0.808f, 0.98f, 1.0f},
  };

  create_gl_objects(game);
  game->map_index = 0;
  game->options.visualize_lod = false;
  game->options.visualize_frustum = false;
  game->options.show_wireframe = false;
  game->render_state.capacity = sizeof(game->render_state.commands) /
                                sizeof(game->render_state.commands[0]);

  int32_t map_min = -3, map_max = 3;
  assert(
      (map_max - map_min) * 2 * MAP_SECTION_COUNT <=
          (int32_t)(sizeof(game->render_state.sections_by_distance) /
                    sizeof(game->render_state.sections_by_distance[0])) &&
      "The capacity of RenderState's sections_by_distance is not high enough");
  int32_t *i_section = &game->render_state.num_sections;
  for (int32_t map_x = map_min; map_x <= map_max; ++map_x) {
    for (int32_t map_y = map_min; map_y <= map_max; ++map_y) {
      for (int32_t section = 0; section < MAP_SECTION_COUNT; ++section) {
        struct WorldSection *world_section =
            &game->render_state.sections_by_distance[*i_section];
        world_section->map_x = map_x;
        world_section->map_y = map_y;
        world_section->section_index = section;
        world_section->camera_distance = 0.0f;
        ++(*i_section);
      }
    }
  }

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
