#include "types.h"
#include "game.h"
#include "math.h"
#include "file.h"
#include "assert.h"
#include "shader.h"
#include "image.h"
#include "raycasting.h"
#include "string.h"
#include "platform.h"

static void render_real_3d(struct OpenGLData *gl, struct Camera *camera, mat4 in_projection_matrix) {
  glEnable(GL_DEPTH_TEST);
  glClearColor(0.529f, 0.808f, 0.98f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
 
  // NOTE: Flip the z-axis so that x increases to the camera's right
  // matching the way the map appears in the image
  vec3 position = {camera->position_x, camera->position_y, -camera->position_height};

  // Forward is +y direction.
  vec3 center = {0, 1, 0};

  // Find the direction vector indicating where the camera is pointing
  mat4 rotate = GLM_MAT4_IDENTITY_INIT;
  glm_quat_rotate(rotate, camera->quat, rotate);

  mat4 translation = GLM_MAT4_IDENTITY_INIT;
  glm_translate(translation, position);

  glm_mat4_mul(translation, rotate, rotate);
  glm_mat4_mulv3(rotate, center, 1, center);

  vec3 up = {0.0, 0.0, -1.0};
  mat4 view_matrix = GLM_MAT4_IDENTITY;
  glm_lookat(position, center, up, view_matrix);

  glUseProgram(gl->poly_shader_program);
	glUniformMatrix4fv(glGetUniformLocation(gl->poly_shader_program, "view"), 1, GL_FALSE, (float *) view_matrix);
	glUniformMatrix4fv(glGetUniformLocation(gl->poly_shader_program, "projection"), 1, GL_FALSE, (float *) in_projection_matrix);
  glBindVertexArray(gl->map_vao);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gl->color_map_tex_id);


  // TODO: rendering 9 big meshes is too slow on quest, need to find a more efficient way to do this.
  /* for (int32_t i = 0; i < 9; ++i) { */
  for (int32_t i = 4; i < 5; ++i) {
    int32_t x = (i / 3) - 1;
    int32_t y = (i % 3) - 1;


    mat4 model = GLM_MAT4_IDENTITY;
    vec4 translate = {x * 1024.0f, y * 1024.0f, 0.0};
    glm_translate(model, translate);
    glUniformMatrix4fv(glGetUniformLocation(gl->poly_shader_program, "model"), 1, GL_FALSE, (float *) model);
    glDrawElements(GL_TRIANGLES, gl->num_map_vbo_indices, GL_UNSIGNED_INT, (void*)0);
  }
}

static void check_opengl_error(char *name) {
  GLenum gl_error = glGetError();
  if (gl_error) {
    error("(%s) error: %i\n", name, gl_error);
  }
}

static void render_buffer_to_gl(struct FrameBuffer *frame, struct OpenGLData *gl, int clip) {
  glEnable(GL_DEPTH_TEST);
  glClearColor(0.529f, 0.808f, 0.98f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glUseProgram(gl->shader_program);
  glBindVertexArray(gl->vao);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gl->tex_id);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->pitch / 4 );
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width - (clip * 2), frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame->pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  check_opengl_error("glTexImage2D");

  glDrawArrays(GL_TRIANGLES, 0, gl->vao_num_vertices);
}       

void render_game(struct Game *game, mat4 projection) {
    glViewport(0, 0, game->camera.viewport_width, game->camera.viewport_height);

    game->camera.position_height = get_image_grey(&game->height_map, game->camera.position_x, game->camera.position_y) + 50;

    if (game->options.do_raycasting) {
      memset(game->frame.pixels, 0, game->frame.height * game->frame.pitch);
      if (game->options.render_stereo) {
        for (int eye = 0; eye < 2; ++eye) {
          struct FrameBuffer eye_buffer;
          eye_buffer.width = game->frame.width / 2;
          eye_buffer.height = game->frame.height;
          eye_buffer.pitch = game->frame.pitch;
          eye_buffer.y_buffer = &game->frame.y_buffer[eye * (eye_buffer.width / 2)];
          eye_buffer.clip_left_x = eye == 0 ? 0 : game->camera.clip;
          eye_buffer.clip_right_x = eye_buffer.width - (eye == 0 ? game->camera.clip : 0);
          eye_buffer.pixels = eye == 0 ? game->frame.pixels : &game->frame.pixels[(eye_buffer.width - game->camera.clip * 2) * 4];

          int eye_mod = eye == 1 ? 1 : -1;
          int eye_dist = 3;
          struct Camera eye_cam = game->camera;
          eye_cam.position_x += (int)(eye_mod * eye_dist * sin(eye_cam.pitch + (M_PI / 2)));
          eye_cam.position_y += (int)(eye_mod * eye_dist * cos(eye_cam.pitch + (M_PI / 2)));

          render(&eye_buffer, &game->color_map, &game->height_map, &eye_cam);
        }
      } else {
        render(&game->frame, &game->color_map, &game->height_map, &game->camera);
      }

      render_buffer_to_gl(&game->frame, &game->gl, game->camera.clip);
    } else {
      render_real_3d(&game->gl, &game->camera, projection);
    }
}

