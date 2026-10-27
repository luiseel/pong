#ifndef NET_H
#define NET_H

// The client side of the online protocol. All ENet use is in net.c, and it
// does not use SDL, so the headless bot links it too.
//
// Test knobs, read from the environment by net_connect:
//   PONG_FAKE_LAG_MS  holds every packet sent and every snapshot received
//                     this long (so the round trip grows by twice this)
//   PONG_FAKE_LOSS    percent of input packets and snapshots dropped

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "protocol.h"

#define NET_CONNECT_TIMEOUT 5000 // ms before a connection attempt fails
#define NET_HOST_LENGTH 128
#define NET_SNAPSHOT_QUEUE 32    // snapshots received and not yet taken
#define NET_DELAYED_PACKETS 128  // packets held back by PONG_FAKE_LAG_MS
#define NET_DELAYED_SIZE 64      // bytes; the largest client message is the INPUT

typedef enum {
    NET_IDLE,
    NET_CONNECTING, // waiting for the ENet connection
    NET_HELLO,      // HELLO sent, waiting for WELCOME
    NET_WAITING,    // in the room, waiting for START
    NET_PLAYING,
    NET_OVER,       // GAME_OVER or OPPONENT_LEFT arrived
    NET_FAILED      // see net_t.error
} net_status_t;

// Values of net_t.error, next to the NET_ERROR_* reasons from the server
enum { NET_FAIL_CONNECT = 100, NET_FAIL_LOST };

typedef struct {
    uint32_t due; // ENet time to let it go
    uint8_t channel;
    uint32_t flags;
    size_t size;
    uint8_t data[NET_DELAYED_SIZE];
} net_delayed_t;

typedef struct {
    uint32_t due;
    net_snapshot_t snapshot;
} net_queued_snapshot_t;

typedef struct {
    struct _ENetHost *host;
    struct _ENetPeer *peer;
    net_status_t status;
    int error;
    int op;                         // NET_OP_* sent in the HELLO
    int side;                       // 1 or 2 after WELCOME
    char code[NET_CODE_LENGTH + 1]; // the room code after WELCOME
    int target_score;
    uint32_t countdown_ticks;       // from the START
    uint32_t connect_started;       // ENet time when the attempt began
    // Latest snapshot taken with net_take_snapshot
    bool has_snapshot;
    uint32_t tick;
    uint32_t received_tick;         // newest tick that came in, taken or not
    uint16_t acked_seq;
    sim_state_t state;
    uint32_t snapshot_received_at;  // ENet time of the newest snapshot that came in
    bool opponent_silent;           // the newest snapshot has NET_SNAPSHOT_OPPONENT_SILENT
    // Snapshots in order of arrival, for net_take_snapshot
    net_queued_snapshot_t queue[NET_SNAPSHOT_QUEUE];
    int queue_first;
    int queue_count;
    // End of the match
    int winner;
    int player_one_score;
    int player_two_score;
    bool opponent_left;             // also set by a forfeit
    bool forfeit;                   // the match ended because a player left
    bool rematch_requested;
    // Inputs sent, the latest first, for redundancy
    uint16_t next_seq;
    net_input_entry_t history[NET_INPUT_HISTORY];
    int history_count;
    // Fake latency and loss
    int fake_lag;  // ms each way
    int fake_loss; // percent
    net_delayed_t delayed[NET_DELAYED_PACKETS];
    int delayed_first;
    int delayed_count;
} net_t;

// host:port from PONG_SERVER, or the defaults
void net_server_address(char *, size_t, uint16_t *);
bool net_connect(net_t *, char const *, uint16_t, int, char const *);
void net_disconnect(net_t *);
// Returns the seq of the input, or 0 when the match is not on
uint16_t net_send_input(net_t *, sim_input_t);
void net_request_rematch(net_t *);
// Services the connection for up to the given ms and updates the fields
void net_poll(net_t *, uint32_t);
// The next snapshot that came in, oldest first; false when there is none
bool net_take_snapshot(net_t *, net_snapshot_t *);
uint32_t net_rtt(net_t const *); // ms, ENet's estimate
char const *net_error_text(int);
// A hint while the match is on and the snapshots stop or the opponent is
// silent, NULL when all is well
char const *net_problem_text(net_t const *);

#endif
