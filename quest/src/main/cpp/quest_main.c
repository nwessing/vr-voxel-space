#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_Input.h"
#include "VrApi_SystemUtils.h"
#include "android_native_app_glue.h"
#include "cglm/cglm.h"
#include "game.h"
#include "types.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl31.h>
#include <android/log.h>
#include <android/window.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define WESSING_DEBUG
#define MULTISAMPLE_SAMPLES 1

static const char *TAG = "com.wessing.vr_voxel_space";

// GL extension function pointers
static PFNGLDEBUGMESSAGECALLBACKKHRPROC glDebugMessageCallbackKHR;
static PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC
    glFramebufferTextureMultisampleMultiviewOVR;

int error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  int result = __android_log_vprint(ANDROID_LOG_ERROR, TAG, format, args);
  va_end(args);
  return result;
}

int info(const char *format, ...) {
  va_list args;
  va_start(args, format);
  int result = __android_log_vprint(ANDROID_LOG_VERBOSE, TAG, format, args);
  va_end(args);
  return result;
}

static const char *egl_get_error_string(EGLint error) {
  switch (error) {
  case EGL_SUCCESS:
    return "EGL_SUCCESS";
  case EGL_NOT_INITIALIZED:
    return "EGL_NOT_INITIALIZED";
  case EGL_BAD_ACCESS:
    return "EGL_BAD_ACCESS";
  case EGL_BAD_ALLOC:
    return "EGL_BAD_ALLOC";
  case EGL_BAD_ATTRIBUTE:
    return "EGL_BAD_ATTRIBUTE";
  case EGL_BAD_CONTEXT:
    return "EGL_BAD_CONTEXT";
  case EGL_BAD_CONFIG:
    return "EGL_BAD_CONFIG";
  case EGL_BAD_CURRENT_SURFACE:
    return "EGL_BAD_CURRENT_SURFACE";
  case EGL_BAD_DISPLAY:
    return "EGL_BAD_DISPLAY";
  case EGL_BAD_SURFACE:
    return "EGL_BAD_SURFACE";
  case EGL_BAD_MATCH:
    return "EGL_BAD_MATCH";
  case EGL_BAD_PARAMETER:
    return "EGL_BAD_PARAMETER";
  case EGL_BAD_NATIVE_PIXMAP:
    return "EGL_BAD_NATIVE_PIXMAP";
  case EGL_BAD_NATIVE_WINDOW:
    return "EGL_BAD_NATIVE_WINDOW";
  case EGL_CONTEXT_LOST:
    return "EGL_CONTEXT_LOST";
  default:
    abort();
  }
}

static const char *gl_get_framebuffer_status_string(GLenum status) {
  switch (status) {
  case GL_FRAMEBUFFER_UNDEFINED:
    return "GL_FRAMEBUFFER_UNDEFINED";
  case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
    return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
  case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
    return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
  case GL_FRAMEBUFFER_UNSUPPORTED:
    return "GL_FRAMEBUFFER_UNSUPPORTED";
  case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
    return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
  default:
    abort();
  }
}

struct Egl {
  EGLDisplay display;
  EGLContext context;
  EGLSurface surface;
};

