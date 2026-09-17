# 0009: No persistent/keep-alive connections in the first API composition

## Context

`HttpRequestParser` already supports pipelining (parsing multiple requests
buffered from one `feed()` call), and nothing about the parser or `Router`
requires closing a connection after one response. But wiring real
persistent-connection semantics correctly means: honoring `Connection:
keep-alive` vs. `close` per the client's HTTP version and headers, correctly
framing each response so the client knows where one ends and the next
begins even without closing (Content-Length is enough for our responses
here, but this still needs to be gotten right project-wide), and handling a
client that never sends another request on an idle kept-alive connection
(a read timeout / idle-connection reaper `EventLoop` doesn't have yet).

## Decision

The first `main()` composition (wiring `EventLoop` + `HttpRequestParser` +
`Router` together) closes the connection after exactly one response,
unconditionally: every response gets `Connection: close`, and
`TcpConnection::closeAfterFlush()` is called right after dispatching,
regardless of what the request asked for. `HttpRequestParser`'s pipelining
support is unused by this composition for now (a second buffered request on
the same connection is simply never read, since the connection closes as
soon as the first response drains) — it stays available in `HttpRequestParser`
itself since Agent 2 already built and tested it, ready for whenever
keep-alive is added.

## Alternatives Considered

* **Implement real keep-alive now.** Rejected for this pass: doing it
  correctly (idle timeouts, `Connection` header negotiation across HTTP/1.0
  vs. 1.1 default semantics) is a meaningful independent chunk of work, and
  getting it subtly wrong (e.g. a client hanging waiting for a response on a
  connection the server considers idle-timed-out) is worse than the
  connection-per-request overhead of doing without it. A LAN-local streaming
  server serving one phone is not latency- or connection-count-sensitive
  enough for this to matter yet.

## Consequences

* One TCP connection (and one `HttpRequestParser` instance) per request.
  Acceptable overhead for this project's target scale (`Plan.md` §16: 10
  simultaneous streams, not thousands of req/s), but a real cost if API
  latency ever needs to drop meaningfully below what a fresh TCP handshake
  costs on a given network.
* The per-connection `HttpRequestParser` map the composition root owns
  (`main.cpp`) can erase an entry as soon as that connection's one response
  is dispatched, rather than needing to track request/response framing
  across multiple requests per connection.
* Revisit this once real usage (or `docs/performance.md` benchmarks) shows
  connection setup overhead actually matters; `HttpRequestParser`'s
  pipelining support means the parser side of keep-alive needs no rework
  when that happens — only the composition root's dispatch loop does.
