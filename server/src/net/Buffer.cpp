#include "musicbox/net/Buffer.hpp"

#include <cassert>

namespace musicbox::net {

// Implementation note (compaction-on-append, not a ring buffer): `consume()` is
// O(1) — it only advances `readOffset_` (and resets both to zero once every byte
// has been consumed, which is the common case for small request/response
// bodies). `append()` only pays a compaction cost (shifting unread bytes down to
// index 0) when at least half of `storage_` is already-consumed slack; that cost
// is amortized against the bytes appended since the last compaction, so a
// steady-state connection that alternates "read some, consume some" never
// re-copies more than a small constant factor of its own throughput. A true
// ring buffer would avoid the occasional shift entirely at the cost of modular
// index arithmetic everywhere `readableView()` is used (which must currently
// return one contiguous span, per the header contract); revisit only if
// profiling shows this is a hot path (see docs/networking.md).

std::size_t Buffer::readableBytes() const noexcept { return storage_.size() - readOffset_; }

std::span<const std::byte> Buffer::readableView() const noexcept {
    return std::span<const std::byte>(storage_.data() + readOffset_, readableBytes());
}

void Buffer::consume(std::size_t count) {
    assert(count <= readableBytes() && "Buffer::consume: count exceeds readableBytes()");
    readOffset_ += count;
    if (readOffset_ == storage_.size()) {
        // Fully drained: reclaim all backing storage rather than letting an
        // ever-growing vector sit around empty.
        storage_.clear();
        readOffset_ = 0;
    }
}

void Buffer::append(std::span<const std::byte> data) {
    if (data.empty()) {
        return;
    }

    if (readOffset_ > 0 && readOffset_ >= storage_.size() / 2) {
        // At least half of the buffer is already-consumed slack; shift the
        // remaining readable bytes down to the front before growing further.
        storage_.erase(storage_.begin(), storage_.begin() + static_cast<std::ptrdiff_t>(readOffset_));
        readOffset_ = 0;
    }

    storage_.insert(storage_.end(), data.begin(), data.end());
}

} // namespace musicbox::net
