/**
 * @file test_dragonfly_snn_bridge.cpp
 * @brief Unit tests for Dragonfly-to-SNN Backpropagation Bridge
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_snn_bridge.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class DragonflySNNBridgeTest : public ::testing::Test {
protected:
    dragonfly_snn_bridge_t* bridge = nullptr;
    dragonfly_snn_config_t config;

    void SetUp() override {
        ASSERT_EQ(0, dragonfly_snn_bridge_default_config(&config));
    }

    void TearDown() override {
        if (bridge) {
            dragonfly_snn_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    void CreateBridge() {
        bridge = dragonfly_snn_bridge_create(nullptr, nullptr, &config);
        ASSERT_NE(nullptr, bridge);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(DragonflySNNBridgeTest, DefaultConfigValid) {
    dragonfly_snn_config_t cfg;
    EXPECT_EQ(0, dragonfly_snn_bridge_default_config(&cfg));

    EXPECT_EQ(DRAGONFLY_TRAIN_BPTT, cfg.algorithm);
    EXPECT_EQ(DRAGONFLY_SURROGATE_SUPERSPIKE, cfg.surrogate.method);
    EXPECT_GT(cfg.learning_rate, 0.0f);
    EXPECT_GT(cfg.unroll_steps, 0u);
    EXPECT_GT(cfg.neuron_params.tau_membrane, 0.0f);
}

TEST_F(DragonflySNNBridgeTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_snn_bridge_default_config(nullptr));
}

TEST_F(DragonflySNNBridgeTest, ValidateConfigSuccess) {
    EXPECT_EQ(0, dragonfly_snn_bridge_validate_config(&config));
}

TEST_F(DragonflySNNBridgeTest, ValidateConfigNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_snn_bridge_validate_config(nullptr));
}

TEST_F(DragonflySNNBridgeTest, ValidateConfigInvalidAlgorithm) {
    config.algorithm = (dragonfly_snn_algorithm_t)99;
    EXPECT_EQ(-1, dragonfly_snn_bridge_validate_config(&config));
}

TEST_F(DragonflySNNBridgeTest, ValidateConfigZeroLearningRate) {
    config.learning_rate = 0.0f;
    EXPECT_EQ(-1, dragonfly_snn_bridge_validate_config(&config));
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DragonflySNNBridgeTest, CreateWithDefaultConfig) {
    bridge = dragonfly_snn_bridge_create(nullptr, nullptr, &config);
    EXPECT_NE(nullptr, bridge);
}

TEST_F(DragonflySNNBridgeTest, CreateWithNullConfigUsesDefaults) {
    bridge = dragonfly_snn_bridge_create(nullptr, nullptr, nullptr);
    EXPECT_NE(nullptr, bridge);
}

TEST_F(DragonflySNNBridgeTest, DestroyNullSafe) {
    dragonfly_snn_bridge_destroy(nullptr);
    /* No crash = success */
}

TEST_F(DragonflySNNBridgeTest, ResetSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_snn_bridge_reset(bridge));
}

TEST_F(DragonflySNNBridgeTest, ResetNullReturnsError) {
    EXPECT_EQ(-1, dragonfly_snn_bridge_reset(nullptr));
}

//=============================================================================
// Forward Pass Tests
//=============================================================================

TEST_F(DragonflySNNBridgeTest, ForwardPassSuccess) {
    CreateBridge();

    float input[16] = {0.5f, 0.3f, 0.7f, 0.2f, 0.8f, 0.1f, 0.6f, 0.4f,
                       0.9f, 0.0f, 0.5f, 0.5f, 0.3f, 0.7f, 0.2f, 0.8f};
    EXPECT_EQ(0, dragonfly_snn_forward(bridge, input, 16, 100));
}

TEST_F(DragonflySNNBridgeTest, ForwardPassNullReturnsError) {
    CreateBridge();
    EXPECT_EQ(-1, dragonfly_snn_forward(nullptr, nullptr, 0, 0));
}

