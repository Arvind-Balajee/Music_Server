#include <catch2/catch_test_macros.hpp>

#include "musicbox/db/ArtistRepository.hpp"
#include "musicbox/db/LibraryRootRepository.hpp"
#include "musicbox/db/TrackRepository.hpp"

#include "UniqueMemoryDb.hpp"

using namespace musicbox;
using namespace musicbox::db;

namespace {

TrackUpsert basicUpsert(LibraryRootId rootId, const std::string& relativePath, const std::string& title) {
    TrackUpsert data;
    data.libraryRootId = rootId;
    data.relativePath = relativePath;
    data.title = title;
    data.artistName = "Daft Punk";
    data.albumTitle = "Random Access Memories";
    data.duration = std::chrono::milliseconds(337000);
    data.codec = "flac";
    data.fileSizeBytes = 43892012;
    data.contentHash = "deadbeef";
    data.modifiedAt = std::chrono::system_clock::now();
    return data;
}

} // namespace

TEST_CASE("TrackRepository upsert inserts a new track and findById returns it", "[db][track]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto repo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    const auto data = basicUpsert(root.id, "Daft Punk/RAM/Instant Crush.flac", "Instant Crush");
    const Track inserted = repo->upsert(data);

    REQUIRE(inserted.id.valid());
    const auto found = repo->findById(inserted.id);
    REQUIRE(found.has_value());
    CHECK(found->title == "Instant Crush");
    CHECK(found->relativePath == "Daft Punk/RAM/Instant Crush.flac");
    CHECK(found->codec == "flac");
    CHECK(found->duration == std::chrono::milliseconds(337000));
    CHECK_FALSE(found->deletedAt.has_value());
    REQUIRE(found->artistId.has_value());
}

TEST_CASE("TrackRepository upsert find-or-creates the artist by name", "[db][track]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto trackRepo = makeSqliteTrackRepository(dbUri);
    auto artistRepo = makeSqliteArtistRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    const auto track1 = trackRepo->upsert(basicUpsert(root.id, "a.flac", "A"));
    const auto track2 = trackRepo->upsert(basicUpsert(root.id, "b.flac", "B"));

    REQUIRE(track1.artistId.has_value());
    REQUIRE(track2.artistId.has_value());
    CHECK(track1.artistId == track2.artistId); // same artist name -> same row, not a duplicate

    const auto artist = artistRepo->findById(*track1.artistId);
    REQUIRE(artist.has_value());
    CHECK(artist->name == "Daft Punk");
}

TEST_CASE("TrackRepository upsert updates an existing row for the same (root, relativePath)", "[db][track]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto repo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    auto data = basicUpsert(root.id, "song.flac", "Original Title");
    const auto first = repo->upsert(data);

    data.title = "Renamed Title";
    data.contentHash = "newhash";
    const auto second = repo->upsert(data);

    CHECK(first.id == second.id); // same row, not a duplicate insert
    const auto found = repo->findById(first.id);
    REQUIRE(found.has_value());
    CHECK(found->title == "Renamed Title");
    CHECK(found->contentHash == "newhash");
}

TEST_CASE("TrackRepository findByPath returns nullopt for an unknown path", "[db][track]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto repo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    CHECK_FALSE(repo->findByPath(root.id, "nonexistent.flac").has_value());
}

TEST_CASE("TrackRepository softDelete hides a track from list() but not findById()", "[db][track]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto repo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    const auto track = repo->upsert(basicUpsert(root.id, "song.flac", "Song"));

    TrackQuery query;
    CHECK(repo->count(query) == 1);

    CHECK(repo->softDelete(track.id, std::chrono::system_clock::now()));

    // findById still resolves the row (it's a tombstone, not gone) --
    // list()/count() exclude it from library browsing, and
    // resolveAbsolutePath refuses to serve it for streaming.
    const auto found = repo->findById(track.id);
    REQUIRE(found.has_value());
    CHECK(found->deletedAt.has_value());

    CHECK(repo->count(query) == 0);
    CHECK(repo->list(query).empty());
    CHECK_FALSE(repo->resolveAbsolutePath(track.id).has_value());
}

TEST_CASE("TrackRepository softDelete is idempotent and reports false the second time", "[db][track]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto repo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");
    const auto track = repo->upsert(basicUpsert(root.id, "song.flac", "Song"));

    CHECK(repo->softDelete(track.id, std::chrono::system_clock::now()));
    CHECK_FALSE(repo->softDelete(track.id, std::chrono::system_clock::now()));
}

TEST_CASE("TrackRepository upsert reappearance clears deletedAt on the same row", "[db][track]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto repo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    const auto data = basicUpsert(root.id, "song.flac", "Song");
    const auto original = repo->upsert(data);
    REQUIRE(repo->softDelete(original.id, std::chrono::system_clock::now()));

    const auto reappeared = repo->upsert(data);
    CHECK(reappeared.id == original.id); // UNIQUE(library_root_id, relative_path) -- same row reused
    const auto found = repo->findById(original.id);
    REQUIRE(found.has_value());
    CHECK_FALSE(found->deletedAt.has_value());
}

TEST_CASE("TrackRepository listByLibraryRoot includes soft-deleted rows", "[db][track]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto repo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    const auto track = repo->upsert(basicUpsert(root.id, "song.flac", "Song"));
    REQUIRE(repo->softDelete(track.id, std::chrono::system_clock::now()));

    const auto all = repo->listByLibraryRoot(root.id);
    REQUIRE(all.size() == 1);
    CHECK(all.front().deletedAt.has_value());
}

TEST_CASE("TrackRepository resolveAbsolutePath joins the library root path with relativePath", "[db][track]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto repo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    const auto track = repo->upsert(basicUpsert(root.id, "Daft Punk/RAM/Instant Crush.flac", "Instant Crush"));
    const auto path = repo->resolveAbsolutePath(track.id);
    REQUIRE(path.has_value());
    CHECK(*path == "/media/music/Daft Punk/RAM/Instant Crush.flac");
}

TEST_CASE("TrackRepository resolveAbsolutePath returns nullopt for an unknown id", "[db][track]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto repo = makeSqliteTrackRepository(dbUri);
    CHECK_FALSE(repo->resolveAbsolutePath(TrackId(999999)).has_value());
}

TEST_CASE("TrackRepository list supports pagination", "[db][track]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto repo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    for (int i = 0; i < 5; ++i) {
        repo->upsert(basicUpsert(root.id, "track" + std::to_string(i) + ".flac", "Track " + std::to_string(i)));
    }

    TrackQuery query;
    query.limit = 2;
    query.offset = 0;
    CHECK(repo->list(query).size() == 2);

    query.offset = 4;
    CHECK(repo->list(query).size() == 1);

    query.limit = 100;
    query.offset = 0;
    CHECK(repo->count(query) == 5);
}

TEST_CASE("TrackRepository list search matches title, artist, and album case-insensitively", "[db][track]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto repo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    repo->upsert(basicUpsert(root.id, "instant-crush.flac", "Instant Crush"));

    TrackQuery byTitle;
    byTitle.search = "crush";
    CHECK(repo->list(byTitle).size() == 1);

    TrackQuery byArtist;
    byArtist.search = "DAFT punk";
    CHECK(repo->list(byArtist).size() == 1);

    TrackQuery byAlbum;
    byAlbum.search = "Random Access";
    CHECK(repo->list(byAlbum).size() == 1);

    TrackQuery noMatch;
    noMatch.search = "nonexistent artist or title";
    CHECK(repo->list(noMatch).empty());
}
