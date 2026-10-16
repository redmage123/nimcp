/**
 * @file test_training_integration.cpp
 * @brief Integration tests for the cognitive training-integration facade
 *
 * WHAT: Tests the brain_ti_* unified facade that connects reward/RPE,
 *       habit, arousal/circadian, symbolic-logic, and reasoning subsystems
 *       to a live brain instance.
 * WHY:  The training pipeline relies on these accessors to drive adaptive
 *       learning-rate, curriculum selection, and post-batch consolidation.
 *       Integration tests ensure the delegation layer correctly interacts
 *       with the real brain subsystems.
 * HOW:  GTest executable linked against nimcp; creates a BRAIN_SIZE_SMALL
 *       brain (8 inputs, 3 outputs) and exercises each brain_ti_* function.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/training/nimcp_training_integration.h"
#include "core/brain/nimcp_brain.h"
}

// ============================================================================
// Shared fixture: real BRAIN_SIZE_SMALL brain for training integration tests
// ============================================================================
class TrainingIntegrationBrain : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create("training_test", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 8, 3);
        ASSERT_NE(brain, nullptr) << "Failed to create test brain";
    }

    void TearDown() override {
        // Destroy reasoning engine if it was initialised during the test
        brain_ti_destroy_reasoning(brain);

        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

// ============================================================================
// Basal Ganglia — Reward / RPE / Dopamine
// ============================================================================

TEST_F(TrainingIntegrationBrain, BgUpdateRewardSucceeds) {
    // Inject a reward signal: actual 0.8 vs expected 0.5
    int rc = brain_ti_update_reward(brain, 0.8f, 0.5f);
    EXPECT_EQ(rc, 0) << "brain_ti_update_reward should return 0 on success";
}

TEST_F(TrainingIntegrationBrain, BgConflictInRange) {
    float conflict = brain_ti_get_conflict(brain);
    EXPECT_GE(conflict, 0.0f) << "Conflict must be >= 0.0";
    EXPECT_LE(conflict, 1.0f) << "Conflict must be <= 1.0";
}

TEST_F(TrainingIntegrationBrain, BgModeIsValid) {
    int mode = brain_ti_get_mode(brain);
    EXPECT_GE(mode, 0) << "Mode must be non-negative";
    EXPECT_LE(mode, 3) << "Mode must be 0-3 (goal-directed / habitual / ...)";
}

TEST_F(TrainingIntegrationBrain, BgDopamineInRange) {
    float da = brain_ti_get_dopamine(brain);
    EXPECT_GE(da, 0.0f) << "Dopamine must be >= 0.0";
    EXPECT_LE(da, 1.0f) << "Dopamine must be <= 1.0";
}

TEST_F(TrainingIntegrationBrain, BgRewardUpdatesRpe) {
    // Before any reward signal, RPE should be 0 or near-zero
    float rpe_before = brain_ti_get_rpe(brain);

    // Inject a large positive surprise: actual 0.9 vs expected 0.5
    int rc = brain_ti_update_reward(brain, 0.9f, 0.5f);
    EXPECT_EQ(rc, 0);

    float rpe_after = brain_ti_get_rpe(brain);
    // RPE should have changed (positive surprise expected)
    EXPECT_NE(rpe_after, rpe_before)
        << "RPE should change after update_reward with a surprise";
}

// ============================================================================
// Medulla — Arousal / Circadian
// ============================================================================

TEST_F(TrainingIntegrationBrain, MedullaArousalInRange) {
    float arousal = brain_ti_get_arousal(brain);
    EXPECT_GE(arousal, 0.0f) << "Arousal must be >= 0.0";
    EXPECT_LE(arousal, 1.0f) << "Arousal must be <= 1.0";
}

TEST_F(TrainingIntegrationBrain, MedullaCircadianPhaseValid) {
    int phase = brain_ti_get_circadian_phase(brain);
    EXPECT_GE(phase, 0)  << "Circadian phase must be >= 0";
    EXPECT_LE(phase, 7)  << "Circadian phase must be <= 7";
}

TEST_F(TrainingIntegrationBrain, MedullaCircadianEfficiencyReasonable) {
    float eff = brain_ti_get_circadian_efficiency(brain);
    EXPECT_GE(eff, 0.5f) << "Circadian efficiency must be >= 0.5";
    EXPECT_LE(eff, 2.0f) << "Circadian efficiency must be <= 2.0";
}

// ============================================================================
// Symbolic-Logic Knowledge Base
// ============================================================================

TEST_F(TrainingIntegrationBrain, AddFactDoesNotCrash) {
    // May succeed or fail depending on whether the logic engine is
    // initialised by the brain factory, but must not crash.
    bool ok = brain_ti_add_fact(brain, "Bird(tweety)", 0.9f);
    // We do not assert on the return value: some brain configurations
    // may not initialise the logic subsystem.  The key is no segfault.
    (void)ok;
}

TEST_F(TrainingIntegrationBrain, ForwardChainDoesNotCrash) {
    // Forward-chaining should return >= -1 (number of new facts, or -1
    // if the logic subsystem is absent).
    int derived = brain_ti_forward_chain(brain, 10);
    EXPECT_GE(derived, -1) << "forward_chain should return >= -1";
}

// ============================================================================
// Reasoning Subsystem
// ============================================================================

TEST_F(TrainingIntegrationBrain, InitReasoningSucceeds) {
    int rc = brain_ti_init_reasoning(brain);
    EXPECT_EQ(rc, 0) << "brain_ti_init_reasoning should return 0 on success";
}

TEST_F(TrainingIntegrationBrain, ReasonReturnsConfidence) {
    // Initialise the reasoning engine first
    int rc = brain_ti_init_reasoning(brain);
    ASSERT_EQ(rc, 0) << "Reasoning init must succeed before querying";

    float conf = brain_ti_reason(brain, "What is 2+2?");
    // Confidence should be >= -1.0 (negative signals error, 0..1 is valid)
    EXPECT_GE(conf, -1.0f)
        << "brain_ti_reason should return >= -1.0";
}

// ============================================================================
// Combined / Adaptive Training Helpers
// ============================================================================

TEST_F(TrainingIntegrationBrain, ComputeAdaptiveLrReasonable) {
    float adjusted = brain_ti_compute_adaptive_lr(brain, 0.5f);
    // The adjusted LR should remain in a reasonable range.
    // With default arousal/circadian, the multiplier is ~1.0, so the
    // result should be in [0.1, 2.0] for a base LR of 0.5.
    EXPECT_GT(adjusted, 0.0f)  << "Adjusted LR must be positive";
    EXPECT_LT(adjusted, 2.0f)  << "Adjusted LR should not be unreasonably large";
}

TEST_F(TrainingIntegrationBrain, PostBatchUpdateSucceeds) {
    int rc = brain_ti_post_batch_update(brain, 0.8f, 0.5f, "science");
    EXPECT_EQ(rc, 0) << "brain_ti_post_batch_update should return 0";
}

TEST_F(TrainingIntegrationBrain, PostBatchSequenceWorks) {
    // Run 5 sequential post-batch updates with increasing accuracy.
    // The key assertion is that nothing crashes and all calls return 0.
    for (int i = 0; i < 5; ++i) {
        float accuracy = 0.5f + 0.1f * static_cast<float>(i);
        float expected = 0.5f;
        int rc = brain_ti_post_batch_update(brain, accuracy, expected, "science");
        EXPECT_EQ(rc, 0)
            << "post_batch_update iteration " << i << " should succeed";
    }
}
