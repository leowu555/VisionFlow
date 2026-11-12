#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "visionflow/bounded_queue.hpp"
#include "visionflow/frame.hpp"

namespace {

using visionflow::BoundedQueue;

// Minimal test harness: a failed CHECK throws, which fails the current test.
struct CheckFailure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            throw CheckFailure(std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                               ": CHECK(" #condition ") failed");                          \
        }                                                                                   \
    } while (false)

// How long to wait before asserting that a thread is still blocked. Blocking
// can't be proven without waiting; if this is too short the "still blocked"
// check passes trivially, but it can never cause a false failure.
constexpr auto kBlockGracePeriod = std::chrono::milliseconds(100);

// Owns test threads. On scope exit (including a failed CHECK) it closes the
// queue to unblock them and joins, so a failure is reported instead of
// std::terminate being called on a joinable thread.
template <typename Queue>
class ThreadGroup {
public:
    explicit ThreadGroup(Queue& queue) : queue_(queue) {}
    ~ThreadGroup() {
        queue_.close();
        joinAll();
    }
    ThreadGroup(const ThreadGroup&) = delete;
    ThreadGroup& operator=(const ThreadGroup&) = delete;

    template <typename F>
    void spawn(F&& f) {
        threads_.emplace_back(std::forward<F>(f));
    }

