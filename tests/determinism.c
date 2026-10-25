#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sim.h"

#define TICKS 20000 // a little over 5 minutes at 60 Hz

// The same inputs for every run: the pads sweep up and down at different rates,
// and both players get power-ups now and then and use them
static void script(int tick, sim_input_t inputs[2]) {
    inputs[0].move = (int8_t)(((tick / 23) % 3 - 1) * 127);
    inputs[1].move = (int8_t)(((tick / 41) % 5 - 2) * 60);
    inputs[0].actions = tick % 97 == 0 ? SIM_ACTION_USE : 0;
    inputs[1].actions = tick % 131 == 0 ? SIM_ACTION_SELECT | SIM_ACTION_USE : 0;
}

static uint32_t run(sim_state_t *state, uint64_t seed) {
    uint32_t events = 0;
    sim_init(state, seed, 0);
    for (int tick = 0; tick < TICKS; tick++) {
        sim_input_t inputs[2];
        script(tick, inputs);
        if (tick % 300 == 0) {
            debug_give_power_up(state, (power_up_kind_t)(tick / 300 % 4), 1 + tick / 300 % 2);
        }
        sim_step(state, inputs, 1.f / 60.f);
        events |= state->events;
    }
    return events;
}

int main(void) {
    static sim_state_t first, second, other;
    uint32_t events = run(&first, 1234);
    assert(run(&second, 1234) == events);
    assert(memcmp(&first, &second, sizeof(first)) == 0);

    // The match really used the parts that need the random numbers and the actions
    assert(first.player_one_score + first.player_two_score > 0);
    assert(events & SIM_EVENT_PAD_HIT);
    assert(events & SIM_EVENT_WALL_HIT);
    assert(events & SIM_EVENT_GOAL);
    assert(events & SIM_EVENT_PICKUP);
    assert(events & SIM_EVENT_BLAST);
    assert(first.accumulator == 0);

    // A copy continues the same as the original
    sim_state_t copy = first;
    sim_input_t inputs[2] = {{127, SIM_ACTION_USE}, {-127, 0}};
    for (int i = 0; i < 600; i++) {
        sim_step(&first, inputs, 1.f / 60.f);
        sim_step(&copy, inputs, 1.f / 60.f);
    }
    assert(memcmp(&first, &copy, sizeof(first)) == 0);

    run(&other, 4321);
    assert(memcmp(&first, &other, sizeof(first)) != 0);
    assert(memcmp(&second, &other, sizeof(second)) != 0);
    printf("Determinism checks passed (score %d - %d)\n", second.player_one_score,
        second.player_two_score);
    return 0;
}
