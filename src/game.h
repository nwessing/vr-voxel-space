#pragma once
#include "types.h"

void update_game(struct Game *, struct KeyboardState *,
                 struct ControllerState *left_controller,
                 struct ControllerState *right_controller, float elapsed);

void render_game(struct Game *, mat4 projection, mat4 view);
int32_t game_init(struct Game *, int32_t width, int32_t height);
void game_free(struct Game *);
