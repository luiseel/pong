#include "scene.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "game.h"

#define MENU_ITEM_COUNT 3
#define DEBUG_ITEM_COUNT 4

static SDL_Color const normal_color = { 235, 219, 178, 255 };
static SDL_Color const selected_color = { 251, 73, 52, 255 };
static SDL_Color const background_color = { 39, 39, 39, 255 };

static struct {
    char const *label;
    int y;
} const title_items[MENU_ITEM_COUNT] = {
    { "1 PLAYER", 210 },
    { "2 PLAYERS", 274 },
    { "CREDITS", 338 },
};

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
    game->current_scene = next_scene;

    switch (next_scene) {
        case SCENE_TITLE:
            game->selected_menu_item = 0;
            break;
        case SCENE_SETUP:
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

static void handle_title_event(game_t *game, SDL_Event const *event) {
    if (event->type != SDL_KEYDOWN || event->key.repeat) {
        return;
    }

    switch (event->key.keysym.sym) {
        case SDLK_UP:
        case SDLK_w:
            game->selected_menu_item--;
            if (game->selected_menu_item < 0) {
                game->selected_menu_item = MENU_ITEM_COUNT - 1;
            }
            break;
        case SDLK_DOWN:
        case SDLK_s:
            game->selected_menu_item++;
            if (game->selected_menu_item >= MENU_ITEM_COUNT) {
                game->selected_menu_item = 0;
            }
            break;
        case SDLK_RETURN:
        case SDLK_SPACE:
            if (game->selected_menu_item == 0) {
                game->mode = MODE_PVC;
                change_scene(game, SCENE_SETUP);
            } else if (game->selected_menu_item == 1) {
                game->mode = MODE_PVP;
                change_scene(game, SCENE_SETUP);
            } else {
                change_scene(game, SCENE_CREDITS);
            }
            break;
        case SDLK_ESCAPE:
            game->is_running = FALSE;
            break;
        default:
            break;
    }
}

static void handle_play_event(game_t *game, SDL_Event const *event) {
    if (event->type != SDL_KEYDOWN || event->key.repeat) return;
    bool solo = game->mode == MODE_PVC;
    switch (event->key.keysym.sym) {
        case SDLK_ESCAPE:
            change_scene(game, SCENE_PAUSE);
            break;
        case SDLK_F1:
            if (game->debug) change_scene(game, SCENE_DEBUG);
            break;
        case SDLK_a:
            select_power_up(game, 1);
            break;
        case SDLK_d:
            use_power_up(game, 1);
            break;
        case SDLK_SPACE:
            if (solo) use_power_up(game, 1);
            break;
        case SDLK_LEFT:
            if (!solo) use_power_up(game, 2);
            break;
        case SDLK_RIGHT:
            if (!solo) select_power_up(game, 2);
            break;
        default:
            break;
    }
}

static void handle_pause_event(game_t *game, SDL_Event const *event) {
    if (event->type != SDL_KEYDOWN || event->key.repeat) return;
    switch (event->key.keysym.sym) {
        case SDLK_UP:
        case SDLK_DOWN:
        case SDLK_w:
        case SDLK_s:
            game->selected_menu_item = 1 - game->selected_menu_item;
            break;
        case SDLK_RETURN:
        case SDLK_SPACE:
            change_scene(game, game->selected_menu_item == 0 ? SCENE_PLAY : SCENE_TITLE);
            break;
        case SDLK_ESCAPE:
            change_scene(game, SCENE_PLAY);
            break;
        default:
            break;
    }
}

static void handle_debug_event(game_t *game, SDL_Event const *event) {
    if (event->type != SDL_KEYDOWN || event->key.repeat) return;
    switch (event->key.keysym.sym) {
        case SDLK_UP:
        case SDLK_w:
            game->selected_menu_item = (game->selected_menu_item + DEBUG_ITEM_COUNT - 1) %
                DEBUG_ITEM_COUNT;
            break;
        case SDLK_DOWN:
        case SDLK_s:
            game->selected_menu_item = (game->selected_menu_item + 1) % DEBUG_ITEM_COUNT;
            break;
        case SDLK_LEFT:
        case SDLK_RIGHT:
        case SDLK_a:
        case SDLK_d:
            game->debug_target_right = !game->debug_target_right;
            break;
        case SDLK_RETURN:
        case SDLK_SPACE:
            debug_spawn_power_up(game, (power_up_kind_t)game->selected_menu_item,
                game->debug_target_right ? 2 : 1);
            change_scene(game, SCENE_PLAY);
            break;
        case SDLK_g:
            debug_give_power_up(game, (power_up_kind_t)game->selected_menu_item,
                game->debug_target_right ? 2 : 1);
            change_scene(game, SCENE_PLAY);
            break;
        case SDLK_ESCAPE:
        case SDLK_F1:
            change_scene(game, SCENE_PLAY);
            break;
        default:
            break;
    }
}

static void handle_credits_event(game_t *game, SDL_Event const *event) {
    if (event->type != SDL_KEYDOWN) {
        return;
    }

    if (event->key.keysym.sym == SDLK_ESCAPE ||
        event->key.keysym.sym == SDLK_BACKSPACE) {
        change_scene(game, SCENE_TITLE);
    }
}

static void handle_setup_event(game_t *game, SDL_Event const *event) {
    if (event->type != SDL_KEYDOWN || event->key.repeat) return;
    switch (event->key.keysym.sym) {
        case SDLK_LEFT:
        case SDLK_a:
            if (game->target_score > 1) game->target_score--;
            break;
        case SDLK_RIGHT:
        case SDLK_d:
            if (game->target_score < MATCH_MAX_TARGET) game->target_score++;
            break;
        case SDLK_RETURN:
        case SDLK_SPACE:
            change_scene(game, SCENE_COUNTDOWN);
            break;
        case SDLK_ESCAPE:
            change_scene(game, SCENE_TITLE);
            break;
        default:
            break;
    }
}

static void handle_winner_event(game_t *game, SDL_Event const *event) {
    if (event->type != SDL_KEYDOWN || event->key.repeat) return;
    switch (event->key.keysym.sym) {
        case SDLK_UP:
        case SDLK_DOWN:
        case SDLK_w:
        case SDLK_s:
            game->selected_menu_item = 1 - game->selected_menu_item;
            break;
        case SDLK_RETURN:
        case SDLK_SPACE:
            change_scene(game, game->selected_menu_item == 0 ?
                SCENE_COUNTDOWN : SCENE_TITLE);
            break;
        case SDLK_ESCAPE:
            change_scene(game, SCENE_TITLE);
            break;
        default:
            break;
    }
}

void scene_handle_event(game_t *game, SDL_Event const *event) {
    switch (game->current_scene) {
        case SCENE_TITLE:
            handle_title_event(game, event);
            break;
        case SCENE_SETUP:
            handle_setup_event(game, event);
            break;
        case SCENE_COUNTDOWN:
            if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
                change_scene(game, SCENE_TITLE);
            }
            break;
        case SCENE_WINNER:
            handle_winner_event(game, event);
            break;
        case SCENE_PLAY:
            handle_play_event(game, event);
            break;
        case SCENE_PAUSE:
            handle_pause_event(game, event);
            break;
        case SCENE_CREDITS:
            handle_credits_event(game, event);
            break;
        case SCENE_DEBUG:
            handle_debug_event(game, event);
            break;
    }
}

