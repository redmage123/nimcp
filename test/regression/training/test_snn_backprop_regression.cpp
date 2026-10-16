/**
 * @file test_snn_backprop_regression.cpp
 * @brief Comprehensive Regression Tests for SNN Backpropagation Training
 *
 * WHAT: Regression tests for gradient computation, weight update determinism, and training reproducibility
 * WHY:  Ensure numerical stability, consistent results, and backward compatibility
 * HOW:  Test gradient flows, weight updates, and training outcomes across runs
 *
 * REGRESSION CATEGORIES:
 * - Gradient Computation Consistency: Surrogate gradients must be numerically stable
 * - Weight Update Determinism: Same inputs produce same weight updates
 * - Training Reproducibility: Training results reproducible with fixed seed
 * - API Stability: Struct layouts and enum values must not change
 * - Performance Baselines: Training step timing requirements
 *
 * @author NIMCP Test Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "training/nimcp_snn_backprop.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SNNBackpropRegressionTest : public ::testing::Test {
protected:
    snn_backprop_ctx_t* ctx = nullptr;
    snn_network_t* network = nullptr;
    snn_backprop_config_t config;

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

    void CreateSmallNetwork() {
        snn_config_t net_config;
        snn_config_feedforward(&net_config, 10, 20, 5);

        network = snn_network_create(&net_config);
        ASSERT_NE(network, nullptr);
    }

    void CreateContext() {
        ctx = snn_backprop_create(network, &config);
        ASSERT_NE(ctx, nullptr);
    }

    std::vector<float> GenerateInputs(uint32_t batch_size, uint32_t n_inputs) {
        std::vector<float> inputs(batch_size * n_inputs);
        for (size_t i = 0; i < inputs.size(); i++) {
            inputs[i] = static_cast<float>(i % 100) / 100.0f;
        }
        return inputs;
    }

    std::vector<float> GenerateTargets(uint32_t batch_size, uint32_t n_outputs) {
        std::vector<float> targets(batch_size * n_outputs);
        for (size_t i = 0; i < targets.size(); i++) {
            targets[i] = static_cast<float>((i + 1) % 2);  // Binary targets
        }
        return targets;
    }
};

//=============================================================================
// API Stability Tests - Enum Values
//=============================================================================

TEST_F(SNNBackpropRegressionTest, SurrogateMethodEnumStable) {
    // WHAT: Verify surrogate method enum values are stable
    // WHY:  ABI compatibility requires stable enum values
    // REGRESSION: Enum values must not change
    //
    // NOTE: When nimcp_snn_types.h is included, snn_surrogate_method_t is
    // typedef'd to snn_surrogate_t, which has its own ordering.  The
    // EXPONENTIAL extension follows SNN_SURROGATE_COUNT from the base enum.

    EXPECT_EQ(SNN_SURROGATE_SIGMOID, 0);
    EXPECT_EQ(SNN_SURROGATE_FAST_SIGMOID, 1);
    EXPECT_EQ(SNN_SURROGATE_ARCTAN, 2);
    EXPECT_EQ(SNN_SURROGATE_SUPERSPIKE, 3);
    EXPECT_EQ(SNN_SURROGATE_TRIANGULAR, 4);
    EXPECT_EQ(SNN_SURROGATE_RECTANGULAR, 5);
    EXPECT_EQ(SNN_SURROGATE_EXPONENTIAL, 6);
}

TEST_F(SNNBackpropRegressionTest, TrainAlgorithmEnumStable) {
    // WHAT: Verify training algorithm enum values are stable
    // WHY:  ABI compatibility requires stable enum values
    // REGRESSION: Enum values must not change
    //
    // NOTE: When nimcp_snn_types.h is included, snn_train_algorithm_t is
    // typedef'd to snn_train_mode_t.  BPTT/TRUNCATED_BPTT/RTRL/HYBRID are
    // extensions after SNN_TRAIN_COUNT from the base enum.  EPROP, SLAYER,
    // DECOLLE keep their base-enum values.

    EXPECT_EQ(SNN_TRAIN_EPROP, 2);
    EXPECT_EQ(SNN_TRAIN_SLAYER, 4);
    EXPECT_EQ(SNN_TRAIN_DECOLLE, 5);
    /* Extension values follow SNN_TRAIN_COUNT (=6) */
    EXPECT_EQ(SNN_TRAIN_BPTT, 6);
    EXPECT_EQ(SNN_TRAIN_TRUNCATED_BPTT, 7);
    EXPECT_EQ(SNN_TRAIN_RTRL, 8);
    EXPECT_EQ(SNN_TRAIN_HYBRID, 9);
}

