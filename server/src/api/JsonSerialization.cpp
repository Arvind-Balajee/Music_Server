#include "JsonSerialization.hpp"

#include <ctime>

namespace musicbox::api {

namespace {

std::string toIso8601(std::chrono::system_clock::time_point tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

} // namespace

nlohmann::json artistJson(const musicbox::Artist& artist) {
    nlohmann::json j;
    j["id"] = artist.id.value();
    j["name"] = artist.name;
    if (artist.sortName) {
        j["sortName"] = *artist.sortName;
    }
    return j;
}

nlohmann::json artistRefJson(musicbox::ArtistId id, const std::string& name) {
    return {{"id", id.value()}, {"name", name}};
}

nlohmann::json albumRefJson(musicbox::AlbumId id, const std::string& title) {
    return {{"id", id.value()}, {"title", title}};
}

nlohmann::json trackJson(const musicbox::Track& track, musicbox::db::ArtistRepository& artistRepo,
                         musicbox::db::AlbumRepository& albumRepo) {
    nlohmann::json j;
    j["id"] = track.id.value();
    j["title"] = track.title;

    if (track.artistId) {
        auto artist = artistRepo.findById(*track.artistId);
        j["artist"] = artistRefJson(*track.artistId, artist ? artist->name : "Unknown Artist");
    } else {
        // Track.artist is non-optional on the client -- an untagged file
        // still needs some ref. id 0 is never a real row (Id<Tag>::valid()
        // requires > 0), so a client that ever tried to use it as a real
        // artist id would harmlessly get ARTIST_NOT_FOUND back.
        j["artist"] = artistRefJson(musicbox::ArtistId(0), "Unknown Artist");
    }

    if (track.albumId) {
        auto album = albumRepo.findById(*track.albumId);
        if (album) {
            j["album"] = albumRefJson(*track.albumId, album->title);
        }
    }

    if (track.trackNumber)
        j["trackNumber"] = *track.trackNumber;
    if (track.discNumber)
        j["discNumber"] = *track.discNumber;
    j["durationMs"] = track.duration.count();
    j["codec"] = track.codec;
    if (track.bitrateKbps)
        j["bitrateKbps"] = *track.bitrateKbps;
    j["fileSize"] = track.fileSizeBytes;
    if (track.genre)
        j["genre"] = *track.genre;
    j["hasArtwork"] = track.hasArtwork;
    j["streamUrl"] = "/api/v1/tracks/" + std::to_string(track.id.value()) + "/stream";
    return j;
}

nlohmann::json albumJson(const musicbox::Album& album, musicbox::db::ArtistRepository& artistRepo,
                         musicbox::db::TrackRepository& trackRepo) {
    nlohmann::json j;
    j["id"] = album.id.value();
    j["title"] = album.title;

    if (album.albumArtistId) {
        auto artist = artistRepo.findById(*album.albumArtistId);
        if (artist) {
            j["artist"] = artistRefJson(*album.albumArtistId, artist->name);
        }
    }

    if (album.year)
        j["year"] = *album.year;
    j["hasArtwork"] = album.hasArtwork;

    musicbox::db::TrackQuery query;
    query.albumId = album.id;
    query.limit = 1; // count() ignores limit/offset, but keep the query well-formed
    j["trackCount"] = trackRepo.count(query);

    return j;
}

nlohmann::json playlistSummaryJson(const musicbox::Playlist& playlist) {
    nlohmann::json j;
    j["id"] = playlist.id.value();
    j["name"] = playlist.name;
    j["createdAt"] = toIso8601(playlist.createdAt);
    j["updatedAt"] = toIso8601(playlist.updatedAt);
    return j;
}

nlohmann::json playlistDetailJson(const musicbox::Playlist& playlist,
                                  const std::vector<musicbox::Track>& tracks,
                                  musicbox::db::ArtistRepository& artistRepo,
                                  musicbox::db::AlbumRepository& albumRepo) {
    nlohmann::json j;
    j["id"] = playlist.id.value();
    j["name"] = playlist.name;
    j["createdAt"] = toIso8601(playlist.createdAt);
    j["updatedAt"] = toIso8601(playlist.updatedAt);

    nlohmann::json trackArray = nlohmann::json::array();
    for (const auto& track : tracks) {
        trackArray.push_back(trackJson(track, artistRepo, albumRepo));
    }
    j["tracks"] = std::move(trackArray);

    return j;
}

} // namespace musicbox::api
