CC = gcc
CFLAGS = -std=c99 -Wall -g

pong: bin
	$(CC) $(CFLAGS) -o bin/pong -I/usr/local/include/SDL2 -L/usr/local/lib -lSDL2 -lSDL2_ttf src/*.c

bin:
	mkdir bin

clean:
	rm -rf bin
