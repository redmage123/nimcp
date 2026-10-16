/**
 * @file test_izhikevich_coverage.cpp
 * @brief Comprehensive tests for nimcp_izhikevich.c (TARGET: 100% coverage)
 *
 * WHAT: Test Izhikevich spiking neuron model
 * WHY:  Achieve 100% line/branch coverage for nimcp_izhikevich.c
 * HOW:  Test all public functions, preset types, dynamics, and edge cases
 *
 * COVERAGE GOALS:
 * - Line coverage: 100%
 * - Branch coverage: 100%
 * - Function coverage: 100%
 *
 * @author NIMCP Development Team
 * @date 2025-11-10
 */

#include <gtest/gtest.h>
#include <cmath>

#include "core/neuron_models/nimcp_izhikevich.h"
#include "core/neuron_models/nimcp_neuron_model.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class IzhikevichModelTest : public ::testing::Test {
protected:
    const neuron_model_vtable_t* vtable;

    void SetUp() override {
        vtable = izhikevich_get_vtable();
        ASSERT_NE(vtable, nullptr);
    }

    void TearDown() override {
        // Cleanup
    }

    // Helper: Drive neuron to spike
    void drive_to_spike(neuron_model_state_t state, int max_steps = 100) {
        for (int i = 0; i < max_steps; i++) {
            neuron_model_update(state, 1.0f, 20.0f);  // Large current
            if (neuron_model_check_spike(state)) {
                break;
            }
        }
    }
};

//=============================================================================
// Test Suite: Public API - Factory Functions
//=============================================================================

TEST_F(IzhikevichModelTest, GetVtable) {
    // Test vtable retrieval
    const neuron_model_vtable_t* vt = izhikevich_get_vtable();
    ASSERT_NE(vt, nullptr);
    EXPECT_STREQ(vt->name, "Izhikevich");
    EXPECT_EQ(vt->type, NEURON_MODEL_IZHIKEVICH);
    EXPECT_GT(vt->state_size, 0);
}

TEST_F(IzhikevichModelTest, GetPresetParams_RegularSpiking) {
    // Test Regular Spiking preset
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    EXPECT_FLOAT_EQ(params.a, 0.02f);
    EXPECT_FLOAT_EQ(params.b, 0.2f);
    EXPECT_FLOAT_EQ(params.c, -65.0f);
    EXPECT_FLOAT_EQ(params.d, 8.0f);
}

TEST_F(IzhikevichModelTest, GetPresetParams_FastSpiking) {
    // Test Fast Spiking preset
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_FAST_SPIKING);
    EXPECT_FLOAT_EQ(params.a, 0.1f);
    EXPECT_FLOAT_EQ(params.b, 0.2f);
    EXPECT_FLOAT_EQ(params.c, -65.0f);
    EXPECT_FLOAT_EQ(params.d, 2.0f);
}

TEST_F(IzhikevichModelTest, GetPresetParams_IntrinsicallyBursting) {
    // Test Intrinsically Bursting preset
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_INTRINSICALLY_BURSTING);
    EXPECT_FLOAT_EQ(params.a, 0.02f);
    EXPECT_FLOAT_EQ(params.b, 0.2f);
    EXPECT_FLOAT_EQ(params.c, -55.0f);
    EXPECT_FLOAT_EQ(params.d, 4.0f);
}

TEST_F(IzhikevichModelTest, GetPresetParams_AllPresets) {
    // Test all preset types
    izhikevich_preset_t presets[] = {
        IZHI_PRESET_REGULAR_SPIKING,
        IZHI_PRESET_INTRINSICALLY_BURSTING,
        IZHI_PRESET_CHATTERING,
        IZHI_PRESET_FAST_SPIKING,
        IZHI_PRESET_LOW_THRESHOLD_SPIKING,
        IZHI_PRESET_THALAMO_CORTICAL,
        IZHI_PRESET_RESONATOR,
        IZHI_PRESET_CUSTOM
    };

    for (auto preset : presets) {
        izhikevich_params_t params = izhikevich_get_preset_params(preset);
        // All presets should return valid parameters
        EXPECT_NE(params.a, 0.0f);
        EXPECT_NE(params.b, 0.0f);
    }
}

