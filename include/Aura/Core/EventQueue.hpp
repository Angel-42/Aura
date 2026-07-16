#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstddef>

namespace Aura::Core {

template<typename T>
class EventQueue {
public:
    void push(T event) {
        {
            std::lock_guard lock(mutex_);
            queue_.push(std::move(event));
        }
        cv_.notify_one();
    }

    bool tryPop(T& out) {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    T waitPop() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty(); });
        T val = std::move(queue_.front());
        queue_.pop();
        return val;
    }

    void clear() {
        std::lock_guard lock(mutex_);
        while (!queue_.empty()) queue_.pop();
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

private:
    std::queue<T>           queue_;
    mutable std::mutex      mutex_;
    std::condition_variable cv_;
};

} // namespace Aura::Core
