#ifndef INPUT_H
#define INPUT_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#define INPUT_CONTROLLERS 8

typedef struct {
    SDL_GameController *handle;
    SDL_JoystickID id;
    SDL_JoystickGUID guid;
    char name[80];
    int menu_direction;
    Uint32 repeat_at;
} controller_t;

typedef struct {
    controller_t controllers[INPUT_CONTROLLERS];
    int player_device[2];
} input_t;

void input_init(input_t *);
void input_close(input_t *);
void input_device_event(input_t *, SDL_Event const *);
bool input_ready(input_t const *, bool);
void input_cycle_device(input_t *, int, int, bool);
void input_device_label(input_t const *, int, char *, size_t);
bool input_translate(input_t *, SDL_Event const *, bool, bool, SDL_Event *);
bool input_menu_repeat(input_t *, SDL_Event *);
void input_reset_menu(input_t *);
float input_movement(input_t const *, int, bool);

#endif
