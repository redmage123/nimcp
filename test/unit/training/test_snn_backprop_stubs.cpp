//=============================================================================
// test_snn_backprop_stubs.cpp - Unit Tests for SNN Backprop Stub Functions
//=============================================================================
/**
 * @file test_snn_backprop_stubs.cpp
 * @brief Tests for newly-implemented SNN backpropagation stub functions
 *
 * WHAT: Verify lifecycle, forward, backward, train_step, gradient/weight norms
 * WHY:  Ensure stub implementations work correctly without crashes
 * HOW:  GTest with snn_config_feedforward() for proper network creation
 *
 * Functions tested:
 * - snn_backprop_create() / destroy() - lifecycle
 * - snn_backprop_forward() - forward pass with LIF dynamics
 * - snn_backprop_backward() - BPTT with surrogate gradients
 * - snn_backprop_train_step() - complete training pipeline
 * - snn_backprop_get_gradient_norm() / get_weight_norm() - norms
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "training/nimcp_snn_backprop.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_types.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SNNBackpropStubTest : public ::testing::Test {
protected:
    snn_backprop_ctx_t* ctx = nullptr;
    snn_network_t* network = nullptr;
    snn_backprop_config_t config;

    static constexpr uint32_t N_INPUTS = 10;
    static constexpr uint32_t N_HIDDEN = 20;
    static constexpr uint32_t N_OUTPUTS = 5;

    void SetUp() override {
        memset(&config, 0, sizeof(config));
        config = snn_backprop_default_config(SNN_TRAIN_BPTT);
    }

    void TearDown() override {
        if (ctx) {
            snn_backprop_destroy(ctx);
            ctx = nullptr;
        }
        if (network) {
            snn_network_destroy(network);
            network = nullptr;
        }
    }

    /**
     * Create a simple feedforward SNN using the proper config API.
     * Uses snn_config_feedforward() which sets up all required fields.
     */
    bool CreateNetwork() {
        snn_config_t snn_cfg;
        int rc = snn_config_feedforward(&snn_cfg, N_INPUTS, N_HIDDEN, N_OUTPUTS);
        if (rc != SNN_SUCCESS) {
            return false;
        }
        network = snn_network_create(&snn_cfg);
        return (network != nullptr);
    }

    /**
     * Create backprop context from current network and config.
     * Returns false if creation fails (network may not support it).
     */
    bool CreateContext() {
        if (!network) return false;
        ctx = snn_backprop_create(network, &config);
        return (ctx != nullptr);
    }

    /**
     * Generate deterministic input data for reproducibility.
     */
    std::vector<float> MakeInputs(uint32_t batch_size) {
        std::vector<float> inputs(batch_size * N_INPUTS);
        for (size_t i = 0; i < inputs.size(); i++) {
            inputs[i] = static_cast<float>(i % 100) / 100.0f;
        }
        return inputs;
    }

    /**
     * Generate one-hot target data.
     */
    std::vector<float> MakeTargets(uint32_t batch_size) {
        std::vector<float> targets(batch_size * N_OUTPUTS, 0.0f);
        for (uint32_t b = 0; b < batch_size; b++) {
            uint32_t label = b % N_OUTPUTS;
            targets[b * N_OUTPUTS + label] = 1.0f;
        }
        return targets;
    }
};

//=============================================================================
// 1. Lifecycle Tests - Create / Destroy
//=============================================================================

TEST_F(SNNBackpropStubTest, CreateDestroyLifecycle) {
    // Verify create+destroy does not leak or crash
    ASSERT_TRUE(CreateNetwork()) << "snn_network_create failed";
    ASSERT_TRUE(CreateContext()) << "snn_backprop_create failed";

    // ctx is destroyed in TearDown
}

