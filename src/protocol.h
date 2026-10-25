#ifndef PROTOCOL_H
#define PROTOCOL_H

// The wire format between the client and the server. Both compile this header
// (the server through translate-c), so a struct goes on the wire as it is in
// memory and the layout cannot drift. That assumes little-endian hosts with
// the same ABI for the fields; the HELLO carries sizeof(sim_state_t) so the
// server rejects a client built with another layout.

#include <stdint.h>
#include "sim.h"

#define NET_VERSION 3
#ifndef NET_DEFAULT_HOST
#define NET_DEFAULT_HOST "localhost" // set with make NET_DEFAULT_HOST=host
#endif
#define NET_DEFAULT_PORT 7777
#define NET_CODE_LENGTH 5
#define NET_CODE_ALPHABET "ABCDEFGHJKLMNPQRSTUVWXYZ23456789" // no 0/O and 1/I
#define NET_INPUT_HISTORY 5      // inputs in an INPUT packet, the latest first
#define NET_CHANNEL_STATE 0      // unreliable sequenced: INPUT and SNAPSHOT
#define NET_CHANNEL_CONTROL 1    // reliable: all other messages
#define NET_CHANNELS 2
#define NET_TICK_RATE 60         // server sim steps per second
#define NET_SNAPSHOT_INTERVAL 2  // ticks between snapshots
#define NET_COUNTDOWN_TICKS 210  // MATCH_COUNTDOWN_SECONDS of ticks before the serve (a test checks it)
#define NET_EVENT_COUNTS 8       // per-type event counters in a snapshot, one per SIM_EVENT_* bit
#define NET_PEER_TIMEOUT_MS 10000 // a peer silent this long is gone; in a match it forfeits
#define NET_SILENT_MS 2000        // a peer silent this long has a connection problem

// Every message starts with its type byte
enum {
    NET_MSG_HELLO = 1,    // C->S: net_hello_t
    NET_MSG_INPUT,        // C->S: net_input_t
    NET_MSG_WELCOME,      // S->C: net_welcome_t
    NET_MSG_ERROR,        // S->C: net_error_t, then the server disconnects
    NET_MSG_START,        // S->C: net_start_t, both players are in
    NET_MSG_SNAPSHOT,     // S->C: net_snapshot_t
    NET_MSG_GAME_OVER,    // S->C: net_game_over_t
    NET_MSG_OPPONENT_LEFT, // S->C: net_header_t, the opponent left after the match
    NET_MSG_REMATCH        // C->S: net_header_t, play again; a START follows when both ask
};

enum { NET_OP_CREATE = 1, NET_OP_JOIN };

enum {
    NET_ERROR_VERSION = 1,
    NET_ERROR_NOT_FOUND,
    NET_ERROR_FULL,    // the room has two players, or the server has no free room
    NET_ERROR_EXPIRED, // the room waited too long without a match
    NET_ERROR_INVALID  // a HELLO the server cannot use
};

// net_game_over_t.reason
enum {
    NET_OVER_SCORE = 1, // the winner got to the target score
    NET_OVER_FORFEIT    // the other player left or was silent for NET_PEER_TIMEOUT_MS
};

// net_snapshot_t.flags
#define NET_SNAPSHOT_OPPONENT_SILENT 0x01 // nothing from the opponent for NET_SILENT_MS

typedef struct {
    uint8_t type;
} net_header_t;

typedef struct {
    uint8_t type;
    uint8_t version;
    uint8_t op;                    // NET_OP_*
    uint8_t code[NET_CODE_LENGTH]; // for NET_OP_JOIN
    uint16_t state_size;           // sizeof(sim_state_t) of the client build
} net_hello_t;

typedef struct {
    uint16_t seq;
    sim_input_t input;
} net_input_entry_t;

typedef struct {
    uint8_t type;
    uint8_t count;       // valid entries in inputs
    uint16_t latest_seq; // same as inputs[0].seq
    net_input_entry_t inputs[NET_INPUT_HISTORY];
} net_input_t;

typedef struct {
    uint8_t type;
    uint8_t side; // 1 is the left pad, 2 the right
    uint8_t code[NET_CODE_LENGTH];
    uint8_t reserved;
} net_welcome_t;

typedef struct {
    uint8_t type;
    uint8_t reason; // NET_ERROR_*
} net_error_t;

typedef struct {
    uint8_t type;
    uint8_t reserved;
    uint16_t target_score;
    uint16_t countdown_ticks; // the sim starts to step at this tick
} net_start_t;

typedef struct {
    uint8_t type;
    uint8_t flags;      // NET_SNAPSHOT_*
    uint16_t input_seq; // last input of the receiving client applied
    uint32_t tick;
    // Running counts of the SIM_EVENT_* bits, by bit index; they wrap. A lost
    // snapshot loses no event: the receiver plays the counts that changed.
    uint8_t event_counts[NET_EVENT_COUNTS];
    sim_state_t state;
} net_snapshot_t;

typedef struct {
    uint8_t type;
    uint8_t winner; // 1 or 2
    uint8_t reason; // NET_OVER_*
    uint8_t reserved;
    uint16_t player_one_score;
    uint16_t player_two_score;
} net_game_over_t;

// The structs have no padding by construction; a wrong size does not compile
#define NET_SIZE_CHECK(name, type, size) typedef char name[sizeof(type) == (size) ? 1 : -1]
NET_SIZE_CHECK(net_hello_size_check, net_hello_t, 10);
NET_SIZE_CHECK(net_input_entry_size_check, net_input_entry_t, 4);
NET_SIZE_CHECK(net_input_size_check, net_input_t, 4 + 4 * NET_INPUT_HISTORY);
NET_SIZE_CHECK(net_welcome_size_check, net_welcome_t, 8);
NET_SIZE_CHECK(net_error_size_check, net_error_t, 2);
NET_SIZE_CHECK(net_start_size_check, net_start_t, 6);
NET_SIZE_CHECK(net_snapshot_size_check, net_snapshot_t, 16 + sizeof(sim_state_t));
NET_SIZE_CHECK(net_game_over_size_check, net_game_over_t, 8);

#endif
