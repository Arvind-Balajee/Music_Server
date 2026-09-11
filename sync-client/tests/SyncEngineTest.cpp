#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "TempDirFixture.hpp"
#include "musicbox/sync/FileHasher.hpp"
#include "musicbox/sync/SyncEngine.hpp"
#include "musicbox/util/Sha256.hpp"

using musicbox::sync::FileHasher;
using musicbox::sync::MockServerManifestSource;
using musicbox::sync::NullUploadTransport;
using musicbox::sync::PushSummary;
using musicbox::sync::ServerManifestEntry;
using musicbox::sync::Sha256FileHasher;
using musicbox::sync::SyncEngine;
using musicbox::sync::test::TempDirFixture;

namespace {

bool contains(const std::vector<std::string>& v, const std::string& item) {
    return std::find(v.begin(), v.end(), item) != v.end();
}

} // namespace

TEST_CASE("SyncEngine::push wires LocalScanner + ManifestDiffer + UploadTransport end to end",
          "[sync][engine]") {
    TempDirFixture dir;
    const auto keepPath = dir.writeFile("keep.mp3", "same content");
    dir.writeFile("brand_new.flac", "totally new content");

    const std::string keepHash = musicbox::util::sha256File(keepPath.string());
    const std::chrono::system_clock::time_point keepMtime =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            std::chrono::file_clock::to_sys(std::filesystem::last_write_time(keepPath)));
    const auto keepSize = std::filesystem::file_size(keepPath);

    // Server already has "keep.mp3" (identical) and "gone.wav" (missing locally
    // -> deleted), but not "brand_new.flac".
    MockServerManifestSource serverManifest({
        ServerManifestEntry{"keep.mp3", keepSize, keepMtime, keepHash},
        ServerManifestEntry{"gone.wav", 12345, keepMtime, "hash-of-gone"},
    });

    NullUploadTransport uploadTransport;
    Sha256FileHasher hasher;
    SyncEngine engine(serverManifest, uploadTransport, hasher);

    const PushSummary summary = engine.push(dir.path().string());

    CHECK(summary.newCount == 1);
    CHECK(summary.deletedCount == 1);
    CHECK(summary.unchangedCount == 1);
    CHECK(summary.modifiedCount == 0);
    CHECK(summary.uploadedOk == 1); // only the new file is uploaded
    CHECK(summary.uploadFailed == 0);

    CHECK(contains(uploadTransport.uploaded(), "brand_new.flac"));
    CHECK_FALSE(contains(uploadTransport.uploaded(), "keep.mp3"));
    CHECK_FALSE(contains(uploadTransport.uploaded(), "gone.wav"));
}

TEST_CASE("SyncEngine::push dryRun computes the diff but uploads nothing", "[sync][engine]") {
    TempDirFixture dir;
    dir.writeFile("only_local.mp3", "content");

    MockServerManifestSource serverManifest({});
    NullUploadTransport uploadTransport;
    Sha256FileHasher hasher;
    SyncEngine engine(serverManifest, uploadTransport, hasher);

    const PushSummary summary = engine.push(dir.path().string(), /*dryRun=*/true);

    CHECK(summary.newCount == 1);
    CHECK(uploadTransport.uploaded().empty());
}

TEST_CASE("SyncEngine::computeDiff with forceHash re-verifies file content", "[sync][engine]") {
    TempDirFixture dir;
    const auto path = dir.writeFile("track.mp3", "original content");
    const std::string correctHash = musicbox::util::sha256File(path.string());
    const std::chrono::system_clock::time_point mtime =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            std::chrono::file_clock::to_sys(std::filesystem::last_write_time(path)));
    const auto size = std::filesystem::file_size(path);

    // Server manifest claims a different hash for the same size+mtime,
    // simulating silent on-disk corruption that `verify` should catch.
    MockServerManifestSource serverManifest({
        ServerManifestEntry{"track.mp3", size, mtime, "not-" + correctHash},
    });

    NullUploadTransport uploadTransport;
    Sha256FileHasher hasher;
    SyncEngine engine(serverManifest, uploadTransport, hasher);

    const auto diffResult = engine.computeDiff(dir.path().string(), /*forceHash=*/true);
    CHECK(diffResult.modifiedFiles.size() == 1);

    // Without forceHash, the size+mtime pre-check alone would call it unchanged.
    const auto quickDiffResult = engine.computeDiff(dir.path().string(), /*forceHash=*/false);
    CHECK(quickDiffResult.unchangedFiles.size() == 1);
}