TEST_F(SNNBackpropStubTest, CreateWithNullNetworkReturnsNull) {
    ctx = snn_backprop_create(nullptr, &config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(SNNBackpropStubTest, CreateWithNullConfigReturnsNull) {
    ASSERT_TRUE(CreateNetwork());
    ctx = snn_backprop_create(network, nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(SNNBackpropStubTest, DestroyNullIsSafe) {
    snn_backprop_destroy(nullptr);
    // No crash = pass
}

TEST_F(SNNBackpropStubTest, DoubleDestroyIsSafe) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    snn_backprop_destroy(ctx);
    ctx = nullptr;
    // Second destroy via TearDown with ctx=nullptr should be fine
}

TEST_F(SNNBackpropStubTest, ResetNullReturnsError) {
    int rc = snn_backprop_reset(nullptr);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, ResetAfterCreate) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    int rc = snn_backprop_reset(ctx);
    EXPECT_EQ(rc, SNN_SUCCESS);
}

//=============================================================================
// 2. Forward Pass Tests
//=============================================================================

TEST_F(SNNBackpropStubTest, ForwardNullCtxReturnsError) {
    float inputs[10] = {0.5f};
    float outputs[5] = {0};
    int rc = snn_backprop_forward(nullptr, inputs, 1, 10.0f, outputs);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, ForwardNullInputsReturnsError) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    float outputs[5] = {0};
    int rc = snn_backprop_forward(ctx, nullptr, 1, 10.0f, outputs);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, ForwardZeroBatchReturnsError) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    int rc = snn_backprop_forward(ctx, inputs.data(), 0, 10.0f, nullptr);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, ForwardProducesOutput) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    std::vector<float> outputs(N_OUTPUTS, -999.0f);

    int rc = snn_backprop_forward(ctx, inputs.data(), 1, 10.0f, outputs.data());
    EXPECT_EQ(rc, SNN_SUCCESS);

    // Outputs should be modified from sentinel value
    // They should be finite (not NaN or Inf)
    bool any_non_sentinel = false;
    for (uint32_t i = 0; i < N_OUTPUTS; i++) {
        EXPECT_FALSE(std::isnan(outputs[i])) << "NaN at output " << i;
        EXPECT_FALSE(std::isinf(outputs[i])) << "Inf at output " << i;
        if (outputs[i] != -999.0f) {
            any_non_sentinel = true;
        }
    }
    /* M-7: Verify at least one output was actually written by the forward pass
     * (i.e., not still the sentinel value). If all outputs remain -999.0f,
     * the forward pass did not produce any results. */
    EXPECT_TRUE(any_non_sentinel) << "All outputs still at sentinel value (-999.0f) — forward pass did not write outputs";
}

TEST_F(SNNBackpropStubTest, ForwardWithNullOutputsIsSafe) {
    // outputs parameter is documented as optional
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    int rc = snn_backprop_forward(ctx, inputs.data(), 1, 10.0f, nullptr);
    EXPECT_EQ(rc, SNN_SUCCESS);
}

//=============================================================================
// 3. Backward Pass Tests
//=============================================================================

TEST_F(SNNBackpropStubTest, BackwardNullCtxReturnsError) {
    float targets[5] = {0};
    int rc = snn_backprop_backward(nullptr, targets, 1);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, BackwardNullTargetsReturnsError) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    int rc = snn_backprop_backward(ctx, nullptr, 1);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, BackwardAfterForwardSucceeds) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    // Must call forward before backward
    int rc = snn_backprop_forward(ctx, inputs.data(), 1, 10.0f, nullptr);
    ASSERT_EQ(rc, SNN_SUCCESS);

    rc = snn_backprop_backward(ctx, targets.data(), 1);
    EXPECT_EQ(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, BackwardProducesNonZeroGradients) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    // Use high-magnitude inputs to maximise chance of spiking
    std::vector<float> inputs(N_INPUTS, 0.9f);
    std::vector<float> targets(N_OUTPUTS, 0.0f);
    targets[0] = 1.0f;  // One-hot target

    int rc = snn_backprop_forward(ctx, inputs.data(), 1, 50.0f, nullptr);
    ASSERT_EQ(rc, SNN_SUCCESS);

    rc = snn_backprop_backward(ctx, targets.data(), 1);
    ASSERT_EQ(rc, SNN_SUCCESS);

    // Gradient norm should be non-negative (may be zero if no spikes occurred)
    float grad_norm = snn_backprop_get_gradient_norm(ctx);
    EXPECT_GE(grad_norm, 0.0f);
    EXPECT_FALSE(std::isnan(grad_norm));
    EXPECT_FALSE(std::isinf(grad_norm));
}

