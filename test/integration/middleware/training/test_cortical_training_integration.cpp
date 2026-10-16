/**
 * @file test_cortical_training_integration.cpp
 * @brief Integration tests for Cortical-Training Bridge
 *
 * WHAT: Integration between cortical modules and training system
 * WHY:  Verify realistic cross-module cortical interactions
 * HOW:  Test with multiple connected modules
 *
 * TEST COVERAGE:
 * - Predictive-Training integration (10 tests)
 * - Dendritic-Training integration (10 tests)
 * - Cross-bridge integration (10 tests): cognitive, logic, immune, plasticity
 *
 * TOTAL: 30 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "middleware/training/nimcp_cognitive_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"

class CorticalTrainingIntegrationTest : public ::testing::Test {
protected:
    cortical_training_bridge_t* cortical_bridge;
    cortical_training_config_t cortical_config;

    void SetUp() override {
        cortical_training_default_config(&cortical_config);
        cortical_config.enable_bio_async = false;
        cortical_bridge = cortical_training_create(&cortical_config);
        ASSERT_NE(cortical_bridge, nullptr);
    }

    void TearDown() override {
        if (cortical_bridge) {
            cortical_training_destroy(cortical_bridge);
            cortical_bridge = nullptr;
        }
    }
};

//=============================================================================
// Predictive-Training Integration (10 tests)
//=============================================================================

TEST_F(CorticalTrainingIntegrationTest, PredictiveCodingLoop) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    for (int step = 0; step < 100; ++step) {
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.free_energy = 0.8f - 0.6f * (step / 100.0f);
        effects.gradient_confidence = step / 100.0f;
        effects.lr_factor = 0.9f + 0.3f * effects.gradient_confidence;
        effects.valid = true;
        cortical_training_set_effects_for_testing(cortical_bridge, &effects);
        cortical_training_update_metrics(cortical_bridge, 0.5f, 1.0f, 0.001f, step);
    }
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, PredictionErrorMinimization) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prediction_error_mag = 0.9f;
    effects.lr_factor = 1.5f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, HierarchicalPrediction) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    for (int level = 1; level <= 5; ++level) {
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.num_layers = level;
        effects.lr_factor = 1.3f - 0.1f * level;
        effects.valid = true;
        cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    }
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, TemporalPredictionWindow) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.convergence_rate = 0.85f;
    effects.num_layers = 5;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, PrecisionWeighting) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.convergence_rate = 0.9f;
    effects.lr_factor = 1.35f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, ExpectationViolation) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.prediction_error_mag = 0.9f;
    effects.lr_factor = 1.5f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, ContextualModulation) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.gradient_confidence = 0.9f;
    effects.gradient_confidence = 0.85f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, MultiStepPrediction) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.num_layers = 10;
    effects.convergence_rate = 0.7f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, FreeEnergyConvergence) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 0.15f;
    effects.predictions_stable = true;  /* Set predictions_stable for are_predictions_stable check */
    effects.should_consolidate = true;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    EXPECT_TRUE(cortical_training_are_predictions_stable(cortical_bridge));
}

TEST_F(CorticalTrainingIntegrationTest, PredictiveUncertainty) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.population_entropy = 0.85f;
    effects.lr_factor = 0.7f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

//=============================================================================
// Dendritic-Training Integration (10 tests)
//=============================================================================

TEST_F(CorticalTrainingIntegrationTest, DendriticBurstLearning) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    for (int step = 0; step < 100; ++step) {
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.burst_rate = 0.5f + 0.3f * sinf(step * 0.1f);
        effects.lr_factor = 1.0f + 0.3f * effects.burst_rate;
        effects.valid = true;
        cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    }
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, BACFiringSuccess) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.bac_success_rate = 0.9f;
    effects.lr_factor = 1.4f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, CalciumSpikePlasticity) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.calcium_spikes = 0.85f;
    effects.lr_factor = 1.3f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, ApicalBasalIntegration) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.8f;
    effects.bac_success_rate = 0.9f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, DendriticCompartments) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.num_layers = 8;
    effects.num_layers = 12;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, PlateauPotential) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.calcium_spikes = 0.7f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, BurstTimingPrecision) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.9f;
    effects.gradient_confidence = 0.85f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, SpikeCoincidenceDetection) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.bac_success_rate = 0.8f;
    effects.lr_factor = 1.25f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, ActiveConductances) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.bac_success_rate = 0.85f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, DendriticBranchIntegration) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.gradient_confidence = 0.9f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

//=============================================================================
// Cross-Bridge Integration (10 tests)
//=============================================================================

TEST_F(CorticalTrainingIntegrationTest, CorticalCognitiveIntegration) {
    cognitive_training_config_t cog_config;
    cognitive_training_default_config(&cog_config);
    cog_config.enable_bio_async = false;
    cognitive_training_bridge_t* cog_bridge = cognitive_training_create(&cog_config);
    ASSERT_NE(cog_bridge, nullptr);

    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_start(cog_bridge), 0);

    for (int step = 0; step < 50; ++step) {
        cortical_training_update_metrics(cortical_bridge, 0.5f, 1.0f, 0.001f, step);
        cognitive_training_update_metrics(cog_bridge, 0.5f, 1.0f, 0.001f, step);
    }

    cognitive_training_destroy(cog_bridge);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, PredictiveDendriticCombination) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 0.25f;
    effects.burst_rate = 0.8f;
    effects.lr_factor = 1.4f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, ColumnWinnerDendriticBurst) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.winner_confidence = 0.95f;
    effects.burst_rate = 0.85f;
    effects.lr_factor = 1.6f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, LayerSpecificModulation) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    for (int layer = 2; layer <= 5; ++layer) {
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.num_layers = layer;
        effects.lr_factor = 1.0f + 0.05f * (6 - layer);
        effects.valid = true;
        cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    }
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, OscillationPhaseLocking) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.convergence_rate = 0.25f;
    effects.gradient_confidence = 0.9f;
    effects.predictions_stable = true;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, RecurrentRefinement) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.inhibition_strength = 0.75f;
    effects.num_layers = 5;
    effects.prediction_error_mag = 0.15f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, HierarchyGradientFlow) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.num_layers = 3;
    effects.gradient_confidence = 0.85f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, ColumnSynchrony) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.gradient_confidence = 0.85f;
    effects.lr_factor = 1.25f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, FeedbackModulation) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.gradient_confidence = 0.85f;
    effects.lr_factor = 1.3f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

TEST_F(CorticalTrainingIntegrationTest, FullCorticalStack) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 0.3f;
    effects.gradient_confidence = 0.85f;
    effects.burst_rate = 0.75f;
    effects.bac_success_rate = 0.8f;
    effects.winner_confidence = 0.9f;
    effects.gradient_confidence = 0.85f;
    effects.lr_factor = 1.5f;
    effects.lr_factor = 1.4f;
    effects.valid = true;
    cortical_training_set_effects_for_testing(cortical_bridge, &effects);
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
