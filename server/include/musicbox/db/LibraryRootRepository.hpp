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

    // Roots currently in use. Retired roots (see `retire`) are excluded, so
    // callers counting or iterating "the library roots" -- e.g. the track count
    // in GET /api/v1/status -- don't see paths the server no longer serves.
    [[nodiscard]] virtual std::vector<musicbox::LibraryRoot> list() = 0;

    // Finds the root for `absolutePath` or creates it. Reviving: if the path
    // was previously retired, this un-retires the existing row (keeping its id,
    // so its soft-deleted tracks are reused rather than duplicated).
    [[nodiscard]] virtual musicbox::LibraryRoot upsert(const std::string& absolutePath) = 0;

    virtual void markScanned(musicbox::LibraryRootId id,
                             std::chrono::system_clock::time_point when) = 0;

    // Marks a root as no longer served, for paths dropped from [library].paths.
    // The caller is responsible for soft-deleting the root's tracks; this only
    // records the root's own state. Kept rather than deleted -- see
    // migration 0002 for why deletion isn't possible.
    virtual void retire(musicbox::LibraryRootId id,
                        std::chrono::system_clock::time_point when) = 0;
};

// See TrackRepository.hpp's makeSqliteTrackRepository for the databasePath
// contract.
[[nodiscard]] std::unique_ptr<LibraryRootRepository>
makeSqliteLibraryRootRepository(const std::string& databasePath);

} // namespace musicbox::db
