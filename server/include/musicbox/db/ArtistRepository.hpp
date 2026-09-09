#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "musicbox/model/Artist.hpp"
#include "musicbox/model/Ids.hpp"

namespace musicbox::db {

struct ArtistQuery {
    std::optional<std::string> search;
    std::size_t limit = 100;
    std::size_t offset = 0;
};

class ArtistRepository {
public:
    virtual ~ArtistRepository() = default;

    [[nodiscard]] virtual std::optional<musicbox::Artist> findById(musicbox::ArtistId id) = 0;
    [[nodiscard]] virtual std::vector<musicbox::Artist> list(const ArtistQuery& query) = 0;
    [[nodiscard]] virtual std::size_t count(const ArtistQuery& query) = 0;
};

} // namespace musicbox::db
