/**
 * @file test_self_model_fep_bridge.cpp
 * @brief Unit tests for Self-Model FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Self-Model bidirectional integration
 * WHY:  Ensure self-awareness, belief updates, and capability learning work correctly
 * HOW:  Test lifecycle, connections, belief updates, exploration, and bio-async integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/self_model/nimcp_self_model_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class SelfModelFepBridgeTest : public ::testing::Test {
protected:
    self_model_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        self_model_fep_config_t config;
        self_model_fep_bridge_default_config(&config);
        bridge = self_model_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            self_model_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(SelfModelFepBridgeTest, CreateDestroy) {
    // WHAT: Test basic bridge creation and destruction
    // WHY:  Verify lifecycle management works correctly
    // HOW:  Check non-null creation
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SelfModelFepBridgeTest, CreateWithNullConfig) {
    // WHAT: Test creation with null config uses defaults
    // WHY:  Ensure robustness to missing configuration
    // HOW:  Create with nullptr, verify non-null
    self_model_fep_bridge_t* br = self_model_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    self_model_fep_bridge_destroy(br);
}

TEST_F(SelfModelFepBridgeTest, DestroyNull) {
    // WHAT: Test destroying null pointer doesn't crash
    // WHY:  Defensive programming safety check
    // HOW:  Call destroy with nullptr
    self_model_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(SelfModelFepBridgeTest, DefaultConfig) {
    // WHAT: Test default configuration values
    // WHY:  Ensure sensible defaults for biologically-plausible behavior
    // HOW:  Verify all config fields are properly initialized
    self_model_fep_config_t config;
    int ret = self_model_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.capability_pe_threshold, 0.0f);
    EXPECT_GT(config.belief_pe_threshold, 0.0f);
    EXPECT_GT(config.belief_update_rate, 0.0f);
    EXPECT_GT(config.capability_update_rate, 0.0f);
    EXPECT_TRUE(config.enable_belief_updates);
    EXPECT_TRUE(config.enable_capability_learning);
}

TEST_F(SelfModelFepBridgeTest, DefaultConfigNullPtr) {
    // WHAT: Test default config with null pointer
    // WHY:  Verify null safety
    // HOW:  Pass nullptr, expect error
    int ret = self_model_fep_bridge_default_config(nullptr);
    EXPECT_NE(ret, 0);  /* Returns error code for NULL */
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(SelfModelFepBridgeTest, ConnectFep) {
    // WHAT: Test connecting FEP system to bridge
    // WHY:  Bridge needs FEP for prediction error monitoring
    // HOW:  Create FEP system, connect, verify success
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = self_model_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(SelfModelFepBridgeTest, ConnectFepNull) {
    // WHAT: Test null pointer safety for FEP connection
    // WHY:  Prevent crashes from invalid inputs
    // HOW:  Try all null combinations
    EXPECT_NE(self_model_fep_bridge_connect_fep(nullptr, nullptr), 0);

    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);

    EXPECT_NE(self_model_fep_bridge_connect_fep(nullptr, fep), 0);
    EXPECT_NE(self_model_fep_bridge_connect_fep(bridge, nullptr), 0);

    fep_destroy(fep);
}

