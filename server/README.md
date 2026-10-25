# Pong server

The authoritative online server: one thread, one UDP port (ENet), rooms of
two players. It runs the same C simulation as the game.

## Build and run

Needs Zig 0.17.0.

```sh
cd server
zig build                 # debug build, zig-out/bin/pong-server
zig build test            # unit tests
zig build run -- --port 7777
```

Static Linux binaries (musl), for the home server:

```sh
zig build -Dtarget=x86_64-linux-musl --release=safe -p zig-out/x86_64
zig build -Dtarget=aarch64-linux-musl --release=safe -p zig-out/aarch64
```

`--release=safe` (ReleaseSafe) keeps the safety checks; use it in production.

From the repo root, `make net-test` builds the server and plays bot matches
against it (also forfeit, timeouts, bad packets and fake lag).

## Flags

| Flag | Default | |
| --- | --- | --- |
| `--port N` | 7777 | UDP port |
| `--target-score N` | 5 | points to win, 1-21 |
| `--max-rooms N` | 32 | rooms at the same time, 1-1000; peers are 2 per room + 8 |
| `--fake-lag-ms N` | 0 | test only: holds snapshots out and inputs in N ms |
| `--fake-loss N` | 0 | test only: drops N % of snapshots out and inputs in |
| `--verbose` | off | logs each connect and disconnect |
| `--help` | | |

Logs go to stderr, one line per event with a UTC timestamp (rooms, matches,
dropped peers). There are no per-tick logs.

Limits: a peer that is silent for 10 s is gone, and in a match it forfeits.
A peer without a valid HELLO in 5 s, with more than 20 malformed packets,
or with more than 200 packets/s for 3 s is dropped. A room with no match on
(waiting for the second player, or after a match) closes after 10 minutes.
At most 16 peers come from one IP address.

## Docker

The Dockerfile is in `server/`, but the build needs `src/` and
`third_party/`, so build from the repo root:

```sh
docker build -f server/Dockerfile -t pong-server .
docker run -d --name pong-server -p 7777:7777/udp \
  --restart unless-stopped --read-only --cap-drop ALL pong-server
docker logs -f pong-server
```

The image has only the static binary (`FROM scratch`), runs as user 65532,
and exposes 7777/udp. Flags go after the image name, for example
`pong-server --max-rooms 64`. For another CPU:
`docker buildx build --platform linux/arm64 -f server/Dockerfile .`

## Home server checklist

1. **Port forward**: on the router, forward UDP 7777 to the server's LAN IP.
   Give the server a fixed LAN IP (DHCP reservation). Open UDP 7777 in the
   server's firewall (for example `ufw allow 7777/udp`).
2. **CGNAT check**: compare the router's WAN IP (router status page) with
   `curl ifconfig.me`. When they differ, or the WAN IP is in 100.64.0.0/10,
   the ISP uses CGNAT and a port forward cannot work. Ask the ISP for a
   public IP, or rent a small VPS, connect the home server to it with
   WireGuard, and forward UDP 7777 on the VPS to the home server over the
   tunnel (or run the container on the VPS).
3. **DNS**: `pong.luiseel.dev` must be an A record to the home IP. It must
   not go through an HTTP proxy, because the game uses UDP.
4. **Test from outside**: turn off Wi-Fi on a phone, share its mobile data
   as a hotspot, connect a laptop to it, and play an online match (or run
   `PONG_SERVER=pong.luiseel.dev ./bin/net_bot create`). On the server,
   `docker logs pong-server` shows the room.

## Client

The game connects to `pong.luiseel.dev:7777`. To use another server, set
`PONG_SERVER=host[:port]`, for example
`PONG_SERVER=192.168.1.20:7777 ./bin/pong`. The client and the server must
have the same protocol version (`NET_VERSION` in `src/protocol.h`).
