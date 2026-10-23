#include "scene.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "game.h"

#define MENU_ITEM_COUNT 2

static SDL_Color const normal_color = { 235, 219, 178, 255 };
static SDL_Color const selected_color = { 251, 73, 52, 255 };
static SDL_Color const background_color = { 39, 39, 39, 255 };

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
        case SCENE_WINNER:
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
            change_scene(
                game,
                game->selected_menu_item == 0 ? SCENE_SETUP : SCENE_CREDITS
            );
            break;
        case SDLK_ESCAPE:
            game->is_running = FALSE;
            break;
        default:
            break;
    }
}

static void handle_play_event(game_t *game, SDL_Event const *event) {
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
        game->player_one_pad.goal_velocity.y = 0;
        game->player_two_pad.goal_velocity.y = 0;
        change_scene(game, SCENE_TITLE);
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
        case SCENE_CREDITS:
            handle_credits_event(game, event);
            break;
    }
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
    handle_pad_key_release(&game->player_one_pad, keyboard, 1);
    handle_pad_key_release(&game->player_two_pad, keyboard, 2);
    handle_pad_key_press(&game->player_one_pad, keyboard, 1);
    handle_pad_key_press(&game->player_two_pad, keyboard, 2);

    int old_one_score = game->player_one_score;
    int old_two_score = game->player_two_score;
    float remaining = delta_time;
    while (remaining > 0) {
        float step = remaining > 1.f / 240.f ? 1.f / 240.f : remaining;
        remaining -= step;
        update_player_one_pad(&game->player_one_pad, step);
        update_player_two_pad(&game->player_two_pad, step);
        update_power_ups(game, step);
        update_ball(&game->ball, step,
            game->power_ups.ball_powered ? POWER_UP_BALL_MULTIPLIER : 1.f);
        if (check_collisions(&game->ball, &game->player_one_pad)) {
            game->power_ups.ball_powered = game->power_ups.player_one_remaining > 0;
        }
        if (check_collisions(&game->ball, &game->player_two_pad)) {
            game->power_ups.ball_powered = game->power_ups.player_two_remaining > 0;
        }
        check_wall_collisions(&game->ball, &game->player_one_score, &game->player_two_score);
        if (old_one_score != game->player_one_score || old_two_score != game->player_two_score) {
            game->power_ups.ball_powered = false;
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
    SDL_Color play_color = normal_color;
    SDL_Color credits_color = normal_color;

    if (game->selected_menu_item == 0) {
        play_color = selected_color;
    } else {
        credits_color = selected_color;
    }

    render_centered_text(game, game->font, 70, "PONG", normal_color);
    render_centered_text(
        game, game->menu_font,
        210,
        "PLAY",
        play_color
    );
    render_centered_text(
        game, game->menu_font,
        274,
        "CREDITS",
        credits_color
    );

    if (game->selected_menu_item == 0) {
        render_selection_indicator(game, 210, "PLAY");
    } else {
        render_selection_indicator(game, 274, "CREDITS");
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
    render_centered_text(game, game->font, 30,
        game->player_one_score >= game->target_score ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!",
        selected_color);
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
        case SCENE_CREDITS:
            render_credits(game);
            break;
    }

    SDL_RenderPresent(game->renderer);
}
