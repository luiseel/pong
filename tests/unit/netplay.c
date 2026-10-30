// Checks of the client-side prediction and interpolation, without a network:
// a server sim and a netplay client talk through pipes with a fixed delay.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "netplay.h"

#define DELAY 3        // ticks each way
#define TICKS 600      // 10 s
#define USE_TICK 200   // the client grows its pad here
#define TICK_SECONDS (1.f / NET_TICK_RATE)
#define CLOCK_OFFSET 10.0 // the client clock is this far from the server's

static float const tolerance = 1e-4f;

// The pad sweeps up, stops, sweeps down
static int8_t script(int tick) {
    return (int8_t)(((tick / 17) % 3 - 1) * 127);
}

static void make_snapshot(net_snapshot_t *out, uint32_t tick, uint16_t acked,
                          sim_state_t const *state, uint8_t const counts[NET_EVENT_COUNTS]) {
    memset(out, 0, sizeof(*out));
    out->type = NET_MSG_SNAPSHOT;
    out->tick = tick;
    out->input_seq = acked;
    if (counts) memcpy(out->event_counts, counts, NET_EVENT_COUNTS);
    out->state = *state;
}

// The server applies each client input DELAY ticks after it was sent, one per
// tick, and sends a snapshot every second tick that gets back DELAY ticks
// later. After reconciliation the client's pad is the server's pad to come.
static void test_reconciliation(void) {
    static sim_state_t server;
    static netplay_t client;
    static struct { bool valid; uint16_t seq; sim_input_t input; } to_server[TICKS + DELAY + 1];
    static struct { bool valid; net_snapshot_t snapshot; } to_client[TICKS + DELAY + 2];
    static float predicted_y[TICKS + 2];
    static bool comparable[TICKS + 2];
    static float server_y[TICKS + 2];
    static bool applied[TICKS + 2];
    uint8_t counts[NET_EVENT_COUNTS] = {0};
    uint16_t seq_counter = 0;
    uint16_t acked = 0;
    uint16_t use_seq = 0;
    uint16_t known_seq = 0; // the newest acked seq the client has heard of
    int8_t last_move = 0;
    float max_error = 0;
    float grow_error = 0;
    int compared = 0;

    sim_init(&server, 42, 0);
    server.ball.velocity = (vector_2d_t){0, 0}; // the ball stays: no goal cuts a tick short
    netplay_start(&client, 1, 0);

    for (int tick = 0; tick < TICKS; tick++) {
        double now = tick * (double)TICK_SECONDS + CLOCK_OFFSET;
        // Snapshots that get to the client now
        if (to_client[tick].valid) {
            netplay_receive(&client, &to_client[tick].snapshot, now);
            known_seq = to_client[tick].snapshot.input_seq;
            if (client.has_predicted && use_seq && known_seq < use_seq) assert(client.last_error <= tolerance);
            if (client.has_predicted && use_seq && known_seq >= use_seq && grow_error == 0) {
                grow_error = client.last_error; // the first snapshot with the grown pad
            }
        }
        netplay_update(&client, now, TICK_SECONDS);
        if (!netplay_in_countdown(&client)) {
            sim_input_t input = {script(tick), 0};
            if (tick == USE_TICK) {
                input.actions = SIM_ACTION_USE;
                debug_give_power_up(&server, POWER_UP_GROW, 1); // the server has it before the input gets there
            }
            uint16_t seq = ++seq_counter;
            netplay_input_sent(&client, seq, input);
            if (tick == USE_TICK) use_seq = seq;
            predicted_y[seq] = client.predicted.position.y;
            // Before the first snapshot nothing is predicted; between the grow and
            // the news of it the client cannot know the pad moved
            comparable[seq] = client.has_predicted && (!use_seq || known_seq >= use_seq);
            to_server[tick + DELAY].valid = true;
            to_server[tick + DELAY].seq = seq;
            to_server[tick + DELAY].input = input;
        }
        // The server tick
        sim_input_t inputs[2] = {{last_move, 0}, {0, 0}};
        if (to_server[tick].valid) {
            inputs[0] = to_server[tick].input;
            acked = to_server[tick].seq;
            last_move = inputs[0].move;
        }
        sim_step(&server, inputs, TICK_SECONDS);
        assert(!(server.events & SIM_EVENT_GOAL));
        for (int i = 0; i < NET_EVENT_COUNTS; i++) counts[i] += (server.events >> i) & 1;
        if (to_server[tick].valid) {
            server_y[acked] = server.player_one_pad.position.y;
            applied[acked] = true;
        }
        if ((tick + 1) % NET_SNAPSHOT_INTERVAL == 0) {
            to_client[tick + 1 + DELAY].valid = true;
            make_snapshot(&to_client[tick + 1 + DELAY].snapshot, (uint32_t)tick + 1, acked, &server, counts);
        }
    }

    for (int seq = 1; seq <= seq_counter; seq++) {
        if (!applied[seq] || !comparable[seq]) continue;
        float error = fabsf(predicted_y[seq] - server_y[seq]);
        if (error > max_error) max_error = error;
        compared++;
    }
    assert(compared > TICKS / 2);
    assert(max_error <= tolerance);
    // The grow moved the pad 25 px up on the server; the client took the news as
    // a correction and faded it within the blend time
    assert(client.predicted.height == POWER_UP_GROW_HEIGHT);
    assert(grow_error > 20 && grow_error < NETPLAY_SNAP_DISTANCE);
    assert(client.correction == 0);
    printf("Reconciliation: %d inputs compared, max error %g px, grow correction %g px\n",
        compared, max_error, grow_error);
}

