#include "scene.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "game.h"

#define MENU_ITEM_COUNT 4
#define LOBBY_ITEM_COUNT 2
#define DEBUG_ITEM_COUNT 4

static SDL_Color const normal_color = { 235, 219, 178, 255 };
static SDL_Color const selected_color = { 251, 73, 52, 255 };
static SDL_Color const background_color = { 39, 39, 39, 255 };

static struct {
    char const *label;
    int y;
} const title_items[MENU_ITEM_COUNT] = {
    { "1 PLAYER", 180 },
    { "2 PLAYERS", 244 },
    { "ONLINE", 308 },
    { "CREDITS", 372 },
};

static struct {
    char const *label;
    int y;
} const lobby_items[LOBBY_ITEM_COUNT] = {
    { "CREATE ROOM", 210 },
    { "JOIN ROOM", 274 },
};

// Player one's device is the only one that matters outside two-player mode
static bool solo(game_t const *game) {
    return game->mode != MODE_PVP;
}

// Indexed by power_up_kind_t
static struct {
    char const *label;
    int y;
} const debug_items[DEBUG_ITEM_COUNT] = {
    { "SPEED", 110 },
    { "GROW", 160 },
    { "SERPENT", 210 },
    { "CHARGE", 260 },
};

static void render_selection_indicator(game_t *game, int y, char const *text) {
    int text_width;
    int text_height;
    int indicator_x;
    int indicator_center_y;
    int line_offset;
    int arrow_size = MENU_FONT_SIZE / 5;

    if (TTF_SizeText(game->menu_font, text, &text_width, &text_height) != 0) {
        return;
    }

    indicator_x = (GAME_WIDTH - text_width) / 2 - arrow_size - 12;
    indicator_center_y = y + text_height / 2;

    SDL_SetRenderDrawColor(
        game->renderer,
        selected_color.r,
        selected_color.g,
        selected_color.b,
        selected_color.a
    );

    for (line_offset = -1; line_offset <= 1; line_offset++) {
        SDL_RenderDrawLine(
            game->renderer,
            indicator_x,
            indicator_center_y - arrow_size + line_offset,
            indicator_x + arrow_size,
            indicator_center_y + line_offset
        );
        SDL_RenderDrawLine(
            game->renderer,
            indicator_x + arrow_size,
            indicator_center_y + line_offset,
            indicator_x,
            indicator_center_y + arrow_size + line_offset
        );
    }
}

void change_scene(game_t *game, scene_id_t next_scene) {
    if (next_scene == SCENE_PAUSE) game->paused_scene = game->current_scene;
    game->current_scene = next_scene;
    input_reset_menu(&game->input);

    switch (next_scene) {
        case SCENE_TITLE:
            net_disconnect(&game->net);
            game->selected_menu_item = 0;
            break;
        case SCENE_LOBBY:
            net_disconnect(&game->net);
            game->selected_menu_item = 0;
            game->lobby_step = LOBBY_MENU;
            memset(game->lobby_code, 0, sizeof(game->lobby_code));
            break;
        case SCENE_ONLINE:
            reset_match(game);
            netplay_start(&game->netplay, game->net.side, game->net.countdown_ticks);
            game->send_accumulator = 0;
            break;
        case SCENE_SETUP:
            game->selected_menu_item = 0;
            if (game->mode == MODE_PVP && game->input.player_device[0] &&
                game->input.player_device[0] == game->input.player_device[1])
                game->input.player_device[1] = 0;
            if (game->target_score < 1 || game->target_score > MATCH_MAX_TARGET) {
                game->target_score = MATCH_DEFAULT_TARGET;
            }
            break;
        case SCENE_COUNTDOWN:
            reset_match(game);
            game->countdown_remaining = MATCH_COUNTDOWN_SECONDS;
            break;
        case SCENE_PLAY:
            break;
        case SCENE_PAUSE:
        case SCENE_WINNER:
        case SCENE_DEBUG:
            game->selected_menu_item = 0;
            break;
        case SCENE_CREDITS:
            break;
    }
}

