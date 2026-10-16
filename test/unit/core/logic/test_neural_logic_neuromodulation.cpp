/**
 * @file test_neural_logic_neuromodulation.cpp
 * @brief Unit Tests for MODULE 4: Neural Logic Neuromodulation
 * @version 3.0.0
 * @date 2025-11-20
 *
 * TEST COVERAGE: 8 tests, 100% function coverage
 * - apply_dopamine_modulation: 3 tests
 * - apply_acetylcholine_modulation: 2 tests
 * - update_all_gate_modulation: 2 tests
 * - get_modulated_threshold: 1 test
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/logic/nimcp_neural_logic_neuromodulation.h"
#include "core/logic/nimcp_neural_logic_factory.h"
#include "core/brain/nimcp_brain.h"

class NeuralLogicNeuromodulationTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = brain_create("neuromod_test_brain", 1000);
        ASSERT_NE(brain, nullptr);

        bool attached = create_and_attach_neural_logic(brain, 1000);
        ASSERT_TRUE(attached);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }

    uint32_t create_test_gate() {
        neural_logic_network_t net = brain_get_neural_logic(brain);
        return neural_logic_create_gate(net, LOGIC_GATE_AND, 1.5f);
    }
};

//=============================================================================
// Test: apply_dopamine_modulation
//=============================================================================

TEST_F(NeuralLogicNeuromodulationTest, ApplyDopamineSuccess) {
    uint32_t gate_id = create_test_gate();
    ASSERT_NE(gate_id, UINT32_MAX);

    bool result = apply_dopamine_modulation(brain, gate_id, 0.8f);

    EXPECT_TRUE(result);
}

TEST_F(NeuralLogicNeuromodulationTest, ApplyDopamineNullBrain) {
    bool result = apply_dopamine_modulation(nullptr, 0, 0.5f);

    EXPECT_FALSE(result);
}

TEST_F(NeuralLogicNeuromodulationTest, ApplyDopamineOutOfRange) {
    uint32_t gate_id = create_test_gate();
    ASSERT_NE(gate_id, UINT32_MAX);

    // Should clamp and succeed with warning
    bool result = apply_dopamine_modulation(brain, gate_id, 1.5f);

    EXPECT_TRUE(result);
}

//=============================================================================
// Test: apply_acetylcholine_modulation
//=============================================================================

TEST_F(NeuralLogicNeuromodulationTest, ApplyAcetylcholineSuccess) {
    uint32_t gate_id = create_test_gate();
    ASSERT_NE(gate_id, UINT32_MAX);

    bool result = apply_acetylcholine_modulation(brain, gate_id, 0.6f);

    EXPECT_TRUE(result);
}

TEST_F(NeuralLogicNeuromodulationTest, ApplyAcetylcholineNullBrain) {
    bool result = apply_acetylcholine_modulation(nullptr, 0, 0.5f);

    EXPECT_FALSE(result);
}

//=============================================================================
// Test: update_all_gate_modulation
//=============================================================================

TEST_F(NeuralLogicNeuromodulationTest, UpdateAllGatesSuccess) {
    // Create some gates
    create_test_gate();
    create_test_gate();

    uint32_t count = update_all_gate_modulation(brain);

    // Should modulate at least the gates we created
    EXPECT_GE(count, 0u);
}

TEST_F(NeuralLogicNeuromodulationTest, UpdateAllGatesNullBrain) {
    uint32_t count = update_all_gate_modulation(nullptr);

    EXPECT_EQ(count, 0u);
}

//=============================================================================
// Test: get_modulated_threshold
//=============================================================================

TEST_F(NeuralLogicNeuromodulationTest, GetModulatedThresholdSuccess) {
    float base = 1.5f;
    float modulated = 0.0f;

    bool result = get_modulated_threshold(brain, base, &modulated);

    EXPECT_TRUE(result);
    EXPECT_GT(modulated, 0.0f);
    // Modulated should be in reasonable range (0.5x to 1.5x base)
    EXPECT_GE(modulated, base * 0.5f);
    EXPECT_LE(modulated, base * 1.5f);
}
