CC = gcc
CFLAGS = -std=c99 -Wall -g

bin/pong: bin
	$(CC) $(CFLAGS) -o bin/pong -F/Library/Frameworks -framework SDL2 -framework SDL2_ttf src/*.c

bin:
	mkdir bin

.PHONY: clean
clean:
	rm -rf bin
