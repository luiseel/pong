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
- Match setup: use Left/Right or A/D to choose a winning score from 1–21 (default 5), then Enter or Space to start.
- Each new match and rematch begins with a 3, 2, 1, GO countdown; gameplay and power-up timers stay frozen until it finishes.
- The first player to reach the selected score wins. On the winner screen, use W/S or Up/Down and Enter or Space to choose Rematch or Main menu. Rematches keep the selected winning score.
- Play: use W/S for player one and Up/Down for player two.
- Press Escape during a match to pause. Scores, positions, and power-up timers stay frozen. Press Escape again to resume, or use W/S or Up/Down and Enter or Space to choose Resume or Main Menu.
- Press Escape during setup, the countdown, or on the winner or credits screen to return to the title screen.
- Press Escape on the title screen to quit.

## Power-ups

Glowing pickups spawn at the center and drift toward a player, bouncing off the
top and bottom walls. Catch a gold pickup with your paddle to gain 8 seconds of powerful
hits: your shots travel at 1.5× speed with a glowing trail. An ordinary return
restores normal speed. Collecting another pickup of the same type refreshes its duration.
The paddle glow and shrinking bar show how much power remains.

Gold pickups favor the losing player: 65% when one point behind, 80% when two behind,
and 90% when three or more behind. Tied scores give each player a 50% chance.
The direction is chosen when the pickup spawns; you still have to catch it.

Blue pickups make your paddle twice as tall for 8 seconds. They account for
roughly 25% of spawns; gold pickups account for the other 75%. Blue pickups
alternate between players, starting with a randomly chosen player each match,
regardless of score or whether the previous pickup was caught. You still need
to catch them. A blue outline and timer bar show the remaining growth time.
Growth and powerful hits can be active together; repeated growth refreshes the
timer without increasing the paddle further.

Only one pickup appears at a time. The first spawns after 8–14 seconds, with a
new random countdown after each collection or miss. Pickups, active paddle powers,
and the spawn countdown continue across points; each new serve starts at normal
speed. Starting a new match clears all power-ups. Power timers run only during play.

Run the gameplay checks with `make test`.
