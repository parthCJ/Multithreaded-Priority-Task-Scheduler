#include "task_scheduler.h"

#include <stdexcept>

namespace scheduler {

TaskScheduler::TaskScheduler(std::size_t worker_count) {
    if (worker_count == 0) {
        throw std::invalid_argument("worker count must be greater than zero");
    }
    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back(&TaskScheduler::worker_loop, this);
    }
}

TaskScheduler::~TaskScheduler() {
    shutdown();
}

void TaskScheduler::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            return;
        }
        stopping_ = true;
    }
    condition_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

SchedulerStats TaskScheduler::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return SchedulerStats{
        submitted_.load(std::memory_order_relaxed),
        completed_.load(std::memory_order_relaxed),
        active_.load(std::memory_order_relaxed),
        static_cast<std::uint64_t>(queue_.size())};
}

void TaskScheduler::worker_loop() {
    while (true) {
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) {
                return;
            }
            task = std::move(queue_.top().task);
            queue_.pop();
            active_.fetch_add(1, std::memory_order_relaxed);
        }

        task->run();
        active_.fetch_sub(1, std::memory_order_relaxed);
        completed_.fetch_add(1, std::memory_order_relaxed);
    }
}

}  // namespace scheduler