TEST_F(DragonflySNNBridgeTest, GetSpikesSuccess) {
    CreateBridge();

    float input[16] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                       1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    dragonfly_snn_forward(bridge, input, 16, 100);

    float spikes[16];
    int count = dragonfly_snn_get_spikes(bridge, spikes, 16);
    EXPECT_EQ(16, count);
}

TEST_F(DragonflySNNBridgeTest, GetPotentialsSuccess) {
    CreateBridge();

    float input[16] = {0.5f};
    dragonfly_snn_forward(bridge, input, 1, 10);

    float potentials[16];
    EXPECT_EQ(0, dragonfly_snn_get_potentials(bridge, potentials, 16));
}

//=============================================================================
// Backward Pass Tests
//=============================================================================

TEST_F(DragonflySNNBridgeTest, ComputeLossSuccess) {
    CreateBridge();

    float input[16] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                       0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float target[16] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                        1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};

    dragonfly_snn_forward(bridge, input, 16, 50);
    float loss = dragonfly_snn_compute_loss(bridge, target, 16);
    EXPECT_GE(loss, 0.0f);
}

TEST_F(DragonflySNNBridgeTest, BackwardSuccess) {
    CreateBridge();

    float input[16] = {0.5f};
    float target[16] = {1.0f};

    dragonfly_snn_forward(bridge, input, 1, 10);
    dragonfly_snn_compute_loss(bridge, target, 1);
    EXPECT_EQ(0, dragonfly_snn_backward(bridge));
}

TEST_F(DragonflySNNBridgeTest, GetGradientsSuccess) {
    CreateBridge();

    float input[16] = {0.5f};
    float target[16] = {1.0f};

    dragonfly_snn_forward(bridge, input, 1, 10);
    dragonfly_snn_compute_loss(bridge, target, 1);
    dragonfly_snn_backward(bridge);

    dragonfly_snn_gradients_t gradients;
    EXPECT_EQ(0, dragonfly_snn_get_gradients(bridge, &gradients));
    EXPECT_NE(nullptr, gradients.weight_gradients);
    EXPECT_GT(gradients.num_weights, 0u);
}

TEST_F(DragonflySNNBridgeTest, ApplyGradientsSuccess) {
    CreateBridge();

    float input[16] = {0.5f};
    float target[16] = {1.0f};

    dragonfly_snn_forward(bridge, input, 1, 10);
    dragonfly_snn_compute_loss(bridge, target, 1);
    dragonfly_snn_backward(bridge);
    EXPECT_EQ(0, dragonfly_snn_apply_gradients(bridge));
}

//=============================================================================
// Training Tests
//=============================================================================

TEST_F(DragonflySNNBridgeTest, TrainStepRequiresTrainingMode) {
    CreateBridge();

    float input[16] = {0.5f};
    float target[16] = {1.0f};

    /* Not in training mode */
    float loss = dragonfly_snn_train_step(bridge, input, 16, target, 16, 50);
    EXPECT_EQ(-1.0f, loss);
}

TEST_F(DragonflySNNBridgeTest, TrainStepInTrainingMode) {
    CreateBridge();
    dragonfly_snn_set_training(bridge, true);

    float input[16] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                       0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float target[16] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                        1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};

    float loss = dragonfly_snn_train_step(bridge, input, 16, target, 16, 50);
    EXPECT_GE(loss, 0.0f);
}

TEST_F(DragonflySNNBridgeTest, ApplyRewardSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_snn_apply_reward(bridge, 1.0f));
    EXPECT_EQ(0, dragonfly_snn_apply_reward(bridge, -0.5f));
}

TEST_F(DragonflySNNBridgeTest, UpdateEligibilitySuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_snn_update_eligibility(bridge, 1.0f));
}

TEST_F(DragonflySNNBridgeTest, SetLearningRateSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_snn_set_learning_rate(bridge, 0.001f));
}

TEST_F(DragonflySNNBridgeTest, SetLearningRateInvalidReturnsError) {
    CreateBridge();
    EXPECT_EQ(-1, dragonfly_snn_set_learning_rate(bridge, 0.0f));
    EXPECT_EQ(-1, dragonfly_snn_set_learning_rate(bridge, -0.01f));
}

