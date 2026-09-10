#include <catch2/catch_test_macros.hpp>

#include "musicbox/db/ArtistRepository.hpp"
#include "musicbox/db/LibraryRootRepository.hpp"
#include "musicbox/db/TrackRepository.hpp"

#include "UniqueMemoryDb.hpp"

using namespace musicbox;
using namespace musicbox::db;

TEST_CASE("ArtistRepository findById returns nullopt for an unknown id", "[db][artist]") {
    auto repo = makeSqliteArtistRepository(musicbox::test::uniqueMemoryDbUri());
    CHECK_FALSE(repo->findById(ArtistId(1)).has_value());
}

TEST_CASE("ArtistRepository sees artists created by TrackRepository::upsert", "[db][artist]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto trackRepo = makeSqliteTrackRepository(dbUri);
    auto artistRepo = makeSqliteArtistRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    TrackUpsert data;
    data.libraryRootId = root.id;
    data.relativePath = "a.flac";
    data.title = "A";
    data.artistName = "Tame Impala";
    data.codec = "flac";
    data.modifiedAt = std::chrono::system_clock::now();
    trackRepo->upsert(data);

    ArtistQuery query;
    const auto all = artistRepo->list(query);
    REQUIRE(all.size() == 1);
    CHECK(all.front().name == "Tame Impala");
    CHECK(artistRepo->count(query) == 1);
}

TEST_CASE("ArtistRepository list search is case-insensitive substring match", "[db][artist]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto trackRepo = makeSqliteTrackRepository(dbUri);
    auto artistRepo = makeSqliteArtistRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    TrackUpsert data;
    data.libraryRootId = root.id;
    data.relativePath = "a.flac";
    data.title = "A";
    data.artistName = "Daft Punk";
    data.codec = "flac";
    data.modifiedAt = std::chrono::system_clock::now();
    trackRepo->upsert(data);

    ArtistQuery query;
    query.search = "daft";
    CHECK(artistRepo->list(query).size() == 1);

    query.search = "nonexistent";
    CHECK(artistRepo->list(query).empty());
}

TEST_CASE("ArtistRepository list paginates and orders by name", "[db][artist]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto trackRepo = makeSqliteTrackRepository(dbUri);
    auto artistRepo = makeSqliteArtistRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    for (const std::string& name : {"Zebra", "Apple", "Mango"}) {
        TrackUpsert data;
        data.libraryRootId = root.id;
        data.relativePath = name + ".flac";
        data.title = name;
        data.artistName = name;
        data.codec = "flac";
        data.modifiedAt = std::chrono::system_clock::now();
        trackRepo->upsert(data);
    }

    ArtistQuery query;
    const auto all = artistRepo->list(query);
    REQUIRE(all.size() == 3);
    CHECK(all[0].name == "Apple");
    CHECK(all[1].name == "Mango");
    CHECK(all[2].name == "Zebra");

    query.limit = 1;
    query.offset = 1;
    const auto page = artistRepo->list(query);
    REQUIRE(page.size() == 1);
    CHECK(page.front().name == "Mango");
}