static void handle_title_action(game_t *game, input_action_t const *action) {
    switch (action->kind) {
        case INPUT_UP:
            game->selected_menu_item--;
            if (game->selected_menu_item < 0) {
                game->selected_menu_item = MENU_ITEM_COUNT - 1;
            }
            break;
        case INPUT_DOWN:
            game->selected_menu_item++;
            if (game->selected_menu_item >= MENU_ITEM_COUNT) {
                game->selected_menu_item = 0;
            }
            break;
        case INPUT_CONFIRM:
            if (game->selected_menu_item == 0) {
                game->mode = MODE_PVC;
                change_scene(game, SCENE_SETUP);
            } else if (game->selected_menu_item == 1) {
                game->mode = MODE_PVP;
                change_scene(game, SCENE_SETUP);
            } else if (game->selected_menu_item == 2) {
                game->mode = MODE_ONLINE;
                change_scene(game, SCENE_LOBBY);
            } else {
                change_scene(game, SCENE_CREDITS);
            }
            break;
        case INPUT_BACK:
            game->is_running = FALSE;
            break;
        default:
            break;
    }
}

static void handle_play_action(game_t *game, input_action_t const *action) {
    switch (action->kind) {
        case INPUT_PAUSE:
            change_scene(game, game->mode == MODE_ONLINE ? SCENE_TITLE : SCENE_PAUSE);
            break;
        case INPUT_DEBUG:
            if (game->debug) change_scene(game, SCENE_DEBUG);
            break;
        case INPUT_SELECT_POWER:
            game->actions[action->player - 1] |= SIM_ACTION_SELECT;
            break;
        case INPUT_USE_POWER:
            game->actions[action->player - 1] |= SIM_ACTION_USE;
            break;
        default: break;
    }
}

static void resume_game(game_t *game) {
    if (!input_ready(&game->input, solo(game))) return;
    if (game->paused_scene == SCENE_COUNTDOWN) {
        game->current_scene = SCENE_COUNTDOWN;
        input_reset_menu(&game->input);
    } else change_scene(game, SCENE_PLAY);
}

static void handle_pause_action(game_t *game, input_action_t const *action) {
    int count = game->mode == MODE_PVC ? 3 : 4;
    switch (action->kind) {
        case INPUT_UP:
            game->selected_menu_item = (game->selected_menu_item + count - 1) % count;
            break;
        case INPUT_DOWN:
            game->selected_menu_item = (game->selected_menu_item + 1) % count;
            break;
        case INPUT_LEFT:
        case INPUT_RIGHT:
            if (game->selected_menu_item >= 2)
                input_cycle_device(&game->input, game->selected_menu_item - 2,
                    action->kind == INPUT_LEFT ? -1 : 1, solo(game));
            break;
        case INPUT_CONFIRM:
            if (game->selected_menu_item == 0) resume_game(game);
            if (game->selected_menu_item == 1) change_scene(game, SCENE_TITLE);
            break;
        case INPUT_PAUSE:
        case INPUT_BACK:
            resume_game(game);
            break;
        default: break;
    }
}

static void handle_debug_action(game_t *game, input_action_t const *action) {
    switch (action->kind) {
        case INPUT_UP:
            game->selected_menu_item = (game->selected_menu_item + DEBUG_ITEM_COUNT - 1) %
                DEBUG_ITEM_COUNT;
            break;
        case INPUT_DOWN:
            game->selected_menu_item = (game->selected_menu_item + 1) % DEBUG_ITEM_COUNT;
            break;
        case INPUT_LEFT:
        case INPUT_RIGHT:
            game->debug_target_right = !game->debug_target_right;
            break;
        case INPUT_CONFIRM:
            debug_spawn_power_up(&game->sim, (power_up_kind_t)game->selected_menu_item,
                game->debug_target_right ? 2 : 1);
            change_scene(game, SCENE_PLAY);
            break;
        case INPUT_DEBUG_GIVE:
            debug_give_power_up(&game->sim, (power_up_kind_t)game->selected_menu_item,
                game->debug_target_right ? 2 : 1);
            change_scene(game, SCENE_PLAY);
            break;
        case INPUT_BACK:
        case INPUT_DEBUG:
            change_scene(game, SCENE_PLAY);
            break;
        default:
            break;
    }
}

