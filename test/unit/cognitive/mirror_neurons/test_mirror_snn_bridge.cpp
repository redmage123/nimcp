/**
 * @file test_mirror_snn_bridge.cpp
 * @brief Unit tests for Mirror Neuron - SNN Bridge integration
 * @date 2026-01-05
 *
 * Tests bidirectional integration between mirror neurons and SNN module:
 * - Mirror --> SNN: Observation/execution encoding to spikes
 * - SNN --> Mirror: Action recognition from spike output
 * - Bio-async message handling
 * - Training and STDP integration
 */

#include <gtest/gtest.h>

// Note: Header has its own extern "C" guards; CUDA headers can't be in extern "C"
#include "cognitive/mirror_neurons/nimcp_mirror_snn_bridge.h"

#include "utils/time/nimcp_time.h"

#include <cmath>
#include <cstring>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class MirrorSNNBridgeTest : public ::testing::Test {
protected:
    mirror_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        mirror_snn_config_t config = mirror_snn_config_default();
        config.input_dim = 64;
        config.hidden_dim = 128;
        config.output_dim = 16;
        config.enable_bio_async = false;  /* Disable for unit tests */
        config.enable_immune_integration = false;
        bridge = mirror_snn_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed to create mirror-SNN bridge";
    }

    void TearDown() override {
        if (bridge) {
            mirror_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(MirrorSNNBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(MirrorSNNBridgeTest, CreateWithDefaultConfig) {
    mirror_snn_bridge_t* b = mirror_snn_create(nullptr);
    ASSERT_NE(b, nullptr);
    mirror_snn_destroy(b);
}

TEST_F(MirrorSNNBridgeTest, DefaultConfigValues) {
    mirror_snn_config_t config = mirror_snn_config_default();

    EXPECT_EQ(config.input_dim, MIRROR_SNN_INPUT_DIM);
    EXPECT_EQ(config.output_dim, MIRROR_SNN_OUTPUT_DIM);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_GT(config.simulation_duration_ms, 0.0f);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_training);
}

TEST_F(MirrorSNNBridgeTest, GetSNNNetworkNotNull) {
    snn_network_t* snn = mirror_snn_get_network(bridge);
    EXPECT_NE(snn, nullptr);
}

//=============================================================================
// Mirror --> SNN Pathway Tests
//=============================================================================

TEST_F(MirrorSNNBridgeTest, EncodeObservation) {
    float features[64];
    for (int i = 0; i < 64; i++) {
        features[i] = 0.5f;
    }

    int spikes = mirror_snn_encode_observation(bridge, 1, features, 64, 1.0f);
    EXPECT_GE(spikes, 0) << "Encoding should succeed";
}

TEST_F(MirrorSNNBridgeTest, EncodeObservationNullFeatures) {
    int spikes = mirror_snn_encode_observation(bridge, 1, nullptr, 64, 1.0f);
    EXPECT_EQ(spikes, -1) << "Should fail with null features";
}

TEST_F(MirrorSNNBridgeTest, EncodeObservationZeroStrength) {
    float features[64] = {0.5f};
    int spikes = mirror_snn_encode_observation(bridge, 1, features, 64, 0.0f);
    EXPECT_GE(spikes, 0) << "Zero strength should still succeed (produces minimal spikes)";
}

TEST_F(MirrorSNNBridgeTest, EncodeExecution) {
    float motor_cmd[64];
    for (int i = 0; i < 64; i++) {
        motor_cmd[i] = 0.7f;
    }

    int spikes = mirror_snn_encode_execution(bridge, 2, motor_cmd, 64, 0.8f);
    EXPECT_GE(spikes, 0) << "Execution encoding should succeed";
}

//=============================================================================
// SNN --> Mirror Pathway Tests
//=============================================================================

TEST_F(MirrorSNNBridgeTest, Simulate) {
    float features[64];
    for (int i = 0; i < 64; i++) {
        features[i] = 0.5f;
    }

    mirror_snn_encode_observation(bridge, 1, features, 64, 1.0f);
    int spikes = mirror_snn_simulate(bridge, 50.0f);
    EXPECT_GE(spikes, 0) << "Simulation should succeed";
}

TEST_F(MirrorSNNBridgeTest, GetActionConfidences) {
    float features[64];
    for (int i = 0; i < 64; i++) {
        features[i] = 0.5f;
    }

    mirror_snn_encode_observation(bridge, 1, features, 64, 1.0f);
    mirror_snn_simulate(bridge, 50.0f);

    float confidences[16];
    int count = mirror_snn_get_action_confidences(bridge, confidences, 16);
    EXPECT_GE(count, 0) << "Should get confidence scores";

    /* Confidences should be in valid range */
    for (int i = 0; i < 16; i++) {
        EXPECT_GE(confidences[i], 0.0f);
        EXPECT_LE(confidences[i], 1.0f);
    }
}

TEST_F(MirrorSNNBridgeTest, GetPopulationRate) {
    float features[64];
    for (int i = 0; i < 64; i++) {
        features[i] = 0.8f;
    }

    mirror_snn_encode_observation(bridge, 0, features, 64, 1.0f);
    mirror_snn_simulate(bridge, 100.0f);

    float rate = mirror_snn_get_population_rate(bridge, 0, 100.0f);
    EXPECT_GE(rate, 0.0f) << "Population rate should be non-negative";
}

//=============================================================================
// Complete Forward Pass Tests
//=============================================================================

TEST_F(MirrorSNNBridgeTest, Forward) {
    float features[64];
    for (int i = 0; i < 64; i++) {
        features[i] = 0.6f + 0.2f * std::sin((float)i / 10.0f);
    }

    uint32_t recognized_action;
    float confidence;

    int ret = mirror_snn_forward(bridge, 3, features, 64, 0.9f,
                                  &recognized_action, &confidence);

    /* Forward may return -1 if no confident recognition */
    if (ret == 0) {
        EXPECT_LT(recognized_action, 16u);
        EXPECT_GE(confidence, 0.0f);
        EXPECT_LE(confidence, 1.0f);
    }
}

//=============================================================================
// Training Tests
//=============================================================================

TEST_F(MirrorSNNBridgeTest, SetTrainingMode) {
    int ret = mirror_snn_set_training(bridge, true);
    EXPECT_EQ(ret, 0) << "Should enable training mode";

    ret = mirror_snn_set_training(bridge, false);
    EXPECT_EQ(ret, 0) << "Should disable training mode";
}

TEST_F(MirrorSNNBridgeTest, ApplySTDP) {
    mirror_snn_set_training(bridge, true);

    float features[64];
    for (int i = 0; i < 64; i++) {
        features[i] = 0.5f;
    }

    /* Create some activity */
    mirror_snn_encode_observation(bridge, 1, features, 64, 1.0f);
    mirror_snn_simulate(bridge, 50.0f);

    int updates = mirror_snn_apply_stdp(bridge);
    EXPECT_GE(updates, 0) << "STDP should return update count";
}

TEST_F(MirrorSNNBridgeTest, ApplyReward) {
    mirror_snn_set_training(bridge, true);

    float features[64];
    for (int i = 0; i < 64; i++) {
        features[i] = 0.5f;
    }

    mirror_snn_encode_observation(bridge, 1, features, 64, 1.0f);
    mirror_snn_simulate(bridge, 50.0f);

    int updates = mirror_snn_apply_reward(bridge, 0.5f);
    EXPECT_GE(updates, 0) << "Reward-modulated learning should succeed";
}

TEST_F(MirrorSNNBridgeTest, TrainStep) {
    mirror_snn_set_training(bridge, true);

    float features[64];
    for (int i = 0; i < 64; i++) {
        features[i] = 0.5f;
    }

    float loss = mirror_snn_train_step(bridge, features, 64, 3);
    /* Loss can be positive or negative initially */
    EXPECT_FALSE(std::isnan(loss)) << "Loss should not be NaN";
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(MirrorSNNBridgeTest, GetState) {
    mirror_snn_bridge_state_t state;
    int ret = mirror_snn_get_state(bridge, &state);

    EXPECT_EQ(ret, 0) << "Should get state";
    EXPECT_EQ(state.state, MIRROR_SNN_STATE_IDLE);
}

TEST_F(MirrorSNNBridgeTest, GetStats) {
    mirror_snn_stats_t stats;
    int ret = mirror_snn_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0) << "Should get stats";
    EXPECT_EQ(stats.total_observations, 0u);
}

TEST_F(MirrorSNNBridgeTest, ResetStats) {
    float features[64] = {0.5f};
    mirror_snn_encode_observation(bridge, 1, features, 64, 1.0f);

    mirror_snn_stats_t stats;
    mirror_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_observations, 0u);

    mirror_snn_reset_stats(bridge);

    mirror_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_observations, 0u);
}

