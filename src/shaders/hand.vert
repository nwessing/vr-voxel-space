#ifdef OPENGL_ES
  precision highp float;
  precision highp int;
#endif

#if defined(GL_OVR_multiview2)
  #extension GL_OVR_multiview2 : enable
  layout(num_views = 2) in;
  #define VIEW_ID gl_ViewID_OVR
#else
  #define VIEW_ID 0
#endif

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUv;

out vec3 Position;
out vec2 Uv;

uniform mat4 mvp[2];

void main() {
  gl_Position = mvp[VIEW_ID] * vec4(aPos.x, aPos.y, aPos.z, 1.0);
  Uv = aUv;
}
