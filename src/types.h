#pragma once
#include "stdint.h"
#include "game_gl.h"
#include <cglm/cglm.h>

#define GAME_SUCCESS 0
#define GAME_ERROR 1

struct FrameBuffer {
  int32_t width;
  int32_t height;
  int32_t clip_left_x;
  int32_t clip_right_x;
  uint8_t *pixels;
  int32_t pitch;
  int32_t *y_buffer;
};

struct ImageBuffer {
  int width;
  int height;
  uint8_t *pixels;
  int num_channels;
};

typedef float V3[3];

struct Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

struct Camera {
  int32_t viewport_width;
  int32_t viewport_height;
  int32_t distance;
  float yaw;
  float pitch;
  float roll;
  versor quat;
  int32_t horizon;
  int32_t scale_height;
  float position_x;
  float position_y;
  int32_t position_height;
  int32_t clip;
};

struct OpenGLData {
  GLuint frame_buffer;
  GLuint poly_shader_program;
  GLuint shader_program;
  GLuint vbo;
  GLuint vao;
  GLuint map_vbo;
  GLuint map_vbo_indices;
  GLuint map_vao;
  GLuint tex_id;
  GLuint color_map_tex_id;
  GLuint height_map_tex_id;
  uint32_t vao_num_vertices;
  uint32_t map_vao_num_vertices;
  uint32_t num_map_vbo_indices;
};

struct GameOptions {
  bool do_raycasting;
  bool render_stereo;
};

enum GameEventType {
  KeyDown = 1,
  KeyUp = 2
};

struct GameInputEvent {
  int32_t key;  
  int32_t type;
};

#define EVENT_QUEUE_CAPACITY 32
struct EventQueue {
  struct GameInputEvent events[EVENT_QUEUE_CAPACITY];
  int32_t index_next;
  int32_t length;
  int32_t capacity;
};

struct GameController {
  bool move_forward;
  bool move_backward;
  bool turn_left;
  bool turn_right;
};

struct Game {
  struct GameOptions options;
  struct GameController controller;
  struct Camera camera;
  struct ImageBuffer color_map;
  struct ImageBuffer height_map;
  struct FrameBuffer frame;
  struct OpenGLData gl;
  struct EventQueue queue;
};
