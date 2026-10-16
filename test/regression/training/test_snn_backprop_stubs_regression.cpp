//=============================================================================
// test_snn_backprop_stubs_regression.cpp - Regression Tests for SNN Backprop
//=============================================================================
/**
 * @file test_snn_backprop_stubs_regression.cpp
 * @brief Regression tests for SNN backprop stability and correctness
 *
 * WHAT: Test numerical stability, memory safety, and config validation
 * WHY:  Ensure no regressions in gradient norms, weight bounds, loss
 *       positivity, result struct consistency, and memory behavior
 * HOW:  Extended training loops (100 steps), repeated create/destroy
 *       cycles, and exhaustive config validation
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

class SNNBackpropRegressionTest : public ::testing::Test {
protected:
    snn_backprop_ctx_t* ctx = nullptr;
    snn_network_t* network = nullptr;
    snn_backprop_config_t config;

    static constexpr uint32_t N_INPUTS = 10;
    static constexpr uint32_t N_HIDDEN = 20;
    static constexpr uint32_t N_OUTPUTS = 5;
    static constexpr float DURATION_MS = 10.0f;

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

    bool CreateNetwork() {
        snn_config_t snn_cfg;
        int rc = snn_config_feedforward(&snn_cfg, N_INPUTS, N_HIDDEN, N_OUTPUTS);
        if (rc != SNN_SUCCESS) return false;
        network = snn_network_create(&snn_cfg);
        return (network != nullptr);
    }

    bool CreateContext() {
        if (!network) return false;
        ctx = snn_backprop_create(network, &config);
        return (ctx != nullptr);
    }

    std::vector<float> MakeInputs(uint32_t batch_size) {
        std::vector<float> inputs(batch_size * N_INPUTS);
        for (size_t i = 0; i < inputs.size(); i++) {
            inputs[i] = static_cast<float>(i % 100) / 100.0f;
        }
        return inputs;
    }

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
// 1. Gradient Norms Bounded After 100 Steps (No Explosion)
//=============================================================================

TEST_F(SNNBackpropRegressionTest, GradientNormsBoundedAfter100Steps) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    float max_grad_norm = 0.0f;

    for (int step = 0; step < 100; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Failed at step " << step;

        float gn = snn_backprop_get_gradient_norm(ctx);
        ASSERT_FALSE(std::isnan(gn)) << "NaN gradient norm at step " << step;
        ASSERT_FALSE(std::isinf(gn)) << "Inf gradient norm at step " << step;

        if (gn > max_grad_norm) {
            max_grad_norm = gn;
        }
    }

    // Gradient norms should stay bounded (gradient clipping is enabled by default)
    // Default clip norm is SNN_GRADIENT_CLIP_DEFAULT (10.0f)
    // Allow some headroom above the clip value
    EXPECT_LT(max_grad_norm, 1000.0f)
        << "Gradient norms exploded: max = " << max_grad_norm;
}

//=============================================================================
// 2. Weight Norms Remain Bounded After Training
//=============================================================================

TEST_F(SNNBackpropRegressionTest, WeightNormsBoundedAfterTraining) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    float initial_weight_norm = snn_backprop_get_weight_norm(ctx);
    EXPECT_GE(initial_weight_norm, 0.0f);

    for (int step = 0; step < 100; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Failed at step " << step;
    }

    float final_weight_norm = snn_backprop_get_weight_norm(ctx);
    EXPECT_GE(final_weight_norm, 0.0f);
    EXPECT_FALSE(std::isnan(final_weight_norm));
    EXPECT_FALSE(std::isinf(final_weight_norm));

    // Weights shouldn't explode to extreme values
    EXPECT_LT(final_weight_norm, 1e6f)
        << "Weight norms exploded after 100 steps";
}

//=============================================================================
// 3. Loss is Non-Negative for All Surrogate Methods
//=============================================================================

TEST_F(SNNBackpropRegressionTest, LossNonNegativeForAllSurrogateMethods) {
    // Test each surrogate method that is valid
    snn_surrogate_method_t methods[] = {
        (snn_surrogate_method_t)SNN_SURROGATE_SIGMOID,
        (snn_surrogate_method_t)SNN_SURROGATE_FAST_SIGMOID,
        (snn_surrogate_method_t)SNN_SURROGATE_ARCTAN,
        (snn_surrogate_method_t)SNN_SURROGATE_SUPERSPIKE,
        (snn_surrogate_method_t)SNN_SURROGATE_TRIANGULAR,
        (snn_surrogate_method_t)SNN_SURROGATE_RECTANGULAR,
    };

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    for (size_t m = 0; m < sizeof(methods) / sizeof(methods[0]); m++) {
        // Clean up from previous iteration
        if (ctx) { snn_backprop_destroy(ctx); ctx = nullptr; }
        if (network) { snn_network_destroy(network); network = nullptr; }

        config = snn_backprop_default_config(SNN_TRAIN_BPTT);
        config.surrogate.method = methods[m];

        ASSERT_TRUE(CreateNetwork())
            << "Network create failed for surrogate " << m;
        ASSERT_TRUE(CreateContext())
            << "Context create failed for surrogate " << m;

        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS)
            << "train_step failed for surrogate method " << m;

        EXPECT_GE(result.loss, 0.0f)
            << "Negative loss for surrogate method " << m
            << " (" << snn_surrogate_method_name(methods[m]) << ")";
        EXPECT_FALSE(std::isnan(result.loss))
            << "NaN loss for surrogate method " << m;
        EXPECT_FALSE(std::isinf(result.loss))
            << "Inf loss for surrogate method " << m;
    }
}

//=============================================================================
// 4. Train Step Result Struct Has Consistent Values
//=============================================================================

TEST_F(SNNBackpropRegressionTest, TrainStepResultStructConsistentValues) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    for (int step = 0; step < 10; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Failed at step " << step;

        // Loss should be non-negative
        EXPECT_GE(result.loss, 0.0f)
            << "Negative loss at step " << step;

        // Gradient norm should be non-negative
        EXPECT_GE(result.gradient_norm, 0.0f)
            << "Negative gradient norm at step " << step;

        // Weight norm should be non-negative
        EXPECT_GE(result.weight_norm, 0.0f)
            << "Negative weight norm at step " << step;

        // Mean firing rate should be non-negative
        EXPECT_GE(result.mean_firing_rate, 0.0f)
            << "Negative firing rate at step " << step;

        // Forward/backward times should be non-negative
        EXPECT_GE(result.forward_time_ms, 0.0f)
            << "Negative forward time at step " << step;
        EXPECT_GE(result.backward_time_ms, 0.0f)
            << "Negative backward time at step " << step;

        // All float fields should be finite
        EXPECT_FALSE(std::isnan(result.loss));
        EXPECT_FALSE(std::isnan(result.gradient_norm));
        EXPECT_FALSE(std::isnan(result.weight_norm));
        EXPECT_FALSE(std::isnan(result.mean_firing_rate));
    }
}

//=============================================================================
// 5. Repeated Create/Destroy Cycles Don't Leak (100 Iterations)
//=============================================================================

TEST_F(SNNBackpropRegressionTest, RepeatedCreateDestroyCyclesNoLeak) {
    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    // Destroy anything from SetUp
    if (ctx) { snn_backprop_destroy(ctx); ctx = nullptr; }
    if (network) { snn_network_destroy(network); network = nullptr; }

    for (int cycle = 0; cycle < 100; cycle++) {
        // Create network
        snn_config_t snn_cfg;
        int rc = snn_config_feedforward(&snn_cfg, N_INPUTS, N_HIDDEN, N_OUTPUTS);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Config failed at cycle " << cycle;

        snn_network_t* net = snn_network_create(&snn_cfg);
        ASSERT_NE(net, nullptr) << "Network create failed at cycle " << cycle;

        // Create backprop context
        snn_backprop_config_t cfg = snn_backprop_default_config(SNN_TRAIN_BPTT);
        snn_backprop_ctx_t* bp = snn_backprop_create(net, &cfg);
        ASSERT_NE(bp, nullptr) << "Backprop create failed at cycle " << cycle;

        // Do one training step to exercise memory allocation paths
        snn_train_result_t result = {};
        rc = snn_backprop_train_step(
            bp, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        EXPECT_EQ(rc, SNN_SUCCESS) << "Train step failed at cycle " << cycle;

        // Destroy in correct order
        snn_backprop_destroy(bp);
        snn_network_destroy(net);
    }

    // If we get here without ASAN/MSAN complaints, no leak
}

//=============================================================================
// 6. Config Validation Rejects Invalid Parameters Consistently
//=============================================================================

TEST_F(SNNBackpropRegressionTest, ConfigValidationRejectsNullConfig) {
    int rc = snn_backprop_validate_config(nullptr);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropRegressionTest, ConfigValidationRejectsZeroBatchSize) {
    config.batch_size = 0;
    int rc = snn_backprop_validate_config(&config);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropRegressionTest, ConfigValidationRejectsZeroSequenceLength) {
    config.sequence_length = 0;
    int rc = snn_backprop_validate_config(&config);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropRegressionTest, ConfigValidationRejectsNegativeLearningRate) {
    config.learning_rate = -0.01f;
    int rc = snn_backprop_validate_config(&config);
    EXPECT_NE(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropRegressionTest, ConfigValidationAcceptsDefaultConfig) {
    snn_backprop_config_t default_cfg = snn_backprop_default_config(SNN_TRAIN_BPTT);
    int rc = snn_backprop_validate_config(&default_cfg);
    EXPECT_EQ(rc, SNN_SUCCESS);
}

TEST_F(SNNBackpropRegressionTest, ConfigValidationAcceptsAllAlgorithms) {
    int algorithms[] = {
        SNN_TRAIN_BPTT,
        SNN_TRAIN_TRUNCATED_BPTT,
        SNN_TRAIN_EPROP,
        SNN_TRAIN_RTRL,
        SNN_TRAIN_SLAYER,
        SNN_TRAIN_DECOLLE,
        SNN_TRAIN_HYBRID,
    };

    for (size_t a = 0; a < sizeof(algorithms) / sizeof(algorithms[0]); a++) {
        snn_backprop_config_t cfg = snn_backprop_default_config(
            (snn_train_algorithm_t)algorithms[a]);
        int rc = snn_backprop_validate_config(&cfg);
        EXPECT_EQ(rc, SNN_SUCCESS)
            << "Default config for algorithm " << algorithms[a]
            << " (" << snn_train_algorithm_name((snn_train_algorithm_t)algorithms[a])
            << ") failed validation";
    }
}

//=============================================================================
// 7. Surrogate Gradient Values Are Bounded
//=============================================================================

TEST_F(SNNBackpropRegressionTest, SurrogateGradientValuesBounded) {
    ASSERT_TRUE(CreateNetwork());
    config.surrogate.method = (snn_surrogate_method_t)SNN_SURROGATE_SUPERSPIKE;
    config.surrogate.beta = 1.0f;
    ASSERT_TRUE(CreateContext());

    // Test a range of membrane potential values
    float test_voltages[] = {-100.0f, -10.0f, -1.0f, -0.1f, 0.0f,
                             0.1f, 1.0f, 10.0f, 100.0f};

    for (size_t i = 0; i < sizeof(test_voltages) / sizeof(test_voltages[0]); i++) {
        float grad = snn_surrogate_gradient(ctx, test_voltages[i]);
        EXPECT_FALSE(std::isnan(grad))
            << "NaN gradient at V=" << test_voltages[i];
        EXPECT_FALSE(std::isinf(grad))
            << "Inf gradient at V=" << test_voltages[i];
        EXPECT_GE(grad, 0.0f)
            << "Negative gradient at V=" << test_voltages[i];
        EXPECT_LE(grad, 100.0f)
            << "Excessive gradient at V=" << test_voltages[i];
    }
}

//=============================================================================
// 8. Statistics Accumulate Correctly Over Many Steps
//=============================================================================

TEST_F(SNNBackpropRegressionTest, StatisticsAccumulateCorrectlyOverManySteps) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    uint32_t num_steps = 50;
    for (uint32_t step = 0; step < num_steps; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Failed at step " << step;
    }

    snn_backprop_stats_t stats = {};
    int rc = snn_backprop_get_stats(ctx, &stats);
    EXPECT_EQ(rc, SNN_SUCCESS);
    EXPECT_GE(stats.total_steps, num_steps);

    // Cumulative loss should be non-negative
    EXPECT_GE(stats.total_loss, 0.0);

    // Min loss should be <= max loss
    EXPECT_LE(stats.min_loss, stats.max_loss);

    // Average gradient norm should be non-negative
    EXPECT_GE(stats.avg_grad_norm, 0.0);

    // Average firing rate should be non-negative
    EXPECT_GE(stats.avg_firing_rate, 0.0);
}

//=============================================================================
// 9. Zero Grad Fully Clears Gradients
//=============================================================================

TEST_F(SNNBackpropRegressionTest, ZeroGradFullyClearsGradients) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    // Accumulate gradients
    int rc = snn_backprop_forward(ctx, inputs.data(), 1, DURATION_MS, nullptr);
    ASSERT_EQ(rc, SNN_SUCCESS);
    rc = snn_backprop_backward(ctx, targets.data(), 1);
    ASSERT_EQ(rc, SNN_SUCCESS);

    // Zero gradients
    rc = snn_backprop_zero_grad(ctx);
    EXPECT_EQ(rc, SNN_SUCCESS);

    // Gradient norm must be exactly zero
    float gn = snn_backprop_get_gradient_norm(ctx);
    EXPECT_FLOAT_EQ(gn, 0.0f);
}

//=============================================================================
// 10. Set Surrogate Method Mid-Training
//=============================================================================

TEST_F(SNNBackpropRegressionTest, SetSurrogateMethodMidTraining) {
    ASSERT_TRUE(CreateNetwork());
    config.surrogate.method = (snn_surrogate_method_t)SNN_SURROGATE_SUPERSPIKE;
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    // Train 5 steps with SuperSpike
    for (int step = 0; step < 5; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS);
    }

    // Switch to Fast Sigmoid
    int rc = snn_backprop_set_surrogate(
        ctx, (snn_surrogate_method_t)SNN_SURROGATE_FAST_SIGMOID);
    EXPECT_EQ(rc, SNN_SUCCESS);

    // Train 5 more steps with new surrogate
    for (int step = 0; step < 5; step++) {
        snn_train_result_t result = {};
        rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Failed after surrogate switch at step " << step;
        EXPECT_GE(result.loss, 0.0f);
        EXPECT_FALSE(std::isnan(result.loss));
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
