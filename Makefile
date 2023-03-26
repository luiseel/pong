CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

TARGET = game
SRC_DIR = src
BIN_DIR = bin
SDL2_FLAGS = -F/Library/Frameworks -framework SDL2 -framework SDL2_ttf

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.o, $(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(SDL2_FLAGS) -o $(BIN_DIR)/$@ $^

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(SDL2_FLAGS) -c -o $@ $<

clean:
	$(RM) $(BIN_DIR)/$(TARGET) $(OBJS)
