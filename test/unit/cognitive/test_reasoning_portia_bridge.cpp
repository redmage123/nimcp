/**
 * @file test_reasoning_portia_bridge.cpp
 * @brief Unit tests for the Portia-Reasoning bridge
 *
 * WHAT: Tests reasoning budget computation, application, and summary in isolation
 * WHY:  Verify budget logic independently of Portia's actual runtime state.
 *       In unit tests Portia is NOT initialized, so compute_budget() returns a
 *       full budget and should_skip() returns false — that's the expected behavior.
 * HOW:  GTest fixture exercising each function with crafted budgets
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

class ReasoningPortiaBridgeUnit : public ::testing::Test {
protected:
    reasoning_engine_config_t config;
    reasoning_budget_t budget;

    void SetUp() override {
        config = reasoning_engine_default_config();
        budget = reasoning_portia_full_budget();
    }

    // Helper: create a default config with all flags explicitly enabled
    reasoning_engine_config_t make_all_enabled_config() {
        reasoning_engine_config_t c = reasoning_engine_default_config();
        c.enable_engram_recall        = true;
        c.enable_knowledge_query      = true;
        c.enable_predictive_verify    = true;
        c.enable_epistemic_check      = true;
        c.enable_analogical           = true;
        c.enable_working_memory       = true;
        c.enable_world_model          = true;
        c.enable_jepa_prediction      = true;
        c.enable_symbolic_logic       = true;
        c.enable_concurrent_pipeline  = true;
        return c;
    }
};

// =============================================================================
// Tests: Full Budget
// =============================================================================

TEST_F(ReasoningPortiaBridgeUnit, FullBudgetHasAllEnabled) {
    reasoning_budget_t fb = reasoning_portia_full_budget();

    EXPECT_TRUE(fb.allow_recall);
    EXPECT_TRUE(fb.allow_knowledge);
    EXPECT_TRUE(fb.allow_world_model);
    EXPECT_TRUE(fb.allow_jepa);
    EXPECT_TRUE(fb.allow_symbolic_inference);
    EXPECT_TRUE(fb.allow_symbolic_query);
    EXPECT_TRUE(fb.allow_verification);
    EXPECT_TRUE(fb.allow_epistemic);
    EXPECT_TRUE(fb.allow_concurrent);
    EXPECT_EQ(fb.max_steps_override, 0u);
    EXPECT_FLOAT_EQ(fb.confidence_boost, 0.0f);
    EXPECT_EQ(fb.source_degradation, PORTIA_DEGRADATION_NONE);
}

// =============================================================================
// Tests: Compute Budget Without Portia
// =============================================================================

TEST_F(ReasoningPortiaBridgeUnit, ComputeBudgetWithoutPortiaReturnsFull) {
    // Portia is not initialized in unit tests
    reasoning_budget_t computed = reasoning_portia_compute_budget();
    reasoning_budget_t full = reasoning_portia_full_budget();

    EXPECT_EQ(computed.allow_recall,             full.allow_recall);
    EXPECT_EQ(computed.allow_knowledge,           full.allow_knowledge);
    EXPECT_EQ(computed.allow_world_model,         full.allow_world_model);
    EXPECT_EQ(computed.allow_jepa,                full.allow_jepa);
    EXPECT_EQ(computed.allow_symbolic_inference,  full.allow_symbolic_inference);
    EXPECT_EQ(computed.allow_symbolic_query,      full.allow_symbolic_query);
    EXPECT_EQ(computed.allow_verification,        full.allow_verification);
    EXPECT_EQ(computed.allow_epistemic,           full.allow_epistemic);
    EXPECT_EQ(computed.allow_concurrent,          full.allow_concurrent);
    EXPECT_EQ(computed.max_steps_override,        full.max_steps_override);
    EXPECT_FLOAT_EQ(computed.confidence_boost,    full.confidence_boost);
    EXPECT_EQ(computed.source_degradation,        full.source_degradation);
}

// =============================================================================
// Tests: Should Skip
// =============================================================================

TEST_F(ReasoningPortiaBridgeUnit, ShouldSkipWithoutPortiaReturnsFalse) {
    // Portia not initialized => safe default is "don't skip"
    EXPECT_FALSE(reasoning_portia_should_skip());
}

// =============================================================================
// Tests: Apply Budget - Error Handling
// =============================================================================

TEST_F(ReasoningPortiaBridgeUnit, ApplyBudgetNullConfigReturnsError) {
    EXPECT_EQ(reasoning_portia_apply_budget(nullptr, &budget), -1);
}

TEST_F(ReasoningPortiaBridgeUnit, ApplyBudgetNullBudgetReturnsError) {
    EXPECT_EQ(reasoning_portia_apply_budget(&config, nullptr), -1);
}

// =============================================================================
// Tests: Apply Full Budget
// =============================================================================

TEST_F(ReasoningPortiaBridgeUnit, ApplyFullBudgetDisablesNothing) {
    config = make_all_enabled_config();
    int disabled = reasoning_portia_apply_budget(&config, &budget);
    EXPECT_EQ(disabled, 0);

    // All enable flags must still be true
    EXPECT_TRUE(config.enable_engram_recall);
    EXPECT_TRUE(config.enable_knowledge_query);
    EXPECT_TRUE(config.enable_predictive_verify);
    EXPECT_TRUE(config.enable_epistemic_check);
    EXPECT_TRUE(config.enable_world_model);
    EXPECT_TRUE(config.enable_jepa_prediction);
    EXPECT_TRUE(config.enable_symbolic_logic);
    EXPECT_TRUE(config.enable_concurrent_pipeline);
}

// =============================================================================
// Tests: Apply Budget - MINOR degradation (JEPA + epistemic disabled)
// =============================================================================

TEST_F(ReasoningPortiaBridgeUnit, ApplyBudgetDisablesJepaAndEpistemic) {
    budget.allow_jepa      = false;
    budget.allow_epistemic = false;

    config = make_all_enabled_config();
    int disabled = reasoning_portia_apply_budget(&config, &budget);

    EXPECT_EQ(disabled, 2);
    EXPECT_FALSE(config.enable_jepa_prediction);
    EXPECT_FALSE(config.enable_epistemic_check);

    // Everything else still enabled
    EXPECT_TRUE(config.enable_engram_recall);
    EXPECT_TRUE(config.enable_knowledge_query);
    EXPECT_TRUE(config.enable_predictive_verify);
    EXPECT_TRUE(config.enable_world_model);
    EXPECT_TRUE(config.enable_symbolic_logic);
    EXPECT_TRUE(config.enable_concurrent_pipeline);
}

// =============================================================================
// Tests: Apply Budget - MODERATE degradation
// =============================================================================

TEST_F(ReasoningPortiaBridgeUnit, ApplyBudgetModerateShedsMultiple) {
    // Simulate MODERATE: jepa, epistemic, world_model, verification,
    // symbolic_inference disabled; concurrent off; steps capped at 20
    budget.allow_jepa               = false;
    budget.allow_epistemic          = false;
    budget.allow_world_model        = false;
    budget.allow_verification       = false;
    budget.allow_symbolic_inference = false;
    budget.allow_concurrent         = false;
    budget.max_steps_override       = 20;

    config = make_all_enabled_config();
    config.max_steps = 50;
    int disabled = reasoning_portia_apply_budget(&config, &budget);

    // jepa(1) + epistemic(1) + world_model(1) + verification(1) + symbolic_inference(1 — counted but flag stays) = 5
    EXPECT_GE(disabled, 4);
    EXPECT_FALSE(config.enable_concurrent_pipeline);
    EXPECT_EQ(config.max_steps, 20u);
    EXPECT_FALSE(config.enable_jepa_prediction);
    EXPECT_FALSE(config.enable_epistemic_check);
    EXPECT_FALSE(config.enable_world_model);
    EXPECT_FALSE(config.enable_predictive_verify);
}

// =============================================================================
// Tests: Apply Budget - SEVERE degradation
// =============================================================================

TEST_F(ReasoningPortiaBridgeUnit, ApplySevereBudget) {
    // SEVERE: only recall + knowledge + symbolic_query enabled
    // (symbolic_inference disabled, but symbolic_query still on)
    budget.allow_recall             = true;
    budget.allow_knowledge          = true;
    budget.allow_world_model        = false;
    budget.allow_jepa               = false;
    budget.allow_symbolic_inference = false;
    budget.allow_symbolic_query     = true;
    budget.allow_verification       = false;
    budget.allow_epistemic          = false;
    budget.allow_concurrent         = false;
    budget.max_steps_override       = 10;
    budget.confidence_boost         = 0.05f;

    config = make_all_enabled_config();
    int disabled = reasoning_portia_apply_budget(&config, &budget);

    // world_model(1) + jepa(1) + sym_inference(1) + verify(1) + epistemic(1) = 5
    EXPECT_EQ(disabled, 5);
    EXPECT_TRUE(config.enable_engram_recall);
    EXPECT_TRUE(config.enable_knowledge_query);
    EXPECT_TRUE(config.enable_symbolic_logic);  // query is still on
    EXPECT_FALSE(config.enable_world_model);
    EXPECT_FALSE(config.enable_jepa_prediction);
    EXPECT_FALSE(config.enable_predictive_verify);
    EXPECT_FALSE(config.enable_epistemic_check);
    EXPECT_FALSE(config.enable_concurrent_pipeline);
    EXPECT_EQ(config.max_steps, 10u);
}

// =============================================================================
// Tests: Apply Budget - EMERGENCY degradation
// =============================================================================

TEST_F(ReasoningPortiaBridgeUnit, ApplyEmergencyBudget) {
    // EMERGENCY: only recall enabled, everything else shed
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
    budget.confidence_boost         = 0.1f;

    config = make_all_enabled_config();
    int disabled = reasoning_portia_apply_budget(&config, &budget);

    // knowledge(1) + world_model(1) + jepa(1) + verify(1) + epistemic(1) + sym_both(2) = 7
    EXPECT_EQ(disabled, 7);
    EXPECT_TRUE(config.enable_engram_recall);
    EXPECT_FALSE(config.enable_knowledge_query);
    EXPECT_FALSE(config.enable_world_model);
    EXPECT_FALSE(config.enable_jepa_prediction);
    EXPECT_FALSE(config.enable_predictive_verify);
    EXPECT_FALSE(config.enable_epistemic_check);
    EXPECT_FALSE(config.enable_symbolic_logic);
    EXPECT_FALSE(config.enable_concurrent_pipeline);
    EXPECT_EQ(config.max_steps, 5u);
}

// =============================================================================
// Tests: Symbolic Logic Subtlety
// =============================================================================

TEST_F(ReasoningPortiaBridgeUnit, ApplyBudgetSymbolicLogicOnlyDisabledWhenBothFalse) {
    // Case 1: only inference denied, query still allowed
    budget.allow_symbolic_query     = true;
    budget.allow_symbolic_inference = false;

    config = make_all_enabled_config();
    int disabled = reasoning_portia_apply_budget(&config, &budget);

    // Inference counted as disabled, but enable_symbolic_logic stays true
    EXPECT_EQ(disabled, 1);
    EXPECT_TRUE(config.enable_symbolic_logic);

    // Case 2: only query denied, inference still allowed
    budget = reasoning_portia_full_budget();
    budget.allow_symbolic_query     = false;
    budget.allow_symbolic_inference = true;

    config = make_all_enabled_config();
    disabled = reasoning_portia_apply_budget(&config, &budget);

    EXPECT_EQ(disabled, 1);
    EXPECT_TRUE(config.enable_symbolic_logic);

    // Case 3: both denied => enable_symbolic_logic = false
    budget = reasoning_portia_full_budget();
    budget.allow_symbolic_query     = false;
    budget.allow_symbolic_inference = false;

    config = make_all_enabled_config();
    disabled = reasoning_portia_apply_budget(&config, &budget);

    EXPECT_EQ(disabled, 2);
    EXPECT_FALSE(config.enable_symbolic_logic);
}

// =============================================================================
// Tests: Budget Summary
// =============================================================================

TEST_F(ReasoningPortiaBridgeUnit, BudgetSummaryFormatsCorrectly) {
    char buffer[512];
    int written = reasoning_portia_budget_summary(&budget, buffer, sizeof(buffer));

    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(buffer, "NONE"), nullptr);
    EXPECT_NE(strstr(buffer, "concurrent=yes"), nullptr);
    EXPECT_NE(strstr(buffer, "recall=yes"), nullptr);
    EXPECT_NE(strstr(buffer, "knowledge=yes"), nullptr);
    EXPECT_NE(strstr(buffer, "jepa=yes"), nullptr);
    EXPECT_NE(strstr(buffer, "epistemic=yes"), nullptr);
}

TEST_F(ReasoningPortiaBridgeUnit, BudgetSummaryNullReturnsError) {
    char buffer[128];
    EXPECT_EQ(reasoning_portia_budget_summary(nullptr, buffer, sizeof(buffer)), -1);
    EXPECT_EQ(reasoning_portia_budget_summary(&budget, nullptr, sizeof(buffer)), -1);
    EXPECT_EQ(reasoning_portia_budget_summary(&budget, buffer, 0), -1);
}

TEST_F(ReasoningPortiaBridgeUnit, BudgetSummarySevereShowsDisabled) {
    // Create a severe budget with many phases disabled
    budget.allow_jepa      = false;
    budget.allow_epistemic = false;
    budget.allow_concurrent = false;
    budget.source_degradation = PORTIA_DEGRADATION_SEVERE;

    char buffer[512];
    int written = reasoning_portia_budget_summary(&budget, buffer, sizeof(buffer));
    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(buffer, "SEVERE"), nullptr);
    EXPECT_NE(strstr(buffer, "concurrent=no"), nullptr);
    EXPECT_NE(strstr(buffer, "jepa=no"), nullptr);
    EXPECT_NE(strstr(buffer, "epistemic=no"), nullptr);
}

// =============================================================================
// Tests: Steps Override
// =============================================================================

TEST_F(ReasoningPortiaBridgeUnit, StepsOverrideApplied) {
    budget.max_steps_override = 15;
    config.max_steps = 50;

    int disabled = reasoning_portia_apply_budget(&config, &budget);
    EXPECT_GE(disabled, 0);
    EXPECT_EQ(config.max_steps, 15u);
}

TEST_F(ReasoningPortiaBridgeUnit, StepsOverrideZeroKeepsDefault) {
    budget.max_steps_override = 0;
    config.max_steps = 50;

    int disabled = reasoning_portia_apply_budget(&config, &budget);
    EXPECT_GE(disabled, 0);
    EXPECT_EQ(config.max_steps, 50u);
}

// =============================================================================
// Tests: Training Integration Wrappers
// =============================================================================

TEST_F(ReasoningPortiaBridgeUnit, TrainingIntegrationShouldSkipReturnsFalse) {
    // Portia not initialized => delegates to reasoning_portia_should_skip() => false
    EXPECT_FALSE(brain_ti_should_skip_reasoning());
}

TEST_F(ReasoningPortiaBridgeUnit, TrainingIntegrationDegradationReturnsZero) {
    // Portia not initialized => compute_budget returns full => NONE = 0
    EXPECT_EQ(brain_ti_get_reasoning_degradation(), 0);
}

TEST_F(ReasoningPortiaBridgeUnit, TrainingIntegrationPhasesDisabledReturnsZero) {
    // Portia not initialized => full budget => apply disables 0 phases
    EXPECT_EQ(brain_ti_get_reasoning_phases_disabled(), 0);
}
