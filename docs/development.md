# Development Guide

## Prerequisites

* CMake >= 3.24, a C++20 compiler (Clang 15+/GCC 12+/AppleClang 15+).
* SQLite3 dev headers (system package; `brew install sqlite3` / `apt install
  libsqlite3-dev` — CMake also falls back to `FetchContent` if not found).
* OpenSSL `libcrypto` dev headers, used for SHA-256 (`brew install openssl@3` /
  `apt install libssl-dev`; see `docs/adr/0010-use-openssl-for-sha256.md`).
* Xcode 15+ for `client-ios/`.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Sanitizer build (development only):

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
    -DMUSICBOX_SANITIZE=address,undefined
cmake --build build-asan -j
```

## Repository Layout

See `docs/architecture.md` §3.

## Server CLI

```bash
musicbox-server run    --config config/musicbox.toml
musicbox-server scan   --config config/musicbox.toml
musicbox-server status --config config/musicbox.toml
musicbox-server doctor --config config/musicbox.toml
musicbox-server version
```

`run` and `scan` are implemented: `run` starts the real event loop + HTTP API
(`docs/api.md`); `scan` runs the library scanner once against `[library].paths`
and exits (useful for cron/manual re-index without restarting the server).
`status` (query a running instance's `/api/v1/status` from the CLI) and
`doctor` (config/DB/permissions preflight checks) are not implemented yet —
use `curl http://<host>:<port>/api/v1/status` in the meantime.

## Configuration

TOML, loaded only when `--config <path>` is explicitly passed (otherwise
`run`/`scan` use built-in defaults: `0.0.0.0:8080`, no library paths,
`./musicbox.db`) — see `Plan.md` §19 for the canonical file example. Only
`--config` exists today; per-field CLI overrides (`--port`, `--library`,
`--log-level`) described in earlier drafts of this doc are not implemented.

## Git Workflow

* `main` is always buildable.
* One branch (or worktree) per agent: `agent/networking`, `agent/http`,
  `agent/database`, `agent/api`, `agent/ios`, `agent/sync`, `agent/deployment`,
  `agent/testing`.
* Commits are scoped and imperative: `feat(net): add RAII socket wrapper`,
  `fix(http): handle chunked Range header casing`, `test(db): cover incremental
  rescan`. No `stuff` / `wip` / `updates` commits.
* Changes to a header under `shared/include` or a contract described in
  `docs/*.md` that another agent already depends on must be additive and get a
  short ADR under `docs/adr/` explaining what/why/who's affected/compat impact.
* Integration lead (Agent 0) merges agent branches into `main`, running the full
  build + test suite (and sanitizers where practical) before each merge.

## Testing

Catch2 (via `FetchContent`) for unit + integration tests under `tests/`. Every
subsystem lands with tests in the same change: HTTP parser, range parser, router,
DB repositories, library scanning, buffer handling, JSON (de)serialization, cache
policies (iOS). See `Plan.md` §15 for the required scenario list (fragmented
requests, pipelined requests, partial writes, slow clients, mid-transfer
disconnects, huge headers, concurrent streams).

## CI

GitHub Actions (`.github/workflows/ci.yml`) runs, on Linux and macOS:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

plus a `clang-format --dry-run --Werror` check and a strict-warnings build
(`-Wall -Wextra -Wpedantic`, no warnings suppressed cross-platform without a
documented reason).
