//! A room: two players and the authoritative match between them
const std = @import("std");
const c = @import("c");
const protocol = @import("protocol.zig");
const log = @import("log.zig");

const Room = @This();

pub const code_length = c.NET_CODE_LENGTH;
const pending_slots = 16; // inputs buffered per player, indexed by seq
const max_backlog = 4; // inputs allowed to wait; more and the oldest are skipped
const tick_seconds: f32 = 1.0 / @as(f32, @floatFromInt(c.NET_TICK_RATE));
/// A room without a match on (waiting for the second player, or after the
/// match) goes after this long
pub const idle_ms: u32 = 10 * 60 * 1000;
pub const max_fake_lag_ms = 1000;

/// Fake latency and loss for tests: on the snapshots out and the inputs in
pub const Fake = struct {
    lag_ms: u32 = 0,
    loss_percent: u8 = 0,
    random: std.Random.DefaultPrng = .init(0),

    fn lost(self: *Fake) bool {
        return self.loss_percent > 0 and self.random.random().uintLessThan(u8, 100) < self.loss_percent;
    }
};

pub var fake: Fake = .{};

/// A fixed queue of packets held back by the fake lag, oldest first. When it
/// is full the oldest is lost.
fn Delayed(comptime T: type, comptime capacity: usize) type {
    return struct {
        const Self = @This();
        items: [capacity]struct { due: u32, value: T } = undefined,
        first: usize = 0,
        count: usize = 0,

        pub fn push(queue: *Self, due: u32, value: T) void {
            if (queue.count == capacity) {
                queue.first = (queue.first + 1) % capacity;
                queue.count -= 1;
            }
            queue.items[(queue.first + queue.count) % capacity] = .{ .due = due, .value = value };
            queue.count += 1;
        }

        /// The oldest when it is due
        pub fn pop(queue: *Self, now: u32) ?T {
            if (queue.count == 0) return null;
            const item = queue.items[queue.first];
            if (@as(i32, @bitCast(now -% item.due)) < 0) return null;
            queue.first = (queue.first + 1) % capacity;
            queue.count -= 1;
            return item.value;
        }
    };
}

pub const Client = struct {
    peer: *c.ENetPeer,
    room: ?*Room = null,
    side: u8 = 0, // 1 or 2 once in a room
    connected_at: u32 = 0, // ENet ms
    said_hello: bool = false, // a HELLO was handled, with a room or an error
    limiter: protocol.RateLimiter = .{},
    bad_packets: u32 = 0,
    delayed_inputs: Delayed(c.net_input_t, 2 * max_fake_lag_ms * c.NET_TICK_RATE / 1000) = .{},
    delayed_snapshots: Delayed(c.net_snapshot_t, max_fake_lag_ms * c.NET_TICK_RATE / c.NET_SNAPSHOT_INTERVAL / 1000 + 2) = .{},

    pub const hello_timeout_ms: u32 = 5000;
    pub const max_bad_packets = 20;

    /// Connected without a HELLO for too long
    pub fn helloOverdue(client: *const Client, now: u32) bool {
        return !client.said_hello and protocol.elapsed(now, client.connected_at, hello_timeout_ms);
    }

    /// Counts a malformed packet; true when the client has sent too many
    pub fn badPacket(client: *Client) bool {
        client.bad_packets += 1;
        return client.bad_packets > max_bad_packets;
    }

    /// An input that came in, after the fake lag and loss
    pub fn receiveInput(client: *Client, now: u32, message: *const c.net_input_t) void {
        if (fake.lost()) return;
        if (fake.lag_ms > 0) return client.delayed_inputs.push(now +% fake.lag_ms, message.*);
        if (client.room) |room| room.receiveInput(client, message);
    }

    fn sendSnapshot(client: *Client, now: u32, message: c.net_snapshot_t) void {
        if (fake.lost()) return;
        if (fake.lag_ms > 0) return client.delayed_snapshots.push(now +% fake.lag_ms, message);
        protocol.sendState(client.peer, message);
    }

    /// Lets the held packets go when they are due
    pub fn flushDelayed(client: *Client, now: u32) void {
        while (client.delayed_snapshots.pop(now)) |message| protocol.sendState(client.peer, message);
        while (client.delayed_inputs.pop(now)) |message| {
            if (client.room) |room| room.receiveInput(client, &message);
        }
    }
};

const Slot = struct {
    valid: bool = false,
    seq: u16 = 0,
    input: c.sim_input_t = .{ .move = 0, .actions = 0 },
};

