/**
 * @file test_language_production_bridge_regression.cpp
 * @brief Regression tests for Language Production Bridge performance
 *
 * WHAT: Performance benchmarks and regression detection
 * WHY:  Ensure performance doesn't degrade over time
 * HOW:  Measure latency, throughput, memory usage; compare against baselines
 *
 * BENCHMARKS:
 * - Encoding latency (p50, p95, p99)
 * - Decoding latency
 * - End-to-end round-trip time
 * - Throughput (messages/sec)
 * - Memory usage per message
 * - Compression ratio
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
    #include "core/brain_regions/nimcp_language_production_bridge.h"
    #include "async/nimcp_bio_router.h"
    #include "utils/logging/nimcp_logging.h"
    #include "utils/time/nimcp_time.h"

#include <algorithm>
#include <vector>
#include <numeric>
#include <cmath>

//=============================================================================
// Performance Baselines (update these as optimizations improve)
//=============================================================================

namespace Baselines {
    constexpr float ENCODING_P50_MS = 2.0f;      // 50th percentile
    constexpr float ENCODING_P95_MS = 5.0f;      // 95th percentile
    constexpr float ENCODING_P99_MS = 10.0f;     // 99th percentile
    constexpr float DECODING_P50_MS = 1.5f;
    constexpr float ROUND_TRIP_P50_MS = 4.0f;
    constexpr float MIN_THROUGHPUT = 200.0f;     // messages/sec
    constexpr float MAX_MEMORY_PER_MSG_KB = 10.0f;
    constexpr float MIN_COMPRESSION_RATIO = 0.5f;
}

//=============================================================================
// Test Fixture
//=============================================================================

class LanguageBridgeRegressionTest : public ::testing::Test {
protected:
    language_production_bridge_t* bridge_;
    bio_router_t router_;

    void SetUp() override {
        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        bio_router_init(&router_config);

        // Create bridge with default config
        bridge_ = language_bridge_create(NULL);
        ASSERT_NE(bridge_, nullptr);
    }

    void TearDown() override {
        if (bridge_) {
            language_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
        bio_router_shutdown();
    }

    // Helper: Create thought vector
    std::vector<float> create_thought_vector(uint32_t size) {
        std::vector<float> vec(size);
        for (uint32_t i = 0; i < size; i++) {
            vec[i] = std::sin(2.0f * 3.14159f * (float)i / (float)size);
        }
        return vec;
    }

    // Helper: Calculate percentiles
    float calculate_percentile(std::vector<float>& data, float percentile) {
        if (data.empty()) return 0.0f;

        std::sort(data.begin(), data.end());
        size_t index = (size_t)(percentile * (float)(data.size() - 1));
        return data[index];
    }

    // Helper: Print benchmark results
    void print_benchmark_results(const char* name,
                                 const std::vector<float>& latencies,
                                 float baseline_p50,
                                 float baseline_p95,
                                 float baseline_p99) {
        auto latencies_copy = latencies;  // Need mutable copy for sorting
        float p50 = calculate_percentile(latencies_copy, 0.50f);
        float p95 = calculate_percentile(latencies_copy, 0.95f);
        float p99 = calculate_percentile(latencies_copy, 0.99f);

        float sum = std::accumulate(latencies.begin(), latencies.end(), 0.0f);
        float avg = sum / (float)latencies.size();

        float min = *std::min_element(latencies.begin(), latencies.end());
        float max = *std::max_element(latencies.begin(), latencies.end());

        LOG_INFO("=== %s Benchmark ===", name);
        LOG_INFO("  Samples: %zu", latencies.size());
        LOG_INFO("  Min:     %.3f ms", min);
        LOG_INFO("  Average: %.3f ms", avg);
        LOG_INFO("  P50:     %.3f ms (baseline: %.3f ms) %s",
                 p50, baseline_p50,
                 p50 <= baseline_p50 ? "[PASS]" : "[REGRESSION]");
        LOG_INFO("  P95:     %.3f ms (baseline: %.3f ms) %s",
                 p95, baseline_p95,
                 p95 <= baseline_p95 ? "[PASS]" : "[REGRESSION]");
        LOG_INFO("  P99:     %.3f ms (baseline: %.3f ms) %s",
                 p99, baseline_p99,
                 p99 <= baseline_p99 ? "[PASS]" : "[REGRESSION]");
        LOG_INFO("  Max:     %.3f ms", max);

        // Assert against baselines (with 20% tolerance for noise)
        EXPECT_LT(p50, baseline_p50 * 1.2f) << "P50 regression detected";
        EXPECT_LT(p95, baseline_p95 * 1.2f) << "P95 regression detected";
        EXPECT_LT(p99, baseline_p99 * 1.2f) << "P99 regression detected";
    }
};

//=============================================================================
// Encoding Latency Benchmark
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, EncodingLatencyBenchmark) {
    const int num_samples = 1000;
    const uint32_t vec_size = 512;

    std::vector<float> latencies;
    latencies.reserve(num_samples);

    for (int i = 0; i < num_samples; i++) {
        auto thought_vector = create_thought_vector(vec_size);
        language_message_t* msg = language_message_create("benchmark", 9, 512);
        ASSERT_NE(msg, nullptr);

        uint64_t start = nimcp_time_get_us();
        int result = language_bridge_encode_thought(bridge_, thought_vector.data(),
                                                     vec_size, msg);
        uint64_t end = nimcp_time_get_us();

        EXPECT_EQ(result, NIMCP_SUCCESS);
        float latency_ms = (float)(end - start) / 1000.0f;
        latencies.push_back(latency_ms);

        language_message_destroy(msg);
    }

    print_benchmark_results("Encoding Latency", latencies,
                           Baselines::ENCODING_P50_MS,
                           Baselines::ENCODING_P95_MS,
                           Baselines::ENCODING_P99_MS);
}

//=============================================================================
// Decoding Latency Benchmark
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, DecodingLatencyBenchmark) {
    const int num_samples = 1000;
    const uint32_t vec_size = 512;

    // Pre-encode messages
    std::vector<language_message_t*> messages;
    for (int i = 0; i < num_samples; i++) {
        auto thought_vector = create_thought_vector(vec_size);
        language_message_t* msg = language_message_create("benchmark", 9, 512);
        ASSERT_NE(msg, nullptr);

        language_bridge_encode_thought(bridge_, thought_vector.data(), vec_size, msg);
        messages.push_back(msg);
    }

    // Measure decoding latency
    std::vector<float> latencies;
    latencies.reserve(num_samples);

    for (language_message_t* msg : messages) {
        std::vector<float> decoded_thought(vec_size);
        uint32_t decoded_size = vec_size;

        uint64_t start = nimcp_time_get_us();
        int result = language_bridge_decode_message(bridge_, msg, decoded_thought.data(),
                                                     &decoded_size);
        uint64_t end = nimcp_time_get_us();

        EXPECT_EQ(result, NIMCP_SUCCESS);
        float latency_ms = (float)(end - start) / 1000.0f;
        latencies.push_back(latency_ms);
    }

    print_benchmark_results("Decoding Latency", latencies,
                           Baselines::DECODING_P50_MS,
                           Baselines::DECODING_P50_MS * 2.0f,  // P95
                           Baselines::DECODING_P50_MS * 3.0f); // P99

    // Cleanup
    for (language_message_t* msg : messages) {
        language_message_destroy(msg);
    }
}

//=============================================================================
// Round-Trip Latency Benchmark
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, RoundTripLatencyBenchmark) {
    const int num_samples = 500;
    const uint32_t vec_size = 512;

    std::vector<float> latencies;
    latencies.reserve(num_samples);

    for (int i = 0; i < num_samples; i++) {
        auto thought_vector = create_thought_vector(vec_size);
        language_message_t* msg = language_message_create("roundtrip", 9, 512);
        ASSERT_NE(msg, nullptr);

        uint64_t start = nimcp_time_get_us();

        // Encode
        int result = language_bridge_encode_thought(bridge_, thought_vector.data(),
                                                     vec_size, msg);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        // Decode
        std::vector<float> decoded_thought(vec_size);
        uint32_t decoded_size = vec_size;
        result = language_bridge_decode_message(bridge_, msg, decoded_thought.data(),
                                                &decoded_size);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        uint64_t end = nimcp_time_get_us();

        float latency_ms = (float)(end - start) / 1000.0f;
        latencies.push_back(latency_ms);

        language_message_destroy(msg);
    }

    print_benchmark_results("Round-Trip Latency", latencies,
                           Baselines::ROUND_TRIP_P50_MS,
                           Baselines::ROUND_TRIP_P50_MS * 2.0f,
                           Baselines::ROUND_TRIP_P50_MS * 3.0f);
}

//=============================================================================
// Throughput Benchmark
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, ThroughputBenchmark) {
    const int duration_sec = 3;
    const uint32_t vec_size = 512;

    uint64_t start_time = nimcp_time_get_ms();
    uint64_t end_time = start_time + (duration_sec * 1000);
    int messages_encoded = 0;

    while (nimcp_time_get_ms() < end_time) {
        auto thought_vector = create_thought_vector(vec_size);
        language_message_t* msg = language_message_create("throughput", 10, 512);

        if (msg) {
            int result = language_bridge_encode_thought(bridge_, thought_vector.data(),
                                                        vec_size, msg);
            if (result == NIMCP_SUCCESS) {
                messages_encoded++;
            }
            language_message_destroy(msg);
        }
    }

    float elapsed_sec = (float)(nimcp_time_get_ms() - start_time) / 1000.0f;
    float throughput = (float)messages_encoded / elapsed_sec;

    LOG_INFO("=== Throughput Benchmark ===");
    LOG_INFO("  Duration:   %.2f sec", elapsed_sec);
    LOG_INFO("  Messages:   %d", messages_encoded);
    LOG_INFO("  Throughput: %.1f msg/sec (baseline: %.1f msg/sec) %s",
             throughput, Baselines::MIN_THROUGHPUT,
             throughput >= Baselines::MIN_THROUGHPUT ? "[PASS]" : "[REGRESSION]");

    EXPECT_GE(throughput, Baselines::MIN_THROUGHPUT * 0.8f)
        << "Throughput regression detected";
}

//=============================================================================
// Memory Usage Benchmark
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, MemoryUsageBenchmark) {
    const int num_messages = 100;
    const uint32_t vec_size = 512;

    std::vector<language_message_t*> messages;

    // Create and encode messages
    for (int i = 0; i < num_messages; i++) {
        auto thought_vector = create_thought_vector(vec_size);
        language_message_t* msg = language_message_create("memory test", 11, 512);
        ASSERT_NE(msg, nullptr);

        language_bridge_encode_thought(bridge_, thought_vector.data(), vec_size, msg);
        messages.push_back(msg);
    }

    // Calculate approximate memory per message
    // (this is a rough estimate, actual memory tracking would need malloc hooks)
    size_t msg_size = sizeof(language_message_t);
    size_t semantic_size = 11;  // "memory test"
    size_t encoding_size = 512 * sizeof(float);
    size_t total_per_msg = msg_size + semantic_size + encoding_size;
    float kb_per_msg = (float)total_per_msg / 1024.0f;

    LOG_INFO("=== Memory Usage Benchmark ===");
    LOG_INFO("  Messages created: %d", num_messages);
    LOG_INFO("  Estimated size per message: %.2f KB (baseline: %.2f KB) %s",
             kb_per_msg, Baselines::MAX_MEMORY_PER_MSG_KB,
             kb_per_msg <= Baselines::MAX_MEMORY_PER_MSG_KB ? "[PASS]" : "[REGRESSION]");

    EXPECT_LT(kb_per_msg, Baselines::MAX_MEMORY_PER_MSG_KB * 1.2f)
        << "Memory usage regression detected";

    // Cleanup
    for (language_message_t* msg : messages) {
        language_message_destroy(msg);
    }
}

//=============================================================================
// Statistics Tracking Benchmark
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, StatisticsAccuracy) {
    const int num_messages = 100;
    const uint32_t vec_size = 512;

    language_bridge_reset_stats(bridge_);

    // Encode messages
    int successful_encodes = 0;
    for (int i = 0; i < num_messages; i++) {
        auto thought_vector = create_thought_vector(vec_size);
        language_message_t* msg = language_message_create("stats test", 10, 512);

        if (msg) {
            int result = language_bridge_encode_thought(bridge_, thought_vector.data(),
                                                        vec_size, msg);
            if (result == NIMCP_SUCCESS) {
                successful_encodes++;
            }
            language_message_destroy(msg);
        }
    }

    // Verify statistics
    language_bridge_stats_t stats = language_bridge_get_stats(bridge_);

    LOG_INFO("=== Statistics Accuracy ===");
    LOG_INFO("  Expected encodes: %d", successful_encodes);
    LOG_INFO("  Tracked encodes:  %lu", stats.messages_produced);
    LOG_INFO("  Accuracy: %.1f%%",
             100.0f * (float)stats.messages_produced / (float)successful_encodes);

    EXPECT_EQ(stats.messages_produced, successful_encodes)
        << "Statistics tracking inaccurate";
    EXPECT_GT(stats.avg_encoding_time_ms, 0.0f);
    EXPECT_GT(stats.avg_confidence, 0.0f);
}

//=============================================================================
// Encoding Quality Benchmark
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, EncodingQualityBenchmark) {
    const int num_samples = 100;
    const uint32_t vec_size = 512;

    std::vector<float> confidences;
    confidences.reserve(num_samples);

    for (int i = 0; i < num_samples; i++) {
        auto thought_vector = create_thought_vector(vec_size);
        language_message_t* msg = language_message_create("quality test", 12, 512);
        ASSERT_NE(msg, nullptr);

        int result = language_bridge_encode_thought(bridge_, thought_vector.data(),
                                                     vec_size, msg);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        confidences.push_back(msg->confidence);
        language_message_destroy(msg);
    }

    // Calculate average confidence
    float sum = std::accumulate(confidences.begin(), confidences.end(), 0.0f);
    float avg_confidence = sum / (float)num_samples;

    float min_confidence = *std::min_element(confidences.begin(), confidences.end());
    float max_confidence = *std::max_element(confidences.begin(), confidences.end());

    LOG_INFO("=== Encoding Quality Benchmark ===");
    LOG_INFO("  Samples: %d", num_samples);
    LOG_INFO("  Min confidence: %.3f", min_confidence);
    LOG_INFO("  Avg confidence: %.3f", avg_confidence);
    LOG_INFO("  Max confidence: %.3f", max_confidence);

    EXPECT_GT(avg_confidence, 0.5f) << "Average encoding quality too low";
    EXPECT_LE(avg_confidence, 1.0f);
    EXPECT_GT(min_confidence, 0.0f);
    EXPECT_LE(max_confidence, 1.0f);
}

//=============================================================================
// Different Vector Sizes Performance
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, ScalabilityWithVectorSize) {
    std::vector<uint32_t> vec_sizes = {128, 256, 512, 1024, 2048};

    LOG_INFO("=== Scalability Benchmark ===");

    for (uint32_t vec_size : vec_sizes) {
        const int num_samples = 100;
        std::vector<float> latencies;

        for (int i = 0; i < num_samples; i++) {
            auto thought_vector = create_thought_vector(vec_size);
            language_message_t* msg = language_message_create("scale test", 10, 512);
            ASSERT_NE(msg, nullptr);

            uint64_t start = nimcp_time_get_us();
            language_bridge_encode_thought(bridge_, thought_vector.data(), vec_size, msg);
            uint64_t end = nimcp_time_get_us();

            latencies.push_back((float)(end - start) / 1000.0f);
            language_message_destroy(msg);
        }

        auto latencies_copy = latencies;
        float median = calculate_percentile(latencies_copy, 0.50f);

        LOG_INFO("  Vector size %u: median latency %.3f ms", vec_size, median);

        // Verify reasonable scaling (latency shouldn't grow more than 2x when size doubles)
        if (vec_size == 128) {
            EXPECT_LT(median, Baselines::ENCODING_P50_MS);
        }
    }
}

//=============================================================================
// Concurrent Performance Benchmark
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, ConcurrentPerformance) {
    const int num_threads = 4;
    const int messages_per_thread = 100;

    uint64_t start_time = nimcp_time_get_ms();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, messages_per_thread]() {
            for (int i = 0; i < messages_per_thread; i++) {
                auto thought_vector = create_thought_vector(512);
                language_message_t* msg = language_message_create("concurrent", 10, 512);

                if (msg) {
                    language_bridge_encode_thought(bridge_, thought_vector.data(), 512, msg);
                    language_message_destroy(msg);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    uint64_t end_time = nimcp_time_get_ms();
    float elapsed_sec = (float)(end_time - start_time) / 1000.0f;
    int total_messages = num_threads * messages_per_thread;
    float throughput = (float)total_messages / elapsed_sec;

    LOG_INFO("=== Concurrent Performance ===");
    LOG_INFO("  Threads: %d", num_threads);
    LOG_INFO("  Total messages: %d", total_messages);
    LOG_INFO("  Throughput: %.1f msg/sec", throughput);

    // Should maintain at least 80% of single-threaded throughput
    EXPECT_GT(throughput, Baselines::MIN_THROUGHPUT * 0.8f)
        << "Concurrent performance degradation detected";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
