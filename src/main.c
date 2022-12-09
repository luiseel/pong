#include "game.h"

int main(int argc, char * args[]) {
    game_t game = { .is_running = FALSE };
    return run_game(&game);
}
