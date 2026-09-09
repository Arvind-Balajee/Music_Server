# 0002: Use SQLite for the music metadata database

## Context

The server needs to store and query artists/albums/tracks/playlists for a library
that may run into the tens of thousands of tracks, on a single embedded device with
no separate database server.

## Decision

Use SQLite3, one connection per worker thread, accessed only through repository
interfaces (`docs/database.md`).

## Alternatives Considered

* **Postgres/MySQL**: requires a separate server process — unnecessary operational
  weight for a single-device embedded system, and explicitly against `Plan.md` §31
  ("avoid unnecessary cloud infrastructure" / keep this a local embedded system).
* **Flat files / custom binary index**: would need to reimplement indexing, querying,
  and transactional updates that SQLite already provides for free, for no real
  benefit at this scale.
* **In-process key-value store (e.g. LevelDB)**: workable, but loses relational
  queries (search across joined artist/album/track) that the API needs, and SQLite
  is the more standard choice for this domain (see Navidrome, Plex, etc., which all
  use SQLite).

## Consequences

* One `sqlite3*` per thread avoids cross-thread connection sharing bugs; repository
  implementations must be constructed per-worker, not shared as a singleton.
* WAL mode is used to allow the scanner (writer) and API handlers (readers) to
  proceed concurrently without blocking each other on every query.
* Schema evolves via numbered, append-only migration files (`docs/database.md`).
