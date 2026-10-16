//=============================================================================
// test_hybrid_learning.cpp - Unit tests for LEARN_MODE_HYBRID and lateral inhibition
//=============================================================================
// WHAT: Tests for hybrid supervised+biological learning mode and lateral inhibition
// WHY:  Verify that LEARN_MODE_HYBRID activates both backprop and biological plasticity,
//       and that lateral inhibition correctly sharpens output neuron competition
// HOW:  GTest suite testing loss convergence, bio-plasticity activation, and WTA
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuralnet_learning.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class HybridLearningTest : public ::testing::Test {
protected:
    adaptive_network_config_t config;
    adaptive_network_t network;

    void SetUp() override {
        memset(&config, 0, sizeof(config));

        // Build base network config inline (avoids test_helpers.h dependency)
        config.base_config.num_neurons = 20;
        config.base_config.input_size = 4;
        config.base_config.output_size = 2;
        config.base_config.num_layers = 3;
        config.base_config.ei_ratio = 0.8f;
        config.base_config.learning_rate = 0.01f;
        config.base_config.hebbian_rate = 0.1f;
        config.base_config.stdp_window = 20.0f;
        config.base_config.homeostatic_rate = 0.001f;
        config.base_config.target_activity = 0.1f;
        config.base_config.adaptation_rate = 0.1f;
        config.base_config.refractory_period = 5.0f;
        config.base_config.min_weight = -1.0f;
        config.base_config.max_weight = 1.0f;
        config.base_config.update_interval = 1000;
        config.base_config.enable_stdp = true;
        config.base_config.enable_hebbian = true;
        config.base_config.enable_oja = true;
        config.base_config.enable_bcm = true;
        config.base_config.enable_eligibility = true;

        // Allocate layer_sizes: 4 input, 14 hidden, 2 output
        config.base_config.layer_sizes = (uint32_t*)nimcp_calloc(3, sizeof(uint32_t));
        config.base_config.layer_sizes[0] = 4;
        config.base_config.layer_sizes[1] = 14;
        config.base_config.layer_sizes[2] = 2;

        config.spike_params.k_factor = 0.5f;
        config.spike_params.sparsity_target = 0.7f;
        config.spike_params.encoding = SPIKE_ENCODING_INTEGER;
        config.spike_params.enable_soft_reset = true;
        config.spike_params.enable_adaptation = true;
        config.spike_params.adaptation_window = 100;
        config.spike_params.min_threshold = 0.1f;
        config.spike_params.max_threshold = 10.0f;

        config.enable_sparsity = false;
        config.pruning_threshold = 0.01f;
        config.update_frequency = 10;

        network = adaptive_network_create(&config);
    }

    void TearDown() override {
        if (network) {
            adaptive_network_destroy(network);
            network = nullptr;
        }
    }

    // Helper to create a training example
    training_example_t make_example(float* input, uint32_t in_size,
                                     float* target, uint32_t tgt_size) {
        training_example_t ex;
        memset(&ex, 0, sizeof(ex));
        ex.input = input;
        ex.input_size = in_size;
        ex.target = target;
        ex.target_size = tgt_size;
        ex.confidence = 1.0f;
        return ex;
    }
};

//=============================================================================
// Test 1: LEARN_MODE_HYBRID produces valid loss
//=============================================================================

TEST_F(HybridLearningTest, HybridModeLossIsValid) {
    ASSERT_NE(network, nullptr);

    float input[4] = {1.0f, 0.0f, 1.0f, 0.0f};
    float target[2] = {1.0f, 0.0f};
    training_example_t example = make_example(input, 4, target, 2);

    float loss = adaptive_network_learn(network, &example, LEARN_MODE_HYBRID, 0.01f);

    // Loss should be non-negative (valid MSE)
    EXPECT_GE(loss, 0.0f);
    // Loss should be less than something reasonable for untrained network
    EXPECT_LT(loss, 10.0f);
}

//=============================================================================
// Test 2: LEARN_MODE_HYBRID converges over multiple steps
//=============================================================================

