#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "game.h"

static game_t new_match(void) {
    game_t game = {0};
    reset_match(&game);
    game.current_scene = SCENE_PLAY;
    return game;
}

static void check_spawn_weighting(void) {
    struct { int one; int two; float expected_left; } cases[] = {
        {0, 0, 0.5f}, {0, 1, 0.6f}, {0, 2, 0.7f}, {0, 3, 0.8f},
        {0, 12, 0.8f}, {1, 0, 0.4f}, {2, 0, 0.3f}, {12, 0, 0.2f}
    };
    for (unsigned int c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        game_t game = new_match();
        game.player_one_score = cases[c].one;
        game.player_two_score = cases[c].two;
        int left = 0;
        for (int i = 0; i < 20000; i++) {
            game.power_ups.active = false;
            game.power_ups.spawn_remaining = 0;
            update_power_ups(&game, 0.001f);
            assert(game.power_ups.active);
            assert(fabsf(game.power_ups.velocity.x) > 100);
            if (game.power_ups.velocity.x < 0) left++;
        }
        assert(fabsf(left / 20000.f - cases[c].expected_left) < 0.02f);
    }
}

static void check_pickup_lifecycle(void) {
    for (int player = 0; player < 2; player++) {
        game_t game = new_match();
        pad_t *pad = player == 0 ? &game.player_one_pad : &game.player_two_pad;
        float *timer = player == 0 ? &game.power_ups.player_one_remaining :
            &game.power_ups.player_two_remaining;
        game.power_ups.active = true;
        game.power_ups.position = pad->position;
        *timer = 2;
        update_power_ups(&game, 0.01f);
        assert(!game.power_ups.active);
        assert(*timer == 8.f);
        assert(game.power_ups.spawn_remaining >= 8 && game.power_ups.spawn_remaining <= 14);
        update_power_ups(&game, 1);
        assert(*timer == 7.f);
        game.power_ups.spawn_remaining = 100;
        update_power_ups(&game, 8);
        assert(*timer == 0);
    }
    game_t game = new_match();
    game.power_ups.active = true;
    game.power_ups.position = (vector_2d_t){GAME_WIDTH + 1, 100};
    update_power_ups(&game, 0.01f);
    assert(!game.power_ups.active);
    assert(game.power_ups.player_one_remaining == 0 && game.power_ups.player_two_remaining == 0);
    game.power_ups.active = true;
    game.power_ups.position = (vector_2d_t){300, 0};
    game.power_ups.velocity = (vector_2d_t){100, -100};
    update_power_ups(&game, 0.01f);
    assert(game.power_ups.position.y >= 0 && game.power_ups.velocity.y > 0);
}

static void aim_at_pad(game_t *game, int player) {
    pad_t *pad = player == 0 ? &game->player_one_pad : &game->player_two_pad;
    game->ball.position = (vector_2d_t){
        player == 0 ? pad->position.x + pad->width : pad->position.x - game->ball.width,
        pad->position.y + 10
    };
    game->ball.velocity = (vector_2d_t){player == 0 ? -1 : 1, 0};
}

static void check_powered_rally(void) {
    game_t game = new_match();
    game.power_ups.player_one_remaining = 8;
    aim_at_pad(&game, 0);
    scene_update(&game, 1.f / 60.f);
    assert(game.power_ups.ball_powered && game.ball.velocity.x > 0);
    assert(!check_collisions(&game.ball, &game.player_one_pad));
    game.power_ups.player_one_remaining = 0.001f;
    game.ball.position = (vector_2d_t){300, 200};
    game.ball.velocity = (vector_2d_t){1, 0};
    scene_update(&game, 0.01f);
    assert(game.power_ups.player_one_remaining == 0);
    assert(game.power_ups.ball_powered);
    assert(fabsf(game.ball.position.x - 303.75f) < 0.001f);
    aim_at_pad(&game, 1);
    scene_update(&game, 1.f / 60.f);
    assert(!game.power_ups.ball_powered && game.ball.velocity.x < 0);
    game.ball.position = (vector_2d_t){300, 200};
    game.ball.velocity = (vector_2d_t){1, 0};
    scene_update(&game, 0.01f);
    assert(fabsf(game.ball.position.x - 302.5f) < 0.001f);
    game.power_ups.player_one_remaining = 8;
    game.power_ups.player_two_remaining = 8;
    for (int player = 0; player < 2; player++) {
        aim_at_pad(&game, player);
        scene_update(&game, 1.f / 60.f);
        assert(game.power_ups.ball_powered);
    }
    game.ball.position = (vector_2d_t){300, 200};
    game.ball.velocity = (vector_2d_t){1, 0};
    scene_update(&game, 0.01f);
    assert(fabsf(game.ball.position.x - 303.75f) < 0.001f);
}

static void check_resets_and_scenes(void) {
    for (int scenario = 0; scenario < 2; scenario++) {
        game_t game = new_match();
        game.power_ups.active = scenario == 0;
        game.power_ups.position = (vector_2d_t){300, 100};
        game.power_ups.velocity = (vector_2d_t){-150, 0};
        game.power_ups.spawn_remaining = 3;
        game.power_ups.player_one_remaining = 5;
        game.power_ups.player_two_remaining = 6;
        game.power_ups.ball_powered = true;
        game.power_ups.trail_count = 4;
        game.ball.position.x = scenario == 0 ? GAME_WIDTH + 1 : -game.ball.width - 1;
        game.ball.velocity.x = scenario == 0 ? 1 : -1;
        scene_update(&game, 0.01f);
        assert(game.player_one_score == (scenario == 0 ? 1 : 0));
        assert(game.player_two_score == (scenario == 1 ? 1 : 0));
        assert(game.power_ups.active == (scenario == 0));
        assert(game.power_ups.player_one_remaining > 4.98f && game.power_ups.player_one_remaining < 5);
        assert(game.power_ups.player_two_remaining > 5.98f && game.power_ups.player_two_remaining < 6);
        assert(game.power_ups.spawn_remaining > 2.98f && game.power_ups.spawn_remaining <= 3);
        if (game.power_ups.active) {
            assert(game.power_ups.position.x < 300 && game.power_ups.position.x > 298);
            assert(game.power_ups.velocity.x == -150);
        }
        assert(!game.power_ups.ball_powered && game.power_ups.trail_count == 0);
        float countdown = game.power_ups.spawn_remaining;
        float power_remaining = game.power_ups.player_one_remaining;
        change_scene(&game, SCENE_TITLE);
        scene_update(&game, 2);
        assert(game.power_ups.spawn_remaining == countdown);
        assert(game.power_ups.player_one_remaining == power_remaining);
        change_scene(&game, SCENE_PLAY);
        assert(game.player_one_score == 0 && game.player_two_score == 0);
        assert(!game.power_ups.active && !game.power_ups.ball_powered);
        assert(game.power_ups.player_one_remaining == 0 && game.power_ups.player_two_remaining == 0);
        assert(game.power_ups.spawn_remaining >= 8 && game.power_ups.spawn_remaining <= 14);
    }
}

int main(void) {
    srand(42);
    assert(SDL_Init(SDL_INIT_AUDIO) == 0);
    assert(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == 0);
    check_spawn_weighting();
    check_pickup_lifecycle();
    check_powered_rally();
    check_resets_and_scenes();
    Mix_CloseAudio();
    SDL_Quit();
    puts("Gameplay checks passed");
    return 0;
}
