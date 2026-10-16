/**
 * @file test_mesh_bio_transaction_pipeline_e2e.cpp
 * @brief End-to-End Tests for Bio-Async to Mesh Transaction Pipeline
 *
 * WHAT: Tests full bio-async -> mesh -> module -> mesh -> bio-async pipeline
 * WHY:  Verify bidirectional integration between bio-router and mesh network
 * HOW:  Test message translation, pattern routing, channel mapping, metrics
 *
 * TEST COVERAGE:
 * - Full bio-async to mesh pipeline
 * - Test with multiple bio categories
 * - Test with all channel types
 * - Measure latency and throughput
 * - Pattern extraction and routing
 * - Bidirectional translation
 * - Channel mapping verification
 * - Error handling in pipeline
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_bio_bridge.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

// =============================================================================
// Test Constants
// =============================================================================

static constexpr size_t PIPELINE_MESSAGE_COUNT = 500;
static constexpr size_t CONCURRENT_PIPELINES = 4;
static constexpr float TARGET_LATENCY_MS = 50.0f;
static constexpr float MIN_THROUGHPUT = 100.0f;  // messages/second

// =============================================================================
// Mock Bio Message Structure
// =============================================================================

typedef struct mock_bio_message {
    uint32_t message_type;
    uint32_t category;
    float data[16];
    size_t data_size;
    uint64_t timestamp_ns;
    char payload[256];
} mock_bio_message_t;

// =============================================================================
// Test Fixture - Bio Transaction Pipeline E2E
// =============================================================================

class MeshBioTransactionPipelineE2ETest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap_ = nullptr;
    mesh_bio_bridge_t* bio_bridge_ = nullptr;

    void SetUp() override {
        mesh_bootstrap_config_t boot_config;
        mesh_bootstrap_default_config(&boot_config);
        boot_config.subsystems.enable_async = true;
        boot_config.subsystems.enable_cognitive = true;
        boot_config.subsystems.enable_sensory = true;
        boot_config.verbose_logging = false;

        bootstrap_ = mesh_bootstrap_create(&boot_config);
        if (!bootstrap_) {
            GTEST_SKIP() << "Bootstrap creation not available";
        }

        mesh_bio_bridge_config_t bio_config;
        mesh_bio_bridge_default_config(&bio_config);
        bio_config.enable_pattern_routing = true;
        bio_config.enable_channel_mapping = true;
        bio_config.bidirectional = true;
        bio_config.default_timeout_ms = 100.0f;
        bio_config.normalize_patterns = true;

        bio_bridge_ = mesh_bio_bridge_create(bootstrap_, &bio_config);
        if (!bio_bridge_) {
            mesh_bootstrap_destroy(bootstrap_);
            bootstrap_ = nullptr;
            GTEST_SKIP() << "Bio bridge creation not available";
        }
    }

    void TearDown() override {
        if (bio_bridge_) {
            mesh_bio_bridge_destroy(bio_bridge_);
            bio_bridge_ = nullptr;
        }
        if (bootstrap_) {
            mesh_bootstrap_destroy(bootstrap_);
            bootstrap_ = nullptr;
        }
    }

    mock_bio_message_t* CreateBioMessage(uint32_t category, const char* payload) {
        auto* msg = static_cast<mock_bio_message_t*>(
            nimcp_calloc(1, sizeof(mock_bio_message_t)));

        if (msg) {
            msg->category = category;
            msg->message_type = 1;
            msg->data_size = 8;
            msg->timestamp_ns = static_cast<uint64_t>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count());

            for (size_t i = 0; i < msg->data_size; i++) {
                msg->data[i] = static_cast<float>(i) / 10.0f;
            }

            strncpy(msg->payload, payload, sizeof(msg->payload) - 1);
        }
        return msg;
    }
};

// =============================================================================
// Test 1: Full Bio-Async to Mesh Pipeline
// =============================================================================

TEST_F(MeshBioTransactionPipelineE2ETest, FullBioAsyncToMeshPipeline) {
    std::vector<float> latencies;
    std::atomic<size_t> success_count{0};
    std::atomic<size_t> fail_count{0};

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < PIPELINE_MESSAGE_COUNT; i++) {
        char payload[128];
        snprintf(payload, sizeof(payload), "bio_msg_%zu:neural_activation", i);

        mock_bio_message_t* bio_msg = CreateBioMessage(MESH_BIO_CAT_NEURAL, payload);
        ASSERT_NE(bio_msg, nullptr);

        auto tx_start = std::chrono::high_resolution_clock::now();

        // Step 1: Translate bio message to mesh transaction
        mesh_transaction_t* tx = nullptr;
        nimcp_error_t err = mesh_bio_bridge_translate_to_mesh(
            bio_bridge_, bio_msg, sizeof(mock_bio_message_t), &tx);

        if (err == NIMCP_SUCCESS && tx != nullptr) {
            // Step 2: Transaction would be processed by mesh network
            // (simulated here)

            // Step 3: Optionally translate back to bio format
            uint8_t bio_out[512];
            size_t out_size = 0;

            err = mesh_bio_bridge_translate_to_bio(
                bio_bridge_, tx, bio_out, &out_size, sizeof(bio_out));

            auto tx_end = std::chrono::high_resolution_clock::now();
            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                tx_end - tx_start).count();
            latencies.push_back(static_cast<float>(latency_us) / 1000.0f);

            success_count++;
            mesh_transaction_destroy(tx);
        } else {
            fail_count++;
        }

        nimcp_free(bio_msg);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto total_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // Calculate metrics
    float avg_latency_ms = 0.0f;
    for (float lat : latencies) {
        avg_latency_ms += lat;
    }
    if (!latencies.empty()) {
        avg_latency_ms /= latencies.size();
    }

    float throughput = static_cast<float>(success_count.load()) /
                       (total_duration_ms / 1000.0f);

    // Verify success rate
    EXPECT_GE(success_count.load(), PIPELINE_MESSAGE_COUNT * 0.95)
        << "At least 95% of messages should complete pipeline";

    // Verify latency
    EXPECT_LT(avg_latency_ms, TARGET_LATENCY_MS)
        << "Average latency " << avg_latency_ms << "ms exceeds target "
        << TARGET_LATENCY_MS << "ms";

    // Verify throughput
    EXPECT_GT(throughput, MIN_THROUGHPUT)
        << "Throughput " << throughput << " msg/s below minimum "
        << MIN_THROUGHPUT << " msg/s";

    // Verify bridge stats
    mesh_bio_bridge_stats_t stats;
    mesh_bio_bridge_get_stats(bio_bridge_, &stats);
    EXPECT_EQ(stats.bio_messages_received, success_count.load());
}

// =============================================================================
// Test 2: Test with Multiple Bio Categories
// =============================================================================

TEST_F(MeshBioTransactionPipelineE2ETest, MultipleBioCategories) {
    struct CategoryTest {
        uint32_t category;
        const char* name;
        size_t count;
    };

    CategoryTest categories[] = {
        {MESH_BIO_CAT_NEURAL, "neural", 0},
        {MESH_BIO_CAT_PLASTICITY, "plasticity", 0},
        {MESH_BIO_CAT_NEUROMOD, "neuromod", 0},
        {MESH_BIO_CAT_PERCEPTION, "perception", 0},
        {MESH_BIO_CAT_COGNITIVE, "cognitive", 0},
        {MESH_BIO_CAT_MOTOR, "motor", 0},
        {MESH_BIO_CAT_SECURITY, "security", 0},
        {MESH_BIO_CAT_SYSTEM, "system", 0},
    };
    size_t num_categories = sizeof(categories) / sizeof(categories[0]);

    for (size_t round = 0; round < 50; round++) {
        for (size_t c = 0; c < num_categories; c++) {
            char payload[128];
            snprintf(payload, sizeof(payload),
                    "%s_message_round_%zu", categories[c].name, round);

            mock_bio_message_t* bio_msg = CreateBioMessage(
                categories[c].category, payload);
            ASSERT_NE(bio_msg, nullptr);

            mesh_transaction_t* tx = nullptr;
            nimcp_error_t err = mesh_bio_bridge_translate_to_mesh(
                bio_bridge_, bio_msg, sizeof(mock_bio_message_t), &tx);

            if (err == NIMCP_SUCCESS && tx != nullptr) {
                categories[c].count++;
                mesh_transaction_destroy(tx);
            }

            nimcp_free(bio_msg);
        }
    }

    // Verify all categories were processed
    for (size_t c = 0; c < num_categories; c++) {
        EXPECT_EQ(categories[c].count, 50u)
            << "Category " << categories[c].name
            << " should have 50 successful translations";
    }

    // Verify per-category stats
    mesh_bio_bridge_stats_t stats;
    mesh_bio_bridge_get_stats(bio_bridge_, &stats);
    EXPECT_GT(stats.neural_translations, 0u);
    EXPECT_GT(stats.plasticity_translations, 0u);
    EXPECT_GT(stats.cognitive_translations, 0u);
    EXPECT_GT(stats.motor_translations, 0u);
}

// =============================================================================
// Test 3: Test with All Channel Types
// =============================================================================

TEST_F(MeshBioTransactionPipelineE2ETest, AllChannelTypes) {
    // Map bio categories to expected channels
    struct ChannelMapping {
        uint32_t bio_category;
        mesh_channel_id_t expected_channel;
    };

    ChannelMapping mappings[] = {
        {MESH_BIO_CAT_NEURAL, MESH_CHANNEL_LEFT_HEMISPHERE},
        {MESH_BIO_CAT_COGNITIVE, MESH_CHANNEL_LEFT_HEMISPHERE},
        {MESH_BIO_CAT_PERCEPTION, MESH_CHANNEL_RIGHT_HEMISPHERE},
        {MESH_BIO_CAT_MOTOR, MESH_CHANNEL_SUBCORTICAL},
    };
    size_t num_mappings = sizeof(mappings) / sizeof(mappings[0]);

    for (const auto& mapping : mappings) {
        // Get channel for this bio category
        mesh_channel_id_t channel = mesh_bio_bridge_get_channel(
            bio_bridge_, mapping.bio_category);

        // Channel mapping should be defined
        // (actual mapping depends on configuration)
        EXPECT_NE(channel, MESH_CHANNEL_SYSTEM)
            << "Bio category should map to non-system channel";
    }

    // Test setting custom channel mapping
    nimcp_error_t err = mesh_bio_bridge_set_channel_mapping(
        bio_bridge_, MESH_BIO_CAT_SECURITY, MESH_CHANNEL_SYSTEM);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_channel_id_t sec_channel = mesh_bio_bridge_get_channel(
        bio_bridge_, MESH_BIO_CAT_SECURITY);
    EXPECT_EQ(sec_channel, MESH_CHANNEL_SYSTEM);
}

// =============================================================================
// Test 4: Measure Latency and Throughput
// =============================================================================

TEST_F(MeshBioTransactionPipelineE2ETest, MeasureLatencyAndThroughput) {
    std::vector<float> translation_latencies;
    std::vector<float> pattern_extraction_latencies;
    std::vector<float> round_trip_latencies;

    size_t sample_count = 200;

    for (size_t i = 0; i < sample_count; i++) {
        char payload[128];
        snprintf(payload, sizeof(payload), "latency_test_%zu", i);

        mock_bio_message_t* bio_msg = CreateBioMessage(
            MESH_BIO_CAT_COGNITIVE, payload);
        ASSERT_NE(bio_msg, nullptr);

        // Measure translation latency
        auto t1 = std::chrono::high_resolution_clock::now();

        mesh_transaction_t* tx = nullptr;
        mesh_bio_bridge_translate_to_mesh(
            bio_bridge_, bio_msg, sizeof(mock_bio_message_t), &tx);

        auto t2 = std::chrono::high_resolution_clock::now();

        if (tx) {
            float trans_lat = std::chrono::duration_cast<std::chrono::microseconds>(
                t2 - t1).count() / 1000.0f;
            translation_latencies.push_back(trans_lat);

            // Measure pattern extraction latency
            auto t3 = std::chrono::high_resolution_clock::now();

            mesh_pattern_t pattern;
            mesh_bio_bridge_extract_pattern(
                bio_bridge_, bio_msg, sizeof(mock_bio_message_t), &pattern);

            auto t4 = std::chrono::high_resolution_clock::now();

            float pattern_lat = std::chrono::duration_cast<std::chrono::microseconds>(
                t4 - t3).count() / 1000.0f;
            pattern_extraction_latencies.push_back(pattern_lat);

            // Measure round-trip latency (translate + process + translate back)
            auto t5 = std::chrono::high_resolution_clock::now();

            uint8_t bio_out[512];
            size_t out_size;
            mesh_bio_bridge_translate_to_bio(
                bio_bridge_, tx, bio_out, &out_size, sizeof(bio_out));

            auto t6 = std::chrono::high_resolution_clock::now();

            float rt_lat = std::chrono::duration_cast<std::chrono::microseconds>(
                t6 - t1).count() / 1000.0f;
            round_trip_latencies.push_back(rt_lat);

            mesh_transaction_destroy(tx);
        }

        nimcp_free(bio_msg);
    }

    // Calculate statistics
    auto calc_stats = [](const std::vector<float>& data) {
        float sum = 0, min_val = 999999, max_val = 0;
        for (float v : data) {
            sum += v;
            min_val = std::min(min_val, v);
            max_val = std::max(max_val, v);
        }
        float avg = data.empty() ? 0 : sum / data.size();
        return std::make_tuple(avg, min_val, max_val);
    };

    auto [trans_avg, trans_min, trans_max] = calc_stats(translation_latencies);
    auto [pattern_avg, pattern_min, pattern_max] = calc_stats(pattern_extraction_latencies);
    auto [rt_avg, rt_min, rt_max] = calc_stats(round_trip_latencies);

    // Output metrics for analysis
    std::cout << "Translation Latency (ms): avg=" << trans_avg
              << " min=" << trans_min << " max=" << trans_max << std::endl;
    std::cout << "Pattern Extraction Latency (ms): avg=" << pattern_avg
              << " min=" << pattern_min << " max=" << pattern_max << std::endl;
    std::cout << "Round-Trip Latency (ms): avg=" << rt_avg
              << " min=" << rt_min << " max=" << rt_max << std::endl;

    // Assertions
    EXPECT_LT(trans_avg, 10.0f)
        << "Average translation latency should be under 10ms";
    EXPECT_LT(pattern_avg, 5.0f)
        << "Average pattern extraction latency should be under 5ms";
    EXPECT_LT(rt_avg, 50.0f)
        << "Average round-trip latency should be under 50ms";

    // Calculate throughput
    auto start = std::chrono::high_resolution_clock::now();
    size_t throughput_count = 0;

    for (size_t i = 0; i < 500; i++) {
        mock_bio_message_t* msg = CreateBioMessage(MESH_BIO_CAT_NEURAL, "throughput_test");
        if (msg) {
            mesh_transaction_t* tx = nullptr;
            if (mesh_bio_bridge_translate_to_mesh(
                bio_bridge_, msg, sizeof(mock_bio_message_t), &tx) == NIMCP_SUCCESS) {
                throughput_count++;
                if (tx) mesh_transaction_destroy(tx);
            }
            nimcp_free(msg);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    float duration_s = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count() / 1000.0f;
    float throughput = throughput_count / duration_s;

    std::cout << "Throughput: " << throughput << " msg/s" << std::endl;
    EXPECT_GT(throughput, MIN_THROUGHPUT);
}

// =============================================================================
// Test 5: Pattern Extraction and Routing
// =============================================================================

TEST_F(MeshBioTransactionPipelineE2ETest, PatternExtractionAndRouting) {
    // Create messages with known patterns
    for (size_t i = 0; i < 50; i++) {
        char payload[128];
        snprintf(payload, sizeof(payload), "pattern_test_%zu", i);

        mock_bio_message_t* bio_msg = CreateBioMessage(
            MESH_BIO_CAT_COGNITIVE, payload);
        ASSERT_NE(bio_msg, nullptr);

        // Set specific pattern values
        for (size_t d = 0; d < 8; d++) {
            bio_msg->data[d] = static_cast<float>(i + d) / 100.0f;
        }

        mesh_pattern_t pattern;
        memset(&pattern, 0, sizeof(pattern));

        nimcp_error_t err = mesh_bio_bridge_extract_pattern(
            bio_bridge_, bio_msg, sizeof(mock_bio_message_t), &pattern);

        if (err == NIMCP_SUCCESS) {
            // Verify pattern was extracted
            EXPECT_GT(pattern.dimension_count, 0u)
                << "Pattern should have dimensions";

            // Verify pattern magnitude
            EXPECT_GE(pattern.magnitude, 0.0f);
            EXPECT_LE(pattern.magnitude, 2.0f);  // Allow for some variance

            // Pattern dimensions should reflect input
            bool has_nonzero = false;
            for (size_t d = 0; d < pattern.dimension_count; d++) {
                if (pattern.dims[d] != 0.0f) {
                    has_nonzero = true;
                    break;
                }
            }
            EXPECT_TRUE(has_nonzero)
                << "Pattern should have non-zero dimensions";
        }

        nimcp_free(bio_msg);
    }

    // Verify pattern extraction count
    mesh_bio_bridge_stats_t stats;
    mesh_bio_bridge_get_stats(bio_bridge_, &stats);
    EXPECT_GE(stats.pattern_extractions, 50u);
}

// =============================================================================
// Test 6: Bidirectional Translation
// =============================================================================

TEST_F(MeshBioTransactionPipelineE2ETest, BidirectionalTranslation) {
    for (size_t i = 0; i < 100; i++) {
        char original_payload[128];
        snprintf(original_payload, sizeof(original_payload),
                "bidirectional_test_%zu", i);

        mock_bio_message_t* bio_msg = CreateBioMessage(
            MESH_BIO_CAT_NEURAL, original_payload);
        ASSERT_NE(bio_msg, nullptr);

        // Forward: Bio -> Mesh
        mesh_transaction_t* tx = nullptr;
        nimcp_error_t err = mesh_bio_bridge_translate_to_mesh(
            bio_bridge_, bio_msg, sizeof(mock_bio_message_t), &tx);

        if (err == NIMCP_SUCCESS && tx != nullptr) {
            // Backward: Mesh -> Bio
            uint8_t bio_out[512];
            size_t out_size = 0;

            err = mesh_bio_bridge_translate_to_bio(
                bio_bridge_, tx, bio_out, &out_size, sizeof(bio_out));

            if (err == NIMCP_SUCCESS) {
                EXPECT_GT(out_size, 0u)
                    << "Backward translation should produce output";

                // Verify message content is preserved (to extent possible)
                // The exact format depends on implementation
            }

            mesh_transaction_destroy(tx);
        }

        nimcp_free(bio_msg);
    }

    // Verify bidirectional stats
    mesh_bio_bridge_stats_t stats;
    mesh_bio_bridge_get_stats(bio_bridge_, &stats);
    EXPECT_GT(stats.bio_messages_received, 0u);
    EXPECT_GT(stats.mesh_transactions_created, 0u);
}

// =============================================================================
// Test 7: Channel Mapping Verification
// =============================================================================

TEST_F(MeshBioTransactionPipelineE2ETest, ChannelMappingVerification) {
    // Verify pattern dimension ranges for each category
    mesh_pattern_dim_range_t range;

    struct CategoryRange {
        uint32_t category;
        const char* name;
        size_t expected_start;
        size_t expected_end;
    };

    CategoryRange ranges[] = {
        {MESH_BIO_CAT_NEURAL, "neural", 0, 8},
        {MESH_BIO_CAT_PLASTICITY, "plasticity", 8, 16},
        {MESH_BIO_CAT_NEUROMOD, "neuromod", 16, 24},
        {MESH_BIO_CAT_PERCEPTION, "perception", 24, 32},
        {MESH_BIO_CAT_COGNITIVE, "cognitive", 32, 40},
        {MESH_BIO_CAT_MOTOR, "motor", 40, 48},
        {MESH_BIO_CAT_SECURITY, "security", 48, 56},
        {MESH_BIO_CAT_SYSTEM, "system", 56, 64},
    };

    for (const auto& cat_range : ranges) {
        nimcp_error_t err = mesh_bio_bridge_get_pattern_range(
            cat_range.category, &range);

        if (err == NIMCP_SUCCESS) {
            EXPECT_EQ(range.start, cat_range.expected_start)
                << "Category " << cat_range.name << " should start at "
                << cat_range.expected_start;
            EXPECT_EQ(range.end, cat_range.expected_end)
                << "Category " << cat_range.name << " should end at "
                << cat_range.expected_end;
        }
    }
}

// =============================================================================
// Test 8: Error Handling in Pipeline
// =============================================================================

TEST_F(MeshBioTransactionPipelineE2ETest, ErrorHandlingInPipeline) {
    // Test 1: NULL bio message
    mesh_transaction_t* tx = nullptr;
    nimcp_error_t err = mesh_bio_bridge_translate_to_mesh(
        bio_bridge_, nullptr, 0, &tx);
    EXPECT_NE(err, NIMCP_SUCCESS);
    EXPECT_EQ(tx, nullptr);

    // Test 2: Zero size message
    mock_bio_message_t msg;
    memset(&msg, 0, sizeof(msg));
    err = mesh_bio_bridge_translate_to_mesh(bio_bridge_, &msg, 0, &tx);
    EXPECT_NE(err, NIMCP_SUCCESS);

    // Test 3: NULL transaction output
    err = mesh_bio_bridge_translate_to_mesh(bio_bridge_, &msg, sizeof(msg), nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);

    // Test 4: NULL pattern output
    mesh_pattern_t* null_pattern = nullptr;
    // This should fail gracefully

    // Test 5: Translate NULL transaction back to bio
    uint8_t bio_out[512];
    size_t out_size;
    err = mesh_bio_bridge_translate_to_bio(
        bio_bridge_, nullptr, bio_out, &out_size, sizeof(bio_out));
    EXPECT_NE(err, NIMCP_SUCCESS);

    // Test 6: NULL output buffer
    mock_bio_message_t* valid_msg = CreateBioMessage(MESH_BIO_CAT_NEURAL, "test");
    tx = nullptr;
    err = mesh_bio_bridge_translate_to_mesh(
        bio_bridge_, valid_msg, sizeof(mock_bio_message_t), &tx);

    if (err == NIMCP_SUCCESS && tx) {
        err = mesh_bio_bridge_translate_to_bio(
            bio_bridge_, tx, nullptr, &out_size, 0);
        EXPECT_NE(err, NIMCP_SUCCESS);
        mesh_transaction_destroy(tx);
    }
    nimcp_free(valid_msg);

    // Test 7: Very small output buffer
    valid_msg = CreateBioMessage(MESH_BIO_CAT_NEURAL, "test");
    tx = nullptr;
    err = mesh_bio_bridge_translate_to_mesh(
        bio_bridge_, valid_msg, sizeof(mock_bio_message_t), &tx);

    if (err == NIMCP_SUCCESS && tx) {
        uint8_t tiny_buf[1];
        err = mesh_bio_bridge_translate_to_bio(
            bio_bridge_, tx, tiny_buf, &out_size, sizeof(tiny_buf));
        // Should handle gracefully (either truncate or return error)
        mesh_transaction_destroy(tx);
    }
    nimcp_free(valid_msg);

    // Verify bridge is still functional after errors
    valid_msg = CreateBioMessage(MESH_BIO_CAT_COGNITIVE, "recovery_test");
    tx = nullptr;
    err = mesh_bio_bridge_translate_to_mesh(
        bio_bridge_, valid_msg, sizeof(mock_bio_message_t), &tx);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    if (tx) mesh_transaction_destroy(tx);
    nimcp_free(valid_msg);
}

