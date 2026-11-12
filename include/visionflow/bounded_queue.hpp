#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace visionflow {

// A fixed-capacity, thread-safe FIFO queue for producer/consumer pipelines.
//
// Producers block while the queue is full (backpressure) and consumers block
// while it is empty. close() ends the stream: no further items are accepted,
// all blocked threads wake up, and items already queued can still be drained.
//
// Items are passed by value and moved in and out. If a push fails (queue full
// or closed), the item passed in is destroyed along with the argument.
template <typename T>
class BoundedQueue {
    static_assert(std::is_move_constructible_v<T>, "BoundedQueue<T> requires a movable T");
    static_assert(std::is_move_assignable_v<T>, "BoundedQueue<T> requires a move-assignable T");

public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) {
            throw std::invalid_argument("BoundedQueue capacity must be greater than 0");
        }
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;
    BoundedQueue(BoundedQueue&&) = delete;
    BoundedQueue& operator=(BoundedQueue&&) = delete;

    ~BoundedQueue() = default;

    // Blocks until there is space or the queue is closed. Returns false (and
    // discards `value`) if the queue was closed before the item was inserted.
    bool push(T value) {
        {
            std::unique_lock lock(mutex_);
            notFull_.wait(lock, [this] { return closed_ || items_.size() < capacity_; });
            if (closed_) {
                return false;
            }
            items_.push_back(std::move(value));
        }
        notEmpty_.notify_one();
        return true;
    }

    // Blocks until an item is available or the queue is closed. Items queued
    // before close() are still returned; returns false only once the queue is
    // closed and empty.
    bool pop(T& value) {
        {
            std::unique_lock lock(mutex_);
            notEmpty_.wait(lock, [this] { return closed_ || !items_.empty(); });
            if (items_.empty()) {
                return false;
            }
            value = std::move(items_.front());
            items_.pop_front();
        }
        notFull_.notify_one();
        return true;
    }

    // Returns false immediately (and discards `value`) if full or closed.
    bool try_push(T value) {
        {
            std::lock_guard lock(mutex_);
            if (closed_ || items_.size() >= capacity_) {
                return false;
            }
            items_.push_back(std::move(value));
        }
        notEmpty_.notify_one();
        return true;
    }

    // Returns false immediately if empty. Still drains items after close().
    bool try_pop(T& value) {
        {
            std::lock_guard lock(mutex_);
            if (items_.empty()) {
                return false;
            }
            value = std::move(items_.front());
            items_.pop_front();
        }
        notFull_.notify_one();
        return true;
    }

    // Stops accepting items and wakes every blocked producer and consumer.
    // Idempotent.
    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        notFull_.notify_all();
        notEmpty_.notify_all();
    }

    [[nodiscard]] bool is_closed() const {
        std::lock_guard lock(mutex_);
        return closed_;
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock(mutex_);
        return items_.size();
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return items_.empty();
    }

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable notFull_;
    std::condition_variable notEmpty_;
    std::deque<T> items_;
    bool closed_{false};
};

}  
