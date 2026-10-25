//! Pong online server: one thread, one ENet host, rooms of two players.
//! Run with --help for the flags.
const std = @import("std");
const c = @import("c");
const Room = @import("room.zig");
const Client = Room.Client;
const protocol = @import("protocol.zig");
const log = @import("log.zig");

const alphabet = c.NET_CODE_ALPHABET;
const max_rooms_limit = 1000;
/// Peers beyond two per room, for clients that get an error or are dropped
const spare_peers = 8;
/// Peers from one IP address; a household behind one NAT has a few
const peers_per_address = 16;
/// The largest packet a client may send; the largest valid one is an INPUT
const max_packet_size = 1024;
const maintenance_ms = 1000;

const usage =
    \\usage: pong-server [options]
    \\  --port N           UDP port (default 7777)
    \\  --target-score N   points to win, 1-21 (default 5)
    \\  --max-rooms N      rooms at the same time, 1-1000 (default 32)
    \\  --fake-lag-ms N    test: hold snapshots out and inputs in N ms, 0-1000
    \\  --fake-loss N      test: drop N percent of snapshots out and inputs in
    \\  --verbose          log each connect and disconnect
    \\  --help             show this help
    \\
;

const Server = struct {
    host: *c.ENetHost,
    io: std.Io,
    target_score: u16,
    clients: []Client,
    client_used: []bool,
    rooms: []Room,

    fn connect(server: *Server, peer: *c.ENetPeer, now: u32) void {
        // A silent peer goes in 10 s, not ENet's default 30 s
        c.enet_peer_timeout(peer, 0, c.NET_PEER_TIMEOUT_MS, c.NET_PEER_TIMEOUT_MS);
        for (server.client_used, 0..) |*used, i| {
            if (used.*) continue;
            used.* = true;
            server.clients[i] = .{ .peer = peer, .connected_at = now, .limiter = .init(now) };
            peer.data = &server.clients[i];
            log.debug("client {d} connected", .{i});
            return;
        }
        log.info("no free client slot, peer dropped", .{});
        c.enet_peer_disconnect_now(peer, 0);
    }

    fn clientOf(peer: *c.ENetPeer) ?*Client {
        return @ptrCast(@alignCast(peer.data orelse return null));
    }

    fn clientIndex(server: *Server, client: *Client) usize {
        return (@intFromPtr(client) - @intFromPtr(server.clients.ptr)) / @sizeOf(Client);
    }

    /// The peer is gone (ENet said so, or the server dropped it)
    fn disconnect(server: *Server, peer: *c.ENetPeer, now: u32) void {
        const client = clientOf(peer) orelse return;
        if (client.room) |room| {
            log.info("room {s}: side {d} left", .{ room.code, client.side });
            room.leave(client, now);
        }
        const index = server.clientIndex(client);
        server.client_used[index] = false;
        peer.data = null;
        log.debug("client {d} disconnected", .{index});
    }

    /// Drops a peer at once. ENet sends no DISCONNECT event for it.
    fn drop(server: *Server, peer: *c.ENetPeer, now: u32, comptime why: []const u8) void {
        if (clientOf(peer)) |client| log.info("client {d} dropped: " ++ why, .{server.clientIndex(client)});
        server.disconnect(peer, now);
        c.enet_peer_disconnect_now(peer, 0);
    }

    fn receive(server: *Server, peer: *c.ENetPeer, channel: u8, data: []const u8, now: u32) void {
        const client = clientOf(peer) orelse return;
        switch (client.limiter.check(now)) {
            .accept => {},
            .ignore => return,
            .drop => return server.drop(peer, now, "flooding"),
        }
        const message = protocol.decode(channel, data) catch {
            if (client.badPacket()) server.drop(peer, now, "bad packets");
            return;
        };
        switch (message) {
            .hello => |message_hello| server.hello(client, &message_hello, now),
            .input => |input| client.receiveInput(now, &input),
            .rematch => {
                const room = client.room orelse return;
                if (!room.requestRematch(client)) return;
                room.start(server.newSeed());
                log.info("room {s}: rematch started", .{room.code});
            },
        }
    }

    fn hello(server: *Server, client: *Client, message: *const c.net_hello_t, now: u32) void {
        if (client.said_hello) return;
        client.said_hello = true;
        if (message.version != c.NET_VERSION or message.state_size != @sizeOf(c.sim_state_t)) {
            log.info("version mismatch: v{d}, state {d} bytes", .{ message.version, message.state_size });
            protocol.sendError(client.peer, c.NET_ERROR_VERSION);
            return;
        }
        switch (message.op) {
            c.NET_OP_CREATE => {
                const room = server.freeRoom() orelse {
                    log.info("all {d} rooms in use", .{server.rooms.len});
                    protocol.sendError(client.peer, c.NET_ERROR_FULL);
                    return;
                };
                room.open(server.newCode(), client, now);
                log.info("room {s}: created", .{room.code});
            },
            c.NET_OP_JOIN => {
                // Any bytes can come in: the code only has to match a room's
                var code = message.code;
                for (&code) |*letter| letter.* = std.ascii.toUpper(letter.*);
                const room = server.findRoom(code) orelse {
                    protocol.sendError(client.peer, c.NET_ERROR_NOT_FOUND);
                    return;
                };
                if (room.isFull()) {
                    protocol.sendError(client.peer, c.NET_ERROR_FULL);
                    return;
                }
                room.join(client, server.newSeed(), server.target_score);
                log.info("room {s}: match started, first to {d}", .{ room.code, server.target_score });
            },
            else => protocol.sendError(client.peer, c.NET_ERROR_INVALID), // decode lets none through
        }
    }

    fn newSeed(server: *Server) u64 {
        var bytes: [8]u8 = undefined;
        server.io.random(&bytes);
        return std.mem.readInt(u64, &bytes, .little);
    }

    fn freeRoom(server: *Server) ?*Room {
        for (server.rooms) |*room| {
            if (!room.active) return room;
        }
        return null;
    }

    fn findRoom(server: *Server, code: [Room.code_length]u8) ?*Room {
        for (server.rooms) |*room| {
            if (room.active and std.mem.eql(u8, &room.code, &code)) return room;
        }
        return null;
    }

    /// A code no open room has
    fn newCode(server: *Server) [Room.code_length]u8 {
        while (true) {
            var code: [Room.code_length]u8 = undefined;
            var bytes: [Room.code_length]u8 = undefined;
            server.io.random(&bytes);
            for (&code, bytes) |*letter, byte| letter.* = alphabet[byte % alphabet.len];
            if (server.findRoom(code) == null) return code;
        }
    }

    fn step(server: *Server, now: u32) void {
        for (server.rooms) |*room| {
            if (room.active) room.step(now);
        }
    }

    fn flushDelayed(server: *Server, now: u32) void {
        for (server.clients, server.client_used) |*client, used| {
            if (used) client.flushDelayed(now);
        }
    }

    /// Peers without a HELLO and rooms idle too long go
    fn maintain(server: *Server, now: u32) void {
        for (server.clients, server.client_used) |*client, used| {
            if (used and client.helloOverdue(now)) server.drop(client.peer, now, "no HELLO in 5 s");
        }
        for (server.rooms) |*room| {
            if (!room.expired(now)) continue;
            log.info("room {s}: expired", .{room.code});
            room.close(c.NET_ERROR_EXPIRED);
        }
    }

    fn service(server: *Server, timeout_ms: u32) void {
        var timeout = timeout_ms;
        var event: c.ENetEvent = undefined;
        while (c.enet_host_service(server.host, &event, timeout) > 0) {
            timeout = 0;
            const now = c.enet_time_get();
            switch (event.type) {
                c.ENET_EVENT_TYPE_CONNECT => server.connect(event.peer, now),
                c.ENET_EVENT_TYPE_RECEIVE => {
                    const packet = event.packet;
                    const data: []const u8 = if (packet.*.dataLength == 0) &.{} else packet.*.data[0..packet.*.dataLength];
                    server.receive(event.peer, event.channelID, data, now);
                    c.enet_packet_destroy(packet);
                },
                c.ENET_EVENT_TYPE_DISCONNECT => server.disconnect(event.peer, now),
                else => {},
            }
        }
    }

    /// The loop: packets as they come, a room step every tick. ENet time is
    /// u32 ms and wraps (about every 49 days), so all of it is wrapping math.
    fn run(server: *Server) noreturn {
        const tick_ms = 1000.0 / @as(f64, c.NET_TICK_RATE);
        var start: u32 = c.enet_time_get();
        var ticks: u64 = 0;
        var next_maintenance: u32 = start +% maintenance_ms;
        while (true) {
            const due: u32 = start +% @as(u32, @intFromFloat(@as(f64, @floatFromInt(ticks + 1)) * tick_ms));
            const wait = @as(i32, @bitCast(due -% c.enet_time_get()));
            server.service(if (wait > 0) @intCast(wait) else 0);
            const now = c.enet_time_get();
            server.flushDelayed(now);
            if (@as(i32, @bitCast(now -% next_maintenance)) >= 0) {
                server.maintain(now);
                next_maintenance = now +% maintenance_ms;
            }
            const behind = @as(i32, @bitCast(now -% due));
            if (behind > 1000) { // far behind (the process was stopped): skip ahead
                start = now;
                ticks = 0;
                continue;
            }
            if (behind >= 0) {
                ticks += 1;
                server.step(now);
                // Whole seconds of ticks reset the base, so ticks stays small
                if (ticks == c.NET_TICK_RATE) {
                    start +%= 1000;
                    ticks = 0;
                }
            }
        }
    }
};