TEST_F(MirrorSNNBridgeTest, CheckHealth) {
    snn_state_health_t health = mirror_snn_check_health(bridge);
    EXPECT_EQ(health, SNN_STATE_HEALTHY);
}

TEST_F(MirrorSNNBridgeTest, GetActionState) {
    float features[64];
    for (int i = 0; i < 64; i++) {
        features[i] = 0.5f;
    }

    mirror_snn_encode_observation(bridge, 5, features, 64, 1.0f);

    mirror_snn_action_state_t action_state;
    int ret = mirror_snn_get_action_state(bridge, 5, &action_state);

    EXPECT_EQ(ret, 0) << "Should get action state";
    EXPECT_EQ(action_state.action_id, 5u);
    EXPECT_EQ(action_state.status, MIRROR_SNN_ACTION_OBSERVED);
}

//=============================================================================
// Update Loop Tests
//=============================================================================

TEST_F(MirrorSNNBridgeTest, Update) {
    int ret = mirror_snn_update(bridge, 10.0f);
    EXPECT_EQ(ret, 0) << "Update should succeed";
}

TEST_F(MirrorSNNBridgeTest, Reset) {
    float features[64] = {0.5f};
    mirror_snn_encode_observation(bridge, 1, features, 64, 1.0f);
    mirror_snn_simulate(bridge, 50.0f);

    int ret = mirror_snn_reset(bridge);
    EXPECT_EQ(ret, 0) << "Reset should succeed";

    mirror_snn_bridge_state_t state;
    mirror_snn_get_state(bridge, &state);
    EXPECT_EQ(state.state, MIRROR_SNN_STATE_IDLE);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int g_spike_callback_count = 0;
static void test_spike_callback(uint32_t pop_id, uint32_t neuron_id,
                                 float spike_time, void* user_data) {
    g_spike_callback_count++;
}

TEST_F(MirrorSNNBridgeTest, RegisterSpikeCallback) {
    g_spike_callback_count = 0;
    int ret = mirror_snn_register_spike_callback(bridge, test_spike_callback, nullptr);
    EXPECT_EQ(ret, 0) << "Should register callback";
}

static int g_recognition_callback_count = 0;
static uint32_t g_last_recognized_action = 0;
static void test_recognition_callback(uint32_t action_id, float confidence,
                                       float latency_ms, void* user_data) {
    g_recognition_callback_count++;
    g_last_recognized_action = action_id;
}

TEST_F(MirrorSNNBridgeTest, RegisterRecognitionCallback) {
    g_recognition_callback_count = 0;
    int ret = mirror_snn_register_recognition_callback(bridge, test_recognition_callback, nullptr);
    EXPECT_EQ(ret, 0) << "Should register callback";
}

static int g_training_callback_count = 0;
static void test_training_callback(uint32_t synapse_id, float weight_change,
                                    float new_weight, void* user_data) {
    g_training_callback_count++;
}

TEST_F(MirrorSNNBridgeTest, RegisterTrainingCallback) {
    g_training_callback_count = 0;
    int ret = mirror_snn_register_training_callback(bridge, test_training_callback, nullptr);
    EXPECT_EQ(ret, 0) << "Should register callback";
}

static int g_health_callback_count = 0;
static void test_health_callback(snn_state_health_t old_health,
                                  snn_state_health_t new_health, void* user_data) {
    g_health_callback_count++;
}

TEST_F(MirrorSNNBridgeTest, RegisterHealthCallback) {
    g_health_callback_count = 0;
    int ret = mirror_snn_register_health_callback(bridge, test_health_callback, nullptr);
    EXPECT_EQ(ret, 0) << "Should register callback";
}

//=============================================================================
// SNN Statistics Tests
//=============================================================================

TEST_F(MirrorSNNBridgeTest, GetSNNStats) {
    snn_stats_t stats;
    int ret = mirror_snn_get_snn_stats(bridge, &stats);
    EXPECT_EQ(ret, 0) << "Should get SNN stats";
}

//=============================================================================
// Null Parameter Handling
//=============================================================================

TEST_F(MirrorSNNBridgeTest, NullBridgeHandling) {
    EXPECT_EQ(mirror_snn_encode_observation(nullptr, 0, nullptr, 0, 0), -1);
    EXPECT_EQ(mirror_snn_simulate(nullptr, 0), -1);
    EXPECT_EQ(mirror_snn_update(nullptr, 0), -1);
    EXPECT_EQ(mirror_snn_reset(nullptr), -1);
    EXPECT_EQ(mirror_snn_get_network(nullptr), nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
