#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "musicbox/model/Ids.hpp"
#include "musicbox/model/LibraryRoot.hpp"

namespace musicbox::db {

class LibraryRootRepository {
public:
    virtual ~LibraryRootRepository() = default;

    [[nodiscard]] virtual std::vector<musicbox::LibraryRoot> list() = 0;
    [[nodiscard]] virtual musicbox::LibraryRoot upsert(const std::string& absolutePath) = 0;
    virtual void markScanned(musicbox::LibraryRootId id,
                             std::chrono::system_clock::time_point when) = 0;
};

// See TrackRepository.hpp's makeSqliteTrackRepository for the databasePath
// contract.
[[nodiscard]] std::unique_ptr<LibraryRootRepository>
makeSqliteLibraryRootRepository(const std::string& databasePath);

} // namespace musicbox::db