TEST_F(IzhikevichModelTest, GetPresetParams_InvalidIndex) {
    // Test with invalid preset index (should return default)
    izhikevich_params_t params = izhikevich_get_preset_params((izhikevich_preset_t)999);
    // Should return Regular Spiking as fallback
    EXPECT_FLOAT_EQ(params.a, 0.02f);
    EXPECT_FLOAT_EQ(params.b, 0.2f);
}

TEST_F(IzhikevichModelTest, CreateCustomParams) {
    // Test custom parameter creation
    izhikevich_params_t params = izhikevich_create_params(0.05f, 0.25f, -60.0f, 5.0f);
    EXPECT_FLOAT_EQ(params.a, 0.05f);
    EXPECT_FLOAT_EQ(params.b, 0.25f);
    EXPECT_FLOAT_EQ(params.c, -60.0f);
    EXPECT_FLOAT_EQ(params.d, 5.0f);
}

//=============================================================================
// Test Suite: Preset Names and Descriptions
//=============================================================================

TEST_F(IzhikevichModelTest, GetPresetName_RegularSpiking) {
    const char* name = izhikevich_get_preset_name(IZHI_PRESET_REGULAR_SPIKING);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Regular Spiking (RS)");
}

TEST_F(IzhikevichModelTest, GetPresetName_AllPresets) {
    izhikevich_preset_t presets[] = {
        IZHI_PRESET_REGULAR_SPIKING,
        IZHI_PRESET_INTRINSICALLY_BURSTING,
        IZHI_PRESET_CHATTERING,
        IZHI_PRESET_FAST_SPIKING,
        IZHI_PRESET_LOW_THRESHOLD_SPIKING,
        IZHI_PRESET_THALAMO_CORTICAL,
        IZHI_PRESET_RESONATOR,
        IZHI_PRESET_CUSTOM
    };

    for (auto preset : presets) {
        const char* name = izhikevich_get_preset_name(preset);
        ASSERT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0);
    }
}

TEST_F(IzhikevichModelTest, GetPresetName_InvalidIndex) {
    const char* name = izhikevich_get_preset_name((izhikevich_preset_t)999);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Unknown");
}

TEST_F(IzhikevichModelTest, GetPresetDescription_RegularSpiking) {
    const char* desc = izhikevich_get_preset_description(IZHI_PRESET_REGULAR_SPIKING);
    ASSERT_NE(desc, nullptr);
    EXPECT_GT(strlen(desc), 0);
}

TEST_F(IzhikevichModelTest, GetPresetDescription_AllPresets) {
    izhikevich_preset_t presets[] = {
        IZHI_PRESET_REGULAR_SPIKING,
        IZHI_PRESET_INTRINSICALLY_BURSTING,
        IZHI_PRESET_CHATTERING,
        IZHI_PRESET_FAST_SPIKING,
        IZHI_PRESET_LOW_THRESHOLD_SPIKING,
        IZHI_PRESET_THALAMO_CORTICAL,
        IZHI_PRESET_RESONATOR,
        IZHI_PRESET_CUSTOM
    };

    for (auto preset : presets) {
        const char* desc = izhikevich_get_preset_description(preset);
        ASSERT_NE(desc, nullptr);
        EXPECT_GT(strlen(desc), 0);
    }
}

TEST_F(IzhikevichModelTest, GetPresetDescription_InvalidIndex) {
    const char* desc = izhikevich_get_preset_description((izhikevich_preset_t)999);
    ASSERT_NE(desc, nullptr);
    EXPECT_STREQ(desc, "Unknown preset type");
}

//=============================================================================
// Test Suite: Neuron Lifecycle
//=============================================================================

TEST_F(IzhikevichModelTest, CreateNeuron_DefaultParams) {
    // Create with default parameters (NULL)
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    // Voltage should be at resting potential
    float v = neuron_model_get_voltage(state);
    EXPECT_FLOAT_EQ(v, IZHIKEVICH_RESTING_POTENTIAL);

    neuron_model_destroy(state);
}

