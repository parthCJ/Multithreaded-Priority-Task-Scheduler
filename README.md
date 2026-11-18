# Priority Task Scheduler

A C++17 thread pool that schedules polymorphic tasks by priority. Workers use
`std::thread`, a mutex-protected `std::priority_queue`, and a condition
variable. Submission is `O(log n)` and extraction is `O(log n)`.

## Build

```powershell
cmake -S . -B build -DBUILD_TESTING=ON -DBUILD_BENCHMARK=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

GoogleTest is downloaded by CMake's `FetchContent` on the first configure.

## Benchmark

The benchmark runs the same CPU-heavy workload with 1, 2, 4, and 8 workers:

```powershell
.\build\Release\scheduler_benchmark.exe 32
```

## ThreadSanitizer

On GCC or Clang, configure a separate build with:

```powershell
cmake -S . -B build-tsan -DENABLE_TSAN=ON
cmake --build build-tsan
ctest --test-dir build-tsan --output-on-failure
```

`TaskScheduler::shutdown()` stops accepting work, drains queued tasks, and
joins every worker. Exceptions are delivered through each task's future.
