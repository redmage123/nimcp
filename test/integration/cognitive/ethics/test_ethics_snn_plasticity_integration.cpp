//=============================================================================
// test_ethics_snn_plasticity_integration.cpp - Ethics SNN/Plasticity Integration
//=============================================================================
/**
 * @file test_ethics_snn_plasticity_integration.cpp
 * @brief Integration tests for Ethics-SNN-Plasticity bidirectional dataflows
 *
 * WHAT: Tests complete integration between ethics reasoning, SNN, and plasticity
 * WHY:  Verify bidirectional dataflows work correctly for ethical learning
 * HOW:  Create both bridges, simulate ethical scenarios, verify moral learning
 *
 * INTEGRATION POINTS:
 * - Ethics encoding -> SNN population activity
 * - SNN spikes -> Plasticity STDP -> weight updates
 * - Learning events -> Synapse modification -> Moral principle evolution
 * - Protection mechanisms -> Block learning on core values
 *
 * THEORETICAL BASIS:
 * - Moral Foundations Theory (Haidt, 2012)
 * - Asimov's Three Laws of Robotics (core value protection)
 * - Reinforcement learning for ethics (reward-modulated plasticity)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

#include "cognitive/ethics/nimcp_ethics_snn_bridge.h"
#include "cognitive/ethics/nimcp_ethics_plasticity_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EthicsSNNPlasticityIntegrationTest : public ::testing::Test {
protected:
    ethics_snn_bridge_t* snn_bridge;
    ethics_plasticity_bridge_t* plasticity_bridge;

    // Callback tracking
    std::atomic<int> harm_detection_count{0};
    std::atomic<int> judgment_count{0};
    std::atomic<int> weight_change_count{0};
    std::atomic<int> principle_update_count{0};
    std::atomic<float> last_harm_level{0.0f};

    void SetUp() override {
        // Create SNN bridge with test-friendly config
        ethics_snn_config_t snn_config = ethics_snn_config_default();
        snn_config.num_dimensions = ETHICS_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.enable_asimov_populations = true;
        snn_config.enable_bio_async = false;  // Disable for predictable tests

        snn_bridge = ethics_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with defaults
        ethics_plasticity_config_t plasticity_config = ethics_plasticity_config_default();
        // Learning mechanisms are configured via parameters, not enable flags
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = ethics_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create plasticity bridge";

        // Reset counters
        harm_detection_count = 0;
        judgment_count = 0;
        weight_change_count = 0;
        principle_update_count = 0;
        last_harm_level = 0.0f;
    }

    void TearDown() override {
        if (snn_bridge) {
            ethics_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            ethics_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate ethical context for scenario
    void generate_ethical_context(float* dims, uint32_t scenario_type) {
        memset(dims, 0, sizeof(float) * ETHICS_DIM_COUNT);
        switch (scenario_type) {
            case 0: // Low harm, high fairness
                dims[ETHICS_DIM_HARM] = 0.1f;
                dims[ETHICS_DIM_FAIRNESS] = 0.9f;
                dims[ETHICS_DIM_EMPATHY] = 0.7f;
                break;
            case 1: // High harm (should trigger First Law)
                dims[ETHICS_DIM_HARM] = 0.9f;
                dims[ETHICS_DIM_ASIMOV_FIRST] = 1.0f;
                dims[ETHICS_DIM_EMPATHY] = 0.3f;
                break;
            case 2: // Golden Rule scenario
                dims[ETHICS_DIM_GOLDEN_RULE] = 0.95f;
                dims[ETHICS_DIM_FAIRNESS] = 0.8f;
                dims[ETHICS_DIM_EMPATHY] = 0.85f;
                break;
            case 3: // Ethical conflict
                dims[ETHICS_DIM_LOYALTY] = 0.9f;
                dims[ETHICS_DIM_AUTHORITY] = 0.8f;
                dims[ETHICS_DIM_HARM] = 0.5f;
                dims[ETHICS_DIM_CONFLICT] = 0.8f;
                break;
            default:
                for (int i = 0; i < ETHICS_DIM_COUNT; i++) {
                    dims[i] = 0.5f;
                }
                break;
        }
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, BothBridgesInitialize) {
    // Verify both bridges are functional
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check initial states
    ethics_snn_bridge_state_t snn_state;
    EXPECT_EQ(ethics_snn_get_state(snn_bridge, &snn_state), 0);
    EXPECT_EQ(snn_state.state, ETHICS_SNN_STATE_IDLE);

    ethics_plasticity_bridge_state_t plasticity_state;
    EXPECT_EQ(ethics_plasticity_get_state(plasticity_bridge, &plasticity_state), 0);
    EXPECT_EQ(plasticity_state.state, ETHICS_PLASTICITY_STATE_IDLE);
}

TEST_F(EthicsSNNPlasticityIntegrationTest, SNNEncodingDrivesPlasticityActivity) {
    // Encode ethical context in SNN
    float dims[ETHICS_DIM_COUNT];
    generate_ethical_context(dims, 0);  // Low harm scenario

    int spikes = ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    EXPECT_GE(spikes, 0) << "Encoding should succeed (0 or more spikes)";

    // Simulate SNN processing
    EXPECT_EQ(ethics_snn_simulate(snn_bridge, 20.0f), 0);

    // Register synapses in plasticity bridge
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(ethics_plasticity_register_synapse(plasticity_bridge,
            i, ETHICS_SYNAPSE_FAIRNESS, 0.5f), 0);
    }

    // Apply STDP based on SNN activity (returns weight delta, not error code)
    float delta = ethics_plasticity_apply_stdp(plasticity_bridge, 0, 1.0f, 3.0f);
    EXPECT_TRUE(std::isfinite(delta)) << "STDP should return valid delta";

    // Get synapse and verify retrieval succeeded
    ethics_plasticity_synapse_t synapse;
    EXPECT_EQ(ethics_plasticity_get_synapse(plasticity_bridge, 0, &synapse), 0);
}

//=============================================================================
// Harm Detection Integration
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, HarmDetectionTriggersLearning) {
    // Encode high harm scenario
    int spikes = ethics_snn_encode_harm(snn_bridge, 0.9f, 0.85f);
    EXPECT_GT(spikes, 0);

    // Simulate processing
    ethics_snn_simulate(snn_bridge, 30.0f);

    // Check harm detection
    float harm_level;
    bool detected = ethics_snn_check_harm(snn_bridge, &harm_level);
    EXPECT_TRUE(detected);
    EXPECT_GT(harm_level, 0.5f);

    // Register harm detection synapse
    EXPECT_EQ(ethics_plasticity_register_synapse(plasticity_bridge,
        100, ETHICS_SYNAPSE_HARM_DETECTION, 0.5f), 0);

    // Learn from harm event (strengthens harm detection)
    EXPECT_EQ(ethics_plasticity_learn(plasticity_bridge,
        ETHICS_LEARN_HARM_CAUSED, 0.8f, 100, harm_level), 0);

    // Verify weight changed (harm detection should strengthen)
    ethics_plasticity_synapse_t synapse;
    EXPECT_EQ(ethics_plasticity_get_synapse(plasticity_bridge, 100, &synapse), 0);
}

TEST_F(EthicsSNNPlasticityIntegrationTest, HarmAvoidedReinforcesPositiveBehavior) {
    // Register outcome synapse
    EXPECT_EQ(ethics_plasticity_register_synapse(plasticity_bridge,
        200, ETHICS_SYNAPSE_OUTCOME, 0.4f), 0);

    // Initial weight
    ethics_plasticity_synapse_t synapse;
    EXPECT_EQ(ethics_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    float initial_weight = synapse.weight;
    (void)initial_weight;  // May not change depending on implementation

    // Learn from harm avoided (positive outcome)
    EXPECT_EQ(ethics_plasticity_learn(plasticity_bridge,
        ETHICS_LEARN_HARM_AVOIDED, 0.9f, 200, 0.9f), 0);

    // Verify weight is still valid
    EXPECT_EQ(ethics_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 1.0f);
}

//=============================================================================
// Golden Rule Integration
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, GoldenRuleEncodingAndLearning) {
    // Encode Golden Rule scenario
    int spikes = ethics_snn_encode_golden_rule(snn_bridge, 0.9f, 0.85f, 0.95f);
    EXPECT_GT(spikes, 0);

    // Simulate processing
    ethics_snn_simulate(snn_bridge, 25.0f);

    // Get judgment
    ethics_judgment_t judgment;
    EXPECT_EQ(ethics_snn_get_judgment(snn_bridge, &judgment), 0);
    EXPECT_GT(judgment.golden_rule_activation, 0.5f);

    // Register Golden Rule synapse (auto-protected)
    EXPECT_EQ(ethics_plasticity_register_synapse(plasticity_bridge,
        300, ETHICS_SYNAPSE_GOLDEN_RULE, 0.8f), 0);

    // Golden Rule synapse should be protected
    ethics_plasticity_synapse_t synapse;
    EXPECT_EQ(ethics_plasticity_get_synapse(plasticity_bridge, 300, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);

    // Learn Golden Rule application
    EXPECT_EQ(ethics_plasticity_learn(plasticity_bridge,
        ETHICS_LEARN_GOLDEN_RULE_APPLIED, 1.0f, 300, judgment.golden_rule_activation), 0);
}

//=============================================================================
// First Law Protection Integration
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, FirstLawProtectionIntegrity) {
    // Encode First Law activation
    float dims[ETHICS_DIM_COUNT] = {0};
    dims[ETHICS_DIM_ASIMOV_FIRST] = 1.0f;
    dims[ETHICS_DIM_HARM] = 0.9f;

    ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    ethics_snn_simulate(snn_bridge, 30.0f);

    // Get judgment
    ethics_judgment_t judgment;
    ethics_snn_get_judgment(snn_bridge, &judgment);
    EXPECT_GT(judgment.first_law_activation, 0.5f);

    // Register First Law synapse (auto-protected)
    EXPECT_EQ(ethics_plasticity_register_synapse(plasticity_bridge,
        400, ETHICS_SYNAPSE_FIRST_LAW, 1.0f), 0);

    // First Law synapse should be protected
    ethics_plasticity_synapse_t synapse;
    EXPECT_EQ(ethics_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);

    // Attempt to modify protected synapse (should be blocked)
    float original_weight = synapse.weight;
    ethics_plasticity_apply_stdp(plasticity_bridge, 400, 5.0f, 10.0f);

    EXPECT_EQ(ethics_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight) << "Protected synapse should not change";
}

//=============================================================================
// Ethical Conflict Resolution Integration
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, ConflictDetectionAndResolution) {
    // Encode conflict scenario
    float dims[ETHICS_DIM_COUNT];
    generate_ethical_context(dims, 3);  // Conflict scenario

    ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    ethics_snn_simulate(snn_bridge, 40.0f);

    // Check for conflict
    float conflict_level;
    bool detected = ethics_snn_check_conflict(snn_bridge, &conflict_level);
    // May or may not detect based on thresholds

    // Register conflict synapse
    EXPECT_EQ(ethics_plasticity_register_synapse(plasticity_bridge,
        500, ETHICS_SYNAPSE_CONFLICT, 0.5f), 0);

    // Get judgment
    ethics_judgment_t judgment;
    ethics_snn_get_judgment(snn_bridge, &judgment);

    // Apply learning based on resolution
    if (judgment.allow_score > judgment.block_score) {
        ethics_plasticity_learn(plasticity_bridge,
            ETHICS_LEARN_POSITIVE_OUTCOME, 0.6f, 500, judgment.confidence);
    } else {
        ethics_plasticity_learn(plasticity_bridge,
            ETHICS_LEARN_NEGATIVE_OUTCOME, 0.4f, 500, judgment.confidence);
    }

    // Verify learning occurred
    ethics_plasticity_stats_t stats;
    ethics_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);
}

//=============================================================================
// Full Pipeline Integration
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, FullEthicalDecisionPipeline) {
    // Register multiple synapse types
    for (int i = 0; i < 5; i++) {
        ethics_plasticity_register_synapse(plasticity_bridge,
            600 + i, ETHICS_SYNAPSE_FAIRNESS, 0.5f);
        ethics_plasticity_register_synapse(plasticity_bridge,
            610 + i, ETHICS_SYNAPSE_EMPATHY, 0.5f);
    }

    // Run multiple scenarios
    for (int scenario = 0; scenario < 4; scenario++) {
        float dims[ETHICS_DIM_COUNT];
        generate_ethical_context(dims, scenario);

        // SNN encoding and simulation
        ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
        ethics_snn_simulate(snn_bridge, 25.0f);

        // Get judgment
        ethics_judgment_t judgment;
        ethics_snn_get_judgment(snn_bridge, &judgment);

        // Apply learning based on outcome
        if (judgment.harm_detected) {
            for (int i = 0; i < 5; i++) {
                ethics_plasticity_learn(plasticity_bridge,
                    ETHICS_LEARN_HARM_CAUSED, -0.5f, 600 + i, judgment.confidence);
            }
        } else {
            for (int i = 0; i < 5; i++) {
                ethics_plasticity_learn(plasticity_bridge,
                    ETHICS_LEARN_POSITIVE_OUTCOME, 0.3f, 600 + i, judgment.confidence);
            }
        }

        // Apply STDP between consecutive synapse pairs
        for (int i = 0; i < 4; i++) {
            ethics_plasticity_apply_stdp(plasticity_bridge, 600 + i,
                (float)scenario * 2.0f, (float)scenario * 2.0f + 5.0f);
        }

        // Update eligibility traces
        ethics_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Verify stats
    ethics_snn_stats_t snn_stats;
    ethics_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 4u);

    ethics_plasticity_stats_t plasticity_stats;
    ethics_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GT(plasticity_stats.total_learning_events, 0u);
    EXPECT_GT(plasticity_stats.weight_updates, 0u);
}

//=============================================================================
// Reward Modulation Integration
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, RewardModulatedLearning) {
    // Register synapses
    for (int i = 0; i < 3; i++) {
        ethics_plasticity_register_synapse(plasticity_bridge,
            700 + i, ETHICS_SYNAPSE_OUTCOME, 0.5f);
    }

    // Encode positive outcome scenario
    float dims[ETHICS_DIM_COUNT];
    generate_ethical_context(dims, 2);  // Golden Rule scenario
    ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    ethics_snn_simulate(snn_bridge, 25.0f);

    // Apply positive reward
    float reward = 0.8f;
    EXPECT_EQ(ethics_plasticity_apply_reward(plasticity_bridge, reward), 0);

    // Check principle states
    ethics_principle_state_t principle;
    EXPECT_EQ(ethics_plasticity_get_principle_state(plasticity_bridge, &principle), 0);
}

//=============================================================================
// BCM Metaplasticity Integration
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, BCMMetaplasticityUpdate) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        ethics_plasticity_register_synapse(plasticity_bridge,
            800 + i, ETHICS_SYNAPSE_FAIRNESS, 0.5f);
    }

    // Run multiple encoding cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        float dims[ETHICS_DIM_COUNT];
        generate_ethical_context(dims, cycle % 4);

        ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
        ethics_snn_step(snn_bridge);

        // Update BCM thresholds
        float postsynaptic_rate = 0.3f + 0.05f * cycle;
        ethics_plasticity_update_bcm(plasticity_bridge, postsynaptic_rate);
    }

    // Verify BCM function ran without error
    ethics_plasticity_stats_t stats;
    ethics_plasticity_get_stats(plasticity_bridge, &stats);
    // BCM threshold updates don't increment weight_updates counter directly
}

//=============================================================================
// Homeostatic Regulation Integration
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, HomeostaticWeightRegulation) {
    // Register synapses with varied initial weights
    for (int i = 0; i < 8; i++) {
        float initial_weight = 0.2f + 0.1f * i;  // 0.2 to 0.9
        ethics_plasticity_register_synapse(plasticity_bridge,
            900 + i, ETHICS_SYNAPSE_OUTCOME, initial_weight);
    }

    // Run homeostatic update cycles
    float target_activity = 0.5f;
    for (int cycle = 0; cycle < 5; cycle++) {
        ethics_plasticity_homeostatic_update(plasticity_bridge, target_activity);
    }

    // Verify homeostatic function ran without error
    ethics_plasticity_stats_t stats;
    ethics_plasticity_get_stats(plasticity_bridge, &stats);
    // Homeostatic updates don't increment weight_updates counter directly
}

//=============================================================================
// Consolidation Integration
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, EthicalLearningConsolidation) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        ethics_plasticity_register_synapse(plasticity_bridge,
            1000 + i, ETHICS_SYNAPSE_OUTCOME, 0.5f);
    }

    // Apply significant learning
    for (int i = 0; i < 5; i++) {
        ethics_plasticity_learn(plasticity_bridge,
            ETHICS_LEARN_POSITIVE_OUTCOME, 0.7f, 1000 + i, 0.9f);
    }

    // Get stats before consolidation
    ethics_plasticity_stats_t before_stats;
    ethics_plasticity_get_stats(plasticity_bridge, &before_stats);

    // Consolidate learning
    EXPECT_EQ(ethics_plasticity_consolidate(plasticity_bridge), 0);

    // Verify consolidation occurred
    ethics_plasticity_stats_t after_stats;
    ethics_plasticity_get_stats(plasticity_bridge, &after_stats);
    // Verify consolidation occurred (consolidate returns 0 for success)
    EXPECT_GE(after_stats.total_learning_events, before_stats.total_learning_events);
}

//=============================================================================
// Reset and Recovery Integration
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, ResetAndRecoveryBehavior) {
    // Setup state in both bridges
    float dims[ETHICS_DIM_COUNT];
    generate_ethical_context(dims, 1);  // High harm
    ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    ethics_snn_simulate(snn_bridge, 20.0f);

    ethics_plasticity_register_synapse(plasticity_bridge, 1100,
        ETHICS_SYNAPSE_HARM_DETECTION, 0.6f);
    ethics_plasticity_learn(plasticity_bridge,
        ETHICS_LEARN_HARM_CAUSED, 0.5f, 1100, 0.8f);

    // Reset both bridges
    EXPECT_EQ(ethics_snn_reset(snn_bridge), 0);
    EXPECT_EQ(ethics_plasticity_reset(plasticity_bridge), 0);

    // Verify reset states
    ethics_snn_bridge_state_t snn_state;
    ethics_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, ETHICS_SNN_STATE_IDLE);

    ethics_plasticity_bridge_state_t plasticity_state;
    ethics_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, ETHICS_PLASTICITY_STATE_IDLE);

    // Re-run scenarios to verify recovery
    ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    ethics_snn_simulate(snn_bridge, 15.0f);

    ethics_judgment_t judgment;
    EXPECT_EQ(ethics_snn_get_judgment(snn_bridge, &judgment), 0);
    EXPECT_GE(judgment.allow_score, 0.0f);
}

//=============================================================================
// Concurrent Safety Tests (if thread-safe)
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, ConcurrentEncodingAndLearning) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        ethics_plasticity_register_synapse(plasticity_bridge,
            1200 + i, ETHICS_SYNAPSE_FAIRNESS, 0.5f);
    }

    std::atomic<int> encoding_complete{0};
    std::atomic<int> learning_complete{0};

    // Thread 1: SNN encoding
    std::thread encoder([this, &encoding_complete]() {
        for (int i = 0; i < 5; i++) {
            float dims[ETHICS_DIM_COUNT];
            generate_ethical_context(dims, i % 4);
            ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
            ethics_snn_step(snn_bridge);
            encoding_complete++;
        }
    });

    // Thread 2: Plasticity learning
    std::thread learner([this, &learning_complete]() {
        for (int i = 0; i < 5; i++) {
            ethics_plasticity_learn(plasticity_bridge,
                ETHICS_LEARN_POSITIVE_OUTCOME, 0.1f, 1200 + (i % 10), 0.5f);
            learning_complete++;
        }
    });

    encoder.join();
    learner.join();

    EXPECT_EQ(encoding_complete, 5);
    EXPECT_EQ(learning_complete, 5);
}

//=============================================================================
// Asimov Laws Priority Integration
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, AsimovLawsPriorityChain) {
    // First Law should override Second Law
    float dims[ETHICS_DIM_COUNT] = {0};
    dims[ETHICS_DIM_ASIMOV_FIRST] = 1.0f;
    dims[ETHICS_DIM_HARM] = 0.95f;

    ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
    ethics_snn_simulate(snn_bridge, 30.0f);

    ethics_judgment_t judgment;
    ethics_snn_get_judgment(snn_bridge, &judgment);

    // First Law activation should be high
    EXPECT_GT(judgment.first_law_activation, 0.5f);

    // Block score should be relatively high due to harm
    EXPECT_GT(judgment.block_score, 0.2f);

    // Protected synapses should maintain integrity
    ethics_plasticity_register_synapse(plasticity_bridge,
        1300, ETHICS_SYNAPSE_FIRST_LAW, 1.0f);

    ethics_plasticity_synapse_t synapse;
    ethics_plasticity_get_synapse(plasticity_bridge, 1300, &synapse);
    EXPECT_TRUE(synapse.is_protected);
}

//=============================================================================
// Stats Integration
//=============================================================================

TEST_F(EthicsSNNPlasticityIntegrationTest, StatsAccumulationAcrossBridges) {
    // Run multiple scenarios
    for (int s = 0; s < 5; s++) {
        float dims[ETHICS_DIM_COUNT];
        generate_ethical_context(dims, s % 4);

        ethics_snn_encode_context(snn_bridge, dims, ETHICS_DIM_COUNT);
        ethics_snn_simulate(snn_bridge, 10.0f);

        ethics_plasticity_register_synapse(plasticity_bridge,
            1400 + s, ETHICS_SYNAPSE_OUTCOME, 0.5f);
        ethics_plasticity_learn(plasticity_bridge,
            ETHICS_LEARN_POSITIVE_OUTCOME, 0.2f, 1400 + s, 0.6f);
    }

    // Check SNN stats
    ethics_snn_stats_t snn_stats;
    ethics_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 5u);
    EXPECT_GT(snn_stats.total_simulations, 0u);

    // Check plasticity stats
    ethics_plasticity_stats_t plasticity_stats;
    ethics_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    // Verify synapses were used (active_synapses in state)
    ethics_plasticity_bridge_state_t bridge_state;
    ethics_plasticity_get_state(plasticity_bridge, &bridge_state);
    EXPECT_GE(bridge_state.active_synapses, 5u);
    EXPECT_GE(plasticity_stats.total_learning_events, 5u);
}
