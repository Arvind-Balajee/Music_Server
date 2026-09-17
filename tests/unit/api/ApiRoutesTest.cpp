#include "musicbox/api/Api.hpp"

#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

#include "musicbox/db/AlbumRepository.hpp"
#include "musicbox/db/ArtistRepository.hpp"
#include "musicbox/db/LibraryRootRepository.hpp"
#include "musicbox/db/PlaylistRepository.hpp"
#include "musicbox/db/TrackRepository.hpp"
#include "musicbox/http/HttpRequest.hpp"
#include "musicbox/http/Router.hpp"

using namespace musicbox;
using namespace musicbox::http;

namespace {

std::string uniqueDbUri() {
    static int counter = 0;
    return "file:musicbox_api_test_" + std::to_string(counter++) + "?mode=memory&cache=shared";
}

std::string bodyText(const HttpResponse& response) {
    return std::string(reinterpret_cast<const char*>(response.body.data()), response.body.size());
}

nlohmann::json bodyJson(const HttpResponse& response) {
    return nlohmann::json::parse(bodyText(response));
}

HttpRequest makeRequest(std::string method, std::string path, std::string query = "") {
    HttpRequest request;
    request.method = std::move(method);
    request.path = std::move(path);
    request.query = std::move(query);
    request.version = "HTTP/1.1";
    return request;
}

HttpRequest makeJsonRequest(std::string method, std::string path, const nlohmann::json& body) {
    HttpRequest request = makeRequest(std::move(method), std::move(path));
    std::string dumped = body.dump();
    request.body.resize(dumped.size());
    std::memcpy(request.body.data(), dumped.data(), dumped.size());
    request.headers["content-type"] = "application/json";
    return request;
}

// Full set of repositories + a router wired via registerApiRoutes(), backed
// by a fresh in-memory database per test case.
struct ApiFixture {
    std::string dbUri = uniqueDbUri();
    std::unique_ptr<db::TrackRepository> trackRepo = db::makeSqliteTrackRepository(dbUri);
    std::unique_ptr<db::ArtistRepository> artistRepo = db::makeSqliteArtistRepository(dbUri);
    std::unique_ptr<db::AlbumRepository> albumRepo = db::makeSqliteAlbumRepository(dbUri);
    std::unique_ptr<db::PlaylistRepository> playlistRepo = db::makeSqlitePlaylistRepository(dbUri);
    std::unique_ptr<db::LibraryRootRepository> libraryRootRepo =
        db::makeSqliteLibraryRootRepository(dbUri);
    std::unique_ptr<Router> router = createRouter();

    ApiFixture() {
        api::registerApiRoutes(*router, *trackRepo, *artistRepo, *albumRepo, *playlistRepo,
                               *libraryRootRepo);
    }

    Track addTrack(LibraryRootId rootId, const std::string& relativePath, const std::string& title,
                   const std::string& artist, const std::string& album, int trackNumber = 1) {
        db::TrackUpsert data;
        data.libraryRootId = rootId;
        data.relativePath = relativePath;
        data.title = title;
        data.artistName = artist;
        data.albumTitle = album;
        data.trackNumber = trackNumber;
        data.duration = std::chrono::milliseconds(200000);
        data.codec = "flac";
        data.fileSizeBytes = 12345;
        data.contentHash = "hash-" + relativePath;
        data.modifiedAt = std::chrono::system_clock::now();
        return trackRepo->upsert(data);
    }
};

} // namespace

TEST_CASE("GET /api/v1/status reports track and library root counts", "[api][status]") {
    ApiFixture fx;
    const auto root = fx.libraryRootRepo->upsert("/media/music");
    fx.addTrack(root.id, "a.flac", "A", "Artist", "Album");

    auto response = fx.router->dispatch(makeRequest("GET", "/api/v1/status"));
    REQUIRE(response.statusCode == HttpStatus::Ok);
    auto j = bodyJson(response);
    CHECK(j["status"] == "ok");
    CHECK(j["trackCount"] == 1);
    CHECK(j["libraryRoots"] == 1);
    CHECK(j.contains("version"));
}

