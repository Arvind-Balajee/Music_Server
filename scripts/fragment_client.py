#!/usr/bin/env python3
"""
fragment_client.py — raw-socket TCP test client that sends a payload split
into attacker/network-realistic fragments, for exercising the incremental
HTTP parser and connection layer built by Agents 1/2 (see docs/http.md
"Incremental Parsing" and docs/networking.md "Fragmentation & Backpressure
Test Contract").

This deliberately does NOT use `requests`/`http.client` or even a buffered
socket file object: it opens a raw socket and calls send()/recv() directly so
every byte boundary is under the caller's control, matching Plan.md §15's
required scenarios:

    fragmented requests               --chunk-size 1 (or 2, or N)
    multiple requests per connection  --repeat N
    partial writes / slow clients     --delay-ms, --recv-delay-ms
    disconnect during transfer        --close-after N (bytes sent, then abort)
    huge headers                      --pad-header-bytes N
    invalid requests                  --data '<anything, including garbage>'

Usage
-----

Send a single request as one N-byte-at-a-time fragmented write, print the
response:

    python3 scripts/fragment_client.py --host 127.0.0.1 --port 8080 \\
        --data $'GET / HTTP/1.1\\r\\nHost: musicbox\\r\\n\\r\\n' \\
        --chunk-size 1

Simulate the exact split from Plan.md §8 (a request line split mid-token,
across three packets with a delay between each, as a slow/jittery client
would produce):

    python3 scripts/fragment_client.py --host 127.0.0.1 --port 8080 \\
        --data $'GET /api/tracks/10 HTTP/1.1\\r\\nHost: musicbox\\r\\n\\r\\n' \\
        --chunk-size 12 --delay-ms 50

Pipe a raw request from a file (useful for huge-header / malformed-request
fixtures you don't want to inline on a command line):

    python3 scripts/fragment_client.py --host 127.0.0.1 --port 8080 \\
        --data-file huge_header_request.bin --chunk-size 4096

Generate and send a request with an oversized header value inline (tests the
server's max-header-bytes limit from docs/http.md):

    python3 scripts/fragment_client.py --host 127.0.0.1 --port 8080 \\
        --pad-header-bytes 200000

Send N pipelined requests back-to-back over one connection (persistent
connection reuse):

    python3 scripts/fragment_client.py --host 127.0.0.1 --port 8080 \\
        --data $'GET / HTTP/1.1\\r\\nHost: musicbox\\r\\n\\r\\n' --repeat 5

Simulate a disconnect mid-request (send half the bytes, then close):

    python3 scripts/fragment_client.py --host 127.0.0.1 --port 8080 \\
        --data $'GET / HTTP/1.1\\r\\nHost: musicbox\\r\\n\\r\\n' \\
        --chunk-size 4 --close-after-bytes 10

Simulate a slow client draining a streamed response one byte at a time (pairs
with a large Range request to exercise server-side backpressure, per
docs/networking.md):

    python3 scripts/fragment_client.py --host 127.0.0.1 --port 8080 \\
        --data $'GET /api/v1/tracks/1/stream HTTP/1.1\\r\\nHost: musicbox\\r\\nRange: bytes=0-1048575\\r\\n\\r\\n' \\
        --recv-chunk-size 1 --recv-delay-ms 5 --recv-timeout 30 \\
        --out /tmp/streamed_response.bin

Exit status is 0 if the connection completed (all bytes sent and either EOF
or --recv-timeout reached while reading the response), non-zero on a socket
error. This script does not interpret HTTP itself — it is a byte-fidelity
tool; pipe --out to a parser or eyeball it for the expected status
line/headers.
"""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path


def build_padded_header_request(pad_bytes: int) -> bytes:
    """A syntactically-plausible request whose Host header is padded to
    `pad_bytes`, for exercising a server's max-header-size limit."""
    padding = "x" * pad_bytes
    request = f"GET / HTTP/1.1\r\nHost: musicbox\r\nX-Pad: {padding}\r\n\r\n"
    return request.encode("ascii")


def send_fragmented(sock: socket.socket, payload: bytes, chunk_size: int, delay_ms: float,
                     close_after_bytes: int | None) -> int:
    """Send `payload` in `chunk_size`-byte pieces with `delay_ms` between each.
    Returns the number of bytes actually sent. If close_after_bytes is set,
    stops (without closing here — caller closes) once that many bytes have
    gone out, simulating a client that disconnects mid-request/mid-stream."""
    sent_total = 0
    offset = 0
    while offset < len(payload):
        if close_after_bytes is not None and sent_total >= close_after_bytes:
            break
        piece = payload[offset:offset + chunk_size]
        sent = sock.send(piece)  # deliberately not sendall(): observe partial-write behavior too
        if sent == 0:
            raise ConnectionError("send() returned 0 — peer likely closed the connection")
        offset += sent
        sent_total += sent
        if delay_ms > 0:
            time.sleep(delay_ms / 1000.0)
    return sent_total


