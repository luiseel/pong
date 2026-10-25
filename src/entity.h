#ifndef ENTITY_H
#define ENTITY_H

#include <stdbool.h>
#include "game_settings.h"
#include "utils.h"
#include "vector.h"

typedef struct {
    vector_2d_t position;
    vector_2d_t previous_position;
    vector_2d_t velocity;
    vector_2d_t goal_velocity;
    int width;
    int height;
} entity_t;

typedef entity_t ball_t;
typedef entity_t pad_t;

void update_ball(ball_t *, float, float);
void update_pad(pad_t *, float);
bool check_collisions(ball_t *, pad_t *);
bool bounce_off_walls(ball_t *);
int check_goal(ball_t const *);

#endif
