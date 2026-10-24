#include "game.h"
#include <stdlib.h>
#include <time.h>

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

    game->target_score = MATCH_DEFAULT_TARGET;
    game->mode = MODE_PVP;
    srand((unsigned int)time(NULL));
    reset_match(game);
    game->current_scene = SCENE_TITLE;
    game->selected_menu_item = 0;
    game->is_running = TRUE;
    return 0;
}

void reset_match(game_t *game) {
    game->ball.width = 12;
    game->ball.height = 12;
    game->ball.position.x = GAME_WIDTH / 2 - 6;
    game->ball.position.y = GAME_HEIGHT / 2 - 6;
    game->ball.velocity.x = 1;
    game->ball.velocity.y = 1;
    game->ball.has_collided = false;

    game->player_one_pad.width = 10;
    game->player_one_pad.height = PADDLE_HEIGHT;
    game->player_one_pad.position.x = 15;
    game->player_one_pad.position.y = GAME_HEIGHT / 2 - 25;
    game->player_one_pad.velocity.x = 0;
    game->player_one_pad.velocity.y = 0;
    game->player_one_pad.goal_velocity.x = 0;
    game->player_one_pad.goal_velocity.y = 0;

    game->player_two_pad.width = 10;
    game->player_two_pad.height = PADDLE_HEIGHT;
    game->player_two_pad.position.x = GAME_WIDTH - 10 - 15;
    game->player_two_pad.position.y = GAME_HEIGHT / 2 - 25;
    game->player_two_pad.velocity.x = 0;
    game->player_two_pad.velocity.y = 0;
    game->player_two_pad.goal_velocity.x = 0;
    game->player_two_pad.goal_velocity.y = 0;

    game->player_one_score = 0;
    game->player_two_score = 0;
    reset_power_ups(game);
    reset_ai(&game->ai);
}

void terminate_game(game_t *game) {
    SDL_DestroyWindow(game->window);
    SDL_DestroyRenderer(game->renderer);
    Mix_FreeChunk(game->hit_sound);
    Mix_CloseAudio();
    SDL_Quit();
}

int run_game(game_t *game) {
    const Uint64 target_fps = 60;
    const float max_delta_time = 0.1f;
    Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 last_time, current_time;
    float delta_time;
#ifdef PONG_LOG_FPS
    Uint64 fps_last_time;
    int frame_count = 0;
#endif
    SDL_bool has_vsync = SDL_FALSE;
    SDL_RendererInfo info;

    if (init_game(game) != 0) {
        SDL_Log("Couldn't initialize game: %s", SDL_GetError());
        return 1;
    }
    if (init_game_text(game) != 0) {
        SDL_Log("Couldn't initialize game text: %s", TTF_GetError());
        return 1;
    }
    change_scene(game, SCENE_TITLE);

    if (SDL_GetRendererInfo(game->renderer, &info) == 0 &&
        (info.flags & SDL_RENDERER_PRESENTVSYNC)) {
        has_vsync = SDL_TRUE;
    }
    const Uint64 target_frame_counts = frequency / target_fps;

    current_time = SDL_GetPerformanceCounter();
#ifdef PONG_LOG_FPS
    fps_last_time = current_time;
#endif

    while (game->is_running) {
        last_time = current_time;
        current_time = SDL_GetPerformanceCounter();
        delta_time = (float)((double)(current_time - last_time) / (double)frequency);
        if (delta_time > max_delta_time) {
            delta_time = max_delta_time;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                game->is_running = FALSE;
            } else {
                scene_handle_event(game, &event);
            }
        }

        scene_update(game, delta_time);
        scene_render(game);

        Uint64 now = SDL_GetPerformanceCounter();
#ifdef PONG_LOG_FPS
        frame_count++;
        Uint64 fps_elapsed = now - fps_last_time;
        if (fps_elapsed >= frequency) {
            double fps = (double)frame_count * (double)frequency / (double)fps_elapsed;
            SDL_Log("FPS: %.2f", fps);
            frame_count = 0;
            fps_last_time = now;
        }
#endif

        if (!has_vsync) {
            Uint64 frame_elapsed = now - current_time;
            if (frame_elapsed < target_frame_counts) {
                Uint64 remaining = target_frame_counts - frame_elapsed;
                Uint32 delay_ms = (Uint32)((remaining * 1000 + frequency - 1) / frequency);
                SDL_Delay(delay_ms);
            }
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
        TTF_Quit();
        return 1;
    }
    game->menu_font = TTF_OpenFont("src/fonts/just-my-type.ttf", MENU_FONT_SIZE);
    if (game->menu_font == NULL) {
        terminate_game_text(game);
        return 1;
    }
    game->hint_font = TTF_OpenFont("src/fonts/just-my-type.ttf", HINT_FONT_SIZE);
    if (game->hint_font == NULL) {
        terminate_game_text(game);
        return 1;
    }
    return 0;
}

void render_game_text(game_t *game, TTF_Font *font, int x, int y, char const *text, SDL_Color color) {
    int font_width = 0;
    int font_height = 0;

    SDL_Surface *surface = TTF_RenderText_Solid(font, text, color);
    if (surface == NULL) {
        return;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(game->renderer, surface);
    if (texture == NULL) {
        SDL_FreeSurface(surface);
        return;
    }
    SDL_QueryTexture(texture, NULL, NULL, &font_width, &font_height);
    SDL_Rect dest = { x, y, font_width, font_height };

    SDL_RenderCopy(game->renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void render_centered_text(game_t *game, TTF_Font *font, int y, char const *text, SDL_Color color) {
    int width;
    int height;

    if (TTF_SizeText(font, text, &width, &height) != 0) {
        return;
    }

    render_game_text(game, font, (GAME_WIDTH - width) / 2, y, text, color);
}

void terminate_game_text(game_t *game) {
    if (game->hint_font != NULL) TTF_CloseFont(game->hint_font);
    if (game->menu_font != NULL) TTF_CloseFont(game->menu_font);
    if (game->font != NULL) TTF_CloseFont(game->font);
    game->hint_font = NULL;
    game->menu_font = NULL;
    game->font = NULL;
    TTF_Quit();
}

void play_sounds(game_t *game) {
    if (game->ball.has_collided == 1) {
        Mix_PlayChannel(-1, game->hit_sound, 0);
        game->ball.has_collided = 0;
    }
}
