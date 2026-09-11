#include <catch2/catch_test_macros.hpp>

#include <unordered_map>

#include "musicbox/db/LibraryRootRepository.hpp"
#include "musicbox/db/TrackRepository.hpp"
#include "musicbox/library/LibraryScanner.hpp"

#include "TempDirFixture.hpp"

using namespace musicbox;
using namespace musicbox::library;
using musicbox::library::test::TempDirFixture;

namespace {

// Test double per docs/database.md's own suggestion: lets tests assert
// exactly how many times metadata extraction ran, independent of the real
// TagLib-backed extractor and its parsing quirks.
class FakeMetadataExtractor : public MetadataExtractor {
public:
    int extractCallCount = 0;
    std::unordered_map<std::string, RawTrackMetadata> byPath;

    std::optional<RawTrackMetadata> extract(const std::string& absolutePath) override {
        ++extractCallCount;
        auto it = byPath.find(absolutePath);
        if (it == byPath.end()) {
            return std::nullopt;
        }
        return it->second;
    }
};

RawTrackMetadata makeMetadata(const std::string& title) {
    RawTrackMetadata meta;
    meta.title = title;
    meta.artist = "Test Artist";
    meta.durationMs = 1000;
    meta.codec = "wav";
    return meta;
}

std::string uniqueDbUri() {
    static int counter = 0;
    return "file:musicbox_scanner_test_" + std::to_string(counter++) + "?mode=memory&cache=shared";
}

} // namespace

TEST_CASE("LibraryScanner inserts a new file and extracts metadata exactly once",
          "[library][scanner]") {
    TempDirFixture dir;
    const auto filePath = dir.writeMinimalWav("song.wav");

    const auto dbUri = uniqueDbUri();
    auto trackRepo = musicbox::db::makeSqliteTrackRepository(dbUri);
    auto rootRepo = musicbox::db::makeSqliteLibraryRootRepository(dbUri);
    const auto root = rootRepo->upsert(dir.path().string());

    FakeMetadataExtractor extractor;
    extractor.byPath[filePath.string()] = makeMetadata("Song");

    auto scanner = makeLibraryScanner(*trackRepo, extractor);
    const auto result = scanner->scan(root);

    CHECK(result.inserted == 1);
    CHECK(result.updated == 0);
    CHECK(result.unchanged == 0);
    CHECK(result.deleted == 0);
    CHECK(extractor.extractCallCount == 1);

    const auto found = trackRepo->findByPath(root.id, "song.wav");
    REQUIRE(found.has_value());
    CHECK(found->title == "Song");
}

TEST_CASE("LibraryScanner rescan of an untouched file does not re-extract metadata",
          "[library][scanner]") {
    TempDirFixture dir;
    const auto filePath = dir.writeMinimalWav("song.wav");

    const auto dbUri = uniqueDbUri();
    auto trackRepo = musicbox::db::makeSqliteTrackRepository(dbUri);
    auto rootRepo = musicbox::db::makeSqliteLibraryRootRepository(dbUri);
    const auto root = rootRepo->upsert(dir.path().string());

    FakeMetadataExtractor extractor;
    extractor.byPath[filePath.string()] = makeMetadata("Song");

    auto scanner = makeLibraryScanner(*trackRepo, extractor);
    (void)scanner->scan(root); // first scan: inserts, extracts once
    REQUIRE(extractor.extractCallCount == 1);

    const auto result = scanner->scan(root); // second scan: nothing changed on disk

    CHECK(result.inserted == 0);
    CHECK(result.updated == 0);
    CHECK(result.unchanged == 1);
    CHECK(extractor.extractCallCount ==
          1); // still 1 -- the cheap size+mtime check skipped re-extraction entirely
}

TEST_CASE("LibraryScanner re-extracts and marks updated when file content changes",
          "[library][scanner]") {
    TempDirFixture dir;
    const auto filePath = dir.writeMinimalWav("song.wav", /*durationMs=*/500);

    const auto dbUri = uniqueDbUri();
    auto trackRepo = musicbox::db::makeSqliteTrackRepository(dbUri);
    auto rootRepo = musicbox::db::makeSqliteLibraryRootRepository(dbUri);
    const auto root = rootRepo->upsert(dir.path().string());

    FakeMetadataExtractor extractor;
    extractor.byPath[filePath.string()] = makeMetadata("Song v1");

    auto scanner = makeLibraryScanner(*trackRepo, extractor);
    (void)scanner->scan(root);
    REQUIRE(extractor.extractCallCount == 1);

    // Rewrite with different content (different size -> different duration),
    // so the size+mtime pre-check flags a possible change.
    dir.writeMinimalWav("song.wav", /*durationMs=*/900);
    extractor.byPath[filePath.string()] = makeMetadata("Song v2");

    const auto result = scanner->scan(root);
    CHECK(result.updated == 1);
    CHECK(result.unchanged == 0);
    CHECK(extractor.extractCallCount == 2);

    const auto found = trackRepo->findByPath(root.id, "song.wav");
    REQUIRE(found.has_value());
    CHECK(found->title == "Song v2");
}

