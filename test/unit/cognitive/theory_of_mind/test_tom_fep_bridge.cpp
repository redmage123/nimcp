/**
 * @file test_tom_fep_bridge.cpp
 * @brief Unit tests for Theory of Mind FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-ToM bidirectional integration
 * WHY:  Ensure belief inference, intention inference, and empathy work correctly
 * HOW:  Test lifecycle, connections, social inference, empathy, and bio-async integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/theory_of_mind/nimcp_tom_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class TomFepBridgeTest : public ::testing::Test {
protected:
    tom_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        tom_fep_config_t config;
        tom_fep_bridge_default_config(&config);
        bridge = tom_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            tom_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(TomFepBridgeTest, CreateDestroy) {
    // WHAT: Test basic bridge creation and destruction
    // WHY:  Verify lifecycle management works correctly
    // HOW:  Check non-null creation
    ASSERT_NE(bridge, nullptr);
}

TEST_F(TomFepBridgeTest, CreateWithNullConfig) {
    // WHAT: Test creation with null config uses defaults
    // WHY:  Ensure robustness to missing configuration
    // HOW:  Create with nullptr, verify non-null
    tom_fep_bridge_t* br = tom_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    tom_fep_bridge_destroy(br);
}

TEST_F(TomFepBridgeTest, DestroyNull) {
    // WHAT: Test destroying null pointer doesn't crash
    // WHY:  Defensive programming safety check
    // HOW:  Call destroy with nullptr
    tom_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(TomFepBridgeTest, DefaultConfig) {
    // WHAT: Test default configuration values
    // WHY:  Ensure sensible defaults for biologically-plausible behavior
    // HOW:  Verify all config fields are properly initialized
    tom_fep_config_t config;
    int ret = tom_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.action_pe_threshold, 0.0f);
    EXPECT_GT(config.belief_pe_threshold, 0.0f);
    EXPECT_GT(config.empathy_threshold, 0.0f);
    EXPECT_GT(config.max_recursion_depth, 0);
    EXPECT_TRUE(config.enable_belief_inference);
    EXPECT_TRUE(config.enable_intention_inference);
    EXPECT_TRUE(config.enable_empathy);
}

TEST_F(TomFepBridgeTest, DefaultConfigNullPtr) {
    // WHAT: Test default config with null pointer
    // WHY:  Verify null safety
    // HOW:  Pass nullptr, expect error
    int ret = tom_fep_bridge_default_config(nullptr);
    EXPECT_NE(ret, 0);  /* Returns error code for NULL */
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(TomFepBridgeTest, ConnectFep) {
    // WHAT: Test connecting FEP system to bridge
    // WHY:  Bridge needs FEP for social prediction errors
    // HOW:  Create FEP system, connect, verify success
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = tom_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(TomFepBridgeTest, ConnectFepNull) {
    // WHAT: Test null pointer safety for FEP connection
    // WHY:  Prevent crashes from invalid inputs
    // HOW:  Try all null combinations
    EXPECT_NE(tom_fep_bridge_connect_fep(nullptr, nullptr), 0);

    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);

    EXPECT_NE(tom_fep_bridge_connect_fep(nullptr, fep), 0);
    EXPECT_NE(tom_fep_bridge_connect_fep(bridge, nullptr), 0);

    fep_destroy(fep);
}

TEST_F(TomFepBridgeTest, ConnectTom) {
    // WHAT: Test connecting ToM system to bridge
    // WHY:  Bridge needs ToM for social cognition
    // HOW:  Test that NULL handle is rejected
    theory_of_mind_t tom = 0;  // NULL handle

    /* Passing NULL/0 ToM handle should fail */
    int ret = tom_fep_bridge_connect_tom(bridge, tom);
    EXPECT_NE(ret, 0);  /* NULL ToM handle should be rejected */
}

TEST_F(TomFepBridgeTest, ConnectTomNull) {
    // WHAT: Test null pointer safety for ToM connection
    // WHY:  Prevent crashes from invalid inputs
    // HOW:  Pass nullptr for bridge
    theory_of_mind_t tom = 0;
    EXPECT_NE(tom_fep_bridge_connect_tom(nullptr, tom), 0);
}

