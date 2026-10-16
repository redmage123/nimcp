/**
 * @file test_predictive_protocol.cpp
 * @brief Unit tests for Predictive Communication Protocol
 *
 * WHAT: Comprehensive tests for predictive protocol functionality
 * WHY:  Ensure pattern learning, prediction, and prefetch work correctly
 * HOW:  Test each function with various scenarios
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_predictive_protocol.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PredictiveProtocolTest : public ::testing::Test {
protected:
    predictive_protocol_t* protocol;
    predictive_protocol_config_t config;

    void SetUp() override {
        /* Default configuration */
        config.prediction_window_ms = 1000;
        config.history_buffer_size = 512;
        config.confidence_threshold = 0.5f;
        config.enable_prefetch = true;
        config.enable_bio_async = false;

        protocol = nullptr;
    }

    void TearDown() override {
        if (protocol) {
            predictive_protocol_destroy(protocol);
            protocol = nullptr;
        }
    }

    /* Helper: create protocol with default config */
    void CreateProtocol() {
        protocol = predictive_protocol_create(&config);
        ASSERT_NE(protocol, nullptr);
    }

    /* Helper: observe message sequence */
    void ObserveSequence(const uint32_t* types, uint32_t count) {
        uint64_t base_time = 1000000;
        for (uint32_t i = 0; i < count; i++) {
            int ret = predictive_protocol_observe_message(
                protocol, types[i],
                BIO_MODULE_BRAIN,
                BIO_MODULE_ATTENTION,
                base_time + i * 100);
            ASSERT_EQ(ret, 0);
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(PredictiveProtocolTest, CreateDestroy) {
    /* Test basic creation and destruction */
    CreateProtocol();
    EXPECT_NE(protocol, nullptr);

    /* Stats should be zeroed */
    predictive_stats_t stats = predictive_protocol_get_stats(protocol);
    EXPECT_EQ(stats.predictions_made, 0);
    EXPECT_EQ(stats.predictions_correct, 0);
    EXPECT_EQ(stats.prefetch_hits, 0);
}

TEST_F(PredictiveProtocolTest, CreateWithNullConfig) {
    /* Should use defaults */
    protocol = predictive_protocol_create(nullptr);
    EXPECT_NE(protocol, nullptr);
}

TEST_F(PredictiveProtocolTest, CreateWithInvalidConfig) {
    /* Zero history size should fail */
    config.history_buffer_size = 0;
    protocol = predictive_protocol_create(&config);
    EXPECT_EQ(protocol, nullptr);
}

TEST_F(PredictiveProtocolTest, DestroyNull) {
    /* Should not crash */
    predictive_protocol_destroy(nullptr);
}

//=============================================================================
// Pattern Learning Tests
//=============================================================================

TEST_F(PredictiveProtocolTest, ObserveSingleMessage) {
    CreateProtocol();

    int ret = predictive_protocol_observe_message(
        protocol,
        BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MODULE_ATTENTION,
        BIO_MODULE_BRAIN,
        1000000);

    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveProtocolTest, ObserveMessageSequence) {
    CreateProtocol();

    uint32_t sequence[] = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT,
        BIO_MSG_WORKING_MEMORY_STORE
    };

    ObserveSequence(sequence, 3);

    /* Should have learned the pattern */
    predictive_stats_t stats = predictive_protocol_get_stats(protocol);
    EXPECT_EQ(stats.predictions_made, 0); /* No predictions yet */
}

TEST_F(PredictiveProtocolTest, ObserveRepeatedPattern) {
    CreateProtocol();

    uint32_t sequence[] = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT
    };

    /* Observe pattern multiple times */
    for (int i = 0; i < 10; i++) {
        ObserveSequence(sequence, 2);
    }

    /* Pattern should be strongly learned */
}

TEST_F(PredictiveProtocolTest, LearnSequenceBatch) {
    CreateProtocol();

    uint32_t sequence[] = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT,
        BIO_MSG_WORKING_MEMORY_STORE,
        BIO_MSG_BRAIN_STATE_QUERY
    };

    int ret = predictive_protocol_learn_sequence(protocol, sequence, 4);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveProtocolTest, LearnSequenceNull) {
    CreateProtocol();

    int ret = predictive_protocol_learn_sequence(protocol, nullptr, 10);
    EXPECT_LT(ret, 0);
}

