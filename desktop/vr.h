#pragma once
#include "LibOVR/OVR_CAPI.h"
#include "LibOVR/OVR_CAPI_GL.h"

struct vr_data {
  ovrSession session;
  ovrEyeRenderDesc eye_render_desc[2];
  ovrPosef hmd_to_eye_view_pose[2];
  ovrHmdDesc hmd_desc;
  ovrLayerEyeFov layer;

  ovrTextureSwapChainDesc chain_desc;
  ovrTextureSwapChain swap_chain;
};
