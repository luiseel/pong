#include "game.h"

#ifdef PONG_DEBUG
#define DEBUG_MENU TRUE
#else
#define DEBUG_MENU FALSE
#endif

int main(void) {
    game_t game = { .is_running = FALSE, .debug = DEBUG_MENU };
    return run_game(&game);
}
