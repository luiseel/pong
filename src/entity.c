#include "entity.h"
#include <stdlib.h>

void update_ball(ball_t *ball, float delta_time) {
    float const speed = 250.f;

    vector_2d_t velocity = ball->velocity;
    multiply_vector_by_scalar(&velocity, delta_time * speed);
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

void update_player_two_pad(pad_t *pad, ball_t *ball, float delta_time) {
    pad->position.y = ball->position.y - (pad->height / 2.f - ball->height / 2.f);
    if (pad->position.y < 0.f) {
        pad->position.y = 0.f;
    }
    if (pad->position.y + pad->height > GAME_HEIGHT) {
        pad->position.y = GAME_HEIGHT - pad->height;
    }
}

void check_collisions(ball_t *ball, pad_t *pad) {
    const float ball_x = ball->position.x;
    const float ball_xw = ball->position.x + ball->width;
    const float pad_x = pad->position.x;
    const float pad_xw = pad->position.x + pad->width;
    const float ball_y = ball->position.y;
    const float ball_yh = ball->position.y + ball->height;
    const float pad_y = pad->position.y;
    const float pad_yh = pad->position.y + pad->height;

    const bool collide_x = (ball_x < pad_xw && ball_x > pad_x)
                            || (ball_xw > pad_x && ball_xw < pad_xw);
    const bool collide_y = (ball_y < pad_yh && ball_yh > pad_y)
                            || (ball_yh > pad_y && ball_y < pad_yh);

    if (collide_x && collide_y) {
        const float ball_cx = ball->position.x + ball->width / 2.f;
        const float pad_cx = pad->position.x + pad->width / 2.f;
        const float rel_cx = ball_cx - pad_cx;
        const float max_dist = (ball->width + pad->width) / 2.f;
        const float norm_dist = rel_cx / max_dist;

        // Add paddle velocity influence to ball's Y velocity
        float paddle_influence = pad->velocity.y * 0.5f;
        float base_y_velocity = ball_yh < pad_y + pad->height / 2.f ? -1.f : 1.f;

        ball->velocity.x = norm_dist;
        ball->velocity.y = base_y_velocity + paddle_influence;
        ball->has_collided = true;
    }
}

void handle_pad_key_press(pad_t *pad, Uint8 const *state) {
    if (state[SDL_SCANCODE_S]) {
        pad->goal_velocity.y = 1.f;
    }
    if (state[SDL_SCANCODE_W]) {
        pad->goal_velocity.y = -1.f;
    }
}

void handle_pad_key_release(pad_t *pad, Uint8 const *state) {
    if (!state[SDL_SCANCODE_S] && !state[SDL_SCANCODE_W]) {
        pad->goal_velocity.y = 0;
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
