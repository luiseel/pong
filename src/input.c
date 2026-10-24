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
    if (event->type == SDL_JOYDEVICEADDED && !SDL_IsGameController(event->jdevice.which))
        open_controller(input, event->jdevice.which);
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

static input_action_t keyboard_action(SDL_Keycode key, bool playing, bool solo) {
    if (playing) {
        switch (key) {
            case SDLK_ESCAPE: return (input_action_t){INPUT_PAUSE, 0};
            case SDLK_F1: return (input_action_t){INPUT_DEBUG, 0};
            case SDLK_a: return (input_action_t){INPUT_SELECT_POWER, 1};
            case SDLK_d: return (input_action_t){INPUT_USE_POWER, 1};
            case SDLK_SPACE: return (input_action_t){solo ? INPUT_USE_POWER : INPUT_NONE, 1};
            case SDLK_LEFT: return (input_action_t){solo ? INPUT_NONE : INPUT_USE_POWER, 2};
            case SDLK_RIGHT: return (input_action_t){solo ? INPUT_NONE : INPUT_SELECT_POWER, 2};
            default: return (input_action_t){INPUT_NONE, 0};
        }
    }
    input_action_kind_t kind = INPUT_NONE;
    switch (key) {
        case SDLK_UP: case SDLK_w: kind = INPUT_UP; break;
        case SDLK_DOWN: case SDLK_s: kind = INPUT_DOWN; break;
        case SDLK_LEFT: case SDLK_a: kind = INPUT_LEFT; break;
        case SDLK_RIGHT: case SDLK_d: kind = INPUT_RIGHT; break;
        case SDLK_RETURN: case SDLK_SPACE: kind = INPUT_CONFIRM; break;
        case SDLK_ESCAPE: kind = INPUT_BACK; break;
        case SDLK_BACKSPACE: kind = INPUT_CREDITS_BACK; break;
        case SDLK_F1: kind = INPUT_DEBUG; break;
        case SDLK_g: kind = INPUT_DEBUG_GIVE; break;
    }
    return (input_action_t){kind, 0};
}

bool input_translate(input_t *input, SDL_Event const *event, bool playing, bool solo,
    input_action_t *out) {
    *out = (input_action_t){INPUT_NONE, 0};
    if (event->type == SDL_KEYDOWN) {
        if (event->key.repeat) return false;
        *out = keyboard_action(event->key.keysym.sym, playing, solo);
        if (out->player && input->player_device[out->player - 1]) return false;
        return out->kind != INPUT_NONE;
    }
    if (event->type != SDL_CONTROLLERBUTTONDOWN) return false;
    int slot = -1;
    for (int i = 0; i < INPUT_CONTROLLERS; i++)
        if (input->controllers[i].handle && input->controllers[i].id == event->cbutton.which) slot = i;
    if (slot < 0) return false;
    int button = event->cbutton.button;
    if (playing) {
        for (int p = 0; p < (solo ? 1 : 2); p++)
            if (input->player_device[p] == slot + 1) out->player = p + 1;
        if (!out->player) return false;
        if (button == SDL_CONTROLLER_BUTTON_START) out->kind = INPUT_PAUSE;
        if (button == SDL_CONTROLLER_BUTTON_A) out->kind = INPUT_USE_POWER;
        if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) out->kind = INPUT_SELECT_POWER;
    } else {
        switch (button) {
            case SDL_CONTROLLER_BUTTON_A: out->kind = INPUT_CONFIRM; break;
            case SDL_CONTROLLER_BUTTON_B: out->kind = INPUT_BACK; break;
            case SDL_CONTROLLER_BUTTON_START: out->kind = INPUT_PAUSE; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: out->kind = INPUT_UP; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: out->kind = INPUT_DOWN; break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: out->kind = INPUT_LEFT; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: out->kind = INPUT_RIGHT; break;
        }
    }
    return out->kind != INPUT_NONE;
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

bool input_menu_repeat(input_t *input, input_action_t *out) {
    input_action_kind_t directions[] = {INPUT_NONE, INPUT_UP, INPUT_DOWN, INPUT_LEFT, INPUT_RIGHT};
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
            *out = (input_action_t){directions[direction], 0};
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
