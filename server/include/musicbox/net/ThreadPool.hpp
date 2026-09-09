#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace musicbox::net {

// A fixed-size pool of worker threads draining a shared task queue. Used to move
// blocking work (SQLite queries, stat(), metadata parsing) off the event loop
// thread. See docs/architecture.md §4 and docs/networking.md.
//
// Results must be marshalled back to the event loop thread by the caller (e.g. via
// a lock-protected completion queue plus an eventfd/self-pipe that wakes the
// poller) — this class only runs arbitrary callables on worker threads and does
// not itself touch sockets.
class ThreadPool {
public:
    explicit ThreadPool(std::size_t threadCount);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Submits a task and returns a future for its result.
    template <typename Fn, typename R = std::invoke_result_t<Fn>>
    std::future<R> submit(Fn task) {
        auto packaged = std::make_shared<std::packaged_task<R()>>(std::move(task));
        std::future<R> result = packaged->get_future();
        enqueue([packaged] { (*packaged)(); });
        return result;
    }

private:
    void enqueue(std::function<void()> task);
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool stopping_ = false;
};

} // namespace musicbox::net
