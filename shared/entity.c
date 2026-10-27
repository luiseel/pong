#include "entity.h"
#include <math.h>

void update_ball(ball_t *ball, float delta_time, float multiplier) {
    ball->previous_position = ball->position;
    float const speed = BALL_SPEED;

    vector_2d_t velocity = ball->velocity;
    multiply_vector_by_scalar(&velocity, delta_time * speed * multiplier);
    add_vectors(&ball->position, ball->position, velocity);
}

// Top/bottom walls keep the angle; true on a bounce
bool bounce_off_walls(ball_t *ball) {
    if (ball->position.y < 0.f) {
        ball->position.y = 0.f;
        ball->velocity.y = -ball->velocity.y;
        return true;
    }
    if (ball->position.y + ball->height > GAME_HEIGHT) {
        ball->position.y = GAME_HEIGHT - ball->height;
        ball->velocity.y = -ball->velocity.y;
        return true;
    }
    return false;
}

// The player that scores when the ball is past a side wall, or 0
int check_goal(ball_t const *ball) {
    if (ball->position.x < 0.f) return 2;
    if (ball->position.x + ball->width > GAME_WIDTH) return 1;
    return 0;
}

// Both pads move the same way: the velocity eases toward the goal, and the
// position stays inside the field
void update_pad(pad_t *pad, float delta_time) {
    pad->previous_position = pad->position;
    float const max_speed = 400.f;
    float const friction = 10.f;

    pad->velocity.y += (pad->goal_velocity.y - pad->velocity.y) * friction * delta_time;

    float speed = pad->velocity.y * max_speed;
    pad->position.y += speed * delta_time;

    if (pad->position.y < 0.f) {
        pad->position.y = 0.f;
    }
    if (pad->position.y + pad->height > GAME_HEIGHT) {
        pad->position.y = GAME_HEIGHT - pad->height;
    }
}

static float clampf(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static bool check_pad_edge_collision(ball_t *ball, pad_t *pad) {
    float start_y = ball->previous_position.y - pad->previous_position.y;
    float end_y = ball->position.y - pad->position.y;
    bool top = start_y <= -ball->height && end_y > -ball->height;
    bool bottom = start_y >= pad->height && end_y < pad->height;
    if (!top && !bottom) return false;

    float edge = top ? -ball->height : pad->height;
    float contact_time = (edge - start_y) / (end_y - start_y);
    float ball_x = ball->previous_position.x +
        (ball->position.x - ball->previous_position.x) * contact_time;
    if (ball_x >= pad->position.x + pad->width ||
        ball_x + ball->width <= pad->position.x) {
        return false;
    }

    float movement_scale = 0.f;
    if (ball->velocity.x != 0.f) {
        movement_scale = (ball->position.x - ball->previous_position.x) / ball->velocity.x;
    } else if (ball->velocity.y != 0.f) {
        movement_scale = (ball->position.y - ball->previous_position.y) / ball->velocity.y;
    }
    float pad_velocity = movement_scale > 0.f ?
        (pad->position.y - pad->previous_position.y) / movement_scale : 0.f;
    ball->velocity.y = clampf(2.f * pad_velocity - ball->velocity.y,
        -BALL_MAX_EDGE_SPEED_Y, BALL_MAX_EDGE_SPEED_Y);
    ball->position.y = pad->position.y + edge;
    return true;
}

bool check_collisions(ball_t *ball, pad_t *pad) {
    if (check_pad_edge_collision(ball, pad)) return true;
    bool left_pad = pad->position.x < GAME_WIDTH / 2;
    if ((left_pad && ball->velocity.x >= 0) ||
        (!left_pad && ball->velocity.x <= 0)) {
        return false;
    }
    float face = left_pad ? pad->position.x + pad->width : pad->position.x - ball->width;
    float start_x = ball->previous_position.x;
    float end_x = ball->position.x;
    if (left_pad ? (start_x < face || end_x > face) :
                   (start_x > face || end_x < face)) {
        return false;
    }
    if (start_x == end_x) {
        return false;
    }

    float contact_time = (face - start_x) / (end_x - start_x);
    float ball_y = ball->previous_position.y +
        (ball->position.y - ball->previous_position.y) * contact_time;
    float pad_y = pad->previous_position.y +
        (pad->position.y - pad->previous_position.y) * contact_time;
    if (ball_y >= pad_y + pad->height || ball_y + ball->height <= pad_y) {
        return false;
    }

    float offset = ((ball_y + ball->height / 2.f) - (pad_y + pad->height / 2.f)) /
        (pad->height / 2.f + ball->height / 2.f);
    offset = clampf(offset + pad->velocity.y * BALL_PAD_SPIN, -1.f, 1.f);
    float const max_angle = BALL_MAX_BOUNCE_DEGREES * 3.14159265f / 180.f;
    float angle = offset * max_angle;
    ball->velocity.x = left_pad ? cosf(angle) : -cosf(angle);
    ball->velocity.y = sinf(angle);
    ball->position.x = left_pad ? pad->position.x + pad->width : pad->position.x - ball->width;
    return true;
}