static void egl_create(struct Egl *egl) {
  info("get EGL display");
  egl->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (egl->display == EGL_NO_DISPLAY) {
    error("can't get EGL display: %s", egl_get_error_string(eglGetError()));
    exit(EXIT_FAILURE);
  }

  info("initialize EGL display");
  if (eglInitialize(egl->display, NULL, NULL) == EGL_FALSE) {
    error("can't initialize EGL display: %s",
          egl_get_error_string(eglGetError()));
    exit(EXIT_FAILURE);
  }

  info("get number of EGL configs");
  EGLint num_configs = 0;
  if (eglGetConfigs(egl->display, NULL, 0, &num_configs) == EGL_FALSE) {
    error("can't get number of EGL configs: %s",
          egl_get_error_string(eglGetError()));
    exit(EXIT_FAILURE);
  }

  info("allocate EGL configs");
  EGLConfig *configs = malloc(num_configs * sizeof(EGLConfig));
  if (configs == NULL) {
    error("cant allocate EGL configs: %s", egl_get_error_string(eglGetError()));
    exit(EXIT_FAILURE);
  }

  info("get EGL configs");
  if (eglGetConfigs(egl->display, configs, num_configs, &num_configs) ==
      EGL_FALSE) {
    error("can't get EGL configs: %s", egl_get_error_string(eglGetError()));
    exit(EXIT_FAILURE);
  }

  info("choose EGL config");
  static const EGLint CONFIG_ATTRIBS[] = {
      EGL_RED_SIZE,   8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE,    8,
      EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0,
      EGL_SAMPLES,    0, EGL_NONE,
  };
  EGLConfig found_config = NULL;
  for (int i = 0; i < num_configs; ++i) {
    EGLConfig config = configs[i];

    info("get EGL config renderable type");
    EGLint renderable_type = 0;
    if (eglGetConfigAttrib(egl->display, config, EGL_RENDERABLE_TYPE,
                           &renderable_type) == EGL_FALSE) {
      error("can't get EGL config renderable type: %s",
            egl_get_error_string(eglGetError()));
      exit(EXIT_FAILURE);
    }
    if ((renderable_type & EGL_OPENGL_ES3_BIT_KHR) == 0) {
      continue;
    }

    info("get EGL config surface type");
    EGLint surface_type = 0;
    if (eglGetConfigAttrib(egl->display, config, EGL_SURFACE_TYPE,
                           &surface_type) == EGL_FALSE) {
      error("can't get EGL config surface type: %s",
            egl_get_error_string(eglGetError()));
      exit(EXIT_FAILURE);
    }
    if ((renderable_type & EGL_PBUFFER_BIT) == 0) {
      continue;
    }
    if ((renderable_type & EGL_WINDOW_BIT) == 0) {
      continue;
    }

    const EGLint *attrib = CONFIG_ATTRIBS;
    while (attrib[0] != EGL_NONE) {
      info("get EGL config attrib");
      EGLint value = 0;
      if (eglGetConfigAttrib(egl->display, config, attrib[0], &value) ==
          EGL_FALSE) {
        error("can't get EGL config attrib: %s",
              egl_get_error_string(eglGetError()));
        exit(EXIT_FAILURE);
      }
      if (value != attrib[1]) {
        break;
      }
      attrib += 2;
    }
    if (attrib[0] != EGL_NONE) {
      continue;
    }

    found_config = config;
    break;
  }
  if (found_config == NULL) {
    error("can't choose EGL config");
    exit(EXIT_FAILURE);
  }

  info("free EGL configs");
  free(configs);

  info("create EGL context");
  // clang-format off
  static const EGLint CONTEXT_ATTRIBS[] = {EGL_CONTEXT_CLIENT_VERSION, 3,
#ifdef WESSING_DEBUG
                                           EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR,
#endif
                                           EGL_NONE};
  // clang-format on
  egl->context = eglCreateContext(egl->display, found_config, EGL_NO_CONTEXT,
                                  CONTEXT_ATTRIBS);
  if (egl->context == EGL_NO_CONTEXT) {
    error("can't create EGL context: %s", egl_get_error_string(eglGetError()));
    exit(EXIT_FAILURE);
  }

  info("create EGL surface");
  static const EGLint SURFACE_ATTRIBS[] = {
      EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE,
  };
  egl->surface =
      eglCreatePbufferSurface(egl->display, found_config, SURFACE_ATTRIBS);
  if (egl->surface == EGL_NO_SURFACE) {
    error("can't create EGL pixel buffer surface: %s",
          egl_get_error_string(eglGetError()));
    exit(EXIT_FAILURE);
  }

  info("make EGL context current");
  if (eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context) ==
      EGL_FALSE) {
    error("can't make EGL context current: %s",
          egl_get_error_string(eglGetError()));
  }
}

static void egl_destroy(struct Egl *egl) {
  eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroySurface(egl->display, egl->surface);
  eglDestroyContext(egl->display, egl->context);
  eglTerminate(egl->display);
}