static void handle_credits_action(game_t *game, input_action_t const *action) {
    if (action->kind == INPUT_BACK ||
        action->kind == INPUT_CREDITS_BACK) {
        change_scene(game, SCENE_TITLE);
    }
}

static void handle_setup_action(game_t *game, input_action_t const *action) {
    int count = game->mode == MODE_PVC ? 2 : 3;
    switch (action->kind) {
        case INPUT_UP:
            game->selected_menu_item = (game->selected_menu_item + count - 1) % count;
            break;
        case INPUT_DOWN:
            game->selected_menu_item = (game->selected_menu_item + 1) % count;
            break;
        case INPUT_LEFT:
        case INPUT_RIGHT: {
            int direction = action->kind == INPUT_LEFT ? -1 : 1;
            if (game->selected_menu_item == 0) {
                game->target_score += direction;
                if (game->target_score < 1) game->target_score = 1;
                if (game->target_score > MATCH_MAX_TARGET) game->target_score = MATCH_MAX_TARGET;
            } else input_cycle_device(&game->input, game->selected_menu_item - 1, direction,
                solo(game));
            break;
        }
        case INPUT_CONFIRM:
            if (input_ready(&game->input, solo(game))) change_scene(game, SCENE_COUNTDOWN);
            break;
        case INPUT_BACK:
            change_scene(game, SCENE_TITLE);
            break;
        default: break;
    }
}

// A rematch needs a finished match and an opponent who is still there
static bool online_rematch_possible(game_t const *game) {
    net_t const *net = &game->net;
    return game->mode == MODE_ONLINE && net->status == NET_OVER && net->winner && !net->opponent_left;
}

static void handle_winner_action(game_t *game, input_action_t const *action) {
    if (game->mode == MODE_ONLINE) {
        bool rematch = online_rematch_possible(game);
        switch (action->kind) {
            case INPUT_UP:
            case INPUT_DOWN:
                if (rematch) game->selected_menu_item = 1 - game->selected_menu_item;
                break;
            case INPUT_CONFIRM:
                if (rematch && game->selected_menu_item == 0) net_request_rematch(&game->net);
                else change_scene(game, SCENE_TITLE);
                break;
            case INPUT_BACK:
                change_scene(game, SCENE_TITLE);
                break;
            default:
                break;
        }
        return;
    }
    switch (action->kind) {
        case INPUT_UP:
        case INPUT_DOWN:
            game->selected_menu_item = 1 - game->selected_menu_item;
            break;
        case INPUT_CONFIRM:
            change_scene(game, game->selected_menu_item == 0 ?
                (input_ready(&game->input, solo(game)) ? SCENE_COUNTDOWN : SCENE_SETUP) : SCENE_TITLE);
            break;
        case INPUT_BACK:
            change_scene(game, SCENE_TITLE);
            break;
        default:
            break;
    }
}

static void lobby_connect(game_t *game, int op) {
    char host[NET_HOST_LENGTH];
    uint16_t port;
    net_server_address(host, sizeof(host), &port);
    net_connect(&game->net, host, port, op, game->lobby_code);
    game->lobby_step = LOBBY_CONNECTED;
}

static void handle_lobby_action(game_t *game, input_action_t const *action) {
    if (game->lobby_step == LOBBY_CONNECTED) {
        if (action->kind == INPUT_BACK) change_scene(game, SCENE_LOBBY);
        return;
    }
    if (game->lobby_step == LOBBY_CODE) {
        if (action->kind == INPUT_BACK) game->lobby_step = LOBBY_MENU;
        if (action->kind == INPUT_CONFIRM && strlen(game->lobby_code) == NET_CODE_LENGTH) {
            lobby_connect(game, NET_OP_JOIN);
        }
        return;
    }
    switch (action->kind) {
        case INPUT_UP:
        case INPUT_DOWN:
            game->selected_menu_item = 1 - game->selected_menu_item;
            break;
        case INPUT_CONFIRM:
            if (game->selected_menu_item == 0) lobby_connect(game, NET_OP_CREATE);
            else game->lobby_step = LOBBY_CODE;
            break;
        case INPUT_BACK:
            change_scene(game, SCENE_TITLE);
            break;
        default:
            break;
    }
}

