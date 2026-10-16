/**
 * @file test_lc_adapter.cpp
 * @brief Unit tests for Locus Coeruleus Adapter
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/locus_coeruleus/nimcp_lc_adapter.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class LCAdapterTest : public ::testing::Test {
protected:
    nimcp_lc_adapter_t adapter;

    void SetUp() override {
        adapter = nimcp_lc_adapter_create(nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            nimcp_lc_adapter_destroy(adapter);
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(LCAdapterTest, CreateSucceeds) {
    EXPECT_NE(adapter, nullptr);
}

TEST_F(LCAdapterTest, CreateWithConfigSucceeds) {
    nimcp_lc_adapter_destroy(adapter);

    nimcp_lc_adapter_config_t config = nimcp_lc_adapter_default_config();
    config.enable_bio_async = false;

    adapter = nimcp_lc_adapter_create(&config);
    EXPECT_NE(adapter, nullptr);
}

TEST_F(LCAdapterTest, DestroyNullSafe) {
    nimcp_lc_adapter_destroy(nullptr);
    /* Should not crash */
}

TEST_F(LCAdapterTest, DefaultConfigValid) {
    nimcp_lc_adapter_config_t config = nimcp_lc_adapter_default_config();
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.auto_create_projections);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(LCAdapterTest, ConnectBrainNullSucceeds) {
    /* NULL brain is valid for disconnected state */
    int err = nimcp_lc_adapter_connect_brain(adapter, nullptr);
    EXPECT_EQ(err, 0);
}

TEST_F(LCAdapterTest, DisconnectSucceeds) {
    int err = nimcp_lc_adapter_disconnect(adapter);
    EXPECT_EQ(err, 0);
}

TEST_F(LCAdapterTest, DisconnectNullAdapterReturnsError) {
    int err = nimcp_lc_adapter_disconnect(nullptr);
    EXPECT_EQ(err, -1);
}

//=============================================================================
// LC Access Tests
//=============================================================================

TEST_F(LCAdapterTest, GetLCReturnsValid) {
    nimcp_lc_system_t* lc = nimcp_lc_adapter_get_lc(adapter);
    ASSERT_NE(lc, nullptr);
    EXPECT_TRUE(lc->initialized);
}

TEST_F(LCAdapterTest, GetLCNullAdapterReturnsNull) {
    nimcp_lc_system_t* lc = nimcp_lc_adapter_get_lc(nullptr);
    EXPECT_EQ(lc, nullptr);
}

TEST_F(LCAdapterTest, AutoCreatedProjectionsExist) {
    nimcp_lc_system_t* lc = nimcp_lc_adapter_get_lc(adapter);
    EXPECT_GT(lc->num_projections, 0u);
}

//=============================================================================
// Message Tests
//=============================================================================

TEST_F(LCAdapterTest, SendMessageSucceeds) {
    nimcp_lc_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = LC_MSG_AROUSAL_UPDATE;
    msg.data.arousal.arousal_level = 0.7f;

    int err = nimcp_lc_adapter_send_message(adapter, &msg);
    EXPECT_EQ(err, 0);
}

TEST_F(LCAdapterTest, SendMessageNullReturnsError) {
    int err = nimcp_lc_adapter_send_message(adapter, nullptr);
    EXPECT_EQ(err, -1);
}

TEST_F(LCAdapterTest, ProcessMessagesSucceeds) {
    nimcp_lc_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = LC_MSG_STRESS_RESPONSE;
    msg.data.stress.stress_level = 0.5f;

    nimcp_lc_adapter_send_message(adapter, &msg);

    int processed = nimcp_lc_adapter_process_messages(adapter, 10);
    EXPECT_GE(processed, 0);
}

TEST_F(LCAdapterTest, ProcessMessagesNullReturnsError) {
    int err = nimcp_lc_adapter_process_messages(nullptr, 10);
    EXPECT_EQ(err, -1);
}

//=============================================================================
// Callback Tests
//=============================================================================

static bool callback_invoked = false;
static void test_callback(nimcp_lc_adapter_t /*adapter*/,
                          const nimcp_lc_message_t* /*msg*/,
                          void* /*user_data*/) {
    callback_invoked = true;
}

TEST_F(LCAdapterTest, RegisterCallbackSucceeds) {
    int err = nimcp_lc_adapter_register_callback(adapter, LC_MSG_NOVELTY_DETECTED,
                                                  test_callback, nullptr);
    EXPECT_EQ(err, 0);
}

TEST_F(LCAdapterTest, RegisterCallbackNullReturnsError) {
    int err = nimcp_lc_adapter_register_callback(adapter, LC_MSG_NOVELTY_DETECTED,
                                                  nullptr, nullptr);
    EXPECT_EQ(err, -1);
}