struct Renderer {
  int swap_chain_index;
  int swap_chain_length;
  GLsizei width;
  GLsizei height;
  ovrTextureSwapChain *color_texture_swap_chain;
  GLuint *depth_textures;
  GLuint *framebuffers;
};

static void renderer_create(struct Renderer *renderer, GLsizei width,
                            GLsizei height) {

  int32_t num_extensions = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
  for (int32_t i = 0; i < num_extensions; ++i) {
    const GLubyte *extension_name = glGetStringi(GL_EXTENSIONS, i);
    info("EXTENSION: %s\n", extension_name);
  }

  renderer->swap_chain_index = 0;
  renderer->width = width;
  renderer->height = height;

  renderer->color_texture_swap_chain = vrapi_CreateTextureSwapChain3(
      VRAPI_TEXTURE_TYPE_2D_ARRAY, GL_RGBA8, width, height, 1, 3);
  if (renderer->color_texture_swap_chain == NULL) {
    error("can't create color texture swap chain");
    exit(EXIT_FAILURE);
  }

  renderer->swap_chain_length =
      vrapi_GetTextureSwapChainLength(renderer->color_texture_swap_chain);

  renderer->depth_textures =
      malloc(renderer->swap_chain_length * sizeof(GLuint));
  if (renderer->depth_textures == NULL) {
    error("can't allocate depth textures");
    exit(EXIT_FAILURE);
  }

  renderer->framebuffers = malloc(renderer->swap_chain_length * sizeof(GLuint));
  if (renderer->framebuffers == NULL) {
    error("can't allocate framebuffers");
    exit(EXIT_FAILURE);
  }

  glGenFramebuffers(renderer->swap_chain_length, renderer->framebuffers);
  glGenTextures(renderer->swap_chain_length, renderer->depth_textures);
  for (int i = 0; i < renderer->swap_chain_length; ++i) {
    GLuint color_texture =
        vrapi_GetTextureSwapChainHandle(renderer->color_texture_swap_chain, i);
    glBindTexture(GL_TEXTURE_2D_ARRAY, color_texture);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    glBindTexture(GL_TEXTURE_2D_ARRAY, renderer->depth_textures[i]);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT24, width, height,
                   2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, renderer->framebuffers[i]);
    glFramebufferTextureMultisampleMultiviewOVR(
        GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, renderer->depth_textures[i],
        0, MULTISAMPLE_SAMPLES, 0, 2);

    glFramebufferTextureMultisampleMultiviewOVR(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color_texture, 0,
        MULTISAMPLE_SAMPLES, 0, 2);

    GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      error("can't create framebuffer %d: %s", i,
            gl_get_framebuffer_status_string(status));
      exit(EXIT_FAILURE);
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  }
}

static void renderer_destroy(struct Renderer *renderer) {
  glDeleteFramebuffers(renderer->swap_chain_length, renderer->framebuffers);
  glDeleteTextures(renderer->swap_chain_length, renderer->depth_textures);

  free(renderer->framebuffers);
  free(renderer->depth_textures);

  vrapi_DestroyTextureSwapChain(renderer->color_texture_swap_chain);
}

