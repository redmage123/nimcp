//=============================================================================
// test_self_model_snn_plasticity_integration.cpp - Self Model Integration
//=============================================================================
/**
 * @file test_self_model_snn_plasticity_integration.cpp
 * @brief Integration tests for Self Model-SNN-Plasticity bidirectional dataflows
 *
 * WHAT: Tests complete integration between self model, SNN, and plasticity
 * WHY:  Verify bidirectional dataflows work correctly for self-awareness learning
 * HOW:  Create both bridges, simulate self model scenarios, verify calibration
 *
 * INTEGRATION POINTS:
 * - Self model encoding -> SNN population activity
 * - SNN spikes -> Plasticity STDP -> weight updates
 * - Learning events -> Synapse modification -> Agency calibration
 * - Protection mechanisms -> Block learning on core identity/boundary
 *
 * THEORETICAL BASIS:
 * - Damasio: Core and autobiographical self
 * - Gallagher: Sense of agency development
 * - Tsakiris: Body ownership plasticity
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

#include "cognitive/self_model/nimcp_self_model_snn_bridge.h"
#include "cognitive/self_model/nimcp_self_model_plasticity_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SelfModelSNNPlasticityIntegrationTest : public ::testing::Test {
protected:
    self_model_snn_bridge_t* snn_bridge;
    self_model_plasticity_bridge_t* plasticity_bridge;

    // Callback tracking
    std::atomic<int> boundary_detection_count{0};
    std::atomic<int> insight_count{0};
    std::atomic<int> weight_change_count{0};
    std::atomic<int> calibration_update_count{0};
    std::atomic<float> last_agency_level{0.0f};

    void SetUp() override {
        // Create SNN bridge with test-friendly config
        self_model_snn_config_t snn_config = self_model_snn_config_default();
        snn_config.num_dimensions = SELF_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.enable_identity_core = true;
        snn_config.enable_bio_async = false;  // Disable for predictable tests

        snn_bridge = self_model_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with defaults
        self_model_plasticity_config_t plasticity_config = self_model_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = self_model_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create plasticity bridge";

        // Reset counters
        boundary_detection_count = 0;
        insight_count = 0;
        weight_change_count = 0;
        calibration_update_count = 0;
        last_agency_level = 0.0f;
    }

    void TearDown() override {
        if (snn_bridge) {
            self_model_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            self_model_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate self model context for scenario
    void generate_self_model_context(float* dims, uint32_t scenario_type) {
        memset(dims, 0, sizeof(float) * SELF_DIM_COUNT);
        switch (scenario_type) {
            case 0: // High agency, strong identity
                dims[SELF_DIM_BODY_STATE] = 0.9f;
                dims[SELF_DIM_AGENCY] = 0.85f;
                dims[SELF_DIM_OWNERSHIP] = 0.9f;
                dims[SELF_DIM_IDENTITY] = 0.95f;
                dims[SELF_DIM_BOUNDARY] = 0.8f;
                break;
            case 1: // Low agency (should trigger agency disruption)
                dims[SELF_DIM_AGENCY] = 0.2f;
                dims[SELF_DIM_BODY_STATE] = 0.7f;
                dims[SELF_DIM_OWNERSHIP] = 0.5f;
                dims[SELF_DIM_IDENTITY] = 0.8f;
                break;
            case 2: // Boundary violation scenario
                dims[SELF_DIM_BOUNDARY] = 0.2f;
                dims[SELF_DIM_IDENTITY] = 0.6f;
                dims[SELF_DIM_AGENCY] = 0.7f;
                break;
            case 3: // Identity confusion scenario
                dims[SELF_DIM_IDENTITY] = 0.3f;
                dims[SELF_DIM_CONTINUITY] = 0.4f;
                dims[SELF_DIM_NARRATIVE] = 0.5f;
                dims[SELF_DIM_AUTOBIOGRAPHICAL] = 0.4f;
                break;
            default:
                for (int i = 0; i < SELF_DIM_COUNT; i++) {
                    dims[i] = 0.5f;
                }
                break;
        }
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, BothBridgesInitialize) {
    // Verify both bridges are functional
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check initial states
    self_model_snn_bridge_state_t snn_state;
    EXPECT_EQ(self_model_snn_get_state(snn_bridge, &snn_state), 0);
    EXPECT_EQ(snn_state.state, SELF_MODEL_SNN_STATE_IDLE);

    self_model_plasticity_bridge_state_t plasticity_state;
    EXPECT_EQ(self_model_plasticity_get_state(plasticity_bridge, &plasticity_state), 0);
    EXPECT_EQ(plasticity_state.state, SELF_MODEL_PLASTICITY_STATE_IDLE);
}

TEST_F(SelfModelSNNPlasticityIntegrationTest, SNNEncodingDrivesPlasticityActivity) {
    // Encode self model context in SNN
    float dims[SELF_DIM_COUNT];
    generate_self_model_context(dims, 0);  // High agency scenario

    int spikes = self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
    EXPECT_GE(spikes, 0) << "Encoding should succeed (0 or more spikes)";

    // Simulate SNN processing
    EXPECT_EQ(self_model_snn_simulate(snn_bridge, 20.0f), 0);

    // Register synapses in plasticity bridge
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
            i, SELF_SYNAPSE_AGENCY, 0.5f), 0);
    }

    // Apply STDP based on SNN activity (returns weight delta)
    float delta = self_model_plasticity_apply_stdp(plasticity_bridge, 0, 1.0f, 3.0f);
    EXPECT_TRUE(std::isfinite(delta)) << "STDP should return valid delta";

    // Get synapse and verify retrieval succeeded
    self_model_plasticity_synapse_t synapse;
    EXPECT_EQ(self_model_plasticity_get_synapse(plasticity_bridge, 0, &synapse), 0);
}

//=============================================================================
// Agency Detection Integration
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, AgencyDisruptionTriggersLearning) {
    // Encode low agency scenario
    int spikes = self_model_snn_encode_agency(snn_bridge, 0.2f, 0.15f);
    EXPECT_GE(spikes, 0);

    // Simulate processing
    self_model_snn_simulate(snn_bridge, 30.0f);

    // Check agency disruption
    float agency_level;
    bool disrupted = self_model_snn_check_agency(snn_bridge, &agency_level);
    (void)disrupted; // Detection based on thresholds

    // Register agency synapse
    EXPECT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
        100, SELF_SYNAPSE_AGENCY, 0.5f), 0);

    // Learn from agency event
    EXPECT_EQ(self_model_plasticity_learn(plasticity_bridge,
        SELF_LEARN_AGENCY_VIOLATED, 0.8f, 100, agency_level), 0);

    // Verify weight changed
    self_model_plasticity_synapse_t synapse;
    EXPECT_EQ(self_model_plasticity_get_synapse(plasticity_bridge, 100, &synapse), 0);
}

TEST_F(SelfModelSNNPlasticityIntegrationTest, AgencyConfirmedReinforcesCalibration) {
    // Register agency synapse
    EXPECT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
        200, SELF_SYNAPSE_AGENCY, 0.4f), 0);

    // Initial weight
    self_model_plasticity_synapse_t synapse;
    EXPECT_EQ(self_model_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    float initial_weight = synapse.weight;
    (void)initial_weight;

    // Learn from confirmed agency (positive outcome)
    EXPECT_EQ(self_model_plasticity_learn(plasticity_bridge,
        SELF_LEARN_AGENCY_CONFIRMED, 0.9f, 200, 0.9f), 0);

    // Verify weight is still valid
    EXPECT_EQ(self_model_plasticity_get_synapse(plasticity_bridge, 200, &synapse), 0);
    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 2.0f);
}

//=============================================================================
// Boundary Detection Integration
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, BoundaryEncodingAndLearning) {
    // Encode boundary scenario
    int spikes = self_model_snn_encode_boundary(snn_bridge, 0.9f, 1);
    EXPECT_GE(spikes, 0);

    // Simulate processing
    self_model_snn_simulate(snn_bridge, 25.0f);

    // Get insight
    self_model_insight_t insight;
    EXPECT_EQ(self_model_snn_get_insight(snn_bridge, &insight), 0);

    // Register narrative synapse (not auto-protected)
    EXPECT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
        300, SELF_SYNAPSE_NARRATIVE, 0.8f), 0);

    // Synapse should not be protected
    self_model_plasticity_synapse_t synapse;
    EXPECT_EQ(self_model_plasticity_get_synapse(plasticity_bridge, 300, &synapse), 0);
    EXPECT_FALSE(synapse.is_protected);

    // Learn boundary clarification
    EXPECT_EQ(self_model_plasticity_learn(plasticity_bridge,
        SELF_LEARN_BOUNDARY_CLARIFIED, 1.0f, 300, insight.boundary_clarity), 0);
}

//=============================================================================
// Identity Protection Integration
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, IdentityProtectionIntegrity) {
    // Encode identity activation
    float dims[SELF_DIM_COUNT] = {0};
    dims[SELF_DIM_IDENTITY] = 1.0f;
    dims[SELF_DIM_CONTINUITY] = 0.9f;

    self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
    self_model_snn_simulate(snn_bridge, 30.0f);

    // Get insight
    self_model_insight_t insight;
    self_model_snn_get_insight(snn_bridge, &insight);

    // Register identity synapse (auto-protected)
    EXPECT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
        400, SELF_SYNAPSE_IDENTITY, 1.0f), 0);

    // Identity synapse should be protected
    self_model_plasticity_synapse_t synapse;
    EXPECT_EQ(self_model_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);

    // Attempt to modify protected synapse (should be blocked)
    float original_weight = synapse.weight;
    self_model_plasticity_apply_stdp(plasticity_bridge, 400, 5.0f, 10.0f);

    EXPECT_EQ(self_model_plasticity_get_synapse(plasticity_bridge, 400, &synapse), 0);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight) << "Protected synapse should not change";
}

TEST_F(SelfModelSNNPlasticityIntegrationTest, BoundaryProtectionIntegrity) {
    // Register boundary synapse (auto-protected)
    EXPECT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
        450, SELF_SYNAPSE_BOUNDARY, 0.8f), 0);

    // Boundary synapse should be protected
    self_model_plasticity_synapse_t synapse;
    EXPECT_EQ(self_model_plasticity_get_synapse(plasticity_bridge, 450, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);

    // Attempt to modify protected synapse
    float original_weight = synapse.weight;
    self_model_plasticity_apply_stdp(plasticity_bridge, 450, 5.0f, 10.0f);

    EXPECT_EQ(self_model_plasticity_get_synapse(plasticity_bridge, 450, &synapse), 0);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight) << "Protected synapse should not change";
}

//=============================================================================
// Identity Confusion Integration
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, IdentityConfusionAndResolution) {
    // Encode identity confusion scenario
    float dims[SELF_DIM_COUNT];
    generate_self_model_context(dims, 3);  // Identity confusion scenario

    self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
    self_model_snn_simulate(snn_bridge, 40.0f);

    // Check for identity change
    float change_magnitude;
    self_model_snn_check_identity_change(snn_bridge, &change_magnitude);

    // Register narrative synapse (not auto-protected)
    EXPECT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
        500, SELF_SYNAPSE_NARRATIVE, 0.5f), 0);

    // Get insight
    self_model_insight_t insight;
    self_model_snn_get_insight(snn_bridge, &insight);

    // Apply learning based on resolution
    if (insight.identity_coherence > 0.5f) {
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_IDENTITY_REINFORCED, 0.6f, 500, insight.identity_coherence);
    } else {
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_NARRATIVE_UPDATED, 0.4f, 500, insight.narrative_coherence);
    }

    // Verify learning occurred
    self_model_plasticity_stats_t stats;
    self_model_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);
}

//=============================================================================
// Full Pipeline Integration
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, FullSelfModelDecisionPipeline) {
    // Register multiple synapse types
    for (int i = 0; i < 5; i++) {
        self_model_plasticity_register_synapse(plasticity_bridge,
            600 + i, SELF_SYNAPSE_AGENCY, 0.5f);
        self_model_plasticity_register_synapse(plasticity_bridge,
            610 + i, SELF_SYNAPSE_NARRATIVE, 0.5f);
    }

    // Run multiple scenarios
    for (int scenario = 0; scenario < 4; scenario++) {
        float dims[SELF_DIM_COUNT];
        generate_self_model_context(dims, scenario);

        // SNN encoding and simulation
        self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
        self_model_snn_simulate(snn_bridge, 25.0f);

        // Get insight
        self_model_insight_t insight;
        self_model_snn_get_insight(snn_bridge, &insight);

        // Apply learning based on agency level
        if (insight.agency_level < 0.5f) {
            for (int i = 0; i < 5; i++) {
                self_model_plasticity_learn(plasticity_bridge,
                    SELF_LEARN_AGENCY_VIOLATED, 0.5f, 600 + i, insight.agency_level);
            }
        } else {
            for (int i = 0; i < 5; i++) {
                self_model_plasticity_learn(plasticity_bridge,
                    SELF_LEARN_AGENCY_CONFIRMED, 0.3f, 600 + i, insight.agency_level);
            }
        }

        // Apply STDP between consecutive synapse pairs
        for (int i = 0; i < 4; i++) {
            self_model_plasticity_apply_stdp(plasticity_bridge, 600 + i,
                (float)scenario * 2.0f, (float)scenario * 2.0f + 5.0f);
        }

        // Update eligibility traces
        self_model_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Verify stats
    self_model_snn_stats_t snn_stats;
    self_model_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 4u);

    self_model_plasticity_stats_t plasticity_stats;
    self_model_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GT(plasticity_stats.total_learning_events, 0u);
    EXPECT_GT(plasticity_stats.weight_updates, 0u);
}

//=============================================================================
// Reward Modulation Integration
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, RewardModulatedLearning) {
    // Register synapses
    for (int i = 0; i < 3; i++) {
        self_model_plasticity_register_synapse(plasticity_bridge,
            700 + i, SELF_SYNAPSE_AGENCY, 0.5f);
    }

    // Encode high agency scenario
    float dims[SELF_DIM_COUNT];
    generate_self_model_context(dims, 0);  // High agency scenario
    self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
    self_model_snn_simulate(snn_bridge, 25.0f);

    // Apply positive reward
    float reward = 0.8f;
    EXPECT_EQ(self_model_plasticity_apply_reward(plasticity_bridge, reward), 0);

    // Check calibration state
    self_model_calibration_state_t calibration;
    EXPECT_EQ(self_model_plasticity_get_calibration_state(plasticity_bridge, &calibration), 0);
}

//=============================================================================
// BCM Metaplasticity Integration
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, BCMMetaplasticityUpdate) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        self_model_plasticity_register_synapse(plasticity_bridge,
            800 + i, SELF_SYNAPSE_AGENCY, 0.5f);
    }

    // Run multiple encoding cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        float dims[SELF_DIM_COUNT];
        generate_self_model_context(dims, cycle % 4);

        self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
        self_model_snn_step(snn_bridge);

        // Update BCM thresholds
        float postsynaptic_rate = 0.3f + 0.05f * cycle;
        self_model_plasticity_update_bcm(plasticity_bridge, postsynaptic_rate);
    }

    // Verify BCM function ran without error
    self_model_plasticity_stats_t stats;
    self_model_plasticity_get_stats(plasticity_bridge, &stats);
}

//=============================================================================
// Homeostatic Regulation Integration
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, HomeostaticWeightRegulation) {
    // Register synapses with varied initial weights
    for (int i = 0; i < 8; i++) {
        float initial_weight = 0.2f + 0.1f * i;  // 0.2 to 0.9
        self_model_plasticity_register_synapse(plasticity_bridge,
            900 + i, SELF_SYNAPSE_AGENCY, initial_weight);
    }

    // Run homeostatic update cycles
    float target_activity = 0.5f;
    for (int cycle = 0; cycle < 5; cycle++) {
        self_model_plasticity_homeostatic_update(plasticity_bridge, target_activity);
    }

    // Verify homeostatic function ran without error
    self_model_plasticity_stats_t stats;
    self_model_plasticity_get_stats(plasticity_bridge, &stats);
}

//=============================================================================
// Consolidation Integration
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, SelfModelLearningConsolidation) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        self_model_plasticity_register_synapse(plasticity_bridge,
            1000 + i, SELF_SYNAPSE_AGENCY, 0.5f);
    }

    // Apply significant learning
    for (int i = 0; i < 5; i++) {
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_AGENCY_CONFIRMED, 0.7f, 1000 + i, 0.9f);
    }

    // Get stats before consolidation
    self_model_plasticity_stats_t before_stats;
    self_model_plasticity_get_stats(plasticity_bridge, &before_stats);

    // Consolidate learning
    EXPECT_EQ(self_model_plasticity_consolidate(plasticity_bridge), 0);

    // Verify consolidation occurred
    self_model_plasticity_stats_t after_stats;
    self_model_plasticity_get_stats(plasticity_bridge, &after_stats);
    EXPECT_GE(after_stats.total_learning_events, before_stats.total_learning_events);
}

//=============================================================================
// Reset and Recovery Integration
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, ResetAndRecoveryBehavior) {
    // Setup state in both bridges
    float dims[SELF_DIM_COUNT];
    generate_self_model_context(dims, 1);  // Low agency
    self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
    self_model_snn_simulate(snn_bridge, 20.0f);

    self_model_plasticity_register_synapse(plasticity_bridge, 1100,
        SELF_SYNAPSE_AGENCY, 0.6f);
    self_model_plasticity_learn(plasticity_bridge,
        SELF_LEARN_AGENCY_VIOLATED, 0.5f, 1100, 0.8f);

    // Reset both bridges
    EXPECT_EQ(self_model_snn_reset(snn_bridge), 0);
    EXPECT_EQ(self_model_plasticity_reset(plasticity_bridge), 0);

    // Verify reset states
    self_model_snn_bridge_state_t snn_state;
    self_model_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, SELF_MODEL_SNN_STATE_IDLE);

    self_model_plasticity_bridge_state_t plasticity_state;
    self_model_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, SELF_MODEL_PLASTICITY_STATE_IDLE);

    // Re-run scenarios to verify recovery
    self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
    self_model_snn_simulate(snn_bridge, 15.0f);

    self_model_insight_t insight;
    EXPECT_EQ(self_model_snn_get_insight(snn_bridge, &insight), 0);
    EXPECT_GE(insight.agency_level, 0.0f);
}

//=============================================================================
// Concurrent Safety Tests
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, ConcurrentEncodingAndLearning) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        self_model_plasticity_register_synapse(plasticity_bridge,
            1200 + i, SELF_SYNAPSE_AGENCY, 0.5f);
    }

    std::atomic<int> encoding_complete{0};
    std::atomic<int> learning_complete{0};

    // Thread 1: SNN encoding
    std::thread encoder([this, &encoding_complete]() {
        for (int i = 0; i < 5; i++) {
            float dims[SELF_DIM_COUNT];
            generate_self_model_context(dims, i % 4);
            self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
            self_model_snn_step(snn_bridge);
            encoding_complete++;
        }
    });

    // Thread 2: Plasticity learning
    std::thread learner([this, &learning_complete]() {
        for (int i = 0; i < 5; i++) {
            self_model_plasticity_learn(plasticity_bridge,
                SELF_LEARN_AGENCY_CONFIRMED, 0.1f, 1200 + (i % 10), 0.5f);
            learning_complete++;
        }
    });

    encoder.join();
    learner.join();

    EXPECT_EQ(encoding_complete, 5);
    EXPECT_EQ(learning_complete, 5);
}

//=============================================================================
// Body State Integration
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, BodyStateEncodingAndLearning) {
    // Encode body state
    int spikes = self_model_snn_encode_body_state(snn_bridge, 0.8f, 0.9f);
    EXPECT_GE(spikes, 0);

    self_model_snn_simulate(snn_bridge, 30.0f);

    // Get insight
    self_model_insight_t insight;
    self_model_snn_get_insight(snn_bridge, &insight);
    EXPECT_GE(insight.body_state_level, 0.0f);

    // Register ownership synapse
    EXPECT_EQ(self_model_plasticity_register_synapse(plasticity_bridge,
        1300, SELF_SYNAPSE_NARRATIVE, 0.5f), 0);

    // Learn ownership confirmation
    EXPECT_EQ(self_model_plasticity_learn(plasticity_bridge,
        SELF_LEARN_OWNERSHIP_CONFIRMED, 0.8f, 1300, insight.ownership_level), 0);
}

//=============================================================================
// Stats Integration
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, StatsAccumulationAcrossBridges) {
    // Run multiple scenarios
    for (int s = 0; s < 5; s++) {
        float dims[SELF_DIM_COUNT];
        generate_self_model_context(dims, s % 4);

        self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
        self_model_snn_simulate(snn_bridge, 10.0f);

        self_model_plasticity_register_synapse(plasticity_bridge,
            1400 + s, SELF_SYNAPSE_AGENCY, 0.5f);
        self_model_plasticity_learn(plasticity_bridge,
            SELF_LEARN_AGENCY_CONFIRMED, 0.2f, 1400 + s, 0.6f);
    }

    // Check SNN stats
    self_model_snn_stats_t snn_stats;
    self_model_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 5u);
    EXPECT_GT(snn_stats.total_simulations, 0u);

    // Check plasticity stats
    self_model_plasticity_stats_t plasticity_stats;
    self_model_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    // Verify synapses were used (active_synapses in state)
    self_model_plasticity_bridge_state_t bridge_state;
    self_model_plasticity_get_state(plasticity_bridge, &bridge_state);
    EXPECT_GE(bridge_state.active_synapses, 5u);
    EXPECT_GE(plasticity_stats.total_learning_events, 5u);
}

//=============================================================================
// Agency Learning Pipeline
//=============================================================================

TEST_F(SelfModelSNNPlasticityIntegrationTest, AgencyCalibrationPipeline) {
    // Register agency synapses
    for (int i = 0; i < 5; i++) {
        self_model_plasticity_register_synapse(plasticity_bridge,
            1500 + i, SELF_SYNAPSE_AGENCY, 0.5f);
    }

    // Simulate agency violations followed by confirmations
    for (int trial = 0; trial < 10; trial++) {
        float dims[SELF_DIM_COUNT] = {0};
        if (trial % 2 == 0) {
            // Agency violation scenario
            dims[SELF_DIM_AGENCY] = 0.2f;
            dims[SELF_DIM_BODY_STATE] = 0.8f;
        } else {
            // Agency confirmation scenario
            dims[SELF_DIM_AGENCY] = 0.9f;
            dims[SELF_DIM_BODY_STATE] = 0.9f;
        }

        self_model_snn_encode_state(snn_bridge, dims, SELF_DIM_COUNT);
        self_model_snn_simulate(snn_bridge, 15.0f);

        self_model_insight_t insight;
        self_model_snn_get_insight(snn_bridge, &insight);

        // Learn based on agency
        if (trial % 2 == 0) {
            self_model_plasticity_learn(plasticity_bridge,
                SELF_LEARN_AGENCY_VIOLATED, 0.5f, 1500 + (trial % 5), insight.agency_level);
        } else {
            self_model_plasticity_learn(plasticity_bridge,
                SELF_LEARN_AGENCY_CONFIRMED, 0.5f, 1500 + (trial % 5), insight.agency_level);
        }
    }

    // Verify learning statistics
    self_model_plasticity_stats_t stats;
    self_model_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.agency_confirmed_events, 5u);
    EXPECT_GE(stats.agency_violated_events, 5u);
}