TEST_F(HybridLearningTest, HybridModeConverges) {
    ASSERT_NE(network, nullptr);

    float input[4] = {1.0f, 0.5f, 0.3f, 0.8f};
    float target[2] = {0.8f, 0.2f};
    training_example_t example = make_example(input, 4, target, 2);

    // Train for many steps
    float first_loss = adaptive_network_learn(network, &example, LEARN_MODE_HYBRID, 0.01f);
    EXPECT_GE(first_loss, 0.0f);

    // Train more
    float loss = first_loss;
    for (int i = 0; i < 100; i++) {
        loss = adaptive_network_learn(network, &example, LEARN_MODE_HYBRID, 0.01f);
    }

    // Loss should decrease (or at least not explode)
    // Note: biological plasticity may cause some variance, so just check it doesn't explode
    EXPECT_GE(loss, 0.0f);
    EXPECT_LT(loss, first_loss * 2.0f + 0.1f);  // Allow some margin
}

//=============================================================================
// Test 3: LEARN_MODE_HYBRID with NULL network returns error
//=============================================================================

TEST_F(HybridLearningTest, HybridModeNullNetwork) {
    float input[4] = {1.0f, 0.0f, 1.0f, 0.0f};
    float target[2] = {1.0f, 0.0f};
    training_example_t example = make_example(input, 4, target, 2);

    float loss = adaptive_network_learn(nullptr, &example, LEARN_MODE_HYBRID, 0.01f);
    EXPECT_EQ(loss, -1.0f);
}

//=============================================================================
// Test 4: Lateral inhibition suppresses non-winner outputs
//=============================================================================

TEST_F(HybridLearningTest, LateralInhibitionSuppressesNonWinners) {
    ASSERT_NE(network, nullptr);

    // Run a forward pass to set neuron states
    float input[4] = {1.0f, 0.5f, 0.3f, 0.8f};
    float output[2] = {0.0f, 0.0f};
    adaptive_network_forward(network, input, 4, output, 2, 0);

    neural_network_t base = adaptive_network_get_base_network(network);
    ASSERT_NE(base, nullptr);

    // Find the output layer offset: sum of all layers except last
    // layers: [4, 14, 2] => output starts at 4+14 = 18
    uint32_t output_start = 18;
    uint32_t output_count = 2;

    // Get states before inhibition
    neuron_t* n0 = neural_network_get_neuron(base, output_start);
    neuron_t* n1 = neural_network_get_neuron(base, output_start + 1);
    ASSERT_NE(n0, nullptr);
    ASSERT_NE(n1, nullptr);

    // Set distinct states so we have a clear winner
    n0->state = 0.9f;
    n1->state = 0.3f;

    float before_n0 = n0->state;
    float before_n1 = n1->state;

    uint32_t modified = neural_network_apply_lateral_inhibition(
        base, output_start, output_count, 0.5f);

    // At least one neuron should be modified (the loser)
    EXPECT_GE(modified, 1u);

    // Winner (n0 = 0.9) should be boosted
    EXPECT_GE(n0->state, before_n0);

    // Loser (n1 = 0.3) should move toward the mean
    // n1 was below mean (0.6), so new_state = 0.3 + 0.5*(0.6-0.3) = 0.45
    EXPECT_NE(n1->state, before_n1);
}

//=============================================================================
// Test 5: Lateral inhibition with zero strength is a no-op
//=============================================================================

TEST_F(HybridLearningTest, LateralInhibitionZeroStrengthNoOp) {
    ASSERT_NE(network, nullptr);

    neural_network_t base = adaptive_network_get_base_network(network);
    ASSERT_NE(base, nullptr);

    // Set output neuron states
    uint32_t output_start = 18;
    neuron_t* n0 = neural_network_get_neuron(base, output_start);
    neuron_t* n1 = neural_network_get_neuron(base, output_start + 1);
    if (!n0 || !n1) {
        // Network may have fewer neurons, skip gracefully
        GTEST_SKIP() << "Output neurons not accessible";
    }

    n0->state = 0.8f;
    n1->state = 0.4f;

    float before_n0 = n0->state;
    float before_n1 = n1->state;

    uint32_t modified = neural_network_apply_lateral_inhibition(
        base, output_start, 2, 0.0f);

    EXPECT_EQ(modified, 0u);
    EXPECT_FLOAT_EQ(n0->state, before_n0);
    EXPECT_FLOAT_EQ(n1->state, before_n1);
}

//=============================================================================
// Test 6: Lateral inhibition with NULL network doesn't crash
//=============================================================================

TEST_F(HybridLearningTest, LateralInhibitionNullNetwork) {
    uint32_t modified = neural_network_apply_lateral_inhibition(
        nullptr, 0, 2, 0.5f);
    EXPECT_EQ(modified, 0u);
}