def recv_response(sock: socket.socket, recv_chunk_size: int, recv_delay_ms: float,
                   timeout_s: float) -> bytes:
    """Read the response in `recv_chunk_size`-byte pulls with `recv_delay_ms`
    between each (simulates a slow client draining a stream), stopping on EOF
    or once `timeout_s` total wall-clock has elapsed."""
    sock.settimeout(timeout_s)
    chunks: list[bytes] = []
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        try:
            data = sock.recv(recv_chunk_size)
        except socket.timeout:
            break
        if not data:
            break
        chunks.append(data)
        if recv_delay_ms > 0:
            time.sleep(recv_delay_ms / 1000.0)
    return b"".join(chunks)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Send a payload to a TCP server split into controlled fragments (Plan.md §15 networking scenarios).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, required=True)

    payload_group = parser.add_mutually_exclusive_group()
    payload_group.add_argument("--data", help="Raw request bytes as a string (use $'...' in bash for \\r\\n).")
    payload_group.add_argument("--data-file", type=Path, help="Read raw request bytes from a file.")
    payload_group.add_argument("--pad-header-bytes", type=int,
                                help="Build a GET / request with a header padded to this many bytes (huge-header test).")

    parser.add_argument("--chunk-size", type=int, default=1, help="Bytes per send() call (default: 1).")
    parser.add_argument("--delay-ms", type=float, default=0.0, help="Delay between send() calls, in milliseconds.")
    parser.add_argument("--repeat", type=int, default=1,
                         help="Send the payload this many times over the same connection (pipelined requests).")
    parser.add_argument("--close-after-bytes", type=int, default=None,
                         help="Stop sending (and close without finishing) after this many bytes — simulates a mid-transfer disconnect.")

    parser.add_argument("--recv-chunk-size", type=int, default=4096, help="Bytes per recv() call (default: 4096; use 1 for a byte-at-a-time slow reader).")
    parser.add_argument("--recv-delay-ms", type=float, default=0.0, help="Delay between recv() calls, in milliseconds.")
    parser.add_argument("--recv-timeout", type=float, default=5.0, help="Total seconds to keep reading the response (default: 5).")
    parser.add_argument("--connect-timeout", type=float, default=5.0, help="Seconds to wait for the TCP connect (default: 5).")
    parser.add_argument("--no-recv", action="store_true", help="Don't read a response at all (send-only, e.g. for pure disconnect tests).")

    parser.add_argument("--out", type=Path, help="Write the raw response bytes to this file instead of stdout.")
    parser.add_argument("-v", "--verbose", action="store_true")

    args = parser.parse_args()

    if args.data is not None:
        payload = args.data.encode("utf-8")
    elif args.data_file is not None:
        payload = args.data_file.read_bytes()
    elif args.pad_header_bytes is not None:
        payload = build_padded_header_request(args.pad_header_bytes)
    else:
        parser.error("one of --data, --data-file, or --pad-header-bytes is required")
        return 2  # unreachable, keeps type-checkers happy

    if args.chunk_size < 1:
        parser.error("--chunk-size must be >= 1")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(args.connect_timeout)
        sock.connect((args.host, args.port))
        sock.settimeout(None)

        total_sent = 0
        for i in range(args.repeat):
            if args.verbose:
                print(f"[fragment_client] sending copy {i + 1}/{args.repeat} of {len(payload)} bytes "
                      f"in chunks of {args.chunk_size} (delay={args.delay_ms}ms)", file=sys.stderr)
            sent = send_fragmented(sock, payload, args.chunk_size, args.delay_ms, args.close_after_bytes)
            total_sent += sent
            if args.close_after_bytes is not None and sent < len(payload):
                if args.verbose:
                    print(f"[fragment_client] stopped after {sent}/{len(payload)} bytes "
                          f"(--close-after-bytes {args.close_after_bytes}); closing socket now", file=sys.stderr)
                break

        response = b""
        if not args.no_recv:
            response = recv_response(sock, args.recv_chunk_size, args.recv_delay_ms, args.recv_timeout)

        sock.close()

        if args.verbose:
            print(f"[fragment_client] sent {total_sent} bytes total, received {len(response)} bytes", file=sys.stderr)

        if args.out:
            args.out.write_bytes(response)
        elif response:
            sys.stdout.buffer.write(response)

        return 0
    except OSError as exc:
        print(f"fragment_client: connection error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