static void hit_ball(game_t *game, int player_num) {
    power_up_state_t *power = &game->power_ups;
    power->ball_powered = (player_num == 1 ? power->player_one_remaining :
        power->player_two_remaining) > 0;
    set_ball_serpent(game, (player_num == 1 ? power->player_one_serpent_remaining :
        power->player_two_serpent_remaining) > 0);
    power->ball_charged = false;
    catch_charged_ball(game, player_num);
}

void scene_update(game_t *game, float delta_time) {
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

    Uint8 const *keyboard = SDL_GetKeyboardState(NULL);
    if (game->mode == MODE_PVC) {
        handle_solo_pad_keys(&game->player_one_pad, keyboard);
        update_ai_pad(&game->ai, &game->player_two_pad, &game->ball,
            &game->power_ups, delta_time);
        int slot = choose_ai_power_up(&game->power_ups.player_two_inventory,
            &game->player_two_pad, &game->ball, game->power_ups.player_two_remaining,
            game->power_ups.player_two_grow_remaining,
            game->power_ups.player_two_serpent_remaining,
            game->power_ups.player_two_charge_remaining);
        if (slot >= 0) {
            game->power_ups.player_two_inventory.selected = slot;
            use_power_up(game, 2);
        }
    } else {
        handle_pad_key_release(&game->player_one_pad, keyboard, 1);
        handle_pad_key_release(&game->player_two_pad, keyboard, 2);
        handle_pad_key_press(&game->player_one_pad, keyboard, 1);
        handle_pad_key_press(&game->player_two_pad, keyboard, 2);
    }

    int old_one_score = game->player_one_score;
    int old_two_score = game->player_two_score;
    float remaining = delta_time;
    while (remaining > 0) {
        float step = remaining > 1.f / 240.f ? 1.f / 240.f : remaining;
        remaining -= step;
        update_player_one_pad(&game->player_one_pad, step);
        update_player_two_pad(&game->player_two_pad, step);
        update_power_ups(game, step);
        if (game->power_ups.ball_held_by) {
            update_held_ball(game, step);
            continue;
        }
        // A charged shot is faster than a speed boost, so it wins
        update_ball(&game->ball, step,
            game->power_ups.ball_charged ? POWER_UP_CHARGE_MULTIPLIER :
            game->power_ups.ball_powered ? POWER_UP_BALL_MULTIPLIER : 1.f);
        weave_ball(game);
        if (check_collisions(&game->ball, &game->player_one_pad)) {
            hit_ball(game, 1);
            if (game->power_ups.ball_held_by) continue;
        }
        if (check_collisions(&game->ball, &game->player_two_pad)) {
            hit_ball(game, 2);
            if (game->power_ups.ball_held_by) continue;
        }
        check_wall_collisions(&game->ball, &game->player_one_score, &game->player_two_score);
        if (old_one_score != game->player_one_score || old_two_score != game->player_two_score) {
            game->power_ups.ball_powered = false;
            game->power_ups.ball_charged = false;
            game->power_ups.ball_held_by = 0;
            set_ball_serpent(game, false);
            game->power_ups.trail_count = 0;
            if (game->target_score > 0 &&
                (game->player_one_score >= game->target_score ||
                 game->player_two_score >= game->target_score)) {
                change_scene(game, SCENE_WINNER);
            }
            break;
        }
    }
    record_power_trail(game);
    play_sounds(game);
}

