#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <set>

#include "TempDirFixture.hpp"
#include "musicbox/sync/LocalScanner.hpp"

using musicbox::sync::LocalFileEntry;
using musicbox::sync::LocalScanner;
using musicbox::sync::test::TempDirFixture;

namespace {

std::set<std::string> relativePaths(const std::vector<LocalFileEntry>& entries) {
    std::set<std::string> paths;
    for (const auto& e : entries) {
        paths.insert(e.relativePath);
    }
    return paths;
}

} // namespace

TEST_CASE("LocalScanner finds supported audio files recursively", "[sync][scanner]") {
    TempDirFixture dir;
    dir.writeFile("top.mp3", "abc");
    dir.writeFile("Artist/Album/track.flac", "0123456789");
    dir.writeFile("Artist/Album/other.m4a", "xy");
    dir.writeFile("Artist/Album/notes.txt", "not audio");
    dir.writeFile("Artist/cover.jpg", "not audio either");

    const LocalScanner scanner;
    const auto entries = scanner.scan(dir.path().string());

    const auto paths = relativePaths(entries);
    CHECK(paths.count("top.mp3") == 1);
    CHECK(paths.count("Artist/Album/track.flac") == 1);
    CHECK(paths.count("Artist/Album/other.m4a") == 1);
    CHECK(paths.count("Artist/Album/notes.txt") == 0);
    CHECK(paths.count("Artist/cover.jpg") == 0);
    CHECK(entries.size() == 3);
}

TEST_CASE("LocalScanner reports correct sizes", "[sync][scanner]") {
    TempDirFixture dir;
    dir.writeFile("song.wav", "0123456789"); // 10 bytes

    const LocalScanner scanner;
    const auto entries = scanner.scan(dir.path().string());

    REQUIRE(entries.size() == 1);
    CHECK(entries[0].relativePath == "song.wav");
    CHECK(entries[0].sizeBytes == 10);
}

TEST_CASE("LocalScanner is case-insensitive on extensions", "[sync][scanner]") {
    TempDirFixture dir;
    dir.writeFile("upper.MP3", "abc");
    dir.writeFile("mixed.Flac", "abc");

    const LocalScanner scanner;
    const auto entries = scanner.scan(dir.path().string());

    CHECK(entries.size() == 2);
}

TEST_CASE("LocalScanner does not follow symlinked directories", "[sync][scanner]") {
    TempDirFixture dir;
    dir.writeFile("real/track.mp3", "abc");

    std::error_code ec;
    std::filesystem::create_directory_symlink(dir.path() / "real", dir.path() / "link", ec);
    if (ec) {
        // Symlink creation can fail in restricted sandboxes; skip rather than
        // fail the whole suite in that environment.
        SUCCEED("skipping: could not create a symlink in this environment");
        return;
    }

    const LocalScanner scanner;
    const auto entries = scanner.scan(dir.path().string());
    const auto paths = relativePaths(entries);

    CHECK(paths.count("real/track.mp3") == 1);
    CHECK(paths.count("link/track.mp3") == 0);
}

TEST_CASE("LocalScanner throws for a missing root directory", "[sync][scanner]") {
    const LocalScanner scanner;
    CHECK_THROWS_AS(scanner.scan("/nonexistent/musicbox/root/that/should/not/exist"),
                    std::runtime_error);
}

TEST_CASE("LocalScanner returns an empty manifest for an empty directory", "[sync][scanner]") {
    TempDirFixture dir;
    const LocalScanner scanner;
    const auto entries = scanner.scan(dir.path().string());
    CHECK(entries.empty());
}
