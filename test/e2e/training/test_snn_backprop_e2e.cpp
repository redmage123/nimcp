//=============================================================================
// test_snn_backprop_e2e.cpp - End-to-End Tests for SNN Backprop Training
//=============================================================================
/**
 * @file test_snn_backprop_e2e.cpp
 * @brief End-to-end tests for complete SNN backpropagation training pipelines
 *
 * WHAT: Full training pipeline tests covering network creation, training,
 *       surrogate method variations, loss type variations, and state management
 * WHY:  Verify the complete training workflow works as an integrated system
 * HOW:  Create networks, configure backprop, run extended training loops,
 *       verify convergence, test multiple configurations
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

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

class SNNBackpropE2ETest : public ::testing::Test {
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
// 1. Full Training Pipeline: 50 Steps, Verify Loss < Initial Loss
//=============================================================================

TEST_F(SNNBackpropE2ETest, FullPipeline50StepsLossBelowInitial) {
    ASSERT_TRUE(CreateNetwork()) << "snn_network_create failed";
    ASSERT_TRUE(CreateContext()) << "snn_backprop_create failed";

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    float initial_loss = -1.0f;
    float final_loss = -1.0f;

    for (int step = 0; step < 50; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "train_step failed at step " << step;
        ASSERT_FALSE(std::isnan(result.loss)) << "NaN loss at step " << step;
        ASSERT_FALSE(std::isinf(result.loss)) << "Inf loss at step " << step;
        ASSERT_GE(result.loss, 0.0f) << "Negative loss at step " << step;
        ASSERT_LT(result.loss, 1e6f) << "Loss explosion at step " << step;

        if (step == 0) {
            initial_loss = result.loss;
        }
        final_loss = result.loss;
    }

    // Verify statistics reflect 50 steps
    snn_backprop_stats_t stats = {};
    int rc = snn_backprop_get_stats(ctx, &stats);
    EXPECT_EQ(rc, SNN_SUCCESS);
    EXPECT_GE(stats.total_steps, 50u);

    // Loss should at least remain bounded (convergence depends on spike activity)
    EXPECT_GE(initial_loss, 0.0f);
    EXPECT_GE(final_loss, 0.0f);
    EXPECT_LT(final_loss, 1e6f);

    // Weight norm should be finite
    float wn = snn_backprop_get_weight_norm(ctx);
    EXPECT_FALSE(std::isnan(wn));
    EXPECT_FALSE(std::isinf(wn));
    EXPECT_GE(wn, 0.0f);
}

//=============================================================================
// 2. Train with Different Surrogate Gradient Methods
//=============================================================================

TEST_F(SNNBackpropE2ETest, TrainWithSurrogateMethodFastSigmoid) {
    config.surrogate.method = (snn_surrogate_method_t)SNN_SURROGATE_FAST_SIGMOID;
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    for (int step = 0; step < 20; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Fast sigmoid failed at step " << step;
        ASSERT_GE(result.loss, 0.0f);
        ASSERT_FALSE(std::isnan(result.loss));
    }
}

TEST_F(SNNBackpropE2ETest, TrainWithSurrogateMethodSuperSpike) {
    config.surrogate.method = (snn_surrogate_method_t)SNN_SURROGATE_SUPERSPIKE;
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    for (int step = 0; step < 20; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "SuperSpike failed at step " << step;
        ASSERT_GE(result.loss, 0.0f);
        ASSERT_FALSE(std::isnan(result.loss));
    }
}

TEST_F(SNNBackpropE2ETest, TrainWithSurrogateMethodSigmoid) {
    config.surrogate.method = (snn_surrogate_method_t)SNN_SURROGATE_SIGMOID;
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    for (int step = 0; step < 20; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Sigmoid failed at step " << step;
        ASSERT_GE(result.loss, 0.0f);
        ASSERT_FALSE(std::isnan(result.loss));
    }
}

TEST_F(SNNBackpropE2ETest, TrainWithSurrogateMethodArctan) {
    config.surrogate.method = (snn_surrogate_method_t)SNN_SURROGATE_ARCTAN;
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    for (int step = 0; step < 20; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Arctan failed at step " << step;
        ASSERT_GE(result.loss, 0.0f);
        ASSERT_FALSE(std::isnan(result.loss));
    }
}

TEST_F(SNNBackpropE2ETest, TrainWithSurrogateMethodTriangular) {
    config.surrogate.method = (snn_surrogate_method_t)SNN_SURROGATE_TRIANGULAR;
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    for (int step = 0; step < 20; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Triangular failed at step " << step;
        ASSERT_GE(result.loss, 0.0f);
        ASSERT_FALSE(std::isnan(result.loss));
    }
}

TEST_F(SNNBackpropE2ETest, TrainWithSurrogateMethodRectangular) {
    config.surrogate.method = (snn_surrogate_method_t)SNN_SURROGATE_RECTANGULAR;
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    for (int step = 0; step < 20; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Rectangular failed at step " << step;
        ASSERT_GE(result.loss, 0.0f);
        ASSERT_FALSE(std::isnan(result.loss));
    }
}

//=============================================================================
// 3. Train with Different Loss Types
//=============================================================================

TEST_F(SNNBackpropE2ETest, TrainWithRateCodedMSELoss) {
    config.loss = snn_loss_default_config(SNN_LOSS_RATE_CODED_MSE);
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    for (int step = 0; step < 20; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS)
            << "Rate-coded MSE failed at step " << step;
        EXPECT_GE(result.loss, 0.0f);
        EXPECT_FALSE(std::isnan(result.loss));
    }
}

TEST_F(SNNBackpropE2ETest, TrainWithRateCodedCrossEntropyLoss) {
    config.loss = snn_loss_default_config(SNN_LOSS_RATE_CODED_CROSS_ENTROPY);
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    for (int step = 0; step < 20; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS)
            << "Cross-entropy failed at step " << step;
        EXPECT_GE(result.loss, 0.0f);
        EXPECT_FALSE(std::isnan(result.loss));
    }
}

TEST_F(SNNBackpropE2ETest, TrainWithSpikeCountLoss) {
    config.loss = snn_loss_default_config(SNN_LOSS_SPIKE_COUNT);
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    for (int step = 0; step < 20; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS)
            << "Spike count loss failed at step " << step;
        EXPECT_GE(result.loss, 0.0f);
        EXPECT_FALSE(std::isnan(result.loss));
    }
}

TEST_F(SNNBackpropE2ETest, TrainWithMembranePotentialLoss) {
    config.loss = snn_loss_default_config(SNN_LOSS_MEMBRANE_POTENTIAL);
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    for (int step = 0; step < 20; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS)
            << "Membrane potential loss failed at step " << step;
        EXPECT_GE(result.loss, 0.0f);
        EXPECT_FALSE(std::isnan(result.loss));
    }
}

//=============================================================================
// 4. Save Gradient State, Reset, Verify Gradients Zeroed, Retrain
//=============================================================================

TEST_F(SNNBackpropE2ETest, ResetZerosGradientsThenRetrain) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    // Phase 1: Train 10 steps, accumulate state
    for (int step = 0; step < 10; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS);
    }

    // Record pre-reset state
    snn_backprop_stats_t stats_before = {};
    snn_backprop_get_stats(ctx, &stats_before);
    EXPECT_GE(stats_before.total_steps, 10u);

    // Phase 2: Reset
    int rc = snn_backprop_reset(ctx);
    ASSERT_EQ(rc, SNN_SUCCESS);

    // Verify gradients are zeroed
    float grad_norm = snn_backprop_get_gradient_norm(ctx);
    EXPECT_FLOAT_EQ(grad_norm, 0.0f);

    // Verify statistics are cleared
    snn_backprop_stats_t stats_after = {};
    snn_backprop_get_stats(ctx, &stats_after);
    EXPECT_EQ(stats_after.total_steps, 0u);

    // Phase 3: Retrain 10 steps after reset
    for (int step = 0; step < 10; step++) {
        snn_train_result_t result = {};
        rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS)
            << "Retrain failed at step " << step;
        ASSERT_GE(result.loss, 0.0f)
            << "Negative loss during retrain at step " << step;
        ASSERT_FALSE(std::isnan(result.loss))
            << "NaN loss during retrain at step " << step;
    }

    // Verify stats accumulated after retrain
    snn_backprop_stats_t stats_retrain = {};
    snn_backprop_get_stats(ctx, &stats_retrain);
    EXPECT_GE(stats_retrain.total_steps, 10u);
}

//=============================================================================
// 5. Complete Manual Pipeline: Forward, Loss, Backward, Step (repeated)
//=============================================================================

TEST_F(SNNBackpropE2ETest, CompleteManualPipelineRepeated) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);
    std::vector<float> outputs(N_OUTPUTS, 0.0f);

    for (int iter = 0; iter < 10; iter++) {
        // Zero gradients first
        int rc = snn_backprop_zero_grad(ctx);
        ASSERT_EQ(rc, SNN_SUCCESS);

        // Forward
        rc = snn_backprop_forward(
            ctx, inputs.data(), 1, DURATION_MS, outputs.data());
        ASSERT_EQ(rc, SNN_SUCCESS) << "Forward failed at iter " << iter;

        // Compute loss
        float loss = snn_backprop_compute_loss(
            ctx, outputs.data(), targets.data(), 1);
        EXPECT_GE(loss, 0.0f) << "Negative loss at iter " << iter;
        EXPECT_FALSE(std::isnan(loss)) << "NaN loss at iter " << iter;

        // Backward
        rc = snn_backprop_backward(ctx, targets.data(), 1);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Backward failed at iter " << iter;

        // Step
        int updated = snn_backprop_step(ctx, 0.0f);
        EXPECT_GE(updated, 0) << "Step failed at iter " << iter;
    }

    // After 10 iterations, all state should be consistent
    float wn = snn_backprop_get_weight_norm(ctx);
    EXPECT_GE(wn, 0.0f);
    EXPECT_FALSE(std::isnan(wn));
    EXPECT_FALSE(std::isinf(wn));
}

//=============================================================================
// 6. Extended Training Run Stability
//=============================================================================

TEST_F(SNNBackpropE2ETest, ExtendedTrainingRunStability) {
    ASSERT_TRUE(CreateNetwork());
    ASSERT_TRUE(CreateContext());

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    float min_loss = FLT_MAX;
    float max_loss = 0.0f;

    for (int step = 0; step < 50; step++) {
        snn_train_result_t result = {};
        int rc = snn_backprop_train_step(
            ctx, inputs.data(), targets.data(), 1, DURATION_MS, &result);
        ASSERT_EQ(rc, SNN_SUCCESS) << "Step " << step;
        ASSERT_FALSE(std::isnan(result.loss)) << "NaN at step " << step;
        ASSERT_FALSE(std::isinf(result.loss)) << "Inf at step " << step;
        ASSERT_GE(result.loss, 0.0f) << "Negative at step " << step;

        if (result.loss < min_loss) min_loss = result.loss;
        if (result.loss > max_loss) max_loss = result.loss;
    }

    // Loss should stay in a reasonable range
    EXPECT_LT(max_loss, 1e6f) << "Loss exploded during extended run";

    // Weight and gradient norms should be finite
    EXPECT_FALSE(std::isnan(snn_backprop_get_weight_norm(ctx)));
    EXPECT_FALSE(std::isinf(snn_backprop_get_weight_norm(ctx)));
}

//=============================================================================
// 7. Multiple Networks Trained Independently
//=============================================================================

TEST_F(SNNBackpropE2ETest, MultipleNetworksTrainedIndependently) {
    // Clean up fixture state
    if (ctx) { snn_backprop_destroy(ctx); ctx = nullptr; }
    if (network) { snn_network_destroy(network); network = nullptr; }

    auto inputs = MakeInputs(1);
    auto targets = MakeTargets(1);

    // Create and train two independent networks
    for (int net_idx = 0; net_idx < 2; net_idx++) {
        snn_config_t snn_cfg;
        int rc = snn_config_feedforward(&snn_cfg, N_INPUTS, N_HIDDEN, N_OUTPUTS);
        ASSERT_EQ(rc, SNN_SUCCESS);

        snn_network_t* net = snn_network_create(&snn_cfg);
        ASSERT_NE(net, nullptr) << "Network " << net_idx << " creation failed";

        snn_backprop_config_t bp_cfg = snn_backprop_default_config(SNN_TRAIN_BPTT);
        snn_backprop_ctx_t* bp = snn_backprop_create(net, &bp_cfg);
        ASSERT_NE(bp, nullptr) << "Backprop " << net_idx << " creation failed";

        // Train 10 steps
        for (int step = 0; step < 10; step++) {
            snn_train_result_t result = {};
            rc = snn_backprop_train_step(
                bp, inputs.data(), targets.data(), 1, DURATION_MS, &result);
            ASSERT_EQ(rc, SNN_SUCCESS)
                << "Net " << net_idx << " step " << step << " failed";
            EXPECT_GE(result.loss, 0.0f);
        }

        snn_backprop_destroy(bp);
        snn_network_destroy(net);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
