#include "netplay.h"
#include <math.h>
#include <string.h>

#define OFFSET_GAIN 0.1 // of each clock sample that goes into the smoothed offset

static double tick_time(uint32_t tick) {
    return tick * (double)NETPLAY_TICK_SECONDS;
}

// True when a comes after b, with wrap-around
static bool seq_newer(uint16_t a, uint16_t b) {
    return a != b && (uint16_t)(a - b) < 0x8000;
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static void lerp_vector(vector_2d_t *out, vector_2d_t const *a, vector_2d_t const *b, float t) {
    out->x = lerp(a->x, b->x, t);
    out->y = lerp(a->y, b->y, t);
}

static float distance(vector_2d_t const *a, vector_2d_t const *b) {
    return sqrtf((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y));
}

static pad_t const *local_pad(netplay_t const *np, sim_state_t const *state) {
    return np->side == 1 ? &state->player_one_pad : &state->player_two_pad;
}

static pad_t *local_pad_of(netplay_t const *np, sim_state_t *state) {
    return np->side == 1 ? &state->player_one_pad : &state->player_two_pad;
}

// The i-th snapshot, the oldest first
static netplay_snapshot_t const *snapshot_at(netplay_t const *np, int i) {
    int index = (np->snapshot_next - np->snapshot_count + i + NETPLAY_SNAPSHOT_BUFFER) % NETPLAY_SNAPSHOT_BUFFER;
    return &np->snapshots[index];
}

static netplay_snapshot_t *snapshot_slot(netplay_t *np, int i) {
    return (netplay_snapshot_t *)snapshot_at(np, i);
}

static netplay_snapshot_t const *newest(netplay_t const *np) {
    return snapshot_at(np, np->snapshot_count - 1);
}

void netplay_start(netplay_t *np, int side, uint32_t countdown_ticks) {
    memset(np, 0, sizeof(*np));
    np->side = side;
    np->countdown_ticks = countdown_ticks;
}

// The server's pad for this side, moved by the inputs it has not applied yet
static void replay(netplay_t *np, uint16_t acked_seq) {
    for (int i = 0; i < np->input_count; i++) {
        int index = (np->input_next - np->input_count + i + NETPLAY_INPUT_BUFFER) % NETPLAY_INPUT_BUFFER;
        netplay_input_t const *input = &np->inputs[index];
        if (seq_newer(input->seq, acked_seq)) sim_step_pad(&np->predicted, input->move, NETPLAY_TICK_SECONDS);
    }
}

void netplay_input_sent(netplay_t *np, uint16_t seq, sim_input_t input) {
    np->inputs[np->input_next].seq = seq;
    np->inputs[np->input_next].move = input.move;
    np->input_next = (np->input_next + 1) % NETPLAY_INPUT_BUFFER;
    if (np->input_count < NETPLAY_INPUT_BUFFER) np->input_count++;
    if (np->has_predicted) {
        sim_step_pad(&np->predicted, input.move, NETPLAY_TICK_SECONDS);
        return;
    }
    if (np->snapshot_count == 0) return;
    netplay_snapshot_t const *latest = newest(np);
    np->predicted = *local_pad(np, &latest->state);
    np->has_predicted = true;
    replay(np, latest->acked_seq);
}

// Keep the pad on screen where it was, then fade the difference out
static void reconcile(netplay_t *np, netplay_snapshot_t const *snapshot) {
    if (!np->has_predicted) return;
    float before = np->predicted.position.y;
    np->predicted = *local_pad(np, &snapshot->state);
    replay(np, snapshot->acked_seq);
    float error = before - np->predicted.position.y;
    np->last_error = fabsf(error);
    if (np->last_error > np->max_error_window) np->max_error_window = np->last_error;
    if (np->last_error > NETPLAY_SNAP_DISTANCE) {
        np->correction = 0;
        np->correction_speed = 0;
        return;
    }
    np->correction += error;
    np->correction_speed = fmaxf(np->correction_speed, fabsf(np->correction) / NETPLAY_BLEND_TIME);
}

// The event bits whose count moved since the last snapshot
static uint32_t new_events(netplay_t *np, uint8_t const counts[NET_EVENT_COUNTS]) {
    uint32_t events = 0;
    if (np->has_counts) {
        for (int i = 0; i < NET_EVENT_COUNTS; i++) {
            if ((uint8_t)(counts[i] - np->event_counts[i]) != 0) events |= 1u << i;
        }
    }
    memcpy(np->event_counts, counts, NET_EVENT_COUNTS);
    np->has_counts = true;
    return events;
}

void netplay_receive(netplay_t *np, net_snapshot_t const *snapshot, double now) {
    if (np->snapshot_count > 0 && (int32_t)(snapshot->tick - newest(np)->tick) <= 0) return;

    double sample = now - tick_time(snapshot->tick);
    if (!np->has_offset || fabs(sample - np->offset) > NETPLAY_OFFSET_RESET) {
        np->offset = sample;
        np->has_offset = true;
        np->window_start = now;
    } else {
        np->offset += (sample - np->offset) * OFFSET_GAIN;
    }
    np->snapshots_in_window++;

    if (np->snapshot_count == NETPLAY_SNAPSHOT_BUFFER) {
        netplay_snapshot_t *oldest = snapshot_slot(np, 0);
        if (!oldest->events_taken) np->events |= oldest->events; // late, but not lost
        np->snapshot_count--;
    }
    netplay_snapshot_t *entry = &np->snapshots[np->snapshot_next];
    np->snapshot_next = (np->snapshot_next + 1) % NETPLAY_SNAPSHOT_BUFFER;
    np->snapshot_count++;
    entry->tick = snapshot->tick;
    entry->time = now;
    entry->acked_seq = snapshot->input_seq;
    entry->events = new_events(np, snapshot->event_counts);
    entry->events_taken = false;
    entry->state = snapshot->state;

    reconcile(np, entry);
}

void netplay_update(netplay_t *np, double now, float delta_time) {
    np->now = now;
    if (np->has_offset) np->render_time = now - np->offset - NETPLAY_DELAY;

    float step = np->correction_speed * delta_time;
    if (fabsf(np->correction) <= step) {
        np->correction = 0;
        np->correction_speed = 0;
    } else {
        np->correction -= np->correction < 0 ? -step : step;
    }

    // The sounds of a snapshot play when the view gets to its time; the events
    // happened in the ticks before it, so a tick early
    for (int i = 0; i < np->snapshot_count; i++) {
        netplay_snapshot_t *entry = snapshot_slot(np, i);
        if (entry->events_taken) continue;
        if (tick_time(entry->tick) - NETPLAY_TICK_SECONDS > np->render_time) break;
        np->events |= entry->events;
        entry->events_taken = true;
    }

    if (np->has_offset && now - np->window_start >= 1.0) {
        np->snapshot_rate = (float)(np->snapshots_in_window / (now - np->window_start));
        np->max_error = np->max_error_window;
        np->snapshots_in_window = 0;
        np->max_error_window = 0;
        np->window_start = now;
    }
}

// A timer that went up was restarted: that shows at once
static float lerp_timer(float a, float b, float t) {
    return b <= a ? lerp(a, b, t) : b;
}

static void lerp_pickup(power_up_pickup_t *out, power_up_pickup_t const *a, power_up_pickup_t const *b, float t) {
    if (!a->active || !b->active || a->kind != b->kind ||
        distance(&a->position, &b->position) > NETPLAY_BALL_SNAP_DISTANCE) return;
    lerp_vector(&out->position, &a->position, &b->position, t);
    out->animation_time = lerp(a->animation_time, b->animation_time, t);
}

// The view between two snapshots. The discrete parts come from the nearer one;
// positions are interpolated unless something happened between the two: a
// goal, a catch, a release or any other jump of the ball.
static void interpolate(sim_state_t *out, sim_state_t const *a, sim_state_t const *b, float t) {
    *out = t < 0.5f ? *a : *b;
    power_up_state_t const *pa = &a->power_ups;
    power_up_state_t const *pb = &b->power_ups;
    power_up_state_t *po = &out->power_ups;

    if (a->player_one_pad.height == b->player_one_pad.height) {
        out->player_one_pad.position.y = lerp(a->player_one_pad.position.y, b->player_one_pad.position.y, t);
    }
    if (a->player_two_pad.height == b->player_two_pad.height) {
        out->player_two_pad.position.y = lerp(a->player_two_pad.position.y, b->player_two_pad.position.y, t);
    }

    bool ball_continuous = a->player_one_score == b->player_one_score &&
        a->player_two_score == b->player_two_score &&
        pa->ball_held_by == pb->ball_held_by &&
        distance(&a->ball.position, &b->ball.position) <= NETPLAY_BALL_SNAP_DISTANCE;
    if (ball_continuous) {
        lerp_vector(&out->ball.position, &a->ball.position, &b->ball.position, t);
        if (pa->trail_count == pb->trail_count) {
            for (int i = 0; i < pa->trail_count; i++) lerp_vector(&po->trail[i], &pa->trail[i], &pb->trail[i], t);
        }
        if (pa->ball_held_by) po->hold_remaining = lerp(pa->hold_remaining, pb->hold_remaining, t);
    }

    for (int i = 0; i < POWER_UP_MAX_PICKUPS; i++) lerp_pickup(&po->pickups[i], &pa->pickups[i], &pb->pickups[i], t);

    po->player_one_remaining = lerp_timer(pa->player_one_remaining, pb->player_one_remaining, t);
    po->player_two_remaining = lerp_timer(pa->player_two_remaining, pb->player_two_remaining, t);
    po->player_one_grow_remaining = lerp_timer(pa->player_one_grow_remaining, pb->player_one_grow_remaining, t);
    po->player_two_grow_remaining = lerp_timer(pa->player_two_grow_remaining, pb->player_two_grow_remaining, t);
    po->player_one_serpent_remaining = lerp_timer(pa->player_one_serpent_remaining, pb->player_one_serpent_remaining, t);
    po->player_two_serpent_remaining = lerp_timer(pa->player_two_serpent_remaining, pb->player_two_serpent_remaining, t);
    po->player_one_charge_remaining = lerp_timer(pa->player_one_charge_remaining, pb->player_one_charge_remaining, t);
    po->player_two_charge_remaining = lerp_timer(pa->player_two_charge_remaining, pb->player_two_charge_remaining, t);
    po->spawn_remaining = lerp_timer(pa->spawn_remaining, pb->spawn_remaining, t);
}

float netplay_local_pad_y(netplay_t const *np) {
    return np->predicted.position.y + np->correction;
}

bool netplay_view(netplay_t const *np, sim_state_t *out) {
    if (np->snapshot_count == 0) return false;
    // The last snapshot at or before the render time, and the one after it
    int before = -1;
    for (int i = 0; i < np->snapshot_count; i++) {
        if (tick_time(snapshot_at(np, i)->tick) <= np->render_time) before = i;
    }
    if (before < 0) {
        *out = snapshot_at(np, 0)->state; // not yet in the buffer: the oldest, held
    } else if (before == np->snapshot_count - 1) {
        *out = newest(np)->state;         // starved: hold, no guess at where the ball went
    } else {
        netplay_snapshot_t const *a = snapshot_at(np, before);
        netplay_snapshot_t const *b = snapshot_at(np, before + 1);
        double ta = tick_time(a->tick);
        double tb = tick_time(b->tick);
        float t = (float)((np->render_time - ta) / (tb - ta));
        interpolate(out, &a->state, &b->state, t);
    }
    if (np->has_predicted) {
        pad_t *pad = local_pad_of(np, out);
        pad->position.y = netplay_local_pad_y(np);
        pad->velocity = np->predicted.velocity;
        pad->height = np->predicted.height;
    }
    return true;
}

float netplay_countdown(netplay_t const *np) {
    float total = (float)tick_time(np->countdown_ticks);
    if (!np->has_offset) return total;
    float remaining = (float)(tick_time(np->countdown_ticks) - np->render_time);
    return remaining < 0 ? 0 : remaining;
}

bool netplay_in_countdown(netplay_t const *np) {
    if (!np->has_offset) return true;
    return np->now - np->offset < tick_time(np->countdown_ticks);
}

bool netplay_caught_up(netplay_t const *np) {
    if (np->snapshot_count == 0) return true;
    return np->render_time >= tick_time(newest(np)->tick);
}

uint32_t netplay_take_events(netplay_t *np) {
    uint32_t events = np->events;
    np->events = 0;
    return events;
}
