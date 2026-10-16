/**
 * @file test_vta_adapter.cpp
 * @brief Unit tests for VTA Adapter
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/vta/nimcp_vta_adapter.h"
#include "core/brain/regions/vta/nimcp_vta.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class VTAAdapterTest : public ::testing::Test {
protected:
    nimcp_vta_adapter_t adapter;

    void SetUp() override {
        adapter = nimcp_vta_adapter_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            nimcp_vta_adapter_destroy(adapter);
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(VTAAdapterTest, CreateSucceeds) {
    EXPECT_NE(adapter, nullptr);
}

TEST_F(VTAAdapterTest, CreateWithConfigSucceeds) {
    nimcp_vta_adapter_destroy(adapter);

    nimcp_vta_adapter_config_t config = nimcp_vta_adapter_default_config();
    config.enable_bio_async = false;
    config.auto_create_projections = false;

    adapter = nimcp_vta_adapter_create(&config);
    EXPECT_NE(adapter, nullptr);
}

TEST_F(VTAAdapterTest, DestroyNullSafe) {
    nimcp_vta_adapter_destroy(nullptr);  /* Should not crash */
}

TEST_F(VTAAdapterTest, DefaultConfigValid) {
    nimcp_vta_adapter_config_t config = nimcp_vta_adapter_default_config();
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.auto_create_projections);
    EXPECT_GT(config.message_rate_limit, 0.0f);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(VTAAdapterTest, ConnectBrainSucceeds) {
    int err = nimcp_vta_adapter_connect_brain(adapter, nullptr);
    EXPECT_EQ(err, 0);
}

TEST_F(VTAAdapterTest, ConnectBrainNullAdapterReturnsError) {
    int err = nimcp_vta_adapter_connect_brain(nullptr, nullptr);
    EXPECT_EQ(err, -1);
}

TEST_F(VTAAdapterTest, DisconnectSucceeds) {
    int err = nimcp_vta_adapter_disconnect(adapter);
    EXPECT_EQ(err, 0);
}

TEST_F(VTAAdapterTest, DisconnectNullReturnsError) {
    int err = nimcp_vta_adapter_disconnect(nullptr);
    EXPECT_EQ(err, -1);
}

TEST_F(VTAAdapterTest, SetRouterSucceeds) {
    int err = nimcp_vta_adapter_set_router(adapter, nullptr);
    EXPECT_EQ(err, 0);
}

TEST_F(VTAAdapterTest, ConnectImmuneSucceeds) {
    int err = nimcp_vta_adapter_connect_immune(adapter, nullptr);
    EXPECT_EQ(err, 0);
}

//=============================================================================
// VTA Access Tests
//=============================================================================

TEST_F(VTAAdapterTest, GetVTAReturnsValid) {
    nimcp_vta_system_t* vta = nimcp_vta_adapter_get_vta(adapter);
    EXPECT_NE(vta, nullptr);
    EXPECT_TRUE(vta->initialized);
}

TEST_F(VTAAdapterTest, GetVTANullReturnsNull) {
    nimcp_vta_system_t* vta = nimcp_vta_adapter_get_vta(nullptr);
    EXPECT_EQ(vta, nullptr);
}

TEST_F(VTAAdapterTest, VTAHasProjections) {
    nimcp_vta_system_t* vta = nimcp_vta_adapter_get_vta(adapter);
    /* With auto_create_projections = true, should have standard projections */
    EXPECT_GT(vta->num_projections, 0u);
}

//=============================================================================
// Messaging Tests
//=============================================================================

TEST_F(VTAAdapterTest, SendMessageSucceeds) {
    nimcp_vta_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = VTA_MSG_REWARD;
    msg.data.reward.reward_magnitude = 1.0f;

    int err = nimcp_vta_adapter_send_message(adapter, &msg);
    EXPECT_EQ(err, 0);
}

TEST_F(VTAAdapterTest, SendMessageNullReturnsError) {
    nimcp_vta_message_t msg;
    EXPECT_EQ(nimcp_vta_adapter_send_message(nullptr, &msg), -1);
    EXPECT_EQ(nimcp_vta_adapter_send_message(adapter, nullptr), -1);
}

TEST_F(VTAAdapterTest, ProcessMessagesSucceeds) {
    nimcp_vta_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = VTA_MSG_REWARD;
    msg.data.reward.reward_magnitude = 1.0f;
    nimcp_vta_adapter_send_message(adapter, &msg);

    int processed = nimcp_vta_adapter_process_messages(adapter, 10);
    EXPECT_GT(processed, 0);
}

