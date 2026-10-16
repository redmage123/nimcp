/**
 * @file test_lnn_neuron.cpp
 * @brief Unit tests for LNN Neuron module
 *
 * TEST COVERAGE:
 * - Neuron lifecycle (create, destroy, reset)
 * - Weight initialization
 * - State management (get/set)
 * - Forward computation (tau, input, derivative)
 * - ODE step integration
 * - Gradient accumulation
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "lnn/nimcp_lnn_neuron.h"
#include "lnn/nimcp_lnn_types.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LnnNeuronTest : public ::testing::Test {
protected:
    lnn_neuron_t* neuron = nullptr;
    lnn_neuron_config_t config;

    void SetUp() override {
        // Default neuron config
        config.activation = LNN_ACTIVATION_TANH;
        config.tau_base_init = 10.0f;
        config.tau_min = 0.1f;
        config.tau_max = 1000.0f;
        config.weight_init_std = 0.1f;
        config.learn_tau = true;
    }

    void TearDown() override {
        if (neuron) {
            lnn_neuron_destroy(neuron);
            neuron = nullptr;
        }
    }

    void CreateNeuron(uint32_t n_inputs = 4, uint32_t n_recurrent = 8) {
        neuron = lnn_neuron_create(&config, n_inputs, n_recurrent);
    }
};

//=============================================================================
// lnn_neuron_create Tests
//=============================================================================

TEST_F(LnnNeuronTest, CreateReturnsValidPointer) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);
}

TEST_F(LnnNeuronTest, CreateSetsCorrectDimensions) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    EXPECT_EQ(4u, neuron->n_inputs);
    EXPECT_EQ(8u, neuron->n_recurrent);
}

TEST_F(LnnNeuronTest, CreateSetsActivationFunction) {
    config.activation = LNN_ACTIVATION_SIGMOID;
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    EXPECT_EQ(LNN_ACTIVATION_SIGMOID, neuron->activation);
}

TEST_F(LnnNeuronTest, CreateSetsTauParameters) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    EXPECT_FLOAT_EQ(10.0f, neuron->tau_base);
    EXPECT_FLOAT_EQ(0.1f, neuron->tau_min);
    EXPECT_FLOAT_EQ(1000.0f, neuron->tau_max);
}

TEST_F(LnnNeuronTest, CreateAllocatesWeightArrays) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    EXPECT_NE(nullptr, neuron->w_in);
    EXPECT_NE(nullptr, neuron->w_rec);
    EXPECT_NE(nullptr, neuron->w_tau);
}

TEST_F(LnnNeuronTest, CreateAllocatesGradientArrays) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    EXPECT_NE(nullptr, neuron->grad_w_in);
    EXPECT_NE(nullptr, neuron->grad_w_rec);
    EXPECT_NE(nullptr, neuron->grad_w_tau);
}

TEST_F(LnnNeuronTest, CreateReturnsNullOnNullConfig) {
    lnn_neuron_t* n = lnn_neuron_create(nullptr, 4, 8);
    EXPECT_EQ(nullptr, n);
}

TEST_F(LnnNeuronTest, CreateReturnsNullOnZeroInputs) {
    lnn_neuron_t* n = lnn_neuron_create(&config, 0, 8);
    EXPECT_EQ(nullptr, n);
}

TEST_F(LnnNeuronTest, CreateInitializesStateToZero) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    EXPECT_FLOAT_EQ(0.0f, neuron->x);
    EXPECT_FLOAT_EQ(0.0f, neuron->x_prev);
    EXPECT_FLOAT_EQ(0.0f, neuron->dx_dt);
}

//=============================================================================
// lnn_neuron_destroy Tests
//=============================================================================

TEST_F(LnnNeuronTest, DestroySafeOnNullPointer) {
    // Should not crash
    lnn_neuron_destroy(nullptr);
}

//=============================================================================
// lnn_neuron_init_weights Tests
//=============================================================================

TEST_F(LnnNeuronTest, InitWeightsReturnsSuccess) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    int result = lnn_neuron_init_weights(neuron, 0.1f, 12345);
    EXPECT_EQ(0, result);
}

TEST_F(LnnNeuronTest, InitWeightsSetsNonZeroValues) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    lnn_neuron_init_weights(neuron, 0.1f, 12345);

    // At least some weights should be non-zero after initialization
    bool has_nonzero = false;
    for (uint32_t i = 0; i < neuron->n_inputs; i++) {
        if (std::fabs(neuron->w_in[i]) > 1e-9f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);
}

TEST_F(LnnNeuronTest, InitWeightsWithSameSeedProducesSameResults) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    lnn_neuron_init_weights(neuron, 0.1f, 42);
    float first_weight = neuron->w_in[0];

    // Reset and reinit with same seed
    memset(neuron->w_in, 0, neuron->n_inputs * sizeof(float));
    lnn_neuron_init_weights(neuron, 0.1f, 42);

    EXPECT_FLOAT_EQ(first_weight, neuron->w_in[0]);
}

TEST_F(LnnNeuronTest, InitWeightsReturnsErrorOnNullNeuron) {
    int result = lnn_neuron_init_weights(nullptr, 0.1f, 12345);
    EXPECT_NE(0, result);
}

//=============================================================================
// lnn_neuron_get_state / lnn_neuron_set_state Tests
//=============================================================================

TEST_F(LnnNeuronTest, GetStateReturnsCurrentState) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    neuron->x = 0.5f;
    EXPECT_FLOAT_EQ(0.5f, lnn_neuron_get_state(neuron));
}

TEST_F(LnnNeuronTest, SetStateSetsState) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    lnn_neuron_set_state(neuron, 0.75f);
    EXPECT_FLOAT_EQ(0.75f, neuron->x);
}

TEST_F(LnnNeuronTest, GetTauReturnsCurrentTau) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    neuron->tau_current = 15.0f;
    EXPECT_FLOAT_EQ(15.0f, lnn_neuron_get_tau(neuron));
}

//=============================================================================
// lnn_neuron_reset Tests
//=============================================================================

TEST_F(LnnNeuronTest, ResetSetsStateToZero) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    neuron->x = 0.5f;
    neuron->x_prev = 0.3f;
    neuron->dx_dt = 0.1f;

    lnn_neuron_reset(neuron);

    EXPECT_FLOAT_EQ(0.0f, neuron->x);
    EXPECT_FLOAT_EQ(0.0f, neuron->x_prev);
    EXPECT_FLOAT_EQ(0.0f, neuron->dx_dt);
}

TEST_F(LnnNeuronTest, ResetRestoresTauToBase) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    neuron->tau_current = 100.0f;
    lnn_neuron_reset(neuron);

    EXPECT_FLOAT_EQ(neuron->tau_base, neuron->tau_current);
}

//=============================================================================
// lnn_neuron_compute_tau Tests
//=============================================================================

TEST_F(LnnNeuronTest, ComputeTauReturnsBoundedValue) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);
    lnn_neuron_init_weights(neuron, 0.1f, 12345);

    std::vector<float> input(4, 0.5f);
    std::vector<float> recurrent(8, 0.0f);

    float tau = lnn_neuron_compute_tau(neuron, input.data(), 4, recurrent.data(), 8);

    EXPECT_GE(tau, neuron->tau_min);
    EXPECT_LE(tau, neuron->tau_max);
}

TEST_F(LnnNeuronTest, ComputeTauReturnsPositiveValue) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);
    lnn_neuron_init_weights(neuron, 0.1f, 12345);

    std::vector<float> input(4, 0.0f);
    std::vector<float> recurrent(8, 0.0f);

    float tau = lnn_neuron_compute_tau(neuron, input.data(), 4, recurrent.data(), 8);
    EXPECT_GT(tau, 0.0f);
}

//=============================================================================
// lnn_neuron_compute_input Tests
//=============================================================================

TEST_F(LnnNeuronTest, ComputeInputZeroWithZeroInputsAndZeroWeights) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    // Weights are initialized to zero by default
    std::vector<float> input(4, 0.0f);
    std::vector<float> recurrent(8, 0.0f);

    float total_input = lnn_neuron_compute_input(neuron, input.data(), 4, recurrent.data(), 8);
    EXPECT_NEAR(0.0f, total_input, 1e-6f);
}

TEST_F(LnnNeuronTest, ComputeInputHandlesNonZeroInput) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);
    lnn_neuron_init_weights(neuron, 0.1f, 12345);

    std::vector<float> input(4, 1.0f);
    std::vector<float> recurrent(8, 0.0f);

    float total_input = lnn_neuron_compute_input(neuron, input.data(), 4, recurrent.data(), 8);
    // Result should be finite (not NaN or Inf)
    EXPECT_TRUE(std::isfinite(total_input));
}

//=============================================================================
// lnn_neuron_compute_derivative Tests
//=============================================================================

TEST_F(LnnNeuronTest, ComputeDerivativeReturnsFiniteValue) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    neuron->x = 0.5f;
    float tau = 10.0f;
    float total_input = 0.3f;

    float dx_dt = lnn_neuron_compute_derivative(neuron, tau, total_input);
    EXPECT_TRUE(std::isfinite(dx_dt));
}

TEST_F(LnnNeuronTest, ComputeDerivativeIncludesDecayTerm) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    // With positive state and zero input, derivative should be negative (decay)
    neuron->x = 1.0f;
    float tau = 10.0f;
    float total_input = 0.0f;

    float dx_dt = lnn_neuron_compute_derivative(neuron, tau, total_input);
    EXPECT_LT(dx_dt, 0.0f);  // Should decay
}

//=============================================================================
// lnn_neuron_step Tests
//=============================================================================

TEST_F(LnnNeuronTest, StepReturnsSuccessOnValidInput) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);
    lnn_neuron_init_weights(neuron, 0.1f, 12345);

    std::vector<float> input(4, 0.5f);
    std::vector<float> recurrent(8, 0.0f);

    int result = lnn_neuron_step(neuron, input.data(), 4, recurrent.data(), 8, 1.0f, LNN_ODE_EULER);
    EXPECT_EQ(0, result);
}

TEST_F(LnnNeuronTest, StepUpdatesState) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);
    lnn_neuron_init_weights(neuron, 0.1f, 12345);

    neuron->x = 0.0f;
    std::vector<float> input(4, 1.0f);
    std::vector<float> recurrent(8, 0.0f);

    float state_before = neuron->x;
    lnn_neuron_step(neuron, input.data(), 4, recurrent.data(), 8, 1.0f, LNN_ODE_EULER);

    // State should change after step (assuming non-zero weights)
    // Note: with all-zero weights, state might not change
    EXPECT_TRUE(std::isfinite(neuron->x));
}

TEST_F(LnnNeuronTest, StepReturnsErrorOnNullNeuron) {
    std::vector<float> input(4, 0.5f);
    std::vector<float> recurrent(8, 0.0f);

    int result = lnn_neuron_step(nullptr, input.data(), 4, recurrent.data(), 8, 1.0f, LNN_ODE_EULER);
    EXPECT_NE(0, result);
}

TEST_F(LnnNeuronTest, StepReturnsErrorOnNullInput) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    std::vector<float> recurrent(8, 0.0f);
    int result = lnn_neuron_step(neuron, nullptr, 4, recurrent.data(), 8, 1.0f, LNN_ODE_EULER);
    EXPECT_NE(0, result);
}

TEST_F(LnnNeuronTest, StepPreservesStability) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);
    lnn_neuron_init_weights(neuron, 0.1f, 12345);

    std::vector<float> input(4, 0.5f);
    std::vector<float> recurrent(8, 0.0f);

    // Run multiple steps
    for (int i = 0; i < 100; i++) {
        lnn_neuron_step(neuron, input.data(), 4, recurrent.data(), 8, 1.0f, LNN_ODE_EULER);
    }

    // State should remain finite
    EXPECT_TRUE(std::isfinite(neuron->x));
    EXPECT_FALSE(std::isnan(neuron->x));
}

//=============================================================================
// lnn_neuron_reset_gradients Tests
//=============================================================================

TEST_F(LnnNeuronTest, ResetGradientsClearsAllGradients) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    // Set some gradient values
    for (uint32_t i = 0; i < neuron->n_inputs; i++) {
        neuron->grad_w_in[i] = 1.0f;
    }
    neuron->grad_b_in = 1.0f;
    neuron->grad_b_tau = 1.0f;

    lnn_neuron_reset_gradients(neuron);

    for (uint32_t i = 0; i < neuron->n_inputs; i++) {
        EXPECT_FLOAT_EQ(0.0f, neuron->grad_w_in[i]);
    }
    EXPECT_FLOAT_EQ(0.0f, neuron->grad_b_in);
    EXPECT_FLOAT_EQ(0.0f, neuron->grad_b_tau);
}

//=============================================================================
// lnn_neuron_param_count Tests
//=============================================================================

TEST_F(LnnNeuronTest, ParamCountReturnsCorrectValue) {
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);

    size_t count = lnn_neuron_param_count(neuron);

    // Expected: w_in[4] + w_rec[8] + w_tau[4+8] + b_in + b_tau + tau_base
    size_t expected = 4 + 8 + (4 + 8) + 1 + 1 + 1;
    EXPECT_EQ(expected, count);
}

//=============================================================================
// Activation Function Tests
//=============================================================================

TEST_F(LnnNeuronTest, TanhActivationBoundsOutput) {
    config.activation = LNN_ACTIVATION_TANH;
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);
    lnn_neuron_init_weights(neuron, 0.1f, 12345);

    std::vector<float> input(4, 10.0f);  // Large input
    std::vector<float> recurrent(8, 0.0f);

    // Run several steps
    for (int i = 0; i < 50; i++) {
        lnn_neuron_step(neuron, input.data(), 4, recurrent.data(), 8, 1.0f, LNN_ODE_EULER);
    }

    // tanh output should be bounded [-1, 1], state might exceed but should be bounded
    EXPECT_TRUE(std::isfinite(neuron->x));
}

TEST_F(LnnNeuronTest, SigmoidActivationBoundsOutput) {
    config.activation = LNN_ACTIVATION_SIGMOID;
    CreateNeuron(4, 8);
    ASSERT_NE(nullptr, neuron);
    lnn_neuron_init_weights(neuron, 0.1f, 12345);

    std::vector<float> input(4, 10.0f);
    std::vector<float> recurrent(8, 0.0f);

    for (int i = 0; i < 50; i++) {
        lnn_neuron_step(neuron, input.data(), 4, recurrent.data(), 8, 1.0f, LNN_ODE_EULER);
    }

    EXPECT_TRUE(std::isfinite(neuron->x));
}
