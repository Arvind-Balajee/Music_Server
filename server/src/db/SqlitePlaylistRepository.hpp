#pragma once

#include <string>

#include "musicbox/db/PlaylistRepository.hpp"

#include "SqliteConnection.hpp"

namespace musicbox::db {

class SqlitePlaylistRepository final : public PlaylistRepository {
public:
    explicit SqlitePlaylistRepository(const std::string& databasePath);

    [[nodiscard]] std::optional<musicbox::Playlist> findById(musicbox::PlaylistId id) override;
    [[nodiscard]] std::vector<musicbox::Playlist> list(std::size_t limit, std::size_t offset) override;

    [[nodiscard]] musicbox::Playlist create(std::string name) override;
    [[nodiscard]] bool rename(musicbox::PlaylistId id, std::string newName) override;
    bool remove(musicbox::PlaylistId id) override;

    [[nodiscard]] std::vector<musicbox::Track> tracks(musicbox::PlaylistId id) override;

    bool addTrack(musicbox::PlaylistId id, musicbox::TrackId trackId) override;
    bool removeTrack(musicbox::PlaylistId id, musicbox::TrackId trackId) override;

private:
    SqliteConnection connection_;
};

} // namespace musicbox::db