int32_t add_event(struct Game *game, struct GameInputEvent event) {
  struct EventQueue *queue = &game->queue;
  if (queue->length >= queue->capacity - 1) {
    error("WARNING: Game event buffer full\n");
    return GAME_ERROR;
  }

  int32_t new_index = (queue->index_next + queue->length) % queue->capacity;
  queue->events[new_index] = event;
  queue->length++;

  return GAME_SUCCESS;
}

static bool pop_event(struct Game *game, struct GameInputEvent *output_event) {
  struct EventQueue *queue = &game->queue;
  if (queue->length == 0) {
    return false;
  }

  struct GameInputEvent event = queue->events[queue->index_next];
  output_event->key = event.key;  
  output_event->type = event.type;  
  queue->index_next = (queue->index_next + 1) % queue->capacity;
  queue->length--;

  return true;
}

void update_game(struct Game *game, float elapsed) {
  struct GameController *controller = &game->controller;

  struct GameInputEvent event;
  while (pop_event(game, &event)) {
    if (event.key == 'w')
    {
      controller->move_forward = event.type == KeyDown;
    }

    if (event.key == 's')
    {
      controller->move_backward = event.type == KeyDown;
    }

    if (event.key == 'a')
    {
      controller->turn_left = event.type == KeyDown;
    }

    if (event.key == 'd')
    {
      controller->turn_right = event.type == KeyDown;
    }

    if (event.key == 'e')
    {
      game->camera.clip++;
      info("clip %i\n", game->camera.clip);
    }

    if (event.key == 'r')
    {
      game->camera.clip--;
      info("clip %i\n", game->camera.clip);
    }

    if (event.key == 'v' && event.type == KeyDown)
    {
      game->options.render_stereo = !game->options.render_stereo;
    }

    if (event.key == 't' && event.type == KeyDown)
    {
      game->options.do_raycasting = !game->options.do_raycasting;
    }
  }

  if (controller->move_forward || controller->move_backward) {
    int modifier = controller->move_forward ? -1 : 1;
    game->camera.position_y += modifier * cos(game->camera.pitch) * 25 * elapsed;
    game->camera.position_x += modifier * sin(game->camera.pitch) * 25 * elapsed;
  }

  if (controller->turn_left || controller->turn_right) {
    int modifier = controller->turn_left ? 1 : -1;
    game->camera.pitch += modifier * M_PI * elapsed;
  }

  while (game->camera.pitch >= 2 * M_PI) {
    game->camera.pitch -= 2 * M_PI;
  }

  while (game->camera.pitch < 0) {
    game->camera.pitch += 2 * M_PI;
  }

  while (game->camera.position_x >= game->height_map.width) {
    game->camera.position_x -= game->height_map.width;
  }

  while (game->camera.position_y >= game->height_map.height) {
    game->camera.position_y -= game->height_map.height;
  }

  while (game->camera.position_x < 0) {
    game->camera.position_x += game->height_map.width;
  }

  while (game->camera.position_y < 0) {
    game->camera.position_y += game->height_map.height;
  }
}                 

