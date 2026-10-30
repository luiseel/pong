//! Packets in and out: the structs from shared/protocol.h go on the wire as they
//! are in memory (little-endian, same layout on both sides). Nothing from a
//! client is trusted: decode checks the channel, the size and the fields.
const std = @import("std");
const c = @import("c");

comptime {
    // The wire format is the in-memory layout, so only little-endian hosts work
    std.debug.assert(@import("builtin").cpu.arch.endian() == .little);
}

pub const reliable: u32 = @intCast(c.ENET_PACKET_FLAG_RELIABLE);

fn send(peer: *c.ENetPeer, channel: u8, flags: u32, value: anytype) void {
    const bytes = std.mem.asBytes(&value);
    const packet = c.enet_packet_create(bytes.ptr, bytes.len, flags) orelse return;
    if (c.enet_peer_send(peer, channel, packet) < 0) c.enet_packet_destroy(packet);
}

pub fn sendControl(peer: *c.ENetPeer, value: anytype) void {
    send(peer, c.NET_CHANNEL_CONTROL, reliable, value);
}

/// Unreliable sequenced: an old snapshot that arrives late is dropped
pub fn sendState(peer: *c.ENetPeer, value: anytype) void {
    send(peer, c.NET_CHANNEL_STATE, 0, value);
}

pub fn sendError(peer: *c.ENetPeer, reason: u8) void {
    sendControl(peer, c.net_error_t{ .type = c.NET_MSG_ERROR, .reason = reason });
    // The error goes out first, then the peer is dropped
    c.enet_peer_disconnect_later(peer, 0);
}

/// A message from a client, checked
pub const Message = union(enum) {
    hello: c.net_hello_t,
    input: c.net_input_t,
    rematch,
};

pub const DecodeError = error{ UnknownType, BadChannel, BadSize, BadField };

/// All the bits a client can set in sim_input_t.actions
const action_bits: u8 = c.SIM_ACTION_SELECT | c.SIM_ACTION_USE | c.SIM_ACTION_PICK_MASK;

pub fn decode(channel: u8, data: []const u8) DecodeError!Message {
    if (data.len == 0) return error.BadSize;
    switch (data[0]) {
        c.NET_MSG_HELLO => {
            const hello = try parse(c.net_hello_t, c.NET_CHANNEL_CONTROL, channel, data);
            // The version and the code are checked by the server, which replies
            if (hello.op != c.NET_OP_CREATE and hello.op != c.NET_OP_JOIN) return error.BadField;
            return .{ .hello = hello };
        },
        c.NET_MSG_INPUT => {
            const input = try parse(c.net_input_t, c.NET_CHANNEL_STATE, channel, data);
            if (input.count < 1 or input.count > c.NET_INPUT_HISTORY) return error.BadField;
            if (input.inputs[0].seq != input.latest_seq) return error.BadField;
            for (input.inputs[0..input.count], 0..) |entry, i| {
                if (entry.input.actions & ~action_bits != 0) return error.BadField;
                // The latest first: each entry is older than the one before it
                if (i > 0 and !seqNewer(input.inputs[i - 1].seq, entry.seq)) return error.BadField;
            }
            return .{ .input = input };
        },
        c.NET_MSG_REMATCH => {
            _ = try parse(c.net_header_t, c.NET_CHANNEL_CONTROL, channel, data);
            return .rematch;
        },
        else => return error.UnknownType,
    }
}

/// The message when the bytes are exactly one of it, on its channel
fn parse(comptime T: type, expected_channel: u8, channel: u8, data: []const u8) DecodeError!T {
    if (channel != expected_channel) return error.BadChannel;
    if (data.len != @sizeOf(T)) return error.BadSize;
    return std.mem.bytesToValue(T, data[0..@sizeOf(T)]);
}

/// True when a comes after b, with wrap-around
pub fn seqNewer(a: u16, b: u16) bool {
    return a != b and (a -% b) < 0x8000;
}

/// True when the time from then to now (ENet ms, they wrap) is at least ms
pub fn elapsed(now: u32, then: u32, ms: u32) bool {
    return now -% then >= ms;
}

/// Packets per second from one peer. Over the limit the excess is ignored;
/// over it for `strikes_to_drop` seconds in a row, or far over it in one
/// second, the peer goes.
pub const RateLimiter = struct {
    pub const limit = 200;
    const strikes_to_drop = 3;
    const window_ms = 1000;

    window_start: u32 = 0,
    count: u32 = 0,
    strikes: u8 = 0,

    pub const Verdict = enum { accept, ignore, drop };

    pub fn init(now: u32) RateLimiter {
        return .{ .window_start = now };
    }

    pub fn check(limiter: *RateLimiter, now: u32) Verdict {
        if (elapsed(now, limiter.window_start, window_ms)) {
            // A quiet window between two busy ones breaks the run
            const consecutive = !elapsed(now, limiter.window_start, 2 * window_ms);
            limiter.strikes = if (limiter.count > limit and consecutive) limiter.strikes + 1 else 0;
            limiter.window_start = now;
            limiter.count = 0;
        }
        limiter.count +|= 1;
        if (limiter.count <= limit) return .accept;
        if (limiter.strikes + 1 >= strikes_to_drop or limiter.count > 5 * limit) return .drop;
        return .ignore;
    }
};

