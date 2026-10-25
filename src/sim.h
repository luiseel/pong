#ifndef SIM_H
#define SIM_H

#include <stdbool.h>
#include <stdint.h>
#include "entity.h"
#include "power_up.h"

#define SIM_STEP (1.f / 240.f) // every substep is this long, in seconds

// Bits of sim_input_t.actions; they apply once, at the start of a sim_step
#define SIM_ACTION_SELECT 0x01 // select the next inventory slot
#define SIM_ACTION_USE 0x02    // use the selected slot
#define SIM_ACTION_PICK_MASK 0x0c
#define SIM_ACTION_PICK(slot) ((uint8_t)(((slot) + 1) << 2)) // select this slot first

// Bits of sim_state_t.events, set during the last sim_step
#define SIM_EVENT_PAD_HIT 0x01  // a pad hit or caught the ball
#define SIM_EVENT_WALL_HIT 0x02 // the ball bounced off the top or bottom wall
#define SIM_EVENT_GOAL 0x04
#define SIM_EVENT_PICKUP 0x08   // a pad collected a power-up
#define SIM_EVENT_BLAST 0x10    // a pad shot the charged ball it held

typedef struct {
    int8_t move;     // -127 is full speed up, 127 full speed down
    uint8_t actions; // SIM_ACTION_* bits
} sim_input_t;

// Plain data without pointers, so a copy or memcmp covers the whole match
typedef struct sim_state {
    ball_t ball;
    pad_t player_one_pad;
    pad_t player_two_pad;
    power_up_state_t power_ups;
    int player_one_score;
    int player_two_score;
    int target_score;  // 0 plays without an end
    int winner;        // 0, or the player that got to target_score
    float accumulator; // time not yet used by a substep
    uint64_t random;   // PCG32 state
    uint32_t events;   // SIM_EVENT_* bits
} sim_state_t;

void sim_init(sim_state_t *, uint64_t, int);
void sim_step(sim_state_t *, sim_input_t const[2], float);
void sim_step_pad(pad_t *, int8_t, float);
int8_t sim_move(float);
uint32_t sim_random(sim_state_t *);
float sim_random_unit(sim_state_t *);

#endif
