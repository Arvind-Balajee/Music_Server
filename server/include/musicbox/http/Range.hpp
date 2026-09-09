#pragma once

#include <cstdint>
#include <string_view>

namespace musicbox::http {

// An inclusive, fully-resolved byte range against a known resource size.
struct ByteRange {
    std::uint64_t start = 0;
    std::uint64_t end = 0; // inclusive

    [[nodiscard]] std::uint64_t length() const noexcept { return end - start + 1; }
};

enum class RangeParseOutcome {
    NoRangeHeader, // request had no Range header at all -> caller sends 200, full body
    Satisfiable,   // `range` is valid against resourceSize -> caller sends 206
    NotSatisfiable // header present but invalid/out of bounds -> caller sends 416
};

struct RangeParseResult {
    RangeParseOutcome outcome = RangeParseOutcome::NoRangeHeader;
    ByteRange range{}; // meaningful only when outcome == Satisfiable
};

// Parses a `Range` header value (e.g. "bytes=0-999", "bytes=1000-", "bytes=-500")
// against a resource of `resourceSize` bytes. `headerValue` should be the raw
// header value; pass an empty string_view if the request had no Range header.
//
// Only the single-range `bytes=` unit is supported; multi-range specs
// (e.g. "bytes=0-99,200-299") are treated as NotSatisfiable (see docs/http.md).
[[nodiscard]] RangeParseResult parseRange(std::string_view headerValue,
                                           std::uint64_t resourceSize);

} // namespace musicbox::http
