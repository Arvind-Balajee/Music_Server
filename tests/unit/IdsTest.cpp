#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <unordered_set>

#include "musicbox/model/Ids.hpp"

using musicbox::AlbumId;
using musicbox::TrackId;

TEST_CASE("Id defaults to invalid", "[model][ids]") {
    TrackId id;
    CHECK_FALSE(id.valid());
    CHECK(id.value() == 0);
}

TEST_CASE("Id constructed with a positive value is valid", "[model][ids]") {
    TrackId id{172};
    CHECK(id.valid());
    CHECK(id.value() == 172);
}

TEST_CASE("Ids with different tags are distinct types", "[model][ids]") {
    TrackId track{1};
    AlbumId album{1};
    // This is a compile-time guarantee, not just a runtime check: the following
    // line would fail to compile if uncommented, since TrackId and AlbumId are
    // unrelated types despite wrapping the same underlying value:
    //   bool same = (track == album);
    CHECK(track.value() == album.value());
}

TEST_CASE("Id equality and ordering", "[model][ids]") {
    CHECK(TrackId{1} == TrackId{1});
    CHECK(TrackId{1} != TrackId{2});
    CHECK(TrackId{1} < TrackId{2});
}

// Boundary-value coverage for validity: 0 is the sentinel "no row" value
// (Ids.hpp: "SQLite AUTOINCREMENT primary keys start at 1"), so the boundary
// between invalid and valid is exactly 0 -> 1, and negative values (which
// should never occur in practice, but the type doesn't forbid them) must
// still report as invalid rather than, say, being treated as "some value so
// it must be fine".
TEST_CASE("Id validity boundary values", "[model][ids]") {
    CHECK_FALSE(TrackId{0}.valid());
    CHECK(TrackId{1}.valid());
    CHECK_FALSE(TrackId{-1}.valid());
    CHECK_FALSE(TrackId{std::numeric_limits<TrackId::ValueType>::min()}.valid());
    CHECK(TrackId{std::numeric_limits<TrackId::ValueType>::max()}.valid());
}

TEST_CASE("Id ordering across negative, zero, and large values", "[model][ids]") {
    CHECK(TrackId{-1} < TrackId{0});
    CHECK(TrackId{0} < TrackId{1});
    CHECK(TrackId{std::numeric_limits<TrackId::ValueType>::min()} <
          TrackId{std::numeric_limits<TrackId::ValueType>::max()});
    CHECK(TrackId{std::numeric_limits<TrackId::ValueType>::max()} ==
          TrackId{std::numeric_limits<TrackId::ValueType>::max()});
}

TEST_CASE("Id is usable as a key in unordered associative containers", "[model][ids]") {
    // Exercises std::hash<Id<Tag>> (Ids.hpp) end-to-end, not just its raw
    // return value: equal ids must hash and compare equal as map/set keys.
    std::unordered_set<TrackId> ids;
    ids.insert(TrackId{1});
    ids.insert(TrackId{2});
    ids.insert(TrackId{1}); // duplicate, should not grow the set
    CHECK(ids.size() == 2);
    CHECK(ids.count(TrackId{1}) == 1);
    CHECK(ids.count(TrackId{3}) == 0);
}
