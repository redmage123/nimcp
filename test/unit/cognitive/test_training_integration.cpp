/**
 * @file test_training_integration.cpp
 * @brief Unit tests for the cognitive training integration facade
 *
 * WHAT: Tests NULL-safety and default-return behaviour of every brain_ti_*
 *       function, plus input-validation edge cases and struct zero-init.
 * WHY:  The training integration layer is a thin delegation facade; its
 *       primary unit-level contract is that every function degrades
 *       gracefully when handed a NULL brain or NULL string arguments.
 * HOW:  GTest fixture with no brain dependency -- exercises the API with
 *       NULL pointers and verifies return values / defaults.
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/training/nimcp_training_integration.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class TrainingIntegrationUnit : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// =============================================================================
// NULL-safety: Reward / RPE / Dopamine
// =============================================================================

TEST_F(TrainingIntegrationUnit, NullBrainRewardReturnsError) {
    int rc = brain_ti_update_reward(NULL, 0.5f, 0.5f);
    EXPECT_EQ(rc, -1);
}

TEST_F(TrainingIntegrationUnit, NullBrainConflictReturnsDefault) {
    float conflict = brain_ti_get_conflict(NULL);
    EXPECT_FLOAT_EQ(conflict, 0.0f);
}

TEST_F(TrainingIntegrationUnit, NullBrainModeReturnsDefault) {
    int mode = brain_ti_get_mode(NULL);
    EXPECT_EQ(mode, 0);  // 0 = goal-directed
}

TEST_F(TrainingIntegrationUnit, NullBrainDopamineReturnsDefault) {
    float dopamine = brain_ti_get_dopamine(NULL);
    EXPECT_FLOAT_EQ(dopamine, 0.5f);
}

TEST_F(TrainingIntegrationUnit, NullBrainRpeReturnsDefault) {
    float rpe = brain_ti_get_rpe(NULL);
    EXPECT_FLOAT_EQ(rpe, 0.0f);
}

// =============================================================================
// NULL-safety: Habit Subsystem
// =============================================================================

TEST_F(TrainingIntegrationUnit, NullBrainHabitRegisterReturnsError) {
    int rc = brain_ti_register_habit(NULL, "test", 0);
    EXPECT_EQ(rc, -1);
}

TEST_F(TrainingIntegrationUnit, NullBrainHabitCheckReturnsError) {
    int rc = brain_ti_check_habit(NULL, "test");
    EXPECT_EQ(rc, -1);
}

// =============================================================================
// NULL-safety: Arousal / Circadian
// =============================================================================

TEST_F(TrainingIntegrationUnit, NullBrainArousalReturnsDefault) {
    float arousal = brain_ti_get_arousal(NULL);
    EXPECT_FLOAT_EQ(arousal, 0.5f);
}

TEST_F(TrainingIntegrationUnit, NullBrainCircadianReturnsDefault) {
    int phase = brain_ti_get_circadian_phase(NULL);
    EXPECT_EQ(phase, 0);
}

TEST_F(TrainingIntegrationUnit, NullBrainBoostArousalReturnsError) {
    int rc = brain_ti_boost_arousal(NULL, 0.1f);
    EXPECT_EQ(rc, -1);
}

TEST_F(TrainingIntegrationUnit, NullBrainCircadianEfficiencyReturnsDefault) {
    float eff = brain_ti_get_circadian_efficiency(NULL);
    EXPECT_FLOAT_EQ(eff, 1.0f);
}

// =============================================================================
// NULL-safety: Symbolic-Logic Knowledge Base
// =============================================================================

TEST_F(TrainingIntegrationUnit, NullBrainAddFactReturnsFalse) {
    bool ok = brain_ti_add_fact(NULL, "test", 0.5f);
    EXPECT_FALSE(ok);
}

TEST_F(TrainingIntegrationUnit, NullBrainAddRuleReturnsFalse) {
    bool ok = brain_ti_add_rule(NULL, "test", 0.5f);
    EXPECT_FALSE(ok);
}

TEST_F(TrainingIntegrationUnit, NullBrainForwardChainReturnsZero) {
    int rc = brain_ti_forward_chain(NULL, 10);
    EXPECT_EQ(rc, 0);  // Returns 0 (no facts derived) when unavailable
}

TEST_F(TrainingIntegrationUnit, NullBrainBackwardChainReturnsError) {
    float conf = brain_ti_backward_chain(NULL, "test");
    EXPECT_FLOAT_EQ(conf, -1.0f);
}

TEST_F(TrainingIntegrationUnit, NullBrainQueryReturnsZero) {
    int rc = brain_ti_query_knowledge(NULL, "test");
    EXPECT_EQ(rc, 0);  // Returns 0 (no matches) when unavailable
}

TEST_F(TrainingIntegrationUnit, NullBrainLogicStatsReturnsError) {
    brain_ti_logic_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int rc = brain_ti_get_logic_stats(NULL, &stats);
    EXPECT_EQ(rc, -1);
}

// =============================================================================
// NULL-safety: Reasoning Subsystem
// =============================================================================

TEST_F(TrainingIntegrationUnit, NullBrainInitReasoningReturnsError) {
    int rc = brain_ti_init_reasoning(NULL);
    EXPECT_EQ(rc, -1);
}

TEST_F(TrainingIntegrationUnit, NullBrainReasonReturnsError) {
    float conf = brain_ti_reason(NULL, "test");
    EXPECT_FLOAT_EQ(conf, -1.0f);
}

// =============================================================================
// NULL-safety: Adaptive Training Helpers
// =============================================================================

TEST_F(TrainingIntegrationUnit, NullBrainAdaptiveLrReturnsBase) {
    float lr = brain_ti_compute_adaptive_lr(NULL, 0.5f);
    EXPECT_FLOAT_EQ(lr, 0.5f);  // passthrough when brain is NULL
}

TEST_F(TrainingIntegrationUnit, NullBrainPostBatchReturnsError) {
    int rc = brain_ti_post_batch_update(NULL, 0.5f, 0.5f, "test");
    EXPECT_EQ(rc, -1);
}

// =============================================================================
// Input Validation (NULL strings with NULL brain)
// =============================================================================

TEST_F(TrainingIntegrationUnit, NullDomainHabitReturnsError) {
    int rc = brain_ti_register_habit(NULL, NULL, 0);
    EXPECT_EQ(rc, -1);
}

TEST_F(TrainingIntegrationUnit, NullQueryReasonReturnsError) {
    float conf = brain_ti_reason(NULL, NULL);
    EXPECT_FLOAT_EQ(conf, -1.0f);
}

TEST_F(TrainingIntegrationUnit, NullFactAddReturnsFalse) {
    bool ok = brain_ti_add_fact(NULL, NULL, 0.5f);
    EXPECT_FALSE(ok);
}

// =============================================================================
// Struct / Stats
// =============================================================================

TEST_F(TrainingIntegrationUnit, LogicStatsStructZeroInit) {
    brain_ti_logic_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    EXPECT_EQ(stats.total_facts, 0u);
    EXPECT_EQ(stats.total_rules, 0u);
    EXPECT_EQ(stats.facts_derived, 0u);
    EXPECT_EQ(stats.proofs_completed, 0u);
    EXPECT_EQ(stats.proofs_failed, 0u);
}
