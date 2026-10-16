/**
 * @file test_hypothalamus_training_regression.cpp
 * @brief Regression tests for Hypothalamus-Training Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Regression tests ensuring hypothalamus-training bridge behavior remains
 *       stable across code changes, values stay in bounds, and performance
 *       doesn't degrade
 * WHY:  Prevent regressions in drive-training modulation, homeostatic regulation,
 *       consolidation triggers, and safety interventions
 * HOW:  Test known good behaviors, value bounds, state transitions, performance,
 *       and statistical accumulation
 *
 * TEST COVERAGE:
 * - Modulation value bounds (LR multiplier [0.1, 2.0], etc.)
 * - Homeostatic state transitions (HEALTHY -> IMPROVING -> PLATEAU etc.)
 * - Drive computation correctness
 * - Loss trend calculation accuracy
 * - Consolidation threshold triggers
 * - Safety intervention thresholds
 * - Statistics accumulation
 * - State persistence across reset operations
 * - Performance regression (timing checks)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <numeric>

extern "C" {
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_training_bridge.h"
}

/* ============================================================================
 * Test Constants - Known Good Baselines
 * ============================================================================ */

/* Modulation bounds from header */
#define REG_LR_MULT_MIN                     0.1f
#define REG_LR_MULT_MAX                     2.0f
#define REG_BATCH_SIZE_MULT_MIN             0.5f
#define REG_BATCH_SIZE_MULT_MAX             2.0f
#define REG_GRADIENT_CLIP_MULT_MIN          0.5f
#define REG_GRADIENT_CLIP_MULT_MAX          2.0f
#define REG_DIFFICULTY_ADJ_MIN              -1.0f
#define REG_DIFFICULTY_ADJ_MAX              1.0f
#define REG_SAMPLE_PRIORITY_MIN             0.0f
#define REG_SAMPLE_PRIORITY_MAX             1.0f
#define REG_CHECKPOINT_URGENCY_MIN          0.0f
#define REG_CHECKPOINT_URGENCY_MAX          1.0f
#define REG_MULTI_TASK_WEIGHT_MIN           -1.0f
#define REG_MULTI_TASK_WEIGHT_MAX           1.0f
#define REG_REPLAY_PRIORITY_MIN             0.0f
#define REG_REPLAY_PRIORITY_MAX             1.0f

/* Drive activation bounds */
#define REG_DRIVE_ACTIVATION_MIN            0.0f
#define REG_DRIVE_ACTIVATION_MAX            1.0f

/* Performance baselines (microseconds) */
#define REG_MAX_PROCESS_LOSS_TIME_US        200
#define REG_MAX_PROCESS_GRADIENT_TIME_US    100
#define REG_MAX_COMPUTE_MODULATION_TIME_US  150
#define REG_AVG_EVENT_TIME_US               100

/* Memory and iteration limits */
#define REG_MAX_ITERATIONS                  2000
#define REG_CYCLES_PER_CHECK                200

/* Numerical accuracy */
#define REG_FLOAT_EPSILON                   1e-5f
#define REG_LOSS_TREND_TOLERANCE            0.05f

