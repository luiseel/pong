// Headless online client for automated tests. No SDL.
//
//   net_bot create [--server host:port] [--rematch]   prints the room code, then plays
//   net_bot join CODE [--server host:port] [--rematch]
//   net_bot no-hello | garbage | flood [--server host:port]
//
// With --rematch the bot asks for a rematch after the first match and plays
// a second one. PONG_FAKE_LAG_MS and PONG_FAKE_LOSS (see net.h) add latency
// and loss.
//
// The other modes misbehave, and the server must drop them: no-hello
// connects and sends nothing; garbage sends malformed packets; flood creates
// a room, then sends far more inputs than a client does. They print
// "dropped after N ms" and exit 0 when the server drops them, 5 when it
// does not in 15 s.
//
// Exit codes: 0 GAME_OVER, 2 ERROR from the server, 3 OPPONENT_LEFT,
// 4 cannot connect or connection lost, 5 timeout, 6 stopped by a signal
// (leaves the room first, like a player who quits), 7 won by forfeit,
// 1 bad usage. Prints "winner N score A-B" when a match ends (with
// " forfeit" after it for a forfeit), "opponent silent" the first time a
// snapshot says so, and at the end a "stats" line with the prediction
// error seen.

#define _POSIX_C_SOURCE 200809L // setenv, getpid

#include <enet/enet.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "net.h"
#include "netplay.h"

#define BOT_TIMEOUT 300000 // ms for the whole run
#define BOT_FRAME 16       // ms to wait for packets
#define BOT_INPUT_MS (1000.0 / NET_TICK_RATE) // one input per server tick, as the game sends them
#define BOT_DROP_TIMEOUT 15000 // ms the misbehaving modes wait to be dropped

static volatile sig_atomic_t stop_requested;