TEST_F(SelfModelFepBridgeTest, ConnectSelfModel) {
    // WHAT: Test connecting self-model system to bridge
    // WHY:  Bridge needs self-model for belief/capability updates
    // HOW:  Create mock self-model, connect, verify success
    self_model_t* self_model = (self_model_t*)1;  // Mock pointer

    int ret = self_model_fep_bridge_connect_self_model(bridge, self_model);
    EXPECT_EQ(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, ConnectSelfModelNull) {
    // WHAT: Test null pointer safety for self-model connection
    // WHY:  Prevent crashes from invalid inputs
    // HOW:  Try null combinations
    EXPECT_NE(self_model_fep_bridge_connect_self_model(nullptr, nullptr), 0);
    EXPECT_NE(self_model_fep_bridge_connect_self_model(bridge, nullptr), 0);
}

/* ============================================================================
 * FEP → Self-Model Direction Tests
 * ============================================================================ */

TEST_F(SelfModelFepBridgeTest, UpdateBelief) {
    // WHAT: Test belief update from prediction error
    // WHY:  PE indicates belief inaccuracy, should revise belief
    // HOW:  Trigger belief update with PE value
    int ret = self_model_fep_update_belief(bridge, 0, 5.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, UpdateBeliefNull) {
    // WHAT: Test null safety for belief update
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = self_model_fep_update_belief(nullptr, 0, 5.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, UpdateCapability) {
    // WHAT: Test capability update from performance PE
    // WHY:  Learn true capabilities via experience
    // HOW:  Trigger capability update with PE
    int ret = self_model_fep_update_capability(bridge, 0, 3.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, UpdateCapabilityNull) {
    // WHAT: Test null safety for capability update
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = self_model_fep_update_capability(nullptr, 0, 3.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, ExploreSelf) {
    // WHAT: Test self-exploration based on uncertainty
    // WHY:  High uncertainty → epistemic foraging
    // HOW:  Trigger exploration
    int ret = self_model_fep_explore_self(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, ExploreSelfNull) {
    // WHAT: Test null safety for self-exploration
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = self_model_fep_explore_self(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Self-Model → FEP Direction Tests
 * ============================================================================ */

TEST_F(SelfModelFepBridgeTest, ApplyBeliefPriors) {
    // WHAT: Test applying self-beliefs as FEP priors
    // WHY:  Self-beliefs constrain inference
    // HOW:  Apply priors, verify success
    /* Must connect FEP system before applying priors */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    self_model_fep_bridge_connect_fep(bridge, fep);
    int ret = self_model_fep_apply_belief_priors(bridge);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(SelfModelFepBridgeTest, ApplyBeliefPriorsNull) {
    // WHAT: Test null safety for belief priors
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = self_model_fep_apply_belief_priors(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, ConstrainPolicies) {
    // WHAT: Test policy constraint by capabilities
    // WHY:  Don't plan beyond capabilities
    // HOW:  Constrain policies, verify success
    /* Must connect FEP system before constraining policies */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    self_model_fep_bridge_connect_fep(bridge, fep);
    int ret = self_model_fep_constrain_policies(bridge);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(SelfModelFepBridgeTest, ConstrainPoliciesNull) {
    // WHAT: Test null safety for policy constraint
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = self_model_fep_constrain_policies(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, ApplySensoryAttenuation) {
    // WHAT: Test sensory attenuation for self-generated actions
    // WHY:  Self-originated signals are predictable
    // HOW:  Apply attenuation for self-generated and external
    /* Must connect FEP system before sensory attenuation */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    self_model_fep_bridge_connect_fep(bridge, fep);
    int ret1 = self_model_fep_apply_sensory_attenuation(bridge, true);
    EXPECT_EQ(ret1, 0);

    int ret2 = self_model_fep_apply_sensory_attenuation(bridge, false);
    EXPECT_EQ(ret2, 0);

    fep_destroy(fep);
}

TEST_F(SelfModelFepBridgeTest, ApplySensoryAttenuationNull) {
    // WHAT: Test null safety for sensory attenuation
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = self_model_fep_apply_sensory_attenuation(nullptr, true);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Update & State Tests
 * ============================================================================ */

TEST_F(SelfModelFepBridgeTest, Update) {
    // WHAT: Test main update loop
    // WHY:  Bridge must synchronize FEP and self-model
    // HOW:  Call update with time delta
    int ret = self_model_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, UpdateNull) {
    // WHAT: Test null safety for update
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = self_model_fep_bridge_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, GetState) {
    // WHAT: Test retrieving bridge state
    // WHY:  Monitor self-knowledge and exploration state
    // HOW:  Get state, verify success
    self_model_fep_state_t state;
    int ret = self_model_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, GetStateNull) {
    // WHAT: Test null safety for get state
    // WHY:  Prevent crashes
    // HOW:  Try null combinations
    self_model_fep_state_t state;
    EXPECT_NE(self_model_fep_bridge_get_state(nullptr, &state), 0);
    EXPECT_NE(self_model_fep_bridge_get_state(bridge, nullptr), 0);
}

TEST_F(SelfModelFepBridgeTest, GetStats) {
    // WHAT: Test retrieving bridge statistics
    // WHY:  Monitor performance metrics
    // HOW:  Get stats, verify success
    self_model_fep_stats_t stats;
    int ret = self_model_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, GetStatsNull) {
    // WHAT: Test null safety for get stats
    // WHY:  Prevent crashes
    // HOW:  Try null combinations
    self_model_fep_stats_t stats;
    EXPECT_NE(self_model_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(self_model_fep_bridge_get_stats(bridge, nullptr), 0);
}

TEST_F(SelfModelFepBridgeTest, IsExploring) {
    // WHAT: Test checking exploration status
    // WHY:  Query if self-exploration is active
    // HOW:  Call is_exploring
    bool exploring = self_model_fep_is_exploring(bridge);
    // Just verify it doesn't crash, value depends on state
    (void)exploring;
}

TEST_F(SelfModelFepBridgeTest, IsExploringNull) {
    // WHAT: Test null safety for is_exploring
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr, expect false
    bool exploring = self_model_fep_is_exploring(nullptr);
    EXPECT_FALSE(exploring);
}

TEST_F(SelfModelFepBridgeTest, GetSelfCertainty) {
    // WHAT: Test getting self-knowledge certainty
    // WHY:  Monitor "how well do I know myself"
    // HOW:  Call get_self_certainty
    float certainty = self_model_fep_get_self_certainty(bridge);
    EXPECT_GE(certainty, 0.0f);
    EXPECT_LE(certainty, 1.0f);
}

TEST_F(SelfModelFepBridgeTest, GetSelfCertaintyNull) {
    // WHAT: Test null safety for get_self_certainty
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr, expect 0.0
    float certainty = self_model_fep_get_self_certainty(nullptr);
    EXPECT_EQ(certainty, 0.0f);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(SelfModelFepBridgeTest, ConnectBioAsync) {
    // WHAT: Test bio-async connection
    // WHY:  Enable distributed self-awareness signaling
    // HOW:  Connect to bio-async router
    int ret = self_model_fep_bridge_connect_bio_async(bridge);
    // May succeed or fail depending on router availability
    (void)ret;
}

TEST_F(SelfModelFepBridgeTest, ConnectBioAsyncNull) {
    // WHAT: Test null safety for bio-async connect
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = self_model_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, DisconnectBioAsync) {
    // WHAT: Test bio-async disconnection
    // WHY:  Clean shutdown
    // HOW:  Disconnect from router
    self_model_fep_bridge_connect_bio_async(bridge);
    int ret = self_model_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, DisconnectBioAsyncNull) {
    // WHAT: Test null safety for bio-async disconnect
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = self_model_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SelfModelFepBridgeTest, IsBioAsyncConnected) {
    // WHAT: Test checking bio-async connection status
    // WHY:  Query router availability
    // HOW:  Check connection state
    bool connected = self_model_fep_bridge_is_bio_async_connected(bridge);
    // Just verify it doesn't crash
    (void)connected;
}

TEST_F(SelfModelFepBridgeTest, IsBioAsyncConnectedNull) {
    // WHAT: Test null safety for is_bio_async_connected
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr, expect false
    bool connected = self_model_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}
