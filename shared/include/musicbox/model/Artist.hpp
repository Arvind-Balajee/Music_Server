#pragma once

#include <optional>
#include <string>

#include "musicbox/model/Ids.hpp"

namespace musicbox {

struct Artist {
    ArtistId id;
    std::string name;
    std::optional<std::string> sortName;
};

} // namespace musicbox
