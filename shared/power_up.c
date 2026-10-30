#include "power_up.h"
#include "sim.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void schedule_spawn(sim_state_t *state) {
    state->power_ups.spawn_remaining = POWER_UP_SPAWN_MIN +
        sim_random_unit(state) * (POWER_UP_SPAWN_MAX - POWER_UP_SPAWN_MIN);
}

static void resize_pad(pad_t *pad, int height) {
    float shift = (pad->height - height) / 2.f;
    float y = fmaxf(0, fminf(GAME_HEIGHT - height, pad->position.y + shift));
    pad->previous_position.y += y - pad->position.y;
    pad->position.y = y;
    pad->height = height;
}

void clear_power_up_pickups(sim_state_t *state) {
    memset(state->power_ups.pickups, 0, sizeof(state->power_ups.pickups));
}

// memset and not a struct literal, so the padding is the same in every copy
void reset_power_ups(sim_state_t *state) {
    power_up_state_t *power = &state->power_ups;
    resize_pad(&state->player_one_pad, PADDLE_HEIGHT);
    resize_pad(&state->player_two_pad, PADDLE_HEIGHT);
    memset(power, 0, sizeof(*power));
    power->next_grow_left = sim_random_unit(state) < 0.5f;
    power->next_serpent_left = sim_random_unit(state) < 0.5f;
    power->next_charge_left = sim_random_unit(state) < 0.5f;
    schedule_spawn(state);
}

// The first free slot, or NULL when all are in use
static power_up_pickup_t *free_pickup(power_up_state_t *power) {
    for (int i = 0; i < POWER_UP_MAX_PICKUPS; i++) {
        if (!power->pickups[i].active) return &power->pickups[i];
    }
    return NULL;
}

static bool has_active_pickup(power_up_state_t const *power) {
    for (int i = 0; i < POWER_UP_MAX_PICKUPS; i++) {
        if (power->pickups[i].active) return true;
    }
    return false;
}