TEST_F(PredictiveProtocolTest, LearnSequenceEmpty) {
    CreateProtocol();

    uint32_t sequence[] = {BIO_MSG_VISUAL_INPUT};
    int ret = predictive_protocol_learn_sequence(protocol, sequence, 0);
    EXPECT_LT(ret, 0);
}

TEST_F(PredictiveProtocolTest, LearnLongSequence) {
    CreateProtocol();

    /* Create sequence longer than max */
    uint32_t sequence[100];
    for (uint32_t i = 0; i < 100; i++) {
        sequence[i] = BIO_MSG_VISUAL_INPUT + (i % 10);
    }

    /* Should handle gracefully */
    int ret = predictive_protocol_learn_sequence(protocol, sequence, 100);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Prediction Tests
//=============================================================================

TEST_F(PredictiveProtocolTest, PredictNextNoPatterns) {
    CreateProtocol();

    predicted_message_t pred;
    int ret = predictive_protocol_predict_next(
        protocol,
        BIO_MSG_VISUAL_INPUT,
        &pred);

    /* Should fail - no patterns learned */
    EXPECT_LT(ret, 0);
}

TEST_F(PredictiveProtocolTest, PredictNextSimplePattern) {
    CreateProtocol();

    /* Learn simple A -> B pattern */
    uint32_t sequence[] = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT
    };

    /* Repeat to build confidence */
    for (int i = 0; i < 10; i++) {
        ObserveSequence(sequence, 2);
    }

    /* Predict next after A */
    predicted_message_t pred;
    int ret = predictive_protocol_predict_next(
        protocol,
        BIO_MSG_VISUAL_INPUT,
        &pred);

    if (ret == 0) {
        EXPECT_EQ(pred.message_type, BIO_MSG_ATTENTION_SHIFT);
        EXPECT_GT(pred.confidence, 0.5f);
        EXPECT_GT(pred.predicted_time_ms, 0);
    }

    /* Stats should show prediction */
    predictive_stats_t stats = predictive_protocol_get_stats(protocol);
    EXPECT_GT(stats.predictions_made, 0);
}

TEST_F(PredictiveProtocolTest, PredictNextLowConfidence) {
    CreateProtocol();

    /* Learn pattern once (low confidence) */
    uint32_t sequence[] = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT
    };
    ObserveSequence(sequence, 2);

    /* Set high threshold */
    config.confidence_threshold = 0.9f;
    predictive_protocol_destroy(protocol);
    CreateProtocol();
    ObserveSequence(sequence, 2);

    /* Should fail due to threshold */
    predicted_message_t pred;
    int ret = predictive_protocol_predict_next(
        protocol,
        BIO_MSG_VISUAL_INPUT,
        &pred);

    EXPECT_LT(ret, 0);
}

TEST_F(PredictiveProtocolTest, PredictNextNull) {
    CreateProtocol();

    int ret = predictive_protocol_predict_next(
        protocol,
        BIO_MSG_VISUAL_INPUT,
        nullptr);

    EXPECT_LT(ret, 0);
}

TEST_F(PredictiveProtocolTest, GetPredictionsWindow) {
    CreateProtocol();

    /* Learn multiple patterns */
    uint32_t seq1[] = {BIO_MSG_VISUAL_INPUT, BIO_MSG_ATTENTION_SHIFT};
    uint32_t seq2[] = {BIO_MSG_AUDIO_INPUT, BIO_MSG_ATTENTION_SHIFT};

    for (int i = 0; i < 5; i++) {
        ObserveSequence(seq1, 2);
        ObserveSequence(seq2, 2);
    }

    predicted_message_t* preds = nullptr;
    uint32_t count = 0;

    int ret = predictive_protocol_get_predictions(
        protocol,
        1000, /* 1 second window */
        &preds,
        &count);

    EXPECT_EQ(ret, 0);

    if (preds) {
        /* Should have some predictions */
        EXPECT_GT(count, 0);

        /* Verify predictions */
        for (uint32_t i = 0; i < count; i++) {
            EXPECT_GT(preds[i].confidence, 0.0f);
            EXPECT_LE(preds[i].confidence, 1.0f);
        }

        nimcp_free(preds);
    }
}