static void request_stop(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

// Tracks the ball with a random aim error, so points get scored; uses a
// power-up now and then. The pad is where the prediction has it.
static sim_input_t choose_input(net_t const *net, netplay_t const *netplay, uint32_t now,
                                float *aim_error, uint32_t *next_aim) {
    sim_input_t input = {0, 0};
    if (!net->has_snapshot) return input;
    if (now >= *next_aim) {
        *aim_error = (float)(rand() % 200 - 100);
        *next_aim = now + 700 + (uint32_t)(rand() % 800);
    }
    float height = netplay->has_predicted ? netplay->predicted.height : PADDLE_HEIGHT;
    float pad_y = netplay->has_predicted ? netplay_local_pad_y(netplay) :
        (net->side == 1 ? net->state.player_one_pad : net->state.player_two_pad).position.y;
    float target = net->state.ball.position.y + *aim_error - height / 2.f;
    input.move = sim_move((target - pad_y) / 20.f);
    if (rand() % 90 == 0) input.actions = SIM_ACTION_USE;
    if (rand() % 300 == 0) input.actions |= SIM_ACTION_SELECT;
    return input;
}

typedef enum { RAW_NO_HELLO, RAW_GARBAGE, RAW_FLOOD } raw_mode_t;

static void raw_send(ENetPeer *peer, uint8_t channel, bool reliable, void const *data, size_t size) {
    ENetPacket *packet = enet_packet_create(data, size, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    if (packet && enet_peer_send(peer, channel, packet) < 0) enet_packet_destroy(packet);
}

// Malformed packets of each kind the server must refuse
static void send_garbage(ENetPeer *peer) {
    uint8_t bytes[600];
    for (size_t i = 0; i < sizeof(bytes); i++) bytes[i] = (uint8_t)rand();
    net_hello_t hello = {NET_MSG_HELLO, NET_VERSION, 9, {0}, sizeof(sim_state_t)}; // bad op
    net_input_t input = {0};
    input.type = NET_MSG_INPUT;
    input.count = 200; // more than the history
    uint8_t welcome = NET_MSG_WELCOME; // a server message
    raw_send(peer, NET_CHANNEL_CONTROL, true, &hello, sizeof(hello) - 1); // short
    raw_send(peer, NET_CHANNEL_CONTROL, true, &hello, sizeof(hello));
    raw_send(peer, NET_CHANNEL_STATE, false, &hello, sizeof(hello)); // wrong channel
    raw_send(peer, NET_CHANNEL_STATE, false, &input, sizeof(input));
    raw_send(peer, NET_CHANNEL_STATE, false, &input, 3);
    raw_send(peer, NET_CHANNEL_CONTROL, true, &welcome, 1);
    raw_send(peer, NET_CHANNEL_CONTROL, true, bytes, 0);
    for (int i = 0; i < 4; i++) {
        size_t size = 1 + (size_t)(rand() % (int)sizeof(bytes));
        raw_send(peer, (uint8_t)(rand() % NET_CHANNELS), rand() % 2, bytes, size);
    }
}

// Misbehaves until the server drops the connection
static int run_raw(char const *host_name, uint16_t port, raw_mode_t mode) {
    if (enet_initialize() != 0) return 4;
    ENetHost *host = enet_host_create(NULL, 1, NET_CHANNELS, 0, 0);
    ENetAddress address;
    if (!host || enet_address_set_host(&address, host_name) != 0) return 4;
    address.port = port;
    ENetPeer *peer = enet_host_connect(host, &address, NET_CHANNELS, 0);
    if (!peer) return 4;
    uint32_t started = enet_time_get();
    uint32_t connected_at = 0;
    for (;;) {
        ENetEvent event;
        while (enet_host_service(host, &event, BOT_FRAME) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                connected_at = enet_time_get();
                fprintf(stderr, "net_bot: connected\n");
                if (mode == RAW_FLOOD) {
                    net_hello_t hello = {NET_MSG_HELLO, NET_VERSION, NET_OP_CREATE, {0}, sizeof(sim_state_t)};
                    raw_send(peer, NET_CHANNEL_CONTROL, true, &hello, sizeof(hello));
                }
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                enet_packet_destroy(event.packet);
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                if (!connected_at) {
                    fprintf(stderr, "net_bot: cannot connect\n");
                    return 4;
                }
                printf("dropped after %u ms\n", enet_time_get() - connected_at);
                enet_host_destroy(host);
                return 0;
            }
        }
        if (connected_at && mode == RAW_GARBAGE) send_garbage(peer);
        if (connected_at && mode == RAW_FLOOD) {
            net_input_t input = {0};
            input.type = NET_MSG_INPUT;
            input.count = 1;
            for (int i = 0; i < 12; i++) { // about 750 a second
                input.latest_seq = input.inputs[0].seq = (uint16_t)(input.latest_seq + 1);
                raw_send(peer, NET_CHANNEL_STATE, false, &input, sizeof(input));
            }
        }
        if (enet_time_get() - started > BOT_DROP_TIMEOUT) {
            fprintf(stderr, "net_bot: the server did not drop the connection\n");
            return 5;
        }
    }
}

int main(int argc, char **argv) {
    char host[NET_HOST_LENGTH];
    uint16_t port;
    char const *code = NULL;
    int op = 0;
    int matches_wanted = 1;

    if (argc >= 2 && strcmp(argv[1], "create") == 0) op = NET_OP_CREATE;
    if (argc >= 3 && strcmp(argv[1], "join") == 0) {
        op = NET_OP_JOIN;
        code = argv[2];
    }
    int raw = -1;
    if (argc >= 2 && strcmp(argv[1], "no-hello") == 0) raw = RAW_NO_HELLO;
    if (argc >= 2 && strcmp(argv[1], "garbage") == 0) raw = RAW_GARBAGE;
    if (argc >= 2 && strcmp(argv[1], "flood") == 0) raw = RAW_FLOOD;
    if (!op && raw < 0) {
        fprintf(stderr, "usage: net_bot create | join CODE | no-hello | garbage | flood"
            " [--server host:port] [--rematch]\n");
        return 1;
    }
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--server") == 0 && i + 1 < argc) setenv("PONG_SERVER", argv[++i], 1);
        else if (strcmp(argv[i], "--rematch") == 0) matches_wanted = 2;
    }
    net_server_address(host, sizeof(host), &port);
    srand((unsigned int)(enet_time_get() ^ (uint32_t)getpid()));
    if (raw >= 0) return run_raw(host, port, (raw_mode_t)raw);

    signal(SIGTERM, request_stop);
    signal(SIGINT, request_stop);
    net_t net = {0};
    static netplay_t netplay;
    static sim_state_t view;
    if (!net_connect(&net, host, port, op, code)) {
        fprintf(stderr, "net_bot: cannot connect to %s:%u\n", host, port);
        return 4;
    }
    uint32_t started = enet_time_get();
    double next_input = started;
    uint32_t next_aim = started;
    float aim_error = 0;
    bool announced = false;
    bool in_match = false;
    bool over_handled = false;
    int matches_done = 0;
    uint32_t last_tick = 0;
    float max_error = 0;
    int corrections = 0;
    int big_corrections = 0;
    int snapshots = 0;
    bool silent_seen = false;

    for (;;) {
        net_poll(&net, BOT_FRAME);
        uint32_t now = enet_time_get();
        double seconds = now / 1000.0;
        if (net.status == NET_FAILED) {
            fprintf(stderr, "net_bot: %s\n", net_error_text(net.error));
            net_disconnect(&net);
            return net.error >= NET_FAIL_CONNECT ? 4 : 2;
        }
        if (net.status == NET_WAITING && !announced) {
            printf("%s\n", net.code);
            fflush(stdout);
            fprintf(stderr, "net_bot: room %s, side %d\n", net.code, net.side);
            announced = true;
        }
        if (net.status == NET_PLAYING) {
            if (!in_match) {
                netplay_start(&netplay, net.side, net.countdown_ticks);
                in_match = true;
                over_handled = false;
                fprintf(stderr, "net_bot: match %d, countdown %u ticks\n", matches_done + 1, net.countdown_ticks);
            }
            net_snapshot_t snapshot;
            if (net.opponent_silent && !silent_seen) {
                silent_seen = true;
                printf("opponent silent\n");
                fflush(stdout);
            }
            while (net_take_snapshot(&net, &snapshot)) {
                netplay_receive(&netplay, &snapshot, seconds);
                snapshots++;
                if (netplay.has_predicted) {
                    corrections += netplay.last_error > 0.01f;
                    big_corrections += netplay.last_error > NETPLAY_SNAP_DISTANCE;
                    if (netplay.last_error > max_error) max_error = netplay.last_error;
                }
            }
            netplay_update(&netplay, seconds, BOT_FRAME / 1000.f);
            if (netplay_view(&netplay, &view) && netplay.has_predicted) {
                pad_t const *pad = net.side == 1 ? &view.player_one_pad : &view.player_two_pad;
                if (fabsf(pad->position.y - netplay_local_pad_y(&netplay)) > 0.001f) {
                    fprintf(stderr, "net_bot: the view does not show the predicted pad\n");
                    return 1;
                }
            }
            if (!netplay_in_countdown(&netplay) && now >= next_input) {
                next_input += BOT_INPUT_MS;
                if (now > next_input + 100) next_input = now; // far behind: no burst to catch up
                sim_input_t input = choose_input(&net, &netplay, now, &aim_error, &next_aim);
                uint16_t seq = net_send_input(&net, input);
                if (seq) netplay_input_sent(&netplay, seq, input);
                if (net.tick != last_tick && net.tick % 600 == 0) {
                    fprintf(stderr, "net_bot: tick %u score %d-%d rtt %u error %.2f\n", net.tick,
                        net.state.player_one_score, net.state.player_two_score, net_rtt(&net),
                        netplay.last_error);
                }
                last_tick = net.tick;
            }
        }
        if (net.status == NET_OVER && !over_handled) {
            over_handled = true;
            in_match = false;
            if (net.opponent_left && !net.forfeit) {
                printf("opponent left\n");
                net_disconnect(&net);
                return 3;
            }
            printf("winner %d score %d-%d%s\n", net.winner, net.player_one_score, net.player_two_score,
                net.forfeit ? " forfeit" : "");
            fflush(stdout);
            if (net.forfeit) {
                net_disconnect(&net);
                return net.winner == net.side ? 7 : 1;
            }
            matches_done++;
            if (matches_done < matches_wanted) {
                net_request_rematch(&net);
                fprintf(stderr, "net_bot: rematch requested\n");
            } else {
                printf("stats snapshots %d corrections %d big %d max_error %.2f\n", snapshots,
                    corrections, big_corrections, max_error);
                net_disconnect(&net);
                return 0;
            }
        }
        if (stop_requested) {
            fprintf(stderr, "net_bot: stopped\n");
            net_disconnect(&net);
            return 6;
        }
        if (now - started > BOT_TIMEOUT) {
            fprintf(stderr, "net_bot: timeout\n");
            net_disconnect(&net);
            return 5;
        }
    }
}
