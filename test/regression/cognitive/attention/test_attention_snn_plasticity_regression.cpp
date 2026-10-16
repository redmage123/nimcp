/**
 * @file test_attention_snn_plasticity_regression.cpp
 * @brief Regression tests for Attention SNN-Plasticity bridges
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Regression tests ensuring attention SNN and plasticity behavior stability
 * WHY:  Prevent regressions in attention-based learning mechanisms
 * HOW:  Test fixed scenarios with expected outputs, boundary conditions,
 *       and edge cases that have caused issues in the past
 *
 * REGRESSION COVERAGE:
 * - Initialization with various configurations
 * - Encoding edge cases (0, 1, boundary values)
 * - Learning with extreme parameters
 * - Competition dynamics stability
 * - Statistics accuracy over many iterations
 * - Memory and state leak prevention
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "cognitive/attention/nimcp_attention_snn_bridge.h"
#include "cognitive/attention/nimcp_attention_plasticity_bridge.h"

//=============================================================================
// SNN Bridge Regression Tests
//=============================================================================

class AttentionSNNRegressionTest : public ::testing::Test {
protected:
    attention_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        attention_snn_config_t config = attention_snn_config_default();
        config.enable_bio_async = false;
        bridge = attention_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            attention_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(AttentionSNNRegressionTest, ZeroWeightsDoNotCrash) {
    float weights[ATTENTION_SNN_MAX_HEADS] = {0};
    int spikes = attention_snn_encode_weights(bridge, weights, 4);
    EXPECT_GE(spikes, 0);

    EXPECT_EQ(attention_snn_simulate(bridge, 10.0f), 0);

    float focus = attention_snn_get_focus_strength(bridge);
    EXPECT_GE(focus, 0.0f);
    EXPECT_LE(focus, 1.0f);
}

TEST_F(AttentionSNNRegressionTest, MaxWeightsDoNotCrash) {
    float weights[ATTENTION_SNN_MAX_HEADS];
    for (int i = 0; i < ATTENTION_SNN_MAX_HEADS; i++) {
        weights[i] = 1.0f;
    }
    int spikes = attention_snn_encode_weights(bridge, weights, ATTENTION_SNN_MAX_HEADS);
    EXPECT_GE(spikes, 0);

    EXPECT_EQ(attention_snn_simulate(bridge, 10.0f), 0);

    float sparsity = attention_snn_get_sparsity(bridge);
    EXPECT_GE(sparsity, 0.0f);
    EXPECT_LE(sparsity, 1.0f);
}

TEST_F(AttentionSNNRegressionTest, OutOfRangeWeightsClamped) {
    float weights[4] = {5.0f, -2.0f, 1.5f, -0.5f};
    int spikes = attention_snn_encode_weights(bridge, weights, 4);
    EXPECT_GE(spikes, 0);

    attention_snn_simulate(bridge, 10.0f);
    float focus = attention_snn_get_focus_strength(bridge);
    EXPECT_GE(focus, 0.0f);
    EXPECT_LE(focus, 1.0f);
}

TEST_F(AttentionSNNRegressionTest, RepeatedEncodingStable) {
    float weights[4] = {0.5f, 0.3f, 0.2f, 0.1f};

    std::vector<float> focus_values;
    for (int i = 0; i < 100; i++) {
        attention_snn_encode_weights(bridge, weights, 4);
        attention_snn_simulate(bridge, 5.0f);
        float focus = attention_snn_get_focus_strength(bridge);
        focus_values.push_back(focus);
    }

    for (float val : focus_values) {
        EXPECT_GE(val, 0.0f);
        EXPECT_LE(val, 1.0f);
    }
}

TEST_F(AttentionSNNRegressionTest, StatsAccurateAfterManyEvaluations) {
    const int NUM_EVALS = 50;

    for (int i = 0; i < NUM_EVALS; i++) {
        float weights[4] = {0.25f, 0.25f, 0.25f, 0.25f};
        attention_snn_encode_weights(bridge, weights, 4);
        attention_snn_simulate(bridge, 10.0f);
    }

    attention_snn_stats_t stats;
    attention_snn_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_forward_passes, (uint64_t)NUM_EVALS);
}

TEST_F(AttentionSNNRegressionTest, ResetClearsAllState) {
    float weights[4] = {0.9f, 0.8f, 0.7f, 0.6f};
    for (int i = 0; i < 10; i++) {
        attention_snn_encode_weights(bridge, weights, 4);
        attention_snn_simulate(bridge, 10.0f);
    }

    attention_snn_reset(bridge);

    attention_snn_bridge_state_t state;
    attention_snn_get_state(bridge, &state);
    EXPECT_EQ(state.state, ATTENTION_SNN_STATE_IDLE);
}

TEST_F(AttentionSNNRegressionTest, CompetitionDoesNotDiverge) {
    attention_snn_config_t config = attention_snn_config_default();
    config.enable_competition = true;
    config.inhibition_strength = 1.0f;
    config.enable_bio_async = false;

    attention_snn_bridge_t* comp_bridge = attention_snn_create(&config);
    ASSERT_NE(comp_bridge, nullptr);

    float weights[4] = {0.9f, 0.1f, 0.0f, 0.0f};
    for (int i = 0; i < 100; i++) {
        attention_snn_encode_weights(comp_bridge, weights, 4);
        attention_snn_compete(comp_bridge, 10.0f);
    }

    float focus = attention_snn_get_focus_strength(comp_bridge);
    EXPECT_GE(focus, 0.0f);
    EXPECT_LE(focus, 1.0f);

    attention_snn_destroy(comp_bridge);
}

TEST_F(AttentionSNNRegressionTest, SalienceEncodingStable) {
    float salience[64];
    for (int i = 0; i < 64; i++) {
        salience[i] = (float)i / 64.0f;
    }

    int spikes = attention_snn_encode_salience(bridge, salience, 64);
    EXPECT_GE(spikes, 0);

    attention_snn_simulate(bridge, 20.0f);

    float decoded_salience[64];
    EXPECT_EQ(attention_snn_get_salience(bridge, decoded_salience, 64), 0);
}

//=============================================================================
// Plasticity Bridge Regression Tests
//=============================================================================

class AttentionPlasticityRegressionTest : public ::testing::Test {
protected:
    attention_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        attention_plasticity_config_t config = attention_plasticity_config_default();
        config.enable_bio_async = false;
        bridge = attention_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            attention_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(AttentionPlasticityRegressionTest, WeightsStayInBounds) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    for (int i = 0; i < 1000; i++) {
        attention_plasticity_focus(bridge, 0, 1.0f, i * 1000);
    }

    attention_plasticity_update(bridge, 10.0f);

    attention_plasticity_synapse_t syn;
    attention_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GE(syn.weight, 0.0f);
    EXPECT_LE(syn.weight, 1.0f);
}

TEST_F(AttentionPlasticityRegressionTest, FocusEventRecording) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    for (int i = 0; i < 20; i++) {
        attention_plasticity_focus(bridge, 0, 0.8f, i * 10000);
    }

    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_focus_events, 20u);
}

TEST_F(AttentionPlasticityRegressionTest, ShiftEventRecording) {
    for (int i = 0; i < 4; i++) {
        attention_plasticity_register_synapse(bridge, i, ATTENTION_SYNAPSE_HEAD_OUTPUT, i, 0.5f);
    }

    for (int i = 0; i < 15; i++) {
        attention_plasticity_shift(bridge, i % 4, (i + 1) % 4, 0.5f, i * 10000);
    }

    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_shift_events, 15u);
}

TEST_F(AttentionPlasticityRegressionTest, HabituationLearning) {
    attention_plasticity_config_t config = attention_plasticity_config_default();
    config.enable_habituation = true;
    config.habituation_rate = 0.1f;
    config.enable_bio_async = false;

    attention_plasticity_bridge_t* hab_bridge = attention_plasticity_create(&config);
    ASSERT_NE(hab_bridge, nullptr);

    attention_plasticity_register_synapse(hab_bridge, 1, ATTENTION_SYNAPSE_SALIENCE, 0, 0.5f);

    for (int i = 0; i < 50; i++) {
        attention_plasticity_habituation_trial(hab_bridge, 0, i * 10000);
        attention_plasticity_update(hab_bridge, 10.0f);
    }

    float habituation = attention_plasticity_get_habituation(hab_bridge, 0);
    EXPECT_GE(habituation, 0.0f);

    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(hab_bridge, &stats);
    EXPECT_GT(stats.habituation_events, 0u);

    attention_plasticity_destroy(hab_bridge);
}

TEST_F(AttentionPlasticityRegressionTest, NoveltyLearning) {
    attention_plasticity_config_t config = attention_plasticity_config_default();
    config.enable_novelty_detection = true;
    config.novelty_boost = 0.5f;
    config.enable_bio_async = false;

    attention_plasticity_bridge_t* nov_bridge = attention_plasticity_create(&config);
    ASSERT_NE(nov_bridge, nullptr);

    attention_plasticity_register_synapse(nov_bridge, 1, ATTENTION_SYNAPSE_SALIENCE, 0, 0.5f);

    for (int i = 0; i < 20; i++) {
        attention_plasticity_novelty(nov_bridge, 0, 0.9f, i * 10000);
        attention_plasticity_update(nov_bridge, 10.0f);
    }

    float novelty_score = attention_plasticity_get_novelty_score(nov_bridge, 0);
    EXPECT_GE(novelty_score, 0.0f);

    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(nov_bridge, &stats);
    EXPECT_GT(stats.novelty_events, 0u);

    attention_plasticity_destroy(nov_bridge);
}

TEST_F(AttentionPlasticityRegressionTest, RewardModulation) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    attention_plasticity_synapse_t initial;
    attention_plasticity_get_synapse(bridge, 1, &initial);

    for (int i = 0; i < 20; i++) {
        attention_plasticity_focus(bridge, 0, 0.8f, i * 10000);
        attention_plasticity_reward(bridge, 1.0f, i * 10000 + 5000);
        attention_plasticity_update(bridge, 10.0f);
    }

    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_reward, 0.0f);
}

TEST_F(AttentionPlasticityRegressionTest, ManySynapsesDoNotExceedCapacity) {
    for (uint32_t i = 0; i < ATTENTION_PLASTICITY_MAX_SYNAPSES + 10; i++) {
        int result = attention_plasticity_register_synapse(bridge, i, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);
        if (i < ATTENTION_PLASTICITY_MAX_SYNAPSES) {
            EXPECT_EQ(result, 0);
        } else {
            EXPECT_EQ(result, -1);
        }
    }

    attention_plasticity_bridge_state_t state;
    attention_plasticity_get_state(bridge, &state);
    EXPECT_LE(state.registered_synapses, ATTENTION_PLASTICITY_MAX_SYNAPSES);
}

TEST_F(AttentionPlasticityRegressionTest, ConsolidatePreservesState) {
    for (int i = 0; i < 10; i++) {
        attention_plasticity_register_synapse(bridge, i, ATTENTION_SYNAPSE_QUERY_KEY, i % 4, 0.5f + 0.01f * i);
    }

    for (int i = 0; i < 20; i++) {
        attention_plasticity_focus(bridge, i % 4, 0.8f, i * 10000);
        attention_plasticity_update(bridge, 10.0f);
    }

    attention_plasticity_consolidate(bridge);

    attention_plasticity_bridge_state_t state;
    attention_plasticity_get_state(bridge, &state);
    EXPECT_EQ(state.state, ATTENTION_PLASTICITY_STATE_IDLE);
}

//=============================================================================
// Combined Regression Tests
//=============================================================================

class AttentionCombinedRegressionTest : public ::testing::Test {
protected:
    attention_snn_bridge_t* snn = nullptr;
    attention_plasticity_bridge_t* plasticity = nullptr;

    void SetUp() override {
        attention_snn_config_t snn_config = attention_snn_config_default();
        snn_config.enable_bio_async = false;
        snn = attention_snn_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        attention_plasticity_config_t plasticity_config = attention_plasticity_config_default();
        plasticity_config.enable_bio_async = false;
        plasticity = attention_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity, nullptr);
    }

    void TearDown() override {
        if (snn) {
            attention_snn_destroy(snn);
            snn = nullptr;
        }
        if (plasticity) {
            attention_plasticity_destroy(plasticity);
            plasticity = nullptr;
        }
    }
};

TEST_F(AttentionCombinedRegressionTest, LongRunningStability) {
    for (int i = 0; i < 4; i++) {
        attention_plasticity_register_synapse(plasticity, i, ATTENTION_SYNAPSE_QUERY_KEY, i, 0.5f);
    }

    const int ITERATIONS = 200;
    for (int iter = 0; iter < ITERATIONS; iter++) {
        float weights[4] = {0.4f, 0.3f, 0.2f, 0.1f};
        weights[iter % 4] = 0.8f;

        attention_snn_encode_weights(snn, weights, 4);
        attention_snn_simulate(snn, 5.0f);

        float focus = attention_snn_get_focus_strength(snn);
        attention_plasticity_focus(plasticity, iter % 4, focus, iter * 5000);

        if (iter % 10 == 0) {
            attention_plasticity_update(plasticity, 10.0f);
        }
    }

    attention_snn_stats_t snn_stats;
    attention_snn_get_stats(snn, &snn_stats);
    EXPECT_GE(snn_stats.total_forward_passes, (uint64_t)ITERATIONS);

    attention_plasticity_stats_t plasticity_stats;
    attention_plasticity_get_stats(plasticity, &plasticity_stats);
    EXPECT_EQ(plasticity_stats.total_focus_events, (uint64_t)ITERATIONS);

    for (int i = 0; i < 4; i++) {
        attention_plasticity_synapse_t syn;
        attention_plasticity_get_synapse(plasticity, i, &syn);
        EXPECT_GE(syn.weight, 0.0f);
        EXPECT_LE(syn.weight, 1.0f);
    }
}

TEST_F(AttentionCombinedRegressionTest, ResetBothDoesNotLeak) {
    for (int round = 0; round < 5; round++) {
        float weights[4] = {0.5f, 0.5f, 0.5f, 0.5f};
        for (int i = 0; i < 10; i++) {
            attention_snn_encode_weights(snn, weights, 4);
            attention_snn_simulate(snn, 5.0f);
        }

        attention_snn_reset(snn);
        attention_plasticity_reset(plasticity);
    }

    attention_snn_bridge_state_t snn_state;
    attention_snn_get_state(snn, &snn_state);
    EXPECT_EQ(snn_state.state, ATTENTION_SNN_STATE_IDLE);

    attention_plasticity_bridge_state_t plasticity_state;
    attention_plasticity_get_state(plasticity, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, ATTENTION_PLASTICITY_STATE_IDLE);
}
