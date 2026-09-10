#include <catch2/catch_test_macros.hpp>

#include "musicbox/db/AlbumRepository.hpp"
#include "musicbox/db/ArtistRepository.hpp"
#include "musicbox/db/LibraryRootRepository.hpp"
#include "musicbox/db/TrackRepository.hpp"

#include "UniqueMemoryDb.hpp"

using namespace musicbox;
using namespace musicbox::db;

TEST_CASE("AlbumRepository findById returns nullopt for an unknown id", "[db][album]") {
    auto repo = makeSqliteAlbumRepository(musicbox::test::uniqueMemoryDbUri());
    CHECK_FALSE(repo->findById(AlbumId(1)).has_value());
}

TEST_CASE("AlbumRepository sees albums created by TrackRepository::upsert, filterable by artist", "[db][album]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto trackRepo = makeSqliteTrackRepository(dbUri);
    auto albumRepo = makeSqliteAlbumRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    TrackUpsert data;
    data.libraryRootId = root.id;
    data.relativePath = "a.flac";
    data.title = "A";
    data.artistName = "Daft Punk";
    data.albumTitle = "Random Access Memories";
    data.codec = "flac";
    data.modifiedAt = std::chrono::system_clock::now();
    const auto track = trackRepo->upsert(data);

    REQUIRE(track.albumId.has_value());
    const auto album = albumRepo->findById(*track.albumId);
    REQUIRE(album.has_value());
    CHECK(album->title == "Random Access Memories");
    REQUIRE(album->albumArtistId.has_value());
    CHECK(album->albumArtistId == track.artistId);

    AlbumQuery byArtist;
    byArtist.artistId = track.artistId;
    CHECK(albumRepo->list(byArtist).size() == 1);
    CHECK(albumRepo->count(byArtist) == 1);

    AlbumQuery byOtherArtist;
    byOtherArtist.artistId = ArtistId(999999);
    CHECK(albumRepo->list(byOtherArtist).empty());
}

TEST_CASE("AlbumRepository distinguishes same-titled albums by different artists", "[db][album]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto trackRepo = makeSqliteTrackRepository(dbUri);
    auto albumRepo = makeSqliteAlbumRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    for (const std::string& artist : {"Artist One", "Artist Two"}) {
        TrackUpsert data;
        data.libraryRootId = root.id;
        data.relativePath = artist + ".flac";
        data.title = "Track";
        data.artistName = artist;
        data.albumTitle = "Greatest Hits"; // same title, different artist
        data.codec = "flac";
        data.modifiedAt = std::chrono::system_clock::now();
        trackRepo->upsert(data);
    }

    AlbumQuery query;
    query.search = "Greatest Hits";
    CHECK(albumRepo->list(query).size() == 2); // two distinct rows, not collapsed
}
