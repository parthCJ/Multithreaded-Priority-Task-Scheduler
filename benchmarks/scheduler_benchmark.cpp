#include "task_scheduler.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

std::uint64_t cpu_work(std::uint64_t input) {
    std::uint64_t value = input | 1U;
    for (int i = 0; i < 250000; ++i) {
        value = value * 2862933555777941757ULL + 3037000493ULL;
        value ^= value >> 29;
    }
    return value;
}

double run_benchmark(std::size_t worker_count, int task_count) {
    scheduler::TaskScheduler scheduler(worker_count);
    std::vector<std::future<std::uint64_t>> results;
    results.reserve(task_count);
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < task_count; ++i) {
        results.push_back(scheduler.submit(0, cpu_work, static_cast<std::uint64_t>(i)));
    }
    for (auto& result : results) {
        result.get();
    }
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start);
    return elapsed.count();
}

}  // namespace

int main(int argc, char** argv) {
    const int task_count = argc > 1 ? std::atoi(argv[1]) : 32;
    std::cout << "tasks=" << task_count << "\n";
    for (const std::size_t workers : {1U, 2U, 4U, 8U}) {
        const double seconds = run_benchmark(workers, task_count);
        std::cout << workers << " threads: " << std::fixed << std::setprecision(4)
                  << seconds << " s, " << task_count / seconds << " tasks/s\n";
    }
}
