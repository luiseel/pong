//! Log lines to stderr, each with a UTC timestamp. Quiet by default: the
//! verbose lines (each connect and disconnect) need --verbose.
const std = @import("std");

pub var io: ?std.Io = null;
pub var verbose = false;

pub fn info(comptime fmt: []const u8, args: anytype) void {
    var stamp: [32]u8 = undefined;
    std.debug.print("{s} " ++ fmt ++ "\n", .{timestamp(&stamp)} ++ args);
}

pub fn debug(comptime fmt: []const u8, args: anytype) void {
    if (verbose) info(fmt, args);
}

fn timestamp(buffer: *[32]u8) []const u8 {
    const now = io orelse return "-";
    const seconds = std.Io.Timestamp.now(now, .real).toSeconds();
    return format(buffer, @intCast(@max(seconds, 0)));
}

fn format(buffer: *[32]u8, seconds: u64) []const u8 {
    const epoch = std.time.epoch.EpochSeconds{ .secs = seconds };
    const year_day = epoch.getEpochDay().calculateYearDay();
    const month_day = year_day.calculateMonthDay();
    const day = epoch.getDaySeconds();
    return std.fmt.bufPrint(buffer, "{d:0>4}-{d:0>2}-{d:0>2}T{d:0>2}:{d:0>2}:{d:0>2}Z", .{
        year_day.year,             month_day.month.numeric(), month_day.day_index + 1,
        day.getHoursIntoDay(),     day.getMinutesIntoHour(),  day.getSecondsIntoMinute(),
    }) catch "-";
}

test "timestamps are ISO 8601 UTC" {
    var buffer: [32]u8 = undefined;
    try std.testing.expectEqualStrings("2026-10-24T19:05:09Z", format(&buffer, 1792868709));
    try std.testing.expectEqualStrings("1970-01-01T00:00:00Z", format(&buffer, 0));
}
