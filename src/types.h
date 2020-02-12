#pragma once
#include "stdint.h"

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

struct Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

struct Camera {
  int distance;
  float rotation;
  int horizon;
  int scale_height;
  int position_x;
  int position_y;
  int position_height;
  int clip;
};

struct OpenGLData {
  GLuint frame_buffer;
  GLuint shader_program;
  GLuint vbo;
  GLuint vao;
  GLuint tex_id;
  uint32_t vao_num_vertices;
};

