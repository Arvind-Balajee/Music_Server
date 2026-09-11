#include "musicbox/net/ThreadPool.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <future>
#include <set>
#include <thread>
#include <vector>

TEST_CASE("ThreadPool runs a submitted task and returns its result via future",
          "[net][threadpool]") {
    musicbox::net::ThreadPool pool(2);
    std::future<int> result = pool.submit([] { return 21 * 2; });
    CHECK(result.get() == 42);
}

TEST_CASE("ThreadPool runs many tasks across its worker threads", "[net][threadpool]") {
    constexpr std::size_t kWorkers = 4;
    constexpr int kTasks = 200;

    musicbox::net::ThreadPool pool(kWorkers);
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    futures.reserve(kTasks);

    for (int i = 0; i < kTasks; ++i) {
        futures.push_back(
            pool.submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); }));
    }
    for (auto& f : futures) {
        f.get();
    }

    CHECK(counter.load() == kTasks);
}

TEST_CASE("ThreadPool exceptions in a task propagate through the future", "[net][threadpool]") {
    musicbox::net::ThreadPool pool(1);
    std::future<int> result = pool.submit([]() -> int { throw std::runtime_error("boom"); });
    CHECK_THROWS_AS(result.get(), std::runtime_error);
}

TEST_CASE("ThreadPool destructor drains queued work and joins all workers", "[net][threadpool]") {
    constexpr int kTasks = 50;
    std::atomic<int> completed{0};
    {
        musicbox::net::ThreadPool pool(2);
        for (int i = 0; i < kTasks; ++i) {
            pool.submit([&completed] {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                completed.fetch_add(1, std::memory_order_relaxed);
            });
        }
        // Pool destructs here; per this implementation (see
        // server/src/net/ThreadPool.cpp workerLoop()), workers keep draining
        // the queue after stopping_ is set and only exit once it's empty, so
        // the destructor must not return until every task above has run.
    }
    CHECK(completed.load() == kTasks);
}
