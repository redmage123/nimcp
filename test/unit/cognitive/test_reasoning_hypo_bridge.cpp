/**
 * @file test_reasoning_hypo_bridge.cpp
 * @brief Unit tests for the Hypothalamus-Reasoning bridge
 *
 * WHAT: Tests hypo modulation computation, application, and summary in isolation
 * WHY:  Verify modulation logic independently of actual hypothalamus state.
 *       In unit tests the brain is NULL, so compute_modulation() returns neutral
 *       and training integration functions return safe defaults — that is expected.
 * HOW:  GTest fixture exercising each function with crafted modulations
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_hypo_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/training/nimcp_training_integration.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class ReasoningHypoBridgeUnit : public ::testing::Test {};

// =============================================================================
// Tests: Neutral Modulation Defaults
// =============================================================================

TEST_F(ReasoningHypoBridgeUnit, NeutralModulationDefaults) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();

    EXPECT_FLOAT_EQ(mod.cognitive_capacity, 1.0f);
    EXPECT_EQ(mod.urgency_mode, REASONING_URGENCY_NORMAL);
    EXPECT_FLOAT_EQ(mod.stress_level, 0.0f);
    EXPECT_FLOAT_EQ(mod.alertness, 1.0f);
    EXPECT_FLOAT_EQ(mod.sleep_pressure, 0.0f);
    EXPECT_EQ(mod.recommended_max_steps, 0u);
    EXPECT_FALSE(mod.force_sequential);
    EXPECT_FALSE(mod.hypothalamus_available);
}

// =============================================================================
// Tests: Compute with NULL brain
// =============================================================================

TEST_F(ReasoningHypoBridgeUnit, ComputeWithNullBrainReturnsNeutral) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_compute_modulation(NULL);

    /* NULL brain should return neutral modulation */
    EXPECT_FLOAT_EQ(mod.cognitive_capacity, 1.0f);
    EXPECT_EQ(mod.urgency_mode, REASONING_URGENCY_NORMAL);
    EXPECT_FLOAT_EQ(mod.stress_level, 0.0f);
    EXPECT_FLOAT_EQ(mod.alertness, 1.0f);
    EXPECT_FLOAT_EQ(mod.sleep_pressure, 0.0f);
    EXPECT_EQ(mod.recommended_max_steps, 0u);
    EXPECT_FALSE(mod.force_sequential);
    EXPECT_FALSE(mod.hypothalamus_available);
}

// =============================================================================
// Tests: Apply NULL guards
// =============================================================================

TEST_F(ReasoningHypoBridgeUnit, ApplyNullConfigReturnsError) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    EXPECT_EQ(reasoning_hypo_apply_modulation(NULL, &mod), -1);
}

TEST_F(ReasoningHypoBridgeUnit, ApplyNullModReturnsError) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    EXPECT_EQ(reasoning_hypo_apply_modulation(&config, NULL), -1);
}

// =============================================================================
// Tests: Apply neutral modulation (hypothalamus_available=false)
// =============================================================================

TEST_F(ReasoningHypoBridgeUnit, ApplyNeutralModChangesNothing) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    reasoning_engine_config_t original = config;
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();

    /* Neutral modulation has hypothalamus_available=false, so nothing changes */
    int rc = reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_EQ(rc, 0);

    /* Config should be identical to before */
    EXPECT_EQ(config.max_steps, original.max_steps);
    EXPECT_EQ(config.max_depth, original.max_depth);
    EXPECT_EQ(config.enable_concurrent_pipeline, original.enable_concurrent_pipeline);
}

// =============================================================================
// Tests: Urgency-based step caps
// =============================================================================

TEST_F(ReasoningHypoBridgeUnit, ApplyFightOrFlightCapsSteps) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.max_steps = 50;
    config.enable_concurrent_pipeline = true;

    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    mod.urgency_mode = REASONING_URGENCY_FIGHT_OR_FLIGHT;
    mod.recommended_max_steps = 10;
    mod.force_sequential = true;
    mod.hypothalamus_available = true;

    int rc = reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_EQ(rc, 0);
    EXPECT_LE(config.max_steps, 10u);
    EXPECT_FALSE(config.enable_concurrent_pipeline);
}

TEST_F(ReasoningHypoBridgeUnit, ApplyAlertCapsSteps) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.max_steps = 50;

    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    mod.urgency_mode = REASONING_URGENCY_ALERT;
    mod.recommended_max_steps = 30;
    mod.hypothalamus_available = true;

    int rc = reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_EQ(rc, 0);
    EXPECT_LE(config.max_steps, 30u);
}

