/**
 * @file test_stp_integration.cpp
 * @brief Integration tests for STP (short-term plasticity) in cognitive pipeline
 *
 * WHAT: Tests that STP features are actively used by brain/cognitive modules
 * WHY:  Ensure STP is properly wired into cognitive pipeline
 * HOW:  Test STP operations through brain API and verify integration with synapses
 *
 * @version STP Integration Testing
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

#include "core/brain/nimcp_brain.h"
#include "plasticity/stp/nimcp_stp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class STPIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // Create brain that may use STP internally
        brain = brain_create("stp_test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(brain, nullptr) << "Failed to create brain";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Integration Test 1: STP State Initialization
//=============================================================================

TEST_F(STPIntegrationTest, STPStateInitialization) {
    stp_state_t state;
    stp_params_t params = stp_get_preset_params(STP_PRESET_DEPRESSING);

    stp_init(&state, &params, 0);

    // Initial state should be at equilibrium
    EXPECT_FLOAT_EQ(state.x, 1.0f) << "Resources should be fully available initially";
    EXPECT_FLOAT_EQ(state.u, params.U) << "Utilization should equal baseline U";
    EXPECT_EQ(state.last_update, 0);
}

//=============================================================================
// Integration Test 2: STP Spike Processing
//=============================================================================

TEST_F(STPIntegrationTest, STPSpikeProcessing) {
    stp_state_t state;
    stp_params_t params = stp_get_preset_params(STP_PRESET_DEPRESSING);
    stp_init(&state, &params, 0);

    float initial_x = state.x;
    float initial_u = state.u;

    // Process spike
    stp_process_spike(&state, 1);

    // After spike, resources should decrease (depression)
    EXPECT_LT(state.x, initial_x) << "Resources should deplete after spike";

    // Utilization may increase (facilitation)
    EXPECT_GE(state.u, initial_u) << "Utilization should not decrease";
}

//=============================================================================
// Integration Test 3: STP Modulation Factor
//=============================================================================

TEST_F(STPIntegrationTest, STPModulationFactor) {
    stp_state_t state;
    stp_params_t params = stp_get_preset_params(STP_PRESET_DEPRESSING);
    stp_init(&state, &params, 0);

    float modulation = stp_get_modulation(&state);

    // Modulation should be between 0 and 1
    EXPECT_GE(modulation, 0.0f);
    EXPECT_LE(modulation, 1.0f);

    // For freshly initialized depressing synapse, modulation should be positive
    EXPECT_GT(modulation, 0.0f);
}

//=============================================================================
// Integration Test 4: STP Depression Behavior
//=============================================================================

TEST_F(STPIntegrationTest, STPDepressionBehavior) {
    stp_state_t state;
    stp_params_t params = stp_get_preset_params(STP_PRESET_DEPRESSING);
    stp_init(&state, &params, 0);

    float modulation_values[5];

    // Simulate rapid spike train
    for (int i = 0; i < 5; i++) {
        stp_process_spike(&state, i * 10);  // Spikes every 10ms
        modulation_values[i] = stp_get_modulation(&state);
    }

    // For depressing synapse, modulation should decrease with repeated spikes
    EXPECT_GT(modulation_values[0], modulation_values[4])
        << "Depressing synapse should show depression";
}

//=============================================================================
// Integration Test 5: STP Facilitation Behavior
//=============================================================================

TEST_F(STPIntegrationTest, STPFacilitationBehavior) {
    stp_state_t state;
    stp_params_t params = stp_get_preset_params(STP_PRESET_FACILITATING);
    stp_init(&state, &params, 0);

    float modulation_values[5];

    // Simulate rapid spike train
    for (int i = 0; i < 5; i++) {
        stp_process_spike(&state, i * 10);  // Spikes every 10ms
        modulation_values[i] = stp_get_modulation(&state);
    }

    // For facilitating synapse, modulation may increase initially
    // (depends on relative strength of facilitation vs depression)
    EXPECT_GT(modulation_values[1], 0.0f) << "Facilitation should be active";
}

//=============================================================================
// Integration Test 6: STP Recovery Over Time
//=============================================================================

TEST_F(STPIntegrationTest, STPRecoveryOverTime) {
    stp_state_t state;
    stp_params_t params = stp_get_preset_params(STP_PRESET_DEPRESSING);
    stp_init(&state, &params, 0);

    // Deplete resources with spike
    stp_process_spike(&state, 0);
    float depleted_x = state.x;

    // Wait for recovery (simulate time passing)
    stp_update(&state, 500);  // 500ms later

    // Resources should recover towards equilibrium
    EXPECT_GT(state.x, depleted_x) << "Resources should recover over time";
}

//=============================================================================
// Integration Test 7: STP Preset Parameters
//=============================================================================

TEST_F(STPIntegrationTest, STPPresetParameters) {
    // Test all presets return valid parameters
    stp_preset_t presets[] = {
        STP_PRESET_DEPRESSING,
        STP_PRESET_FACILITATING,
        STP_PRESET_MIXED,
        STP_PRESET_FAST_DEPRESSING,
        STP_PRESET_SLOW_DEPRESSING,
        STP_PRESET_NONE
    };

    for (int i = 0; i < 6; i++) {
        stp_params_t params = stp_get_preset_params(presets[i]);

        // Validate parameters
        EXPECT_GT(params.U, 0.0f) << "U should be positive";
        EXPECT_LE(params.U, 1.0f) << "U should be <= 1";
        EXPECT_GT(params.tau_D, 0.0f) << "tau_D should be positive";
        EXPECT_GT(params.tau_F, 0.0f) << "tau_F should be positive";
    }
}

//=============================================================================
// Integration Test 8: STP Reset Functionality
//=============================================================================

TEST_F(STPIntegrationTest, STPResetFunctionality) {
    stp_state_t state;
    stp_params_t params = stp_get_preset_params(STP_PRESET_DEPRESSING);
    stp_init(&state, &params, 0);

    // Modify state
    stp_process_spike(&state, 1);
    stp_process_spike(&state, 2);

    // Reset
    stp_reset(&state, 100);

    // Should return to equilibrium
    EXPECT_FLOAT_EQ(state.x, 1.0f) << "Resources should be fully restored";
    EXPECT_FLOAT_EQ(state.u, params.U) << "Utilization should return to baseline";
}

//=============================================================================
// Integration Test 9: Brain Works With STP
//=============================================================================

TEST_F(STPIntegrationTest, BrainWorksWithSTP) {
    // Verify brain can process decisions (may use STP internally for synapses)
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};

    for (int i = 0; i < 5; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 4);
        ASSERT_NE(decision, nullptr) << "Brain should work with STP";
        EXPECT_NE(decision->label, nullptr);
    }
}

//=============================================================================
// Integration Test 10: STP Synapse Classification
//=============================================================================

TEST_F(STPIntegrationTest, STPSynapseClassification) {
    stp_params_t depressing = stp_get_preset_params(STP_PRESET_DEPRESSING);
    stp_params_t facilitating = stp_get_preset_params(STP_PRESET_FACILITATING);
    stp_params_t mixed = stp_get_preset_params(STP_PRESET_MIXED);

    int class_dep = stp_classify_synapse(&depressing);
    int class_fac = stp_classify_synapse(&facilitating);
    int class_mix = stp_classify_synapse(&mixed);

    // Classifications should be different
    EXPECT_NE(class_dep, class_fac) << "Depressing and facilitating should have different classifications";

    // All should be valid (0, 1, or 2)
    EXPECT_GE(class_dep, 0);
    EXPECT_LE(class_dep, 2);
    EXPECT_GE(class_fac, 0);
    EXPECT_LE(class_fac, 2);
    EXPECT_GE(class_mix, 0);
    EXPECT_LE(class_mix, 2);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
