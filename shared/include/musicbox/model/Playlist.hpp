#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "musicbox/model/Ids.hpp"

namespace musicbox {

struct Playlist {
    PlaylistId id;
    std::string name;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point updatedAt;
};

// Row in the playlist_tracks join table. `position` is a dense 0-based ordering
// within the playlist, maintained by the repository (see docs/database.md).
struct PlaylistTrack {
    PlaylistId playlistId;
    TrackId trackId;
    std::uint32_t position = 0;
};

} // namespace musicbox
