#include <string.h>
#include "game.h"

int main(int argc, char **argv) {
    game_t game = { .is_running = FALSE, .debug = argc > 1 && strcmp(argv[1], "--debug") == 0 };
    return run_game(&game);
}
