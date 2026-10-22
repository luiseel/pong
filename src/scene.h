#ifndef SCENE_H
#define SCENE_H

#include <SDL2/SDL.h>

struct game;
typedef struct game game_t;

typedef enum {
    SCENE_TITLE,
    SCENE_PLAY,
    SCENE_CREDITS
} scene_id_t;

void change_scene(game_t *, scene_id_t);
void scene_handle_event(game_t *, SDL_Event const *);
void scene_update(game_t *, float);
void scene_render(game_t *);

#endif
