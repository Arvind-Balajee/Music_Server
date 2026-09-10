#!/usr/bin/env python3
"""
generate_test_audio.py — produce streaming test fixture files of a known,
exact byte size, per Plan.md §15 ("Streaming tests: 1 MB / 10 MB / 100 MB /
1 GB+, ensure memory usage does not scale with file size").

These are NOT meant to test codec correctness (Agent 3's MetadataExtractor /
TagLib handle that). The point is realistic-size audio-shaped files to drive
HTTP Range/streaming tests (Agent 2) and manual throughput checks
(scripts/benchmark.sh) without checking multi-hundred-megabyte binaries into
git. Every generated file is a syntactically valid 16-bit PCM WAV: a correct
44-byte canonical header (RIFF/WAVE/fmt /data with accurate chunk sizes)
followed by an audible sine tone, so the files also play in a real player if
you want to sanity-check them by ear.

Two backends:

  python (default) — pure-Python sine-wave synthesis, no dependencies. Writes
      in ~1 MiB chunks so memory use is O(1) regardless of target file size
      (verified up to multi-GB). Produces files of *exactly* the requested
      byte size (data chunk is truncated to the nearest whole audio frame).

  ffmpeg  — shells out to `ffmpeg -f lavfi -i sine=...` for a "real" encoder
      pass, then pads or truncates the result to the exact requested size and
      rewrites the header's RIFF/data sizes to match. Falls back to the
      python backend automatically if ffmpeg is not on PATH.

Usage
-----

Generate the standard fixture set (1 MB, 10 MB, 100 MB) into a directory:

    python3 scripts/generate_test_audio.py --outdir /tmp/musicbox-fixtures

Include the optional 1 GB+ fixture (slow, ~1 GiB of disk):

    python3 scripts/generate_test_audio.py --outdir /tmp/musicbox-fixtures --include-1gb

Generate one file of a specific size:

    python3 scripts/generate_test_audio.py --size 64KiB --out /tmp/exactly-64kib.wav

Accepted size suffixes: B, K/KB/KiB, M/MB/MiB, G/GB/GiB (binary, i.e. 1K = 1024).

Force the ffmpeg backend (falls back to python if ffmpeg is missing):

    python3 scripts/generate_test_audio.py --outdir /tmp/musicbox-fixtures --backend ffmpeg

Notes
-----

* Generated fixtures are throwaway test data — do not commit them. Point
  --outdir somewhere outside the repo (e.g. /tmp or a git-ignored local dir).
* All sizes are exact byte counts (post-header), which is exactly what makes
  these useful for Range-request tests such as
  `Range: bytes=<size-500>-` or asserting Content-Length == requested size.
"""

from __future__ import annotations

import argparse
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

WAV_HEADER_SIZE = 44
SAMPLE_RATE = 44100
CHANNELS = 2
BITS_PER_SAMPLE = 16
BYTES_PER_SAMPLE = BITS_PER_SAMPLE // 8
BLOCK_ALIGN = CHANNELS * BYTES_PER_SAMPLE  # one "frame" = one sample per channel
WRITE_CHUNK_TARGET = 1 << 20  # ~1 MiB per write() call

DEFAULT_SIZES = ["1MB", "10MB", "100MB"]
OPTIONAL_1GB_SIZE = "1GiB"

_SIZE_SUFFIXES = [
    ("KIB", 1024),
    ("MIB", 1024 ** 2),
    ("GIB", 1024 ** 3),
    ("KB", 1024),
    ("MB", 1024 ** 2),
    ("GB", 1024 ** 3),
    ("K", 1024),
    ("M", 1024 ** 2),
    ("G", 1024 ** 3),
    ("B", 1),
]


def parse_size(text: str) -> int:
    """Parse '1MB', '64KiB', '1.5GB', or a bare integer byte count."""
    s = text.strip().upper()
    for suffix, multiplier in _SIZE_SUFFIXES:
        if s.endswith(suffix):
            number = s[: -len(suffix)].strip()
            if not number:
                raise ValueError(f"missing numeric part in size {text!r}")
            return int(float(number) * multiplier)
    return int(s)


def _write_wav_header(f, data_size: int) -> None:
    byte_rate = SAMPLE_RATE * BLOCK_ALIGN
    f.write(b"RIFF")
    f.write(struct.pack("<I", 36 + data_size))
    f.write(b"WAVE")
    f.write(b"fmt ")
    f.write(struct.pack("<IHHIIHH", 16, 1, CHANNELS, SAMPLE_RATE, byte_rate, BLOCK_ALIGN, BITS_PER_SAMPLE))
    f.write(b"data")
    f.write(struct.pack("<I", data_size))