    void joinAll() {
        for (auto& t : threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

private:
    Queue& queue_;
    std::vector<std::thread> threads_;
};

void testConstructorRejectsZeroCapacity() {
    bool threw = false;
    try {
        BoundedQueue<int> queue(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

void testFifoOrdering() {
    BoundedQueue<int> queue(5);
    for (int i = 0; i < 5; ++i) {
        CHECK(queue.push(i));
    }
    for (int i = 0; i < 5; ++i) {
        int value = -1;
        CHECK(queue.pop(value));
        CHECK(value == i);
    }
}

void testSizeCapacityEmpty() {
    BoundedQueue<int> queue(3);
    CHECK(queue.capacity() == 3);
    CHECK(queue.size() == 0);
    CHECK(queue.empty());

    CHECK(queue.push(1));
    CHECK(queue.size() == 1);
    CHECK(!queue.empty());

    CHECK(queue.push(2));
    CHECK(queue.push(3));
    CHECK(queue.size() == 3);
    CHECK(queue.capacity() == 3);

    int value = 0;
    CHECK(queue.pop(value));
    CHECK(queue.size() == 2);
}

void testTryPushFailsWhenFull() {
    BoundedQueue<int> queue(2);
    CHECK(queue.try_push(1));
    CHECK(queue.try_push(2));
    CHECK(!queue.try_push(3));
    CHECK(queue.size() == 2);
}

void testTryPopFailsWhenEmpty() {
    BoundedQueue<int> queue(2);
    int value = 42;
    CHECK(!queue.try_pop(value));
    CHECK(value == 42);

    CHECK(queue.try_push(7));
    CHECK(queue.try_pop(value));
    CHECK(value == 7);
    CHECK(!queue.try_pop(value));
}

void testPushBlocksWhenFullAndWakesOnPop() {
    BoundedQueue<int> queue(1);
    CHECK(queue.push(1));

    std::atomic<bool> pushReturned{false};
    std::atomic<bool> pushResult{false};
    ThreadGroup threads(queue);
    threads.spawn([&] {
        pushResult = queue.push(2);
        pushReturned = true;
    });

    std::this_thread::sleep_for(kBlockGracePeriod);
    CHECK(!pushReturned);

    int value = 0;
    CHECK(queue.pop(value));
    CHECK(value == 1);

    threads.joinAll();
    CHECK(pushReturned);
    CHECK(pushResult);
    CHECK(queue.pop(value));
    CHECK(value == 2);
}

void testPopBlocksWhenEmptyAndWakesOnPush() {
    BoundedQueue<int> queue(1);

    std::atomic<bool> popReturned{false};
    std::atomic<bool> popResult{false};
    int popped = 0;
    ThreadGroup threads(queue);
    threads.spawn([&] {
        popResult = queue.pop(popped);
        popReturned = true;
    });

    std::this_thread::sleep_for(kBlockGracePeriod);
    CHECK(!popReturned);

    CHECK(queue.push(99));

    threads.joinAll();
    CHECK(popReturned);
    CHECK(popResult);
    CHECK(popped == 99);
}

void testCloseWakesBlockedConsumers() {
    BoundedQueue<int> queue(1);
    constexpr int kConsumers = 4;

    std::atomic<int> returned{0};
    std::atomic<int> succeeded{0};
    ThreadGroup threads(queue);
    for (int i = 0; i < kConsumers; ++i) {
        threads.spawn([&] {
            int value = 0;
            if (queue.pop(value)) {
                ++succeeded;
            }
            ++returned;
        });
    }

    std::this_thread::sleep_for(kBlockGracePeriod);
    CHECK(returned == 0);

    queue.close();
    threads.joinAll();
    CHECK(returned == kConsumers);
    CHECK(succeeded == 0);
}

void testCloseWakesBlockedProducers() {
    BoundedQueue<int> queue(1);
    CHECK(queue.push(0));
    constexpr int kProducers = 4;

    std::atomic<int> returned{0};
    std::atomic<int> succeeded{0};
    ThreadGroup threads(queue);
    for (int i = 0; i < kProducers; ++i) {
        threads.spawn([&, i] {
            if (queue.push(i + 1)) {
                ++succeeded;
            }
            ++returned;
        });
    }

    std::this_thread::sleep_for(kBlockGracePeriod);
    CHECK(returned == 0);

    queue.close();
    threads.joinAll();
    CHECK(returned == kProducers);
    CHECK(succeeded == 0);
    CHECK(queue.size() == 1);
}

void testDrainAfterClose() {
    BoundedQueue<int> queue(4);
    CHECK(queue.push(1));
    CHECK(queue.push(2));
    CHECK(queue.push(3));
    queue.close();
    CHECK(queue.is_closed());

    int value = 0;
    CHECK(queue.pop(value));
    CHECK(value == 1);
    CHECK(queue.try_pop(value));
    CHECK(value == 2);
    CHECK(queue.pop(value));
    CHECK(value == 3);
    CHECK(queue.empty());
}

void testPopReturnsFalseWhenClosedAndEmpty() {
    BoundedQueue<int> queue(2);
    CHECK(queue.push(1));
    queue.close();

    int value = 0;
    CHECK(queue.pop(value));
    CHECK(value == 1);
    value = -1;
    CHECK(!queue.pop(value));
    CHECK(!queue.try_pop(value));
    CHECK(value == -1);
}

void testNoPushAfterClose() {
    BoundedQueue<int> queue(4);
    CHECK(queue.push(1));
    queue.close();

    CHECK(!queue.push(2));
    CHECK(!queue.try_push(3));
    CHECK(queue.size() == 1);
}

void testCloseIsIdempotent() {
    BoundedQueue<int> queue(2);
    CHECK(!queue.is_closed());
    queue.close();
    queue.close();
    CHECK(queue.is_closed());
    CHECK(!queue.push(1));
}

void testMoveOnlyType() {
    BoundedQueue<std::unique_ptr<int>> queue(2);
    CHECK(queue.push(std::make_unique<int>(5)));
    CHECK(queue.try_push(std::make_unique<int>(6)));

    std::unique_ptr<int> value;
    CHECK(queue.pop(value));
    CHECK(value && *value == 5);
    CHECK(queue.try_pop(value));
    CHECK(value && *value == 6);
}

void testFrameIsMovedWithoutCopyingPixels() {
    BoundedQueue<visionflow::Frame> queue(2);

    visionflow::Frame in;
    in.id = 17;
    in.timestamp = visionflow::Clock::now();
    in.image = cv::Mat(4, 4, CV_8UC3, cv::Scalar(1, 2, 3));
    const auto* pixels = in.image.data;
    const auto timestamp = in.timestamp;

    CHECK(queue.push(std::move(in)));

    visionflow::Frame out;
    CHECK(queue.pop(out));
    CHECK(out.id == 17);
    CHECK(out.timestamp == timestamp);
    CHECK(out.image.data == pixels);
}

void testManyProducersManyConsumers() {
    BoundedQueue<int> queue(8);
    constexpr int kProducers = 4;
    constexpr int kConsumers = 4;
    constexpr int kItemsPerProducer = 10000;

    std::atomic<long long> sum{0};
    std::atomic<int> count{0};
    std::atomic<int> failedPushes{0};

    ThreadGroup consumers(queue);
    for (int i = 0; i < kConsumers; ++i) {
        consumers.spawn([&] {
            int value = 0;
            while (queue.pop(value)) {
                sum += value;
                ++count;
            }
        });
    }

    {
        ThreadGroup producers(queue);
        for (int p = 0; p < kProducers; ++p) {
            producers.spawn([&] {
                for (int i = 1; i <= kItemsPerProducer; ++i) {
                    if (!queue.push(i)) {
                        ++failedPushes;
                    }
                }
            });
        }
        producers.joinAll();
    }  // Closes the queue once every producer has finished.
    consumers.joinAll();

    constexpr long long kExpectedSum =
        static_cast<long long>(kProducers) * kItemsPerProducer * (kItemsPerProducer + 1) / 2;
    CHECK(failedPushes == 0);
    CHECK(count == kProducers * kItemsPerProducer);
    CHECK(sum == kExpectedSum);
    CHECK(queue.empty());
}

struct TestCase {
    const char* name;
    std::function<void()> run;
};

}  // namespace

int main() {
    const std::vector<TestCase> tests = {
        {"constructor rejects capacity 0", testConstructorRejectsZeroCapacity},
        {"FIFO ordering", testFifoOrdering},
        {"size/capacity/empty", testSizeCapacityEmpty},
        {"try_push fails when full", testTryPushFailsWhenFull},
        {"try_pop fails when empty", testTryPopFailsWhenEmpty},
        {"push blocks when full and wakes on pop", testPushBlocksWhenFullAndWakesOnPop},
        {"pop blocks when empty and wakes on push", testPopBlocksWhenEmptyAndWakesOnPush},
        {"close wakes blocked consumers", testCloseWakesBlockedConsumers},
        {"close wakes blocked producers", testCloseWakesBlockedProducers},
        {"remaining items drain after close", testDrainAfterClose},
        {"pop returns false when closed and empty", testPopReturnsFalseWhenClosedAndEmpty},
        {"no push after close", testNoPushAfterClose},
        {"close is idempotent", testCloseIsIdempotent},
        {"move-only type", testMoveOnlyType},
        {"Frame is moved without copying pixels", testFrameIsMovedWithoutCopyingPixels},
        {"many producers, many consumers", testManyProducersManyConsumers},
    };

    int failures = 0;
    for (const auto& test : tests) {
        // Flushed so that if a test hangs, the timeout output shows which one.
        std::cout << "[RUN ] " << test.name << std::endl;
        try {
            test.run();
            std::cout << "[PASS] " << test.name << std::endl;
        } catch (const std::exception& e) {
            ++failures;
            std::cout << "[FAIL] " << test.name << ": " << e.what() << std::endl;
        }
    }

    std::cout << (tests.size() - static_cast<std::size_t>(failures)) << "/" << tests.size()
              << " tests passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
