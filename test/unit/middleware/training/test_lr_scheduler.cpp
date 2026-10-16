/**
 * @file test_lr_scheduler.cpp
 * @brief Unit tests for Learning Rate Scheduler module
 *
 * Tests all scheduler types:
 * - StepLR, ExponentialLR, CosineAnnealingLR
 * - LinearWarmup, MultiStepLR, ReduceOnPlateau
 * - CyclicLR, OneCycleLR, PolynomialLR
 *
 * @note Part of Phase TM-4: Learning Rate Scheduling
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_lr_scheduler.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class LRSchedulerTest : public ::testing::Test {
protected:
    void SetUp() override {
        scheduler = nullptr;
    }

    void TearDown() override {
        if (scheduler) {
            nimcp_lr_scheduler_destroy(scheduler);
            scheduler = nullptr;
        }
    }

    nimcp_lr_scheduler_ctx_t* scheduler;
};

/* ============================================================================
 * Constant Scheduler Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, ConstantScheduler_MaintainsLR) {
    nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(NIMCP_LR_CONSTANT, 0.001f);
    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    float initial_lr = nimcp_lr_scheduler_get_lr(scheduler);
    EXPECT_FLOAT_EQ(initial_lr, 0.001f);

    // Step multiple times - LR should not change
    for (int i = 0; i < 100; i++) {
        nimcp_lr_scheduler_step(scheduler);
    }
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.001f);

    // Epoch steps should also not change LR
    for (int i = 0; i < 50; i++) {
        nimcp_lr_scheduler_step_epoch(scheduler);
    }
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.001f);
}

/* ============================================================================
 * StepLR Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, StepLR_DecaysCorrectly) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_STEP;
    config.params.step = nimcp_step_lr_default_config(0.1f, 10);

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Initial LR
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.1f);

    // Step through epochs
    for (int epoch = 1; epoch <= 30; epoch++) {
        float lr = nimcp_lr_scheduler_step_epoch(scheduler);

        if (epoch < 10) {
            EXPECT_FLOAT_EQ(lr, 0.1f) << "Epoch " << epoch;
        } else if (epoch < 20) {
            EXPECT_FLOAT_EQ(lr, 0.01f) << "Epoch " << epoch;
        } else if (epoch < 30) {
            EXPECT_FLOAT_EQ(lr, 0.001f) << "Epoch " << epoch;
        } else {
            EXPECT_FLOAT_EQ(lr, 0.0001f) << "Epoch " << epoch;
        }
    }
}

TEST_F(LRSchedulerTest, StepLR_RespectsMinLR) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_STEP;
    config.params.step.initial_lr = 0.1f;
    config.params.step.step_size = 1;
    config.params.step.gamma = 0.1f;
    config.params.step.min_lr = 1e-5f;

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Step many epochs - should hit min_lr floor
    for (int epoch = 0; epoch < 100; epoch++) {
        nimcp_lr_scheduler_step_epoch(scheduler);
    }

    EXPECT_GE(nimcp_lr_scheduler_get_lr(scheduler), 1e-5f);
}

TEST_F(LRSchedulerTest, StepLR_GetLRAtEpoch) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_STEP;
    config.params.step = nimcp_step_lr_default_config(0.1f, 10);

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Check future epochs without modifying state
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr_at_epoch(scheduler, 0), 0.1f);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr_at_epoch(scheduler, 5), 0.1f);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr_at_epoch(scheduler, 10), 0.01f);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr_at_epoch(scheduler, 20), 0.001f);

    // Current state should be unchanged
    EXPECT_EQ(nimcp_lr_scheduler_get_epoch(scheduler), 0u);
}

/* ============================================================================
 * ExponentialLR Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, ExponentialLR_DecaysCorrectly) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_EXPONENTIAL;
    config.params.exponential = nimcp_exponential_lr_default_config(0.1f, 0.9f);

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Check exponential decay
    for (int epoch = 0; epoch < 10; epoch++) {
        float expected = 0.1f * std::pow(0.9f, (float)epoch);
        float actual = nimcp_lr_scheduler_get_lr_at_epoch(scheduler, epoch);
        EXPECT_NEAR(actual, expected, 1e-6f) << "Epoch " << epoch;
    }
}

TEST_F(LRSchedulerTest, ExponentialLR_StepEpochUpdatesLR) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_EXPONENTIAL;
    config.params.exponential = nimcp_exponential_lr_default_config(1.0f, 0.5f);

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 1.0f);

    nimcp_lr_scheduler_step_epoch(scheduler);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.5f);

    nimcp_lr_scheduler_step_epoch(scheduler);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.25f);

    nimcp_lr_scheduler_step_epoch(scheduler);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.125f);
}

/* ============================================================================
 * CosineAnnealingLR Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, CosineAnnealing_FollowsCosine) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_COSINE_ANNEALING;
    config.params.cosine = nimcp_cosine_lr_default_config(0.1f, 100);

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Check cosine curve at key points
    float lr_0 = nimcp_lr_scheduler_get_lr_at_epoch(scheduler, 0);
    float lr_25 = nimcp_lr_scheduler_get_lr_at_epoch(scheduler, 25);
    float lr_50 = nimcp_lr_scheduler_get_lr_at_epoch(scheduler, 50);
    float lr_75 = nimcp_lr_scheduler_get_lr_at_epoch(scheduler, 75);
    float lr_100 = nimcp_lr_scheduler_get_lr_at_epoch(scheduler, 100);

    EXPECT_FLOAT_EQ(lr_0, 0.1f);  // Start at max
    EXPECT_GT(lr_25, lr_50);      // Decreasing
    EXPECT_NEAR(lr_50, 0.05f, 0.001f);  // Midpoint
    EXPECT_LT(lr_75, lr_50);      // Continuing decrease
    EXPECT_NEAR(lr_100, 0.0f, 0.001f);  // End at eta_min
}

TEST_F(LRSchedulerTest, CosineAnnealing_WithEtaMin) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_COSINE_ANNEALING;
    config.params.cosine.initial_lr = 0.1f;
    config.params.cosine.T_max = 100;
    config.params.cosine.eta_min = 0.01f;
    config.params.cosine.restart = false;

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // At T_max, LR should be eta_min
    float lr_at_tmax = nimcp_lr_scheduler_get_lr_at_epoch(scheduler, 100);
    EXPECT_NEAR(lr_at_tmax, 0.01f, 0.001f);
}

/* ============================================================================
 * LinearWarmup Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, LinearWarmup_RampsUp) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_LINEAR_WARMUP;
    config.params.warmup = nimcp_warmup_lr_default_config(0.1f, 100);

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Should start at 0
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.0f);

    // Linear ramp
    for (int step = 1; step <= 100; step++) {
        float lr = nimcp_lr_scheduler_step(scheduler);
        float expected = 0.1f * (float)step / 100.0f;
        EXPECT_NEAR(lr, expected, 1e-6f) << "Step " << step;
    }

    // After warmup, should stay at target
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.1f);
    nimcp_lr_scheduler_step(scheduler);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.1f);
}

TEST_F(LRSchedulerTest, LinearWarmup_CustomStartLR) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_LINEAR_WARMUP;
    config.params.warmup.start_lr = 0.01f;
    config.params.warmup.target_lr = 0.1f;
    config.params.warmup.warmup_steps = 10;
    config.params.warmup.hold_after_warmup = true;

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.01f);

    // Step halfway
    for (int i = 0; i < 5; i++) {
        nimcp_lr_scheduler_step(scheduler);
    }
    EXPECT_NEAR(nimcp_lr_scheduler_get_lr(scheduler), 0.055f, 0.001f);
}

/* ============================================================================
 * MultiStepLR Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, MultiStepLR_DecaysAtMilestones) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_MULTI_STEP;
    config.params.multi_step.initial_lr = 0.1f;
    config.params.multi_step.gamma = 0.1f;
    config.params.multi_step.milestones[0] = 10;
    config.params.multi_step.milestones[1] = 20;
    config.params.multi_step.milestones[2] = 30;
    config.params.multi_step.num_milestones = 3;
    config.params.multi_step.min_lr = 1e-10f;

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Before first milestone
    for (int epoch = 1; epoch < 10; epoch++) {
        nimcp_lr_scheduler_step_epoch(scheduler);
        EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.1f) << "Epoch " << epoch;
    }

    // After first milestone
    nimcp_lr_scheduler_step_epoch(scheduler);  // Epoch 10
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.01f);

    // Continue to second milestone
    for (int epoch = 11; epoch < 20; epoch++) {
        nimcp_lr_scheduler_step_epoch(scheduler);
        EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.01f);
    }

    // After second milestone
    nimcp_lr_scheduler_step_epoch(scheduler);  // Epoch 20
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.001f);

    // Continue to third milestone
    for (int epoch = 21; epoch < 30; epoch++) {
        nimcp_lr_scheduler_step_epoch(scheduler);
        EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.001f);
    }

    // After third milestone
    nimcp_lr_scheduler_step_epoch(scheduler);  // Epoch 30
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.0001f);
}

/* ============================================================================
 * ReduceOnPlateau Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, ReduceOnPlateau_ReducesOnStagnation) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_REDUCE_ON_PLATEAU;
    config.params.plateau = nimcp_plateau_lr_default_config(0.1f);
    config.params.plateau.patience = 5;
    config.params.plateau.factor = 0.5f;

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Initial LR
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.1f);

    // Improving metric - no reduction
    for (float loss = 1.0f; loss > 0.5f; loss -= 0.1f) {
        nimcp_lr_scheduler_step_metric(scheduler, loss);
    }
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.1f);

    // Stagnant metric - after patience epochs, should reduce
    for (int i = 0; i < 6; i++) {
        nimcp_lr_scheduler_step_metric(scheduler, 0.5f);
    }
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.05f);

    // More stagnation
    for (int i = 0; i < 6; i++) {
        nimcp_lr_scheduler_step_metric(scheduler, 0.5f);
    }
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.025f);
}

TEST_F(LRSchedulerTest, ReduceOnPlateau_MaxMode) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_REDUCE_ON_PLATEAU;
    config.params.plateau.initial_lr = 0.1f;
    config.params.plateau.mode = NIMCP_PLATEAU_MAX;
    config.params.plateau.patience = 3;
    config.params.plateau.factor = 0.1f;
    config.params.plateau.threshold = 1e-4f;
    config.params.plateau.min_lr = 1e-8f;
    config.params.plateau.cooldown = 0;

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Improving accuracy
    nimcp_lr_scheduler_step_metric(scheduler, 0.5f);
    nimcp_lr_scheduler_step_metric(scheduler, 0.6f);
    nimcp_lr_scheduler_step_metric(scheduler, 0.7f);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.1f);

    // Stagnant accuracy
    nimcp_lr_scheduler_step_metric(scheduler, 0.7f);
    nimcp_lr_scheduler_step_metric(scheduler, 0.7f);
    nimcp_lr_scheduler_step_metric(scheduler, 0.7f);
    nimcp_lr_scheduler_step_metric(scheduler, 0.7f);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.01f);
}

TEST_F(LRSchedulerTest, ReduceOnPlateau_Cooldown) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_REDUCE_ON_PLATEAU;
    config.params.plateau.initial_lr = 0.1f;
    config.params.plateau.mode = NIMCP_PLATEAU_MIN;
    config.params.plateau.patience = 2;
    config.params.plateau.factor = 0.5f;
    config.params.plateau.cooldown = 3;
    config.params.plateau.threshold = 1e-4f;
    config.params.plateau.min_lr = 1e-8f;

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Set baseline
    nimcp_lr_scheduler_step_metric(scheduler, 1.0f);

    // Trigger reduction
    nimcp_lr_scheduler_step_metric(scheduler, 1.0f);
    nimcp_lr_scheduler_step_metric(scheduler, 1.0f);
    nimcp_lr_scheduler_step_metric(scheduler, 1.0f);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.05f);

    // During cooldown, no further reductions
    nimcp_lr_scheduler_step_metric(scheduler, 1.0f);
    nimcp_lr_scheduler_step_metric(scheduler, 1.0f);
    nimcp_lr_scheduler_step_metric(scheduler, 1.0f);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.05f);  // Still in cooldown
}

/* ============================================================================
 * CyclicLR Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, CyclicLR_TriangularMode) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_CYCLIC;
    config.params.cyclic = nimcp_cyclic_lr_default_config(0.001f, 0.01f, 10);
    config.params.cyclic.step_size_down = 10;  // Same as up

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    std::vector<float> lrs;
    for (int step = 0; step < 40; step++) {
        lrs.push_back(nimcp_lr_scheduler_step(scheduler));
    }

    // Check cycle structure
    // Steps 1-10: increasing
    EXPECT_LT(lrs[0], lrs[5]);
    EXPECT_LT(lrs[5], lrs[9]);

    // Steps 11-20: decreasing
    EXPECT_GT(lrs[10], lrs[15]);
    EXPECT_GT(lrs[15], lrs[19]);

    // Second cycle should repeat
    EXPECT_NEAR(lrs[0], lrs[20], 0.001f);
}

TEST_F(LRSchedulerTest, CyclicLR_ReachesBounds) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_CYCLIC;
    config.params.cyclic.base_lr = 0.001f;
    config.params.cyclic.max_lr = 0.01f;
    config.params.cyclic.step_size_up = 100;
    config.params.cyclic.step_size_down = 100;
    config.params.cyclic.mode = NIMCP_CYCLIC_TRIANGULAR;
    config.params.cyclic.gamma = 1.0f;

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    float max_lr_seen = 0.0f;
    float min_lr_seen = 1.0f;

    for (int step = 0; step < 200; step++) {
        float lr = nimcp_lr_scheduler_step(scheduler);
        max_lr_seen = std::max(max_lr_seen, lr);
        min_lr_seen = std::min(min_lr_seen, lr);
    }

    EXPECT_NEAR(max_lr_seen, 0.01f, 0.0001f);
    EXPECT_NEAR(min_lr_seen, 0.001f, 0.0001f);
}

/* ============================================================================
 * OneCycleLR Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, OneCycleLR_WarmupAndAnneal) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_ONE_CYCLE;
    config.params.one_cycle = nimcp_one_cycle_lr_default_config(0.1f, 100);

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Initial LR should be max_lr / div_factor
    float initial = 0.1f / 25.0f;  // div_factor = 25
    EXPECT_NEAR(nimcp_lr_scheduler_get_lr(scheduler), initial, 0.001f);

    // Warmup phase (30% of steps)
    for (int step = 0; step < 30; step++) {
        nimcp_lr_scheduler_step(scheduler);
    }
    // At end of warmup, should be near max_lr
    EXPECT_NEAR(nimcp_lr_scheduler_get_lr(scheduler), 0.1f, 0.01f);

    // Annealing phase
    for (int step = 30; step < 100; step++) {
        nimcp_lr_scheduler_step(scheduler);
    }
    // At end, should be near final_lr (max_lr / final_div_factor)
    float final_lr = 0.1f / 10000.0f;
    EXPECT_NEAR(nimcp_lr_scheduler_get_lr(scheduler), final_lr, 0.0001f);
}

/* ============================================================================
 * PolynomialLR Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, PolynomialLR_LinearDecay) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_POLYNOMIAL;
    config.params.polynomial.initial_lr = 0.1f;
    config.params.polynomial.end_lr = 0.0f;
    config.params.polynomial.total_steps = 100;
    config.params.polynomial.power = 1.0f;  // Linear

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.1f);

    // Linear decay
    for (int step = 1; step <= 100; step++) {
        float lr = nimcp_lr_scheduler_step(scheduler);
        float expected = 0.1f * (1.0f - (float)step / 100.0f);
        EXPECT_NEAR(lr, expected, 1e-6f) << "Step " << step;
    }
}

TEST_F(LRSchedulerTest, PolynomialLR_QuadraticDecay) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_POLYNOMIAL;
    config.params.polynomial.initial_lr = 1.0f;
    config.params.polynomial.end_lr = 0.0f;
    config.params.polynomial.total_steps = 100;
    config.params.polynomial.power = 2.0f;  // Quadratic

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Check at midpoint
    float lr_50 = nimcp_lr_scheduler_get_lr_at_step(scheduler, 50);
    // (1.0 - 0.0) * (1 - 50/100)^2 + 0.0 = 0.25
    EXPECT_NEAR(lr_50, 0.25f, 0.001f);
}

/* ============================================================================
 * State Management Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, Reset_RestoresInitialState) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_STEP;
    config.params.step = nimcp_step_lr_default_config(0.1f, 5);

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Step several epochs
    for (int i = 0; i < 20; i++) {
        nimcp_lr_scheduler_step_epoch(scheduler);
    }
    EXPECT_NE(nimcp_lr_scheduler_get_lr(scheduler), 0.1f);

    // Reset
    nimcp_result_t result = nimcp_lr_scheduler_reset(scheduler);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.1f);
    EXPECT_EQ(nimcp_lr_scheduler_get_epoch(scheduler), 0u);
    EXPECT_EQ(nimcp_lr_scheduler_get_step(scheduler), 0u);
}

TEST_F(LRSchedulerTest, SetStep_ResumeTraining) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_LINEAR_WARMUP;
    config.params.warmup = nimcp_warmup_lr_default_config(0.1f, 100);

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Simulate resuming at step 50
    nimcp_lr_scheduler_set_step(scheduler, 50);

    float lr = nimcp_lr_scheduler_step(scheduler);  // Now at step 51
    EXPECT_NEAR(lr, 0.051f, 0.001f);
}

TEST_F(LRSchedulerTest, SetEpoch_ResumeTraining) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_STEP;
    config.params.step = nimcp_step_lr_default_config(0.1f, 10);

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Simulate resuming at epoch 15
    nimcp_lr_scheduler_set_epoch(scheduler, 15);

    float lr = nimcp_lr_scheduler_step_epoch(scheduler);  // Now at epoch 16
    EXPECT_FLOAT_EQ(lr, 0.01f);  // Should be after first decay
}

TEST_F(LRSchedulerTest, ManualSetLR) {
    nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(NIMCP_LR_CONSTANT, 0.001f);
    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    nimcp_result_t result = nimcp_lr_scheduler_set_lr(scheduler, 0.05f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.05f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, Statistics_TracksMinMaxLR) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_CYCLIC;
    config.params.cyclic = nimcp_cyclic_lr_default_config(0.001f, 0.1f, 50);

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Run through a full cycle
    for (int step = 0; step < 200; step++) {
        nimcp_lr_scheduler_step(scheduler);
    }

    nimcp_lr_scheduler_stats_t stats;
    nimcp_result_t result = nimcp_lr_scheduler_get_stats(scheduler, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(stats.total_steps, 200u);
    EXPECT_NEAR(stats.min_lr_seen, 0.001f, 0.001f);
    EXPECT_NEAR(stats.max_lr_seen, 0.1f, 0.001f);
}

TEST_F(LRSchedulerTest, Statistics_TracksReductions) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_REDUCE_ON_PLATEAU;
    config.params.plateau = nimcp_plateau_lr_default_config(0.1f);
    config.params.plateau.patience = 2;
    config.params.plateau.factor = 0.5f;

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Trigger multiple reductions
    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 4; i++) {
            nimcp_lr_scheduler_step_metric(scheduler, 1.0f);
        }
    }

    nimcp_lr_scheduler_stats_t stats;
    nimcp_lr_scheduler_get_stats(scheduler, &stats);
    EXPECT_GE(stats.num_reductions, 2u);
}

TEST_F(LRSchedulerTest, ResetStats) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_STEP;
    config.params.step = nimcp_step_lr_default_config(0.1f, 10);

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    for (int i = 0; i < 50; i++) {
        nimcp_lr_scheduler_step_epoch(scheduler);
    }

    nimcp_lr_scheduler_reset_stats(scheduler);

    nimcp_lr_scheduler_stats_t stats;
    nimcp_lr_scheduler_get_stats(scheduler, &stats);
    EXPECT_EQ(stats.total_steps, 0u);
    EXPECT_EQ(stats.total_epochs, 0u);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, TypeName_ReturnsCorrectStrings) {
    EXPECT_STREQ(nimcp_lr_scheduler_type_name(NIMCP_LR_CONSTANT), "Constant");
    EXPECT_STREQ(nimcp_lr_scheduler_type_name(NIMCP_LR_STEP), "StepLR");
    EXPECT_STREQ(nimcp_lr_scheduler_type_name(NIMCP_LR_EXPONENTIAL), "ExponentialLR");
    EXPECT_STREQ(nimcp_lr_scheduler_type_name(NIMCP_LR_COSINE_ANNEALING), "CosineAnnealingLR");
    EXPECT_STREQ(nimcp_lr_scheduler_type_name(NIMCP_LR_LINEAR_WARMUP), "LinearWarmup");
    EXPECT_STREQ(nimcp_lr_scheduler_type_name(NIMCP_LR_MULTI_STEP), "MultiStepLR");
    EXPECT_STREQ(nimcp_lr_scheduler_type_name(NIMCP_LR_REDUCE_ON_PLATEAU), "ReduceOnPlateau");
    EXPECT_STREQ(nimcp_lr_scheduler_type_name(NIMCP_LR_CYCLIC), "CyclicLR");
    EXPECT_STREQ(nimcp_lr_scheduler_type_name(NIMCP_LR_ONE_CYCLE), "OneCycleLR");
    EXPECT_STREQ(nimcp_lr_scheduler_type_name(NIMCP_LR_POLYNOMIAL), "PolynomialLR");
}

TEST_F(LRSchedulerTest, GetType_ReturnsConfiguredType) {
    nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(NIMCP_LR_EXPONENTIAL, 0.01f);
    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    EXPECT_EQ(nimcp_lr_scheduler_get_type(scheduler), NIMCP_LR_EXPONENTIAL);
}

TEST_F(LRSchedulerTest, ConfigFromType_SetsDefaults) {
    // Test each type
    for (int type = NIMCP_LR_CONSTANT; type < NIMCP_LR_CUSTOM; type++) {
        nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
            (nimcp_lr_scheduler_type_t)type, 0.01f);

        nimcp_result_t result = nimcp_lr_scheduler_validate_config(&config);
        EXPECT_EQ(result, NIMCP_SUCCESS) << "Type " << type << " failed validation";
    }
}

/* ============================================================================
 * Validation Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, Validation_RejectsInvalidConfig) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));

    // Invalid type
    config.type = (nimcp_lr_scheduler_type_t)999;
    EXPECT_NE(nimcp_lr_scheduler_validate_config(&config), NIMCP_SUCCESS);

    // Invalid StepLR
    config.type = NIMCP_LR_STEP;
    config.params.step.initial_lr = -0.1f;
    EXPECT_NE(nimcp_lr_scheduler_validate_config(&config), NIMCP_SUCCESS);

    config.params.step.initial_lr = 0.1f;
    config.params.step.step_size = 0;
    EXPECT_NE(nimcp_lr_scheduler_validate_config(&config), NIMCP_SUCCESS);

    config.params.step.step_size = 10;
    config.params.step.gamma = 1.5f;  // > 1
    EXPECT_NE(nimcp_lr_scheduler_validate_config(&config), NIMCP_SUCCESS);
}

TEST_F(LRSchedulerTest, Validation_RejectsInvalidCyclicConfig) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_CYCLIC;

    // max_lr <= base_lr
    config.params.cyclic.base_lr = 0.1f;
    config.params.cyclic.max_lr = 0.1f;
    config.params.cyclic.step_size_up = 100;
    EXPECT_NE(nimcp_lr_scheduler_validate_config(&config), NIMCP_SUCCESS);
}

TEST_F(LRSchedulerTest, Create_ReturnsNullForInvalidConfig) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_STEP;
    config.params.step.initial_lr = -1.0f;  // Invalid

    scheduler = nimcp_lr_scheduler_create(&config);
    EXPECT_EQ(scheduler, nullptr);
}

TEST_F(LRSchedulerTest, Create_ReturnsNullForNullConfig) {
    scheduler = nimcp_lr_scheduler_create(nullptr);
    EXPECT_EQ(scheduler, nullptr);
}

/* ============================================================================
 * Custom Scheduler Tests
 * ============================================================================ */

