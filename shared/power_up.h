#ifndef POWER_UP_H
#define POWER_UP_H

#include "entity.h"

struct sim_state;

typedef enum { POWER_UP_SPEED_BOOST, POWER_UP_GROW, POWER_UP_SERPENT, POWER_UP_CHARGE } power_up_kind_t;

typedef struct {
    power_up_kind_t slots[POWER_UP_SLOTS];
    int count;
    int selected;
} power_up_inventory_t;

typedef struct {
    power_up_kind_t kind;
    bool active;
    vector_2d_t position;
    vector_2d_t velocity;
    float animation_time;
} power_up_pickup_t;

typedef struct {
    power_up_pickup_t pickups[POWER_UP_MAX_PICKUPS]; // inactive ones are free slots
    bool next_grow_left;
    float player_one_grow_remaining;
    float player_two_grow_remaining;
    bool next_serpent_left;
    float player_one_serpent_remaining;
    float player_two_serpent_remaining;
    bool next_charge_left;
    float player_one_charge_remaining; // time left to catch the ball
    float player_two_charge_remaining;
    int ball_held_by;     // 0, or the player whose pad holds the ball
    float hold_remaining;
    bool ball_charged;
    float spawn_remaining;
    float player_one_remaining;
    float player_two_remaining;
    bool ball_powered;
    bool ball_serpent;
    float serpent_distance;
    float serpent_side;   // 1, or -1 after the wave bounces off a wall
    power_up_inventory_t player_one_inventory;
    power_up_inventory_t player_two_inventory;
    vector_2d_t trail[POWER_UP_TRAIL_LENGTH];
    int trail_count;
} power_up_state_t;

void reset_power_ups(struct sim_state *);
void clear_power_up_pickups(struct sim_state *);
void update_power_ups(struct sim_state *, float);
void debug_spawn_power_up(struct sim_state *, power_up_kind_t, int);
void debug_give_power_up(struct sim_state *, power_up_kind_t, int);
void select_power_up(struct sim_state *, int);
void use_power_up(struct sim_state *, int);
void set_ball_serpent(struct sim_state *, bool);
void weave_ball(struct sim_state *);
bool catch_charged_ball(struct sim_state *, int);
void update_held_ball(struct sim_state *, float);
void record_power_trail(struct sim_state *);

#endif
