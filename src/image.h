#pragma once

#include "stb_image.h"
#include "types.h"

void wrap_coordinates(struct ImageBuffer *, int *x, int *y);
unsigned char get_image_grey(struct ImageBuffer *, int x, int y);
struct Color get_image_color(struct ImageBuffer *, int x, int y); 
int clamp(int position, int min, int max);       
