#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "musicbox/model/Ids.hpp"
#include "musicbox/model/Playlist.hpp"
#include "musicbox/model/Track.hpp"

namespace musicbox::db {

class PlaylistRepository {
public:
    virtual ~PlaylistRepository() = default;

    [[nodiscard]] virtual std::optional<musicbox::Playlist> findById(musicbox::PlaylistId id) = 0;
    [[nodiscard]] virtual std::vector<musicbox::Playlist> list(std::size_t limit,
                                                               std::size_t offset) = 0;

    [[nodiscard]] virtual musicbox::Playlist create(std::string name) = 0;
    [[nodiscard]] virtual bool rename(musicbox::PlaylistId id, std::string newName) = 0;
    virtual bool remove(musicbox::PlaylistId id) = 0;

    // Ordered tracks currently in the playlist.
    [[nodiscard]] virtual std::vector<musicbox::Track> tracks(musicbox::PlaylistId id) = 0;

    // Appends to the end of the playlist. Returns false if the playlist or track
    // doesn't exist.
    virtual bool addTrack(musicbox::PlaylistId id, musicbox::TrackId trackId) = 0;
    virtual bool removeTrack(musicbox::PlaylistId id, musicbox::TrackId trackId) = 0;
};

// See TrackRepository.hpp's makeSqliteTrackRepository for the databasePath
// contract.
[[nodiscard]] std::unique_ptr<PlaylistRepository>
makeSqlitePlaylistRepository(const std::string& databasePath);

} // namespace musicbox::db