TEST_CASE("GET /api/v1/artists lists artists and GET .../{id} matches docs/api.md shape",
          "[api][artists]") {
    ApiFixture fx;
    const auto root = fx.libraryRootRepo->upsert("/media/music");
    const auto track =
        fx.addTrack(root.id, "a.flac", "Instant Crush", "Daft Punk", "Random Access Memories");

    auto listResponse = fx.router->dispatch(makeRequest("GET", "/api/v1/artists"));
    REQUIRE(listResponse.statusCode == HttpStatus::Ok);
    auto list = bodyJson(listResponse);
    REQUIRE(list.size() == 1);
    CHECK(list[0]["name"] == "Daft Punk");

    REQUIRE(track.artistId.has_value());
    auto getResponse = fx.router->dispatch(
        makeRequest("GET", "/api/v1/artists/" + std::to_string(track.artistId->value())));
    REQUIRE(getResponse.statusCode == HttpStatus::Ok);
    auto artist = bodyJson(getResponse);
    CHECK(artist["id"] == track.artistId->value());
    CHECK(artist["name"] == "Daft Punk");
}

TEST_CASE("GET /api/v1/artists/{id} returns 404 ARTIST_NOT_FOUND for an unknown id",
          "[api][artists]") {
    ApiFixture fx;
    auto response = fx.router->dispatch(makeRequest("GET", "/api/v1/artists/999999"));
    REQUIRE(response.statusCode == HttpStatus::NotFound);
    auto j = bodyJson(response);
    CHECK(j["error"]["code"] == "ARTIST_NOT_FOUND");
}

TEST_CASE("GET /api/v1/artists/{id} rejects a non-numeric id with 400", "[api][artists]") {
    ApiFixture fx;
    auto response = fx.router->dispatch(makeRequest("GET", "/api/v1/artists/not-a-number"));
    CHECK(response.statusCode == HttpStatus::BadRequest);
}

TEST_CASE("GET /api/v1/albums embeds the artist ref and trackCount", "[api][albums]") {
    ApiFixture fx;
    const auto root = fx.libraryRootRepo->upsert("/media/music");
    fx.addTrack(root.id, "a.flac", "Track A", "Daft Punk", "Random Access Memories", 1);
    fx.addTrack(root.id, "b.flac", "Track B", "Daft Punk", "Random Access Memories", 2);

    auto response = fx.router->dispatch(makeRequest("GET", "/api/v1/albums"));
    REQUIRE(response.statusCode == HttpStatus::Ok);
    auto albums = bodyJson(response);
    REQUIRE(albums.size() == 1);
    CHECK(albums[0]["title"] == "Random Access Memories");
    CHECK(albums[0]["artist"]["name"] == "Daft Punk");
    CHECK(albums[0]["trackCount"] == 2);
    CHECK(albums[0]["hasArtwork"] == false);
}

TEST_CASE("GET /api/v1/tracks/{id} matches the docs/api.md example shape", "[api][tracks]") {
    ApiFixture fx;
    const auto root = fx.libraryRootRepo->upsert("/media/music");
    const auto track = fx.addTrack(root.id, "song.flac", "Instant Crush", "Daft Punk",
                                   "Random Access Memories", 5);

    auto response = fx.router->dispatch(
        makeRequest("GET", "/api/v1/tracks/" + std::to_string(track.id.value())));
    REQUIRE(response.statusCode == HttpStatus::Ok);
    auto j = bodyJson(response);

    CHECK(j["id"] == track.id.value());
    CHECK(j["title"] == "Instant Crush");
    CHECK(j["artist"]["name"] == "Daft Punk");
    CHECK(j["album"]["title"] == "Random Access Memories");
    CHECK(j["trackNumber"] == 5);
    CHECK(j["durationMs"] == 200000);
    CHECK(j["codec"] == "flac");
    CHECK(j["fileSize"] == 12345);
    CHECK(j["streamUrl"] == "/api/v1/tracks/" + std::to_string(track.id.value()) + "/stream");
}

