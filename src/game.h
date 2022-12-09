#ifndef GAME_H
#define GAME_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "entity.h"
#include "game_settings.h"
#include "utils.h"

static SDL_Color const base_color = { 235, 219, 178 };

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int is_running;
    ball_t ball;
    pad_t player_one_pad;
    pad_t player_two_pad;
    TTF_Font *font;
    int player_one_score;
    int player_two_score; 
} game_t;

int init_game(game_t *);
void terminate_game(game_t *);
void handle_game_events(game_t *);
void update_game(game_t *, float);
void render_game(game_t *);
int run_game(game_t *);

int init_game_text(game_t *);
void render_game_text(game_t *, float, float, char *);
void terminate_game_text(game_t *);

#endif