static void launch_power_up(sim_state_t *state, power_up_pickup_t *power, power_up_kind_t kind,
                            float direction) {
    float angle = (sim_random_unit(state) * 2.f - 1.f) * 0.65f;
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

static void spawn_power_up(sim_state_t *state) {
    power_up_state_t *power = &state->power_ups;
    int difference = state->player_two_score - state->player_one_score;
    int gap = difference < 0 ? -difference : difference;
    int bonus = gap > 3 ? 3 * POWER_UP_WEIGHT_STEP : gap * POWER_UP_WEIGHT_STEP;
    int losing_chance = 50 + bonus;
    if (losing_chance > POWER_UP_WEIGHT_CAP) losing_chance = POWER_UP_WEIGHT_CAP;
    int left_chance = difference > 0 ? losing_chance :
        difference < 0 ? 100 - losing_chance : 50;
    float direction = sim_random_unit(state) < left_chance / 100.f ? -1.f : 1.f;
    float roll = sim_random_unit(state);
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
    launch_power_up(state, free_pickup(power), kind, direction);
}

void debug_spawn_power_up(sim_state_t *state, power_up_kind_t kind, int player_num) {
    power_up_pickup_t *pickup = free_pickup(&state->power_ups);
    if (pickup) launch_power_up(state, pickup, kind, player_num == 1 ? -1.f : 1.f);
}

static bool touches_pad(power_up_pickup_t const *power, pad_t const *pad) {
    return power->position.x < pad->position.x + pad->width &&
        power->position.x + POWER_UP_SIZE > pad->position.x &&
        power->position.y < pad->position.y + pad->height &&
        power->position.y + POWER_UP_SIZE > pad->position.y;
}

static void collect_power(power_up_pickup_t *power, power_up_inventory_t *inventory) {
    if (inventory->count < POWER_UP_SLOTS) inventory->slots[inventory->count++] = power->kind;
    power->active = false;
}

static power_up_inventory_t *inventory_for(sim_state_t *state, int player_num) {
    return player_num == 1 ? &state->power_ups.player_one_inventory :
        &state->power_ups.player_two_inventory;
}

void debug_give_power_up(sim_state_t *state, power_up_kind_t kind, int player_num) {
    power_up_inventory_t *inventory = inventory_for(state, player_num);
    if (inventory->count < POWER_UP_SLOTS) inventory->selected = inventory->count++;
    inventory->slots[inventory->selected] = kind;
}

void select_power_up(sim_state_t *state, int player_num) {
    power_up_inventory_t *inventory = inventory_for(state, player_num);
    if (inventory->count > 0) inventory->selected = (inventory->selected + 1) % inventory->count;
}

void use_power_up(sim_state_t *state, int player_num) {
    power_up_state_t *power = &state->power_ups;
    power_up_inventory_t *inventory = inventory_for(state, player_num);
    if (inventory->count == 0) return;
    switch (inventory->slots[inventory->selected]) {
        case POWER_UP_GROW:
            *(player_num == 1 ? &power->player_one_grow_remaining : &power->player_two_grow_remaining) =
                POWER_UP_DURATION;
            resize_pad(player_num == 1 ? &state->player_one_pad : &state->player_two_pad,
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

static void update_pickup(sim_state_t *state, power_up_pickup_t *power, float delta_time) {
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

    if (touches_pad(power, &state->player_one_pad)) {
        collect_power(power, &state->power_ups.player_one_inventory);
        state->events |= SIM_EVENT_PICKUP;
    } else if (touches_pad(power, &state->player_two_pad)) {
        collect_power(power, &state->power_ups.player_two_inventory);
        state->events |= SIM_EVENT_PICKUP;
    } else if (power->position.x + POWER_UP_SIZE < 0 || power->position.x > GAME_WIDTH) {
        power->active = false;
    }
}

void update_power_ups(sim_state_t *state, float delta_time) {
    power_up_state_t *power = &state->power_ups;
    power->player_one_remaining = fmaxf(0, power->player_one_remaining - delta_time);
    power->player_two_remaining = fmaxf(0, power->player_two_remaining - delta_time);
    power->player_one_grow_remaining = fmaxf(0, power->player_one_grow_remaining - delta_time);
    power->player_two_grow_remaining = fmaxf(0, power->player_two_grow_remaining - delta_time);
    power->player_one_serpent_remaining = fmaxf(0, power->player_one_serpent_remaining - delta_time);
    power->player_two_serpent_remaining = fmaxf(0, power->player_two_serpent_remaining - delta_time);
    power->player_one_charge_remaining = fmaxf(0, power->player_one_charge_remaining - delta_time);
    power->player_two_charge_remaining = fmaxf(0, power->player_two_charge_remaining - delta_time);
    resize_pad(&state->player_one_pad, power->player_one_grow_remaining > 0 ? POWER_UP_GROW_HEIGHT : PADDLE_HEIGHT);
    resize_pad(&state->player_two_pad, power->player_two_grow_remaining > 0 ? POWER_UP_GROW_HEIGHT : PADDLE_HEIGHT);
    if (!has_active_pickup(power)) {
        power->spawn_remaining -= delta_time;
        if (power->spawn_remaining <= 0) spawn_power_up(state);
        return;
    }

    for (int i = 0; i < POWER_UP_MAX_PICKUPS; i++) {
        if (power->pickups[i].active) update_pickup(state, &power->pickups[i], delta_time);
    }
    if (!has_active_pickup(power)) schedule_spawn(state);
}

// Start or stop the weave for a new hit
void set_ball_serpent(sim_state_t *state, bool serpent) {
    state->power_ups.ball_serpent = serpent;
    state->power_ups.serpent_distance = 0;
    state->power_ups.serpent_side = 1;
}

// Shift the ball sideways along a sine wave of its x travel since the last hit.
// The phase follows distance, so the wave keeps its shape at any ball speed.
// At a wall the whole path, straight part and wave, is mirrored.
void weave_ball(sim_state_t *state) {
    power_up_state_t *power = &state->power_ups;
    ball_t *ball = &state->ball;
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
        state->events |= SIM_EVENT_WALL_HIT;
    }
}

// An armed pad uses its charge on this hit and holds the ball
bool catch_charged_ball(sim_state_t *state, int player_num) {
    power_up_state_t *power = &state->power_ups;
    float *armed = player_num == 1 ? &power->player_one_charge_remaining :
        &power->player_two_charge_remaining;
    if (*armed <= 0) return false;
    *armed = 0;
    power->ball_held_by = player_num;
    power->hold_remaining = POWER_UP_CHARGE_HOLD;
    state->ball.velocity = (vector_2d_t){player_num == 1 ? 1.f : -1.f, 0};
    update_held_ball(state, 0);
    return true;
}

// Keep the held ball on the pad center, then shoot it where the pad moves
void update_held_ball(sim_state_t *state, float delta_time) {
    power_up_state_t *power = &state->power_ups;
    ball_t *ball = &state->ball;
    pad_t const *pad = power->ball_held_by == 1 ? &state->player_one_pad : &state->player_two_pad;
    ball->position.x = power->ball_held_by == 1 ? pad->position.x + pad->width :
        pad->position.x - ball->width;
    ball->position.y = pad->position.y + (pad->height - ball->height) / 2.f;
    ball->previous_position = ball->position;
    power->hold_remaining -= delta_time;
    if (power->hold_remaining > 0) return;
    // A speed or serpent used during the hold still changes the shot
    power->ball_powered = (power->ball_held_by == 1 ? power->player_one_remaining :
        power->player_two_remaining) > 0;
    set_ball_serpent(state, (power->ball_held_by == 1 ? power->player_one_serpent_remaining :
        power->player_two_serpent_remaining) > 0);
    // A pad pushed against a wall does not move, so it shoots straight
    float pad_velocity = fmaxf(-1.f, fminf(pad->velocity.y, 1.f));
    if ((pad->position.y <= 0 && pad_velocity < 0) ||
        (pad->position.y + pad->height >= GAME_HEIGHT && pad_velocity > 0)) {
        pad_velocity = 0;
    }
    float aim = pad_velocity * POWER_UP_CHARGE_AIM_DEGREES * (float)M_PI / 180.f;
    ball->velocity = (vector_2d_t){power->ball_held_by == 1 ? cosf(aim) : -cosf(aim), sinf(aim)};
    power->ball_held_by = 0;
    power->hold_remaining = 0;
    power->ball_charged = true;
    state->events |= SIM_EVENT_BLAST;
}

void record_power_trail(sim_state_t *state) {
    power_up_state_t *power = &state->power_ups;
    if (!power->ball_powered && !power->ball_serpent && !power->ball_charged) {
        power->trail_count = 0;
        return;
    }
    int count = power->trail_count;
    if (count >= POWER_UP_TRAIL_LENGTH) count = POWER_UP_TRAIL_LENGTH - 1;
    for (int i = count; i > 0; i--) power->trail[i] = power->trail[i - 1];
    power->trail[0] = state->ball.position;
    power->trail_count = count + 1;
}
