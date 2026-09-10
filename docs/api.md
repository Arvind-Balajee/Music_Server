# REST API

Owner: Agent 4 (API Layer), built on `server/include/musicbox/http/` (Agent 2) and
`server/include/musicbox/db/` (Agent 3). Base path: `/api/v1`.

## Conventions

* All responses are `application/json` except `/stream` (audio bytes) and
  `/artwork` (image bytes).
* Pagination: `?limit=&offset=`, default `limit=100`, max `limit=500`.
* Errors always look like:

```json
{ "error": { "code": "TRACK_NOT_FOUND", "message": "The requested track does not exist." } }
```

with a matching 4xx/5xx status code. Error codes are stable identifiers
(`SCREAMING_SNAKE_CASE`), suitable for the iOS client to switch on without parsing
`message`.

## Endpoints

```text
GET  /api/v1/status

GET  /api/v1/artists
GET  /api/v1/artists/{id}

GET  /api/v1/albums
GET  /api/v1/albums/{id}

GET  /api/v1/tracks
GET  /api/v1/tracks/{id}
GET  /api/v1/tracks/{id}/stream        (supports Range)
GET  /api/v1/tracks/{id}/artwork

GET  /api/v1/search?q=

GET    /api/v1/playlists
GET    /api/v1/playlists/{id}
POST   /api/v1/playlists
PUT    /api/v1/playlists/{id}
DELETE /api/v1/playlists/{id}
POST   /api/v1/playlists/{id}/tracks
DELETE /api/v1/playlists/{id}/tracks/{trackId}
```

### `GET /api/v1/status`

```json
{ "status": "ok", "version": "0.1.0", "trackCount": 4213, "libraryRoots": 1 }
```

### `GET /api/v1/tracks/{id}`

```json
{
    "id": 172,
    "title": "Instant Crush",
    "artist": { "id": 13, "name": "Daft Punk" },
    "album": { "id": 29, "title": "Random Access Memories" },
    "trackNumber": 5,
    "durationMs": 337000,
    "codec": "flac",
    "fileSize": 43892012,
    "streamUrl": "/api/v1/tracks/172/stream"
}
```

### `GET /api/v1/tracks/{id}/stream`

Standard HTTP range semantics (see `docs/http.md`): a plain `GET` returns `200` with
the full file and `Accept-Ranges: bytes`; a `Range` header returns `206` with
`Content-Range`. The track is resolved `id -> DB row -> absolute path` server-side —
the path is never accepted from the client.

### Error codes (initial set)

```text
TRACK_NOT_FOUND, ALBUM_NOT_FOUND, ARTIST_NOT_FOUND, PLAYLIST_NOT_FOUND
BAD_REQUEST, RANGE_NOT_SATISFIABLE, VALIDATION_ERROR, INTERNAL_ERROR
```

## Route -> Repository mapping

Every handler is a thin adapter: parse path/query params -> call a
`TrackRepository`/`AlbumRepository`/... method -> serialize the result. No SQL, no
filesystem access, and no business logic belongs directly in a route handler.

## Composition Root

`main()` builds concrete `Sqlite*Repository` instances and injects them into the API
handlers through the repository interfaces in `docs/database.md`. Tests inject
in-memory fakes instead — the API layer must not `#include` any SQLite header.

## Proposed: Sync Endpoints (not yet implemented server-side)

Owner-to-be: Agent 4 (API Layer). Proposed by Agent 6 (Sync Client) per the
shared-file coordination rule in `docs/development.md` / `Plan.md` §27 — this
section documents WHAT/WHY/WHO's-affected/compat-impact per that rule, since
`sync-client/` already codes against this shape behind an interface
(`ServerManifestSource`, `UploadTransport` in `sync-client/include/musicbox/sync/`)
so the two workstreams don't block each other (Plan.md §25).

**Status: proposed only. Neither endpoint exists in `server/` yet.**
`sync-client`'s `HttpServerManifestSource` / `HttpUploadTransport` are written
against this contract and will fail at runtime with a clear error until Agent 4
implements it; all sync-client tests exercise the diff/scan logic against
`MockServerManifestSource` / `NullUploadTransport` instead, so nothing here blocks
sync-client's own test suite from passing.

* **WHAT**: two new, additive endpoints — no existing route's request/response
  shape changes.

  ```text
  GET  /api/v1/sync/manifest
  POST /api/v1/sync/tracks
  ```

* **WHY**: `musicbox-sync` (Plan.md §12) needs a cheap way to learn what the
  server already has (to avoid re-uploading unchanged files) and a way to push
  new/changed files to it. Every other `/api/v1/*` route is read-oriented
  library browsing for the iOS client; sync is a distinct, additive concern.

* **WHO's affected**: Agent 4 (implements these against `TrackRepository` /
  `LibraryRootRepository`, `docs/database.md`), Agent 3 (the underlying scan/hash
  data these endpoints expose already exists as `Track.relativePath` /
  `Track.contentHash` / `Track.modifiedAt` — no schema change needed), Agent 6
  (consumes both from `sync-client/`).

* **Compat impact**: none. Purely additive; no existing endpoint, error code, or
  response shape is touched.

### `GET /api/v1/sync/manifest`

Returns every non-deleted track's identity for change detection. Mirrors
`Track.relativePath` / `fileSizeBytes` / `modifiedAt` / `contentHash`
(`docs/database.md`) — deliberately does **not** reuse the `/api/v1/tracks`
response shape, since `relativePath` must never otherwise be exposed to a client
(`docs/database.md`, `docs/architecture.md` §7); this is the one sync-specific
exception, scoped to this proposed endpoint.

```json
[
    {
        "relativePath": "Daft Punk/Random Access Memories/05 Instant Crush.flac",
        "size": 43892012,
        "mtime": 1732650000,
        "contentHash": "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"
    }
]
```

`mtime` is a unix epoch timestamp in whole seconds (not milliseconds, unlike
`durationMs` elsewhere in this API — chosen to match `std::filesystem`'s
practical resolution and keep the sync client's size+mtime pre-check simple).
Should support pagination the same way `/api/v1/tracks` does once library sizes
warrant it; not required for the MVP.

### `POST /api/v1/sync/tracks`

Uploads one file's raw bytes. Proposed request shape:

```text
POST /api/v1/sync/tracks HTTP/1.1
X-Relative-Path: Daft Punk/Random Access Memories/05 Instant Crush.flac
Content-Type: application/octet-stream
Content-Length: 43892012

<raw file bytes>
```

Server behavior (proposed): write to a staging path under the configured
library root, then run the same metadata-extraction + upsert path
`LibraryScanner` uses for an on-disk new/modified file (`docs/database.md`), so
sync-uploaded tracks appear through `/api/v1/tracks` identically to ones found
by a filesystem scan. Suggested response: `201 Created` with the resulting
`{ "id": <TrackId> }`, or the standard error envelope (e.g. `VALIDATION_ERROR`
for a path that escapes the library root — same traversal protection as
streaming, `docs/architecture.md` §7).

**Known limitation, noted for whoever implements this**: `sync-client`'s
`HttpUploadTransport` sends the whole file in one request (no resumability).
Resuming a large interrupted upload would need this endpoint (or a new
companion one) to report how many bytes of a given `relativePath` it already
has, mirroring the `Range` support `docs/http.md` already defines for
downloads — deferred per Plan.md §12 ("design resumable transfers if
practical").
