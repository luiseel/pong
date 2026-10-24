#include "game.h"

int main(void) {
    /* Static so it outlives main when the browser runs the loop. */
    static game_t game = { .is_running = FALSE };
    return run_game(&game);
}
