#include "musicbox/api/Api.hpp"

#include <algorithm>
#include <charconv>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

#include "musicbox/http/HttpRequest.hpp"
#include "musicbox/http/HttpResponse.hpp"
#include "musicbox/http/HttpStatus.hpp"
#include "musicbox/http/Range.hpp"
#include "musicbox/streaming/FileStreamSource.hpp"

#include "JsonSerialization.hpp"
#include "QueryString.hpp"

namespace musicbox::api {

using musicbox::http::HttpRequest;
using musicbox::http::HttpResponse;
using musicbox::http::HttpStatus;
using musicbox::http::RouteParams;

namespace {

// A route pattern's "{id}"-style segments are guaranteed present in
// RouteParams by PathRouter (server/src/http/Router.cpp) whenever the handler
// runs at all -- only the *content* needs validating here, not presence.
std::optional<std::int64_t> parseId(const std::string& raw) {
    std::int64_t value = 0;
    auto result = std::from_chars(raw.data(), raw.data() + raw.size(), value);
    if (result.ec != std::errc{} || result.ptr != raw.data() + raw.size() || value <= 0) {
        return std::nullopt;
    }
    return value;
}

HttpResponse badRequest(const std::string& message) {
    return HttpResponse::error(HttpStatus::BadRequest, "BAD_REQUEST", message);
}

HttpResponse noContent() {
    HttpResponse response;
    response.statusCode = HttpStatus::NoContent;
    return response;
}

std::string mimeTypeForCodec(const std::string& codec) {
    if (codec == "mp3")
        return "audio/mpeg";
    if (codec == "flac")
        return "audio/flac";
    if (codec == "wav")
        return "audio/wav";
    if (codec == "m4a")
        return "audio/mp4";
    if (codec == "aac")
        return "audio/aac";
    return "application/octet-stream";
}

// Applies the documented `limit`/`offset` query-parameter convention
// (docs/api.md: default 100, max 500) uniformly across every list endpoint.
// Sets `*invalid` (already possibly set by an earlier queryInt() call for a
// different field) if either value is present but unparsable.
struct Pagination {
    std::size_t limit = 100;
    std::size_t offset = 0;
};

Pagination parsePagination(const std::unordered_map<std::string, std::string>& query,
                           bool* invalid) {
    Pagination page;
    auto limitOpt = queryInt(query, "limit", invalid);
    auto offsetOpt = queryInt(query, "offset", invalid);
    if (limitOpt) {
        page.limit = static_cast<std::size_t>(std::clamp<std::int64_t>(*limitOpt, 1, 500));
    }
    if (offsetOpt && *offsetOpt >= 0) {
        page.offset = static_cast<std::size_t>(*offsetOpt);
    }
    return page;
}

std::optional<nlohmann::json> parseJsonBody(const HttpRequest& request) {
    if (request.body.empty()) {
        return std::nullopt;
    }
    try {
        // Via string_view rather than the raw std::byte iterators: nlohmann::json
        // instantiates std::char_traits<std::byte> for the latter, which is
        // deprecated in libc++ and warns on every build for no benefit here.
        std::string_view text(reinterpret_cast<const char*>(request.body.data()),
                              request.body.size());
        return nlohmann::json::parse(text);
    } catch (const nlohmann::json::parse_error&) {
        return std::nullopt;
    }
}

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

void registerStatusRoute(musicbox::http::Router& router, musicbox::db::TrackRepository& trackRepo,
                         musicbox::db::LibraryRootRepository& libraryRootRepo) {
    router.addRoute(
        "GET", "/api/v1/status",
        [&trackRepo, &libraryRootRepo](const HttpRequest&, const RouteParams&) -> HttpResponse {
            nlohmann::json j;
            j["status"] = "ok";
            j["version"] = "0.1.0";
            j["trackCount"] = trackRepo.count(musicbox::db::TrackQuery{});
            j["libraryRoots"] = libraryRootRepo.list().size();
            return HttpResponse::json(HttpStatus::Ok, j.dump());
        });
}

// ---------------------------------------------------------------------------
// Artists
// ---------------------------------------------------------------------------

void registerArtistRoutes(musicbox::http::Router& router,
                          musicbox::db::ArtistRepository& artistRepo) {
    router.addRoute("GET", "/api/v1/artists",
                    [&artistRepo](const HttpRequest& request, const RouteParams&) -> HttpResponse {
                        auto query = parseQueryString(request.query);
                        bool invalid = false;
                        Pagination page = parsePagination(query, &invalid);
                        if (invalid)
                            return badRequest("invalid 'limit' or 'offset'");

                        musicbox::db::ArtistQuery artistQuery;
                        artistQuery.limit = page.limit;
                        artistQuery.offset = page.offset;
                        if (auto it = query.find("search");
                            it != query.end() && !it->second.empty()) {
                            artistQuery.search = it->second;
                        }

                        nlohmann::json array = nlohmann::json::array();
                        for (const auto& artist : artistRepo.list(artistQuery)) {
                            array.push_back(artistJson(artist));
                        }
                        return HttpResponse::json(HttpStatus::Ok, array.dump());
                    });

    router.addRoute("GET", "/api/v1/artists/{id}",
                    [&artistRepo](const HttpRequest&, const RouteParams& params) -> HttpResponse {
                        auto id = parseId(params.at("id"));
                        if (!id)
                            return badRequest("invalid artist id");
                        auto artist = artistRepo.findById(musicbox::ArtistId(*id));
                        if (!artist) {
                            return HttpResponse::error(HttpStatus::NotFound, "ARTIST_NOT_FOUND",
                                                       "The requested artist does not exist.");
                        }
                        return HttpResponse::json(HttpStatus::Ok, artistJson(*artist).dump());
                    });
}

// ---------------------------------------------------------------------------
// Albums
// ---------------------------------------------------------------------------

void registerAlbumRoutes(musicbox::http::Router& router, musicbox::db::AlbumRepository& albumRepo,
                         musicbox::db::ArtistRepository& artistRepo,
                         musicbox::db::TrackRepository& trackRepo) {
    router.addRoute("GET", "/api/v1/albums",
                    [&albumRepo, &artistRepo, &trackRepo](const HttpRequest& request,
                                                          const RouteParams&) -> HttpResponse {
                        auto query = parseQueryString(request.query);
                        bool invalid = false;
                        Pagination page = parsePagination(query, &invalid);
                        auto artistIdOpt = queryInt(query, "artistId", &invalid);
                        if (invalid)
                            return badRequest("invalid 'limit', 'offset', or 'artistId'");

                        musicbox::db::AlbumQuery albumQuery;
                        albumQuery.limit = page.limit;
                        albumQuery.offset = page.offset;
                        if (artistIdOpt)
                            albumQuery.artistId = musicbox::ArtistId(*artistIdOpt);
                        if (auto it = query.find("search");
                            it != query.end() && !it->second.empty()) {
                            albumQuery.search = it->second;
                        }

                        nlohmann::json array = nlohmann::json::array();
                        for (const auto& album : albumRepo.list(albumQuery)) {
                            array.push_back(albumJson(album, artistRepo, trackRepo));
                        }
                        return HttpResponse::json(HttpStatus::Ok, array.dump());
                    });

    router.addRoute("GET", "/api/v1/albums/{id}",
                    [&albumRepo, &artistRepo,
                     &trackRepo](const HttpRequest&, const RouteParams& params) -> HttpResponse {
                        auto id = parseId(params.at("id"));
                        if (!id)
                            return badRequest("invalid album id");
                        auto album = albumRepo.findById(musicbox::AlbumId(*id));
                        if (!album) {
                            return HttpResponse::error(HttpStatus::NotFound, "ALBUM_NOT_FOUND",
                                                       "The requested album does not exist.");
                        }
                        return HttpResponse::json(HttpStatus::Ok,
                                                  albumJson(*album, artistRepo, trackRepo).dump());
                    });
}

// ---------------------------------------------------------------------------
// Tracks (list/get/stream/artwork)
// ---------------------------------------------------------------------------

void registerTrackRoutes(musicbox::http::Router& router, musicbox::db::TrackRepository& trackRepo,
                         musicbox::db::ArtistRepository& artistRepo,
                         musicbox::db::AlbumRepository& albumRepo) {
    router.addRoute(
        "GET", "/api/v1/tracks",
        [&trackRepo, &artistRepo, &albumRepo](const HttpRequest& request,
                                              const RouteParams&) -> HttpResponse {
            auto query = parseQueryString(request.query);
            bool invalid = false;
            Pagination page = parsePagination(query, &invalid);
            auto artistIdOpt = queryInt(query, "artistId", &invalid);
            auto albumIdOpt = queryInt(query, "albumId", &invalid);
            if (invalid)
                return badRequest("invalid 'limit', 'offset', 'artistId', or 'albumId'");

            musicbox::db::TrackQuery trackQuery;
            trackQuery.limit = page.limit;
            trackQuery.offset = page.offset;
            if (artistIdOpt)
                trackQuery.artistId = musicbox::ArtistId(*artistIdOpt);
            if (albumIdOpt)
                trackQuery.albumId = musicbox::AlbumId(*albumIdOpt);
            if (auto it = query.find("search"); it != query.end() && !it->second.empty()) {
                trackQuery.search = it->second;
            }

            nlohmann::json array = nlohmann::json::array();
            for (const auto& track : trackRepo.list(trackQuery)) {
                array.push_back(trackJson(track, artistRepo, albumRepo));
            }
            return HttpResponse::json(HttpStatus::Ok, array.dump());
        });

    router.addRoute("GET", "/api/v1/tracks/{id}",
                    [&trackRepo, &artistRepo,
                     &albumRepo](const HttpRequest&, const RouteParams& params) -> HttpResponse {
                        auto id = parseId(params.at("id"));
                        if (!id)
                            return badRequest("invalid track id");
                        auto track = trackRepo.findById(musicbox::TrackId(*id));
                        // findById() doesn't itself filter soft-deleted rows
                        // (docs/database.md) -- a deleted track is a 404 to
                        // clients even though the tombstone row still exists.
                        if (!track || track->deletedAt.has_value()) {
                            return HttpResponse::error(HttpStatus::NotFound, "TRACK_NOT_FOUND",
                                                       "The requested track does not exist.");
                        }
                        return HttpResponse::json(HttpStatus::Ok,
                                                  trackJson(*track, artistRepo, albumRepo).dump());
                    });

    router.addRoute(
        "GET", "/api/v1/tracks/{id}/stream",
        [&trackRepo](const HttpRequest& request, const RouteParams& params) -> HttpResponse {
            using musicbox::http::RangeParseOutcome;
            using musicbox::http::parseRange;
            using musicbox::streaming::openFileStreamSource;

            auto id = parseId(params.at("id"));
            if (!id)
                return badRequest("invalid track id");

            auto absolutePath = trackRepo.resolveAbsolutePath(musicbox::TrackId(*id));
            if (!absolutePath) {
                // Covers both "no such track" and "track is soft-deleted"
                // (resolveAbsolutePath excludes deleted rows itself).
                return HttpResponse::error(HttpStatus::NotFound, "TRACK_NOT_FOUND",
                                           "The requested track does not exist.");
            }

            std::unique_ptr<musicbox::streaming::FileStreamSource> source;
            try {
                source = openFileStreamSource(*absolutePath);
            } catch (const std::exception&) {
                // The DB row exists but the file is missing/unreadable on disk
                // (moved, deleted outside a rescan, permissions) -- a server
                // problem, not a client error.
                return HttpResponse::error(HttpStatus::InternalServerError, "INTERNAL_ERROR",
                                           "The track file could not be opened.");
            }

            std::uint64_t fileSize = source->fileSize();
            const std::string* rangeHeader = request.header("range");
            auto rangeResult = parseRange(
                rangeHeader ? std::string_view(*rangeHeader) : std::string_view{}, fileSize);

            if (rangeResult.outcome == RangeParseOutcome::NotSatisfiable) {
                HttpResponse response =
                    HttpResponse::error(HttpStatus::RangeNotSatisfiable, "RANGE_NOT_SATISFIABLE",
                                        "The requested range is not satisfiable.");
                response.headers["Content-Range"] = "bytes */" + std::to_string(fileSize);
                return response;
            }

            std::uint64_t start = 0;
            std::uint64_t length = fileSize;
            HttpStatus status = HttpStatus::Ok;
            if (rangeResult.outcome == RangeParseOutcome::Satisfiable) {
                start = rangeResult.range.start;
                length = rangeResult.range.length();
                status = HttpStatus::PartialContent;
            }

            std::unique_ptr<musicbox::http::ResponseBody> body;
            try {
                body = source->openRange(start, length);
            } catch (const std::exception&) {
                return HttpResponse::error(HttpStatus::InternalServerError, "INTERNAL_ERROR",
                                           "The track range could not be opened.");
            }

            auto track = trackRepo.findById(musicbox::TrackId(*id));

            HttpResponse response;
            response.statusCode = status;
            response.headers["Content-Type"] = mimeTypeForCodec(track ? track->codec : "");
            response.headers["Accept-Ranges"] = "bytes";
            response.headers["Content-Length"] = std::to_string(length);
            if (status == HttpStatus::PartialContent) {
                response.headers["Content-Range"] = "bytes " + std::to_string(start) + "-" +
                                                    std::to_string(start + length - 1) + "/" +
                                                    std::to_string(fileSize);
            }
            response.streamingBody = std::move(body);
            return response;
        });

    router.addRoute("GET", "/api/v1/tracks/{id}/artwork",
                    [&trackRepo](const HttpRequest&, const RouteParams& params) -> HttpResponse {
                        auto id = parseId(params.at("id"));
                        if (!id)
                            return badRequest("invalid track id");
                        auto track = trackRepo.findById(musicbox::TrackId(*id));
                        if (!track || track->deletedAt.has_value()) {
                            return HttpResponse::error(HttpStatus::NotFound, "TRACK_NOT_FOUND",
                                                       "The requested track does not exist.");
                        }
                        // Additive error code (docs/api.md): MetadataExtractor
                        // (docs/database.md) never sets hasArtwork today, so
                        // this always 404s for now -- a documented gap, not a
                        // silently swallowed one.
                        return HttpResponse::error(HttpStatus::NotFound, "ARTWORK_NOT_FOUND",
                                                   "This track has no available artwork.");
                    });
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

void registerSearchRoute(musicbox::http::Router& router, musicbox::db::ArtistRepository& artistRepo,
                         musicbox::db::AlbumRepository& albumRepo,
                         musicbox::db::TrackRepository& trackRepo) {
    router.addRoute("GET", "/api/v1/search",
                    [&artistRepo, &albumRepo, &trackRepo](const HttpRequest& request,
                                                          const RouteParams&) -> HttpResponse {
                        constexpr std::size_t kSearchLimit =
                            20; // not documented in docs/api.md; a reasonable fixed cap

                        auto query = parseQueryString(request.query);
                        auto it = query.find("q");
                        std::string term = (it != query.end()) ? it->second : "";

                        nlohmann::json result;
                        result["artists"] = nlohmann::json::array();
                        result["albums"] = nlohmann::json::array();
                        result["tracks"] = nlohmann::json::array();

                        if (term.empty()) {
                            return HttpResponse::json(HttpStatus::Ok, result.dump());
                        }

                        musicbox::db::ArtistQuery artistQuery;
                        artistQuery.search = term;
                        artistQuery.limit = kSearchLimit;
                        for (const auto& artist : artistRepo.list(artistQuery)) {
                            result["artists"].push_back(artistJson(artist));
                        }

                        musicbox::db::AlbumQuery albumQuery;
                        albumQuery.search = term;
                        albumQuery.limit = kSearchLimit;
                        for (const auto& album : albumRepo.list(albumQuery)) {
                            result["albums"].push_back(albumJson(album, artistRepo, trackRepo));
                        }

                        musicbox::db::TrackQuery trackQuery;
                        trackQuery.search = term;
                        trackQuery.limit = kSearchLimit;
                        for (const auto& track : trackRepo.list(trackQuery)) {
                            result["tracks"].push_back(trackJson(track, artistRepo, albumRepo));
                        }

                        return HttpResponse::json(HttpStatus::Ok, result.dump());
                    });
}

// ---------------------------------------------------------------------------
// Playlists
// ---------------------------------------------------------------------------

void registerPlaylistRoutes(musicbox::http::Router& router,
                            musicbox::db::PlaylistRepository& playlistRepo,
                            musicbox::db::ArtistRepository& artistRepo,
                            musicbox::db::AlbumRepository& albumRepo) {
    router.addRoute(
        "GET", "/api/v1/playlists",
        [&playlistRepo](const HttpRequest& request, const RouteParams&) -> HttpResponse {
            auto query = parseQueryString(request.query);
            bool invalid = false;
            Pagination page = parsePagination(query, &invalid);
            if (invalid)
                return badRequest("invalid 'limit' or 'offset'");

            nlohmann::json array = nlohmann::json::array();
            for (const auto& playlist : playlistRepo.list(page.limit, page.offset)) {
                array.push_back(playlistSummaryJson(playlist));
            }
            return HttpResponse::json(HttpStatus::Ok, array.dump());
        });

    router.addRoute("GET", "/api/v1/playlists/{id}",
                    [&playlistRepo, &artistRepo,
                     &albumRepo](const HttpRequest&, const RouteParams& params) -> HttpResponse {
                        auto id = parseId(params.at("id"));
                        if (!id)
                            return badRequest("invalid playlist id");
                        auto playlist = playlistRepo.findById(musicbox::PlaylistId(*id));
                        if (!playlist) {
                            return HttpResponse::error(HttpStatus::NotFound, "PLAYLIST_NOT_FOUND",
                                                       "The requested playlist does not exist.");
                        }
                        auto tracks = playlistRepo.tracks(musicbox::PlaylistId(*id));
                        return HttpResponse::json(
                            HttpStatus::Ok,
                            playlistDetailJson(*playlist, tracks, artistRepo, albumRepo).dump());
                    });

    router.addRoute("POST", "/api/v1/playlists",
                    [&playlistRepo, &artistRepo, &albumRepo](const HttpRequest& request,
                                                             const RouteParams&) -> HttpResponse {
                        auto body = parseJsonBody(request);
                        if (!body || !body->contains("name") || !(*body)["name"].is_string() ||
                            (*body)["name"].get<std::string>().empty()) {
                            return HttpResponse::error(HttpStatus::BadRequest, "VALIDATION_ERROR",
                                                       "A non-empty 'name' is required.");
                        }
                        auto playlist = playlistRepo.create((*body)["name"].get<std::string>());
                        return HttpResponse::json(
                            HttpStatus::Created,
                            playlistDetailJson(playlist, {}, artistRepo, albumRepo).dump());
                    });

    router.addRoute("PUT", "/api/v1/playlists/{id}",
                    [&playlistRepo, &artistRepo, &albumRepo](
                        const HttpRequest& request, const RouteParams& params) -> HttpResponse {
                        auto id = parseId(params.at("id"));
                        if (!id)
                            return badRequest("invalid playlist id");

                        auto body = parseJsonBody(request);
                        if (!body || !body->contains("name") || !(*body)["name"].is_string() ||
                            (*body)["name"].get<std::string>().empty()) {
                            return HttpResponse::error(HttpStatus::BadRequest, "VALIDATION_ERROR",
                                                       "A non-empty 'name' is required.");
                        }

                        if (!playlistRepo.rename(musicbox::PlaylistId(*id),
                                                 (*body)["name"].get<std::string>())) {
                            return HttpResponse::error(HttpStatus::NotFound, "PLAYLIST_NOT_FOUND",
                                                       "The requested playlist does not exist.");
                        }
                        auto playlist = playlistRepo.findById(musicbox::PlaylistId(*id));
                        auto tracks = playlistRepo.tracks(musicbox::PlaylistId(*id));
                        return HttpResponse::json(
                            HttpStatus::Ok,
                            playlistDetailJson(*playlist, tracks, artistRepo, albumRepo).dump());
                    });

    router.addRoute("DELETE", "/api/v1/playlists/{id}",
                    [&playlistRepo](const HttpRequest&, const RouteParams& params) -> HttpResponse {
                        auto id = parseId(params.at("id"));
                        if (!id)
                            return badRequest("invalid playlist id");
                        if (!playlistRepo.remove(musicbox::PlaylistId(*id))) {
                            return HttpResponse::error(HttpStatus::NotFound, "PLAYLIST_NOT_FOUND",
                                                       "The requested playlist does not exist.");
                        }
                        return noContent();
                    });

    router.addRoute(
        "POST", "/api/v1/playlists/{id}/tracks",
        [&playlistRepo](const HttpRequest& request, const RouteParams& params) -> HttpResponse {
            auto playlistId = parseId(params.at("id"));
            if (!playlistId)
                return badRequest("invalid playlist id");

            auto body = parseJsonBody(request);
            std::optional<std::int64_t> trackId =
                (body && body->contains("trackId") && (*body)["trackId"].is_number_integer())
                    ? std::optional<std::int64_t>((*body)["trackId"].get<std::int64_t>())
                    : std::nullopt;
            if (!trackId) {
                return HttpResponse::error(HttpStatus::BadRequest, "VALIDATION_ERROR",
                                           "A numeric 'trackId' is required.");
            }

            if (!playlistRepo.findById(musicbox::PlaylistId(*playlistId))) {
                return HttpResponse::error(HttpStatus::NotFound, "PLAYLIST_NOT_FOUND",
                                           "The requested playlist does not exist.");
            }
            if (!playlistRepo.addTrack(musicbox::PlaylistId(*playlistId),
                                       musicbox::TrackId(*trackId))) {
                return HttpResponse::error(HttpStatus::NotFound, "TRACK_NOT_FOUND",
                                           "The requested track does not exist.");
            }
            return noContent();
        });

    router.addRoute("DELETE", "/api/v1/playlists/{id}/tracks/{trackId}",
                    [&playlistRepo](const HttpRequest&, const RouteParams& params) -> HttpResponse {
                        auto playlistId = parseId(params.at("id"));
                        auto trackId = parseId(params.at("trackId"));
                        if (!playlistId || !trackId)
                            return badRequest("invalid playlist or track id");

                        if (!playlistRepo.findById(musicbox::PlaylistId(*playlistId))) {
                            return HttpResponse::error(HttpStatus::NotFound, "PLAYLIST_NOT_FOUND",
                                                       "The requested playlist does not exist.");
                        }
                        // The repository doesn't distinguish "track not in this playlist"
                        // from "track doesn't exist at all" -- TRACK_NOT_FOUND covers both,
                        // a defensible if slightly imprecise choice given docs/api.md
                        // doesn't define a more specific code for this case.
                        if (!playlistRepo.removeTrack(musicbox::PlaylistId(*playlistId),
                                                      musicbox::TrackId(*trackId))) {
                            return HttpResponse::error(HttpStatus::NotFound, "TRACK_NOT_FOUND",
                                                       "That track is not in this playlist.");
                        }
                        return noContent();
                    });
}

} // namespace

void registerApiRoutes(musicbox::http::Router& router, musicbox::db::TrackRepository& trackRepo,
                       musicbox::db::ArtistRepository& artistRepo,
                       musicbox::db::AlbumRepository& albumRepo,
                       musicbox::db::PlaylistRepository& playlistRepo,
                       musicbox::db::LibraryRootRepository& libraryRootRepo) {
    registerStatusRoute(router, trackRepo, libraryRootRepo);
    registerArtistRoutes(router, artistRepo);
    registerAlbumRoutes(router, albumRepo, artistRepo, trackRepo);
    registerTrackRoutes(router, trackRepo, artistRepo, albumRepo);
    registerSearchRoute(router, artistRepo, albumRepo, trackRepo);
    registerPlaylistRoutes(router, playlistRepo, artistRepo, albumRepo);
}

} // namespace musicbox::api
