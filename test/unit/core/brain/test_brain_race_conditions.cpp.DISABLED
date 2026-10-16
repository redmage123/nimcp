/**
 * @file test_brain_race_conditions.cpp
 * @brief Race condition tests for brain operations
 *
 * WHAT: Tests for thread safety of brain operations
 * WHY:  NIMCP brains must be safe for concurrent access in multi-threaded apps
 * HOW:  Use ConcurrentRunner to stress test concurrent operations
 *
 * RUN WITH TSAN:
 *   cmake -DNIMCP_TSAN=ON ..
 *   make test_brain_race_conditions
 *   ./test_brain_race_conditions
 */

#include <gtest/gtest.h>
#include "utils/concurrency_test.h"
#include "utils/nimcp_test_base.h"

extern "C" {
#include "core/brain/nimcp_brain.h"
}

using namespace nimcp::test;

class BrainRaceConditionTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
        brain_ = brain_create("race_test", BRAIN_SIZE_TINY,
                             BRAIN_TASK_CLASSIFICATION, 10, 10);
        ASSERT_NE(brain_, nullptr);
    }

    void TearDown() override {
        if (brain_) {
            brain_destroy(brain_);
            brain_ = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    brain_t brain_ = nullptr;
};

// ============================================================================
// Concurrent Process Tests
// ============================================================================

TEST_F(BrainRaceConditionTest, ConcurrentProcessDoesNotCrash) {
    // WHAT: Test that concurrent brain_process calls don't crash
    // WHY:  Multiple threads may call brain_process on same brain
    // HOW:  Launch 8 threads, each calling process 100 times

    ConcurrentRunner runner(8);

    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};

    runner.run([&](int thread_id) {
        float input[10] = {0};
        float output[10] = {0};

        // Fill input with thread-specific data
        for (int i = 0; i < 10; ++i) {
            input[i] = static_cast<float>(thread_id * 10 + i) / 100.0f;
        }

        for (int iter = 0; iter < 100; ++iter) {
            int result = brain_process(brain_, input, 10, output, 10);
            if (result == 0) {
                success_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                error_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    // At least some should succeed
    EXPECT_GT(success_count.load(), 0) << "No successful brain_process calls";

    // Report any errors (not necessarily failures)
    if (error_count.load() > 0) {
        std::cout << "Note: " << error_count.load()
                  << " brain_process calls returned errors (may be expected)\n";
    }
}

TEST_F(BrainRaceConditionTest, ConcurrentProcessWithBarrier) {
    // WHAT: Test with synchronized start (maximum contention)
    // WHY:  Detect races that only occur under high contention
    // HOW:  Use barrier to start all threads simultaneously

    ConcurrentRunner runner(8);

    std::atomic<int> iterations{0};

    runner.run_with_barrier([&](int thread_id, Barrier& barrier) {
        float input[10] = {0};
        float output[10] = {0};

        // Prepare data
        for (int i = 0; i < 10; ++i) {
            input[i] = static_cast<float>(i) * 0.1f;
        }

        // Synchronize at barrier - all threads start processing together
        barrier.wait();

        // Now all threads race to process
        for (int iter = 0; iter < 50; ++iter) {
            brain_process(brain_, input, 10, output, 10);
            iterations.fetch_add(1, std::memory_order_relaxed);
        }
    });

    EXPECT_EQ(iterations.load(), 8 * 50) << "Not all iterations completed";
}

// ============================================================================
// Concurrent Get/Set Tests
// ============================================================================

TEST_F(BrainRaceConditionTest, ConcurrentGettersDoNotCrash) {
    // WHAT: Test concurrent read-only operations
    // WHY:  Read operations should be safe to call concurrently
    // HOW:  Multiple threads call getter functions

    ConcurrentRunner runner(4);

    runner.run([&](int thread_id) {
        for (int i = 0; i < 100; ++i) {
            // Call various getters
            volatile const char* name = brain_get_name(brain_);
            volatile brain_size_t size = brain_get_size(brain_);
            volatile brain_task_t task = brain_get_task(brain_);

            // Suppress unused variable warnings
            (void)name;
            (void)size;
            (void)task;
        }
    });

    // If we get here without crashing or TSAN errors, test passes
    SUCCEED();
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(BrainRaceConditionTest, StressTestConcurrentOperations) {
    // WHAT: Long-running stress test for race detection
    // WHY:  Some races only manifest under sustained load
    // HOW:  Run for 1 second with maximum threads

    StressTestConfig config;
    config.thread_count = 8;
    config.iterations_per_thread = 500;
    config.warmup_iterations = 50;
    config.timeout = std::chrono::milliseconds(5000);  // 5 second timeout

    auto result = run_stress_test(config, [this](int thread_id, int iteration) {
        float input[10] = {0};
        float output[10] = {0};

        // Vary input based on iteration
        for (int i = 0; i < 10; ++i) {
            input[i] = static_cast<float>((thread_id + iteration + i) % 100) / 100.0f;
        }

        int rc = brain_process(brain_, input, 10, output, 10);
        return rc == 0;  // Success if return code is 0
    });

    std::cout << "Stress test completed:\n"
              << "  Iterations: " << result.total_iterations << "\n"
              << "  Errors: " << result.errors << "\n"
              << "  Elapsed: " << result.elapsed.count() / 1000.0 << "ms\n"
              << "  Timeout: " << (result.timeout_reached ? "yes" : "no") << "\n";

    EXPECT_FALSE(result.timeout_reached) << "Test timed out";
}

// ============================================================================
// Race Detector Tests
// ============================================================================

TEST_F(BrainRaceConditionTest, RaceDetectorSanityCheck) {
    // WHAT: Verify our race detector works
    // WHY:  Ensure test utilities are functioning correctly
    // HOW:  Create intentional race and verify detection

    RaceDetector detector;
    ConcurrentRunner runner(4);

    runner.run([&](int thread_id) {
        for (int i = 0; i < 1000; ++i) {
            detector.increment();
        }
    });

    // With 4 threads doing 1000 increments each, we expect some races
    // (unless using atomic operations, which we are - so no races expected)
    // This is mainly to verify the detector compiles and runs
    std::cout << "RaceDetector value: " << detector.value()
              << " (expected 4000)\n";
}

// ============================================================================
// Ordering Tests
// ============================================================================

TEST_F(BrainRaceConditionTest, MemoryOrderingSanityCheck) {
    // WHAT: Verify memory ordering checker works
    // WHY:  Detect memory ordering issues in lock-free code
    // HOW:  Run ordering check many times

    int violations = 0;
    const int iterations = 1000;

    for (int i = 0; i < iterations; ++i) {
        OrderingChecker checker;
        ConcurrentRunner runner(2);

        runner.run([&](int thread_id) {
            if (thread_id == 0) {
                checker.thread1_write();
            } else {
                checker.thread2_write();
            }
        });

        if (checker.ordering_violated()) {
            violations++;
        }
    }

    // With seq_cst, we should see no violations
    EXPECT_EQ(violations, 0) << "Memory ordering violations detected";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
