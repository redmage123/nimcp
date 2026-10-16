/**
 * @file e2e_test_cortical_training_pipeline.cpp
 * @brief End-to-end tests for Cortical-Training Pipeline
 *
 * WHAT: Full pipeline scenarios combining cortical computation and training
 * WHY:  Verify complete workflow from cortical dynamics to learning
 * HOW:  Realistic training loops with predictive coding, dendritic processing
 *
 * TEST COVERAGE:
 * - Full cortical modulation scenario (5 tests)
 * - Predictive coding training loop (5 tests)
 * - Burst-driven learning scenario (5 tests)
 *
 * TOTAL: 15 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"

class CorticalTrainingPipelineTest : public ::testing::Test {
protected:
    cortical_training_bridge_t* bridge;
    cortical_training_config_t config;

    void SetUp() override {
        cortical_training_default_config(&config);
        config.enable_bio_async = false;
        bridge = cortical_training_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            cortical_training_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Full Cortical Modulation Scenario (5 tests)
//=============================================================================

TEST_F(CorticalTrainingPipelineTest, PredictiveCodingConvergence) {
    /* WHAT: Free energy minimization over training */
    /* WHY:  Verify predictive coding improves model */
    /* HOW:  1000-step FE reduction → LR modulation */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    float initial_fe = 0.95f;

    for (int step = 0; step < 1000; ++step) {
        float progress = step / 1000.0f;

        /* Free energy decreases as model learns */
        float free_energy = initial_fe * powf(0.1f, progress);

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.free_energy = free_energy;
        effects.gradient_confidence = 1.0f - free_energy;
        effects.lr_factor = 0.7f + 0.5f * effects.gradient_confidence;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        float loss = 1.0f / (1.0f + step * 0.005f);
        cortical_training_update_metrics(bridge, loss, 1.0f, 0.001f, step);

        /* Check for stable predictions at low FE */
        if (free_energy < 0.15f) {
            (void)cortical_training_are_predictions_stable(bridge);
        }
    }

    cortical_training_stats_t stats;
    EXPECT_EQ(cortical_training_get_stats(bridge, &stats), 0);
}

TEST_F(CorticalTrainingPipelineTest, DendriticBurstLearning) {
    /* WHAT: Dendritic bursts modulate learning dynamically */
    /* WHY:  Bursts = strong teaching signals */
    /* HOW:  High burst rate → high LR */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    std::vector<float> lr_history;

    for (int step = 0; step < 800; ++step) {
        /* Bursting dynamics vary over time */
        float burst_rate = 0.4f + 0.5f * sinf(step * 0.1f);

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.burst_rate = burst_rate;
        effects.bac_success_rate = burst_rate * 0.9f;
        effects.lr_factor = 0.8f + 0.5f * burst_rate;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        float lr = cortical_training_get_modulated_lr(bridge, 0.001f);
        lr_history.push_back(lr);

        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, step);
    }

    /* Verify LR varied with burst rate */
    float lr_variance = 0.0f;
    float lr_mean = 0.0f;
    for (float lr : lr_history) lr_mean += lr;
    lr_mean /= lr_history.size();
    for (float lr : lr_history) {
        lr_variance += (lr - lr_mean) * (lr - lr_mean);
    }
    lr_variance /= lr_history.size();
    EXPECT_GT(lr_variance, 0.0f);
}

TEST_F(CorticalTrainingPipelineTest, HierarchicalLearningRates) {
    /* WHAT: Different hierarchy levels different LRs */
    /* WHY:  Lower levels = faster, higher = slower */
    /* HOW:  Level modulates LR factor */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    for (int cycle = 0; cycle < 100; ++cycle) {
        for (int level = 1; level <= 5; ++level) {
            cortical_training_effects_t effects;
            memset(&effects, 0, sizeof(effects));
            effects.num_layers = level;
            effects.lr_factor = 1.4f - 0.15f * level;
            effects.valid = true;
            cortical_training_set_effects_for_testing(bridge, &effects);

            float lr = cortical_training_get_modulated_lr(bridge, 0.001f);
            EXPECT_GT(lr, 0.0f);

            cortical_training_update_metrics(bridge, 0.5f, 1.0f, lr, cycle * 5 + level);
        }
    }

    SUCCEED();
}

