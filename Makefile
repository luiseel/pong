CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

TARGET = pong
# shared/ has no SDL, so the server builds it too; client/ is the SDL game
SHARED_DIR = shared
CLIENT_DIR = client
BIN_DIR = bin
SDL2_FLAGS = `sdl2-config --cflags --libs` `pkg-config --cflags --libs SDL2_mixer` `pkg-config --cflags --libs SDL2_ttf`

SRCS = $(wildcard $(SHARED_DIR)/*.c) $(wildcard $(CLIENT_DIR)/*.c)
OBJS = $(patsubst %.c, $(BIN_DIR)/%.o, $(SRCS))
DEBUG_DIR = $(BIN_DIR)/debug
DEBUG_FLAGS = -g -DPONG_DEBUG
DEBUG_OBJS = $(patsubst %.c, $(DEBUG_DIR)/%.o, $(SRCS))
# The match simulation: no SDL, so a server can build it too
SIM_SRCS = $(addprefix $(SHARED_DIR)/, sim.c entity.c power_up.c vector.c)
# The simulation plus the client-side prediction and interpolation, still no SDL
NETPLAY_SRCS = $(SIM_SRCS) $(SHARED_DIR)/netplay.c
# ENet (third_party/enet, MIT), built with its own warning level
ENET_DIR = third_party/enet
ENET_SRCS = $(wildcard $(ENET_DIR)/*.c)
ENET_OBJS = $(patsubst $(ENET_DIR)/%.c, $(BIN_DIR)/enet/%.o, $(ENET_SRCS))
ENET_FLAGS = -D_DEFAULT_SOURCE -I$(ENET_DIR)/include -DHAS_FCNTL -DHAS_POLL -DHAS_GETADDRINFO -DHAS_GETNAMEINFO \
	-DHAS_INET_PTON -DHAS_INET_NTOP -DHAS_MSGHDR_FLAGS -DHAS_SOCKLEN_T
CFLAGS += -I$(SHARED_DIR) -I$(ENET_DIR)/include
CLIENT_FLAGS = -I$(CLIENT_DIR) $(SDL2_FLAGS)
ifdef NET_DEFAULT_HOST
CFLAGS += -DNET_DEFAULT_HOST='"$(NET_DEFAULT_HOST)"'
endif

.PHONY: all clean debug

all: $(TARGET)

$(TARGET): $(OBJS) $(ENET_OBJS)
	$(CC) $(CFLAGS) $(CLIENT_FLAGS) -o $(BIN_DIR)/$@ $^ -lm

$(BIN_DIR)/enet/%.o: $(ENET_DIR)/%.c | $(BIN_DIR)/enet
	$(CC) -Wall -std=c99 $(ENET_FLAGS) -c -o $@ $<

$(BIN_DIR)/enet:
	mkdir -p $@

# Debug build: the debug menu is on (F1 during a match)
debug: $(BIN_DIR)/$(TARGET)-debug

$(BIN_DIR)/$(TARGET)-debug: $(DEBUG_OBJS) $(ENET_OBJS)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(CLIENT_FLAGS) -o $@ $^ -lm

$(DEBUG_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(CLIENT_FLAGS) -MMD -MP -c -o $@ $<

$(BIN_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CLIENT_FLAGS) -MMD -MP -c -o $@ $<

clean:
	$(RM) $(BIN_DIR)/$(TARGET) bin/gameplay-test bin/controllers-test bin/determinism-test bin/netplay-test bin/net_bot
	$(RM) -r $(BIN_DIR)/$(TARGET)-debug $(DEBUG_DIR) $(BIN_DIR)/enet $(BIN_DIR)/$(SHARED_DIR) $(BIN_DIR)/$(CLIENT_DIR)

-include $(OBJS:.o=.d) $(DEBUG_OBJS:.o=.d)

.PHONY: test net-test
test: bin/gameplay-test bin/controllers-test bin/determinism-test bin/netplay-test
	SDL_AUDIODRIVER=dummy ./bin/gameplay-test
	SDL_AUDIODRIVER=dummy ./bin/controllers-test
	./bin/determinism-test
	./bin/netplay-test

bin/gameplay-test: tests/unit/gameplay.c $(filter-out $(BIN_DIR)/$(CLIENT_DIR)/main.o,$(OBJS)) $(ENET_OBJS)
	$(CC) $(CFLAGS) $(CLIENT_FLAGS) -o $@ $^ -lm

bin/controllers-test: tests/unit/controllers.c $(filter-out $(BIN_DIR)/$(CLIENT_DIR)/main.o,$(OBJS)) $(ENET_OBJS)
	$(CC) $(CFLAGS) $(CLIENT_FLAGS) -o $@ $^ -lm

# Built without SDL flags, so it also checks that the simulation does not use SDL
bin/determinism-test: tests/unit/determinism.c $(SIM_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# Prediction and interpolation against a simulated server with delay
bin/netplay-test: tests/unit/netplay.c $(NETPLAY_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# Headless online client, without SDL: net.c and netplay.c must stay SDL-free
bin/net_bot: tests/net/net_bot.c $(CLIENT_DIR)/net.c $(NETPLAY_SRCS) $(ENET_OBJS)
	$(CC) $(CFLAGS) -I$(CLIENT_DIR) -o $@ $^ -lm

# Builds the server, then two bots play a match against it
net-test: bin/net_bot
	cd server && zig build
	./tests/net/net_test.sh
