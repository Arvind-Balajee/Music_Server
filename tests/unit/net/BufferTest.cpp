#include "musicbox/net/Buffer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <string>
#include <string_view>

namespace {

std::span<const std::byte> asBytes(std::string_view s) {
    return std::span<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
}

std::string toString(std::span<const std::byte> bytes) {
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

} // namespace

TEST_CASE("Buffer starts empty", "[net][buffer]") {
    musicbox::net::Buffer buffer;
    CHECK(buffer.empty());
    CHECK(buffer.readableBytes() == 0);
    CHECK(buffer.readableView().empty());
}

TEST_CASE("Buffer append makes bytes readable in order", "[net][buffer]") {
    musicbox::net::Buffer buffer;
    buffer.append(asBytes("hello, "));
    buffer.append(asBytes("world"));

    REQUIRE(buffer.readableBytes() == 12);
    CHECK(toString(buffer.readableView()) == "hello, world");
    CHECK_FALSE(buffer.empty());
}

TEST_CASE("Buffer consume advances the read cursor without disturbing remaining bytes", "[net][buffer]") {
    musicbox::net::Buffer buffer;
    buffer.append(asBytes("abcdefghij"));

    buffer.consume(3);
    CHECK(buffer.readableBytes() == 7);
    CHECK(toString(buffer.readableView()) == "defghij");

    buffer.consume(7);
    CHECK(buffer.empty());
    CHECK(buffer.readableBytes() == 0);
}

TEST_CASE("Buffer consuming everything then appending again starts clean", "[net][buffer]") {
    musicbox::net::Buffer buffer;
    buffer.append(asBytes("first"));
    buffer.consume(5);
    REQUIRE(buffer.empty());

    buffer.append(asBytes("second"));
    CHECK(toString(buffer.readableView()) == "second");
}

TEST_CASE("Buffer partial consume followed by many appends preserves content (compaction correctness)",
          "[net][buffer]") {
    musicbox::net::Buffer buffer;

    // Simulate a connection that reads a large header off the front, then
    // keeps appending small chunks -- this is exactly the pattern that
    // triggers front-compaction in the current implementation (see
    // server/src/net/Buffer.cpp).
    std::string built;
    buffer.append(asBytes("HEADERHEADERHEADERHEADER"));
    buffer.consume(20); // consume most of it, simulating a parsed header

    for (int i = 0; i < 500; ++i) {
        std::string chunk = "chunk" + std::to_string(i) + ";";
        buffer.append(asBytes(chunk));
        built += chunk;
    }

    std::string expected = "ADER" + built; // "HEADERHEADERHEADERHEADER"[20:] == "ADER"
    REQUIRE(buffer.readableBytes() == expected.size());
    CHECK(toString(buffer.readableView()) == expected);
}

TEST_CASE("Buffer handles interleaved append/consume of varying sizes", "[net][buffer]") {
    musicbox::net::Buffer buffer;
    std::string reference;

    for (int i = 0; i < 200; ++i) {
        std::string chunk(static_cast<std::size_t>(1 + (i % 7)), static_cast<char>('a' + (i % 26)));
        buffer.append(asBytes(chunk));
        reference += chunk;

        if (i % 3 == 0 && !reference.empty()) {
            std::size_t toConsume = reference.size() / 2;
            buffer.consume(toConsume);
            reference.erase(0, toConsume);
        }

        REQUIRE(buffer.readableBytes() == reference.size());
        CHECK(toString(buffer.readableView()) == reference);
    }
}

TEST_CASE("Buffer append is a no-op for empty spans", "[net][buffer]") {
    musicbox::net::Buffer buffer;
    buffer.append(asBytes("data"));
    buffer.append(std::span<const std::byte>{});
    CHECK(toString(buffer.readableView()) == "data");
}