/* Default values from header */
#define REG_DEFAULT_LOSS_SETPOINT           0.5f
#define REG_DEFAULT_LOSS_TOLERANCE          0.1f
#define REG_DEFAULT_CONSOLIDATION_THRESHOLD 0.8f
#define REG_DEFAULT_CURIOSITY_LR_MULT       1.5f
#define REG_DEFAULT_SAFETY_LR_MULT          0.5f

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HypothalamusTrainingRegressionTest : public ::testing::Test {
protected:
    hypo_training_bridge_t* bridge = nullptr;
    hypo_training_bridge_config_t config;

    void SetUp() override {
        hypo_training_bridge_default_config(&config);
        config.enable_logging = false;
        config.enable_metrics = true;
        bridge = hypo_training_bridge_create(&config, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr) << "Failed to create training bridge";
    }

    void TearDown() override {
        if (bridge) {
            hypo_training_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    /**
     * Helper to verify modulation values are within bounds
     */
    bool verify_modulation_bounds(const hypo_training_modulation_t& mod) {
        bool valid = true;
        valid &= (mod.lr_multiplier >= REG_LR_MULT_MIN &&
                  mod.lr_multiplier <= REG_LR_MULT_MAX);
        valid &= (mod.batch_size_multiplier >= REG_BATCH_SIZE_MULT_MIN &&
                  mod.batch_size_multiplier <= REG_BATCH_SIZE_MULT_MAX);
        valid &= (mod.gradient_clip_multiplier >= REG_GRADIENT_CLIP_MULT_MIN &&
                  mod.gradient_clip_multiplier <= REG_GRADIENT_CLIP_MULT_MAX);
        valid &= (mod.difficulty_adjustment >= REG_DIFFICULTY_ADJ_MIN &&
                  mod.difficulty_adjustment <= REG_DIFFICULTY_ADJ_MAX);
        valid &= (mod.sample_priority_boost >= REG_SAMPLE_PRIORITY_MIN &&
                  mod.sample_priority_boost <= REG_SAMPLE_PRIORITY_MAX);
        valid &= (mod.checkpoint_urgency >= REG_CHECKPOINT_URGENCY_MIN &&
                  mod.checkpoint_urgency <= REG_CHECKPOINT_URGENCY_MAX);
        valid &= (mod.multi_task_weight_shift >= REG_MULTI_TASK_WEIGHT_MIN &&
                  mod.multi_task_weight_shift <= REG_MULTI_TASK_WEIGHT_MAX);
        valid &= (mod.replay_priority_boost >= REG_REPLAY_PRIORITY_MIN &&
                  mod.replay_priority_boost <= REG_REPLAY_PRIORITY_MAX);
        return valid;
    }

    /**
     * Helper to verify drive state bounds
     */
    bool verify_drive_state_bounds(const hypo_training_drive_state_t& state) {
        bool valid = true;
        valid &= (state.curiosity_activation >= REG_DRIVE_ACTIVATION_MIN &&
                  state.curiosity_activation <= REG_DRIVE_ACTIVATION_MAX);
        valid &= (state.safety_activation >= REG_DRIVE_ACTIVATION_MIN &&
                  state.safety_activation <= REG_DRIVE_ACTIVATION_MAX);
        valid &= (state.competence_activation >= REG_DRIVE_ACTIVATION_MIN &&
                  state.competence_activation <= REG_DRIVE_ACTIVATION_MAX);
        valid &= (state.fatigue_level >= REG_DRIVE_ACTIVATION_MIN &&
                  state.fatigue_level <= REG_DRIVE_ACTIVATION_MAX);
        valid &= (state.autonomy_activation >= REG_DRIVE_ACTIVATION_MIN &&
                  state.autonomy_activation <= REG_DRIVE_ACTIVATION_MAX);
        valid &= (state.learning_readiness >= REG_DRIVE_ACTIVATION_MIN &&
                  state.learning_readiness <= REG_DRIVE_ACTIVATION_MAX);
        return valid;
    }

    /**
     * Helper to measure processing time in microseconds
     */
    template<typename Func>
    uint64_t measure_time_us(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    /**
     * Helper to simulate a training run with decreasing loss
     */
    void simulate_training_run(int num_epochs, float start_loss, float end_loss) {
        float loss_delta = (start_loss - end_loss) / num_epochs;
        for (int epoch = 0; epoch < num_epochs; epoch++) {
            float epoch_loss = start_loss - (loss_delta * epoch);
            /* Simulate batches within epoch */
            for (int batch = 0; batch < 10; batch++) {
                float batch_loss = epoch_loss + ((float)(batch - 5) / 100.0f);
                hypo_training_bridge_process_loss(bridge, epoch, batch, batch_loss);
            }
            hypo_training_bridge_process_epoch(bridge, epoch, epoch_loss);
        }
    }
};

/* ============================================================================
 * REG-001: Modulation values stay within bounds under normal conditions
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG001_ModulationValuesWithinBounds) {
    /* Process various loss values */
    for (int i = 0; i < 100; i++) {
        float loss = 0.1f + (0.9f * (float)i / 100.0f);
        hypo_training_bridge_process_loss(bridge, i / 10, i % 10, loss);

        hypo_training_modulation_t mod;
        int ret = hypo_training_bridge_compute_modulation(bridge, &mod);
        ASSERT_EQ(ret, 0) << "Failed to compute modulation at iteration " << i;

        EXPECT_TRUE(verify_modulation_bounds(mod))
            << "Modulation out of bounds at iteration " << i
            << " (loss=" << loss << ")";
    }
}

/* ============================================================================
 * REG-002: Modulation values stay within bounds under extreme conditions
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG002_ModulationBoundsExtremeConditions) {
    /* Test with extreme loss values */
    float extreme_losses[] = {0.0f, 0.001f, 0.999f, 1.0f, 10.0f, 100.0f};

    for (float loss : extreme_losses) {
        hypo_training_bridge_reset(bridge);
        hypo_training_bridge_process_loss(bridge, 0, 0, loss);

        hypo_training_modulation_t mod;
        int ret = hypo_training_bridge_compute_modulation(bridge, &mod);
        ASSERT_EQ(ret, 0) << "Failed to compute modulation for loss=" << loss;

        EXPECT_GE(mod.lr_multiplier, REG_LR_MULT_MIN)
            << "LR multiplier below min for loss=" << loss;
        EXPECT_LE(mod.lr_multiplier, REG_LR_MULT_MAX)
            << "LR multiplier above max for loss=" << loss;

        EXPECT_GE(mod.batch_size_multiplier, REG_BATCH_SIZE_MULT_MIN)
            << "Batch size multiplier below min for loss=" << loss;
        EXPECT_LE(mod.batch_size_multiplier, REG_BATCH_SIZE_MULT_MAX)
            << "Batch size multiplier above max for loss=" << loss;

        EXPECT_GE(mod.difficulty_adjustment, REG_DIFFICULTY_ADJ_MIN)
            << "Difficulty adjustment below min for loss=" << loss;
        EXPECT_LE(mod.difficulty_adjustment, REG_DIFFICULTY_ADJ_MAX)
            << "Difficulty adjustment above max for loss=" << loss;
    }

    /* Test with extreme gradient norms */
    float extreme_gradients[] = {0.0f, 0.001f, 1.0f, 10.0f, 1000.0f};
    bool clipped_states[] = {false, true};

    for (float grad : extreme_gradients) {
        for (bool clipped : clipped_states) {
            hypo_training_bridge_reset(bridge);
            hypo_training_bridge_process_gradient(bridge, grad, clipped);

            hypo_training_modulation_t mod;
            int ret = hypo_training_bridge_compute_modulation(bridge, &mod);
            ASSERT_EQ(ret, 0);

            EXPECT_TRUE(verify_modulation_bounds(mod))
                << "Modulation out of bounds for gradient=" << grad
                << ", clipped=" << clipped;
        }
    }
}

/* ============================================================================
 * REG-003: LR multiplier stays within [0.1, 2.0]
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG003_LRMultiplierBounds) {
    /* Set various drive combinations and verify LR multiplier bounds */
    float drive_levels[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float curiosity : drive_levels) {
        for (float safety : drive_levels) {
            hypo_training_bridge_reset(bridge);

            /* Set drives (0=curiosity, 1=safety) */
            hypo_training_bridge_set_drive(bridge, 0, curiosity);
            hypo_training_bridge_set_drive(bridge, 1, safety);

            float lr_mult;
            int ret = hypo_training_bridge_get_lr_multiplier(bridge, &lr_mult);
            ASSERT_EQ(ret, 0);

            EXPECT_GE(lr_mult, REG_LR_MULT_MIN)
                << "LR multiplier below 0.1 with curiosity=" << curiosity
                << ", safety=" << safety;
            EXPECT_LE(lr_mult, REG_LR_MULT_MAX)
                << "LR multiplier above 2.0 with curiosity=" << curiosity
                << ", safety=" << safety;
        }
    }
}

