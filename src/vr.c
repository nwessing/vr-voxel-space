#include "vr.h"
#include "stdint.h"
#include "stdio.h"
#include "assert.h"

void render_buffer_to_hmd(struct vr_data *vr, struct FrameBuffer *frame, struct OpenGLData *gl,
  struct ImageBuffer *color_map, struct ImageBuffer *height_map, struct Camera *camera, int frame_index) {
  assert(glGetError() == GL_NO_ERROR);

  int32_t current_index = 0;
  ovr_GetTextureSwapChainCurrentIndex(vr->session, vr->swap_chain, &current_index);

  int32_t tex_id;
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
  if (OVR_FAILURE(wait_begin_frame_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_WaitToBeginFrame failed: %s\n", error_info.ErrorString);
    assert(false);
  }

  ovrResult begin_frame_result = ovr_BeginFrame(vr->session, frame_index);
  if (OVR_FAILURE(begin_frame_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_BeginFrame failed: %s\n", error_info.ErrorString);
    assert(false);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, gl->frame_buffer);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0);

  for (int32_t eye = 0; eye < 2; ++eye) {
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

    int32_t eye_mod = eye == 1 ? 1 : -1;
    int32_t eye_dist = 3;
    struct Camera eye_cam = *camera;
    eye_cam.position_x += (int32_t)(eye_mod * eye_dist * sin(eye_cam.rotation + (M_PI / 2)));
    eye_cam.position_y += (int32_t)(eye_mod * eye_dist * cos(eye_cam.rotation + (M_PI / 2)));

    render(&eye_buffer, color_map, height_map, &eye_cam);
  }
  // larger than than HMD res, check vp structs
  glViewport(0, 0, 3616, 2000);
  render_buffer_to_gl(frame, gl, camera->clip);

  ovrResult commit_result = ovr_CommitTextureSwapChain(vr->session, vr->swap_chain);
  if (OVR_FAILURE(commit_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_CommitTextureSwapChain failed: %s\n", error_info.ErrorString);
    assert(false);
  }

  ovrLayerHeader* layers = &vr->layer.Header;
  ovrResult end_frame_result = ovr_EndFrame(vr->session, frame_index, NULL, &layers, 1);
  if (OVR_FAILURE(end_frame_result)) {
    ovrErrorInfo error_info;
    ovr_GetLastErrorInfo(&error_info);
    printf("ovr_EndFrame failed: %s\n", error_info.ErrorString);
    assert(false);
  }
}

int32_t init_ovr(struct vr_data *vr) {
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
