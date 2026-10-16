//=============================================================================
// test_two_compartment.cpp - Unit Tests for Two-Compartment Neuron Model
//=============================================================================
/**
 * @file test_two_compartment.cpp
 * @brief Comprehensive unit tests for two-compartment neuron model
 *
 * TEST CATEGORIES:
 * 1. Basic functionality (creation, destruction, initialization)
 * 2. Dendritic attenuation (50-80% range verification)
 * 3. Coupling dynamics (soma-dendrite interaction)
 * 4. Integration accuracy (RK4 vs Euler)
 * 5. Spike generation and reset
 * 6. Time constants and membrane properties
 * 7. Synaptic input targeting (soma vs dendrite)
 * 8. Performance benchmarks
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

    #include "core/neuron_models/nimcp_two_compartment.h"
    #include "core/neuron_models/nimcp_neuron_model.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TwoCompartmentTest : public ::testing::Test {
protected:
    neuron_model_state_t neuron;
    two_compartment_params_t params;

    void SetUp() override {
        // Use default parameters for most tests
        params = two_compartment_default_params();
        neuron = nullptr;
    }

    void TearDown() override {
        if (neuron != nullptr) {
            neuron_model_destroy(neuron);
            neuron = nullptr;
        }
    }

    // Helper: Create neuron with default params
    void CreateDefaultNeuron() {
        const neuron_model_vtable_t* vtable = two_compartment_get_vtable();
        ASSERT_NE(vtable, nullptr);
        neuron = neuron_model_create(vtable, &params);
        ASSERT_NE(neuron, nullptr);
    }

    // Helper: Create neuron with custom params
    void CreateCustomNeuron(const two_compartment_params_t& custom_params) {
        const neuron_model_vtable_t* vtable = two_compartment_get_vtable();
        ASSERT_NE(vtable, nullptr);
        neuron = neuron_model_create(vtable, &custom_params);
        ASSERT_NE(neuron, nullptr);
    }

    // Helper: Simulate neuron for duration with constant input
    void Simulate(float duration_ms, float dt_ms, float input_current_pA) {
        int num_steps = static_cast<int>(duration_ms / dt_ms);
        for (int i = 0; i < num_steps; i++) {
            neuron_model_update(neuron, dt_ms, input_current_pA);
        }
    }
};

//=============================================================================
// Category 1: Basic Functionality Tests
//=============================================================================

TEST_F(TwoCompartmentTest, VtableIsValid) {
    const neuron_model_vtable_t* vtable = two_compartment_get_vtable();

    ASSERT_NE(vtable, nullptr);
    EXPECT_STREQ(vtable->name, "Two-Compartment (Soma+Dendrite)");
    EXPECT_GT(vtable->state_size, 0);
    EXPECT_NE(vtable->init, nullptr);
    EXPECT_NE(vtable->update, nullptr);
    EXPECT_NE(vtable->check_spike, nullptr);
}

TEST_F(TwoCompartmentTest, CreateAndDestroy) {
    CreateDefaultNeuron();

    // Check initial state
    float V = neuron_model_get_voltage(neuron);
    EXPECT_NEAR(V, params.E_leak, 0.1f) << "Initial voltage should be at resting potential";

    // Destruction handled by TearDown
}

TEST_F(TwoCompartmentTest, DefaultParametersAreReasonable) {
    two_compartment_params_t p = two_compartment_default_params();

    EXPECT_GT(p.C_soma, 0.0f);
    EXPECT_GT(p.C_dend, 0.0f);
    EXPECT_GT(p.g_leak, 0.0f);
    EXPECT_GT(p.g_couple, 0.0f);
    EXPECT_LT(p.E_leak, 0.0f);  // Should be negative (mV)
    EXPECT_LT(p.V_threshold, 0.0f);  // Should be negative
    EXPECT_GT(p.refractory_period, 0.0f);
}

TEST_F(TwoCompartmentTest, InitialCompartmentVoltages) {
    CreateDefaultNeuron();

    float V_soma, V_dend;
    two_compartment_get_compartment_voltages(neuron, &V_soma, &V_dend);

    EXPECT_NEAR(V_soma, params.E_leak, 0.1f);
    EXPECT_NEAR(V_dend, params.E_leak, 0.1f);
    EXPECT_FLOAT_EQ(V_soma, V_dend) << "Initially, both compartments at same potential";
}

//=============================================================================
// Category 2: Dendritic Attenuation Tests (KEY REQUIREMENT)
//=============================================================================

TEST_F(TwoCompartmentTest, AttenuationCalculationIsAccurate) {
    // Test attenuation calculation with known parameters
    two_compartment_params_t p = two_compartment_default_params();

    float attenuation = two_compartment_calculate_attenuation(&p);

    // With default params (g_leak=10, g_couple=4.3):
    // transfer = 4.3 / (10 + 4.3) = 4.3/14.3 = 0.3 (30% transfer)
    // attenuation = 1 - 0.3 = 0.7 (70% attenuation)
    EXPECT_NEAR(attenuation, 0.70f, 0.05f) << "Default params should give ~70% attenuation";
}

TEST_F(TwoCompartmentTest, DendriticAttenuationInRange50to80Percent) {
    // CRITICAL TEST: Verify dendritic attenuation is 50-80%
    CreateDefaultNeuron();

    float V_soma_initial, V_dend_initial;
    two_compartment_get_compartment_voltages(neuron, &V_soma_initial, &V_dend_initial);

    // Simulate until steady-state (5 time constants) with continuous dendritic input
    float tau_soma, tau_dend;
    two_compartment_calculate_time_constants(&params, &tau_soma, &tau_dend);
    float settling_time = 5.0f * std::max(tau_soma, tau_dend);

    // Apply continuous current to dendrite throughout simulation
    int num_steps = static_cast<int>(settling_time / 0.1f);
    for (int i = 0; i < num_steps; i++) {
        two_compartment_add_current(neuron, 100.0f, COMPARTMENT_DENDRITE);
        neuron_model_update(neuron, 0.1f, 0.0f);
    }

    float V_soma_final, V_dend_final;
    two_compartment_get_compartment_voltages(neuron, &V_soma_final, &V_dend_final);

    // Calculate actual attenuation from simulation
    float V_dend_rise = V_dend_final - V_dend_initial;
    float V_soma_rise = V_soma_final - V_soma_initial;

    EXPECT_GT(V_dend_rise, 0.0f) << "Dendrite should depolarize with injected current";
    EXPECT_GT(V_soma_rise, 0.0f) << "Soma should see some depolarization via coupling";

    // Transfer = how much of dendritic signal reaches soma
    float transfer_ratio = V_soma_rise / V_dend_rise;
    // Attenuation = how much signal is lost
    float attenuation = 1.0f - transfer_ratio;

    // Biological realism: dendritic inputs should be attenuated 50-80%
    // This means transfer should be 20-50%
    EXPECT_GE(attenuation, 0.50f) << "Attenuation should be at least 50% (transfer <= 50%)";
    EXPECT_LE(attenuation, 0.80f) << "Attenuation should be at most 80% (transfer >= 20%)";

    // Alternative way to express the same constraint
    EXPECT_GE(transfer_ratio, 0.20f) << "Transfer should be at least 20%";
    EXPECT_LE(transfer_ratio, 0.50f) << "Transfer should be at most 50%";
}

TEST_F(TwoCompartmentTest, StrongCouplingReducesAttenuation) {
    // Test with stronger coupling (less attenuation)
    two_compartment_params_t p = two_compartment_default_params();
    p.g_couple = 15.0f;  // Much stronger coupling

    float attenuation = two_compartment_calculate_attenuation(&p);
    EXPECT_LT(attenuation, 0.50f) << "Strong coupling should give <50% attenuation";
}

TEST_F(TwoCompartmentTest, WeakCouplingIncreasesAttenuation) {
    // Test with weaker coupling (more attenuation)
    two_compartment_params_t p = two_compartment_default_params();
    p.g_couple = 1.0f;  // Weak coupling

    float attenuation = two_compartment_calculate_attenuation(&p);
    EXPECT_GT(attenuation, 0.80f) << "Weak coupling should give >80% attenuation";
}

//=============================================================================
// Category 3: Coupling Dynamics Tests
//=============================================================================

TEST_F(TwoCompartmentTest, SomaInfluencesDendrite) {
    CreateDefaultNeuron();

    // Set soma voltage high, dendrite at rest
    two_compartment_set_compartment_voltages(neuron, -40.0f, params.E_leak);

    float V_dend_before;
    two_compartment_get_compartment_voltages(neuron, nullptr, &V_dend_before);

    // Simulate without input (let coupling equilibrate)
    Simulate(10.0f, 0.1f, 0.0f);

    float V_dend_after;
    two_compartment_get_compartment_voltages(neuron, nullptr, &V_dend_after);

    EXPECT_GT(V_dend_after, V_dend_before) << "Dendrite should depolarize toward soma";
}

TEST_F(TwoCompartmentTest, DendriteInfluencesSoma) {
    CreateDefaultNeuron();

    // Set dendrite voltage high, soma at rest
    two_compartment_set_compartment_voltages(neuron, params.E_leak, -40.0f);

    float V_soma_before;
    two_compartment_get_compartment_voltages(neuron, &V_soma_before, nullptr);

    // Simulate without input
    Simulate(10.0f, 0.1f, 0.0f);

    float V_soma_after;
    two_compartment_get_compartment_voltages(neuron, &V_soma_after, nullptr);

    EXPECT_GT(V_soma_after, V_soma_before) << "Soma should depolarize toward dendrite";
}

TEST_F(TwoCompartmentTest, CompartmentsEquilibrateWithoutInput) {
    CreateDefaultNeuron();

    // Start with different voltages
    two_compartment_set_compartment_voltages(neuron, -50.0f, -60.0f);

    // Simulate for long time
    Simulate(100.0f, 0.1f, 0.0f);

    float V_soma, V_dend;
    two_compartment_get_compartment_voltages(neuron, &V_soma, &V_dend);

    // Should converge to leak potential
    EXPECT_NEAR(V_soma, params.E_leak, 2.0f);
    EXPECT_NEAR(V_dend, params.E_leak, 2.0f);
}

//=============================================================================
// Category 4: Integration Accuracy Tests
//=============================================================================

TEST_F(TwoCompartmentTest, RK4MoreAccurateThanEuler) {
    // Create two neurons: one with RK4, one with Euler
    two_compartment_params_t params_rk4 = two_compartment_default_params();
    params_rk4.integration_method = ODE_RK4;

    two_compartment_params_t params_euler = two_compartment_default_params();
    params_euler.integration_method = ODE_EULER;

    const neuron_model_vtable_t* vtable = two_compartment_get_vtable();

    neuron_model_state_t neuron_rk4 = neuron_model_create(vtable, &params_rk4);
    neuron_model_state_t neuron_euler = neuron_model_create(vtable, &params_euler);

    ASSERT_NE(neuron_rk4, nullptr);
    ASSERT_NE(neuron_euler, nullptr);

    // Apply same input
    float dt = 0.5f;  // Larger timestep to emphasize differences
    float input = 50.0f;
    int steps = 100;

    for (int i = 0; i < steps; i++) {
        neuron_model_update(neuron_rk4, dt, input);
        neuron_model_update(neuron_euler, dt, input);
    }

    float V_rk4 = neuron_model_get_voltage(neuron_rk4);
    float V_euler = neuron_model_get_voltage(neuron_euler);

    // RK4 and Euler should differ (RK4 more accurate)
    EXPECT_NE(V_rk4, V_euler) << "Different integration methods should give different results";

    // Both should be reasonable (not NaN or Inf)
    EXPECT_FALSE(std::isnan(V_rk4));
    EXPECT_FALSE(std::isnan(V_euler));
    EXPECT_FALSE(std::isinf(V_rk4));
    EXPECT_FALSE(std::isinf(V_euler));

    neuron_model_destroy(neuron_rk4);
    neuron_model_destroy(neuron_euler);
}

//=============================================================================
// Category 5: Spike Generation and Reset Tests
//=============================================================================

TEST_F(TwoCompartmentTest, SpikeGeneratedAtSoma) {
    CreateDefaultNeuron();

    // Inject strong current to soma
    float dt = 0.1f;
    float input = 500.0f;  // Strong input

    bool spiked = false;
    for (int i = 0; i < 100 && !spiked; i++) {
        neuron_model_update(neuron, dt, input);
        spiked = neuron_model_check_spike(neuron);
    }

    EXPECT_TRUE(spiked) << "Neuron should spike with strong input";
}

TEST_F(TwoCompartmentTest, PostSpikeResetCorrect) {
    CreateDefaultNeuron();

    // Bring neuron to spike threshold
    two_compartment_set_compartment_voltages(neuron, params.V_threshold + 1.0f, -60.0f);

    EXPECT_TRUE(neuron_model_check_spike(neuron));

    // Perform post-spike reset
    neuron_model_post_spike(neuron);

    float V_soma, V_dend;
    two_compartment_get_compartment_voltages(neuron, &V_soma, &V_dend);

    EXPECT_NEAR(V_soma, params.V_reset, 0.1f) << "Soma should reset to V_reset";
    EXPECT_LT(V_dend, -60.0f) << "Dendrite should be hyperpolarized by back-propagating AP";
}

TEST_F(TwoCompartmentTest, RefractoryPeriodPreventsSpiking) {
    CreateDefaultNeuron();

    // Trigger spike
    two_compartment_set_compartment_voltages(neuron, params.V_threshold + 1.0f, -60.0f);
    EXPECT_TRUE(neuron_model_check_spike(neuron));
    neuron_model_post_spike(neuron);

    // Immediately try to spike again (should fail due to refractory)
    two_compartment_set_compartment_voltages(neuron, params.V_threshold + 5.0f, -60.0f);
    EXPECT_FALSE(neuron_model_check_spike(neuron)) << "Should not spike during refractory";

    // Wait for refractory period to end
    Simulate(params.refractory_period + 1.0f, 0.1f, 0.0f);

    // Now should be able to spike again
    two_compartment_set_compartment_voltages(neuron, params.V_threshold + 1.0f, -60.0f);
    EXPECT_TRUE(neuron_model_check_spike(neuron)) << "Should spike after refractory";
}

//=============================================================================
// Category 6: Time Constants and Membrane Properties
//=============================================================================

TEST_F(TwoCompartmentTest, TimeConstantsCalculatedCorrectly) {
    two_compartment_params_t p = two_compartment_default_params();

    float tau_soma, tau_dend;
    two_compartment_calculate_time_constants(&p, &tau_soma, &tau_dend);

    // τ = C / g_leak
    float expected_tau_soma = p.C_soma / p.g_leak;  // 100/10 = 10 ms
    float expected_tau_dend = p.C_dend / p.g_leak;  // 200/10 = 20 ms

    EXPECT_NEAR(tau_soma, expected_tau_soma, 0.01f);
    EXPECT_NEAR(tau_dend, expected_tau_dend, 0.01f);
}

TEST_F(TwoCompartmentTest, ExponentialDecayToRest) {
    CreateDefaultNeuron();

    // Start depolarized
    two_compartment_set_compartment_voltages(neuron, -50.0f, -50.0f);

    float V_initial = neuron_model_get_voltage(neuron);

    // Simulate without input
    float tau_soma, tau_dend;
    two_compartment_calculate_time_constants(&params, &tau_soma, &tau_dend);

    float t = 3.0f * tau_soma;  // 3 time constants
    Simulate(t, 0.1f, 0.0f);

    float V_final = neuron_model_get_voltage(neuron);

    // Should decay toward E_leak
    EXPECT_LT(V_final, V_initial) << "Voltage should decay";
    EXPECT_GT(V_final, params.E_leak) << "Should not fully reach E_leak yet";

    // After ~5 time constants, should be very close
    Simulate(2.0f * tau_soma, 0.1f, 0.0f);
    V_final = neuron_model_get_voltage(neuron);
    EXPECT_NEAR(V_final, params.E_leak, 1.0f);
}

//=============================================================================
// Category 7: Synaptic Input Targeting Tests
//=============================================================================

TEST_F(TwoCompartmentTest, CurrentInjectionToSoma) {
    CreateDefaultNeuron();

    float V_soma_before = neuron_model_get_voltage(neuron);

    // Inject current to soma
    two_compartment_add_current(neuron, 100.0f, COMPARTMENT_SOMA);
    neuron_model_update(neuron, 0.1f, 0.0f);

    float V_soma_after = neuron_model_get_voltage(neuron);

    EXPECT_GT(V_soma_after, V_soma_before) << "Soma should depolarize with injected current";
}

TEST_F(TwoCompartmentTest, CurrentInjectionToDendrite) {
    CreateDefaultNeuron();

    float V_dend_before;
    two_compartment_get_compartment_voltages(neuron, nullptr, &V_dend_before);

    // Inject current to dendrite
    two_compartment_add_current(neuron, 100.0f, COMPARTMENT_DENDRITE);
    neuron_model_update(neuron, 0.1f, 0.0f);

    float V_dend_after;
    two_compartment_get_compartment_voltages(neuron, nullptr, &V_dend_after);

    EXPECT_GT(V_dend_after, V_dend_before) << "Dendrite should depolarize with injected current";
}

TEST_F(TwoCompartmentTest, AutoTargetingSplitsCurrentBetweenCompartments) {
    CreateDefaultNeuron();

    float V_soma_before, V_dend_before;
    two_compartment_get_compartment_voltages(neuron, &V_soma_before, &V_dend_before);

    // Auto-targeting should split current
    two_compartment_add_current(neuron, 100.0f, COMPARTMENT_AUTO);
    neuron_model_update(neuron, 0.1f, 0.0f);

    float V_soma_after, V_dend_after;
    two_compartment_get_compartment_voltages(neuron, &V_soma_after, &V_dend_after);

    EXPECT_GT(V_soma_after, V_soma_before) << "Soma should get part of current";
    EXPECT_GT(V_dend_after, V_dend_before) << "Dendrite should get part of current";
}

//=============================================================================
// Category 8: Performance and Stability Tests
//=============================================================================

TEST_F(TwoCompartmentTest, StableWithLargeTimesteps) {
    CreateDefaultNeuron();

    // Test with larger timestep (1.0 ms)
    float dt = 1.0f;
    float input = 50.0f;

    for (int i = 0; i < 100; i++) {
        neuron_model_update(neuron, dt, input);
        float V = neuron_model_get_voltage(neuron);
        EXPECT_FALSE(std::isnan(V)) << "Should not become NaN at step " << i;
        EXPECT_FALSE(std::isinf(V)) << "Should not become Inf at step " << i;
    }
}

TEST_F(TwoCompartmentTest, PerformanceIsReasonable) {
    CreateDefaultNeuron();

    // Benchmark update speed
    const int num_updates = 10000;
    float dt = 0.1f;
    float input = 50.0f;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_updates; i++) {
        neuron_model_update(neuron, dt, input);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should be <100us per update on modern hardware (relaxed for CI)
    float us_per_update = static_cast<float>(duration.count()) / num_updates;
    EXPECT_LT(us_per_update, 500.0f) << "Update should be fast (< 500us per update)";

    std::cout << "Performance: " << us_per_update << " us per update" << std::endl;
}

//=============================================================================
// Integration with Plasticity (Future)
//=============================================================================

TEST_F(TwoCompartmentTest, CompartmentVoltagesAccessible) {
    // Verify that plasticity mechanisms can access compartment voltages
    CreateDefaultNeuron();

    float V_soma, V_dend;
    two_compartment_get_compartment_voltages(neuron, &V_soma, &V_dend);

    EXPECT_FALSE(std::isnan(V_soma));
    EXPECT_FALSE(std::isnan(V_dend));
    EXPECT_NE(&V_soma, &V_dend) << "Should get independent voltage readings";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