// The correction fades in NETPLAY_BLEND_TIME, and a big one is applied at once
static void test_correction_fade(void) {
    static netplay_t client;
    static sim_state_t state;
    net_snapshot_t snapshot;
    sim_init(&state, 1, 0);
    netplay_start(&client, 2, 0);
    make_snapshot(&snapshot, 2, 0, &state, NULL);
    netplay_receive(&client, &snapshot, 1.0);
    netplay_update(&client, 1.0, TICK_SECONDS);
    netplay_input_sent(&client, 1, (sim_input_t){0, 0});
    assert(client.has_predicted);

    state.player_two_pad.position.y += 20;
    make_snapshot(&snapshot, 4, 1, &state, NULL);
    netplay_receive(&client, &snapshot, 1.0 + 2 * TICK_SECONDS);
    assert(fabsf(client.correction + 20) < 1e-3); // the pad is drawn where it was
    assert(fabsf(netplay_local_pad_y(&client) - (state.player_two_pad.position.y - 20)) < 1e-3);
    for (int i = 0; i < 5; i++) netplay_update(&client, 1.1, NETPLAY_BLEND_TIME / 10);
    assert(fabsf(client.correction + 20 * 0.5f) < 1e-3); // half way after half the time
    for (int i = 0; i < 5; i++) netplay_update(&client, 1.2, NETPLAY_BLEND_TIME / 10);
    assert(client.correction == 0);

    state.player_two_pad.position.y += 100;
    make_snapshot(&snapshot, 6, 1, &state, NULL);
    netplay_receive(&client, &snapshot, 1.0 + 4 * TICK_SECONDS);
    assert(client.correction == 0 && client.last_error > 99);
    printf("Correction fade checks passed\n");
}

static void feed(netplay_t *client, uint32_t tick, sim_state_t const *state, uint8_t const counts[NET_EVENT_COUNTS]) {
    net_snapshot_t snapshot;
    make_snapshot(&snapshot, tick, 0, state, counts);
    netplay_receive(client, &snapshot, tick * (double)TICK_SECONDS + CLOCK_OFFSET);
}

// Shows the view at this server time
static void view_at(netplay_t *client, double server_time, sim_state_t *view) {
    netplay_update(client, server_time + CLOCK_OFFSET + NETPLAY_DELAY, TICK_SECONDS);
    assert(netplay_view(client, view));
}