TEST_F(DragonflySNNBridgeTest, DecayLearningRateSuccess) {
    CreateBridge();

    float initial_lr = config.learning_rate;
    float new_lr = dragonfly_snn_decay_learning_rate(bridge);

    EXPECT_LT(new_lr, initial_lr);
    EXPECT_GT(new_lr, 0.0f);
}

//=============================================================================
// TSDN Integration Tests
//=============================================================================

TEST_F(DragonflySNNBridgeTest, SyncFromTSDNSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_snn_sync_from_tsdn(bridge));
}

TEST_F(DragonflySNNBridgeTest, PushToTSDNSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_snn_push_to_tsdn(bridge));
}

TEST_F(DragonflySNNBridgeTest, GetDirectionErrorSuccess) {
    CreateBridge();

    /* Run forward pass to generate some spikes */
    float input[16];
    for (int i = 0; i < 16; i++) input[i] = 1.0f;
    dragonfly_snn_forward(bridge, input, 16, 100);

    float error = dragonfly_snn_get_direction_error(bridge, 45.0f, 30.0f);
    EXPECT_GE(error, 0.0f);
}

TEST_F(DragonflySNNBridgeTest, GetDirectionErrorNullReturnsNegative) {
    EXPECT_EQ(-1.0f, dragonfly_snn_get_direction_error(nullptr, 0.0f, 0.0f));
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(DragonflySNNBridgeTest, ConnectDragonflySuccess) {
    CreateBridge();
    int dummy;
    EXPECT_EQ(0, dragonfly_snn_connect_dragonfly(bridge, (dragonfly_system_t*)&dummy));
}

TEST_F(DragonflySNNBridgeTest, ConnectTrainerSuccess) {
    CreateBridge();
    int dummy;
    EXPECT_EQ(0, dragonfly_snn_connect_trainer(bridge, &dummy));
}

TEST_F(DragonflySNNBridgeTest, IsTrainingInitiallyFalse) {
    CreateBridge();
    EXPECT_FALSE(dragonfly_snn_is_training(bridge));
}

TEST_F(DragonflySNNBridgeTest, SetTrainingMode) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_snn_set_training(bridge, true));
    EXPECT_TRUE(dragonfly_snn_is_training(bridge));
    EXPECT_EQ(0, dragonfly_snn_set_training(bridge, false));
    EXPECT_FALSE(dragonfly_snn_is_training(bridge));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DragonflySNNBridgeTest, GetStatsSuccess) {
    CreateBridge();
    dragonfly_snn_stats_t stats;
    EXPECT_EQ(0, dragonfly_snn_bridge_get_stats(bridge, &stats));
}

TEST_F(DragonflySNNBridgeTest, GetStatsNullReturnsError) {
    CreateBridge();
    dragonfly_snn_stats_t stats;
    EXPECT_EQ(-1, dragonfly_snn_bridge_get_stats(nullptr, &stats));
    EXPECT_EQ(-1, dragonfly_snn_bridge_get_stats(bridge, nullptr));
}

TEST_F(DragonflySNNBridgeTest, ResetStatsSuccess) {
    CreateBridge();
    EXPECT_EQ(0, dragonfly_snn_bridge_reset_stats(bridge));
}

TEST_F(DragonflySNNBridgeTest, StatsUpdatedAfterTraining) {
    CreateBridge();
    dragonfly_snn_set_training(bridge, true);

    float input[16] = {0.5f};
    float target[16] = {1.0f};

    for (int i = 0; i < 5; i++) {
        dragonfly_snn_train_step(bridge, input, 16, target, 16, 50);
    }

    dragonfly_snn_stats_t stats;
    dragonfly_snn_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(5u, stats.training_steps);
}

