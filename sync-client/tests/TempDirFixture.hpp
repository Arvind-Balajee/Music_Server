#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

namespace musicbox::sync::test {

// RAII temp directory fixture shared by sync-client tests. Creates a unique
// directory under the system temp path and recursively removes it on
// destruction.
class TempDirFixture {
public:
    TempDirFixture() {
        const auto base = std::filesystem::temp_directory_path();
        static int counter = 0;
        path_ = base / ("musicbox_sync_test_" + std::to_string(::getpid()) + "_" +
                        std::to_string(counter++));
        std::filesystem::create_directories(path_);
    }

    ~TempDirFixture() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    TempDirFixture(const TempDirFixture&) = delete;
    TempDirFixture& operator=(const TempDirFixture&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

    // Writes a file with the given content (creating parent directories as
    // needed) and returns its absolute path.
    std::filesystem::path writeFile(const std::string& relativePath,
                                    const std::string& content) const {
        const std::filesystem::path full = path_ / relativePath;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream out(full, std::ios::binary);
        out << content;
        out.close();
        return full;
    }

private:
    std::filesystem::path path_;
};

} // namespace musicbox::sync::test
