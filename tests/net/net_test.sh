#!/bin/sh
# Starts three servers on high ports and checks them with the headless bots.
# Main server: a full match and a rematch, the error replies, a forfeit when
# a player quits, a match with fake latency and loss on the bots. Lag server
# (--fake-lag-ms, --fake-loss): two matches. Robustness server: a forfeit
# when a player goes silent (SIGKILL), and peers without a HELLO, with
# malformed packets, or with a flood of them get dropped while a match goes
# on. The lag and robustness runs go in the background, so the whole test
# takes about as long as the main run.
# Run from the repo root after `make bin/net_bot` and `cd server && zig build`.
set -u

BOT=./bin/net_bot
SERVER=./server/zig-out/bin/pong-server
PORT=$((20000 + $$ % 20000))
LAG_PORT=$((PORT + 1))
ROBUST_PORT=$((PORT + 2))
WORK=$(mktemp -d)
trap 'kill $SERVER_PID $LAG_SERVER_PID $ROBUST_SERVER_PID 2>/dev/null; rm -rf "$WORK"' EXIT

fail() {
    echo "net-test: FAIL: $*" >&2
    exit 1
}

# Starts a create bot in the background; its pid is in CREATE_PID and its
# room code in CODE. Runs in the main shell so that wait sees the bot.
create_room() {
    name=$1
    shift
    "$BOT" create "$@" >"$WORK/$name.out" 2>"$WORK/$name.err" &
    CREATE_PID=$!
    for _ in $(seq 1 100); do
        CODE=$(head -n 1 "$WORK/$name.out")
        [ -n "$CODE" ] && return 0
        sleep 0.1
    done
    fail "the create bot got no room code: $(cat "$WORK/$name.err")"
}

# Two bots play two matches (a rematch); $1 names the run
two_matches() {
    create_room "$1-one" --rematch
    echo "net-test: $1: room $CODE"
    "$BOT" join "$CODE" --rematch >"$WORK/$1-two.out" 2>"$WORK/$1-two.err"
    TWO_STATUS=$?
    wait $CREATE_PID
    ONE_STATUS=$?
    ONE=$(grep '^winner' "$WORK/$1-one.out")
    TWO=$(grep '^winner' "$WORK/$1-two.out")
    echo "net-test: $1: bot one ($ONE_STATUS): $(echo $ONE)"
    echo "net-test: $1: bot two ($TWO_STATUS): $(echo $TWO)"
    echo "net-test: $1: $(grep '^stats' "$WORK/$1-one.out")"
    echo "net-test: $1: $(grep '^stats' "$WORK/$1-two.out")"
    [ "$ONE_STATUS" = 0 ] && [ "$TWO_STATUS" = 0 ] || fail "a bot did not finish: $(cat "$WORK/$1-one.err" "$WORK/$1-two.err")"
    [ "$(echo "$ONE" | wc -l)" -eq 2 ] || fail "expected two matches"
    [ "$ONE" = "$TWO" ] || fail "the bots saw different results"
}

# Time in seconds since the epoch
now() {
    date +%s
}

# Checks a misbehaving bot ($1 names it, $2 its status) was dropped
expect_dropped() {
    echo "net-test: $1 ($2): $(cat "$WORK/$1.out")"
    [ "$2" = 0 ] && grep -q "^dropped" "$WORK/$1.out" || fail "$1: the server did not drop it: $(cat "$WORK/$1.err")"
}

# A player goes silent mid-match and forfeits; misbehaving peers connect at
# the same time. Runs in a subshell against the robustness server.
robustness() {
    export PONG_SERVER="127.0.0.1:$ROBUST_PORT"
    create_room forfeit-one
    "$BOT" join "$CODE" >"$WORK/forfeit-two.out" 2>"$WORK/forfeit-two.err" &
    TWO_PID=$!
    sleep 1
    "$BOT" no-hello >"$WORK/no-hello.out" 2>"$WORK/no-hello.err" &
    NO_HELLO_PID=$!
    "$BOT" garbage >"$WORK/garbage.out" 2>"$WORK/garbage.err" &
    GARBAGE_PID=$!
    "$BOT" flood >"$WORK/flood.out" 2>"$WORK/flood.err" &
    FLOOD_PID=$!
    sleep 4 # the countdown is over, the match is on
    kill -9 $TWO_PID
    KILLED_AT=$(now)
    wait $TWO_PID 2>/dev/null
    wait $CREATE_PID
    STATUS=$?
    ELAPSED=$(($(now) - KILLED_AT))
    echo "net-test: forfeit ($STATUS) after ${ELAPSED} s: $(grep -v '^[A-Z0-9]*$' "$WORK/forfeit-one.out" | tr '\n' ' ')"
    [ "$STATUS" = 7 ] && grep -q "^winner 1 .*forfeit" "$WORK/forfeit-one.out" ||
        fail "expected a forfeit win for bot one: $(cat "$WORK/forfeit-one.err")"
    [ "$ELAPSED" -ge 8 ] && [ "$ELAPSED" -le 13 ] || fail "the forfeit came after $ELAPSED s, not about 10 s"
    grep -q "^opponent silent" "$WORK/forfeit-one.out" || fail "the snapshots did not flag the silent opponent"

    wait $NO_HELLO_PID
    expect_dropped no-hello $?
    DROPPED_MS=$(sed -n 's/^dropped after \([0-9]*\) ms/\1/p' "$WORK/no-hello.out")
    [ "$DROPPED_MS" -ge 4500 ] && [ "$DROPPED_MS" -le 7000 ] || fail "no-hello: dropped after $DROPPED_MS ms, not about 5 s"
    wait $GARBAGE_PID
    expect_dropped garbage $?
    wait $FLOOD_PID
    expect_dropped flood $?
    grep -q "dropped: bad packets" "$WORK/robust-server.err" || fail "the server did not log the bad packets"
    grep -q "dropped: flooding" "$WORK/robust-server.err" || fail "the server did not log the flood"

    # The server is still up and answers
    "$BOT" join ZZZZZ >"$WORK/robust-none.out" 2>"$WORK/robust-none.err"
    STATUS=$?
    [ "$STATUS" = 2 ] && grep -q "ROOM NOT FOUND" "$WORK/robust-none.err" || fail "the robustness server stopped answering"
    echo "net-test: robustness: PASS"
}

