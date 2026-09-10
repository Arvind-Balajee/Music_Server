#include "musicbox/streaming/FileStreamSource.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace musicbox::streaming {

namespace {

// Owns the open file descriptor. Shared (not exclusive) between the
// FileStreamSource and every ResponseBody it has handed out, so the fd stays
// valid for as long as either is still alive -- callers are free to drop the
// FileStreamSource once they've extracted the ResponseBody they need for the
// current request.
class FdHolder {
public:
    explicit FdHolder(int fd) : fd_(fd) {}
    ~FdHolder() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    FdHolder(const FdHolder&) = delete;
    FdHolder& operator=(const FdHolder&) = delete;

    [[nodiscard]] int get() const noexcept { return fd_; }

private:
    int fd_;
};

// Streams a [offset, offset+length) window of a file via pread(), never
// buffering more than the caller's destination span for a single read() call
// regardless of how large `length` is.
class FileRangeResponseBody final : public musicbox::http::ResponseBody {
public:
    FileRangeResponseBody(std::shared_ptr<FdHolder> fd, std::uint64_t offset, std::uint64_t length)
        : fd_(std::move(fd)), cursor_(offset), remaining_(length) {}

    std::size_t read(std::span<std::byte> destination) override {
        if (remaining_ == 0 || destination.empty()) {
            return 0;
        }

        std::size_t toRead = destination.size();
        if (static_cast<std::uint64_t>(toRead) > remaining_) {
            toRead = static_cast<std::size_t>(remaining_);
        }

        ssize_t bytesRead = 0;
        do {
            bytesRead = ::pread(fd_->get(), destination.data(), toRead, static_cast<off_t>(cursor_));
        } while (bytesRead < 0 && errno == EINTR);

        if (bytesRead < 0) {
            throw std::runtime_error(std::string("FileStreamSource: pread failed: ") +
                                      std::strerror(errno));
        }
        if (bytesRead == 0) {
            // File shrank/was truncated concurrently; stop rather than spin.
            remaining_ = 0;
            return 0;
        }

        cursor_ += static_cast<std::uint64_t>(bytesRead);
        remaining_ -= static_cast<std::uint64_t>(bytesRead);
        return static_cast<std::size_t>(bytesRead);
    }

    [[nodiscard]] std::optional<std::uint64_t> remaining() const override { return remaining_; }

private:
    std::shared_ptr<FdHolder> fd_;
    std::uint64_t cursor_;
    std::uint64_t remaining_;
};

class FileStreamSourceImpl final : public FileStreamSource {
public:
    FileStreamSourceImpl(std::shared_ptr<FdHolder> fd, std::uint64_t size)
        : fd_(std::move(fd)), size_(size) {}

    [[nodiscard]] std::uint64_t fileSize() const override { return size_; }

    [[nodiscard]] std::unique_ptr<musicbox::http::ResponseBody> openRange(std::uint64_t offset,
                                                                           std::uint64_t length) override {
        if (offset > size_ || length > size_ - offset) {
            throw std::runtime_error(
                "FileStreamSource::openRange: [offset, offset+length) exceeds fileSize()");
        }
        return std::make_unique<FileRangeResponseBody>(fd_, offset, length);
    }

private:
    std::shared_ptr<FdHolder> fd_;
    std::uint64_t size_;
};

} // namespace

std::unique_ptr<FileStreamSource> openFileStreamSource(const std::string& absolutePath) {
    const int fd = ::open(absolutePath.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("openFileStreamSource: failed to open '" + absolutePath +
                                  "': " + std::strerror(errno));
    }
    auto holder = std::make_shared<FdHolder>(fd);

    struct stat st {};
    if (::fstat(holder->get(), &st) != 0) {
        throw std::runtime_error("openFileStreamSource: fstat('" + absolutePath +
                                  "') failed: " + std::strerror(errno));
    }
    if (!S_ISREG(st.st_mode)) {
        throw std::runtime_error("openFileStreamSource: '" + absolutePath +
                                  "' is not a regular file");
    }

    return std::make_unique<FileStreamSourceImpl>(std::move(holder),
                                                   static_cast<std::uint64_t>(st.st_size));
}

} // namespace musicbox::streaming
