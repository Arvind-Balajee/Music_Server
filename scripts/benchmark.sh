#!/usr/bin/env bash
#
# benchmark.sh — measures requests/sec, concurrent-client stability, stream
# throughput, and Range-request (seek) latency against a running
# musicbox-server, per Plan.md §16's metric list and target thresholds:
#
#   requests/sec, simultaneous clients, stream throughput, seek latency
#   idle RAM < 100 MB, 10 stable simultaneous streams,
#   API latency < 50 ms typical on LAN, playback start < 500 ms
#
# Requires: bash, curl, python3 (used only for high-resolution timing/stats —
# `date +%N` isn't portable to macOS's BSD date). Does NOT require ab/wrk/hey;
# if `ab` is present it is used opportunistically for a second requests/sec
# data point, purely as a cross-check.
#
# Usage
# -----
#
#   scripts/benchmark.sh [options]
#
# Options:
#   --host HOST              Server host (default: 127.0.0.1)
#   --port PORT              Server port (default: 8080)
#   --status-path PATH       Cheap endpoint for the requests/sec + latency
#                            benches (default: /api/v1/status)
#   --stream-path PATH       A streamable resource for throughput/seek/
#                            concurrency benches, e.g. /api/v1/tracks/1/stream
#                            (no default — those sections are skipped without it)
#   --requests N             Total requests for the requests/sec bench (default: 200)
#   --concurrency N          Concurrent workers for requests/sec + concurrent-
#                            streams benches (default: 10, matching the
#                            "10 simultaneous audio streams" target)
#   --stream-range-bytes N   Bytes to pull via Range for the throughput bench
#                            (default: 10485760, i.e. 10 MB)
#   --skip-requests          Skip the requests/sec section
#   --skip-stream            Skip the stream-throughput section
#   --skip-seek              Skip the Range/seek-latency section
#   --skip-concurrent        Skip the concurrent-streams section
#   --out FILE               Also append results as JSON lines to FILE
#   -h, --help               Show this help and exit
#
# Example (once Milestones 1-7 land — see docs/performance.md):
#
#   ./build/server/musicbox-server run --config config/musicbox.toml &
#   scripts/benchmark.sh --host 127.0.0.1 --port 8080 \
#       --status-path /api/v1/status \
#       --stream-path /api/v1/tracks/1/stream
#
# Current status: no HTTP endpoints exist on `main` yet (server/src/main.cpp
# is still a CLI stub — see Plan.md milestones 1-6). Until then this script
# can only be smoke-tested against a throwaway HTTP stub (e.g. `python3 -m
# http.server`) to validate the harness itself; the requests/sec and seek-
# latency sections work against any HTTP server today, the stream/concurrency
# sections need a real Range-capable endpoint.

set -euo pipefail

HOST="127.0.0.1"
PORT="8080"
STATUS_PATH="/api/v1/status"
STREAM_PATH=""
REQUESTS=200
CONCURRENCY=10
STREAM_RANGE_BYTES=10485760
OUT_FILE=""
DO_REQUESTS=1
DO_STREAM=1
DO_SEEK=1
DO_CONCURRENT=1

usage() {
    sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --host) HOST="$2"; shift 2 ;;
        --port) PORT="$2"; shift 2 ;;
        --status-path) STATUS_PATH="$2"; shift 2 ;;
        --stream-path) STREAM_PATH="$2"; shift 2 ;;
        --requests) REQUESTS="$2"; shift 2 ;;
        --concurrency) CONCURRENCY="$2"; shift 2 ;;
        --stream-range-bytes) STREAM_RANGE_BYTES="$2"; shift 2 ;;
        --skip-requests) DO_REQUESTS=0; shift ;;
        --skip-stream) DO_STREAM=0; shift ;;
        --skip-seek) DO_SEEK=0; shift ;;
        --skip-concurrent) DO_CONCURRENT=0; shift ;;
        --out) OUT_FILE="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

BASE_URL="http://${HOST}:${PORT}"

now_ms() { python3 -c 'import time; print(int(time.time() * 1000))'; }

emit_json() {
    # emit_json <metric> <value> <unit>
    if [ -n "$OUT_FILE" ]; then
        python3 -c "
import json, sys, time
print(json.dumps({'metric': sys.argv[1], 'value': float(sys.argv[2]), 'unit': sys.argv[3], 'ts': time.time()}))
" "$1" "$2" "$3" >> "$OUT_FILE"
    fi
}

echo "== MusicBox benchmark =="
echo "target: ${BASE_URL}"
echo

echo "-- reachability --"
if ! curl -s -o /dev/null -m 3 "${BASE_URL}${STATUS_PATH}"; then
    echo "warning: ${BASE_URL}${STATUS_PATH} did not respond. This is expected" \
         "on current main (no HTTP server exists yet — see Plan.md milestones" \
         "1-6). Sections below will report failures rather than crash." >&2
fi
echo

