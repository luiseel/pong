#include "ai.h"
#include <stdlib.h>
#include <math.h>

void reset_ai(ai_state_t *ai) {
    *ai = (ai_state_t){0};
}

static float random_aim_offset(void) {
    float unit = (float)rand() / (float)RAND_MAX;
    return (unit * 2.f - 1.f) * AI_AIM_ERROR;
}

// Center y of a moving square when its left edge reaches target_x,
// with bounces off the top and bottom walls
static float predict_y(vector_2d_t position, float size, vector_2d_t velocity, float target_x) {
    float const low = size / 2.f;
    float const span = GAME_HEIGHT - size;
    float distance = target_x - position.x;
    float y = position.y + size / 2.f;

    // Project along the direction of travel; the speed scale cancels out
    y += distance * velocity.y / velocity.x;
    if (span <= 0.f) return low;

    // Fold the straight-line position back into the walls
    float folded = fmodf(y - low, 2.f * span);
    if (folded < 0.f) folded += 2.f * span;
    if (folded > span) folded = 2.f * span - folded;
    return low + folded;
}

static bool is_ball_incoming(ball_t const *ball, pad_t const *pad) {
    return ball->velocity.x > 0.f && ball->position.x < pad->position.x;
}

static float clamp_target(pad_t const *pad, float target) {
    float const half = pad->height / 2.f;
    return fmaxf(half, fminf(GAME_HEIGHT - half, target));
}

void update_ai_pad(ai_state_t *ai, pad_t *pad, ball_t const *ball,
                   power_up_state_t const *power, float delta_time) {
    // Only objects still in front of the pad count; anything past it is gone
    bool ball_incoming = is_ball_incoming(ball, pad);
    bool power_incoming = power->active && power->velocity.x > 0.f &&
        power->position.x < pad->position.x;

    if (ball_incoming) {
        if (!ai->tracking) {
            ai->tracking = true;
            ai->aim_offset = random_aim_offset();
        }
        ai->reaction_remaining -= delta_time;
        if (ai->reaction_remaining <= 0.f) {
            ai->reaction_remaining = AI_REACTION_TIME;
            float target = predict_y(ball->position, ball->height, ball->velocity,
                pad->position.x - ball->width) + ai->aim_offset;
            ai->target_y = clamp_target(pad, target);
        }
    } else if (power_incoming) {
        // Ball is no threat: go and collect the pickup drifting this way
        ai->tracking = false;
        ai->reaction_remaining = 0.f;
        ai->target_y = clamp_target(pad, predict_y(power->position, POWER_UP_SIZE,
            power->velocity, pad->position.x - POWER_UP_SIZE));
    } else {
        // Nothing to do: drift to center, react at once later
        ai->tracking = false;
        ai->reaction_remaining = 0.f;
        ai->target_y = GAME_HEIGHT / 2.f;
    }

    float diff = ai->target_y - (pad->position.y + pad->height / 2.f);
    if (fabsf(diff) < AI_DEAD_ZONE) {
        pad->goal_velocity.y = 0.f;
    } else {
        pad->goal_velocity.y = diff > 0.f ? AI_MAX_SPEED : -AI_MAX_SPEED;
    }
}

// Slot to use now, or -1: grow at once, speed when the ball comes back
int choose_ai_power_up(power_up_inventory_t const *inventory, pad_t const *pad,
                       ball_t const *ball, float speed_remaining, float grow_remaining) {
    bool ball_incoming = is_ball_incoming(ball, pad);
    for (int i = 0; i < inventory->count; i++) {
        if (inventory->slots[i] == POWER_UP_GROW ? grow_remaining <= 0.f :
            ball_incoming && speed_remaining <= 0.f) {
            return i;
        }
    }
    return -1;
}
