# 0003: Use epoll on Linux behind a portable Poller abstraction

## Context

The server must handle many concurrent connections (streaming audio to multiple
clients) without one OS thread per connection, on Linux (deployment target) and
macOS (development target).

## Decision

Implement a reactor event loop driven by `epoll` on Linux, behind a `Poller`
interface (`docs/networking.md`) that also has a `kqueue`-based implementation for
macOS (falling back to `select()` if neither is available). `EventLoop` and
everything above it depends only on `Poller`, never on epoll/kqueue directly.

## Alternatives Considered

* **Thread-per-connection**: simple, but does not scale, contradicts `Plan.md` §4
  and §31 explicitly, and does not teach/demonstrate reactor-style async networking.
* **libuv/Boost.Asio**: would remove the actual learning/demonstration goal of this
  project (writing the event loop ourselves) even though it would be less code.
* **io_uring**: more modern and higher-performance on recent Linux kernels, but adds
  significant complexity and a Linux-only code path with less mature macOS parity;
  left as a documented future optimization (`docs/performance.md`), not a v1
  requirement.

## Consequences

* Requires careful edge-triggered vs. level-triggered handling and correct
  add/modify/remove bookkeeping per fd — the `Poller` interface is intentionally
  narrow so this complexity is isolated to one implementation file per platform.
* macOS development parity depends on the `KqueuePoller`/`SelectPoller` staying
  behaviorally equivalent to `EpollPoller`; the fragmentation/backpressure test suite
  (`docs/networking.md`) runs against whichever poller the host build selects.
