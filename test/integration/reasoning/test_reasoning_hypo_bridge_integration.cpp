/**
 * @file test_reasoning_hypo_bridge_integration.cpp
 * @brief Integration tests for the Hypothalamus-Reasoning bridge with a live brain
 *
 * WHAT: Integration tests verifying hypothalamus bridge works with real brain
 * WHY:  Verify that compute_modulation and TI wrappers produce valid results
 *       when connected to a live brain instance (even without hypothalamus,
 *       the functions should return neutral/safe defaults)
 * HOW:  GTest fixture with brain_create/destroy lifecycle
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "cognitive/reasoning/nimcp_reasoning_hypo_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/training/nimcp_training_integration.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class ReasoningHypoBridgeIntegration : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create("hypo_bridge_test", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 32, 8);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) brain_destroy(brain);
    }
};

// =============================================================================
// Tests: Compute modulation with real brain
// =============================================================================

TEST_F(ReasoningHypoBridgeIntegration, ComputeWithRealBrain) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_compute_modulation(brain);

    /* Should not crash; capacity should be valid */
    EXPECT_GE(mod.cognitive_capacity, 0.0f);
    EXPECT_LE(mod.cognitive_capacity, 1.0f);
}

// =============================================================================
// Tests: Summary with brain
// =============================================================================

TEST_F(ReasoningHypoBridgeIntegration, ModulationSummaryWithBrain) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_compute_modulation(brain);

    char buf[512];
    int written = reasoning_hypo_modulation_summary(&mod, buf, sizeof(buf));
    EXPECT_GT(written, 0);
    EXPECT_GT(strlen(buf), 0u);
}

// =============================================================================
// Tests: Apply modulation to config with brain
// =============================================================================

TEST_F(ReasoningHypoBridgeIntegration, ApplyModulationToConfigWithBrain) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_compute_modulation(brain);
    reasoning_engine_config_t config = reasoning_engine_default_config();

    int rc = reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_EQ(rc, 0);
    /* max_steps should remain positive regardless of modulation */
    EXPECT_GT(config.max_steps, 0u);
}

// =============================================================================
// Tests: Training Integration with real brain
// =============================================================================

TEST_F(ReasoningHypoBridgeIntegration, TrainingIntegrationCapacityWithBrain) {
    float cap = brain_ti_get_cognitive_capacity(brain);
    EXPECT_GE(cap, 0.0f);
    EXPECT_LE(cap, 1.0f);
}

TEST_F(ReasoningHypoBridgeIntegration, TrainingIntegrationUrgencyWithBrain) {
    int urgency = brain_ti_get_urgency_mode(brain);
    EXPECT_GE(urgency, 0);
    EXPECT_LE(urgency, 3);
}

TEST_F(ReasoningHypoBridgeIntegration, TrainingIntegrationStressWithBrain) {
    float stress = brain_ti_get_stress_level(brain);
    EXPECT_GE(stress, 0.0f);
    EXPECT_LE(stress, 1.0f);
}

// =============================================================================
// Tests: Repeated modulation stability
// =============================================================================

TEST_F(ReasoningHypoBridgeIntegration, RepeatedModulationStable) {
    for (int i = 0; i < 10; i++) {
        reasoning_hypo_modulation_t mod = reasoning_hypo_compute_modulation(brain);
        EXPECT_GE(mod.cognitive_capacity, 0.0f) << "Iteration " << i;
        EXPECT_LE(mod.cognitive_capacity, 1.0f) << "Iteration " << i;
    }
}
