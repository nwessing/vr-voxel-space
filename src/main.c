#include "glad/glad.h"
#include <SDL2/SDL.h>
#include <cglm/cglm.h>

#ifdef INCLUDE_LIBOVR
#include "vr.h"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "file.h"
#include "shader.h"
#include "assert.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdint.h"
#include "math.h"
#include "shader.h"
#include "types.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 640

void wrap_coordinates(struct ImageBuffer *image, int *x, int *y) {
  while (*x < 0) {
    *x += image->width;
  }

  while (*y < 0) {
    *y += image->height;
  }

  *x = *x % image->width;
  *y = *y % image->height;
}

unsigned char get_image_grey(struct ImageBuffer *image, int x, int y) {
  wrap_coordinates(image, &x, &y);

  if (x < 0 || x >= image->width) {
    printf("x = %i, image_w = %i\n", x, image->width);
  }
  assert(x >=0 && x < image->width);
  assert(y >=0 && y < image->height);
  assert(image->num_channels == 1);

  int offset = (x + (y * image->width)) * image->num_channels;
   return image->pixels[offset];
}

struct Color get_image_color(struct ImageBuffer *image, int x, int y) {
  wrap_coordinates(image, &x, &y);

  assert(x >=0 && x < image->width);
  assert(y >=0 && y < image->height);

  int offset = (x + (y * image->width)) * image->num_channels;
  int offset_r = offset;
  int offset_g = image->num_channels > 2 ? offset + 1 : offset;
  int offset_b = image->num_channels > 2 ? offset + 2 : offset;
  int offset_a = image->num_channels > 2 ? offset + 3 : offset + 1;

  struct Color result;
  result.r = image->pixels[offset_r];
  result.g = image->pixels[offset_g];
  result.b = image->pixels[offset_b];
  if (image->num_channels == 2 || image->num_channels == 4) {
    result.a = image->pixels[offset_a];
  } else {
    result.a = 255;
  }

  return result;
}

void put_pixel(struct FrameBuffer *frame, struct Color color, int x, int y) {
  if (x < 0 || x >= frame->width || y < 0 || y >= frame->height) {
    return;
  }

  assert(x >= 0 && x < frame->width);
  assert(y >= 0 && y < frame->height);

  int32_t offset = (x * 4) + (y * frame->pitch);
  frame->pixels[offset] = color.r;
  frame->pixels[offset + 1] = color.g;
  frame->pixels[offset + 2] = color.b;
  frame->pixels[offset + 3] = color.a;
}

inline int clamp(int position, int min, int max) {
  if (position < min) {
    return min;
  }

  if (position > max) {
    return max;
  }

  return position;
}

void render_vertical_line(struct FrameBuffer *frame, int x, int y_start, int y_end, struct Color color) {
  x = clamp(x, 0, frame->width - 1);
  y_start = clamp(y_start, 0, frame->height - 1);
  y_end = clamp(y_end, 0, frame->height - 1);

  assert(y_end >= 0 && y_end < frame->height);

  for (int y = y_start; y < y_end; ++y) {
    put_pixel(frame, color, x, y);
  }
}

void render(struct FrameBuffer *frame, struct ImageBuffer *color_map, struct ImageBuffer *height_map, struct Camera *camera) {
  float cosphi = cos(camera->rotation);
  float sinphi = sin(camera->rotation);

  for (int i = 0; i < frame->width; ++i) {
    frame->y_buffer[i] = frame->height;
  }

  float delta_z = 1.0f;
  for (float z = 1; z < camera->distance; z += delta_z) {
    float a_point_left_x = (-cosphi * z - sinphi * z) + camera->position_x;
    float a_point_left_y = (-cosphi * z + sinphi * z) + camera->position_y;
    float point_right_x = (cosphi * z - sinphi * z) + camera->position_x;
    float point_right_y = (-cosphi * z - sinphi * z) + camera->position_y;

    float dx = (point_right_x - a_point_left_x) / (float) frame->width;
    float dy = (point_right_y - a_point_left_y) / (float) frame->width;

    for (int x = frame->clip_left_x; x < frame->clip_right_x; x++) {
      float point_left_x = a_point_left_x + (x * dx);
      float point_left_y = a_point_left_y + (x * dy);

      int terrain_height = get_image_grey(height_map, point_left_x, point_left_y);
      int height_on_screen = ((float) (camera->position_height - terrain_height) / z) * camera->scale_height + camera->horizon;

      int y_start = frame->y_buffer[x];
      if (height_on_screen < y_start) {
        struct Color color = get_image_color(color_map, point_left_x, point_left_y);
        render_vertical_line(frame, x, height_on_screen, y_start, color);
        frame->y_buffer[x] = height_on_screen;
      }
      // point_left_x += dx;
      // point_left_y += dy;
    }

    delta_z += 0.005f;
  }
}