static void create_gl_objects(struct Game *game) {
  float vertices[] = {
    -1.0, -1.0, 0.0,
    -1.0,  1.0, 0.0,
     1.0,  1.0, 0.0,
    -1.0, -1.0, 0.0,
     1.0, -1.0, 0.0,
     1.0,  1.0, 0.0,
  };


  struct OpenGLData *gl = &game->gl;

  glGenFramebuffers(1, &gl->frame_buffer);

	glGenBuffers(1, &gl->vbo);

	glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
  gl->vao_num_vertices = sizeof(vertices) / 3;
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glGenVertexArrays(1, &gl->vao);
	glBindVertexArray(gl->vao);
	glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

  char *vertex_shader_source = read_file("src/shaders/to_screen_space.vert");
  assert(vertex_shader_source != NULL);

  char *fragment_shader_source = read_file("src/shaders/blit.frag");
  assert(fragment_shader_source != NULL);

  gl->shader_program = create_shader(vertex_shader_source, fragment_shader_source);
  free(vertex_shader_source);
  free(fragment_shader_source);
  assert(gl->shader_program);

  glGenTextures(1, &gl->tex_id);
  glBindTexture(GL_TEXTURE_2D, gl->tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenTextures(1, &gl->color_map_tex_id);
  glBindTexture(GL_TEXTURE_2D, gl->color_map_tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, game->color_map.width, game->color_map.height, 0, GL_RGB, GL_UNSIGNED_BYTE, game->color_map.pixels);
  
  glGenTextures(1, &gl->height_map_tex_id);
  glBindTexture(GL_TEXTURE_2D, gl->height_map_tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, game->height_map.width, game->height_map.height, 0, GL_RED, GL_UNSIGNED_BYTE, game->height_map.pixels); 

  char *model_vertex_shader_source = read_file("src/shaders/model_view.vert");
  assert(model_vertex_shader_source != NULL);

  char *get_color_fragment_shader_source = read_file("src/shaders/get_color.frag");
  assert(get_color_fragment_shader_source != NULL);

  gl->poly_shader_program = create_shader(model_vertex_shader_source, get_color_fragment_shader_source);
  free(model_vertex_shader_source);
  free(get_color_fragment_shader_source);
  assert(gl->poly_shader_program);    

  int32_t num_map_vertices = (game->height_map.width * game->height_map.height);
  V3 *map_vertices = malloc(sizeof(V3) * num_map_vertices);

  int32_t indices_per_vert = 6;
  int32_t num_indices = 0;
  
  // NOTE: allocating slightly more space than we need since we don't generate 
  // indices for the final row and column
  int32_t *index_buffer = malloc(sizeof(int32_t) * num_map_vertices * indices_per_vert);

  for (int32_t y = 0; y < game->height_map.height; y++) {
    for (int32_t x = 0; x < game->height_map.width; x++) {
      int32_t v_index = ((y * game->height_map.width) + x);
      map_vertices[v_index][0] = x;
      map_vertices[v_index][1] = y;
      map_vertices[v_index][2] = ((int32_t) get_image_grey(&game->height_map, x, y)) / 255.0f;

      if (x >= game->height_map.width - 1 || y >= game->height_map.height - 1) {
        continue;
      }

      int32_t i_index = v_index * indices_per_vert;
      
      index_buffer[i_index] = v_index;
      index_buffer[i_index + 1] = v_index + game->height_map.width;
      index_buffer[i_index + 2] = v_index + 1;

      index_buffer[i_index + 3] = v_index + game->height_map.width;
      index_buffer[i_index + 4] = v_index + game->height_map.width + 1;
      index_buffer[i_index + 5] = v_index + 1;
      num_indices += indices_per_vert;
    }
  }
  info("# of vertices in terrain mesh: %i\n", num_indices);

  glGenBuffers(1, &gl->map_vbo);

	glBindBuffer(GL_ARRAY_BUFFER, gl->map_vbo);
  gl->map_vao_num_vertices = num_map_vertices;
	glBufferData(GL_ARRAY_BUFFER, num_map_vertices * sizeof(V3), map_vertices, GL_STATIC_DRAW);
	free(map_vertices);

	glGenBuffers(1, &gl->map_vbo_indices);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl->map_vbo_indices);
	gl->num_map_vbo_indices = num_indices;
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_indices * sizeof(int32_t), index_buffer, GL_STATIC_DRAW);
	free(index_buffer);

	glGenVertexArrays(1, &gl->map_vao);
	glBindVertexArray(gl->map_vao);
	glBindBuffer(GL_ARRAY_BUFFER, gl->map_vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl->map_vbo_indices);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0); 
}



static int32_t load_assets(struct Game *game) {
  game->color_map.pixels = stbi_load("C1W.png", &game->color_map.width, &game->color_map.height, &game->color_map.num_channels, 0);
  if (game->color_map.pixels == NULL) {
    error(stbi_failure_reason());
    error("Could not load color map");
    return GAME_ERROR;
  }

  game->height_map.pixels = stbi_load("D1.png", &game->height_map.width, &game->height_map.height, &game->height_map.num_channels, 0);
  if (game->height_map.pixels == NULL) {
    error(stbi_failure_reason());
    error("Could not load height map");
    return GAME_ERROR;
  }            

  return GAME_SUCCESS;
}

static void create_frame_buffer(struct Game *game, int32_t width, int32_t height) {
  game->frame.width = width;
  game->frame.height = height;
  // frame.width = 2528;
  // frame.height = 1408;
  game->frame.clip_left_x = 0,
  game->frame.clip_right_x = game->frame.width,
  game->frame.y_buffer = malloc(game->frame.width * sizeof(int32_t));
  game->frame.pixels = malloc(game->frame.width * game->frame.height * sizeof(uint8_t) * 4);
  game->frame.pitch = game->frame.width * sizeof(uint32_t); 
}

int32_t game_init(struct Game *game, int32_t width, int32_t height) {
  if (load_assets(game) == GAME_ERROR) {
    return GAME_ERROR;
  }

  game->queue.capacity = EVENT_QUEUE_CAPACITY;
  game->queue.index_next = 0;
  game->queue.length = 0;

  create_frame_buffer(game, width, height);

  game->camera = (struct Camera) { 
  .viewport_width = width,
  .viewport_height = height,
  .distance = 800,
  .quat = GLM_QUAT_IDENTITY_INIT,
  .pitch = M_PI,
  .horizon = game->frame.height / 2,
  .scale_height = game->frame.height * 0.35,
  .position_x = 436,
  .position_y = 54,
  .position_height = 50,
  .clip = .06f * game->frame.width
  };

  create_gl_objects(game);

  return GAME_SUCCESS;
}

void game_free(struct Game *game) {
  if (game->color_map.pixels != NULL) {
    stbi_image_free(game->color_map.pixels);
    game->color_map.pixels = NULL;
  }

  if (game->height_map.pixels != NULL) {
    stbi_image_free(game->height_map.pixels);
    game->height_map.pixels = NULL;
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
