#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

// A whole number of substeps: sim_step keeps a part of a substep for later
#define TICK (3 * SIM_STEP)

static game_t new_match(void) {
    game_t game = {0};
    reset_match(&game);
    game.current_scene = SCENE_PLAY;
    return game;
}

static void check_spawn_weighting(void) {
    struct { int one; int two; float expected_left; } cases[] = {
        {0, 0, 0.5f}, {0, 1, 0.65f}, {0, 2, 0.8f}, {0, 3, 0.9f},
        {0, 12, 0.9f}, {1, 0, 0.35f}, {2, 0, 0.2f}, {12, 0, 0.1f}
    };
    for (unsigned int c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        game_t game = new_match();
        game.sim.player_one_score = cases[c].one;
        game.sim.player_two_score = cases[c].two;
        int left = 0, speed = 0, grow_left = 0, grow_right = 0, serpent_left = 0, serpent_right = 0;
        int charge_left = 0, charge_right = 0;
        for (int i = 0; i < 20000; i++) {
            game.sim.power_ups.pickups[0].active = false;
            game.sim.power_ups.spawn_remaining = 0;
            update_power_ups(&game.sim, 0.001f);
            assert(game.sim.power_ups.pickups[0].active);
            assert(fabsf(game.sim.power_ups.pickups[0].velocity.x) > 100);
            if (game.sim.power_ups.pickups[0].kind == POWER_UP_GROW) {
                if (game.sim.power_ups.pickups[0].velocity.x < 0) grow_left++;
                else grow_right++;
                assert(abs(grow_left - grow_right) <= 1);
            } else if (game.sim.power_ups.pickups[0].kind == POWER_UP_SERPENT) {
                if (game.sim.power_ups.pickups[0].velocity.x < 0) serpent_left++;
                else serpent_right++;
                assert(abs(serpent_left - serpent_right) <= 1);
            } else if (game.sim.power_ups.pickups[0].kind == POWER_UP_CHARGE) {
                if (game.sim.power_ups.pickups[0].velocity.x < 0) charge_left++;
                else charge_right++;
                assert(abs(charge_left - charge_right) <= 1);
            } else {
                speed++;
                if (game.sim.power_ups.pickups[0].velocity.x < 0) left++;
            }
        }
        assert(fabsf(left / (float)speed - cases[c].expected_left) < 0.02f);
        assert(fabsf((grow_left + grow_right) / 20000.f - 0.2f) < 0.02f);
        assert(fabsf((serpent_left + serpent_right) / 20000.f - 0.2f) < 0.02f);
        assert(fabsf((charge_left + charge_right) / 20000.f - 0.2f) < 0.02f);
    }
}

static void catch_power_up(game_t *game, int player, power_up_kind_t kind) {
    game->sim.power_ups.pickups[0].active = true;
    game->sim.power_ups.pickups[0].kind = kind;
    game->sim.power_ups.pickups[0].position = player == 0 ? game->sim.player_one_pad.position :
        game->sim.player_two_pad.position;
    game->sim.power_ups.pickups[0].velocity = (vector_2d_t){0, 0};
    update_power_ups(&game->sim, 0.01f);
}

static void check_pickup_lifecycle(void) {
    for (int test = 0; test < 4; test++) {
        int player = test % 2;
        power_up_kind_t kind = test < 2 ? POWER_UP_SPEED_BOOST : POWER_UP_SERPENT;
        game_t game = new_match();
        float *timer = kind == POWER_UP_SERPENT ?
            (player == 0 ? &game.sim.power_ups.player_one_serpent_remaining :
                &game.sim.power_ups.player_two_serpent_remaining) :
            (player == 0 ? &game.sim.power_ups.player_one_remaining :
                &game.sim.power_ups.player_two_remaining);
        power_up_inventory_t *inventory = player == 0 ? &game.sim.power_ups.player_one_inventory :
            &game.sim.power_ups.player_two_inventory;
        *timer = 2;
        catch_power_up(&game, player, kind);
        assert(!game.sim.power_ups.pickups[0].active);
        assert(inventory->count == 1 && inventory->slots[0] == kind);
        assert(*timer < 2);
        assert(game.sim.power_ups.spawn_remaining >= POWER_UP_SPAWN_MIN && game.sim.power_ups.spawn_remaining <= POWER_UP_SPAWN_MAX);
        use_power_up(&game.sim, player + 1);
        assert(*timer == 8.f && inventory->count == 0);
        update_power_ups(&game.sim, 1);
        assert(*timer == 7.f);
        game.sim.power_ups.spawn_remaining = 100;
        update_power_ups(&game.sim, 8);
        assert(*timer == 0);
        use_power_up(&game.sim, player + 1);
        assert(*timer == 0);
    }
    game_t game = new_match();
    game.sim.power_ups.pickups[0].active = true;
    game.sim.power_ups.pickups[0].position = (vector_2d_t){GAME_WIDTH + 1, 100};
    update_power_ups(&game.sim, 0.01f);
    assert(!game.sim.power_ups.pickups[0].active);
    assert(game.sim.power_ups.player_one_remaining == 0 && game.sim.power_ups.player_two_remaining == 0);
    game.sim.power_ups.pickups[0].active = true;
    game.sim.power_ups.pickups[0].position = (vector_2d_t){300, 0};
    game.sim.power_ups.pickups[0].velocity = (vector_2d_t){100, -100};
    update_power_ups(&game.sim, 0.01f);
    assert(game.sim.power_ups.pickups[0].position.y >= 0 && game.sim.power_ups.pickups[0].velocity.y > 0);
}

static void aim_at_pad(game_t *game, int player) {
    pad_t *pad = player == 0 ? &game->sim.player_one_pad : &game->sim.player_two_pad;
    game->sim.ball.position = (vector_2d_t){
        player == 0 ? pad->position.x + pad->width : pad->position.x - game->sim.ball.width,
        pad->position.y + 10
    };
    game->sim.ball.velocity = (vector_2d_t){player == 0 ? -1 : 1, 0};
}

static void check_paddle_contact(void) {
    struct {
        float past_face;
        float y_offset;
        float velocity_y;
        float pad_velocity_y;
        bool should_hit;
        int edge;
    } cases[] = {
        {2.f, 10.f, 0.f, 0.f, false, 0},
        {-0.5f, 10.f, 0.f, 0.f, true, 0},
        {-0.5f, -12.75f, 1.f, 0.f, false, -1},
        {-0.5f, -11.25f, -1.f, 0.f, true, 0},
        {-0.5f, -13.f, 0.f, -1.f, false, -1},
        {-0.5f, -12.5f, 0.f, -1.f, true, 0},
        {2.f, 50.75f, -1.f, 0.f, false, 1},
        {2.f, 51.f, 0.f, 1.f, false, 1}
    };
    for (int player = 0; player < 2; player++) {
        for (unsigned int c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
            game_t game = new_match();
            pad_t *pad = player == 0 ? &game.sim.player_one_pad : &game.sim.player_two_pad;
            float direction = player == 0 ? -1.f : 1.f;
            float face = player == 0 ? pad->position.x + pad->width :
                pad->position.x - game.sim.ball.width;
            game.sim.ball.position.x = face + direction * cases[c].past_face;
            game.sim.ball.position.y = pad->position.y + cases[c].y_offset;
            game.sim.ball.velocity = (vector_2d_t){direction, cases[c].velocity_y};
            pad->velocity.y = cases[c].pad_velocity_y;
            scene_update(&game, 1.f / 240.f);
            assert((game.sim.ball.velocity.x * direction < 0) == cases[c].should_hit);
            if (cases[c].edge < 0) {
                assert(game.sim.ball.position.y + game.sim.ball.height <= pad->position.y);
                assert(game.sim.ball.velocity.y < 0);
            } else if (cases[c].edge > 0) {
                assert(game.sim.ball.position.y >= pad->position.y + pad->height);
                assert(game.sim.ball.velocity.y > 0);
            }
        }
    }
}