TEST_CASE("GET /api/v1/tracks/{id} falls back to 'Unknown Artist' for an untagged track",
          "[api][tracks]") {
    ApiFixture fx;
    const auto root = fx.libraryRootRepo->upsert("/media/music");

    db::TrackUpsert data;
    data.libraryRootId = root.id;
    data.relativePath = "untagged.flac";
    data.title = "untagged";
    data.codec = "flac";
    data.modifiedAt = std::chrono::system_clock::now();
    const auto track = fx.trackRepo->upsert(data);

    auto response = fx.router->dispatch(
        makeRequest("GET", "/api/v1/tracks/" + std::to_string(track.id.value())));
    REQUIRE(response.statusCode == HttpStatus::Ok);
    auto j = bodyJson(response);
    CHECK(j["artist"]["name"] == "Unknown Artist");
    CHECK_FALSE(j.contains("album")); // no albumId -> omitted entirely (optional on the client)
}

TEST_CASE("GET /api/v1/tracks/{id} 404s for a soft-deleted track", "[api][tracks]") {
    ApiFixture fx;
    const auto root = fx.libraryRootRepo->upsert("/media/music");
    const auto track = fx.addTrack(root.id, "a.flac", "A", "Artist", "Album");
    REQUIRE(fx.trackRepo->softDelete(track.id, std::chrono::system_clock::now()));

    auto response = fx.router->dispatch(
        makeRequest("GET", "/api/v1/tracks/" + std::to_string(track.id.value())));
    CHECK(response.statusCode == HttpStatus::NotFound);
}

TEST_CASE("GET /api/v1/tracks/{id}/stream streams the real file and supports Range",
          "[api][tracks][stream]") {
    ApiFixture fx;

    const std::string tmpDir = "musicbox_api_stream_test_dir";
    std::filesystem::create_directories(tmpDir);
    const std::string relativePath = "song.flac";
    const std::string content = "0123456789ABCDEF"; // 16 bytes, easy to index into
    {
        std::ofstream out(tmpDir + "/" + relativePath, std::ios::binary);
        out << content;
    }
    const auto root = fx.libraryRootRepo->upsert(std::filesystem::absolute(tmpDir).string());
    const auto track = fx.addTrack(root.id, relativePath, "Song", "Artist", "Album");
    // fileSizeBytes in the DB doesn't need to match the real file for this
    // route -- it re-derives size from FileStreamSource::fileSize(), i.e. a
    // real stat() of the file on disk, not the (possibly stale) DB column.

    SECTION("no Range header returns the whole file with 200") {
        auto response = fx.router->dispatch(
            makeRequest("GET", "/api/v1/tracks/" + std::to_string(track.id.value()) + "/stream"));
        REQUIRE(response.statusCode == HttpStatus::Ok);
        REQUIRE(response.isStreaming());
        std::array<std::byte, 64> buf{};
        std::size_t n = response.streamingBody->read(buf);
        CHECK(std::string(reinterpret_cast<char*>(buf.data()), n) == content);
    }

    SECTION("a satisfiable Range returns 206 with the exact requested window") {
        auto request =
            makeRequest("GET", "/api/v1/tracks/" + std::to_string(track.id.value()) + "/stream");
        request.headers["range"] = "bytes=2-5";
        auto response = fx.router->dispatch(request);
        REQUIRE(response.statusCode == HttpStatus::PartialContent);
        CHECK(response.headers.at("Content-Range") == "bytes 2-5/16");
        CHECK(response.headers.at("Content-Length") == "4");
        std::array<std::byte, 64> buf{};
        std::size_t n = response.streamingBody->read(buf);
        CHECK(std::string(reinterpret_cast<char*>(buf.data()), n) == "2345");
    }

    SECTION("an unsatisfiable Range returns 416") {
        auto request =
            makeRequest("GET", "/api/v1/tracks/" + std::to_string(track.id.value()) + "/stream");
        request.headers["range"] = "bytes=1000-2000";
        auto response = fx.router->dispatch(request);
        CHECK(response.statusCode == HttpStatus::RangeNotSatisfiable);
        CHECK(response.headers.at("Content-Range") == "bytes */16");
    }

    std::filesystem::remove_all(tmpDir);
}

