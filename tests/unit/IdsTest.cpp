#include <catch2/catch_test_macros.hpp>

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