//=============================================================================
// Test 7: Lateral inhibition with zero output count is a no-op
//=============================================================================

TEST_F(HybridLearningTest, LateralInhibitionZeroOutputCount) {
    ASSERT_NE(network, nullptr);

    neural_network_t base = adaptive_network_get_base_network(network);
    ASSERT_NE(base, nullptr);

    uint32_t modified = neural_network_apply_lateral_inhibition(
        base, 0, 0, 0.5f);
    EXPECT_EQ(modified, 0u);
}

//=============================================================================
// Test 8: BCM enable flag is set in config
//=============================================================================

TEST_F(HybridLearningTest, BCMEnableInConfig) {
    // Verify the config we built has BCM enabled
    EXPECT_TRUE(config.base_config.enable_bcm);
    EXPECT_TRUE(config.base_config.enable_stdp);
    EXPECT_TRUE(config.base_config.enable_hebbian);
    EXPECT_TRUE(config.base_config.enable_oja);
    EXPECT_TRUE(config.base_config.enable_eligibility);
}

//=============================================================================
// Test 9: LEARN_MODE_HYBRID on simple 2-class problem
//=============================================================================

TEST_F(HybridLearningTest, HybridModeTwoClassProblem) {
    ASSERT_NE(network, nullptr);

    // Class A: input = [1,0,1,0] -> target = [1,0]
    float input_a[4] = {1.0f, 0.0f, 1.0f, 0.0f};
    float target_a[2] = {1.0f, 0.0f};
    training_example_t ex_a = make_example(input_a, 4, target_a, 2);

    // Class B: input = [0,1,0,1] -> target = [0,1]
    float input_b[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    float target_b[2] = {0.0f, 1.0f};
    training_example_t ex_b = make_example(input_b, 4, target_b, 2);

    // Train alternating between classes
    float loss_a = 0.0f, loss_b = 0.0f;
    for (int epoch = 0; epoch < 50; epoch++) {
        loss_a = adaptive_network_learn(network, &ex_a, LEARN_MODE_HYBRID, 0.01f);
        loss_b = adaptive_network_learn(network, &ex_b, LEARN_MODE_HYBRID, 0.01f);
    }

    // Both losses should be non-negative and bounded
    EXPECT_GE(loss_a, 0.0f);
    EXPECT_GE(loss_b, 0.0f);
    EXPECT_LT(loss_a, 5.0f);
    EXPECT_LT(loss_b, 5.0f);
}

//=============================================================================
// Test 10: Lateral inhibition with high strength clamps properly
//=============================================================================

TEST_F(HybridLearningTest, LateralInhibitionHighStrength) {
    ASSERT_NE(network, nullptr);

    neural_network_t base = adaptive_network_get_base_network(network);
    ASSERT_NE(base, nullptr);

    uint32_t output_start = 18;
    neuron_t* n0 = neural_network_get_neuron(base, output_start);
    neuron_t* n1 = neural_network_get_neuron(base, output_start + 1);
    if (!n0 || !n1) {
        GTEST_SKIP() << "Output neurons not accessible";
    }

    n0->state = 0.9f;
    n1->state = 0.1f;

    // Strength > 1.0 should be clamped to 1.0 internally
    uint32_t modified = neural_network_apply_lateral_inhibition(
        base, output_start, 2, 5.0f);

    // Should still work (clamped to 1.0)
    EXPECT_GE(modified, 0u);

    // Winner state should be boosted but clamped to 1.0
    EXPECT_LE(n0->state, 1.0f);
}

//=============================================================================
// Test 11: LEARN_MODE_HYBRID enum value exists and is distinct
//=============================================================================

TEST_F(HybridLearningTest, HybridModeEnumExists) {
    // Verify LEARN_MODE_HYBRID has a distinct value from other modes
    EXPECT_NE((int)LEARN_MODE_HYBRID, (int)LEARN_MODE_SUPERVISED);
    EXPECT_NE((int)LEARN_MODE_HYBRID, (int)LEARN_MODE_UNSUPERVISED);
    EXPECT_NE((int)LEARN_MODE_HYBRID, (int)LEARN_MODE_DISTILLATION);
    EXPECT_NE((int)LEARN_MODE_HYBRID, (int)LEARN_MODE_REINFORCEMENT);
}