fn testInput(count: u8, seqs: []const u16) [@sizeOf(c.net_input_t)]u8 {
    var message = std.mem.zeroes(c.net_input_t);
    message.type = c.NET_MSG_INPUT;
    message.count = count;
    message.latest_seq = seqs[0];
    for (seqs, 0..) |seq, i| message.inputs[i].seq = seq;
    return std.mem.toBytes(message);
}

test "seq order wraps" {
    try std.testing.expect(seqNewer(1, 0));
    try std.testing.expect(seqNewer(0, 65535));
    try std.testing.expect(!seqNewer(65535, 0));
    try std.testing.expect(!seqNewer(5, 5));
}

test "decode takes only well-formed messages on their channel" {
    const state = c.NET_CHANNEL_STATE;
    const control = c.NET_CHANNEL_CONTROL;
    try std.testing.expectError(error.BadSize, decode(state, &.{}));
    try std.testing.expectError(error.UnknownType, decode(control, &.{0}));
    try std.testing.expectError(error.UnknownType, decode(control, &.{c.NET_MSG_WELCOME}));
    try std.testing.expectError(error.UnknownType, decode(control, &.{255}));

    var hello = std.mem.zeroes(c.net_hello_t);
    hello.type = c.NET_MSG_HELLO;
    hello.op = c.NET_OP_CREATE;
    const hello_bytes = std.mem.toBytes(hello);
    _ = try decode(control, &hello_bytes);
    try std.testing.expectError(error.BadChannel, decode(state, &hello_bytes));
    try std.testing.expectError(error.BadSize, decode(control, hello_bytes[0 .. hello_bytes.len - 1]));
    try std.testing.expectError(error.BadSize, decode(control, &(hello_bytes ++ [_]u8{0})));
    hello.op = 9;
    try std.testing.expectError(error.BadField, decode(control, &std.mem.toBytes(hello)));

    try std.testing.expect((try decode(control, &.{c.NET_MSG_REMATCH})) == .rematch);
    try std.testing.expectError(error.BadSize, decode(control, &.{ c.NET_MSG_REMATCH, 0 }));
}

test "decode checks the inputs" {
    const state = c.NET_CHANNEL_STATE;
    _ = try decode(state, &testInput(3, &.{ 7, 6, 5 }));
    _ = try decode(state, &testInput(2, &.{ 1, 65535 })); // wraps
    try std.testing.expectError(error.BadField, decode(state, &testInput(0, &.{1})));
    try std.testing.expectError(error.BadField, decode(state, &testInput(6, &.{1})));
    try std.testing.expectError(error.BadField, decode(state, &testInput(200, &.{1})));
    try std.testing.expectError(error.BadField, decode(state, &testInput(2, &.{ 5, 6 })));
    try std.testing.expectError(error.BadField, decode(state, &testInput(2, &.{ 5, 5 })));
    var bytes = testInput(1, &.{4});
    bytes[2] = 9; // latest_seq is not inputs[0].seq
    try std.testing.expectError(error.BadField, decode(state, &bytes));
    var message = std.mem.bytesToValue(c.net_input_t, &testInput(1, &.{4}));
    message.inputs[0].input.actions = 0x80;
    try std.testing.expectError(error.BadField, decode(state, &std.mem.toBytes(message)));
    message.inputs[0].input = .{ .move = -128, .actions = c.SIM_ACTION_USE | c.SIM_ACTION_PICK_MASK };
    _ = try decode(state, &std.mem.toBytes(message));
}

test "the rate limiter ignores the excess, then drops a peer that keeps it up" {
    var limiter = RateLimiter.init(1000);
    for (0..RateLimiter.limit) |_| try std.testing.expectEqual(.accept, limiter.check(1000));
    try std.testing.expectEqual(.ignore, limiter.check(1500));
    try std.testing.expectEqual(.accept, limiter.check(2000)); // a new second
    for (0..RateLimiter.limit) |_| _ = limiter.check(2100);
    try std.testing.expectEqual(.ignore, limiter.check(2200)); // two busy seconds
    try std.testing.expectEqual(.accept, limiter.check(3000));
    for (0..RateLimiter.limit) |_| _ = limiter.check(3100);
    try std.testing.expectEqual(.drop, limiter.check(3200)); // the third
}

test "the rate limiter forgives after a quiet second, and the clock wraps" {
    const start: u32 = std.math.maxInt(u32) - 500;
    var limiter = RateLimiter.init(start);
    for (0..RateLimiter.limit + 1) |_| _ = limiter.check(start);
    _ = limiter.check(start +% 1000);
    for (0..RateLimiter.limit + 1) |_| _ = limiter.check(start +% 1000);
    try std.testing.expectEqual(@as(u8, 1), limiter.strikes);
    try std.testing.expectEqual(.accept, limiter.check(start +% 4000)); // after a quiet window
    try std.testing.expectEqual(@as(u8, 0), limiter.strikes);
}

test "a burst far over the limit drops at once" {
    var limiter = RateLimiter.init(0);
    var verdict: RateLimiter.Verdict = .accept;
    for (0..5 * RateLimiter.limit + 1) |_| verdict = limiter.check(10);
    try std.testing.expectEqual(.drop, verdict);
}
