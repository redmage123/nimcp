/**
 * @file test_reasoning_portia_bridge_regression.cpp
 * @brief Regression tests for the Portia-Reasoning bridge
 *
 * WHAT: Guards against specific edge-case bugs in budget computation/application
 * WHY:  Prevent regressions in NULL handling, boundary conditions, buffer
 *       overflow in summary, and symbolic logic dual-flag subtlety
 * HOW:  GTest suite with targeted edge-case scenarios
 *
 * REGRESSIONS GUARDED:
 * - NULL pointer dereference in apply_budget
 * - Buffer overflow in budget_summary with tiny buffer
 * - Symbolic logic flag only disabled when BOTH query+inference denied
 * - max_steps_override=0 must not clobber config.max_steps
 * - Confidence boost field not corrupted during budget copy
 * - Budget summary with extreme/unknown degradation level
 * - apply_budget return value accuracy for each degradation ladder level
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_portia_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/training/nimcp_training_integration.h"
#include "portia/nimcp_portia.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class ReasoningPortiaBridgeRegression : public ::testing::Test {
protected:
    reasoning_engine_config_t config;
    reasoning_budget_t budget;

    void SetUp() override {
        config = reasoning_engine_default_config();
        budget = reasoning_portia_full_budget();
    }
};

// =============================================================================
// Regression: NULL Pointer Safety
// =============================================================================

TEST_F(ReasoningPortiaBridgeRegression, ApplyBudgetBothNullReturnsError) {
    EXPECT_EQ(reasoning_portia_apply_budget(nullptr, nullptr), -1);
}

TEST_F(ReasoningPortiaBridgeRegression, SummaryBothNullReturnsError) {
    EXPECT_EQ(reasoning_portia_budget_summary(nullptr, nullptr, 0), -1);
}

TEST_F(ReasoningPortiaBridgeRegression, SummaryNullBudgetValidBufferReturnsError) {
    char buf[128];
    EXPECT_EQ(reasoning_portia_budget_summary(nullptr, buf, sizeof(buf)), -1);
}

TEST_F(ReasoningPortiaBridgeRegression, SummaryValidBudgetNullBufferReturnsError) {
    EXPECT_EQ(reasoning_portia_budget_summary(&budget, nullptr, 128), -1);
}

TEST_F(ReasoningPortiaBridgeRegression, SummaryZeroSizeReturnsError) {
    char buf[128];
    EXPECT_EQ(reasoning_portia_budget_summary(&budget, buf, 0), -1);
}

// =============================================================================
// Regression: Budget Summary Buffer Boundary
// =============================================================================

TEST_F(ReasoningPortiaBridgeRegression, SummaryTinyBufferDoesNotOverflow) {
    // Buffer of size 1 — snprintf should write at most the NUL terminator
    char buf[1] = { 'X' };
    int written = reasoning_portia_budget_summary(&budget, buf, 1);

    // snprintf returns total characters that would have been written (truncated)
    // The return value should be positive (many chars would be needed)
    EXPECT_GT(written, 0);
    // buf[0] should be NUL (snprintf always NUL-terminates)
    EXPECT_EQ(buf[0], '\0');
}

TEST_F(ReasoningPortiaBridgeRegression, SummarySmallBufferTruncatesGracefully) {
    char buf[16];
    memset(buf, 'X', sizeof(buf));
    int written = reasoning_portia_budget_summary(&budget, buf, sizeof(buf));

    // written = total chars that would have been needed (may exceed buffer_size)
    EXPECT_GT(written, 0);
    // Buffer is NUL-terminated
    EXPECT_EQ(buf[15], '\0');
}

// =============================================================================
// Regression: Symbolic Logic Dual-Flag Subtlety
// =============================================================================

TEST_F(ReasoningPortiaBridgeRegression, SymbolicQueryOnlyDeniedDoesNotDisableFlag) {
    budget.allow_symbolic_query     = false;
    budget.allow_symbolic_inference = true;

    int disabled = reasoning_portia_apply_budget(&config, &budget);
    EXPECT_EQ(disabled, 1);
    EXPECT_TRUE(config.enable_symbolic_logic)
        << "enable_symbolic_logic must stay true when only query is denied";
}

TEST_F(ReasoningPortiaBridgeRegression, SymbolicInferenceOnlyDeniedDoesNotDisableFlag) {
    budget.allow_symbolic_query     = true;
    budget.allow_symbolic_inference = false;

    int disabled = reasoning_portia_apply_budget(&config, &budget);
    EXPECT_EQ(disabled, 1);
    EXPECT_TRUE(config.enable_symbolic_logic)
        << "enable_symbolic_logic must stay true when only inference is denied";
}

TEST_F(ReasoningPortiaBridgeRegression, BothSymbolicDeniedDisablesFlag) {
    budget.allow_symbolic_query     = false;
    budget.allow_symbolic_inference = false;

    int disabled = reasoning_portia_apply_budget(&config, &budget);
    EXPECT_EQ(disabled, 2);
    EXPECT_FALSE(config.enable_symbolic_logic)
        << "enable_symbolic_logic must be false when both query and inference are denied";
}

// =============================================================================
// Regression: max_steps_override = 0 Preserves Default
// =============================================================================

TEST_F(ReasoningPortiaBridgeRegression, ZeroStepsOverridePreservesConfigSteps) {
    budget.max_steps_override = 0;
    uint32_t original_steps = config.max_steps;

    reasoning_portia_apply_budget(&config, &budget);
    EXPECT_EQ(config.max_steps, original_steps)
        << "max_steps_override=0 must not clobber config.max_steps";
}

TEST_F(ReasoningPortiaBridgeRegression, NonZeroStepsOverrideReplacesConfigSteps) {
    budget.max_steps_override = 7;
    config.max_steps = 100;

    reasoning_portia_apply_budget(&config, &budget);
    EXPECT_EQ(config.max_steps, 7u)
        << "max_steps_override=7 must replace config.max_steps";
}

// =============================================================================
// Regression: Confidence Boost Not Corrupted
// =============================================================================

TEST_F(ReasoningPortiaBridgeRegression, ConfidenceBoostPreservedInBudget) {
    reasoning_budget_t b = reasoning_portia_full_budget();
    EXPECT_FLOAT_EQ(b.confidence_boost, 0.0f);

    b.confidence_boost = 0.05f;
    // Copy via value semantics (C struct)
    reasoning_budget_t copy = b;
    EXPECT_FLOAT_EQ(copy.confidence_boost, 0.05f);

    // After summary, the budget should not be modified
    char buf[512];
    reasoning_portia_budget_summary(&b, buf, sizeof(buf));
    EXPECT_FLOAT_EQ(b.confidence_boost, 0.05f)
        << "budget_summary must not modify the budget";
}

// =============================================================================
// Regression: Disabled Count Accuracy at Each Level
// =============================================================================

TEST_F(ReasoningPortiaBridgeRegression, DisabledCountAccurateForFullBudget) {
    int d = reasoning_portia_apply_budget(&config, &budget);
    EXPECT_EQ(d, 0);
}

TEST_F(ReasoningPortiaBridgeRegression, DisabledCountAccurateForEmergencyBudget) {
    budget.allow_recall             = true;
    budget.allow_knowledge          = false;
    budget.allow_world_model        = false;
    budget.allow_jepa               = false;
    budget.allow_symbolic_inference = false;
    budget.allow_symbolic_query     = false;
    budget.allow_verification       = false;
    budget.allow_epistemic          = false;
    budget.allow_concurrent         = false;
    budget.max_steps_override       = 5;

    config = reasoning_engine_default_config();
    int d = reasoning_portia_apply_budget(&config, &budget);

    // knowledge(1) + world_model(1) + jepa(1) + verify(1) + epistemic(1) + sym_both(2) = 7
    EXPECT_EQ(d, 7);
}

// =============================================================================
// Regression: Apply Budget Does Not Enable Flags
// =============================================================================

TEST_F(ReasoningPortiaBridgeRegression, ApplyDoesNotReEnableDisabledFlags) {
    // Start with config where recall is already disabled
    config.enable_engram_recall = false;

    // Full budget allows recall, but apply should NOT re-enable it
    int disabled = reasoning_portia_apply_budget(&config, &budget);
    EXPECT_EQ(disabled, 0);
    EXPECT_FALSE(config.enable_engram_recall)
        << "apply_budget must not re-enable flags that were already disabled";
}

// =============================================================================
// Regression: Concurrent Flag Independent of Phase Count
// =============================================================================

TEST_F(ReasoningPortiaBridgeRegression, ConcurrentDisabledNotCountedInDisabledPhases) {
    // Only disable concurrent, keep all phases enabled
    budget.allow_concurrent = false;

    int disabled = reasoning_portia_apply_budget(&config, &budget);

    // Concurrent is not counted as a "phase" in the disabled count
    EXPECT_EQ(disabled, 0);
    EXPECT_FALSE(config.enable_concurrent_pipeline);
}

// =============================================================================
// Regression: Training Integration Consistency
// =============================================================================

TEST_F(ReasoningPortiaBridgeRegression, TrainingIntegrationDegradationMatchesBudget) {
    // Both paths should agree: direct budget query vs TI wrapper
    reasoning_budget_t b = reasoning_portia_compute_budget();
    int direct_deg = (int)b.source_degradation;
    int ti_deg = brain_ti_get_reasoning_degradation();
    EXPECT_EQ(direct_deg, ti_deg);
}

TEST_F(ReasoningPortiaBridgeRegression, TrainingIntegrationPhasesMatchDirectApply) {
    // Direct path
    reasoning_budget_t b = reasoning_portia_compute_budget();
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    int direct_phases = reasoning_portia_apply_budget(&cfg, &b);

    // TI wrapper path
    int ti_phases = brain_ti_get_reasoning_phases_disabled();

    EXPECT_EQ(direct_phases, ti_phases);
}

TEST_F(ReasoningPortiaBridgeRegression, TrainingIntegrationSkipMatchesDirect) {
    bool direct_skip = reasoning_portia_should_skip();
    bool ti_skip = brain_ti_should_skip_reasoning();
    EXPECT_EQ(direct_skip, ti_skip);
}
