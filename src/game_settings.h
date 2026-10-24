#ifndef GAME_SETTINGS_H
#define GAME_SETTINGS_H

#define MATCH_DEFAULT_TARGET 5
#define MATCH_MAX_TARGET 21
#define MATCH_COUNTDOWN_SECONDS 3.5f

#define GAME_WIDTH 640
#define GAME_HEIGHT 480	
#define PADDLE_HEIGHT 50
#define POWER_UP_GROW_HEIGHT 100
#define POWER_UP_GROW_CHANCE 0.2f
#define POWER_UP_SERPENT_CHANCE 0.2f
#define POWER_UP_CHARGE_CHANCE 0.2f
#define POWER_UP_CHARGE_HOLD 3.f        // seconds the ball sticks to the pad
#define POWER_UP_CHARGE_MULTIPLIER 2.2f // ball speed of the released shot
#define POWER_UP_SERPENT_AMPLITUDE 35.f   // pixels to each side of the straight path
#define POWER_UP_SERPENT_WAVELENGTH 160.f // pixels of x travel per full wave
#define POWER_UP_SPAWN_MIN 4.f
#define POWER_UP_SPAWN_MAX 8.f
#define POWER_UP_SPEED 150.f
#define POWER_UP_SIZE 12
#define POWER_UP_DURATION 8.f
#define POWER_UP_BALL_MULTIPLIER 2.5f
#define POWER_UP_WEIGHT_STEP 15
#define POWER_UP_WEIGHT_CAP 90
#define POWER_UP_TRAIL_LENGTH 8
#define POWER_UP_SLOTS 3
#define POWER_UP_SLOTS_Y 92
#define HIT_SOUND_CHANNEL 0
#define BALL_SPEED 340.f            // pixels per second at unit velocity
#define BALL_MAX_BOUNCE_DEGREES 60.f // max exit angle from a pad face hit
#define BALL_PAD_SPIN 0.25f          // hit offset added per unit of pad velocity
#define BALL_MAX_EDGE_SPEED_Y 1.5f   // max |velocity.y| after a pad edge hit
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
