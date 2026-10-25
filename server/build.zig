const std = @import("std");

// The match simulation shared with the client; the server is the authority
const sim_sources = [_][]const u8{ "src/sim.c", "src/entity.c", "src/power_up.c", "src/vector.c" };
const enet_sources = [_][]const u8{
    "callbacks.c", "compress.c", "host.c", "list.c", "packet.c", "peer.c", "protocol.c", "unix.c", "win32.c",
};
// ENet's configure results, valid for Linux (musl and glibc) and macOS. ENet
// computes offsetof through a null pointer, which the debug sanitizer traps.
const enet_flags = [_][]const u8{
    "-std=c99",          "-fno-sanitize=undefined", "-DHAS_FCNTL",     "-DHAS_POLL",         "-DHAS_GETADDRINFO",
    "-DHAS_GETNAMEINFO", "-DHAS_INET_PTON",         "-DHAS_INET_NTOP", "-DHAS_MSGHDR_FLAGS", "-DHAS_SOCKLEN_T",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseSafe });

    // Zig 0.17 has no @cImport: the C headers become a module through translate-c
    const translate = b.addTranslateC(.{
        .root_source_file = b.path("src/c.h"),
        .target = target,
        .optimize = optimize,
    });
    translate.addIncludePath(b.path("../src"));
    translate.addIncludePath(b.path("../third_party/enet/include"));

    const exe = b.addExecutable(.{
        .name = "pong-server",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/main.zig"),
            .target = target,
            .optimize = optimize,
            .link_libc = true,
            .imports = &.{.{ .name = "c", .module = translate.createModule() }},
        }),
    });
    exe.root_module.addIncludePath(b.path("../src"));
    exe.root_module.addIncludePath(b.path("../third_party/enet/include"));
    exe.root_module.addCSourceFiles(.{
        .root = b.path(".."),
        .files = &sim_sources,
        .flags = &.{ "-std=c99", "-Wall", "-Wextra" },
    });
    exe.root_module.addCSourceFiles(.{
        .root = b.path("../third_party/enet"),
        .files = &enet_sources,
        .flags = &enet_flags,
    });
    b.installArtifact(exe);

    const run = b.addRunArtifact(exe);
    run.addPassthruArgs();
    b.step("run", "Run the server").dependOn(&run.step);

    const tests = b.addTest(.{ .root_module = exe.root_module });
    b.step("test", "Run the unit tests").dependOn(&b.addRunArtifact(tests).step);
}