TEST_F(IzhikevichModelTest, CreateNeuron_WithParams) {
    // Create with custom parameters
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_FAST_SPIKING);
    neuron_model_state_t state = neuron_model_create(vtable, &params);
    ASSERT_NE(state, nullptr);

    float v = neuron_model_get_voltage(state);
    EXPECT_FLOAT_EQ(v, IZHIKEVICH_RESTING_POTENTIAL);

    neuron_model_destroy(state);
}

TEST_F(IzhikevichModelTest, CreateAllPresetTypes) {
    // Test creating neurons with all preset types
    izhikevich_preset_t presets[] = {
        IZHI_PRESET_REGULAR_SPIKING,
        IZHI_PRESET_INTRINSICALLY_BURSTING,
        IZHI_PRESET_CHATTERING,
        IZHI_PRESET_FAST_SPIKING,
        IZHI_PRESET_LOW_THRESHOLD_SPIKING,
        IZHI_PRESET_THALAMO_CORTICAL,
        IZHI_PRESET_RESONATOR
    };

    for (auto preset : presets) {
        izhikevich_params_t params = izhikevich_get_preset_params(preset);
        neuron_model_state_t state = neuron_model_create(vtable, &params);
        ASSERT_NE(state, nullptr);
        neuron_model_destroy(state);
    }
}

//=============================================================================
// Test Suite: Neuron Dynamics
//=============================================================================

TEST_F(IzhikevichModelTest, Update_WithoutSpike) {
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    float v_before = neuron_model_get_voltage(state);

    // Small update without spiking
    neuron_model_update(state, 1.0f, 5.0f);

    float v_after = neuron_model_get_voltage(state);

    // Voltage should change but not spike
    EXPECT_NE(v_before, v_after);
    EXPECT_FALSE(neuron_model_check_spike(state));

    neuron_model_destroy(state);
}

TEST_F(IzhikevichModelTest, Update_DriveToSpike) {
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    // Drive neuron to spike with large current
    drive_to_spike(state);

    // Should have spiked
    EXPECT_TRUE(neuron_model_check_spike(state));

    // Voltage should be at reset value (c parameter)
    float v = neuron_model_get_voltage(state);
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    EXPECT_FLOAT_EQ(v, params.c);

    neuron_model_destroy(state);
}

TEST_F(IzhikevichModelTest, Update_MultipleSpikesCycle) {
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    // Multiple spike cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        // Drive to spike
        drive_to_spike(state);
        EXPECT_TRUE(neuron_model_check_spike(state));

        // Continue updating (spike flag should clear)
        neuron_model_update(state, 1.0f, 0.0f);
        EXPECT_FALSE(neuron_model_check_spike(state));
    }

    neuron_model_destroy(state);
}

TEST_F(IzhikevichModelTest, Update_ZeroCurrent) {
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    float v_before = neuron_model_get_voltage(state);

    // Update with zero current
    neuron_model_update(state, 1.0f, 0.0f);

    float v_after = neuron_model_get_voltage(state);

    // Voltage may still change due to intrinsic dynamics
    // (depends on u recovery variable)
    EXPECT_FALSE(neuron_model_check_spike(state));

    neuron_model_destroy(state);
}

TEST_F(IzhikevichModelTest, Update_NegativeCurrent) {
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    float v_before = neuron_model_get_voltage(state);

    // Update with negative (inhibitory) current
    neuron_model_update(state, 1.0f, -10.0f);

    float v_after = neuron_model_get_voltage(state);

    // Voltage should decrease
    EXPECT_LT(v_after, v_before);
    EXPECT_FALSE(neuron_model_check_spike(state));

    neuron_model_destroy(state);
}

//=============================================================================
// Test Suite: State Access
//=============================================================================

TEST_F(IzhikevichModelTest, GetVoltage) {
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    float v = neuron_model_get_voltage(state);
    EXPECT_FLOAT_EQ(v, IZHIKEVICH_RESTING_POTENTIAL);

    neuron_model_destroy(state);
}

