#include "net.h"

#include <ctype.h>
#include <enet/enet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The peer stays, so net_disconnect can still tell the server
static void fail(net_t *net, int error) {
    net->status = NET_FAILED;
    net->error = error;
}

void net_server_address(char *host, size_t size, uint16_t *port) {
    char const *text = getenv("PONG_SERVER");
    snprintf(host, size, "%s", NET_DEFAULT_HOST);
    *port = NET_DEFAULT_PORT;
    if (!text || !*text) return;
    char const *colon = strrchr(text, ':');
    size_t length = colon ? (size_t)(colon - text) : strlen(text);
    if (length == 0 || length >= size) return;
    memcpy(host, text, length);
    host[length] = 0;
    if (colon && colon[1]) {
        long value = strtol(colon + 1, NULL, 10);
        if (value > 0 && value < 65536) *port = (uint16_t)value;
    }
}

static int env_int(char const *name, int max) {
    char const *text = getenv(name);
    if (!text || !*text) return 0;
    long value = strtol(text, NULL, 10);
    return value < 0 ? 0 : value > max ? max : (int)value;
}

bool net_connect(net_t *net, char const *host, uint16_t port, int op, char const *code) {
    static bool initialized;
    if (!initialized) {
        if (enet_initialize() != 0) return false;
        atexit(enet_deinitialize);
        initialized = true;
    }
    net_disconnect(net);
    memset(net, 0, sizeof(*net));
    net->fake_lag = env_int("PONG_FAKE_LAG_MS", 5000);
    net->fake_loss = env_int("PONG_FAKE_LOSS", 100);
    for (int i = 0; code && code[i] && i < NET_CODE_LENGTH; i++) {
        net->code[i] = (char)toupper((unsigned char)code[i]);
    }
    net->host = enet_host_create(NULL, 1, NET_CHANNELS, 0, 0);
    if (!net->host) {
        fail(net, NET_FAIL_CONNECT);
        return false;
    }
    ENetAddress address;
    if (enet_address_set_host(&address, host) != 0) {
        fail(net, NET_FAIL_CONNECT);
        return false;
    }
    address.port = port;
    net->op = op;
    net->peer = enet_host_connect(net->host, &address, NET_CHANNELS, 0);
    if (!net->peer) {
        fail(net, NET_FAIL_CONNECT);
        return false;
    }
    // A lost server shows in about 10 s, not ENet's default 30 s
    enet_peer_timeout(net->peer, 0, NET_PEER_TIMEOUT_MS, NET_PEER_TIMEOUT_MS);
    net->status = NET_CONNECTING;
    net->connect_started = enet_time_get();
    return true;
}

void net_disconnect(net_t *net) {
    if (net->peer) enet_peer_disconnect_now(net->peer, 0);
    if (net->host) enet_host_destroy(net->host);
    net->host = NULL;
    net->peer = NULL;
    net->status = NET_IDLE;
}

static bool dropped(net_t const *net) {
    return net->fake_loss > 0 && rand() % 100 < net->fake_loss;
}

static void send_now(net_t *net, uint8_t channel, uint32_t flags, void const *data, size_t size) {
    if (!net->peer) return;
    ENetPacket *packet = enet_packet_create(data, size, flags);
    if (packet) enet_peer_send(net->peer, channel, packet);
}

// Held back by the fake lag; the oldest is lost when the queue is full
static void send_packet(net_t *net, uint8_t channel, uint32_t flags, void const *data, size_t size) {
    if (net->fake_lag <= 0 || size > NET_DELAYED_SIZE) {
        send_now(net, channel, flags, data, size);
        return;
    }
    if (net->delayed_count == NET_DELAYED_PACKETS) {
        net->delayed_first = (net->delayed_first + 1) % NET_DELAYED_PACKETS;
        net->delayed_count--;
    }
    net_delayed_t *entry = &net->delayed[(net->delayed_first + net->delayed_count++) % NET_DELAYED_PACKETS];
    entry->due = enet_time_get() + (uint32_t)net->fake_lag;
    entry->channel = channel;
    entry->flags = flags;
    entry->size = size;
    memcpy(entry->data, data, size);
}

