#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace musicbox::library {

// Raw metadata pulled from a single audio file, prior to being resolved against
// (or inserted into) the artists/albums tables. Kept separate from
// musicbox::Track because a Track has database-assigned foreign keys
// (artistId/albumId) that don't exist until this raw data is reconciled with the
// database (see docs/database.md, LibraryScanner).
struct RawTrackMetadata {
    std::string title;
    std::optional<std::string> artist;
    std::optional<std::string> album;
    std::optional<std::string> albumArtist;
    std::optional<int> trackNumber;
    std::optional<int> discNumber;
    std::optional<int> year;
    std::optional<std::string> genre;

    std::int64_t durationMs = 0;
    std::string codec;
    std::optional<int> bitrateKbps;
    bool hasArtwork = false;
};

// Wraps a third-party tag-reading library (e.g. TagLib) behind a narrow interface
// so it can be mocked in tests and swapped without touching scanner logic.
class MetadataExtractor {
public:
    virtual ~MetadataExtractor() = default;

    // Returns std::nullopt if the file cannot be parsed (corrupt/unsupported) —
    // the scanner logs and skips such files rather than failing the whole scan.
    [[nodiscard]] virtual std::optional<RawTrackMetadata>
    extract(const std::string& absolutePath) = 0;
};

[[nodiscard]] std::unique_ptr<MetadataExtractor> makeDefaultMetadataExtractor();

} // namespace musicbox::library
