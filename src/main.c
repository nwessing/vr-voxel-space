#include "SDL2/SDL.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdint.h"
#include "math.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 640

typedef uint8_t u8;
typedef uint32_t u32;
typedef int8_t i8;
typedef int32_t i32;

struct FrameBuffer {
  int width;
  int height;
  unsigned int *pixels;
  int pitch;
  int *y_buffer;
};

struct ImageBuffer {
  int width;
  int height;
  unsigned char *pixels;
  int num_channels;
};

struct Color {
  unsigned char r;
  unsigned char g;
  unsigned char b;
  unsigned char a;
};

struct Camera {
  int distance;
  float rotation;
  int horizon;
  int scale_height;
  int position_x;
  int position_y;
  int position_height;
};

void wrap_coordinates(struct ImageBuffer *image, int *x, int *y) {
  if (*x < 0) {
    *x += image->width;
  }

  if (*y < 0) {
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
    result.a = 0;
  }

  return result;
}

void put_pixel(struct FrameBuffer *frame, struct Color color, int x, int y) {
  if (x < 0 || x >= frame->width || y < 0 || y >= frame->height) {
    return;
  }

  assert(x >= 0 && x < frame->width);
  assert(y >= 0 && y < frame->height);

  frame->pixels[x + (y * frame->pitch / 4)] = (color.r << 24) | (color.g << 16) | (color.b << 8) | 0x000000FF;
}

void render_vertical_line(struct FrameBuffer *frame, int x, int y_start, int y_end, struct Color color) {
  if (x < 0 || x >= frame->width || y_start < 0 || y_end < 0 || y_start >= frame->height || y_end >= frame->height) {
    return;
  }

  assert(x >= 0 && x < frame->width);
  assert(y_start >= 0 && y_start < frame->height);
  assert(y_end >= 0 && y_end < frame->height);

  for (int y = y_start; y < y_end; ++y) {
    put_pixel(frame, color, x, y);
  }
}

void render(struct FrameBuffer *frame, struct ImageBuffer *color_map, struct ImageBuffer *height_map, struct Camera *camera) {
  float cosphi = cos(camera->rotation);
  float sinphi = sin(camera->rotation);

  /* int *y_buffer = malloc(frame->width * sizeof(int)); */
  for (int i = 0; i < frame->width; ++i) {
    frame->y_buffer[i] = frame->height;
  }

  for (int z = 1; z < camera->distance; ++z) {
    float point_left_x = (-cosphi * z - sinphi * z) + camera->position_x;
    float point_left_y = (-cosphi * z + sinphi * z) + camera->position_y;
    float point_right_x = (cosphi * z - sinphi * z) + camera->position_x;
    float point_right_y = (-cosphi * z - sinphi * z) + camera->position_y;

    float dx = (point_right_x - point_left_x) / (float) frame->width;
    float dy = (point_right_y - point_left_y) / (float) frame->width;

    for (int x = 0; x < frame->width; x++) {
      int terrain_height = get_image_grey(height_map, point_left_x, point_left_y);
      int height_on_screen = ((float) (camera->position_height - terrain_height) / z) * camera->scale_height + camera->horizon;

      int y_start = frame->y_buffer[x];
      if (height_on_screen < y_start) {
        struct Color color = get_image_color(color_map, point_left_x, point_left_y);
        render_vertical_line(frame, x, height_on_screen, y_start, color);
        frame->y_buffer[x] = height_on_screen;
      }
      point_left_x += dx;
      point_left_y += dy;
    }
  }
}

int main() {
  //Initialize SDL
  if(SDL_Init(SDL_INIT_VIDEO) < 0 )
  {
      printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
      return 1;
  }

  //Create window
  SDL_Window *window = SDL_CreateWindow("VR Voxel Space", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
  if(window == NULL)
  {
      printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
      return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  SDL_Texture* buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

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
  f_buffer.width = SCREEN_WIDTH;
  f_buffer.height = SCREEN_HEIGHT;
  f_buffer.y_buffer = malloc(f_buffer.width * sizeof(int));

  struct Camera camera = {
    .distance = 300,
    .rotation = M_PI,
    .horizon = 120,
    .scale_height = 120,
    .position_x = 512,
    .position_y = 512,
    .position_height = 50
  };

  bool quit = false;
  unsigned int time_last = SDL_GetTicks();
  unsigned int num_frames = 0;
  unsigned int time_begin = SDL_GetTicks();
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

      if (e.type == SDL_KEYDOWN &&
          e.key.keysym.sym == SDLK_w)
      {
        camera.position_y += 600 * elapsed;
      }

      if (e.type == SDL_KEYDOWN &&
          e.key.keysym.sym == SDLK_a)
      {
        camera.rotation += M_PI * elapsed;
      }

      if (e.type == SDL_KEYDOWN &&
          e.key.keysym.sym == SDLK_d)
      {
        camera.rotation -= M_PI * elapsed;
      }

      if (camera.rotation >= 2 * M_PI) {
        camera.rotation -= 2 * M_PI;
      }

      if (camera.rotation < 0) {
        camera.rotation += 2 * M_PI;
      }
    }

    SDL_LockTexture(buffer, NULL, (void**) &f_buffer.pixels, &f_buffer.pitch);

    memset(f_buffer.pixels, 0, f_buffer.height * f_buffer.pitch);

    render(&f_buffer, &color_map, &height_map, &camera);
    SDL_UnlockTexture(buffer);

    SDL_RenderCopy(renderer, buffer, NULL, NULL);
    SDL_RenderPresent(renderer);
    num_frames++;
    /* SDL_Delay(16); */
  }

  free(f_buffer.y_buffer);

  //Destroy window
  SDL_DestroyWindow(window);
  stbi_image_free(color_map.pixels);

  //Quit SDL subsystems
  SDL_Quit();

  return 0;
}

