# syntax=docker/dockerfile:1
# The pong server as a static musl binary in an empty image. Build it from
# the repo root, since the server compiles ../shared and ../third_party:
#
#   docker build -f server/Dockerfile -t pong-server .
#
# Zig runs on the build machine and cross-compiles, so
# `docker buildx build --platform linux/arm64 ...` also works.

FROM --platform=$BUILDPLATFORM alpine:3.20 AS build
ARG ZIG_VERSION=0.17.0
# From https://ziglang.org/download/index.json
ARG ZIG_SHA256_X86_64=1cbe9df9f27e6b78d14ccbca43b6703a404ef79ef1c463de901d7f088d4e2026
ARG ZIG_SHA256_AARCH64=9e8d11661d4ae3bd57702a3832781e23ad151dde5798e16a5ccd503f65234ff8
ARG BUILDARCH
ARG TARGETARCH
RUN apk add --no-cache curl xz
RUN set -eu; \
    case "$BUILDARCH" in \
        amd64) arch=x86_64; sum=$ZIG_SHA256_X86_64 ;; \
        arm64) arch=aarch64; sum=$ZIG_SHA256_AARCH64 ;; \
        *) echo "no Zig for $BUILDARCH" >&2; exit 1 ;; \
    esac; \
    curl -fsSL -o /tmp/zig.tar.xz "https://ziglang.org/download/$ZIG_VERSION/zig-$arch-linux-$ZIG_VERSION.tar.xz"; \
    echo "$sum  /tmp/zig.tar.xz" | sha256sum -c -; \
    mkdir /opt/zig; \
    tar -xJf /tmp/zig.tar.xz -C /opt/zig --strip-components=1; \
    rm /tmp/zig.tar.xz

WORKDIR /pong
COPY shared shared
COPY third_party third_party
COPY server/build.zig server/build.zig.zon server/
COPY server/src server/src
RUN set -eu; \
    case "$TARGETARCH" in \
        amd64) arch=x86_64 ;; \
        arm64) arch=aarch64 ;; \
        *) echo "no build for $TARGETARCH" >&2; exit 1 ;; \
    esac; \
    cd server; \
    /opt/zig/zig build -Dtarget=$arch-linux-musl --release=safe -p /out

FROM scratch
COPY --from=build /out/bin/pong-server /pong-server
USER 65532:65532
EXPOSE 7777/udp
ENTRYPOINT ["/pong-server"]
