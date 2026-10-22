#ifndef GAME_H
#define GAME_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include "entity.h"
#include "game_settings.h"
#include "scene.h"
#include "utils.h"

struct game {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int is_running;
    scene_id_t current_scene;
    int selected_menu_item;
    ball_t ball;
    pad_t player_one_pad;
    pad_t player_two_pad;
    TTF_Font *font;
    int player_one_score;
    int player_two_score; 
    Mix_Chunk *hit_sound;
};

int init_game(game_t *);
void terminate_game(game_t *);
void reset_match(game_t *);
void play_sounds(game_t *);
int run_game(game_t *);

int init_game_text(game_t *);
void render_game_text(game_t *, int, int, char const *, SDL_Color);
void render_centered_text(game_t *, int, char const *, SDL_Color);
void terminate_game_text(game_t *);

#endif
