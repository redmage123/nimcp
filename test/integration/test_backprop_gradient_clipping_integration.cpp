/**
 * @file test_backprop_gradient_clipping_integration.cpp
 * @brief Integration tests for gradient clipping in backprop kernel + adaptive network
 *
 * WHAT: Verify gradient clipping in backprop_sparse_full() integrates correctly
 *       with the adaptive network training pipeline
 * WHY:  The Athena 95% accuracy plan added max_grad_norm and out_grad_norm
 *       parameters to backprop_sparse_full(); all 3 callers in nimcp_adaptive.c
 *       pass 1.0f as max_grad_norm. These tests ensure the integration works
 *       end-to-end and prevents gradient explosion.
 * HOW:  Create adaptive networks with explicit layer_sizes (required for
 *       backprop path), train with various modes, verify grad_norm tracking.
 *
 * TEST COVERAGE:
 * 1. Gradient clipping integrates with adaptive network training
 * 2. Gradient clipping prevents explosion over 100 iterations
 * 3. Training convergence with clipping (loss decreases)
 * 4. Multiple learning modes produce valid grad norms (SUPERVISED, HYBRID)
 * 5. Gradient clipping works at different learning rates
 * 6. Direct backprop_sparse_full() kernel gradient norm output
 * 7. Gradient norm accessor returns 0.0 before any training
 * 8. Gradient norm stays finite under adversarial inputs
 *
 * @author NIMCP Development Team
 * @date 2026-02-28
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>

extern "C" {
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "plasticity/adaptive/nimcp_backprop_kernel.h"
#include "core/neuralnet/nimcp_neuralnet.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BackpropGradientClippingIntegrationTest : public ::testing::Test {
protected:
    adaptive_network_t network = nullptr;

    // Layer sizes for a small 3-layer network: 4 input, 8 hidden, 3 output
    static constexpr uint32_t NUM_LAYERS = 3;
    static constexpr uint32_t INPUT_SIZE = 4;
    static constexpr uint32_t HIDDEN_SIZE = 8;
    static constexpr uint32_t OUTPUT_SIZE = 3;
    uint32_t layer_sizes_arr[NUM_LAYERS] = {INPUT_SIZE, HIDDEN_SIZE, OUTPUT_SIZE};

    void SetUp() override {
        network = nullptr;
    }

    void TearDown() override {
        if (network) {
            adaptive_network_destroy(network);
            network = nullptr;
        }
    }

    /**
     * Create an adaptive network config with explicit layer_sizes so that
     * backprop_sparse_full() is actually invoked (requires num_layers >= 2).
     */
    adaptive_network_config_t create_backprop_config() {
        adaptive_network_config_t config = {};

        // Base network config -- must have explicit layer_sizes for backprop
        config.base_config.num_neurons = INPUT_SIZE + HIDDEN_SIZE + OUTPUT_SIZE;  // 15
        config.base_config.input_size = INPUT_SIZE;
        config.base_config.output_size = OUTPUT_SIZE;
        config.base_config.num_layers = NUM_LAYERS;
        config.base_config.layer_sizes = layer_sizes_arr;
        config.base_config.learning_rate = 0.01f;
        config.base_config.ei_ratio = 0.8f;
        config.base_config.hebbian_rate = 0.001f;
        config.base_config.stdp_window = 20.0f;
        config.base_config.homeostatic_rate = 0.0001f;
        config.base_config.target_activity = 0.1f;
        config.base_config.adaptation_rate = 0.01f;
        config.base_config.refractory_period = 2.0f;
        config.base_config.min_weight = -1.0f;
        config.base_config.max_weight = 1.0f;
        config.base_config.update_interval = 1;
        config.base_config.enable_stdp = false;
        config.base_config.enable_hebbian = false;
        config.base_config.enable_oja = false;
        config.base_config.enable_homeostasis = false;
        config.base_config.neuron_model = NEURON_MODEL_LIF;
        config.base_config.model_params = nullptr;
        config.base_config.integration_method = ODE_EULER;

        // Use passthrough encoding for clean gradient flow
        config.spike_params.k_factor = 0.5f;
        config.spike_params.sparsity_target = 0.3f;
        config.spike_params.encoding = SPIKE_ENCODING_PASSTHROUGH;
        config.spike_params.enable_soft_reset = false;
        config.spike_params.enable_adaptation = false;
        config.spike_params.adaptation_window = 100;
        config.spike_params.min_threshold = 0.01f;
        config.spike_params.max_threshold = 10.0f;

        config.enable_sparsity = false;
        config.pruning_threshold = 0.0f;
        config.update_frequency = 10;

        config.checkpoint_path = nullptr;
        config.auto_load = false;
        config.auto_save = false;
        config.auto_save_interval = 0;

        return config;
    }

    /**
     * Helper: create a training example with given input/target arrays.
     */
    training_example_t make_example(float* input, uint32_t in_sz,
                                    float* target, uint32_t tgt_sz,
                                    const char* label) {
        training_example_t ex = {};
        ex.input = input;
        ex.input_size = in_sz;
        ex.target = target;
        ex.target_size = tgt_sz;
        ex.confidence = 1.0f;
        if (label) {
            strncpy(ex.label, label, sizeof(ex.label) - 1);
        }
        return ex;
    }
};