if [ "$DO_REQUESTS" = "1" ]; then
    echo "-- requests/sec (N=${REQUESTS}, concurrency=${CONCURRENCY}) against ${STATUS_PATH} --"
    tmp_latencies="$(mktemp)"
    start_ms=$(now_ms)
    ok_count=0
    per_worker=$(( (REQUESTS + CONCURRENCY - 1) / CONCURRENCY ))
    for w in $(seq 1 "$CONCURRENCY"); do
        (
            for _ in $(seq 1 "$per_worker"); do
                curl -s -o /dev/null -m 5 -w '%{http_code} %{time_total}\n' "${BASE_URL}${STATUS_PATH}" || echo "000 0"
            done
        ) >> "$tmp_latencies" &
    done
    wait
    end_ms=$(now_ms)
    elapsed_s=$(python3 -c "print((${end_ms} - ${start_ms}) / 1000.0)")
    total_issued=$(wc -l < "$tmp_latencies" | tr -d ' ')
    ok_count=$(awk '$1 ~ /^2/ {c++} END {print c+0}' "$tmp_latencies")
    rps=$(python3 -c "print(${total_issued} / ${elapsed_s}) if ${elapsed_s} > 0 else print(0)")
    stats=$(awk '$1 ~ /^2/ {print $2}' "$tmp_latencies" | python3 -c "
import sys
vals = sorted(float(l) for l in sys.stdin if l.strip())
if not vals:
    print('no successful requests to compute latency stats from')
else:
    n = len(vals)
    p50 = vals[int(n * 0.50)] * 1000
    p95 = vals[min(n - 1, int(n * 0.95))] * 1000
    avg = sum(vals) / n * 1000
    print(f'avg={avg:.1f}ms p50={p50:.1f}ms p95={p95:.1f}ms (n={n})')
")
    echo "issued=${total_issued} ok=${ok_count} elapsed=${elapsed_s}s rps=${rps}"
    echo "latency: ${stats}"
    emit_json "requests_per_sec" "$rps" "req/s"
    rm -f "$tmp_latencies"

    if command -v ab >/dev/null 2>&1; then
        echo
        echo "cross-check via ab (best-effort, informational only):"
        ab -n "$REQUESTS" -c "$CONCURRENCY" "${BASE_URL}${STATUS_PATH}" 2>/dev/null \
            | grep -E "Requests per second|Time per request|Failed requests" || true
    fi
    echo
fi

if [ -z "$STREAM_PATH" ]; then
    echo "-- stream throughput / seek latency / concurrent streams: SKIPPED --"
    echo "   (pass --stream-path /api/v1/tracks/<id>/stream once the streaming"
    echo "    endpoint exists — see docs/http.md and docs/api.md)"
    echo
else
    STREAM_URL="${BASE_URL}${STREAM_PATH}"

    if [ "$DO_STREAM" = "1" ]; then
        echo "-- stream throughput (first ${STREAM_RANGE_BYTES} bytes of ${STREAM_PATH}) --"
        result=$(curl -s -o /dev/null -m 60 \
            -H "Range: bytes=0-$((STREAM_RANGE_BYTES - 1))" \
            -w '%{http_code} %{size_download} %{time_total}' \
            "$STREAM_URL" || echo "000 0 0")
        read -r code size time_total <<< "$result"
        throughput_mb_s=$(python3 -c "print((${size} / 1048576.0) / ${time_total}) if ${time_total} > 0 else print(0)")
        echo "http_code=${code} bytes=${size} time=${time_total}s throughput=${throughput_mb_s} MB/s"
        emit_json "stream_throughput_mb_s" "$throughput_mb_s" "MB/s"
        echo
    fi

    if [ "$DO_SEEK" = "1" ]; then
        echo "-- seek latency (time-to-first-byte for Range requests at several offsets) --"
        for offset in 0 1048576 10485760; do
            result=$(curl -s -o /dev/null -m 10 \
                -H "Range: bytes=${offset}-$((offset + 65535))" \
                -w '%{http_code} %{time_starttransfer}' \
                "$STREAM_URL" || echo "000 0")
            read -r code ttfb <<< "$result"
            ttfb_ms=$(python3 -c "print(${ttfb} * 1000)")
            echo "offset=${offset} http_code=${code} time_to_first_byte=${ttfb_ms}ms"
            emit_json "seek_latency_ms_offset_${offset}" "$ttfb_ms" "ms"
        done
        echo
    fi

    if [ "$DO_CONCURRENT" = "1" ]; then
        echo "-- concurrent streams (N=${CONCURRENCY}, target from Plan.md §16: 10 stable) --"
        tmp_results="$(mktemp)"
        start_ms=$(now_ms)
        for _ in $(seq 1 "$CONCURRENCY"); do
            (curl -s -o /dev/null -m 30 -w '%{http_code}\n' "$STREAM_URL" || echo "000") >> "$tmp_results" &
        done
        wait
        end_ms=$(now_ms)
        elapsed_s=$(python3 -c "print((${end_ms} - ${start_ms}) / 1000.0)")
        succeeded=$(grep -c '^2' "$tmp_results" || true)
        echo "requested=${CONCURRENCY} succeeded=${succeeded} elapsed=${elapsed_s}s"
        emit_json "concurrent_streams_succeeded" "$succeeded" "count"
        rm -f "$tmp_results"
        echo
    fi
fi

echo "== done =="
echo "Compare against Plan.md §16 targets: idle RAM < 100 MB, 10 stable"
echo "simultaneous streams, API latency < 50 ms LAN, playback start < 500 ms."
echo "Idle RAM and playback-start-time are not measured by this script (RAM:"
echo "sample the server process with ps/top while idle; playback start: time"
echo "from client request to first audio byte, best measured client-side in"
echo "the iOS app or with 'curl -w %{time_starttransfer}' as a proxy, as done"
echo "above for seek latency). Record results in docs/performance.md."
