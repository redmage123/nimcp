/**
 * @file test_predictive_protocol.cpp
 * @brief Unit tests for predictive communication protocol
 *
 * WHAT: Tests pattern learning, Markov chain prediction, and LRU caching
 * WHY:  Ensure correct predictive behavior and cache management
 * HOW:  Feed known patterns, verify predictions, check cache hits/misses
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "async/nimcp_predictive_protocol.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_time.h"

class PredictiveProtocolTest : public ::testing::Test {
protected:
    predictive_protocol_t proto;
    predictive_config_t config;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        config = predictive_protocol_default_config();
        config.cache_size = 16;
        config.learning_rate = 0.2f;
        config.min_confidence = 0.5f;
        config.max_patterns = 128;
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
 * WHAT: Test basic protocol creation and destruction
 * WHY:  Ensure lifecycle management works correctly
 * HOW:  Create and destroy protocol, check no leaks
 */
TEST_F(PredictiveProtocolTest, BasicLifecycle) {
    // Already created in SetUp, will be destroyed in TearDown
    EXPECT_NE(proto, nullptr);

    // Get initial stats
    prefetch_result_t stats;
    ASSERT_EQ(predictive_protocol_get_stats(proto, &stats), 0);
    EXPECT_EQ(stats.predictions_made, 0UL);
    EXPECT_EQ(stats.cache_hits, 0UL);
    EXPECT_EQ(stats.cache_misses, 0UL);
}

/**
 * WHAT: Test pattern learning from observed messages
 * WHY:  Ensure protocol learns communication patterns
 * HOW:  Observe repeated pattern, verify it's learned
 */
TEST_F(PredictiveProtocolTest, PatternLearning) {
    // Observe a repeating pattern: MODULE_A -> MODULE_B with MSG_TYPE_X
    bio_module_id_t source = BIO_MODULE_BRAIN;
    bio_module_id_t target = BIO_MODULE_INTROSPECTION;
    bio_message_type_t msg_type = BIO_MSG_BRAIN_STATE_QUERY;

    // Observe the pattern 10 times
    for (int i = 0; i < 10; i++) {
        auto header = make_header(source, target, msg_type);
        ASSERT_EQ(predictive_protocol_observe(proto, &header), 0);
        nimcp_platform_sleep_ms(10);  // Simulate time between messages
    }

    // Query the pattern
    message_pattern_t pattern;
    ASSERT_EQ(predictive_protocol_get_pattern(proto, source, target, msg_type, &pattern), 0);

    EXPECT_EQ(pattern.source_module, source);
    EXPECT_EQ(pattern.target_module, target);
    EXPECT_EQ(pattern.msg_type, msg_type);
    EXPECT_EQ(pattern.frequency, 10U);
    EXPECT_GT(pattern.avg_interval_ms, 0.0f);  // Should have learned interval
}

/**
 * WHAT: Test Markov chain prediction
 * WHY:  Ensure protocol predicts next messages correctly
 * HOW:  Train on A->B sequence, verify B is predicted after A
 */
TEST_F(PredictiveProtocolTest, MarkovPrediction) {
    // Train on sequence: BRAIN -> INTROSPECTION -> ETHICS
    bio_message_header_t msg1 = make_header(BIO_MODULE_BRAIN,
                                             BIO_MODULE_INTROSPECTION,
                                             BIO_MSG_BRAIN_STATE_QUERY);

    bio_message_header_t msg2 = make_header(BIO_MODULE_INTROSPECTION,
                                             BIO_MODULE_ETHICS,
                                             BIO_MSG_INTROSPECTION_QUERY);

    // Observe this sequence 20 times to build strong pattern
    for (int i = 0; i < 20; i++) {
        predictive_protocol_observe(proto, &msg1);
        nimcp_platform_sleep_ms(5);
        predictive_protocol_observe(proto, &msg2);
        nimcp_platform_sleep_ms(5);
    }

    // Predict next message after msg1
    prediction_t predictions[5];
    uint32_t count = predictive_protocol_predict_next(proto, &msg1, predictions, 5);

    ASSERT_GT(count, 0U);  // Should have at least one prediction

    // The first prediction should be msg2 (highest confidence)
    EXPECT_EQ(predictions[0].predicted_msg_type, msg2.type);
    EXPECT_EQ(predictions[0].predicted_target, msg2.target_module);
    EXPECT_GT(predictions[0].confidence, 0.5f);  // Should have learned strong pattern

    // Verify prediction confidence
    float confidence = predictive_protocol_get_confidence(proto, &msg1,
                                                          msg2.type,
                                                          msg2.target_module);
    EXPECT_GT(confidence, 0.5f);
}

/**
 * WHAT: Test prefetch and cache operations
 * WHY:  Ensure prefetching and cache retrieval work correctly
 * HOW:  Prefetch a prediction, verify cache hit
 */
