#include "musicbox/util/Sha256.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

// Self-contained SHA-256 (FIPS 180-4). No external dependency: this is a small,
// well-defined algorithm and pulling in a crypto library for it would be
// disproportionate for a content-change hash used only for incremental
// scan/sync (docs/database.md), not for security-sensitive purposes.
namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

constexpr std::uint32_t rotr(std::uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

class Sha256State {
public:
    Sha256State() = default;

    void update(const std::uint8_t* data, std::size_t size) {
        totalBits_ += static_cast<std::uint64_t>(size) * 8;

        while (size > 0) {
            std::size_t take = std::min(size, std::size_t{64} - bufferLen_);
            std::memcpy(buffer_.data() + bufferLen_, data, take);
            bufferLen_ += take;
            data += take;
            size -= take;

            if (bufferLen_ == 64) {
                processBlock(buffer_.data());
                bufferLen_ = 0;
            }
        }
    }

    [[nodiscard]] std::string finalizeHex() {
        std::uint64_t bitLength = totalBits_;

        std::uint8_t padByte = 0x80;
        update(&padByte, 1);

        std::uint8_t zero = 0x00;
        while (bufferLen_ != 56) {
            update(&zero, 1);
        }

        std::array<std::uint8_t, 8> lengthBytes{};
        for (int i = 0; i < 8; ++i) {
            lengthBytes[7 - i] = static_cast<std::uint8_t>(bitLength >> (8 * i));
        }
        // Bypass update()'s bit-length accounting for the length field itself.
        std::memcpy(buffer_.data() + bufferLen_, lengthBytes.data(), 8);
        processBlock(buffer_.data());

        std::ostringstream out;
        out << std::hex << std::setfill('0');
        for (std::uint32_t word : h_) {
            out << std::setw(8) << word;
        }
        return out.str();
    }

private:
    void processBlock(const std::uint8_t* block) {
        std::array<std::uint32_t, 64> w{};
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
                   (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                   (static_cast<std::uint32_t>(block[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
        std::uint32_t e = h_[4], f = h_[5], g = h_[6], hh = h_[7];

        for (int i = 0; i < 64; ++i) {
            std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            std::uint32_t ch = (e & f) ^ (~e & g);
            std::uint32_t temp1 = hh + s1 + ch + kRoundConstants[i] + w[i];
            std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            std::uint32_t temp2 = s0 + maj;

            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d;
        h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += hh;
    }

    std::array<std::uint32_t, 8> h_ = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t bufferLen_ = 0;
    std::uint64_t totalBits_ = 0;
};

} // namespace

namespace musicbox::util {

std::string sha256Bytes(const void* data, std::size_t size) {
    Sha256State state;
    state.update(static_cast<const std::uint8_t*>(data), size);
    return state.finalizeHex();
}

std::string sha256File(const std::string& absolutePath) {
    std::ifstream file(absolutePath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("sha256File: unable to open " + absolutePath);
    }

    Sha256State state;
    std::vector<char> chunk(1 << 16); // 64 KiB — bounded memory regardless of file size
    while (file) {
        file.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            state.update(reinterpret_cast<const std::uint8_t*>(chunk.data()),
                         static_cast<std::size_t>(bytesRead));
        }
    }
    return state.finalizeHex();
}

} // namespace musicbox::util
