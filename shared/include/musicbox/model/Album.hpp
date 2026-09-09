#pragma once

#include <optional>
#include <string>

#include "musicbox/model/Ids.hpp"

namespace musicbox {

struct Album {
    AlbumId id;
    std::string title;
    std::optional<ArtistId> albumArtistId;
    std::optional<int> year;
    bool hasArtwork = false;
};

} // namespace musicbox