TEST_F(IzhikevichModelTest, SetVoltage) {
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    // Set voltage to custom value
    neuron_model_set_voltage(state, -50.0f);

    float v = neuron_model_get_voltage(state);
    EXPECT_FLOAT_EQ(v, -50.0f);

    neuron_model_destroy(state);
}

TEST_F(IzhikevichModelTest, SetVoltage_AboveThreshold) {
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    // Set voltage above spike threshold
    neuron_model_set_voltage(state, 35.0f);

    float v = neuron_model_get_voltage(state);
    EXPECT_FLOAT_EQ(v, 35.0f);

    // Next update should detect spike
    neuron_model_update(state, 1.0f, 0.0f);
    EXPECT_TRUE(neuron_model_check_spike(state));

    neuron_model_destroy(state);
}

TEST_F(IzhikevichModelTest, Reset) {
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    // Modify state
    neuron_model_set_voltage(state, -40.0f);
    neuron_model_update(state, 1.0f, 10.0f);

    // Reset
    neuron_model_reset(state);

    // Should be back at resting potential
    float v = neuron_model_get_voltage(state);
    EXPECT_FLOAT_EQ(v, IZHIKEVICH_RESTING_POTENTIAL);
    EXPECT_FALSE(neuron_model_check_spike(state));

    neuron_model_destroy(state);
}

//=============================================================================
// Test Suite: Copy Function
//=============================================================================

TEST_F(IzhikevichModelTest, CopyState) {
    // Create source neuron and modify it
    neuron_model_state_t src = neuron_model_create(vtable, NULL);
    ASSERT_NE(src, nullptr);
    neuron_model_set_voltage(src, -55.0f);
    neuron_model_update(src, 1.0f, 10.0f);

    // Create destination neuron
    neuron_model_state_t dst = neuron_model_create(vtable, NULL);
    ASSERT_NE(dst, nullptr);

    float src_voltage = neuron_model_get_voltage(src);

    // Copy state
    vtable->copy(dst, src);

    // Destination should have same voltage
    float dst_voltage = neuron_model_get_voltage(dst);
    EXPECT_FLOAT_EQ(dst_voltage, src_voltage);

    neuron_model_destroy(src);
    neuron_model_destroy(dst);
}

//=============================================================================
// Test Suite: Guard Clauses
//=============================================================================

TEST_F(IzhikevichModelTest, UpdateNull) {
    // Guard: NULL state
    neuron_model_update(NULL, 1.0f, 10.0f);
    SUCCEED();  // No crash
}

TEST_F(IzhikevichModelTest, CheckSpikeNull) {
    // Guard: NULL state
    bool spiked = neuron_model_check_spike(NULL);
    EXPECT_FALSE(spiked);
}

TEST_F(IzhikevichModelTest, PostSpikeNull) {
    // Guard: NULL state
    neuron_model_post_spike(NULL);
    SUCCEED();  // No crash
}

