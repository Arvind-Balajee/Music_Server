#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <string_view>

#include "musicbox/http/HttpRequestParser.hpp"

using musicbox::http::HttpRequestParser;
using musicbox::http::ParseStatus;

namespace {

std::span<const std::byte> asBytes(std::string_view s) {
    return std::span<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
}

} // namespace

TEST_CASE("Parser completes when fed one byte at a time (Plan.md fragmentation example)",
          "[http][parser]") {
    // Plan.md §8: split across three packets mid-path and mid-header; here we
    // go further and feed every single byte of the concatenated request
    // through its own feed() call, which must still parse correctly since the
    // parser preserves state across arbitrarily small chunks.
    const std::string request = "GET /api/tracks/10 HTTP/1.1\r\nHost: musicbox\r\n\r\n";

    HttpRequestParser parser;
    musicbox::http::ParseResult result;
    for (char c : request) {
        parser.feed(asBytes(std::string_view(&c, 1)));
        result = parser.next();
        if (result.status != ParseStatus::NeedMoreData) {
            break;
        }
    }

    REQUIRE(result.status == ParseStatus::Complete);
    CHECK(result.request.method == "GET");
    CHECK(result.request.path == "/api/tracks/10");
    CHECK(result.request.version == "HTTP/1.1");
    REQUIRE(result.request.header("host") != nullptr);
    CHECK(*result.request.header("host") == "musicbox");
    CHECK(result.request.body.empty());
}

TEST_CASE("Parser completes when the exact Plan.md three-packet split is fed as three feed() calls",
          "[http][parser]") {
    HttpRequestParser parser;

    parser.feed(asBytes("GET /api/tr"));
    auto result = parser.next();
    CHECK(result.status == ParseStatus::NeedMoreData);

    parser.feed(asBytes("acks/10 HTTP/1.1\r\nHost: "));
    result = parser.next();
    CHECK(result.status == ParseStatus::NeedMoreData);

    parser.feed(asBytes("musicbox\r\n\r\n"));
    result = parser.next();
    REQUIRE(result.status == ParseStatus::Complete);
    CHECK(result.request.method == "GET");
    CHECK(result.request.path == "/api/tracks/10");
    REQUIRE(result.request.header("host") != nullptr);
    CHECK(*result.request.header("host") == "musicbox");
}

TEST_CASE("Parser drains two pipelined requests delivered in a single feed() call",
          "[http][parser]") {
    HttpRequestParser parser;
    const std::string twoRequests = "GET /a HTTP/1.1\r\nHost: x\r\n\r\n"
                                     "GET /b HTTP/1.1\r\nHost: y\r\n\r\n";
    parser.feed(asBytes(twoRequests));

    auto first = parser.next();
    REQUIRE(first.status == ParseStatus::Complete);
    CHECK(first.request.path == "/a");
    REQUIRE(first.request.header("host") != nullptr);
    CHECK(*first.request.header("host") == "x");

    auto second = parser.next();
    REQUIRE(second.status == ParseStatus::Complete);
    CHECK(second.request.path == "/b");
    REQUIRE(second.request.header("host") != nullptr);
    CHECK(*second.request.header("host") == "y");

    auto third = parser.next();
    CHECK(third.status == ParseStatus::NeedMoreData);
}

TEST_CASE("Parser handles a request split byte-by-byte across the header/body boundary",
          "[http][parser]") {
    HttpRequestParser parser;
    const std::string request =
        "POST /api/v1/playlists HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello";

    musicbox::http::ParseResult result;
    for (char c : request) {
        parser.feed(asBytes(std::string_view(&c, 1)));
        result = parser.next();
        if (result.status != ParseStatus::NeedMoreData) {
            break;
        }
    }

    REQUIRE(result.status == ParseStatus::Complete);
    CHECK(result.request.method == "POST");
    REQUIRE(result.request.body.size() == 5);
    CHECK(std::memcmp(result.request.body.data(), "hello", 5) == 0);
}

TEST_CASE("Parser parses query string separately from the decoded path", "[http][parser]") {
    HttpRequestParser parser;
    parser.feed(asBytes("GET /api/v1/search?q=daft%20punk HTTP/1.1\r\n\r\n"));
    auto result = parser.next();
    REQUIRE(result.status == ParseStatus::Complete);
    CHECK(result.request.path == "/api/v1/search");
    CHECK(result.request.query == "q=daft%20punk");
}