static void check_face_bounce_angle(void) {
    // Ball top relative to the pad top; the pad is 50 tall, the ball 12
    struct { float y_offset; float pad_velocity_y; float expected_y; } cases[] = {
        {19.f, 0.f, 0.f},        // center hit: flat
        {0.f, 0.f, -0.599f},     // ball top at pad top: 36.8 degrees up
        {38.f, 0.f, 0.599f},     // ball bottom at pad bottom: mirror
        {-11.f, 0.f, -0.848f},   // one pixel of overlap: steep but under 60
        {19.f, 1.f, 0.259f}      // center hit, pad moving down: 15 degrees
    };
    for (int player = 0; player < 2; player++) {
        for (unsigned int c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
            game_t game = new_match();
            pad_t *pad = player == 0 ? &game.sim.player_one_pad : &game.sim.player_two_pad;
            float direction = player == 0 ? -1.f : 1.f;
            float face = player == 0 ? pad->position.x + pad->width :
                pad->position.x - game.sim.ball.width;
            pad->previous_position = pad->position;
            pad->velocity.y = cases[c].pad_velocity_y;
            game.sim.ball.previous_position = (vector_2d_t){
                face - direction, pad->position.y + cases[c].y_offset};
            game.sim.ball.position = (vector_2d_t){
                face + direction, pad->position.y + cases[c].y_offset};
            game.sim.ball.velocity = (vector_2d_t){direction, 0};
            assert(check_collisions(&game.sim.ball, pad));
            assert(game.sim.ball.velocity.x * direction < 0);
            assert(fabsf(game.sim.ball.velocity.y - cases[c].expected_y) < 0.01f);
            float speed = sqrtf(game.sim.ball.velocity.x * game.sim.ball.velocity.x +
                game.sim.ball.velocity.y * game.sim.ball.velocity.y);
            assert(fabsf(speed - 1.f) < 0.001f);
        }
    }
}

static void check_growth_lifecycle(void) {
    for (int player = 0; player < 2; player++) {
        for (int edge = 0; edge < 3; edge++) {
            game_t game = new_match();
            pad_t *pad = player == 0 ? &game.sim.player_one_pad : &game.sim.player_two_pad;
            pad_t *other = player == 0 ? &game.sim.player_two_pad : &game.sim.player_one_pad;
            float *timer = player == 0 ? &game.sim.power_ups.player_one_grow_remaining :
                &game.sim.power_ups.player_two_grow_remaining;
            pad->position.y = edge == 0 ? 0 : edge == 1 ? 215 : 430;
            for (int pickup = 0; pickup < 2; pickup++) {
                catch_power_up(&game, player, POWER_UP_GROW);
                assert(pad->height == (pickup == 0 ? 50 : 100));
                use_power_up(&game.sim, player + 1);
                assert(pad->height == 100 && other->height == 50);
                assert(pad->position.y >= 0 && pad->position.y + pad->height <= GAME_HEIGHT);
                assert(*timer == 8);
                assert(game.sim.power_ups.player_one_remaining == 0);
                assert(game.sim.power_ups.player_two_remaining == 0);
                update_power_ups(&game.sim, 1);
            }
            if (edge == 1) {
                assert(pad->position.y == 190);
                aim_at_pad(&game, player);
                game.sim.ball.position.y = pad->position.y + 85;
                scene_update(&game, 1.f / 60.f);
                assert(player == 0 ? game.sim.ball.velocity.x > 0 : game.sim.ball.velocity.x < 0);
            }
            catch_power_up(&game, player, POWER_UP_SPEED_BOOST);
            use_power_up(&game.sim, player + 1);
            assert(pad->height == 100 && *timer > 0);
            assert(player == 0 ? game.sim.power_ups.player_one_remaining == 8 :
                game.sim.power_ups.player_two_remaining == 8);
            change_scene(&game, SCENE_TITLE);
            float remaining = *timer;
            scene_update(&game, 10);
            assert(*timer == remaining && pad->height == 100);
            update_power_ups(&game.sim, 8);
            assert(*timer == 0 && pad->height == 50);
            if (edge == 1) assert(pad->position.y == 215);
            catch_power_up(&game, player, POWER_UP_GROW);
            use_power_up(&game.sim, player + 1);
            assert(pad->height == 100);
            catch_power_up(&game, player, POWER_UP_GROW);
            reset_match(&game);
            assert(pad->height == 50 && *timer == 0);
            assert(game.sim.power_ups.player_one_inventory.count == 0);
            assert(game.sim.power_ups.player_two_inventory.count == 0);
        }
    }
}

static void check_powered_rally(void) {
    game_t game = new_match();
    game.sim.power_ups.player_one_remaining = 8;
    aim_at_pad(&game, 0);
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.ball_powered && game.sim.ball.velocity.x > 0);
    assert(!check_collisions(&game.sim.ball, &game.sim.player_one_pad));
    game.sim.power_ups.player_one_remaining = 0.001f;
    game.sim.ball.position = (vector_2d_t){300, 200};
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    scene_update(&game, TICK);
    assert(game.sim.power_ups.player_one_remaining == 0);
    assert(game.sim.power_ups.ball_powered);
    assert(fabsf(game.sim.ball.position.x - (300 + BALL_SPEED * POWER_UP_BALL_MULTIPLIER * TICK)) < 0.001f);
    aim_at_pad(&game, 1);
    scene_update(&game, 1.f / 60.f);
    assert(!game.sim.power_ups.ball_powered && game.sim.ball.velocity.x < 0);
    game.sim.ball.position = (vector_2d_t){300, 200};
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    scene_update(&game, TICK);
    assert(fabsf(game.sim.ball.position.x - (300 + BALL_SPEED * TICK)) < 0.001f);
    game.sim.power_ups.player_one_remaining = 8;
    game.sim.power_ups.player_two_remaining = 8;
    for (int player = 0; player < 2; player++) {
        aim_at_pad(&game, player);
        scene_update(&game, 1.f / 60.f);
        assert(game.sim.power_ups.ball_powered);
    }
    game.sim.ball.position = (vector_2d_t){300, 200};
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    scene_update(&game, TICK);
    assert(fabsf(game.sim.ball.position.x - (300 + BALL_SPEED * POWER_UP_BALL_MULTIPLIER * TICK)) < 0.001f);
}