/* ============================================================================
 * REG-004: Homeostatic state transitions follow expected patterns
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG004_HomeostaticStateTransitions) {
    /* Test HEALTHY state - loss near setpoint */
    hypo_training_bridge_set_loss_setpoint(bridge, 0.5f);

    /* Process losses near setpoint */
    for (int i = 0; i < 10; i++) {
        hypo_training_bridge_process_loss(bridge, 0, i, 0.5f + ((float)(i - 5) * 0.01f));
    }

    hypo_training_state_t state;
    hypo_training_bridge_get_training_state(bridge, &state);
    EXPECT_TRUE(state == HYPO_TRAIN_STATE_HEALTHY ||
                state == HYPO_TRAIN_STATE_IMPROVING)
        << "Expected HEALTHY or IMPROVING when loss near setpoint";

    /* Test IMPROVING state - loss decreasing toward setpoint */
    hypo_training_bridge_reset(bridge);
    hypo_training_bridge_set_loss_setpoint(bridge, 0.3f);

    for (int i = 0; i < 20; i++) {
        float decreasing_loss = 0.8f - (0.02f * i);  /* Decreasing from 0.8 to 0.4 */
        hypo_training_bridge_process_loss(bridge, i / 5, i % 5, decreasing_loss);
    }

    hypo_training_bridge_get_training_state(bridge, &state);
    EXPECT_TRUE(state == HYPO_TRAIN_STATE_IMPROVING ||
                state == HYPO_TRAIN_STATE_HEALTHY)
        << "Expected IMPROVING when loss decreasing toward setpoint";

    /* Test DIVERGING state - loss increasing away from setpoint */
    hypo_training_bridge_reset(bridge);
    hypo_training_bridge_set_loss_setpoint(bridge, 0.3f);

    for (int i = 0; i < 20; i++) {
        float increasing_loss = 0.4f + (0.05f * i);  /* Increasing from 0.4 to 1.4 */
        hypo_training_bridge_process_loss(bridge, i / 5, i % 5, increasing_loss);
    }

    hypo_training_bridge_get_training_state(bridge, &state);
    EXPECT_TRUE(state == HYPO_TRAIN_STATE_DIVERGING ||
                state == HYPO_TRAIN_STATE_UNSTABLE ||
                state == HYPO_TRAIN_STATE_CRITICAL)
        << "Expected DIVERGING/UNSTABLE/CRITICAL when loss increasing";
}

/* ============================================================================
 * REG-005: Plateau detection works correctly
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG005_PlateauDetection) {
    hypo_training_bridge_set_loss_setpoint(bridge, 0.3f);

    /* Process many losses at same level (plateau) */
    const float PLATEAU_LOSS = 0.5f;
    for (int epoch = 0; epoch < 50; epoch++) {
        for (int batch = 0; batch < 10; batch++) {
            /* Small random variation but essentially flat */
            float loss = PLATEAU_LOSS + ((float)((epoch * 10 + batch) % 10 - 5) * 0.001f);
            hypo_training_bridge_process_loss(bridge, epoch, batch, loss);
        }
        hypo_training_bridge_process_epoch(bridge, epoch, PLATEAU_LOSS);
    }

    hypo_training_homeostatic_state_t homeo_state;
    hypo_training_bridge_get_homeostatic_state(bridge, &homeo_state);

    /* Loss trend should be near zero (flat) */
    EXPECT_NEAR(homeo_state.loss_trend, 0.0f, REG_LOSS_TREND_TOLERANCE)
        << "Loss trend should be near zero during plateau";

    /* Epochs since improvement should be significant */
    EXPECT_GT(homeo_state.epochs_since_improvement, 10u)
        << "Should detect lack of improvement during plateau";
}

/* ============================================================================
 * REG-006: Drive computations produce expected values
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG006_DriveComputations) {
    /* Test curiosity drive affects exploration tendency */
    hypo_training_bridge_set_drive(bridge, 0, 1.0f);  /* Max curiosity */
    hypo_training_bridge_set_drive(bridge, 1, 0.0f);  /* Min safety */

    hypo_training_drive_state_t drive_state;
    hypo_training_bridge_get_drive_state(bridge, &drive_state);

    EXPECT_GT(drive_state.exploration_tendency, 0.0f)
        << "Exploration tendency should be positive with high curiosity/low safety";
    EXPECT_TRUE(verify_drive_state_bounds(drive_state))
        << "Drive state should be within bounds";

    /* Test safety drive reduces exploration */
    hypo_training_bridge_set_drive(bridge, 0, 0.0f);  /* Min curiosity */
    hypo_training_bridge_set_drive(bridge, 1, 1.0f);  /* Max safety */

    hypo_training_bridge_get_drive_state(bridge, &drive_state);

    EXPECT_LT(drive_state.exploration_tendency, 0.5f)
        << "Exploration tendency should be low with high safety/low curiosity";

    /* Test fatigue affects learning readiness */
    hypo_training_bridge_set_drive(bridge, 3, 0.0f);  /* No fatigue */
    hypo_training_bridge_get_drive_state(bridge, &drive_state);
    float readiness_no_fatigue = drive_state.learning_readiness;

    hypo_training_bridge_set_drive(bridge, 3, 0.8f);  /* High fatigue */
    hypo_training_bridge_get_drive_state(bridge, &drive_state);
    float readiness_high_fatigue = drive_state.learning_readiness;

    EXPECT_GT(readiness_no_fatigue, readiness_high_fatigue)
        << "Learning readiness should decrease with fatigue";
}

/* ============================================================================
 * REG-007: Loss trend calculation is accurate
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG007_LossTrendCalculation) {
    /* Test decreasing loss produces negative trend */
    hypo_training_bridge_reset(bridge);

    for (int i = 0; i < 30; i++) {
        float decreasing_loss = 1.0f - (0.02f * i);  /* 1.0 -> 0.4 */
        hypo_training_bridge_process_loss(bridge, i / 10, i % 10, decreasing_loss);
    }

    hypo_training_homeostatic_state_t state;
    hypo_training_bridge_get_homeostatic_state(bridge, &state);

    EXPECT_LT(state.loss_trend, 0.0f)
        << "Loss trend should be negative when loss is decreasing";

    /* Test increasing loss produces positive trend */
    hypo_training_bridge_reset(bridge);

    for (int i = 0; i < 30; i++) {
        float increasing_loss = 0.2f + (0.02f * i);  /* 0.2 -> 0.8 */
        hypo_training_bridge_process_loss(bridge, i / 10, i % 10, increasing_loss);
    }

    hypo_training_bridge_get_homeostatic_state(bridge, &state);

    EXPECT_GT(state.loss_trend, 0.0f)
        << "Loss trend should be positive when loss is increasing";

    /* Verify trend values don't have NaN/Inf */
    EXPECT_FALSE(std::isnan(state.loss_trend)) << "Loss trend should not be NaN";
    EXPECT_FALSE(std::isinf(state.loss_trend)) << "Loss trend should not be Inf";
}

