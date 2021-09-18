#pragma once
#include "game_gl.h"
#include "stdint.h"
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
  float position_z;
  int32_t position_y;
  int32_t clip;
  bool is_z_relative_to_ground;
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

// NOTE: Represent states of all keys for ASCII codes 32-127
#define KEYBOAD_STATE_MIN_CHAR 32
#define KEYBOARD_STATE_MAX_CHAR 127

#define KEYBOARD_STATE_NUM_KEYS                                                \
  KEYBOARD_STATE_MAX_CHAR - KEYBOAD_STATE_MIN_CHAR + 1
struct KeyboardState {
  bool down[KEYBOARD_STATE_NUM_KEYS];
};

// -1.0 to 1.0
// for both x and y
struct Vec2 {
  float x;
  float y;
};

struct ControllerState {
  struct Vec2 joy_stick;

  // 0.0 to 1.0
  float trigger;

  // 0.0 to 1.0
  float grip;
  bool primary_button;
  bool secondary_button;
  bool scale_rotation_by_time;
};

#define LEFT_CONTROLLER_INDEX 0
#define RIGHT_CONTROLLER_INDEX 1
struct Game {
  struct GameOptions options;
  struct Camera camera;
  struct ImageBuffer color_map;
  struct ImageBuffer height_map;
  struct FrameBuffer frame;
  struct OpenGLData gl;
  struct KeyboardState prev_keyboard;
  struct KeyboardState keyboard;
  struct ControllerState prev_controller[2];
  struct ControllerState controller[2];
};
