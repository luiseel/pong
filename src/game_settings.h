#ifndef GAME_SETTINGS_H
#define GAME_SETTINGS_H

#define MATCH_DEFAULT_TARGET 5
#define MATCH_MAX_TARGET 21
#define MATCH_COUNTDOWN_SECONDS 3.5f

#define GAME_WIDTH 640
#define GAME_HEIGHT 480	
#define PADDLE_HEIGHT 50
#define POWER_UP_GROW_HEIGHT 100
#define POWER_UP_GROW_CHANCE 0.25f
#define POWER_UP_SPAWN_MIN 8.f
#define POWER_UP_SPAWN_MAX 14.f
#define POWER_UP_SPEED 150.f
#define POWER_UP_SIZE 12
#define POWER_UP_DURATION 8.f
#define POWER_UP_BALL_MULTIPLIER 1.5f
#define POWER_UP_WEIGHT_STEP 15
#define POWER_UP_WEIGHT_CAP 90
#define POWER_UP_TRAIL_LENGTH 8
#define AI_REACTION_TIME 0.12f  // seconds between target re-evaluations
#define AI_AIM_ERROR 36.f       // max random target offset per rally, pixels
#define AI_MAX_SPEED 0.75f      // fraction of pad goal velocity; a human uses 1.0
#define AI_DEAD_ZONE 6.f        // pixels from target where the pad stops
#define GAME_TITLE "Pong"

#define GAME_FONT_SIZE 64
#define MENU_FONT_SIZE 40
#define HINT_FONT_SIZE 24
#define GAME_PLAYER_ONE_SCORE_X GAME_WIDTH / 4 - (GAME_FONT_SIZE / 2)
#define GAME_SCORE_Y 15
#define GAME_PLAYER_TWO_SCORE_X GAME_WIDTH - (GAME_WIDTH / 4) - (GAME_FONT_SIZE / 2)

#endif
