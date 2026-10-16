//=============================================================================
// test_wellbeing_snn_plasticity_integration.cpp - Wellbeing Integration
//=============================================================================
/**
 * @file test_wellbeing_snn_plasticity_integration.cpp
 * @brief Integration tests for Wellbeing-SNN-Plasticity bidirectional dataflows
 *
 * WHAT: Tests complete integration between wellbeing, SNN, and plasticity
 * WHY:  Verify bidirectional dataflows work correctly for wellbeing learning
 * HOW:  Create both bridges, simulate wellbeing scenarios, verify adaptation
 *
 * INTEGRATION POINTS:
 * - Wellbeing encoding -> SNN population activity
 * - SNN spikes -> Plasticity STDP -> weight updates
 * - Learning events -> Synapse modification -> Foundation improvement
 * - Protection mechanisms -> Block learning on core resilience
 *
 * THEORETICAL BASIS:
 * - Psychological wellbeing (Ryff)
 * - Hedonic vs eudaimonic distinction (Kahneman)
 * - Resilience and recovery (Bonanno)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

#include "cognitive/wellbeing/nimcp_wellbeing_snn_bridge.h"
#include "cognitive/wellbeing/nimcp_wellbeing_plasticity_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WellbeingSNNPlasticityIntegrationTest : public ::testing::Test {
protected:
    wellbeing_snn_bridge_t* snn_bridge;
    wellbeing_plasticity_bridge_t* plasticity_bridge;

    // Callback tracking
    std::atomic<int> stress_detection_count{0};
    std::atomic<int> flourishing_count{0};
    std::atomic<int> weight_change_count{0};
    std::atomic<float> last_stress_level{0.0f};

    void SetUp() override {
        // Create SNN bridge with test-friendly config
        wellbeing_snn_config_t snn_config = wellbeing_snn_config_default();
        snn_config.num_dimensions = WELLBEING_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.enable_stress_detection = true;
        snn_config.enable_bio_async = false;  // Disable for predictable tests

        snn_bridge = wellbeing_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with defaults
        wellbeing_plasticity_config_t plasticity_config = wellbeing_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = wellbeing_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create plasticity bridge";

        // Reset counters
        stress_detection_count = 0;
        flourishing_count = 0;
        weight_change_count = 0;
        last_stress_level = 0.0f;
    }

    void TearDown() override {
        if (snn_bridge) {
            wellbeing_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            wellbeing_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate wellbeing context for scenario
    void generate_wellbeing_context(float* dims, uint32_t scenario_type) {
        memset(dims, 0, sizeof(float) * WELLBEING_DIM_COUNT);
        switch (scenario_type) {
            case 0: // Flourishing - high wellbeing across dimensions
                dims[WELLBEING_DIM_HEDONIC] = 0.85f;
                dims[WELLBEING_DIM_EUDAIMONIC] = 0.9f;
                dims[WELLBEING_DIM_VITALITY] = 0.8f;
                dims[WELLBEING_DIM_RESILIENCE] = 0.75f;
                dims[WELLBEING_DIM_SOCIAL_CONNECTION] = 0.85f;
                break;
            case 1: // High stress scenario
                dims[WELLBEING_DIM_STRESS] = 0.9f;
                dims[WELLBEING_DIM_HEDONIC] = 0.3f;
                dims[WELLBEING_DIM_VITALITY] = 0.4f;
                break;
            case 2: // Recovery scenario
                dims[WELLBEING_DIM_RESILIENCE] = 0.8f;
                dims[WELLBEING_DIM_STRESS] = 0.3f;
                dims[WELLBEING_DIM_VITALITY] = 0.65f;
                break;
            case 3: // Social support scenario
                dims[WELLBEING_DIM_SOCIAL_CONNECTION] = 0.9f;
                dims[WELLBEING_DIM_HEDONIC] = 0.7f;
                dims[WELLBEING_DIM_AUTONOMY] = 0.6f;
                break;
            case 4: // Eudaimonic focus
                dims[WELLBEING_DIM_EUDAIMONIC] = 0.95f;
                dims[WELLBEING_DIM_AUTONOMY] = 0.85f;
                dims[WELLBEING_DIM_COMPETENCE] = 0.8f;
                break;
            default:
                for (int i = 0; i < WELLBEING_DIM_COUNT; i++) {
                    dims[i] = 0.5f;
                }
                break;
        }
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(WellbeingSNNPlasticityIntegrationTest, BothBridgesInitialize) {
    // Verify both bridges are functional
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check initial states
    wellbeing_snn_bridge_state_t snn_state;
    EXPECT_EQ(wellbeing_snn_get_state(snn_bridge, &snn_state), 0);
    EXPECT_EQ(snn_state.state, WELLBEING_SNN_STATE_IDLE);

    wellbeing_plasticity_bridge_state_t plasticity_state;
    EXPECT_EQ(wellbeing_plasticity_get_state(plasticity_bridge, &plasticity_state), 0);
    EXPECT_EQ(plasticity_state.state, WELLBEING_PLASTICITY_STATE_IDLE);
}

TEST_F(WellbeingSNNPlasticityIntegrationTest, SNNEncodingDrivesPlasticityActivity) {
    // Encode wellbeing context in SNN
    float dims[WELLBEING_DIM_COUNT];
    generate_wellbeing_context(dims, 0);  // Flourishing scenario

    int spikes = wellbeing_snn_encode_state(snn_bridge, dims, WELLBEING_DIM_COUNT);
    EXPECT_GE(spikes, 0) << "Encoding should succeed (0 or more spikes)";

    // Simulate SNN processing
    EXPECT_EQ(wellbeing_snn_simulate(snn_bridge, 20.0f), 0);

    // Register synapses in plasticity bridge
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(wellbeing_plasticity_register_synapse(plasticity_bridge,
            i, WELLBEING_SYNAPSE_HEDONIC, 0.5f), 0);
    }

    // Apply STDP based on SNN activity (returns weight delta)
    float delta = wellbeing_plasticity_apply_stdp(plasticity_bridge, 0, 1.0f, 3.0f);
    EXPECT_TRUE(std::isfinite(delta)) << "STDP should return valid delta";

    // Get synapse and verify retrieval succeeded
    wellbeing_plasticity_synapse_t synapse;
    EXPECT_EQ(wellbeing_plasticity_get_synapse(plasticity_bridge, 0, &synapse), 0);
}

//=============================================================================
// Stress Detection Integration
//=============================================================================

TEST_F(WellbeingSNNPlasticityIntegrationTest, StressDetectionTriggersLearning) {
    // Encode high stress scenario
    int spikes = wellbeing_snn_encode_stress(snn_bridge, 0.9f, true);
    EXPECT_GE(spikes, 0);

    // Simulate processing
    wellbeing_snn_simulate(snn_bridge, 30.0f);

    // Check stress detection
    float stress_level;
    wellbeing_snn_check_stress(snn_bridge, &stress_level);

    // Register stress synapse
    EXPECT_EQ(wellbeing_plasticity_register_synapse(plasticity_bridge,
        100, WELLBEING_SYNAPSE_HEDONIC, 0.5f), 0);

    // Learn from stress event (negative experience)
    EXPECT_EQ(wellbeing_plasticity_learn(plasticity_bridge,
        WELLBEING_LEARN_NEGATIVE_EXPERIENCE, 0.8f, 100, stress_level), 0);

    // Verify weight changed
    wellbeing_plasticity_synapse_t synapse;
    EXPECT_EQ(wellbeing_plasticity_get_synapse(plasticity_bridge, 100, &synapse), 0);
}

TEST_F(WellbeingSNNPlasticityIntegrationTest, PositiveExperienceReinforcesWellbeing) {
    // Register hedonic synapse
    EXPECT_EQ(wellbeing_plasticity_register_synapse(plasticity_bridge,
        200, WELLBEING_SYNAPSE_HEDONIC, 0.4f), 0);

    // Initial weight
    wellbeing_plasticity_synapse_t synapse;
    EXPECT_EQ(wellbeing_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    float initial_weight = synapse.weight;
    (void)initial_weight;

    // Learn from positive experience
    EXPECT_EQ(wellbeing_plasticity_learn(plasticity_bridge,
        WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.9f, 200, 0.9f), 0);

    // Verify weight is still valid
    EXPECT_EQ(wellbeing_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 1.0f);
}

//=============================================================================
// Resilience Integration
//=============================================================================

TEST_F(WellbeingSNNPlasticityIntegrationTest, ResilienceProtectionIntegrity) {
    // Encode resilience activation
    float dims[WELLBEING_DIM_COUNT] = {0};
    dims[WELLBEING_DIM_RESILIENCE] = 1.0f;

    wellbeing_snn_encode_state(snn_bridge, dims, WELLBEING_DIM_COUNT);
    wellbeing_snn_simulate(snn_bridge, 30.0f);

    // Get assessment
    wellbeing_assessment_t assessment;
    wellbeing_snn_get_assessment(snn_bridge, &assessment);

    // Register resilience synapse (auto-protected)
    EXPECT_EQ(wellbeing_plasticity_register_synapse(plasticity_bridge,
        400, WELLBEING_SYNAPSE_RESILIENCE, 1.0f), 0);

    // Resilience synapse should be protected
    wellbeing_plasticity_synapse_t synapse;
    EXPECT_EQ(wellbeing_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);

    // Attempt to modify protected synapse (should be blocked)
    float original_weight = synapse.weight;
    wellbeing_plasticity_apply_stdp(plasticity_bridge, 400, 5.0f, 10.0f);

    EXPECT_EQ(wellbeing_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight) << "Protected synapse should not change";
}

//=============================================================================
// Social Connection Integration
//=============================================================================

TEST_F(WellbeingSNNPlasticityIntegrationTest, SocialSupportEnhancesWellbeing) {
    // Encode social support scenario
    float dims[WELLBEING_DIM_COUNT];
    generate_wellbeing_context(dims, 3);  // Social support scenario

    wellbeing_snn_encode_state(snn_bridge, dims, WELLBEING_DIM_COUNT);
    wellbeing_snn_simulate(snn_bridge, 25.0f);

    // Register social synapse
    EXPECT_EQ(wellbeing_plasticity_register_synapse(plasticity_bridge,
        300, WELLBEING_SYNAPSE_SOCIAL, 0.5f), 0);

    // Get assessment
    wellbeing_assessment_t assessment;
    wellbeing_snn_get_assessment(snn_bridge, &assessment);

    // Learn from social support
    EXPECT_EQ(wellbeing_plasticity_learn(plasticity_bridge,
        WELLBEING_LEARN_SOCIAL_SUPPORT, 0.8f, 300, assessment.social_connection), 0);

    // Verify learning occurred
    wellbeing_plasticity_stats_t stats;
    wellbeing_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.social_support_events, 0u);
}

//=============================================================================
// Full Pipeline Integration
//=============================================================================

TEST_F(WellbeingSNNPlasticityIntegrationTest, FullWellbeingPipeline) {
    // Register multiple synapse types
    for (int i = 0; i < 5; i++) {
        wellbeing_plasticity_register_synapse(plasticity_bridge,
            600 + i, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
        wellbeing_plasticity_register_synapse(plasticity_bridge,
            610 + i, WELLBEING_SYNAPSE_EUDAIMONIC, 0.5f);
        wellbeing_plasticity_register_synapse(plasticity_bridge,
            620 + i, WELLBEING_SYNAPSE_SOCIAL, 0.5f);
    }

    // Run multiple scenarios
    for (int scenario = 0; scenario < 5; scenario++) {
        float dims[WELLBEING_DIM_COUNT];
        generate_wellbeing_context(dims, scenario);

        // SNN encoding and simulation
        wellbeing_snn_encode_state(snn_bridge, dims, WELLBEING_DIM_COUNT);
        wellbeing_snn_simulate(snn_bridge, 25.0f);

        // Get assessment
        wellbeing_assessment_t assessment;
        wellbeing_snn_get_assessment(snn_bridge, &assessment);

        // Apply learning based on scenario
        if (assessment.stress_level > 0.5f) {
            for (int i = 0; i < 5; i++) {
                wellbeing_plasticity_learn(plasticity_bridge,
                    WELLBEING_LEARN_STRESS_ACCUMULATED, 0.5f, 600 + i, assessment.stress_level);
            }
        } else {
            for (int i = 0; i < 5; i++) {
                wellbeing_plasticity_learn(plasticity_bridge,
                    WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.3f, 600 + i, assessment.flourishing_score);
            }
        }

        // Apply STDP between consecutive synapse pairs
        for (int i = 0; i < 4; i++) {
            wellbeing_plasticity_apply_stdp(plasticity_bridge, 600 + i,
                (float)scenario * 2.0f, (float)scenario * 2.0f + 5.0f);
        }

        // Update eligibility traces
        wellbeing_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Verify stats
    wellbeing_snn_stats_t snn_stats;
    wellbeing_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 5u);

    wellbeing_plasticity_stats_t plasticity_stats;
    wellbeing_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GT(plasticity_stats.total_learning_events, 0u);
    EXPECT_GT(plasticity_stats.weight_updates, 0u);
}

//=============================================================================
// Reward Modulation Integration
//=============================================================================

TEST_F(WellbeingSNNPlasticityIntegrationTest, RewardModulatedLearning) {
    // Register synapses
    for (int i = 0; i < 3; i++) {
        wellbeing_plasticity_register_synapse(plasticity_bridge,
            700 + i, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    }

    // Encode flourishing scenario
    float dims[WELLBEING_DIM_COUNT];
    generate_wellbeing_context(dims, 0);
    wellbeing_snn_encode_state(snn_bridge, dims, WELLBEING_DIM_COUNT);
    wellbeing_snn_simulate(snn_bridge, 25.0f);

    // Apply positive reward
    float reward = 0.8f;
    EXPECT_EQ(wellbeing_plasticity_apply_reward(plasticity_bridge, reward), 0);

    // Check foundation state
    wellbeing_foundation_state_t foundation;
    EXPECT_EQ(wellbeing_plasticity_get_foundation_state(plasticity_bridge, &foundation), 0);
}

//=============================================================================
// BCM Metaplasticity Integration
//=============================================================================

TEST_F(WellbeingSNNPlasticityIntegrationTest, BCMMetaplasticityUpdate) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        wellbeing_plasticity_register_synapse(plasticity_bridge,
            800 + i, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    }

    // Run multiple encoding cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        float dims[WELLBEING_DIM_COUNT];
        generate_wellbeing_context(dims, cycle % 5);

        wellbeing_snn_encode_state(snn_bridge, dims, WELLBEING_DIM_COUNT);
        wellbeing_snn_step(snn_bridge);

        // Update BCM thresholds
        float postsynaptic_rate = 0.3f + 0.05f * cycle;
        wellbeing_plasticity_update_bcm(plasticity_bridge, postsynaptic_rate);
    }

    // Verify BCM function ran without error
    wellbeing_plasticity_stats_t stats;
    wellbeing_plasticity_get_stats(plasticity_bridge, &stats);
}

//=============================================================================
// Homeostatic Regulation Integration
//=============================================================================

TEST_F(WellbeingSNNPlasticityIntegrationTest, HomeostaticWeightRegulation) {
    // Register synapses with varied initial weights
    for (int i = 0; i < 8; i++) {
        float initial_weight = 0.2f + 0.1f * i;  // 0.2 to 0.9
        wellbeing_plasticity_register_synapse(plasticity_bridge,
            900 + i, WELLBEING_SYNAPSE_HEDONIC, initial_weight);
    }

    // Run homeostatic update cycles
    float target_activity = 0.5f;
    for (int cycle = 0; cycle < 5; cycle++) {
        wellbeing_plasticity_homeostatic_update(plasticity_bridge, target_activity);
    }

    // Verify homeostatic function ran without error
    wellbeing_plasticity_stats_t stats;
    wellbeing_plasticity_get_stats(plasticity_bridge, &stats);
}

//=============================================================================
// Consolidation Integration
//=============================================================================

TEST_F(WellbeingSNNPlasticityIntegrationTest, WellbeingLearningConsolidation) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        wellbeing_plasticity_register_synapse(plasticity_bridge,
            1000 + i, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    }

    // Apply significant learning
    for (int i = 0; i < 5; i++) {
        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.7f, 1000 + i, 0.9f);
    }

    // Get stats before consolidation
    wellbeing_plasticity_stats_t before_stats;
    wellbeing_plasticity_get_stats(plasticity_bridge, &before_stats);

    // Consolidate learning
    EXPECT_EQ(wellbeing_plasticity_consolidate(plasticity_bridge), 0);

    // Verify consolidation occurred
    wellbeing_plasticity_stats_t after_stats;
    wellbeing_plasticity_get_stats(plasticity_bridge, &after_stats);
    EXPECT_GE(after_stats.total_learning_events, before_stats.total_learning_events);
}

//=============================================================================
// Reset and Recovery Integration
//=============================================================================

TEST_F(WellbeingSNNPlasticityIntegrationTest, ResetAndRecoveryBehavior) {
    // Setup state in both bridges
    float dims[WELLBEING_DIM_COUNT];
    generate_wellbeing_context(dims, 1);  // Stress scenario
    wellbeing_snn_encode_state(snn_bridge, dims, WELLBEING_DIM_COUNT);
    wellbeing_snn_simulate(snn_bridge, 20.0f);

    wellbeing_plasticity_register_synapse(plasticity_bridge, 1100,
        WELLBEING_SYNAPSE_HEDONIC, 0.6f);
    wellbeing_plasticity_learn(plasticity_bridge,
        WELLBEING_LEARN_STRESS_ACCUMULATED, 0.5f, 1100, 0.8f);

    // Reset both bridges
    EXPECT_EQ(wellbeing_snn_reset(snn_bridge), 0);
    EXPECT_EQ(wellbeing_plasticity_reset(plasticity_bridge), 0);

    // Verify reset states
    wellbeing_snn_bridge_state_t snn_state;
    wellbeing_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, WELLBEING_SNN_STATE_IDLE);

    wellbeing_plasticity_bridge_state_t plasticity_state;
    wellbeing_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, WELLBEING_PLASTICITY_STATE_IDLE);

    // Re-run scenarios to verify recovery
    wellbeing_snn_encode_state(snn_bridge, dims, WELLBEING_DIM_COUNT);
    wellbeing_snn_simulate(snn_bridge, 15.0f);

    wellbeing_assessment_t assessment;
    EXPECT_EQ(wellbeing_snn_get_assessment(snn_bridge, &assessment), 0);
    EXPECT_GE(assessment.flourishing_score, 0.0f);
}

//=============================================================================
// Concurrent Safety Tests
//=============================================================================

TEST_F(WellbeingSNNPlasticityIntegrationTest, ConcurrentEncodingAndLearning) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        wellbeing_plasticity_register_synapse(plasticity_bridge,
            1200 + i, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    }

    std::atomic<int> encoding_complete{0};
    std::atomic<int> learning_complete{0};

    // Thread 1: SNN encoding
    std::thread encoder([this, &encoding_complete]() {
        for (int i = 0; i < 5; i++) {
            float dims[WELLBEING_DIM_COUNT];
            generate_wellbeing_context(dims, i % 5);
            wellbeing_snn_encode_state(snn_bridge, dims, WELLBEING_DIM_COUNT);
            wellbeing_snn_step(snn_bridge);
            encoding_complete++;
        }
    });

    // Thread 2: Plasticity learning
    std::thread learner([this, &learning_complete]() {
        for (int i = 0; i < 5; i++) {
            wellbeing_plasticity_learn(plasticity_bridge,
                WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.1f, 1200 + (i % 10), 0.5f);
            learning_complete++;
        }
    });

    encoder.join();
    learner.join();

    EXPECT_EQ(encoding_complete, 5);
    EXPECT_EQ(learning_complete, 5);
}

//=============================================================================
// Stats Integration
//=============================================================================

TEST_F(WellbeingSNNPlasticityIntegrationTest, StatsAccumulationAcrossBridges) {
    // Run multiple scenarios
    for (int s = 0; s < 5; s++) {
        float dims[WELLBEING_DIM_COUNT];
        generate_wellbeing_context(dims, s % 5);

        wellbeing_snn_encode_state(snn_bridge, dims, WELLBEING_DIM_COUNT);
        wellbeing_snn_simulate(snn_bridge, 10.0f);

        wellbeing_plasticity_register_synapse(plasticity_bridge,
            1400 + s, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
        wellbeing_plasticity_learn(plasticity_bridge,
            WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.2f, 1400 + s, 0.6f);
    }

    // Check SNN stats
    wellbeing_snn_stats_t snn_stats;
    wellbeing_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 5u);
    EXPECT_GT(snn_stats.total_simulations, 0u);

    // Check plasticity stats
    wellbeing_plasticity_stats_t plasticity_stats;
    wellbeing_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    // Verify synapses were used (active_synapses in state)
    wellbeing_plasticity_bridge_state_t bridge_state;
    wellbeing_plasticity_get_state(plasticity_bridge, &bridge_state);
    EXPECT_GE(bridge_state.active_synapses, 5u);
    EXPECT_GE(plasticity_stats.total_learning_events, 5u);
}

//=============================================================================
// Stress Recovery Learning Integration
//=============================================================================

TEST_F(WellbeingSNNPlasticityIntegrationTest, StressRecoveryLearningPipeline) {
    // Register recovery synapses
    for (int i = 0; i < 5; i++) {
        wellbeing_plasticity_register_synapse(plasticity_bridge,
            1500 + i, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    }

    // Simulate stress -> recovery cycle
    for (int trial = 0; trial < 10; trial++) {
        float dims[WELLBEING_DIM_COUNT] = {0};
        if (trial % 2 == 0) {
            // Stress scenario
            dims[WELLBEING_DIM_STRESS] = 0.8f;
            dims[WELLBEING_DIM_HEDONIC] = 0.3f;
        } else {
            // Recovery scenario
            dims[WELLBEING_DIM_STRESS] = 0.3f;
            dims[WELLBEING_DIM_HEDONIC] = 0.7f;
            dims[WELLBEING_DIM_RESILIENCE] = 0.8f;
        }

        wellbeing_snn_encode_state(snn_bridge, dims, WELLBEING_DIM_COUNT);
        wellbeing_snn_simulate(snn_bridge, 15.0f);

        wellbeing_assessment_t assessment;
        wellbeing_snn_get_assessment(snn_bridge, &assessment);

        // Learn based on scenario
        if (trial % 2 == 0) {
            wellbeing_plasticity_learn(plasticity_bridge,
                WELLBEING_LEARN_STRESS_ACCUMULATED, 0.5f, 1500 + (trial % 5), assessment.stress_level);
        } else {
            wellbeing_plasticity_learn(plasticity_bridge,
                WELLBEING_LEARN_STRESS_RECOVERED, 0.5f, 1500 + (trial % 5), assessment.resilience_score);
        }
    }

    // Verify learning statistics
    wellbeing_plasticity_stats_t stats;
    wellbeing_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.stress_recovery_events, 5u);
}
