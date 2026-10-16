/**
 * @file test_auto_activity_history_regression.cpp
 * @brief Regression tests for auto activity history performance and stability
 *
 * TEST COVERAGE:
 * - Sampling performance overhead
 * - History buffer memory usage
 * - Long-running stability
 * - Memory leak detection
 * - Thread safety under load
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AutoActivityHistoryRegressionTest : public ::testing::Test {
protected:
    brain_t brain;
    introspection_context_t context;

    void SetUp() override {
        brain = brain_create(
            "test_regression",
            BRAIN_SIZE_MEDIUM,  // Larger for performance testing
            BRAIN_TASK_CLASSIFICATION,
            50,  // num_inputs
            10   // num_outputs
        );

        if (brain != nullptr) {
            introspection_config_t config = introspection_default_config();
            config.enable_auto_history = true;
            config.history_sample_interval_ms = 10;  // High frequency
            config.history_change_threshold = 0.0F;
            config.history_size = 1000;  // Large buffer

            context = introspection_context_create(brain, &config);
        } else {
            context = nullptr;
        }
    }

    void TearDown() override {
        if (context != nullptr) {
            introspection_context_destroy(context);
            context = nullptr;
        }
        if (brain != nullptr) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// 1. Performance Tests
//=============================================================================

TEST_F(AutoActivityHistoryRegressionTest, SamplingOverheadAcceptable) {
    if (context == nullptr) GTEST_SKIP();

    const int num_samples = 1000;
    uint64_t start_time = nimcp_time_monotonic_us();

    for (int i = 0; i < num_samples; i++) {
        introspection_sample_activity(context);
    }

    uint64_t elapsed_us = nimcp_time_elapsed_us(start_time);
    double avg_time_us = (double)elapsed_us / num_samples;

    // Sampling should be fast (< 100us per sample for MEDIUM network)
    EXPECT_LT(avg_time_us, 100.0);

    // Log performance for monitoring
    std::cout << "Average sampling time: " << avg_time_us << " us/sample\n";
}

TEST_F(AutoActivityHistoryRegressionTest, SamplingScalesLinearly) {
    if (context == nullptr) GTEST_SKIP();

    std::vector<int> sample_counts = {100, 500, 1000};
    std::vector<double> times_per_sample;

    for (int count : sample_counts) {
        introspection_clear_history(context);

        uint64_t start = nimcp_time_monotonic_us();
        for (int i = 0; i < count; i++) {
            introspection_sample_activity(context);
        }
        uint64_t elapsed = nimcp_time_elapsed_us(start);

        double time_per_sample = (double)elapsed / count;
        times_per_sample.push_back(time_per_sample);
    }

    // Time per sample should be relatively constant (within 2x)
    double ratio = times_per_sample.back() / times_per_sample.front();
    EXPECT_LT(ratio, 2.0);

    std::cout << "Scaling: 100 samples: " << times_per_sample[0] << " us/sample, "
              << "1000 samples: " << times_per_sample.back() << " us/sample\n";
}

//=============================================================================
// 2. Memory Usage Tests
//=============================================================================

TEST_F(AutoActivityHistoryRegressionTest, MemoryUsageStable) {
    if (context == nullptr) GTEST_SKIP();

    // Fill buffer to capacity
    uint32_t capacity, size_before;
    float util;
    introspection_get_history_stats(context, &size_before, &capacity, &util);

    // Fill to capacity
    for (uint32_t i = 0; i < capacity; i++) {
        introspection_sample_activity(context);
    }

    uint32_t size_after;
    introspection_get_history_stats(context, &size_after, &capacity, &util);

    // Continue sampling - should not grow beyond capacity
    for (int i = 0; i < 500; i++) {
        introspection_sample_activity(context);
    }

    uint32_t size_final;
    introspection_get_history_stats(context, &size_final, &capacity, &util);

    // Size should stabilize at capacity
    EXPECT_LE(size_final, capacity);
    EXPECT_GE(size_final, capacity * 0.9);  // Should be near full

    std::cout << "Memory stable: capacity=" << capacity
              << " size after overflow=" << size_final << "\n";
}

TEST_F(AutoActivityHistoryRegressionTest, NoMemoryLeakAfterManySamples) {
    if (context == nullptr) GTEST_SKIP();

    uint32_t capacity, initial_size;
    float util;
    introspection_get_history_stats(context, &initial_size, &capacity, &util);

    // Sample many times, clearing periodically
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 100; i++) {
            introspection_sample_activity(context);
        }
        introspection_clear_history(context);
    }

    uint32_t final_size;
    introspection_get_history_stats(context, &final_size, &capacity, &util);

    // After clearing, size should be 0
    EXPECT_EQ(final_size, 0);

    // Capacity should remain constant
    uint32_t final_capacity;
    introspection_get_history_stats(context, &final_size, &final_capacity, &util);
    EXPECT_EQ(final_capacity, capacity);
}

//=============================================================================
// 3. Long-Running Stability Tests
//=============================================================================

TEST_F(AutoActivityHistoryRegressionTest, StableOverLongPeriod) {
    if (context == nullptr) GTEST_SKIP();

    const int duration_seconds = 2;  // Short for testing
    const int sample_interval_ms = 10;

    uint64_t start_time = nimcp_time_monotonic_ms();
    int samples_taken = 0;

    while (nimcp_time_elapsed_ms(start_time) < duration_seconds * 1000) {
        introspection_sample_activity(context);
        samples_taken++;
        std::this_thread::sleep_for(std::chrono::milliseconds(sample_interval_ms));
    }

    // Verify system still functional
    uint32_t size, capacity;
    float util;
    bool result = introspection_get_history_stats(context, &size, &capacity, &util);

    EXPECT_TRUE(result);
    EXPECT_GT(samples_taken, 0);

    std::cout << "Long-run test: " << samples_taken << " samples over "
              << duration_seconds << " seconds\n";
}

TEST_F(AutoActivityHistoryRegressionTest, NoDataCorruptionAfterManyOperations) {
    if (context == nullptr) GTEST_SKIP();

    // Perform many mixed operations
    for (int i = 0; i < 100; i++) {
        introspection_sample_activity(context);

        if (i % 10 == 0) {
            uint32_t size, capacity;
            float util;
            introspection_get_history_stats(context, &size, &capacity, &util);
        }

        if (i % 20 == 0) {
            uint32_t num_entries = 0;
            activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);
            if (history != nullptr) free(history);
        }

        if (i % 30 == 0) {
            introspection_clear_history(context);
        }
    }

    // Final sampling should still work
    bool result = introspection_sample_activity(context);
    EXPECT_TRUE(result);

    // History should be accessible
    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    // Validate entries if present
    for (uint32_t i = 0; i < num_entries; i++) {
        EXPECT_GE(history[i].avg_activation, 0.0F);
        EXPECT_LE(history[i].avg_activation, 1.0F);
        EXPECT_GT(history[i].timestamp, 0);
    }

    if (history != nullptr) free(history);
}

//=============================================================================
// 4. Thread Safety Tests
//=============================================================================

TEST_F(AutoActivityHistoryRegressionTest, ConcurrentSamplingThreadSafe) {
    if (context == nullptr) GTEST_SKIP();

    std::atomic<int> successful_samples{0};
    std::atomic<bool> stop{false};

    auto sampler = [&]() {
        while (!stop.load()) {
            if (introspection_sample_activity(context)) {
                successful_samples++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    // Launch multiple sampling threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(sampler);
    }

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop = true;

    for (auto& thread : threads) {
        thread.join();
    }

    // Should have taken many samples without crashes
    EXPECT_GT(successful_samples.load(), 0);

    std::cout << "Concurrent sampling: " << successful_samples.load()
              << " successful samples\n";
}

TEST_F(AutoActivityHistoryRegressionTest, ConcurrentReadWriteThreadSafe) {
    if (context == nullptr) GTEST_SKIP();

    std::atomic<bool> stop{false};
    std::atomic<int> reads{0};
    std::atomic<int> writes{0};

    auto writer = [&]() {
        while (!stop.load()) {
            if (introspection_sample_activity(context)) {
                writes++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    };

    auto reader = [&]() {
        while (!stop.load()) {
            uint32_t num_entries = 0;
            activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);
            if (history != nullptr) {
                reads++;
                free(history);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    // Launch threads
    std::thread writer_thread(writer);
    std::thread reader_thread(reader);

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop = true;

    writer_thread.join();
    reader_thread.join();

    EXPECT_GT(reads.load(), 0);
    EXPECT_GT(writes.load(), 0);

    std::cout << "Concurrent R/W: " << reads.load() << " reads, "
              << writes.load() << " writes\n";
}

//=============================================================================
// 5. Stress Tests
//=============================================================================

TEST_F(AutoActivityHistoryRegressionTest, HighFrequencySamplingStressTest) {
    if (context == nullptr) GTEST_SKIP();

    const int stress_samples = 10000;
    int successful = 0;

    uint64_t start = nimcp_time_monotonic_us();

    for (int i = 0; i < stress_samples; i++) {
        if (introspection_sample_activity(context)) {
            successful++;
        }
    }

    uint64_t elapsed = nimcp_time_elapsed_us(start);

    // All samples should succeed
    EXPECT_EQ(successful, stress_samples);

    // Average time should still be reasonable
    double avg_time = (double)elapsed / stress_samples;
    EXPECT_LT(avg_time, 200.0);  // < 200us per sample

    std::cout << "Stress test: " << stress_samples << " samples in "
              << elapsed / 1000.0 << " ms (avg: " << avg_time << " us/sample)\n";
}

TEST_F(AutoActivityHistoryRegressionTest, BufferOverflowStressTest) {
    if (context == nullptr) GTEST_SKIP();

    uint32_t capacity, size;
    float util;
    introspection_get_history_stats(context, &size, &capacity, &util);

    // Sample way beyond capacity
    const int overflow_samples = capacity * 5;

    for (int i = 0; i < overflow_samples; i++) {
        bool result = introspection_sample_activity(context);
        EXPECT_TRUE(result);  // Should not fail even when overflowing
    }

    // Size should be at most capacity
    introspection_get_history_stats(context, &size, &capacity, &util);
    EXPECT_LE(size, capacity);

    // Should still be functional
    introspection_clear_history(context);
    introspection_sample_activity(context);

    introspection_get_history_stats(context, &size, &capacity, &util);
    EXPECT_EQ(size, 1);
}

//=============================================================================
// 6. Callback Performance Tests
//=============================================================================

static std::atomic<int> regression_callback_count{0};

static void regression_callback(const activity_history_entry_t* entry, void* user_data) {
    regression_callback_count++;
    // Minimal processing to test overhead
}

TEST_F(AutoActivityHistoryRegressionTest, CallbackOverheadMinimal) {
    if (context == nullptr) GTEST_SKIP();

    regression_callback_count = 0;

    // Measure without callback
    uint64_t start_no_callback = nimcp_time_monotonic_us();
    for (int i = 0; i < 1000; i++) {
        introspection_sample_activity(context);
    }
    uint64_t elapsed_no_callback = nimcp_time_elapsed_us(start_no_callback);

    introspection_clear_history(context);

    // Measure with callback
    introspection_set_activity_callback(context, regression_callback, nullptr);

    uint64_t start_with_callback = nimcp_time_monotonic_us();
    for (int i = 0; i < 1000; i++) {
        introspection_sample_activity(context);
    }
    uint64_t elapsed_with_callback = nimcp_time_elapsed_us(start_with_callback);

    // Callback overhead should be small (< 2x slowdown)
    double overhead_ratio = (double)elapsed_with_callback / (double)elapsed_no_callback;
    EXPECT_LT(overhead_ratio, 2.0);

    std::cout << "Callback overhead: " << overhead_ratio << "x\n";
    std::cout << "No callback: " << elapsed_no_callback / 1000.0 << " us/sample\n";
    std::cout << "With callback: " << elapsed_with_callback / 1000.0 << " us/sample\n";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
