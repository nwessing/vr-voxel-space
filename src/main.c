#include "sdl2/SDL.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stdbool.h"
#include "stdio.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 640

struct FrameBuffer {
  int width;
  int height;
  unsigned int* pixels;
  int pitch;
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

unsigned char get_image_grey(struct ImageBuffer *image, int x, int y) {
  assert(x >=0 && x < image->width);
  assert(y >=0 && y < image->height);
  assert(image->num_channels == 1);

  int offset = (x + (y * image->width)) * image->num_channels;
  return image->pixels[offset];
}

struct Color get_image_color(struct ImageBuffer *image, int x, int y) {
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

void render_vertical_line(struct FrameBuffer *frame, int x, int height, struct Color color) {
  // printf("x = %i", x);
  if (x < 0 || x >= frame->width || height < 0 || height >= frame->height) {
    return;
  }

  assert(x >= 0 && x < frame->width);
  assert(height >= 0 && height < frame->height);

  for (int y = frame->height - height; y < frame->height; ++y) {
    frame->pixels[x + (y * frame->pitch / 4)] = (color.r << 24) | (color.g << 16) | (color.b << 8) | 0x000000FF;
  }
}

void render(struct FrameBuffer *frame, struct ImageBuffer *color_map, struct ImageBuffer *height_map) {
  int distance = 300;
  int position_height = 50;
  int horizon = 120;
  int scale_height = 120;
  int position_x = 512;
  int position_y = 512;
  for (int z = distance; z > 1; --z) {
    int point_left_x = position_x - z;
    int point_left_y = position_y - z;
    int point_right_x = position_x + z;
    int point_right_y = position_y - z;

    int dx = (point_right_x - point_left_x) / SCREEN_WIDTH;
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      int terrain_height = get_image_grey(height_map, point_left_x, point_left_y);
      int height_on_screen = (position_height - terrain_height) / z * scale_height + horizon;
      render_vertical_line(frame, x, height_on_screen, get_image_color(color_map, point_left_x, point_left_y));
      point_left_x += dx;
    }
  }
// def Render(p, height, horizon, scale_height, distance, screen_width, screen_height):
//     # Draw from back to the front (high z coordinate to low z coordinate)
//     for z in range(distance, 1, -1):
//         # Find line on map. This calculation corresponds to a field of view of 90°
//         pleft  = Point(-z + p.x, -z + p.y)
//         pright = Point( z + p.x, -z + p.y)
//         # segment the line
//         dx = (pright.x - pleft.x) / screen_width
//         # Raster line and draw a vertical line for each segment
//         for i in range(0, screen_width):
//             height_on_screen = (height - heightmap[pleft.x, pleft.y]) / z * scale_height. + horizon
//             DrawVerticalLine(i, height_on_screen, screen_height, colormap[pleft.x, pleft.y])
//             pleft.x += dx

// # Call the render function with the camera parameters:
// # position, height, horizon line position,
// # scaling factor for the height, the largest distance,
// # screen width and the screen height parameter
// Render( Point(0, 0), 50, 120, 120, 300, 800, 600 )
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
  printf("height map has %i channels", height_map.num_channels);

  struct FrameBuffer f_buffer;
  f_buffer.width = SCREEN_WIDTH;
  f_buffer.height = SCREEN_HEIGHT;
  SDL_LockTexture(buffer, NULL, (void**) &f_buffer.pixels, &f_buffer.pitch);

  for (int row = 0; row < SCREEN_HEIGHT; ++row)
  {
    for (int column = 0; column < SCREEN_WIDTH; ++column)
    {
      int offset = (column + (row * color_map.width)) * color_map.num_channels;
      unsigned char red = color_map.pixels[offset];
      unsigned char green = color_map.pixels[offset + 1];
      unsigned char blue = color_map.pixels[offset + 2];
      f_buffer.pixels[column + (row * f_buffer.pitch / 4)] =  (red << 24) | (green << 16) | (blue << 8) | 0x000000FF;
    }
  }

  render(&f_buffer, &color_map, &height_map);

  SDL_UnlockTexture(buffer);

  SDL_RenderCopy(renderer, buffer, NULL, NULL);
  SDL_RenderPresent(renderer);

  bool quit = false;
  while (!quit)
  {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0)
    {
      if (e.type == SDL_QUIT)
      {
        quit = true;
      }
    }
    SDL_Delay(10);
  }

  //Destroy window
  SDL_DestroyWindow(window);
  stbi_image_free(color_map.pixels);

  //Quit SDL subsystems
  SDL_Quit();

  return 0;
}

