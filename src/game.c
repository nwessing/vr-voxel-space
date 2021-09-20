#include "game.h"
#include "assert.h"
#include "cglm/affine.h"
#include "file.h"
#include "image.h"
#include "math.h"
#include "platform.h"
#include "raycasting.h"
#include "shader.h"
#include "string.h"
#include "types.h"
#include "util.h"

static vec3 CAMERA_TO_TERRAIN = {1024.0, 255.0, 1024.0};

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

static void render_real_3d(struct Game *game, mat4 in_projection_matrix,
                           mat4 in_view_matrix) {
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glClearColor(0.529f, 0.808f, 0.98f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  struct Camera *camera = &game->camera;
  vec3 camera_offset = {-camera->position_x, -camera->position_y,
                        -camera->position_z};
  glm_vec3_mul(camera_offset, CAMERA_TO_TERRAIN, camera_offset);
  glm_vec3_scale(camera_offset, camera->terrain_scale, camera_offset);

  mat4 view_matrix = GLM_MAT4_IDENTITY_INIT;
  glm_rotate(view_matrix, -camera->pitch, (vec3){0.0, 1.0, 0.0});
  glm_translate(view_matrix, camera_offset);

  glm_mat4_mul(in_view_matrix, view_matrix, view_matrix);

  struct OpenGLData *gl = &game->gl;
  struct Map *map = &game->maps[game->map_index];
  glUseProgram(gl->poly_shader_program);
  glBindVertexArray(map->map_vao);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, map->color_map_tex_id);

  mat4 projection_view = GLM_MAT4_IDENTITY_INIT;
  glm_mat4_mul(in_projection_matrix, view_matrix, projection_view);

  glUniform2i(glGetUniformLocation(gl->poly_shader_program, "heightMapSize"),
              map->height_map.width, map->height_map.height);

  // TODO: rendering 9 big meshes is too slow on quest, need to find a more
  // efficient way to do this.
  /* for (int32_t i = 0; i < 9; ++i) { */
  for (int32_t i = 4; i < 5; ++i) {
    int32_t x = (i / 3) - 1;
    int32_t z = (i % 3) - 1;

    mat4 model = GLM_MAT4_IDENTITY_INIT;
    float map_modifier = 1024.0f / map->height_map.width;
    glm_scale(model, (vec3){camera->terrain_scale * map_modifier,
                            camera->terrain_scale,
                            camera->terrain_scale * map_modifier});
    vec4 translate = {x * 1024.0f, 0.0f, z * 1024.0f};
    glm_translate(model, translate);
    mat4 mvp = GLM_MAT4_IDENTITY_INIT;
    glm_mat4_mul(projection_view, model, mvp);
    glUniformMatrix4fv(glGetUniformLocation(gl->poly_shader_program, "mvp"), 1,
                       GL_FALSE, (float *)mvp);
    glDrawElements(GL_TRIANGLES, map->num_map_vbo_indices, GL_UNSIGNED_INT,
                   (void *)0);
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

void render_game(struct Game *game, mat4 projection, mat4 view_matrix) {
  glViewport(0, 0, game->camera.viewport_width, game->camera.viewport_height);

  /* if (game->options.do_raycasting) { */
  /*   memset(game->frame.pixels, 0, game->frame.height * game->frame.pitch); */
  /*   if (game->options.render_stereo) { */
  /*     for (int eye = 0; eye < 2; ++eye) { */
  /*       struct FrameBuffer eye_buffer; */
  /*       eye_buffer.width = game->frame.width / 2; */
  /*       eye_buffer.height = game->frame.height; */
  /*       eye_buffer.pitch = game->frame.pitch; */
  /*       eye_buffer.y_buffer = &game->frame.y_buffer[eye * (eye_buffer.width /
   * 2)]; */
  /*       eye_buffer.clip_left_x = eye == 0 ? 0 : game->camera.clip; */
  /*       eye_buffer.clip_right_x = eye_buffer.width - (eye == 0 ?
   * game->camera.clip : 0); */
  /*       eye_buffer.pixels = eye == 0 ? game->frame.pixels :
   * &game->frame.pixels[(eye_buffer.width - game->camera.clip * 2) * 4]; */

  /*       int eye_mod = eye == 1 ? 1 : -1; */
  /*       int eye_dist = 3; */
  /*       struct Camera eye_cam = game->camera; */
  /*       eye_cam.position_x += (int)(eye_mod * eye_dist * sin(eye_cam.pitch +
   * (M_PI / 2))); */
  /*       eye_cam.position_y += (int)(eye_mod * eye_dist * cos(eye_cam.pitch +
   * (M_PI / 2))); */

  /*       render(&eye_buffer, &game->color_map, &game->height_map, &eye_cam);
   */
  /*     } */
  /*   } else { */
  /*     render(&game->frame, &game->color_map, &game->height_map,
   * &game->camera); */
  /*   } */

  /*   render_buffer_to_gl(&game->frame, &game->gl, game->camera.clip); */
  /* } else { */
  render_real_3d(game, projection, view_matrix);
  /* } */
}

static inline bool is_key_pressed(struct KeyboardState *keyboard, int32_t key) {
  assert(key >= KEYBOAD_STATE_MIN_CHAR && key <= KEYBOARD_STATE_MAX_CHAR);
  return keyboard->down[key - KEYBOAD_STATE_MIN_CHAR];
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

void update_game(struct Game *game, struct KeyboardState *keyboard,
                 struct ControllerState *left_controller,
                 struct ControllerState *right_controller, float elapsed) {
  game->prev_keyboard = game->keyboard;
  game->keyboard = *keyboard;

  game->prev_controller[LEFT_CONTROLLER_INDEX] =
      game->controller[LEFT_CONTROLLER_INDEX];
  game->prev_controller[RIGHT_CONTROLLER_INDEX] =
      game->controller[RIGHT_CONTROLLER_INDEX];
  game->controller[LEFT_CONTROLLER_INDEX] = *left_controller;
  game->controller[RIGHT_CONTROLLER_INDEX] = *right_controller;

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
  if (left_controller->primary_button &&
      left_controller_prev->primary_button != left_controller->primary_button) {
    game->camera.is_z_relative_to_ground =
        !game->camera.is_z_relative_to_ground;
  }

  bool change_map_mode = game->controller[RIGHT_CONTROLLER_INDEX].grip > 0.9f;
  if ((is_key_pressed(&game->keyboard, 'j') &&
       !is_key_pressed(&game->prev_keyboard, 'j')) ||
      (game->trigger_set[LEFT_CONTROLLER_INDEX] && change_map_mode &&
       game->controller[LEFT_CONTROLLER_INDEX].trigger > 0.9f)) {
    game->trigger_set[LEFT_CONTROLLER_INDEX] = false;
    --game->map_index;
    if (game->map_index < 0) {
      game->map_index = MAP_COUNT - 1;
    }
  } else if ((is_key_pressed(&game->keyboard, 'k') &&
              !is_key_pressed(&game->prev_keyboard, 'k')) ||
             (game->trigger_set[RIGHT_CONTROLLER_INDEX] && change_map_mode &&
              game->controller[RIGHT_CONTROLLER_INDEX].trigger > 0.9f)) {
    game->trigger_set[RIGHT_CONTROLLER_INDEX] = false;
    ++game->map_index;
    if (game->map_index >= MAP_COUNT) {
      game->map_index = 0;
    }
  }

  for (int i = 0; i < 2; ++i) {
    if (game->controller[i].trigger < 0.1) {
      game->trigger_set[i] = true;
    }
  }

  struct Map *map = &game->maps[game->map_index];
  float prev_ground_height = get_ground_height(
      &map->height_map, game->camera.position_x, game->camera.position_z);
  vec3 direction_intensities = {horizontal_movement, vertical_movement,
                                -forward_movement};
  vec3 movement;
  {
    vec3 forward;
    get_forward_vector(&game->camera, direction_intensities, true, forward);

    float units_per_second = 0.25f;
    float delta_units = units_per_second * elapsed;
    glm_vec3_scale(forward, delta_units, movement);
    glm_vec3_clamp(movement, -delta_units, delta_units);
  }

  game->camera.position_x += movement[0];
  game->camera.position_y += movement[1];
  game->camera.position_z += movement[2];

  game->camera.pitch +=
      -rotation_movement * M_PI *
      (right_controller->scale_rotation_by_time ? elapsed : 1.0f);

  if (game->camera.is_z_relative_to_ground) {
    float ground_height = get_ground_height(
        &map->height_map, game->camera.position_x, game->camera.position_z);
    float ground_height_diff = ground_height - prev_ground_height;
    game->camera.position_y = clamp(
        game->camera.position_y + ground_height_diff, ground_height, 10000.0f);
  }

  while (game->camera.pitch >= 2 * M_PI) {
    game->camera.pitch -= 2 * M_PI;
  }

  while (game->camera.pitch < 0) {
    game->camera.pitch += 2 * M_PI;
  }

  while (game->camera.position_x >= 1.0) {
    game->camera.position_x -= 1.0;
  }

  while (game->camera.position_z >= 1.0) {
    game->camera.position_z -= 1.0;
  }

  while (game->camera.position_x < 0) {
    game->camera.position_x += 1.0;
  }

  while (game->camera.position_z < 0) {
    game->camera.position_z += 1.0;
  }

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

  int32_t num_map_vertices = (map->height_map.width * map->height_map.height);
  V3 *map_vertices = malloc(sizeof(V3) * num_map_vertices);

  int32_t indices_per_vert = 6;
  int32_t num_indices = 0;

  // NOTE: allocating slightly more space than we need since we don't generate
  // indices for the final row and column
  int32_t *index_buffer =
      malloc(sizeof(int32_t) * num_map_vertices * indices_per_vert);

  for (int32_t y = 0; y < map->height_map.height; y++) {
    for (int32_t x = 0; x < map->height_map.width; x++) {
      int32_t v_index = ((y * map->height_map.width) + x);
      map_vertices[v_index][0] = x;
      map_vertices[v_index][1] = (float)get_image_grey(&map->height_map, x, y);
      map_vertices[v_index][2] = y;
      assert(v_index < num_map_vertices);

      if (x >= map->height_map.width - 1 || y >= map->height_map.height - 1) {
        continue;
      }

      int32_t i_index = v_index * indices_per_vert;

      index_buffer[i_index] = v_index;
      index_buffer[i_index + 1] = v_index + map->height_map.width;
      index_buffer[i_index + 2] = v_index + 1;

      index_buffer[i_index + 3] = v_index + map->height_map.width;
      index_buffer[i_index + 4] = v_index + map->height_map.width + 1;
      index_buffer[i_index + 5] = v_index + 1;
      num_indices += indices_per_vert;
      assert(i_index + 5 < (indices_per_vert * num_map_vertices));
    }
  }
  info("# of vertices in terrain mesh: %i\n", num_indices);

  glGenVertexArrays(1, &map->map_vao);
  glBindVertexArray(map->map_vao);

  glGenBuffers(1, &map->map_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, map->map_vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  map->map_vao_num_vertices = num_map_vertices;
  glBufferData(GL_ARRAY_BUFFER, num_map_vertices * sizeof(V3), map_vertices,
               GL_STATIC_DRAW);
  free(map_vertices);

  glGenBuffers(1, &map->map_vbo_indices);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, map->map_vbo_indices);
  map->num_map_vbo_indices = num_indices;
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
  /* info("color = %s, height = %s\n", map_entry->color, map_entry->height); */
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

  game->camera = (struct Camera){.viewport_width = width,
                                 .viewport_height = height,
                                 .distance = 800,
                                 .quat = GLM_QUAT_IDENTITY_INIT,
                                 .pitch = M_PI,
                                 .horizon = game->frame.height / 2,
                                 .scale_height = game->frame.height * 0.35,
                                 .position_x = 436.0 / 1024.0,
                                 .position_z = 54 / 1024.0,
                                 .position_y = 200.0f / 255.0,
                                 .terrain_scale = 0.5f,
                                 .clip = .06f * game->frame.width,
                                 .is_z_relative_to_ground = false};

  create_gl_objects(game);
  game->map_index = 0;
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
