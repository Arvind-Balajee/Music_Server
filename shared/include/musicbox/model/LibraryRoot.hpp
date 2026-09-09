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
};

} // namespace musicbox
