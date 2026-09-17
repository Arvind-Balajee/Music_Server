#pragma once

#include <nlohmann/json.hpp>
#include <vector>

#include "musicbox/db/AlbumRepository.hpp"
#include "musicbox/db/ArtistRepository.hpp"
#include "musicbox/db/TrackRepository.hpp"
#include "musicbox/model/Album.hpp"
#include "musicbox/model/Artist.hpp"
#include "musicbox/model/Playlist.hpp"
#include "musicbox/model/Track.hpp"

// JSON shapes here are matched field-for-field against the iOS client's
// Codable models (client-ios/MusicBox/Models/*.swift) and docs/api.md --
// changing a field name/shape here is a client-breaking change, not just an
// internal refactor.
namespace musicbox::api {

nlohmann::json artistJson(const musicbox::Artist& artist);

// The compact {id, name} / {id, title} refs embedded inside Track/Album
// responses (docs/api.md's GET /tracks/{id} example).
nlohmann::json artistRefJson(musicbox::ArtistId id, const std::string& name);
nlohmann::json albumRefJson(musicbox::AlbumId id, const std::string& title);

// Resolves the track's artistId/albumId into embedded refs via the given
// repositories. A track with no artistId (untagged file) still needs *some*
// artist ref, since the client's Track.artist is non-optional -- falls back
// to a synthetic "Unknown Artist" (id 0, which Id<Tag>::valid() reports as
// invalid -- never a real row -- documented at the call site).
nlohmann::json trackJson(const musicbox::Track& track, musicbox::db::ArtistRepository& artistRepo,
                         musicbox::db::AlbumRepository& albumRepo);

// `trackCount` is computed via trackRepo.count() filtered to this album --
// one extra query per album, acceptable at this project's target library
// sizes (see docs/database.md).
nlohmann::json albumJson(const musicbox::Album& album, musicbox::db::ArtistRepository& artistRepo,
                         musicbox::db::TrackRepository& trackRepo);

nlohmann::json playlistSummaryJson(const musicbox::Playlist& playlist);

// Embeds the full ordered track list (already resolved via
// PlaylistRepository::tracks()) as full trackJson() entries.
nlohmann::json playlistDetailJson(const musicbox::Playlist& playlist,
                                  const std::vector<musicbox::Track>& tracks,
                                  musicbox::db::ArtistRepository& artistRepo,
                                  musicbox::db::AlbumRepository& albumRepo);

} // namespace musicbox::api
