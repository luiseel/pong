#include "input.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void open_controller(input_t *input, int index) {
    if (!SDL_IsGameController(index)) {
        char guid[33];
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof(guid));
        SDL_Log("Unmapped controller: %s (%s). Add its mapping to gamecontrollerdb.txt.",
            SDL_JoystickNameForIndex(index), guid);
        return;
    }
    SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
    SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(index);
    int slot = -1;
    for (int i = 0; i < INPUT_CONTROLLERS; i++) {
        controller_t *c = &input->controllers[i];
        if (c->handle && c->id == id) return;
        if (!c->handle && !memcmp(&c->guid, &guid, sizeof(guid))) {
            if (slot < 0 || input->player_device[0] == i + 1 || input->player_device[1] == i + 1)
                slot = i;
        }
    }
    if (slot < 0) {
        for (int i = 0; i < INPUT_CONTROLLERS; i++) {
            if (!input->controllers[i].handle && input->player_device[0] != i + 1 &&
                input->player_device[1] != i + 1) { slot = i; break; }
        }
    }
    if (slot < 0) return;
    SDL_GameController *handle = SDL_GameControllerOpen(index);
    if (!handle) { SDL_Log("Controller open failed: %s", SDL_GetError()); return; }
    controller_t *c = &input->controllers[slot];
    *c = (controller_t){ .handle = handle, .id = id, .guid = guid };
    snprintf(c->name, sizeof(c->name), "%s", SDL_GameControllerName(handle));
    SDL_Log("Controller %d: %s", slot + 1, c->name);
}

void input_init(input_t *input) {
    memset(input, 0, sizeof(*input));
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "1");
    SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");
    for (int i = 0; i < SDL_NumJoysticks(); i++) open_controller(input, i);
}

void input_close(input_t *input) {
    for (int i = 0; i < INPUT_CONTROLLERS; i++) {
        if (input->controllers[i].handle) SDL_GameControllerClose(input->controllers[i].handle);
    }
    memset(input, 0, sizeof(*input));
}

void input_device_event(input_t *input, SDL_Event const *event) {
    if (event->type == SDL_CONTROLLERDEVICEADDED) open_controller(input, event->cdevice.which);
    if (event->type != SDL_CONTROLLERDEVICEREMOVED) return;
    for (int i = 0; i < INPUT_CONTROLLERS; i++) {
        controller_t *c = &input->controllers[i];
        if (!c->handle || c->id != event->cdevice.which) continue;
        SDL_GameControllerClose(c->handle);
        c->handle = NULL;
        c->menu_direction = 0;
    }
}

bool input_ready(input_t const *input, bool solo) {
    for (int p = 0; p < (solo ? 1 : 2); p++) {
        int device = input->player_device[p];
        if (device && (!input->controllers[device - 1].handle ||
            !SDL_GameControllerGetAttached(input->controllers[device - 1].handle))) return false;
    }
    return true;
}

void input_cycle_device(input_t *input, int player, int direction, bool solo) {
    int device = input->player_device[player];
    do {
        device = (device + direction + INPUT_CONTROLLERS + 1) % (INPUT_CONTROLLERS + 1);
    } while (device && (!input->controllers[device - 1].handle ||
        (!solo && input->player_device[1 - player] == device)));
    input->player_device[player] = device;
}

void input_device_label(input_t const *input, int player, char *text, size_t size) {
    int device = input->player_device[player];
    if (!device) snprintf(text, size, "P%d: < KEYBOARD >", player + 1);
    else {
        controller_t const *c = &input->controllers[device - 1];
        snprintf(text, size, "P%d: < %d %.22s%s >", player + 1, device,
            c->name, c->handle ? "" : " OFFLINE");
    }
}

static void key_event(SDL_Event *event, SDL_Keycode key) {
    *event = (SDL_Event){0};
    event->type = SDL_KEYDOWN;
    event->key.keysym.sym = key;
}