TEST_F(PredictiveProtocolTest, PrefetchAndCache) {
    prediction_t pred;
    pred.predicted_msg_type = BIO_MSG_BRAIN_STATE_QUERY;
    pred.predicted_target = BIO_MODULE_INTROSPECTION;
    pred.confidence = 0.9f;
    pred.prefetch_size = 1024;
    pred.prefetch_data = nullptr;

    // Prefetch the prediction
    ASSERT_EQ(predictive_protocol_prefetch(proto, &pred), 0);

    // Check cache hit
    void* cached = predictive_protocol_get_prefetched(proto,
                                                      pred.predicted_msg_type,
                                                      pred.predicted_target);
    EXPECT_NE(cached, nullptr);  // Should be in cache (even if data is NULL)

    // Check stats
    prefetch_result_t stats;
    ASSERT_EQ(predictive_protocol_get_stats(proto, &stats), 0);
    EXPECT_EQ(stats.prefetches_attempted, 1UL);
    EXPECT_EQ(stats.cache_hits, 1UL);
    EXPECT_EQ(stats.cache_misses, 0UL);
    EXPECT_GT(stats.hit_rate, 0.0f);
}

/**
 * WHAT: Test cache miss behavior
 * WHY:  Ensure cache misses are tracked correctly
 * HOW:  Query non-prefetched data, verify cache miss
 */
TEST_F(PredictiveProtocolTest, CacheMiss) {
    // Query something not in cache
    void* cached = predictive_protocol_get_prefetched(proto,
                                                      BIO_MSG_BRAIN_STATE_QUERY,
                                                      BIO_MODULE_INTROSPECTION);
    EXPECT_EQ(cached, nullptr);

    // Check stats
    prefetch_result_t stats;
    ASSERT_EQ(predictive_protocol_get_stats(proto, &stats), 0);
    EXPECT_EQ(stats.cache_misses, 1UL);
    EXPECT_EQ(stats.cache_hits, 0UL);
}

/**
 * WHAT: Test LRU cache eviction
 * WHY:  Ensure cache evicts least recently used entries
 * HOW:  Fill cache beyond capacity, verify LRU eviction
 */
TEST_F(PredictiveProtocolTest, LRUEviction) {
    // Fill cache to capacity
    for (uint32_t i = 0; i < config.cache_size; i++) {
        prediction_t pred;
        pred.predicted_msg_type = (bio_message_type_t)(BIO_MSG_BRAIN_STATE_QUERY + i);
        pred.predicted_target = BIO_MODULE_INTROSPECTION;
        pred.confidence = 0.9f;
        pred.prefetch_size = 1024;
        pred.prefetch_data = nullptr;

        ASSERT_EQ(predictive_protocol_prefetch(proto, &pred), 0);
    }

    // Cache should be full
    prefetch_result_t stats;
    ASSERT_EQ(predictive_protocol_get_stats(proto, &stats), 0);
    EXPECT_EQ(stats.current_cache_size, config.cache_size);

    // Add one more - should evict LRU
    prediction_t pred;
    pred.predicted_msg_type = BIO_MSG_ETHICS_EVALUATION_REQUEST;
    pred.predicted_target = BIO_MODULE_ETHICS;
    pred.confidence = 0.9f;
    pred.prefetch_size = 1024;
    pred.prefetch_data = nullptr;

    ASSERT_EQ(predictive_protocol_prefetch(proto, &pred), 0);

    // Cache size should still be at capacity
    ASSERT_EQ(predictive_protocol_get_stats(proto, &stats), 0);
    EXPECT_EQ(stats.current_cache_size, config.cache_size);

    // First entry (LRU) should have been evicted
    void* cached = predictive_protocol_get_prefetched(proto,
                                                      BIO_MSG_BRAIN_STATE_QUERY,
                                                      BIO_MODULE_INTROSPECTION);
    EXPECT_EQ(cached, nullptr);  // Should be evicted

    // Most recent entry should still be there
    cached = predictive_protocol_get_prefetched(proto,
                                                pred.predicted_msg_type,
                                                pred.predicted_target);
    EXPECT_NE(cached, nullptr);
}

/**
 * WHAT: Test cache invalidation
 * WHY:  Ensure stale cache entries can be removed
 * HOW:  Prefetch, then invalidate, verify removal
 */
TEST_F(PredictiveProtocolTest, CacheInvalidation) {
    // Prefetch some predictions
    for (int i = 0; i < 5; i++) {
        prediction_t pred;
        pred.predicted_msg_type = (bio_message_type_t)(BIO_MSG_BRAIN_STATE_QUERY + i);
        pred.predicted_target = BIO_MODULE_INTROSPECTION;
        pred.confidence = 0.9f;
        pred.prefetch_size = 1024;
        pred.prefetch_data = nullptr;

        predictive_protocol_prefetch(proto, &pred);
    }

    // Invalidate specific message type
    uint32_t invalidated = predictive_protocol_invalidate(proto, BIO_MSG_BRAIN_STATE_QUERY);
    EXPECT_EQ(invalidated, 1U);

    // Verify it's gone
    void* cached = predictive_protocol_get_prefetched(proto,
                                                      BIO_MSG_BRAIN_STATE_QUERY,
                                                      BIO_MODULE_INTROSPECTION);
    EXPECT_EQ(cached, nullptr);

    // Invalidate all
    invalidated = predictive_protocol_invalidate(proto, 0);
    EXPECT_EQ(invalidated, 4U);  // Remaining 4 entries

    // Cache should be empty
    prefetch_result_t stats;
    ASSERT_EQ(predictive_protocol_get_stats(proto, &stats), 0);
    EXPECT_EQ(stats.current_cache_size, 0U);
}

