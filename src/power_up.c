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

static void resize_pad(pad_t *pad, int height) {
    float shift = (pad->height - height) / 2.f;
    float y = fmaxf(0, fminf(GAME_HEIGHT - height, pad->position.y + shift));
    pad->previous_position.y += y - pad->position.y;
    pad->position.y = y;
    pad->height = height;
}

void reset_power_ups(game_t *game) {
    resize_pad(&game->player_one_pad, PADDLE_HEIGHT);
    resize_pad(&game->player_two_pad, PADDLE_HEIGHT);
    game->power_ups = (power_up_state_t){0};
    game->power_ups.next_grow_left = random_unit() < 0.5f;
    game->power_ups.next_serpent_left = random_unit() < 0.5f;
    game->power_ups.next_charge_left = random_unit() < 0.5f;
    schedule_spawn(&game->power_ups);
}

static void launch_power_up(power_up_state_t *power, power_up_kind_t kind, float direction) {
    float angle = (random_unit() * 2.f - 1.f) * 0.65f;
    power->kind = kind;
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
    float roll = random_unit();
    power_up_kind_t kind = roll < POWER_UP_GROW_CHANCE ? POWER_UP_GROW :
        roll < POWER_UP_GROW_CHANCE + POWER_UP_SERPENT_CHANCE ? POWER_UP_SERPENT :
        roll < POWER_UP_GROW_CHANCE + POWER_UP_SERPENT_CHANCE + POWER_UP_CHARGE_CHANCE ?
        POWER_UP_CHARGE : POWER_UP_SPEED_BOOST;
    if (kind == POWER_UP_GROW) {
        direction = power->next_grow_left ? -1.f : 1.f;
        power->next_grow_left = !power->next_grow_left;
    } else if (kind == POWER_UP_SERPENT) {
        direction = power->next_serpent_left ? -1.f : 1.f;
        power->next_serpent_left = !power->next_serpent_left;
    } else if (kind == POWER_UP_CHARGE) {
        direction = power->next_charge_left ? -1.f : 1.f;
        power->next_charge_left = !power->next_charge_left;
    }
    launch_power_up(power, kind, direction);
}

// Replaces any pickup in the field; leaves the spawn rotation alone
void debug_spawn_power_up(game_t *game, power_up_kind_t kind, int player_num) {
    launch_power_up(&game->power_ups, kind, player_num == 1 ? -1.f : 1.f);
}

static bool touches_pad(power_up_state_t const *power, pad_t const *pad) {
    return power->position.x < pad->position.x + pad->width &&
        power->position.x + POWER_UP_SIZE > pad->position.x &&
        power->position.y < pad->position.y + pad->height &&
        power->position.y + POWER_UP_SIZE > pad->position.y;
}

static void collect_power(power_up_state_t *power, power_up_inventory_t *inventory) {
    if (inventory->count < POWER_UP_SLOTS) inventory->slots[inventory->count++] = power->kind;
    schedule_spawn(power);
}

static power_up_inventory_t *inventory_for(game_t *game, int player_num) {
    return player_num == 1 ? &game->power_ups.player_one_inventory :
        &game->power_ups.player_two_inventory;
}

void debug_give_power_up(game_t *game, power_up_kind_t kind, int player_num) {
    power_up_inventory_t *inventory = inventory_for(game, player_num);
    if (inventory->count < POWER_UP_SLOTS) inventory->selected = inventory->count++;
    inventory->slots[inventory->selected] = kind;
}

void select_power_up(game_t *game, int player_num) {
    power_up_inventory_t *inventory = inventory_for(game, player_num);
    if (inventory->count > 0) inventory->selected = (inventory->selected + 1) % inventory->count;
}

void use_power_up(game_t *game, int player_num) {
    power_up_state_t *power = &game->power_ups;
    power_up_inventory_t *inventory = inventory_for(game, player_num);
    if (inventory->count == 0) return;
    switch (inventory->slots[inventory->selected]) {
        case POWER_UP_GROW:
            *(player_num == 1 ? &power->player_one_grow_remaining : &power->player_two_grow_remaining) =
                POWER_UP_DURATION;
            resize_pad(player_num == 1 ? &game->player_one_pad : &game->player_two_pad,
                POWER_UP_GROW_HEIGHT);
            break;
        case POWER_UP_SERPENT:
            *(player_num == 1 ? &power->player_one_serpent_remaining :
                &power->player_two_serpent_remaining) = POWER_UP_DURATION;
            break;
        case POWER_UP_CHARGE:
            *(player_num == 1 ? &power->player_one_charge_remaining :
                &power->player_two_charge_remaining) = POWER_UP_DURATION;
            break;
        case POWER_UP_SPEED_BOOST:
            *(player_num == 1 ? &power->player_one_remaining : &power->player_two_remaining) =
                POWER_UP_DURATION;
            break;
    }
    inventory->count--;
    for (int i = inventory->selected; i < inventory->count; i++) {
        inventory->slots[i] = inventory->slots[i + 1];
    }
    if (inventory->selected >= inventory->count) inventory->selected = 0;
}