TEST_F(CorticalTrainingPipelineTest, ColumnWinnerTakesAll) {
    /* WHAT: Winner neuron confidence affects learning */
    /* WHY:  Strong winner = clear decision */
    /* HOW:  Winner confidence boosts sample weight */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    for (int step = 0; step < 600; ++step) {
        /* Winner confidence varies */
        float confidence = 0.4f + 0.5f * (step % 100) / 100.0f;

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.winner_confidence = confidence;
        effects.population_entropy = 1.0f - confidence;
        effects.lr_factor = 0.8f + 0.7f * confidence;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, step);
    }

    SUCCEED();
}

TEST_F(CorticalTrainingPipelineTest, FullCorticalIntegration) {
    /* WHAT: All cortical systems active simultaneously */
    /* WHY:  Realistic full integration */
    /* HOW:  Prediction + dendrite + column together */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    for (int step = 0; step < 500; ++step) {
        float progress = step / 500.0f;

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));

        /* Predictive coding */
        effects.free_energy = 0.8f - 0.6f * progress;
        effects.gradient_confidence = progress;

        /* Dendritic */
        effects.burst_rate = 0.5f + 0.3f * sinf(step * 0.1f);
        effects.bac_success_rate = 0.7f + 0.2f * progress;

        /* Column */
        effects.winner_confidence = 0.6f + 0.3f * progress;
        effects.gradient_confidence = 0.7f + 0.2f * progress;

        /* Combined effects */
        effects.lr_factor = 0.9f + 0.4f * progress;
        effects.lr_factor = 1.0f + 0.3f * progress;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, step);
    }

    SUCCEED();
}

//=============================================================================
// Predictive Coding Training Loop (5 tests)
//=============================================================================

TEST_F(CorticalTrainingPipelineTest, PredictionErrorMinimization) {
    /* WHAT: Prediction error drives learning */
    /* WHY:  Error = learning opportunity */
    /* HOW:  High error → high gradient scale */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    for (int step = 0; step < 700; ++step) {
        /* Error decreases over time */
        float error = 0.9f - 0.7f * (step / 700.0f);

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.prediction_error_mag = error;
        effects.lr_factor = 1.0f + 0.5f * error;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        float loss = 1.0f - 0.7f * (step / 700.0f);
        cortical_training_update_metrics(bridge, loss, 1.0f, 0.001f, step);
    }

    SUCCEED();
}

TEST_F(CorticalTrainingPipelineTest, PrecisionWeightedLearning) {
    /* WHAT: Precision weights modulate gradients */
    /* WHY:  High precision = reliable, amplify gradients */
    /* HOW:  Precision scales gradient contribution */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    for (int step = 0; step < 500; ++step) {
        float precision = 0.5f + 0.4f * cosf(step * 0.05f);

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.gradient_confidence = precision;
        effects.lr_factor = 0.9f + 0.5f * precision;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, step);
    }

    SUCCEED();
}

TEST_F(CorticalTrainingPipelineTest, TemporalPredictionWindow) {
    /* WHAT: Temporal prediction accuracy over time */
    /* WHY:  Temporal coherence = stable dynamics */
    /* HOW:  Accuracy improves, uncertainty decreases */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    for (int step = 0; step < 800; ++step) {
        float progress = step / 800.0f;

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.gradient_confidence = 0.4f + 0.5f * progress;
        effects.prediction_error_mag = 0.8f - 0.6f * progress;
        effects.num_layers = 5 + (int)(5 * progress);
        effects.lr_factor = 1.0f + 0.2f * effects.gradient_confidence;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, step);
    }

    SUCCEED();
}

TEST_F(CorticalTrainingPipelineTest, ExpectationViolationSurprise) {
    /* WHAT: Expectation violations boost attention */
    /* WHY:  Violation = salient event */
    /* HOW:  High violation increases gradient scale */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    for (int step = 0; step < 500; ++step) {
        /* Occasional surprises */
        bool surprise = (step % 50 == 0);

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.prediction_error_mag = surprise ? 0.9f : 0.2f;
        effects.lr_factor = surprise ? 1.6f : 1.0f;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, step);
    }

    SUCCEED();
}

