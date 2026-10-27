#ifndef GAME_H
#define GAME_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdint.h>
#include "sim.h"
#include "render.h"
#include "input.h"
#include "ai.h"
#include "net.h"
#include "netplay.h"
#include "game_settings.h"
#include "scene.h"
#include "utils.h"

typedef enum { MODE_PVP, MODE_PVC, MODE_ONLINE } game_mode_t;

// Steps of the lobby scene before the connection takes over
typedef enum { LOBBY_MENU, LOBBY_CODE, LOBBY_CONNECTED } lobby_step_t;

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
    sim_state_t sim;
    uint8_t actions[2];      // SIM_ACTION_* bits from events, for the next sim_step
    game_mode_t mode;
    ai_state_t ai;
    net_t net;
    netplay_t netplay;
    lobby_step_t lobby_step;
    char lobby_code[NET_CODE_LENGTH + 1]; // the code typed to join
    float send_accumulator;               // time since the last input sent online
    bool debug;              // make debug: F1 opens the debug menu during play
    bool debug_target_right; // debug pickups drift toward player two
    TTF_Font *font;
    TTF_Font *menu_font;
    TTF_Font *hint_font;
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
