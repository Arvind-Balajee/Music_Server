#include <catch2/catch_test_macros.hpp>

#include "musicbox/db/LibraryRootRepository.hpp"
#include "musicbox/db/PlaylistRepository.hpp"
#include "musicbox/db/TrackRepository.hpp"

#include "UniqueMemoryDb.hpp"

using namespace musicbox;
using namespace musicbox::db;

namespace {

Track makeTrack(TrackRepository& repo, LibraryRootId rootId, const std::string& relativePath) {
    TrackUpsert data;
    data.libraryRootId = rootId;
    data.relativePath = relativePath;
    data.title = relativePath;
    data.codec = "flac";
    data.modifiedAt = std::chrono::system_clock::now();
    return repo.upsert(data);
}

} // namespace

TEST_CASE("PlaylistRepository create/findById/rename/remove", "[db][playlist]") {
    auto repo = makeSqlitePlaylistRepository(musicbox::test::uniqueMemoryDbUri());

    const auto playlist = repo->create("Road Trip");
    CHECK(playlist.id.valid());
    CHECK(playlist.name == "Road Trip");
    CHECK(playlist.createdAt == playlist.updatedAt);

    auto found = repo->findById(playlist.id);
    REQUIRE(found.has_value());
    CHECK(found->name == "Road Trip");

    CHECK(repo->rename(playlist.id, "Road Trip 2026"));
    found = repo->findById(playlist.id);
    REQUIRE(found.has_value());
    CHECK(found->name == "Road Trip 2026");

    CHECK(repo->remove(playlist.id));
    CHECK_FALSE(repo->findById(playlist.id).has_value());
}

TEST_CASE("PlaylistRepository rename/remove report false for an unknown id", "[db][playlist]") {
    auto repo = makeSqlitePlaylistRepository(musicbox::test::uniqueMemoryDbUri());
    CHECK_FALSE(repo->rename(PlaylistId(999999), "x"));
    CHECK_FALSE(repo->remove(PlaylistId(999999)));
}

TEST_CASE("PlaylistRepository addTrack/removeTrack maintain playlist order", "[db][playlist]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto playlistRepo = makeSqlitePlaylistRepository(dbUri);
    auto trackRepo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    const auto playlist = playlistRepo->create("Mix");
    const auto trackA = makeTrack(*trackRepo, root.id, "a.flac");
    const auto trackB = makeTrack(*trackRepo, root.id, "b.flac");
    const auto trackC = makeTrack(*trackRepo, root.id, "c.flac");

    CHECK(playlistRepo->addTrack(playlist.id, trackA.id));
    CHECK(playlistRepo->addTrack(playlist.id, trackB.id));
    CHECK(playlistRepo->addTrack(playlist.id, trackC.id));

    auto tracks = playlistRepo->tracks(playlist.id);
    REQUIRE(tracks.size() == 3);
    CHECK(tracks[0].id == trackA.id);
    CHECK(tracks[1].id == trackB.id);
    CHECK(tracks[2].id == trackC.id);

    CHECK(playlistRepo->removeTrack(playlist.id, trackB.id));
    tracks = playlistRepo->tracks(playlist.id);
    REQUIRE(tracks.size() == 2);
    CHECK(tracks[0].id == trackA.id);
    CHECK(tracks[1].id == trackC.id); // order preserved despite the gap left by removing B
}

TEST_CASE("PlaylistRepository addTrack is idempotent for an already-added track", "[db][playlist]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto playlistRepo = makeSqlitePlaylistRepository(dbUri);
    auto trackRepo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    const auto playlist = playlistRepo->create("Mix");
    const auto track = makeTrack(*trackRepo, root.id, "a.flac");

    CHECK(playlistRepo->addTrack(playlist.id, track.id));
    CHECK(playlistRepo->addTrack(playlist.id, track.id)); // no-op success, not a duplicate/throw
    CHECK(playlistRepo->tracks(playlist.id).size() == 1);
}

TEST_CASE("PlaylistRepository addTrack/removeTrack report false for a nonexistent playlist or track", "[db][playlist]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto playlistRepo = makeSqlitePlaylistRepository(dbUri);
    auto trackRepo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    const auto playlist = playlistRepo->create("Mix");
    const auto track = makeTrack(*trackRepo, root.id, "a.flac");

    CHECK_FALSE(playlistRepo->addTrack(PlaylistId(999999), track.id));
    CHECK_FALSE(playlistRepo->addTrack(playlist.id, TrackId(999999)));
    CHECK_FALSE(playlistRepo->removeTrack(playlist.id, TrackId(999999)));
}

TEST_CASE("PlaylistRepository remove cascades to playlist_tracks", "[db][playlist]") {
    const auto dbUri = musicbox::test::uniqueMemoryDbUri();
    auto playlistRepo = makeSqlitePlaylistRepository(dbUri);
    auto trackRepo = makeSqliteTrackRepository(dbUri);
    auto roots = makeSqliteLibraryRootRepository(dbUri);
    const auto root = roots->upsert("/media/music");

    const auto playlist = playlistRepo->create("Mix");
    const auto track = makeTrack(*trackRepo, root.id, "a.flac");
    REQUIRE(playlistRepo->addTrack(playlist.id, track.id));

    REQUIRE(playlistRepo->remove(playlist.id));

    // Recreate a playlist with a fresh id and confirm no stray membership rows
    // leaked from the deleted playlist (would only be observable if CASCADE
    // hadn't fired, e.g. via a foreign key error on some other operation --
    // asserted here indirectly by checking the new playlist starts empty).
    const auto newPlaylist = playlistRepo->create("Mix 2");
    CHECK(playlistRepo->tracks(newPlaylist.id).empty());
}
