#include "glad/glad.h"
#include <SDL2/SDL.h>
#include <cglm/cglm.h>

#ifdef INCLUDE_LIBOVR
#include "vr.h"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "file.h"
#include "shader.h"
#include "assert.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdint.h"
#include "math.h"
#include "shader.h"
#include "types.h"
#include "game.h"
#include "image.h"
#include "raycasting.h"

#define INITIAL_SCREEN_WIDTH 800
#define INITIAL_SCREEN_HEIGHT 640

int main(void) {
  if(SDL_Init(SDL_INIT_VIDEO) < 0 ) {
      printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
      return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

  SDL_Window *window = SDL_CreateWindow("VR Voxel Space", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, INITIAL_SCREEN_WIDTH, INITIAL_SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
  if(window == NULL) {
      printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
      return 1;
  }

  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  const char *sdl_error = SDL_GetError();
  if (*sdl_error != '\0') {
    printf("ERROR: %s\n", sdl_error);
    return 1;
  }

  SDL_GL_MakeCurrent(window, gl_context);

	if (!gladLoadGLLoader((GLADloadproc) SDL_GL_GetProcAddress))
	{
		printf("Failed to initialize GLAD\n");
		return 1;
	}

  printf("%s\n", glGetString(GL_VERSION));

	sdl_error = SDL_GetError();
  if (*sdl_error != '\0') {
    printf("ERROR: %s\n", sdl_error);
    //return 1;
  }

	assert(glGetError() == GL_NO_ERROR);
	glEnable(GL_DEPTH_TEST);

  struct Game game = {0};
  if (game_init(&game, 1264, 704) == GAME_ERROR) {
    return 1;
  }

  printf("PITCH: %i\n", game.frame.pitch);

  bool quit = false;
  uint32_t time_last = SDL_GetTicks();
  uint32_t num_frames = 0;
  uint32_t time_begin = SDL_GetTicks();

  bool turn_right = false;
  bool turn_left = false;
  bool move_forward = false;
  bool move_backward = false;
  bool render_stereo = false;
  bool do_raycasting = false;
  while (!quit)
  {
    uint32_t time = SDL_GetTicks();
    if (time - time_begin >= 1000) {
      printf("%ix%i, FPS: %f\n", game.camera.viewport_width, game.camera.viewport_height, num_frames / ((time - time_begin) / (float) 1000));
      num_frames = 0;
      time_begin = time;
    }

    float elapsed = (time - time_last) / 1000.0;
    time_last = time;

    SDL_Event e;

    while (SDL_PollEvent(&e) != 0)
    {
      if (e.type == SDL_QUIT ||
          e.key.keysym.sym == SDLK_q)
      {
        quit = true;
      }

      if (e.key.keysym.sym == SDLK_f && e.key.repeat == 0 && e.type == SDL_KEYDOWN) {
        SDL_DisplayMode mode;
        if (SDL_GetDisplayMode(0, 0, &mode) == 0) {
          printf("Display Mode: %ix%i\n", mode.w, mode.h);
          SDL_SetWindowDisplayMode(window, &mode);
        }

        unsigned int flags = SDL_GetWindowFlags(window);
        unsigned int new_mode = (flags & SDL_WINDOW_FULLSCREEN) == 0 ? SDL_WINDOW_FULLSCREEN : 0;
        SDL_SetWindowFullscreen(window, new_mode);
      }

      if (e.key.keysym.sym == SDLK_w)
      {
        move_forward = e.type == SDL_KEYDOWN;
      }

      if (e.key.keysym.sym == SDLK_s)
      {
        move_backward = e.type == SDL_KEYDOWN;
      }

      if (e.key.keysym.sym == SDLK_a)
      {
        turn_left = e.type == SDL_KEYDOWN;
      }

      if (e.key.keysym.sym == SDLK_d)
      {
        turn_right = e.type == SDL_KEYDOWN;
      }

      if (e.key.keysym.sym == SDLK_e)
      {
        game.camera.clip++;
        printf("clip %i\n", game.camera.clip);
      }

      if (e.key.keysym.sym == SDLK_r)
      {
        game.camera.clip--;
        printf("clip %i\n", game.camera.clip);
      }

      if (e.key.keysym.sym == SDLK_v && e.key.repeat == 0 && e.type == SDL_KEYDOWN)
      {
        render_stereo = !render_stereo;
      }

      if (e.key.keysym.sym == SDLK_t && e.key.repeat == 0 && e.type == SDL_KEYDOWN)
      {
        do_raycasting = !do_raycasting;
      }
    }

    if (move_forward || move_backward) {
      int modifier = move_forward ? -1 : 1;
      game.camera.position_y += modifier * cos(game.camera.rotation) * 200 * elapsed;
      game.camera.position_x += modifier * sin(game.camera.rotation) * 200 * elapsed;
    }

    if (turn_left || turn_right) {
      int modifier = turn_left ? 1 : -1;
      game.camera.rotation += modifier * M_PI * elapsed;
    }

    while (game.camera.rotation >= 2 * M_PI) {
      game.camera.rotation -= 2 * M_PI;
    }

    while (game.camera.rotation < 0) {
      game.camera.rotation += 2 * M_PI;
    }

    game.camera.position_x = game.camera.position_x % game.height_map.width;
    game.camera.position_y = game.camera.position_y % game.height_map.height;

    if (game.camera.position_x < 0) {
      game.camera.position_x += game.height_map.width;
    }

    if (game.camera.position_y < 0) {
      game.camera.position_y += game.height_map.height;
    }

    /* game.camera.position_height = get_image_grey(&game.height_map, game.camera.position_x, game.camera.position_y) + 50; */
    memset(game.frame.pixels, 0, game.frame.height * game.frame.pitch);

#ifdef INCLUDE_LIBOVR
    render_buffer_to_hmd(&vr, &game.frame, &gl, &color_map, &height_map, &camera, num_frames);
#else
    
    /* glBindFramebuffer(GL_FRAMEBUFFER, 0); */
    SDL_GetWindowSize(window, &game.camera.viewport_width, &game.camera.viewport_height);
    game.options.do_raycasting = do_raycasting;
    game.options.render_stereo = render_stereo;
    render_game(&game);

    SDL_GL_SwapWindow(window);

#endif

    num_frames++;
  }

  //Destroy window
  SDL_DestroyWindow(window);
  game_free(&game);

  //Quit SDL subsystems
  SDL_Quit();

  printf("EXIT\n");
  return 0;
}
