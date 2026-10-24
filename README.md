# Pong

Pong game written in C using SDL2.

https://github.com/user-attachments/assets/cbf3c81e-f0a9-4291-9aa2-cf5d62417355

## Build and Run

```bash
make
./bin/pong
```

## Controls

- Press F11 or Alt+Enter (Option+Return on Mac) on any screen to toggle fullscreen. On Mac, you may need fn+F11. The playfield keeps its 4:3 proportions, with bars on wider screens.
- Title screen: use W/S or the arrow keys to select 1 Player, 2 Players, or Credits, then Enter or Space to confirm.
- Match setup: use Left/Right or A/D to choose a winning score from 1–21 (default 5), then Enter or Space to start.
- Each new match and rematch begins with a 3, 2, 1, GO countdown; gameplay and power-up timers stay frozen until it finishes.
- The first player to reach the selected score wins. On the winner screen, use W/S or Up/Down and Enter or Space to choose Rematch or Main menu. Rematches keep the selected winning score.
- Play (2 players): use W/S for player one and Up/Down for player two. Player one uses A to select a stored power-up and D to use it. Player two uses Right to select and Left to use.
- Play (1 player): use W/S or Up/Down for player one. Use A to select a stored power-up, and D or Space to use it. The computer controls the right paddle. It moves a little slower than you and has some aim error. It also goes after pickups that drift toward it when the ball is not a threat. It uses growth immediately and powerful hits when the ball comes toward it.
- Press Escape during a match to pause. Scores, positions, and power-up timers stay frozen. Press Escape again to resume, or use W/S or Up/Down and Enter or Space to choose Resume or Main Menu.
- Press Escape during setup, the countdown, or on the winner or credits screen to return to the title screen.
- Press Escape on the title screen to quit.

## Power-ups

Glowing pickups spawn at the center and drift toward a player, bouncing off the
top and bottom walls. Catch a pickup with your paddle to store it. Each player can store
up to 3 power-ups. A pickup caught with 3 stored power-ups is lost. The slots below each
score show the stored power-ups; the outlined slot is the selected one. Select a slot,
then use it to start its effect.

A gold power-up gives 8 seconds of powerful hits: your shots travel at 1.5× speed
with a glowing trail. An ordinary return restores normal speed. Using another power-up of the same type refreshes its duration.
The paddle glow and shrinking bar show how much power remains.

Gold pickups favor the losing player: 65% when one point behind, 80% when two behind,
and 90% when three or more behind. Tied scores give each player a 50% chance.
The direction is chosen when the pickup spawns; you still have to catch it.

Blue power-ups make your paddle twice as tall for 8 seconds. They account for
roughly 20% of spawns; green and pink pickups account for 20% each and gold pickups for the other 40%. Blue pickups
alternate between players, starting with a randomly chosen player each match,
regardless of score or whether the previous pickup was caught. You still need
to catch them. A blue outline and timer bar show the remaining growth time.
Growth and powerful hits can be active together; repeated growth refreshes the
timer without increasing the paddle further.

Green power-ups give 8 seconds of serpent hits: your shots weave up and down in a
wave as they cross the field, with a green trail. An ordinary return straightens
the ball again. Green pickups alternate between players in the same way as blue
pickups. A green timer bar beside the paddle shows the remaining time. Serpent and
powerful hits can be active together: the ball then weaves and moves at 1.5× speed.
The computer uses a serpent power-up when the ball comes toward it.

Pink power-ups give one charge blast. Use it to arm your paddle for 8 seconds. A pink
bar behind the paddle shows the time left. If no ball hits the paddle in
that time, the charge is lost. The next ball that hits the armed paddle sticks to its
center and follows the paddle for 3 seconds while it pulses faster. Then the ball
shoots straight ahead at 2.2× speed with a pink trail. A return by your opponent
restores normal speed. With powerful hits active, the charge speed is used; with
serpent hits active, the charged shot also weaves. Other timers and pickups continue
during the hold. Pink pickups alternate between players in the same way as blue
pickups. The computer arms a charge when the ball comes toward it.

Normal play spawns one pickup at a time. The first spawns after 8–14 seconds, with a
new random countdown after each collection or miss. Pickups, active paddle powers,
stored power-ups, and the spawn countdown continue across points; each new serve starts
at normal speed. Starting a new match clears all power-ups. Power timers run only during play.

## Debug menu

Build and start the debug version with `make debug && ./bin/pong-debug` to turn on
the debug menu. During a match, press F1 to open it (on a Mac, hold fn unless F-keys
are set as standard function keys); play stays frozen while it is open. Use W/S or
Up/Down to choose a speed, grow, serpent, or charge power-up, and Left/Right or A/D to choose the
player it drifts toward. Press Enter or Space to spawn it at the center. Each spawn
adds a pickup; existing pickups keep moving and can be collected independently.
Debug spawning does not change the normal spawn countdown; the countdown restarts
after the last field pickup is collected or missed. Press G to
give it directly to the target player's inventory and select it, ready to use. If the
inventory is full, this replaces its selected slot. Giving leaves field pickups and
the spawn countdown unchanged. Both actions resume play. Press Escape or F1 to close
the menu without giving or spawning. In the normal build, F1 does nothing.

Run the gameplay checks with `make test`.