// Keyboard text entry of the room code; true when the key was used
static bool lobby_type_code(game_t *game, SDL_Keycode key) {
    size_t length = strlen(game->lobby_code);
    if (key == SDLK_BACKSPACE) {
        if (length > 0) game->lobby_code[length - 1] = 0;
        return true;
    }
    bool letter = key >= SDLK_a && key <= SDLK_z;
    bool digit = key >= SDLK_0 && key <= SDLK_9;
    if (!letter && !digit) return false;
    if (length < NET_CODE_LENGTH) game->lobby_code[length] = (char)toupper((int)key);
    return true;
}

void scene_handle_event(game_t *game, SDL_Event const *event) {
    input_device_event(&game->input, event);
    if (game->current_scene == SCENE_LOBBY && game->lobby_step == LOBBY_CODE &&
        event->type == SDL_KEYDOWN && lobby_type_code(game, event->key.keysym.sym)) return;
    if (!input_ready(&game->input, solo(game)) &&
        (game->current_scene == SCENE_PLAY || game->current_scene == SCENE_COUNTDOWN ||
         game->current_scene == SCENE_DEBUG)) {
        game->sim.player_one_pad.goal_velocity.y = game->sim.player_one_pad.velocity.y = 0;
        game->sim.player_two_pad.goal_velocity.y = game->sim.player_two_pad.velocity.y = 0;
        change_scene(game, SCENE_PAUSE);
        return;
    }
    input_action_t action;
    bool playing = game->current_scene == SCENE_PLAY || game->current_scene == SCENE_ONLINE;
    if (input_translate(&game->input, event, playing, solo(game), &action)) {
        scene_handle_action(game, &action);
    }
}

void scene_handle_action(game_t *game, input_action_t const *action) {
    switch (game->current_scene) {
        case SCENE_TITLE:
            handle_title_action(game, action);
            break;
        case SCENE_SETUP:
            handle_setup_action(game, action);
            break;
        case SCENE_COUNTDOWN:
            if (action->kind == INPUT_BACK) {
                change_scene(game, SCENE_TITLE);
            } else if (action->kind == INPUT_PAUSE) change_scene(game, SCENE_PAUSE);
            break;
        case SCENE_WINNER:
            handle_winner_action(game, action);
            break;
        case SCENE_PLAY:
        case SCENE_ONLINE:
            handle_play_action(game, action);
            break;
        case SCENE_PAUSE:
            handle_pause_action(game, action);
            break;
        case SCENE_LOBBY:
            handle_lobby_action(game, action);
            break;
        case SCENE_CREDITS:
            handle_credits_action(game, action);
            break;
        case SCENE_DEBUG:
            handle_debug_action(game, action);
            break;
    }
}

// A START (the first, or a rematch) takes the player to the match
static void update_lobby(game_t *game) {
    net_poll(&game->net, 0);
    if (game->net.status == NET_PLAYING) change_scene(game, SCENE_ONLINE);
}

static double now_seconds(void) {
    return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}

