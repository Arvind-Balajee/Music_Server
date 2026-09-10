# Performance & Benchmarks

Owner: Agent 8 (Testing/QA/Performance). See `Plan.md` §16 for the required
benchmark list and initial targets (idle RAM < 100 MB, 10 stable simultaneous
streams, API latency < 50 ms LAN, playback start < 500 ms).

Results recorded here must state the actual hardware/OS/build config measured —
never fabricated numbers. If a row has no measurement yet, it says `TBD` — do
not fill it in until you have actually run the benchmark.

## How to benchmark this project

1. Build and run the server (once Milestones 1-7 land — see `Plan.md` and
   `docs/testing.md` for current status; as of this writing there is no HTTP
   server on `main` yet, only the CLI stub):

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j
   ./build/server/musicbox-server run --config config/musicbox.toml &
   ```

2. Generate streaming test fixtures if you need them for a real library to
   stream from (`Plan.md` §15 sizes: 1 MB / 10 MB / 100 MB / 1 GB+):

   ```bash
   python3 scripts/generate_test_audio.py --outdir /tmp/musicbox-fixtures --include-1gb
   ```

3. Run the benchmark harness against the running instance:

   ```bash
   scripts/benchmark.sh \
       --host 127.0.0.1 --port 8080 \
       --status-path /api/v1/status \
       --stream-path /api/v1/tracks/1/stream \
       --requests 500 --concurrency 10 \
       --out results.jsonl
   ```

   This measures requests/sec, response-latency percentiles, stream
   throughput, seek latency (time-to-first-byte across several `Range`
   offsets), and concurrent-stream stability at the given concurrency. Pass
   `--help` for the full option list (host/port, request counts, which
   sections to skip, etc.) — see the script header in
   `scripts/benchmark.sh` for details and worked examples.

4. Sample RAM/CPU alongside a run (not yet automated by `benchmark.sh`):

   ```bash
   # macOS
   /usr/bin/time -l ./build/server/musicbox-server run --config config/musicbox.toml
   # or, while it's running:
   ps -o rss,%cpu -p <pid>

   # Linux
   ps -o rss,%cpu -p <pid>
   # or for peak RSS of a single run:
   /usr/bin/time -v ./build/server/musicbox-server run --config config/musicbox.toml
   ```

5. For fragmented/slow-client/malformed-request behavior under load (as
   opposed to throughput), use `scripts/fragment_client.py` — see its header
   comment for scenario-by-scenario examples (fragmented sends, pipelined
   requests, mid-transfer disconnects, huge headers, byte-at-a-time slow
   reads). This is a correctness/robustness tool, not a throughput
   benchmark, but a robustness regression often shows up as a
   requests/sec or latency regression too, so run both.

6. Record results below: exact hardware, OS, build type (Debug builds are
   *not* representative — always benchmark a `Release` build), and the
   command used, alongside the numbers. Never fabricate or estimate a number
   you have not actually measured — leave it `TBD`.

## Results

_No benchmarks have been run against a real `musicbox-server` yet — there are
no HTTP endpoints on `main` to benchmark (see `docs/testing.md`). The rows
below are the target metrics from `Plan.md` §16, to be filled in once
Milestones 1-7 land._

**Environment:** TBD (record: CPU, RAM, OS/kernel version, storage — SSD/HDD/
model —, network — wired/Wi-Fi generation/link speed —, and whether this is
the target Raspberry Pi hardware or a development machine)

**Build:** TBD (record: `Release`/`Debug`, compiler + version, commit hash)

| Metric | Target (Plan.md §16) | Measured | Command | Date |
|---|---|---|---|---|
| Idle RAM | < 100 MB | TBD | `ps -o rss -p <pid>` while idle (no active streams) | TBD |
| Simultaneous stable streams | 10 | TBD | `scripts/benchmark.sh --stream-path ... --concurrency 10` | TBD |
| Requests/sec | — (record actual) | TBD | `scripts/benchmark.sh --status-path /api/v1/status` | TBD |
| API latency (typical, LAN) | < 50 ms | TBD | `scripts/benchmark.sh` latency stats (p50) | TBD |
| Stream throughput | — (record actual) | TBD | `scripts/benchmark.sh --stream-path ...` | TBD |
| Seek latency (Range TTFB) | — (record actual) | TBD | `scripts/benchmark.sh --stream-path ...` | TBD |
| Playback start | < 500 ms | TBD | client-side measurement (iOS) or TTFB proxy | TBD |
| CPU usage (idle) | — (record actual) | TBD | `ps -o %cpu -p <pid>` | TBD |
| CPU usage (10 streams) | — (record actual) | TBD | `ps -o %cpu -p <pid>` during `benchmark.sh` concurrent-streams run | TBD |
| Database query latency | — (record actual) | TBD | needs Agent 3's repositories | TBD |
| Library indexing time | — (record actual) | TBD | `time musicbox-server scan --config ...` | TBD |

## Status

No benchmarks recorded yet — tooling (`scripts/benchmark.sh`,
`scripts/generate_test_audio.py`, `scripts/fragment_client.py`) is built and
was smoke-tested against throwaway HTTP/TCP stubs during development (see
Agent 8's report), but there is no real `musicbox-server` HTTP endpoint on
`main` yet to produce actual numbers against. Re-run this doc's "How to
benchmark" steps and fill in the Results table once Milestones 1-7 land.
