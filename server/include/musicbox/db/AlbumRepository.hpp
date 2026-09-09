#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "musicbox/model/Album.hpp"
#include "musicbox/model/Ids.hpp"

namespace musicbox::db {

struct AlbumQuery {
    std::optional<musicbox::ArtistId> artistId;
    std::optional<std::string> search;
    std::size_t limit = 100;
    std::size_t offset = 0;
};

class AlbumRepository {
public:
    virtual ~AlbumRepository() = default;

    [[nodiscard]] virtual std::optional<musicbox::Album> findById(musicbox::AlbumId id) = 0;
    [[nodiscard]] virtual std::vector<musicbox::Album> list(const AlbumQuery& query) = 0;
    [[nodiscard]] virtual std::size_t count(const AlbumQuery& query) = 0;
};

} // namespace musicbox::db