void render_real_3d(struct OpenGLData *gl, struct Camera *camera) {
  glClearColor(0.529f, 0.808f, 0.98f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // NOTE: Flip the z-axis so that x increases to the camera's right
  // matching the way the map appears in the image
  vec3 position = {camera->position_x, camera->position_y, -camera->position_height};
  vec3 center = {
    position[0] - sin(camera->rotation), 
    position[1] - cos(camera->rotation), 
    position[2]
  };

  vec3 up = {0.0, 0.0, -1.0};

  mat4 view_matrix = GLM_MAT4_IDENTITY;
  glm_lookat(position, center, up, view_matrix);

  mat4 projection_matrix = GLM_MAT4_IDENTITY;
  glm_perspective(glm_rad(90), camera->viewport_width/ (float) camera->viewport_height, 0.01f, 1000.0f, projection_matrix);

  glUseProgram(gl->poly_shader_program);
	glUniformMatrix4fv(glGetUniformLocation(gl->poly_shader_program, "view"), 1, GL_FALSE, (float *) view_matrix);
	glUniformMatrix4fv(glGetUniformLocation(gl->poly_shader_program, "projection"), 1, GL_FALSE, (float *) projection_matrix);
  glBindVertexArray(gl->map_vao);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gl->color_map_tex_id);

  glDrawElements(GL_TRIANGLES, gl->num_map_vbo_indices, GL_UNSIGNED_INT, (void*)0);
}

void check_opengl_error(char *name) {
  GLenum gl_error = glGetError();
  if (gl_error) {
    printf("(%s) error: %i\n", name, gl_error);
  }
}

void render_buffer_to_gl(struct FrameBuffer *frame, struct OpenGLData *gl, int clip) {
  glClearColor(0.529f, 0.808f, 0.98f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glUseProgram(gl->shader_program);
  glBindVertexArray(gl->vao);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gl->tex_id);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->pitch / 4 );
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width - (clip * 2), frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame->pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, clipped_buffer_width, frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, clipped_buffer);
  // free(clipped_buffer);
  check_opengl_error("glTexImage2D");

  glDrawArrays(GL_TRIANGLES, 0, gl->vao_num_vertices);
}

