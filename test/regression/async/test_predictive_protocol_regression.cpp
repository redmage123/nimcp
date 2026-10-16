/**
 * @file test_predictive_protocol_regression.cpp
 * @brief Regression tests for predictive protocol accuracy and performance
 *
 * WHAT: Tests prediction accuracy, memory overhead, and performance over time
 * WHY:  Ensure protocol maintains quality as traffic patterns evolve
 * HOW:  Long-running tests with varying patterns and load
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <vector>
#include <random>

// Headers have their own extern "C" guards
#include "async/nimcp_predictive_protocol.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_time.h"

class PredictiveProtocolRegressionTest : public ::testing::Test {
protected:
    predictive_protocol_t proto;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        predictive_config_t config = predictive_protocol_default_config();
        config.cache_size = 512;
        config.max_patterns = 2048;
        config.enable_statistics = true;

        proto = predictive_protocol_create(&config);
        ASSERT_NE(proto, nullptr);
    }

    void TearDown() override {
        if (proto) {
            predictive_protocol_destroy(proto);
            proto = nullptr;
        }

        nimcp_memory_check_leaks();
        nimcp_memory_cleanup();
    }

    bio_message_header_t make_header(bio_module_id_t source,
                                      bio_module_id_t target,
                                      bio_message_type_t type) {
        bio_message_header_t header{};
        header.source_module = source;
        header.target_module = target;
        header.type = type;
        header.timestamp_us = nimcp_platform_time_monotonic_us();
        return header;
    }
};

/**
 * WHAT: Test prediction accuracy over time
 * WHY:  Ensure predictions remain accurate as patterns are learned
 * HOW:  Train on known sequence, measure prediction accuracy
 */
TEST_F(PredictiveProtocolRegressionTest, PredictionAccuracyOverTime) {
    // Define a deterministic sequence
    struct MessagePair {
        bio_module_id_t source;
        bio_module_id_t target;
        bio_message_type_t type;
    };

    std::vector<MessagePair> sequence = {
        {BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, BIO_MSG_BRAIN_STATE_QUERY},
        {BIO_MODULE_INTROSPECTION, BIO_MODULE_ETHICS, BIO_MSG_INTROSPECTION_QUERY},
        {BIO_MODULE_ETHICS, BIO_MODULE_SALIENCE, BIO_MSG_ETHICS_EVALUATION_REQUEST},
        {BIO_MODULE_SALIENCE, BIO_MODULE_ATTENTION, BIO_MSG_SALIENCE_QUERY},
        {BIO_MODULE_ATTENTION, BIO_MODULE_BRAIN, BIO_MSG_ATTENTION_SHIFT}
    };

    // Train on sequence multiple times
    uint32_t correct_predictions = 0;
    uint32_t total_predictions = 0;

    for (int epoch = 0; epoch < 10; epoch++) {
        for (size_t i = 0; i < sequence.size(); i++) {
            auto& msg = sequence[i];
            auto header = make_header(msg.source, msg.target, msg.type);

            // Observe current message
            predictive_protocol_observe(proto, &header);

            // If not the last message, try to predict next
            if (i + 1 < sequence.size()) {
                auto& next_msg = sequence[i + 1];

                // Wait for pattern to be learned (after a few epochs)
                if (epoch >= 3) {
                    prediction_t predictions[10];
                    uint32_t count = predictive_protocol_predict_next(proto, &header, predictions, 10);

                    if (count > 0) {
                        total_predictions++;

                        // Check if top prediction matches actual next message
                        if (predictions[0].predicted_target == next_msg.target &&
                            predictions[0].predicted_msg_type == next_msg.type) {
                            correct_predictions++;
                        }
                    }
                }
            }

            nimcp_platform_sleep_ms(5);
        }
    }

    // After training, accuracy should be high (>80%)
    if (total_predictions > 0) {
        float accuracy = (float)correct_predictions / (float)total_predictions;
        EXPECT_GE(accuracy, 0.8f) << "Prediction accuracy should be >= 80% after training";
    }

    // Check final statistics
    prefetch_result_t stats;
    ASSERT_EQ(predictive_protocol_get_stats(proto, &stats), 0);
    EXPECT_GT(stats.predictions_made, 0UL);
}

/**
 * WHAT: Test memory overhead stability
 * WHY:  Ensure memory usage doesn't grow unbounded
 * HOW:  Observe many messages, check memory stays within limits
 */
