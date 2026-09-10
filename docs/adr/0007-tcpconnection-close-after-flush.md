# 0007: Additive `closeAfterFlush()` on `TcpConnection`

## Context

`docs/networking.md` specifies `EventLoop`'s dispatch loop but the `TcpConnection`
interface (`server/include/musicbox/net/TcpConnection.hpp`) has no way for a
readable-callback (the HTTP layer, or the temporary Milestone-1 demo handler in
`server/src/main.cpp`) to tell `EventLoop` "close this connection once the
response I just queued has fully drained" — which is exactly the behavior HTTP/1.0
and `Connection: close` responses need, and what the Milestone-1 demo (`curl`
receives `MusicBox` and the connection closes) requires. `EventLoop::run()`'s
readable/writable dispatch loop is the only code that knows when a connection's
outbound buffer has actually finished draining (`flushOutbound()` may take
several calls across several writability events per docs/networking.md's
partial-write contract), so this can't be decided by the callback alone without
a side channel back into the loop.

## Decision

Add two small, additive members to `TcpConnection`:

* `void closeAfterFlush() noexcept` — sets a flag.
* `bool wantsCloseAfterFlush() const noexcept` — reads it.

No existing method signature changed. `EventLoop::run()` checks
`wantsCloseAfterFlush() && !hasPendingWrites()` after each read/write dispatch
and closes the connection at that point. This is the same pattern used by every
production reactor (e.g. libevent's `BEV_EVENT_...` + deferred close, or
nginx's `r->keepalive = 0`); it belongs in `TcpConnection` because it needs to
survive across multiple `flushOutbound()` calls, i.e. across multiple
`EventLoop::run()` iterations, which callback-local state cannot do (a callback
only runs once per readability event).

## Alternatives Considered

* **Return a `bool`/enum from `onReadable_`**: would change the
  `ConnectionReadableCallback` signature (`std::function<void(TcpConnection&)>`),
  a more invasive header change to a type Agent 2 (HTTP) also depends on, for
  no functional benefit over a flag on the connection itself.
* **Let the HTTP layer call `shutdown(fd, SHUT_WR)` directly**: would require
  exposing raw fd/Socket mutation outside `EventLoop`'s single-writer-thread
  invariant (docs/architecture.md §4) and duplicate the partial-flush bookkeeping
  `EventLoop` already does.
* **Do nothing; let the temporary demo leak connections until client-side
  timeout**: violates the "many connections opened/closed rapidly" test
  contract in `docs/networking.md` and would leave real HTTP/1.0 semantics
  unimplementable later without this exact addition anyway.

## Consequences

* `TcpConnection` gains one `bool` field and two trivial inline methods; ABI/API
  is otherwise unchanged, and no existing caller needs to change.
* Agent 2's real HTTP layer can reuse `closeAfterFlush()` directly for
  `Connection: close` / HTTP/1.0 responses instead of inventing an equivalent
  mechanism.