static void check_serpent_rally(void) {
    game_t game = new_match();
    game.sim.power_ups.player_one_serpent_remaining = 8;
    aim_at_pad(&game, 0);
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.ball_serpent && !game.sim.power_ups.ball_powered);

    // The ball weaves to both sides of its straight path and keeps its x speed
    game.sim.ball.position = (vector_2d_t){100, 200};
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    game.sim.power_ups.serpent_distance = 0;
    float low = 200, high = 200;
    for (int i = 0; i < 120; i++) {
        float x = game.sim.ball.position.x;
        scene_update(&game, 1.f / 240.f);
        assert(fabsf(game.sim.ball.position.x - (x + BALL_SPEED / 240.f)) < 0.001f);
        low = fminf(low, game.sim.ball.position.y);
        high = fmaxf(high, game.sim.ball.position.y);
    }
    assert(high - 200 > POWER_UP_SERPENT_AMPLITUDE * 0.9f);
    assert(200 - low > POWER_UP_SERPENT_AMPLITUDE * 0.9f);
    assert(high - 200 <= POWER_UP_SERPENT_AMPLITUDE + 0.01f);

    // A wave that reaches a wall bounces off it instead of sliding along it
    game.sim.ball.position = (vector_2d_t){100, 1};
    game.sim.power_ups.serpent_distance = POWER_UP_SERPENT_WAVELENGTH / 2.f;
    int steps_at_wall = 0;
    for (int i = 0; i < 60; i++) {
        scene_update(&game, 1.f / 240.f);
        assert(game.sim.ball.position.y >= 0);
        if (game.sim.ball.position.y < 1) steps_at_wall++;
    }
    assert(steps_at_wall <= 2);

    // Stacked with speed, the ball weaves and moves faster
    game.sim.power_ups.player_one_remaining = 8;
    aim_at_pad(&game, 0);
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.ball_serpent && game.sim.power_ups.ball_powered);
    game.sim.ball.position = (vector_2d_t){300, 200};
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    scene_update(&game, TICK);
    assert(fabsf(game.sim.ball.position.x - (300 + BALL_SPEED * POWER_UP_BALL_MULTIPLIER * TICK)) < 0.001f);
    assert(game.sim.ball.position.y != 200);

    // An ordinary return straightens the ball
    aim_at_pad(&game, 1);
    scene_update(&game, 1.f / 60.f);
    assert(!game.sim.power_ups.ball_serpent && !game.sim.power_ups.ball_powered);
    game.sim.ball.position = (vector_2d_t){300, 200};
    game.sim.ball.velocity = (vector_2d_t){-1, 0};
    scene_update(&game, TICK);
    assert(game.sim.ball.position.y == 200);
}

static void check_charge_blast(void) {
    game_t game = new_match();
    game.sim.power_ups.spawn_remaining = 100;
    catch_power_up(&game, 0, POWER_UP_CHARGE);
    use_power_up(&game.sim, 1);
    assert(game.sim.power_ups.player_one_charge_remaining == POWER_UP_DURATION);
    assert(game.sim.power_ups.player_one_inventory.count == 0);

    // The hit uses the charge; the ball sticks to the pad center and follows it
    aim_at_pad(&game, 0);
    scene_update(&game, 1.f / 60.f);
    pad_t *pad = &game.sim.player_one_pad;
    assert(game.sim.power_ups.ball_held_by == 1 && game.sim.power_ups.player_one_charge_remaining == 0);
    for (int i = 0; i < 60; i++) {
        pad->position.y += 3;
        scene_update(&game, 1.f / 60.f);
        assert(game.sim.ball.position.x == pad->position.x + pad->width);
        assert(fabsf(game.sim.ball.position.y + game.sim.ball.height / 2.f -
            (pad->position.y + pad->height / 2.f)) < 0.001f);
    }
    assert(pad->position.y > GAME_HEIGHT / 2);

    // Other timers continue during the hold
    float spawn = game.sim.power_ups.spawn_remaining;
    game.sim.power_ups.player_two_remaining = 8;
    for (int i = 0; i < 114; i++) scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.ball_held_by == 1);
    assert(fabsf(game.sim.power_ups.player_two_remaining - (8 - 114 / 60.f)) < 0.01f);
    assert(fabsf(game.sim.power_ups.spawn_remaining - (spawn - 114 / 60.f)) < 0.01f);
    game.sim.power_ups.player_two_remaining = 0;

    // After the hold, the ball goes straight at the charge speed
    for (int i = 0; i < 6 && game.sim.power_ups.ball_held_by; i++) scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.ball_held_by == 0 && game.sim.power_ups.ball_charged);
    float x = game.sim.ball.position.x, y = game.sim.ball.position.y;
    scene_update(&game, TICK);
    assert(game.sim.ball.velocity.y == 0 && game.sim.ball.position.y == y);
    assert(fabsf(game.sim.ball.position.x - (x + BALL_SPEED * POWER_UP_CHARGE_MULTIPLIER * TICK)) < 0.01f);

    // A moving pad aims the shot up to the aim angle
    for (int player = 1; player <= 2; player++) {
        for (int i = 0; i < 2; i++) {
            float pad_velocity = i == 0 ? 1.f : -0.5f;
            game.sim.power_ups.ball_held_by = player;
            game.sim.power_ups.hold_remaining = 0.001f;
            pad_t *aiming = player == 1 ? &game.sim.player_one_pad : &game.sim.player_two_pad;
            aiming->velocity.y = pad_velocity;
            update_held_ball(&game.sim, 1.f / 60.f);
            float aim = pad_velocity * POWER_UP_CHARGE_AIM_DEGREES * (float)M_PI / 180.f;
            assert(fabsf(game.sim.ball.velocity.x - (player == 1 ? cosf(aim) : -cosf(aim))) < 0.001f);
            assert(fabsf(game.sim.ball.velocity.y - sinf(aim)) < 0.001f);
            aiming->velocity.y = 0;
        }
    }

    // A pad pushed against a wall shoots straight
    for (int edge = 0; edge < 2; edge++) {
        game.sim.power_ups.ball_held_by = 1;
        game.sim.power_ups.hold_remaining = 0.001f;
        pad->position.y = edge == 0 ? 0 : GAME_HEIGHT - pad->height;
        pad->velocity.y = edge == 0 ? -1.f : 1.f;
        update_held_ball(&game.sim, 1.f / 60.f);
        assert(game.sim.ball.velocity.x == 1.f && game.sim.ball.velocity.y == 0);
    }
    pad->velocity.y = 0;
    pad->position.y = (GAME_HEIGHT - pad->height) / 2.f;

    // A speed boost used during the hold makes the shot use the speed boost
    game.sim.power_ups.ball_powered = false;
    game.sim.power_ups.player_one_remaining = 8;
    game.sim.power_ups.ball_held_by = 1;
    game.sim.power_ups.hold_remaining = 0.001f;
    update_held_ball(&game.sim, 1.f / 60.f);
    assert(game.sim.power_ups.ball_powered && game.sim.power_ups.ball_charged);
    game.sim.power_ups.player_one_remaining = 0;
    game.sim.power_ups.ball_powered = false;
    game.sim.ball.velocity = (vector_2d_t){1, 0};

    // A return clears the charged shot; the next hit of player one is normal
    aim_at_pad(&game, 1);
    scene_update(&game, 1.f / 60.f);
    assert(!game.sim.power_ups.ball_charged && game.sim.ball.velocity.x < 0);
    aim_at_pad(&game, 0);
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.ball_held_by == 0 && !game.sim.power_ups.ball_charged);

    // With speed, the faster speed boost wins; a serpent used during the hold makes the shot weave
    game.sim.power_ups.player_one_charge_remaining = 8;
    game.sim.power_ups.player_one_remaining = 8;
    aim_at_pad(&game, 0);
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.ball_held_by == 1 && !game.sim.power_ups.ball_serpent);
    game.sim.power_ups.player_one_serpent_remaining = 8;
    game.sim.power_ups.hold_remaining = 0.001f;
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.ball_charged && game.sim.power_ups.ball_serpent);
    x = game.sim.ball.position.x;
    y = game.sim.ball.position.y;
    scene_update(&game, TICK);
    assert(fabsf(game.sim.ball.position.x - (x + BALL_SPEED * POWER_UP_BALL_MULTIPLIER * TICK)) < 0.01f);
    assert(game.sim.ball.position.y != y);

    // A point clears the charged shot
    game.sim.ball.position = (vector_2d_t){GAME_WIDTH - game.sim.ball.width - 1, 0};
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.player_one_score == 1 && !game.sim.power_ups.ball_charged);

    // Player two holds the ball on its face and shoots it to the left
    game.sim.power_ups.player_two_charge_remaining = 8;
    aim_at_pad(&game, 1);
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.ball_held_by == 2);
    assert(game.sim.ball.position.x == game.sim.player_two_pad.position.x - game.sim.ball.width);
    game.sim.power_ups.hold_remaining = 0.001f;
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.ball_charged && game.sim.ball.velocity.x < 0 && game.sim.ball.velocity.y == 0);

    // An unused charge runs out, and a later hit is normal
    game.sim.power_ups.player_one_charge_remaining = 8;
    game.sim.ball.position = (vector_2d_t){300, 200};
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    game.sim.power_ups.player_one_remaining = 0;
    game.sim.power_ups.player_one_serpent_remaining = 0;
    update_power_ups(&game.sim, 8);
    assert(game.sim.power_ups.player_one_charge_remaining == 0);
    aim_at_pad(&game, 0);
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.ball_held_by == 0 && game.sim.ball.velocity.x > 0);
}

