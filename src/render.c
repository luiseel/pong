#include "render.h"
#include <math.h>

void render_entity(entity_t const *entity, SDL_Renderer *renderer) {
    SDL_Rect dest;
    dest.x = entity->position.x;
    dest.y = entity->position.y;
    dest.w = entity->width;
    dest.h = entity->height;
    SDL_SetRenderDrawColor(renderer, 235, 219, 178, 0);
    SDL_RenderFillRect(renderer, &dest);
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

void render_power_ups(SDL_Renderer *renderer, sim_state_t const *state) {
    power_up_state_t const *power = &state->power_ups;
    SDL_BlendMode previous;
    SDL_GetRenderDrawBlendMode(renderer, &previous);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < POWER_UP_MAX_PICKUPS; i++) {
        power_up_pickup_t const *pickup = &power->pickups[i];
        if (!pickup->active) continue;
        SDL_Rect rect = {(int)pickup->position.x, (int)pickup->position.y, POWER_UP_SIZE, POWER_UP_SIZE};
        glow(renderer, rect, 0.7f + 0.3f * sinf(pickup->animation_time * 8.f), pickup->kind);
        if (pickup->kind == POWER_UP_GROW) {
            SDL_Rect icon = {rect.x + 4, rect.y - 4, 4, rect.h + 8};
            SDL_RenderFillRect(renderer, &icon);
        } else if (pickup->kind == POWER_UP_SERPENT) {
            SDL_Point zigzag[] = {{rect.x + 1, rect.y + 6}, {rect.x + 4, rect.y + 2},
                {rect.x + 7, rect.y + 9}, {rect.x + 10, rect.y + 5}};
            SDL_SetRenderDrawColor(renderer, 20, 70, 30, 255);
            SDL_RenderDrawLines(renderer, zigzag, 4);
        } else if (pickup->kind == POWER_UP_CHARGE) {
            SDL_Rect core = {rect.x + 4, rect.y + 4, 4, 4};
            SDL_SetRenderDrawColor(renderer, 90, 10, 50, 255);
            SDL_RenderFillRect(renderer, &core);
        }
    }
    render_inventory(renderer, &power->player_one_inventory, GAME_WIDTH / 4);
    render_inventory(renderer, &power->player_two_inventory, GAME_WIDTH - GAME_WIDTH / 4);
    render_pad_timers(renderer, &state->player_one_pad, power->player_one_remaining,
        power->player_one_grow_remaining, power->player_one_serpent_remaining,
        power->player_one_charge_remaining);
    render_pad_timers(renderer, &state->player_two_pad, power->player_two_remaining,
        power->player_two_grow_remaining, power->player_two_serpent_remaining,
        power->player_two_charge_remaining);
    if (power->ball_held_by) {
        // The pulse gets faster as the release comes closer
        float elapsed = POWER_UP_CHARGE_HOLD - power->hold_remaining;
        float pulse = 0.6f + 0.4f * sinf(elapsed * (6.f + 6.f * elapsed));
        SDL_Rect ball = {(int)state->ball.position.x, (int)state->ball.position.y,
            state->ball.width, state->ball.height};
        glow(renderer, ball, pulse, POWER_UP_CHARGE);
    } else if (power->ball_powered || power->ball_serpent || power->ball_charged) {
        // A charged shot leaves a pink trail, a weaving ball a green one; speed keeps the gold glow
        power_up_kind_t kind = power->ball_charged ? POWER_UP_CHARGE :
            power->ball_serpent ? POWER_UP_SERPENT : POWER_UP_SPEED_BOOST;
        SDL_Color trail = halo_colors[kind];
        for (int i = power->trail_count - 1; i >= 0; i--) {
            SDL_Rect rect = {(int)power->trail[i].x, (int)power->trail[i].y,
                state->ball.width, state->ball.height};
            SDL_SetRenderDrawColor(renderer, trail.r, trail.g, trail.b,
                (Uint8)(140 * (POWER_UP_TRAIL_LENGTH - i) / POWER_UP_TRAIL_LENGTH));
            SDL_RenderFillRect(renderer, &rect);
        }
        SDL_Rect ball = {(int)state->ball.position.x, (int)state->ball.position.y,
            state->ball.width, state->ball.height};
        glow(renderer, ball, 1, power->ball_charged ? POWER_UP_CHARGE :
            power->ball_powered ? POWER_UP_SPEED_BOOST : POWER_UP_SERPENT);
    }
    SDL_SetRenderDrawBlendMode(renderer, previous);
}
