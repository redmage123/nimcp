/**
 * @file test_reasoning_hypo_bridge_regression.cpp
 * @brief Regression tests for the Hypothalamus-Reasoning bridge
 *
 * WHAT: Regression tests guarding against specific edge-case bugs
 * WHY:  Prevent regressions in NULL handling, buffer overflow in summary,
 *       capacity invariants, and apply-never-loosens semantics
 * HOW:  GTest fixture exercising edge cases and invariants
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

class ReasoningHypoBridgeRegression : public ::testing::Test {};

// =============================================================================
// Tests: Repeated neutral modulation stability
// =============================================================================

TEST_F(ReasoningHypoBridgeRegression, RepeatedNeutralModulationStable) {
    reasoning_hypo_modulation_t first = reasoning_hypo_neutral_modulation();

    for (int i = 0; i < 100; i++) {
        reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
        EXPECT_FLOAT_EQ(mod.cognitive_capacity, first.cognitive_capacity) << "Iteration " << i;
        EXPECT_EQ(mod.urgency_mode, first.urgency_mode) << "Iteration " << i;
        EXPECT_FLOAT_EQ(mod.stress_level, first.stress_level) << "Iteration " << i;
        EXPECT_FLOAT_EQ(mod.alertness, first.alertness) << "Iteration " << i;
        EXPECT_FLOAT_EQ(mod.sleep_pressure, first.sleep_pressure) << "Iteration " << i;
        EXPECT_EQ(mod.recommended_max_steps, first.recommended_max_steps) << "Iteration " << i;
        EXPECT_EQ(mod.force_sequential, first.force_sequential) << "Iteration " << i;
        EXPECT_EQ(mod.hypothalamus_available, first.hypothalamus_available) << "Iteration " << i;
    }
}

// =============================================================================
// Tests: Repeated compute with NULL brain stability
// =============================================================================

TEST_F(ReasoningHypoBridgeRegression, RepeatedComputeNullBrainStable) {
    reasoning_hypo_modulation_t first = reasoning_hypo_compute_modulation(NULL);

    for (int i = 0; i < 100; i++) {
        reasoning_hypo_modulation_t mod = reasoning_hypo_compute_modulation(NULL);
        EXPECT_FLOAT_EQ(mod.cognitive_capacity, first.cognitive_capacity) << "Iteration " << i;
        EXPECT_EQ(mod.urgency_mode, first.urgency_mode) << "Iteration " << i;
        EXPECT_FLOAT_EQ(mod.stress_level, first.stress_level) << "Iteration " << i;
        EXPECT_FALSE(mod.hypothalamus_available) << "Iteration " << i;
    }
}

// =============================================================================
// Tests: Both NULL returns error
// =============================================================================

TEST_F(ReasoningHypoBridgeRegression, ApplyBothNullReturnsError) {
    EXPECT_EQ(reasoning_hypo_apply_modulation(NULL, NULL), -1);
}

// =============================================================================
// Tests: Buffer overflow protection in summary
// =============================================================================

TEST_F(ReasoningHypoBridgeRegression, SummaryTinyBufferNoOverflow) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    char buf[1];
    buf[0] = 'X';

    /* Buffer size 1: snprintf writes NUL only */
    int written = reasoning_hypo_modulation_summary(&mod, buf, 1);
    /* Should succeed (snprintf returns chars that would have been written) */
    EXPECT_GE(written, 0);
    EXPECT_EQ(buf[0], '\0');
}

TEST_F(ReasoningHypoBridgeRegression, SummarySmallBufferTruncates) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    char buf[16];
    memset(buf, 'X', sizeof(buf));

    int written = reasoning_hypo_modulation_summary(&mod, buf, sizeof(buf));
    EXPECT_GE(written, 0);
    /* Must be NUL-terminated */
    EXPECT_EQ(buf[15], '\0');
}

// =============================================================================
// Tests: Capacity invariants
// =============================================================================

TEST_F(ReasoningHypoBridgeRegression, CapacityNeverNegative) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    EXPECT_GE(mod.cognitive_capacity, 0.0f);
}

TEST_F(ReasoningHypoBridgeRegression, CapacityNeverExceedsOne) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    EXPECT_LE(mod.cognitive_capacity, 1.0f);
}

// =============================================================================
// Tests: Neutral modulation source flag
// =============================================================================

TEST_F(ReasoningHypoBridgeRegression, NeutralModNotAvailable) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    EXPECT_FALSE(mod.hypothalamus_available);
}

// =============================================================================
// Tests: Apply neutral does not mutate config
// =============================================================================

TEST_F(ReasoningHypoBridgeRegression, ApplyNeutralDoesNotMutateConfig) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    reasoning_engine_config_t original = config;
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();

    int rc = reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_EQ(rc, 0);

    /* Byte-for-byte identical */
    EXPECT_EQ(memcmp(&config, &original, sizeof(config)), 0);
}

// =============================================================================
// Tests: max_steps floor
// =============================================================================

TEST_F(ReasoningHypoBridgeRegression, MaxStepsNeverDropsBelowThree) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.max_steps = 50;

    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    mod.cognitive_capacity = 0.01f;
    mod.hypothalamus_available = true;

    int rc = reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(config.max_steps, 3u);
}

// =============================================================================
// Tests: Apply does not loosen existing constraints
// =============================================================================

TEST_F(ReasoningHypoBridgeRegression, ApplyDoesNotLoosenExistingConstraints) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.max_steps = 5;

    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    mod.recommended_max_steps = 30;
    mod.cognitive_capacity = 1.0f;
    mod.hypothalamus_available = true;

    int rc = reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_EQ(rc, 0);
    /* max_steps=5 should NOT be loosened to 30 */
    EXPECT_LE(config.max_steps, 5u);
}

// =============================================================================
// Tests: force_sequential applied
// =============================================================================

TEST_F(ReasoningHypoBridgeRegression, ForceSequentialApplied) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_concurrent_pipeline = true;

    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    mod.force_sequential = true;
    mod.hypothalamus_available = true;

    int rc = reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_EQ(rc, 0);
    EXPECT_FALSE(config.enable_concurrent_pipeline);
}
