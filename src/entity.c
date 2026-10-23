#include "entity.h"
#include <stdlib.h>

void update_ball(ball_t *ball, float delta_time, float multiplier) {
    float const speed = 250.f;

    vector_2d_t velocity = ball->velocity;
    multiply_vector_by_scalar(&velocity, delta_time * speed * multiplier);
    add_vectors(&ball->position, ball->position, velocity);
}

void reset_ball(ball_t *ball) {
    ball->position.x = GAME_WIDTH / 2 - ball->width / 2;
    ball->position.y = GAME_HEIGHT / 2 - ball->height / 2;
    ball->velocity.x = (rand() % 2 == 0) ? 1.f : -1.f;
    ball->velocity.y = (rand() % 2 == 0) ? 0.5f : -0.5f;
}

void check_wall_collisions(ball_t *ball, int *player_one_score, int *player_two_score) {
    // Top/bottom wall collisions - preserve angle
    if (ball->position.y < 0.f) {
        ball->position.y = 0.f;
        ball->velocity.y = -ball->velocity.y;
    }
    if (ball->position.y + ball->height > GAME_HEIGHT) {
        ball->position.y = GAME_HEIGHT - ball->height;
        ball->velocity.y = -ball->velocity.y;
    }

    // Left/right wall collisions - scoring
    if (ball->position.x < 0.f) {
        (*player_two_score)++;
        reset_ball(ball);
    }
    if (ball->position.x + ball->width > GAME_WIDTH) {
        (*player_one_score)++;
        reset_ball(ball);
    }
}

void update_player_one_pad(pad_t *pad, float delta_time) {
    float const max_speed = 400.f;
    float const friction = 10.f;

    // Smooth acceleration toward goal velocity
    pad->velocity.y += (pad->goal_velocity.y - pad->velocity.y) * friction * delta_time;

    // Apply velocity with clamped speed
    float speed = pad->velocity.y * max_speed;
    pad->position.y += speed * delta_time;

    if (pad->position.y < 0.f) {
        pad->position.y = 0.f;
    }
    if (pad->position.y + pad->height > GAME_HEIGHT) {
        pad->position.y = GAME_HEIGHT - pad->height;
    }
}

void update_player_two_pad(pad_t *pad, float delta_time) {
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

bool check_collisions(ball_t *ball, pad_t *pad) {
    bool left_pad = pad->position.x < GAME_WIDTH / 2;
    if ((left_pad && ball->velocity.x >= 0) ||
        (!left_pad && ball->velocity.x <= 0)) {
        return false;
    }
    if (ball->position.x >= pad->position.x + pad->width ||
        ball->position.x + ball->width <= pad->position.x ||
        ball->position.y >= pad->position.y + pad->height ||
        ball->position.y + ball->height <= pad->position.y) {
        return false;
    }

    float ball_cx = ball->position.x + ball->width / 2.f;
    float pad_cx = pad->position.x + pad->width / 2.f;
    float max_dist = (ball->width + pad->width) / 2.f;
    float horizontal = (ball_cx - pad_cx) / max_dist;
    if (horizontal < 0) horizontal = -horizontal;
    if (horizontal < 0.5f) horizontal = 0.5f;
    ball->velocity.x = left_pad ? horizontal : -horizontal;
    ball->velocity.y =
        (ball->position.y + ball->height < pad->position.y + pad->height / 2.f ? -1.f : 1.f)
        + pad->velocity.y * 0.5f;
    ball->position.x = left_pad ? pad->position.x + pad->width : pad->position.x - ball->width;
    ball->has_collided = true;
    return true;
}

void handle_pad_key_press(pad_t *pad, Uint8 const *state, int player_num) {
    if (player_num == 1) {
        if (state[SDL_SCANCODE_S]) {
            pad->goal_velocity.y = 1.f;
        }
        if (state[SDL_SCANCODE_W]) {
            pad->goal_velocity.y = -1.f;
        }
    } else {
        if (state[SDL_SCANCODE_DOWN]) {
            pad->goal_velocity.y = 1.f;
        }
        if (state[SDL_SCANCODE_UP]) {
            pad->goal_velocity.y = -1.f;
        }
    }
}

void handle_pad_key_release(pad_t *pad, Uint8 const *state, int player_num) {
    if (player_num == 1) {
        if (!state[SDL_SCANCODE_S] && !state[SDL_SCANCODE_W]) {
            pad->goal_velocity.y = 0;
        }
    } else {
        if (!state[SDL_SCANCODE_UP] && !state[SDL_SCANCODE_DOWN]) {
            pad->goal_velocity.y = 0;
        }
    }
}

void render_entity(entity_t *entity, SDL_Renderer *renderer) {
    SDL_Rect dest;
    dest.x = entity->position.x;
    dest.y = entity->position.y;
    dest.w = entity->width;
    dest.h = entity->height;
    SDL_SetRenderDrawColor(renderer, 235, 219, 178, 0);
    SDL_RenderFillRect(renderer, &dest);
}
