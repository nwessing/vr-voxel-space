#include "glad/glad.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>

#ifdef INCLUDE_LIBOVR
#include "vr.h"
#endif

#include "assert.h"
#include "game.h"
#include "platform.h"
#include "stdarg.h"
#include "stdbool.h"
#include "stdio.h"
#include "types.h"
#include "util.h"

#define INITIAL_SCREEN_WIDTH 800
#define INITIAL_SCREEN_HEIGHT 640

#define MOUSE_SENSITIVITY 0.001f

int error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  int result = vfprintf(stderr, format, args);
  va_end(args);
  return result;
}

int info(const char *format, ...) {
  va_list args;
  va_start(args, format);
  int result = vprintf(format, args);
  va_end(args);
  return result;
}

int main(void) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    error("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
                      SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

  SDL_Window *window = SDL_CreateWindow(
      "VR Voxel Space", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      INITIAL_SCREEN_WIDTH, INITIAL_SCREEN_HEIGHT,
      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
  if (window == NULL) {
    error("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  const char *sdl_error = SDL_GetError();
  if (*sdl_error != '\0') {
    error("ERROR: %s\n", sdl_error);
    return 1;
  }

  SDL_GL_MakeCurrent(window, gl_context);

  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    error("Failed to initialize GLAD\n");
    return 1;
  }

  SDL_SetRelativeMouseMode(SDL_TRUE);

  error("%s\n", glGetString(GL_VERSION));

  sdl_error = SDL_GetError();
  if (*sdl_error != '\0') {
    error("ERROR: %s\n", sdl_error);
    // return 1;
  }

  assert(glGetError() == GL_NO_ERROR);

  int32_t gl_major, gl_minor;
  glGetIntegerv(GL_MAJOR_VERSION, &gl_major);
  glGetIntegerv(GL_MINOR_VERSION, &gl_minor);
  info("OpenGL %d.%d\n", gl_major, gl_minor);

  info("Game data struct is %lu bytes\n", sizeof(struct Game));
  struct Game *game = calloc(1, sizeof(struct Game));
  if (game_init(game, 1264, 704) == GAME_ERROR) {
    return 1;
  }

  SDL_GL_SetSwapInterval(1);
  info("PITCH: %i\n", game->frame.pitch);

  bool quit = false;
  uint32_t time_last = SDL_GetTicks();
  uint32_t num_frames = 0;
  uint32_t time_begin = SDL_GetTicks();

  struct KeyboardState key_state = {0};
  while (!quit) {
    uint32_t time = SDL_GetTicks();
    if (time - time_begin >= 1000) {
      info("%ix%i, FPS: %f\n", game->camera.viewport_width,
           game->camera.viewport_height,
           num_frames / ((time - time_begin) / (float)1000));
      num_frames = 0;
      time_begin = time;
    }

    float elapsed = (time - time_last) / 1000.0;
    time_last = time;

    struct ControllerState left_controller = {0};
    struct ControllerState right_controller = {0};
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT ||
          (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_q)) {
        quit = true;
      }

      if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_g &&
          e.key.repeat == 0) {
        SDL_DisplayMode mode;
        if (SDL_GetDisplayMode(0, 0, &mode) == 0) {
          info("Display Mode: %ix%i\n", mode.w, mode.h);
          SDL_SetWindowDisplayMode(window, &mode);
        }

        unsigned int flags = SDL_GetWindowFlags(window);
        unsigned int new_mode =
            (flags & SDL_WINDOW_FULLSCREEN) == 0 ? SDL_WINDOW_FULLSCREEN : 0;
        SDL_SetWindowFullscreen(window, new_mode);
      }

      if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && e.key.repeat == 0) {
        int32_t key_index = e.key.keysym.sym - KEYBOAD_STATE_MIN_CHAR;
        if (key_index >= 0 && key_index <= KEYBOARD_STATE_NUM_KEYS) {
          key_state.down[key_index] = e.type == SDL_KEYDOWN;
        }
      }

      if (e.type == SDL_MOUSEMOTION) {
        right_controller.joy_stick.x = e.motion.xrel * MOUSE_SENSITIVITY;
        right_controller.scale_rotation_by_time = false;
      }
    }

#if 0
    left_controller.is_connected = true;
    right_controller.is_connected = true;
    glm_vec3_copy((vec3){0.358801, -0.422574, -0.369324},
                  left_controller.pose.position);
    glm_vec3_copy((vec3){0.358801, -0.422574, -0.369324},
                  right_controller.pose.position);
#endif

    struct Pose head_pose = {.position = GLM_VEC3_ZERO_INIT,
                             .orientation = GLM_QUAT_IDENTITY_INIT};
    update_game(game, &key_state, &left_controller, &right_controller,
                head_pose, elapsed);
#ifdef INCLUDE_LIBOVR
    render_buffer_to_hmd(&vr, &game.frame, &gl, &color_map, &height_map,
                         &camera, num_frames);
#else

    /* glBindFramebuffer(GL_FRAMEBUFFER, 0); */

    struct InputMatrices input_matrices = {0};
    input_matrices.enable_stereo = false;
    input_matrices.framebuffer = 0;

    int32_t window_width, window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);
    input_matrices.framebuffer_width = window_width;
    input_matrices.framebuffer_height = window_height;

    glm_perspective(glm_rad(90), window_width / (float)window_height, 0.01f,
                    5000.0f, input_matrices.projection_matrices[0]);

    glm_lookat((vec3){0.0f, 0.0f, 0.0f}, (vec3){0.0f, 0.0f, -1.0f},
               (vec3){0.0f, 1.0f, 0.0f}, input_matrices.view_matrices[0]);
    render_game(game, &input_matrices);

    SDL_GL_SwapWindow(window);

#endif

    num_frames++;
  }

  // Destroy window
  SDL_DestroyWindow(window);
  game_free(game);
  free(game);

  // Quit SDL subsystems
  SDL_Quit();

  info("EXIT\n");
  return 0;
}