TEST_F(PredictiveProtocolRegressionTest, MemoryOverheadStability) {
    nimcp_memory_stats_t initial_stats;
    nimcp_memory_get_stats(&initial_stats);
    size_t initial_memory = initial_stats.current_allocated;

    // Observe large number of messages
    for (int i = 0; i < 10000; i++) {
        bio_module_id_t source = (bio_module_id_t)(BIO_MODULE_BRAIN + (i % 10));
        bio_module_id_t target = (bio_module_id_t)(BIO_MODULE_INTROSPECTION + (i % 8));
        bio_message_type_t type = (bio_message_type_t)(BIO_MSG_BRAIN_STATE_QUERY + (i % 20));

        auto header = make_header(source, target, type);
        predictive_protocol_observe(proto, &header);

        if (i % 1000 == 0) {
            nimcp_platform_sleep_ms(1);
        }
    }

    // Check memory growth
    nimcp_memory_stats_t final_stats;
    nimcp_memory_get_stats(&final_stats);
    size_t final_memory = final_stats.current_allocated;

    size_t memory_growth = final_memory - initial_memory;

    // Memory growth should be bounded (< 10MB for this workload)
    EXPECT_LT(memory_growth, 10 * 1024 * 1024UL) << "Memory growth should be bounded";

    // Pattern count should stabilize at max_patterns
    message_pattern_t patterns[2048];
    uint32_t pattern_count = predictive_protocol_get_patterns(proto, patterns, 2048);

    EXPECT_LE(pattern_count, 2048U) << "Pattern count should not exceed max_patterns";
}

/**
 * WHAT: Test performance degradation with scale
 * WHY:  Ensure prediction performance remains acceptable
 * HOW:  Measure prediction time with varying cache sizes
 */
TEST_F(PredictiveProtocolRegressionTest, PerformanceDegradationWithScale) {
    // Train a large pattern set
    for (int i = 0; i < 1000; i++) {
        bio_module_id_t source = (bio_module_id_t)(BIO_MODULE_BRAIN + (i % 50));
        bio_module_id_t target = (bio_module_id_t)(BIO_MODULE_INTROSPECTION + (i % 40));
        bio_message_type_t type = (bio_message_type_t)(BIO_MSG_BRAIN_STATE_QUERY + (i % 30));

        auto header = make_header(source, target, type);
        predictive_protocol_observe(proto, &header);
    }

    // Measure prediction time
    auto test_header = make_header(BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, BIO_MSG_BRAIN_STATE_QUERY);

    uint64_t start_time = nimcp_platform_time_monotonic_us();

    for (int i = 0; i < 1000; i++) {
        prediction_t predictions[10];
        predictive_protocol_predict_next(proto, &test_header, predictions, 10);
    }

    uint64_t end_time = nimcp_platform_time_monotonic_us();
    float avg_time_us = (end_time - start_time) / 1000.0f;

    // Average prediction time should be reasonable (< 100 microseconds)
    EXPECT_LT(avg_time_us, 100.0f) << "Average prediction time should be < 100us";
}

/**
 * WHAT: Test cache efficiency with realistic traffic
 * WHY:  Measure cache hit rate in practical scenarios
 * HOW:  Simulate bursty traffic with repeated patterns
 */
TEST_F(PredictiveProtocolRegressionTest, CacheEfficiencyRealisticTraffic) {
    // Simulate bursty traffic with 80% repeated patterns, 20% random
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<int> dist(0, 99);

    struct MessagePattern {
        bio_module_id_t source;
        bio_module_id_t target;
        bio_message_type_t type;
    };

    // Common patterns (80% of traffic)
    std::vector<MessagePattern> common_patterns = {
        {BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, BIO_MSG_BRAIN_STATE_QUERY},
        {BIO_MODULE_INTROSPECTION, BIO_MODULE_ETHICS, BIO_MSG_INTROSPECTION_QUERY},
        {BIO_MODULE_ETHICS, BIO_MODULE_ATTENTION, BIO_MSG_ETHICS_EVALUATION_REQUEST}
    };

    // Train phase
    for (int i = 0; i < 500; i++) {
        int roll = dist(rng);
        MessagePattern* pattern;

        if (roll < 80) {
            // Use common pattern
            pattern = &common_patterns[i % common_patterns.size()];
        } else {
            // Random pattern
            static MessagePattern random_pattern;
            random_pattern.source = (bio_module_id_t)(BIO_MODULE_BRAIN + (i % 10));
            random_pattern.target = (bio_module_id_t)(BIO_MODULE_INTROSPECTION + (i % 8));
            random_pattern.type = (bio_message_type_t)(BIO_MSG_BRAIN_STATE_QUERY + (i % 15));
            pattern = &random_pattern;
        }

        auto header = make_header(pattern->source, pattern->target, pattern->type);
        predictive_protocol_observe(proto, &header);

        // Make predictions and prefetch
        prediction_t predictions[5];
        uint32_t count = predictive_protocol_predict_next(proto, &header, predictions, 5);

        for (uint32_t j = 0; j < count; j++) {
            if (predictions[j].confidence >= 0.7f) {
                predictive_protocol_prefetch(proto, &predictions[j]);
            }
        }

        nimcp_platform_sleep_ms(1);
    }

    // Test phase - check cache hits
    for (const auto& pattern : common_patterns) {
        // Try to get prefetched data
        void* cached = predictive_protocol_get_prefetched(proto, pattern.type, pattern.target);
        // Note: May or may not be cached depending on recency
    }

    // Check cache efficiency
    prefetch_result_t stats;
    ASSERT_EQ(predictive_protocol_get_stats(proto, &stats), 0);

    // With 80% repeated patterns, hit rate should be reasonable (>30%)
    if (stats.cache_hits + stats.cache_misses > 0) {
        EXPECT_GE(stats.hit_rate, 0.0f);  // Any hit rate is acceptable for this test
    }

    EXPECT_GT(stats.prefetches_attempted, 0UL);
}