// Sends the local input once per server tick, and shows the match as netplay
// sees it: the local pad predicted, the rest interpolated between snapshots
static void update_online(game_t *game, float delta_time) {
    net_t *net = &game->net;
    netplay_t *netplay = &game->netplay;
    double now = now_seconds();
    if (netplay_in_countdown(netplay)) { // the pads wait for the serve, as in the local countdown
        game->actions[0] = 0;
        game->send_accumulator = 0;
    } else {
        sim_input_t input;
        input.move = sim_move(input_movement(&game->input, 0, true));
        input.actions = game->actions[0];
        game->send_accumulator += delta_time;
        if (game->send_accumulator > 0.1f) game->send_accumulator = 0.1f;
        while (game->send_accumulator >= 1.f / NET_TICK_RATE) {
            game->send_accumulator -= 1.f / NET_TICK_RATE;
            uint16_t seq = net_send_input(net, input);
            if (seq) netplay_input_sent(netplay, seq, input);
            game->actions[0] = 0; // an action goes out once; a fast frame keeps it for the next send
            input.actions = 0;
        }
    }
    net_poll(net, 0);
    net_snapshot_t snapshot;
    while (net_take_snapshot(net, &snapshot)) netplay_receive(netplay, &snapshot, now);
    netplay_update(netplay, now, delta_time);
    netplay_view(netplay, &game->sim);
    game->sim.events = netplay_take_events(netplay);
    play_sounds(game);
    // The view is a little behind: the last point is shown before the winner screen
    if (net->status == NET_FAILED ||
        (net->status == NET_OVER && (net->opponent_left || netplay_caught_up(netplay)))) {
        change_scene(game, SCENE_WINNER);
    }
}

void scene_update(game_t *game, float delta_time) {
    if (game->current_scene == SCENE_LOBBY ||
        (game->current_scene == SCENE_WINNER && game->mode == MODE_ONLINE)) {
        update_lobby(game);
        return;
    }
    if (game->current_scene == SCENE_ONLINE) {
        update_online(game, delta_time);
        return;
    }
    if (game->current_scene == SCENE_COUNTDOWN) {
        game->countdown_remaining -= delta_time;
        if (game->countdown_remaining <= 0) {
            game->countdown_remaining = 0;
            change_scene(game, SCENE_PLAY);
        }
        return;
    }
    if (game->current_scene != SCENE_PLAY) {
        return;
    }

    sim_state_t *sim = &game->sim;
    sim_input_t inputs[2] = {{0, 0}, {0, 0}};
    inputs[0].move = sim_move(input_movement(&game->input, 0, game->mode == MODE_PVC));
    inputs[0].actions = game->actions[0];
    if (game->mode == MODE_PVC) {
        inputs[1].move = sim_move(update_ai_pad(&game->ai, &sim->player_two_pad, &sim->ball,
            &sim->power_ups, delta_time));
        int slot = choose_ai_power_up(&sim->power_ups.player_two_inventory,
            &sim->player_two_pad, &sim->ball, sim->power_ups.player_two_remaining,
            sim->power_ups.player_two_grow_remaining,
            sim->power_ups.player_two_serpent_remaining,
            sim->power_ups.player_two_charge_remaining);
        if (slot >= 0) inputs[1].actions = SIM_ACTION_PICK(slot) | SIM_ACTION_USE;
    } else {
        inputs[1].move = sim_move(input_movement(&game->input, 1, false));
        inputs[1].actions = game->actions[1];
    }
    game->actions[0] = 0;
    game->actions[1] = 0;

    sim_step(sim, inputs, delta_time);
    play_sounds(game);
    if (sim->winner) change_scene(game, SCENE_WINNER);
}

static void render_title(game_t *game) {
    render_centered_text(game, game->hint_font, 430, "ARROWS / D-PAD: SELECT  ENTER / A: OK", normal_color);
    render_centered_text(game, game->font, 70, "PONG", normal_color);
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        bool selected = game->selected_menu_item == i;
        render_centered_text(
            game, game->menu_font,
            title_items[i].y,
            title_items[i].label,
            selected ? selected_color : normal_color
        );
        if (selected) {
            render_selection_indicator(game, title_items[i].y, title_items[i].label);
        }
    }
}

