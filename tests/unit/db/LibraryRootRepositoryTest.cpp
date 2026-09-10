#include <catch2/catch_test_macros.hpp>

#include "musicbox/db/LibraryRootRepository.hpp"

#include "UniqueMemoryDb.hpp"

using namespace musicbox;
using namespace musicbox::db;

TEST_CASE("LibraryRootRepository upsert creates a new root", "[db][libraryroot]") {
    auto repo = makeSqliteLibraryRootRepository(musicbox::test::uniqueMemoryDbUri());

    const auto root = repo->upsert("/media/music");
    CHECK(root.id.valid());
    CHECK(root.absolutePath == "/media/music");
    CHECK_FALSE(root.lastScanAt.has_value());

    const auto all = repo->list();
    REQUIRE(all.size() == 1);
    CHECK(all.front().id == root.id);
}

TEST_CASE("LibraryRootRepository upsert is idempotent for the same path", "[db][libraryroot]") {
    auto repo = makeSqliteLibraryRootRepository(musicbox::test::uniqueMemoryDbUri());

    const auto first = repo->upsert("/media/music");
    const auto second = repo->upsert("/media/music");

    CHECK(first.id == second.id);
    CHECK(repo->list().size() == 1);
}

TEST_CASE("LibraryRootRepository markScanned sets lastScanAt", "[db][libraryroot]") {
    auto repo = makeSqliteLibraryRootRepository(musicbox::test::uniqueMemoryDbUri());
    const auto root = repo->upsert("/media/music");

    const auto scanTime = std::chrono::system_clock::now();
    repo->markScanned(root.id, scanTime);

    const auto all = repo->list();
    REQUIRE(all.size() == 1);
    REQUIRE(all.front().lastScanAt.has_value());
    // Stored as epoch millis -- compare at millisecond granularity.
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(*all.front().lastScanAt - scanTime).count() == 0);
}

TEST_CASE("LibraryRootRepository supports multiple distinct roots", "[db][libraryroot]") {
    auto repo = makeSqliteLibraryRootRepository(musicbox::test::uniqueMemoryDbUri());
    (void)repo->upsert("/media/music");
    (void)repo->upsert("/mnt/nas/music");

    CHECK(repo->list().size() == 2);
}
