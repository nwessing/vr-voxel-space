#include "types.h"
#include "game.h"
#include "math.h"
#include "stb_image.h"

int32_t game_init(struct Game *game, int32_t width, int32_t height) {
  if (load_assets(game) == GAME_ERROR) {
    return GAME_ERROR;
  }

  create_frame_buffer(game, width, height);

  game->camera.viewport_width = width;
  game->camera.viewport_height = height;
  game->camera.distance = 800;
  game->camera.rotation = M_PI;
  game->camera.horizon = game->frame.height / 2;
  game->camera.scale_height = game->frame.height * 0.35;
  game->camera.position_x = 436;
  game->camera.position_y = 54;
  game->camera.position_height = 50;
  game->camera.clip = .06f * game->frame.width;

  return GAME_SUCCESS;
}

int32_t load_assets(struct Game *game) {
  /* struct ImageBuffer color_map; */
  game->color_map.pixels = stbi_load("C1W.png", &game->color_map.width, &game->color_map.height, &game->color_map.num_channels, 0);
  if (game->color_map.pixels == NULL) {
    printf("Could not load color map");
    return GAME_ERROR;
  }

  /* struct ImageBuffer height_map; */
  game->height_map.pixels = stbi_load("D1.png", &game->height_map.width, &game->height_map.height, &game->height_map.num_channels, 0);
  if (game->height_map.pixels == NULL) {
    printf("Could not load height map");
    return GAME_ERROR;
  }            

  return GAME_SUCCESS;
}

void create_frame_buffer(struct Game *game, int32_t width, int32_t height) {
  game->frame.width = width;
  game->frame.height = height;
  // frame.width = 2528;
  // frame.height = 1408;
  game->frame.clip_left_x = 0,
  game->frame.clip_right_x = game->frame.width,
  game->frame.y_buffer = malloc(game->frame.width * sizeof(int32_t));
  game->frame.pixels = malloc(game->frame.width * game->frame.height * sizeof(uint8_t) * 4);
  game->frame.pitch = game->frame.width * sizeof(uint32_t); 
}

void game_free(struct Game *game) {
  if (game->color_map.pixels != NULL) {
    stbi_image_free(game->color_map.pixels);
    game->color_map.pixels = NULL;
  }

  if (game->height_map.pixels != NULL) {
    stbi_image_free(game->height_map.pixels);
    game->height_map.pixels = NULL;
  }

  if (game->frame.y_buffer != NULL) {
    free(game->frame.y_buffer);
    game->frame.y_buffer = NULL;
  }

  if (game->frame.pixels != NULL) {
    free(game->frame.pixels);
    game->frame.pixels = NULL;
  }
}