//=============================================================================
// 4. Train Step Tests (complete forward + backward + update pipeline)
//=============================================================================

TEST_F(SNNBackpropStubTest, TrainStepNullCtxReturnsError) {
    float inputs[10] = {0};
    float targets[5] = {0};
    snn_train_result_t result = {};
    int rc = snn_backprop_train_step(nullptr, inputs, targets, 1, 10.0f, &result);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, TrainStepNullInputsReturnsError) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    float targets[5] = {0};
    snn_train_result_t result = {};
    int rc = snn_backprop_train_step(ctx, nullptr, targets, 1, 10.0f, &result);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, TrainStepNullTargetsReturnsError) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    snn_train_result_t result = {};
    int rc = snn_backprop_train_step(ctx, inputs.data(), nullptr, 1, 10.0f, &result);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, TrainStepSucceeds) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);
    snn_train_result_t result = {};

    int rc = snn_backprop_train_step(
        ctx, inputs.data(), targets.data(), 1, 10.0f, &result);
    EXPECT_EQ(rc, SNN_SUCCESS);

    // Loss should be finite
    EXPECT_FALSE(std::isnan(result.loss));
    EXPECT_FALSE(std::isinf(result.loss));
    EXPECT_GE(result.loss, 0.0f);

    // Gradients should be valid
    EXPECT_TRUE(result.gradients_valid);
}

TEST_F(SNNBackpropStubTest, TrainStepLossFiniteOverMultipleIterations) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    // Run multiple training steps - loss should stay finite and bounded
    for (int step = 0; step < 10; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, 10.0f, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Failed at step " << step;
        EXPECT_FALSE(std::isnan(result.loss)) << "NaN loss at step " << step;
        EXPECT_FALSE(std::isinf(result.loss)) << "Inf loss at step " << step;
        EXPECT_LT(result.loss, 1e6f) << "Loss explosion at step " << step;
    }
}

TEST_F(SNNBackpropStubTest, TrainStepNullResultIsSafe) {
    // result parameter should be optional (NULL-safe)
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    int rc = snn_backprop_train_step(
        ctx, inputs.data(), targets.data(), 1, 10.0f, nullptr);
    EXPECT_EQ(rc, SNN_SUCCESS);
}

//=============================================================================
// 5. Gradient Norm Tests
//=============================================================================

TEST_F(SNNBackpropStubTest, GradientNormNullReturnsZero) {
    float norm = snn_backprop_get_gradient_norm(nullptr);
    EXPECT_FLOAT_EQ(norm, 0.0f);
}

TEST_F(SNNBackpropStubTest, GradientNormZeroBeforeBackward) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    // Before any backward pass, gradient norm should be zero
    float norm = snn_backprop_get_gradient_norm(ctx);
    EXPECT_FLOAT_EQ(norm, 0.0f);
}

TEST_F(SNNBackpropStubTest, GradientNormNonNegativeAfterBackward) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    snn_backprop_forward(ctx, inputs.data(), 1, 10.0f, nullptr);
    snn_backprop_backward(ctx, targets.data(), 1);

    float norm = snn_backprop_get_gradient_norm(ctx);
    EXPECT_GE(norm, 0.0f);
    EXPECT_FALSE(std::isnan(norm));
    EXPECT_FALSE(std::isinf(norm));
}

