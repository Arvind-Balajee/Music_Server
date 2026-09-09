#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace musicbox::net {

// A growable byte buffer with separate read/write cursors, used for both the
// inbound (parsing) and outbound (send queue) sides of a TcpConnection. Appending
// never invalidates already-consumed bytes' logical position; consuming from the
// front is O(1) amortized (implementation may compact or use a ring layout).
class Buffer {
public:
    Buffer() = default;

    [[nodiscard]] std::size_t readableBytes() const noexcept;
    [[nodiscard]] std::span<const std::byte> readableView() const noexcept;

    // Marks the first `count` readable bytes as consumed. `count` must not exceed
    // readableBytes().
    void consume(std::size_t count);

    // Appends bytes to the end of the buffer.
    void append(std::span<const std::byte> data);

    [[nodiscard]] bool empty() const noexcept { return readableBytes() == 0; }

private:
    std::vector<std::byte> storage_;
    std::size_t readOffset_ = 0;
};

} // namespace musicbox::net