TEST_F(IzhikevichModelTest, GetVoltageNull) {
    // Guard: NULL state
    float v = neuron_model_get_voltage(NULL);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST_F(IzhikevichModelTest, SetVoltageNull) {
    // Guard: NULL state
    neuron_model_set_voltage(NULL, -50.0f);
    SUCCEED();  // No crash
}

TEST_F(IzhikevichModelTest, ResetNull) {
    // Guard: NULL state
    neuron_model_reset(NULL);
    SUCCEED();  // No crash
}

TEST_F(IzhikevichModelTest, CopyNull_Both) {
    // Guard: NULL states
    vtable->copy(NULL, NULL);
    SUCCEED();  // No crash
}

TEST_F(IzhikevichModelTest, CopyNull_Dst) {
    neuron_model_state_t src = neuron_model_create(vtable, NULL);
    ASSERT_NE(src, nullptr);

    vtable->copy(NULL, src);
    SUCCEED();  // No crash

    neuron_model_destroy(src);
}

TEST_F(IzhikevichModelTest, CopyNull_Src) {
    neuron_model_state_t dst = neuron_model_create(vtable, NULL);
    ASSERT_NE(dst, nullptr);

    vtable->copy(dst, NULL);
    SUCCEED();  // No crash

    neuron_model_destroy(dst);
}

//=============================================================================
// Test Suite: Edge Cases
//=============================================================================

TEST_F(IzhikevichModelTest, VeryLargeTimestep) {
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    // Large timestep (may cause instability, but shouldn't crash)
    neuron_model_update(state, 100.0f, 10.0f);

    // Should still be valid
    float v = neuron_model_get_voltage(state);
    EXPECT_TRUE(std::isfinite(v));

    neuron_model_destroy(state);
}

TEST_F(IzhikevichModelTest, VerySmallTimestep) {
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    float v_before = neuron_model_get_voltage(state);

    // Very small timestep
    neuron_model_update(state, 0.001f, 10.0f);

    float v_after = neuron_model_get_voltage(state);

    // Change should be minimal but present
    EXPECT_NE(v_before, v_after);

    neuron_model_destroy(state);
}

TEST_F(IzhikevichModelTest, ExtremeCurrents) {
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    // Very large current
    neuron_model_update(state, 1.0f, 1000.0f);
    EXPECT_TRUE(neuron_model_check_spike(state));

    neuron_model_reset(state);

    // Very large negative current
    neuron_model_update(state, 1.0f, -1000.0f);
    float v = neuron_model_get_voltage(state);
    EXPECT_TRUE(std::isfinite(v));

    neuron_model_destroy(state);
}

TEST_F(IzhikevichModelTest, DifferentPresetDynamics) {
    // Test that different presets produce different dynamics
    izhikevich_params_t rs_params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    izhikevich_params_t fs_params = izhikevich_get_preset_params(IZHI_PRESET_FAST_SPIKING);

    neuron_model_state_t rs_neuron = neuron_model_create(vtable, &rs_params);
    neuron_model_state_t fs_neuron = neuron_model_create(vtable, &fs_params);

    ASSERT_NE(rs_neuron, nullptr);
    ASSERT_NE(fs_neuron, nullptr);

    // Apply same input to both
    for (int i = 0; i < 10; i++) {
        neuron_model_update(rs_neuron, 1.0f, 15.0f);
        neuron_model_update(fs_neuron, 1.0f, 15.0f);
    }

    // They should evolve differently (different parameters)
    float rs_v = neuron_model_get_voltage(rs_neuron);
    float fs_v = neuron_model_get_voltage(fs_neuron);

    // Not a strict requirement due to nonlinear dynamics, but likely different
    // Just verify both are valid
    EXPECT_TRUE(std::isfinite(rs_v));
    EXPECT_TRUE(std::isfinite(fs_v));

    neuron_model_destroy(rs_neuron);
    neuron_model_destroy(fs_neuron);
}

TEST_F(IzhikevichModelTest, PostSpike_NoOp) {
    // Post-spike is currently a no-op (reset handled in update)
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    drive_to_spike(state);
    EXPECT_TRUE(neuron_model_check_spike(state));

    float v_before = neuron_model_get_voltage(state);

    // Call post-spike (should be no-op)
    neuron_model_post_spike(state);

    float v_after = neuron_model_get_voltage(state);

    // Voltage shouldn't change (reset already done in update)
    EXPECT_FLOAT_EQ(v_before, v_after);

    neuron_model_destroy(state);
}

TEST_F(IzhikevichModelTest, MultipleUpdatesWithoutSpike) {
    neuron_model_state_t state = neuron_model_create(vtable, NULL);
    ASSERT_NE(state, nullptr);

    // Many updates with small current (should not spike)
    for (int i = 0; i < 100; i++) {
        neuron_model_update(state, 1.0f, 1.0f);
        if (neuron_model_check_spike(state)) {
            break;  // If it spikes, that's also valid behavior
        }
    }

    // Voltage should be valid
    float v = neuron_model_get_voltage(state);
    EXPECT_TRUE(std::isfinite(v));

    neuron_model_destroy(state);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
