# ADR 0010: Use OpenSSL for SHA-256

## Status

Accepted.

## Context

`musicbox::util::sha256File()` (`shared/src/util/Sha256.cpp`) hashes every
newly scanned track and every file whose size or mtime changed, on both the
server's library scanner and the sync client. It was originally a portable
C++ implementation of FIPS 180-4, chosen to avoid a crypto dependency for a
non-security content-change hash.

Measured on Apple Silicon, that implementation ran at roughly 56 MB/s. It
dominated scan time: a first scan of a 500 MB WAV took about 9 s, nearly all
of it hashing, and a first scan of 119 MB of MP3s took about 2 s, almost none
of it tag reading. `openssl dgst -sha256` hashed the same 500 MB file in
0.27 s by using the CPU's SHA-256 instructions.

## Decision

Implement `sha256File()` and `sha256Bytes()` on OpenSSL's EVP digest API
(`libcrypto`), which selects a hardware-accelerated SHA-256 at runtime when
the CPU provides one (ARMv8 crypto extensions, x86 SHA-NI) and an optimized
assembly implementation otherwise.

The public header (`shared/include/musicbox/util/Sha256.hpp`) is unchanged,
and so are the digests it produces. Files are still read in 64 KiB chunks, so
memory use stays bounded.

## Consequences

* New build dependency: OpenSSL `libcrypto` headers — `libssl-dev` on
  Debian/Raspberry Pi OS/Ubuntu, `openssl@3` on macOS via Homebrew
  (`shared/CMakeLists.txt` finds Homebrew's keg-only install automatically).
  The runtime library ships with Raspberry Pi OS by default.
* Existing `content_hash` values stay valid: SHA-256 is SHA-256, so nothing
  needs to be rehashed.
* The speedup depends on the CPU. Raspberry Pi 5's Cortex-A76 has SHA-256
  instructions; the Pi 4's BCM2711 does not expose the ARMv8 crypto
  extensions, so there OpenSSL falls back to optimized assembly and the gain
  is smaller.
