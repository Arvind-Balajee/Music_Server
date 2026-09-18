#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "musicbox/model/Ids.hpp"

namespace musicbox {

// A configured top-level directory that the LibraryScanner indexes (e.g.
// "/media/music"). All Track::relativePath values are relative to their
// LibraryRoot's absolutePath.
struct LibraryRoot {
    LibraryRootId id;
    std::string absolutePath;
    std::chrono::system_clock::time_point addedAt;
    std::optional<std::chrono::system_clock::time_point> lastScanAt;

    // Set when the path is dropped from [library].paths: the root stops being
    // served (and its tracks are soft-deleted) but is kept so that re-adding
    // the same path revives it. Absent for roots currently in use.
    std::optional<std::chrono::system_clock::time_point> retiredAt;
};

} // namespace musicbox