// =============================================================================
// Tests: Cognitive capacity effects
// =============================================================================

TEST_F(ReasoningHypoBridgeUnit, ApplyLowCapacityForcesSequential) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_concurrent_pipeline = true;

    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    mod.cognitive_capacity = 0.2f;
    mod.hypothalamus_available = true;
    mod.force_sequential = true;

    int rc = reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_EQ(rc, 0);
    EXPECT_FALSE(config.enable_concurrent_pipeline);
}

TEST_F(ReasoningHypoBridgeUnit, ApplyLowCapacityScalesSteps) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.max_steps = 50;

    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    mod.cognitive_capacity = 0.5f;
    mod.hypothalamus_available = true;

    int rc = reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_EQ(rc, 0);
    /* 50 * 0.5 = 25 */
    EXPECT_LE(config.max_steps, 25u);
}

TEST_F(ReasoningHypoBridgeUnit, ApplyVeryLowCapacityFloorsSteps) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.max_steps = 50;

    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    mod.cognitive_capacity = 0.1f;
    mod.hypothalamus_available = true;

    int rc = reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_EQ(rc, 0);
    /* Capacity 0.1 < 0.2 is floored to 0.2: 50*0.2=10. Absolute minimum is 3 */
    EXPECT_GE(config.max_steps, 3u);
}

// =============================================================================
// Tests: Summary NULL guards
// =============================================================================

TEST_F(ReasoningHypoBridgeUnit, SummaryNullReturnsError) {
    char buf[256];
    EXPECT_EQ(reasoning_hypo_modulation_summary(NULL, buf, sizeof(buf)), -1);
}

TEST_F(ReasoningHypoBridgeUnit, SummaryNullBufferReturnsError) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    EXPECT_EQ(reasoning_hypo_modulation_summary(&mod, NULL, 256), -1);
}

TEST_F(ReasoningHypoBridgeUnit, SummaryZeroSizeReturnsError) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    char buf[1];
    EXPECT_EQ(reasoning_hypo_modulation_summary(&mod, buf, 0), -1);
}

// =============================================================================
// Tests: Summary content
// =============================================================================

TEST_F(ReasoningHypoBridgeUnit, SummaryContainsUrgencyName) {
    char buf[512];

    /* Neutral (NORMAL) */
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    int written = reasoning_hypo_modulation_summary(&mod, buf, sizeof(buf));
    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(buf, "NORMAL"), nullptr);

    /* FIGHT_OR_FLIGHT */
    mod.urgency_mode = REASONING_URGENCY_FIGHT_OR_FLIGHT;
    written = reasoning_hypo_modulation_summary(&mod, buf, sizeof(buf));
    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(buf, "FIGHT_OR_FLIGHT"), nullptr);
}

TEST_F(ReasoningHypoBridgeUnit, SummaryContainsCapacity) {
    char buf[512];
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    int written = reasoning_hypo_modulation_summary(&mod, buf, sizeof(buf));
    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(buf, "capacity="), nullptr);
}

// =============================================================================
// Tests: Training Integration NULL brain defaults
// =============================================================================

TEST_F(ReasoningHypoBridgeUnit, TrainingIntegrationNullBrainCapacity) {
    float cap = brain_ti_get_cognitive_capacity(NULL);
    EXPECT_FLOAT_EQ(cap, 1.0f);
}

TEST_F(ReasoningHypoBridgeUnit, TrainingIntegrationNullBrainUrgency) {
    int urgency = brain_ti_get_urgency_mode(NULL);
    EXPECT_EQ(urgency, 1);  /* REASONING_URGENCY_NORMAL = 1 */
}

TEST_F(ReasoningHypoBridgeUnit, TrainingIntegrationNullBrainStress) {
    float stress = brain_ti_get_stress_level(NULL);
    EXPECT_FLOAT_EQ(stress, 0.0f);
}

// =============================================================================
// Tests: Apply never loosens previous constraints
// =============================================================================

TEST_F(ReasoningHypoBridgeUnit, ApplyNeverLoosensPreviousConstraints) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.max_steps = 10;  /* Already constrained */

    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    mod.recommended_max_steps = 30;
    mod.hypothalamus_available = true;

    int rc = reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_EQ(rc, 0);
    /* Config already had max_steps=10, recommended=30 should NOT loosen */
    EXPECT_LE(config.max_steps, 10u);
}