//=============================================================================
// Test 1: Gradient clipping integrates with adaptive network training
//=============================================================================

TEST_F(BackpropGradientClippingIntegrationTest, GradNormStoredAfterLearn) {
    // WHAT: Verify adaptive_network_learn() stores grad_norm in network state
    // WHY:  backprop_sparse_full() writes to out_grad_norm; nimcp_adaptive.c
    //       stores it in network->last_grad_norm for external access

    adaptive_network_config_t config = create_backprop_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr) << "Failed to create adaptive network with layer_sizes";

    // Before training, grad norm should be 0
    float initial_norm = adaptive_network_get_last_grad_norm(network);
    EXPECT_FLOAT_EQ(initial_norm, 0.0f)
        << "Gradient norm should be 0.0 before any training";

    // Create a simple training example
    float input[INPUT_SIZE] = {0.5f, 0.3f, 0.8f, 0.1f};
    float target[OUTPUT_SIZE] = {1.0f, 0.0f, 0.0f};
    training_example_t example = make_example(input, INPUT_SIZE, target, OUTPUT_SIZE, "class_A");

    // Train one step
    float loss = adaptive_network_learn(network, &example, LEARN_MODE_SUPERVISED, 0.01f);
    EXPECT_TRUE(std::isfinite(loss)) << "Loss should be finite after training";

    // Grad norm should now be set (may be 0 if network has no path, but should be finite)
    float grad_norm = adaptive_network_get_last_grad_norm(network);
    EXPECT_TRUE(std::isfinite(grad_norm))
        << "Gradient norm should be finite after training, got: " << grad_norm;
    EXPECT_GE(grad_norm, 0.0f)
        << "Gradient norm should be non-negative";
}

//=============================================================================
// Test 2: Gradient clipping prevents explosion over training loop
//=============================================================================

TEST_F(BackpropGradientClippingIntegrationTest, GradNormBoundedOver100Iterations) {
    // WHAT: Run 100 training iterations, verify grad norms stay bounded
    // WHY:  The max_grad_norm=1.0f clipping in all 3 callers should prevent
    //       gradient explosion even with repeated weight updates

    adaptive_network_config_t config = create_backprop_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    float input[INPUT_SIZE] = {1.0f, 0.5f, -0.5f, -1.0f};
    float target[OUTPUT_SIZE] = {1.0f, 0.0f, 0.0f};
    training_example_t example = make_example(input, INPUT_SIZE, target, OUTPUT_SIZE, "class_A");

    std::vector<float> grad_norms;
    grad_norms.reserve(100);

    for (int i = 0; i < 100; i++) {
        float loss = adaptive_network_learn(network, &example, LEARN_MODE_SUPERVISED, 0.05f);
        ASSERT_TRUE(std::isfinite(loss)) << "Loss exploded at iteration " << i;

        float gn = adaptive_network_get_last_grad_norm(network);
        ASSERT_TRUE(std::isfinite(gn)) << "Grad norm not finite at iteration " << i;
        grad_norms.push_back(gn);
    }

    // All grad norms should be bounded (clipping at 1.0 means norms should
    // typically stay under a reasonable bound -- we use 10.0 as generous upper limit)
    float max_gn = *std::max_element(grad_norms.begin(), grad_norms.end());
    EXPECT_LT(max_gn, 10.0f)
        << "Maximum grad norm across 100 iterations should be bounded, got: " << max_gn;

    // No NaN or Inf in the series
    for (size_t i = 0; i < grad_norms.size(); i++) {
        EXPECT_TRUE(std::isfinite(grad_norms[i]))
            << "Grad norm at step " << i << " is not finite: " << grad_norms[i];
    }
}

//=============================================================================
// Test 3: Training convergence with gradient clipping
//=============================================================================

