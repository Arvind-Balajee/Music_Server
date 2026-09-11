#pragma once

#include <compare>
#include <cstdint>
#include <functional>

namespace musicbox {

// Strong, non-interchangeable entity identifier. `Id<ArtistTag>` and `Id<TrackId>`
// are distinct types even though both wrap an int64_t, so a value from one
// entity can never be accidentally passed where another is expected.
// See docs/adr and docs/database.md for rationale.
template <typename Tag> class Id {
public:
    using ValueType = std::int64_t;

    constexpr Id() noexcept = default;
    constexpr explicit Id(ValueType value) noexcept : value_(value) {}

    [[nodiscard]] constexpr ValueType value() const noexcept { return value_; }

    // An id of 0 (the default) never corresponds to a real row; SQLite AUTOINCREMENT
    // primary keys start at 1.
    [[nodiscard]] constexpr bool valid() const noexcept { return value_ > 0; }

    friend constexpr bool operator==(const Id&, const Id&) = default;
    friend constexpr auto operator<=>(const Id&, const Id&) = default;

private:
    ValueType value_ = 0;
};

struct ArtistTag {};
struct AlbumTag {};
struct TrackTag {};
struct PlaylistTag {};
struct LibraryRootTag {};

using ArtistId = Id<ArtistTag>;
using AlbumId = Id<AlbumTag>;
using TrackId = Id<TrackTag>;
using PlaylistId = Id<PlaylistTag>;
using LibraryRootId = Id<LibraryRootTag>;

} // namespace musicbox

// Enables Id<Tag> as a key in std::unordered_map/std::unordered_set.
template <typename Tag> struct std::hash<musicbox::Id<Tag>> {
    std::size_t operator()(const musicbox::Id<Tag>& id) const noexcept {
        return std::hash<typename musicbox::Id<Tag>::ValueType>{}(id.value());
    }
};
