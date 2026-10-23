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

#endif
