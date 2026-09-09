# Networking

Owner: Agent 1 (Networking Core). This document is the contract for
`server/include/musicbox/net/`.

## Goals

* Non-blocking, single-threaded (per event loop) reactor over epoll (Linux) with a
  portable fallback for macOS development.
* RAII everywhere: every file descriptor is owned by exactly one object at a time.
* Correct handling of partial reads/writes, `EAGAIN`/`EWOULDBLOCK`/`EINTR`, and
  client disconnects.

## Interfaces (headers already declared under `server/include/musicbox/net/`)

### `Socket`

RAII wrapper around a file descriptor. Move-only. `~Socket()` calls `close()` iff it
still owns a valid fd. No implicit copies of a live fd are ever allowed.

### `Poller`

```cpp
struct PollEvent {
    int fd;
    bool readable;
    bool writable;
    bool error;
    bool hangup;
};

class Poller {
public:
    virtual ~Poller() = default;
    virtual void add(int fd, bool wantRead, bool wantWrite) = 0;
    virtual void modify(int fd, bool wantRead, bool wantWrite) = 0;
    virtual void remove(int fd) = 0;
    // Blocks up to timeoutMs (negative = forever); returns ready events.
    virtual std::vector<PollEvent> wait(int timeoutMs) = 0;
};
```

Implementations: `EpollPoller` (Linux, required), `KqueuePoller` (macOS, preferred),
`SelectPoller` (portable fallback, used only if neither is available). Selection is a
compile-time choice in `server/src/net/PollerFactory.cpp`, not scattered `#ifdef`s.

### `TcpListener`

Owns a bound, listening, non-blocking `Socket`. `accept()` returns
`std::optional<Socket>` — `std::nullopt` on `EAGAIN`/`EWOULDBLOCK` (no pending
connections), throws/logs on a real error.

### `TcpConnection`

Wraps a connected `Socket` plus an inbound `Buffer` and an outbound `Buffer`.

* `ssize_t readInto(Buffer&)` — one non-blocking `recv()`, appended to the buffer.
  Returns bytes read, `0` on orderly shutdown, `-1` on `EAGAIN` (not an error).
* `ssize_t flushOutbound()` — one non-blocking `send()` of as much of the outbound
  buffer as the kernel accepts. Callers must be prepared to call this again when the
  fd becomes writable; **never assume a single `send()` drains the buffer**.
* Does not know about HTTP. The HTTP parser/router (Agent 2) sits above this and is
  driven by the connection's read/write events.

### `Buffer`

A growable byte buffer with a read cursor, supporting cheap "consume N bytes" and
"append" without repeated reallocation of already-consumed data (ring buffer or
compaction on read is acceptable; document which one is used and why in an ADR if it
becomes a bottleneck).

### `EventLoop` / `ConnectionManager`

`EventLoop` owns one `Poller` and the `TcpListener`(s) registered with it. On each
`wait()` iteration:

1. Listener readable -> `accept()` in a loop until `EAGAIN`, register each new fd with
   the poller (read-interest only, initially) and register it in `ConnectionManager`.
2. Connection readable -> `readInto()`, hand new bytes to the HTTP layer.
3. Connection writable (only if it has pending outbound bytes) -> `flushOutbound()`.
4. Hangup/error -> close and unregister.

`ConnectionManager` owns all live `TcpConnection`s keyed by fd (e.g.
`std::unordered_map<int, std::unique_ptr<TcpConnection>>`), so the loop never holds
a dangling pointer across an iteration boundary.

### `ThreadPool`

Fixed-size pool of worker threads draining a thread-safe task queue
(`std::function<void()>`). Used for blocking work (SQLite queries, `stat()`,
metadata parsing) so the event loop thread is never blocked. Results are marshalled
back to the event loop via a lock-protected queue plus a self-pipe/eventfd that wakes
`wait()` — the event loop thread is the only thread that ever calls `send()`.

## Fragmentation & Backpressure Test Contract

Agent 8 (Testing) will exercise this layer with:

* A request split across 1-, 2-, and N-byte `send()` calls from the test client.
* A slow client that reads output one byte per tick during a large file stream.
* Many concurrent connections opened and closed rapidly.

None of these should cause unbounded memory growth, a stuck loop, or a crash.

## Non-goals (MVP)

* No TLS. This is a private offline LAN device.
* No HTTP/2 or HTTP/3.
* No per-connection thread. One event loop thread is sufficient for the target LAN
  client counts (v1 target: 10 simultaneous audio streams).