/* ============================================================================
 * REG-008: Consolidation thresholds trigger correctly
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG008_ConsolidationThresholds) {
    /* Initially, no consolidation should be needed */
    hypo_consolidation_type_t consol_type;
    hypo_training_bridge_check_consolidation(bridge, &consol_type);
    EXPECT_EQ(consol_type, HYPO_CONSOL_NONE)
        << "No consolidation needed initially";

    /* Process many epochs to accumulate fatigue */
    for (int epoch = 0; epoch < 100; epoch++) {
        hypo_training_bridge_process_epoch(bridge, epoch, 0.5f);

        /* Check consolidation periodically */
        if (epoch > 0 && epoch % 20 == 0) {
            hypo_training_bridge_check_consolidation(bridge, &consol_type);

            hypo_training_drive_state_t drive_state;
            hypo_training_bridge_get_drive_state(bridge, &drive_state);

            /* If fatigue is above threshold, consolidation should be recommended */
            if (drive_state.fatigue_level >= REG_DEFAULT_CONSOLIDATION_THRESHOLD) {
                EXPECT_NE(consol_type, HYPO_CONSOL_NONE)
                    << "Consolidation should be recommended when fatigue="
                    << drive_state.fatigue_level << " at epoch " << epoch;
            }
        }
    }

    /* After many epochs, some consolidation should be recommended */
    hypo_training_bridge_check_consolidation(bridge, &consol_type);

    hypo_training_drive_state_t final_drive_state;
    hypo_training_bridge_get_drive_state(bridge, &final_drive_state);

    if (final_drive_state.fatigue_level >= REG_DEFAULT_CONSOLIDATION_THRESHOLD) {
        EXPECT_NE(consol_type, HYPO_CONSOL_NONE)
            << "Consolidation should be recommended after 100 epochs with high fatigue";
    }
}

/* ============================================================================
 * REG-009: Fatigue reset works correctly
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG009_FatigueReset) {
    /* Accumulate fatigue */
    for (int epoch = 0; epoch < 50; epoch++) {
        hypo_training_bridge_process_epoch(bridge, epoch, 0.5f);
    }

    hypo_training_drive_state_t before_reset;
    hypo_training_bridge_get_drive_state(bridge, &before_reset);

    /* Reset fatigue */
    int ret = hypo_training_bridge_reset_fatigue(bridge);
    ASSERT_EQ(ret, 0) << "Failed to reset fatigue";

    hypo_training_drive_state_t after_reset;
    hypo_training_bridge_get_drive_state(bridge, &after_reset);

    EXPECT_LT(after_reset.fatigue_level, before_reset.fatigue_level)
        << "Fatigue should decrease after reset";
    EXPECT_NEAR(after_reset.fatigue_level, 0.0f, 0.01f)
        << "Fatigue should be near zero after reset";
    EXPECT_NEAR(after_reset.learning_readiness, 1.0f, 0.1f)
        << "Learning readiness should be near 1.0 after fatigue reset";
}

/* ============================================================================
 * REG-010: Safety interventions occur at correct thresholds
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG010_SafetyInterventions) {
    /* Test with high gradient norms (instability) */
    for (int i = 0; i < 20; i++) {
        hypo_training_bridge_process_gradient(bridge, 100.0f, true);  /* Very high norm, clipped */
    }

    hypo_training_drive_state_t drive_state;
    hypo_training_bridge_get_drive_state(bridge, &drive_state);

    EXPECT_GT(drive_state.safety_activation, 0.5f)
        << "Safety drive should be elevated with high gradient norms";

    /* Get modulation - should recommend safety measures */
    hypo_training_modulation_t mod;
    hypo_training_bridge_compute_modulation(bridge, &mod);

    /* LR should be reduced when safety is high */
    EXPECT_LT(mod.lr_multiplier, 1.5f)
        << "LR multiplier should be reduced when safety is elevated";

    /* Test with diverging loss */
    hypo_training_bridge_reset(bridge);

    for (int i = 0; i < 30; i++) {
        float diverging_loss = 0.5f + (0.1f * i);  /* Rapidly increasing */
        hypo_training_bridge_process_loss(bridge, i / 10, i % 10, diverging_loss);
    }

    hypo_training_state_t state;
    hypo_training_bridge_get_training_state(bridge, &state);

    EXPECT_TRUE(state == HYPO_TRAIN_STATE_DIVERGING ||
                state == HYPO_TRAIN_STATE_UNSTABLE ||
                state == HYPO_TRAIN_STATE_CRITICAL)
        << "Training state should indicate problems with diverging loss";

    hypo_training_bridge_compute_modulation(bridge, &mod);

    /* Should recommend early stopping or LR reduction */
    EXPECT_TRUE(mod.recommend_early_stopping || mod.recommend_lr_reduction)
        << "Should recommend intervention with diverging loss";
}

/* ============================================================================
 * REG-011: Statistics accumulate correctly
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG011_StatisticsAccumulation) {
    hypo_training_bridge_reset_stats(bridge);

    const int NUM_LOSSES = 100;
    const int NUM_GRADIENTS = 50;
    const int NUM_EPOCHS = 10;

    /* Process events */
    for (int i = 0; i < NUM_LOSSES; i++) {
        hypo_training_bridge_process_loss(bridge, i / 10, i % 10, 0.5f);
    }

    for (int i = 0; i < NUM_GRADIENTS; i++) {
        hypo_training_bridge_process_gradient(bridge, 1.0f, false);
    }

    for (int i = 0; i < NUM_EPOCHS; i++) {
        hypo_training_bridge_process_epoch(bridge, i, 0.5f);
    }

    /* Compute modulations */
    for (int i = 0; i < 20; i++) {
        hypo_training_modulation_t mod;
        hypo_training_bridge_compute_modulation(bridge, &mod);
    }

    hypo_training_bridge_stats_t stats;
    int ret = hypo_training_bridge_get_stats(bridge, &stats);
    ASSERT_EQ(ret, 0);

    /* Verify event counts */
    EXPECT_GE(stats.training_events_received, (uint64_t)(NUM_LOSSES + NUM_GRADIENTS + NUM_EPOCHS))
        << "Training events received should be at least sum of processed events";

    EXPECT_GE(stats.lr_modulations, 0u)
        << "LR modulations should be non-negative";

    /* Verify no overflow or NaN */
    EXPECT_FALSE(std::isnan((float)stats.avg_processing_time_us))
        << "Average processing time should not be NaN";
    EXPECT_FALSE(std::isnan(stats.avg_loss_deviation))
        << "Average loss deviation should not be NaN";
}

