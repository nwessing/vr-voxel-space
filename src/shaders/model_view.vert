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

out vec3 Position;
out vec4 WorldPosition;
out float CameraDistance;

uniform vec3 cameraPosition;

uniform mat4 projectionViews[2];

// Per instance uniforms
uniform mat4 model;

void main() {
  gl_Position = projectionViews[VIEW_ID] * vec4(aPos.x, aPos.y, aPos.z, 1.0);
  WorldPosition = vec4(gl_Position);
  CameraDistance = distance(cameraPosition, vec3(model * vec4(aPos, 1.0)));
  Position = aPos;
}
