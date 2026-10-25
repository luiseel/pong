#ifndef RENDER_H
#define RENDER_H

#include <SDL2/SDL.h>
#include "sim.h"

void render_entity(entity_t const *, SDL_Renderer *);
void render_power_ups(SDL_Renderer *, sim_state_t const *);

#endif
