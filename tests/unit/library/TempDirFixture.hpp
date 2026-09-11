#pragma once

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

namespace musicbox::library::test {

// RAII temp directory fixture. Creates a unique directory under the system
// temp path and recursively removes it on destruction.
class TempDirFixture {
public:
    TempDirFixture() {
        const auto base = std::filesystem::temp_directory_path();
        static int counter = 0;
        path_ = base / ("musicbox_library_test_" + std::to_string(::getpid()) + "_" +
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

    std::filesystem::path writeFile(const std::string& relativePath,
                                    const std::string& content) const {
        const std::filesystem::path full = path_ / relativePath;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream out(full, std::ios::binary);
        out << content;
        out.close();
        return full;
    }

    // Writes a minimal, syntactically valid mono 16-bit PCM WAV file (silent,
    // durationMs long) -- real enough for TagLib to open and report
    // duration/codec, without needing a checked-in binary fixture.
    std::filesystem::path writeMinimalWav(const std::string& relativePath, int durationMs = 500,
                                          int sampleRate = 44100) const {
        const std::filesystem::path full = path_ / relativePath;
        std::filesystem::create_directories(full.parent_path());

        const std::uint32_t numSamples = static_cast<std::uint32_t>(sampleRate * durationMs / 1000);
        const std::uint16_t numChannels = 1;
        const std::uint16_t bitsPerSample = 16;
        const std::uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
        const std::uint16_t blockAlign = numChannels * bitsPerSample / 8;
        const std::uint32_t dataSize = numSamples * blockAlign;
        const std::uint32_t riffSize = 36 + dataSize;

        std::ofstream out(full, std::ios::binary);
        auto writeU32 = [&](std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
        auto writeU16 = [&](std::uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };

        out.write("RIFF", 4);
        writeU32(riffSize);
        out.write("WAVE", 4);
        out.write("fmt ", 4);
        writeU32(16); // fmt chunk size (PCM)
        writeU16(1);  // PCM format
        writeU16(numChannels);
        writeU32(static_cast<std::uint32_t>(sampleRate));
        writeU32(byteRate);
        writeU16(blockAlign);
        writeU16(bitsPerSample);
        out.write("data", 4);
        writeU32(dataSize);

        std::vector<char> silence(dataSize, 0);
        out.write(silence.data(), static_cast<std::streamsize>(silence.size()));

        return full;
    }

private:
    std::filesystem::path path_;
};

} // namespace musicbox::library::test
