#include "musicbox/util/Sha256.hpp"

#include <openssl/evp.h>

#include <array>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

// Backed by OpenSSL's EVP digest API rather than a portable C++ implementation
// because EVP dispatches to the CPU's SHA-256 instructions where present
// (ARMv8 crypto extensions, x86 SHA-NI) -- see
// docs/adr/0010-use-openssl-for-sha256.md.
namespace {

struct EvpMdCtxDeleter {
    void operator()(EVP_MD_CTX* ctx) const noexcept { EVP_MD_CTX_free(ctx); }
};

class Sha256Hasher {
public:
    Sha256Hasher() : ctx_(EVP_MD_CTX_new()) {
        if (!ctx_ || EVP_DigestInit_ex(ctx_.get(), EVP_sha256(), nullptr) != 1) {
            throw std::runtime_error("sha256: OpenSSL digest initialization failed");
        }
    }

    void update(const void* data, std::size_t size) {
        if (EVP_DigestUpdate(ctx_.get(), data, size) != 1) {
            throw std::runtime_error("sha256: OpenSSL digest update failed");
        }
    }

    [[nodiscard]] std::string finalizeHex() {
        std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
        unsigned int digestLength = 0;
        if (EVP_DigestFinal_ex(ctx_.get(), digest.data(), &digestLength) != 1) {
            throw std::runtime_error("sha256: OpenSSL digest finalization failed");
        }

        static constexpr char kHexDigits[] = "0123456789abcdef";
        std::string hex;
        hex.reserve(static_cast<std::size_t>(digestLength) * 2);
        for (unsigned int i = 0; i < digestLength; ++i) {
            hex.push_back(kHexDigits[digest[i] >> 4]);
            hex.push_back(kHexDigits[digest[i] & 0x0f]);
        }
        return hex;
    }

private:
    std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter> ctx_;
};

} // namespace

namespace musicbox::util {

std::string sha256Bytes(const void* data, std::size_t size) {
    Sha256Hasher hasher;
    hasher.update(data, size);
    return hasher.finalizeHex();
}

std::string sha256File(const std::string& absolutePath) {
    std::ifstream file(absolutePath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("sha256File: unable to open " + absolutePath);
    }

    Sha256Hasher hasher;
    std::vector<char> chunk(1 << 16); // 64 KiB — bounded memory regardless of file size
    while (file) {
        file.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        const std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            hasher.update(chunk.data(), static_cast<std::size_t>(bytesRead));
        }
    }
    return hasher.finalizeHex();
}

} // namespace musicbox::util
