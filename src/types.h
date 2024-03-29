#pragma once
#include "game_gl.h"
#include "stdint.h"
#include <cglm/cglm.h>

#define BASE_MAP_SIZE 1024

#define GAME_SUCCESS 0
#define GAME_ERROR 1

struct FrameBuffer {
  int32_t width;
  int32_t height;
  int32_t clip_left_x;
  int32_t clip_right_x;
  uint8_t *pixels;
  int32_t pitch;
  int32_t *y_buffer;
};

struct AstcHeader {
  uint8_t magic[4];
  uint8_t blockdim_x;
  uint8_t blockdim_y;
  uint8_t blockdim_z;
  uint8_t xsize[3];
  uint8_t ysize[3];
  uint8_t zsize[3];
};

struct AstcImageBuffer {
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t num_blocks_x;
  uint32_t num_blocks_y;
  uint32_t num_blocks_z;
  uint32_t data_size;
  uint32_t image_data_size;
  uint8_t *data;
  uint8_t *image_data;
};

struct ImageBuffer {
  int width;
  int height;
  uint8_t *pixels;
  int num_channels;
};

typedef float V3[3];

struct Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

struct RaytraceCamera {
  int32_t distance;
  int32_t horizon;
  int32_t clip;
};

struct Camera {
  int32_t viewport_width;
  int32_t viewport_height;
  float pitch;
  versor quat;
  int32_t scale_height;
  vec3 position;
  vec3 last_hmd_position;
  float terrain_scale;
  struct RaytraceCamera ray;
  // Derived data
  vec3 up;
  vec3 right;
  vec3 front;
  vec4 sky_color;
};

struct InputMatrices {
  bool enable_stereo;
  mat4 projection_matrices[2];
  mat4 view_matrices[2];
  GLuint framebuffer;
  int32_t framebuffer_width;
  int32_t framebuffer_height;
};

struct RenderingMatrices {
  bool enable_stereo;
  mat4 projection_matrices[2];
  mat4 view_matrices[2];
  mat4 projection_view_matrices[2];
};

struct DrawElementsIndirectCommand {
  uint32_t count;
  uint32_t instance_count;
  uint32_t first_index;
  uint32_t base_vertex;
  uint32_t reserved_must_be_zero;
};

struct TerrainShaderUniforms {
  GLint height_map_size;
  GLint fog_color;
  GLint terrain_scale;
  GLint flags;
  GLint camera_position;
  GLint projection_views[2];
  GLint model;
  GLint blend_color;
};

struct HandShaderUniforms {
  GLint color_map;
  GLint mvp[2];
};

struct CubeBuffer {
  GLuint vao;
  GLuint vertex_vbo;
  GLuint index_vbo;
  GLsizei index_count;
  void *index_offset;
};

struct OpenGLData {
  GLuint frame_buffer;
  GLuint terrain_shader;
  struct TerrainShaderUniforms terrain_shader_uniforms;
  GLuint hand_shader;
  struct HandShaderUniforms hand_shader_uniforms;
  GLuint shader_program;
  GLuint vbo;
  GLuint vao;
  GLuint tex_id;
  uint32_t vao_num_vertices;
  struct CubeBuffer cube_buffer;
  GLuint frustum_vis_vao;
  GLuint frustum_vis_vbo;
  GLuint white_tex_id;
  GLuint draw_command_vbo;
};

struct GameOptions {
  bool visualize_lod;
  bool show_fog;
  bool show_wireframe;
  bool do_raycasting;
  bool render_stereo;
  bool visualize_frustum;
};

// NOTE: Represent states of all keys for ASCII codes 32-127
#define KEYBOAD_STATE_MIN_CHAR 32
#define KEYBOARD_STATE_MAX_CHAR 127

#define KEYBOARD_STATE_NUM_KEYS                                                \
  KEYBOARD_STATE_MAX_CHAR - KEYBOAD_STATE_MIN_CHAR + 1
struct KeyboardState {
  bool down[KEYBOARD_STATE_NUM_KEYS];
};

// -1.0 to 1.0
// for both x and y
struct Vec2 {
  float x;
  float y;
};
struct Pose {
  vec3 position;
  versor orientation;
};

struct ControllerState {
  struct Pose pose;
  struct Vec2 joy_stick;

  // 0.0 to 1.0
  float trigger;

  // 0.0 to 1.0
  float grip;
  bool primary_button;
  bool secondary_button;
  bool scale_rotation_by_time;
  bool is_connected;
};

struct MapEntry {
  char *color;
  char *height;
};

struct Mesh {
  int32_t offset;
  int32_t num_indices;
};

struct DrawCommand {
  mat4 model_matrix;
  struct Mesh mesh;
  int32_t lod;
};

#define LOD_COUNT 3
struct MapSection {
  vec3 center;
  float bounding_sphere_radius;
  struct Mesh lods[LOD_COUNT];
};

#define MAP_X_SEGMENTS 4
#define MAP_Y_SEGMENTS MAP_X_SEGMENTS
#define MAP_SECTION_COUNT (MAP_X_SEGMENTS * MAP_Y_SEGMENTS)

#define MAX_MIP_LEVELS 16

struct Map {
#ifdef VR_VOX_USE_ASTC
  struct AstcImageBuffer color_map[MAX_MIP_LEVELS];
  uint32_t num_mip_levels;
#else
  struct ImageBuffer color_map;
#endif
  struct ImageBuffer height_map;
  // multiply by this value to get 1024
  float modifier;
  GLuint map_vbo;
  GLuint map_vbo_indices;
  GLuint map_vao;
  GLuint color_map_tex_id;
  struct MapSection sections[MAP_SECTION_COUNT];
};

struct WorldSection {
  uint32_t section_index;
  int32_t map_x;
  int32_t map_y;
  float camera_distance;
};

struct RenderState {
  struct DrawCommand commands[1024];
  int32_t num_commands;
  int32_t capacity;
  struct WorldSection sections_by_distance[1024];
  int32_t num_sections;
};

#define LEFT_CONTROLLER_INDEX 0
#define RIGHT_CONTROLLER_INDEX 1
#define MAP_COUNT 30
struct Game {
  struct GameOptions options;
  struct Camera camera;
  int map_index;
  struct Map maps[MAP_COUNT];
  struct FrameBuffer frame;
  struct OpenGLData gl;
  struct KeyboardState prev_keyboard;
  struct KeyboardState keyboard;
  struct ControllerState prev_controller[2];
  struct ControllerState controller[2];
  bool trigger_set[2];
  struct RenderState render_state;
};