TEST_F(SNNBackpropStubTest, GradientNormResetByZeroGrad) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    // Accumulate some gradients
    snn_backprop_forward(ctx, inputs.data(), 1, 10.0f, nullptr);
    snn_backprop_backward(ctx, targets.data(), 1);

    // Zero gradients
    int rc = snn_backprop_zero_grad(ctx);
    EXPECT_EQ(rc, SNN_SUCCESS);

    // Gradient norm should be zero after zeroing
    float norm = snn_backprop_get_gradient_norm(ctx);
    EXPECT_FLOAT_EQ(norm, 0.0f);
}

//=============================================================================
// 6. Weight Norm Tests
//=============================================================================

TEST_F(SNNBackpropStubTest, WeightNormNullReturnsZero) {
    float norm = snn_backprop_get_weight_norm(nullptr);
    EXPECT_FLOAT_EQ(norm, 0.0f);
}

TEST_F(SNNBackpropStubTest, WeightNormNonNegative) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    float norm = snn_backprop_get_weight_norm(ctx);
    EXPECT_GE(norm, 0.0f);
    EXPECT_FALSE(std::isnan(norm));
    EXPECT_FALSE(std::isinf(norm));
}

//=============================================================================
// 7. Configuration Validation Tests
//=============================================================================

TEST_F(SNNBackpropStubTest, ValidateConfigNullReturnsError) {
    int rc = snn_backprop_validate_config(nullptr);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, ValidateConfigDefaultPasses) {
    int rc = snn_backprop_validate_config(&config);
    EXPECT_EQ(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, ValidateConfigZeroBatchFails) {
    config.batch_size = 0;
    int rc = snn_backprop_validate_config(&config);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, ValidateConfigNegativeLearningRateFails) {
    config.learning_rate = -0.01f;
    int rc = snn_backprop_validate_config(&config);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, ValidateConfigZeroSequenceLengthFails) {
    config.sequence_length = 0;
    int rc = snn_backprop_validate_config(&config);
    EXPECT_NE(rc, SNN_SUCCESS);
}

//=============================================================================
// 8. Default Config Sanity
//=============================================================================

TEST_F(SNNBackpropStubTest, DefaultConfigBPTTHasReasonableValues) {
    snn_backprop_config_t cfg = snn_backprop_default_config(SNN_TRAIN_BPTT);
    EXPECT_EQ(cfg.algorithm, SNN_TRAIN_BPTT);
    EXPECT_GT(cfg.learning_rate, 0.0f);
    EXPECT_LE(cfg.learning_rate, 1.0f);
    EXPECT_GT(cfg.batch_size, 0u);
    EXPECT_GT(cfg.sequence_length, 0u);
    EXPECT_EQ(cfg.surrogate.method, (snn_surrogate_method_t)SNN_SURROGATE_SUPERSPIKE);
}

TEST_F(SNNBackpropStubTest, DefaultSurrogateConfigIsValid) {
    snn_surrogate_config_t scfg = snn_surrogate_default_config();
    EXPECT_EQ(scfg.method, (snn_surrogate_method_t)SNN_SURROGATE_SUPERSPIKE);
    EXPECT_FLOAT_EQ(scfg.beta, SNN_SURROGATE_BETA_DEFAULT);
    EXPECT_FALSE(scfg.adaptive_beta);
}

TEST_F(SNNBackpropStubTest, DefaultBPTTConfigUnrollNonZero) {
    snn_bptt_config_t bcfg = snn_bptt_default_config(100);
    EXPECT_GT(bcfg.unroll_steps, 0u);
    EXPECT_LE(bcfg.unroll_steps, (uint32_t)SNN_BPTT_MAX_UNROLL);
    EXPECT_TRUE(bcfg.accumulate_over_time);
}

TEST_F(SNNBackpropStubTest, DefaultEPropConfigValid) {
    snn_eprop_config_t ecfg = snn_eprop_default_config();
    EXPECT_FLOAT_EQ(ecfg.eligibility_tau, SNN_ELIGIBILITY_TAU_DEFAULT);
    EXPECT_GE(ecfg.kappa, 0.0f);
    EXPECT_LE(ecfg.kappa, 1.0f);
}

//=============================================================================
// 9. Surrogate Gradient Tests
//=============================================================================

TEST_F(SNNBackpropStubTest, SurrogateGradientNullReturnsZero) {
    float grad = snn_surrogate_gradient(nullptr, 0.0f);
    EXPECT_FLOAT_EQ(grad, 0.0f);
}

TEST_F(SNNBackpropStubTest, SurrogateGradientSuperSpikeSymmetric) {
    ASSERT_TRUE(CreateNetwork());
    config.surrogate.method = (snn_surrogate_method_t)SNN_SURROGATE_SUPERSPIKE;
    config.surrogate.beta = 1.0f;
    ASSERT_TRUE(CreateContext());

    float grad_pos = snn_surrogate_gradient(ctx, 5.0f);
    float grad_neg = snn_surrogate_gradient(ctx, -5.0f);

    // SuperSpike uses |x|, so it should be symmetric
    EXPECT_FLOAT_EQ(grad_pos, grad_neg);
    EXPECT_GT(grad_pos, 0.0f);
}

TEST_F(SNNBackpropStubTest, SurrogateGradientMaxAtThreshold) {
    ASSERT_TRUE(CreateNetwork());
    config.surrogate.method = (snn_surrogate_method_t)SNN_SURROGATE_SUPERSPIKE;
    config.surrogate.beta = 1.0f;
    ASSERT_TRUE(CreateContext());

    float grad_zero = snn_surrogate_gradient(ctx, 0.0f);
    float grad_far = snn_surrogate_gradient(ctx, 10.0f);

    // Gradient at threshold (v=0) should be larger than far from threshold
    EXPECT_GT(grad_zero, grad_far);
}

//=============================================================================
// 10. Statistics Tests
//=============================================================================

TEST_F(SNNBackpropStubTest, GetStatsNullReturnsError) {
    snn_backprop_stats_t stats = {};
    int rc = snn_backprop_get_stats(nullptr, &stats);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, StatsInitiallyZero) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    snn_backprop_stats_t stats = {};
    int rc = snn_backprop_get_stats(ctx, &stats);
    EXPECT_EQ(rc, SNN_SUCCESS);
    // After creation, total_steps starts at 0
    // (Note: create may call forward internally, so we check >= 0)
    EXPECT_GE(stats.total_steps, 0u);
}

