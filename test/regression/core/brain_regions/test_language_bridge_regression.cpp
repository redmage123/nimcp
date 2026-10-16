/**
 * @file test_language_bridge_regression.cpp
 * @brief Regression tests for Language Production Bridge
 *
 * WHAT: Performance baselines and regression detection for language bridge
 * WHY:  Ensure encoding/decoding performance doesn't degrade over time
 * HOW:  Benchmark encoding latency, throughput, memory usage, semantic loss
 *
 * BASELINE TARGETS:
 * - Encoding latency: < 5ms for 512-dim thought vector
 * - Throughput: > 200 encodings/sec
 * - Memory: < 10MB for 1000 messages
 * - Semantic loss: < 15% after encode/decode cycle
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
    #include "core/brain_regions/nimcp_language_production_bridge.h"
    #include "async/nimcp_bio_router.h"
    #include "utils/memory/nimcp_memory.h"
    #include "utils/time/nimcp_time.h"
    #include "utils/logging/nimcp_logging.h"

#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cstring>

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

    // Helper: Create deterministic thought vector
    std::vector<float> create_deterministic_thought(uint32_t size, uint32_t seed) {
        std::vector<float> vec(size);
        srand(seed);
        for (uint32_t i = 0; i < size; i++) {
            vec[i] = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;  // [-1, 1]
        }
        return vec;
    }

    // Helper: Create pattern-based thought vector
    std::vector<float> create_pattern_thought(uint32_t size, float freq = 1.0f) {
        std::vector<float> vec(size);
        for (uint32_t i = 0; i < size; i++) {
            float t = (float)i / (float)size;
            vec[i] = std::sin(2.0f * 3.14159f * freq * t);
        }
        return vec;
    }

    // Helper: Compute L2 norm
    float compute_norm(const std::vector<float>& vec) {
        float sum = 0.0f;
        for (float v : vec) {
            sum += v * v;
        }
        return std::sqrt(sum);
    }

    // Helper: Compute cosine similarity
    float compute_similarity(const std::vector<float>& v1, const std::vector<float>& v2) {
        float dot = 0.0f;
        float norm1 = 0.0f;
        float norm2 = 0.0f;

        for (size_t i = 0; i < v1.size() && i < v2.size(); i++) {
            dot += v1[i] * v2[i];
            norm1 += v1[i] * v1[i];
            norm2 += v2[i] * v2[i];
        }

        return dot / (std::sqrt(norm1) * std::sqrt(norm2));
    }

    // Helper: Compute semantic loss percentage
    float compute_semantic_loss(const std::vector<float>& original,
                                const std::vector<float>& decoded) {
        float similarity = compute_similarity(original, decoded);
        return (1.0f - similarity) * 100.0f;  // Percentage
    }
};

//=============================================================================
// Regression Test 1: Encoding Latency Baseline
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, EncodingLatencyBaseline) {
    // WHAT: Measure baseline encoding latency
    // WHY:  Detect performance regressions in encoding
    // HOW:  Time multiple encoding operations, compute statistics
    // BASELINE: < 5ms average for 512-dim thought vector

    const int num_iterations = 1000;
    const uint32_t vec_size = 512;

    std::vector<float> latencies;
    latencies.reserve(num_iterations);

    // Warm-up
    for (int i = 0; i < 10; i++) {
        auto thought = create_deterministic_thought(vec_size, i);
        language_message_t* msg = language_message_create("warmup", 6, 512);
        if (msg) {
            language_bridge_encode_thought(bridge_, thought.data(), vec_size, msg);
            language_message_destroy(msg);
        }
    }

    // Benchmark
    for (int i = 0; i < num_iterations; i++) {
        auto thought = create_deterministic_thought(vec_size, i + 1000);
        language_message_t* msg = language_message_create("benchmark", 9, 512);
        ASSERT_NE(msg, nullptr);

        uint64_t start = nimcp_time_get_us();
        int result = language_bridge_encode_thought(bridge_, thought.data(), vec_size, msg);
        uint64_t end = nimcp_time_get_us();

        if (result == NIMCP_SUCCESS) {
            float latency_ms = (float)(end - start) / 1000.0f;
            latencies.push_back(latency_ms);
        }

        language_message_destroy(msg);
    }

    // Compute statistics
    std::sort(latencies.begin(), latencies.end());
    float min_latency = latencies.front();
    float max_latency = latencies.back();
    float median = latencies[latencies.size() / 2];
    float p95 = latencies[(latencies.size() * 95) / 100];
    float p99 = latencies[(latencies.size() * 99) / 100];

    float sum = 0.0f;
    for (float lat : latencies) {
        sum += lat;
    }
    float avg_latency = sum / (float)latencies.size();

    // Log baseline metrics
    std::cout << "[BASELINE] Encoding Latency Statistics:" << std::endl;
    std::cout << "  Min:    " << min_latency << " ms" << std::endl;
    std::cout << "  Avg:    " << avg_latency << " ms" << std::endl;
    std::cout << "  Median: " << median << " ms" << std::endl;
    std::cout << "  P95:    " << p95 << " ms" << std::endl;
    std::cout << "  P99:    " << p99 << " ms" << std::endl;
    std::cout << "  Max:    " << max_latency << " ms" << std::endl;

    // Regression checks
    EXPECT_LT(avg_latency, 5.0f) << "Average encoding latency exceeded 5ms baseline";
    EXPECT_LT(p95, 10.0f) << "P95 encoding latency exceeded 10ms";
    EXPECT_LT(p99, 20.0f) << "P99 encoding latency exceeded 20ms";
}

//=============================================================================
// Regression Test 2: Throughput Baseline
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, ThroughputBaseline) {
    // WHAT: Measure encoding throughput (messages/sec)
    // WHY:  Ensure sustained performance under load
    // HOW:  Encode as many messages as possible in fixed time
    // BASELINE: > 200 encodings/sec

    const int duration_ms = 5000;  // 5 seconds
    const uint32_t vec_size = 512;

    uint64_t start_time = nimcp_time_get_ms();
    uint64_t end_time = start_time + duration_ms;
    int successful_encodings = 0;
    int total_attempts = 0;

    while (nimcp_time_get_ms() < end_time) {
        auto thought = create_pattern_thought(vec_size, (float)(total_attempts % 10));
        language_message_t* msg = language_message_create("throughput", 10, 512);

        if (msg) {
            total_attempts++;
            int result = language_bridge_encode_thought(bridge_, thought.data(), vec_size, msg);
            if (result == NIMCP_SUCCESS) {
                successful_encodings++;
            }
            language_message_destroy(msg);
        }
    }

    float actual_duration_sec = (float)(nimcp_time_get_ms() - start_time) / 1000.0f;
    float throughput = (float)successful_encodings / actual_duration_sec;
    float success_rate = (float)successful_encodings / (float)total_attempts * 100.0f;

    std::cout << "[BASELINE] Throughput Statistics:" << std::endl;
    std::cout << "  Duration:    " << actual_duration_sec << " seconds" << std::endl;
    std::cout << "  Total:       " << total_attempts << " attempts" << std::endl;
    std::cout << "  Successful:  " << successful_encodings << " encodings" << std::endl;
    std::cout << "  Success Rate: " << success_rate << "%" << std::endl;
    std::cout << "  Throughput:  " << throughput << " encodings/sec" << std::endl;

    // Regression checks
    EXPECT_GT(throughput, 200.0f) << "Throughput below 200 encodings/sec baseline";
    EXPECT_GT(success_rate, 95.0f) << "Success rate below 95%";
}

//=============================================================================
// Regression Test 3: Memory Usage Baseline
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, MemoryUsageBaseline) {
    // WHAT: Measure memory consumption for message handling
    // WHY:  Detect memory leaks and excessive allocation
    // HOW:  Track memory before/after creating many messages
    // BASELINE: < 10MB for 1000 messages

    const int num_messages = 1000;
    const uint32_t vec_size = 512;

    // Get initial memory stats
    nimcp_memory_stats_t stats_before;
    nimcp_memory_get_stats(&stats_before);

    // Create and encode messages
    std::vector<language_message_t*> messages;
    messages.reserve(num_messages);

    for (int i = 0; i < num_messages; i++) {
        auto thought = create_deterministic_thought(vec_size, i);
        language_message_t* msg = language_message_create("memory test", 11, 512);

        if (msg) {
            int result = language_bridge_encode_thought(bridge_, thought.data(), vec_size, msg);
            if (result == NIMCP_SUCCESS) {
                messages.push_back(msg);
            } else {
                language_message_destroy(msg);
            }
        }
    }

    // Get memory stats after allocation
    nimcp_memory_stats_t stats_after;
    nimcp_memory_get_stats(&stats_after);

    size_t memory_used = stats_after.current_allocated - stats_before.current_allocated;
    float memory_mb = (float)memory_used / (1024.0f * 1024.0f);
    float memory_per_message = (float)memory_used / (float)messages.size();

    std::cout << "[BASELINE] Memory Usage Statistics:" << std::endl;
    std::cout << "  Messages:        " << messages.size() << std::endl;
    std::cout << "  Total Memory:    " << memory_mb << " MB" << std::endl;
    std::cout << "  Per Message:     " << memory_per_message << " bytes" << std::endl;

    // Cleanup
    for (auto* msg : messages) {
        language_message_destroy(msg);
    }

    // Regression checks
    EXPECT_LT(memory_mb, 10.0f) << "Memory usage exceeded 10MB for 1000 messages";
    EXPECT_LT(memory_per_message, 10240.0f) << "Per-message memory exceeded 10KB";
}

//=============================================================================
// Regression Test 4: Semantic Loss Measurement
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, SemanticLossBaseline) {
    // WHAT: Measure information loss in encode/decode cycle
    // WHY:  Ensure encoding preserves semantic content
    // HOW:  Compare original thought with decoded version
    // BASELINE: < 15% semantic loss

    const int num_samples = 100;
    const uint32_t vec_size = 512;

    std::vector<float> semantic_losses;
    semantic_losses.reserve(num_samples);

    for (int i = 0; i < num_samples; i++) {
        // Create original thought
        auto original_thought = create_pattern_thought(vec_size, (float)(i % 5));

        // Encode
        language_message_t* msg = language_message_create("semantic test", 13, 512);
        ASSERT_NE(msg, nullptr);

        int result = language_bridge_encode_thought(bridge_, original_thought.data(),
                                                     vec_size, msg);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        // Decode
        std::vector<float> decoded_thought(vec_size);
        uint32_t decoded_size = vec_size;

        result = language_bridge_decode_message(bridge_, msg, decoded_thought.data(),
                                                &decoded_size);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        // Compute semantic loss
        float loss = compute_semantic_loss(original_thought, decoded_thought);
        semantic_losses.push_back(loss);

        language_message_destroy(msg);
    }

    // Compute statistics
    std::sort(semantic_losses.begin(), semantic_losses.end());
    float min_loss = semantic_losses.front();
    float max_loss = semantic_losses.back();
    float median_loss = semantic_losses[semantic_losses.size() / 2];

    float sum = 0.0f;
    for (float loss : semantic_losses) {
        sum += loss;
    }
    float avg_loss = sum / (float)semantic_losses.size();

    std::cout << "[BASELINE] Semantic Loss Statistics:" << std::endl;
    std::cout << "  Min:    " << min_loss << "%" << std::endl;
    std::cout << "  Avg:    " << avg_loss << "%" << std::endl;
    std::cout << "  Median: " << median_loss << "%" << std::endl;
    std::cout << "  Max:    " << max_loss << "%" << std::endl;

    // Regression checks
    EXPECT_LT(avg_loss, 15.0f) << "Average semantic loss exceeded 15% baseline";
    EXPECT_LT(max_loss, 30.0f) << "Max semantic loss exceeded 30%";
}

//=============================================================================
// Regression Test 5: Determinism Check
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, EncodingDeterminism) {
    // WHAT: Verify encoding is deterministic for same input
    // WHY:  Non-determinism breaks reproducibility and debugging
    // HOW:  Encode same thought multiple times, verify identical output

    const uint32_t vec_size = 512;
    auto thought = create_deterministic_thought(vec_size, 42);

    // First encoding
    language_message_t* msg1 = language_message_create("determinism", 11, 512);
    ASSERT_NE(msg1, nullptr);
    int result1 = language_bridge_encode_thought(bridge_, thought.data(), vec_size, msg1);
    ASSERT_EQ(result1, NIMCP_SUCCESS);

    // Second encoding (same input)
    language_message_t* msg2 = language_message_create("determinism", 11, 512);
    ASSERT_NE(msg2, nullptr);
    int result2 = language_bridge_encode_thought(bridge_, thought.data(), vec_size, msg2);
    ASSERT_EQ(result2, NIMCP_SUCCESS);

    // Verify identical neural encoding
    bool identical = true;
    for (uint32_t i = 0; i < msg1->encoding_size; i++) {
        if (msg1->neural_encoding[i] != msg2->neural_encoding[i]) {
            identical = false;
            std::cout << "Mismatch at index " << i << ": "
                     << msg1->neural_encoding[i] << " vs "
                     << msg2->neural_encoding[i] << std::endl;
            break;
        }
    }

    EXPECT_TRUE(identical) << "Encoding is not deterministic";
    EXPECT_FLOAT_EQ(msg1->confidence, msg2->confidence) << "Confidence differs";

    language_message_destroy(msg1);
    language_message_destroy(msg2);
}

//=============================================================================
// Regression Test 6: Concurrent Performance
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, ConcurrentPerformanceBaseline) {
    // WHAT: Measure throughput with concurrent encoding
    // WHY:  Verify thread-safety doesn't degrade performance
    // HOW:  Multiple threads encoding simultaneously
    // BASELINE: Linear scaling up to 4 threads

    const int num_threads = 4;
    const int messages_per_thread = 250;
    const uint32_t vec_size = 512;

    std::vector<std::thread> threads;
    std::vector<int> success_counts(num_threads, 0);

    uint64_t start_time = nimcp_time_get_ms();

    // Launch encoding threads
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, messages_per_thread, vec_size, &success_counts]() {
            for (int i = 0; i < messages_per_thread; i++) {
                auto thought = create_pattern_thought(vec_size, (float)(t * 100 + i));
                language_message_t* msg = language_message_create("concurrent", 10, 512);

                if (msg) {
                    int result = language_bridge_encode_thought(bridge_, thought.data(),
                                                                vec_size, msg);
                    if (result == NIMCP_SUCCESS) {
                        success_counts[t]++;
                    }
                    language_message_destroy(msg);
                }
            }
        });
    }

    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }

    uint64_t end_time = nimcp_time_get_ms();
    float duration_sec = (float)(end_time - start_time) / 1000.0f;

    int total_success = 0;
    for (int count : success_counts) {
        total_success += count;
    }

    float throughput = (float)total_success / duration_sec;

    std::cout << "[BASELINE] Concurrent Performance:" << std::endl;
    std::cout << "  Threads:    " << num_threads << std::endl;
    std::cout << "  Duration:   " << duration_sec << " seconds" << std::endl;
    std::cout << "  Total:      " << total_success << " encodings" << std::endl;
    std::cout << "  Throughput: " << throughput << " encodings/sec" << std::endl;

    // Regression checks
    EXPECT_GT(throughput, 400.0f) << "Concurrent throughput below 400 encodings/sec";
    EXPECT_EQ(total_success, num_threads * messages_per_thread) << "Some encodings failed";
}

//=============================================================================
// Regression Test 7: Statistics Overhead
//=============================================================================

TEST_F(LanguageBridgeRegressionTest, StatisticsOverheadBaseline) {
    // WHAT: Measure overhead of statistics tracking
    // WHY:  Ensure stats don't add significant overhead
    // HOW:  Compare performance with/without stat retrieval
    // BASELINE: < 5% overhead

    const int num_iterations = 1000;
    const uint32_t vec_size = 512;

    // Benchmark without stats retrieval
    uint64_t start_no_stats = nimcp_time_get_us();
    for (int i = 0; i < num_iterations; i++) {
        auto thought = create_deterministic_thought(vec_size, i);
        language_message_t* msg = language_message_create("overhead", 8, 512);
        if (msg) {
            language_bridge_encode_thought(bridge_, thought.data(), vec_size, msg);
            language_message_destroy(msg);
        }
    }
    uint64_t end_no_stats = nimcp_time_get_us();

    // Reset stats
    language_bridge_reset_stats(bridge_);

    // Benchmark with frequent stats retrieval
    uint64_t start_with_stats = nimcp_time_get_us();
    for (int i = 0; i < num_iterations; i++) {
        auto thought = create_deterministic_thought(vec_size, i + 10000);
        language_message_t* msg = language_message_create("overhead", 8, 512);
        if (msg) {
            language_bridge_encode_thought(bridge_, thought.data(), vec_size, msg);
            language_bridge_get_stats(bridge_);  // Retrieve stats every iteration
            language_message_destroy(msg);
        }
    }
    uint64_t end_with_stats = nimcp_time_get_us();

    float time_no_stats = (float)(end_no_stats - start_no_stats) / 1000.0f;
    float time_with_stats = (float)(end_with_stats - start_with_stats) / 1000.0f;
    float overhead_percent = ((time_with_stats - time_no_stats) / time_no_stats) * 100.0f;

    std::cout << "[BASELINE] Statistics Overhead:" << std::endl;
    std::cout << "  Without Stats: " << time_no_stats << " ms" << std::endl;
    std::cout << "  With Stats:    " << time_with_stats << " ms" << std::endl;
    std::cout << "  Overhead:      " << overhead_percent << "%" << std::endl;

    // Regression check
    EXPECT_LT(overhead_percent, 5.0f) << "Statistics overhead exceeded 5%";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