void update_power_ups(game_t *game, float delta_time) {
    power_up_state_t *power = &game->power_ups;
    power->player_one_remaining = fmaxf(0, power->player_one_remaining - delta_time);
    power->player_two_remaining = fmaxf(0, power->player_two_remaining - delta_time);
    power->player_one_grow_remaining = fmaxf(0, power->player_one_grow_remaining - delta_time);
    power->player_two_grow_remaining = fmaxf(0, power->player_two_grow_remaining - delta_time);
    power->player_one_serpent_remaining = fmaxf(0, power->player_one_serpent_remaining - delta_time);
    power->player_two_serpent_remaining = fmaxf(0, power->player_two_serpent_remaining - delta_time);
    power->player_one_charge_remaining = fmaxf(0, power->player_one_charge_remaining - delta_time);
    power->player_two_charge_remaining = fmaxf(0, power->player_two_charge_remaining - delta_time);
    resize_pad(&game->player_one_pad, power->player_one_grow_remaining > 0 ? POWER_UP_GROW_HEIGHT : PADDLE_HEIGHT);
    resize_pad(&game->player_two_pad, power->player_two_grow_remaining > 0 ? POWER_UP_GROW_HEIGHT : PADDLE_HEIGHT);
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
        collect_power(power, &power->player_one_inventory);
    } else if (touches_pad(power, &game->player_two_pad)) {
        collect_power(power, &power->player_two_inventory);
    } else if (power->position.x + POWER_UP_SIZE < 0 || power->position.x > GAME_WIDTH) {
        schedule_spawn(power);
    }
}

// Start or stop the weave for a new hit
void set_ball_serpent(game_t *game, bool serpent) {
    game->power_ups.ball_serpent = serpent;
    game->power_ups.serpent_distance = 0;
    game->power_ups.serpent_side = 1;
}

// Shift the ball sideways along a sine wave of its x travel since the last hit.
// The phase follows distance, so the wave keeps its shape at any ball speed.
// At a wall the whole path, straight part and wave, is mirrored.
void weave_ball(game_t *game) {
    power_up_state_t *power = &game->power_ups;
    ball_t *ball = &game->ball;
    if (!power->ball_serpent) return;
    float const k = 2.f * (float)M_PI / POWER_UP_SERPENT_WAVELENGTH;
    float before = sinf(k * power->serpent_distance);
    power->serpent_distance += fabsf(ball->position.x - ball->previous_position.x);
    ball->position.y += power->serpent_side * POWER_UP_SERPENT_AMPLITUDE *
        (sinf(k * power->serpent_distance) - before);
    float bottom = GAME_HEIGHT - ball->height;
    if (ball->position.y < 0 || ball->position.y > bottom) {
        ball->position.y = ball->position.y < 0 ? -ball->position.y :
            2 * bottom - ball->position.y;
        ball->velocity.y = -ball->velocity.y;
        power->serpent_side = -power->serpent_side;
    }
}

// An armed pad uses its charge on this hit and holds the ball
bool catch_charged_ball(game_t *game, int player_num) {
    power_up_state_t *power = &game->power_ups;
    float *armed = player_num == 1 ? &power->player_one_charge_remaining :
        &power->player_two_charge_remaining;
    if (*armed <= 0) return false;
    *armed = 0;
    power->ball_held_by = player_num;
    power->hold_remaining = POWER_UP_CHARGE_HOLD;
    game->ball.velocity = (vector_2d_t){player_num == 1 ? 1.f : -1.f, 0};
    update_held_ball(game, 0);
    return true;
}

// Keep the held ball on the pad center, then shoot it straight
void update_held_ball(game_t *game, float delta_time) {
    power_up_state_t *power = &game->power_ups;
    ball_t *ball = &game->ball;
    pad_t const *pad = power->ball_held_by == 1 ? &game->player_one_pad : &game->player_two_pad;
    ball->position.x = power->ball_held_by == 1 ? pad->position.x + pad->width :
        pad->position.x - ball->width;
    ball->position.y = pad->position.y + (pad->height - ball->height) / 2.f;
    ball->previous_position = ball->position;
    power->hold_remaining -= delta_time;
    if (power->hold_remaining > 0) return;
    // A serpent used during the hold still makes the shot weave
    set_ball_serpent(game, (power->ball_held_by == 1 ? power->player_one_serpent_remaining :
        power->player_two_serpent_remaining) > 0);
    power->ball_held_by = 0;
    power->hold_remaining = 0;
    power->ball_charged = true;
    ball->has_collided = true;
}