static ovrLayerProjection2 renderer_render_frame(struct Game *game,
                                                 struct Renderer *renderer,
                                                 ovrTracking2 *tracking) {
  ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
  layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
  layer.HeadPose = tracking->HeadPose;

  struct InputMatrices matrices = {0};
  matrices.enable_stereo = true;

  for (int32_t i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; ++i) {
    ovrMatrix4f view_matrix =
        ovrMatrix4f_Transpose(&tracking->Eye[i].ViewMatrix);
    ovrMatrix4f projection_matrix =
        ovrMatrix4f_Transpose(&tracking->Eye[i].ProjectionMatrix);

    glm_mat4_copy(projection_matrix.M, matrices.projection_matrices[i]);
    glm_mat4_copy(view_matrix.M, matrices.view_matrices[i]);

    layer.Textures[i].ColorSwapChain = renderer->color_texture_swap_chain;
    layer.Textures[i].SwapChainIndex = renderer->swap_chain_index;
    layer.Textures[i].TexCoordsFromTanAngles =
        ovrMatrix4f_TanAngleMatrixFromProjection(
            &tracking->Eye[i].ProjectionMatrix);

    // TODO: what is this here for?
    /* glClearColor(0.0, 0.0, 0.0, 1.0); */
    /* glScissor(0, 0, 1, framebuffer->height); */
    /* glClear(GL_COLOR_BUFFER_BIT); */
    /* glScissor(framebuffer->width - 1, 0, 1, framebuffer->height); */
    /* glClear(GL_COLOR_BUFFER_BIT); */
    /* glScissor(0, 0, framebuffer->width, 1); */
    /* glClear(GL_COLOR_BUFFER_BIT); */
    /* glScissor(0, framebuffer->height - 1, framebuffer->width, 1); */
    /* glClear(GL_COLOR_BUFFER_BIT); */
  }

  matrices.framebuffer = renderer->framebuffers[renderer->swap_chain_index];

  matrices.framebuffer_width = renderer->width;
  matrices.framebuffer_height = renderer->height;

  render_game(game, &matrices);
  static const GLenum ATTACHMENTS[] = {GL_DEPTH_ATTACHMENT};
  static const GLsizei NUM_ATTACHMENTS =
      sizeof(ATTACHMENTS) / sizeof(ATTACHMENTS[0]);
  glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, NUM_ATTACHMENTS, ATTACHMENTS);
  glFlush();
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  renderer->swap_chain_index =
      (renderer->swap_chain_index + 1) % renderer->swap_chain_length;

  return layer;
}

struct App {
  ovrJava *java;
  struct Egl egl;
  struct Renderer renderer;
  bool resumed;
  ANativeWindow *window;
  ovrMobile *ovr;
  bool back_button_down_previous_frame;
  uint64_t frame_index;
};

static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;

static void app_on_cmd(struct android_app *android_app, int32_t cmd) {
  struct App *app = (struct App *)android_app->userData;
  switch (cmd) {
  case APP_CMD_START:
    info("onStart()");
    break;
  case APP_CMD_RESUME:
    info("onResume()");
    app->resumed = true;
    break;
  case APP_CMD_PAUSE:
    info("onPause()");
    app->resumed = false;
    break;
  case APP_CMD_STOP:
    info("onStop()");
    break;
  case APP_CMD_DESTROY:
    info("onDestroy()");
    app->window = NULL;
    break;
  case APP_CMD_INIT_WINDOW:
    info("surfaceCreated()");
    app->window = android_app->window;
    break;
  case APP_CMD_TERM_WINDOW:
    info("surfaceDestroyed()");
    app->window = NULL;
    break;
  default:
    break;
  }
}

static void app_update_vr_mode(struct App *app) {
  if (app->resumed && app->window != NULL) {
    if (app->ovr == NULL) {
      ovrModeParms mode_parms = vrapi_DefaultModeParms(app->java);
      mode_parms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
      mode_parms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;
      mode_parms.Display = (size_t)app->egl.display;
      mode_parms.WindowSurface = (size_t)app->window;
      mode_parms.ShareContext = (size_t)app->egl.context;

      info("enter vr mode");
      app->ovr = vrapi_EnterVrMode(&mode_parms);
      if (app->ovr == NULL) {
        error("can't enter vr mode");
        exit(EXIT_FAILURE);
      }

      vrapi_SetClockLevels(app->ovr, CPU_LEVEL, GPU_LEVEL);
    }
  } else {
    if (app->ovr != NULL) {
      info("leave vr mode");
      vrapi_LeaveVrMode(app->ovr);
      app->ovr = NULL;
    }
  }
}