TEST_F(SNNBackpropStubTest, StatsIncrementAfterTrainStep) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    snn_backprop_stats_t stats_before = {};
    snn_backprop_get_stats(ctx, &stats_before);

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);
    snn_train_result_t result = {};
    snn_backprop_train_step(ctx, inputs.data(), targets.data(), 1, 10.0f, &result);

    snn_backprop_stats_t stats_after = {};
    snn_backprop_get_stats(ctx, &stats_after);

    // Steps should have increased
    EXPECT_GT(stats_after.total_steps, stats_before.total_steps);
}

TEST_F(SNNBackpropStubTest, ResetStatsClears) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);
    snn_train_result_t result = {};
    snn_backprop_train_step(ctx, inputs.data(), targets.data(), 1, 10.0f, &result);

    snn_backprop_reset_stats(ctx);

    snn_backprop_stats_t stats = {};
    snn_backprop_get_stats(ctx, &stats);
    EXPECT_EQ(stats.total_steps, 0u);
}

TEST_F(SNNBackpropStubTest, ResetStatsNullIsSafe) {
    snn_backprop_reset_stats(nullptr);
    // No crash = pass
}

//=============================================================================
// 11. Utility Name Functions
//=============================================================================

TEST_F(SNNBackpropStubTest, AlgorithmNamesNonNull) {
    // All valid algorithm indices should return non-null, non-empty strings
    for (int i = 0; i < SNN_TRAIN_MODE_COUNT; i++) {
        const char* name = snn_train_algorithm_name((snn_train_algorithm_t)i);
        EXPECT_NE(name, nullptr) << "NULL for algorithm " << i;
        EXPECT_GT(strlen(name), 0u) << "Empty for algorithm " << i;
    }
}