TEST_CASE("LibraryScanner treats a pure mtime touch with identical content as unchanged",
          "[library][scanner]") {
    TempDirFixture dir;
    const auto filePath = dir.writeMinimalWav("song.wav");

    const auto dbUri = uniqueDbUri();
    auto trackRepo = musicbox::db::makeSqliteTrackRepository(dbUri);
    auto rootRepo = musicbox::db::makeSqliteLibraryRootRepository(dbUri);
    const auto root = rootRepo->upsert(dir.path().string());

    FakeMetadataExtractor extractor;
    extractor.byPath[filePath.string()] = makeMetadata("Song");

    auto scanner = makeLibraryScanner(*trackRepo, extractor);
    (void)scanner->scan(root);

    // Touch mtime forward without changing a single byte of content.
    const auto newTime = std::filesystem::last_write_time(filePath) + std::chrono::seconds(5);
    std::filesystem::last_write_time(filePath, newTime);

    const auto result = scanner->scan(root);
    // Content hash matches despite the mtime bump -> classified as unchanged
    // from the caller's point of view, even though the scanner internally
    // re-extracted to refresh the stored mtime (see LibraryScanner.cpp's
    // comment on this tradeoff).
    CHECK(result.unchanged == 1);
    CHECK(result.updated == 0);
}

TEST_CASE("LibraryScanner soft-deletes a track whose file was removed from disk",
          "[library][scanner]") {
    TempDirFixture dir;
    const auto filePath = dir.writeMinimalWav("song.wav");

    const auto dbUri = uniqueDbUri();
    auto trackRepo = musicbox::db::makeSqliteTrackRepository(dbUri);
    auto rootRepo = musicbox::db::makeSqliteLibraryRootRepository(dbUri);
    const auto root = rootRepo->upsert(dir.path().string());

    FakeMetadataExtractor extractor;
    extractor.byPath[filePath.string()] = makeMetadata("Song");

    auto scanner = makeLibraryScanner(*trackRepo, extractor);
    (void)scanner->scan(root);

    std::filesystem::remove(filePath);
    const auto result = scanner->scan(root);

    CHECK(result.deleted == 1);
    const auto found = trackRepo->findByPath(root.id, "song.wav");
    REQUIRE(found.has_value());
    CHECK(found->deletedAt.has_value());
}

TEST_CASE("LibraryScanner treats a reappearing file as newly present", "[library][scanner]") {
    TempDirFixture dir;
    const auto filePath = dir.writeMinimalWav("song.wav");

    const auto dbUri = uniqueDbUri();
    auto trackRepo = musicbox::db::makeSqliteTrackRepository(dbUri);
    auto rootRepo = musicbox::db::makeSqliteLibraryRootRepository(dbUri);
    const auto root = rootRepo->upsert(dir.path().string());

    FakeMetadataExtractor extractor;
    extractor.byPath[filePath.string()] = makeMetadata("Song");

    auto scanner = makeLibraryScanner(*trackRepo, extractor);
    (void)scanner->scan(root);
    std::filesystem::remove(filePath);
    (void)scanner->scan(root); // soft-deletes it

    dir.writeMinimalWav("song.wav"); // reappears with the same relative path
    const auto result = scanner->scan(root);

    CHECK(result.inserted == 1);
    const auto found = trackRepo->findByPath(root.id, "song.wav");
    REQUIRE(found.has_value());
    CHECK_FALSE(found->deletedAt.has_value());
}

TEST_CASE("LibraryScanner skips files the metadata extractor can't parse", "[library][scanner]") {
    TempDirFixture dir;
    const auto filePath = dir.writeFile("corrupt.mp3", "not a real mp3");

    const auto dbUri = uniqueDbUri();
    auto trackRepo = musicbox::db::makeSqliteTrackRepository(dbUri);
    auto rootRepo = musicbox::db::makeSqliteLibraryRootRepository(dbUri);
    const auto root = rootRepo->upsert(dir.path().string());

    FakeMetadataExtractor
        extractor; // byPath left empty -> extract() returns nullopt for everything

    auto scanner = makeLibraryScanner(*trackRepo, extractor);
    const auto result = scanner->scan(root);

    CHECK(result.skippedUnreadable == 1);
    CHECK(result.inserted == 0);
    CHECK_FALSE(trackRepo->findByPath(root.id, "corrupt.mp3").has_value());
}

TEST_CASE("LibraryScanner ignores unsupported file extensions", "[library][scanner]") {
    TempDirFixture dir;
    dir.writeFile("notes.txt", "not audio");
    dir.writeFile("cover.jpg", "not audio either");

    const auto dbUri = uniqueDbUri();
    auto trackRepo = musicbox::db::makeSqliteTrackRepository(dbUri);
    auto rootRepo = musicbox::db::makeSqliteLibraryRootRepository(dbUri);
    const auto root = rootRepo->upsert(dir.path().string());

    FakeMetadataExtractor extractor;
    auto scanner = makeLibraryScanner(*trackRepo, extractor);
    const auto result = scanner->scan(root);

    CHECK(result.inserted == 0);
    CHECK(result.skippedUnreadable == 0);
    CHECK(extractor.extractCallCount == 0);
}

TEST_CASE("LibraryScanner does not follow symlinks", "[library][scanner]") {
    TempDirFixture dir;
    const auto realFile = dir.writeMinimalWav("real/song.wav");

    std::error_code ec;
    std::filesystem::create_symlink(realFile, dir.path() / "link-to-song.wav", ec);
    REQUIRE_FALSE(ec); // symlink creation itself must succeed for this test to be meaningful

    const auto dbUri = uniqueDbUri();
    auto trackRepo = musicbox::db::makeSqliteTrackRepository(dbUri);
    auto rootRepo = musicbox::db::makeSqliteLibraryRootRepository(dbUri);
    const auto root = rootRepo->upsert(dir.path().string());

    FakeMetadataExtractor extractor;
    extractor.byPath[realFile.string()] = makeMetadata("Song");

    auto scanner = makeLibraryScanner(*trackRepo, extractor);
    const auto result = scanner->scan(root);

    // Only the real file is indexed; the symlink is skipped entirely.
    CHECK(result.inserted == 1);
    CHECK_FALSE(trackRepo->findByPath(root.id, "link-to-song.wav").has_value());
}
