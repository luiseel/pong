#include "game.h"
#include <stdlib.h>
#include <time.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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

typedef struct {
    game_t *game;
    Uint64 frequency;
    Uint64 current_time;
    Uint64 next_frame_time;
    Uint64 target_frame_counts;
    SDL_bool has_vsync;
#ifdef PONG_LOG_FPS
    Uint64 fps_last_time;
    int frame_count;
#endif
} frame_context_t;

static void shutdown_game(game_t *game) {
    terminate_game_text(game);
    terminate_game(game);
}

static void game_frame(void *arg) {
    const float max_delta_time = 0.1f;
    frame_context_t *ctx = arg;
    game_t *game = ctx->game;
    Uint64 last_time = ctx->current_time;
    float delta_time;

    ctx->current_time = SDL_GetPerformanceCounter();
    delta_time = (float)((double)(ctx->current_time - last_time) / (double)ctx->frequency);
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

#ifdef PONG_LOG_FPS
    Uint64 fps_now = SDL_GetPerformanceCounter();
    ctx->frame_count++;
    Uint64 fps_elapsed = fps_now - ctx->fps_last_time;
    if (fps_elapsed >= ctx->frequency) {
        double fps = (double)ctx->frame_count * (double)ctx->frequency / (double)fps_elapsed;
        SDL_Log("FPS: %.2f", fps);
        ctx->frame_count = 0;
        ctx->fps_last_time = fps_now;
    }
#endif

#ifdef __EMSCRIPTEN__
    /* The browser schedules frames, so there is no frame limiter here. */
    if (!game->is_running) {
        emscripten_cancel_main_loop();
        shutdown_game(game);
    }
#else
    if (!ctx->has_vsync) {
        /* Sleep to a fixed schedule so sleep overshoot does not accumulate. */
        Uint64 now = SDL_GetPerformanceCounter();
        if (now < ctx->next_frame_time) {
            Uint64 remaining = ctx->next_frame_time - now;
            Uint32 delay_ms = (Uint32)((remaining * 1000 + ctx->frequency - 1) / ctx->frequency);
            SDL_Delay(delay_ms);
        }
        ctx->next_frame_time += ctx->target_frame_counts;
        if (ctx->next_frame_time < now) {
            /* More than a frame behind: restart the schedule from now. */
            ctx->next_frame_time = now + ctx->target_frame_counts;
        }
    }
#endif
}

int run_game(game_t *game) {
    const Uint64 target_fps = 60;
    /* Static so it stays valid after run_game unwinds under Emscripten. */
    static frame_context_t ctx;
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

    ctx.game = game;
    ctx.frequency = SDL_GetPerformanceFrequency();
    ctx.has_vsync = SDL_FALSE;
    if (SDL_GetRendererInfo(game->renderer, &info) == 0 &&
        (info.flags & SDL_RENDERER_PRESENTVSYNC)) {
        ctx.has_vsync = SDL_TRUE;
    }
    ctx.target_frame_counts = ctx.frequency / target_fps;
    ctx.current_time = SDL_GetPerformanceCounter();
    ctx.next_frame_time = ctx.current_time + ctx.target_frame_counts;
#ifdef PONG_LOG_FPS
    ctx.fps_last_time = ctx.current_time;
    ctx.frame_count = 0;
#endif

#ifdef __EMSCRIPTEN__
    /* fps 0 uses requestAnimationFrame; this call does not return. */
    emscripten_set_main_loop_arg(game_frame, &ctx, 0, 1);
#else
    while (game->is_running) {
        game_frame(&ctx);
    }
    shutdown_game(game);
#endif
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