/// One player's seat and the inputs waiting to be applied
pub const Side = struct {
    client: ?*Client = null,
    pending: [pending_slots]Slot = @splat(.{}),
    last_seq: u16 = 0,
    has_input: bool = false,
    last_move: i8 = 0,
    wants_rematch: bool = false,

    /// A new match: the client's seqs start over
    pub fn reset(side: *Side) void {
        side.* = .{ .client = side.client };
    }

    /// Keeps an input unless it is old or already here
    pub fn offer(side: *Side, seq: u16, input: c.sim_input_t) void {
        if (side.has_input and !protocol.seqNewer(seq, side.last_seq)) return;
        const slot = &side.pending[seq % pending_slots];
        if (slot.valid and slot.seq == seq) return;
        slot.* = .{ .valid = true, .seq = seq, .input = input };
    }

    fn oldestPending(side: *Side) ?*Slot {
        var best: ?*Slot = null;
        for (&side.pending) |*slot| {
            if (!slot.valid) continue;
            if (side.has_input and !protocol.seqNewer(slot.seq, side.last_seq)) {
                slot.valid = false; // stale
                continue;
            }
            if (best == null or protocol.seqNewer(best.?.seq, slot.seq)) best = slot;
        }
        return best;
    }

    fn countPending(side: *Side) usize {
        var count: usize = 0;
        for (side.pending) |slot| {
            if (slot.valid) count += 1;
        }
        return count;
    }

    /// The input for this tick: the next in seq order, or the last move
    /// repeated without its actions when none arrived
    pub fn next(side: *Side) c.sim_input_t {
        var slot = side.oldestPending();
        // A client that runs ahead would add latency; skip to catch up
        while (slot != null and side.countPending() > max_backlog) {
            slot.?.valid = false;
            slot = side.oldestPending();
        }
        const taken = slot orelse return .{ .move = side.last_move, .actions = 0 };
        taken.valid = false;
        side.last_seq = taken.seq;
        side.has_input = true;
        side.last_move = taken.input.move;
        return taken.input;
    }
};

active: bool = false,
code: [code_length]u8 = @splat(' '),
idle_since: u32 = 0, // ENet ms when the room opened or its last match ended
sides: [2]Side = .{ .{}, .{} },
started: bool = false,
finished: bool = false,
target_score: u16 = 0,
tick: u32 = 0, // counts from the START; the sim steps from NET_COUNTDOWN_TICKS on
event_counts: [c.NET_EVENT_COUNTS]u8 = @splat(0), // SIM_EVENT_* bits seen, by bit
sim: c.sim_state_t = undefined,

pub fn open(room: *Room, code: [code_length]u8, creator: *Client, now: u32) void {
    room.* = .{ .active = true, .code = code, .idle_since = now };
    room.seat(creator, 1);
}

fn seat(room: *Room, client: *Client, side: u8) void {
    room.sides[side - 1].client = client;
    client.room = room;
    client.side = side;
    protocol.sendControl(client.peer, c.net_welcome_t{
        .type = c.NET_MSG_WELCOME,
        .side = side,
        .code = room.code,
        .reserved = 0,
    });
}

pub fn isFull(room: *const Room) bool {
    return room.sides[1].client != null or room.finished;
}

/// The second player joins and the match starts
pub fn join(room: *Room, client: *Client, seed: u64, target_score: u16) void {
    room.seat(client, 2);
    room.target_score = target_score;
    room.start(seed);
}

/// A match from the top, for both players in the room
pub fn start(room: *Room, seed: u64) void {
    for (&room.sides) |*side| side.reset();
    c.sim_init(&room.sim, seed, room.target_score);
    room.tick = 0;
    room.event_counts = @splat(0);
    room.started = true;
    room.finished = false;
    const message = c.net_start_t{
        .type = c.NET_MSG_START,
        .reserved = 0,
        .target_score = room.target_score,
        .countdown_ticks = c.NET_COUNTDOWN_TICKS,
    };
    for (room.sides) |side| protocol.sendControl(side.client.?.peer, message);
}

/// True when both players asked for one after a match with a winner
pub fn requestRematch(room: *Room, client: *Client) bool {
    if (!room.started or !room.finished or room.sim.winner == 0) return false;
    room.sides[client.side - 1].wants_rematch = true;
    for (room.sides) |side| {
        if (side.client == null or !side.wants_rematch) return false;
    }
    return true;
}

/// True when the room has had no match on for idle_ms
pub fn expired(room: *const Room, now: u32) bool {
    return room.active and (!room.started or room.finished) and protocol.elapsed(now, room.idle_since, idle_ms);
}

