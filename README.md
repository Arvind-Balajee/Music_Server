# MusicBox

A portable, **offline** music server: a Raspberry Pi (or any Linux/macOS box) with
an attached SSD indexes a large DRM-free music library and streams it over local
Wi-Fi to a native iOS app — no internet connection required, ever.

```text
Raspberry Pi + SSD  ──Wi-Fi──▶  iPhone (SwiftUI + AVFoundation)  ──Bluetooth/CarPlay──▶  Car
```

## Why

Streaming apps need signal. A large FLAC/ALAC library doesn't fit on a phone. This
project exists to solve that specific problem (offline library access while
driving/traveling) and, just as importantly, to be a serious systems-programming
exercise: **the core of this project is a custom C++ TCP/HTTP server** — not an
off-the-shelf media server — built with POSIX sockets, epoll, incremental HTTP
parsing, and zero-copy byte-range file streaming. See
`docs/adr/0001-use-custom-http-server.md`.

## Status

Core server loop works end-to-end: build, `scan` a real library, `run`, and
browse/search/stream/manage-playlists against the real `/api/v1/*` API
(`docs/api.md`) from `curl` or the iOS app. Not yet done: `musicbox-sync`'s
proposed server-side endpoints, real Raspberry Pi hardware validation, and
performance benchmarking against the real server. See `Plan.md` §23 for the
full milestone list and `docs/architecture.md` for the current design.

## Architecture

See `docs/architecture.md` for the full system diagram, threading model, and
module layout. Short version:

```text
musicbox/
├── shared/       domain models + utilities shared by server and sync-client
├── server/       the C++ TCP/HTTP music server (the core of the project)
├── client-ios/   SwiftUI + AVFoundation iOS app
├── sync-client/  desktop CLI that pushes a local library onto the device
├── tests/        Catch2 unit + integration tests
├── scripts/      dev helper scripts (benchmarks, test fixtures)
└── deployment/   systemd units + Wi-Fi access point config for Raspberry Pi
```

Deep dives: `docs/networking.md`, `docs/http.md`, `docs/database.md`, `docs/api.md`,
`docs/ios.md`, `docs/deployment.md`, `docs/performance.md`. Design decisions with
their alternatives/tradeoffs are recorded in `docs/adr/`.

## Hardware (target deployment)

* Raspberry Pi Zero 2 W / 4 / 5
* USB battery pack
* 512 GB – 2 TB external SSD
* No internet connection required or used

Development happens on macOS/Linux before Raspberry Pi deployment.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

See `docs/development.md` for prerequisites, sanitizer builds, and the full CLI.

## Running

```bash
./build/server/musicbox-server scan --config config/musicbox.toml   # index your library first
./build/server/musicbox-server run --config config/musicbox.toml
```

`run` starts the real event loop and serves the real REST API (`docs/api.md`)
against whatever `scan` has indexed. `musicbox-server status`/`doctor` (CLI
subcommands that would query/preflight-check a running instance) aren't
implemented yet — use `curl http://localhost:8080/api/v1/status` instead.

## Adding Music

Point `[library].paths` in `config/musicbox.toml` at a directory tree of
`.mp3`/`.flac`/`.m4a`/`.aac`/`.wav` files, then:

```bash
./build/server/musicbox-server scan --config config/musicbox.toml
```

Or push a library from a desktop machine with the sync client (in progress):

```bash
musicbox-sync ~/Music musicbox.local
```

## Using the iOS App

See `client-ios/` and `docs/ios.md`. Connect to the MusicBox's Wi-Fi (or the same
LAN during development), open the app, and browse Artists/Albums/Songs/Playlists.
No Apple Music / MusicKit / DRM content is involved — this plays your own
DRM-free files only.

## Deploying to Raspberry Pi

See `deployment/README.md` and `docs/deployment.md` for the systemd service and
offline Wi-Fi access point setup (`hostapd`/`dnsmasq`, `192.168.50.1`).

## Networking

Custom epoll/kqueue-based reactor, incremental HTTP/1.1 parsing (correct under
arbitrary TCP fragmentation), and HTTP byte-range streaming backed by
`pread`/`sendfile` — never a whole-file read into memory. Full design in
`docs/networking.md` and `docs/http.md`.

## Benchmarks

Recorded in `docs/performance.md` as they're measured, against real hardware —
never fabricated. Initial targets: idle RAM < 100 MB, 10 stable simultaneous audio
streams, API latency < 50 ms on LAN, playback start < 500 ms.

## Design Decisions

`docs/adr/` — one file per significant decision (context, alternatives considered,
consequences). Start with `0001-use-custom-http-server.md`.

## Future Work

Sync client resumable transfers, client-side bounded LRU cache and prefetch for
in-car resilience, Raspberry Pi field deployment, and performance tuning
(`io_uring` is deliberately deferred — see `docs/adr/0003-use-epoll-linux.md`).
