#ifndef POWER_UP_H
#define POWER_UP_H

#include "entity.h"

struct game;

typedef enum { POWER_UP_SPEED_BOOST, POWER_UP_GROW } power_up_kind_t;

typedef struct {
    power_up_kind_t kind;
    bool next_grow_left;
    float player_one_grow_remaining;
    float player_two_grow_remaining;
    bool active;
    vector_2d_t position;
    vector_2d_t velocity;
    float spawn_remaining;
    float animation_time;
    float player_one_remaining;
    float player_two_remaining;
    bool ball_powered;
    vector_2d_t trail[POWER_UP_TRAIL_LENGTH];
    int trail_count;
} power_up_state_t;

void reset_power_ups(struct game *);
void update_power_ups(struct game *, float);
void record_power_trail(struct game *);
void render_power_ups(struct game *);

#endif