/// The other player hears of it. A player who leaves a match that is on
/// (quits or goes silent) forfeits; after the match no rematch can come.
pub fn leave(room: *Room, client: *Client, now: u32) void {
    const side = client.side;
    room.sides[side - 1].client = null;
    client.room = null;
    if (room.started) {
        const forfeit = !room.finished;
        room.finished = true;
        room.idle_since = now;
        const winner: u8 = 3 - side;
        if (forfeit) log.info("room {s}: side {d} forfeits, side {d} wins", .{ room.code, side, winner });
        if (room.sides[winner - 1].client) |other| {
            if (forfeit) {
                protocol.sendControl(other.peer, room.gameOver(winner, c.NET_OVER_FORFEIT));
            } else {
                protocol.sendControl(other.peer, c.net_header_t{ .type = c.NET_MSG_OPPONENT_LEFT });
            }
        }
    }
    if (room.sides[0].client == null and room.sides[1].client == null) room.active = false;
}

/// Closes the room; its players get the error and are let go
pub fn close(room: *Room, reason: u8) void {
    for (&room.sides) |*side| {
        const client = side.client orelse continue;
        client.room = null;
        side.client = null;
        protocol.sendError(client.peer, reason);
    }
    room.active = false;
}

fn gameOver(room: *const Room, winner: u8, reason: u8) c.net_game_over_t {
    return .{
        .type = c.NET_MSG_GAME_OVER,
        .winner = winner,
        .reason = reason,
        .reserved = 0,
        .player_one_score = @intCast(std.math.clamp(room.sim.player_one_score, 0, std.math.maxInt(u16))),
        .player_two_score = @intCast(std.math.clamp(room.sim.player_two_score, 0, std.math.maxInt(u16))),
    };
}

pub fn receiveInput(room: *Room, client: *Client, message: *const c.net_input_t) void {
    if (!room.started or room.finished) return;
    const count = @min(message.count, c.NET_INPUT_HISTORY); // decode checked it
    const side = &room.sides[client.side - 1];
    // The oldest first, so the ring keeps the newest when it overflows
    var i = count;
    while (i > 0) {
        i -= 1;
        side.offer(message.inputs[i].seq, message.inputs[i].input);
    }
}

/// One server tick: apply inputs, step the match, publish. During the
/// countdown the clock runs and the match waits.
pub fn step(room: *Room, now: u32) void {
    if (!room.started or room.finished) return;
    if (room.tick < c.NET_COUNTDOWN_TICKS) {
        room.tick += 1;
        if (room.tick % c.NET_SNAPSHOT_INTERVAL == 0) room.sendSnapshots(now);
        return;
    }
    const inputs = [2]c.sim_input_t{ room.sides[0].next(), room.sides[1].next() };
    c.sim_step(&room.sim, &inputs, tick_seconds);
    room.tick += 1;
    countEvents(&room.event_counts, room.sim.events);
    if (room.sim.winner != 0) {
        room.finished = true;
        room.idle_since = now;
        room.sendSnapshots(now);
        const over = room.gameOver(@intCast(room.sim.winner), c.NET_OVER_SCORE);
        for (room.sides) |side| {
            if (side.client) |client| protocol.sendControl(client.peer, over);
        }
        log.info("room {s}: side {d} won {d}-{d}", .{
            room.code, over.winner, over.player_one_score, over.player_two_score,
        });
        return;
    }
    if (room.tick % c.NET_SNAPSHOT_INTERVAL == 0) room.sendSnapshots(now);
}

fn sendSnapshots(room: *Room, now: u32) void {
    for (room.sides, 0..) |side, i| {
        const client = side.client orelse continue;
        const opponent = room.sides[1 - i].client;
        const silent = if (opponent) |other| protocol.elapsed(now, other.peer.lastReceiveTime, c.NET_SILENT_MS) else true;
        client.sendSnapshot(now, c.net_snapshot_t{
            .type = c.NET_MSG_SNAPSHOT,
            .flags = if (silent) c.NET_SNAPSHOT_OPPONENT_SILENT else 0,
            .input_seq = side.last_seq,
            .tick = room.tick,
            .event_counts = room.event_counts,
            .state = room.sim,
        });
    }
}

/// One count per event bit; they wrap, the client looks at the change
fn countEvents(counts: *[c.NET_EVENT_COUNTS]u8, events: u32) void {
    for (counts, 0..) |*count, bit| {
        if (events & (@as(u32, 1) << @intCast(bit)) != 0) count.* +%= 1;
    }
}