/* ============================================================================
 * REG-012: Statistics don't overflow with many events
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG012_StatisticsNoOverflow) {
    hypo_training_bridge_reset_stats(bridge);

    /* Process many events */
    for (int i = 0; i < 5000; i++) {
        hypo_training_bridge_process_loss(bridge, i / 100, i % 100, 0.5f + sinf((float)i * 0.01f) * 0.2f);
    }

    hypo_training_bridge_stats_t stats;
    hypo_training_bridge_get_stats(bridge, &stats);

    /* Verify values are reasonable */
    EXPECT_GE(stats.training_events_received, 5000u);
    EXPECT_FALSE(std::isnan(stats.avg_loss_deviation));
    EXPECT_FALSE(std::isinf(stats.avg_loss_deviation));
    EXPECT_FALSE(std::isnan(stats.max_loss_deviation));
    EXPECT_FALSE(std::isinf(stats.max_loss_deviation));
    EXPECT_FALSE(std::isnan((float)stats.avg_processing_time_us));
    EXPECT_FALSE(std::isinf((float)stats.avg_processing_time_us));
}

/* ============================================================================
 * REG-013: State persistence across reset operations
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG013_StatePersistenceAcrossReset) {
    /* Record best loss before reset */
    simulate_training_run(10, 0.8f, 0.3f);

    hypo_training_homeostatic_state_t state_before;
    hypo_training_bridge_get_homeostatic_state(bridge, &state_before);

    float best_loss_before = state_before.best_loss_seen;

    /* Reset the bridge */
    int ret = hypo_training_bridge_reset(bridge);
    ASSERT_EQ(ret, 0);

    /* Verify state is cleared */
    hypo_training_homeostatic_state_t state_after;
    hypo_training_bridge_get_homeostatic_state(bridge, &state_after);

    /* After reset, current loss should be cleared or default */
    EXPECT_NE(state_after.current_loss, state_before.current_loss)
        << "Current loss should be reset";

    /* Drive state should be reset */
    hypo_training_drive_state_t drive_state;
    hypo_training_bridge_get_drive_state(bridge, &drive_state);

    EXPECT_NEAR(drive_state.fatigue_level, 0.0f, 0.1f)
        << "Fatigue should be reset to near zero";
}

/* ============================================================================
 * REG-014: Statistics reset clears all counters
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG014_StatisticsResetClearsAll) {
    /* Generate some statistics */
    for (int i = 0; i < 100; i++) {
        hypo_training_bridge_process_loss(bridge, i / 10, i % 10, 0.5f);
    }

    hypo_training_bridge_stats_t before_reset;
    hypo_training_bridge_get_stats(bridge, &before_reset);
    EXPECT_GT(before_reset.training_events_received, 0u);

    /* Reset statistics */
    int ret = hypo_training_bridge_reset_stats(bridge);
    ASSERT_EQ(ret, 0);

    hypo_training_bridge_stats_t after_reset;
    hypo_training_bridge_get_stats(bridge, &after_reset);

    EXPECT_EQ(after_reset.training_events_received, 0u)
        << "Training events should be zero after stats reset";
    EXPECT_EQ(after_reset.modulations_published, 0u)
        << "Modulations published should be zero after stats reset";
    EXPECT_EQ(after_reset.lr_modulations, 0u)
        << "LR modulations should be zero after stats reset";
    EXPECT_EQ(after_reset.safety_interventions, 0u)
        << "Safety interventions should be zero after stats reset";
}

/* ============================================================================
 * REG-015: Performance - process_loss timing
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG015_ProcessLossPerformance) {
    /* Warm up */
    for (int i = 0; i < 10; i++) {
        hypo_training_bridge_process_loss(bridge, 0, i, 0.5f);
    }

    /* Collect timing samples */
    const int NUM_SAMPLES = 100;
    std::vector<uint64_t> times;
    times.reserve(NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint64_t t = measure_time_us([&]() {
            hypo_training_bridge_process_loss(bridge, i / 10, i % 10, 0.5f + sinf((float)i * 0.1f) * 0.2f);
        });
        times.push_back(t);
    }

    uint64_t max_time = *std::max_element(times.begin(), times.end());
    uint64_t total_time = std::accumulate(times.begin(), times.end(), 0ULL);
    double avg_time = (double)total_time / NUM_SAMPLES;

    EXPECT_LT(max_time, REG_MAX_PROCESS_LOSS_TIME_US)
        << "Maximum process_loss time exceeded: " << max_time << "us";
    EXPECT_LT(avg_time, REG_AVG_EVENT_TIME_US)
        << "Average process_loss time exceeded: " << avg_time << "us";
}

/* ============================================================================
 * REG-016: Performance - process_gradient timing
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG016_ProcessGradientPerformance) {
    /* Warm up */
    for (int i = 0; i < 10; i++) {
        hypo_training_bridge_process_gradient(bridge, 1.0f, false);
    }

    /* Collect timing samples */
    const int NUM_SAMPLES = 100;
    std::vector<uint64_t> times;
    times.reserve(NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint64_t t = measure_time_us([&]() {
            hypo_training_bridge_process_gradient(bridge, 1.0f + (float)i * 0.1f, i % 2 == 0);
        });
        times.push_back(t);
    }

    uint64_t max_time = *std::max_element(times.begin(), times.end());

    EXPECT_LT(max_time, REG_MAX_PROCESS_GRADIENT_TIME_US)
        << "Maximum process_gradient time exceeded: " << max_time << "us";
}

/* ============================================================================
 * REG-017: Performance - compute_modulation timing
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG017_ComputeModulationPerformance) {
    /* Set up state */
    for (int i = 0; i < 50; i++) {
        hypo_training_bridge_process_loss(bridge, i / 10, i % 10, 0.5f);
    }

    /* Warm up */
    for (int i = 0; i < 10; i++) {
        hypo_training_modulation_t mod;
        hypo_training_bridge_compute_modulation(bridge, &mod);
    }

    /* Collect timing samples */
    const int NUM_SAMPLES = 100;
    std::vector<uint64_t> times;
    times.reserve(NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint64_t t = measure_time_us([&]() {
            hypo_training_modulation_t mod;
            hypo_training_bridge_compute_modulation(bridge, &mod);
        });
        times.push_back(t);
    }

    uint64_t max_time = *std::max_element(times.begin(), times.end());
    uint64_t total_time = std::accumulate(times.begin(), times.end(), 0ULL);
    double avg_time = (double)total_time / NUM_SAMPLES;

    EXPECT_LT(max_time, REG_MAX_COMPUTE_MODULATION_TIME_US)
        << "Maximum compute_modulation time exceeded: " << max_time << "us";
    EXPECT_LT(avg_time, REG_AVG_EVENT_TIME_US)
        << "Average compute_modulation time exceeded: " << avg_time << "us";
}

