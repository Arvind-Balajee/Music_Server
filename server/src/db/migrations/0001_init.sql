-- 0001_init.sql
--
-- Initial MusicBox schema. Verbatim copy of the schema documented in
-- docs/database.md. Migrations are append-only: never edit this file after it
-- has shipped -- add a new numbered migration instead (docs/database.md).

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
