#include "glad/glad.h"
#include <SDL2/SDL.h>

#ifdef INCLUDE_LIBOVR
#include "vr.h"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "assert.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdint.h"
#include "math.h"
#include "shader.h"
#include "types.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 640

const char *VERTEX_SHADER_SOURCE = "\n\
#version 330 core\n\
layout (location = 0) in vec2 aPos;\n\
\n\
out vec2 TexCoords;\n\
\n\
void main()\n\
{\n\
    gl_Position = vec4(aPos.x, -aPos.y, 0.0, 1.0); \n\
    TexCoords = vec2((aPos.x + 1.0) / 2.0, (aPos.y + 1.0) / 2.0);\n\
}";

const char* FRAGMENT_SHADER_SOURCE = "\n\
#version 330 core\n\
out vec4 FragColor;\n\
\n\
in vec2 TexCoords;\n\
\n\
uniform sampler2D screenTexture;\n\
\n\
void main()\n\
{ \n\
    vec4 texColor = texture(screenTexture, TexCoords);\n\
    if (texColor.a < 0.5) {\n\
      discard;\n\
    }\n\
    FragColor = texColor;\n\
}";

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
    result.a = 255;
  }

  return result;
}

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

inline int clamp(int position, int min, int max) {
  if (position < min) {
    return min;
  }

  if (position > max) {
    return max;
  }

  return position;
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

void check_opengl_error(char *name) {
  GLenum gl_error = glGetError();
  if (gl_error) {
    printf("(%s) error: %i\n", name, gl_error);
  }
}

void render_buffer_to_gl(struct FrameBuffer *frame, struct OpenGLData *gl, int clip) {
  glClearColor(0.529f, 0.808f, 0.98f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glUseProgram(gl->shader_program);
  glBindVertexArray(gl->vao);

  // int32_t clipped_buffer_width = (frame->width - (clip * 2));
  // uint8_t *clipped_buffer = malloc(clipped_buffer_width * frame->height * 4);
  // for (uint32_t i = 0; i < clipped_buffer_width * frame->height; i++) {
  //   int32_t column = i % clipped_buffer_width;
  //   int32_t row = i / clipped_buffer_width;

  //   bool right = column >= clipped_buffer_width / 2;
  //   int32_t source_row = row;
  //   int32_t source_column = column;
  //   if (right) {
  //     source_column = column + (clip * 2);
  //   }

  //   int32_t offset = (source_column * 4) + (source_row * frame->pitch);
  //   for (int32_t component = 0; component < 4; component++) {
  //     clipped_buffer[(i * 4) + component] = frame->pixels[offset + component];
  //   }
  // }

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gl->tex_id);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->pitch / 4 );
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width - (clip * 2), frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame->pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, clipped_buffer_width, frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, clipped_buffer);
  // free(clipped_buffer);
  check_opengl_error("glTexImage2D");

  glDrawArrays(GL_TRIANGLES, 0, gl->vao_num_vertices);
}

