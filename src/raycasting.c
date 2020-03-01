#include "types.h"
#include "stdio.h"
#include "assert.h"
#include "math.h"
#include "image.h"

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