static void render_play(game_t *game) {
    char *player_one_score = itos(game->sim.player_one_score);
    char *player_two_score = itos(game->sim.player_two_score);

    if (player_one_score != NULL) {
        render_game_text(
            game, game->font,
            GAME_PLAYER_ONE_SCORE_X,
            GAME_SCORE_Y,
            player_one_score,
            normal_color
        );
    }
    if (player_two_score != NULL) {
        render_game_text(
            game, game->font,
            GAME_PLAYER_TWO_SCORE_X,
            GAME_SCORE_Y,
            player_two_score,
            normal_color
        );
    }

    render_entity(&game->sim.ball, game->renderer);
    render_entity(&game->sim.player_one_pad, game->renderer);
    render_entity(&game->sim.player_two_pad, game->renderer);

    render_power_ups(game->renderer, &game->sim);

    free(player_one_score);
    free(player_two_score);
}

static void render_pause(game_t *game) {
    render_play(game);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(game->renderer, 39, 39, 39, 220);
    SDL_RenderFillRect(game->renderer, NULL);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_NONE);

    render_centered_text(game, game->font, 70, "PAUSED", normal_color);
    render_centered_text(game, game->menu_font, 210, "RESUME",
        game->selected_menu_item == 0 ? selected_color : normal_color);
    render_centered_text(game, game->menu_font, 274, "MAIN MENU",
        game->selected_menu_item == 1 ? selected_color : normal_color);
    if (game->selected_menu_item < 2) render_selection_indicator(game, game->selected_menu_item == 0 ? 210 : 274,
        game->selected_menu_item == 0 ? "RESUME" : "MAIN MENU");
    for (int p = 0; p < (game->mode == MODE_PVC ? 1 : 2); p++) {
        char label[100];
        input_device_label(&game->input, p, label, sizeof(label));
        render_centered_text(game, game->hint_font, 330 + p * 30, label,
            game->selected_menu_item == p + 2 ? selected_color : normal_color);
    }
    render_centered_text(game, game->hint_font, 410,
        input_ready(&game->input, game->mode == MODE_PVC) ? "ESC / START: RESUME" : "RECONNECT OR CHANGE DEVICE", normal_color);
    render_centered_text(game, game->hint_font, 445, "UP/DOWN: SELECT  LEFT/RIGHT: CHANGE", normal_color);
}

static void render_debug(game_t *game) {
    render_play(game);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(game->renderer, 39, 39, 39, 220);
    SDL_RenderFillRect(game->renderer, NULL);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_NONE);

    render_centered_text(game, game->font, 30, "POWER-UPS", normal_color);
    for (int i = 0; i < DEBUG_ITEM_COUNT; i++) {
        render_centered_text(game, game->menu_font, debug_items[i].y, debug_items[i].label,
            game->selected_menu_item == i ? selected_color : normal_color);
    }
    render_selection_indicator(game, debug_items[game->selected_menu_item].y,
        debug_items[game->selected_menu_item].label);
    char const *target = game->debug_target_right ?
        (game->mode == MODE_PVC ? "TARGET < CPU >" : "TARGET < PLAYER 2 >") :
        "TARGET < PLAYER 1 >";
    render_centered_text(game, game->menu_font, 330, target, normal_color);
    render_centered_text(game, game->hint_font, 390, "ENTER/SPACE: SPAWN - G: GIVE NOW", normal_color);
    render_centered_text(game, game->hint_font, 430, "ESC / B: RETURN", normal_color);
}

static void render_credits(game_t *game) {
    render_centered_text(game, game->font, 70, "CREDITS", normal_color);
    render_centered_text(game, game->menu_font, 210, "LUISEEL", normal_color);
    render_centered_text(game, game->hint_font, 340, "ESC / B: RETURN", normal_color);
}