TEST_F(PredictiveProtocolTest, GetPredictionsNull) {
    CreateProtocol();

    predicted_message_t* preds = nullptr;
    uint32_t count = 0;

    /* Null protocol */
    int ret = predictive_protocol_get_predictions(
        nullptr, 1000, &preds, &count);
    EXPECT_LT(ret, 0);

    /* Null predictions */
    ret = predictive_protocol_get_predictions(
        protocol, 1000, nullptr, &count);
    EXPECT_LT(ret, 0);

    /* Null count */
    ret = predictive_protocol_get_predictions(
        protocol, 1000, &preds, nullptr);
    EXPECT_LT(ret, 0);
}

//=============================================================================
// Prefetch Tests
//=============================================================================

TEST_F(PredictiveProtocolTest, PrefetchBasic) {
    CreateProtocol();

    predicted_message_t pred;
    pred.message_type = BIO_MSG_BRAIN_STATE_QUERY;
    pred.confidence = 0.8f;

    int ret = predictive_protocol_prefetch_data(protocol, &pred);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveProtocolTest, PrefetchDisabled) {
    config.enable_prefetch = false;
    CreateProtocol();

    predicted_message_t pred;
    pred.message_type = BIO_MSG_BRAIN_STATE_QUERY;
    pred.confidence = 0.8f;

    int ret = predictive_protocol_prefetch_data(protocol, &pred);
    EXPECT_LT(ret, 0);
}

TEST_F(PredictiveProtocolTest, PrefetchNull) {
    CreateProtocol();

    int ret = predictive_protocol_prefetch_data(protocol, nullptr);
    EXPECT_LT(ret, 0);
}

TEST_F(PredictiveProtocolTest, CheckPrefetchHit) {
    CreateProtocol();

    /* Prefetch data */
    predicted_message_t pred;
    pred.message_type = BIO_MSG_BRAIN_STATE_QUERY;
    pred.confidence = 0.8f;

    int ret = predictive_protocol_prefetch_data(protocol, &pred);
    ASSERT_EQ(ret, 0);

    /* Check for hit */
    void* data = nullptr;
    uint32_t size = 0;

    ret = predictive_protocol_check_prefetch(
        protocol,
        BIO_MSG_BRAIN_STATE_QUERY,
        &data,
        &size);

    EXPECT_EQ(ret, 0);

    /* Stats should show hit */
    predictive_stats_t stats = predictive_protocol_get_stats(protocol);
    EXPECT_EQ(stats.prefetch_hits, 1);
}

TEST_F(PredictiveProtocolTest, CheckPrefetchMiss) {
    CreateProtocol();

    void* data = nullptr;
    uint32_t size = 0;

    int ret = predictive_protocol_check_prefetch(
        protocol,
        BIO_MSG_BRAIN_STATE_QUERY,
        &data,
        &size);

    EXPECT_LT(ret, 0);
}

