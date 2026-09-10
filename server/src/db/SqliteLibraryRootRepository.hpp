#pragma once

#include <string>

#include "musicbox/db/LibraryRootRepository.hpp"

#include "SqliteConnection.hpp"

namespace musicbox::db {

class SqliteLibraryRootRepository final : public LibraryRootRepository {
public:
    explicit SqliteLibraryRootRepository(const std::string& databasePath);

    [[nodiscard]] std::vector<musicbox::LibraryRoot> list() override;
    musicbox::LibraryRoot upsert(const std::string& absolutePath) override;
    void markScanned(musicbox::LibraryRootId id, std::chrono::system_clock::time_point when) override;

private:
    SqliteConnection connection_;
};

} // namespace musicbox::db
