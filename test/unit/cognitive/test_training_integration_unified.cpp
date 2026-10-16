/**
 * @file test_training_integration_unified.cpp
 * @brief Unit tests for the unified continuous modulation pipeline
 *
 * WHAT: Tests NULL-safety, default values, and composition correctness
 *       of brain_ti_compute_modulation_state / unified_lr / unified_batch /
 *       unified_clip functions.
 * WHY:  The unified pipeline composes arousal, inflammation, instability,
 *       Portia, and stress modulation; we verify each degrades gracefully
 *       with NULL brain and produces bounded, non-negative outputs.
 * HOW:  GTest fixture with bio_router_init(NULL) in SetUp for subsystem
 *       compatibility. All tests use NULL brain (no brain allocation).
 *
 * @author NIMCP Development Team
 * @date 2026-02-26
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/training/nimcp_training_integration.h"
#include "async/nimcp_bio_router.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class TrainingIntegrationUnifiedTest : public ::testing::Test {
protected:
    void SetUp() override {
        bio_router_init(NULL);
    }

    void TearDown() override {}
};

// =============================================================================
// NULL brain returns base values unchanged
// =============================================================================

TEST_F(TrainingIntegrationUnifiedTest, NullBrainReturnsBase) {
    float lr = brain_ti_compute_unified_lr(NULL, 0.01f, NULL);
    EXPECT_FLOAT_EQ(lr, 0.01f)
        << "NULL brain should return base_lr unchanged";
}

TEST_F(TrainingIntegrationUnifiedTest, NullBrainBatchReturnsBase) {
    float batch = brain_ti_compute_unified_batch(NULL, 32.0f);
    EXPECT_FLOAT_EQ(batch, 32.0f)
        << "NULL brain should return base_batch unchanged";
}

TEST_F(TrainingIntegrationUnifiedTest, NullBrainClipReturnsBase) {
    float clip = brain_ti_compute_unified_clip(NULL, 1.0f);
    EXPECT_FLOAT_EQ(clip, 1.0f)
        << "NULL brain should return base_clip unchanged";
}

// =============================================================================
// Modulation state with NULL brain
// =============================================================================

TEST_F(TrainingIntegrationUnifiedTest, NullBrainModulationState) {
    brain_ti_modulation_state_t state;
    memset(&state, 0xFF, sizeof(state));  // Poison

    int rc = brain_ti_compute_modulation_state(NULL, &state);
    EXPECT_EQ(rc, 0) << "NULL brain should succeed with defaults";

    EXPECT_FLOAT_EQ(state.final_lr_factor, 1.0f)
        << "NULL brain final_lr_factor should be 1.0";
    EXPECT_FLOAT_EQ(state.final_batch_factor, 1.0f)
        << "NULL brain final_batch_factor should be 1.0";
    EXPECT_FLOAT_EQ(state.final_clip_factor, 1.0f)
        << "NULL brain final_clip_factor should be 1.0";
    EXPECT_FALSE(state.should_pause)
        << "NULL brain should not signal pause";
}

// =============================================================================
// All fields populated with sensible defaults
// =============================================================================

TEST_F(TrainingIntegrationUnifiedTest, ModulationStateFields) {
    brain_ti_modulation_state_t state;
    memset(&state, 0xFF, sizeof(state));

    int rc = brain_ti_compute_modulation_state(NULL, &state);
    EXPECT_EQ(rc, 0);

    // Individual module outputs should be sensible defaults
    EXPECT_FLOAT_EQ(state.arousal_level, 0.5f);
    EXPECT_FLOAT_EQ(state.arousal_cognitive_gain, 1.0f);
    EXPECT_FLOAT_EQ(state.arousal_memory_consolidation, 1.0f);
    EXPECT_FLOAT_EQ(state.circadian_efficiency, 1.0f);
    EXPECT_FLOAT_EQ(state.rpe_bonus, 0.0f);
    EXPECT_FLOAT_EQ(state.inflammation_learning_factor, 1.0f);
    EXPECT_FLOAT_EQ(state.inflammation_precision, 1.0f);
    EXPECT_FLOAT_EQ(state.instability_lr_scale, 1.0f);
    EXPECT_FLOAT_EQ(state.instability_batch_scale, 1.0f);
    EXPECT_FLOAT_EQ(state.instability_clip_factor, 1.0f);
    EXPECT_FLOAT_EQ(state.portia_learning_gate, 1.0f);
    EXPECT_FLOAT_EQ(state.portia_compute_budget, 1.0f);
    EXPECT_FLOAT_EQ(state.stress_level, 0.0f);
    EXPECT_FLOAT_EQ(state.cognitive_capacity, 1.0f);
    EXPECT_FLOAT_EQ(state.conflict_level, 0.0f);
}

// =============================================================================
// Bounds checking
// =============================================================================

TEST_F(TrainingIntegrationUnifiedTest, FinalLrFactorBounded) {
    brain_ti_modulation_state_t state;
    int rc = brain_ti_compute_modulation_state(NULL, &state);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(state.final_lr_factor, 0.001f)
        << "final_lr_factor should be at least 0.001";
    EXPECT_LE(state.final_lr_factor, 10.0f)
        << "final_lr_factor should be at most 10.0";
}

TEST_F(TrainingIntegrationUnifiedTest, FinalBatchFactorBounded) {
    brain_ti_modulation_state_t state;
    int rc = brain_ti_compute_modulation_state(NULL, &state);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(state.final_batch_factor, 0.01f)
        << "final_batch_factor should be at least 0.01";
    EXPECT_LE(state.final_batch_factor, 2.0f)
        << "final_batch_factor should be at most 2.0";
}

// =============================================================================
// Pause behavior
// =============================================================================

TEST_F(TrainingIntegrationUnifiedTest, ShouldPauseDefault) {
    brain_ti_modulation_state_t state;
    int rc = brain_ti_compute_modulation_state(NULL, &state);
    EXPECT_EQ(rc, 0);
    EXPECT_FALSE(state.should_pause)
        << "Default modulation should not signal pause";
}

// =============================================================================
// Output safety
// =============================================================================

TEST_F(TrainingIntegrationUnifiedTest, UnifiedLrNeverNegative) {
    float lr = brain_ti_compute_unified_lr(NULL, 0.01f, NULL);
    EXPECT_GE(lr, 0.0f) << "Unified LR must never be negative";
}

TEST_F(TrainingIntegrationUnifiedTest, UnifiedLrNeverZero) {
    float lr = brain_ti_compute_unified_lr(NULL, 0.01f, NULL);
    EXPECT_GT(lr, 0.0f) << "Unified LR must be greater than zero";
}

// =============================================================================
// Optional state parameter
// =============================================================================

TEST_F(TrainingIntegrationUnifiedTest, StateNullCheck) {
    // Passing NULL for the optional state param should still work
    float lr = brain_ti_compute_unified_lr(NULL, 0.05f, NULL);
    EXPECT_FLOAT_EQ(lr, 0.05f)
        << "NULL state pointer should not cause crash, returns base_lr";
}

// =============================================================================
// Backward compatibility
// =============================================================================

TEST_F(TrainingIntegrationUnifiedTest, OldApiFunctionStillExists) {
    // Verify the old API is still callable and returns base_lr for NULL brain
    float lr = brain_ti_compute_adaptive_lr(NULL, 0.01f);
    EXPECT_FLOAT_EQ(lr, 0.01f)
        << "Old brain_ti_compute_adaptive_lr must still exist and work";
}
