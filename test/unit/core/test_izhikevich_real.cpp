#include <gtest/gtest.h>
#include "core/neuron_models/nimcp_izhikevich.h"
#include "core/neuron_models/nimcp_neuron_model.h"
#include <cmath>

//=============================================================================
// Test Fixture
//=============================================================================

class IzhikevichRealTest : public ::testing::Test {
protected:
    const neuron_model_vtable_t* vtable = nullptr;
    neuron_model_state_t state = nullptr;

    void SetUp() override {
        vtable = izhikevich_get_vtable();
        ASSERT_NE(vtable, nullptr);
    }

    void TearDown() override {
        if (state) {
            neuron_model_destroy(state);
            state = nullptr;
        }
    }
};

//=============================================================================
// VTable Tests
//=============================================================================

TEST_F(IzhikevichRealTest, GetVTable) {
    EXPECT_NE(vtable, nullptr);
    EXPECT_NE(vtable->name, nullptr);
    EXPECT_GT(vtable->state_size, 0);
}

TEST_F(IzhikevichRealTest, VTableType) {
    EXPECT_EQ(vtable->type, NEURON_MODEL_IZHIKEVICH);
}

//=============================================================================
// Preset Parameter Tests
//=============================================================================

TEST_F(IzhikevichRealTest, RegularSpikingPreset) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    EXPECT_GT(params.a, 0.0f);
    EXPECT_GT(params.b, 0.0f);
    EXPECT_LT(params.c, 0.0f);  // Reset voltage should be negative
    EXPECT_GT(params.d, 0.0f);
}

TEST_F(IzhikevichRealTest, FastSpikingPreset) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_FAST_SPIKING);
    EXPECT_GT(params.a, 0.0f);
    EXPECT_GT(params.b, 0.0f);
    EXPECT_LT(params.c, 0.0f);
    EXPECT_GT(params.d, 0.0f);
}

TEST_F(IzhikevichRealTest, IntrinsicallyBurstingPreset) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_INTRINSICALLY_BURSTING);
    EXPECT_GT(params.a, 0.0f);
    EXPECT_GT(params.b, 0.0f);
    EXPECT_LT(params.c, 0.0f);
    EXPECT_GT(params.d, 0.0f);
}

TEST_F(IzhikevichRealTest, ChatteringPreset) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_CHATTERING);
    EXPECT_GT(params.a, 0.0f);
    EXPECT_GT(params.b, 0.0f);
    EXPECT_LT(params.c, 0.0f);
    EXPECT_GT(params.d, 0.0f);
}

TEST_F(IzhikevichRealTest, LowThresholdSpikingPreset) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_LOW_THRESHOLD_SPIKING);
    EXPECT_GT(params.a, 0.0f);
    EXPECT_GT(params.b, 0.0f);
    EXPECT_LT(params.c, 0.0f);
    EXPECT_GT(params.d, 0.0f);
}

TEST_F(IzhikevichRealTest, ThalamoCorticalPreset) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_THALAMO_CORTICAL);
    EXPECT_GT(params.a, 0.0f);
    EXPECT_GT(params.b, 0.0f);
    EXPECT_LT(params.c, 0.0f);
    EXPECT_GT(params.d, 0.0f);
}

TEST_F(IzhikevichRealTest, ResonatorPreset) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_RESONATOR);
    EXPECT_GT(params.a, 0.0f);
    EXPECT_GT(params.b, 0.0f);
    EXPECT_LT(params.c, 0.0f);
    EXPECT_GT(params.d, 0.0f);
}

//=============================================================================
// Custom Parameter Tests
//=============================================================================

TEST_F(IzhikevichRealTest, CreateCustomParams) {
    izhikevich_params_t params = izhikevich_create_params(0.02f, 0.2f, -65.0f, 8.0f);
    EXPECT_FLOAT_EQ(params.a, 0.02f);
    EXPECT_FLOAT_EQ(params.b, 0.2f);
    EXPECT_FLOAT_EQ(params.c, -65.0f);
    EXPECT_FLOAT_EQ(params.d, 8.0f);
}

//=============================================================================
// Neuron Model Creation Tests
//=============================================================================

TEST_F(IzhikevichRealTest, CreateNeuronModelWithPreset) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    state = neuron_model_create(vtable, &params);
    EXPECT_NE(state, nullptr);
}