const Options = struct {
    port: u16 = c.NET_DEFAULT_PORT,
    target_score: u16 = c.MATCH_DEFAULT_TARGET,
    max_rooms: u16 = 32,
    fake_lag_ms: u32 = 0,
    fake_loss: u8 = 0,
    verbose: bool = false,
    help: bool = false,
};

fn parseValue(comptime T: type, text: ?[]const u8, min: T, max: T) !T {
    const value = std.fmt.parseInt(T, text orelse return error.MissingValue, 10) catch return error.BadValue;
    if (value < min or value > max) return error.BadValue;
    return value;
}

fn parseOptions(args: []const [:0]const u8) !Options {
    var options = Options{};
    var i: usize = 1;
    while (i < args.len) : (i += 1) {
        const arg = args[i];
        const value: ?[]const u8 = if (i + 1 < args.len) args[i + 1] else null;
        if (std.mem.eql(u8, arg, "--help") or std.mem.eql(u8, arg, "-h")) {
            options.help = true;
            continue;
        } else if (std.mem.eql(u8, arg, "--verbose") or std.mem.eql(u8, arg, "-v")) {
            options.verbose = true;
            continue;
        } else if (std.mem.eql(u8, arg, "--port")) {
            options.port = try parseValue(u16, value, 1, 65535);
        } else if (std.mem.eql(u8, arg, "--target-score")) {
            options.target_score = try parseValue(u16, value, 1, c.MATCH_MAX_TARGET);
        } else if (std.mem.eql(u8, arg, "--max-rooms")) {
            options.max_rooms = try parseValue(u16, value, 1, max_rooms_limit);
        } else if (std.mem.eql(u8, arg, "--fake-lag-ms")) {
            options.fake_lag_ms = try parseValue(u32, value, 0, Room.max_fake_lag_ms);
        } else if (std.mem.eql(u8, arg, "--fake-loss")) {
            options.fake_loss = try parseValue(u8, value, 0, 100);
        } else return error.UnknownOption;
        i += 1;
    }
    return options;
}