TEST_CASE("Parser percent-decodes the path", "[http][parser]") {
    HttpRequestParser parser;
    parser.feed(asBytes("GET /api/v1/Daft%20Punk HTTP/1.1\r\n\r\n"));
    auto result = parser.next();
    REQUIRE(result.status == ParseStatus::Complete);
    CHECK(result.request.path == "/api/v1/Daft Punk");
}

TEST_CASE("Parser rejects a request line without HTTP-version", "[http][parser]") {
    HttpRequestParser parser;
    parser.feed(asBytes("GET /x\r\n\r\n"));
    auto result = parser.next();
    REQUIRE(result.status == ParseStatus::Error);
    CHECK(result.errorStatus == musicbox::http::HttpStatus::BadRequest);
}

TEST_CASE("Parser rejects a malformed header line with no colon", "[http][parser]") {
    HttpRequestParser parser;
    parser.feed(asBytes("GET / HTTP/1.1\r\nNotAHeader\r\n\r\n"));
    auto result = parser.next();
    REQUIRE(result.status == ParseStatus::Error);
    CHECK(result.errorStatus == musicbox::http::HttpStatus::BadRequest);
}

TEST_CASE("Parser rejects an oversized request line rather than buffering it unbounded",
          "[http][parser]") {
    HttpRequestParser::Limits limits;
    limits.maxRequestLineLength = 16;
    HttpRequestParser parser(limits);

    parser.feed(asBytes("GET /this-path-is-definitely-too-long-for-the-limit HTTP/1.1\r\n"));
    auto result = parser.next();
    REQUIRE(result.status == ParseStatus::Error);
    CHECK(result.errorStatus == musicbox::http::HttpStatus::BadRequest);
}

TEST_CASE("Parser rejects headers exceeding the configured byte budget", "[http][parser]") {
    HttpRequestParser::Limits limits;
    limits.maxHeaderBytes = 32;
    HttpRequestParser parser(limits);

    parser.feed(asBytes("GET / HTTP/1.1\r\n"));
    CHECK(parser.next().status == ParseStatus::NeedMoreData);

    parser.feed(asBytes("X-Very-Long-Header-Name: some-fairly-long-value\r\n\r\n"));
    auto result = parser.next();
    REQUIRE(result.status == ParseStatus::Error);
    CHECK(result.errorStatus == musicbox::http::HttpStatus::BadRequest);
}

TEST_CASE("Parser rejects too many header lines", "[http][parser]") {
    HttpRequestParser::Limits limits;
    limits.maxHeaderCount = 2;
    HttpRequestParser parser(limits);

    parser.feed(asBytes("GET / HTTP/1.1\r\nA: 1\r\nB: 2\r\nC: 3\r\n\r\n"));
    auto result = parser.next();
    REQUIRE(result.status == ParseStatus::Error);
    CHECK(result.errorStatus == musicbox::http::HttpStatus::BadRequest);
}

TEST_CASE("Parser rejects a Content-Length exceeding the max body size", "[http][parser]") {
    HttpRequestParser::Limits limits;
    limits.maxBodySize = 10;
    HttpRequestParser parser(limits);

    parser.feed(asBytes("POST / HTTP/1.1\r\nContent-Length: 1000\r\n\r\n"));
    auto result = parser.next();
    REQUIRE(result.status == ParseStatus::Error);
    CHECK(result.errorStatus == musicbox::http::HttpStatus::BadRequest);
}

TEST_CASE("Parser rejects a non-numeric Content-Length", "[http][parser]") {
    HttpRequestParser parser;
    parser.feed(asBytes("POST / HTTP/1.1\r\nContent-Length: abc\r\n\r\n"));
    auto result = parser.next();
    REQUIRE(result.status == ParseStatus::Error);
    CHECK(result.errorStatus == musicbox::http::HttpStatus::BadRequest);
}

TEST_CASE("Parser lower-cases header names", "[http][parser]") {
    HttpRequestParser parser;
    parser.feed(asBytes("GET / HTTP/1.1\r\nX-Custom-Header: Value\r\n\r\n"));
    auto result = parser.next();
    REQUIRE(result.status == ParseStatus::Complete);
    REQUIRE(result.request.header("x-custom-header") != nullptr);
    CHECK(*result.request.header("x-custom-header") == "Value");
}
