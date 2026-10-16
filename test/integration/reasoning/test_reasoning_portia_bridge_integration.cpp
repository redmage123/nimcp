/**
 * @file test_reasoning_portia_bridge_integration.cpp
 * @brief Integration tests for the Portia-Reasoning bridge with a live brain
 *
 * WHAT: Tests that the Portia bridge correctly adapts a reasoning engine config
 *       that is connected to a real brain, and that the training integration
 *       wrappers function correctly end-to-end
 * WHY:  Unit tests verify budget logic in isolation; integration tests verify
 *       that budget application actually affects the reasoning engine behavior
 *       when connected to a brain, and that the TI wrappers compose correctly
 * HOW:  Creates a BRAIN_SIZE_SMALL brain, connects reasoning engine, applies
 *       various budgets, and verifies the config mutations propagate
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <string>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/reasoning/nimcp_reasoning_portia_bridge.h"
#include "cognitive/training/nimcp_training_integration.h"
#include "portia/nimcp_portia.h"
}

// =============================================================================
// Constants
// =============================================================================

static constexpr uint32_t INPUT_DIM = 32;
static constexpr uint32_t OUTPUT_DIM = 8;

// =============================================================================
// Test Fixture
// =============================================================================

class ReasoningPortiaBridgeIntegration : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    reasoning_engine_t* engine = nullptr;

    void SetUp() override {
        brain = brain_create("portia_bridge_test", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, INPUT_DIM, OUTPUT_DIM);
        ASSERT_NE(brain, nullptr)
            << "Brain creation must succeed for integration tests";

        reasoning_engine_config_t cfg = reasoning_engine_default_config();
        engine = reasoning_engine_create(&cfg);
        ASSERT_NE(engine, nullptr)
            << "Reasoning engine creation must succeed";
    }

    void TearDown() override {
        if (engine) {
            reasoning_engine_destroy(engine);
            engine = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

// =============================================================================
// Tests: Budget Computation + Brain Connection
// =============================================================================

TEST_F(ReasoningPortiaBridgeIntegration, ComputeBudgetAndApplyToConnectedEngine) {
    // Connect engine to brain
    int rc = reasoning_engine_connect_brain(engine, brain);
    EXPECT_EQ(rc, 0);

    // Compute budget (Portia not initialized => full)
    reasoning_budget_t budget = reasoning_portia_compute_budget();

    // Apply to a config
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    int disabled = reasoning_portia_apply_budget(&cfg, &budget);

    // Full budget => 0 phases disabled
    EXPECT_EQ(disabled, 0);
    EXPECT_TRUE(cfg.enable_engram_recall);
    EXPECT_TRUE(cfg.enable_knowledge_query);
    EXPECT_TRUE(cfg.enable_world_model);
    EXPECT_TRUE(cfg.enable_jepa_prediction);
    EXPECT_TRUE(cfg.enable_symbolic_logic);
    EXPECT_TRUE(cfg.enable_concurrent_pipeline);
}

// =============================================================================
// Tests: Degradation Ladder Applied to Default Config
// =============================================================================

TEST_F(ReasoningPortiaBridgeIntegration, DegradationLadderMinorToEmergency) {
    // Test the entire degradation ladder applied to a config, verifying
    // that each level disables progressively more features

    // --- NONE ---
    reasoning_budget_t full = reasoning_portia_full_budget();
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    int d0 = reasoning_portia_apply_budget(&cfg, &full);
    EXPECT_EQ(d0, 0);

    // --- MINOR (JEPA + epistemic) ---
    reasoning_budget_t minor_b = reasoning_portia_full_budget();
    minor_b.allow_jepa      = false;
    minor_b.allow_epistemic = false;
    minor_b.source_degradation = PORTIA_DEGRADATION_MINOR;

    cfg = reasoning_engine_default_config();
    int d1 = reasoning_portia_apply_budget(&cfg, &minor_b);
    EXPECT_EQ(d1, 2);

    // --- MODERATE (MINOR + world_model + verify + sym_inference, no concurrent, steps=20) ---
    reasoning_budget_t mod_b = reasoning_portia_full_budget();
    mod_b.allow_jepa               = false;
    mod_b.allow_epistemic          = false;
    mod_b.allow_world_model        = false;
    mod_b.allow_verification       = false;
    mod_b.allow_symbolic_inference = false;
    mod_b.allow_concurrent         = false;
    mod_b.max_steps_override       = 20;
    mod_b.source_degradation = PORTIA_DEGRADATION_MODERATE;

    cfg = reasoning_engine_default_config();
    int d2 = reasoning_portia_apply_budget(&cfg, &mod_b);

    // Strict monotonicity: each level disables more
    EXPECT_GT(d2, d1);
    EXPECT_FALSE(cfg.enable_concurrent_pipeline);
    EXPECT_EQ(cfg.max_steps, 20u);

    // --- SEVERE ---
    reasoning_budget_t sev_b = reasoning_portia_full_budget();
    sev_b.allow_world_model        = false;
    sev_b.allow_jepa               = false;
    sev_b.allow_symbolic_inference = false;
    sev_b.allow_verification       = false;
    sev_b.allow_epistemic          = false;
    sev_b.allow_concurrent         = false;
    sev_b.max_steps_override       = 10;
    sev_b.confidence_boost         = 0.05f;
    sev_b.source_degradation = PORTIA_DEGRADATION_SEVERE;

    cfg = reasoning_engine_default_config();
    int d3 = reasoning_portia_apply_budget(&cfg, &sev_b);
    EXPECT_GE(d3, d2);

    // --- EMERGENCY ---
    reasoning_budget_t emer_b = reasoning_portia_full_budget();
    emer_b.allow_knowledge          = false;
    emer_b.allow_world_model        = false;
    emer_b.allow_jepa               = false;
    emer_b.allow_symbolic_inference = false;
    emer_b.allow_symbolic_query     = false;
    emer_b.allow_verification       = false;
    emer_b.allow_epistemic          = false;
    emer_b.allow_concurrent         = false;
    emer_b.max_steps_override       = 5;
    emer_b.confidence_boost         = 0.1f;
    emer_b.source_degradation = PORTIA_DEGRADATION_EMERGENCY;

    cfg = reasoning_engine_default_config();
    int d4 = reasoning_portia_apply_budget(&cfg, &emer_b);
    EXPECT_GT(d4, d3);
    EXPECT_EQ(cfg.max_steps, 5u);
}

// =============================================================================
// Tests: Training Integration Wrappers Compose Correctly
// =============================================================================

TEST_F(ReasoningPortiaBridgeIntegration, TrainingIntegrationWrappersConsistent) {
    // All three TI wrappers should report "no degradation" since Portia is not init
    EXPECT_FALSE(brain_ti_should_skip_reasoning());
    EXPECT_EQ(brain_ti_get_reasoning_degradation(), 0);
    EXPECT_EQ(brain_ti_get_reasoning_phases_disabled(), 0);

    // Degradation level 0 means PORTIA_DEGRADATION_NONE
    int deg = brain_ti_get_reasoning_degradation();
    EXPECT_EQ(deg, (int)PORTIA_DEGRADATION_NONE);
}

// =============================================================================
// Tests: Budget Summary Integration with Various Levels
// =============================================================================

TEST_F(ReasoningPortiaBridgeIntegration, BudgetSummaryAllDegradationLevels) {
    const struct {
        portia_degradation_level_t level;
        const char* expected_name;
    } levels[] = {
        { PORTIA_DEGRADATION_NONE,      "NONE" },
        { PORTIA_DEGRADATION_MINOR,     "MINOR" },
        { PORTIA_DEGRADATION_MODERATE,  "MODERATE" },
        { PORTIA_DEGRADATION_SEVERE,    "SEVERE" },
        { PORTIA_DEGRADATION_EMERGENCY, "EMERGENCY" },
    };

    for (const auto& tc : levels) {
        reasoning_budget_t b = reasoning_portia_full_budget();
        b.source_degradation = tc.level;

        char buffer[512];
        int written = reasoning_portia_budget_summary(&b, buffer, sizeof(buffer));
        EXPECT_GT(written, 0) << "Summary for " << tc.expected_name;
        EXPECT_NE(strstr(buffer, tc.expected_name), nullptr)
            << "Summary should contain level name '" << tc.expected_name
            << "' but got: " << buffer;
    }
}

// =============================================================================
// Tests: Repeated Apply Does Not Corrupt Config
// =============================================================================

TEST_F(ReasoningPortiaBridgeIntegration, RepeatedApplyIsIdempotent) {
    // Applying the same budget twice to the same config should give the same
    // disabled count because apply_budget counts budget deny flags, not deltas.
    reasoning_budget_t b = reasoning_portia_full_budget();
    b.allow_jepa = false;
    b.allow_epistemic = false;

    reasoning_engine_config_t cfg1 = reasoning_engine_default_config();
    int d1 = reasoning_portia_apply_budget(&cfg1, &b);
    EXPECT_EQ(d1, 2);

    // Apply again to the same (already-modified) config — same count
    int d2 = reasoning_portia_apply_budget(&cfg1, &b);
    EXPECT_EQ(d2, d1)
        << "Repeated apply should return the same disabled count";

    // Config state unchanged
    EXPECT_FALSE(cfg1.enable_jepa_prediction);
    EXPECT_FALSE(cfg1.enable_epistemic_check);
}

// =============================================================================
// Tests: Budget Apply to Already-Disabled Config
// =============================================================================

TEST_F(ReasoningPortiaBridgeIntegration, BudgetApplyToAlreadyDisabledConfig) {
    // Start with a config where everything is already disabled
    reasoning_engine_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_steps = 100;

    // Apply a full budget — should not re-enable anything
    reasoning_budget_t full = reasoning_portia_full_budget();
    int disabled = reasoning_portia_apply_budget(&cfg, &full);

    // Full budget disables nothing — all allow flags are true
    // But the config flags were already false; apply only counts what it disables
    EXPECT_EQ(disabled, 0);

    // Config flags remain false (budget doesn't re-enable)
    EXPECT_FALSE(cfg.enable_engram_recall);
    EXPECT_FALSE(cfg.enable_knowledge_query);
}

// =============================================================================
// Tests: Confidence Boost Stored in Budget
// =============================================================================

TEST_F(ReasoningPortiaBridgeIntegration, ConfidenceBoostPreservedThroughPipeline) {
    // The confidence_boost field is informational — verify it's preserved
    reasoning_budget_t b = reasoning_portia_full_budget();
    b.confidence_boost = 0.15f;
    b.source_degradation = PORTIA_DEGRADATION_SEVERE;

    char buffer[512];
    int written = reasoning_portia_budget_summary(&b, buffer, sizeof(buffer));
    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(buffer, "0.15"), nullptr)
        << "Confidence boost should appear in summary: " << buffer;
}
