#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace scheduler {

class Task {
public:
    virtual ~Task() = default;
    virtual void run() = 0;
};

struct SchedulerStats {
    std::uint64_t submitted = 0;
    std::uint64_t completed = 0;
    std::uint64_t active = 0;
    std::uint64_t queued = 0;
};

class TaskScheduler {
public:
    explicit TaskScheduler(std::size_t worker_count);
    ~TaskScheduler();

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    template <typename Function, typename... Args>
    auto submit(int priority, Function&& function, Args&&... args)
        -> std::future<typename std::result_of<Function(Args...)>::type>;

    void shutdown();
    SchedulerStats stats() const;
    std::size_t worker_count() const noexcept { return workers_.size(); }

private:
    struct QueuedTask {
        int priority;
        std::uint64_t sequence;
        std::shared_ptr<Task> task;
    };

    struct QueueOrder {
        bool operator()(const QueuedTask& left, const QueuedTask& right) const noexcept {
            if (left.priority != right.priority) {
                return left.priority < right.priority;
            }
            return left.sequence > right.sequence;
        }
    };

    void worker_loop();

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::priority_queue<QueuedTask, std::vector<QueuedTask>, QueueOrder> queue_;
    std::vector<std::thread> workers_;
    bool stopping_ = false;
    std::uint64_t next_sequence_ = 0;
    std::atomic<std::uint64_t> submitted_{0};
    std::atomic<std::uint64_t> completed_{0};
    std::atomic<std::uint64_t> active_{0};
};

template <typename Result>
class FunctionTask final : public Task {
public:
    explicit FunctionTask(std::packaged_task<Result()>&& task) : task_(std::move(task)) {}

    void run() override { task_(); }

private:
    std::packaged_task<Result()> task_;
};

template <typename Function, typename... Args>
auto TaskScheduler::submit(int priority, Function&& function, Args&&... args)
    -> std::future<typename std::result_of<Function(Args...)>::type> {
    using Result = typename std::result_of<Function(Args...)>::type;
    auto bound = [function = std::forward<Function>(function),
                  tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable -> Result {
        return std::apply(std::move(function), std::move(tuple));
    };
    std::packaged_task<Result()> packaged(std::move(bound));
    auto result = packaged.get_future();
    auto task = std::make_shared<FunctionTask<Result>>(std::move(packaged));

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            throw std::runtime_error("cannot submit a task after shutdown");
        }
        queue_.push(QueuedTask{priority, next_sequence_++, std::move(task)});
        submitted_.fetch_add(1, std::memory_order_relaxed);
    }
    condition_.notify_one();
    return result;
}

}  // namespace scheduler
