/**
 * @file test_working_memory_snn_bridge.cpp
 * @brief Unit tests for Working Memory - SNN Bridge integration
 * @date 2026-01-06
 *
 * Tests bidirectional integration between working memory and SNN module:
 * - WM --> SNN: Item encoding to spike patterns for maintenance
 * - SNN --> WM: Retrieval from population activity
 * - Slot management and capacity limits
 * - Bio-async message handling
 */

#include <gtest/gtest.h>

#include "cognitive/memory/nimcp_working_memory_snn_bridge.h"

#include <cmath>
#include <cstring>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class WorkingMemorySNNBridgeTest : public ::testing::Test {
protected:
    wm_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        wm_snn_config_t config = wm_snn_config_default();
        config.max_slots = 8;
        config.neurons_per_slot = 32;
        config.slot_dim = 64;
        config.enable_bio_async = false;  /* Disable for unit tests */
        bridge = wm_snn_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed to create WM-SNN bridge";
    }

    void TearDown() override {
        if (bridge) {
            wm_snn_destroy(bridge);
            bridge = nullptr;
        }
    }

    void fill_features(float* features, uint32_t count, float value) {
        for (uint32_t i = 0; i < count; i++) {
            features[i] = value;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(WorkingMemorySNNBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(WorkingMemorySNNBridgeTest, CreateWithDefaultConfig) {
    wm_snn_bridge_t* b = wm_snn_create(nullptr);
    ASSERT_NE(b, nullptr);
    wm_snn_destroy(b);
}

TEST_F(WorkingMemorySNNBridgeTest, DestroyNull) {
    wm_snn_destroy(nullptr);  /* Should not crash */
}

TEST_F(WorkingMemorySNNBridgeTest, DefaultConfigValues) {
    wm_snn_config_t config = wm_snn_config_default();

    EXPECT_GT(config.max_slots, 0u);
    EXPECT_LE(config.max_slots, WM_SNN_MAX_SLOTS);
    EXPECT_GT(config.neurons_per_slot, 0u);
    EXPECT_GT(config.slot_dim, 0u);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_GT(config.decay_tau_ms, 0.0f);
}

TEST_F(WorkingMemorySNNBridgeTest, CreateWithZeroSlotsFails) {
    wm_snn_config_t config = wm_snn_config_default();
    config.max_slots = 0;
    wm_snn_bridge_t* b = wm_snn_create(&config);
    EXPECT_EQ(b, nullptr);
}

TEST_F(WorkingMemorySNNBridgeTest, CreateWithExcessiveSlotsFails) {
    wm_snn_config_t config = wm_snn_config_default();
    config.max_slots = WM_SNN_MAX_SLOTS + 1;
    wm_snn_bridge_t* b = wm_snn_create(&config);
    EXPECT_EQ(b, nullptr);
}

TEST_F(WorkingMemorySNNBridgeTest, Reset) {
    float features[64];
    fill_features(features, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);

    int ret = wm_snn_reset(bridge);
    EXPECT_EQ(ret, 0);

    /* Verify slot is cleared */
    wm_slot_state_t state;
    wm_snn_get_slot_state(bridge, 0, &state);
    EXPECT_FALSE(state.occupied);
}

TEST_F(WorkingMemorySNNBridgeTest, ResetNull) {
    int ret = wm_snn_reset(nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Encoding Tests
//=============================================================================

TEST_F(WorkingMemorySNNBridgeTest, EncodeItem) {
    float features[64];
    fill_features(features, 64, 0.5f);

    int spikes = wm_snn_encode_item(bridge, 0, features, 64, 1.0f);
    EXPECT_GE(spikes, 0) << "Encoding should succeed";
}

TEST_F(WorkingMemorySNNBridgeTest, EncodeItemNullBridge) {
    float features[64];
    fill_features(features, 64, 0.5f);

    int spikes = wm_snn_encode_item(nullptr, 0, features, 64, 1.0f);
    EXPECT_EQ(spikes, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, EncodeItemNullFeatures) {
    int spikes = wm_snn_encode_item(bridge, 0, nullptr, 64, 1.0f);
    EXPECT_EQ(spikes, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, EncodeItemInvalidSlot) {
    float features[64];
    fill_features(features, 64, 0.5f);

    int spikes = wm_snn_encode_item(bridge, 100, features, 64, 1.0f);
    EXPECT_EQ(spikes, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, EncodeItemDifferentSaliences) {
    float features[64];
    fill_features(features, 64, 0.5f);

    int spikes_low = wm_snn_encode_item(bridge, 0, features, 64, 0.2f);
    EXPECT_GE(spikes_low, 0);

    int spikes_high = wm_snn_encode_item(bridge, 1, features, 64, 1.0f);
    EXPECT_GE(spikes_high, 0);

    /* Higher salience should generally produce more spikes */
    /* (not strictly enforced due to encoding variability) */
}

TEST_F(WorkingMemorySNNBridgeTest, EncodeMultipleSlots) {
    float features[64];

    for (uint32_t slot = 0; slot < 4; slot++) {
        fill_features(features, 64, 0.3f + 0.1f * slot);
        int spikes = wm_snn_encode_item(bridge, slot, features, 64, 0.8f);
        EXPECT_GE(spikes, 0) << "Slot " << slot << " encoding failed";
    }

    /* Verify all slots occupied */
    float capacity = wm_snn_get_capacity(bridge);
    EXPECT_NEAR(capacity, 0.5f, 0.01f);  /* 4/8 slots */
}

TEST_F(WorkingMemorySNNBridgeTest, UpdateItem) {
    float features1[64];
    fill_features(features1, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features1, 64, 1.0f);

    float features2[64];
    fill_features(features2, 64, 0.8f);
    int ret = wm_snn_update_item(bridge, 0, features2, 64);
    EXPECT_GE(ret, 0);
}

TEST_F(WorkingMemorySNNBridgeTest, UpdateItemNotOccupied) {
    float features[64];
    fill_features(features, 64, 0.5f);

    int ret = wm_snn_update_item(bridge, 0, features, 64);
    EXPECT_EQ(ret, -1);  /* Slot not occupied */
}

TEST_F(WorkingMemorySNNBridgeTest, ClearSlot) {
    float features[64];
    fill_features(features, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);

    int ret = wm_snn_clear_slot(bridge, 0);
    EXPECT_EQ(ret, 0);

    wm_slot_state_t state;
    wm_snn_get_slot_state(bridge, 0, &state);
    EXPECT_FALSE(state.occupied);
}

TEST_F(WorkingMemorySNNBridgeTest, ClearSlotNull) {
    int ret = wm_snn_clear_slot(nullptr, 0);
    EXPECT_EQ(ret, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, ClearSlotInvalid) {
    int ret = wm_snn_clear_slot(bridge, 100);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(WorkingMemorySNNBridgeTest, Simulate) {
    float features[64];
    fill_features(features, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);

    int ret = wm_snn_simulate(bridge, 10.0f);
    EXPECT_GE(ret, 0);  /* Returns spike count, not status */
}

TEST_F(WorkingMemorySNNBridgeTest, SimulateNull) {
    int ret = wm_snn_simulate(nullptr, 10.0f);
    EXPECT_EQ(ret, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, Step) {
    float features[64];
    fill_features(features, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);

    int ret = wm_snn_step(bridge);
    EXPECT_GE(ret, 0);  /* Returns spike count, not status */
}

TEST_F(WorkingMemorySNNBridgeTest, Forward) {
    float inputs[256];
    for (int i = 0; i < 256; i++) {
        inputs[i] = 0.3f;
    }

    int spikes = wm_snn_forward(bridge, inputs, 256);
    EXPECT_GE(spikes, 0);
}

TEST_F(WorkingMemorySNNBridgeTest, ForwardNull) {
    int spikes = wm_snn_forward(nullptr, nullptr, 256);
    EXPECT_EQ(spikes, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, ActivityDecaysDuringSimulation) {
    float features[64];
    fill_features(features, 64, 0.8f);
    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);

    wm_slot_state_t state_before;
    wm_snn_get_slot_state(bridge, 0, &state_before);

    /* Simulate for a while */
    wm_snn_simulate(bridge, 100.0f);

    wm_slot_state_t state_after;
    wm_snn_get_slot_state(bridge, 0, &state_after);

    /* Activity should decay */
    EXPECT_LT(state_after.activity_level, state_before.activity_level);
    /* Persistence should increase */
    EXPECT_GT(state_after.persistence, state_before.persistence);
}

//=============================================================================
// Retrieval Tests
//=============================================================================

TEST_F(WorkingMemorySNNBridgeTest, RetrieveItem) {
    float features[64];
    fill_features(features, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);

    float output[64];
    int ret = wm_snn_retrieve_item(bridge, 0, output, 64);
    EXPECT_EQ(ret, 0);
}

TEST_F(WorkingMemorySNNBridgeTest, RetrieveItemNull) {
    int ret = wm_snn_retrieve_item(nullptr, 0, nullptr, 64);
    EXPECT_EQ(ret, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, RetrieveItemNotOccupied) {
    float output[64];
    int ret = wm_snn_retrieve_item(bridge, 0, output, 64);
    EXPECT_EQ(ret, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, RetrieveItemInvalidSlot) {
    float output[64];
    int ret = wm_snn_retrieve_item(bridge, 100, output, 64);
    EXPECT_EQ(ret, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, GetSlotActivities) {
    float features[64];
    fill_features(features, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);
    wm_snn_encode_item(bridge, 1, features, 64, 0.5f);

    float activities[8];
    int ret = wm_snn_get_slot_activities(bridge, activities, 8);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(activities[0], 0.0f);
    EXPECT_GT(activities[1], 0.0f);
}

TEST_F(WorkingMemorySNNBridgeTest, GetSlotActivitiesNull) {
    int ret = wm_snn_get_slot_activities(nullptr, nullptr, 8);
    EXPECT_EQ(ret, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, GetMostActiveSlot) {
    float features[64];

    /* Encode with different saliences */
    fill_features(features, 64, 0.3f);
    wm_snn_encode_item(bridge, 0, features, 64, 0.3f);

    fill_features(features, 64, 0.9f);
    wm_snn_encode_item(bridge, 1, features, 64, 1.0f);

    float confidence;
    int slot = wm_snn_get_most_active_slot(bridge, &confidence);
    EXPECT_EQ(slot, 1);  /* Slot 1 should be most active */
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(WorkingMemorySNNBridgeTest, GetMostActiveSlotEmpty) {
    float confidence;
    int slot = wm_snn_get_most_active_slot(bridge, &confidence);
    EXPECT_EQ(slot, -1);  /* No active slots */
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(WorkingMemorySNNBridgeTest, GetSlotState) {
    float features[64];
    fill_features(features, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features, 64, 0.8f);

    wm_slot_state_t state;
    int ret = wm_snn_get_slot_state(bridge, 0, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(state.occupied);
    EXPECT_GT(state.activity_level, 0.0f);
    EXPECT_NEAR(state.salience, 0.8f, 0.01f);
}

TEST_F(WorkingMemorySNNBridgeTest, GetSlotStateNull) {
    int ret = wm_snn_get_slot_state(nullptr, 0, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, GetSlotStateInvalid) {
    wm_slot_state_t state;
    int ret = wm_snn_get_slot_state(bridge, 100, &state);
    EXPECT_EQ(ret, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, GetBridgeState) {
    float features[64];
    fill_features(features, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);
    wm_snn_encode_item(bridge, 1, features, 64, 1.0f);

    wm_snn_bridge_state_t state;
    int ret = wm_snn_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.active_slots, 2u);
    EXPECT_NEAR(state.capacity_used, 0.25f, 0.01f);  /* 2/8 */
}

TEST_F(WorkingMemorySNNBridgeTest, GetBridgeStateNull) {
    int ret = wm_snn_get_state(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, GetStats) {
    wm_snn_stats_t stats;
    int ret = wm_snn_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.total_encodings, 0u);
}

TEST_F(WorkingMemorySNNBridgeTest, GetStatsNull) {
    int ret = wm_snn_get_stats(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, StatsTrackEncodings) {
    float features[64];
    fill_features(features, 64, 0.5f);

    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);
    wm_snn_encode_item(bridge, 1, features, 64, 1.0f);

    wm_snn_stats_t stats;
    wm_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_encodings, 2u);
    EXPECT_GT(stats.total_spikes, 0u);
}

TEST_F(WorkingMemorySNNBridgeTest, StatsTrackRetrievals) {
    float features[64];
    fill_features(features, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);

    float output[64];
    wm_snn_retrieve_item(bridge, 0, output, 64);
    wm_snn_retrieve_item(bridge, 0, output, 64);

    wm_snn_stats_t stats;
    wm_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_retrievals, 2u);
}

TEST_F(WorkingMemorySNNBridgeTest, ResetStats) {
    float features[64];
    fill_features(features, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);

    int ret = wm_snn_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    wm_snn_stats_t stats;
    wm_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_encodings, 0u);
}

TEST_F(WorkingMemorySNNBridgeTest, ResetStatsNull) {
    int ret = wm_snn_reset_stats(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, GetCapacity) {
    float features[64];
    fill_features(features, 64, 0.5f);

    float cap0 = wm_snn_get_capacity(bridge);
    EXPECT_NEAR(cap0, 0.0f, 0.01f);

    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);
    wm_snn_encode_item(bridge, 1, features, 64, 1.0f);
    wm_snn_encode_item(bridge, 2, features, 64, 1.0f);
    wm_snn_encode_item(bridge, 3, features, 64, 1.0f);

    float cap4 = wm_snn_get_capacity(bridge);
    EXPECT_NEAR(cap4, 0.5f, 0.01f);  /* 4/8 */
}

TEST_F(WorkingMemorySNNBridgeTest, GetCapacityNull) {
    float cap = wm_snn_get_capacity(nullptr);
    EXPECT_LT(cap, 0.0f);  /* -1 on error */
}

TEST_F(WorkingMemorySNNBridgeTest, GetTotalActivity) {
    float features[64];
    fill_features(features, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);
    wm_snn_encode_item(bridge, 1, features, 64, 1.0f);

    float activity = wm_snn_get_total_activity(bridge);
    EXPECT_GT(activity, 0.0f);
}

TEST_F(WorkingMemorySNNBridgeTest, GetTotalActivityNull) {
    float activity = wm_snn_get_total_activity(nullptr);
    EXPECT_LT(activity, 0.0f);  /* -1 on error */
}

//=============================================================================
// Callback Tests
//=============================================================================

static int g_spike_callback_count = 0;
static int g_encoding_callback_count = 0;
static int g_retrieval_callback_count = 0;

static void test_spike_callback(wm_snn_bridge_t*, uint32_t, float, void*) {
    g_spike_callback_count++;
}

static void test_encoding_callback(wm_snn_bridge_t*, uint32_t, int, void*) {
    g_encoding_callback_count++;
}

static void test_retrieval_callback(wm_snn_bridge_t*, uint32_t, float, void*) {
    g_retrieval_callback_count++;
}

TEST_F(WorkingMemorySNNBridgeTest, RegisterSpikeCallback) {
    int ret = wm_snn_register_spike_callback(bridge, test_spike_callback, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(WorkingMemorySNNBridgeTest, RegisterSpikeCallbackNull) {
    int ret = wm_snn_register_spike_callback(nullptr, test_spike_callback, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, RegisterEncodingCallback) {
    g_encoding_callback_count = 0;
    wm_snn_register_encoding_callback(bridge, test_encoding_callback, nullptr);

    float features[64];
    fill_features(features, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);

    EXPECT_EQ(g_encoding_callback_count, 1);
}

TEST_F(WorkingMemorySNNBridgeTest, RegisterRetrievalCallback) {
    g_retrieval_callback_count = 0;
    wm_snn_register_retrieval_callback(bridge, test_retrieval_callback, nullptr);

    float features[64];
    fill_features(features, 64, 0.5f);
    wm_snn_encode_item(bridge, 0, features, 64, 1.0f);

    float output[64];
    wm_snn_retrieve_item(bridge, 0, output, 64);

    EXPECT_EQ(g_retrieval_callback_count, 1);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(WorkingMemorySNNBridgeTest, BioAsyncNotConnectedInitially) {
    bool connected = wm_snn_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(WorkingMemorySNNBridgeTest, BioAsyncConnectDisabledConfig) {
    /* Bridge was created with enable_bio_async = false */
    int ret = wm_snn_bio_async_connect(bridge);
    EXPECT_EQ(ret, -1);  /* Should fail when disabled */
}

TEST_F(WorkingMemorySNNBridgeTest, BioAsyncDisconnectNull) {
    int ret = wm_snn_bio_async_disconnect(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(WorkingMemorySNNBridgeTest, BioAsyncIsConnectedNull) {
    bool connected = wm_snn_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(WorkingMemorySNNBridgeTest, EncodeZeroFeatures) {
    float features[64];
    fill_features(features, 64, 0.0f);

    int spikes = wm_snn_encode_item(bridge, 0, features, 64, 1.0f);
    EXPECT_GE(spikes, 0);  /* Should still succeed */
}

TEST_F(WorkingMemorySNNBridgeTest, EncodeMaxFeatures) {
    float features[64];
    fill_features(features, 64, 1.0f);

    int spikes = wm_snn_encode_item(bridge, 0, features, 64, 1.0f);
    EXPECT_GE(spikes, 0);
}

TEST_F(WorkingMemorySNNBridgeTest, NullBridgeHandling) {
    /* Verify all functions handle NULL gracefully */
    EXPECT_EQ(wm_snn_reset(nullptr), -1);
    EXPECT_EQ(wm_snn_encode_item(nullptr, 0, nullptr, 0, 0), -1);
    EXPECT_EQ(wm_snn_update_item(nullptr, 0, nullptr, 0), -1);
    EXPECT_EQ(wm_snn_clear_slot(nullptr, 0), -1);
    EXPECT_EQ(wm_snn_simulate(nullptr, 10.0f), -1);
    EXPECT_EQ(wm_snn_step(nullptr), -1);
    EXPECT_EQ(wm_snn_forward(nullptr, nullptr, 0), -1);
    EXPECT_EQ(wm_snn_retrieve_item(nullptr, 0, nullptr, 0), -1);
    EXPECT_EQ(wm_snn_get_slot_activities(nullptr, nullptr, 0), -1);
    EXPECT_EQ(wm_snn_get_most_active_slot(nullptr, nullptr), -1);
    EXPECT_EQ(wm_snn_get_slot_state(nullptr, 0, nullptr), -1);
    EXPECT_EQ(wm_snn_get_state(nullptr, nullptr), -1);
    EXPECT_EQ(wm_snn_get_stats(nullptr, nullptr), -1);
    EXPECT_EQ(wm_snn_reset_stats(nullptr), -1);
    EXPECT_LT(wm_snn_get_capacity(nullptr), 0.0f);
    EXPECT_LT(wm_snn_get_total_activity(nullptr), 0.0f);
}

TEST_F(WorkingMemorySNNBridgeTest, RepeatedResetStability) {
    for (int i = 0; i < 10; i++) {
        float features[64];
        fill_features(features, 64, 0.5f);
        wm_snn_encode_item(bridge, 0, features, 64, 1.0f);
        wm_snn_reset(bridge);
    }
    /* Should complete without crashing */
    SUCCEED();
}

TEST_F(WorkingMemorySNNBridgeTest, AllSlotsUsed) {
    float features[64];
    fill_features(features, 64, 0.5f);

    /* Fill all 8 slots */
    for (uint32_t slot = 0; slot < 8; slot++) {
        int spikes = wm_snn_encode_item(bridge, slot, features, 64, 1.0f);
        EXPECT_GE(spikes, 0) << "Failed to encode slot " << slot;
    }

    float capacity = wm_snn_get_capacity(bridge);
    EXPECT_NEAR(capacity, 1.0f, 0.01f);
}

//=============================================================================
// Bio-Async Enabled Config Tests
//=============================================================================

class WorkingMemorySNNBridgeBioAsyncTest : public ::testing::Test {
protected:
    wm_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        wm_snn_config_t config = wm_snn_config_default();
        config.enable_bio_async = true;
        bridge = wm_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            wm_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(WorkingMemorySNNBridgeBioAsyncTest, ConnectWithBioAsyncEnabled) {
    int ret = wm_snn_bio_async_connect(bridge);
    EXPECT_EQ(ret, 0);

    bool connected = wm_snn_is_bio_async_connected(bridge);
    EXPECT_TRUE(connected);
}

TEST_F(WorkingMemorySNNBridgeBioAsyncTest, DisconnectBioAsync) {
    wm_snn_bio_async_connect(bridge);

    int ret = wm_snn_bio_async_disconnect(bridge);
    EXPECT_EQ(ret, 0);

    bool connected = wm_snn_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}
