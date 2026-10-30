#ifndef NETPLAY_H
#define NETPLAY_H

// The feel of the online match on the client. The local pad moves at once:
// it is predicted from the inputs the server has not applied yet, and when a
// snapshot comes in, the inputs after its acked seq are replayed on the server's
// pad. The rest of the match (the other pad, the ball, the pickups) is shown
// NETPLAY_DELAY behind the server, interpolated between the two snapshots
// around that time. No SDL and no ENet, so the tests build it alone.

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

#define NETPLAY_INPUT_BUFFER 64         // sent inputs kept for the replay: about 1 s
#define NETPLAY_SNAPSHOT_BUFFER 16      // about 0.5 s of snapshots
#define NETPLAY_DELAY 0.1f              // seconds the remote view lags the server
#define NETPLAY_BLEND_TIME 0.1f         // seconds to fade a small pad correction
#define NETPLAY_SNAP_DISTANCE 40.f      // px; a bigger pad correction is applied at once
#define NETPLAY_BALL_SNAP_DISTANCE 150.f // px between two snapshots: a teleport, not a move
#define NETPLAY_OFFSET_RESET 0.25f      // s; a clock sample this far off restarts the estimate
#define NETPLAY_TICK_SECONDS (1.f / NET_TICK_RATE)

typedef struct {
    uint32_t tick;
    double time;       // local time of arrival, seconds
    uint16_t acked_seq; // last local input the server applied in it
    uint32_t events;   // SIM_EVENT_* bits new in this snapshot
    bool events_taken;
    sim_state_t state;
} netplay_snapshot_t;

typedef struct {
    uint16_t seq;
    int8_t move;
} netplay_input_t;

typedef struct {
    int side;                // 1 or 2: the local pad
    uint32_t countdown_ticks;
    // Prediction
    netplay_input_t inputs[NETPLAY_INPUT_BUFFER]; // ring, the newest at input_next - 1
    int input_next;
    int input_count;
    pad_t predicted;         // the local pad after all the sent inputs
    bool has_predicted;
    float correction;        // px added to the predicted pad on screen, fading to 0
    float correction_speed;  // px per second of the fade
    // Interpolation
    netplay_snapshot_t snapshots[NETPLAY_SNAPSHOT_BUFFER]; // ring, the newest at snapshot_next - 1
    int snapshot_next;
    int snapshot_count;
    double offset;           // local time minus server time, smoothed
    bool has_offset;
    double now;
    double render_time;      // server time the view shows
    uint8_t event_counts[NET_EVENT_COUNTS];
    bool has_counts;
    uint32_t events;         // ready to play, see netplay_take_events
    // Stats
    float last_error;        // px, the correction the last snapshot made
    float max_error;         // the largest in the last second
    float max_error_window;  // the largest so far in the second being counted
    int snapshots_in_window;
    double window_start;
    float snapshot_rate;     // per second
} netplay_t;

void netplay_start(netplay_t *, int, uint32_t);
void netplay_input_sent(netplay_t *, uint16_t, sim_input_t);
void netplay_receive(netplay_t *, net_snapshot_t const *, double);
void netplay_update(netplay_t *, double, float);
// The match to draw: false when no snapshot has come yet
bool netplay_view(netplay_t const *, sim_state_t *);
// Seconds of countdown left in the view, or 0
float netplay_countdown(netplay_t const *);
// True until the server steps the sim; no input goes out before that
bool netplay_in_countdown(netplay_t const *);
// True when the view shows the newest snapshot, so nothing is still to come
bool netplay_caught_up(netplay_t const *);
uint32_t netplay_take_events(netplay_t *);
// The y of the local pad as shown, prediction plus the fading correction
float netplay_local_pad_y(netplay_t const *);

#endif