static void check_resets_and_scenes(void) {
    for (int scenario = 0; scenario < 2; scenario++) {
        game_t game = new_match();
        game.sim.power_ups.pickups[0].active = scenario == 0;
        game.sim.power_ups.pickups[0].position = (vector_2d_t){300, 100};
        game.sim.power_ups.pickups[0].velocity = (vector_2d_t){-150, 0};
        game.sim.power_ups.spawn_remaining = 3;
        game.sim.power_ups.player_one_remaining = 5;
        game.sim.power_ups.player_two_remaining = 6;
        game.sim.power_ups.ball_powered = true;
        game.sim.power_ups.ball_serpent = true;
        game.sim.power_ups.trail_count = 4;
        game.sim.ball.position.x = scenario == 0 ? GAME_WIDTH + 1 : -game.sim.ball.width - 1;
        game.sim.ball.velocity.x = scenario == 0 ? 1 : -1;
        scene_update(&game, TICK);
        assert(game.sim.player_one_score == (scenario == 0 ? 1 : 0));
        assert(game.sim.player_two_score == (scenario == 1 ? 1 : 0));
        assert(game.sim.power_ups.pickups[0].active == (scenario == 0));
        assert(game.sim.power_ups.player_one_remaining > 4.98f && game.sim.power_ups.player_one_remaining < 5);
        assert(game.sim.power_ups.player_two_remaining > 5.98f && game.sim.power_ups.player_two_remaining < 6);
        assert(game.sim.power_ups.spawn_remaining > 2.98f && game.sim.power_ups.spawn_remaining <= 3);
        if (game.sim.power_ups.pickups[0].active) {
            assert(game.sim.power_ups.pickups[0].position.x < 300 && game.sim.power_ups.pickups[0].position.x > 298);
            assert(game.sim.power_ups.pickups[0].velocity.x == -150);
        }
        assert(!game.sim.power_ups.ball_powered && !game.sim.power_ups.ball_serpent);
        assert(game.sim.power_ups.trail_count == 0);
        float countdown = game.sim.power_ups.spawn_remaining;
        float power_remaining = game.sim.power_ups.player_one_remaining;
        change_scene(&game, SCENE_TITLE);
        scene_update(&game, 2);
        assert(game.sim.power_ups.spawn_remaining == countdown);
        assert(game.sim.power_ups.player_one_remaining == power_remaining);
        change_scene(&game, SCENE_COUNTDOWN);
        assert(game.sim.player_one_score == 0 && game.sim.player_two_score == 0);
        assert(!game.sim.power_ups.pickups[0].active && !game.sim.power_ups.ball_powered);
        assert(game.sim.power_ups.player_one_remaining == 0 && game.sim.power_ups.player_two_remaining == 0);
        assert(game.sim.power_ups.spawn_remaining >= POWER_UP_SPAWN_MIN && game.sim.power_ups.spawn_remaining <= POWER_UP_SPAWN_MAX);
    }
}

static void press_key(game_t *game, SDL_Keycode key) {
    SDL_Event event = {0};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = key;
    scene_handle_event(game, &event);
}

// Power-up keys take effect on the next sim_step
static void press_play_key(game_t *game, SDL_Keycode key) {
    press_key(game, key);
    scene_update(game, 0);
}

static void check_pause_flow(void) {
    game_t game = new_match();
    game.sim.player_one_score = 2;
    game.sim.player_two_score = 3;
    game.sim.power_ups.spawn_remaining = 4;
    game.sim.power_ups.player_one_remaining = 5;
    game.sim.power_ups.player_two_grow_remaining = 6;
    game.sim.player_two_pad.height = 100;
    game.sim.player_one_pad.velocity.y = 1;
    float ball_x = game.sim.ball.position.x;
    float pad_y = game.sim.player_one_pad.position.y;
    SDL_Event repeat = {0};
    repeat.type = SDL_KEYDOWN;
    repeat.key.keysym.sym = SDLK_ESCAPE;
    repeat.key.repeat = 1;

    press_key(&game, SDLK_ESCAPE);
    assert(game.current_scene == SCENE_PAUSE);
    scene_handle_event(&game, &repeat);
    scene_update(&game, 30);
    assert(game.current_scene == SCENE_PAUSE);
    assert(game.sim.ball.position.x == ball_x);
    assert(game.sim.player_one_pad.position.y == pad_y);
    assert(game.sim.player_one_pad.velocity.y == 1);
    assert(game.sim.player_one_score == 2 && game.sim.player_two_score == 3);
    assert(game.sim.power_ups.spawn_remaining == 4);
    assert(game.sim.power_ups.player_one_remaining == 5);
    assert(game.sim.power_ups.player_two_grow_remaining == 6);
    assert(game.sim.player_two_pad.height == 100);

    press_key(&game, SDLK_ESCAPE);
    scene_handle_event(&game, &repeat);
    assert(game.current_scene == SCENE_PLAY);
    assert(game.sim.ball.position.x == ball_x);
    scene_update(&game, TICK);
    assert(game.sim.ball.position.x != ball_x);
    assert(game.sim.power_ups.player_one_remaining < 5);
    assert(game.sim.player_one_score == 2 && game.sim.player_two_score == 3);

    press_key(&game, SDLK_ESCAPE);
    press_key(&game, SDLK_RETURN);
    assert(game.current_scene == SCENE_PLAY);
    press_key(&game, SDLK_ESCAPE);
    press_key(&game, SDLK_DOWN);
    press_key(&game, SDLK_RETURN);
    assert(game.current_scene == SCENE_TITLE);
}