TEST_F(VTAAdapterTest, ProcessMessagesNullReturnsError) {
    int result = nimcp_vta_adapter_process_messages(nullptr, 10);
    EXPECT_EQ(result, -1);
}

static int callback_count = 0;
static void test_callback(nimcp_vta_adapter_t, const nimcp_vta_message_t*, void*) {
    callback_count++;
}

TEST_F(VTAAdapterTest, RegisterCallbackSucceeds) {
    int err = nimcp_vta_adapter_register_callback(adapter, VTA_MSG_REWARD, test_callback, nullptr);
    EXPECT_EQ(err, 0);
}

TEST_F(VTAAdapterTest, RegisterCallbackNullReturnsError) {
    EXPECT_EQ(nimcp_vta_adapter_register_callback(nullptr, VTA_MSG_REWARD, test_callback, nullptr), -1);
    EXPECT_EQ(nimcp_vta_adapter_register_callback(adapter, VTA_MSG_REWARD, nullptr, nullptr), -1);
}

TEST_F(VTAAdapterTest, CallbackInvoked) {
    callback_count = 0;
    nimcp_vta_adapter_register_callback(adapter, VTA_MSG_REWARD, test_callback, nullptr);

    nimcp_vta_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = VTA_MSG_REWARD;
    msg.data.reward.reward_magnitude = 1.0f;
    nimcp_vta_adapter_send_message(adapter, &msg);

    nimcp_vta_adapter_process_messages(adapter, 10);
    EXPECT_GT(callback_count, 0);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(VTAAdapterTest, UpdateSucceeds) {
    int err = nimcp_vta_adapter_update(adapter, 10.0f);
    EXPECT_EQ(err, 0);
}

TEST_F(VTAAdapterTest, UpdateNullReturnsError) {
    int err = nimcp_vta_adapter_update(nullptr, 10.0f);
    EXPECT_EQ(err, -1);
}

TEST_F(VTAAdapterTest, MultipleUpdatesStable) {
    for (int i = 0; i < 1000; i++) {
        int err = nimcp_vta_adapter_update(adapter, 10.0f);
        EXPECT_EQ(err, 0);
    }

    nimcp_vta_adapter_state_t state;
    nimcp_vta_adapter_get_state(adapter, &state);
    EXPECT_FALSE(std::isnan(state.da_level));
    EXPECT_FALSE(std::isinf(state.da_level));
}

//=============================================================================
// Reward Processing Tests
//=============================================================================

TEST_F(VTAAdapterTest, ProcessRewardSucceeds) {
    int err = nimcp_vta_adapter_process_reward(adapter, 1.0f);
    EXPECT_EQ(err, 0);
}

TEST_F(VTAAdapterTest, ProcessRewardNullReturnsError) {
    int err = nimcp_vta_adapter_process_reward(nullptr, 1.0f);
    EXPECT_EQ(err, -1);
}

TEST_F(VTAAdapterTest, ProcessRewardGeneratesRPE) {
    nimcp_vta_adapter_process_reward(adapter, 1.0f);
    nimcp_vta_adapter_process_messages(adapter, 10);

    nimcp_vta_adapter_state_t state;
    nimcp_vta_adapter_get_state(adapter, &state);
    /* After unexpected reward, should have positive RPE */
    EXPECT_GT(state.current_rpe, 0.0f);
}

TEST_F(VTAAdapterTest, ProcessCueSucceeds) {
    int err = nimcp_vta_adapter_process_cue(adapter, 1, 0.8f);
    EXPECT_EQ(err, 0);
}

TEST_F(VTAAdapterTest, ProcessCueNullReturnsError) {
    int err = nimcp_vta_adapter_process_cue(nullptr, 1, 0.8f);
    EXPECT_EQ(err, -1);
}

TEST_F(VTAAdapterTest, ProcessGoalSucceeds) {
    int err = nimcp_vta_adapter_process_goal(adapter, 1, 1.0f, 0.3f, 0.5f);
    EXPECT_EQ(err, 0);
}

TEST_F(VTAAdapterTest, ProcessGoalNullReturnsError) {
    int err = nimcp_vta_adapter_process_goal(nullptr, 1, 1.0f, 0.3f, 0.5f);
    EXPECT_EQ(err, -1);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(VTAAdapterTest, ProcessImmuneSucceeds) {
    float cytokines[] = {0.5f, 0.3f, 0.2f};
    int err = nimcp_vta_adapter_process_immune(adapter, 0.5f, cytokines, 3);
    EXPECT_EQ(err, 0);
}

TEST_F(VTAAdapterTest, ProcessImmuneNullReturnsError) {
    int err = nimcp_vta_adapter_process_immune(nullptr, 0.5f, nullptr, 0);
    EXPECT_EQ(err, -1);
}

TEST_F(VTAAdapterTest, ProcessImmuneNullCytokinesOK) {
    int err = nimcp_vta_adapter_process_immune(adapter, 0.5f, nullptr, 0);
    EXPECT_EQ(err, 0);
}

TEST_F(VTAAdapterTest, HighInflammationReducesDA) {
    nimcp_vta_adapter_state_t before;
    nimcp_vta_adapter_get_state(adapter, &before);

    /* Apply high inflammation repeatedly */
    for (int i = 0; i < 50; i++) {
        nimcp_vta_adapter_process_immune(adapter, 0.9f, nullptr, 0);
        nimcp_vta_adapter_update(adapter, 10.0f);
    }

    nimcp_vta_adapter_state_t after;
    nimcp_vta_adapter_get_state(adapter, &after);

    /* High inflammation should reduce DA (sickness behavior) */
    EXPECT_LT(after.da_level, before.da_level);
}

TEST_F(VTAAdapterTest, ApplyPFCModulationSucceeds) {
    int err = nimcp_vta_adapter_apply_pfc_modulation(adapter, 0.5f);
    EXPECT_EQ(err, 0);
}

TEST_F(VTAAdapterTest, ApplyPFCModulationNullReturnsError) {
    int err = nimcp_vta_adapter_apply_pfc_modulation(nullptr, 0.5f);
    EXPECT_EQ(err, -1);
}

TEST_F(VTAAdapterTest, ApplyHabenulaInhibitionSucceeds) {
    int err = nimcp_vta_adapter_apply_habenula_inhibition(adapter, 0.5f);
    EXPECT_EQ(err, 0);
}

TEST_F(VTAAdapterTest, ApplyHabenulaInhibitionNullReturnsError) {
    int err = nimcp_vta_adapter_apply_habenula_inhibition(nullptr, 0.5f);
    EXPECT_EQ(err, -1);
}

TEST_F(VTAAdapterTest, StrongHabenulaInhibitionTriggersPause) {
    nimcp_vta_adapter_apply_habenula_inhibition(adapter, 0.8f);

    nimcp_vta_system_t* vta = nimcp_vta_adapter_get_vta(adapter);
    EXPECT_EQ(vta->mode, VTA_MODE_PHASIC_PAUSE);
}

//=============================================================================
// State Tests
//=============================================================================

TEST_F(VTAAdapterTest, GetStateSucceeds) {
    nimcp_vta_adapter_state_t state;
    int err = nimcp_vta_adapter_get_state(adapter, &state);
    EXPECT_EQ(err, 0);
    EXPECT_TRUE(state.is_active);
    EXPECT_GT(state.da_level, 0.0f);
}

TEST_F(VTAAdapterTest, GetStateNullReturnsError) {
    nimcp_vta_adapter_state_t state;
    EXPECT_EQ(nimcp_vta_adapter_get_state(nullptr, &state), -1);
    EXPECT_EQ(nimcp_vta_adapter_get_state(adapter, nullptr), -1);
}

TEST_F(VTAAdapterTest, StateTracksUpdates) {
    for (int i = 0; i < 100; i++) {
        nimcp_vta_adapter_update(adapter, 10.0f);
    }

    nimcp_vta_adapter_state_t state;
    nimcp_vta_adapter_get_state(adapter, &state);
    EXPECT_EQ(state.updates_processed, 100u);
}

TEST_F(VTAAdapterTest, ResetStatsSucceeds) {
    for (int i = 0; i < 50; i++) {
        nimcp_vta_adapter_update(adapter, 10.0f);
    }

    int err = nimcp_vta_adapter_reset_stats(adapter);
    EXPECT_EQ(err, 0);

    nimcp_vta_adapter_state_t state;
    nimcp_vta_adapter_get_state(adapter, &state);
    EXPECT_EQ(state.updates_processed, 0u);
    EXPECT_EQ(state.messages_sent, 0u);
}

TEST_F(VTAAdapterTest, ResetStatsNullReturnsError) {
    int err = nimcp_vta_adapter_reset_stats(nullptr);
    EXPECT_EQ(err, -1);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