"$SERVER" --port "$PORT" --target-score 3 2>"$WORK/server.err" &
SERVER_PID=$!
"$SERVER" --port "$LAG_PORT" --target-score 2 --fake-lag-ms 50 --fake-loss 5 2>"$WORK/lag-server.err" &
LAG_SERVER_PID=$!
"$SERVER" --port "$ROBUST_PORT" --target-score 21 2>"$WORK/robust-server.err" &
ROBUST_SERVER_PID=$!
export PONG_SERVER="127.0.0.1:$PORT"
sleep 0.3
kill -0 $SERVER_PID 2>/dev/null || fail "the server did not start: $(cat "$WORK/server.err")"
kill -0 $LAG_SERVER_PID 2>/dev/null || fail "the lag server did not start: $(cat "$WORK/lag-server.err")"
kill -0 $ROBUST_SERVER_PID 2>/dev/null || fail "the robustness server did not start: $(cat "$WORK/robust-server.err")"

echo "net-test: servers on $PONG_SERVER, lag server on $LAG_PORT, robustness server on $ROBUST_PORT"

(PONG_SERVER="127.0.0.1:$LAG_PORT" two_matches server-lag) >"$WORK/server-lag.log" 2>&1 &
LAG_RUN_PID=$!
robustness >"$WORK/robustness.log" 2>&1 &
ROBUST_RUN_PID=$!

# 1. A full match between two bots, then a rematch
two_matches clean
grep -q "rematch started" "$WORK/server.err" || fail "the server did not log the rematch"

# 2. A code that no room has
"$BOT" join ZZZZZ >"$WORK/none.out" 2>"$WORK/none.err"
STATUS=$?
echo "net-test: unknown code ($STATUS): $(cat "$WORK/none.err")"
[ "$STATUS" = 2 ] && grep -q "ROOM NOT FOUND" "$WORK/none.err" || fail "expected ROOM NOT FOUND"

# 3. A third client to a full room, then one player quits mid-match: a forfeit
create_room three
"$BOT" join "$CODE" >"$WORK/four.out" 2>"$WORK/four.err" &
four_PID=$!
sleep 0.3
"$BOT" join "$CODE" >"$WORK/five.out" 2>"$WORK/five.err"
STATUS=$?
kill $four_PID # the match is less than a second old
echo "net-test: full room ($STATUS): $(cat "$WORK/five.err")"
[ "$STATUS" = 2 ] && grep -q "ROOM IS FULL" "$WORK/five.err" || fail "expected ROOM IS FULL"
wait $four_PID 2>/dev/null
wait $CREATE_PID
STATUS=$?
echo "net-test: quit ($STATUS): $(tail -n 1 "$WORK/three.out")"
[ "$STATUS" = 7 ] && grep -q "^winner 1 .*forfeit" "$WORK/three.out" || fail "expected the other bot to win by forfeit"

# 4. The same with 80 ms of fake lag each way and 10 % loss on both bots
PONG_FAKE_LAG_MS=80 PONG_FAKE_LOSS=10 two_matches lossy
BIG=$(grep '^stats' "$WORK/lossy-one.out" | sed 's/.*big \([0-9]*\).*/\1/')
echo "net-test: lossy: $BIG big corrections on bot one"

# 5. The background runs
wait $LAG_RUN_PID
LAG_STATUS=$?
cat "$WORK/server-lag.log"
[ "$LAG_STATUS" = 0 ] || fail "the run against the lag server failed"
wait $ROBUST_RUN_PID
ROBUST_STATUS=$?
cat "$WORK/robustness.log"
[ "$ROBUST_STATUS" = 0 ] || fail "the robustness run failed"

for name in server lag-server robust-server; do
    echo "net-test: $name log:"
    sed 's/^/  /' "$WORK/$name.err"
done
echo "net-test: PASS"
