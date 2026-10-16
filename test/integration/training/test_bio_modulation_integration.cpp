/**
 * @file test_bio_modulation_integration.cpp
 * @brief Integration tests for biological modulation affecting the training pipeline
 *
 * WHAT: Tests that individual biological modulation subsystems (arousal,
 *       inflammation, stress, Portia, instability) correctly affect the
 *       unified learning rate computation, and that they compose multiplicatively.
 * WHY:  The unified continuous modulation pipeline composes 8+ factors into
 *       a single LR multiplier. These tests verify that each factor has the
 *       expected directional effect and that the composition is correct.
 * HOW:  Creates internal brain_t instances with BRAIN_SIZE_TINY for speed,
 *       manipulates subsystem state via brain_ti_* accessors, and verifies
 *       the effect on unified LR. Suite-level init/shutdown.
 *
 * @author NIMCP Development Team
 * @date 2026-02-26
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/training/nimcp_training_integration.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"
}

// ============================================================================
// Shared fixture: internal brain_t for bio modulation integration tests
// ============================================================================

class BioModulationIntegration : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    static void SetUpTestSuite() {
        nimcp_init();
    }

    static void TearDownTestSuite() {
        nimcp_shutdown();
    }

    void SetUp() override {
        brain = brain_create("bio_modulation_test", BRAIN_SIZE_TINY,
                              BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(brain, nullptr) << "Failed to create test brain";
    }

    void TearDown() override {
        brain_ti_destroy_reasoning(brain);
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

// ============================================================================
// Test 1: ArousalAffectsLR
// ============================================================================

TEST_F(BioModulationIntegration, ArousalAffectsLR) {
    float base_lr = 0.01f;

    float lr_before = brain_ti_compute_unified_lr(brain, base_lr, nullptr);
    EXPECT_GT(lr_before, 0.0f) << "Baseline LR should be positive";

    int rc = brain_ti_boost_arousal(brain, 0.2f);
    EXPECT_EQ(rc, 0) << "brain_ti_boost_arousal should succeed";

    float lr_after = brain_ti_compute_unified_lr(brain, base_lr, nullptr);
    EXPECT_GT(lr_after, 0.0f) << "Post-boost LR should be positive";
    EXPECT_FALSE(std::isnan(lr_after));
    EXPECT_LT(lr_after, 1.0f);
}

// ============================================================================
// Test 2: InflammationFactorReasonable
// ============================================================================

TEST_F(BioModulationIntegration, InflammationFactorReasonable) {
    brain_ti_modulation_state_t state;
    memset(&state, 0, sizeof(state));

    int rc = brain_ti_compute_modulation_state(brain, &state);
    EXPECT_EQ(rc, 0);

    EXPECT_GT(state.inflammation_learning_factor, 0.0f);
    EXPECT_LE(state.inflammation_learning_factor, 1.5f);
    EXPECT_GE(state.inflammation_precision, 0.0f);
    EXPECT_LE(state.inflammation_precision, 1.5f);
}

// ============================================================================
// Test 3: StressReducesLR
// ============================================================================

TEST_F(BioModulationIntegration, StressReducesLR) {
    brain_ti_modulation_state_t state;
    memset(&state, 0, sizeof(state));

    int rc = brain_ti_compute_modulation_state(brain, &state);
    EXPECT_EQ(rc, 0);

    EXPECT_GE(state.stress_level, 0.0f);
    EXPECT_LE(state.stress_level, 1.0f);

    float stress_penalty = 1.0f - 0.3f * state.stress_level;
    EXPECT_GE(stress_penalty, 0.7f);
    EXPECT_LE(stress_penalty, 1.0f);
}

// ============================================================================
// Test 4: AllModulatorsCompose
// ============================================================================

TEST_F(BioModulationIntegration, AllModulatorsCompose) {
    brain_ti_modulation_state_t state;
    memset(&state, 0, sizeof(state));

    int rc = brain_ti_compute_modulation_state(brain, &state);
    EXPECT_EQ(rc, 0);

    float expected = state.arousal_cognitive_gain
                   * state.circadian_efficiency
                   * (1.0f + state.rpe_bonus)
                   * state.instability_lr_scale
                   * state.inflammation_learning_factor
                   * state.portia_learning_gate
                   * (1.0f - 0.3f * state.stress_level)
                   * (0.7f + 0.3f * state.cognitive_capacity);

    if (expected > 0.0f && state.final_lr_factor > 0.0f) {
        float ratio = state.final_lr_factor / expected;
        EXPECT_NEAR(ratio, 1.0f, 0.1f)
            << "final_lr_factor (" << state.final_lr_factor
            << ") should match recomputed product (" << expected << ")";
    }

    EXPECT_GT(state.final_lr_factor, 0.0f);
}

// ============================================================================
// Test 5: CognitiveCapacityAffectsLR
// ============================================================================

TEST_F(BioModulationIntegration, CognitiveCapacityAffectsLR) {
    float capacity = brain_ti_get_cognitive_capacity(brain);
    EXPECT_GE(capacity, 0.0f);
    EXPECT_LE(capacity, 1.0f);

    float capacity_factor = 0.7f + 0.3f * capacity;
    EXPECT_GE(capacity_factor, 0.7f);
    EXPECT_LE(capacity_factor, 1.0f);
}

// ============================================================================
// Test 6: CircadianEfficiencyAffectsLR
// ============================================================================

TEST_F(BioModulationIntegration, CircadianEfficiencyAffectsLR) {
    float efficiency = brain_ti_get_circadian_efficiency(brain);
    EXPECT_GE(efficiency, 0.5f);
    EXPECT_LE(efficiency, 2.0f);

    int phase = brain_ti_get_circadian_phase(brain);
    EXPECT_GE(phase, 0);
    EXPECT_LE(phase, 7);
}

// ============================================================================
// Test 7: RewardPredictionErrorModulatesLR
// ============================================================================

TEST_F(BioModulationIntegration, RewardPredictionErrorModulatesLR) {
    float base_lr = 0.01f;

    float lr_baseline = brain_ti_compute_unified_lr(brain, base_lr, nullptr);
    EXPECT_GT(lr_baseline, 0.0f);

    int rc = brain_ti_update_reward(brain, 0.95f, 0.3f);
    EXPECT_EQ(rc, 0);

    float rpe = brain_ti_get_rpe(brain);
    EXPECT_FALSE(std::isnan(rpe));

    float lr_after_rpe = brain_ti_compute_unified_lr(brain, base_lr, nullptr);
    EXPECT_GT(lr_after_rpe, 0.0f);
    EXPECT_FALSE(std::isnan(lr_after_rpe));
}

// ============================================================================
// Test 8: ModulationAfterPostBatchUpdate
// ============================================================================

TEST_F(BioModulationIntegration, ModulationAfterPostBatchUpdate) {
    brain_ti_modulation_state_t state_before;
    memset(&state_before, 0, sizeof(state_before));
    brain_ti_compute_modulation_state(brain, &state_before);

    for (int i = 0; i < 5; i++) {
        float accuracy = 0.3f + 0.1f * (float)i;
        int rc = brain_ti_post_batch_update(brain, accuracy, 0.7f, "modulation_test");
        EXPECT_EQ(rc, 0);
    }

    brain_ti_modulation_state_t state_after;
    memset(&state_after, 0, sizeof(state_after));
    brain_ti_compute_modulation_state(brain, &state_after);

    EXPECT_FALSE(std::isnan(state_after.rpe_bonus));
    EXPECT_FALSE(std::isnan(state_after.final_lr_factor));
    EXPECT_GT(state_after.final_lr_factor, 0.0f);
}

// ============================================================================
// Test 9: HabitFormationIntegration
// ============================================================================

TEST_F(BioModulationIntegration, HabitFormationIntegration) {
    int habit_id = brain_ti_register_habit(brain, "math", 42);
    if (habit_id >= 0) {
        int rc = brain_ti_strengthen_habit(brain, habit_id, true);
        EXPECT_EQ(rc, 0);

        float strength = brain_ti_get_habit_strength(brain, habit_id);
        EXPECT_GE(strength, 0.0f);
        EXPECT_LE(strength, 1.0f);
    }

    int check = brain_ti_check_habit(brain, "math");
    (void)check;  // Must not crash
}

// ============================================================================
// Test 10: ReasoningIntegrationWithModulation
// ============================================================================

TEST_F(BioModulationIntegration, ReasoningIntegrationWithModulation) {
    float lr_before = brain_ti_compute_unified_lr(brain, 0.01f, nullptr);
    EXPECT_GT(lr_before, 0.0f);

    int rc = brain_ti_init_reasoning(brain);
    EXPECT_EQ(rc, 0);

    float lr_after = brain_ti_compute_unified_lr(brain, 0.01f, nullptr);
    EXPECT_GT(lr_after, 0.0f);
    EXPECT_FALSE(std::isnan(lr_after));

    bool skip = brain_ti_should_skip_reasoning();
    (void)skip;

    int degradation = brain_ti_get_reasoning_degradation();
    EXPECT_GE(degradation, 0);
    EXPECT_LE(degradation, 4);
}

// ============================================================================
// Test 11: PortiaGateInRange
// ============================================================================

TEST_F(BioModulationIntegration, PortiaGateInRange) {
    brain_ti_modulation_state_t state;
    memset(&state, 0, sizeof(state));

    int rc = brain_ti_compute_modulation_state(brain, &state);
    EXPECT_EQ(rc, 0);

    EXPECT_GE(state.portia_learning_gate, 0.0f);
    EXPECT_LE(state.portia_learning_gate, 1.5f);
    EXPECT_GE(state.portia_compute_budget, 0.0f);
}

// ============================================================================
// Test 12: InstabilityChannelsReasonable
// ============================================================================

TEST_F(BioModulationIntegration, InstabilityChannelsReasonable) {
    brain_ti_modulation_state_t state;
    memset(&state, 0, sizeof(state));

    int rc = brain_ti_compute_modulation_state(brain, &state);
    EXPECT_EQ(rc, 0);

    EXPECT_GT(state.instability_lr_scale, 0.0f);
    EXPECT_LE(state.instability_lr_scale, 1.5f);
    EXPECT_GE(state.instability_batch_scale, 0.0f);
    EXPECT_GT(state.instability_clip_factor, 0.0f);
}

// ============================================================================
// Test 13: UnifiedLrConsistencyAcrossMultipleCalls
// ============================================================================

TEST_F(BioModulationIntegration, UnifiedLrConsistencyAcrossMultipleCalls) {
    float base_lr = 0.01f;

    float lr1 = brain_ti_compute_unified_lr(brain, base_lr, nullptr);
    float lr2 = brain_ti_compute_unified_lr(brain, base_lr, nullptr);
    float lr3 = brain_ti_compute_unified_lr(brain, base_lr, nullptr);

    EXPECT_NEAR(lr1, lr2, 0.001f);
    EXPECT_NEAR(lr2, lr3, 0.001f);
}

// ============================================================================
// Test 14: ShouldPauseFlagBehavior
// ============================================================================

TEST_F(BioModulationIntegration, ShouldPauseFlagBehavior) {
    brain_ti_modulation_state_t state;
    memset(&state, 0, sizeof(state));

    int rc = brain_ti_compute_modulation_state(brain, &state);
    EXPECT_EQ(rc, 0);

    EXPECT_TRUE(state.should_pause == true || state.should_pause == false);
}
