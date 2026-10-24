#ifndef AI_H
#define AI_H

#include <stdbool.h>
#include "entity.h"
#include "power_up.h"

typedef struct {
    float reaction_remaining;
    float aim_offset;
    float target_y;
    bool tracking;   // true while the ball moves toward the AI pad
} ai_state_t;

void reset_ai(ai_state_t *);
void update_ai_pad(ai_state_t *, pad_t *, ball_t const *, power_up_state_t const *, float);
int choose_ai_power_up(power_up_inventory_t const *, pad_t const *, ball_t const *, float, float);

#endif