/**
 * WHAT: Test confidence threshold filtering
 * WHY:  Ensure low-confidence predictions are filtered out
 * HOW:  Set high threshold, verify low-confidence predictions excluded
 */
TEST_F(PredictiveProtocolTest, ConfidenceThreshold) {
    // Set high confidence threshold
    predictive_config_t high_conf_config = predictive_protocol_default_config();
    high_conf_config.min_confidence = 0.9f;

    predictive_protocol_t high_conf_proto = predictive_protocol_create(&high_conf_config);
    ASSERT_NE(high_conf_proto, nullptr);

    // Train on weak pattern (only observed 2 times)
    bio_message_header_t msg1 = make_header(BIO_MODULE_BRAIN,
                                             BIO_MODULE_INTROSPECTION,
                                             BIO_MSG_BRAIN_STATE_QUERY);
    bio_message_header_t msg2 = make_header(BIO_MODULE_INTROSPECTION,
                                             BIO_MODULE_ETHICS,
                                             BIO_MSG_INTROSPECTION_QUERY);

    for (int i = 0; i < 2; i++) {
        predictive_protocol_observe(high_conf_proto, &msg1);
        predictive_protocol_observe(high_conf_proto, &msg2);
    }

    // Try to predict - should get 0 predictions due to low confidence
    prediction_t predictions[5];
    uint32_t count = predictive_protocol_predict_next(high_conf_proto, &msg1, predictions, 5);

    EXPECT_EQ(count, 0U);  // No predictions meet high threshold

    predictive_protocol_destroy(high_conf_proto);
}

/**
 * WHAT: Test statistics tracking
 * WHY:  Ensure all statistics are tracked correctly
 * HOW:  Perform operations, verify stat counters
 */
TEST_F(PredictiveProtocolTest, StatisticsTracking) {
    // Observe some patterns
    bio_message_header_t msg = make_header(BIO_MODULE_BRAIN,
                                            BIO_MODULE_INTROSPECTION,
                                            BIO_MSG_BRAIN_STATE_QUERY);

    for (int i = 0; i < 10; i++) {
        predictive_protocol_observe(proto, &msg);
    }

    // Make some predictions
    prediction_t predictions[5];
    uint32_t count = predictive_protocol_predict_next(proto, &msg, predictions, 5);

    // Prefetch if we have predictions
    if (count > 0) {
        predictive_protocol_prefetch(proto, &predictions[0]);
    }

    // Get some hits and misses
    predictive_protocol_get_prefetched(proto, BIO_MSG_BRAIN_STATE_QUERY, BIO_MODULE_INTROSPECTION);
    predictive_protocol_get_prefetched(proto, BIO_MSG_ETHICS_EVALUATION_REQUEST, BIO_MODULE_ETHICS);

    // Check stats
    prefetch_result_t stats;
    ASSERT_EQ(predictive_protocol_get_stats(proto, &stats), 0);

    EXPECT_GT(stats.predictions_made, 0UL);
    EXPECT_GT(stats.cache_hits + stats.cache_misses, 0UL);

    // Reset stats
    predictive_protocol_reset_stats(proto);

    ASSERT_EQ(predictive_protocol_get_stats(proto, &stats), 0);
    EXPECT_EQ(stats.predictions_made, 0UL);
    EXPECT_EQ(stats.cache_hits, 0UL);
    EXPECT_EQ(stats.cache_misses, 0UL);
}

/**
 * WHAT: Test pattern reset
 * WHY:  Ensure learned patterns can be cleared
 * HOW:  Learn pattern, reset, verify it's gone
 */
TEST_F(PredictiveProtocolTest, PatternReset) {
    // Learn a pattern
    bio_message_header_t msg = make_header(BIO_MODULE_BRAIN,
                                            BIO_MODULE_INTROSPECTION,
                                            BIO_MSG_BRAIN_STATE_QUERY);

    for (int i = 0; i < 10; i++) {
        predictive_protocol_observe(proto, &msg);
    }

    // Verify pattern exists
    message_pattern_t pattern;
    ASSERT_EQ(predictive_protocol_get_pattern(proto, BIO_MODULE_BRAIN,
                                               BIO_MODULE_INTROSPECTION,
                                               BIO_MSG_BRAIN_STATE_QUERY, &pattern), 0);

    // Reset patterns
    predictive_protocol_reset_patterns(proto);

    // Pattern should be gone
    EXPECT_EQ(predictive_protocol_get_pattern(proto, BIO_MODULE_BRAIN,
                                               BIO_MODULE_INTROSPECTION,
                                               BIO_MSG_BRAIN_STATE_QUERY, &pattern), -1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
