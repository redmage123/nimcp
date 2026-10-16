/**
 * @file test_curiosity_snn_plasticity_integration.cpp
 * @brief Integration tests for Curiosity SNN-Plasticity bridges
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Integration tests for curiosity SNN + plasticity interaction
 * WHY:  Verify end-to-end curiosity-driven learning via spiking networks
 * HOW:  Test novelty encoding, exploration learning, and protected synapse handling
 *
 * Test Coverage:
 * - Combined SNN encoding and plasticity learning
 * - Exploration/exploitation trade-off learning
 * - Novelty-driven weight updates
 * - Information gain optimization
 * - Protected synapse integrity
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/curiosity/nimcp_curiosity_snn_bridge.h"
#include "cognitive/curiosity/nimcp_curiosity_plasticity_bridge.h"

class CuriositySNNPlasticityIntegration : public ::testing::Test {
protected:
    curiosity_snn_bridge_t* snn_bridge = nullptr;
    curiosity_plasticity_bridge_t* plasticity_bridge = nullptr;

    void SetUp() override {
        // Create SNN bridge
        curiosity_snn_config_t snn_config = curiosity_snn_config_default();
        snn_config.enable_bio_async = false;
        snn_bridge = curiosity_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr);

        // Create Plasticity bridge
        curiosity_plasticity_config_t plasticity_config = curiosity_plasticity_config_default();
        plasticity_config.enable_bio_async = false;
        plasticity_bridge = curiosity_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr);

        // Register base synapses
        for (uint32_t i = 0; i < CURIOSITY_DIM_COUNT; i++) {
            curiosity_plasticity_register_synapse(plasticity_bridge, i,
                CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
        }

        // Register protected synapses
        curiosity_plasticity_register_synapse(plasticity_bridge, 100,
            CURIOSITY_SYNAPSE_EXPLORATION, 1.0f);
        curiosity_plasticity_register_synapse(plasticity_bridge, 101,
            CURIOSITY_SYNAPSE_LEARNING, 0.9f);
    }

    void TearDown() override {
        if (snn_bridge) {
            curiosity_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            curiosity_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Exploration scenarios
    enum ExplorationScenario {
        HIGH_NOVELTY,           // High novelty stimulus
        LOW_NOVELTY,            // Familiar stimulus
        HIGH_INFO_GAIN,         // High information gain expected
        LOW_INFO_GAIN,          // Low information gain
        EXPLORATION_SUCCESS,    // Successful exploration
        EXPLORATION_FAILURE,    // Failed exploration
        INTEREST_MATCH,         // Interest prediction matched
        SURPRISE_EVENT          // Unexpected event
    };

    void generate_scenario(float* dims, ExplorationScenario scenario) {
        memset(dims, 0, sizeof(float) * CURIOSITY_DIM_COUNT);

        switch (scenario) {
            case HIGH_NOVELTY:
                dims[CURIOSITY_DIM_NOVELTY] = 0.9f;
                dims[CURIOSITY_DIM_SURPRISE] = 0.7f;
                dims[CURIOSITY_DIM_EXPLORATION] = 0.8f;
                break;

            case LOW_NOVELTY:
                dims[CURIOSITY_DIM_NOVELTY] = 0.2f;
                dims[CURIOSITY_DIM_SURPRISE] = 0.1f;
                dims[CURIOSITY_DIM_EXPLORATION] = 0.3f;
                break;

            case HIGH_INFO_GAIN:
                dims[CURIOSITY_DIM_INFORMATION_GAIN] = 0.9f;
                dims[CURIOSITY_DIM_INTEREST] = 0.85f;
                dims[CURIOSITY_DIM_SEEKING] = 0.8f;
                break;

            case LOW_INFO_GAIN:
                dims[CURIOSITY_DIM_INFORMATION_GAIN] = 0.2f;
                dims[CURIOSITY_DIM_INTEREST] = 0.3f;
                break;

            case EXPLORATION_SUCCESS:
                dims[CURIOSITY_DIM_EXPLORATION] = 0.8f;
                dims[CURIOSITY_DIM_INFORMATION_GAIN] = 0.7f;
                dims[CURIOSITY_DIM_LEARNING_PROGRESS] = 0.9f;
                break;

            case EXPLORATION_FAILURE:
                dims[CURIOSITY_DIM_EXPLORATION] = 0.3f;
                dims[CURIOSITY_DIM_INFORMATION_GAIN] = 0.1f;
                dims[CURIOSITY_DIM_LEARNING_PROGRESS] = 0.2f;
                break;

            case INTEREST_MATCH:
                dims[CURIOSITY_DIM_INTEREST] = 0.8f;
                dims[CURIOSITY_DIM_NOVELTY] = 0.6f;
                dims[CURIOSITY_DIM_SEEKING] = 0.7f;
                break;

            case SURPRISE_EVENT:
                dims[CURIOSITY_DIM_SURPRISE] = 0.95f;
                dims[CURIOSITY_DIM_NOVELTY] = 0.8f;
                dims[CURIOSITY_DIM_INTEREST] = 0.85f;
                break;
        }
    }

    // Run single evaluation pipeline
    struct EvaluationResult {
        float novelty_level;
        float exploration_drive;
        float information_gain;
        bool novelty_detected;
        int spike_count;
    };

    EvaluationResult run_evaluation(ExplorationScenario scenario) {
        EvaluationResult result = {0};

        float dims[CURIOSITY_DIM_COUNT];
        generate_scenario(dims, scenario);

        // Encode and simulate
        result.spike_count = curiosity_snn_encode_state(snn_bridge, dims, CURIOSITY_DIM_COUNT);
        curiosity_snn_simulate(snn_bridge, 30.0f);

        // Get drive
        curiosity_drive_t drive;
        curiosity_snn_get_drive(snn_bridge, &drive);

        result.novelty_level = drive.novelty_level;
        result.exploration_drive = drive.exploration_drive;
        result.information_gain = drive.information_gain;
        result.novelty_detected = drive.novelty_detected;

        return result;
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityIntegration, BothBridgesInitialized) {
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);
}

TEST_F(CuriositySNNPlasticityIntegration, SingleEvaluationPipeline) {
    auto result = run_evaluation(HIGH_NOVELTY);

    EXPECT_GE(result.novelty_level, 0.0f);
    EXPECT_LE(result.novelty_level, 1.0f);
    EXPECT_GE(result.exploration_drive, 0.0f);
    EXPECT_GE(result.spike_count, 0);

    // Apply learning based on result
    int ret = curiosity_plasticity_learn(plasticity_bridge,
        CURIOSITY_LEARN_NOVELTY_CONFIRMED, 0.5f, 0, result.novelty_level);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Novelty-Driven Learning Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityIntegration, NoveltyLearningIntegration) {
    // Accumulate novelty signals
    int novelty_events = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(HIGH_NOVELTY);

        if (result.novelty_detected || result.novelty_level > 0.5f) {
            novelty_events++;
            curiosity_plasticity_learn(plasticity_bridge,
                CURIOSITY_LEARN_NOVELTY_CONFIRMED, 0.5f, 0, result.novelty_level);
        }
    }

    // Should have some novelty detections
    EXPECT_GT(novelty_events, 0);

    // Check learning occurred
    curiosity_plasticity_stats_t stats;
    curiosity_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.novelty_confirmed_events, 0u);
}

TEST_F(CuriositySNNPlasticityIntegration, FalseNoveltyCorrection) {
    // Register more synapses for this test
    for (int i = 200; i < 210; i++) {
        curiosity_plasticity_register_synapse(plasticity_bridge, i,
            CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    }

    // First report high novelty, then learn it was false
    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(LOW_NOVELTY);

        // Learn false novelty - should decrease weights
        curiosity_plasticity_learn(plasticity_bridge,
            CURIOSITY_LEARN_FALSE_NOVELTY, 0.3f, 200 + trial % 10, 0.8f);
    }

    // Check false novelty events recorded
    curiosity_plasticity_stats_t stats;
    curiosity_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.false_novelty_events, 0u);
}

//=============================================================================
// Information Gain Learning Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityIntegration, InformationGainLearning) {
    for (int i = 300; i < 305; i++) {
        curiosity_plasticity_register_synapse(plasticity_bridge, i,
            CURIOSITY_SYNAPSE_INFORMATION, 0.5f);
    }

    int high_gain_count = 0;

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(HIGH_INFO_GAIN);

        if (result.information_gain > 0.0f) {
            high_gain_count++;
            curiosity_plasticity_learn(plasticity_bridge,
                CURIOSITY_LEARN_INFO_GAIN_HIGH, 0.5f, 300 + trial % 5,
                result.information_gain);
        }
    }

    // Verify learning
    curiosity_plasticity_stats_t stats;
    curiosity_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.high_info_gain_events, 0u);
}

//=============================================================================
// Exploration Learning Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityIntegration, ExplorationSuccessLearning) {
    for (int i = 400; i < 410; i++) {
        curiosity_plasticity_register_synapse(plasticity_bridge, i,
            CURIOSITY_SYNAPSE_INTEREST, 0.5f);
    }

    for (int trial = 0; trial < 10; trial++) {
        auto result = run_evaluation(EXPLORATION_SUCCESS);

        curiosity_plasticity_learn(plasticity_bridge,
            CURIOSITY_LEARN_EXPLORATION_SUCCESS, 0.5f, 400 + trial % 10,
            result.exploration_drive);
    }

    curiosity_plasticity_stats_t stats;
    curiosity_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.exploration_success_events, 0u);
}

//=============================================================================
// Protected Synapse Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityIntegration, ExplorationSynapseProtection) {
    // Get initial protected synapse weight
    curiosity_plasticity_synapse_t exploration_syn;
    curiosity_plasticity_get_synapse(plasticity_bridge, 100, &exploration_syn);
    float original_weight = exploration_syn.weight;
    EXPECT_TRUE(exploration_syn.is_protected);

    // Try to modify via learning
    for (int trial = 0; trial < 20; trial++) {
        auto result = run_evaluation((ExplorationScenario)(trial % 8));

        curiosity_plasticity_learn(plasticity_bridge,
            CURIOSITY_LEARN_FALSE_NOVELTY, 1.0f, 100, 1.0f);
        curiosity_plasticity_apply_stdp(plasticity_bridge, 100,
            (float)trial, (float)trial + 10.0f);
    }

    // Protected synapse should remain unchanged
    curiosity_plasticity_get_synapse(plasticity_bridge, 100, &exploration_syn);
    EXPECT_FLOAT_EQ(exploration_syn.weight, original_weight);
}

TEST_F(CuriositySNNPlasticityIntegration, LearningSynapseProtection) {
    // Learning synapse should also be protected
    curiosity_plasticity_synapse_t learning_syn;
    curiosity_plasticity_get_synapse(plasticity_bridge, 101, &learning_syn);
    float original_weight = learning_syn.weight;
    EXPECT_TRUE(learning_syn.is_protected);

    // Stress test protection
    for (int i = 0; i < 50; i++) {
        curiosity_plasticity_apply_stdp(plasticity_bridge, 101, (float)i, (float)i + 5.0f);
        curiosity_plasticity_learn(plasticity_bridge,
            CURIOSITY_LEARN_EXPLORATION_FAILURE, 1.0f, 101, 0.1f);
    }

    // Weight must remain unchanged
    curiosity_plasticity_get_synapse(plasticity_bridge, 101, &learning_syn);
    EXPECT_FLOAT_EQ(learning_syn.weight, original_weight);
}

//=============================================================================
// Multi-Scenario Integration Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityIntegration, CompleteExplorationWorkflow) {
    for (int i = 500; i < 520; i++) {
        curiosity_plasticity_register_synapse(plasticity_bridge, i,
            CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    }

    // Run complete exploration workflow
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int scenario = 0; scenario < 8; scenario++) {
            auto result = run_evaluation((ExplorationScenario)scenario);

            curiosity_learn_event_t event;
            float magnitude = 0.3f;

            switch ((ExplorationScenario)scenario) {
                case HIGH_NOVELTY:
                    event = CURIOSITY_LEARN_NOVELTY_CONFIRMED;
                    break;
                case LOW_NOVELTY:
                    event = CURIOSITY_LEARN_FALSE_NOVELTY;
                    break;
                case HIGH_INFO_GAIN:
                    event = CURIOSITY_LEARN_INFO_GAIN_HIGH;
                    break;
                case LOW_INFO_GAIN:
                    event = CURIOSITY_LEARN_INFO_GAIN_LOW;
                    break;
                case EXPLORATION_SUCCESS:
                    event = CURIOSITY_LEARN_EXPLORATION_SUCCESS;
                    break;
                case EXPLORATION_FAILURE:
                    event = CURIOSITY_LEARN_EXPLORATION_FAILURE;
                    break;
                case INTEREST_MATCH:
                    event = CURIOSITY_LEARN_INTEREST_MATCHED;
                    break;
                case SURPRISE_EVENT:
                    event = CURIOSITY_LEARN_SURPRISE_POSITIVE;
                    break;
            }

            int synapse_id = 500 + (epoch * 8 + scenario) % 20;
            curiosity_plasticity_learn(plasticity_bridge, event, magnitude,
                synapse_id, result.novelty_level);

            // Apply STDP
            curiosity_plasticity_apply_stdp(plasticity_bridge, synapse_id,
                (float)(epoch * 10 + scenario), (float)(epoch * 10 + scenario + 5));
        }

        // Periodic maintenance
        curiosity_plasticity_update_bcm(plasticity_bridge, 0.5f);
        curiosity_plasticity_homeostatic_update(plasticity_bridge, 0.5f);
        curiosity_plasticity_update_traces(plasticity_bridge, 1.0f);
    }

    // Consolidate learning
    curiosity_plasticity_consolidate(plasticity_bridge);

    // Verify extensive learning occurred
    curiosity_plasticity_stats_t final_stats;
    curiosity_plasticity_get_stats(plasticity_bridge, &final_stats);
    EXPECT_GT(final_stats.total_learning_events, 30u);
    EXPECT_GT(final_stats.weight_updates, 30u);

    curiosity_snn_stats_t snn_stats;
    curiosity_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_evaluations, 40u);
}

//=============================================================================
// State Consistency Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityIntegration, StateConsistencyAcrossEvaluations) {
    for (int i = 0; i < 20; i++) {
        auto result = run_evaluation((ExplorationScenario)(i % 8));

        // State should always be valid
        EXPECT_GE(result.novelty_level, 0.0f);
        EXPECT_LE(result.novelty_level, 1.0f);
        EXPECT_GE(result.exploration_drive, 0.0f);
        EXPECT_LE(result.exploration_drive, 1.0f);
    }

    curiosity_snn_bridge_state_t snn_state;
    curiosity_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, CURIOSITY_SNN_STATE_IDLE);

    curiosity_plasticity_bridge_state_t plasticity_state;
    curiosity_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, CURIOSITY_PLASTICITY_STATE_IDLE);
}

//=============================================================================
// Reset and Recovery Tests
//=============================================================================

TEST_F(CuriositySNNPlasticityIntegration, ResetAndRecovery) {
    // Accumulate some state
    for (int i = 0; i < 10; i++) {
        run_evaluation((ExplorationScenario)(i % 8));
    }

    // Reset both bridges
    curiosity_snn_reset(snn_bridge);
    curiosity_plasticity_reset(plasticity_bridge);

    // Verify recovery
    curiosity_snn_bridge_state_t snn_state;
    curiosity_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, CURIOSITY_SNN_STATE_IDLE);

    curiosity_plasticity_bridge_state_t plasticity_state;
    curiosity_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, CURIOSITY_PLASTICITY_STATE_IDLE);

    // Can continue processing
    auto result = run_evaluation(HIGH_NOVELTY);
    EXPECT_GE(result.novelty_level, 0.0f);
}