bool input_translate(input_t *input, SDL_Event const *event, bool playing, bool solo,
    SDL_Event *out) {
    if (event->type == SDL_KEYDOWN) {
        *out = *event;
        if (!playing) return true;
        SDL_Keycode key = event->key.keysym.sym;
        if ((key == SDLK_a || key == SDLK_d || (solo && key == SDLK_SPACE)) &&
            input->player_device[0]) return false;
        if (!solo && (key == SDLK_LEFT || key == SDLK_RIGHT) && input->player_device[1]) return false;
        return true;
    }
    if (event->type != SDL_CONTROLLERBUTTONDOWN) return false;
    int slot = -1;
    for (int i = 0; i < INPUT_CONTROLLERS; i++)
        if (input->controllers[i].handle && input->controllers[i].id == event->cbutton.which) slot = i;
    if (slot < 0) return false;
    int button = event->cbutton.button;
    SDL_Keycode key = 0;
    if (playing) {
        int player = -1;
        for (int p = 0; p < (solo ? 1 : 2); p++)
            if (input->player_device[p] == slot + 1) player = p;
        if (player < 0) return false;
        if (button == SDL_CONTROLLER_BUTTON_START) key = SDLK_ESCAPE;
        if (button == SDL_CONTROLLER_BUTTON_A) key = player == 0 ? SDLK_d : SDLK_LEFT;
        if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) key = player == 0 ? SDLK_a : SDLK_RIGHT;
    } else {
        switch (button) {
            case SDL_CONTROLLER_BUTTON_A: key = SDLK_RETURN; break;
            case SDL_CONTROLLER_BUTTON_B: key = SDLK_ESCAPE; break;
            case SDL_CONTROLLER_BUTTON_START: key = SDLK_ESCAPE; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: key = SDLK_UP; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: key = SDLK_DOWN; break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: key = SDLK_LEFT; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: key = SDLK_RIGHT; break;
        }
    }
    if (!key) return false;
    key_event(out, key);
    return true;
}

static int stick_direction(SDL_GameController *handle) {
    int x = SDL_GameControllerGetAxis(handle, SDL_CONTROLLER_AXIS_LEFTX);
    int y = SDL_GameControllerGetAxis(handle, SDL_CONTROLLER_AXIS_LEFTY);
    if (abs(x) > abs(y)) return x > 16000 ? 4 : x < -16000 ? 3 : 0;
    return y > 16000 ? 2 : y < -16000 ? 1 : 0;
}

void input_reset_menu(input_t *input) {
    for (int i = 0; i < INPUT_CONTROLLERS; i++) {
        controller_t *c = &input->controllers[i];
        c->menu_direction = c->handle ? stick_direction(c->handle) : 0;
        c->repeat_at = SDL_GetTicks() + 350;
    }
}

bool input_menu_repeat(input_t *input, SDL_Event *out) {
    SDL_Keycode keys[] = {0, SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT};
    Uint32 now = SDL_GetTicks();
    for (int i = 0; i < INPUT_CONTROLLERS; i++) {
        controller_t *c = &input->controllers[i];
        if (!c->handle) continue;
        int direction = stick_direction(c->handle);
        bool changed = direction != c->menu_direction;
        c->menu_direction = direction;
        if (!direction) continue;
        if (changed || SDL_TICKS_PASSED(now, c->repeat_at)) {
            c->repeat_at = now + (changed ? 350 : 140);
            key_event(out, keys[direction]);
            return true;
        }
    }
    return false;
}

float input_movement(input_t const *input, int player, bool solo) {
    int device = input->player_device[player];
    if (!device) {
        Uint8 const *keys = SDL_GetKeyboardState(NULL);
        bool up = player == 0 ? keys[SDL_SCANCODE_W] : keys[SDL_SCANCODE_UP];
        bool down = player == 0 ? keys[SDL_SCANCODE_S] : keys[SDL_SCANCODE_DOWN];
        if (solo) { up |= keys[SDL_SCANCODE_UP]; down |= keys[SDL_SCANCODE_DOWN]; }
        return up ? -1.f : down ? 1.f : 0.f;
    }
    SDL_GameController *handle = input->controllers[device - 1].handle;
    if (!handle || !SDL_GameControllerGetAttached(handle)) return 0;
    bool up = SDL_GameControllerGetButton(handle, SDL_CONTROLLER_BUTTON_DPAD_UP);
    bool down = SDL_GameControllerGetButton(handle, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    if (up || down) return (float)down - (float)up;
    int raw = SDL_GameControllerGetAxis(handle, SDL_CONTROLLER_AXIS_LEFTY);
    float axis = raw < 0 ? raw / 32768.f : raw / 32767.f;
    if (fabsf(axis) <= 0.2f) return 0;
    return copysignf((fabsf(axis) - 0.2f) / 0.8f, axis);
}
