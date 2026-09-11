#pragma once

#include <atomic>
#include <string>

namespace musicbox::test {

// A fresh SQLite shared-cache in-memory database URI, unique per call, so
// parallel/sequential test cases never collide (see SqliteConnection.hpp's
// "cache=shared" doc comment). Pass the same returned string to every
// repository constructed within one test case that needs to see the same
// logical database; a different call (or a different test) gets its own
// isolated database.
inline std::string uniqueMemoryDbUri() {
    static std::atomic<int> counter{0};
    return "file:musicbox_test_" + std::to_string(counter.fetch_add(1)) +
           "?mode=memory&cache=shared";
}

} // namespace musicbox::test
