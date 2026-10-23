# Pong

Pong game written in C using SDL2.

https://github.com/user-attachments/assets/cbf3c81e-f0a9-4291-9aa2-cf5d62417355

## Build and Run

```bash
make
./bin/pong
```

## Controls

- Title screen: use W/S or the arrow keys to select an option, then Enter or Space to confirm.
- Play: use W/S for player one and Up/Down for player two.
- Press Escape during a match or on the credits screen to return to the title screen.
- Press Escape on the title screen to quit.

## Power-ups

Glowing pickups spawn at the center and drift toward a player, bouncing off the
top and bottom walls. Catch one with your paddle to gain 8 seconds of powerful
hits: your shots travel at 1.5× speed with a glowing trail. An ordinary return
restores normal speed. Collecting another pickup refreshes your power duration.
The paddle glow and shrinking bar show how much power remains.

Pickups favor the losing player: 65% when one point behind, 80% when two behind,
and 90% when three or more behind. Tied scores give each player a 50% chance.
The direction is chosen when the pickup spawns; you still have to catch it.

Only one pickup appears at a time. The first spawns after 8–14 seconds, with a
new random countdown after each collection or miss. Pickups, active paddle powers,
and the spawn countdown continue across points; each new serve starts at normal
speed. Starting a new match clears all power-ups. Power timers run only during play.

Run the gameplay checks with `make test`.