static void flush_delayed(net_t *net) {
    uint32_t now = enet_time_get();
    while (net->delayed_count > 0) {
        net_delayed_t *entry = &net->delayed[net->delayed_first];
        if ((int32_t)(now - entry->due) < 0) return;
        send_now(net, entry->channel, entry->flags, entry->data, entry->size);
        net->delayed_first = (net->delayed_first + 1) % NET_DELAYED_PACKETS;
        net->delayed_count--;
    }
}

static void send_control(net_t *net, void const *data, size_t size) {
    send_packet(net, NET_CHANNEL_CONTROL, ENET_PACKET_FLAG_RELIABLE, data, size);
}

static void send_hello(net_t *net, int op) {
    net_hello_t hello = {0};
    hello.type = NET_MSG_HELLO;
    hello.version = NET_VERSION;
    hello.op = (uint8_t)op;
    memcpy(hello.code, net->code, NET_CODE_LENGTH);
    hello.state_size = (uint16_t)sizeof(sim_state_t);
    send_control(net, &hello, sizeof(hello));
    net->status = NET_HELLO;
}

uint16_t net_send_input(net_t *net, sim_input_t input) {
    if (net->status != NET_PLAYING) return 0;
    if (net->history_count < NET_INPUT_HISTORY) net->history_count++;
    memmove(&net->history[1], &net->history[0], (size_t)(net->history_count - 1) * sizeof(net->history[0]));
    if (net->next_seq == 0) net->next_seq = 1; // 0 is "none applied" in a snapshot
    net->history[0].seq = net->next_seq++;
    net->history[0].input = input;

    net_input_t message = {0};
    message.type = NET_MSG_INPUT;
    message.count = (uint8_t)net->history_count;
    message.latest_seq = net->history[0].seq;
    memcpy(message.inputs, net->history, sizeof(message.inputs));
    if (!dropped(net)) send_packet(net, NET_CHANNEL_STATE, 0, &message, sizeof(message));
    return net->history[0].seq;
}

void net_request_rematch(net_t *net) {
    if (net->status != NET_OVER || net->opponent_left || !net->winner || net->rematch_requested) return;
    net_header_t header = {NET_MSG_REMATCH};
    send_control(net, &header, sizeof(header));
    net->rematch_requested = true;
}

// A new match: the ticks and the input seqs start over
static void start_match(net_t *net, net_start_t const *start) {
    net->target_score = start->target_score;
    net->countdown_ticks = start->countdown_ticks;
    net->has_snapshot = false;
    net->tick = 0;
    net->received_tick = 0;
    net->acked_seq = 0;
    net->queue_count = 0;
    net->winner = 0;
    net->player_one_score = 0;
    net->player_two_score = 0;
    net->rematch_requested = false;
    net->opponent_silent = false;
    net->snapshot_received_at = 0;
    net->next_seq = 1;
    net->history_count = 0;
    net->status = NET_PLAYING;
}

static void queue_snapshot(net_t *net, net_snapshot_t const *snapshot) {
    if (net->queue_count == NET_SNAPSHOT_QUEUE) { // the oldest goes: the newest matters most
        net->queue_first = (net->queue_first + 1) % NET_SNAPSHOT_QUEUE;
        net->queue_count--;
    }
    net_queued_snapshot_t *entry = &net->queue[(net->queue_first + net->queue_count++) % NET_SNAPSHOT_QUEUE];
    entry->due = enet_time_get() + (uint32_t)net->fake_lag;
    entry->snapshot = *snapshot;
}

