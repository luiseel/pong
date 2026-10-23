#include "power_up.h"
#include "game.h"
#include <stdlib.h>
#include <math.h>

static float random_unit(void) {
    return (float)rand() / (float)RAND_MAX;
}

static void schedule_spawn(power_up_state_t *power) {
    power->active = false;
    power->spawn_remaining = POWER_UP_SPAWN_MIN +
        random_unit() * (POWER_UP_SPAWN_MAX - POWER_UP_SPAWN_MIN);
}

void reset_power_ups(game_t *game) {
    game->power_ups = (power_up_state_t){0};
    schedule_spawn(&game->power_ups);
}

static void spawn_power_up(game_t *game) {
    power_up_state_t *power = &game->power_ups;
    int difference = game->player_two_score - game->player_one_score;
    int gap = difference < 0 ? -difference : difference;
    int bonus = gap > 3 ? 3 * POWER_UP_WEIGHT_STEP : gap * POWER_UP_WEIGHT_STEP;
    int losing_chance = 50 + bonus;
    if (losing_chance > POWER_UP_WEIGHT_CAP) losing_chance = POWER_UP_WEIGHT_CAP;
    int left_chance = difference > 0 ? losing_chance :
        difference < 0 ? 100 - losing_chance : 50;
    float direction = random_unit() < left_chance / 100.f ? -1.f : 1.f;
    float angle = (random_unit() * 2.f - 1.f) * 0.65f;
    power->position = (vector_2d_t){
        (GAME_WIDTH - POWER_UP_SIZE) / 2.f,
        (GAME_HEIGHT - POWER_UP_SIZE) / 2.f
    };
    power->velocity = (vector_2d_t){
        direction * cosf(angle) * POWER_UP_SPEED,
        sinf(angle) * POWER_UP_SPEED
    };
    power->animation_time = 0;
    power->active = true;
}

static bool touches_pad(power_up_state_t const *power, pad_t const *pad) {
    return power->position.x < pad->position.x + pad->width &&
        power->position.x + POWER_UP_SIZE > pad->position.x &&
        power->position.y < pad->position.y + pad->height &&
        power->position.y + POWER_UP_SIZE > pad->position.y;
}

void update_power_ups(game_t *game, float delta_time) {
    power_up_state_t *power = &game->power_ups;
    power->player_one_remaining = fmaxf(0, power->player_one_remaining - delta_time);
    power->player_two_remaining = fmaxf(0, power->player_two_remaining - delta_time);
    if (!power->active) {
        power->spawn_remaining -= delta_time;
        if (power->spawn_remaining <= 0) spawn_power_up(game);
        return;
    }

    power->animation_time += delta_time;
    power->position.x += power->velocity.x * delta_time;
    power->position.y += power->velocity.y * delta_time;
    if (power->position.y < 0) {
        power->position.y = -power->position.y;
        power->velocity.y = -power->velocity.y;
    } else if (power->position.y + POWER_UP_SIZE > GAME_HEIGHT) {
        power->position.y = 2 * (GAME_HEIGHT - POWER_UP_SIZE) - power->position.y;
        power->velocity.y = -power->velocity.y;
    }

    if (touches_pad(power, &game->player_one_pad)) {
        power->player_one_remaining = POWER_UP_DURATION;
        schedule_spawn(power);
    } else if (touches_pad(power, &game->player_two_pad)) {
        power->player_two_remaining = POWER_UP_DURATION;
        schedule_spawn(power);
    } else if (power->position.x + POWER_UP_SIZE < 0 || power->position.x > GAME_WIDTH) {
        schedule_spawn(power);
    }
}

void record_power_trail(game_t *game) {
    power_up_state_t *power = &game->power_ups;
    if (!power->ball_powered) {
        power->trail_count = 0;
        return;
    }
    int count = power->trail_count;
    if (count >= POWER_UP_TRAIL_LENGTH) count = POWER_UP_TRAIL_LENGTH - 1;
    for (int i = count; i > 0; i--) power->trail[i] = power->trail[i - 1];
    power->trail[0] = game->ball.position;
    power->trail_count = count + 1;
}

static void glow(SDL_Renderer *renderer, SDL_Rect rect, float pulse) {
    for (int margin = 8; margin >= 2; margin -= 2) {
        SDL_Rect halo = {rect.x - margin, rect.y - margin,
            rect.w + 2 * margin, rect.h + 2 * margin};
        SDL_SetRenderDrawColor(renderer, 255, 190, 65, (Uint8)(pulse * 18));
        SDL_RenderFillRect(renderer, &halo);
    }
    SDL_SetRenderDrawColor(renderer, 255, 220, 110, 255);
    SDL_RenderFillRect(renderer, &rect);
}

static void render_pad_power(SDL_Renderer *renderer, pad_t const *pad, float remaining) {
    if (remaining <= 0) return;
    SDL_Rect rect = {(int)pad->position.x, (int)pad->position.y, pad->width, pad->height};
    glow(renderer, rect, 1);
    SDL_Rect bar = {pad->position.x < GAME_WIDTH / 2 ? rect.x + rect.w + 5 : rect.x - 8,
        rect.y, 3, (int)(pad->height * remaining / POWER_UP_DURATION)};
    SDL_RenderFillRect(renderer, &bar);
}

void render_power_ups(game_t *game) {
    power_up_state_t const *power = &game->power_ups;
    SDL_BlendMode previous;
    SDL_GetRenderDrawBlendMode(game->renderer, &previous);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    if (power->active) {
        SDL_Rect rect = {(int)power->position.x, (int)power->position.y, POWER_UP_SIZE, POWER_UP_SIZE};
        glow(game->renderer, rect, 0.7f + 0.3f * sinf(power->animation_time * 8.f));
    }
    render_pad_power(game->renderer, &game->player_one_pad, power->player_one_remaining);
    render_pad_power(game->renderer, &game->player_two_pad, power->player_two_remaining);
    if (power->ball_powered) {
        for (int i = power->trail_count - 1; i >= 0; i--) {
            SDL_Rect rect = {(int)power->trail[i].x, (int)power->trail[i].y,
                game->ball.width, game->ball.height};
            SDL_SetRenderDrawColor(game->renderer, 255, 190, 65,
                (Uint8)(140 * (POWER_UP_TRAIL_LENGTH - i) / POWER_UP_TRAIL_LENGTH));
            SDL_RenderFillRect(game->renderer, &rect);
        }
        SDL_Rect ball = {(int)game->ball.position.x, (int)game->ball.position.y,
            game->ball.width, game->ball.height};
        glow(game->renderer, ball, 1);
    }
    SDL_SetRenderDrawBlendMode(game->renderer, previous);
}