static void render_setup(game_t *game) {
    char target[32];
    snprintf(target, sizeof(target), "FIRST TO < %d >", game->target_score);
    render_centered_text(game, game->font, 30, "MATCH SETUP", normal_color);
    render_centered_text(game, game->menu_font, 140, target,
        game->selected_menu_item == 0 ? selected_color : normal_color);
    for (int p = 0; p < (game->mode == MODE_PVC ? 1 : 2); p++) {
        char label[100];
        input_device_label(&game->input, p, label, sizeof(label));
        render_centered_text(game, game->hint_font, 230 + p * 50, label,
            game->selected_menu_item == p + 1 ? selected_color : normal_color);
    }
    render_centered_text(game, game->hint_font, 350, "UP/DOWN: SELECT  LEFT/RIGHT: CHANGE", normal_color);
    render_centered_text(game, game->hint_font, 390,
        input_ready(&game->input, game->mode == MODE_PVC) ? "ENTER / A: START" : "RECONNECT OR CHANGE DEVICE", normal_color);
    render_centered_text(game, game->hint_font, 430, "ESC / B: RETURN", normal_color);
}

// The count, then "GO!" for the last half second
static void render_countdown_banner(game_t *game, float remaining) {
    char count[8];
    remaining -= 0.5f;
    snprintf(count, sizeof(count), "%d", (int)ceilf(remaining));
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(game->renderer, 39, 39, 39, 220);
    SDL_Rect banner = {0, 170, GAME_WIDTH, 140};
    SDL_RenderFillRect(game->renderer, &banner);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_NONE);
    render_centered_text(game, game->font, 200, remaining > 0 ? count : "GO!", selected_color);
}

static void render_countdown(game_t *game) {
    render_play(game);
    render_countdown_banner(game, game->countdown_remaining);
}

static void render_online(game_t *game) {
    render_play(game);
    float remaining = netplay_countdown(&game->netplay);
    if (remaining > 0) render_countdown_banner(game, remaining);
    char const *problem = net_problem_text(&game->net);
    if (problem) render_centered_text(game, game->hint_font, GAME_HEIGHT - 56, problem, selected_color);
#ifdef PONG_DEBUG
    char stats[96];
    netplay_t const *netplay = &game->netplay;
    snprintf(stats, sizeof(stats), "RTT %u MS  SNAPSHOTS %.0f/S  ERROR %.1f PX  MAX %.1f PX",
        net_rtt(&game->net), netplay->snapshot_rate, netplay->last_error, netplay->max_error);
    render_centered_text(game, game->hint_font, GAME_HEIGHT - 28, stats, normal_color);
#endif
}

static void render_lobby(game_t *game) {
    net_t const *net = &game->net;
    char text[64];
    render_centered_text(game, game->font, 30, "ONLINE", normal_color);
    switch (game->lobby_step) {
        case LOBBY_MENU:
            for (int i = 0; i < LOBBY_ITEM_COUNT; i++) {
                bool selected = game->selected_menu_item == i;
                render_centered_text(game, game->menu_font, lobby_items[i].y, lobby_items[i].label,
                    selected ? selected_color : normal_color);
                if (selected) render_selection_indicator(game, lobby_items[i].y, lobby_items[i].label);
            }
            render_centered_text(game, game->hint_font, 430, "ENTER / A: OK  ESC / B: RETURN", normal_color);
            break;
        case LOBBY_CODE: {
            char shown[NET_CODE_LENGTH * 2 + 1];
            size_t length = strlen(game->lobby_code);
            for (int i = 0; i < NET_CODE_LENGTH; i++) {
                shown[2 * i] = (size_t)i < length ? game->lobby_code[i] : '_';
                shown[2 * i + 1] = ' ';
            }
            shown[NET_CODE_LENGTH * 2 - 1] = 0;
            render_centered_text(game, game->menu_font, 180, "ROOM CODE", normal_color);
            render_centered_text(game, game->font, 240, shown, selected_color);
            render_centered_text(game, game->hint_font, 390, "TYPE THE CODE  ENTER: JOIN", normal_color);
            render_centered_text(game, game->hint_font, 430, "ESC / B: RETURN", normal_color);
            break;
        }
        case LOBBY_CONNECTED:
            if (net->status == NET_FAILED) {
                render_centered_text(game, game->menu_font, 210, net_error_text(net->error), selected_color);
            } else if (net->status == NET_WAITING) {
                render_centered_text(game, game->menu_font, 160, "ROOM CODE", normal_color);
                render_centered_text(game, game->font, 210, net->code, selected_color);
                snprintf(text, sizeof(text), "YOU PLAY %s", net->side == 1 ? "LEFT" : "RIGHT");
                render_centered_text(game, game->hint_font, 300, text, normal_color);
                render_centered_text(game, game->menu_font, 340, "WAITING FOR OPPONENT", normal_color);
            } else {
                render_centered_text(game, game->menu_font, 210, "CONNECTING...", normal_color);
            }
            render_centered_text(game, game->hint_font, 430, "ESC / B: RETURN", normal_color);
            break;
    }
}