static void app_handle_input(struct App *app, double display_time,
                             struct ControllerState *left_controller,
                             struct ControllerState *right_controller) {
  bool back_button_down_current_frame = false;

  int i = 0;
  ovrInputCapabilityHeader capability;
  while (vrapi_EnumerateInputDevices(app->ovr, i, &capability) >= 0) {
    if (capability.Type == ovrControllerType_TrackedRemote) {
      ovrInputTrackedRemoteCapabilities remote_capability;
      remote_capability.Header = capability;
      if (vrapi_GetInputDeviceCapabilities(
              app->ovr, &remote_capability.Header) != ovrSuccess) {
        error("Could not get tracked remote capabilities");
        continue;
      }

      ovrInputStateTrackedRemote input_state;
      input_state.Header.ControllerType = ovrControllerType_TrackedRemote;
      if (vrapi_GetCurrentInputState(app->ovr, capability.DeviceID,
                                     &input_state.Header) != ovrSuccess) {
        error("Could not get input state for tracked remote");
        continue;
      }

      back_button_down_current_frame |= input_state.Buttons & ovrButton_Back;

      bool is_left = (remote_capability.ControllerCapabilities &
                      ovrControllerCaps_LeftHand) > 0;
      bool is_right = (remote_capability.ControllerCapabilities &
                       ovrControllerCaps_RightHand) > 0;
      if (is_left || is_right) {
        struct ControllerState *controller =
            is_left ? left_controller : right_controller;
        controller->joy_stick.x = input_state.Joystick.x;
        controller->joy_stick.y = input_state.Joystick.y;
        controller->trigger = input_state.IndexTrigger;
        controller->grip = input_state.GripTrigger;

        ovrTracking hand_tracking;
        if (vrapi_GetInputTrackingState(app->ovr, capability.DeviceID,
                                        display_time,
                                        &hand_tracking) == ovrSuccess) {
          controller->is_connected = true;

          ovrPosef *input_pose = &hand_tracking.HeadPose.Pose;
          controller->pose.position[0] = input_pose->Position.x;
          controller->pose.position[1] = input_pose->Position.y;
          controller->pose.position[2] = input_pose->Position.z;
          controller->pose.orientation[0] = input_pose->Orientation.x;
          controller->pose.orientation[1] = input_pose->Orientation.y;
          controller->pose.orientation[2] = input_pose->Orientation.z;
          controller->pose.orientation[3] = input_pose->Orientation.w;
        } else {
          controller->is_connected = false;
        }

        if (is_right) {
          controller->primary_button = (input_state.Buttons & ovrButton_A) > 0;
          controller->secondary_button =
              (input_state.Buttons & ovrButton_B) > 0;
        }

        if (is_left) {
          controller->primary_button = (input_state.Buttons & ovrButton_X) > 0;
          controller->secondary_button =
              (input_state.Buttons & ovrButton_Y) > 0;
        }
      }
    }

    ++i;
  }

  if (app->back_button_down_previous_frame && !back_button_down_current_frame) {
    vrapi_ShowSystemUI(app->java, VRAPI_SYS_UI_CONFIRM_QUIT_MENU);
  }
  app->back_button_down_previous_frame = back_button_down_current_frame;
}

static void app_create(struct App *app, ovrJava *java) {
  app->java = java;
  egl_create(&app->egl);

  GLsizei tex_width = vrapi_GetSystemPropertyInt(
      java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH);
  GLsizei tex_height = vrapi_GetSystemPropertyInt(
      java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT);
  renderer_create(&app->renderer, tex_width, tex_height);

  app->resumed = false;
  app->window = NULL;
  app->ovr = NULL;
  app->back_button_down_previous_frame = false;
  app->frame_index = 0;
}

static void debug_message_callback(GLenum source, GLenum type, GLuint id,
                                   GLenum severity, GLsizei length,
                                   const GLchar *message,
                                   const void *user_param) {
  (void)source;
  (void)type;
  (void)id;
  (void)severity;
  (void)length;
  (void)user_param;
  info("GL: %s\n", message);
}

static void load_gl_extension_functions() {
  glDebugMessageCallbackKHR =
      (PFNGLDEBUGMESSAGECALLBACKKHRPROC)eglGetProcAddress(
          "glDebugMessageCallbackKHR");

  glFramebufferTextureMultisampleMultiviewOVR =
      (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)eglGetProcAddress(
          "glFramebufferTextureMultisampleMultiviewOVR");

  if (glFramebufferTextureMultisampleMultiviewOVR == NULL) {
    error("ERROR loading glFramebufferTextureMultisampleMultiviewOVR function "
          "pointer");
    assert(glFramebufferTextureMultisampleMultiviewOVR != NULL);
  }
}

