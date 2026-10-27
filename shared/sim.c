#include "sim.h"
#include <math.h>
#include <string.h>

// Accumulated float error smaller than this does not cost or give a substep
#define SIM_STEP_SLACK (SIM_STEP * 0.001f)

// PCG32 (pcg-random.org): uniform and the same on every platform
uint32_t sim_random(sim_state_t *state) {
    uint64_t old = state->random;
    state->random = old * 6364136223846793005ULL + 1442695040888963407ULL;
    uint32_t shifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rotation = (uint32_t)(old >> 59u);
    return (shifted >> rotation) | (shifted << ((-rotation) & 31u));
}

// In [0, 1)
float sim_random_unit(sim_state_t *state) {
    return (float)(sim_random(state) >> 8) / 16777216.f;
}

int8_t sim_move(float goal_velocity) {
    goal_velocity = fmaxf(-1.f, fminf(goal_velocity, 1.f));
    return (int8_t)lroundf(goal_velocity * 127.f);
}

static void reset_ball(sim_state_t *state) {
    ball_t *ball = &state->ball;
    ball->position.x = GAME_WIDTH / 2 - ball->width / 2;
    ball->position.y = GAME_HEIGHT / 2 - ball->height / 2;
    ball->velocity.x = (sim_random(state) >> 31) ? 1.f : -1.f;
    ball->velocity.y = (sim_random(state) >> 31) ? 0.5f : -0.5f;
    float length = sqrtf(ball->velocity.x * ball->velocity.x +
                         ball->velocity.y * ball->velocity.y);
    ball->velocity.x /= length;
    ball->velocity.y /= length;
}

static void init_pad(pad_t *pad, float x) {
    pad->width = 10;
    pad->height = PADDLE_HEIGHT;
    pad->position.x = x;
    pad->position.y = GAME_HEIGHT / 2 - 25;
}

void sim_init(sim_state_t *state, uint64_t seed, int target_score) {
    memset(state, 0, sizeof(*state));
    sim_random(state);
    state->random += seed;
    sim_random(state);

    state->ball.width = 12;
    state->ball.height = 12;
    state->ball.position.x = GAME_WIDTH / 2 - 6;
    state->ball.position.y = GAME_HEIGHT / 2 - 6;
    state->ball.velocity.x = 1;
    state->ball.velocity.y = 1;
    init_pad(&state->player_one_pad, 15);
    init_pad(&state->player_two_pad, GAME_WIDTH - 10 - 15);
    state->target_score = target_score;
    reset_power_ups(state);
}

static void steer_pad(pad_t *pad, int8_t move) {
    pad->goal_velocity.y = (move < -127 ? -127 : move) / 127.f;
}

// The substeps of a whole step, for one pad alone. The client predicts its
// own pad with it, so it has to move the pad as sim_step does: the same
// steering, the same substeps. The height is the caller's (a grow is the
// server's call).
void sim_step_pad(pad_t *pad, int8_t move, float delta_time) {
    steer_pad(pad, move);
    while (delta_time >= SIM_STEP - SIM_STEP_SLACK) {
        delta_time -= SIM_STEP;
        update_pad(pad, SIM_STEP);
    }
}

static void apply_input(sim_state_t *state, int player_num, sim_input_t const *input) {
    pad_t *pad = player_num == 1 ? &state->player_one_pad : &state->player_two_pad;
    power_up_inventory_t *inventory = player_num == 1 ? &state->power_ups.player_one_inventory :
        &state->power_ups.player_two_inventory;
    steer_pad(pad, input->move);
    int slot = ((input->actions & SIM_ACTION_PICK_MASK) >> 2) - 1;
    if (slot >= 0 && slot < inventory->count) inventory->selected = slot;
    if (input->actions & SIM_ACTION_SELECT) select_power_up(state, player_num);
    if (input->actions & SIM_ACTION_USE) use_power_up(state, player_num);
}

static void hit_ball(sim_state_t *state, int player_num) {
    power_up_state_t *power = &state->power_ups;
    power->ball_powered = (player_num == 1 ? power->player_one_remaining :
        power->player_two_remaining) > 0;
    set_ball_serpent(state, (player_num == 1 ? power->player_one_serpent_remaining :
        power->player_two_serpent_remaining) > 0);
    power->ball_charged = false;
    catch_charged_ball(state, player_num);
    state->events |= SIM_EVENT_PAD_HIT;
}

// One substep; true when a point ends it
static bool substep(sim_state_t *state) {
    power_up_state_t *power = &state->power_ups;
    update_pad(&state->player_one_pad, SIM_STEP);
    update_pad(&state->player_two_pad, SIM_STEP);
    update_power_ups(state, SIM_STEP);
    if (power->ball_held_by) {
        update_held_ball(state, SIM_STEP);
        return false;
    }
    // A speed boost is faster than a charged shot, so it wins
    update_ball(&state->ball, SIM_STEP,
        power->ball_powered ? POWER_UP_BALL_MULTIPLIER :
        power->ball_charged ? POWER_UP_CHARGE_MULTIPLIER : 1.f);
    weave_ball(state);
    if (check_collisions(&state->ball, &state->player_one_pad)) {
        hit_ball(state, 1);
        if (power->ball_held_by) return false;
    }
    if (check_collisions(&state->ball, &state->player_two_pad)) {
        hit_ball(state, 2);
        if (power->ball_held_by) return false;
    }
    if (bounce_off_walls(&state->ball)) state->events |= SIM_EVENT_WALL_HIT;
    int scorer = check_goal(&state->ball);
    if (!scorer) return false;
    int *score = scorer == 1 ? &state->player_one_score : &state->player_two_score;
    (*score)++;
    reset_ball(state);
    power->ball_powered = false;
    power->ball_charged = false;
    power->ball_held_by = 0;
    set_ball_serpent(state, false);
    power->trail_count = 0;
    if (state->target_score > 0 && *score >= state->target_score) state->winner = scorer;
    state->events |= SIM_EVENT_GOAL;
    return true;
}

// Inputs apply at the start; then the time runs in whole substeps only.
// A point ends the step and drops the time that is left.
void sim_step(sim_state_t *state, sim_input_t const inputs[2], float delta_time) {
    state->events = 0;
    if (state->winner) return;
    apply_input(state, 1, &inputs[0]);
    apply_input(state, 2, &inputs[1]);
    state->accumulator += delta_time;
    while (state->accumulator >= SIM_STEP - SIM_STEP_SLACK) {
        state->accumulator -= SIM_STEP;
        if (substep(state)) {
            state->accumulator = 0;
            break;
        }
    }
    if (state->accumulator < SIM_STEP_SLACK) state->accumulator = 0;
    record_power_trail(state);
}