#ifdef INCLUDE_LIBOVR
void render_buffer_to_hmd(struct vr_data *vr, struct FrameBuffer *frame, struct OpenGLData *gl,
  struct ImageBuffer *color_map, struct ImageBuffer *height_map, struct Camera *camera, int frame_index) {
  assert(glGetError() == GL_NO_ERROR);

  int current_index = 0;
  ovr_GetTextureSwapChainCurrentIndex(vr->session, vr->swap_chain, &current_index);

  int tex_id;
  ovr_GetTextureSwapChainBufferGL(vr->session, vr->swap_chain, current_index, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  ovrTrackingState tracking_state = ovr_GetTrackingState(vr->session, 0, false);
  ovrPosef eyePoses[2];
  ovr_CalcEyePoses(tracking_state.HeadPose.ThePose, vr->hmd_to_eye_view_pose, eyePoses);
  vr->layer.RenderPose[0] = eyePoses[0];
  vr->layer.RenderPose[1] = eyePoses[1];

  ovrResult wait_begin_frame_result = ovr_WaitToBeginFrame(vr->session, frame_index);
  if(OVR_FAILURE(wait_begin_frame_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_WaitToBeginFrame failed: %s\n", error_info.ErrorString);
    assert(false);
  }

  ovrResult begin_frame_result = ovr_BeginFrame(vr->session, frame_index);
  if(OVR_FAILURE(begin_frame_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_BeginFrame failed: %s\n", error_info.ErrorString);
    assert(false);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, gl->frame_buffer);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0);

  for (int eye = 0; eye < 2; ++eye) {
    ovrRecti vp = vr->layer.Viewport[eye];

    struct FrameBuffer eye_buffer;
    eye_buffer.width = frame->width / 2;
    eye_buffer.height = frame->height;
    eye_buffer.pitch = frame->pitch;
    // eye_buffer.pixels = &frame->pixels[eye * (frame->width / 2) * 4];
    eye_buffer.y_buffer = &frame->y_buffer[eye * (eye_buffer.width / 2)];
    eye_buffer.clip_left_x = eye == 0 ? 0 : camera->clip;
    eye_buffer.clip_right_x = eye_buffer.width - (eye == 0 ? camera->clip : 0);
    eye_buffer.pixels = eye == 0 ? frame->pixels : &frame->pixels[(eye_buffer.width - camera->clip * 2) * 4];

    int eye_mod = eye == 1 ? 1 : -1;
    int eye_dist = 3;
    struct Camera eye_cam = *camera;
    eye_cam.position_x += (int)(eye_mod * eye_dist * sin(eye_cam.rotation + (M_PI / 2)));
    eye_cam.position_y += (int)(eye_mod * eye_dist * cos(eye_cam.rotation + (M_PI / 2)));

    render(&eye_buffer, color_map, height_map, &eye_cam);
  }
  // larger than than HMD res, check vp structs
  glViewport(0, 0, 3616, 2000);
  render_buffer_to_gl(frame, gl, camera->clip);

  ovrResult commit_result = ovr_CommitTextureSwapChain(vr->session, vr->swap_chain);
  if(OVR_FAILURE(commit_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_CommitTextureSwapChain failed: %s\n", error_info.ErrorString);
    assert(false);
  }

  ovrLayerHeader* layers = &vr->layer.Header;
  ovrResult end_frame_result = ovr_EndFrame(vr->session, frame_index, NULL, &layers, 1);
  if(OVR_FAILURE(end_frame_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_EndFrame failed: %s\n", error_info.ErrorString);
    assert(false);
  }

  // printf("current_index = %i, tex_id = %i\n", current_index, tex_id);
  // glBindTexture(GL_TEXTURE_2D, tex_id);
}
#endif

void create_gl_objects(struct OpenGLData *gl) {
  float vertices[] = {
    -1.0, -1.0, 0.0,
    -1.0,  1.0, 0.0,
     1.0,  1.0, 0.0,
    -1.0, -1.0, 0.0,
     1.0, -1.0, 0.0,
     1.0,  1.0, 0.0,
  };

  glGenFramebuffers(1, &gl->frame_buffer);

	glGenBuffers(1, &gl->vbo);

	glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
  gl->vao_num_vertices = sizeof(vertices) / 3;
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glGenVertexArrays(1, &gl->vao);
	glBindVertexArray(gl->vao);
	glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

  gl->shader_program = create_shader(VERTEX_SHADER_SOURCE, FRAGMENT_SHADER_SOURCE);
  assert(gl->shader_program);

  glGenTextures(1, &gl->tex_id);
  glBindTexture(GL_TEXTURE_2D, gl->tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

#ifdef INCLUDE_LIBOVR
int init_ovr(struct vr_data *vr) {
  ovrResult init_result = ovr_Initialize(NULL);
  if (OVR_FAILURE(init_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_Initialize failed: %s\n", error_info.ErrorString);
    return 1;
  }

  ovrSession session;
  ovrGraphicsLuid luid;
  ovrResult create_result = ovr_Create(&session, &luid);
  if(OVR_FAILURE(create_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_Create failed: %s\n", error_info.ErrorString);
    return 1;
  }

  ovrHmdDesc hmd_desc = ovr_GetHmdDesc(session);
  printf("HMD Type: %i\n", hmd_desc.Type);
  printf("HMD Manufacturer: %s\n", hmd_desc.Manufacturer);
  ovrSizei resolution = hmd_desc.Resolution;
  printf("HMD Resolution: %i x %i\n", resolution.w, resolution.h);

  ovrFovPort left_fov = hmd_desc.DefaultEyeFov[ovrEye_Left];
  ovrFovPort right_fov = hmd_desc.DefaultEyeFov[ovrEye_Right];
  ovrSizei recommenedTex0Size = ovr_GetFovTextureSize(session, ovrEye_Left, left_fov , 1.0f);
  ovrSizei recommenedTex1Size = ovr_GetFovTextureSize(session, ovrEye_Right, right_fov, 1.0f);
  ovrSizei bufferSize;
  bufferSize.w  = recommenedTex0Size.w + recommenedTex1Size.w;
  bufferSize.h = max ( recommenedTex0Size.h, recommenedTex1Size.h );

  ovrTextureSwapChainDesc chain_desc = {0};
  chain_desc.Type = ovrTexture_2D;
  chain_desc.ArraySize = 1;
  chain_desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
  chain_desc.Width = bufferSize.w;
  chain_desc.Height = bufferSize.h;
  chain_desc.MipLevels = 1;
  chain_desc.SampleCount = 1;
  chain_desc.StaticImage = ovrFalse;

  ovrTextureSwapChain chain;
  ovrResult create_swap_chain_result = ovr_CreateTextureSwapChainGL(session, &chain_desc, &chain);
  if (OVR_FAILURE(create_swap_chain_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_CreateTextureSwapChainGL failed: %s\n", error_info.ErrorString);
    return 1;
  }

    // Initialize VR structures, filling out description.
  ovrEyeRenderDesc eyeRenderDesc[2];
  ovrPosef      hmdToEyeViewPose[2];
  // ovrHmdDesc hmdDesc = ovr_GetHmdDesc(session);
  eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmd_desc.DefaultEyeFov[0]);
  eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmd_desc.DefaultEyeFov[1]);
  hmdToEyeViewPose[0] = eyeRenderDesc[0].HmdToEyePose;
  hmdToEyeViewPose[1] = eyeRenderDesc[1].HmdToEyePose;

  // Initialize our single full screen Fov layer.
  ovrLayerEyeFov layer;
  layer.Header.Type      = ovrLayerType_EyeFov;
  layer.Header.Flags     = ovrLayerFlag_TextureOriginAtBottomLeft;
  layer.ColorTexture[0]  = chain;
  layer.ColorTexture[1]  = chain;
  layer.Fov[0]           = eyeRenderDesc[0].Fov;
  layer.Fov[1]           = eyeRenderDesc[1].Fov;
  layer.Viewport[0]      = (ovrRecti) {
    .Pos = { .x = 0, .y = 0 },
    .Size = { .w = bufferSize.w / 2, .h = bufferSize.h }
  };
  layer.Viewport[1] = (ovrRecti) {
    .Pos = { .x = bufferSize.w / 2, .y = 0 },
    .Size = { .w = bufferSize.w / 2, .h = bufferSize.h }
  };

  // ovrResult create_swap_chain_result = ovr_GetTextureSwapChainLength(session, &chain, &chain_len);

  // *tex_width = chain_desc.Width;
  // *tex_height = chain_desc.Height;
  // ovr_GetTextureSwapChainBufferGL(session, chain, 0, gl_tex_id);
  // printf("GL TEXTURE ID: %i\n", *gl_tex_id);

  // ovr_Destroy(session);
  // ovr_Shutdown();

  vr->session = session;
  vr->eye_render_desc[0] = eyeRenderDesc[0];
  vr->eye_render_desc[1] = eyeRenderDesc[1];
  vr->hmd_to_eye_view_pose[0] = hmdToEyeViewPose[0];
  vr->hmd_to_eye_view_pose[1] = hmdToEyeViewPose[1];
  vr->layer = layer;
  vr->session = session;
  vr->hmd_desc = hmd_desc;
  vr->chain_desc = chain_desc;
  vr->swap_chain = chain;

  return 0;
}
#endif

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

  SDL_Window *window = SDL_CreateWindow("VR Voxel Space", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
  if(window == NULL) {
      printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
      return 1;
  }

  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  char *sdl_error = SDL_GetError();
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
  f_buffer.width = 1264;
  f_buffer.height = 704;
  // f_buffer.width = 2528;
  // f_buffer.height = 1408;
  f_buffer.clip_left_x = 0,
  f_buffer.clip_right_x = f_buffer.width,
  f_buffer.y_buffer = malloc(f_buffer.width * sizeof(int32_t));
  f_buffer.pixels = malloc(f_buffer.width * f_buffer.height * sizeof(uint8_t) * 4);
  f_buffer.pitch = f_buffer.width * sizeof(uint32_t);
  printf("PITCH: %i\n", f_buffer.pitch);

  struct OpenGLData gl = {0};
  create_gl_objects(&gl);

  struct Camera camera = {
    .distance = 800,
    .rotation = M_PI,
    .horizon = f_buffer.height / 2,
    .scale_height = f_buffer.height * 0.35,
    .position_x = 436,
    .position_y = 54,
    .position_height = 50,
    .clip = .06f * f_buffer.width //163
  };

  bool quit = false;
  unsigned int time_last = SDL_GetTicks();
  unsigned int num_frames = 0;
  unsigned int time_begin = SDL_GetTicks();

  bool turn_right = false;
  bool turn_left = false;
  bool move_forward = false;
  bool move_backward = false;
  bool render_stereo = false;
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

      if (e.key.keysym.sym == SDLK_f && e.key.repeat == 0 && e.type == SDL_KEYDOWN) {
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
        camera.clip++;
        printf("clip %i\n", camera.clip);
      }

      if (e.key.keysym.sym == SDLK_r)
      {
        camera.clip--;
        printf("clip %i\n", camera.clip);
      }
      if (e.key.keysym.sym == SDLK_v && e.key.repeat == 0 && e.type == SDL_KEYDOWN)
      {
        render_stereo = !render_stereo;
      }
    }

    if (move_forward || move_backward) {
      int modifier = move_forward ? -1 : 1;
      camera.position_y += modifier * cos(camera.rotation) * 200 * elapsed;
      camera.position_x += modifier * sin(camera.rotation) * 200 * elapsed;
    }

    if (turn_left || turn_right) {
      int modifier = turn_left ? 1 : -1;
      camera.rotation += modifier * M_PI * elapsed;
    }

    while (camera.rotation >= 2 * M_PI) {
      camera.rotation -= 2 * M_PI;
    }

    while (camera.rotation < 0) {
      camera.rotation += 2 * M_PI;
    }

    camera.position_x = camera.position_x % height_map.width;
    camera.position_y = camera.position_y % height_map.height;

    if (camera.position_x < 0) {
      camera.position_x += height_map.width;
    }

    if (camera.position_y < 0) {
      camera.position_y += height_map.height;
    }

    camera.position_height = get_image_grey(&height_map, camera.position_x, camera.position_y) + 50;
    memset(f_buffer.pixels, 0, f_buffer.height * f_buffer.pitch);

#ifdef INCLUDE_LIBOVR
    render_buffer_to_hmd(&vr, &f_buffer, &gl, &color_map, &height_map, &camera, num_frames);
#else
    if (render_stereo) {
      for (int eye = 0; eye < 2; ++eye) {
        struct FrameBuffer eye_buffer;
        eye_buffer.width = f_buffer.width / 2;
        eye_buffer.height = f_buffer.height;
        eye_buffer.pitch = f_buffer.pitch;
        eye_buffer.y_buffer = &f_buffer.y_buffer[eye * (eye_buffer.width / 2)];
        eye_buffer.clip_left_x = eye == 0 ? 0 : camera.clip;
        eye_buffer.clip_right_x = eye_buffer.width - (eye == 0 ? camera.clip : 0);
        eye_buffer.pixels = eye == 0 ? f_buffer.pixels : &f_buffer.pixels[(eye_buffer.width - camera.clip * 2) * 4];

        int eye_mod = eye == 1 ? 1 : -1;
        int eye_dist = 3;
        struct Camera eye_cam = camera;
        eye_cam.position_x += (int)(eye_mod * eye_dist * sin(eye_cam.rotation + (M_PI / 2)));
        eye_cam.position_y += (int)(eye_mod * eye_dist * cos(eye_cam.rotation + (M_PI / 2)));

        render(&eye_buffer, &color_map, &height_map, &eye_cam);
      }
    } else {
      render(&f_buffer, &color_map, &height_map, &camera);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    render_buffer_to_gl(&f_buffer, &gl, camera.clip);
    SDL_GL_SwapWindow(window);

#endif

    num_frames++;
  }

  free(f_buffer.y_buffer);
  free(f_buffer.pixels);

  //Destroy window
  SDL_DestroyWindow(window);
  stbi_image_free(color_map.pixels);
  stbi_image_free(height_map.pixels);

  //Quit SDL subsystems
  SDL_Quit();

  printf("EXIT\n");
  return 0;
}