static void check_match_flow(void) {
    game_t game = new_match();
    change_scene(&game, SCENE_TITLE);
    press_key(&game, SDLK_DOWN);
    press_key(&game, SDLK_RETURN);
    assert(game.current_scene == SCENE_SETUP);
    assert(game.mode == MODE_PVP);
    assert(game.target_score == MATCH_DEFAULT_TARGET);
    for (int i = 0; i < 30; i++) press_key(&game, SDLK_LEFT);
    assert(game.target_score == 1);
    for (int i = 0; i < 30; i++) press_key(&game, SDLK_RIGHT);
    assert(game.target_score == 21);
    press_key(&game, SDLK_LEFT);
    assert(game.target_score == 20);
    press_key(&game, SDLK_RETURN);
    assert(game.current_scene == SCENE_COUNTDOWN);
    float ball_x = game.sim.ball.position.x;
    float power_timer = game.sim.power_ups.spawn_remaining;
    scene_update(&game, 3);
    assert(game.current_scene == SCENE_COUNTDOWN);
    assert(game.sim.ball.position.x == ball_x);
    assert(game.sim.power_ups.spawn_remaining == power_timer);
    scene_update(&game, 0.5f);
    assert(game.current_scene == SCENE_PLAY);
    scene_update(&game, TICK);
    assert(game.sim.ball.position.x != ball_x);

    for (int player = 0; player < 2; player++) {
        game.target_score = 2;
        change_scene(&game, SCENE_COUNTDOWN);
        scene_update(&game, MATCH_COUNTDOWN_SECONDS);
        for (int point = 1; point <= 2; point++) {
            game.sim.ball.position.x = player == 0 ? GAME_WIDTH + 1 : -20;
            scene_update(&game, TICK);
            assert((player == 0 ? game.sim.player_one_score : game.sim.player_two_score) == point);
            assert(game.current_scene == (point == 2 ? SCENE_WINNER : SCENE_PLAY));
        }
        ball_x = game.sim.ball.position.x;
        power_timer = game.sim.power_ups.spawn_remaining;
        scene_update(&game, 10);
        assert(game.sim.ball.position.x == ball_x);
        assert(game.sim.power_ups.spawn_remaining == power_timer);
        press_key(&game, SDLK_RETURN);
        assert(game.current_scene == SCENE_COUNTDOWN);
        assert(game.sim.player_one_score == 0 && game.sim.player_two_score == 0);
        assert(game.target_score == 2);
        press_key(&game, SDLK_ESCAPE);
        assert(game.current_scene == SCENE_TITLE);
    }
    change_scene(&game, SCENE_WINNER);
    press_key(&game, SDLK_DOWN);
    press_key(&game, SDLK_RETURN);
    assert(game.current_scene == SCENE_TITLE);
    press_key(&game, SDLK_RETURN);
    assert(game.target_score == 2);
    press_key(&game, SDLK_ESCAPE);
    assert(game.current_scene == SCENE_TITLE);
}

static void play_ai_rally(vector_2d_t velocity) {
    game_t game = new_match();
    game.mode = MODE_PVC;
    pad_t *pad = &game.sim.player_two_pad;
    pad->position.y = 0;
    game.sim.ball.position = (vector_2d_t){100, GAME_HEIGHT / 2 - game.sim.ball.height / 2};
    game.sim.ball.velocity = velocity;
    float const face = pad->position.x - game.sim.ball.width;
    bool overlapped = false;
    for (int i = 0; i < 60 * 8 && game.sim.ball.velocity.x > 0; i++) {
        scene_update(&game, 1.f / 60.f);
        game.ai.aim_offset = 0;
        assert(game.ai.target_y >= 0 && game.ai.target_y <= GAME_HEIGHT);
        if (game.sim.ball.position.x >= face - 5) {
            overlapped = game.sim.ball.position.y < pad->position.y + pad->height &&
                game.sim.ball.position.y + game.sim.ball.height > pad->position.y;
        }
    }
    assert(pad->position.y > 0);
    assert(overlapped);
    assert(game.sim.ball.velocity.x < 0);
    assert(game.sim.player_one_score == 0 && game.sim.player_two_score == 0);
}

static void check_ai_tracking(void) {
    for (int i = 0; i < 20; i++) play_ai_rally((vector_2d_t){1, 0.5f});
}

static void check_ai_wall_bounce(void) {
    for (int i = 0; i < 20; i++) play_ai_rally((vector_2d_t){0.5f, 1});
    struct { float velocity_y; float expected; } cases[] = {{1, 435}, {-1, 45}};
    for (unsigned int c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        game_t game = new_match();
        game.sim.ball.position = (vector_2d_t){330, GAME_HEIGHT / 2 - game.sim.ball.height / 2};
        game.sim.ball.velocity = (vector_2d_t){1, cases[c].velocity_y};
        update_ai_pad(&game.ai, &game.sim.player_two_pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
        game.ai.aim_offset = 0;
        game.ai.reaction_remaining = 0;
        update_ai_pad(&game.ai, &game.sim.player_two_pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
        assert(fabsf(game.ai.target_y - cases[c].expected) < 0.01f);
    }
}

static void check_ai_idle(void) {
    game_t game = new_match();
    game.mode = MODE_PVC;
    pad_t *pad = &game.sim.player_two_pad;
    pad->position.y = 0;
    game.sim.ball.position = (vector_2d_t){GAME_WIDTH / 2, 100};
    // Slow enough that the ball stays in flight for the whole two seconds
    game.sim.ball.velocity = (vector_2d_t){-0.3f, 0};
    for (int i = 0; i < 120; i++) scene_update(&game, 1.f / 60.f);
    assert(game.sim.ball.velocity.x < 0);
    float center = pad->position.y + pad->height / 2.f;
    assert(fabsf(center - GAME_HEIGHT / 2.f) <= AI_DEAD_ZONE + 2);
    assert(pad->goal_velocity.y == 0);
    assert(!game.ai.tracking);
}

static void check_ai_power_up(void) {
    game_t game = new_match();
    game.mode = MODE_PVC;
    pad_t *pad = &game.sim.player_two_pad;
    pad->position.y = GAME_HEIGHT - pad->height;
    game.sim.ball.position = (vector_2d_t){600, 100};
    game.sim.ball.velocity = (vector_2d_t){-0.1f, 0};
    game.sim.power_ups.pickups[0].active = true;
    game.sim.power_ups.pickups[0].kind = POWER_UP_SPEED_BOOST;
    game.sim.power_ups.pickups[0].position = (vector_2d_t){(GAME_WIDTH - POWER_UP_SIZE) / 2.f, 100};
    game.sim.power_ups.pickups[0].velocity = (vector_2d_t){POWER_UP_SPEED, 0};
    for (int i = 0; i < 150; i++) scene_update(&game, 1.f / 60.f);
    assert(!game.sim.power_ups.pickups[0].active);
    assert(game.sim.power_ups.player_two_inventory.count == 1);
    assert(game.sim.power_ups.player_two_remaining == 0);
    game.sim.ball.position = (vector_2d_t){300, 200};
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.player_two_inventory.count == 0);
    assert(game.sim.power_ups.player_two_remaining > 0);

    // Growth is used at once, but not while it is still active
    game = new_match();
    game.mode = MODE_PVC;
    catch_power_up(&game, 1, POWER_UP_GROW);
    catch_power_up(&game, 1, POWER_UP_GROW);
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.player_two_pad.height == POWER_UP_GROW_HEIGHT);
    assert(game.sim.power_ups.player_two_inventory.count == 1);

    // Serpent waits for the ball to come back
    game = new_match();
    game.mode = MODE_PVC;
    catch_power_up(&game, 1, POWER_UP_SERPENT);
    game.sim.ball.position = (vector_2d_t){300, 200};
    game.sim.ball.velocity = (vector_2d_t){-1, 0};
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.player_two_inventory.count == 1);
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.player_two_inventory.count == 0);
    assert(game.sim.power_ups.player_two_serpent_remaining > 0);

    // Charge waits for the ball to come back, and is not used while armed
    game = new_match();
    game.mode = MODE_PVC;
    catch_power_up(&game, 1, POWER_UP_CHARGE);
    catch_power_up(&game, 1, POWER_UP_CHARGE);
    game.sim.ball.position = (vector_2d_t){300, 200};
    game.sim.ball.velocity = (vector_2d_t){-1, 0};
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.player_two_inventory.count == 2);
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.player_two_inventory.count == 1);
    assert(game.sim.power_ups.player_two_charge_remaining > 0);
    scene_update(&game, 1.f / 60.f);
    assert(game.sim.power_ups.player_two_inventory.count == 1);

    // An incoming ball takes priority over the pickup
    game = new_match();
    game.sim.ball.position = (vector_2d_t){400, 8};
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    game.sim.power_ups.pickups[0].active = true;
    game.sim.power_ups.pickups[0].position = (vector_2d_t){400, 400};
    game.sim.power_ups.pickups[0].velocity = (vector_2d_t){POWER_UP_SPEED, 0};
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
    game.ai.aim_offset = 0;
    game.ai.reaction_remaining = 0;
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
    assert(game.ai.tracking);
    assert(fabsf(game.ai.target_y - pad->height / 2.f) < 0.01f);
}

