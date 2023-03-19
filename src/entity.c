#include "entity.h"

void update_ball(ball_t *ball, float delta_time) {
    float const speed = 40.f;

    if (ball->position.x < 0.f) {
        ball->velocity.x = 1.f;
    }
    if (ball->position.x + ball->width > GAME_WIDTH) {
        ball->velocity.x = -1.f;
    }
    if (ball->position.y < 0.f) {
        ball->velocity.y = 1.f;
    }
    if (ball->position.y + ball->height > GAME_HEIGHT) {
        ball->velocity.y = -1.f;
    }

    vector_2d_t velocity = ball->velocity;
    multiply_vector_by_scalar(&velocity, delta_time * speed); // this is so weird!!
    add_vectors(&ball->position, ball->position, velocity);
}

void update_player_one_pad(pad_t *pad, float delta_time) {
    float const speed = 32.f;
    vector_2d_t velocity = pad->velocity;
    multiply_vector_by_scalar(&velocity, delta_time * speed);
    add_vectors(&pad->position, pad->position, velocity);

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
    float const ball_x = ball->position.x;
    float const ball_xw = ball->position.x + ball->width;
    float const pad_x = pad->position.x;
    float const pad_xw = pad->position.x + pad->width;
    float const ball_y = ball->position.y;
    float const ball_yh = ball->position.y + ball->height;
    float const pad_y = pad->position.y;
    float const pad_yh = pad->position.y + pad->height;

    if (ball_x < pad_xw && ball_x > pad_x
            && ((ball_y > pad_y && ball_y < pad_yh)
             || (ball_yh > pad_y && ball_yh < pad_yh))) {
        ball->velocity.x = 1.f;
    }
    if (ball_xw > pad_x && ball_xw < pad_xw
            && ((ball_y > pad_y && ball_y < pad_yh)
            || (ball_yh > pad_y && ball_yh < pad_yh))) {
        ball->velocity.x = -1.f;
    }
}

void handle_pad_key_press(pad_t *pad, Uint8 const *state) {
    if (state[SDL_SCANCODE_S] || state[SDL_SCANCODE_W]) {
        if (state[SDL_SCANCODE_S]) {
            pad->velocity.y = 1.f;
        }
        if (state[SDL_SCANCODE_W]) {
            pad->velocity.y = -1.f;
        }
    }
}

void handle_pad_key_release(pad_t *pad, Uint8 const *state) {
    if (!state[SDL_SCANCODE_S] && !state[SDL_SCANCODE_W]) {
        pad->velocity.y = 0;
    }
    if (!state[SDL_SCANCODE_W] && !state[SDL_SCANCODE_S]) {
        pad->velocity.y = 0;
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