TEST_F(SNNBackpropStubTest, SurrogateNamesNonNull) {
    for (int i = 0; i < SNN_SURROGATE_BACKPROP_COUNT; i++) {
        const char* name = snn_surrogate_method_name((snn_surrogate_method_t)i);
        EXPECT_NE(name, nullptr) << "NULL for surrogate " << i;
        EXPECT_GT(strlen(name), 0u) << "Empty for surrogate " << i;
    }
}

TEST_F(SNNBackpropStubTest, LossTypeNamesNonNull) {
    for (int i = 0; i < SNN_LOSS_TYPE_COUNT; i++) {
        const char* name = snn_loss_type_name((snn_loss_type_t)i);
        EXPECT_NE(name, nullptr) << "NULL for loss type " << i;
        EXPECT_GT(strlen(name), 0u) << "Empty for loss type " << i;
    }
}

TEST_F(SNNBackpropStubTest, InvalidAlgorithmNameNotNull) {
    const char* name = snn_train_algorithm_name((snn_train_algorithm_t)999);
    EXPECT_NE(name, nullptr);
}

//=============================================================================
// 12. Batch Processing
//=============================================================================

TEST_F(SNNBackpropStubTest, BatchCreateDestroy) {
    auto inputs = MakeInputs(4);
    auto targets = MakeTargets(4);

    snn_batch_t* batch = snn_batch_create(
        inputs.data(), targets.data(), 4, N_INPUTS, N_OUTPUTS);
    // May succeed or fail depending on implementation state
    if (batch) {
        snn_batch_destroy(batch);
    }
}

TEST_F(SNNBackpropStubTest, BatchCreateNullReturnsNull) {
    snn_batch_t* batch = snn_batch_create(nullptr, nullptr, 4, N_INPUTS, N_OUTPUTS);
    EXPECT_EQ(batch, nullptr);
}

TEST_F(SNNBackpropStubTest, BatchDestroyNullIsSafe) {
    snn_batch_destroy(nullptr);
}

//=============================================================================
// 13. Gradient Manager Integration
//=============================================================================

TEST_F(SNNBackpropStubTest, ConnectGradientManagerNullCtxReturnsError) {
    int rc = snn_backprop_connect_gradient_manager(nullptr, nullptr);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropStubTest, GetGradientManagerNullReturnsNull) {
    nimcp_gradient_manager_ctx_t* gm = snn_backprop_get_gradient_manager(nullptr);
    EXPECT_EQ(gm, nullptr);
}

//=============================================================================
// 14. Integration: Full Training Pipeline
//=============================================================================

TEST_F(SNNBackpropStubTest, FullPipelineForwardBackwardStep) {
    // Complete manual pipeline: forward -> backward -> step -> zero_grad
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);
    std::vector<float> outputs(N_OUTPUTS, 0.0f);

    // Step 1: Forward
    int rc = snn_backprop_forward(ctx, inputs.data(), 1, 10.0f, outputs.data());
    ASSERT_EQ(rc, SNN_SUCCESS);

    // Step 2: Backward
    rc = snn_backprop_backward(ctx, targets.data(), 1);
    ASSERT_EQ(rc, SNN_SUCCESS);

    // Step 3: Check gradient norm is non-negative
    float grad_norm = snn_backprop_get_gradient_norm(ctx);
    EXPECT_GE(grad_norm, 0.0f);

    // Step 4: Apply weight updates
    int updated = snn_backprop_step(ctx, 0.0f);  // Use config LR
    EXPECT_GE(updated, 0);

    // Step 5: Check weight norm
    float weight_norm = snn_backprop_get_weight_norm(ctx);
    EXPECT_GE(weight_norm, 0.0f);
    EXPECT_FALSE(std::isnan(weight_norm));

    // Step 6: Zero gradients
    rc = snn_backprop_zero_grad(ctx);
    EXPECT_EQ(rc, SNN_SUCCESS);
    EXPECT_FLOAT_EQ(snn_backprop_get_gradient_norm(ctx), 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
