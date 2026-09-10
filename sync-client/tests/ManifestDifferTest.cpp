#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <map>
#include <vector>

#include "musicbox/sync/ManifestDiffer.hpp"

using namespace std::chrono_literals;
using musicbox::sync::DiffResult;
using musicbox::sync::FileHasher;
using musicbox::sync::LocalFileEntry;
using musicbox::sync::ManifestDiffer;
using musicbox::sync::ServerManifestEntry;

namespace {

// Test double: returns a scripted hash per relativePath suffix and counts how
// many times it was actually invoked, so tests can verify the size+mtime
// pre-check in ManifestDiffer skips hashing for files it can already prove are
// unchanged (or new).
class ScriptedCountingHasher final : public FileHasher {
public:
    void script(const std::string& relativePathSuffix, std::string hash) {
        scripts_[relativePathSuffix] = std::move(hash);
    }

    [[nodiscard]] std::string hashFile(const std::string& absolutePath) const override {
        ++callCount_;
        calledPaths_.push_back(absolutePath);
        for (const auto& [suffix, hash] : scripts_) {
            if (absolutePath.size() >= suffix.size() &&
                absolutePath.compare(absolutePath.size() - suffix.size(), suffix.size(), suffix) == 0) {
                return hash;
            }
        }
        return "unscripted-hash";
    }