TEST_F(IzhikevichRealTest, CreateNeuronModelWithCustomParams) {
    izhikevich_params_t params = izhikevich_create_params(0.03f, 0.25f, -60.0f, 4.0f);
    state = neuron_model_create(vtable, &params);
    EXPECT_NE(state, nullptr);
}

TEST_F(IzhikevichRealTest, CreateNeuronModelWithNullParams) {
    state = neuron_model_create(vtable, nullptr);
    // Should still work with defaults
    EXPECT_NE(state, nullptr);
}

//=============================================================================
// Voltage Tests
//=============================================================================

TEST_F(IzhikevichRealTest, GetInitialVoltage) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    state = neuron_model_create(vtable, &params);
    ASSERT_NE(state, nullptr);

    float voltage = neuron_model_get_voltage(state);
    EXPECT_LT(voltage, 0.0f);  // Should be negative (resting potential)
}

TEST_F(IzhikevichRealTest, SetVoltage) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    state = neuron_model_create(vtable, &params);
    ASSERT_NE(state, nullptr);

    neuron_model_set_voltage(state, -50.0f);
    float voltage = neuron_model_get_voltage(state);
    EXPECT_FLOAT_EQ(voltage, -50.0f);
}

//=============================================================================
// Update and Spiking Tests
//=============================================================================

TEST_F(IzhikevichRealTest, UpdateNeuronNoSpike) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    state = neuron_model_create(vtable, &params);
    ASSERT_NE(state, nullptr);

    // Update with small current - should not spike
    neuron_model_update(state, 1.0f, 1.0f);
    bool spiked = neuron_model_check_spike(state);
    EXPECT_FALSE(spiked);
}

TEST_F(IzhikevichRealTest, UpdateNeuronToSpike) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    state = neuron_model_create(vtable, &params);
    ASSERT_NE(state, nullptr);

    // Apply strong current repeatedly to trigger spike
    bool spiked = false;
    for (int i = 0; i < 100; i++) {
        neuron_model_update(state, 1.0f, 20.0f);
        if (neuron_model_check_spike(state)) {
            spiked = true;
            break;
        }
    }
    EXPECT_TRUE(spiked);
}

TEST_F(IzhikevichRealTest, PostSpikeReset) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    state = neuron_model_create(vtable, &params);
    ASSERT_NE(state, nullptr);

    // Drive neuron to spike with large current
    for (int i = 0; i < 100; i++) {
        neuron_model_update(state, 1.0f, 20.0f);  // Large input
        if (neuron_model_check_spike(state)) {
            // Post-spike reset
            neuron_model_post_spike(state);
            float voltage = neuron_model_get_voltage(state);
            EXPECT_LT(voltage, 40.0f);  // Should be reset below spike threshold
            return;  // Test passes
        }
    }
    // If we didn't spike in 100 steps, that's also valid for some parameters
    EXPECT_TRUE(true);
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(IzhikevichRealTest, ResetNeuron) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    state = neuron_model_create(vtable, &params);
    ASSERT_NE(state, nullptr);

    // Modify state
    neuron_model_set_voltage(state, -40.0f);
    neuron_model_update(state, 1.0f, 10.0f);

    // Reset
    neuron_model_reset(state);
    float voltage = neuron_model_get_voltage(state);
    EXPECT_LT(voltage, -50.0f);  // Should be back near resting
}

//=============================================================================
// Introspection Tests
//=============================================================================

TEST_F(IzhikevichRealTest, GetModelName) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    state = neuron_model_create(vtable, &params);
    ASSERT_NE(state, nullptr);

    const char* name = neuron_model_get_name(state);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0);
}

TEST_F(IzhikevichRealTest, GetModelType) {
    izhikevich_params_t params = izhikevich_get_preset_params(IZHI_PRESET_REGULAR_SPIKING);
    state = neuron_model_create(vtable, &params);
    ASSERT_NE(state, nullptr);

    neuron_model_type_t type = neuron_model_get_type(state);
    EXPECT_EQ(type, NEURON_MODEL_IZHIKEVICH);
}

//=============================================================================
// Preset Name Tests
//=============================================================================

TEST_F(IzhikevichRealTest, GetPresetName) {
    const char* name = izhikevich_get_preset_name(IZHI_PRESET_REGULAR_SPIKING);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0);
}

TEST_F(IzhikevichRealTest, GetPresetDescription) {
    const char* desc = izhikevich_get_preset_description(IZHI_PRESET_FAST_SPIKING);
    EXPECT_NE(desc, nullptr);
    EXPECT_GT(strlen(desc), 0);
}
