#include "task_scheduler.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using scheduler::TaskScheduler;

TEST(TaskSchedulerTest, RunsTasksAndReturnsResults) {
    TaskScheduler scheduler(2);
    auto first = scheduler.submit(1, [](int value) { return value * 2; }, 21);
    auto second = scheduler.submit(1, [] { return 7; });

    EXPECT_EQ(first.get(), 42);
    EXPECT_EQ(second.get(), 7);
    scheduler.shutdown();

    const auto stats = scheduler.stats();
    EXPECT_EQ(stats.submitted, 2);
    EXPECT_EQ(stats.completed, 2);
    EXPECT_EQ(stats.queued, 0);
    EXPECT_EQ(stats.active, 0);
}

TEST(TaskSchedulerTest, HigherPriorityRunsFirstWhenTasksAreQueued) {
    TaskScheduler scheduler(1);
    std::promise<void> release;
    auto gate = release.get_future().share();
    std::promise<void> started;
    auto started_future = started.get_future();
    std::vector<int> order;
    std::mutex order_mutex;

    auto blocker = scheduler.submit(0, [&] {
        started.set_value();
        gate.wait();
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(0);
    });
    started_future.wait();
    auto low = scheduler.submit(1, [&] {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(1);
    });
    auto high = scheduler.submit(10, [&] {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(10);
    });

    release.set_value();
    blocker.get();
    low.get();
    high.get();
    EXPECT_EQ(order, (std::vector<int>{0, 10, 1}));
}

TEST(TaskSchedulerTest, PropagatesTaskExceptions) {
    TaskScheduler scheduler(1);
    auto result = scheduler.submit(1, []() -> int {
        throw std::runtime_error("task failed");
    });

    EXPECT_THROW(result.get(), std::runtime_error);
}

TEST(TaskSchedulerTest, RejectsSubmissionAfterShutdown) {
    TaskScheduler scheduler(1);
    scheduler.shutdown();

    EXPECT_THROW(scheduler.submit(1, [] {}), std::runtime_error);
}

TEST(TaskSchedulerTest, ExecutesTasksConcurrently) {
    TaskScheduler scheduler(4);
    std::atomic<int> running{0};
    std::atomic<int> maximum{0};
    std::vector<std::future<void>> results;
    for (int i = 0; i < 4; ++i) {
        results.push_back(scheduler.submit(1, [&] {
            const int now = running.fetch_add(1) + 1;
            int previous = maximum.load();
            while (now > previous && !maximum.compare_exchange_weak(previous, now)) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            running.fetch_sub(1);
        }));
    }
    for (auto& result : results) {
        result.get();
    }
    EXPECT_GT(maximum.load(), 1);
}