#ifdef INCLUDE_LIBOVR
void render_buffer_to_hmd(struct vr_data *vr, struct FrameBuffer *frame, struct OpenGLData *gl,
  struct ImageBuffer *color_map, struct ImageBuffer *height_map, struct Camera *camera, int frame_index) {
  assert(glGetError() == GL_NO_ERROR);

  int current_index = 0;
  ovr_GetTextureSwapChainCurrentIndex(vr->session, vr->swap_chain, &current_index);

  int tex_id;
  ovr_GetTextureSwapChainBufferGL(vr->session, vr->swap_chain, current_index, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  ovrTrackingState tracking_state = ovr_GetTrackingState(vr->session, 0, false);
  ovrPosef eyePoses[2];
  ovr_CalcEyePoses(tracking_state.HeadPose.ThePose, vr->hmd_to_eye_view_pose, eyePoses);
  vr->layer.RenderPose[0] = eyePoses[0];
  vr->layer.RenderPose[1] = eyePoses[1];

  ovrResult wait_begin_frame_result = ovr_WaitToBeginFrame(vr->session, frame_index);
  if(OVR_FAILURE(wait_begin_frame_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_WaitToBeginFrame failed: %s\n", error_info.ErrorString);
    assert(false);
  }

  ovrResult begin_frame_result = ovr_BeginFrame(vr->session, frame_index);
  if(OVR_FAILURE(begin_frame_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_BeginFrame failed: %s\n", error_info.ErrorString);
    assert(false);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, gl->frame_buffer);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0);

  for (int eye = 0; eye < 2; ++eye) {
    ovrRecti vp = vr->layer.Viewport[eye];

    struct FrameBuffer eye_buffer;
    eye_buffer.width = frame->width / 2;
    eye_buffer.height = frame->height;
    eye_buffer.pitch = frame->pitch;
    // eye_buffer.pixels = &frame->pixels[eye * (frame->width / 2) * 4];
    eye_buffer.y_buffer = &frame->y_buffer[eye * (eye_buffer.width / 2)];
    eye_buffer.clip_left_x = eye == 0 ? 0 : camera->clip;
    eye_buffer.clip_right_x = eye_buffer.width - (eye == 0 ? camera->clip : 0);
    eye_buffer.pixels = eye == 0 ? frame->pixels : &frame->pixels[(eye_buffer.width - camera->clip * 2) * 4];

    int eye_mod = eye == 1 ? 1 : -1;
    int eye_dist = 3;
    struct Camera eye_cam = *camera;
    eye_cam.position_x += (int)(eye_mod * eye_dist * sin(eye_cam.rotation + (M_PI / 2)));
    eye_cam.position_y += (int)(eye_mod * eye_dist * cos(eye_cam.rotation + (M_PI / 2)));

    render(&eye_buffer, color_map, height_map, &eye_cam);
  }
  // larger than than HMD res, check vp structs
  glViewport(0, 0, 3616, 2000);
  render_buffer_to_gl(frame, gl, camera->clip);

  ovrResult commit_result = ovr_CommitTextureSwapChain(vr->session, vr->swap_chain);
  if(OVR_FAILURE(commit_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_CommitTextureSwapChain failed: %s\n", error_info.ErrorString);
    assert(false);
  }

  ovrLayerHeader* layers = &vr->layer.Header;
  ovrResult end_frame_result = ovr_EndFrame(vr->session, frame_index, NULL, &layers, 1);
  if(OVR_FAILURE(end_frame_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_EndFrame failed: %s\n", error_info.ErrorString);
    assert(false);
  }

  // printf("current_index = %i, tex_id = %i\n", current_index, tex_id);
  // glBindTexture(GL_TEXTURE_2D, tex_id);
}
#endif

void create_gl_objects(struct OpenGLData *gl, struct ImageBuffer *color_map, struct ImageBuffer *height_map) {
  float vertices[] = {
    -1.0, -1.0, 0.0,
    -1.0,  1.0, 0.0,
     1.0,  1.0, 0.0,
    -1.0, -1.0, 0.0,
     1.0, -1.0, 0.0,
     1.0,  1.0, 0.0,
  };

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
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, color_map->width, color_map->height, 0, GL_RGB, GL_UNSIGNED_BYTE, color_map->pixels);
  
  glGenTextures(1, &gl->height_map_tex_id);
  glBindTexture(GL_TEXTURE_2D, gl->height_map_tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, height_map->width, height_map->height, 0, GL_RED, GL_UNSIGNED_BYTE, height_map->pixels); 

  char *model_vertex_shader_source = read_file("src/shaders/model_view.vert");
  assert(model_vertex_shader_source != NULL);

  char *get_color_fragment_shader_source = read_file("src/shaders/get_color.frag");
  assert(get_color_fragment_shader_source != NULL);

  gl->poly_shader_program = create_shader(model_vertex_shader_source, get_color_fragment_shader_source);
  free(model_vertex_shader_source);
  free(get_color_fragment_shader_source);
  assert(gl->poly_shader_program);    
  /* const int32_t num_vertices_per_peak = 4; */
  int32_t num_map_vertices = (height_map->width * height_map->height);
  V3 *map_vertices = malloc(sizeof(V3) * num_map_vertices);

  int32_t index_buffer_stride = height_map->width * 3;
  int32_t indices_per_vert = 6;
  int32_t num_indices = (num_map_vertices * indices_per_vert) - index_buffer_stride;
  int32_t *index_buffer = malloc(sizeof(int32_t) * num_indices);

  for (int32_t y = 0; y < height_map->height; y++) {
    for (int32_t x = 0; x < height_map->width; x++) {
      int32_t v_index = ((y * height_map->width) + x);
      map_vertices[v_index][0] = x;
      map_vertices[v_index][1] = y;
      map_vertices[v_index][2] = ((int32_t) get_image_grey(height_map, x, y)) / 255.0f;

      if (x >= height_map->width - 1 || y >= height_map->height - 1) {
        continue;
      }

      int32_t i_index = v_index * indices_per_vert;
      
      index_buffer[i_index] = v_index;
      index_buffer[i_index + 1] = v_index + height_map->width;
      index_buffer[i_index + 2] = v_index + 1;

      index_buffer[i_index + 3] = v_index + height_map->width;
      index_buffer[i_index + 4] = v_index + height_map->width + 1;
      index_buffer[i_index + 5] = v_index + 1;
    }
  }

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

int main(void) {
  if(SDL_Init(SDL_INIT_VIDEO) < 0 ) {
      printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
      return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

  SDL_Window *window = SDL_CreateWindow("VR Voxel Space", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
  if(window == NULL) {
      printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
      return 1;
  }

  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  const char *sdl_error = SDL_GetError();
  if (*sdl_error != '\0') {
    printf("ERROR: %s\n", sdl_error);
    return 1;
  }

  SDL_GL_MakeCurrent(window, gl_context);

	if (!gladLoadGLLoader((GLADloadproc) SDL_GL_GetProcAddress))
	{
		printf("Failed to initialize GLAD\n");
		return 1;
	}

  printf("%s\n", glGetString(GL_VERSION));

	sdl_error = SDL_GetError();
  if (*sdl_error != '\0') {
    printf("ERROR: %s\n", sdl_error);
    //return 1;
  }

	assert(glGetError() == GL_NO_ERROR);
	glEnable(GL_DEPTH_TEST);

  struct ImageBuffer color_map;
  color_map.pixels = stbi_load("C1W.png", &color_map.width, &color_map.height, &color_map.num_channels, 0);
  if (color_map.pixels == NULL) {
    printf("Could not load color map");
    return 1;
  }

  struct ImageBuffer height_map;
  height_map.pixels = stbi_load("D1.png", &height_map.width, &height_map.height, &height_map.num_channels, 0);
  if (height_map.pixels == NULL) {
    printf("Could not load height map");
    return 1;
  }

  struct FrameBuffer f_buffer;
  f_buffer.width = 1264;
  f_buffer.height = 704;
  // f_buffer.width = 2528;
  // f_buffer.height = 1408;
  f_buffer.clip_left_x = 0,
  f_buffer.clip_right_x = f_buffer.width,
  f_buffer.y_buffer = malloc(f_buffer.width * sizeof(int32_t));
  f_buffer.pixels = malloc(f_buffer.width * f_buffer.height * sizeof(uint8_t) * 4);
  f_buffer.pitch = f_buffer.width * sizeof(uint32_t);
  printf("PITCH: %i\n", f_buffer.pitch);

  struct OpenGLData gl = {0};
  create_gl_objects(&gl, &color_map, &height_map);

  struct Camera camera = {
    .viewport_width = SCREEN_WIDTH,
    .viewport_height = SCREEN_HEIGHT,
    .distance = 800,
    .rotation = M_PI,
    .horizon = f_buffer.height / 2,
    .scale_height = f_buffer.height * 0.35,
    .position_x = 436,
    .position_y = 54,
    .position_height = 50,
    .clip = .06f * f_buffer.width //163
  };

  bool quit = false;
  unsigned int time_last = SDL_GetTicks();
  unsigned int num_frames = 0;
  unsigned int time_begin = SDL_GetTicks();

  bool turn_right = false;
  bool turn_left = false;
  bool move_forward = false;
  bool move_backward = false;
  bool render_stereo = false;
  bool do_raycasting = false;
  while (!quit)
  {
    unsigned int time = SDL_GetTicks();
    float elapsed = (time - time_last) / 1000.0;
    if ((time % 1000) < (time_last % 1000)) {
      printf("FPS: %f\n", num_frames / ((SDL_GetTicks() - time_begin) / (float) 1000));
    }
    time_last = time;

    SDL_Event e;

    while (SDL_PollEvent(&e) != 0)
    {
      if (e.type == SDL_QUIT ||
          e.key.keysym.sym == SDLK_q)
      {
        quit = true;
      }

      if (e.key.keysym.sym == SDLK_f && e.key.repeat == 0 && e.type == SDL_KEYDOWN) {
        unsigned int flags = SDL_GetWindowFlags(window);
        unsigned int new_mode = (flags & SDL_WINDOW_FULLSCREEN) == 0 ? SDL_WINDOW_FULLSCREEN : 0;
        SDL_SetWindowFullscreen(window, new_mode);
      }

      if (e.key.keysym.sym == SDLK_w)
      {
        move_forward = e.type == SDL_KEYDOWN;
      }

      if (e.key.keysym.sym == SDLK_s)
      {
        move_backward = e.type == SDL_KEYDOWN;
      }

      if (e.key.keysym.sym == SDLK_a)
      {
        turn_left = e.type == SDL_KEYDOWN;
      }

      if (e.key.keysym.sym == SDLK_d)
      {
        turn_right = e.type == SDL_KEYDOWN;
      }

      if (e.key.keysym.sym == SDLK_e)
      {
        camera.clip++;
        printf("clip %i\n", camera.clip);
      }

      if (e.key.keysym.sym == SDLK_r)
      {
        camera.clip--;
        printf("clip %i\n", camera.clip);
      }

      if (e.key.keysym.sym == SDLK_v && e.key.repeat == 0 && e.type == SDL_KEYDOWN)
      {
        render_stereo = !render_stereo;
      }

      if (e.key.keysym.sym == SDLK_t && e.key.repeat == 0 && e.type == SDL_KEYDOWN)
      {
        do_raycasting = !do_raycasting;
      }
    }

    if (move_forward || move_backward) {
      int modifier = move_forward ? -1 : 1;
      camera.position_y += modifier * cos(camera.rotation) * 200 * elapsed;
      camera.position_x += modifier * sin(camera.rotation) * 200 * elapsed;
    }

    if (turn_left || turn_right) {
      int modifier = turn_left ? 1 : -1;
      camera.rotation += modifier * M_PI * elapsed;
    }

    while (camera.rotation >= 2 * M_PI) {
      camera.rotation -= 2 * M_PI;
    }

    while (camera.rotation < 0) {
      camera.rotation += 2 * M_PI;
    }

    camera.position_x = camera.position_x % height_map.width;
    camera.position_y = camera.position_y % height_map.height;

    if (camera.position_x < 0) {
      camera.position_x += height_map.width;
    }

    if (camera.position_y < 0) {
      camera.position_y += height_map.height;
    }

    camera.position_height = get_image_grey(&height_map, camera.position_x, camera.position_y) + 50;
    memset(f_buffer.pixels, 0, f_buffer.height * f_buffer.pitch);

#ifdef INCLUDE_LIBOVR
    render_buffer_to_hmd(&vr, &f_buffer, &gl, &color_map, &height_map, &camera, num_frames);
#else
    

    /* glBindFramebuffer(GL_FRAMEBUFFER, 0); */
    SDL_GetWindowSize(window, &camera.viewport_width, &camera.viewport_height);
    glViewport(0, 0, camera.viewport_width, camera.viewport_height);
    if (do_raycasting) {
      if (render_stereo) {
        for (int eye = 0; eye < 2; ++eye) {
          struct FrameBuffer eye_buffer;
          eye_buffer.width = f_buffer.width / 2;
          eye_buffer.height = f_buffer.height;
          eye_buffer.pitch = f_buffer.pitch;
          eye_buffer.y_buffer = &f_buffer.y_buffer[eye * (eye_buffer.width / 2)];
          eye_buffer.clip_left_x = eye == 0 ? 0 : camera.clip;
          eye_buffer.clip_right_x = eye_buffer.width - (eye == 0 ? camera.clip : 0);
          eye_buffer.pixels = eye == 0 ? f_buffer.pixels : &f_buffer.pixels[(eye_buffer.width - camera.clip * 2) * 4];

          int eye_mod = eye == 1 ? 1 : -1;
          int eye_dist = 3;
          struct Camera eye_cam = camera;
          eye_cam.position_x += (int)(eye_mod * eye_dist * sin(eye_cam.rotation + (M_PI / 2)));
          eye_cam.position_y += (int)(eye_mod * eye_dist * cos(eye_cam.rotation + (M_PI / 2)));

          render(&eye_buffer, &color_map, &height_map, &eye_cam);
        }
      } else {
        render(&f_buffer, &color_map, &height_map, &camera);
      }

      render_buffer_to_gl(&f_buffer, &gl, camera.clip);
    } else {
      render_real_3d(&gl, &camera);
    }

    SDL_GL_SwapWindow(window);

#endif

    num_frames++;
  }

  free(f_buffer.y_buffer);
  free(f_buffer.pixels);

  //Destroy window
  SDL_DestroyWindow(window);
  stbi_image_free(color_map.pixels);
  stbi_image_free(height_map.pixels);

  //Quit SDL subsystems
  SDL_Quit();

  printf("EXIT\n");
  return 0;
}
