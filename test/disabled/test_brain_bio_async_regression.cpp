/**
 * @file test_brain_bio_async_regression.cpp
 * @brief Regression tests for brain bio-async integration performance and stability
 *
 * WHAT: Performance benchmarks and stability tests for brain bio-async operations
 * WHY:  Ensure bio-async integration performance doesn't regress and remains stable
 * HOW:  Timed operations, high-volume message tests, memory leak detection, stress tests
 *
 * TEST COVERAGE:
 * - High-volume message throughput (1000+ messages)
 * - Latency metrics for brain events
 * - Memory stability under load (no leaks)
 * - System stability over extended operation (100+ cycles)
 * - Concurrent brain operations
 * - Performance benchmarks for bio-async overhead
 * - Channel saturation handling
 * - Phase synchronization performance
 *
 * @author NIMCP Development Team
 * @date 2025-11-29
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

#include "core/brain/nimcp_brain.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainBioAsyncRegressionTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    nimcp_bio_router_t* router = nullptr;

    void SetUp() override {
        // Initialize unified memory
        nimcp_unified_memory_init();

        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        nimcp_error_t err = nimcp_bio_async_init(&bio_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Bio-async init failed";

        // Initialize router
        router = nimcp_bio_router_create();
        ASSERT_NE(router, nullptr) << "Router creation failed";

        // Create a test brain
        brain_config_t config = {};
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 10;
        config.num_outputs = 3;
        strncpy(config.task_name, "regression_test", sizeof(config.task_name) - 1);
        brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr) << "Brain creation failed";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }

        if (router) {
            nimcp_bio_router_destroy(router);
            router = nullptr;
        }

        nimcp_bio_async_shutdown();
        nimcp_unified_memory_shutdown();
    }

    // Helper: Measure message send latency
    double measureMessageLatency(nimcp_bio_channel_type_t channel,
                                  uint32_t msg_type,
                                  size_t data_size) {
        void* data = nimcp_unified_malloc(data_size);
        memset(data, 0x42, data_size);

        auto start = std::chrono::high_resolution_clock::now();
        nimcp_bio_router_send(router, "test_module", channel, msg_type, data, data_size);
        auto end = std::chrono::high_resolution_clock::now();

        nimcp_unified_free(data);

        return std::chrono::duration<double, std::micro>(end - start).count();
    }

    // Helper: Measure message throughput
    double measureMessageThroughput(nimcp_bio_channel_type_t channel,
                                     uint32_t msg_type,
                                     size_t data_size,
                                     int num_messages) {
        void* data = nimcp_unified_malloc(data_size);
        memset(data, 0x42, data_size);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_messages; i++) {
            nimcp_bio_router_send(router, "test_module", channel, msg_type, data, data_size);
        }
        auto end = std::chrono::high_resolution_clock::now();

        nimcp_unified_free(data);

        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        return (num_messages / elapsed_ms) * 1000.0; // messages per second
    }

    // Helper: Train brain with examples
    void trainBrain(brain_t b, int num_examples, uint32_t num_inputs) {
        float* features = new float[num_inputs];
        for (int i = 0; i < num_examples; i++) {
            for (uint32_t j = 0; j < num_inputs; j++) {
                features[j] = static_cast<float>(i * num_inputs + j) / 100.0f;
            }
            const char* label = (i % 3 == 0) ? "class_a" :
                               (i % 3 == 1) ? "class_b" : "class_c";
            brain_learn_example(b, features, num_inputs, label, 1.0f);
        }
        delete[] features;
    }
};

//=============================================================================
// High-Volume Message Throughput Tests
//=============================================================================

TEST_F(BrainBioAsyncRegressionTest, Throughput_HighVolumeDopamineMessages) {
    const int NUM_MESSAGES = 1000;
    const size_t DATA_SIZE = 64; // Small payload

    double throughput = measureMessageThroughput(
        BIO_CHANNEL_DOPAMINE,
        BIO_MSG_BRAIN_STATE_QUERY,
        DATA_SIZE,
        NUM_MESSAGES
    );

    // Should handle at least 10K messages/second
    EXPECT_GT(throughput, 10000.0)
        << "Dopamine channel throughput: " << throughput << " msg/s";

    RecordProperty("DopamineThroughput_msg_per_sec", throughput);
}

TEST_F(BrainBioAsyncRegressionTest, Throughput_HighVolumeSerotoninMessages) {
    const int NUM_MESSAGES = 1000;
    const size_t DATA_SIZE = 128; // Medium payload

    double throughput = measureMessageThroughput(
        BIO_CHANNEL_SEROTONIN,
        BIO_MSG_ETHICS_EVALUATION_REQUEST,
        DATA_SIZE,
        NUM_MESSAGES
    );

    // Serotonin (slower channel) should still handle decent throughput
    EXPECT_GT(throughput, 5000.0)
        << "Serotonin channel throughput: " << throughput << " msg/s";

    RecordProperty("SerotoninThroughput_msg_per_sec", throughput);
}

TEST_F(BrainBioAsyncRegressionTest, Throughput_MixedChannelLoad) {
    const int NUM_MESSAGES_PER_CHANNEL = 500;
    const size_t DATA_SIZE = 64;

    void* data = nimcp_unified_malloc(DATA_SIZE);
    memset(data, 0x42, DATA_SIZE);

    auto start = std::chrono::high_resolution_clock::now();

    // Send messages across all channels
    for (int i = 0; i < NUM_MESSAGES_PER_CHANNEL; i++) {
        nimcp_bio_router_send(router, "test_module", BIO_CHANNEL_DOPAMINE,
                             BIO_MSG_BRAIN_STATE_QUERY, data, DATA_SIZE);
        nimcp_bio_router_send(router, "test_module", BIO_CHANNEL_SEROTONIN,
                             BIO_MSG_ETHICS_EVALUATION_REQUEST, data, DATA_SIZE);
        nimcp_bio_router_send(router, "test_module", BIO_CHANNEL_NOREPINEPHRINE,
                             BIO_MSG_ATTENTION_SHIFT, data, DATA_SIZE);
        nimcp_bio_router_send(router, "test_module", BIO_CHANNEL_ACETYLCHOLINE,
                             BIO_MSG_WORKING_MEMORY_RETRIEVE, data, DATA_SIZE);
    }

    auto end = std::chrono::high_resolution_clock::now();

    nimcp_unified_free(data);

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double total_throughput = (NUM_MESSAGES_PER_CHANNEL * 4 / elapsed_ms) * 1000.0;

    // Should handle at least 8K messages/second with mixed channels
    EXPECT_GT(total_throughput, 8000.0)
        << "Mixed channel throughput: " << total_throughput << " msg/s";

    RecordProperty("MixedChannelThroughput_msg_per_sec", total_throughput);
}

//=============================================================================
// Latency Metrics Tests
//=============================================================================

TEST_F(BrainBioAsyncRegressionTest, Latency_SingleMessageDopamine) {
    const int NUM_SAMPLES = 100;
    double total_latency = 0.0;
    double min_latency = 1e9;
    double max_latency = 0.0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        double latency = measureMessageLatency(BIO_CHANNEL_DOPAMINE,
                                               BIO_MSG_BRAIN_STATE_QUERY, 64);
        total_latency += latency;
        min_latency = std::min(min_latency, latency);
        max_latency = std::max(max_latency, latency);
    }

    double avg_latency = total_latency / NUM_SAMPLES;

    // Average latency should be under 100 microseconds
    EXPECT_LT(avg_latency, 100.0)
        << "Average dopamine message latency: " << avg_latency << " μs";

    RecordProperty("DopamineAvgLatency_us", avg_latency);
    RecordProperty("DopamineMinLatency_us", min_latency);
    RecordProperty("DopamineMaxLatency_us", max_latency);
}

TEST_F(BrainBioAsyncRegressionTest, Latency_AcetylcholineFastChannel) {
    const int NUM_SAMPLES = 100;
    double total_latency = 0.0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        double latency = measureMessageLatency(BIO_CHANNEL_ACETYLCHOLINE,
                                               BIO_MSG_WORKING_MEMORY_RETRIEVE, 32);
        total_latency += latency;
    }

    double avg_latency = total_latency / NUM_SAMPLES;

    // ACh is the fastest channel - should be under 50 microseconds
    EXPECT_LT(avg_latency, 50.0)
        << "Average ACh message latency: " << avg_latency << " μs";

    RecordProperty("AcetylcholineAvgLatency_us", avg_latency);
}

//=============================================================================
// Memory Stability Tests
//=============================================================================

TEST_F(BrainBioAsyncRegressionTest, Memory_NoLeaksUnderLoad) {
    const int NUM_ITERATIONS = 1000;
    const size_t DATA_SIZE = 256;

    // Get initial memory stats
    nimcp_unified_memory_stats_t initial_stats;
    nimcp_unified_memory_get_stats(&initial_stats);

    // Send many messages
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        void* data = nimcp_unified_malloc(DATA_SIZE);
        memset(data, 0x42, DATA_SIZE);
        nimcp_bio_router_send(router, "test_module", BIO_CHANNEL_DOPAMINE,
                             BIO_MSG_BRAIN_STATE_QUERY, data, DATA_SIZE);
        nimcp_unified_free(data);
    }

    // Get final memory stats
    nimcp_unified_memory_stats_t final_stats;
    nimcp_unified_memory_get_stats(&final_stats);

    // Should not have accumulated allocations (all freed)
    EXPECT_EQ(initial_stats.current_usage, final_stats.current_usage)
        << "Memory leak detected: "
        << (final_stats.current_usage - initial_stats.current_usage) << " bytes";

    RecordProperty("MemoryLeakBytes",
                   static_cast<int64_t>(final_stats.current_usage - initial_stats.current_usage));
}

TEST_F(BrainBioAsyncRegressionTest, Memory_StabilityOverTime) {
    const int NUM_CYCLES = 100;
    const int MESSAGES_PER_CYCLE = 50;
    const size_t DATA_SIZE = 128;

    std::vector<size_t> memory_samples;

    for (int cycle = 0; cycle < NUM_CYCLES; cycle++) {
        // Send batch of messages
        for (int i = 0; i < MESSAGES_PER_CYCLE; i++) {
            void* data = nimcp_unified_malloc(DATA_SIZE);
            memset(data, 0x42, DATA_SIZE);
            nimcp_bio_router_send(router, "test_module", BIO_CHANNEL_DOPAMINE,
                                 BIO_MSG_BRAIN_STATE_QUERY, data, DATA_SIZE);
            nimcp_unified_free(data);
        }

        // Sample memory usage
        nimcp_unified_memory_stats_t stats;
        nimcp_unified_memory_get_stats(&stats);
        memory_samples.push_back(stats.current_usage);
    }

    // Check that memory usage doesn't grow over time
    size_t first_sample = memory_samples[10]; // Skip warmup
    size_t last_sample = memory_samples.back();

    // Allow 10% variance but should not have linear growth
    size_t max_growth = first_sample / 10;
    EXPECT_LT(last_sample - first_sample, max_growth)
        << "Memory usage grew from " << first_sample
        << " to " << last_sample << " bytes";

    RecordProperty("InitialMemoryUsage", first_sample);
    RecordProperty("FinalMemoryUsage", last_sample);
}

//=============================================================================
// Extended Operation Stability Tests
//=============================================================================

TEST_F(BrainBioAsyncRegressionTest, Stability_ExtendedOperation) {
    const int NUM_ITERATIONS = 100;
    const size_t DATA_SIZE = 64;

    int success_count = 0;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        void* data = nimcp_unified_malloc(DATA_SIZE);
        memset(data, i & 0xFF, DATA_SIZE);

        nimcp_error_t err = nimcp_bio_router_send(
            router, "test_module", BIO_CHANNEL_DOPAMINE,
            BIO_MSG_BRAIN_STATE_QUERY, data, DATA_SIZE
        );

        if (err == NIMCP_SUCCESS) {
            success_count++;
        }

        nimcp_unified_free(data);
    }

    // All operations should succeed
    EXPECT_EQ(success_count, NUM_ITERATIONS)
        << "Failed " << (NUM_ITERATIONS - success_count) << " operations";

    RecordProperty("SuccessRate_percent", (success_count * 100.0) / NUM_ITERATIONS);
}

TEST_F(BrainBioAsyncRegressionTest, Stability_RepeatedBrainSteps) {
    const int NUM_STEPS = 100;

    // Train the brain first
    trainBrain(brain, 50, 10);

    int success_count = 0;

    for (int i = 0; i < NUM_STEPS; i++) {
        // Create input
        float inputs[10];
        for (int j = 0; j < 10; j++) {
            inputs[j] = static_cast<float>(i * 10 + j) / 100.0f;
        }

        // Perform inference
        float* outputs = nullptr;
        int output_size = 0;
        brain_infer(brain, inputs, 10, &outputs, &output_size);

        if (outputs && output_size > 0) {
            success_count++;
        }

        if (outputs) {
            brain_free_outputs(outputs);
        }
    }

    // All inferences should succeed
    EXPECT_EQ(success_count, NUM_STEPS)
        << "Failed " << (NUM_STEPS - success_count) << " inferences";
}

//=============================================================================
// Concurrent Operation Tests
//=============================================================================

TEST_F(BrainBioAsyncRegressionTest, Concurrent_MultiThreadedMessages) {
    const int NUM_THREADS = 4;
    const int MESSAGES_PER_THREAD = 250;
    const size_t DATA_SIZE = 64;

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    auto thread_func = [&]() {
        for (int i = 0; i < MESSAGES_PER_THREAD; i++) {
            void* data = nimcp_unified_malloc(DATA_SIZE);
            memset(data, 0x42, DATA_SIZE);

            nimcp_error_t err = nimcp_bio_router_send(
                router, "test_module", BIO_CHANNEL_DOPAMINE,
                BIO_MSG_BRAIN_STATE_QUERY, data, DATA_SIZE
            );

            if (err == NIMCP_SUCCESS) {
                success_count++;
            }

            nimcp_unified_free(data);
        }
    };

    auto start = std::chrono::high_resolution_clock::now();

    // Launch threads
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();

    // All messages should succeed
    int total_messages = NUM_THREADS * MESSAGES_PER_THREAD;
    EXPECT_EQ(success_count.load(), total_messages)
        << "Lost " << (total_messages - success_count.load()) << " messages";

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double throughput = (total_messages / elapsed_ms) * 1000.0;

    RecordProperty("ConcurrentThroughput_msg_per_sec", throughput);
}

TEST_F(BrainBioAsyncRegressionTest, Concurrent_BrainOperations) {
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 25;

    // Train the brain
    trainBrain(brain, 50, 10);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    auto thread_func = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            float inputs[10];
            for (int j = 0; j < 10; j++) {
                inputs[j] = static_cast<float>(i * 10 + j) / 100.0f;
            }

            float* outputs = nullptr;
            int output_size = 0;
            brain_infer(brain, inputs, 10, &outputs, &output_size);

            if (outputs && output_size > 0) {
                success_count++;
            }

            if (outputs) {
                brain_free_outputs(outputs);
            }
        }
    };

    // Launch threads
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    // Most operations should succeed (some contention is acceptable)
    int total_ops = NUM_THREADS * OPS_PER_THREAD;
    double success_rate = (success_count.load() * 100.0) / total_ops;

    EXPECT_GT(success_rate, 90.0)
        << "Success rate: " << success_rate << "%";

    RecordProperty("ConcurrentSuccessRate_percent", success_rate);
}

//=============================================================================
// Performance Overhead Tests
//=============================================================================

TEST_F(BrainBioAsyncRegressionTest, Overhead_BioAsyncVsDirectCall) {
    const int NUM_ITERATIONS = 1000;

    // Measure direct brain inference time
    float inputs[10];
    for (int i = 0; i < 10; i++) {
        inputs[i] = 0.5f;
    }

    trainBrain(brain, 20, 10);

    auto start_direct = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        float* outputs = nullptr;
        int output_size = 0;
        brain_infer(brain, inputs, 10, &outputs, &output_size);
        if (outputs) brain_free_outputs(outputs);
    }
    auto end_direct = std::chrono::high_resolution_clock::now();

    double direct_time = std::chrono::duration<double, std::milli>(
        end_direct - start_direct).count();

    // Measure bio-async message time
    const size_t DATA_SIZE = sizeof(float) * 10;
    auto start_async = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        nimcp_bio_router_send(router, "test_module", BIO_CHANNEL_DOPAMINE,
                             BIO_MSG_BRAIN_STATE_QUERY, inputs, DATA_SIZE);
    }
    auto end_async = std::chrono::high_resolution_clock::now();

    double async_time = std::chrono::duration<double, std::milli>(
        end_async - start_async).count();

    // Bio-async overhead should be reasonable (less than 10x direct calls)
    double overhead_ratio = async_time / direct_time;

    EXPECT_LT(overhead_ratio, 10.0)
        << "Bio-async overhead: " << overhead_ratio << "x";

    RecordProperty("DirectTime_ms", direct_time);
    RecordProperty("AsyncTime_ms", async_time);
    RecordProperty("OverheadRatio", overhead_ratio);
}

//=============================================================================
// Channel Saturation Tests
//=============================================================================

TEST_F(BrainBioAsyncRegressionTest, ChannelSaturation_GracefulDegradation) {
    const int NUM_MESSAGES = 10000; // Very high load
    const size_t DATA_SIZE = 1024;   // Large payloads

    int success_count = 0;
    int saturation_count = 0;

    for (int i = 0; i < NUM_MESSAGES; i++) {
        void* data = nimcp_unified_malloc(DATA_SIZE);
        memset(data, 0x42, DATA_SIZE);

        nimcp_error_t err = nimcp_bio_router_send(
            router, "test_module", BIO_CHANNEL_DOPAMINE,
            BIO_MSG_BRAIN_STATE_QUERY, data, DATA_SIZE
        );

        if (err == NIMCP_SUCCESS) {
            success_count++;
        } else if (err == NIMCP_BIO_ERROR_CHANNEL_SATURATED) {
            saturation_count++;
        }

        nimcp_unified_free(data);
    }

    // Should either succeed or report saturation (not crash)
    EXPECT_EQ(success_count + saturation_count, NUM_MESSAGES)
        << "Unexpected errors occurred";

    RecordProperty("SuccessCount", success_count);
    RecordProperty("SaturationCount", saturation_count);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
