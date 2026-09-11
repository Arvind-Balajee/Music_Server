#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace musicbox::db {

// All timestamp columns (added_at, modified_at, deleted_at, ...) are stored as
// INTEGER epoch milliseconds, matching Track::duration's unit for consistency.
[[nodiscard]] inline std::int64_t toEpochMillis(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

[[nodiscard]] inline std::chrono::system_clock::time_point fromEpochMillis(std::int64_t millis) {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(millis));
}

[[nodiscard]] inline std::optional<std::int64_t>
toOptionalEpochMillis(std::optional<std::chrono::system_clock::time_point> tp) {
    if (!tp.has_value()) {
        return std::nullopt;
    }
    return toEpochMillis(*tp);
}

[[nodiscard]] inline std::optional<std::chrono::system_clock::time_point>
fromOptionalEpochMillis(std::optional<std::int64_t> millis) {
    if (!millis.has_value()) {
        return std::nullopt;
    }
    return fromEpochMillis(*millis);
}

} // namespace musicbox::db
