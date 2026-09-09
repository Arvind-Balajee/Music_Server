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
