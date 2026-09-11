#include "musicbox/library/MetadataExtractor.hpp"

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace musicbox::library {

namespace {

std::string toLowerExtension(const std::string& path) {
    auto ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

// Best-effort codec label from the file extension. TagLib doesn't expose a
// single normalized "codec" string across its format backends, and the exact
// codec inside a .m4a container (AAC vs. ALAC) requires deeper inspection
// than the MVP scanner needs — extension is an acceptable approximation for
// display/filtering purposes (docs/database.md's `tracks.codec` column).
std::string codecForExtension(const std::string& lowerExt) {
    if (lowerExt == ".mp3")
        return "mp3";
    if (lowerExt == ".flac")
        return "flac";
    if (lowerExt == ".m4a")
        return "m4a";
    if (lowerExt == ".aac")
        return "aac";
    if (lowerExt == ".wav")
        return "wav";
    return lowerExt.empty() ? "unknown" : lowerExt.substr(1);
}

std::optional<std::string> firstStringProperty(const TagLib::PropertyMap& props, const char* key) {
    auto it = props.find(key);
    if (it == props.end() || it->second.isEmpty()) {
        return std::nullopt;
    }
    std::string value = it->second.front().to8Bit(true);
    return value.empty() ? std::nullopt : std::optional<std::string>(std::move(value));
}

std::optional<int> firstIntProperty(const TagLib::PropertyMap& props, const char* key) {
    auto it = props.find(key);
    if (it == props.end() || it->second.isEmpty()) {
        return std::nullopt;
    }
    bool ok = false;
    // DISCNUMBER is sometimes "1/2" (disc/total) -- toInt() parses the leading
    // integer and reports failure via `ok` only if there's no leading digit at
    // all, so "1/2" correctly yields 1.
    int value = it->second.front().toInt(&ok);
    return ok ? std::optional<int>(value) : std::nullopt;
}

class TagLibMetadataExtractor : public MetadataExtractor {
public:
    std::optional<RawTrackMetadata> extract(const std::string& absolutePath) override {
        TagLib::FileRef file(absolutePath.c_str());
        if (file.isNull() || file.tag() == nullptr) {
            return std::nullopt;
        }

        const TagLib::Tag* tag = file.tag();
        RawTrackMetadata meta;

        meta.title = tag->title().to8Bit(true);
        if (meta.title.empty()) {
            // Fall back to the filename (without extension) rather than leaving
            // an empty title -- an untagged file is still a playable track.
            meta.title = std::filesystem::path(absolutePath).stem().string();
        }
        if (!tag->artist().isEmpty()) {
            meta.artist = tag->artist().to8Bit(true);
        }
        if (!tag->album().isEmpty()) {
            meta.album = tag->album().to8Bit(true);
        }
        if (!tag->genre().isEmpty()) {
            meta.genre = tag->genre().to8Bit(true);
        }
        if (tag->year() != 0) {
            meta.year = static_cast<int>(tag->year());
        }
        if (tag->track() != 0) {
            meta.trackNumber = static_cast<int>(tag->track());
        }

        // ALBUMARTIST and DISCNUMBER aren't part of TagLib's minimal common
        // Tag interface (title/artist/album/genre/year/track only) -- they're
        // read via the generic property map, which every TagLib format
        // backend populates from its native tag format (ID3v2 TPE2/TPOS,
        // Vorbis comments, MP4 atoms, ...).
        if (file.file() != nullptr) {
            const TagLib::PropertyMap props = file.file()->properties();
            meta.albumArtist = firstStringProperty(props, "ALBUMARTIST");
            meta.discNumber = firstIntProperty(props, "DISCNUMBER");
        }

        if (const TagLib::AudioProperties* audio = file.audioProperties()) {
            meta.durationMs = static_cast<std::int64_t>(audio->lengthInMilliseconds());
            if (audio->bitrate() > 0) {
                meta.bitrateKbps = audio->bitrate();
            }
        }

        meta.codec = codecForExtension(toLowerExtension(absolutePath));

        // Embedded artwork extraction is format-specific in TagLib (ID3v2 APIC
        // frames, FLAC::Picture, MP4 covr atoms all have distinct APIs) and is
        // out of scope for the MVP scanner, which only needs to know a file
        // *could* have artwork for later per-format extraction behind
        // GET /api/v1/tracks/{id}/artwork. Reporting false unconditionally
        // here is a documented simplification, not a silent gap.
        meta.hasArtwork = false;

        return meta;
    }
};

} // namespace

std::unique_ptr<MetadataExtractor> makeDefaultMetadataExtractor() {
    return std::make_unique<TagLibMetadataExtractor>();
}

} // namespace musicbox::library
