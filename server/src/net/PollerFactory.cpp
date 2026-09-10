// Compile-time platform selection for Poller, per docs/networking.md: this is
// the one place that decides EpollPoller vs. KqueuePoller vs. SelectPoller.
// No other file in this codebase should #ifdef on __linux__/__APPLE__ to pick
// a poller implementation.
#include "musicbox/net/Poller.hpp"

#include "EpollPoller.hpp"
#include "KqueuePoller.hpp"
#include "SelectPoller.hpp"

namespace musicbox::net {

std::unique_ptr<Poller> makePlatformPoller() {
#if defined(__linux__)
    return std::make_unique<EpollPoller>();
#elif defined(MUSICBOX_HAS_KQUEUE)
    return std::make_unique<KqueuePoller>();
#else
    return std::make_unique<SelectPoller>();
#endif
}

} // namespace musicbox::net
