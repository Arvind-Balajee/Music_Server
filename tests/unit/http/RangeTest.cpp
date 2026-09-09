#include <catch2/catch_test_macros.hpp>

#include "musicbox/http/Range.hpp"

using musicbox::http::parseRange;
using musicbox::http::RangeParseOutcome;

namespace {
constexpr std::uint64_t kSize = 10'485'760; // 10 MiB, matches the Plan.md §8 example
}

TEST_CASE("No Range header yields NoRangeHeader", "[http][range]") {
    auto result = parseRange("", kSize);
    CHECK(result.outcome == RangeParseOutcome::NoRangeHeader);
}

TEST_CASE("bytes=0-999 yields the first 1000 bytes", "[http][range]") {
    auto result = parseRange("bytes=0-999", kSize);
    REQUIRE(result.outcome == RangeParseOutcome::Satisfiable);
    CHECK(result.range.start == 0);
    CHECK(result.range.end == 999);
    CHECK(result.range.length() == 1000);
}

TEST_CASE("bytes=1000- yields byte 1000 through EOF", "[http][range]") {
    auto result = parseRange("bytes=1000-", kSize);
    REQUIRE(result.outcome == RangeParseOutcome::Satisfiable);
    CHECK(result.range.start == 1000);
    CHECK(result.range.end == kSize - 1);
}

TEST_CASE("bytes=-500 yields the last 500 bytes", "[http][range]") {
    auto result = parseRange("bytes=-500", kSize);
    REQUIRE(result.outcome == RangeParseOutcome::Satisfiable);
    CHECK(result.range.start == kSize - 500);
    CHECK(result.range.end == kSize - 1);
    CHECK(result.range.length() == 500);
}

TEST_CASE("Plan.md example: bytes=5242880- against a 10 MiB file", "[http][range]") {
    auto result = parseRange("bytes=5242880-", kSize);
    REQUIRE(result.outcome == RangeParseOutcome::Satisfiable);
    CHECK(result.range.start == 5242880);
    CHECK(result.range.end == kSize - 1);
    CHECK(result.range.length() == 5242880);
}

TEST_CASE("A suffix range longer than the file clamps to the whole file", "[http][range]") {
    auto result = parseRange("bytes=-999999999", kSize);
    REQUIRE(result.outcome == RangeParseOutcome::Satisfiable);
    CHECK(result.range.start == 0);
    CHECK(result.range.end == kSize - 1);
}

TEST_CASE("An end beyond EOF clamps to the last byte", "[http][range]") {
    auto result = parseRange("bytes=0-999999999", kSize);
    REQUIRE(result.outcome == RangeParseOutcome::Satisfiable);
    CHECK(result.range.end == kSize - 1);
}

TEST_CASE("start > end is not satisfiable", "[http][range]") {
    auto result = parseRange("bytes=500-100", kSize);
    CHECK(result.outcome == RangeParseOutcome::NotSatisfiable);
}

TEST_CASE("start >= resourceSize is not satisfiable", "[http][range]") {
    auto result = parseRange("bytes=10485760-", kSize);
    CHECK(result.outcome == RangeParseOutcome::NotSatisfiable);
}

TEST_CASE("A zero-length suffix range is not satisfiable", "[http][range]") {
    auto result = parseRange("bytes=-0", kSize);
    CHECK(result.outcome == RangeParseOutcome::NotSatisfiable);
}

TEST_CASE("Malformed unit is not satisfiable", "[http][range]") {
    CHECK(parseRange("items=0-100", kSize).outcome == RangeParseOutcome::NotSatisfiable);
}

TEST_CASE("Non-numeric range values are not satisfiable", "[http][range]") {
    CHECK(parseRange("bytes=abc-def", kSize).outcome == RangeParseOutcome::NotSatisfiable);
    CHECK(parseRange("bytes=0-abc", kSize).outcome == RangeParseOutcome::NotSatisfiable);
    CHECK(parseRange("bytes=-abc", kSize).outcome == RangeParseOutcome::NotSatisfiable);
}

TEST_CASE("An empty spec or a lone dash is not satisfiable", "[http][range]") {
    CHECK(parseRange("bytes=", kSize).outcome == RangeParseOutcome::NotSatisfiable);
    CHECK(parseRange("bytes=-", kSize).outcome == RangeParseOutcome::NotSatisfiable);
}

TEST_CASE("Multi-range specs are rejected as NotSatisfiable (documented MVP limitation)",
          "[http][range]") {
    CHECK(parseRange("bytes=0-99,200-299", kSize).outcome == RangeParseOutcome::NotSatisfiable);
}

TEST_CASE("A Range header against a zero-size resource is never satisfiable", "[http][range]") {
    CHECK(parseRange("bytes=0-0", 0).outcome == RangeParseOutcome::NotSatisfiable);
}
