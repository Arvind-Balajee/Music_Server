# 0006: Add write methods to TrackRepository

## Context

`docs/database.md` sketches `TrackRepository` as a read-only interface
(`findById`, `list`, `count`) — reasonable as a minimal example for the API
layer (Agent 4), which only ever reads. But `LibraryScanner` (`docs/database.md`
"Library Scanner", `server/include/musicbox/library/LibraryScanner.hpp`) needs a
write path, and its factory function `makeLibraryScanner(TrackRepository&,
MetadataExtractor&)` is handed only a `TrackRepository` and a
`MetadataExtractor` — no `ArtistRepository`/`AlbumRepository`. Something has to
resolve artist/album names into `artist_id`/`album_id` foreign keys, and it must
be `TrackRepository` itself.

## Decision

Add to `TrackRepository` (server/include/musicbox/db/TrackRepository.hpp),
additively (no existing method signature changed or removed):

* `TrackUpsert` — a plain struct carrying everything extracted from a file
  (title, artist/album *names*, codec, hash, mtime, ...) plus the
  `(libraryRootId, relativePath)` that identifies the row.
* `Track upsert(const TrackUpsert&)` — finds-or-creates the artist/album rows
  by name/title (avoiding the two-writer-race that would exist if the scanner
  called `ArtistRepository`/`AlbumRepository` from a separate connection), then
  inserts or updates the track row for `(libraryRootId, relativePath)`,
  clearing `deleted_at` if the file reappeared.
* `findByPath(LibraryRootId, relativePath)` — including soft-deleted rows, so
  the scanner can tell "unchanged" from "reappeared after deletion" from "brand
  new".
* `listByLibraryRoot(LibraryRootId)` — including soft-deleted rows, so the
  scanner can compute "present in DB but no longer on disk" as a set
  difference against what it found while walking the filesystem.
* `bool softDelete(TrackId, time_point)` — sets `deleted_at`.

`ArtistRepository`/`AlbumRepository` are left exactly as documented
(read-only) — the API layer's only consumers of those two interfaces don't
need writes, and giving the scanner a second, independent path to the
artists/albums tables (via separate repository connections) would reintroduce
the race this design avoids.

## Alternatives Considered

* **Pass `ArtistRepository`/`AlbumRepository` into `makeLibraryScanner` too,
  with write methods added to those interfaces instead.** Rejected: three
  separate `sqlite3*` connections writing to related tables (`artists`,
  `albums`, `tracks`) from one logical operation is exactly the kind of
  cross-connection coordination `docs/database.md`'s "one connection per
  repository instance" rule is meant to keep out of a single unit of work.
  `TrackRepository::upsert` keeps the whole find-or-create-artist /
  find-or-create-album / insert-or-update-track sequence on one connection
  inside one transaction.
* **A separate `TrackWriter` interface** so `TrackRepository` itself stays
  read-only. Rejected for the MVP as unnecessary indirection — no consumer
  needs a read-only-only view of tracks, and it would double the number of
  interfaces this document has to keep in sync for one repository.

## Consequences

* No effect on any code written against the original three read methods —
  this is a pure addition to the vtable.
* Any future fake/mock `TrackRepository` (e.g. for API-layer tests) must
  implement the four new pure virtual methods to compile. As of this ADR, no
  such fakes exist yet in the tree.
* `ArtistRepository`/`AlbumRepository` remain read-only by design (see above),
  which is worth calling out explicitly so a future agent doesn't add
  duplicate write paths there.
