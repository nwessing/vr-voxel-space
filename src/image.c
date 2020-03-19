#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "types.h"
#include "assert.h"
#include "stdio.h"
#include "platform.h"

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
    error("x = %i, image_w = %i\n", x, image->width);
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

int clamp_i(int position, int min, int max) {
  if (position < min) {
    return min;
  }

  if (position > max) {
    return max;
  }

  return position;
}