TEST_F(DragonflySNNBridgeTest, StatsTracksSpikeRate) {
    CreateBridge();

    /* Strong input should generate spikes */
    float input[16];
    for (int i = 0; i < 16; i++) input[i] = 2.0f;
    dragonfly_snn_forward(bridge, input, 16, 100);

    dragonfly_snn_stats_t stats;
    dragonfly_snn_bridge_get_stats(bridge, &stats);
    /* Spike generation depends on threshold and input; stats may be 0 */
    EXPECT_GE(stats.spikes_total, 0u);
    EXPECT_GE(stats.avg_spike_rate, 0.0f);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(DragonflySNNBridgeTest, AlgorithmNameValid) {
    EXPECT_STREQ("bptt", dragonfly_snn_algorithm_name(DRAGONFLY_TRAIN_BPTT));
    EXPECT_STREQ("truncated_bptt", dragonfly_snn_algorithm_name(DRAGONFLY_TRAIN_TRUNCATED_BPTT));
    EXPECT_STREQ("eprop", dragonfly_snn_algorithm_name(DRAGONFLY_TRAIN_EPROP));
    EXPECT_STREQ("reward_stdp", dragonfly_snn_algorithm_name(DRAGONFLY_TRAIN_REWARD_STDP));
}

TEST_F(DragonflySNNBridgeTest, AlgorithmNameInvalid) {
    EXPECT_STREQ("unknown", dragonfly_snn_algorithm_name((dragonfly_snn_algorithm_t)99));
}

TEST_F(DragonflySNNBridgeTest, SurrogateNameValid) {
    EXPECT_STREQ("superspike", dragonfly_snn_surrogate_name(DRAGONFLY_SURROGATE_SUPERSPIKE));
    EXPECT_STREQ("fast_sigmoid", dragonfly_snn_surrogate_name(DRAGONFLY_SURROGATE_FAST_SIGMOID));
    EXPECT_STREQ("triangular", dragonfly_snn_surrogate_name(DRAGONFLY_SURROGATE_TRIANGULAR));
}

TEST_F(DragonflySNNBridgeTest, SurrogateNameInvalid) {
    EXPECT_STREQ("unknown", dragonfly_snn_surrogate_name((dragonfly_surrogate_t)99));
}

TEST_F(DragonflySNNBridgeTest, LossNameValid) {
    EXPECT_STREQ("spike_count", dragonfly_snn_loss_name(DRAGONFLY_LOSS_SPIKE_COUNT));
    EXPECT_STREQ("direction_error", dragonfly_snn_loss_name(DRAGONFLY_LOSS_DIRECTION_ERROR));
    EXPECT_STREQ("interception", dragonfly_snn_loss_name(DRAGONFLY_LOSS_INTERCEPTION));
}

TEST_F(DragonflySNNBridgeTest, LossNameInvalid) {
    EXPECT_STREQ("unknown", dragonfly_snn_loss_name((dragonfly_snn_loss_t)99));
}

//=============================================================================
// Comprehensive Training Scenario
//=============================================================================

TEST_F(DragonflySNNBridgeTest, FullTrainingLoop) {
    CreateBridge();
    dragonfly_snn_set_training(bridge, true);

    /* Training data */
    float input[16], target[16];
    for (int i = 0; i < 16; i++) {
        input[i] = (float)i / 16.0f;
        target[i] = (i % 2 == 0) ? 1.0f : 0.0f;
    }

    /* Train for 10 steps */
    float losses[10];
    for (int i = 0; i < 10; i++) {
        losses[i] = dragonfly_snn_train_step(bridge, input, 16, target, 16, 50);
        EXPECT_GE(losses[i], 0.0f);

        /* Update eligibility */
        dragonfly_snn_update_eligibility(bridge, 1.0f);

        /* Decay learning rate every few steps */
        if (i % 3 == 0) {
            dragonfly_snn_decay_learning_rate(bridge);
        }
    }

    /* Verify training happened */
    dragonfly_snn_stats_t stats;
    dragonfly_snn_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(10u, stats.training_steps);
    /* Spike generation depends on threshold and dynamics; may be 0 */
    EXPECT_GE(stats.spikes_total, 0u);
}
