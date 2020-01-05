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
  int* pixels;
  int pitch;
};

struct ImageBuffer {
  int width;
  int height;
  char * pixels;
  int num_channels;
};

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

  int x, y, n;
  unsigned char *color_map = stbi_load("C1W.png", &x, &y, &n, 0);
  printf("NUM COMPONENTS = %i", n);
  if (color_map == NULL) {
    printf("Could not load color map");
    return 1;
  }

  struct FrameBuffer f_buffer;
  f_buffer.width = SCREEN_WIDTH;
  f_buffer.height = SCREEN_HEIGHT;
//   f_buffer.z_buffer = calloc(f_buffer.width * f_buffer.height, sizeof(int));
  SDL_LockTexture(buffer, NULL, (void**) &f_buffer.pixels, &f_buffer.pitch);

  // color black = { .r = 0, .g = 0, .b = 0};
  for (int row = 0; row < SCREEN_HEIGHT; ++row)
  {
    for (int column = 0; column < SCREEN_WIDTH; ++column)
    {
      int offset = (column + (row * x)) * n;
      char red = color_map[offset];
      char green = color_map[offset + 1];
      char blue = color_map[offset + 2];
      f_buffer.pixels[column + (row * f_buffer.pitch / 4)] =  (red << 24) | (green << 16) | (blue << 8) | 0x000000FF;


      // put_pixel(f_buffer, column, row, black);
    }
  }

//   Mesh *mesh = parse_model("assets/head.obj");
//   image image = load_tfa_file("assets/head_diffuse.tga");
//   color white = { .r = 254, .g = 255, .b = 255 };
//   for (int i_face = 0; i_face < mesh->facesSize; ++i_face)
//   {
//     Face *face = &mesh->faces[i_face];

//     int i_vertex1 = face->vertices[0] - 1;
//     int i_vertex2 = face->vertices[1] - 1;
//     int i_vertex3 = face->vertices[2] - 1;
//     assert(i_vertex1 >= 0 && i_vertex1 < mesh->verticesSize);
//     assert(i_vertex2 >= 0 && i_vertex2 < mesh->verticesSize);
//     assert(i_vertex3 >= 0 && i_vertex3 < mesh->verticesSize);
//     vec3 vertex1 = mesh->vertices[i_vertex1];
//     vec3 vertex2 = mesh->vertices[i_vertex2];
//     vec3 vertex3 = mesh->vertices[i_vertex3];
//     //color c = { .r = rand() % 255, .g = rand() % 255, .b = rand() % 255 };
//     draw_triangle(vertex1, vertex2, vertex3, white, f_buffer);
//   }

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
  stbi_image_free(color_map);

  //Quit SDL subsystems
  SDL_Quit();

  return 0;
}