static void check_ai_speed_cap(void) {
    game_t game = new_match();
    pad_t *pad = &game.sim.player_two_pad;
    for (int direction = -1; direction <= 1; direction += 2) {
        reset_ai(&game.ai);
        pad->position.y = direction > 0 ? 0 : GAME_HEIGHT - pad->height;
        game.sim.ball.position = (vector_2d_t){400, direction > 0 ? GAME_HEIGHT - 20 : 8};
        game.sim.ball.velocity = (vector_2d_t){1, 0};
        float goal = update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
        assert(game.ai.tracking);
        assert(fabsf(game.ai.aim_offset) <= AI_AIM_ERROR);
        assert(fabsf(goal - direction * AI_MAX_SPEED) < 1e-6f);
        game.ai.aim_offset = 0;
        game.ai.reaction_remaining = 0;
        update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
        float expected = game.sim.ball.position.y + game.sim.ball.height / 2.f;
        float half = pad->height / 2.f;
        expected = fmaxf(half, fminf(GAME_HEIGHT - half, expected));
        assert(fabsf(game.ai.target_y - expected) < 0.01f);
    }
}

static void check_ai_reaction_timing(void) {
    game_t game = new_match();
    pad_t *pad = &game.sim.player_two_pad;
    game.sim.ball.position = (vector_2d_t){400, 100};
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
    game.ai.aim_offset = 0;
    game.ai.reaction_remaining = 0;
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
    float first = 100 + game.sim.ball.height / 2.f;
    assert(fabsf(game.ai.target_y - first) < 0.01f);

    // The ball moves, but the target only refreshes after the reaction time
    game.sim.ball.position.y = 300;
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, AI_REACTION_TIME / 2);
    assert(fabsf(game.ai.target_y - first) < 0.01f);
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, AI_REACTION_TIME / 2 - 0.01f);
    assert(fabsf(game.ai.target_y - first) < 0.01f);
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 0.02f);
    assert(fabsf(game.ai.target_y - (300 + game.sim.ball.height / 2.f)) < 0.01f);
}

static void check_ai_grown_pad_clamp(void) {
    game_t game = new_match();
    pad_t *pad = &game.sim.player_two_pad;
    pad->height = POWER_UP_GROW_HEIGHT;
    game.sim.ball.position = (vector_2d_t){400, 8};
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
    game.ai.aim_offset = 0;
    game.ai.reaction_remaining = 0;
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
    assert(fabsf(game.ai.target_y - POWER_UP_GROW_HEIGHT / 2.f) < 0.01f);
}

static void check_ai_ignores_passed_objects(void) {
    game_t game = new_match();
    pad_t *pad = &game.sim.player_two_pad;
    pad->position.y = 0;

    // A ball already behind the pad is not tracked
    game.sim.ball.position = (vector_2d_t){pad->position.x + 5, 300};
    game.sim.ball.velocity = (vector_2d_t){1, 0};
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
    assert(!game.ai.tracking);
    assert(fabsf(game.ai.target_y - GAME_HEIGHT / 2.f) < 0.01f);

    // A pickup still in front is chased even though the ball is behind
    game.sim.power_ups.pickups[0].active = true;
    game.sim.power_ups.pickups[0].position = (vector_2d_t){400, 400};
    game.sim.power_ups.pickups[0].velocity = (vector_2d_t){POWER_UP_SPEED, 0};
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
    assert(fabsf(game.ai.target_y - (400 + POWER_UP_SIZE / 2.f)) < 0.01f);

    // A pickup behind the pad is ignored
    game.sim.power_ups.pickups[0].position.x = pad->position.x + 5;
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
    assert(fabsf(game.ai.target_y - GAME_HEIGHT / 2.f) < 0.01f);

    debug_spawn_power_up(&game.sim, POWER_UP_GROW, 2);
    power_up_pickup_t *next = &game.sim.power_ups.pickups[1];
    next->position = (vector_2d_t){400, 100};
    next->velocity = (vector_2d_t){POWER_UP_SPEED, 0};
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
    assert(fabsf(game.ai.target_y - (100 + POWER_UP_SIZE / 2.f)) < 0.01f);

    debug_spawn_power_up(&game.sim, POWER_UP_SERPENT, 2);
    game.sim.power_ups.pickups[2].position = (vector_2d_t){500, 300};
    game.sim.power_ups.pickups[2].velocity = (vector_2d_t){POWER_UP_SPEED, 0};
    update_ai_pad(&game.ai, pad, &game.sim.ball, &game.sim.power_ups, 1.f / 60.f);
    assert(fabsf(game.ai.target_y - (300 + POWER_UP_SIZE / 2.f)) < 0.01f);
    clear_power_up_pickups(&game.sim);
}

