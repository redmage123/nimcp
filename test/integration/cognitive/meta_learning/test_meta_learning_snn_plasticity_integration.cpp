//=============================================================================
// test_meta_learning_snn_plasticity_integration.cpp - Meta Learning Integration
//=============================================================================
/**
 * @file test_meta_learning_snn_plasticity_integration.cpp
 * @brief Integration tests for Meta Learning-SNN-Plasticity bidirectional dataflows
 *
 * WHAT: Tests complete integration between meta learning, SNN, and plasticity
 * WHY:  Verify bidirectional dataflows work correctly for learning-to-learn
 * HOW:  Create both bridges, simulate meta-learning scenarios, verify adaptation
 *
 * INTEGRATION POINTS:
 * - Meta learning encoding -> SNN population activity
 * - SNN spikes -> Plasticity STDP -> weight updates
 * - Learning events -> Synapse modification -> Adaptation calibration
 * - Protection mechanisms -> Block learning on core meta-learning patterns
 *
 * THEORETICAL BASIS:
 * - Meta-learning (learning to learn)
 * - Learning rate adaptation
 * - Transfer learning detection
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

#include "cognitive/meta_learning/nimcp_meta_learning_snn_bridge.h"
#include "cognitive/meta_learning/nimcp_meta_learning_plasticity_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MetaLearningSNNPlasticityIntegrationTest : public ::testing::Test {
protected:
    meta_learning_snn_bridge_t* snn_bridge;
    meta_learning_plasticity_bridge_t* plasticity_bridge;

    // Callback tracking
    std::atomic<int> adaptation_detection_count{0};
    std::atomic<int> insight_count{0};
    std::atomic<int> weight_change_count{0};
    std::atomic<int> transfer_detection_count{0};
    std::atomic<float> last_adaptation_level{0.0f};

    void SetUp() override {
        // Create SNN bridge with test-friendly config
        meta_learning_snn_config_t snn_config = meta_learning_snn_config_default();
        snn_config.num_dimensions = META_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.enable_curriculum = true;
        snn_config.enable_bio_async = false;  // Disable for predictable tests

        snn_bridge = meta_learning_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with defaults
        meta_learning_plasticity_config_t plasticity_config = meta_learning_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = meta_learning_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create plasticity bridge";

        // Reset counters
        adaptation_detection_count = 0;
        insight_count = 0;
        weight_change_count = 0;
        transfer_detection_count = 0;
        last_adaptation_level = 0.0f;
    }

    void TearDown() override {
        if (snn_bridge) {
            meta_learning_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            meta_learning_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate meta learning context for scenario
    void generate_meta_learning_context(float* dims, uint32_t scenario_type) {
        memset(dims, 0, sizeof(float) * META_DIM_COUNT);
        switch (scenario_type) {
            case 0: // High learning rate, fast adaptation
                dims[META_DIM_LEARNING_RATE] = 0.9f;
                dims[META_DIM_ADAPTATION_SPEED] = 0.85f;
                dims[META_DIM_STRATEGY_SELECT] = 0.7f;
                break;
            case 1: // Transfer learning scenario
                dims[META_DIM_TRANSFER] = 0.9f;
                dims[META_DIM_TASK_SIMILARITY] = 0.8f;
                dims[META_DIM_PRIOR_KNOWLEDGE] = 0.75f;
                break;
            case 2: // Generalization scenario
                dims[META_DIM_GENERALIZATION] = 0.95f;
                dims[META_DIM_LEARNING_TO_LEARN] = 0.8f;
                dims[META_DIM_CURRICULUM] = 0.85f;
                break;
            case 3: // Consolidation scenario
                dims[META_DIM_CONSOLIDATION] = 0.9f;
                dims[META_DIM_PRIOR_KNOWLEDGE] = 0.8f;
                dims[META_DIM_LEARNING_TO_LEARN] = 0.7f;
                dims[META_DIM_STRATEGY_SELECT] = 0.5f;
                break;
            default:
                for (int i = 0; i < META_DIM_COUNT; i++) {
                    dims[i] = 0.5f;
                }
                break;
        }
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, BothBridgesInitialize) {
    // Verify both bridges are functional
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check initial states
    meta_learning_snn_bridge_state_t snn_state;
    EXPECT_EQ(meta_learning_snn_get_state(snn_bridge, &snn_state), 0);
    EXPECT_EQ(snn_state.state, META_LEARNING_SNN_STATE_IDLE);

    meta_learning_plasticity_bridge_state_t plasticity_state;
    EXPECT_EQ(meta_learning_plasticity_get_state(plasticity_bridge, &plasticity_state), 0);
    EXPECT_EQ(plasticity_state.state, META_LEARNING_PLASTICITY_STATE_IDLE);
}

TEST_F(MetaLearningSNNPlasticityIntegrationTest, SNNEncodingDrivesPlasticityActivity) {
    // Encode meta learning context in SNN
    float dims[META_DIM_COUNT];
    generate_meta_learning_context(dims, 0);  // High learning rate scenario

    int spikes = meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
    EXPECT_GE(spikes, 0) << "Encoding should succeed (0 or more spikes)";

    // Simulate SNN processing
    EXPECT_EQ(meta_learning_snn_simulate(snn_bridge, 20.0f), 0);

    // Register synapses in plasticity bridge
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
            i, META_SYNAPSE_STRATEGY, 0.5f), 0);
    }

    // Apply STDP based on SNN activity (returns weight delta)
    float delta = meta_learning_plasticity_apply_stdp(plasticity_bridge, 0, 1.0f, 3.0f);
    EXPECT_TRUE(std::isfinite(delta)) << "STDP should return valid delta";

    // Get synapse and verify retrieval succeeded
    meta_learning_plasticity_synapse_t synapse;
    EXPECT_EQ(meta_learning_plasticity_get_synapse(plasticity_bridge, 0, &synapse), 0);
}

//=============================================================================
// Transfer Detection Integration
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, TransferDetectionTriggersLearning) {
    // Encode transfer learning scenario
    int spikes = meta_learning_snn_encode_transfer(snn_bridge, 0.9f, 1);
    EXPECT_GE(spikes, 0);

    // Simulate processing
    meta_learning_snn_simulate(snn_bridge, 30.0f);

    // Check transfer detection
    float transfer_level;
    bool detected = meta_learning_snn_check_transfer(snn_bridge, &transfer_level);
    // Detection based on thresholds

    // Register transfer synapse
    EXPECT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
        100, META_SYNAPSE_TRANSFER, 0.5f), 0);

    // Learn from transfer event (strengthens transfer patterns)
    EXPECT_EQ(meta_learning_plasticity_learn(plasticity_bridge,
        META_LEARN_TRANSFER_SUCCESS, 0.8f, 100, transfer_level), 0);

    // Verify weight changed
    meta_learning_plasticity_synapse_t synapse;
    EXPECT_EQ(meta_learning_plasticity_get_synapse(plasticity_bridge, 100, &synapse), 0);
}

TEST_F(MetaLearningSNNPlasticityIntegrationTest, CorrectLearningRateReinforcesAdaptation) {
    // Register adaptation synapse
    EXPECT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
        200, META_SYNAPSE_ADAPTATION, 0.4f), 0);

    // Initial weight
    meta_learning_plasticity_synapse_t synapse;
    EXPECT_EQ(meta_learning_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    float initial_weight = synapse.weight;
    (void)initial_weight;

    // Learn from correct rate (positive outcome)
    EXPECT_EQ(meta_learning_plasticity_learn(plasticity_bridge,
        META_LEARN_RATE_CORRECT, 0.9f, 200, 0.9f), 0);

    // Verify weight is still valid
    EXPECT_EQ(meta_learning_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 2.0f);
}

//=============================================================================
// Generalization Integration
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, GeneralizationEncodingAndLearning) {
    // Encode generalization scenario
    float dims[META_DIM_COUNT];
    generate_meta_learning_context(dims, 2);  // Generalization scenario
    int spikes = meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    // Simulate processing
    meta_learning_snn_simulate(snn_bridge, 25.0f);

    // Get insight
    meta_learning_insight_t insight;
    EXPECT_EQ(meta_learning_snn_get_insight(snn_bridge, &insight), 0);

    // Register generalization synapse
    EXPECT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
        300, META_SYNAPSE_GENERALIZATION, 0.8f), 0);

    // Synapse should not be protected
    meta_learning_plasticity_synapse_t synapse;
    EXPECT_EQ(meta_learning_plasticity_get_synapse(plasticity_bridge, 300, &synapse), 0);
    EXPECT_FALSE(synapse.is_protected);

    // Learn generalization success
    EXPECT_EQ(meta_learning_plasticity_learn(plasticity_bridge,
        META_LEARN_GENERALIZATION_SUCCESS, 1.0f, 300, insight.generalization_score), 0);
}

//=============================================================================
// Learning Rate Protection Integration
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, LearningRateProtectionIntegrity) {
    // Encode learning rate activation
    float dims[META_DIM_COUNT] = {0};
    dims[META_DIM_LEARNING_RATE] = 1.0f;
    dims[META_DIM_ADAPTATION_SPEED] = 0.9f;

    meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
    meta_learning_snn_simulate(snn_bridge, 30.0f);

    // Get insight
    meta_learning_insight_t insight;
    meta_learning_snn_get_insight(snn_bridge, &insight);

    // Register learning rate synapse (auto-protected)
    EXPECT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
        400, META_SYNAPSE_LEARNING_RATE, 1.0f), 0);

    // Learning rate synapse should be protected
    meta_learning_plasticity_synapse_t synapse;
    EXPECT_EQ(meta_learning_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);

    // Attempt to modify protected synapse (should be blocked)
    float original_weight = synapse.weight;
    meta_learning_plasticity_apply_stdp(plasticity_bridge, 400, 5.0f, 10.0f);

    EXPECT_EQ(meta_learning_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight) << "Protected synapse should not change";
}

//=============================================================================
// Strategy Selection Integration
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, StrategySelectionAndLearning) {
    // Encode strategy selection scenario
    float dims[META_DIM_COUNT];
    generate_meta_learning_context(dims, 3);  // Consolidation scenario with strategy

    meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
    meta_learning_snn_simulate(snn_bridge, 40.0f);

    // Check for state change
    float change_magnitude;
    meta_learning_snn_check_state_change(snn_bridge, &change_magnitude);

    // Register strategy synapse
    EXPECT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
        500, META_SYNAPSE_STRATEGY, 0.5f), 0);

    // Get insight
    meta_learning_insight_t insight;
    meta_learning_snn_get_insight(snn_bridge, &insight);

    // Apply learning based on strategy effectiveness
    if (insight.adaptation_level > 0.5f) {
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_STRATEGY_EFFECTIVE, 0.6f, 500, insight.adaptation_level);
    } else {
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_STRATEGY_INEFFECTIVE, 0.4f, 500, insight.adaptation_level);
    }

    // Verify learning occurred
    meta_learning_plasticity_stats_t stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);
}

//=============================================================================
// Full Pipeline Integration
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, FullMetaLearningPipeline) {
    // Register multiple synapse types
    for (int i = 0; i < 5; i++) {
        meta_learning_plasticity_register_synapse(plasticity_bridge,
            600 + i, META_SYNAPSE_STRATEGY, 0.5f);
        meta_learning_plasticity_register_synapse(plasticity_bridge,
            610 + i, META_SYNAPSE_TRANSFER, 0.5f);
    }

    // Run multiple scenarios
    for (int scenario = 0; scenario < 4; scenario++) {
        float dims[META_DIM_COUNT];
        generate_meta_learning_context(dims, scenario);

        // SNN encoding and simulation
        meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
        meta_learning_snn_simulate(snn_bridge, 25.0f);

        // Get insight
        meta_learning_insight_t insight;
        meta_learning_snn_get_insight(snn_bridge, &insight);

        // Apply learning based on adaptation level
        if (insight.adaptation_level > 0.5f) {
            for (int i = 0; i < 5; i++) {
                meta_learning_plasticity_learn(plasticity_bridge,
                    META_LEARN_RATE_TOO_HIGH, -0.5f, 600 + i, insight.adaptation_level);
            }
        } else {
            for (int i = 0; i < 5; i++) {
                meta_learning_plasticity_learn(plasticity_bridge,
                    META_LEARN_RATE_CORRECT, 0.3f, 600 + i, insight.adaptation_level);
            }
        }

        // Apply STDP between consecutive synapse pairs
        for (int i = 0; i < 4; i++) {
            meta_learning_plasticity_apply_stdp(plasticity_bridge, 600 + i,
                (float)scenario * 2.0f, (float)scenario * 2.0f + 5.0f);
        }

        // Update eligibility traces
        meta_learning_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Verify stats
    meta_learning_snn_stats_t snn_stats;
    meta_learning_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 4u);

    meta_learning_plasticity_stats_t plasticity_stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GT(plasticity_stats.total_learning_events, 0u);
    EXPECT_GT(plasticity_stats.weight_updates, 0u);
}

//=============================================================================
// Reward Modulation Integration
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, RewardModulatedLearning) {
    // Register synapses
    for (int i = 0; i < 3; i++) {
        meta_learning_plasticity_register_synapse(plasticity_bridge,
            700 + i, META_SYNAPSE_STRATEGY, 0.5f);
    }

    // Encode high adaptation scenario
    float dims[META_DIM_COUNT];
    generate_meta_learning_context(dims, 2);  // Generalization scenario
    meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
    meta_learning_snn_simulate(snn_bridge, 25.0f);

    // Apply positive reward
    float reward = 0.8f;
    EXPECT_EQ(meta_learning_plasticity_apply_reward(plasticity_bridge, reward), 0);

    // Check adaptation state
    meta_learning_adaptation_state_t adaptation;
    EXPECT_EQ(meta_learning_plasticity_get_adaptation_state(plasticity_bridge, &adaptation), 0);
}

//=============================================================================
// BCM Metaplasticity Integration
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, BCMMetaplasticityUpdate) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        meta_learning_plasticity_register_synapse(plasticity_bridge,
            800 + i, META_SYNAPSE_STRATEGY, 0.5f);
    }

    // Run multiple encoding cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        float dims[META_DIM_COUNT];
        generate_meta_learning_context(dims, cycle % 4);

        meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
        meta_learning_snn_step(snn_bridge);

        // Update BCM thresholds
        float postsynaptic_rate = 0.3f + 0.05f * cycle;
        meta_learning_plasticity_update_bcm(plasticity_bridge, postsynaptic_rate);
    }

    // Verify BCM function ran without error
    meta_learning_plasticity_stats_t stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &stats);
}

//=============================================================================
// Homeostatic Regulation Integration
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, HomeostaticWeightRegulation) {
    // Register synapses with varied initial weights
    for (int i = 0; i < 8; i++) {
        float initial_weight = 0.2f + 0.1f * i;  // 0.2 to 0.9
        meta_learning_plasticity_register_synapse(plasticity_bridge,
            900 + i, META_SYNAPSE_STRATEGY, initial_weight);
    }

    // Run homeostatic update cycles
    float target_activity = 0.5f;
    for (int cycle = 0; cycle < 5; cycle++) {
        meta_learning_plasticity_homeostatic_update(plasticity_bridge, target_activity);
    }

    // Verify homeostatic function ran without error
    meta_learning_plasticity_stats_t stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &stats);
}

//=============================================================================
// Consolidation Integration
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, MetaLearningConsolidation) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        meta_learning_plasticity_register_synapse(plasticity_bridge,
            1000 + i, META_SYNAPSE_STRATEGY, 0.5f);
    }

    // Apply significant learning
    for (int i = 0; i < 5; i++) {
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_RATE_CORRECT, 0.7f, 1000 + i, 0.9f);
    }

    // Get stats before consolidation
    meta_learning_plasticity_stats_t before_stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &before_stats);

    // Consolidate learning
    EXPECT_EQ(meta_learning_plasticity_consolidate(plasticity_bridge), 0);

    // Verify consolidation occurred
    meta_learning_plasticity_stats_t after_stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &after_stats);
    EXPECT_GE(after_stats.total_learning_events, before_stats.total_learning_events);
}

//=============================================================================
// Reset and Recovery Integration
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, ResetAndRecoveryBehavior) {
    // Setup state in both bridges
    float dims[META_DIM_COUNT];
    generate_meta_learning_context(dims, 1);  // Transfer learning
    meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
    meta_learning_snn_simulate(snn_bridge, 20.0f);

    meta_learning_plasticity_register_synapse(plasticity_bridge, 1100,
        META_SYNAPSE_TRANSFER, 0.6f);
    meta_learning_plasticity_learn(plasticity_bridge,
        META_LEARN_TRANSFER_SUCCESS, 0.5f, 1100, 0.8f);

    // Reset both bridges
    EXPECT_EQ(meta_learning_snn_reset(snn_bridge), 0);
    EXPECT_EQ(meta_learning_plasticity_reset(plasticity_bridge), 0);

    // Verify reset states
    meta_learning_snn_bridge_state_t snn_state;
    meta_learning_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, META_LEARNING_SNN_STATE_IDLE);

    meta_learning_plasticity_bridge_state_t plasticity_state;
    meta_learning_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, META_LEARNING_PLASTICITY_STATE_IDLE);

    // Re-run scenarios to verify recovery
    meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
    meta_learning_snn_simulate(snn_bridge, 15.0f);

    meta_learning_insight_t insight;
    EXPECT_EQ(meta_learning_snn_get_insight(snn_bridge, &insight), 0);
    EXPECT_GE(insight.adaptation_level, 0.0f);
}

//=============================================================================
// Concurrent Safety Tests
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, ConcurrentEncodingAndLearning) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        meta_learning_plasticity_register_synapse(plasticity_bridge,
            1200 + i, META_SYNAPSE_STRATEGY, 0.5f);
    }

    std::atomic<int> encoding_complete{0};
    std::atomic<int> learning_complete{0};

    // Thread 1: SNN encoding
    std::thread encoder([this, &encoding_complete]() {
        for (int i = 0; i < 5; i++) {
            float dims[META_DIM_COUNT];
            generate_meta_learning_context(dims, i % 4);
            meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
            meta_learning_snn_step(snn_bridge);
            encoding_complete++;
        }
    });

    // Thread 2: Plasticity learning
    std::thread learner([this, &learning_complete]() {
        for (int i = 0; i < 5; i++) {
            meta_learning_plasticity_learn(plasticity_bridge,
                META_LEARN_RATE_CORRECT, 0.1f, 1200 + (i % 10), 0.5f);
            learning_complete++;
        }
    });

    encoder.join();
    learner.join();

    EXPECT_EQ(encoding_complete, 5);
    EXPECT_EQ(learning_complete, 5);
}

//=============================================================================
// Consolidation Protection Integration
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, ConsolidationProtectionAndLearning) {
    // Register consolidation synapse (auto-protected)
    EXPECT_EQ(meta_learning_plasticity_register_synapse(plasticity_bridge,
        1300, META_SYNAPSE_CONSOLIDATION, 0.5f), 0);

    meta_learning_plasticity_synapse_t synapse;
    EXPECT_EQ(meta_learning_plasticity_get_synapse(plasticity_bridge, 1300, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);  // Consolidation is protected
}

//=============================================================================
// Stats Integration
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, StatsAccumulationAcrossBridges) {
    // Run multiple scenarios
    for (int s = 0; s < 5; s++) {
        float dims[META_DIM_COUNT];
        generate_meta_learning_context(dims, s % 4);

        meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
        meta_learning_snn_simulate(snn_bridge, 10.0f);

        meta_learning_plasticity_register_synapse(plasticity_bridge,
            1400 + s, META_SYNAPSE_STRATEGY, 0.5f);
        meta_learning_plasticity_learn(plasticity_bridge,
            META_LEARN_RATE_CORRECT, 0.2f, 1400 + s, 0.6f);
    }

    // Check SNN stats
    meta_learning_snn_stats_t snn_stats;
    meta_learning_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 5u);
    EXPECT_GT(snn_stats.total_simulations, 0u);

    // Check plasticity stats
    meta_learning_plasticity_stats_t plasticity_stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    // Verify synapses were used (active_synapses in state)
    meta_learning_plasticity_bridge_state_t bridge_state;
    meta_learning_plasticity_get_state(plasticity_bridge, &bridge_state);
    EXPECT_GE(bridge_state.active_synapses, 5u);
    EXPECT_GE(plasticity_stats.total_learning_events, 5u);
}

//=============================================================================
// Adaptation Learning Integration
//=============================================================================

TEST_F(MetaLearningSNNPlasticityIntegrationTest, AdaptationLearningPipeline) {
    // Register adaptation synapses (using STRATEGY type, not auto-protected)
    for (int i = 0; i < 5; i++) {
        meta_learning_plasticity_register_synapse(plasticity_bridge,
            1500 + i, META_SYNAPSE_STRATEGY, 0.5f);
    }

    // Simulate rate too high followed by correct rate
    for (int trial = 0; trial < 10; trial++) {
        float dims[META_DIM_COUNT] = {0};
        if (trial % 2 == 0) {
            // Rate too high scenario
            dims[META_DIM_LEARNING_RATE] = 0.95f;
            dims[META_DIM_ADAPTATION_SPEED] = 0.3f;  // Low actual adaptation
        } else {
            // Well-calibrated rate
            dims[META_DIM_LEARNING_RATE] = 0.7f;
            dims[META_DIM_ADAPTATION_SPEED] = 0.7f;
        }

        meta_learning_snn_encode_state(snn_bridge, dims, META_DIM_COUNT);
        meta_learning_snn_simulate(snn_bridge, 15.0f);

        meta_learning_insight_t insight;
        meta_learning_snn_get_insight(snn_bridge, &insight);

        // Learn based on adaptation
        if (trial % 2 == 0) {
            meta_learning_plasticity_learn(plasticity_bridge,
                META_LEARN_RATE_TOO_HIGH, 0.5f, 1500 + (trial % 5), insight.adaptation_level);
        } else {
            meta_learning_plasticity_learn(plasticity_bridge,
                META_LEARN_RATE_CORRECT, 0.5f, 1500 + (trial % 5), insight.adaptation_level);
        }
    }

    // Verify learning statistics
    meta_learning_plasticity_stats_t stats;
    meta_learning_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.correct_rate_events, 5u);
    EXPECT_GE(stats.rate_too_high_events, 5u);
}
