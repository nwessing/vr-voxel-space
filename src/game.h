#pragma once
#include "types.h"

void update_game(struct Game *, float elapsed); 
void render_game(struct Game *, mat4 projection); 
int32_t add_event(struct Game *game, struct GameInputEvent event); 
int32_t game_init(struct Game *, int32_t width, int32_t height);
void game_free(struct Game *);