TEST_F(LCAdapterTest, CallbackInvokedOnMessage) {
    callback_invoked = false;

    nimcp_lc_adapter_register_callback(adapter, LC_MSG_MODE_CHANGED,
                                        test_callback, nullptr);

    nimcp_lc_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = LC_MSG_MODE_CHANGED;

    nimcp_lc_adapter_send_message(adapter, &msg);
    nimcp_lc_adapter_process_messages(adapter, 10);

    EXPECT_TRUE(callback_invoked);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(LCAdapterTest, UpdateSucceeds) {
    int err = nimcp_lc_adapter_update(adapter, 10.0f);
    EXPECT_EQ(err, 0);
}

TEST_F(LCAdapterTest, UpdateNullReturnsError) {
    int err = nimcp_lc_adapter_update(nullptr, 10.0f);
    EXPECT_EQ(err, -1);
}

TEST_F(LCAdapterTest, MultipleUpdatesStable) {
    for (int i = 0; i < 1000; i++) {
        int err = nimcp_lc_adapter_update(adapter, 1.0f);
        EXPECT_EQ(err, 0);
    }

    nimcp_lc_system_t* lc = nimcp_lc_adapter_get_lc(adapter);
    EXPECT_FALSE(std::isnan(lc->ne_concentration));
}

//=============================================================================
// Input Processing Tests
//=============================================================================

TEST_F(LCAdapterTest, ProcessInputSucceeds) {
    float input[] = {0.5f, 0.6f, 0.7f, 0.8f};
    int err = nimcp_lc_adapter_process_input(adapter, input, 4);
    EXPECT_EQ(err, 0);
}

TEST_F(LCAdapterTest, ProcessInputNullAdapterReturnsError) {
    float input[] = {0.5f, 0.6f};
    int err = nimcp_lc_adapter_process_input(nullptr, input, 2);
    EXPECT_EQ(err, -1);
}

TEST_F(LCAdapterTest, HighNoveltyInputGeneratesMessage) {
    /* Very different input should trigger novelty */
    float input[] = {10.0f, 10.0f, 10.0f, 10.0f};
    nimcp_lc_adapter_process_input(adapter, input, 4);

    /* Update to process any generated messages */
    nimcp_lc_adapter_update(adapter, 10.0f);

    /* Check state shows some activity */
    nimcp_lc_adapter_state_t state;
    nimcp_lc_adapter_get_state(adapter, &state);
    EXPECT_GT(state.messages_sent, 0u);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(LCAdapterTest, ConnectImmuneNullSucceeds) {
    int err = nimcp_lc_adapter_connect_immune(adapter, nullptr);
    EXPECT_EQ(err, 0);
}

TEST_F(LCAdapterTest, ProcessImmuneSucceeds) {
    float cytokines[] = {0.5f, 0.6f};
    int err = nimcp_lc_adapter_process_immune(adapter, 0.5f, cytokines, 2);
    EXPECT_EQ(err, 0);
}

TEST_F(LCAdapterTest, HighInflammationIncreasesActivity) {
    nimcp_lc_system_t* lc = nimcp_lc_adapter_get_lc(adapter);
    float initial_excitation = lc->neurons.excitatory_input;

    nimcp_lc_adapter_process_immune(adapter, 0.8f, nullptr, 0);

    EXPECT_GT(lc->neurons.excitatory_input, initial_excitation);
}

TEST_F(LCAdapterTest, ThalamicGateApplied) {
    int err = nimcp_lc_adapter_apply_thalamic_gate(adapter, 0.5f);
    EXPECT_EQ(err, 0);
}

TEST_F(LCAdapterTest, ThalamicGateClamped) {
    nimcp_lc_adapter_apply_thalamic_gate(adapter, 1.5f);
    /* Should be clamped internally */

    nimcp_lc_adapter_apply_thalamic_gate(adapter, -0.5f);
    /* Should be clamped to 0 */
}

//=============================================================================
// State Tests
//=============================================================================

TEST_F(LCAdapterTest, GetStateSucceeds) {
    nimcp_lc_adapter_state_t state;
    int err = nimcp_lc_adapter_get_state(adapter, &state);

    EXPECT_EQ(err, 0);
    EXPECT_TRUE(state.is_active);
}

TEST_F(LCAdapterTest, GetStateNullReturnsError) {
    int err = nimcp_lc_adapter_get_state(adapter, nullptr);
    EXPECT_EQ(err, -1);
}

TEST_F(LCAdapterTest, StateTracksUpdates) {
    for (int i = 0; i < 50; i++) {
        nimcp_lc_adapter_update(adapter, 10.0f);
    }

    nimcp_lc_adapter_state_t state;
    nimcp_lc_adapter_get_state(adapter, &state);

    EXPECT_EQ(state.updates_processed, 50u);
    EXPECT_GT(state.total_active_time, 0.0f);
}

TEST_F(LCAdapterTest, ResetStatsWorks) {
    for (int i = 0; i < 10; i++) {
        nimcp_lc_adapter_update(adapter, 10.0f);
    }

    int err = nimcp_lc_adapter_reset_stats(adapter);
    EXPECT_EQ(err, 0);

    nimcp_lc_adapter_state_t state;
    nimcp_lc_adapter_get_state(adapter, &state);
    EXPECT_EQ(state.updates_processed, 0u);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(LCAdapterTest, SetRouterNullSucceeds) {
    int err = nimcp_lc_adapter_set_router(adapter, nullptr);
    EXPECT_EQ(err, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
