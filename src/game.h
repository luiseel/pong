#ifndef GAME_H
#define GAME_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include "entity.h"
#include "input.h"
#include "ai.h"
#include "game_settings.h"
#include "scene.h"
#include "power_up.h"
#include "utils.h"

typedef enum { MODE_PVP, MODE_PVC } game_mode_t;

struct game {
    input_t input;
    scene_id_t paused_scene;
    SDL_Window *window;
    SDL_Renderer *renderer;
    int is_running;
    scene_id_t current_scene;
    int selected_menu_item;
    int target_score;
    float countdown_remaining;
    power_up_state_t power_ups;
    game_mode_t mode;
    ai_state_t ai;
    bool debug;              // make debug: F1 opens the debug menu during play
    bool debug_target_right; // debug pickups drift toward player two
    ball_t ball;
    pad_t player_one_pad;
    pad_t player_two_pad;
    TTF_Font *font;
    TTF_Font *menu_font;
    TTF_Font *hint_font;
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
void render_game_text(game_t *, TTF_Font *, int, int, char const *, SDL_Color);
void render_centered_text(game_t *, TTF_Font *, int, char const *, SDL_Color);
void terminate_game_text(game_t *);

#endif