void record_power_trail(game_t *game) {
    power_up_state_t *power = &game->power_ups;
    if (!power->ball_powered && !power->ball_serpent && !power->ball_charged) {
        power->trail_count = 0;
        return;
    }
    int count = power->trail_count;
    if (count >= POWER_UP_TRAIL_LENGTH) count = POWER_UP_TRAIL_LENGTH - 1;
    for (int i = count; i > 0; i--) power->trail[i] = power->trail[i - 1];
    power->trail[0] = game->ball.position;
    power->trail_count = count + 1;
}

// Indexed by power_up_kind_t
static SDL_Color const halo_colors[] = {{255, 190, 65, 255}, {65, 190, 255, 255}, {90, 230, 110, 255},
    {255, 70, 170, 255}};
static SDL_Color const fill_colors[] = {{255, 220, 110, 255}, {110, 220, 255, 255}, {150, 255, 160, 255},
    {255, 140, 205, 255}};

static void glow(SDL_Renderer *renderer, SDL_Rect rect, float pulse, power_up_kind_t kind) {
    SDL_Color halo_color = halo_colors[kind];
    SDL_Color fill_color = fill_colors[kind];
    for (int margin = 8; margin >= 2; margin -= 2) {
        SDL_Rect halo = {rect.x - margin, rect.y - margin,
            rect.w + 2 * margin, rect.h + 2 * margin};
        SDL_SetRenderDrawColor(renderer, halo_color.r, halo_color.g, halo_color.b, (Uint8)(pulse * 18));
        SDL_RenderFillRect(renderer, &halo);
    }
    SDL_SetRenderDrawColor(renderer, fill_color.r, fill_color.g, fill_color.b, 255);
    SDL_RenderFillRect(renderer, &rect);
}

// Timer bar at the back of the pad; slot 0 is nearest the pad
static void render_bar(SDL_Renderer *renderer, pad_t const *pad, int slot, float remaining) {
    SDL_Rect bar = {pad->position.x < GAME_WIDTH / 2 ? (int)pad->position.x - 6 - 5 * slot :
        (int)pad->position.x + pad->width + 3 + 5 * slot,
        (int)pad->position.y, 3, (int)(pad->height * remaining / POWER_UP_DURATION)};
    SDL_RenderFillRect(renderer, &bar);
}

static void render_pad_power(SDL_Renderer *renderer, pad_t const *pad, float remaining, int slot) {
    if (remaining <= 0) return;
    SDL_Rect rect = {(int)pad->position.x, (int)pad->position.y, pad->width, pad->height};
    glow(renderer, rect, 1, POWER_UP_SPEED_BOOST);
    render_bar(renderer, pad, slot, remaining);
}

static void render_growth(SDL_Renderer *renderer, pad_t const *pad, float remaining) {
    if (remaining <= 0) return;
    SDL_SetRenderDrawColor(renderer, 110, 220, 255, 255);
    SDL_Rect outline = {(int)pad->position.x - 1, (int)pad->position.y - 1,
        pad->width + 2, pad->height + 2};
    SDL_RenderDrawRect(renderer, &outline);
    render_bar(renderer, pad, 0, remaining);
}

static void render_bar_color(SDL_Renderer *renderer, pad_t const *pad, float remaining, int slot,
                             power_up_kind_t kind) {
    if (remaining <= 0) return;
    SDL_Color color = fill_colors[kind];
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    render_bar(renderer, pad, slot, remaining);
}

// Active timers stack behind the pad: grow, speed, serpent, then the charge window
static void render_pad_timers(SDL_Renderer *renderer, pad_t const *pad, float speed, float grow,
                              float serpent, float charge) {
    int speed_slot = grow > 0;
    int serpent_slot = speed_slot + (speed > 0);
    int charge_slot = serpent_slot + (serpent > 0);
    render_pad_power(renderer, pad, speed, speed_slot);
    render_growth(renderer, pad, grow);
    render_bar_color(renderer, pad, serpent, serpent_slot, POWER_UP_SERPENT);
    render_bar_color(renderer, pad, charge, charge_slot, POWER_UP_CHARGE);
}

