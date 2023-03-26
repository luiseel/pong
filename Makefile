CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

TARGET = game
SRC_DIR = src
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.o, $(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -F/Library/Frameworks -framework SDL2 -framework SDL2_ttf -o $(BIN_DIR)/$@ $^

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) $(BIN_DIR)/$(TARGET) $(OBJS)
