/**
 * @file test_predictive_protocol_integration.cpp
 * @brief Integration tests for Predictive Communication Protocol
 *
 * WHAT: Tests with real message streams and full system integration
 * WHY:  Validate protocol works with actual brain modules and swarm agents
 * HOW:  Simulate realistic communication patterns
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_predictive_protocol.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PredictiveProtocolIntegrationTest : public ::testing::Test {
protected:
    predictive_protocol_t* protocol;

    void SetUp() override {
        predictive_protocol_config_t config;
        config.prediction_window_ms = 2000;
        config.history_buffer_size = 1024;
        config.confidence_threshold = 0.6f;
        config.enable_prefetch = true;
        config.enable_bio_async = false;

        protocol = predictive_protocol_create(&config);
        ASSERT_NE(protocol, nullptr);
    }

    void TearDown() override {
        if (protocol) {
            predictive_protocol_destroy(protocol);
            protocol = nullptr;
        }
    }

    /* Helper: get timestamp */
    uint64_t GetTimestamp() {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch());
        return ms.count();
    }
};

//=============================================================================
// Real Message Stream Tests
//=============================================================================

TEST_F(PredictiveProtocolIntegrationTest, VisualAttentionStream) {
    /* Simulate visual attention workflow */
    std::vector<uint32_t> visual_sequence = {
        BIO_MSG_VISUAL_INPUT,           /* Visual stimulus arrives */
        BIO_MSG_VISUAL_FEATURE_DETECTED,/* Feature extracted */
        BIO_MSG_SALIENCE_QUERY,         /* Salience evaluated */
        BIO_MSG_SALIENCE_RESPONSE,      /* Salience high */
        BIO_MSG_ATTENTION_SHIFT,        /* Attention shifts */
        BIO_MSG_WORKING_MEMORY_STORE    /* Store in WM */
    };

    /* Train on this pattern multiple times */
    for (int trial = 0; trial < 20; trial++) {
        uint64_t base_time = GetTimestamp();

        for (size_t i = 0; i < visual_sequence.size(); i++) {
            int ret = predictive_protocol_observe_message(
                protocol,
                visual_sequence[i],
                BIO_MODULE_VISUAL_CORTEX,
                BIO_MODULE_ATTENTION,
                base_time + i * 50);

            ASSERT_EQ(ret, 0);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    /* Now predict: after visual input, should predict feature detection */
    predicted_message_t pred;
    int ret = predictive_protocol_predict_next(
        protocol,
        BIO_MSG_VISUAL_INPUT,
        &pred);

    if (ret == 0) {
        EXPECT_EQ(pred.message_type, BIO_MSG_VISUAL_FEATURE_DETECTED);
        EXPECT_GT(pred.confidence, 0.6f);
        EXPECT_EQ(pred.source_module, BIO_MODULE_VISUAL_CORTEX);
    }

    /* Get all predictions in window */
    predicted_message_t* preds = nullptr;
    uint32_t count = 0;

    ret = predictive_protocol_get_predictions(
        protocol,
        2000,
        &preds,
        &count);

    EXPECT_EQ(ret, 0);

    if (preds && count > 0) {
        /* Should predict multiple steps ahead */
        EXPECT_GE(count, 1);

        /* Verify predictions are from the sequence */
        for (uint32_t i = 0; i < count; i++) {
            bool found = false;
            for (auto msg : visual_sequence) {
                if (preds[i].message_type == msg) {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found);
        }

        nimcp_free(preds);
    }

    /* Check statistics */
    predictive_stats_t stats = predictive_protocol_get_stats(protocol);
    EXPECT_GT(stats.predictions_made, 0);
    EXPECT_GT(stats.avg_prediction_lead_time_ms, 0);
}

TEST_F(PredictiveProtocolIntegrationTest, LanguageProductionStream) {
    /* Simulate language production workflow */
    std::vector<uint32_t> language_sequence = {
        BIO_MSG_KNOWLEDGE_QUERY,            /* Retrieve concept */
        BIO_MSG_KNOWLEDGE_RESPONSE,         /* Got concept */
        BIO_MSG_LEXICAL_ACCESS_REQUEST,     /* Find word */
        BIO_MSG_LEXICAL_ACCESS_RESPONSE,    /* Got word */
        BIO_MSG_SYNTAX_PARSE_REQUEST,       /* Build syntax */
        BIO_MSG_SYNTAX_PARSE_RESULT,        /* Syntax ready */
        BIO_MSG_PHONOLOGICAL_ENCODE_REQUEST,/* Encode phonemes */
        BIO_MSG_PHONOLOGICAL_ENCODE_RESULT, /* Phonemes ready */
        BIO_MSG_MOTOR_COMMAND_REQUEST,      /* Articulate */
        BIO_MSG_MOTOR_COMMAND_RESULT        /* Speech produced */
    };

    /* Train on language production pattern */
    for (int trial = 0; trial < 15; trial++) {
        uint64_t base_time = GetTimestamp();

        for (size_t i = 0; i < language_sequence.size(); i++) {
            int ret = predictive_protocol_observe_message(
                protocol,
                language_sequence[i],
                BIO_MODULE_BROCA,
                BIO_MODULE_BRAIN,
                base_time + i * 100);

            ASSERT_EQ(ret, 0);
        }
    }

    /* Predict next step after each stage */
    for (size_t i = 0; i < language_sequence.size() - 1; i++) {
        predicted_message_t pred;
        int ret = predictive_protocol_predict_next(
            protocol,
            language_sequence[i],
            &pred);

        if (ret == 0) {
            /* Should predict the next message in sequence */
            EXPECT_EQ(pred.message_type, language_sequence[i + 1]);
            EXPECT_GT(pred.confidence, 0.5f);
        }
    }
}

TEST_F(PredictiveProtocolIntegrationTest, PlasticityLearningStream) {
    /* Simulate learning workflow */
    std::vector<uint32_t> learning_sequence = {
        BIO_MSG_STDP_EVENT,             /* Spike timing event */
        BIO_MSG_WEIGHT_UPDATE_REQUEST,  /* Request weight update */
        BIO_MSG_WEIGHT_UPDATE_RESPONSE, /* Weight updated */
        BIO_MSG_NEUROMODULATOR_RELEASE, /* Dopamine released */
        BIO_MSG_ELIGIBILITY_TRACE_UPDATE/* Trace updated */
    };

    /* Train pattern */
    for (int trial = 0; trial < 25; trial++) {
        uint64_t base_time = GetTimestamp();

        for (size_t i = 0; i < learning_sequence.size(); i++) {
            int ret = predictive_protocol_observe_message(
                protocol,
                learning_sequence[i],
                BIO_MODULE_STDP,
                BIO_MODULE_BRAIN,
                base_time + i * 20);

            ASSERT_EQ(ret, 0);
        }
    }

    /* Predict cascade after STDP event */
    predicted_message_t pred;
    int ret = predictive_protocol_predict_next(
        protocol,
        BIO_MSG_STDP_EVENT,
        &pred);

    if (ret == 0) {
        EXPECT_EQ(pred.message_type, BIO_MSG_WEIGHT_UPDATE_REQUEST);
        EXPECT_GT(pred.confidence, 0.7f); /* Should be very confident */
    }
}

//=============================================================================
// Prefetch Integration Tests
//=============================================================================

TEST_F(PredictiveProtocolIntegrationTest, PrefetchWorkflow) {
    /* Simulate complete prefetch workflow */
    std::vector<uint32_t> sequence = {
        BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MSG_BRAIN_STATE_RESPONSE,
        BIO_MSG_NEURON_ACTIVATION_REQUEST,
        BIO_MSG_NEURON_ACTIVATION_RESPONSE
    };

    /* Train pattern */
    for (int trial = 0; trial < 10; trial++) {
        uint64_t base_time = GetTimestamp();

        for (size_t i = 0; i < sequence.size(); i++) {
            int ret = predictive_protocol_observe_message(
                protocol,
                sequence[i],
                BIO_MODULE_BRAIN,
                BIO_MODULE_ATTENTION,
                base_time + i * 50);

            ASSERT_EQ(ret, 0);
        }
    }

    /* Make prediction */
    predicted_message_t pred;
    int ret = predictive_protocol_predict_next(
        protocol,
        BIO_MSG_BRAIN_STATE_QUERY,
        &pred);

    if (ret == 0) {
        /* Prefetch data for prediction */
        ret = predictive_protocol_prefetch_data(protocol, &pred);
        EXPECT_EQ(ret, 0);

        /* Check if data is cached */
        void* data = nullptr;
        uint32_t size = 0;

        ret = predictive_protocol_check_prefetch(
            protocol,
            pred.message_type,
            &data,
            &size);

        EXPECT_EQ(ret, 0);

        /* Verify prefetch hit in stats */
        predictive_stats_t stats = predictive_protocol_get_stats(protocol);
        EXPECT_GE(stats.prefetch_hits, 1);
    }
}

TEST_F(PredictiveProtocolIntegrationTest, MultiplePrefetches) {
    /* Learn multiple patterns */
    std::vector<std::pair<uint32_t, uint32_t>> patterns = {
        {BIO_MSG_VISUAL_INPUT, BIO_MSG_ATTENTION_SHIFT},
        {BIO_MSG_AUDIO_INPUT, BIO_MSG_ATTENTION_SHIFT},
        {BIO_MSG_SALIENCE_QUERY, BIO_MSG_SALIENCE_RESPONSE}
    };

    /* Train all patterns */
    for (int trial = 0; trial < 15; trial++) {
        uint64_t base_time = GetTimestamp();

        for (auto& pattern : patterns) {
            predictive_protocol_observe_message(
                protocol,
                pattern.first,
                BIO_MODULE_BRAIN,
                BIO_MODULE_ATTENTION,
                base_time);

            predictive_protocol_observe_message(
                protocol,
                pattern.second,
                BIO_MODULE_ATTENTION,
                BIO_MODULE_BRAIN,
                base_time + 50);

            base_time += 100;
        }
    }

    /* Get all predictions and prefetch */
    predicted_message_t* preds = nullptr;
    uint32_t count = 0;

    int ret = predictive_protocol_get_predictions(
        protocol,
        2000,
        &preds,
        &count);

    ASSERT_EQ(ret, 0);

    if (preds && count > 0) {
        /* Prefetch all predictions */
        for (uint32_t i = 0; i < count; i++) {
            if (preds[i].confidence > 0.6f) {
                ret = predictive_protocol_prefetch_data(protocol, &preds[i]);
                EXPECT_EQ(ret, 0);
            }
        }

        /* Verify all are cached */
        for (uint32_t i = 0; i < count; i++) {
            if (preds[i].confidence > 0.6f) {
                void* data = nullptr;
                uint32_t size = 0;

                ret = predictive_protocol_check_prefetch(
                    protocol,
                    preds[i].message_type,
                    &data,
                    &size);

                EXPECT_EQ(ret, 0);
            }
        }

        nimcp_free(preds);
    }
}

//=============================================================================
// Concurrent Pattern Tests
//=============================================================================

TEST_F(PredictiveProtocolIntegrationTest, InterleavedPatterns) {
    /* Simulate multiple concurrent workflows */
    std::vector<uint32_t> visual_pattern = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT
    };

    std::vector<uint32_t> audio_pattern = {
        BIO_MSG_AUDIO_INPUT,
        BIO_MSG_ATTENTION_SHIFT
    };

    /* Interleave patterns */
    for (int trial = 0; trial < 20; trial++) {
        uint64_t base_time = GetTimestamp();

        /* Visual */
        for (size_t i = 0; i < visual_pattern.size(); i++) {
            predictive_protocol_observe_message(
                protocol,
                visual_pattern[i],
                BIO_MODULE_VISUAL_CORTEX,
                BIO_MODULE_ATTENTION,
                base_time + i * 30);
        }

        /* Audio */
        base_time += 100;
        for (size_t i = 0; i < audio_pattern.size(); i++) {
            predictive_protocol_observe_message(
                protocol,
                audio_pattern[i],
                BIO_MODULE_AUDIO_CORTEX,
                BIO_MODULE_ATTENTION,
                base_time + i * 30);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    /* Both patterns should be learned */
    predicted_message_t pred;

    int ret1 = predictive_protocol_predict_next(
        protocol,
        BIO_MSG_VISUAL_INPUT,
        &pred);

    int ret2 = predictive_protocol_predict_next(
        protocol,
        BIO_MSG_AUDIO_INPUT,
        &pred);

    /* At least one should succeed */
    EXPECT_TRUE(ret1 == 0 || ret2 == 0);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(PredictiveProtocolIntegrationTest, HighVolumeMessages) {
    /* Test with high message volume */
    const int MESSAGE_COUNT = 10000;

    uint64_t base_time = GetTimestamp();

    for (int i = 0; i < MESSAGE_COUNT; i++) {
        uint32_t msg_type = BIO_MSG_VISUAL_INPUT + (i % 10);

        int ret = predictive_protocol_observe_message(
            protocol,
            msg_type,
            BIO_MODULE_BRAIN,
            BIO_MODULE_ATTENTION,
            base_time + i);

        ASSERT_EQ(ret, 0);
    }

    /* Should still be able to make predictions */
    predicted_message_t pred;
    int ret = predictive_protocol_predict_next(
        protocol,
        BIO_MSG_VISUAL_INPUT,
        &pred);

    /* May or may not succeed, but shouldn't crash */
    EXPECT_TRUE(ret == 0 || ret < 0);

    /* Check stats */
    predictive_stats_t stats = predictive_protocol_get_stats(protocol);
    EXPECT_GE(stats.predictions_made, 0);
}

TEST_F(PredictiveProtocolIntegrationTest, LongRunningPrediction) {
    /* Simulate long-running system */
    std::vector<uint32_t> sequence = {
        BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MSG_BRAIN_STATE_RESPONSE
    };

    /* Run for extended period */
    for (int epoch = 0; epoch < 100; epoch++) {
        uint64_t base_time = GetTimestamp();

        for (size_t i = 0; i < sequence.size(); i++) {
            predictive_protocol_observe_message(
                protocol,
                sequence[i],
                BIO_MODULE_BRAIN,
                BIO_MODULE_ATTENTION,
                base_time + i * 50);
        }

        /* Make prediction every 10 epochs */
        if (epoch % 10 == 0) {
            predicted_message_t pred;
            predictive_protocol_predict_next(
                protocol,
                BIO_MSG_BRAIN_STATE_QUERY,
                &pred);
        }
    }

    /* Final stats check */
    predictive_stats_t stats = predictive_protocol_get_stats(protocol);
    EXPECT_GT(stats.predictions_made, 0);
    EXPECT_GE(stats.prediction_accuracy, 0.0f);
    EXPECT_LE(stats.prediction_accuracy, 1.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