static float custom_step_fn(uint64_t step, uint64_t epoch, void* state, void* user_data) {
    (void)state;
    float* base_lr = (float*)user_data;
    // Simple linear decay
    return *base_lr * (1.0f - (float)step / 100.0f);
}

TEST_F(LRSchedulerTest, CustomScheduler_UsesCallback) {
    static float base_lr = 0.1f;

    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_CUSTOM;
    config.params.custom.initial_lr = 0.1f;
    config.params.custom.step_fn = custom_step_fn;
    config.params.custom.user_data = &base_lr;
    config.params.custom.name = "CustomLinearDecay";

    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.1f);

    // Step and verify custom function is called
    float lr = nimcp_lr_scheduler_step(scheduler);
    EXPECT_NEAR(lr, 0.099f, 0.001f);  // 0.1 * (1 - 1/100)
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, NullContext_HandledGracefully) {
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_step(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_step_epoch(nullptr), 0.0f);
    EXPECT_EQ(nimcp_lr_scheduler_get_step(nullptr), 0u);
    EXPECT_EQ(nimcp_lr_scheduler_get_epoch(nullptr), 0u);
    EXPECT_EQ(nimcp_lr_scheduler_set_lr(nullptr, 0.1f), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_lr_scheduler_reset(nullptr), NIMCP_ERROR_INVALID_PARAM);

    nimcp_lr_scheduler_stats_t stats;
    EXPECT_EQ(nimcp_lr_scheduler_get_stats(nullptr, &stats), NIMCP_ERROR_INVALID_PARAM);

    // Should not crash
    nimcp_lr_scheduler_destroy(nullptr);
    nimcp_lr_scheduler_reset_stats(nullptr);
}

