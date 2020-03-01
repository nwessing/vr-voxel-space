#pragma once
#include "types.h"
#include "assert.h"
#include "image.h"
#include "math.h"

void put_pixel(struct FrameBuffer *frame, struct Color color, int x, int y); 
void render_vertical_line(struct FrameBuffer *frame, int x, int y_start, int y_end, struct Color color); 
void render(struct FrameBuffer *frame, struct ImageBuffer *color_map, struct ImageBuffer *height_map, struct Camera *camera);