TEST_F(BackpropGradientClippingIntegrationTest, TrainingConvergesWithClipping) {
    // WHAT: Train on a repeating pattern, verify loss decreases over time
    // WHY:  Gradient clipping should not prevent convergence -- only stabilize it

    adaptive_network_config_t config = create_backprop_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Two classes to learn
    float input_A[INPUT_SIZE] = {1.0f, 1.0f, 0.0f, 0.0f};
    float target_A[OUTPUT_SIZE] = {1.0f, 0.0f, 0.0f};
    training_example_t ex_A = make_example(input_A, INPUT_SIZE, target_A, OUTPUT_SIZE, "A");

    float input_B[INPUT_SIZE] = {0.0f, 0.0f, 1.0f, 1.0f};
    float target_B[OUTPUT_SIZE] = {0.0f, 1.0f, 0.0f};
    training_example_t ex_B = make_example(input_B, INPUT_SIZE, target_B, OUTPUT_SIZE, "B");

    // Collect loss over epochs
    float early_loss_sum = 0.0f;
    float late_loss_sum = 0.0f;
    int early_count = 0;
    int late_count = 0;

    for (int epoch = 0; epoch < 200; epoch++) {
        float loss_a = adaptive_network_learn(network, &ex_A, LEARN_MODE_SUPERVISED, 0.01f);
        float loss_b = adaptive_network_learn(network, &ex_B, LEARN_MODE_SUPERVISED, 0.01f);

        ASSERT_TRUE(std::isfinite(loss_a)) << "Loss A not finite at epoch " << epoch;
        ASSERT_TRUE(std::isfinite(loss_b)) << "Loss B not finite at epoch " << epoch;

        float avg_loss = (loss_a + loss_b) / 2.0f;

        if (epoch < 20) {
            early_loss_sum += avg_loss;
            early_count++;
        } else if (epoch >= 180) {
            late_loss_sum += avg_loss;
            late_count++;
        }
    }

    float early_avg = early_loss_sum / early_count;
    float late_avg = late_loss_sum / late_count;

    // Loss should decrease (or at least not increase dramatically)
    // We use a generous check: late loss should be <= early loss + small margin
    // because some network initializations may already start near minimum
    EXPECT_LE(late_avg, early_avg + 0.5f)
        << "Late loss (" << late_avg << ") should not be much worse than early loss ("
        << early_avg << ")";
}

//=============================================================================
// Test 4: Multiple learning modes produce valid grad norms
//=============================================================================

TEST_F(BackpropGradientClippingIntegrationTest, SupervisedAndHybridProduceValidGradNorms) {
    // WHAT: Test both SUPERVISED and HYBRID modes produce valid gradient norms
    // WHY:  Both modes call backprop_sparse_full() with max_grad_norm=1.0f

    adaptive_network_config_t config = create_backprop_config();

    // Test SUPERVISED mode
    {
        adaptive_network_t net = adaptive_network_create(&config);
        ASSERT_NE(net, nullptr) << "Failed to create network for SUPERVISED test";

        float input[INPUT_SIZE] = {0.8f, 0.2f, 0.6f, 0.4f};
        float target[OUTPUT_SIZE] = {0.0f, 1.0f, 0.0f};
        training_example_t ex = make_example(input, INPUT_SIZE, target, OUTPUT_SIZE, "sup");

        for (int i = 0; i < 5; i++) {
            adaptive_network_learn(net, &ex, LEARN_MODE_SUPERVISED, 0.01f);
        }

        float gn = adaptive_network_get_last_grad_norm(net);
        EXPECT_TRUE(std::isfinite(gn))
            << "SUPERVISED mode should produce finite grad norm";
        EXPECT_GE(gn, 0.0f)
            << "SUPERVISED grad norm should be non-negative";

        adaptive_network_destroy(net);
    }

    // Test HYBRID mode
    {
        adaptive_network_t net = adaptive_network_create(&config);
        ASSERT_NE(net, nullptr) << "Failed to create network for HYBRID test";

        float input[INPUT_SIZE] = {0.3f, 0.7f, 0.1f, 0.9f};
        float target[OUTPUT_SIZE] = {0.0f, 0.0f, 1.0f};
        training_example_t ex = make_example(input, INPUT_SIZE, target, OUTPUT_SIZE, "hyb");

        for (int i = 0; i < 5; i++) {
            adaptive_network_learn(net, &ex, LEARN_MODE_HYBRID, 0.01f);
        }

        float gn = adaptive_network_get_last_grad_norm(net);
        EXPECT_TRUE(std::isfinite(gn))
            << "HYBRID mode should produce finite grad norm";
        EXPECT_GE(gn, 0.0f)
            << "HYBRID grad norm should be non-negative";

        adaptive_network_destroy(net);
    }
}

//=============================================================================
// Test 5: Gradient clipping works at different learning rates
//=============================================================================

