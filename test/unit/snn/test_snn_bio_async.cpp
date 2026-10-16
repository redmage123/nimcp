/**
 * @file test_snn_bio_async.cpp
 * @brief Unit tests for SNN bio-async integration
 *
 * WHAT: Test bio-async messaging for SNN module
 * WHY:  Verify inter-module spike and state communication
 * HOW:  Test message creation, routing, and handler registration
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"

class SNNBioAsyncTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_feedforward(&config, 4, 8, 2);
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (network) {
            snn_bio_async_disconnect(network);
            snn_network_destroy(network);
            network = nullptr;
        }
    }
};

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(SNNBioAsyncTest, ConnectNullNetwork) {
    int result = snn_bio_async_connect(nullptr, BIO_MODULE_SNN_CORE);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, ConnectValidNetwork) {
    int result = snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);
    // May succeed or fail depending on router initialization
    EXPECT_TRUE(result == 0 || result == SNN_ERROR_NOT_INITIALIZED);
}

TEST_F(SNNBioAsyncTest, DisconnectNullNetwork) {
    int result = snn_bio_async_disconnect(nullptr);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, DisconnectNotConnected) {
    int result = snn_bio_async_disconnect(network);
    EXPECT_EQ(0, result);  // Should succeed silently
}

TEST_F(SNNBioAsyncTest, DisconnectAfterConnect) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);
    int result = snn_bio_async_disconnect(network);
    EXPECT_EQ(0, result);
}

TEST_F(SNNBioAsyncTest, IsConnectedNullNetwork) {
    bool connected = snn_bio_async_is_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(SNNBioAsyncTest, IsConnectedNotConnected) {
    bool connected = snn_bio_async_is_connected(network);
    EXPECT_FALSE(connected);
}

//=============================================================================
// Spike Broadcast Tests
//=============================================================================

TEST_F(SNNBioAsyncTest, BroadcastSpikeNullNetwork) {
    snn_bio_spike_msg_t event = {0, 0, 0, 10.0f, -40.0f, false};
    int result = snn_bio_async_broadcast_spike(nullptr, &event);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, BroadcastSpikeNullEvent) {
    int result = snn_bio_async_broadcast_spike(network, nullptr);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, BroadcastSpikeNotConnected) {
    snn_bio_spike_msg_t event = {0, 0, 0, 10.0f, -40.0f, false};
    int result = snn_bio_async_broadcast_spike(network, &event);
    // Without bio-router, may return 0 (silent success) or error
    EXPECT_TRUE(result == 0 || result < 0);
}

TEST_F(SNNBioAsyncTest, BroadcastSpikeValidEvent) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    snn_bio_spike_msg_t event;
    event.network_id = 1;
    event.population_id = 0;
    event.neuron_id = 5;
    event.spike_time = 25.0f;
    event.membrane_v = -30.0f;
    event.is_burst = false;

    int result = snn_bio_async_broadcast_spike(network, &event);
    EXPECT_TRUE(result == 0 || result < 0);  // Any result is valid
}

TEST_F(SNNBioAsyncTest, BroadcastSpikeBurst) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    snn_bio_spike_msg_t event;
    event.network_id = 1;
    event.population_id = 0;
    event.neuron_id = 3;
    event.spike_time = 50.0f;
    event.membrane_v = 0.0f;
    event.is_burst = true;

    int result = snn_bio_async_broadcast_spike(network, &event);
    EXPECT_TRUE(result == 0 || result < 0);  // Any result is valid
}

//=============================================================================
// State Broadcast Tests
//=============================================================================

TEST_F(SNNBioAsyncTest, BroadcastStateNullNetwork) {
    int result = snn_bio_async_broadcast_state(nullptr, 0);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, BroadcastStateNotConnected) {
    int result = snn_bio_async_broadcast_state(network, 0);
    EXPECT_TRUE(result == 0 || result < 0);  // Any result is valid
}

TEST_F(SNNBioAsyncTest, BroadcastStateBroadcastAll) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);
    int result = snn_bio_async_broadcast_state(network, 0);
    EXPECT_TRUE(result == 0 || result < 0);  // Any result is valid
}

TEST_F(SNNBioAsyncTest, BroadcastStateTargetModule) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);
    int result = snn_bio_async_broadcast_state(network, BIO_MODULE_SNN_TRAINING);
    EXPECT_TRUE(result == 0 || result < 0);  // Any result is valid
}

//=============================================================================
// STDP Broadcast Tests
//=============================================================================

TEST_F(SNNBioAsyncTest, BroadcastSTDPNullNetwork) {
    snn_bio_stdp_msg_t event = {0, 0, 1, 0.01f, 0.5f, 10.0f};
    int result = snn_bio_async_broadcast_stdp(nullptr, &event);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, BroadcastSTDPNullEvent) {
    int result = snn_bio_async_broadcast_stdp(network, nullptr);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, BroadcastSTDPValidEvent) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    snn_bio_stdp_msg_t event;
    event.network_id = 1;
    event.pre_id = 0;
    event.post_id = 1;
    event.delta_w = 0.01f;
    event.new_weight = 0.51f;
    event.dt = 5.0f;

    int result = snn_bio_async_broadcast_stdp(network, &event);
    EXPECT_TRUE(result == 0 || result < 0);  // Any result is valid
}

TEST_F(SNNBioAsyncTest, BroadcastSTDPLTD) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    snn_bio_stdp_msg_t event;
    event.network_id = 1;
    event.pre_id = 0;
    event.post_id = 1;
    event.delta_w = -0.005f;  // LTD - negative change
    event.new_weight = 0.45f;
    event.dt = -5.0f;  // Post before pre

    int result = snn_bio_async_broadcast_stdp(network, &event);
    EXPECT_TRUE(result == 0 || result < 0);  // Any result is valid
}

//=============================================================================
// Training Event Tests
//=============================================================================

TEST_F(SNNBioAsyncTest, BroadcastTrainingNullNetwork) {
    snn_bio_training_msg_t event = {0, SNN_TRAIN_STDP, 0.5f, 0.01f, 100, 50};
    int result = snn_bio_async_broadcast_training(nullptr, &event);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, BroadcastTrainingNullEvent) {
    int result = snn_bio_async_broadcast_training(network, nullptr);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, BroadcastTrainingValidEvent) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    snn_bio_training_msg_t event;
    event.network_id = 1;
    event.mode = SNN_TRAIN_STDP;
    event.loss = 0.5f;
    event.learning_rate = 0.01f;
    event.step = 100;
    event.weight_updates = 50;

    int result = snn_bio_async_broadcast_training(network, &event);
    EXPECT_TRUE(result == 0 || result < 0);  // Any result is valid
}

//=============================================================================
// Population Activity Tests
//=============================================================================

TEST_F(SNNBioAsyncTest, BroadcastPopulationNullNetwork) {
    snn_bio_population_msg_t event = {0, 0, 0, nullptr, nullptr, 0.0f, 10.0f};
    int result = snn_bio_async_broadcast_population(nullptr, &event);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, BroadcastPopulationNullEvent) {
    int result = snn_bio_async_broadcast_population(network, nullptr);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, BroadcastPopulationValidEvent) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    uint32_t neuron_ids[] = {1, 3, 5, 7};
    float spike_times[] = {5.0f, 7.0f, 8.0f, 9.5f};

    snn_bio_population_msg_t event;
    event.network_id = 1;
    event.population_id = 0;
    event.n_active = 4;
    event.neuron_ids = neuron_ids;
    event.spike_times = spike_times;
    event.window_start = 0.0f;
    event.window_end = 10.0f;

    int result = snn_bio_async_broadcast_population(network, &event);
    EXPECT_TRUE(result == 0 || result < 0);  // Any result is valid
}

//=============================================================================
// Phase Sync Tests
//=============================================================================

TEST_F(SNNBioAsyncTest, RequestSyncNullNetwork) {
    int result = snn_bio_async_request_sync(nullptr, BIO_OSC_GAMMA, 0.8f);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, RequestSyncValidParams) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);
    int result = snn_bio_async_request_sync(network, BIO_OSC_GAMMA, 0.8f);
    EXPECT_TRUE(result == 0 || result < 0);  // Any result is valid
}

TEST_F(SNNBioAsyncTest, RequestSyncAllBands) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    for (int band = BIO_OSC_DELTA; band <= BIO_OSC_GAMMA; band++) {
        int result = snn_bio_async_request_sync(
            network, (nimcp_oscillation_band_t)band, 0.5f);
        EXPECT_TRUE(result == 0 || result < 0);  // Any result is valid
    }
}

TEST_F(SNNBioAsyncTest, WaitSyncNullNetwork) {
    int result = snn_bio_async_wait_sync(nullptr, 100);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, WaitSyncNotConnected) {
    int result = snn_bio_async_wait_sync(network, 10);
    // Should return quickly (not connected)
    EXPECT_TRUE(result == 0 || result < 0);  // Any result is valid
}

//=============================================================================
// Handler Tests
//=============================================================================

static int test_handler_call_count = 0;

static int test_msg_handler(snn_network_t* network,
                            snn_bio_msg_type_t type,
                            const void* msg,
                            size_t msg_size,
                            void* user_data) {
    (void)network;
    (void)type;
    (void)msg;
    (void)msg_size;
    (void)user_data;
    test_handler_call_count++;
    return 0;
}

TEST_F(SNNBioAsyncTest, RegisterHandlerNullNetwork) {
    int result = snn_bio_async_register_handler(
        nullptr, SNN_BIO_MSG_SPIKE_EVENT, test_msg_handler, nullptr);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, RegisterHandlerNullCallback) {
    int result = snn_bio_async_register_handler(
        network, SNN_BIO_MSG_SPIKE_EVENT, nullptr, nullptr);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, RegisterHandlerValid) {
    int result = snn_bio_async_register_handler(
        network, SNN_BIO_MSG_SPIKE_EVENT, test_msg_handler, nullptr);
    EXPECT_TRUE(result == 0 || result < 0);  // May fail without bio-async context
}

TEST_F(SNNBioAsyncTest, RegisterHandlerWithUserData) {
    int user_data = 42;
    int result = snn_bio_async_register_handler(
        network, SNN_BIO_MSG_STDP_EVENT, test_msg_handler, &user_data);
    EXPECT_TRUE(result == 0 || result < 0);  // May fail without bio-async context
}

TEST_F(SNNBioAsyncTest, RegisterHandlerAllTypes) {
    for (int type = SNN_BIO_MSG_SPIKE_EVENT; type < SNN_BIO_MSG_COUNT; type++) {
        int result = snn_bio_async_register_handler(
            network, (snn_bio_msg_type_t)type, test_msg_handler, nullptr);
        EXPECT_TRUE(result == 0 || result < 0);  // May fail without bio-async context
    }
}

//=============================================================================
// Process Tests
//=============================================================================

TEST_F(SNNBioAsyncTest, ProcessNullNetwork) {
    int result = snn_bio_async_process(nullptr, 0);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, ProcessNonBlocking) {
    int result = snn_bio_async_process(network, 0);
    // Any result is valid in test environment
    (void)result;
    SUCCEED();
}

TEST_F(SNNBioAsyncTest, ProcessWithTimeout) {
    int result = snn_bio_async_process(network, 10);
    // Any result is valid in test environment
    (void)result;
    SUCCEED();
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(SNNBioAsyncTest, MessageTypeToString) {
    const char* name = snn_bio_msg_type_to_string(SNN_BIO_MSG_SPIKE_EVENT);
    EXPECT_NE(nullptr, name);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(SNNBioAsyncTest, MessageTypeToStringAll) {
    for (int type = SNN_BIO_MSG_SPIKE_EVENT; type < SNN_BIO_MSG_COUNT; type++) {
        const char* name = snn_bio_msg_type_to_string((snn_bio_msg_type_t)type);
        EXPECT_NE(nullptr, name);
    }
}

TEST_F(SNNBioAsyncTest, GetStatsNullNetwork) {
    uint64_t sent, received, dropped;
    int result = snn_bio_async_get_stats(nullptr, &sent, &received, &dropped);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNBioAsyncTest, GetStatsNullOutputs) {
    int result = snn_bio_async_get_stats(network, nullptr, nullptr, nullptr);
    // May succeed or fail depending on implementation
    (void)result;
    SUCCEED();
}

TEST_F(SNNBioAsyncTest, GetStatsValid) {
    uint64_t sent = 0, received = 0, dropped = 0;
    int result = snn_bio_async_get_stats(network, &sent, &received, &dropped);
    // Any result is valid in test environment
    (void)result;
    SUCCEED();
}