static void render_title(game_t *game) {
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
    char *player_one_score = itos(game->player_one_score);
    char *player_two_score = itos(game->player_two_score);

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

    render_entity(&game->ball, game->renderer);
    render_entity(&game->player_one_pad, game->renderer);
    render_entity(&game->player_two_pad, game->renderer);

    render_power_ups(game);

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
    render_selection_indicator(game, game->selected_menu_item == 0 ? 210 : 274,
        game->selected_menu_item == 0 ? "RESUME" : "MAIN MENU");
    render_centered_text(game, game->hint_font, 380, "ESC TO RESUME", normal_color);
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
    render_centered_text(game, game->hint_font, 430, "ESC TO RETURN", normal_color);
}

static void render_credits(game_t *game) {
    render_centered_text(game, game->font, 70, "CREDITS", normal_color);
    render_centered_text(game, game->menu_font, 210, "LUISEEL", normal_color);
    render_centered_text(game, game->hint_font, 340, "ESC TO RETURN", normal_color);
}

static void render_setup(game_t *game) {
    char target[32];
    snprintf(target, sizeof(target), "< %d >", game->target_score);
    render_centered_text(game, game->font, 30, "FIRST TO", normal_color);
    render_centered_text(game, game->font, 130, target, selected_color);
    render_centered_text(game, game->hint_font, 250, "ENTER TO START", normal_color);
    render_centered_text(game, game->hint_font, 350, "ESC TO RETURN", normal_color);
}

static void render_countdown(game_t *game) {
    char count[8];
    float remaining = game->countdown_remaining - 0.5f;
    snprintf(count, sizeof(count), "%d", (int)ceilf(remaining));
    render_play(game);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(game->renderer, 39, 39, 39, 220);
    SDL_Rect banner = {0, 170, GAME_WIDTH, 140};
    SDL_RenderFillRect(game->renderer, &banner);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_NONE);
    render_centered_text(game, game->font, 200, remaining > 0 ? count : "GO!", selected_color);
}

static void render_winner(game_t *game) {
    char score[32];
    snprintf(score, sizeof(score), "%d - %d", game->player_one_score, game->player_two_score);
    bool player_one_won = game->player_one_score >= game->target_score;
    char const *headline = game->mode == MODE_PVC ?
        (player_one_won ? "YOU WIN!" : "CPU WINS!") :
        (player_one_won ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!");
    render_centered_text(game, game->font, 30, headline, selected_color);
    render_centered_text(game, game->font, 130, score, normal_color);
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
    }

    SDL_RenderPresent(game->renderer);
}
