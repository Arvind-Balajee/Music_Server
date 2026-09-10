#include "musicbox/sync/FileHasher.hpp"

#include "musicbox/util/Sha256.hpp"

namespace musicbox::sync {

std::string Sha256FileHasher::hashFile(const std::string& absolutePath) const {
    return musicbox::util::sha256File(absolutePath);
}

} // namespace musicbox::sync