static void app_destroy(struct App *app) {
  egl_destroy(&app->egl);
  renderer_destroy(&app->renderer);
}

void android_main(struct android_app *android_app) {
  ANativeActivity_setWindowFlags(android_app->activity,
                                 AWINDOW_FLAG_KEEP_SCREEN_ON, 0);

  info("starting com.wessing.vr-voxel-space");
  info("attach current thread");
  ovrJava java;
  java.Vm = android_app->activity->vm;
  (*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
  java.ActivityObject = android_app->activity->clazz;

  info("initialize vr api");
  const ovrInitParms init_parms = vrapi_DefaultInitParms(&java);
  if (vrapi_Initialize(&init_parms) != VRAPI_INITIALIZE_SUCCESS) {
    info("can't initialize vr api");
    exit(EXIT_FAILURE);
  }

  load_gl_extension_functions();

  struct App app;
  app_create(&app, &java);

#ifdef WESSING_DEBUG
  assert(glDebugMessageCallbackKHR != NULL);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);
  glDebugMessageCallbackKHR(debug_message_callback, NULL);
#endif

  struct Game *game = calloc(1, sizeof(struct Game));
  if (game_init(game, 800, 600) == GAME_ERROR) {
    error("com.wessing.vr_voxel_space couldn't initialize game");
    exit(EXIT_FAILURE);
  }

  info("com.wessing.vr_voxel_space Game initialized!");

  android_app->userData = &app;
  android_app->onAppCmd = app_on_cmd;
  struct KeyboardState keyboard_state = {0};
  struct ControllerState left_controller = {0};
  struct ControllerState right_controller = {0};
  right_controller.scale_rotation_by_time = true;

  double last_predicted_display_time = 0.0;
  while (!android_app->destroyRequested) {
    for (;;) {
      int events = 0;
      struct android_poll_source *source = NULL;
      if (ALooper_pollAll(android_app->destroyRequested || app.ovr != NULL ? 0
                                                                           : -1,
                          NULL, &events, (void **)&source) < 0) {
        break;
      }
      if (source != NULL) {
        source->process(android_app, source);
      }

      app_update_vr_mode(&app);
    }

    if (app.ovr == NULL) {
      continue;
    }

    app.frame_index++;

    const double display_time =
        vrapi_GetPredictedDisplayTime(app.ovr, app.frame_index);
    float delta = display_time - last_predicted_display_time;
    if (delta < 0) {
      delta = 0;
    }

    app_handle_input(&app, display_time, &left_controller, &right_controller);

    ovrTracking2 tracking = vrapi_GetPredictedTracking2(app.ovr, display_time);

    // Don't update on first render
    if (last_predicted_display_time > 0) {
      struct Pose hmd_pose = {.position =
                                  {
                                      tracking.HeadPose.Pose.Position.x,
                                      tracking.HeadPose.Pose.Position.y,
                                      tracking.HeadPose.Pose.Position.z,
                                  },
                              .orientation = {
                                  tracking.HeadPose.Pose.Orientation.x,
                                  tracking.HeadPose.Pose.Orientation.y,
                                  tracking.HeadPose.Pose.Orientation.z,
                                  tracking.HeadPose.Pose.Orientation.w,
                              }};
      update_game(game, &keyboard_state, &left_controller, &right_controller,
                  hmd_pose, delta);
    }

    const ovrLayerProjection2 layer =
        renderer_render_frame(game, &app.renderer, &tracking);

    const ovrLayerHeader2 *layers[] = {&layer.Header};
    ovrSubmitFrameDescription2 frame;
    frame.Flags = 0;
    frame.SwapInterval = 1;
    frame.FrameIndex = app.frame_index;
    frame.DisplayTime = display_time;
    frame.LayerCount = 1;
    frame.Layers = layers;
    vrapi_SubmitFrame2(app.ovr, &frame);
    last_predicted_display_time = display_time;
  }

  app_destroy(&app);
  game_free(game);
  free(game);

  info("shut down vr api");
  vrapi_Shutdown();

  info("detach current thread");
  (*java.Vm)->DetachCurrentThread(java.Vm);
}