TEST_F(LRSchedulerTest, LRClamping_EnforcedBounds) {
    nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(NIMCP_LR_CONSTANT, 0.001f);
    scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Try to set LR below minimum
    nimcp_lr_scheduler_set_lr(scheduler, 1e-15f);
    EXPECT_GE(nimcp_lr_scheduler_get_lr(scheduler), NIMCP_LR_MIN_VALUE);

    // Try to set LR above maximum
    nimcp_lr_scheduler_set_lr(scheduler, 100.0f);
    EXPECT_LE(nimcp_lr_scheduler_get_lr(scheduler), NIMCP_LR_MAX_VALUE);
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, DefaultConfigs_HaveReasonableValues) {
    nimcp_step_lr_config_t step = nimcp_step_lr_default_config(0.1f, 30);
    EXPECT_FLOAT_EQ(step.initial_lr, 0.1f);
    EXPECT_EQ(step.step_size, 30u);
    EXPECT_FLOAT_EQ(step.gamma, 0.1f);

    nimcp_exponential_lr_config_t exp = nimcp_exponential_lr_default_config(0.1f, 0.9f);
    EXPECT_FLOAT_EQ(exp.initial_lr, 0.1f);
    EXPECT_FLOAT_EQ(exp.gamma, 0.9f);

    nimcp_cosine_lr_config_t cos = nimcp_cosine_lr_default_config(0.1f, 100);
    EXPECT_FLOAT_EQ(cos.initial_lr, 0.1f);
    EXPECT_EQ(cos.T_max, 100u);
    EXPECT_FLOAT_EQ(cos.eta_min, 0.0f);

    nimcp_warmup_lr_config_t warmup = nimcp_warmup_lr_default_config(0.1f, 1000);
    EXPECT_FLOAT_EQ(warmup.start_lr, 0.0f);
    EXPECT_FLOAT_EQ(warmup.target_lr, 0.1f);
    EXPECT_EQ(warmup.warmup_steps, 1000u);

    nimcp_plateau_lr_config_t plateau = nimcp_plateau_lr_default_config(0.1f);
    EXPECT_FLOAT_EQ(plateau.initial_lr, 0.1f);
    EXPECT_EQ(plateau.mode, NIMCP_PLATEAU_MIN);
    EXPECT_FLOAT_EQ(plateau.factor, 0.1f);
    EXPECT_EQ(plateau.patience, 10u);

    nimcp_cyclic_lr_config_t cyclic = nimcp_cyclic_lr_default_config(0.001f, 0.01f, 2000);
    EXPECT_FLOAT_EQ(cyclic.base_lr, 0.001f);
    EXPECT_FLOAT_EQ(cyclic.max_lr, 0.01f);
    EXPECT_EQ(cyclic.step_size_up, 2000u);

    nimcp_one_cycle_lr_config_t one_cycle = nimcp_one_cycle_lr_default_config(0.1f, 10000);
    EXPECT_FLOAT_EQ(one_cycle.max_lr, 0.1f);
    EXPECT_EQ(one_cycle.total_steps, 10000u);
    EXPECT_FLOAT_EQ(one_cycle.pct_start, 0.3f);
}

