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
