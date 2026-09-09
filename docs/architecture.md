# MusicBox Architecture

## 1. Purpose

MusicBox is a portable, offline-first music server. A single-board Linux computer
(development target: macOS/Linux; deployment target: Raspberry Pi) indexes a large
DRM-free music library stored on an attached SSD and streams audio over local Wi-Fi
to a native iOS client. No internet connectivity is required at any point in the
playback path.

The technical core of the project is a custom C++ TCP/HTTP server. Off-the-shelf
media servers (Plex, Jellyfin, Navidrome, nginx, Apache) are explicitly excluded —
the networking stack is implemented from first principles to demonstrate systems
programming.

## 2. System Diagram

```text
                      musicbox-server (C++20, Linux/macOS)
   ┌───────────────────────────────────────────────────────────┐
   │  EventLoop (epoll/kqueue)                                  │
   │       │                                                     │
   │       ├── TcpListener ── accept() ──> ConnectionManager     │
   │       │                                     │                │
   │       │                                     ▼                │
   │       │                            TcpConnection (per fd)    │
   │       │                                     │                │
   │       │                          HttpRequestParser (incremental)
   │       │                                     │                │
   │       │                                  Router              │
   │       │                                     │                │
   │       │                        ┌────────────┴───────────┐    │
   │       │                        ▼                        ▼    │
   │       │                 REST handlers            Streaming    │
   │       │              (JSON, repositories)        handler       │
   │       │                        │                        │    │
   │       │                        ▼                        ▼    │
   │       │                 SqliteXRepository        FileStreamSource
   │       │                        │                 (pread/sendfile)
   │       │                        ▼                        │    │
   │       └──────────────────  SQLite DB            Music files on SSD
   └───────────────────────────────────────────────────────────┘
                                     ▲
                                     │ scans
                            LibraryScanner (Agent 3)

                                     │ Wi-Fi (offline AP, hostapd/dnsmasq)
                                     ▼
                        iOS client (SwiftUI + AVFoundation)
```

## 3. Process & Module Layout

```text
musicbox/
├── shared/    domain models + IDs shared by server and sync-client (C++)
├── server/    the C++ TCP/HTTP music server
│   ├── include/musicbox/net/       Socket, TcpListener, TcpConnection, EventLoop, Poller
│   ├── include/musicbox/http/      HttpRequest, HttpResponse, ResponseBody, Router, Range
│   ├── include/musicbox/db/        domain repositories (interfaces)
│   ├── include/musicbox/library/   filesystem scanner, metadata extraction interfaces
│   ├── include/musicbox/streaming/ streaming handler interfaces
│   └── src/                        implementations
├── client-ios/   SwiftUI application
├── sync-client/  desktop CLI that pushes a local library to the server
├── tests/        unit + integration tests (Catch2)
├── scripts/      dev helper scripts (benchmarks, test file generation, etc.)
└── deployment/   systemd units + Wi-Fi AP config for Raspberry Pi
```

## 4. Threading Model

```text
Main thread
    │
    └── EventLoop::run()
            │  epoll_wait / kqueue, level-triggered on listen + connection fds
            │
            ├── accept new connections -> ConnectionManager (owns TcpConnection by fd)
            │
            ├── on readable: feed bytes into HttpRequestParser incrementally
            │
            ├── cheap handlers (status, JSON list/get) run inline on the loop thread
            │
            └── expensive work (DB queries, file open/stat, hashing) is dispatched to
                the ThreadPool; the result is marshalled back onto the event loop via
                a thread-safe completion queue before writing to the socket.
```

Rules:

* Exactly **one thread** drives `epoll_wait`/`kqueue` per event loop instance. There is
  no thread-per-connection.
* SQLite is accessed with **one connection per worker thread** (`sqlite3_open` per
  thread, `SQLITE_OPEN_FULLMUTEX` not relied upon). Connections are never shared
  across threads. See `docs/database.md`.
* Socket writes from worker threads are never issued directly; workers post results
  back to the owning `TcpConnection` through a lock-protected outbound queue that the
  event loop drains on its own thread. This avoids concurrent `send()` on one fd.
* File streaming (the hot path) reads/`sendfile`s in bounded chunks driven by
  writability events — never reads a whole file into memory (see `docs/networking.md`
  and `docs/http.md`).

## 5. Platform Abstraction

`Poller` is a pure interface with `EpollPoller` (Linux) and a compatibility
implementation for macOS (`KqueuePoller`, falling back to a `select()`-based poller
if neither is available). `EventLoop` depends only on `Poller`; no platform `#ifdef`s
leak outside `server/src/net/`.

## 6. Core Interfaces

Defined under `shared/include/musicbox/model/` and `server/include/musicbox/`:

* `HttpRequest`, `HttpResponse`, `ResponseBody` — `docs/http.md`
* `Socket`, `TcpListener`, `TcpConnection`, `EventLoop`, `Poller` — `docs/networking.md`
* `Track`, `Album`, `Artist`, `Playlist`, `LibraryRoot` — `docs/database.md`
* `TrackRepository`, `AlbumRepository`, `ArtistRepository`, `PlaylistRepository`,
  `LibraryRootRepository` — `docs/database.md`
* `Router`, `RequestHandler` — `docs/api.md`

These headers are the contracts between agents. Any change to a header under
`shared/include` or `server/include/musicbox/**/*.hpp` that is already relied upon by
another agent must be additive (new optional fields, new virtual methods with default
behavior documented) and noted in an ADR under `docs/adr/`. Do not silently break a
signature another agent's code already calls.

## 7. Security Posture (MVP)

* Clients never supply filesystem paths. Every streamed file is resolved
  `TrackId -> DB row -> absolute path` inside the server; the path never appears in
  the URL or request body.
* `..`, absolute paths, and symlink escapes are rejected by the library scanner and
  by path resolution before any `open()`/`pread()` call.
* No authentication in the MVP (private LAN device) — documented as a known
  limitation, not an oversight. See `docs/adr/0005-no-auth-in-mvp.md`.
* Malformed HTTP (oversized headers, invalid Content-Length, invalid Range) is
  rejected with a 4xx before touching the database or filesystem.

## 8. Configuration

TOML config file (`docs/development.md` has the schema and CLI override rules).

## 9. Milestone Mapping

See `Plan.md` §23 for the canonical milestone list. This document will be updated
as each milestone's actual (not planned) architecture lands; see `docs/adr/` for the
record of decisions made along the way.