    [[nodiscard]] int callCount() const noexcept { return callCount_; }
    [[nodiscard]] const std::vector<std::string>& calledPaths() const noexcept { return calledPaths_; }

private:
    std::map<std::string, std::string> scripts_;
    mutable int callCount_ = 0;
    mutable std::vector<std::string> calledPaths_;
};

bool contains(const std::vector<std::string>& v, const std::string& item) {
    return std::find(v.begin(), v.end(), item) != v.end();
}

const auto kBaseTime = std::chrono::system_clock::time_point(1'700'000'000s);

} // namespace

TEST_CASE("ManifestDiffer classifies new/modified/deleted/unchanged correctly", "[sync][diff]") {
    const std::vector<ServerManifestEntry> server = {
        {"unchanged.mp3", 1000, kBaseTime, "hash-unchanged"},
        {"touched_same_content.mp3", 2000, kBaseTime, "hash-touched"},
        {"modified.mp3", 3000, kBaseTime, "hash-old-modified"},
        {"deleted.mp3", 4000, kBaseTime, "hash-deleted"},
    };

    const std::vector<LocalFileEntry> local = {
        {"unchanged.mp3", 1000, kBaseTime},                 // exact size+mtime match
        {"touched_same_content.mp3", 2000, kBaseTime + 60s}, // mtime differs, content doesn't
        {"modified.mp3", 3500, kBaseTime},                   // size differs, content differs
        {"new.mp3", 500, kBaseTime},                         // not present on server at all
    };

    ScriptedCountingHasher hasher;
    hasher.script("touched_same_content.mp3", "hash-touched");     // matches server -> unchanged
    hasher.script("modified.mp3", "hash-new-modified");            // differs from server -> modified

    const ManifestDiffer differ;
    const DiffResult result = differ.diff(local, server, "/music", hasher);

    CHECK(contains(result.unchangedFiles, "unchanged.mp3"));
    CHECK(contains(result.unchangedFiles, "touched_same_content.mp3"));
    CHECK(result.unchangedFiles.size() == 2);

    CHECK(contains(result.modifiedFiles, "modified.mp3"));
    CHECK(result.modifiedFiles.size() == 1);

    CHECK(contains(result.newFiles, "new.mp3"));
    CHECK(result.newFiles.size() == 1);

    CHECK(contains(result.deletedFiles, "deleted.mp3"));
    CHECK(result.deletedFiles.size() == 1);

    // Every local file is classified exactly once, plus the one server-only
    // (deleted) file that has no local counterpart.
    CHECK(result.totalFiles() == local.size() + 1);
}

TEST_CASE("ManifestDiffer skips hashing files the size+mtime pre-check already proves unchanged", "[sync][diff]") {
    const std::vector<ServerManifestEntry> server = {
        {"a.mp3", 100, kBaseTime, "hash-a"},
        {"b.mp3", 200, kBaseTime, "hash-b"},
    };
    const std::vector<LocalFileEntry> local = {
        {"a.mp3", 100, kBaseTime}, // identical size+mtime -> must NOT be hashed
        {"b.mp3", 200, kBaseTime}, // identical size+mtime -> must NOT be hashed
    };

    ScriptedCountingHasher hasher;
    const ManifestDiffer differ;
    const DiffResult result = differ.diff(local, server, "/music", hasher);

    CHECK(hasher.callCount() == 0);
    CHECK(result.unchangedFiles.size() == 2);
}

TEST_CASE("ManifestDiffer hashes only files whose size or mtime differs from the server", "[sync][diff]") {
    const std::vector<ServerManifestEntry> server = {
        {"same.mp3", 100, kBaseTime, "hash-same"},
        {"size-differs.mp3", 100, kBaseTime, "hash-x"},
        {"mtime-differs.mp3", 200, kBaseTime, "hash-y"},
    };
    const std::vector<LocalFileEntry> local = {
        {"same.mp3", 100, kBaseTime},
        {"size-differs.mp3", 999, kBaseTime},
        {"mtime-differs.mp3", 200, kBaseTime + 1h},
    };

    ScriptedCountingHasher hasher;
    hasher.script("size-differs.mp3", "hash-x-new");
    hasher.script("mtime-differs.mp3", "hash-y-new");

    const ManifestDiffer differ;
    const DiffResult result = differ.diff(local, server, "/music", hasher);

    CHECK(hasher.callCount() == 2);
    CHECK(contains(hasher.calledPaths(), std::string("/music/size-differs.mp3")));
    CHECK(contains(hasher.calledPaths(), std::string("/music/mtime-differs.mp3")));
    CHECK(result.unchangedFiles.size() == 1); // same.mp3 only
    CHECK(result.modifiedFiles.size() == 2);
}

TEST_CASE("ManifestDiffer never hashes brand-new files", "[sync][diff]") {
    const std::vector<ServerManifestEntry> server = {};
    const std::vector<LocalFileEntry> local = {
        {"new1.mp3", 100, kBaseTime},
        {"new2.mp3", 200, kBaseTime},
    };

    ScriptedCountingHasher hasher;
    const ManifestDiffer differ;
    const DiffResult result = differ.diff(local, server, "/music", hasher);

    CHECK(hasher.callCount() == 0);
    CHECK(result.newFiles.size() == 2);
}

TEST_CASE("ManifestDiffer forceHash re-hashes everything even when metadata matches", "[sync][diff]") {
    const std::vector<ServerManifestEntry> server = {
        {"a.mp3", 100, kBaseTime, "hash-a"},
    };
    const std::vector<LocalFileEntry> local = {
        {"a.mp3", 100, kBaseTime}, // metadata matches, but forceHash is set
    };

    ScriptedCountingHasher hasher;
    hasher.script("a.mp3", "hash-a-corrupted");

    const ManifestDiffer differ;
    const DiffResult result = differ.diff(local, server, "/music", hasher, /*forceHash=*/true);

    CHECK(hasher.callCount() == 1);
    CHECK(result.modifiedFiles.size() == 1); // hash mismatch detected despite matching size+mtime
}

TEST_CASE("ManifestDiffer with an empty local manifest reports everything as deleted", "[sync][diff]") {
    const std::vector<ServerManifestEntry> server = {
        {"a.mp3", 100, kBaseTime, "hash-a"},
        {"b.mp3", 200, kBaseTime, "hash-b"},
    };
    const std::vector<LocalFileEntry> local = {};

    ScriptedCountingHasher hasher;
    const ManifestDiffer differ;
    const DiffResult result = differ.diff(local, server, "/music", hasher);

    CHECK(result.deletedFiles.size() == 2);
    CHECK(hasher.callCount() == 0);
}

TEST_CASE("ManifestDiffer with an empty server manifest reports everything as new", "[sync][diff]") {
    const std::vector<ServerManifestEntry> server = {};
    const std::vector<LocalFileEntry> local = {
        {"a.mp3", 100, kBaseTime},
    };

    ScriptedCountingHasher hasher;
    const ManifestDiffer differ;
    const DiffResult result = differ.diff(local, server, "/music", hasher);

    CHECK(result.newFiles.size() == 1);
    CHECK(hasher.callCount() == 0);
}