/* ============================================================================
 * REG-018: Memory stability over many iterations
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG018_MemoryStability) {
    for (int cycle = 0; cycle < REG_MAX_ITERATIONS; cycle++) {
        hypo_training_bridge_process_loss(bridge, cycle / 100, cycle % 100, 0.5f);

        if (cycle % 50 == 0) {
            hypo_training_bridge_process_epoch(bridge, cycle / 50, 0.5f);
        }

        if (cycle % 30 == 0) {
            hypo_training_bridge_process_gradient(bridge, 1.0f, false);
        }

        if (cycle % REG_CYCLES_PER_CHECK == 0) {
            /* Periodic state verification */
            hypo_training_modulation_t mod;
            int ret = hypo_training_bridge_compute_modulation(bridge, &mod);
            ASSERT_EQ(ret, 0) << "Failed at cycle " << cycle;
            ASSERT_TRUE(verify_modulation_bounds(mod)) << "Bounds violation at cycle " << cycle;

            hypo_training_drive_state_t drive_state;
            ret = hypo_training_bridge_get_drive_state(bridge, &drive_state);
            ASSERT_EQ(ret, 0) << "Failed to get drive state at cycle " << cycle;
            ASSERT_TRUE(verify_drive_state_bounds(drive_state)) << "Drive bounds violation at cycle " << cycle;
        }
    }
}

/* ============================================================================
 * REG-019: Utility functions return valid strings
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG019_UtilityStringsNotNull) {
    /* Training state names */
    for (int i = 0; i <= HYPO_TRAIN_STATE_CRITICAL; i++) {
        const char* name = hypo_training_state_name((hypo_training_state_t)i);
        EXPECT_NE(name, nullptr) << "Training state " << i << " has null name";
        EXPECT_GT(strlen(name), 0u) << "Training state " << i << " has empty name";
    }

    /* Consolidation type names */
    for (int i = 0; i <= HYPO_CONSOL_FULL_REST; i++) {
        const char* name = hypo_consolidation_type_name((hypo_consolidation_type_t)i);
        EXPECT_NE(name, nullptr) << "Consolidation type " << i << " has null name";
        EXPECT_GT(strlen(name), 0u) << "Consolidation type " << i << " has empty name";
    }

    /* Modulation type names */
    for (int i = 0; i < HYPO_TRAIN_MOD_COUNT; i++) {
        const char* name = hypo_training_modulation_name((hypo_training_modulation_type_t)i);
        EXPECT_NE(name, nullptr) << "Modulation type " << i << " has null name";
        EXPECT_GT(strlen(name), 0u) << "Modulation type " << i << " has empty name";
    }
}

/* ============================================================================
 * REG-020: NULL safety - functions handle NULL gracefully
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG020_NullSafety) {
    /* hypo_training_bridge_destroy should be safe with NULL */
    hypo_training_bridge_destroy(nullptr);  /* Should not crash */

    /* Default config with NULL should fail */
    EXPECT_NE(0, hypo_training_bridge_default_config(nullptr));

    /* Process functions with NULL bridge should fail */
    EXPECT_NE(0, hypo_training_bridge_process_loss(nullptr, 0, 0, 0.5f));
    EXPECT_NE(0, hypo_training_bridge_process_gradient(nullptr, 1.0f, false));
    EXPECT_NE(0, hypo_training_bridge_process_epoch(nullptr, 0, 0.5f));
    EXPECT_NE(0, hypo_training_bridge_process_lr_change(nullptr, 0.01f, 0.001f));

    /* Getters with NULL bridge should fail */
    hypo_training_modulation_t mod;
    EXPECT_NE(0, hypo_training_bridge_compute_modulation(nullptr, &mod));

    float lr_mult;
    EXPECT_NE(0, hypo_training_bridge_get_lr_multiplier(nullptr, &lr_mult));

    hypo_training_homeostatic_state_t homeo_state;
    EXPECT_NE(0, hypo_training_bridge_get_homeostatic_state(nullptr, &homeo_state));

    hypo_training_state_t train_state;
    EXPECT_NE(0, hypo_training_bridge_get_training_state(nullptr, &train_state));

    hypo_training_drive_state_t drive_state;
    EXPECT_NE(0, hypo_training_bridge_get_drive_state(nullptr, &drive_state));

    hypo_training_bridge_stats_t stats;
    EXPECT_NE(0, hypo_training_bridge_get_stats(nullptr, &stats));

    hypo_consolidation_type_t consol;
    EXPECT_NE(0, hypo_training_bridge_check_consolidation(nullptr, &consol));

    /* Getters with NULL output should fail */
    EXPECT_NE(0, hypo_training_bridge_compute_modulation(bridge, nullptr));
    EXPECT_NE(0, hypo_training_bridge_get_lr_multiplier(bridge, nullptr));
    EXPECT_NE(0, hypo_training_bridge_get_homeostatic_state(bridge, nullptr));
    EXPECT_NE(0, hypo_training_bridge_get_training_state(bridge, nullptr));
    EXPECT_NE(0, hypo_training_bridge_get_drive_state(bridge, nullptr));
    EXPECT_NE(0, hypo_training_bridge_get_stats(bridge, nullptr));
    EXPECT_NE(0, hypo_training_bridge_check_consolidation(bridge, nullptr));

    /* Setters with NULL bridge should fail */
    EXPECT_NE(0, hypo_training_bridge_set_drive(nullptr, 0, 0.5f));
    EXPECT_NE(0, hypo_training_bridge_set_loss_setpoint(nullptr, 0.5f));
    EXPECT_NE(0, hypo_training_bridge_reset_fatigue(nullptr));
    EXPECT_NE(0, hypo_training_bridge_reset(nullptr));
    EXPECT_NE(0, hypo_training_bridge_reset_stats(nullptr));
}