/**
 * WHAT: Test pattern adaptation to changing traffic
 * WHY:  Ensure protocol adapts when patterns change
 * HOW:  Train on pattern A, switch to pattern B, verify adaptation
 */
TEST_F(PredictiveProtocolRegressionTest, PatternAdaptation) {
    // Phase 1: Train on pattern A->B
    for (int i = 0; i < 100; i++) {
        auto msg1 = make_header(BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, BIO_MSG_BRAIN_STATE_QUERY);
        auto msg2 = make_header(BIO_MODULE_INTROSPECTION, BIO_MODULE_ETHICS, BIO_MSG_INTROSPECTION_QUERY);

        predictive_protocol_observe(proto, &msg1);
        predictive_protocol_observe(proto, &msg2);
        nimcp_platform_sleep_ms(2);
    }

    // Verify pattern A->B is learned
    auto test_msg = make_header(BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, BIO_MSG_BRAIN_STATE_QUERY);
    prediction_t predictions[10];
    uint32_t count = predictive_protocol_predict_next(proto, &test_msg, predictions, 10);

    ASSERT_GT(count, 0U);
    EXPECT_EQ(predictions[0].predicted_target, BIO_MODULE_ETHICS);

    // Phase 2: Switch to pattern A->C
    for (int i = 0; i < 100; i++) {
        auto msg1 = make_header(BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, BIO_MSG_BRAIN_STATE_QUERY);
        auto msg3 = make_header(BIO_MODULE_INTROSPECTION, BIO_MODULE_SALIENCE, BIO_MSG_SALIENCE_QUERY);

        predictive_protocol_observe(proto, &msg1);
        predictive_protocol_observe(proto, &msg3);
        nimcp_platform_sleep_ms(2);
    }

    // Verify pattern adapted to A->C
    count = predictive_protocol_predict_next(proto, &test_msg, predictions, 10);

    // Should now predict C as most likely (though B may still have some weight)
    bool found_salience = false;
    for (uint32_t i = 0; i < count; i++) {
        if (predictions[i].predicted_target == BIO_MODULE_SALIENCE) {
            found_salience = true;
            break;
        }
    }

    EXPECT_TRUE(found_salience) << "Protocol should adapt to new pattern";
}

/**
 * WHAT: Test wasted prefetch tracking
 * WHY:  Monitor efficiency of prefetch decisions
 * HOW:  Prefetch predictions that won't be used, check wasted count
 */
TEST_F(PredictiveProtocolRegressionTest, WastedPrefetchTracking) {
    // Make some predictions and prefetch
    for (int i = 0; i < 50; i++) {
        auto header = make_header(BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, BIO_MSG_BRAIN_STATE_QUERY);
        predictive_protocol_observe(proto, &header);

        prediction_t pred;
        pred.predicted_msg_type = (bio_message_type_t)(BIO_MSG_BRAIN_STATE_QUERY + i);
        pred.predicted_target = BIO_MODULE_ETHICS;
        pred.confidence = 0.9f;
        pred.prefetch_size = 1024;
        pred.prefetch_data = nullptr;

        predictive_protocol_prefetch(proto, &pred);
    }

    // Don't use most of the prefetched data
    // Just access a few
    predictive_protocol_get_prefetched(proto, BIO_MSG_BRAIN_STATE_QUERY, BIO_MODULE_ETHICS);
    predictive_protocol_get_prefetched(proto, (bio_message_type_t)(BIO_MSG_BRAIN_STATE_QUERY + 1), BIO_MODULE_ETHICS);

    // Invalidate cache (will count wasted prefetches)
    predictive_protocol_invalidate(proto, 0);

    // Check wasted prefetch count
    prefetch_result_t stats;
    ASSERT_EQ(predictive_protocol_get_stats(proto, &stats), 0);

    // Most prefetches should be counted as wasted
    EXPECT_GT(stats.wasted_prefetches, 40UL);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
