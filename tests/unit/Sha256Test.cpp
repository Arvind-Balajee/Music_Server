#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>

#include "musicbox/util/Sha256.hpp"

using musicbox::util::sha256Bytes;
using musicbox::util::sha256File;

TEST_CASE("sha256Bytes matches known test vectors", "[util][sha256]") {
    const std::string empty;
    const std::string abc = "abc";
    CHECK(sha256Bytes(empty.data(), empty.size()) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256Bytes(abc.data(), abc.size()) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE("sha256File matches sha256Bytes for the same content", "[util][sha256]") {
    const std::string content = "The quick brown fox jumps over the lazy dog";
    const std::string path = "musicbox_sha256_test_fixture.tmp";
    {
        std::ofstream out(path, std::ios::binary);
        out << content;
    }

    const std::string expected = sha256Bytes(content.data(), content.size());
    // Known standard test vector, cross-checked independently of sha256Bytes:
    CHECK(expected == "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
    CHECK(sha256File(path) == expected);

    std::remove(path.c_str());
}

TEST_CASE("sha256File throws for a missing file", "[util][sha256]") {
    CHECK_THROWS_AS(sha256File("/nonexistent/path/that/should/not/exist.bin"),
                     std::runtime_error);
}

TEST_CASE("sha256File matches sha256Bytes for an empty file", "[util][sha256]") {
    const std::string path = "musicbox_sha256_test_empty.tmp";
    {
        std::ofstream out(path, std::ios::binary);
    }
    CHECK(sha256File(path) == sha256Bytes("", 0));
    std::remove(path.c_str());
}

// sha256File (shared/src/util/Sha256.cpp) reads in fixed 64 KiB (1 << 16)
// chunks specifically so memory use stays bounded regardless of file size.
// That makes the chunk boundary itself the highest-risk spot for an
// off-by-one: a file of exactly one chunk, one byte under, and one byte over
// must all still hash identically to the equivalent in-memory sha256Bytes()
// call, and the read loop must terminate cleanly (no infinite loop, no
// trailing garbage byte) right at EOF == a chunk boundary.
TEST_CASE("sha256File matches sha256Bytes across the 64 KiB chunk boundary", "[util][sha256]") {
    constexpr std::size_t chunkSize = 1 << 16;

    const auto writeAndHash = [](const std::string& path, std::size_t size) {
        std::string content(size, '\0');
        for (std::size_t i = 0; i < size; ++i) {
            content[i] = static_cast<char>(i % 251); // non-repeating-enough filler
        }
        {
            std::ofstream out(path, std::ios::binary);
            out.write(content.data(), static_cast<std::streamsize>(content.size()));
        }
        const std::string expected = sha256Bytes(content.data(), content.size());
        const std::string actual = sha256File(path);
        std::remove(path.c_str());
        return std::pair{expected, actual};
    };

    {
        auto [expected, actual] = writeAndHash("musicbox_sha256_test_chunk_minus1.tmp", chunkSize - 1);
        CHECK(expected == actual);
    }
    {
        auto [expected, actual] = writeAndHash("musicbox_sha256_test_chunk_exact.tmp", chunkSize);
        CHECK(expected == actual);
    }
    {
        auto [expected, actual] = writeAndHash("musicbox_sha256_test_chunk_plus1.tmp", chunkSize + 1);
        CHECK(expected == actual);
    }
    {
        // Two full chunks exactly - the read loop must correctly stop after
        // the second chunk instead of attempting a third empty read.
        auto [expected, actual] = writeAndHash("musicbox_sha256_test_two_chunks.tmp", chunkSize * 2);
        CHECK(expected == actual);
    }
}