static void check_mode_select(void) {
    game_t game = new_match();
    change_scene(&game, SCENE_TITLE);
    press_key(&game, SDLK_RETURN);
    assert(game.current_scene == SCENE_SETUP && game.mode == MODE_PVC);
    change_scene(&game, SCENE_TITLE);
    press_key(&game, SDLK_DOWN);
    press_key(&game, SDLK_RETURN);
    assert(game.current_scene == SCENE_SETUP && game.mode == MODE_PVP);
    change_scene(&game, SCENE_TITLE);
    press_key(&game, SDLK_UP);
    assert(game.selected_menu_item == 3);
    press_key(&game, SDLK_RETURN);
    assert(game.current_scene == SCENE_CREDITS && game.mode == MODE_PVP);
    change_scene(&game, SCENE_TITLE);
    press_key(&game, SDLK_DOWN);
    press_key(&game, SDLK_DOWN);
    press_key(&game, SDLK_DOWN);
    press_key(&game, SDLK_RETURN);
    assert(game.current_scene == SCENE_CREDITS);
    change_scene(&game, SCENE_TITLE);
    press_key(&game, SDLK_DOWN);
    press_key(&game, SDLK_DOWN);
    press_key(&game, SDLK_RETURN);
    assert(game.current_scene == SCENE_LOBBY && game.mode == MODE_ONLINE);
    assert(game.lobby_step == LOBBY_MENU && game.net.status == NET_IDLE);
    press_key(&game, SDLK_DOWN);
    press_key(&game, SDLK_RETURN);
    assert(game.lobby_step == LOBBY_CODE);
    press_key(&game, SDLK_a);
    press_key(&game, SDLK_b);
    press_key(&game, SDLK_BACKSPACE);
    press_key(&game, SDLK_x);
    press_key(&game, SDLK_7);
    press_key(&game, SDLK_MINUS);
    assert(strcmp(game.lobby_code, "AX7") == 0);
    press_key(&game, SDLK_RETURN); // too short: stays in the code entry
    assert(game.lobby_step == LOBBY_CODE && game.net.status == NET_IDLE);
    press_key(&game, SDLK_ESCAPE);
    assert(game.lobby_step == LOBBY_MENU);
    press_key(&game, SDLK_ESCAPE);
    assert(game.current_scene == SCENE_TITLE);
    change_scene(&game, SCENE_TITLE);
    for (int i = 0; i < 4; i++) press_key(&game, SDLK_DOWN);
    assert(game.selected_menu_item == 0);
    game.mode = MODE_PVC;
    game.ai.tracking = true;
    game.ai.target_y = 100;
    change_scene(&game, SCENE_COUNTDOWN);
    assert(game.mode == MODE_PVC);
    assert(!game.ai.tracking && game.ai.target_y == 0);
}

static void check_inventory(void) {
    game_t game = new_match();
    power_up_inventory_t *one = &game.sim.power_ups.player_one_inventory;
    power_up_inventory_t *two = &game.sim.power_ups.player_two_inventory;
    catch_power_up(&game, 0, POWER_UP_SPEED_BOOST);
    catch_power_up(&game, 0, POWER_UP_GROW);
    catch_power_up(&game, 0, POWER_UP_SPEED_BOOST);
    catch_power_up(&game, 0, POWER_UP_GROW);
    assert(one->count == POWER_UP_SLOTS && one->slots[2] == POWER_UP_SPEED_BOOST);
    assert(!game.sim.power_ups.pickups[0].active && two->count == 0);

    press_play_key(&game, SDLK_a);
    assert(one->selected == 1);
    press_play_key(&game, SDLK_d);
    assert(game.sim.player_one_pad.height == POWER_UP_GROW_HEIGHT);
    assert(game.sim.power_ups.player_one_remaining == 0);
    assert(one->count == 2 && one->selected == 1 && one->slots[1] == POWER_UP_SPEED_BOOST);
    press_play_key(&game, SDLK_a);
    assert(one->selected == 0);
    press_play_key(&game, SDLK_d);
    assert(game.sim.power_ups.player_one_remaining == 8 && one->count == 1);

    catch_power_up(&game, 1, POWER_UP_GROW);
    catch_power_up(&game, 1, POWER_UP_SPEED_BOOST);
    press_play_key(&game, SDLK_RIGHT);
    assert(two->selected == 1);
    press_play_key(&game, SDLK_LEFT);
    assert(game.sim.power_ups.player_two_remaining == 8 && two->count == 1 && two->selected == 0);

    // One player uses A to select and D or Space to use; arrows only move
    game.mode = MODE_PVC;
    game.sim.power_ups.player_two_grow_remaining = POWER_UP_DURATION; // the CPU keeps its grow
    catch_power_up(&game, 0, POWER_UP_GROW);
    press_play_key(&game, SDLK_LEFT);
    press_play_key(&game, SDLK_RIGHT);
    assert(one->count == 2 && one->selected == 0 && two->count == 1);
    press_play_key(&game, SDLK_a);
    press_play_key(&game, SDLK_SPACE);
    assert(one->count == 1 && one->slots[0] == POWER_UP_SPEED_BOOST);

    // Keys do nothing outside play
    change_scene(&game, SCENE_PAUSE);
    press_play_key(&game, SDLK_d);
    assert(one->count == 1);
}

static void check_debug_menu(void) {
    game_t game = new_match();
    press_key(&game, SDLK_F1);
    assert(game.current_scene == SCENE_PLAY);

    // Spawn a serpent pickup toward player two; the menu freezes play
    game.debug = true;
    game.sim.power_ups.spawn_remaining = 100;
    press_key(&game, SDLK_F1);
    assert(game.current_scene == SCENE_DEBUG);
    float ball_x = game.sim.ball.position.x;
    scene_update(&game, 1);
    assert(game.sim.ball.position.x == ball_x);
    press_key(&game, SDLK_RIGHT);
    press_key(&game, SDLK_DOWN);
    press_key(&game, SDLK_DOWN);
    press_key(&game, SDLK_RETURN);
    assert(game.current_scene == SCENE_PLAY);
    assert(game.sim.power_ups.pickups[0].active && game.sim.power_ups.pickups[0].kind == POWER_UP_SERPENT);
    assert(game.sim.power_ups.pickups[0].velocity.x > 0);
    assert(game.sim.power_ups.spawn_remaining == 100);

    // Up wraps from the first row; every kind spawns toward player one
    clear_power_up_pickups(&game.sim);
    press_key(&game, SDLK_F1);
    press_key(&game, SDLK_LEFT);
    press_key(&game, SDLK_UP);
    press_key(&game, SDLK_RETURN);
    assert(game.sim.power_ups.pickups[0].kind == POWER_UP_CHARGE && game.sim.power_ups.pickups[0].velocity.x < 0);
    for (int kind = POWER_UP_SPEED_BOOST; kind <= POWER_UP_SERPENT; kind++) {
        clear_power_up_pickups(&game.sim);
        press_key(&game, SDLK_F1);
        for (int i = 0; i < kind; i++) press_key(&game, SDLK_DOWN);
        press_key(&game, SDLK_RETURN);
        assert(game.sim.power_ups.pickups[0].kind == (power_up_kind_t)kind && game.sim.power_ups.pickups[0].velocity.x < 0);
    }

    // Escape and F1 close the menu without a spawn
    clear_power_up_pickups(&game.sim);
    press_key(&game, SDLK_F1);
    press_key(&game, SDLK_ESCAPE);
    assert(game.current_scene == SCENE_PLAY && !game.sim.power_ups.pickups[0].active);
    press_key(&game, SDLK_F1);
    press_key(&game, SDLK_F1);
    assert(game.current_scene == SCENE_PLAY && !game.sim.power_ups.pickups[0].active);
}

