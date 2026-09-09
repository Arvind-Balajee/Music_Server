# 0004: Stream audio via HTTP byte-range requests, never whole-file buffering

## Context

Music files (especially FLAC/ALAC) can be tens to hundreds of megabytes. The iOS
client needs to seek within a track and needs playback to survive being paused and
resumed without re-downloading from the start.

## Decision

Implement HTTP `Range`/`Content-Range`/`Accept-Ranges`/`206 Partial Content` per
`docs/http.md`, backed by `pread()`/`sendfile()` against the file on disk, resolved
only through `TrackId -> DB row -> absolute path`. No handler ever reads a full
audio file into a `std::vector`/`std::string` before sending it.

## Alternatives Considered

* **Serve the whole file per request, let the client discard unwanted bytes**:
  trivial to implement, but makes seeking expensive (client must re-fetch from byte
  0) and wastes LAN bandwidth and battery on the phone — unacceptable for a car-use
  scenario with a large FLAC library.
* **Custom chunked streaming protocol (WebSocket or raw TCP) instead of HTTP Range**:
  would require a bespoke client-side player instead of `AVFoundation`/`AVPlayer`,
  which already understands standard HTTP range-based progressive download/seek.
  Standard HTTP Range keeps `AVPlayer` usable out of the box (`docs/ios.md`).

## Consequences

* The server must correctly reject malformed/unsatisfiable ranges (`416`) rather
  than guessing, since a wrong `Content-Range` silently corrupts playback/seeking.
* Memory usage during streaming is bounded by chunk size, not file size — this is a
  hard requirement validated by the streaming test suite (1 MB/10 MB/100 MB/1 GB+
  fixtures, `docs/performance.md`).
* Multi-range requests (`bytes=0-99,200-299`) are explicitly out of scope for v1
  (`docs/http.md`) since `AVPlayer` does not need them for normal seek behavior.
