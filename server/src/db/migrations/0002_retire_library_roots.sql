-- A library root that is no longer listed in [library].paths is *retired*
-- rather than deleted. Its tracks stay in the database (soft-deleted via
-- tracks.deleted_at), so playlist entries referencing them survive, and
-- re-adding the same path revives both the root and its tracks.
--
-- Deleting the row outright isn't an option regardless: tracks.library_root_id
-- REFERENCES library_roots(id) with no ON DELETE clause, and every connection
-- runs with foreign_keys=ON (SqliteConnection.cpp), so the DELETE would be
-- rejected while any track row still points at it.

ALTER TABLE library_roots ADD COLUMN retired_at INTEGER;