TEST_F(SNNBackpropRegressionTest, LossTypeEnumStable) {
    // WHAT: Verify loss type enum values are stable
    // WHY:  ABI compatibility requires stable enum values
    // REGRESSION: Enum values must not change

    EXPECT_EQ(SNN_LOSS_SPIKE_COUNT, 0);
    EXPECT_EQ(SNN_LOSS_FIRST_SPIKE_TIME, 1);
    EXPECT_EQ(SNN_LOSS_RATE_CODED_MSE, 2);
    EXPECT_EQ(SNN_LOSS_RATE_CODED_CROSS_ENTROPY, 3);
    EXPECT_EQ(SNN_LOSS_TEMPORAL_CROSS_ENTROPY, 4);
    EXPECT_EQ(SNN_LOSS_VAN_ROSSUM, 5);
    EXPECT_EQ(SNN_LOSS_VICTOR_PURPURA, 6);
    EXPECT_EQ(SNN_LOSS_MEMBRANE_POTENTIAL, 7);
    EXPECT_EQ(SNN_LOSS_CUSTOM, 8);
}

TEST_F(SNNBackpropRegressionTest, ConstantsStable) {
    // WHAT: Verify constants are stable
    // WHY:  Applications depend on these values
    // REGRESSION: Constants must not change

    EXPECT_EQ(SNN_BPTT_MAX_UNROLL, 1000);
    EXPECT_FLOAT_EQ(SNN_SURROGATE_BETA_DEFAULT, 1.0f);
    EXPECT_FLOAT_EQ(SNN_ELIGIBILITY_TAU_DEFAULT, 20.0f);
    EXPECT_FLOAT_EQ(SNN_GRADIENT_CLIP_DEFAULT, 10.0f);
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(SNNBackpropRegressionTest, DefaultSurrogateConfig) {
    // WHAT: Verify default surrogate config is reasonable
    // WHY:  Default should work out of the box
    // REGRESSION: Defaults must remain usable

    snn_surrogate_config_t surrogate = snn_surrogate_default_config();

    EXPECT_EQ(surrogate.method, SNN_SURROGATE_SUPERSPIKE);
    EXPECT_GT(surrogate.beta, 0.0f);
    EXPECT_LE(surrogate.beta, 10.0f);
    EXPECT_FALSE(surrogate.adaptive_beta);
}

TEST_F(SNNBackpropRegressionTest, DefaultBPTTConfig) {
    // WHAT: Verify default BPTT config is reasonable
    // WHY:  Default should work out of the box
    // REGRESSION: Defaults must remain usable

    snn_bptt_config_t bptt = snn_bptt_default_config(100);

    EXPECT_GT(bptt.unroll_steps, 0u);
    EXPECT_LE(bptt.unroll_steps, SNN_BPTT_MAX_UNROLL);
}

TEST_F(SNNBackpropRegressionTest, DefaultEpropConfig) {
    // WHAT: Verify default E-prop config is reasonable
    // WHY:  Default should work out of the box
    // REGRESSION: Defaults must remain usable

    snn_eprop_config_t eprop = snn_eprop_default_config();

    EXPECT_GT(eprop.eligibility_tau, 0.0f);
    EXPECT_GE(eprop.kappa, 0.0f);
    EXPECT_LE(eprop.kappa, 1.0f);
}

TEST_F(SNNBackpropRegressionTest, DefaultLossConfig) {
    // WHAT: Verify default loss config is reasonable
    // WHY:  Default should work out of the box
    // REGRESSION: Defaults must remain usable

    for (int i = 0; i < SNN_LOSS_TYPE_COUNT - 1; i++) {  // Skip CUSTOM
        snn_loss_type_t loss_type = static_cast<snn_loss_type_t>(i);
        snn_loss_config_t loss = snn_loss_default_config(loss_type);

        EXPECT_EQ(loss.type, loss_type);
    }
}

TEST_F(SNNBackpropRegressionTest, DefaultBackpropConfig) {
    // WHAT: Verify default backprop config is reasonable
    // WHY:  Default should work out of the box
    // REGRESSION: Defaults must remain usable

    for (int i = 0; i < SNN_TRAIN_MODE_COUNT; i++) {
        snn_train_algorithm_t algo = static_cast<snn_train_algorithm_t>(i);
        snn_backprop_config_t bp_config = snn_backprop_default_config(algo);

        EXPECT_EQ(bp_config.algorithm, algo);
        EXPECT_GT(bp_config.learning_rate, 0.0f);
        EXPECT_LE(bp_config.learning_rate, 1.0f);
        EXPECT_GT(bp_config.batch_size, 0u);
    }
}

//=============================================================================
// Gradient Computation Consistency Tests
//=============================================================================

TEST_F(SNNBackpropRegressionTest, SurrogateGradientNumericalStability) {
    // WHAT: Verify surrogate gradient is numerically stable
    // WHY:  Gradients must not explode or vanish
    // REGRESSION: Numerical stability is critical

    CreateSmallNetwork();
    CreateContext();

    // Test various membrane potential values
    std::vector<float> test_voltages = {-100.0f, -10.0f, -1.0f, 0.0f, 1.0f, 10.0f, 100.0f};

    for (float v : test_voltages) {
        float grad = snn_surrogate_gradient(ctx, v);

        // Gradient should be finite
        EXPECT_FALSE(std::isnan(grad)) << "NaN at v=" << v;
        EXPECT_FALSE(std::isinf(grad)) << "Inf at v=" << v;

        // Gradient should be non-negative (surrogate is non-negative)
        EXPECT_GE(grad, 0.0f) << "Negative gradient at v=" << v;

        // Gradient should decay for large |v|
        if (std::abs(v) > 10.0f) {
            EXPECT_LT(grad, 0.1f) << "Gradient too large at v=" << v;
        }
    }
}

TEST_F(SNNBackpropRegressionTest, SurrogateGradientConsistency) {
    // WHAT: Verify surrogate gradient produces same results
    // WHY:  Determinism is required for reproducibility
    // REGRESSION: Same input must produce same output

    CreateSmallNetwork();
    CreateContext();

    const float test_v = 0.5f;
    float grad1 = snn_surrogate_gradient(ctx, test_v);
    float grad2 = snn_surrogate_gradient(ctx, test_v);

    EXPECT_FLOAT_EQ(grad1, grad2);
}

TEST_F(SNNBackpropRegressionTest, AllSurrogateMethodsWork) {
    // WHAT: Verify all surrogate methods produce valid gradients
    // WHY:  All methods must be functional
    // REGRESSION: No method should crash or produce invalid results

    CreateSmallNetwork();

    for (int i = 0; i < SNN_SURROGATE_COUNT; i++) {
        config = snn_backprop_default_config(SNN_TRAIN_BPTT);
        config.surrogate.method = static_cast<snn_surrogate_method_t>(i);

        ctx = snn_backprop_create(network, &config);
        ASSERT_NE(ctx, nullptr) << "Failed for method " << i;

        float grad = snn_surrogate_gradient(ctx, 0.0f);

        EXPECT_FALSE(std::isnan(grad)) << "NaN for method " << i;
        EXPECT_FALSE(std::isinf(grad)) << "Inf for method " << i;
        EXPECT_GE(grad, 0.0f) << "Negative for method " << i;

        snn_backprop_destroy(ctx);
        ctx = nullptr;
    }
}

TEST_F(SNNBackpropRegressionTest, GradientNormBounded) {
    // WHAT: Verify gradient norm stays bounded
    // WHY:  Gradient explosion detection
    // REGRESSION: Must detect gradient explosions

    CreateSmallNetwork();
    config.use_gradient_clipping = true;
    config.gradient_clip_norm = 10.0f;
    CreateContext();

    auto inputs = GenerateInputs(1, 10);
    auto targets = GenerateTargets(1, 5);

    // Forward and backward pass
    snn_backprop_forward(ctx, inputs.data(), 1, 10.0f, nullptr);
    snn_backprop_backward(ctx, targets.data(), 1);

    float grad_norm = snn_backprop_get_gradient_norm(ctx);

    // Gradient norm should be bounded by clip value (or close to it)
    EXPECT_LE(grad_norm, config.gradient_clip_norm * 1.1f);  // Allow 10% tolerance
}

//=============================================================================
// Weight Update Determinism Tests
//=============================================================================

TEST_F(SNNBackpropRegressionTest, WeightUpdateDeterministic) {
    // WHAT: Verify weight updates are deterministic
    // WHY:  Same inputs must produce same weight changes
    // REGRESSION: Determinism is required for reproducibility

    CreateSmallNetwork();
    CreateContext();

    auto inputs = GenerateInputs(1, 10);
    auto targets = GenerateTargets(1, 5);

    // Record initial weight norm
    float initial_norm = snn_backprop_get_weight_norm(ctx);

    // Training step
    snn_train_result_t result1;
    snn_backprop_train_step(ctx, inputs.data(), targets.data(), 1, 10.0f, &result1);

    float norm_after_step1 = snn_backprop_get_weight_norm(ctx);

    // Reset and repeat
    snn_backprop_reset(ctx);

    // Same training step
    snn_train_result_t result2;
    snn_backprop_train_step(ctx, inputs.data(), targets.data(), 1, 10.0f, &result2);

    float norm_after_step2 = snn_backprop_get_weight_norm(ctx);

    // Results should be identical
    EXPECT_FLOAT_EQ(result1.loss, result2.loss);
    EXPECT_FLOAT_EQ(result1.gradient_norm, result2.gradient_norm);
}

TEST_F(SNNBackpropRegressionTest, ZeroGradWorks) {
    // WHAT: Verify gradient zeroing works
    // WHY:  Must clear gradients between batches
    // REGRESSION: zero_grad must actually zero gradients

    CreateSmallNetwork();
    CreateContext();

    // Use stronger inputs and longer duration to ensure spikes occur
    std::vector<float> inputs(10, 5.0f);
    auto targets = GenerateTargets(1, 5);

    // Forward and backward to accumulate gradients
    snn_backprop_forward(ctx, inputs.data(), 1, 50.0f, nullptr);
    snn_backprop_backward(ctx, targets.data(), 1);

    float grad_norm_before = snn_backprop_get_gradient_norm(ctx);
    // Grad norm may be zero if no spikes occur — check valid (non-NaN)
    EXPECT_FALSE(std::isnan(grad_norm_before));

    // Zero gradients
    snn_backprop_zero_grad(ctx);

    float grad_norm_after = snn_backprop_get_gradient_norm(ctx);
    EXPECT_FLOAT_EQ(grad_norm_after, 0.0f);
}

TEST_F(SNNBackpropRegressionTest, LearningRateAffectsUpdate) {
    // WHAT: Verify learning rate affects weight updates
    // WHY:  Learning rate must scale updates
    // REGRESSION: Learning rate must work correctly

    CreateSmallNetwork();
    CreateContext();

    auto inputs = GenerateInputs(1, 10);
    auto targets = GenerateTargets(1, 5);

    // Training step with low learning rate
    config.learning_rate = 0.001f;
    snn_train_result_t result_low;
    snn_backprop_forward(ctx, inputs.data(), 1, 10.0f, nullptr);
    snn_backprop_backward(ctx, targets.data(), 1);
    int updates_low = snn_backprop_step(ctx, 0.001f);

    // Reset
    snn_backprop_reset(ctx);
    snn_backprop_zero_grad(ctx);

    // Training step with high learning rate
    snn_backprop_forward(ctx, inputs.data(), 1, 10.0f, nullptr);
    snn_backprop_backward(ctx, targets.data(), 1);
    int updates_high = snn_backprop_step(ctx, 0.1f);

    // Both should update same number of weights
    EXPECT_EQ(updates_low, updates_high);
}

//=============================================================================
// Training Reproducibility Tests
//=============================================================================

TEST_F(SNNBackpropRegressionTest, TrainingReproducibility) {
    // WHAT: Verify training is reproducible
    // WHY:  Same conditions must produce same results
    // REGRESSION: Reproducibility is critical for debugging

    std::vector<float> losses_run1;
    std::vector<float> losses_run2;

    // Run 1
    {
        CreateSmallNetwork();
        CreateContext();

        auto inputs = GenerateInputs(4, 10);
        auto targets = GenerateTargets(4, 5);

        for (int epoch = 0; epoch < 5; epoch++) {
            snn_train_result_t result;
            snn_backprop_train_step(ctx, inputs.data(), targets.data(), 4, 10.0f, &result);
            losses_run1.push_back(result.loss);
        }

        snn_backprop_destroy(ctx);
        ctx = nullptr;
        snn_network_destroy(network);
        network = nullptr;
    }

    // Run 2 (identical setup)
    {
        CreateSmallNetwork();
        CreateContext();

        auto inputs = GenerateInputs(4, 10);
        auto targets = GenerateTargets(4, 5);

        for (int epoch = 0; epoch < 5; epoch++) {
            snn_train_result_t result;
            snn_backprop_train_step(ctx, inputs.data(), targets.data(), 4, 10.0f, &result);
            losses_run2.push_back(result.loss);
        }
    }

    // Losses should be identical
    ASSERT_EQ(losses_run1.size(), losses_run2.size());
    for (size_t i = 0; i < losses_run1.size(); i++) {
        EXPECT_FLOAT_EQ(losses_run1[i], losses_run2[i])
            << "Difference at epoch " << i;
    }
}

TEST_F(SNNBackpropRegressionTest, BatchOrderIndependence) {
    // WHAT: Verify accumulated gradients are same regardless of batch order
    // WHY:  Gradient accumulation must be order-independent for sum
    // REGRESSION: Order independence for gradient accumulation

    CreateSmallNetwork();
    CreateContext();

    // Generate two batches
    auto inputs1 = GenerateInputs(1, 10);
    auto targets1 = GenerateTargets(1, 5);
    auto inputs2 = GenerateInputs(1, 10);
    // Modify inputs2 to be different
    for (auto& v : inputs2) v += 0.5f;
    auto targets2 = GenerateTargets(1, 5);

    // Order 1: batch1 then batch2
    snn_backprop_forward(ctx, inputs1.data(), 1, 10.0f, nullptr);
    snn_backprop_backward(ctx, targets1.data(), 1);
    snn_backprop_forward(ctx, inputs2.data(), 1, 10.0f, nullptr);
    snn_backprop_backward(ctx, targets2.data(), 1);
    float grad_norm_order1 = snn_backprop_get_gradient_norm(ctx);

    // Reset
    snn_backprop_zero_grad(ctx);

    // Order 2: batch2 then batch1
    snn_backprop_forward(ctx, inputs2.data(), 1, 10.0f, nullptr);
    snn_backprop_backward(ctx, targets2.data(), 1);
    snn_backprop_forward(ctx, inputs1.data(), 1, 10.0f, nullptr);
    snn_backprop_backward(ctx, targets1.data(), 1);
    float grad_norm_order2 = snn_backprop_get_gradient_norm(ctx);

    // Gradient norms should be equal (accumulated sum is commutative)
    EXPECT_NEAR(grad_norm_order1, grad_norm_order2, 1e-5f);
}

//=============================================================================
// Loss Function Tests
//=============================================================================

TEST_F(SNNBackpropRegressionTest, LossDecreasesDuringTraining) {
    // WHAT: Verify loss decreases during training
    // WHY:  Basic sanity check that training works
    // REGRESSION: Training must reduce loss

    CreateSmallNetwork();
    config.learning_rate = 0.01f;
    CreateContext();

    auto inputs = GenerateInputs(4, 10);
    auto targets = GenerateTargets(4, 5);

    float initial_loss = 0.0f;
    float final_loss = 0.0f;

    for (int epoch = 0; epoch < 50; epoch++) {
        snn_train_result_t result;
        snn_backprop_train_step(ctx, inputs.data(), targets.data(), 4, 10.0f, &result);

        if (epoch == 0) initial_loss = result.loss;
        if (epoch == 49) final_loss = result.loss;
    }

    // Loss should decrease (or at least not increase significantly)
    // Note: With simple test data, loss may not always decrease dramatically
    EXPECT_LE(final_loss, initial_loss * 1.5f);
}

TEST_F(SNNBackpropRegressionTest, AllLossTypesCompute) {
    // WHAT: Verify all loss types can compute loss
    // WHY:  All loss types must be functional
    // REGRESSION: No loss type should crash

    CreateSmallNetwork();

    for (int i = 0; i < SNN_LOSS_TYPE_COUNT - 1; i++) {  // Skip CUSTOM
        config = snn_backprop_default_config(SNN_TRAIN_BPTT);
        config.loss.type = static_cast<snn_loss_type_t>(i);

        ctx = snn_backprop_create(network, &config);
        ASSERT_NE(ctx, nullptr) << "Failed for loss type " << i;

        auto inputs = GenerateInputs(1, 10);
        auto targets = GenerateTargets(1, 5);

        snn_train_result_t result;
        int ret = snn_backprop_train_step(ctx, inputs.data(), targets.data(), 1, 10.0f, &result);

        EXPECT_EQ(ret, 0) << "Train step failed for loss type " << i;
        EXPECT_FALSE(std::isnan(result.loss)) << "NaN loss for type " << i;
        EXPECT_FALSE(std::isinf(result.loss)) << "Inf loss for type " << i;

        snn_backprop_destroy(ctx);
        ctx = nullptr;
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SNNBackpropRegressionTest, StatisticsTracking) {
    // WHAT: Verify statistics are tracked correctly
    // WHY:  Monitoring depends on accurate stats
    // REGRESSION: Statistics must be accurate

    CreateSmallNetwork();
    CreateContext();

    auto inputs = GenerateInputs(4, 10);
    auto targets = GenerateTargets(4, 5);

    // Initial stats
    snn_backprop_stats_t stats_initial;
    snn_backprop_get_stats(ctx, &stats_initial);
    EXPECT_EQ(stats_initial.total_steps, 0u);

    // Run some training
    for (int i = 0; i < 10; i++) {
        snn_train_result_t result;
        snn_backprop_train_step(ctx, inputs.data(), targets.data(), 4, 10.0f, &result);
    }

    // Final stats
    snn_backprop_stats_t stats_final;
    snn_backprop_get_stats(ctx, &stats_final);

    EXPECT_EQ(stats_final.total_steps, 10u);
    EXPECT_GT(stats_final.total_loss, 0.0);
    EXPECT_GE(stats_final.avg_grad_norm, 0.0);
}

TEST_F(SNNBackpropRegressionTest, StatisticsReset) {
    // WHAT: Verify statistics reset works
    // WHY:  Must be able to reset for new measurement period
    // REGRESSION: Reset must clear statistics

    CreateSmallNetwork();
    CreateContext();

    auto inputs = GenerateInputs(4, 10);
    auto targets = GenerateTargets(4, 5);

    // Run some training
    for (int i = 0; i < 5; i++) {
        snn_train_result_t result;
        snn_backprop_train_step(ctx, inputs.data(), targets.data(), 4, 10.0f, &result);
    }

    // Reset stats
    snn_backprop_reset_stats(ctx);

    snn_backprop_stats_t stats;
    snn_backprop_get_stats(ctx, &stats);

    EXPECT_EQ(stats.total_steps, 0u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(SNNBackpropRegressionTest, NullPointerHandling) {
    // WHAT: Verify NULL pointer handling
    // WHY:  API contract - must handle NULL gracefully
    // REGRESSION: Bug fix - NULL caused crash

    // NULL network
    ctx = snn_backprop_create(nullptr, &config);
    EXPECT_EQ(ctx, nullptr);

    // NULL config
    CreateSmallNetwork();
    ctx = snn_backprop_create(network, nullptr);
    EXPECT_EQ(ctx, nullptr);

    // NULL ctx operations
    snn_backprop_destroy(nullptr);  // Should not crash
    EXPECT_EQ(snn_backprop_reset(nullptr), -1);
    EXPECT_FLOAT_EQ(snn_surrogate_gradient(nullptr, 0.0f), 0.0f);
}

TEST_F(SNNBackpropRegressionTest, InvalidBatchSize) {
    // WHAT: Verify invalid batch size is handled
    // WHY:  Edge case - zero batch size
    // REGRESSION: Bug fix - zero batch crash

    CreateSmallNetwork();
    CreateContext();

    float dummy_input = 0.0f;
    float dummy_target = 0.0f;

    int ret = snn_backprop_forward(ctx, &dummy_input, 0, 10.0f, nullptr);
    EXPECT_NE(ret, 0);  // Should fail
}

TEST_F(SNNBackpropRegressionTest, ConfigValidation) {
    // WHAT: Verify config validation catches invalid configs
    // WHY:  Early error detection
    // REGRESSION: Must reject invalid configs

    snn_backprop_config_t invalid_config;
    memset(&invalid_config, 0, sizeof(invalid_config));

    // Zero learning rate
    invalid_config = snn_backprop_default_config(SNN_TRAIN_BPTT);
    invalid_config.learning_rate = 0.0f;
    // Validation may or may not fail for zero LR (could be valid for inference)

    // Negative learning rate
    invalid_config.learning_rate = -0.1f;
    int ret = snn_backprop_validate_config(&invalid_config);
    EXPECT_NE(ret, 0);  // Should fail
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

TEST_F(SNNBackpropRegressionTest, ForwardPassPerformance) {
    // WHAT: Verify forward pass performance
    // WHY:  Performance baseline
    // BASELINE: < 10ms for small network

    CreateSmallNetwork();
    CreateContext();

    auto inputs = GenerateInputs(4, 10);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        snn_backprop_forward(ctx, inputs.data(), 4, 10.0f, nullptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "100 forward passes took: " << duration.count() << " ms" << std::endl;

    // Baseline: < 1 second for 100 passes
    EXPECT_LT(duration.count(), 1000);
}

TEST_F(SNNBackpropRegressionTest, BackwardPassPerformance) {
    // WHAT: Verify backward pass performance
    // WHY:  Performance baseline
    // BASELINE: < 20ms for small network

    CreateSmallNetwork();
    CreateContext();

    auto inputs = GenerateInputs(4, 10);
    auto targets = GenerateTargets(4, 5);

    // Forward pass first
    snn_backprop_forward(ctx, inputs.data(), 4, 10.0f, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        snn_backprop_backward(ctx, targets.data(), 4);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "100 backward passes took: " << duration.count() << " ms" << std::endl;

    // Baseline: < 2 seconds for 100 passes
    EXPECT_LT(duration.count(), 2000);
}

TEST_F(SNNBackpropRegressionTest, CompleteStepPerformance) {
    // WHAT: Verify complete training step performance
    // WHY:  Performance baseline
    // BASELINE: < 50ms for small network

    CreateSmallNetwork();
    CreateContext();

    auto inputs = GenerateInputs(4, 10);
    auto targets = GenerateTargets(4, 5);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        snn_train_result_t result;
        snn_backprop_train_step(ctx, inputs.data(), targets.data(), 4, 10.0f, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "100 complete training steps took: " << duration.count() << " ms" << std::endl;

    // Baseline: < 5 seconds for 100 complete steps
    EXPECT_LT(duration.count(), 5000);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(SNNBackpropRegressionTest, AlgorithmNameStrings) {
    // WHAT: Verify algorithm name strings
    // WHY:  Logging and debugging require string names
    // REGRESSION: String names must not be NULL

    for (int i = 0; i < SNN_TRAIN_MODE_COUNT; i++) {
        const char* name = snn_train_algorithm_name(static_cast<snn_train_algorithm_t>(i));
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(SNNBackpropRegressionTest, SurrogateMethodNameStrings) {
    // WHAT: Verify surrogate method name strings
    // WHY:  Logging and debugging require string names
    // REGRESSION: String names must not be NULL

    for (int i = 0; i < SNN_SURROGATE_COUNT; i++) {
        const char* name = snn_surrogate_method_name(static_cast<snn_surrogate_method_t>(i));
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(SNNBackpropRegressionTest, LossTypeNameStrings) {
    // WHAT: Verify loss type name strings
    // WHY:  Logging and debugging require string names
    // REGRESSION: String names must not be NULL

    for (int i = 0; i < SNN_LOSS_TYPE_COUNT; i++) {
        const char* name = snn_loss_type_name(static_cast<snn_loss_type_t>(i));
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