TEST_CASE("GET /api/v1/search fans out across artists, albums, and tracks", "[api][search]") {
    ApiFixture fx;
    const auto root = fx.libraryRootRepo->upsert("/media/music");
    fx.addTrack(root.id, "a.flac", "Instant Crush", "Daft Punk", "Random Access Memories");

    auto response = fx.router->dispatch(makeRequest("GET", "/api/v1/search", "q=daft"));
    REQUIRE(response.statusCode == HttpStatus::Ok);
    auto j = bodyJson(response);
    CHECK(j["artists"].size() == 1);
    CHECK(j["tracks"].size() == 1);

    auto emptyResponse = fx.router->dispatch(makeRequest("GET", "/api/v1/search"));
    auto emptyJson = bodyJson(emptyResponse);
    CHECK(emptyJson["artists"].empty());
    CHECK(emptyJson["albums"].empty());
    CHECK(emptyJson["tracks"].empty());
}

TEST_CASE("Playlist CRUD lifecycle end to end", "[api][playlists]") {
    ApiFixture fx;
    const auto root = fx.libraryRootRepo->upsert("/media/music");
    const auto track = fx.addTrack(root.id, "a.flac", "A", "Artist", "Album");

    // Create
    auto createResponse =
        fx.router->dispatch(makeJsonRequest("POST", "/api/v1/playlists", {{"name", "Road Trip"}}));
    REQUIRE(createResponse.statusCode == HttpStatus::Created);
    auto created = bodyJson(createResponse);
    CHECK(created["name"] == "Road Trip");
    CHECK(created["tracks"].empty());
    const auto playlistId = created["id"].get<std::int64_t>();

    // Add a track
    auto addResponse = fx.router->dispatch(
        makeJsonRequest("POST", "/api/v1/playlists/" + std::to_string(playlistId) + "/tracks",
                        {{"trackId", track.id.value()}}));
    CHECK(addResponse.statusCode == HttpStatus::NoContent);

    // Get detail
    auto getResponse =
        fx.router->dispatch(makeRequest("GET", "/api/v1/playlists/" + std::to_string(playlistId)));
    REQUIRE(getResponse.statusCode == HttpStatus::Ok);
    auto detail = bodyJson(getResponse);
    REQUIRE(detail["tracks"].size() == 1);
    CHECK(detail["tracks"][0]["id"] == track.id.value());

    // Rename
    auto renameResponse = fx.router->dispatch(makeJsonRequest(
        "PUT", "/api/v1/playlists/" + std::to_string(playlistId), {{"name", "Road Trip 2026"}}));
    REQUIRE(renameResponse.statusCode == HttpStatus::Ok);
    CHECK(bodyJson(renameResponse)["name"] == "Road Trip 2026");

    // Remove the track
    auto removeTrackResponse = fx.router->dispatch(
        makeRequest("DELETE", "/api/v1/playlists/" + std::to_string(playlistId) + "/tracks/" +
                                  std::to_string(track.id.value())));
    CHECK(removeTrackResponse.statusCode == HttpStatus::NoContent);

    // Delete the playlist
    auto deleteResponse = fx.router->dispatch(
        makeRequest("DELETE", "/api/v1/playlists/" + std::to_string(playlistId)));
    CHECK(deleteResponse.statusCode == HttpStatus::NoContent);

    auto notFoundResponse =
        fx.router->dispatch(makeRequest("GET", "/api/v1/playlists/" + std::to_string(playlistId)));
    CHECK(notFoundResponse.statusCode == HttpStatus::NotFound);
}

TEST_CASE("POST /api/v1/playlists rejects a missing or empty name", "[api][playlists]") {
    ApiFixture fx;
    auto missing =
        fx.router->dispatch(makeJsonRequest("POST", "/api/v1/playlists", nlohmann::json::object()));
    CHECK(missing.statusCode == HttpStatus::BadRequest);
    CHECK(bodyJson(missing)["error"]["code"] == "VALIDATION_ERROR");

    auto empty = fx.router->dispatch(makeJsonRequest("POST", "/api/v1/playlists", {{"name", ""}}));
    CHECK(empty.statusCode == HttpStatus::BadRequest);
}
