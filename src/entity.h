#ifndef ENTITY_H
#define ENTITY_H

#include <SDL.h>
#include "game_settings.h"
#include "utils.h"
#include "vector.h"

typedef struct {
    vector_2d_t position;
    vector_2d_t velocity;
    vector_2d_t goal_velocity;
    int width;
    int height;
} entity_t;

typedef entity_t ball_t;
typedef entity_t pad_t;

void update_ball(ball_t *, float);
void update_player_one_pad(pad_t *, float);
void update_player_two_pad(pad_t *, ball_t *, float);
void check_collisions(ball_t *, pad_t *);
void handle_pad_key_press(pad_t *, Uint8 const *);
void handle_pad_key_release(pad_t *, Uint8 const *);
void render_entity(entity_t *, SDL_Renderer *);

#endif
