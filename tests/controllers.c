#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void pump(game_t *game) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) scene_handle_event(game, &event);
}

static void key(game_t *game, SDL_Keycode code) {
    SDL_Event event = {0};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = code;
    scene_handle_event(game, &event);
}

static int attach(char const *name, int product) {
    SDL_VirtualJoystickDesc desc = {0};
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    desc.vendor_id = 0x1209;
    desc.product_id = product;
    desc.name = name;
    int index = SDL_JoystickAttachVirtualEx(&desc);
    assert(index >= 0);
    return index;
}

static void button(game_t *game, SDL_Joystick *stick, int code) {
    assert(SDL_JoystickSetVirtualButton(stick, code, 1) == 0);
    pump(game);
    assert(SDL_JoystickSetVirtualButton(stick, code, 0) == 0);
    pump(game);
}

static void capture_unmapped(void *userdata, int category, SDL_LogPriority priority, char const *message) {
    (void)category;
    (void)priority;
    if (strstr(message, "Unmapped controller: Pong unmapped") && strstr(message, "gamecontrollerdb.txt"))
        (*(int *)userdata)++;
}

static void check_unmapped_hotplug(game_t *game) {
    SDL_LogOutputFunction previous;
    void *previous_data;
    SDL_LogGetOutputFunction(&previous, &previous_data);
    int diagnostics = 0;
    SDL_LogSetOutputFunction(capture_unmapped, &diagnostics);
    SDL_VirtualJoystickDesc desc = {0};
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_FLIGHT_STICK;
    desc.naxes = 2;
    desc.nbuttons = 2;
    desc.vendor_id = 0x1209;
    desc.product_id = 3;
    desc.name = "Pong unmapped";
    int index = SDL_JoystickAttachVirtualEx(&desc);
    assert(index >= 0 && !SDL_IsGameController(index));
    pump(game);
    assert(diagnostics == 1);
    assert(SDL_JoystickDetachVirtual(index) == 0);
    pump(game);
    SDL_LogSetOutputFunction(previous, previous_data);
}

