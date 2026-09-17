# Testing & QA Tracking

Owner: Agent 8 (Testing/QA/Performance). This is the running checklist for
`Plan.md` §15's required test scenarios, mapped to the agent/area responsible
and current status. Status reflects what is actually on `main` as of this
writing (2026-09-09, updated by the integration lead after merging Agents 1/2/3
following this agent's original pass) — not speculative future work. Update
this file as each scenario is implemented and merged; don't mark something
"done" without a passing test on `main` (`Plan.md` §36).

Status legend: **done** (merged + tested on `main`), **in progress** (partial
support merged, or actively being built per this round's task assignment),
**not started** (interface/contract exists, no implementation yet).

## Unit tests

| Scenario (Plan.md §15) | Responsible area | Status | Notes |
|---|---|---|---|
| HTTP parser | Agent 2 (HTTP + Streaming) | done | `tests/unit/http/HttpRequestParserTest.cpp` — byte-at-a-time fragmentation, pipelined requests, all documented limits. |
| Range parser | Agent 2 | done | `tests/unit/http/RangeTest.cpp` — all documented cases incl. malformed/multi-range. |
| Router | Agent 2 | done | `tests/unit/http/RouterTest.cpp` — `{name}` matching, 404/405, duplicate routes. |
| Database repositories | Agent 3 (Music Library + SQLite) | done | `tests/unit/db/*RepositoryTest.cpp` (Track/Artist/Album/LibraryRoot/Playlist), against real in-memory SQLite. |
| Music scanning | Agent 3 | done | `tests/unit/library/LibraryScannerTest.cpp` + `MetadataExtractorTest.cpp` (real TagLib integration). |
| Buffer handling | Agent 1 (Networking Core) | done | `tests/unit/net/BufferTest.cpp`. |
| JSON serialization | Agent 4 (REST API) | done | `tests/unit/api/ApiRoutesTest.cpp` -- dispatches real `HttpRequest`s through the real router against an in-memory SQLite DB, asserting exact field names/shapes against the iOS client's Codable models. |
| Cache policies | Agent 5 (iOS) | not started | iOS app skeleton (`client-ios/`) exists with mocked networking, but no on-device playback cache yet (documented as a deliberate deferral in `docs/ios.md`). |
| `Id<Tag>` model | Agent 0 / shared | done | `tests/unit/IdsTest.cpp`; extended this round with boundary-value coverage (see below). |
| `Sha256` util | Agent 6 / shared | done | `tests/unit/Sha256Test.cpp`; extended this round with a 64 KiB chunk-boundary case (see below). |

## Networking scenarios

| Scenario (Plan.md §15) | Responsible area | Status | Notes |
|---|---|---|---|
| Fragmented requests | Agent 1 + Agent 2 | done | `tests/integration/net/FragmentationTest.cpp` (raw `EventLoop`, byte-level) + `HttpRequestParserTest.cpp` (parser, byte-at-a-time). `EventLoop` now runs the real `HttpRequestParser` + `Router` (server/src/main.cpp), verified manually via curl; `scripts/fragment_client.py --chunk-size N` remains available for deliberately adversarial fragmentation against the real server. |
| Multiple HTTP requests per connection | Agent 1 + Agent 2 | done (parser-level) | `HttpRequestParserTest.cpp` covers pipelined requests in one `feed()` call; not yet exercised over a real persistent `TcpConnection`. `scripts/fragment_client.py --repeat N` for that once wired. |
| Partial writes | Agent 1 | done | `TcpConnection::flushOutbound()` implemented and exercised by `tests/integration/net/BackpressureTest.cpp` (slow client reading one byte at a time forces multiple partial `send()`s). |
| Slow clients | Agent 1 | done | Same `BackpressureTest.cpp`; `scripts/fragment_client.py --recv-chunk-size 1 --recv-delay-ms N` remains available for manual/exploratory runs against a real server once one exists end-to-end. |
| Disconnect during transfer | Agent 1 | tooling ready, not scenario-tested | `EventLoop`/`TcpConnection` handle peer disconnects (see `docs/networking.md`), but no test specifically disconnects *mid-stream*; `scripts/fragment_client.py --close-after-bytes N` is built for this once a streaming endpoint exists. |
| Invalid requests | Agent 2 | done | `HttpRequestParserTest.cpp` covers malformed request lines/headers/Content-Length -> `400`. |
| Huge headers | Agent 2 | done | `HttpRequestParserTest.cpp` covers `maxHeaderBytes`/`maxHeaderCount`/`maxRequestLineLength` limit rejection. |
| Range requests | Agent 2 + Agent 4 | done | `RangeTest.cpp` (parsing) + `FileStreamSourceTest.cpp` (serving the resolved window) + `ApiRoutesTest.cpp`'s stream section (full route, real file, 200/206/416 cases) + manual curl verification of byte-identical full/ranged downloads against a real file. |
| Concurrent streams | Agent 1 + Agent 4 | tooling ready, not benchmarked | `tests/integration/net/ManyConnectionsTest.cpp` covers connection churn generically. The real stream route now exists (`/api/v1/tracks/{id}/stream`); `scripts/benchmark.sh --stream-path /api/v1/tracks/1/stream --concurrency 10` can now measure the real Plan.md §16 target -- not yet run. |

## Streaming test file sizes (Plan.md §15)

| Size | Status | Notes |
|---|---|---|
| 1 MB | tooling ready | `scripts/generate_test_audio.py --outdir DIR` (default set). |
| 10 MB | tooling ready | ditto |
| 100 MB | tooling ready | ditto |
| 1 GB+ | tooling ready | `scripts/generate_test_audio.py --outdir DIR --include-1gb`. |

"Tooling ready" means the fixture generator and test client exist and were
verified against a throwaway TCP/HTTP stub (see this agent's report). Agent 2's
`FileStreamSource`/`FileStreamResponseBody` (docs/http.md) has since landed and
is unit-tested (`FileStreamSourceTest.cpp`), and Agent 4 has wired it into a
real `GET /api/v1/tracks/{id}/stream` route plus the pull-based streaming body
on `TcpConnection` (docs/adr/0008) so a response is sent in bounded chunks
rather than buffered whole. Verified manually against a real (small, ~500 KB)
file: full download is byte-identical to the source file, and a `Range`
request returns the exact requested window. **Not yet re-verified at the
1 GB+ fixture size** — `ps`/`/usr/bin/time -l` RSS sampling while streaming
the large fixture through the real server would confirm memory stays bounded
in practice, not just in the unit test.

## Benchmarks (Plan.md §16)

| Metric | Target | Tooling | Status |
|---|---|---|---|
| Requests/sec | — | `scripts/benchmark.sh` | tooling ready, verified against a stub HTTP server; no real endpoint yet |
| Simultaneous clients | 10 stable | `scripts/benchmark.sh --concurrency 10` | tooling ready |
| Stream throughput | — | `scripts/benchmark.sh --stream-path` | tooling ready, verified against a stub Range server |
| CPU usage | — | not yet automated | recommend `ps`/`top` sampling alongside a benchmark run |
| RAM usage | idle < 100 MB | not yet automated | recommend `ps -o rss` on the server process; see docs/performance.md |
| Seek latency | — | `scripts/benchmark.sh` (time-to-first-byte on Range requests) | tooling ready |
| Database query latency | — | not yet automated | Agent 3's repositories now exist (`server/src/db/`) and are unit-tested for correctness, but not yet benchmarked for latency at realistic library sizes. |
| Library indexing time | — | not yet automated | Agent 3's `LibraryScanner` now exists and is unit-tested for correctness; not yet timed against a realistic (thousands-of-files) library. |
| API latency | < 50 ms LAN | `scripts/benchmark.sh` | tooling ready |
| Playback start | < 500 ms | manual/iOS-side for now | `time_starttransfer` on the stream endpoint is a server-side proxy; true "tap to sound" requires the iOS app |

## Existing CI/build scaffold (verified this round)

| Item | Status |
|---|---|
| `cmake -S . -B build && cmake --build build -j && ctest --test-dir build` | done, passing (144 tests as of this writing) |
| `-DMUSICBOX_SANITIZE=address,undefined` build + ctest | `sanitize` CI job added (runs on `ubuntu-latest`). **Could not be executed locally** to verify the run (not just the build) on this particular dev machine — a trivial hello-world ASan binary hangs indefinitely here regardless of sandboxing, confirmed as a local toolchain/environment issue unrelated to this codebase. The build itself (compile, no run) does succeed locally. Treat the CI job's first real run on GitHub's runners as the actual verification. |
| `clang-format --dry-run --Werror` CI job | done (pre-existing `format-check` job) |
| Strict warnings (`-Wall -Wextra -Wpedantic`) | done (`musicbox_warnings` target in root `CMakeLists.txt`) — verified zero warnings across all of Agents 1/2/3's merged code |
| TagLib install steps in CI | added to `build-and-test` (both OSes) and `sanitize` (Linux) jobs — required once `server/src/library/MetadataExtractor.cpp` (Agent 3) landed; CI would otherwise fail to link |

## How to update this file

When a scenario moves from "not started" to "in progress" or "done": link the
PR/commit, name the test file(s) that cover it, and re-run the verification
commands in `docs/development.md` before flipping the status. Do not mark a
networking/streaming scenario "done" without an actual test exercising the
real server (Plan.md §36 — "a milestone is complete only when it builds, tests
pass, and behavior has been manually verified").