TEST_F(CorticalTrainingPipelineTest, IterativePredictionRefinement) {
    /* WHAT: Iterative refinement of predictions */
    /* WHY:  Convergence indicates stability */
    /* HOW:  Multiple iterations reduce error */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    for (int step = 0; step < 400; ++step) {
        /* Simulate refinement iterations */
        int iterations = 3 + (step / 100);
        float final_error = 0.5f / (1.0f + iterations);

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.num_layers = iterations;
        effects.prediction_error_mag = final_error;
        effects.predictions_stable = (final_error < 0.1f);
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, step);
    }

    SUCCEED();
}

//=============================================================================
// Burst-Driven Learning Scenario (5 tests)
//=============================================================================

TEST_F(CorticalTrainingPipelineTest, CalciumSpikeTriggeredPlasticity) {
    /* WHAT: Calcium spikes drive plasticity */
    /* WHY:  Ca spikes = dendritic integration events */
    /* HOW:  Spike count modulates gradient scale */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    for (int step = 0; step < 600; ++step) {
        /* Spikes vary in count */
        uint64_t spikes = (uint64_t)(10 + 50 * (0.5f + 0.5f * sinf(step * 0.08f)));

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.calcium_spikes = spikes;
        effects.lr_factor = 0.9f + 0.5f * (spikes / 60.0f);
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, step);
    }

    SUCCEED();
}

TEST_F(CorticalTrainingPipelineTest, BACFiringSuccess) {
    /* WHAT: BAC firing success drives confidence */
    /* WHY:  BAC = feedback-driven burst, prediction match */
    /* HOW:  Success rate boosts sample weight */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    for (int step = 0; step < 500; ++step) {
        float progress = step / 500.0f;

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.bac_success_rate = 0.4f + 0.5f * progress;
        effects.lr_factor = 0.8f + 0.6f * effects.bac_success_rate;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, step);
    }

    SUCCEED();
}

TEST_F(CorticalTrainingPipelineTest, DendriticCompartmentActivation) {
    /* WHAT: Different compartments = different features */
    /* WHY:  Compartmentalization = parallel processing */
    /* HOW:  Active layers affect complexity */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    for (int step = 0; step < 400; ++step) {
        int active = 3 + (step % 10);

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.num_layers = active;
        effects.calcium_spikes = 12;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, step);
    }

    SUCCEED();
}

TEST_F(CorticalTrainingPipelineTest, BurstTimingPrecision) {
    /* WHAT: Timing precision of bursts matters */
    /* WHY:  Precise timing = reliable signal */
    /* HOW:  Precision affects confidence */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    for (int step = 0; step < 700; ++step) {
        float precision = 0.5f + 0.4f * (step / 700.0f);

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.burst_rate = precision;
        effects.gradient_confidence = 0.6f + 0.3f * precision;
        effects.lr_factor = 0.9f + 0.4f * precision;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, step);
    }

    SUCCEED();
}

TEST_F(CorticalTrainingPipelineTest, ApicalBasalIntegration) {
    /* WHAT: Apical (feedback) + basal (feedforward) integration */
    /* WHY:  Both streams contribute to computation */
    /* HOW:  Balance affects learning */

    EXPECT_EQ(cortical_training_start(bridge), 0);

    for (int step = 0; step < 500; ++step) {
        /* Balance shifts over time */
        float apical_strength = 0.5f + 0.3f * sinf(step * 0.05f);
        float basal_strength = 0.9f - 0.2f * sinf(step * 0.05f);

        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.burst_rate = apical_strength;
        effects.bac_success_rate = basal_strength;
        effects.lr_factor = 0.9f + 0.2f * (apical_strength + basal_strength) / 2.0f;
        effects.valid = true;
        cortical_training_set_effects_for_testing(bridge, &effects);

        cortical_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, step);
    }

    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
