CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

TARGET = pong
SRC_DIR = src
BIN_DIR = bin
SDL2_FLAGS = `sdl2-config --cflags --libs` `pkg-config --cflags --libs sdl2_mixer` `pkg-config --cflags --libs sdl2_ttf`

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.o, $(SRCS))
DEBUG_DIR = $(BIN_DIR)/debug
DEBUG_FLAGS = -g -DPONG_DEBUG
DEBUG_OBJS = $(patsubst $(SRC_DIR)/%.c, $(DEBUG_DIR)/%.o, $(SRCS))

.PHONY: all clean debug

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(SDL2_FLAGS) -o $(BIN_DIR)/$@ $^ -lm

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(SDL2_FLAGS) -MMD -MP -c -o $@ $<

# Debug build: the debug menu is on (F1 during a match)
debug: $(BIN_DIR)/$(TARGET)-debug

$(BIN_DIR)/$(TARGET)-debug: $(DEBUG_OBJS)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(SDL2_FLAGS) -o $@ $^ -lm

$(DEBUG_DIR)/%.o: $(SRC_DIR)/%.c | $(DEBUG_DIR)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(SDL2_FLAGS) -MMD -MP -c -o $@ $<

$(DEBUG_DIR):
	mkdir -p $@

clean:
	$(RM) $(BIN_DIR)/$(TARGET) $(OBJS) $(OBJS:.o=.d) bin/gameplay-test bin/controllers-test
	$(RM) -r $(BIN_DIR)/$(TARGET)-debug $(DEBUG_DIR)

-include $(OBJS:.o=.d) $(DEBUG_OBJS:.o=.d)

.PHONY: test
test: bin/gameplay-test bin/controllers-test
	SDL_AUDIODRIVER=dummy ./bin/gameplay-test
	SDL_AUDIODRIVER=dummy ./bin/controllers-test

bin/gameplay-test: tests/gameplay.c $(filter-out bin/main.o,$(OBJS))
	$(CC) $(CFLAGS) -Isrc $(SDL2_FLAGS) -o $@ $^ -lm

bin/controllers-test: tests/controllers.c $(filter-out bin/main.o,$(OBJS))
	$(CC) $(CFLAGS) -Isrc $(SDL2_FLAGS) -o $@ $^ -lm