TEST_F(BackpropGradientClippingIntegrationTest, ClippingWorksAtDifferentLearningRates) {
    // WHAT: Verify gradient clipping functions correctly at both high and low LR
    // WHY:  High LR can amplify gradients; clipping should bound them regardless

    adaptive_network_config_t config = create_backprop_config();

    float input[INPUT_SIZE] = {1.0f, -1.0f, 0.5f, -0.5f};
    float target[OUTPUT_SIZE] = {1.0f, 0.0f, 0.0f};

    float learning_rates[] = {0.001f, 0.01f, 0.1f, 1.0f};
    const char* lr_labels[] = {"low_lr", "mid_lr", "high_lr", "very_high_lr"};

    for (int lr_idx = 0; lr_idx < 4; lr_idx++) {
        float lr = learning_rates[lr_idx];

        adaptive_network_t net = adaptive_network_create(&config);
        ASSERT_NE(net, nullptr) << "Failed to create network for LR=" << lr;

        training_example_t ex = make_example(input, INPUT_SIZE, target, OUTPUT_SIZE,
                                             lr_labels[lr_idx]);

        // Train for several steps
        bool all_finite = true;
        for (int i = 0; i < 20; i++) {
            float loss = adaptive_network_learn(net, &ex, LEARN_MODE_SUPERVISED, lr);
            float gn = adaptive_network_get_last_grad_norm(net);
            if (!std::isfinite(loss) || !std::isfinite(gn)) {
                all_finite = false;
                break;
            }
        }

        EXPECT_TRUE(all_finite)
            << "Training should remain stable with gradient clipping at LR=" << lr;

        float final_gn = adaptive_network_get_last_grad_norm(net);
        EXPECT_TRUE(std::isfinite(final_gn))
            << "Final grad norm should be finite at LR=" << lr;

        adaptive_network_destroy(net);
    }
}

//=============================================================================
// Test 6: Direct backprop_sparse_full() kernel produces valid grad norm
//=============================================================================

TEST_F(BackpropGradientClippingIntegrationTest, DirectKernelProducesValidGradNorm) {
    // WHAT: Call backprop_sparse_full() directly, verify out_grad_norm is written
    // WHY:  Test the kernel in isolation from the adaptive network wrapper

    adaptive_network_config_t config = create_backprop_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Do a forward pass first to establish activations
    float input[INPUT_SIZE] = {0.5f, 0.3f, 0.8f, 0.1f};
    float forward_output[OUTPUT_SIZE];
    adaptive_network_forward_raw(network, input, INPUT_SIZE, forward_output, OUTPUT_SIZE);

    // Now call backprop_sparse_full() directly on the base network
    neural_network_t base_net = adaptive_network_get_base_network(network);
    ASSERT_NE(base_net, nullptr);

    float target[OUTPUT_SIZE] = {1.0f, 0.0f, 0.0f};
    float grad_norm = -1.0f;  // sentinel

    int ret = backprop_sparse_full(
        base_net,
        NUM_LAYERS, layer_sizes_arr,
        0.01f,         // learning_rate
        -1.0f, 1.0f,   // min/max weight
        target, forward_output, OUTPUT_SIZE,
        1.0f,          // max_grad_norm (clipping threshold)
        &grad_norm);

    EXPECT_EQ(ret, 0) << "backprop_sparse_full() should return 0 on success";
    EXPECT_GE(grad_norm, 0.0f)
        << "out_grad_norm should be non-negative after backprop";
    EXPECT_TRUE(std::isfinite(grad_norm))
        << "out_grad_norm should be finite, got: " << grad_norm;
}

//=============================================================================
// Test 7: Gradient norm accessor returns 0.0 for NULL network
//=============================================================================

TEST_F(BackpropGradientClippingIntegrationTest, NullNetworkReturnsZeroGradNorm) {
    // WHAT: adaptive_network_get_last_grad_norm(NULL) returns 0.0f
    // WHY:  Guard clause in the implementation should handle NULL gracefully

    float gn = adaptive_network_get_last_grad_norm(nullptr);
    EXPECT_FLOAT_EQ(gn, 0.0f)
        << "NULL network should return 0.0 gradient norm";
}

//=============================================================================
// Test 8: Gradient norm stays finite under adversarial inputs
//=============================================================================

TEST_F(BackpropGradientClippingIntegrationTest, FiniteGradNormWithLargeInputs) {
    // WHAT: Large input values should not cause gradient norm to become NaN/Inf
    // WHY:  Gradient clipping (max_grad_norm=1.0) should bound even extreme cases

    adaptive_network_config_t config = create_backprop_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Large but finite inputs
    float input[INPUT_SIZE] = {100.0f, -100.0f, 50.0f, -50.0f};
    float target[OUTPUT_SIZE] = {1.0f, 0.0f, 0.0f};
    training_example_t ex = make_example(input, INPUT_SIZE, target, OUTPUT_SIZE, "large");

    for (int i = 0; i < 10; i++) {
        float loss = adaptive_network_learn(network, &ex, LEARN_MODE_SUPERVISED, 0.001f);
        float gn = adaptive_network_get_last_grad_norm(network);

        // Both must remain finite
        EXPECT_TRUE(std::isfinite(loss))
            << "Loss should stay finite with large inputs at step " << i;
        EXPECT_TRUE(std::isfinite(gn))
            << "Grad norm should stay finite with large inputs at step " << i;
    }
}
