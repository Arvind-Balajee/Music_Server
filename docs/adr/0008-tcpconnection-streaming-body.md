# 0008: Pull-based streaming body on TcpConnection

## Context

The API layer (`GET /api/v1/tracks/{id}/stream`) needs to send audio files —
potentially hundreds of MB to 1GB+ — through `TcpConnection`. The only
existing write path, `queueWrite(std::span<const std::byte>)`, appends
directly into an in-memory `Buffer`; calling it with an entire file's bytes
(even read incrementally via `FileStreamSource`/`ResponseBody`, but all
queued before any of it is sent) would hold the whole streamed range in
memory at once — exactly what `docs/architecture.md` §7 and `Plan.md` §21/§31
explicitly forbid.

`TcpConnection`/`EventLoop` already has the machinery to avoid this: writes
are backpressure-driven (`flushOutbound()` is called opportunistically after
`onReadable_`, and again on every subsequent writability event, per
`EventLoop::run()`), tested by `tests/integration/net/BackpressureTest.cpp`.
What's missing is a way for a caller to say "keep pulling more data as room
frees up" instead of handing over the whole payload up front.

## Decision

Add a pull-based streaming body to `TcpConnection`:

```cpp
using StreamingBodyPuller = std::function<std::size_t(std::span<std::byte>)>;
void setStreamingBody(StreamingBodyPuller pull);
```

`flushOutbound()` now refills the outbound `Buffer` from `pull` (in fixed
64 KiB chunks, matching `TcpConnection::readInto()`'s existing read-chunk
size) whenever it's below that threshold, *before* attempting `send()` —
reusing the exact same writability-driven call cycle `EventLoop::run()`
already drives for ordinary buffered writes. `EventLoop.cpp` needed **zero**
changes: `hasPendingWrites()` now also returns `true` while a streaming body
is still active (even if the buffer has momentarily drained), which is the
only signal the existing loop needed to keep calling `flushOutbound()` until
the stream is exhausted.

The API layer adapts `std::unique_ptr<musicbox::http::ResponseBody>` (already
returned by `FileStreamSource::openRange()`) into this puller with a small
lambda capturing a `std::shared_ptr` (needed because `std::function` requires
a copyable target; `unique_ptr` capture would make the lambda move-only).

## Alternatives Considered

* **Have `TcpConnection`/`EventLoop` depend on `musicbox::http::ResponseBody`
  directly**, instead of a generic `std::function` puller. Rejected: `net/`
  has no other dependency on `http/`, and introducing one for this alone
  would be a real (if small) layering violation for no benefit — the pull
  signature (`read(span) -> bytes_written`) is all `TcpConnection` actually
  needs, and `ResponseBody` already matches that shape structurally.
* **Add a writability callback to `EventLoop`** (`onWritable(...)`) and have
  route handlers drive their own chunked writes directly. Rejected as more
  invasive for no benefit: it would duplicate the partial-write/backpressure
  bookkeeping `TcpConnection::flushOutbound()` already implements and tests
  correctly, and would leak per-connection streaming state up into
  `EventLoop` or the API layer instead of staying encapsulated where the
  outbound buffer already lives.
* **Read the whole range into one `std::vector` and `queueWrite()` it.**
  Rejected outright — this is precisely the pattern `Plan.md` §21 calls out
  as unacceptable, and defeats the entire point of `FileStreamSource`/
  `pread()`-based streaming that Agent 2 already built and tested.

## Consequences

* `TcpConnection` gains one `std::function` member and a refill loop inside
  `flushOutbound()`; no existing method's behavior or signature changes for
  callers that never call `setStreamingBody()` (`streamingBody_` stays
  `nullptr`, the refill loop is a no-op).
* At most one streaming body may be active per connection; a new call to
  `setStreamingBody()` replaces (does not queue behind) any previous one —
  fine given this project doesn't support persistent/keep-alive connections
  yet (see `docs/adr/0009-http-connection-close-only.md`), so a connection
  never has two responses in flight.
* Peak memory for a streamed response is bounded to roughly one 64 KiB chunk
  plus whatever the kernel's socket send buffer is holding — independent of
  file size, matching the project's stated streaming requirement.
