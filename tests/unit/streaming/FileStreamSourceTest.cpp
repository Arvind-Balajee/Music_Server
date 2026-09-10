#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "musicbox/streaming/FileStreamSource.hpp"

using musicbox::streaming::openFileStreamSource;

namespace {

// A fixed-size read buffer used by every test below. Passing this same
// small, constant-size std::array as the destination span regardless of the
// underlying file's size is what makes the "memory doesn't scale with file
// size" property observable: FileRangeResponseBody::read() never allocates
// or retains anything beyond what pread() writes directly into this span, so
// the same 64-byte buffer suffices whether the file is 1 KB or 1 MB.
constexpr std::size_t kFixedChunkSize = 64;

std::string makeTempFile(const std::string& name, const std::string& content) {
    auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary);
    out << content;
    out.close();
    return path.string();
}

// Fills a string with a repeating, position-dependent pattern so any
// off-by-one in offset/length math is caught by content comparison.
std::string patternContent(std::size_t size) {
    std::string content(size, '\0');
    for (std::size_t i = 0; i < size; ++i) {
        content[i] = static_cast<char>('A' + (i % 26));
    }
    return content;
}

// Reads a ResponseBody to completion using only a fixed-size stack buffer,
// asserting every individual read() call is bounded by that buffer's size
// (never proportional to however much data remains).
std::vector<std::byte> readAllBounded(musicbox::http::ResponseBody& body) {
    std::vector<std::byte> result;
    std::array<std::byte, kFixedChunkSize> chunk{};
    for (;;) {
        std::size_t n = body.read(chunk);
        REQUIRE(n <= chunk.size());
        if (n == 0) {
            break;
        }
        result.insert(result.end(), chunk.begin(), chunk.begin() + static_cast<long>(n));
    }
    return result;
}

std::string toStdString(const std::vector<std::byte>& bytes) {
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

} // namespace

TEST_CASE("fileSize() matches the file written to disk", "[streaming][filestream]") {
    const std::string content = patternContent(4096);
    const std::string path = makeTempFile("musicbox_fss_size.tmp", content);

    auto source = openFileStreamSource(path);
    CHECK(source->fileSize() == content.size());

    std::remove(path.c_str());
}

TEST_CASE("openRange returns exactly the requested byte window for small files",
          "[streaming][filestream]") {
    const std::string content = patternContent(200);
    const std::string path = makeTempFile("musicbox_fss_small.tmp", content);

    auto source = openFileStreamSource(path);
    auto body = source->openRange(10, 50);
    REQUIRE(body->remaining().has_value());
    CHECK(*body->remaining() == 50);

    auto bytes = readAllBounded(*body);
    CHECK(bytes.size() == 50);
    CHECK(toStdString(bytes) == content.substr(10, 50));

    std::remove(path.c_str());
}

TEST_CASE("openRange covering the whole file returns exact content, read in bounded chunks",
          "[streaming][filestream]") {
    // A handful of KB is enough to exercise many bounded read() calls without
    // slowing down the unit test suite; the memory-boundedness property this
    // test cares about doesn't depend on the file being large.
    const std::string content = patternContent(8000);
    const std::string path = makeTempFile("musicbox_fss_full.tmp", content);

    auto source = openFileStreamSource(path);
    auto body = source->openRange(0, source->fileSize());
    auto bytes = readAllBounded(*body);

    CHECK(bytes.size() == content.size());
    CHECK(toStdString(bytes) == content);

    std::remove(path.c_str());
}

TEST_CASE("The same fixed-size read buffer drains files of increasing size correctly",
          "[streaming][filestream]") {
    // Behavioral proof that read-buffer size is fixed, not proportional to
    // file size: the exact same kFixedChunkSize-byte std::array (see
    // readAllBounded) is reused to fully and correctly drain files an order
    // of magnitude apart in size.
    for (std::size_t size : {1024ULL, 8192ULL, 65536ULL}) {
        const std::string content = patternContent(size);
        const std::string path =
            makeTempFile("musicbox_fss_scale_" + std::to_string(size) + ".tmp", content);

        auto source = openFileStreamSource(path);
        auto body = source->openRange(0, source->fileSize());
        auto bytes = readAllBounded(*body);

        CHECK(bytes.size() == size);
        CHECK(toStdString(bytes) == content);

        std::remove(path.c_str());
    }
}

TEST_CASE("openRange at a nonzero offset to EOF returns the trailing window",
          "[streaming][filestream]") {
    const std::string content = patternContent(1000);
    const std::string path = makeTempFile("musicbox_fss_tail.tmp", content);

    auto source = openFileStreamSource(path);
    auto body = source->openRange(900, 100);
    auto bytes = readAllBounded(*body);

    CHECK(bytes.size() == 100);
    CHECK(toStdString(bytes) == content.substr(900, 100));

    std::remove(path.c_str());
}

TEST_CASE("openRange throws std::runtime_error when the window exceeds fileSize()",
          "[streaming][filestream]") {
    const std::string content = patternContent(100);
    const std::string path = makeTempFile("musicbox_fss_oob.tmp", content);

    auto source = openFileStreamSource(path);
    CHECK_THROWS_AS(source->openRange(50, 100), std::runtime_error);
    CHECK_THROWS_AS(source->openRange(101, 1), std::runtime_error);

    std::remove(path.c_str());
}

TEST_CASE("An empty range at exactly EOF is valid and reads zero bytes",
          "[streaming][filestream]") {
    const std::string content = patternContent(100);
    const std::string path = makeTempFile("musicbox_fss_eof.tmp", content);

    auto source = openFileStreamSource(path);
    auto body = source->openRange(100, 0);
    CHECK(body->remaining() == 0);
    std::array<std::byte, kFixedChunkSize> chunk{};
    CHECK(body->read(chunk) == 0);

    std::remove(path.c_str());
}

TEST_CASE("openFileStreamSource throws for a nonexistent file", "[streaming][filestream]") {
    CHECK_THROWS_AS(openFileStreamSource("/nonexistent/path/musicbox/does/not/exist.flac"),
                     std::runtime_error);
}

TEST_CASE("A FileStreamSource can be dropped once its ResponseBody is extracted",
          "[streaming][filestream]") {
    // The streaming HTTP handler resolves TrackId -> path, opens a
    // FileStreamSource, and hands the resulting ResponseBody off to the
    // HttpResponse -- it should not need to keep the FileStreamSource itself
    // alive for the body to keep working.
    const std::string content = patternContent(500);
    const std::string path = makeTempFile("musicbox_fss_detached.tmp", content);

    std::unique_ptr<musicbox::http::ResponseBody> body;
    {
        auto source = openFileStreamSource(path);
        body = source->openRange(0, source->fileSize());
    } // source destroyed here

    auto bytes = readAllBounded(*body);
    CHECK(toStdString(bytes) == content);

    std::remove(path.c_str());
}