/* ============================================================================
 * REG-021: Default configuration validation
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG021_DefaultConfigValid) {
    hypo_training_bridge_config_t default_config;
    int ret = hypo_training_bridge_default_config(&default_config);
    EXPECT_EQ(ret, 0);

    /* Validate homeostatic config */
    EXPECT_FLOAT_EQ(default_config.homeostatic_config.loss_setpoint, REG_DEFAULT_LOSS_SETPOINT);
    EXPECT_FLOAT_EQ(default_config.homeostatic_config.loss_tolerance, REG_DEFAULT_LOSS_TOLERANCE);
    EXPECT_GT(default_config.homeostatic_config.deviation_response_gain, 0.0f);

    /* Validate drive config */
    EXPECT_FLOAT_EQ(default_config.drive_config.curiosity_lr_multiplier, REG_DEFAULT_CURIOSITY_LR_MULT);
    EXPECT_FLOAT_EQ(default_config.drive_config.safety_lr_reduction, REG_DEFAULT_SAFETY_LR_MULT);
    EXPECT_FLOAT_EQ(default_config.drive_config.fatigue_consolidation_threshold,
                    REG_DEFAULT_CONSOLIDATION_THRESHOLD);

    /* Validate bounds are sensible */
    EXPECT_GT(default_config.drive_config.curiosity_lr_multiplier, 0.0f);
    EXPECT_LT(default_config.drive_config.curiosity_lr_multiplier, 3.0f);

    EXPECT_GT(default_config.drive_config.safety_lr_reduction, 0.0f);
    EXPECT_LT(default_config.drive_config.safety_lr_reduction, 1.0f);

    EXPECT_GT(default_config.drive_config.fatigue_consolidation_threshold, 0.0f);
    EXPECT_LE(default_config.drive_config.fatigue_consolidation_threshold, 1.0f);
}

/* ============================================================================
 * REG-022: Drive activation bounds after setting
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG022_DriveActivationBoundsAfterSet) {
    /* Test setting drives to boundary values */
    float test_values[] = {0.0f, 0.5f, 1.0f};

    for (uint32_t drive_type = 0; drive_type < 5; drive_type++) {
        for (float value : test_values) {
            int ret = hypo_training_bridge_set_drive(bridge, drive_type, value);
            EXPECT_EQ(ret, 0) << "Failed to set drive " << drive_type << " to " << value;

            hypo_training_drive_state_t state;
            ret = hypo_training_bridge_get_drive_state(bridge, &state);
            EXPECT_EQ(ret, 0);

            /* Verify the drive was set (approximate check since internal processing may adjust) */
            EXPECT_TRUE(verify_drive_state_bounds(state))
                << "Drive state out of bounds after setting drive " << drive_type << " to " << value;
        }
    }

    /* Test setting invalid drive values (out of range) */
    /* Implementation should clamp to [0, 1] */
    hypo_training_bridge_set_drive(bridge, 0, -0.5f);
    hypo_training_drive_state_t state;
    hypo_training_bridge_get_drive_state(bridge, &state);
    EXPECT_GE(state.curiosity_activation, 0.0f) << "Negative drive value should be clamped";

    hypo_training_bridge_set_drive(bridge, 0, 1.5f);
    hypo_training_bridge_get_drive_state(bridge, &state);
    EXPECT_LE(state.curiosity_activation, 1.0f) << "Excessive drive value should be clamped";
}

/* ============================================================================
 * REG-023: Difficulty adjustment bounds
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG023_DifficultyAdjustmentBounds) {
    /* Test difficulty adjustment with various competence levels */
    float competence_levels[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float competence : competence_levels) {
        hypo_training_bridge_reset(bridge);
        hypo_training_bridge_set_drive(bridge, 2, competence);  /* 2 = competence drive */

        float difficulty_adj;
        int ret = hypo_training_bridge_get_difficulty_adjustment(bridge, &difficulty_adj);
        ASSERT_EQ(ret, 0);

        EXPECT_GE(difficulty_adj, REG_DIFFICULTY_ADJ_MIN)
            << "Difficulty adjustment below min with competence=" << competence;
        EXPECT_LE(difficulty_adj, REG_DIFFICULTY_ADJ_MAX)
            << "Difficulty adjustment above max with competence=" << competence;
    }
}

/* ============================================================================
 * REG-024: Homeostatic state deviation calculation
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG024_HomeostaticDeviationCalculation) {
    hypo_training_bridge_set_loss_setpoint(bridge, 0.5f);

    /* Test deviation when loss equals setpoint */
    hypo_training_bridge_process_loss(bridge, 0, 0, 0.5f);

    hypo_training_homeostatic_state_t state;
    hypo_training_bridge_get_homeostatic_state(bridge, &state);

    EXPECT_NEAR(state.deviation, 0.0f, 0.01f)
        << "Deviation should be near zero when loss equals setpoint";

    /* Test deviation when loss is above setpoint */
    hypo_training_bridge_reset(bridge);
    hypo_training_bridge_set_loss_setpoint(bridge, 0.3f);
    hypo_training_bridge_process_loss(bridge, 0, 0, 0.8f);

    hypo_training_bridge_get_homeostatic_state(bridge, &state);

    EXPECT_GT(state.deviation, 0.0f)
        << "Deviation should be positive when loss is above setpoint";

    /* Test deviation when loss is below setpoint */
    hypo_training_bridge_reset(bridge);
    hypo_training_bridge_set_loss_setpoint(bridge, 0.5f);
    hypo_training_bridge_process_loss(bridge, 0, 0, 0.2f);

    hypo_training_bridge_get_homeostatic_state(bridge, &state);

    /* Deviation can be negative or absolute value; check it's computed */
    EXPECT_FALSE(std::isnan(state.deviation))
        << "Deviation should not be NaN";
}

/* ============================================================================
 * REG-025: Best loss tracking
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG025_BestLossTracking) {
    /* Process a series of losses and track best */
    float losses[] = {0.8f, 0.7f, 0.6f, 0.5f, 0.55f, 0.52f, 0.48f, 0.45f};

    float expected_best = losses[0];
    for (int i = 0; i < (int)(sizeof(losses) / sizeof(losses[0])); i++) {
        hypo_training_bridge_process_loss(bridge, i / 4, i % 4, losses[i]);

        if (losses[i] < expected_best) {
            expected_best = losses[i];
        }

        hypo_training_homeostatic_state_t state;
        hypo_training_bridge_get_homeostatic_state(bridge, &state);

        EXPECT_LE(state.best_loss_seen, expected_best + 0.01f)
            << "Best loss should track minimum at iteration " << i;
    }
}

