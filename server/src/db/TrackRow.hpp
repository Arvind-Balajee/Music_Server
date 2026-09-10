#pragma once

#include "musicbox/model/Track.hpp"

#include "SqliteStatement.hpp"
#include "TimeUtil.hpp"

// Shared between SqliteTrackRepository and SqlitePlaylistRepository (both read
// full `tracks` rows) so the column list/order and the row->Track mapping live
// in exactly one place.
namespace musicbox::db {

inline constexpr const char* kTrackColumns =
    "id, library_root_id, title, artist_id, album_id, track_number, disc_number, "
    "year, genre, duration_ms, codec, bitrate_kbps, file_size_bytes, relative_path, "
    "content_hash, modified_at, has_artwork, deleted_at";

inline std::optional<int> toOptionalInt(std::optional<std::int64_t> value) {
    if (!value.has_value()) {
        return std::nullopt;
    }
    return static_cast<int>(*value);
}

inline musicbox::Track mapTrackRow(SqliteStatement& stmt) {
    musicbox::Track track;
    track.id = musicbox::TrackId(stmt.columnInt64(0));
    track.libraryRootId = musicbox::LibraryRootId(stmt.columnInt64(1));
    track.title = stmt.columnText(2);
    if (auto v = stmt.columnOptionalInt64(3)) {
        track.artistId = musicbox::ArtistId(*v);
    }
    if (auto v = stmt.columnOptionalInt64(4)) {
        track.albumId = musicbox::AlbumId(*v);
    }
    track.trackNumber = toOptionalInt(stmt.columnOptionalInt64(5));
    track.discNumber = toOptionalInt(stmt.columnOptionalInt64(6));
    track.year = toOptionalInt(stmt.columnOptionalInt64(7));
    track.genre = stmt.columnOptionalText(8);
    track.duration = std::chrono::milliseconds(stmt.columnInt64(9));
    track.codec = stmt.columnText(10);
    track.bitrateKbps = toOptionalInt(stmt.columnOptionalInt64(11));
    track.fileSizeBytes = static_cast<std::uint64_t>(stmt.columnInt64(12));
    track.relativePath = stmt.columnText(13);
    track.contentHash = stmt.columnText(14);
    track.modifiedAt = fromEpochMillis(stmt.columnInt64(15));
    track.hasArtwork = stmt.columnInt64(16) != 0;
    track.deletedAt = fromOptionalEpochMillis(stmt.columnOptionalInt64(17));
    return track;
}

} // namespace musicbox::db