static void receive(net_t *net, uint8_t const *data, size_t size) {
    if (size < sizeof(net_header_t)) return;
    switch (data[0]) {
        case NET_MSG_WELCOME: {
            if (size != sizeof(net_welcome_t)) return;
            net_welcome_t welcome;
            memcpy(&welcome, data, sizeof(welcome));
            net->side = welcome.side;
            memcpy(net->code, welcome.code, NET_CODE_LENGTH);
            net->code[NET_CODE_LENGTH] = 0;
            net->status = NET_WAITING;
            break;
        }
        case NET_MSG_ERROR: {
            if (size != sizeof(net_error_t)) return;
            fail(net, data[1]);
            break;
        }
        case NET_MSG_START: {
            if (size != sizeof(net_start_t)) return;
            if (net->status != NET_WAITING && net->status != NET_OVER) return;
            net_start_t start;
            memcpy(&start, data, sizeof(start));
            start_match(net, &start);
            break;
        }
        case NET_MSG_SNAPSHOT: {
            if (size != sizeof(net_snapshot_t)) return;
            if (net->status != NET_PLAYING) return;
            net_snapshot_t snapshot;
            memcpy(&snapshot, data, sizeof(snapshot));
            // Sequenced delivery drops old ones, but a stale tick is not applied
            if (net->queue_count + net->has_snapshot > 0 &&
                (int32_t)(snapshot.tick - net->received_tick) <= 0) return;
            net->received_tick = snapshot.tick;
            if (dropped(net)) return;
            net->snapshot_received_at = enet_time_get();
            net->opponent_silent = (snapshot.flags & NET_SNAPSHOT_OPPONENT_SILENT) != 0;
            queue_snapshot(net, &snapshot);
            break;
        }
        case NET_MSG_GAME_OVER: {
            if (size != sizeof(net_game_over_t)) return;
            net_game_over_t over;
            memcpy(&over, data, sizeof(over));
            net->winner = over.winner;
            net->player_one_score = over.player_one_score;
            net->player_two_score = over.player_two_score;
            net->forfeit = over.reason == NET_OVER_FORFEIT;
            if (net->forfeit) net->opponent_left = true; // no rematch can come
            net->status = NET_OVER;
            break;
        }
        case NET_MSG_OPPONENT_LEFT:
            if (size != sizeof(net_header_t)) return;
            // Also after the match: no rematch can come
            net->opponent_left = true;
            net->status = NET_OVER;
            break;
        default:
            break;
    }
}

void net_poll(net_t *net, uint32_t timeout) {
    if (!net->host || net->status == NET_IDLE) return;
    flush_delayed(net);
    ENetEvent event;
    while (enet_host_service(net->host, &event, timeout) > 0) {
        timeout = 0;
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                if (net->status == NET_CONNECTING) send_hello(net, net->op);
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                if (net->status != NET_FAILED) receive(net, event.packet->data, event.packet->dataLength);
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                net->peer = NULL; // after an ERROR the server closes the connection
                if (net->status == NET_CONNECTING) fail(net, NET_FAIL_CONNECT);
                else if (net->status != NET_OVER && net->status != NET_FAILED) fail(net, NET_FAIL_LOST);
                break;
            default:
                break;
        }
    }
    flush_delayed(net);
    if (net->status == NET_CONNECTING &&
        enet_time_get() - net->connect_started > NET_CONNECT_TIMEOUT) {
        fail(net, NET_FAIL_CONNECT);
    }
}

bool net_take_snapshot(net_t *net, net_snapshot_t *snapshot) {
    if (net->queue_count == 0) return false;
    net_queued_snapshot_t const *entry = &net->queue[net->queue_first];
    if (net->fake_lag > 0 && (int32_t)(enet_time_get() - entry->due) < 0) return false;
    *snapshot = entry->snapshot;
    net->has_snapshot = true;
    net->tick = snapshot->tick;
    net->acked_seq = snapshot->input_seq;
    net->state = snapshot->state;
    net->queue_first = (net->queue_first + 1) % NET_SNAPSHOT_QUEUE;
    net->queue_count--;
    return true;
}

uint32_t net_rtt(net_t const *net) {
    return net->peer ? net->peer->roundTripTime : 0;
}

char const *net_error_text(int error) {
    switch (error) {
        case NET_ERROR_VERSION: return "VERSION MISMATCH";
        case NET_ERROR_NOT_FOUND: return "ROOM NOT FOUND";
        case NET_ERROR_FULL: return "ROOM IS FULL";
        case NET_ERROR_EXPIRED: return "ROOM EXPIRED";
        case NET_ERROR_INVALID: return "INVALID REQUEST";
        case NET_FAIL_CONNECT: return "CANNOT REACH SERVER";
        case NET_FAIL_LOST: return "CONNECTION LOST";
        default: return "NETWORK ERROR";
    }
}

char const *net_problem_text(net_t const *net) {
    if (net->status != NET_PLAYING || !net->snapshot_received_at) return NULL;
    if (enet_time_get() - net->snapshot_received_at > NET_SILENT_MS) return "CONNECTION PROBLEM";
    if (net->opponent_silent) return "OPPONENT CONNECTION PROBLEM";
    return NULL;
}
