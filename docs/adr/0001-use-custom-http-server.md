# 0001: Build a custom C++ HTTP/TCP server instead of using an existing media server

## Context

Plex, Jellyfin, Navidrome, nginx, and similar projects already solve "serve media
over HTTP." Using one would get a working product faster.

## Decision

Implement the TCP/HTTP server layer ourselves in C++20 using POSIX sockets and
epoll/kqueue. Third-party libraries are used only for narrow, well-bounded concerns
(SQLite, audio metadata parsing, JSON, TOML, unit testing) — never for the core
networking/HTTP path.

## Alternatives Considered

* **Jellyfin/Navidrome + reverse proxy**: fastest to a working product, but defeats
  the project's stated purpose, which is to demonstrate systems/network programming.
* **A C++ HTTP framework (e.g. Boost.Beast, Crow, cpp-httplib)**: would still avoid
  the point of the project — the socket/epoll/HTTP-parsing layer is the thing being
  learned and demonstrated, not the REST endpoints on top of it.

## Consequences

* Significantly more implementation and testing work than integrating an existing
  server (correct partial read/write handling, fragmented-request parsing, Range
  support, backpressure).
* Full control over the streaming path (zero-copy `sendfile`/`pread`, no
  general-purpose framework overhead) and no dependency on a large external project's
  release cadence or feature set.
* This is the single most important technical story of the project (`Plan.md` §37)
  — its correctness and performance are the primary success criteria, not a means
  to an end.
