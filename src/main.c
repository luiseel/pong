#include "game.h"

int main(void) {
    game_t game = { .is_running = FALSE };
    return run_game(&game);
}