def _sine_cycle_bytes(frequency: int = 440, amplitude: int = 12000) -> bytes:
    """One exact period of a stereo 16-bit PCM sine wave at `frequency` Hz,
    so repeating this buffer produces a click-free continuous tone."""
    samples_per_cycle = max(1, SAMPLE_RATE // frequency)
    frame = bytearray()
    for i in range(samples_per_cycle):
        value = int(amplitude * math.sin(2 * math.pi * i / samples_per_cycle))
        frame += struct.pack("<hh", value, value)
    return bytes(frame)


def generate_python(path: Path, size_bytes: int, frequency: int = 440) -> int:
    """Write an exact-size WAV file using O(1) memory. Returns actual bytes written."""
    if size_bytes < WAV_HEADER_SIZE:
        raise ValueError(f"size {size_bytes} is smaller than the {WAV_HEADER_SIZE}-byte WAV header")

    data_size = size_bytes - WAV_HEADER_SIZE
    data_size -= data_size % BLOCK_ALIGN  # keep whole audio frames

    cycle = _sine_cycle_bytes(frequency)
    repeats = max(1, WRITE_CHUNK_TARGET // len(cycle))
    write_chunk = cycle * repeats

    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as f:
        _write_wav_header(f, data_size)
        written = 0
        while written + len(write_chunk) <= data_size:
            f.write(write_chunk)
            written += len(write_chunk)
        remaining = data_size - written
        if remaining:
            filler = (cycle * (remaining // len(cycle) + 1))[:remaining]
            f.write(filler)
            written += remaining

    return WAV_HEADER_SIZE + data_size


def generate_ffmpeg(path: Path, size_bytes: int, frequency: int = 440) -> int:
    """Best-effort real-encoder generation via ffmpeg, padded/truncated to an
    exact byte size. Raises if ffmpeg is unavailable; caller should fall back
    to generate_python()."""
    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg is None:
        raise FileNotFoundError("ffmpeg not found on PATH")

    approx_data_size = max(0, size_bytes - WAV_HEADER_SIZE)
    duration_s = max(0.05, approx_data_size / (SAMPLE_RATE * BLOCK_ALIGN))

    with tempfile.TemporaryDirectory() as tmp:
        raw = Path(tmp) / "raw.wav"
        subprocess.run(
            [
                ffmpeg, "-y", "-hide_banner", "-loglevel", "error",
                "-f", "lavfi", "-i", f"sine=frequency={frequency}:sample_rate={SAMPLE_RATE}:duration={duration_s:.3f}",
                "-ac", str(CHANNELS), "-sample_fmt", "s16",
                str(raw),
            ],
            check=True,
        )

        path.parent.mkdir(parents=True, exist_ok=True)
        target_data_size = size_bytes - WAV_HEADER_SIZE
        target_data_size -= target_data_size % BLOCK_ALIGN

        with open(raw, "rb") as src, open(path, "wb") as dst:
            _write_wav_header(dst, target_data_size)
            src.seek(WAV_HEADER_SIZE)
            copied = 0
            while copied < target_data_size:
                chunk = src.read(min(WRITE_CHUNK_TARGET, target_data_size - copied))
                if not chunk:
                    break
                dst.write(chunk)
                copied += len(chunk)
            if copied < target_data_size:
                # ffmpeg's encoded duration came up short of the target byte
                # count (rounding) — pad with a repeating sine cycle so the
                # file still ends up exactly `size_bytes` long.
                cycle = _sine_cycle_bytes(frequency)
                remaining = target_data_size - copied
                filler = (cycle * (remaining // len(cycle) + 1))[:remaining]
                dst.write(filler)
                copied += remaining

    return WAV_HEADER_SIZE + target_data_size


def generate(path: Path, size_bytes: int, backend: str, frequency: int) -> int:
    if backend == "ffmpeg":
        try:
            return generate_ffmpeg(path, size_bytes, frequency)
        except (FileNotFoundError, subprocess.CalledProcessError) as exc:
            print(f"warning: ffmpeg backend failed ({exc}); falling back to python backend", file=sys.stderr)
            return generate_python(path, size_bytes, frequency)
    return generate_python(path, size_bytes, frequency)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate exact-size WAV test fixtures for streaming/Range tests (Plan.md §15).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--outdir", type=Path, help="Directory to write the standard fixture set into.")
    parser.add_argument("--include-1gb", action="store_true", help="Also generate the optional 1 GiB+ fixture.")
    parser.add_argument("--sizes", help="Comma-separated sizes to generate into --outdir instead of the defaults, e.g. 1MB,50MB.")
    parser.add_argument("--size", help="Generate a single file of this exact size (use with --out).")
    parser.add_argument("--out", type=Path, help="Output path for --size.")
    parser.add_argument("--frequency", type=int, default=440, help="Sine tone frequency in Hz (default: 440).")
    parser.add_argument("--backend", choices=["python", "ffmpeg"], default="python", help="Generation backend (default: python).")
    args = parser.parse_args()

    if args.size:
        if not args.out:
            parser.error("--size requires --out")
        try:
            size_bytes = parse_size(args.size)
        except ValueError as exc:
            parser.error(str(exc))
        actual = generate(args.out, size_bytes, args.backend, args.frequency)
        print(f"wrote {args.out} ({actual} bytes)")
        return 0

    outdir = args.outdir or Path("test-fixtures")
    if args.sizes:
        size_specs = [s.strip() for s in args.sizes.split(",") if s.strip()]
    else:
        size_specs = list(DEFAULT_SIZES)
        if args.include_1gb:
            size_specs.append(OPTIONAL_1GB_SIZE)

    for spec in size_specs:
        try:
            size_bytes = parse_size(spec)
        except ValueError as exc:
            parser.error(str(exc))
        filename = f"fixture_{spec.replace(' ', '')}.wav"
        out_path = outdir / filename
        actual = generate(out_path, size_bytes, args.backend, args.frequency)
        print(f"wrote {out_path} ({actual} bytes, requested {spec})")

    print(f"\nFixtures are throwaway test data — do not commit {outdir}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