static void render_inventory(SDL_Renderer *renderer, power_up_inventory_t const *inventory, int center_x) {
    int const spacing = POWER_UP_SIZE + 8;
    int left = center_x - (POWER_UP_SLOTS * spacing - 8) / 2;
    for (int i = 0; i < POWER_UP_SLOTS; i++) {
        SDL_Rect rect = {left + i * spacing, POWER_UP_SLOTS_Y, POWER_UP_SIZE, POWER_UP_SIZE};
        if (i >= inventory->count) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
            SDL_RenderDrawRect(renderer, &rect);
            continue;
        }
        glow(renderer, rect, i == inventory->selected ? 1 : 0.3f, inventory->slots[i]);
        if (i == inventory->selected) {
            SDL_Rect outline = {rect.x - 3, rect.y - 3, rect.w + 6, rect.h + 6};
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &outline);
        }
    }
}

void render_power_ups(game_t *game) {
    power_up_state_t const *power = &game->power_ups;
    SDL_BlendMode previous;
    SDL_GetRenderDrawBlendMode(game->renderer, &previous);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    if (power->active) {
        SDL_Rect rect = {(int)power->position.x, (int)power->position.y, POWER_UP_SIZE, POWER_UP_SIZE};
        glow(game->renderer, rect, 0.7f + 0.3f * sinf(power->animation_time * 8.f), power->kind);
        if (power->kind == POWER_UP_GROW) {
            SDL_Rect icon = {rect.x + 4, rect.y - 4, 4, rect.h + 8};
            SDL_RenderFillRect(game->renderer, &icon);
        } else if (power->kind == POWER_UP_SERPENT) {
            SDL_Point zigzag[] = {{rect.x + 1, rect.y + 6}, {rect.x + 4, rect.y + 2},
                {rect.x + 7, rect.y + 9}, {rect.x + 10, rect.y + 5}};
            SDL_SetRenderDrawColor(game->renderer, 20, 70, 30, 255);
            SDL_RenderDrawLines(game->renderer, zigzag, 4);
        } else if (power->kind == POWER_UP_CHARGE) {
            SDL_Rect core = {rect.x + 4, rect.y + 4, 4, 4};
            SDL_SetRenderDrawColor(game->renderer, 90, 10, 50, 255);
            SDL_RenderFillRect(game->renderer, &core);
        }
    }
    render_inventory(game->renderer, &power->player_one_inventory, GAME_WIDTH / 4);
    render_inventory(game->renderer, &power->player_two_inventory, GAME_WIDTH - GAME_WIDTH / 4);
    render_pad_timers(game->renderer, &game->player_one_pad, power->player_one_remaining,
        power->player_one_grow_remaining, power->player_one_serpent_remaining,
        power->player_one_charge_remaining);
    render_pad_timers(game->renderer, &game->player_two_pad, power->player_two_remaining,
        power->player_two_grow_remaining, power->player_two_serpent_remaining,
        power->player_two_charge_remaining);
    if (power->ball_held_by) {
        // The pulse gets faster as the release comes closer
        float elapsed = POWER_UP_CHARGE_HOLD - power->hold_remaining;
        float pulse = 0.6f + 0.4f * sinf(elapsed * (6.f + 6.f * elapsed));
        SDL_Rect ball = {(int)game->ball.position.x, (int)game->ball.position.y,
            game->ball.width, game->ball.height};
        glow(game->renderer, ball, pulse, POWER_UP_CHARGE);
    } else if (power->ball_powered || power->ball_serpent || power->ball_charged) {
        // A charged shot leaves a pink trail, a weaving ball a green one; speed keeps the gold glow
        power_up_kind_t kind = power->ball_charged ? POWER_UP_CHARGE :
            power->ball_serpent ? POWER_UP_SERPENT : POWER_UP_SPEED_BOOST;
        SDL_Color trail = halo_colors[kind];
        for (int i = power->trail_count - 1; i >= 0; i--) {
            SDL_Rect rect = {(int)power->trail[i].x, (int)power->trail[i].y,
                game->ball.width, game->ball.height};
            SDL_SetRenderDrawColor(game->renderer, trail.r, trail.g, trail.b,
                (Uint8)(140 * (POWER_UP_TRAIL_LENGTH - i) / POWER_UP_TRAIL_LENGTH));
            SDL_RenderFillRect(game->renderer, &rect);
        }
        SDL_Rect ball = {(int)game->ball.position.x, (int)game->ball.position.y,
            game->ball.width, game->ball.height};
        glow(game->renderer, ball, 1, power->ball_charged ? POWER_UP_CHARGE :
            power->ball_powered ? POWER_UP_SPEED_BOOST : POWER_UP_SERPENT);
    }
    SDL_SetRenderDrawBlendMode(game->renderer, previous);
}