/* ============================================================================
 * FEP → ToM Direction Tests
 * ============================================================================ */

TEST_F(TomFepBridgeTest, InferBelief) {
    // WHAT: Test belief inference from prediction error
    // WHY:  High PE indicates false belief
    // HOW:  Trigger belief inference with PE value
    int ret = tom_fep_infer_belief(bridge, 6.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(TomFepBridgeTest, InferBeliefNull) {
    // WHAT: Test null safety for belief inference
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = tom_fep_infer_belief(nullptr, 6.0f);
    EXPECT_NE(ret, 0);
}

TEST_F(TomFepBridgeTest, InferIntention) {
    // WHAT: Test intention inference from action PE
    // WHY:  Unexpected action reveals goal
    // HOW:  Trigger intention inference
    int ret = tom_fep_infer_intention(bridge, 4.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(TomFepBridgeTest, InferIntentionNull) {
    // WHAT: Test null safety for intention inference
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = tom_fep_infer_intention(nullptr, 4.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(TomFepBridgeTest, ActivateEmpathy) {
    // WHAT: Test empathy activation via shared model
    // WHY:  Empathy simulates others' emotions
    // HOW:  Activate empathy with emotion
    tom_emotion_t emotion = TOM_EMOTION_JOY;
    int ret = tom_fep_activate_empathy(bridge, emotion);
    EXPECT_EQ(ret, 0);
}

TEST_F(TomFepBridgeTest, ActivateEmpathyNull) {
    // WHAT: Test null safety for empathy activation
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    tom_emotion_t emotion = TOM_EMOTION_JOY;
    int ret = tom_fep_activate_empathy(nullptr, emotion);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * ToM → FEP Direction Tests
 * ============================================================================ */

TEST_F(TomFepBridgeTest, ApplySocialPriors) {
    // WHAT: Test applying social priors from trait beliefs
    // WHY:  Known traits constrain inference
    // HOW:  Apply priors, verify success
    /* Must connect FEP system before applying priors */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    tom_fep_bridge_connect_fep(bridge, fep);
    int ret = tom_fep_apply_social_priors(bridge);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(TomFepBridgeTest, ApplySocialPriorsNull) {
    // WHAT: Test null safety for social priors
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = tom_fep_apply_social_priors(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(TomFepBridgeTest, ModulateEmpathicPrecision) {
    // WHAT: Test precision modulation from inferred emotion
    // WHY:  Emotional states heighten attention
    // HOW:  Modulate precision with emotion
    /* Must connect FEP system before precision modulation */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    tom_fep_bridge_connect_fep(bridge, fep);
    tom_emotion_t emotion = TOM_EMOTION_ANXIETY;  // Closest to distress
    int ret = tom_fep_modulate_empathic_precision(bridge, emotion);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(TomFepBridgeTest, ModulateEmpathicPrecisionNull) {
    // WHAT: Test null safety for precision modulation
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    tom_emotion_t emotion = TOM_EMOTION_ANXIETY;  // Closest to distress
    int ret = tom_fep_modulate_empathic_precision(nullptr, emotion);
    EXPECT_NE(ret, 0);
}

TEST_F(TomFepBridgeTest, AddMentalizingOverhead) {
    // WHAT: Test adding mentalizing overhead to free energy
    // WHY:  Nested models are cognitively costly
    // HOW:  Add overhead for different recursion depths
    int ret1 = tom_fep_add_mentalizing_overhead(bridge, 1);
    EXPECT_EQ(ret1, 0);

    int ret2 = tom_fep_add_mentalizing_overhead(bridge, 3);
    EXPECT_EQ(ret2, 0);
}

TEST_F(TomFepBridgeTest, AddMentalizingOverheadNull) {
    // WHAT: Test null safety for mentalizing overhead
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = tom_fep_add_mentalizing_overhead(nullptr, 2);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Update & State Tests
 * ============================================================================ */

TEST_F(TomFepBridgeTest, Update) {
    // WHAT: Test main update loop
    // WHY:  Bridge must synchronize FEP and ToM
    // HOW:  Call update with time delta
    int ret = tom_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(TomFepBridgeTest, UpdateNull) {
    // WHAT: Test null safety for update
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = tom_fep_bridge_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(TomFepBridgeTest, GetState) {
    // WHAT: Test retrieving bridge state
    // WHY:  Monitor social inference and empathy
    // HOW:  Get state, verify success
    tom_fep_state_t state;
    int ret = tom_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(TomFepBridgeTest, GetStateNull) {
    // WHAT: Test null safety for get state
    // WHY:  Prevent crashes
    // HOW:  Try null combinations
    tom_fep_state_t state;
    EXPECT_NE(tom_fep_bridge_get_state(nullptr, &state), 0);
    EXPECT_NE(tom_fep_bridge_get_state(bridge, nullptr), 0);
}

TEST_F(TomFepBridgeTest, GetStats) {
    // WHAT: Test retrieving bridge statistics
    // WHY:  Monitor performance metrics
    // HOW:  Get stats, verify success
    tom_fep_stats_t stats;
    int ret = tom_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(TomFepBridgeTest, GetStatsNull) {
    // WHAT: Test null safety for get stats
    // WHY:  Prevent crashes
    // HOW:  Try null combinations
    tom_fep_stats_t stats;
    EXPECT_NE(tom_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(tom_fep_bridge_get_stats(bridge, nullptr), 0);
}

TEST_F(TomFepBridgeTest, IsEmpathyActive) {
    // WHAT: Test checking empathy status
    // WHY:  Query if empathy is currently active
    // HOW:  Call is_empathy_active
    bool active = tom_fep_is_empathy_active(bridge);
    // Just verify it doesn't crash
    (void)active;
}

TEST_F(TomFepBridgeTest, IsEmpathyActiveNull) {
    // WHAT: Test null safety for is_empathy_active
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr, expect false
    bool active = tom_fep_is_empathy_active(nullptr);
    EXPECT_FALSE(active);
}

TEST_F(TomFepBridgeTest, GetMentalizingDepth) {
    // WHAT: Test getting current mentalizing depth
    // WHY:  Monitor recursion level
    // HOW:  Call get_mentalizing_depth
    uint32_t depth = tom_fep_get_mentalizing_depth(bridge);
    EXPECT_LE(depth, TOM_FEP_MAX_RECURSION_DEPTH);
}

TEST_F(TomFepBridgeTest, GetMentalizingDepthNull) {
    // WHAT: Test null safety for get_mentalizing_depth
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr, expect 0
    uint32_t depth = tom_fep_get_mentalizing_depth(nullptr);
    EXPECT_EQ(depth, 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(TomFepBridgeTest, ConnectBioAsync) {
    // WHAT: Test bio-async connection
    // WHY:  Enable distributed social cognition signaling
    // HOW:  Connect to bio-async router
    int ret = tom_fep_bridge_connect_bio_async(bridge);
    // May succeed or fail depending on router availability
    (void)ret;
}

TEST_F(TomFepBridgeTest, ConnectBioAsyncNull) {
    // WHAT: Test null safety for bio-async connect
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = tom_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(TomFepBridgeTest, DisconnectBioAsync) {
    // WHAT: Test bio-async disconnection
    // WHY:  Clean shutdown
    // HOW:  Disconnect from router
    tom_fep_bridge_connect_bio_async(bridge);
    int ret = tom_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(TomFepBridgeTest, DisconnectBioAsyncNull) {
    // WHAT: Test null safety for bio-async disconnect
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr
    int ret = tom_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(TomFepBridgeTest, IsBioAsyncConnected) {
    // WHAT: Test checking bio-async connection status
    // WHY:  Query router availability
    // HOW:  Check connection state
    bool connected = tom_fep_bridge_is_bio_async_connected(bridge);
    // Just verify it doesn't crash
    (void)connected;
}

TEST_F(TomFepBridgeTest, IsBioAsyncConnectedNull) {
    // WHAT: Test null safety for is_bio_async_connected
    // WHY:  Prevent crashes
    // HOW:  Pass nullptr, expect false
    bool connected = tom_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}
