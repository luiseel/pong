CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

TARGET = pong
SRC_DIR = src
BIN_DIR = bin
SDL2_FLAGS = `sdl2-config --cflags --libs` `pkg-config --cflags --libs sdl2_mixer` `pkg-config --cflags --libs sdl2_ttf`

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.o, $(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(SDL2_FLAGS) -o $(BIN_DIR)/$@ $^ -lm

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(SDL2_FLAGS) -MMD -MP -c -o $@ $<

clean:
	$(RM) $(BIN_DIR)/$(TARGET) $(OBJS) $(OBJS:.o=.d) bin/gameplay-test

-include $(OBJS:.o=.d)

.PHONY: test
test: bin/gameplay-test
	SDL_AUDIODRIVER=dummy ./bin/gameplay-test

bin/gameplay-test: tests/gameplay.c $(filter-out bin/main.o,$(OBJS))
	$(CC) $(CFLAGS) -Isrc $(SDL2_FLAGS) -o $@ $^ -lm