test "inputs apply in seq order, once, and the move repeats" {
    var side = Side{};
    side.offer(2, .{ .move = 20, .actions = 0 });
    side.offer(1, .{ .move = 10, .actions = c.SIM_ACTION_USE });
    side.offer(1, .{ .move = 99, .actions = 0 }); // duplicate seq is ignored
    try std.testing.expectEqual(@as(i8, 10), side.next().move);
    try std.testing.expectEqual(@as(i8, 20), side.next().move);
    const repeated = side.next();
    try std.testing.expectEqual(@as(i8, 20), repeated.move);
    try std.testing.expectEqual(@as(u8, 0), repeated.actions);
    side.offer(1, .{ .move = 50, .actions = 0 }); // old
    try std.testing.expectEqual(@as(i8, 20), side.next().move);
    try std.testing.expectEqual(@as(u16, 2), side.last_seq);
}

test "a backlog is skipped" {
    var side = Side{};
    for (0..8) |i| side.offer(@intCast(i), .{ .move = @intCast(i), .actions = 0 });
    const input = side.next();
    try std.testing.expect(input.move >= 3);
    try std.testing.expect(side.countPending() <= max_backlog);
}

test "event counts move once per event and wrap" {
    var counts: [c.NET_EVENT_COUNTS]u8 = @splat(0);
    countEvents(&counts, c.SIM_EVENT_PAD_HIT | c.SIM_EVENT_GOAL);
    countEvents(&counts, c.SIM_EVENT_PAD_HIT);
    try std.testing.expectEqual(@as(u8, 2), counts[0]);
    try std.testing.expectEqual(@as(u8, 0), counts[1]);
    try std.testing.expectEqual(@as(u8, 1), counts[2]);
    counts[0] = 255;
    countEvents(&counts, c.SIM_EVENT_PAD_HIT);
    try std.testing.expectEqual(@as(u8, 0), counts[0]);
}

test "a side reset keeps the seat and drops the inputs" {
    var side = Side{};
    side.offer(3, .{ .move = 10, .actions = 0 });
    _ = side.next();
    side.wants_rematch = true;
    side.reset();
    try std.testing.expect(!side.has_input and !side.wants_rematch);
    try std.testing.expectEqual(@as(usize, 0), side.countPending());
    try std.testing.expectEqual(@as(i8, 0), side.next().move);
}

test "a room expires only when no match is on, and the clock wraps" {
    const t0: u32 = std.math.maxInt(u32) - 1000;
    var room = Room{ .active = true, .idle_since = t0 };
    try std.testing.expect(!room.expired(t0 +% idle_ms - 1));
    try std.testing.expect(room.expired(t0 +% idle_ms));
    room.started = true; // a match on never expires
    try std.testing.expect(!room.expired(t0 +% 2 * idle_ms));
    room.finished = true;
    try std.testing.expect(room.expired(t0 +% 2 * idle_ms));
    room.active = false;
    try std.testing.expect(!room.expired(t0 +% 2 * idle_ms));
}

test "a client without a HELLO is overdue after 5 s" {
    var peer: c.ENetPeer = undefined;
    var client = Client{ .peer = &peer, .connected_at = 100 };
    try std.testing.expect(!client.helloOverdue(100 + Client.hello_timeout_ms - 1));
    try std.testing.expect(client.helloOverdue(100 + Client.hello_timeout_ms));
    client.said_hello = true;
    try std.testing.expect(!client.helloOverdue(100 + 10 * Client.hello_timeout_ms));
}

test "too many bad packets" {
    var peer: c.ENetPeer = undefined;
    var client = Client{ .peer = &peer };
    for (0..Client.max_bad_packets) |_| try std.testing.expect(!client.badPacket());
    try std.testing.expect(client.badPacket());
}

test "delayed packets come out in order when due, and a full queue drops the oldest" {
    var queue: Delayed(u32, 3) = .{};
    queue.push(10, 1);
    queue.push(20, 2);
    try std.testing.expectEqual(@as(?u32, null), queue.pop(9));
    try std.testing.expectEqual(@as(?u32, 1), queue.pop(10));
    try std.testing.expectEqual(@as(?u32, null), queue.pop(19));
    queue.push(30, 3);
    queue.push(40, 4);
    queue.push(50, 5); // full: 2 goes
    try std.testing.expectEqual(@as(?u32, 3), queue.pop(100));
    try std.testing.expectEqual(@as(?u32, 4), queue.pop(100));
    try std.testing.expectEqual(@as(?u32, 5), queue.pop(100));
    try std.testing.expectEqual(@as(?u32, null), queue.pop(100));
}
