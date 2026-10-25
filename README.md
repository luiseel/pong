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
- Match setup: use Up/Down or W/S to select the winning score or a player’s device; Left/Right or A/D changes it. The winning score ranges from 1–21 (default 5). Press Enter or Space to start.
- Each new match and rematch begins with a 3, 2, 1, GO countdown; gameplay and power-up timers stay frozen until it finishes.
- The first player to reach the selected score wins. On the winner screen, use W/S or Up/Down and Enter or Space to choose Rematch or Main menu. Rematches keep the selected winning score.
- Play (2 players): use W/S for player one and Up/Down for player two. Player one uses A to select a stored power-up and D to use it. Player two uses Right to select and Left to use.
- Play (1 player): use W/S or Up/Down for player one. Use A to select a stored power-up, and D or Space to use it. The computer controls the right paddle. It moves a little slower than you and has some aim error. It also goes after pickups that drift toward it when the ball is not a threat. It uses growth immediately and powerful hits when the ball comes toward it.
- Press Escape during a match to pause. Scores, positions, and power-up timers stay frozen. Press Escape again to resume, or use W/S or Up/Down and Enter or Space to choose Resume or Main Menu.
- Press Escape during setup, the countdown, or on the winner or credits screen to return to the title screen.
- Press Escape on the title screen to quit.

## Online play

Choose Online on the title screen. Create Room gives a 5-letter room code; tell it
to your friend, who chooses Join Room and types it. The creator plays the left
paddle. Matches run on the server at `pong.luiseel.dev` (UDP port 7777), so both
players need internet access but no port forwarding. There is no pause online.

- A "CONNECTION PROBLEM" hint shows when packets stop arriving for 2 seconds.
- A player who quits, or whose connection stays silent for 10 seconds, forfeits:
  the opponent sees "YOU WIN!" and "OPPONENT DISCONNECTED".
- After a match, both players choose Rematch to play again.

To use another server, set `PONG_SERVER=host[:port]`, for example
`PONG_SERVER=localhost:7777 ./bin/pong`. To run your own server, see
[server/README.md](server/README.md).

## Gamepads

Use a Retroflag Classic USB Gamepad, Switch Pro controller (USB or paired Bluetooth),
or another SDL2-mapped controller. In match setup, select each player's device:
keyboard, a connected controller, or one of each. Two players can share the
keyboard using the existing controls; a controller can belong to only one player.
Assignments stay in place for rematches. The computer needs no device in solo play.

- D-pad: move the paddle or navigate menus. The left stick also works, with a 20%
  dead zone and proportional paddle speed. D-pad movement uses full speed.
- A: confirm in menus; use the selected power-up during play.
- B: go back in menus (quits from the title screen).
- L / left shoulder: select the next stored power-up.
- Start / +: pause or resume; also pauses the opening countdown.

Switch Pro face buttons follow their printed labels. Other devices use their SDL
mapping; verify the Retroflag's labels when first connecting it. Keyboard gameplay
controls apply only to players assigned to the keyboard. Keyboard menu controls
remain available, and any connected controller can navigate menus.

Devices can be connected while the game is running. Disconnecting an assigned
controller pauses the match or countdown. Reconnect it, or use Up/Down and
Left/Right in the pause menu to assign a replacement (including the keyboard),
then explicitly resume. Switching between USB and Bluetooth may appear as a new
device and require reassignment. The countdown resumes where it stopped.

If a controller is not listed, check the terminal for its name and GUID. The game
loads optional SDL mappings from `gamecontrollerdb.txt` in the working directory;
add a mapping for that exact device and restart. No raw button numbers are assumed
for unrecognized devices.

`make test` includes virtual-controller checks (SDL 2.24 or newer). Physical button
labels, USB/Bluetooth transport, and stick feel still need checking on the devices.

## Power-ups

Glowing pickups spawn at the center and drift toward a player, bouncing off the
top and bottom walls. Catch a pickup with your paddle to store it. Each player can store
up to 3 power-ups. A pickup caught with 3 stored power-ups is lost. The slots below each
score show the stored power-ups; the outlined slot is the selected one. Select a slot,
then use it to start its effect.

A gold power-up gives 8 seconds of powerful hits: your shots travel at 2.5× speed
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
powerful hits can be active together: the ball then weaves and moves at 2.5× speed.
The computer uses a serpent power-up when the ball comes toward it.

Pink power-ups give one charge blast. Use it to arm your paddle for 8 seconds. A pink
bar behind the paddle shows the time left. If no ball hits the paddle in
that time, the charge is lost. The next ball that hits the armed paddle sticks to its
center and follows the paddle for 3 seconds while it pulses faster. Then the ball
shoots at 2.2× speed in the direction the paddle moves at release, up to 45°, with a pink
trail; a still paddle shoots straight ahead. A return by your opponent
restores normal speed. With powerful hits active, the faster powerful-hit speed is used; with
serpent hits active, the charged shot also weaves. Other timers and pickups continue
during the hold. Pink pickups alternate between players in the same way as blue
pickups. The computer arms a charge when the ball comes toward it.

Normal play spawns one pickup at a time. The first spawns after 4–8 seconds, with a
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