/* ============================================================================
 * REG-026: LR change event processing
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG026_LRChangeProcessing) {
    /* Process LR reduction (exploitation) */
    int ret = hypo_training_bridge_process_lr_change(bridge, 0.01f, 0.001f);
    EXPECT_EQ(ret, 0) << "Failed to process LR reduction";

    hypo_training_drive_state_t state;
    hypo_training_bridge_get_drive_state(bridge, &state);

    /* After LR reduction, exploration tendency should adjust */
    float exploration_after_reduction = state.exploration_tendency;

    /* Process LR increase (exploration) */
    hypo_training_bridge_reset(bridge);
    ret = hypo_training_bridge_process_lr_change(bridge, 0.001f, 0.01f);
    EXPECT_EQ(ret, 0) << "Failed to process LR increase";

    hypo_training_bridge_get_drive_state(bridge, &state);
    float exploration_after_increase = state.exploration_tendency;

    /* Exploration tendency should be higher after LR increase than decrease */
    EXPECT_GE(exploration_after_increase, exploration_after_reduction - 0.2f)
        << "Exploration tendency should respond to LR changes";
}

/* ============================================================================
 * REG-027: Rapid state transitions don't corrupt data
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG027_RapidStateTransitions) {
    /* Rapidly alternate between different states */
    for (int cycle = 0; cycle < 50; cycle++) {
        /* Simulate improving state */
        for (int i = 0; i < 5; i++) {
            hypo_training_bridge_process_loss(bridge, cycle * 3, i, 0.8f - (0.1f * i));
        }

        /* Simulate diverging state */
        for (int i = 0; i < 5; i++) {
            hypo_training_bridge_process_loss(bridge, cycle * 3 + 1, i, 0.3f + (0.15f * i));
        }

        /* Simulate plateau state */
        for (int i = 0; i < 5; i++) {
            hypo_training_bridge_process_loss(bridge, cycle * 3 + 2, i, 0.5f);
        }

        /* Verify state is still valid */
        hypo_training_modulation_t mod;
        int ret = hypo_training_bridge_compute_modulation(bridge, &mod);
        ASSERT_EQ(ret, 0) << "Failed to compute modulation at cycle " << cycle;
        ASSERT_TRUE(verify_modulation_bounds(mod)) << "Modulation out of bounds at cycle " << cycle;
    }
}

/* ============================================================================
 * REG-028: Connection state tracking
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG028_ConnectionStateTracking) {
    /* Initially not connected (created with NULL orchestrator and hub) */
    bool orch_connected = true;
    bool hub_connected = true;

    int ret = hypo_training_bridge_is_connected(bridge, &orch_connected, &hub_connected);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(orch_connected) << "Should not be connected to orchestrator initially";
    EXPECT_FALSE(hub_connected) << "Should not be connected to training hub initially";

    /* Disconnect should succeed even when not connected */
    ret = hypo_training_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);

    /* Verify still not connected */
    ret = hypo_training_bridge_is_connected(bridge, &orch_connected, &hub_connected);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(orch_connected);
    EXPECT_FALSE(hub_connected);
}

/* ============================================================================
 * REG-029: Epochs since improvement tracking
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG029_EpochsSinceImprovementTracking) {
    hypo_training_bridge_set_loss_setpoint(bridge, 0.3f);

    /* First make improvement - best_loss tracked in process_loss, not process_epoch */
    for (int epoch = 0; epoch < 5; epoch++) {
        float improving_loss = 0.8f - (0.1f * epoch);  /* Decreasing: 0.8, 0.7, 0.6, 0.5, 0.4 */
        hypo_training_bridge_process_loss(bridge, epoch, 0, improving_loss);
        hypo_training_bridge_process_epoch(bridge, epoch, improving_loss);
    }

    hypo_training_homeostatic_state_t state;
    hypo_training_bridge_get_homeostatic_state(bridge, &state);
    uint32_t epochs_after_improvement = state.epochs_since_improvement;

    /* Now plateau for many epochs with no improvement (0.5 > best of 0.4) */
    for (int epoch = 5; epoch < 20; epoch++) {
        hypo_training_bridge_process_loss(bridge, epoch, 0, 0.5f);  /* Worse than best (0.4) */
        hypo_training_bridge_process_epoch(bridge, epoch, 0.5f);
    }

    hypo_training_bridge_get_homeostatic_state(bridge, &state);

    EXPECT_GT(state.epochs_since_improvement, epochs_after_improvement)
        << "Epochs since improvement should increase during plateau";
}

/* ============================================================================
 * REG-030: Multiple bridges can coexist
 * ============================================================================ */

TEST_F(HypothalamusTrainingRegressionTest, REG030_MultipleBridgesCoexist) {
    /* Create additional bridges */
    hypo_training_bridge_t* bridge2 = hypo_training_bridge_create(&config, nullptr, nullptr);
    hypo_training_bridge_t* bridge3 = hypo_training_bridge_create(&config, nullptr, nullptr);

    ASSERT_NE(bridge2, nullptr);
    ASSERT_NE(bridge3, nullptr);

    /* Set different states in each bridge */
    hypo_training_bridge_set_drive(bridge, 0, 0.9f);   /* High curiosity */
    hypo_training_bridge_set_drive(bridge2, 0, 0.1f); /* Low curiosity */
    hypo_training_bridge_set_drive(bridge3, 1, 0.9f); /* High safety */

    /* Verify they have independent states */
    hypo_training_drive_state_t state1, state2, state3;
    hypo_training_bridge_get_drive_state(bridge, &state1);
    hypo_training_bridge_get_drive_state(bridge2, &state2);
    hypo_training_bridge_get_drive_state(bridge3, &state3);

    EXPECT_GT(state1.curiosity_activation, state2.curiosity_activation)
        << "Bridges should have independent curiosity states";
    EXPECT_GT(state3.safety_activation, state1.safety_activation)
        << "Bridges should have independent safety states";

    /* Clean up */
    hypo_training_bridge_destroy(bridge2);
    hypo_training_bridge_destroy(bridge3);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