pub fn main(init: std.process.Init) !void {
    log.io = init.io;
    const arena = init.arena.allocator();
    const args = try init.minimal.args.toSlice(arena);
    const options = parseOptions(args) catch |err| {
        std.debug.print("pong-server: {s}\n" ++ usage, .{@errorName(err)});
        std.process.exit(2);
    };
    if (options.help) {
        std.debug.print("{s}", .{usage});
        return;
    }
    log.verbose = options.verbose;
    Room.fake = .{
        .lag_ms = options.fake_lag_ms,
        .loss_percent = options.fake_loss,
        .random = .init(@truncate(@as(u96, @bitCast(std.Io.Timestamp.now(init.io, .awake).nanoseconds)))),
    };

    if (c.enet_initialize() != 0) return error.EnetInit;
    defer c.enet_deinitialize();
    const peer_count = @as(usize, options.max_rooms) * 2 + spare_peers;
    var address = c.ENetAddress{ .host = c.ENET_HOST_ANY, .port = options.port };
    const host = c.enet_host_create(&address, peer_count, c.NET_CHANNELS, 0, 0) orelse {
        log.info("cannot listen on UDP port {d}", .{options.port});
        return error.Listen;
    };
    defer c.enet_host_destroy(host);
    host.*.duplicatePeers = peers_per_address;
    host.*.maximumPacketSize = max_packet_size;

    const server = try arena.create(Server);
    server.* = .{
        .host = host,
        .io = init.io,
        .target_score = options.target_score,
        .clients = try arena.alloc(Client, peer_count),
        .client_used = try arena.alloc(bool, peer_count),
        .rooms = try arena.alloc(Room, options.max_rooms),
    };
    @memset(server.client_used, false);
    @memset(server.rooms, .{});
    log.info("pong-server listening on UDP {d}: first to {d}, {d} rooms, snapshot {d} bytes", .{
        options.port, options.target_score, options.max_rooms, @sizeOf(c.net_snapshot_t),
    });
    if (options.fake_lag_ms > 0 or options.fake_loss > 0) {
        log.info("fake lag {d} ms, fake loss {d} %", .{ options.fake_lag_ms, options.fake_loss });
    }
    server.run();
}

test "options" {
    const defaults = try parseOptions(&.{"pong-server"});
    try std.testing.expectEqual(@as(u16, c.NET_DEFAULT_PORT), defaults.port);
    try std.testing.expectEqual(@as(u16, 32), defaults.max_rooms);
    const set = try parseOptions(&.{ "pong-server", "--port", "9000", "--max-rooms", "4", "--fake-lag-ms", "50", "--fake-loss", "10", "--verbose" });
    try std.testing.expectEqual(@as(u16, 9000), set.port);
    try std.testing.expectEqual(@as(u16, 4), set.max_rooms);
    try std.testing.expectEqual(@as(u32, 50), set.fake_lag_ms);
    try std.testing.expectEqual(@as(u8, 10), set.fake_loss);
    try std.testing.expect(set.verbose);
    try std.testing.expectError(error.BadValue, parseOptions(&.{ "pong-server", "--max-rooms", "0" }));
    try std.testing.expectError(error.BadValue, parseOptions(&.{ "pong-server", "--fake-loss", "101" }));
    try std.testing.expectError(error.BadValue, parseOptions(&.{ "pong-server", "--target-score", "x" }));
    try std.testing.expectError(error.MissingValue, parseOptions(&.{ "pong-server", "--port" }));
    try std.testing.expectError(error.UnknownOption, parseOptions(&.{ "pong-server", "--nope" }));
}

test {
    _ = Room;
    _ = protocol;
    _ = log;
}