static void render_winner(game_t *game) {
    char score[32];
    net_t const *net = &game->net;
    bool player_one_won = game->sim.winner == 1;
    char const *headline = game->mode == MODE_PVC ?
        (player_one_won ? "YOU WIN!" : "CPU WINS!") :
        (player_one_won ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!");
    if (game->mode == MODE_ONLINE) {
        if (net->status == NET_FAILED) headline = net_error_text(net->error);
        else if (!net->winner) headline = "OPPONENT LEFT";
        else headline = net->winner == net->side ? "YOU WIN!" : "YOU LOSE!";
        if (net->winner) {
            game->sim.player_one_score = net->player_one_score;
            game->sim.player_two_score = net->player_two_score;
        }
    }
    snprintf(score, sizeof(score), "%d - %d", game->sim.player_one_score, game->sim.player_two_score);
    render_centered_text(game, game->font, 30, headline, selected_color);
    render_centered_text(game, game->font, 130, score, normal_color);
    if (game->mode == MODE_ONLINE && !online_rematch_possible(game)) {
        if (net->winner && net->opponent_left) {
            render_centered_text(game, game->menu_font, 250,
                net->forfeit ? "OPPONENT DISCONNECTED" : "OPPONENT LEFT", normal_color);
        }
        render_centered_text(game, game->menu_font, 314, "MAIN MENU", selected_color);
        render_selection_indicator(game, 314, "MAIN MENU");
        return;
    }
    if (game->mode == MODE_ONLINE && net->rematch_requested) {
        render_centered_text(game, game->menu_font, 250, "WAITING FOR OPPONENT",
            game->selected_menu_item == 0 ? selected_color : normal_color);
        render_centered_text(game, game->menu_font, 314, "MAIN MENU",
            game->selected_menu_item == 1 ? selected_color : normal_color);
        render_selection_indicator(game, game->selected_menu_item == 0 ? 250 : 314,
            game->selected_menu_item == 0 ? "WAITING FOR OPPONENT" : "MAIN MENU");
        return;
    }
    render_centered_text(game, game->menu_font, 250, "REMATCH",
        game->selected_menu_item == 0 ? selected_color : normal_color);
    render_centered_text(game, game->menu_font, 314, "MAIN MENU",
        game->selected_menu_item == 1 ? selected_color : normal_color);
    render_selection_indicator(game, game->selected_menu_item == 0 ? 250 : 314,
        game->selected_menu_item == 0 ? "REMATCH" : "MAIN MENU");
}

void scene_render(game_t *game) {
    SDL_SetRenderDrawColor(
        game->renderer,
        background_color.r,
        background_color.g,
        background_color.b,
        background_color.a
    );
    SDL_RenderClear(game->renderer);

    switch (game->current_scene) {
        case SCENE_TITLE:
            render_title(game);
            break;
        case SCENE_SETUP:
            render_setup(game);
            break;
        case SCENE_COUNTDOWN:
            render_countdown(game);
            break;
        case SCENE_WINNER:
            render_winner(game);
            break;
        case SCENE_PLAY:
            render_play(game);
            break;
        case SCENE_PAUSE:
            render_pause(game);
            break;
        case SCENE_DEBUG:
            render_debug(game);
            break;
        case SCENE_CREDITS:
            render_credits(game);
            break;
        case SCENE_LOBBY:
            render_lobby(game);
            break;
        case SCENE_ONLINE:
            render_online(game);
            break;
    }

    SDL_RenderPresent(game->renderer);
}
