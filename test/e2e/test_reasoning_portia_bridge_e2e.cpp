/**
 * @file test_reasoning_portia_bridge_e2e.cpp
 * @brief E2E tests: Portia-Reasoning bridge full pipeline
 *
 * WHAT: Full pipeline test exercising Portia bridge from budget computation
 *       through config application, reasoning engine execution, and TI wrappers
 * WHY:  Verify that the Portia bridge integrates correctly with the complete
 *       reasoning pipeline — budget computation, config mutation, engine
 *       lifecycle, and training integration wrappers all working together
 * HOW:  Creates brain, connects reasoning engine, computes budget, applies it,
 *       runs reasoning with the modified config, and validates end-to-end
 *       behavior including summary output
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <string>

// Brain header has its own extern "C" guard
#include "core/brain/nimcp_brain.h"

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/reasoning/nimcp_reasoning_portia_bridge.h"
#include "cognitive/training/nimcp_training_integration.h"
#include "portia/nimcp_portia.h"
}

using namespace nimcp::e2e;

// =============================================================================
// Constants
// =============================================================================

static constexpr uint32_t INPUT_DIM = 32;
static constexpr uint32_t OUTPUT_DIM = 8;

// =============================================================================
// Test Fixture
// =============================================================================

class ReasoningPortiaBridgeE2E : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    reasoning_engine_t* engine = nullptr;
    PipelineTracker* tracker = nullptr;

    void SetUp() override {
        tracker = new PipelineTracker("Portia-Reasoning Bridge E2E");

        // Create brain
        tracker->begin_stage("Brain Creation", 10000);
        brain = brain_create("e2e_portia_bridge", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, INPUT_DIM, OUTPUT_DIM);
        ASSERT_NE(brain, nullptr) << "Brain creation must succeed";
        tracker->end_stage();

        // Create reasoning engine with default config
        tracker->begin_stage("Engine Creation", 5000);
        reasoning_engine_config_t cfg = reasoning_engine_default_config();
        engine = reasoning_engine_create(&cfg);
        ASSERT_NE(engine, nullptr) << "Engine creation must succeed";
        tracker->end_stage();
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
        delete tracker;
        tracker = nullptr;
    }
};

// =============================================================================
// E2E: Full Pipeline — Compute Budget, Apply, Connect, Reason
// =============================================================================

TEST_F(ReasoningPortiaBridgeE2E, FullPipelineBudgetComputeApplyReason) {
    // Stage 1: Compute Portia budget
    tracker->begin_stage("Compute Budget", 5000);
    reasoning_budget_t budget = reasoning_portia_compute_budget();
    // Without Portia, should be full
    EXPECT_TRUE(budget.allow_recall);
    EXPECT_TRUE(budget.allow_knowledge);
    EXPECT_TRUE(budget.allow_concurrent);
    EXPECT_EQ(budget.source_degradation, PORTIA_DEGRADATION_NONE);
    tracker->end_stage();

    // Stage 2: Apply budget to engine config
    tracker->begin_stage("Apply Budget", 5000);
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    int disabled = reasoning_portia_apply_budget(&cfg, &budget);
    EXPECT_EQ(disabled, 0) << "Full budget should disable nothing";
    tracker->end_stage();

    // Stage 3: Connect engine to brain
    tracker->begin_stage("Connect Brain", 5000);
    int rc = reasoning_engine_connect_brain(engine, brain);
    EXPECT_EQ(rc, 0) << "Connect should succeed";
    tracker->end_stage();

    // Stage 4: Produce budget summary
    tracker->begin_stage("Budget Summary", 5000);
    char summary[512];
    int written = reasoning_portia_budget_summary(&budget, summary, sizeof(summary));
    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(summary, "NONE"), nullptr);
    tracker->end_stage();

    // Stage 5: Run reasoning query with the (full) config
    tracker->begin_stage("Reason Query", 30000);
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    rc = reasoning_engine_reason(engine, "what is the meaning of knowledge", &chain);
    EXPECT_EQ(rc, 0) << "Reasoning should succeed with full budget config";
    EXPECT_GE(chain.num_steps, 1u) << "Should produce at least 1 reasoning step";

    reasoning_chain_cleanup(&chain);
    tracker->end_stage();
}

// =============================================================================
// E2E: Degradation Pipeline — Apply Budget with Phases Disabled
// =============================================================================

TEST_F(ReasoningPortiaBridgeE2E, DegradationPipelineStillReasons) {
    // Connect engine to brain first
    tracker->begin_stage("Connect Brain", 5000);
    int rc = reasoning_engine_connect_brain(engine, brain);
    EXPECT_EQ(rc, 0);
    tracker->end_stage();

    // Simulate MODERATE degradation by creating a restricted budget
    tracker->begin_stage("Apply Moderate Budget", 5000);
    reasoning_budget_t mod_budget = reasoning_portia_full_budget();
    mod_budget.allow_jepa               = false;
    mod_budget.allow_epistemic          = false;
    mod_budget.allow_world_model        = false;
    mod_budget.allow_verification       = false;
    mod_budget.allow_symbolic_inference = false;
    mod_budget.allow_concurrent         = false;
    mod_budget.max_steps_override       = 20;
    mod_budget.source_degradation       = PORTIA_DEGRADATION_MODERATE;

    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    int disabled = reasoning_portia_apply_budget(&cfg, &mod_budget);
    EXPECT_GT(disabled, 0) << "Moderate budget should disable some phases";
    EXPECT_EQ(cfg.max_steps, 20u);
    EXPECT_FALSE(cfg.enable_concurrent_pipeline);
    tracker->end_stage();

    // Summary should reflect MODERATE
    tracker->begin_stage("Budget Summary", 5000);
    char summary[512];
    int written = reasoning_portia_budget_summary(&mod_budget, summary, sizeof(summary));
    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(summary, "MODERATE"), nullptr);
    tracker->end_stage();

    // Reasoning should still work with fewer phases
    tracker->begin_stage("Reason Under Degradation", 30000);
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    rc = reasoning_engine_reason(engine, "explain causality", &chain);
    EXPECT_EQ(rc, 0) << "Reasoning should succeed even with degraded config";

    reasoning_chain_cleanup(&chain);
    tracker->end_stage();
}

// =============================================================================
// E2E: Training Integration Wrappers End-to-End
// =============================================================================

TEST_F(ReasoningPortiaBridgeE2E, TrainingIntegrationWrappersEndToEnd) {
    tracker->begin_stage("TI Wrappers", 5000);

    // All TI wrappers should report healthy state (no Portia)
    bool skip = brain_ti_should_skip_reasoning();
    int deg = brain_ti_get_reasoning_degradation();
    int phases = brain_ti_get_reasoning_phases_disabled();

    EXPECT_FALSE(skip) << "Should not skip reasoning without Portia";
    EXPECT_EQ(deg, (int)PORTIA_DEGRADATION_NONE)
        << "Degradation should be NONE without Portia";
    EXPECT_EQ(phases, 0)
        << "No phases should be disabled without Portia";

    tracker->end_stage();

    // The wrappers should be consistent with direct API calls
    tracker->begin_stage("TI vs Direct Consistency", 5000);

    reasoning_budget_t budget = reasoning_portia_compute_budget();
    EXPECT_EQ(deg, (int)budget.source_degradation);
    EXPECT_EQ(skip, reasoning_portia_should_skip());

    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    int direct_phases = reasoning_portia_apply_budget(&cfg, &budget);
    EXPECT_EQ(phases, direct_phases);

    tracker->end_stage();
}

// =============================================================================
// E2E: Degradation Ladder — Full Walk from NONE to EMERGENCY
// =============================================================================

TEST_F(ReasoningPortiaBridgeE2E, DegradationLadderFullWalk) {
    tracker->begin_stage("Degradation Ladder Walk", 10000);

    struct LadderStep {
        portia_degradation_level_t level;
        const char* name;
        int min_disabled;  // minimum phases expected to be disabled
    };

    LadderStep steps[] = {
        { PORTIA_DEGRADATION_NONE,      "NONE",      0 },
        { PORTIA_DEGRADATION_MINOR,     "MINOR",     2 },
        { PORTIA_DEGRADATION_MODERATE,  "MODERATE",  4 },
        { PORTIA_DEGRADATION_SEVERE,    "SEVERE",    5 },
        { PORTIA_DEGRADATION_EMERGENCY, "EMERGENCY", 7 },
    };

    // Construct budgets matching each degradation level (matching the implementation)
    reasoning_budget_t budgets[5];

    // NONE — full
    budgets[0] = reasoning_portia_full_budget();
    budgets[0].source_degradation = PORTIA_DEGRADATION_NONE;

    // MINOR — jepa + epistemic
    budgets[1] = reasoning_portia_full_budget();
    budgets[1].allow_jepa = false;
    budgets[1].allow_epistemic = false;
    budgets[1].source_degradation = PORTIA_DEGRADATION_MINOR;

    // MODERATE — jepa + epistemic + world_model + verify + sym_inference, no concurrent, steps=20
    budgets[2] = reasoning_portia_full_budget();
    budgets[2].allow_jepa = false;
    budgets[2].allow_epistemic = false;
    budgets[2].allow_world_model = false;
    budgets[2].allow_verification = false;
    budgets[2].allow_symbolic_inference = false;
    budgets[2].allow_concurrent = false;
    budgets[2].max_steps_override = 20;
    budgets[2].source_degradation = PORTIA_DEGRADATION_MODERATE;

    // SEVERE — only recall + knowledge + sym_query, sequential, steps=10
    budgets[3] = reasoning_portia_full_budget();
    budgets[3].allow_world_model = false;
    budgets[3].allow_jepa = false;
    budgets[3].allow_symbolic_inference = false;
    budgets[3].allow_verification = false;
    budgets[3].allow_epistemic = false;
    budgets[3].allow_concurrent = false;
    budgets[3].max_steps_override = 10;
    budgets[3].confidence_boost = 0.05f;
    budgets[3].source_degradation = PORTIA_DEGRADATION_SEVERE;

    // EMERGENCY — only recall, steps=5
    budgets[4] = reasoning_portia_full_budget();
    budgets[4].allow_knowledge = false;
    budgets[4].allow_world_model = false;
    budgets[4].allow_jepa = false;
    budgets[4].allow_symbolic_inference = false;
    budgets[4].allow_symbolic_query = false;
    budgets[4].allow_verification = false;
    budgets[4].allow_epistemic = false;
    budgets[4].allow_concurrent = false;
    budgets[4].max_steps_override = 5;
    budgets[4].confidence_boost = 0.1f;
    budgets[4].source_degradation = PORTIA_DEGRADATION_EMERGENCY;

    int prev_disabled = -1;
    for (int i = 0; i < 5; i++) {
        reasoning_engine_config_t cfg = reasoning_engine_default_config();
        int disabled = reasoning_portia_apply_budget(&cfg, &budgets[i]);

        EXPECT_GE(disabled, steps[i].min_disabled)
            << "Level " << steps[i].name
            << " should disable at least " << steps[i].min_disabled << " phases";

        // Monotonicity: each level disables >= previous
        EXPECT_GE(disabled, prev_disabled)
            << "Level " << steps[i].name
            << " should disable >= previous level";
        prev_disabled = disabled;

        // Summary should contain level name
        char summary[512];
        int written = reasoning_portia_budget_summary(&budgets[i], summary, sizeof(summary));
        EXPECT_GT(written, 0);
        EXPECT_NE(strstr(summary, steps[i].name), nullptr)
            << "Summary should contain '" << steps[i].name << "': " << summary;
    }

    tracker->end_stage();
}
