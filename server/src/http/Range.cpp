#include "musicbox/http/Range.hpp"

#include <charconv>

namespace musicbox::http {

namespace {

constexpr std::string_view kBytesPrefix = "bytes=";

[[nodiscard]] RangeParseResult notSatisfiable() {
    return RangeParseResult{RangeParseOutcome::NotSatisfiable, ByteRange{}};
}

[[nodiscard]] bool parseUint64(std::string_view s, std::uint64_t& out) {
    if (s.empty()) {
        return false;
    }
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{} && ptr == s.data() + s.size();
}

} // namespace

RangeParseResult parseRange(std::string_view headerValue, std::uint64_t resourceSize) {
    if (headerValue.empty()) {
        return RangeParseResult{RangeParseOutcome::NoRangeHeader, ByteRange{}};
    }

    if (headerValue.rfind(kBytesPrefix, 0) != 0) {
        // Unsupported range unit.
        return notSatisfiable();
    }

    const std::string_view spec = headerValue.substr(kBytesPrefix.size());
    // Multi-range (e.g. "0-99,200-299") is not required for the MVP
    // (docs/http.md) -- reject as a documented limitation, not a bug.
    if (spec.empty() || spec.find(',') != std::string_view::npos) {
        return notSatisfiable();
    }

    const auto dash = spec.find('-');
    if (dash == std::string_view::npos) {
        return notSatisfiable();
    }

    if (resourceSize == 0) {
        // No bytes exist to satisfy any explicit range.
        return notSatisfiable();
    }

    const std::string_view startPart = spec.substr(0, dash);
    const std::string_view endPart = spec.substr(dash + 1);

    ByteRange range;
    if (startPart.empty()) {
        // Suffix range: "bytes=-500" -> last 500 bytes.
        std::uint64_t suffixLength = 0;
        if (!parseUint64(endPart, suffixLength) || suffixLength == 0) {
            return notSatisfiable();
        }
        if (suffixLength > resourceSize) {
            suffixLength =
                resourceSize; // clamp: "last N bytes" of a smaller file is the whole file
        }
        range.start = resourceSize - suffixLength;
        range.end = resourceSize - 1;
    } else {
        std::uint64_t start = 0;
        if (!parseUint64(startPart, start)) {
            return notSatisfiable();
        }
        if (endPart.empty()) {
            // "bytes=1000-" -> byte 1000 to EOF.
            range.start = start;
            range.end = resourceSize - 1;
        } else {
            std::uint64_t end = 0;
            if (!parseUint64(endPart, end)) {
                return notSatisfiable();
            }
            range.start = start;
            range.end = end;
        }
    }

    if (range.start > range.end || range.start >= resourceSize) {
        return notSatisfiable();
    }
    if (range.end >= resourceSize) {
        range.end = resourceSize - 1;
    }

    return RangeParseResult{RangeParseOutcome::Satisfiable, range};
}

} // namespace musicbox::http