static void test_interpolation(void) {
    static netplay_t client;
    static sim_state_t state, view;
    uint8_t counts[NET_EVENT_COUNTS] = {0};
    sim_init(&state, 7, 0);
    netplay_start(&client, 2, 0);
    state.power_ups.pickups[0].active = true;
    state.power_ups.pickups[0].kind = POWER_UP_SPEED_BOOST;
    for (uint32_t tick = 0; tick <= 6; tick += 2) {
        state.ball.position.x = 100 + 5 * tick;
        state.player_one_pad.position.y = 5 * tick;
        state.power_ups.pickups[0].position.x = 200 + 5 * tick;
        state.power_ups.player_one_remaining = 8 - 0.1f * tick;
        if (tick == 2) counts[0]++;      // a pad hit in the snapshot of tick 2
        if (tick == 4) counts[1]++;      // a wall hit in the one of tick 4
        if (tick != 4) feed(&client, tick, &state, counts); // the snapshot of tick 4 is lost
    }
    assert(client.snapshot_count == 3);
    assert(fabs(client.offset - CLOCK_OFFSET) < 1e-9);

    // Half way between the snapshots of tick 2 and tick 6
    view_at(&client, 4 * TICK_SECONDS, &view);
    assert(fabsf(view.ball.position.x - 120) < 1e-3);
    assert(fabsf(view.player_one_pad.position.y - 20) < 1e-3);
    assert(fabsf(view.power_ups.pickups[0].position.x - 220) < 1e-3);
    assert(fabsf(view.power_ups.player_one_remaining - 7.6f) < 1e-3);
    // Before the buffer: the oldest; after it: the newest, held
    view_at(&client, -1, &view);
    assert(view.ball.position.x == 100);
    view_at(&client, 1, &view);
    assert(view.ball.position.x == 130);
    // Every sound once, the lost snapshot's included, when the view gets there
    assert((netplay_take_events(&client) & (SIM_EVENT_PAD_HIT | SIM_EVENT_WALL_HIT)) ==
        (SIM_EVENT_PAD_HIT | SIM_EVENT_WALL_HIT));
    assert(netplay_take_events(&client) == 0);

    // A goal: the ball and the score jump between the snapshots, so the view
    // jumps with the nearer one
    state.ball.position.x = 314;
    state.player_two_score = 1;
    feed(&client, 8, &state, counts);
    view_at(&client, 6.5f * TICK_SECONDS, &view);
    assert(view.ball.position.x == 130 && view.player_two_score == 0);
    view_at(&client, 7.5f * TICK_SECONDS, &view);
    assert(view.ball.position.x == 314 && view.player_two_score == 1);
    assert(!netplay_caught_up(&client));
    view_at(&client, 8 * TICK_SECONDS, &view);
    assert(netplay_caught_up(&client));

    // A catch: the ball sticks to the pad, no interpolation into the pad
    state.ball.position.x = 300;
    feed(&client, 10, &state, counts);
    state.ball.position.x = 25;
    state.power_ups.ball_held_by = 1;
    state.power_ups.pickups[0].kind = POWER_UP_GROW; // a new pickup in the same slot
    state.power_ups.pickups[0].position.x = 260;
    feed(&client, 12, &state, counts);
    view_at(&client, 11 * TICK_SECONDS, &view);
    assert(view.ball.position.x == 300 || view.ball.position.x == 25);
    assert(view.power_ups.pickups[0].position.x == 230 || view.power_ups.pickups[0].position.x == 260);
    // A snapshot from the past is ignored
    feed(&client, 11, &state, counts);
    assert(client.snapshot_count == 6 && client.snapshots[(client.snapshot_next + NETPLAY_SNAPSHOT_BUFFER - 1) %
        NETPLAY_SNAPSHOT_BUFFER].tick == 12);
    printf("Interpolation checks passed\n");
}

static void test_countdown(void) {
    static netplay_t client;
    static sim_state_t state;
    assert(NET_COUNTDOWN_TICKS == (int)(MATCH_COUNTDOWN_SECONDS * NET_TICK_RATE));
    sim_init(&state, 3, 0);
    netplay_start(&client, 1, NET_COUNTDOWN_TICKS);
    assert(netplay_in_countdown(&client));
    assert(fabsf(netplay_countdown(&client) - MATCH_COUNTDOWN_SECONDS) < 1e-3);
    feed(&client, 60, &state, NULL);
    netplay_update(&client, 1.0 + CLOCK_OFFSET, TICK_SECONDS);
    assert(netplay_in_countdown(&client));
    assert(fabsf(netplay_countdown(&client) - (MATCH_COUNTDOWN_SECONDS - 1 + NETPLAY_DELAY)) < 1e-3);
    // Nothing is predicted before the serve, as nothing was sent
    assert(!client.has_predicted);
    netplay_update(&client, 3.6 + CLOCK_OFFSET, TICK_SECONDS);
    assert(!netplay_in_countdown(&client));
    netplay_update(&client, 3.6 + NETPLAY_DELAY + CLOCK_OFFSET, TICK_SECONDS);
    assert(netplay_countdown(&client) == 0);
    printf("Countdown checks passed\n");
}

int main(void) {
    test_reconciliation();
    test_correction_fade();
    test_interpolation();
    test_countdown();
    printf("Netplay checks passed\n");
    return 0;
}
