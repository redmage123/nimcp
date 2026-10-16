/**
 * @file test_language_production_bridge.cpp
 * @brief Unit tests for Language Production Bridge
 *
 * WHAT: Comprehensive unit tests for language bridge module
 * WHY:  Ensure correct encoding, transmission, and bio-async integration
 * HOW:  Test each function with valid/invalid inputs, edge cases
 *
 * TEST COVERAGE:
 * - Lifecycle: create, destroy, configuration
 * - Encoding: thought vector → neural pattern
 * - Decoding: neural pattern → thought vector
 * - Transmission: NLP integration
 * - Statistics: tracking and reporting
 * - Error handling: invalid parameters, edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
    #include "core/brain_regions/nimcp_language_production_bridge.h"
    #include "async/nimcp_bio_router.h"
    #include "utils/logging/nimcp_logging.h"

#include <cmath>
#include <cstring>

//=============================================================================
// Test Fixture
//=============================================================================

class LanguageProductionBridgeTest : public ::testing::Test {
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

    // Helper: Create test thought vector
    float* create_test_thought_vector(uint32_t size, float value = 1.0f) {
        float* vec = new float[size];
        for (uint32_t i = 0; i < size; i++) {
            vec[i] = value * (float)(i + 1) / (float)size;
        }
        return vec;
    }

    // Helper: Verify neural encoding validity
    bool verify_neural_encoding(const float* encoding, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            if (encoding[i] < 0.0f || encoding[i] > 200.0f) {
                return false;  // Invalid spike rate
            }
        }
        return true;
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(LanguageProductionBridgeTest, CreateWithDefaultConfig) {
    EXPECT_NE(bridge_, nullptr);

    language_bridge_stats_t stats = language_bridge_get_stats(bridge_);
    EXPECT_EQ(stats.messages_produced, 0);
    EXPECT_EQ(stats.messages_transmitted, 0);
}

TEST_F(LanguageProductionBridgeTest, CreateWithCustomConfig) {
    language_bridge_config_t config = language_bridge_default_config();
    config.max_message_queue = 256;
    config.encoding_dim = 300;
    config.enable_compression = false;

    language_production_bridge_t* custom_bridge = language_bridge_create(&config);
    ASSERT_NE(custom_bridge, nullptr);

    language_bridge_destroy(custom_bridge);
}

TEST_F(LanguageProductionBridgeTest, DestroyNullBridge) {
    // Should not crash
    language_bridge_destroy(nullptr);
}

TEST_F(LanguageProductionBridgeTest, GetDefaultConfig) {
    language_bridge_config_t config = language_bridge_default_config();

    EXPECT_GT(config.max_message_queue, 0);
    EXPECT_GT(config.semantic_buffer_size, 0);
    EXPECT_GT(config.articulation_threshold, 0.0f);
    EXPECT_LE(config.articulation_threshold, 1.0f);
    EXPECT_GT(config.encoding_dim, 0);
    EXPECT_GT(config.encoding_rate, 0.0f);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(LanguageProductionBridgeTest, ConnectBrocaNull) {
    int result = language_bridge_connect_broca(bridge_, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(LanguageProductionBridgeTest, ConnectNLPNull) {
    int result = language_bridge_connect_nlp(bridge_, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(LanguageProductionBridgeTest, ConnectInvalidBridge) {
    int result = language_bridge_connect_broca(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);

    result = language_bridge_connect_nlp(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Message Management Tests
//=============================================================================

TEST_F(LanguageProductionBridgeTest, CreateMessageValid) {
    const char* content = "Hello, world!";
    uint32_t content_size = strlen(content);
    uint32_t encoding_size = 512;

    language_message_t* msg = language_message_create(content, content_size, encoding_size);
    ASSERT_NE(msg, nullptr);

    EXPECT_NE(msg->semantic_content, nullptr);
    EXPECT_STREQ(msg->semantic_content, content);
    EXPECT_EQ(msg->semantic_size, content_size);
    EXPECT_NE(msg->neural_encoding, nullptr);
    EXPECT_EQ(msg->encoding_size, encoding_size);
    EXPECT_GT(msg->timestamp_ms, 0);

    language_message_destroy(msg);
}

TEST_F(LanguageProductionBridgeTest, CreateMessageInvalidParams) {
    language_message_t* msg = language_message_create(nullptr, 10, 512);
    EXPECT_EQ(msg, nullptr);

    msg = language_message_create("test", 0, 512);
    EXPECT_EQ(msg, nullptr);

    msg = language_message_create("test", 4, 0);
    EXPECT_EQ(msg, nullptr);
}

TEST_F(LanguageProductionBridgeTest, DestroyMessageNull) {
    // Should not crash
    language_message_destroy(nullptr);
}

TEST_F(LanguageProductionBridgeTest, CloneMessage) {
    const char* content = "Test message";
    language_message_t* original = language_message_create(content, strlen(content), 256);
    ASSERT_NE(original, nullptr);

    original->message_id = 12345;
    original->confidence = 0.95f;
    for (uint32_t i = 0; i < 256; i++) {
        original->neural_encoding[i] = (float)i;
    }

    language_message_t* clone = language_message_clone(original);
    ASSERT_NE(clone, nullptr);

    EXPECT_EQ(clone->message_id, original->message_id);
    EXPECT_FLOAT_EQ(clone->confidence, original->confidence);
    EXPECT_EQ(clone->semantic_size, original->semantic_size);
    EXPECT_EQ(clone->encoding_size, original->encoding_size);
    EXPECT_STREQ(clone->semantic_content, original->semantic_content);

    for (uint32_t i = 0; i < 256; i++) {
        EXPECT_FLOAT_EQ(clone->neural_encoding[i], original->neural_encoding[i]);
    }

    language_message_destroy(clone);
    language_message_destroy(original);
}

//=============================================================================
// Encoding Tests
//=============================================================================

TEST_F(LanguageProductionBridgeTest, EncodeThoughtValid) {
    uint32_t vec_size = 512;
    float* thought_vector = create_test_thought_vector(vec_size, 1.0f);

    language_message_t* msg = language_message_create("test", 4, 512);
    ASSERT_NE(msg, nullptr);

    int result = language_bridge_encode_thought(bridge_, thought_vector, vec_size, msg);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_GT(msg->confidence, 0.0f);
    EXPECT_LE(msg->confidence, 1.0f);
    EXPECT_GT(msg->message_id, 0);
    EXPECT_TRUE(verify_neural_encoding(msg->neural_encoding, msg->encoding_size));

    language_message_destroy(msg);
    delete[] thought_vector;
}

TEST_F(LanguageProductionBridgeTest, EncodeThoughtInvalidParams) {
    language_message_t* msg = language_message_create("test", 4, 512);
    ASSERT_NE(msg, nullptr);

    float* thought_vector = create_test_thought_vector(512);

    // Null bridge
    int result = language_bridge_encode_thought(nullptr, thought_vector, 512, msg);
    EXPECT_NE(result, NIMCP_SUCCESS);

    // Null thought vector
    result = language_bridge_encode_thought(bridge_, nullptr, 512, msg);
    EXPECT_NE(result, NIMCP_SUCCESS);

    // Null output message
    result = language_bridge_encode_thought(bridge_, thought_vector, 512, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);

    language_message_destroy(msg);
    delete[] thought_vector;
}

TEST_F(LanguageProductionBridgeTest, EncodeThoughtZeroVector) {
    uint32_t vec_size = 512;
    float* zero_vector = new float[vec_size];
    memset(zero_vector, 0, vec_size * sizeof(float));

    language_message_t* msg = language_message_create("test", 4, 512);
    ASSERT_NE(msg, nullptr);

    int result = language_bridge_encode_thought(bridge_, zero_vector, vec_size, msg);
    // Should fail due to zero magnitude
    EXPECT_NE(result, NIMCP_SUCCESS);

    language_message_destroy(msg);
    delete[] zero_vector;
}

TEST_F(LanguageProductionBridgeTest, EncodeThoughtDifferentSizes) {
    // Test with input larger than encoding size
    {
        uint32_t vec_size = 1024;
        float* thought_vector = create_test_thought_vector(vec_size);

        language_message_t* msg = language_message_create("test", 4, 512);
        ASSERT_NE(msg, nullptr);

        int result = language_bridge_encode_thought(bridge_, thought_vector, vec_size, msg);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_GT(msg->confidence, 0.0f);

        language_message_destroy(msg);
        delete[] thought_vector;
    }

    // Test with input smaller than encoding size
    {
        uint32_t vec_size = 256;
        float* thought_vector = create_test_thought_vector(vec_size);

        language_message_t* msg = language_message_create("test", 4, 512);
        ASSERT_NE(msg, nullptr);

        int result = language_bridge_encode_thought(bridge_, thought_vector, vec_size, msg);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_GT(msg->confidence, 0.0f);

        language_message_destroy(msg);
        delete[] thought_vector;
    }
}

//=============================================================================
// Decoding Tests
//=============================================================================

TEST_F(LanguageProductionBridgeTest, DecodeMessageValid) {
    // First encode a message
    uint32_t vec_size = 512;
    float* original_thought = create_test_thought_vector(vec_size);

    language_message_t* msg = language_message_create("test", 4, 512);
    ASSERT_NE(msg, nullptr);

    int result = language_bridge_encode_thought(bridge_, original_thought, vec_size, msg);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Now decode it
    float decoded_thought[512];
    uint32_t decoded_size = 512;

    result = language_bridge_decode_message(bridge_, msg, decoded_thought, &decoded_size);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify decoding produced valid output (values should be in reasonable range)
    bool valid = true;
    for (uint32_t i = 0; i < decoded_size; i++) {
        if (std::isnan(decoded_thought[i]) || std::isinf(decoded_thought[i])) {
            valid = false;
            break;
        }
    }
    EXPECT_TRUE(valid);

    language_message_destroy(msg);
    delete[] original_thought;
}

TEST_F(LanguageProductionBridgeTest, DecodeMessageInvalidParams) {
    language_message_t* msg = language_message_create("test", 4, 512);
    ASSERT_NE(msg, nullptr);

    float thought[512];
    uint32_t size = 512;

    // Null bridge
    int result = language_bridge_decode_message(nullptr, msg, thought, &size);
    EXPECT_NE(result, NIMCP_SUCCESS);

    // Null message
    result = language_bridge_decode_message(bridge_, nullptr, thought, &size);
    EXPECT_NE(result, NIMCP_SUCCESS);

    // Null output
    result = language_bridge_decode_message(bridge_, msg, nullptr, &size);
    EXPECT_NE(result, NIMCP_SUCCESS);

    // Null size
    result = language_bridge_decode_message(bridge_, msg, thought, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);

    language_message_destroy(msg);
}

TEST_F(LanguageProductionBridgeTest, EncodeDecodeRoundTrip) {
    uint32_t vec_size = 512;
    float* original_thought = create_test_thought_vector(vec_size, 2.0f);

    language_message_t* msg = language_message_create("roundtrip test", 14, 512);
    ASSERT_NE(msg, nullptr);

    // Encode
    int result = language_bridge_encode_thought(bridge_, original_thought, vec_size, msg);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Decode
    float decoded_thought[512];
    uint32_t decoded_size = 512;

    result = language_bridge_decode_message(bridge_, msg, decoded_thought, &decoded_size);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Verify reconstruction (should be similar but not identical due to lossy encoding)
    // Check that decoded values are in reasonable range
    float max_diff = 0.0f;
    for (uint32_t i = 0; i < std::min(vec_size, decoded_size); i++) {
        float diff = std::fabs(decoded_thought[i]);
        max_diff = std::max(max_diff, diff);
    }

    // Decoded values should be reasonable (not NaN or inf)
    EXPECT_FALSE(std::isnan(max_diff));
    EXPECT_FALSE(std::isinf(max_diff));

    language_message_destroy(msg);
    delete[] original_thought;
}

//=============================================================================
// Transmission Tests
//=============================================================================

TEST_F(LanguageProductionBridgeTest, TransmitWithoutNLP) {
    language_message_t* msg = language_message_create("test", 4, 512);
    ASSERT_NE(msg, nullptr);

    // Should fail without NLP connection
    int result = language_bridge_transmit(bridge_, msg, 12345);
    EXPECT_NE(result, NIMCP_SUCCESS);

    language_message_destroy(msg);
}

TEST_F(LanguageProductionBridgeTest, TransmitInvalidParams) {
    // Null bridge
    language_message_t* msg = language_message_create("test", 4, 512);
    ASSERT_NE(msg, nullptr);

    int result = language_bridge_transmit(nullptr, msg, 12345);
    EXPECT_NE(result, NIMCP_SUCCESS);

    // Null message
    result = language_bridge_transmit(bridge_, nullptr, 12345);
    EXPECT_NE(result, NIMCP_SUCCESS);

    language_message_destroy(msg);
}

TEST_F(LanguageProductionBridgeTest, BroadcastWithoutNLP) {
    language_message_t* msg = language_message_create("broadcast test", 14, 512);
    ASSERT_NE(msg, nullptr);

    // Should fail without NLP connection
    int result = language_bridge_broadcast(bridge_, msg);
    EXPECT_NE(result, NIMCP_SUCCESS);

    language_message_destroy(msg);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(LanguageProductionBridgeTest, GetStatsInitial) {
    language_bridge_stats_t stats = language_bridge_get_stats(bridge_);

    EXPECT_EQ(stats.messages_produced, 0);
    EXPECT_EQ(stats.messages_transmitted, 0);
    EXPECT_EQ(stats.messages_received, 0);
    EXPECT_EQ(stats.encoding_errors, 0);
    EXPECT_EQ(stats.decoding_errors, 0);
    EXPECT_EQ(stats.transmission_errors, 0);
}

TEST_F(LanguageProductionBridgeTest, StatsAfterEncoding) {
    uint32_t vec_size = 512;
    float* thought_vector = create_test_thought_vector(vec_size);

    language_message_t* msg = language_message_create("test", 4, 512);
    ASSERT_NE(msg, nullptr);

    int result = language_bridge_encode_thought(bridge_, thought_vector, vec_size, msg);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    language_bridge_stats_t stats = language_bridge_get_stats(bridge_);
    EXPECT_EQ(stats.messages_produced, 1);
    EXPECT_GT(stats.avg_encoding_time_ms, 0.0f);
    EXPECT_GT(stats.avg_confidence, 0.0f);

    language_message_destroy(msg);
    delete[] thought_vector;
}

TEST_F(LanguageProductionBridgeTest, StatsAfterDecoding) {
    // Create and encode a message first
    uint32_t vec_size = 512;
    float* thought_vector = create_test_thought_vector(vec_size);

    language_message_t* msg = language_message_create("test", 4, 512);
    ASSERT_NE(msg, nullptr);

    language_bridge_encode_thought(bridge_, thought_vector, vec_size, msg);

    // Decode
    float decoded[512];
    uint32_t size = 512;
    int result = language_bridge_decode_message(bridge_, msg, decoded, &size);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    language_bridge_stats_t stats = language_bridge_get_stats(bridge_);
    EXPECT_EQ(stats.messages_received, 1);
    EXPECT_EQ(stats.decoding_errors, 0);

    language_message_destroy(msg);
    delete[] thought_vector;
}

TEST_F(LanguageProductionBridgeTest, ResetStats) {
    // Generate some stats
    uint32_t vec_size = 512;
    float* thought_vector = create_test_thought_vector(vec_size);

    language_message_t* msg = language_message_create("test", 4, 512);
    ASSERT_NE(msg, nullptr);

    language_bridge_encode_thought(bridge_, thought_vector, vec_size, msg);

    language_bridge_stats_t stats = language_bridge_get_stats(bridge_);
    EXPECT_GT(stats.messages_produced, 0);

    // Reset
    language_bridge_reset_stats(bridge_);

    stats = language_bridge_get_stats(bridge_);
    EXPECT_EQ(stats.messages_produced, 0);
    EXPECT_EQ(stats.messages_transmitted, 0);
    EXPECT_EQ(stats.messages_received, 0);

    language_message_destroy(msg);
    delete[] thought_vector;
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(LanguageProductionBridgeTest, ProcessInboxWithoutBioAsync) {
    int processed = language_bridge_process_inbox(bridge_);
    EXPECT_GE(processed, 0);
}

TEST_F(LanguageProductionBridgeTest, ProcessInboxNull) {
    int processed = language_bridge_process_inbox(nullptr);
    EXPECT_EQ(processed, 0);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(LanguageProductionBridgeTest, ErrorStringValid) {
    const char* str = language_bridge_error_string(NIMCP_SUCCESS);
    EXPECT_NE(str, nullptr);
    EXPECT_STRNE(str, "");

    str = language_bridge_error_string(-NIMCP_INVALID_PARAM);
    EXPECT_NE(str, nullptr);
    EXPECT_STRNE(str, "");

    str = language_bridge_error_string(-NIMCP_ERROR);
    EXPECT_NE(str, nullptr);
    EXPECT_STRNE(str, "");
}

TEST_F(LanguageProductionBridgeTest, ErrorStringUnknown) {
    const char* str = language_bridge_error_string(99999);
    EXPECT_NE(str, nullptr);
}

//=============================================================================
// Edge Cases and Stress Tests
//=============================================================================

TEST_F(LanguageProductionBridgeTest, EncodeMultipleMessages) {
    const int num_messages = 100;
    uint32_t vec_size = 512;

    for (int i = 0; i < num_messages; i++) {
        float* thought_vector = create_test_thought_vector(vec_size, (float)(i + 1));
        language_message_t* msg = language_message_create("test", 4, 512);
        ASSERT_NE(msg, nullptr);

        int result = language_bridge_encode_thought(bridge_, thought_vector, vec_size, msg);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        language_message_destroy(msg);
        delete[] thought_vector;
    }

    language_bridge_stats_t stats = language_bridge_get_stats(bridge_);
    EXPECT_EQ(stats.messages_produced, num_messages);
}

TEST_F(LanguageProductionBridgeTest, LargeSemanticContent) {
    const int large_size = 1024 * 1024;  // 1MB
    char* large_content = new char[large_size];
    memset(large_content, 'A', large_size - 1);
    large_content[large_size - 1] = '\0';

    language_message_t* msg = language_message_create(large_content, large_size - 1, 512);
    EXPECT_NE(msg, nullptr);

    if (msg) {
        EXPECT_EQ(msg->semantic_size, large_size - 1);
        language_message_destroy(msg);
    }

    delete[] large_content;
}

TEST_F(LanguageProductionBridgeTest, SmallEncodingDimension) {
    language_bridge_config_t config = language_bridge_default_config();
    config.encoding_dim = 64;  // Small encoding dimension

    language_production_bridge_t* small_bridge = language_bridge_create(&config);
    ASSERT_NE(small_bridge, nullptr);

    uint32_t vec_size = 512;
    float* thought_vector = create_test_thought_vector(vec_size);

    language_message_t* msg = language_message_create("test", 4, 64);
    ASSERT_NE(msg, nullptr);

    int result = language_bridge_encode_thought(small_bridge, thought_vector, vec_size, msg);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    language_message_destroy(msg);
    delete[] thought_vector;
    language_bridge_destroy(small_bridge);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
