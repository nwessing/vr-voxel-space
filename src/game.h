#pragma once
#include "types.h"

void render_game(struct Game *); 
int32_t game_init(struct Game *, int32_t width, int32_t height);
int32_t load_assets(struct Game *);
void create_frame_buffer(struct Game *, int32_t width, int32_t height);
void game_free(struct Game *);

