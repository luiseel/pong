#include "game.h"

int init_game(game_t *game) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        return 1;
    }

    game->window = SDL_CreateWindow(
        GAME_TITLE,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        GAME_WIDTH,
        GAME_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (game->window == NULL) {
        return 1;
    }
    game->renderer = SDL_CreateRenderer(game->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (game->renderer == NULL) {
        return 1;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) != 0) {
        return 1;    
    }
    game->hit_sound = Mix_LoadWAV("src/assets/hit.wav");
    if (game->hit_sound == NULL) {
        return 1;
    }

    game->ball.width = 12;
    game->ball.height = 12;
    game->ball.position.x = GAME_WIDTH / 2 - 6;
    game->ball.position.y = GAME_HEIGHT / 2 - 6;
    game->ball.velocity.x = 1;
    game->ball.velocity.y = 1;

    game->player_one_pad.width = 10;
    game->player_one_pad.height = 50;
    game->player_one_pad.position.x = 15;
    game->player_one_pad.position.y = GAME_HEIGHT / 2 - 25;
    game->player_one_pad.velocity.x = 0;
    game->player_one_pad.velocity.y = 0;
    game->player_one_pad.goal_velocity.x = 0;
    game->player_one_pad.goal_velocity.y = 0;

    game->player_two_pad.width = 10;
    game->player_two_pad.height = 50;
    game->player_two_pad.position.x = GAME_WIDTH - 10 - 15;
    game->player_two_pad.position.y = GAME_HEIGHT / 2 - 25;
    game->player_two_pad.velocity.x = 0;
    game->player_two_pad.velocity.y = 0;
    game->player_two_pad.goal_velocity.x = 0;
    game->player_two_pad.goal_velocity.y = 0;

    game->player_one_score = 0;
    game->player_two_score = 0;
    game->is_running = TRUE;
    return 0;
}

void terminate_game(game_t *game) {
    SDL_DestroyWindow(game->window);
    SDL_DestroyRenderer(game->renderer);
    Mix_FreeChunk(game->hit_sound);
    Mix_CloseAudio();
    SDL_Quit();
}

void handle_game_events(game_t *game) {
    SDL_Event event;
    Uint8 const *state = SDL_GetKeyboardState(NULL);
    if (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                game->is_running = FALSE;
                break;
            case SDL_KEYDOWN:
                handle_pad_key_press(&game->player_one_pad, state);
                break;
            case SDL_KEYUP:
                handle_pad_key_release(&game->player_one_pad, state);
                break;
            default:
                break;
        }
    }
}

void update_game(game_t *game, float delta_time) {
    update_ball(&game->ball, delta_time);
    update_player_one_pad(&game->player_one_pad, delta_time);
    update_player_two_pad(&game->player_two_pad, &game->ball, delta_time);
    check_collisions(&game->ball, &game->player_one_pad);
    check_collisions(&game->ball, &game->player_two_pad);

    if (game->ball.position.x < 0) {
        game->player_two_score++;
    }
    if (game->ball.position.x + game->ball.width > GAME_WIDTH) {
        game->player_one_score++;
    }
}

void render_game(game_t *game) {
    char *player_one_score = itos(game->player_one_score);
    char *player_two_score = itos(game->player_two_score);

    SDL_SetRenderDrawColor(game->renderer, 39, 39, 39, 0);
    SDL_RenderClear(game->renderer);

    render_game_text(game, GAME_PLAYER_ONE_SCORE_X, GAME_SCORE_Y, player_one_score);
    render_game_text(game, GAME_PLAYER_TWO_SCORE_X, GAME_SCORE_Y, player_two_score);

    render_entity(&game->ball, game->renderer);
    render_entity(&game->player_one_pad, game->renderer);
    render_entity(&game->player_two_pad, game->renderer);

    free(player_one_score);
    free(player_two_score);

    SDL_RenderPresent(game->renderer);
}

int run_game(game_t *game) {
    const int target_fps = 60;
    const float target_frame_time = 1000.0f / target_fps;
    Uint32 last_time, fps_last_time = SDL_GetTicks();
    Uint32 current_time = SDL_GetTicks();
    float delta_time;
    int frame_count = 0;
    float fps = 0;

    if (init_game(game) != 0) {
        SDL_Log("Couldn't initialize game: %s", SDL_GetError());
        return 1;
    }
    if (init_game_text(game) != 0) {
        SDL_Log("Couldn't initialize game text: %s", TTF_GetError());
        return 1;
    }

    while (game->is_running) {
        last_time = current_time;
        current_time = SDL_GetTicks();
        delta_time = (current_time - last_time) / 1000.0f;

        if (delta_time > target_frame_time) {
            delta_time = target_frame_time;
        }

        handle_game_events(game);
        update_game(game, delta_time);
        play_sounds(game);
        render_game(game);

        frame_count++;
        Uint32 elapsed_time = SDL_GetTicks() - fps_last_time;
        if (elapsed_time >= 1000) {
            fps = (float) frame_count / elapsed_time * 1000.0f;
            SDL_Log("FPS: %.2f", fps);
            frame_count = 0;
            fps_last_time = SDL_GetTicks();
        }

        Uint32 elapsed_time2 = SDL_GetTicks() - current_time;
        if (elapsed_time2 < target_frame_time) {
            SDL_Delay((Uint32)(target_frame_time - elapsed_time2));
        }
    }

    terminate_game_text(game);
    terminate_game(game);
    return 0;
}

int init_game_text(game_t *game) {
    if (TTF_Init() != 0) {
        return 1;
    }
    game->font = TTF_OpenFont("src/fonts/just-my-type.ttf", GAME_FONT_SIZE);
    if (game->font == NULL) {
        return 1;
    }
    return 0;
}

void render_game_text(game_t *game, float x, float y, char *text) {
    int font_width = 0;
    int font_height = 0;

    SDL_Surface *surface = TTF_RenderText_Solid(game->font, text, base_color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(game->renderer, surface);
    SDL_QueryTexture(texture, NULL, NULL, &font_width, &font_height);
    SDL_Rect dest = { x, y, font_width, font_height };

    SDL_RenderCopy(game->renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void terminate_game_text(game_t *game) {
    TTF_CloseFont(game->font);
    TTF_Quit();
}

void play_sounds(game_t *game) {
    if (game->ball.has_collided == 1) {
        Mix_PlayChannel(-1, game->hit_sound, 0);
        game->ball.has_collided = 0;
    }
}