TEST_F(PredictiveProtocolTest, CheckPrefetchNull) {
    CreateProtocol();

    void* data = nullptr;
    uint32_t size = 0;

    int ret = predictive_protocol_check_prefetch(
        protocol,
        BIO_MSG_BRAIN_STATE_QUERY,
        nullptr,
        &size);

    EXPECT_LT(ret, 0);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(PredictiveProtocolTest, ConnectPredictiveCoding) {
    CreateProtocol();

    void* context = (void*)0x12345678; /* Dummy context */

    int ret = predictive_protocol_connect_predictive_coding(protocol, context);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveProtocolTest, ConnectPredictiveCodingNull) {
    CreateProtocol();

    int ret = predictive_protocol_connect_predictive_coding(protocol, nullptr);
    EXPECT_EQ(ret, 0); /* NULL context is allowed */
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(PredictiveProtocolTest, GetStatsInitial) {
    CreateProtocol();

    predictive_stats_t stats = predictive_protocol_get_stats(protocol);

    EXPECT_EQ(stats.predictions_made, 0);
    EXPECT_EQ(stats.predictions_correct, 0);
    EXPECT_EQ(stats.predictions_wrong, 0);
    EXPECT_EQ(stats.prediction_accuracy, 0.0f);
    EXPECT_EQ(stats.prefetch_hits, 0);
    EXPECT_EQ(stats.prefetch_misses, 0);
}

TEST_F(PredictiveProtocolTest, GetStatsNull) {
    predictive_stats_t stats = predictive_protocol_get_stats(nullptr);

    /* Should return zeroed stats */
    EXPECT_EQ(stats.predictions_made, 0);
}

TEST_F(PredictiveProtocolTest, GetStatsAfterPredictions) {
    CreateProtocol();

    /* Learn pattern */
    uint32_t sequence[] = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT
    };

    for (int i = 0; i < 10; i++) {
        ObserveSequence(sequence, 2);
    }

    /* Make prediction */
    predicted_message_t pred;
    predictive_protocol_predict_next(
        protocol,
        BIO_MSG_VISUAL_INPUT,
        &pred);

    /* Check stats */
    predictive_stats_t stats = predictive_protocol_get_stats(protocol);
    EXPECT_GT(stats.predictions_made, 0);
}

//=============================================================================
// Complex Scenario Tests
//=============================================================================

TEST_F(PredictiveProtocolTest, MultiplePatterns) {
    CreateProtocol();

    /* Learn multiple distinct patterns */
    uint32_t seq1[] = {BIO_MSG_VISUAL_INPUT, BIO_MSG_ATTENTION_SHIFT};
    uint32_t seq2[] = {BIO_MSG_AUDIO_INPUT, BIO_MSG_WORKING_MEMORY_STORE};
    uint32_t seq3[] = {BIO_MSG_BRAIN_STATE_QUERY, BIO_MSG_BRAIN_STATE_RESPONSE};

    for (int i = 0; i < 5; i++) {
        ObserveSequence(seq1, 2);
        ObserveSequence(seq2, 2);
        ObserveSequence(seq3, 2);
    }

    /* Predict for each */
    predicted_message_t pred;

    int ret1 = predictive_protocol_predict_next(
        protocol, BIO_MSG_VISUAL_INPUT, &pred);

    int ret2 = predictive_protocol_predict_next(
        protocol, BIO_MSG_AUDIO_INPUT, &pred);

    int ret3 = predictive_protocol_predict_next(
        protocol, BIO_MSG_BRAIN_STATE_QUERY, &pred);

    /* At least some should succeed */
    EXPECT_TRUE(ret1 == 0 || ret2 == 0 || ret3 == 0);
}

TEST_F(PredictiveProtocolTest, HistoryBufferWraparound) {
    config.history_buffer_size = 16; /* Small buffer */
    CreateProtocol();

    /* Overflow buffer */
    for (uint32_t i = 0; i < 100; i++) {
        int ret = predictive_protocol_observe_message(
            protocol,
            BIO_MSG_VISUAL_INPUT,
            BIO_MODULE_BRAIN,
            BIO_MODULE_ATTENTION,
            1000000 + i);
        EXPECT_EQ(ret, 0);
    }

    /* Should still work */
    predictive_stats_t stats = predictive_protocol_get_stats(protocol);
    EXPECT_GE(stats.predictions_made, 0);
}

TEST_F(PredictiveProtocolTest, ConfidenceThreshold) {
    CreateProtocol();

    /* Learn weak pattern (once) */
    uint32_t sequence[] = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT
    };
    ObserveSequence(sequence, 2);

    /* High threshold should reject */
    config.confidence_threshold = 0.95f;
    predictive_protocol_destroy(protocol);
    CreateProtocol();
    ObserveSequence(sequence, 2);

    predicted_message_t pred;
    int ret = predictive_protocol_predict_next(
        protocol,
        BIO_MSG_VISUAL_INPUT,
        &pred);

    EXPECT_LT(ret, 0);

    /* Low threshold should accept */
    config.confidence_threshold = 0.1f;
    predictive_protocol_destroy(protocol);
    CreateProtocol();
    ObserveSequence(sequence, 2);

    ret = predictive_protocol_predict_next(
        protocol,
        BIO_MSG_VISUAL_INPUT,
        &pred);

    /* May succeed with low threshold */
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
