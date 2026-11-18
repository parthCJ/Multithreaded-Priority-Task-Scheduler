cmake_minimum_required(VERSION 3.16)
project(priority_scheduler VERSION 1.0 LANGUAGES CXX)

option(BUILD_TESTING "Build unit tests" ON)
option(BUILD_BENCHMARK "Build the benchmark executable" ON)
option(ENABLE_TSAN "Enable ThreadSanitizer for supported compilers" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(task_scheduler
    src/task_scheduler.cpp
)
target_include_directories(task_scheduler PUBLIC include)
if (MSVC)
    target_compile_options(task_scheduler PRIVATE /W4 /permissive-)
else()
    target_compile_options(task_scheduler PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (ENABLE_TSAN AND NOT MSVC)
    target_compile_options(task_scheduler PUBLIC -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(task_scheduler PUBLIC -fsanitize=thread)
endif()

if (BUILD_TESTING)
    include(CTest)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(task_scheduler_tests tests/task_scheduler_tests.cpp)
    target_link_libraries(task_scheduler_tests PRIVATE task_scheduler GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(task_scheduler_tests)
endif()

if (BUILD_BENCHMARK)
    add_executable(scheduler_benchmark benchmarks/scheduler_benchmark.cpp)
    target_link_libraries(scheduler_benchmark PRIVATE task_scheduler)
endif()