/* ============================================================================
 * Composite Scheduler Tests
 * ============================================================================ */

TEST_F(LRSchedulerTest, WarmupWithCosine_Combined) {
    nimcp_warmup_lr_config_t warmup;
    warmup.start_lr = 0.0f;
    warmup.target_lr = 0.1f;
    warmup.warmup_steps = 100;
    warmup.hold_after_warmup = false;

    nimcp_lr_scheduler_config_t main_config;
    memset(&main_config, 0, sizeof(main_config));
    main_config.type = NIMCP_LR_COSINE_ANNEALING;
    main_config.params.cosine = nimcp_cosine_lr_default_config(0.1f, 200);

    scheduler = nimcp_lr_scheduler_create_with_warmup(&warmup, &main_config);
    ASSERT_NE(scheduler, nullptr);

    // During warmup
    EXPECT_FLOAT_EQ(nimcp_lr_scheduler_get_lr(scheduler), 0.0f);

    for (int step = 0; step < 50; step++) {
        nimcp_lr_scheduler_step(scheduler);
    }
    EXPECT_NEAR(nimcp_lr_scheduler_get_lr(scheduler), 0.05f, 0.001f);

    // At end of warmup
    for (int step = 50; step < 100; step++) {
        nimcp_lr_scheduler_step(scheduler);
    }
    EXPECT_NEAR(nimcp_lr_scheduler_get_lr(scheduler), 0.1f, 0.001f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
