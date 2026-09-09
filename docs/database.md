# Database & Music Library

Owner: Agent 3 (Music Library + SQLite). This document is the contract for
`shared/include/musicbox/model/` and `server/include/musicbox/db/`.

## Domain Models (`shared/include/musicbox/model/`)

Strong, non-interchangeable IDs (`Id<Tag>` in `Ids.hpp`) are used for every entity so
an `ArtistId` can never be accidentally passed where a `TrackId` is expected.

* `Artist { id, name, sortName? }`
* `Album { id, title, albumArtistId?, year?, hasArtwork }`
* `Track` — see `Track.hpp` for the full field list (title, artistId, albumId,
  albumArtist, trackNumber, discNumber, year, genre, duration, codec, bitrateKbps,
  fileSizeBytes, relativePath, contentHash, modifiedAt, hasArtwork). `relativePath`
  is relative to its `LibraryRoot` and is **never** sent to a client directly —
  clients only ever see a `TrackId` and a `streamUrl`.
* `Playlist { id, name, createdAt, updatedAt }` + `PlaylistTrack { playlistId,
  trackId, position }`.
* `LibraryRoot { id, absolutePath, addedAt, lastScanAt? }`.

## SQLite Schema (minimum, `server/src/db/migrations/0001_init.sql`)

```sql
CREATE TABLE library_roots (
    id            INTEGER PRIMARY KEY,
    path          TEXT NOT NULL UNIQUE,
    added_at      INTEGER NOT NULL,
    last_scan_at  INTEGER
);

CREATE TABLE artists (
    id         INTEGER PRIMARY KEY,
    name       TEXT NOT NULL,
    sort_name  TEXT,
    UNIQUE(name)
);

CREATE TABLE albums (
    id               INTEGER PRIMARY KEY,
    title            TEXT NOT NULL,
    album_artist_id  INTEGER REFERENCES artists(id),
    year             INTEGER,
    has_artwork      INTEGER NOT NULL DEFAULT 0,
    UNIQUE(title, album_artist_id)
);

CREATE TABLE tracks (
    id                INTEGER PRIMARY KEY,
    library_root_id   INTEGER NOT NULL REFERENCES library_roots(id),
    title             TEXT NOT NULL,
    artist_id         INTEGER REFERENCES artists(id),
    album_id          INTEGER REFERENCES albums(id),
    track_number      INTEGER,
    disc_number       INTEGER,
    year              INTEGER,
    genre             TEXT,
    duration_ms       INTEGER NOT NULL,
    codec             TEXT NOT NULL,
    bitrate_kbps      INTEGER,
    file_size_bytes   INTEGER NOT NULL,
    relative_path     TEXT NOT NULL,
    content_hash      TEXT NOT NULL,
    modified_at       INTEGER NOT NULL,
    has_artwork       INTEGER NOT NULL DEFAULT 0,
    deleted_at        INTEGER,
    UNIQUE(library_root_id, relative_path)
);
CREATE INDEX idx_tracks_artist ON tracks(artist_id);
CREATE INDEX idx_tracks_album  ON tracks(album_id);
CREATE INDEX idx_tracks_title  ON tracks(title COLLATE NOCASE);

CREATE TABLE playlists (
    id          INTEGER PRIMARY KEY,
    name        TEXT NOT NULL,
    created_at  INTEGER NOT NULL,
    updated_at  INTEGER NOT NULL
);

CREATE TABLE playlist_tracks (
    playlist_id  INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,
    track_id     INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,
    position     INTEGER NOT NULL,
    PRIMARY KEY (playlist_id, track_id)
);

CREATE TABLE schema_migrations (
    version      INTEGER PRIMARY KEY,
    applied_at   INTEGER NOT NULL
);
```

Migrations are plain numbered `.sql` files applied in order and recorded in
`schema_migrations`; there is no down-migration requirement for the MVP, but a
migration must never be edited after it has shipped — add a new one instead.

`tracks.deleted_at` is a soft-delete marker: a file removed from disk is marked
rather than hard-deleted immediately, so playlists referencing it don't silently
break mid-scan. A later cleanup pass (or the same scan, after a grace period) may
hard-delete.

## Repository Interfaces (`server/include/musicbox/db/`)

```cpp
struct TrackQuery {
    std::optional<ArtistId> artistId;
    std::optional<AlbumId> albumId;
    std::optional<std::string> search; // matches title, artist, album
    std::size_t limit = 100;
    std::size_t offset = 0;
};

class TrackRepository {
public:
    virtual ~TrackRepository() = default;
    virtual std::optional<Track> findById(TrackId id) = 0;
    virtual std::vector<Track> list(const TrackQuery& query) = 0;
    virtual std::size_t count(const TrackQuery& query) = 0;
};
```

Analogous `ArtistRepository`, `AlbumRepository`, `PlaylistRepository`,
`LibraryRootRepository` interfaces live alongside it. The API layer (Agent 4) codes
against these interfaces only; `Sqlite*Repository` implementations (Agent 3) are
swapped in at composition time (`main()`), and tests use in-memory fakes. **One
`sqlite3*` connection per thread** — repositories are constructed per-worker-thread
in the server's thread pool, never shared across threads (see `docs/architecture.md`
§4 and `docs/networking.md`).

## Library Scanner

`LibraryScanner` walks each configured `library_roots` path (recursively) looking
for `.mp3 .flac .m4a .aac .wav`. For each file:

```text
not in DB                              -> extract metadata, insert
in DB, size+mtime unchanged            -> skip (no re-hash, no re-parse)
in DB, size+mtime changed              -> re-hash; if content_hash differs, re-extract
                                           metadata and update the row
in DB, file no longer present on disk  -> set deleted_at = now()
```

`size + mtime` is the cheap pre-check; SHA-256 content hashing only runs when that
pre-check indicates a possible change, or during `musicbox-server verify`/
`musicbox-sync verify`. Symlinks are not followed during traversal (prevents
traversal loops and escapes outside the configured root).

Metadata extraction uses TagLib (third-party, acceptable per project scope) behind
a small `MetadataExtractor` interface so it can be swapped/mocked in tests.

## Playlists

`playlists` / `playlist_tracks` are ordinary CRUD tables; `position` is a dense
0-based integer maintained by the repository on insert/delete/reorder (no gaps
required for the MVP; a reorder rewrites the affected range).