int main(void) {
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT, "0x1209/0x0001,0x1209/0x0002,0x1209/0x0003");
    assert(SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) == 0);
    assert(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == 0);
    game_t game = {0};
    input_init(&game.input);
    check_unmapped_hotplug(&game);
    game.mode = MODE_PVP;
    change_scene(&game, SCENE_TITLE);
    int first = attach("Pong test one", 1);
    int second = attach("Pong test two", 2);
    SDL_Joystick *one = SDL_JoystickOpen(first);
    SDL_Joystick *two = SDL_JoystickOpen(second);
    assert(one && two);
    pump(&game);
    button(&game, one, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    button(&game, one, SDL_CONTROLLER_BUTTON_A);
    assert(game.current_scene == SCENE_SETUP && game.mode == MODE_PVP);
    input_action_t menu_event;
    assert(SDL_JoystickSetVirtualAxis(one, SDL_CONTROLLER_AXIS_LEFTY, 22000) == 0);
    pump(&game);
    assert(input_menu_repeat(&game.input, &menu_event));
    scene_handle_action(&game, &menu_event);
    assert(game.selected_menu_item == 1);
    assert(!input_menu_repeat(&game.input, &menu_event));
    assert(SDL_JoystickSetVirtualAxis(one, SDL_CONTROLLER_AXIS_LEFTY, 0) == 0);
    pump(&game);
    assert(!input_menu_repeat(&game.input, &menu_event));
    key(&game, SDLK_UP);
    key(&game, SDLK_DOWN);
    key(&game, SDLK_RIGHT);
    key(&game, SDLK_DOWN);
    key(&game, SDLK_RIGHT);
    assert(game.input.player_device[0] && game.input.player_device[1]);
    assert(game.input.player_device[0] != game.input.player_device[1]);
    button(&game, one, SDL_CONTROLLER_BUTTON_A);
    assert(game.current_scene == SCENE_COUNTDOWN);
    scene_update(&game, MATCH_COUNTDOWN_SECONDS);
    assert(game.current_scene == SCENE_PLAY);
    assert(SDL_JoystickSetVirtualAxis(one, SDL_CONTROLLER_AXIS_LEFTY, 4000) == 0);
    assert(SDL_JoystickSetVirtualButton(two, SDL_CONTROLLER_BUTTON_DPAD_DOWN, 1) == 0);
    pump(&game);
    scene_update(&game, 0.01f);
    assert(game.sim.player_one_pad.goal_velocity.y == 0);
    assert(game.sim.player_two_pad.goal_velocity.y == 1);
    assert(SDL_JoystickSetVirtualAxis(one, SDL_CONTROLLER_AXIS_LEFTY, -32768) == 0);
    pump(&game);
    scene_update(&game, 0.01f);
    assert(game.sim.player_one_pad.goal_velocity.y == -1);
    assert(SDL_JoystickSetVirtualAxis(one, SDL_CONTROLLER_AXIS_LEFTY, 19660) == 0);
    pump(&game);
    scene_update(&game, 0.01f);
    assert(fabsf(game.sim.player_one_pad.goal_velocity.y - 0.5f) < 0.005f); // 1/127 steps
    debug_give_power_up(&game.sim, POWER_UP_GROW, 1);
    key(&game, SDLK_d);
    scene_update(&game, 0);
    assert(game.sim.power_ups.player_one_grow_remaining == 0);
    button(&game, one, SDL_CONTROLLER_BUTTON_A);
    scene_update(&game, 0);
    assert(game.sim.power_ups.player_one_grow_remaining > 0);
    assert(game.sim.power_ups.player_two_grow_remaining == 0);
    button(&game, two, SDL_CONTROLLER_BUTTON_START);
    assert(game.current_scene == SCENE_PAUSE);
    float y = game.sim.player_two_pad.position.y;
    scene_update(&game, 1);
    assert(game.sim.player_two_pad.position.y == y);
    button(&game, two, SDL_CONTROLLER_BUTTON_START);
    assert(game.current_scene == SCENE_PLAY);

    change_scene(&game, SCENE_COUNTDOWN);
    scene_update(&game, 0.5f);
    float remaining = game.countdown_remaining;
    SDL_JoystickClose(one);
    assert(SDL_JoystickDetachVirtual(first) == 0);
    pump(&game);
    assert(game.current_scene == SCENE_PAUSE);
    key(&game, SDLK_ESCAPE);
    assert(game.current_scene == SCENE_PAUSE);
    first = attach("Pong test one", 1);
    one = SDL_JoystickOpen(first);
    pump(&game);
    assert(game.current_scene == SCENE_PAUSE);
    button(&game, one, SDL_CONTROLLER_BUTTON_START);
    assert(game.current_scene == SCENE_COUNTDOWN);
    assert(game.countdown_remaining == remaining);
    scene_update(&game, remaining);
    assert(game.current_scene == SCENE_PLAY);
    button(&game, one, SDL_CONTROLLER_BUTTON_START);
    key(&game, SDLK_DOWN);
    key(&game, SDLK_DOWN);
    key(&game, SDLK_LEFT);
    assert(game.input.player_device[0] == 0);
    button(&game, two, SDL_CONTROLLER_BUTTON_START);
    debug_give_power_up(&game.sim, POWER_UP_GROW, 1);
    key(&game, SDLK_d);
    scene_update(&game, 0);
    assert(game.sim.power_ups.player_one_grow_remaining > 0);
    debug_give_power_up(&game.sim, POWER_UP_GROW, 2);
    button(&game, two, SDL_CONTROLLER_BUTTON_A);
    scene_update(&game, 0);
    assert(game.sim.power_ups.player_two_grow_remaining > 0);

    int assigned = game.input.player_device[1];
    change_scene(&game, SCENE_WINNER);
    button(&game, two, SDL_CONTROLLER_BUTTON_A);
    assert(game.current_scene == SCENE_COUNTDOWN);
    assert(game.input.player_device[0] == 0 && game.input.player_device[1] == assigned);
    scene_update(&game, MATCH_COUNTDOWN_SECONDS);
    SDL_JoystickClose(one);
    assert(SDL_JoystickDetachVirtual(first) == 0);
    pump(&game);
    assert(game.current_scene == SCENE_PLAY);
    int two_index = -1;
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_JoystickGetDeviceInstanceID(i) == SDL_JoystickInstanceID(two)) two_index = i;
    assert(two_index >= 0);
    SDL_JoystickClose(two);
    assert(SDL_JoystickDetachVirtual(two_index) == 0);
    pump(&game);
    assert(game.current_scene == SCENE_PAUSE);
    key(&game, SDLK_ESCAPE);
    assert(game.current_scene == SCENE_PAUSE);
    key(&game, SDLK_DOWN);
    key(&game, SDLK_DOWN);
    key(&game, SDLK_DOWN);
    key(&game, SDLK_LEFT);
    key(&game, SDLK_ESCAPE);
    assert(game.current_scene == SCENE_PLAY && game.input.player_device[1] == 0);

    input_close(&game.input);
    Mix_CloseAudio();
    SDL_Quit();
    puts("Controller checks passed");
    return 0;
}