static void check_debug_spawn_multiple(void) {
    game_t game = new_match();
    game.debug = true;
    press_key(&game, SDLK_F1);
    press_key(&game, SDLK_RETURN);
    press_key(&game, SDLK_F1);
    press_key(&game, SDLK_DOWN);
    press_key(&game, SDLK_SPACE);
    game.sim.player_one_pad.position.x = (GAME_WIDTH - POWER_UP_SIZE) / 2.f;
    game.sim.player_one_pad.position.y = (GAME_HEIGHT - POWER_UP_SIZE) / 2.f;
    update_power_ups(&game.sim, 0);
    power_up_inventory_t const *inventory = &game.sim.power_ups.player_one_inventory;
    assert(inventory->count == 2);
    assert(inventory->slots[0] == POWER_UP_SPEED_BOOST);
    assert(inventory->slots[1] == POWER_UP_GROW);
}

static void check_multiple_pickup_lifecycle(void) {
    game_t game = new_match();
    game.sim.power_ups.spawn_remaining = 100;
    debug_spawn_power_up(&game.sim, POWER_UP_SPEED_BOOST, 1);
    debug_spawn_power_up(&game.sim, POWER_UP_GROW, 2);
    debug_spawn_power_up(&game.sim, POWER_UP_SERPENT, 2);
    power_up_pickup_t *first = &game.sim.power_ups.pickups[0];
    power_up_pickup_t *second = &game.sim.power_ups.pickups[1];
    power_up_pickup_t *third = &game.sim.power_ups.pickups[2];
    first->position = game.sim.player_one_pad.position;
    second->position = (vector_2d_t){300, 0};
    second->velocity = (vector_2d_t){100, -100};
    third->position = (vector_2d_t){400, 100};
    third->velocity = (vector_2d_t){100, 0};
    update_power_ups(&game.sim, 0.01f);
    assert(game.sim.power_ups.player_one_inventory.count == 1);
    assert(!first->active);
    assert(second->position.x > 300 && second->position.y > 0 && second->velocity.y > 0);
    assert(third->position.x > 400);
    assert(game.sim.power_ups.spawn_remaining == 100);

    second->position.x = GAME_WIDTH + 1;
    update_power_ups(&game.sim, 0);
    assert(game.sim.power_ups.spawn_remaining == 100);
    debug_spawn_power_up(&game.sim, POWER_UP_CHARGE, 1);
    assert(first->active && third->kind == POWER_UP_SERPENT);
    first->position = game.sim.player_one_pad.position;
    third->position = game.sim.player_two_pad.position;
    update_power_ups(&game.sim, 0);
    assert(game.sim.power_ups.player_one_inventory.slots[1] == POWER_UP_CHARGE);
    assert(game.sim.power_ups.player_two_inventory.slots[0] == POWER_UP_SERPENT);
    assert(!first->active && !second->active && !third->active);
    assert(game.sim.power_ups.spawn_remaining >= POWER_UP_SPAWN_MIN &&
        game.sim.power_ups.spawn_remaining <= POWER_UP_SPAWN_MAX);

    // Spawns past the free slots are dropped
    for (int i = 0; i < 100; i++) debug_spawn_power_up(&game.sim, POWER_UP_GROW, 1);
    for (int i = 0; i < POWER_UP_MAX_PICKUPS; i++) assert(game.sim.power_ups.pickups[i].active);
    reset_match(&game);
    for (int i = 0; i < POWER_UP_MAX_PICKUPS; i++) assert(!game.sim.power_ups.pickups[i].active);
}

static void check_debug_give(void) {
    for (int player = 1; player <= 2; player++) {
        game_t game = new_match();
        game.debug = true;
        debug_spawn_power_up(&game.sim, POWER_UP_CHARGE, 1);
        float countdown = game.sim.power_ups.spawn_remaining;
        power_up_inventory_t *inventory = player == 1 ?
            &game.sim.power_ups.player_one_inventory : &game.sim.power_ups.player_two_inventory;
        for (int kind = POWER_UP_SPEED_BOOST; kind <= POWER_UP_CHARGE; kind++) {
            press_key(&game, SDLK_F1);
            if (player == 2 && kind == POWER_UP_SPEED_BOOST) press_key(&game, SDLK_RIGHT);
            for (int i = 0; i < kind; i++) press_key(&game, SDLK_DOWN);
            press_key(&game, SDLK_g);
            assert(game.current_scene == SCENE_PLAY);
            assert(inventory->count == (kind < POWER_UP_SLOTS ? kind + 1 : POWER_UP_SLOTS));
            assert(inventory->slots[inventory->selected] == (power_up_kind_t)kind);
            assert(game.sim.power_ups.pickups[0].active && game.sim.power_ups.pickups[0].kind == POWER_UP_CHARGE);
            assert(game.sim.power_ups.spawn_remaining == countdown);
        }
        power_up_inventory_t *other = player == 1 ?
            &game.sim.power_ups.player_two_inventory : &game.sim.power_ups.player_one_inventory;
        assert(other->count == 0);
        press_play_key(&game, player == 1 ? SDLK_d : SDLK_LEFT);
        assert(inventory->count == POWER_UP_SLOTS - 1);
        assert((player == 1 ? game.sim.power_ups.player_one_charge_remaining :
            game.sim.power_ups.player_two_charge_remaining) == POWER_UP_DURATION);
    }
}

static void check_pvp_unchanged(void) {
    game_t game = new_match();
    game.sim.player_two_pad.position.y = 0;
    game.sim.ball.position = (vector_2d_t){100, GAME_HEIGHT / 2};
    game.sim.ball.velocity = (vector_2d_t){1, 0.5f};
    for (int i = 0; i < 30; i++) scene_update(&game, 1.f / 60.f);
    assert(game.sim.player_two_pad.position.y == 0);
    assert(game.sim.player_two_pad.goal_velocity.y == 0);
    assert(!game.ai.tracking);
}

int main(void) {
    srand(42);
    assert(SDL_Init(SDL_INIT_AUDIO) == 0);
    assert(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == 0);
    check_match_flow();
    check_pause_flow();
    check_spawn_weighting();
    check_pickup_lifecycle();
    check_paddle_contact();
    check_face_bounce_angle();
    check_growth_lifecycle();
    check_powered_rally();
    check_serpent_rally();
    check_charge_blast();
    check_resets_and_scenes();
    check_ai_tracking();
    check_ai_wall_bounce();
    check_ai_idle();
    check_ai_power_up();
    check_ai_speed_cap();
    check_ai_reaction_timing();
    check_ai_grown_pad_clamp();
    check_ai_ignores_passed_objects();
    check_mode_select();
    check_inventory();
    check_debug_menu();
    check_debug_spawn_multiple();
    check_multiple_pickup_lifecycle();
    check_debug_give();
    check_pvp_unchanged();
    Mix_CloseAudio();
    SDL_Quit();
    puts("Gameplay checks passed");
    return 0;
}
