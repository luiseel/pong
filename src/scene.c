#include "scene.h"

#include <stdlib.h>

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

    if (TTF_SizeText(game->font, text, &text_width, &text_height) != 0) {
        return;
    }

    indicator_x = (GAME_WIDTH - text_width) / 2 - 28;
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
            indicator_center_y - 12 + line_offset,
            indicator_x + 12,
            indicator_center_y + line_offset
        );
        SDL_RenderDrawLine(
            game->renderer,
            indicator_x + 12,
            indicator_center_y + line_offset,
            indicator_x,
            indicator_center_y + 12 + line_offset
        );
    }
}

void change_scene(game_t *game, scene_id_t next_scene) {
    game->current_scene = next_scene;

    switch (next_scene) {
        case SCENE_TITLE:
            game->selected_menu_item = 0;
            break;
        case SCENE_PLAY:
            reset_match(game);
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
                game->selected_menu_item == 0 ? SCENE_PLAY : SCENE_CREDITS
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
    Uint8 const *keyboard = SDL_GetKeyboardState(NULL);

    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
        game->player_one_pad.goal_velocity.y = 0;
        game->player_two_pad.goal_velocity.y = 0;
        change_scene(game, SCENE_TITLE);
        return;
    }

    if (event->type == SDL_KEYDOWN) {
        handle_pad_key_press(&game->player_one_pad, keyboard, 1);
        handle_pad_key_press(&game->player_two_pad, keyboard, 2);
    } else if (event->type == SDL_KEYUP) {
        handle_pad_key_release(&game->player_one_pad, keyboard, 1);
        handle_pad_key_release(&game->player_two_pad, keyboard, 2);
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

void scene_handle_event(game_t *game, SDL_Event const *event) {
    switch (game->current_scene) {
        case SCENE_TITLE:
            handle_title_event(game, event);
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
    if (game->current_scene != SCENE_PLAY) {
        return;
    }

    update_ball(&game->ball, delta_time);
    update_player_one_pad(&game->player_one_pad, delta_time);
    update_player_two_pad(&game->player_two_pad, delta_time);
    check_collisions(&game->ball, &game->player_one_pad);
    check_collisions(&game->ball, &game->player_two_pad);
    check_wall_collisions(
        &game->ball,
        &game->player_one_score,
        &game->player_two_score
    );
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

    render_centered_text(game, 70, "PONG", normal_color);
    render_centered_text(
        game,
        210,
        "PLAY",
        play_color
    );
    render_centered_text(
        game,
        290,
        "CREDITS",
        credits_color
    );

    if (game->selected_menu_item == 0) {
        render_selection_indicator(game, 210, "PLAY");
    } else {
        render_selection_indicator(game, 290, "CREDITS");
    }
}

static void render_play(game_t *game) {
    char *player_one_score = itos(game->player_one_score);
    char *player_two_score = itos(game->player_two_score);

    if (player_one_score != NULL) {
        render_game_text(
            game,
            GAME_PLAYER_ONE_SCORE_X,
            GAME_SCORE_Y,
            player_one_score,
            normal_color
        );
    }
    if (player_two_score != NULL) {
        render_game_text(
            game,
            GAME_PLAYER_TWO_SCORE_X,
            GAME_SCORE_Y,
            player_two_score,
            normal_color
        );
    }

    render_entity(&game->ball, game->renderer);
    render_entity(&game->player_one_pad, game->renderer);
    render_entity(&game->player_two_pad, game->renderer);

    free(player_one_score);
    free(player_two_score);
}

static void render_credits(game_t *game) {
    render_centered_text(game, 70, "CREDITS", normal_color);
    render_centered_text(game, 210, "LUISEEL", normal_color);
    render_centered_text(game, 340, "ESC TO RETURN", normal_color);
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
        case SCENE_PLAY:
            render_play(game);
            break;
        case SCENE_CREDITS:
            render_credits(game);
            break;
    }

    SDL_RenderPresent(game->renderer);
}
