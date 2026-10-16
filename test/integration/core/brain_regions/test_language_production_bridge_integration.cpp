/**
 * @file test_language_production_bridge_integration.cpp
 * @brief Integration tests for Language Production Bridge with full pipeline
 *
 * WHAT: Test complete brain-to-brain communication pipeline
 * WHY:  Verify integration with Broca, NLP, Wernicke, bio-async
 * HOW:  Simulate full encoding → transmission → decoding flow
 *
 * TEST SCENARIOS:
 * - Full thought encoding and transmission pipeline
 * - Multi-brain communication (broadcast)
 * - Bio-async message flow
 * - Error recovery and retry logic
 * - Performance under load
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
    #include "core/brain_regions/nimcp_language_production_bridge.h"
    #include "async/nimcp_bio_router.h"
    #include "async/nimcp_bio_messages.h"
    #include "utils/logging/nimcp_logging.h"
    #include "utils/time/nimcp_time.h"

#include <thread>
#include <chrono>
#include <vector>

//=============================================================================
// Test Fixture
//=============================================================================

class LanguageBridgeIntegrationTest : public ::testing::Test {
protected:
    language_production_bridge_t* sender_bridge_;
    language_production_bridge_t* receiver_bridge_;
    bio_router_t router_;

    void SetUp() override {
        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        bio_router_init(&router_config);

        // Create sender bridge
        language_bridge_config_t config = language_bridge_default_config();
        config.enable_bio_async = true;
        config.enable_compression = true;

        sender_bridge_ = language_bridge_create(&config);
        ASSERT_NE(sender_bridge_, nullptr);

        // Create receiver bridge
        receiver_bridge_ = language_bridge_create(&config);
        ASSERT_NE(receiver_bridge_, nullptr);
    }

    void TearDown() override {
        if (sender_bridge_) {
            language_bridge_destroy(sender_bridge_);
            sender_bridge_ = nullptr;
        }
        if (receiver_bridge_) {
            language_bridge_destroy(receiver_bridge_);
            receiver_bridge_ = nullptr;
        }
        bio_router_shutdown();
    }

    // Helper: Create thought vector with pattern
    std::vector<float> create_pattern_vector(uint32_t size, float scale = 1.0f) {
        std::vector<float> vec(size);
        for (uint32_t i = 0; i < size; i++) {
            vec[i] = scale * std::sin(2.0f * 3.14159f * (float)i / (float)size);
        }
        return vec;
    }

    // Helper: Measure encoding latency
    float measure_encoding_latency(language_production_bridge_t* bridge,
                                   const float* thought_vec, uint32_t vec_size,
                                   language_message_t* msg) {
        uint64_t start = nimcp_time_get_us();
        language_bridge_encode_thought(bridge, thought_vec, vec_size, msg);
        uint64_t end = nimcp_time_get_us();
        return (float)(end - start) / 1000.0f;  // ms
    }
};

//=============================================================================
// Full Pipeline Tests
//=============================================================================

TEST_F(LanguageBridgeIntegrationTest, CompleteEncodingPipeline) {
    // Step 1: Create thought vector
    uint32_t vec_size = 512;
    auto thought_vector = create_pattern_vector(vec_size, 2.0f);

    // Step 2: Create message
    const char* semantic = "Integration test message";
    language_message_t* msg = language_message_create(semantic, strlen(semantic), 512);
    ASSERT_NE(msg, nullptr);

    // Step 3: Encode thought
    int result = language_bridge_encode_thought(sender_bridge_, thought_vector.data(),
                                                 vec_size, msg);
    ASSERT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(msg->confidence, 0.5f);

    // Step 4: Verify encoding statistics
    language_bridge_stats_t stats = language_bridge_get_stats(sender_bridge_);
    EXPECT_EQ(stats.messages_produced, 1);
    EXPECT_GT(stats.avg_encoding_time_ms, 0.0f);
    EXPECT_LT(stats.avg_encoding_time_ms, 100.0f);  // Should be fast

    // Step 5: Decode on receiver side
    std::vector<float> decoded_thought(vec_size);
    uint32_t decoded_size = vec_size;

    result = language_bridge_decode_message(receiver_bridge_, msg, decoded_thought.data(),
                                            &decoded_size);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Step 6: Verify receiver statistics
    language_bridge_stats_t receiver_stats = language_bridge_get_stats(receiver_bridge_);
    EXPECT_EQ(receiver_stats.messages_received, 1);
    EXPECT_EQ(receiver_stats.decoding_errors, 0);

    language_message_destroy(msg);
}

TEST_F(LanguageBridgeIntegrationTest, MultipleMessagesSequential) {
    const int num_messages = 50;
    uint32_t vec_size = 512;

    for (int i = 0; i < num_messages; i++) {
        // Create thought with different pattern
        auto thought_vector = create_pattern_vector(vec_size, (float)(i + 1));

        // Encode
        char semantic[64];
        snprintf(semantic, sizeof(semantic), "Message %d", i);
        language_message_t* msg = language_message_create(semantic, strlen(semantic), 512);
        ASSERT_NE(msg, nullptr);

        int result = language_bridge_encode_thought(sender_bridge_, thought_vector.data(),
                                                     vec_size, msg);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        // Decode
        std::vector<float> decoded_thought(vec_size);
        uint32_t decoded_size = vec_size;

        result = language_bridge_decode_message(receiver_bridge_, msg, decoded_thought.data(),
                                                &decoded_size);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        language_message_destroy(msg);
    }

    // Verify statistics
    language_bridge_stats_t sender_stats = language_bridge_get_stats(sender_bridge_);
    EXPECT_EQ(sender_stats.messages_produced, num_messages);
    EXPECT_EQ(sender_stats.encoding_errors, 0);

    language_bridge_stats_t receiver_stats = language_bridge_get_stats(receiver_bridge_);
    EXPECT_EQ(receiver_stats.messages_received, num_messages);
    EXPECT_EQ(receiver_stats.decoding_errors, 0);
}

TEST_F(LanguageBridgeIntegrationTest, BioAsyncEventGeneration) {
    uint32_t vec_size = 512;
    auto thought_vector = create_pattern_vector(vec_size);

    language_message_t* msg = language_message_create("bio-async test", 14, 512);
    ASSERT_NE(msg, nullptr);

    // Encode should generate bio-async event
    int result = language_bridge_encode_thought(sender_bridge_, thought_vector.data(),
                                                 vec_size, msg);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Process inbox (should handle any bio-async messages)
    int processed = language_bridge_process_inbox(sender_bridge_);
    EXPECT_GE(processed, 0);

    language_message_destroy(msg);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(LanguageBridgeIntegrationTest, EncodingLatency) {
    const int num_samples = 100;
    uint32_t vec_size = 512;

    std::vector<float> latencies;
    latencies.reserve(num_samples);

    for (int i = 0; i < num_samples; i++) {
        auto thought_vector = create_pattern_vector(vec_size);
        language_message_t* msg = language_message_create("latency test", 12, 512);
        ASSERT_NE(msg, nullptr);

        float latency = measure_encoding_latency(sender_bridge_, thought_vector.data(),
                                                 vec_size, msg);
        latencies.push_back(latency);

        language_message_destroy(msg);
    }

    // Calculate statistics
    float sum = 0.0f, max_latency = 0.0f;
    for (float latency : latencies) {
        sum += latency;
        max_latency = std::max(max_latency, latency);
    }
    float avg_latency = sum / (float)num_samples;

    LOG_INFO("Encoding latency: avg=%.3fms, max=%.3fms", avg_latency, max_latency);

    // Verify reasonable performance
    EXPECT_LT(avg_latency, 10.0f);   // Average should be under 10ms
    EXPECT_LT(max_latency, 50.0f);   // Max should be under 50ms
}

TEST_F(LanguageBridgeIntegrationTest, ThroughputTest) {
    const int num_messages = 1000;
    uint32_t vec_size = 512;

    uint64_t start_time = nimcp_time_get_ms();

    for (int i = 0; i < num_messages; i++) {
        auto thought_vector = create_pattern_vector(vec_size);
        language_message_t* msg = language_message_create("throughput test", 15, 512);

        if (msg) {
            language_bridge_encode_thought(sender_bridge_, thought_vector.data(),
                                          vec_size, msg);
            language_message_destroy(msg);
        }
    }

    uint64_t end_time = nimcp_time_get_ms();
    float elapsed_sec = (float)(end_time - start_time) / 1000.0f;
    float throughput = (float)num_messages / elapsed_sec;

    LOG_INFO("Throughput: %.1f messages/sec over %.3f seconds",
             throughput, elapsed_sec);

    EXPECT_GT(throughput, 100.0f);  // Should handle at least 100 msg/sec
}

//=============================================================================
// Different Encoding Dimensions
//=============================================================================

TEST_F(LanguageBridgeIntegrationTest, VariableEncodingDimensions) {
    std::vector<uint32_t> dimensions = {64, 128, 256, 512, 1024};

    for (uint32_t dim : dimensions) {
        language_bridge_config_t config = language_bridge_default_config();
        config.encoding_dim = dim;

        language_production_bridge_t* bridge = language_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        // Test encoding
        auto thought_vector = create_pattern_vector(512);  // Input always 512
        language_message_t* msg = language_message_create("dim test", 8, dim);
        ASSERT_NE(msg, nullptr);

        int result = language_bridge_encode_thought(bridge, thought_vector.data(),
                                                     512, msg);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_EQ(msg->encoding_size, dim);

        language_message_destroy(msg);
        language_bridge_destroy(bridge);
    }
}

//=============================================================================
// Error Recovery Tests
//=============================================================================

TEST_F(LanguageBridgeIntegrationTest, RecoverFromEncodingErrors) {
    uint32_t vec_size = 512;

    // First, try to encode zero vector (should fail)
    std::vector<float> zero_vector(vec_size, 0.0f);
    language_message_t* msg1 = language_message_create("error test", 10, 512);
    ASSERT_NE(msg1, nullptr);

    int result = language_bridge_encode_thought(sender_bridge_, zero_vector.data(),
                                                 vec_size, msg1);
    EXPECT_NE(result, NIMCP_SUCCESS);

    // Check error was tracked
    language_bridge_stats_t stats = language_bridge_get_stats(sender_bridge_);
    EXPECT_GT(stats.encoding_errors, 0);

    // Now encode valid vector (should succeed)
    auto valid_vector = create_pattern_vector(vec_size);
    language_message_t* msg2 = language_message_create("valid test", 10, 512);
    ASSERT_NE(msg2, nullptr);

    result = language_bridge_encode_thought(sender_bridge_, valid_vector.data(),
                                            vec_size, msg2);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify recovery
    stats = language_bridge_get_stats(sender_bridge_);
    EXPECT_GT(stats.messages_produced, 0);

    language_message_destroy(msg1);
    language_message_destroy(msg2);
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

TEST_F(LanguageBridgeIntegrationTest, ConcurrentEncoding) {
    const int num_threads = 4;
    const int messages_per_thread = 50;
    std::vector<std::thread> threads;

    auto encode_worker = [this, messages_per_thread](int thread_id) {
        for (int i = 0; i < messages_per_thread; i++) {
            auto thought_vector = create_pattern_vector(512, (float)(thread_id + 1));
            language_message_t* msg = language_message_create("concurrent test", 15, 512);

            if (msg) {
                language_bridge_encode_thought(sender_bridge_, thought_vector.data(),
                                              512, msg);
                language_message_destroy(msg);
            }
        }
    };

    // Launch threads
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(encode_worker, i);
    }

    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all messages were encoded
    language_bridge_stats_t stats = language_bridge_get_stats(sender_bridge_);
    EXPECT_EQ(stats.messages_produced, num_threads * messages_per_thread);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(LanguageBridgeIntegrationTest, SustainedLoad) {
    const int duration_sec = 2;
    const int target_rate = 100;  // messages per second

    uint64_t start_time = nimcp_time_get_ms();
    uint64_t end_time = start_time + (duration_sec * 1000);
    int messages_sent = 0;

    while (nimcp_time_get_ms() < end_time) {
        auto thought_vector = create_pattern_vector(512);
        language_message_t* msg = language_message_create("stress test", 11, 512);

        if (msg) {
            int result = language_bridge_encode_thought(sender_bridge_,
                                                        thought_vector.data(), 512, msg);
            if (result == NIMCP_SUCCESS) {
                messages_sent++;
            }
            language_message_destroy(msg);
        }

        // Rate limiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / target_rate));
    }

    float actual_rate = (float)messages_sent / (float)duration_sec;
    LOG_INFO("Sustained load: %.1f messages/sec (target: %d)", actual_rate, target_rate);

    EXPECT_GT(actual_rate, target_rate * 0.9f);  // Within 10% of target
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(LanguageBridgeIntegrationTest, CompressionEnabled) {
    language_bridge_config_t config = language_bridge_default_config();
    config.enable_compression = true;

    language_production_bridge_t* bridge = language_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    auto thought_vector = create_pattern_vector(512);
    language_message_t* msg = language_message_create("compression test", 16, 512);
    ASSERT_NE(msg, nullptr);

    int result = language_bridge_encode_thought(bridge, thought_vector.data(), 512, msg);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    language_message_destroy(msg);
    language_bridge_destroy(bridge);
}

TEST_F(LanguageBridgeIntegrationTest, LowArticulationThreshold) {
    language_bridge_config_t config = language_bridge_default_config();
    config.articulation_threshold = 0.3f;  // Lower threshold

    language_production_bridge_t* bridge = language_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Encode with lower quality vector
    std::vector<float> low_quality_vector(512, 0.1f);
    language_message_t* msg = language_message_create("low threshold", 13, 512);
    ASSERT_NE(msg, nullptr);

    int result = language_bridge_encode_thought(bridge, low_quality_vector.data(), 512, msg);
    // Should succeed with lower threshold
    EXPECT_EQ(result, NIMCP_SUCCESS);

    language_message_destroy(msg);
    language_bridge_destroy(bridge);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
