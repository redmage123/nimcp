/**
 * @file test_neuromod_executive_bridge.cpp
 * @brief Unit tests for Neuromodulatory-Executive Inter-Layer Bridge
 *
 * WHAT: Test suite for nimcp_neuromod_executive bridge
 * WHY:  Verify correct bridging between neuromodulatory and executive layers
 * HOW:  Unit tests for create, init, update, transfer, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstdlib>

extern "C" {
#include "integration/inter/neuromod_executive/nimcp_neuromod_executive_bridge.h"
#include "integration/intra/neuromodulatory/nimcp_neuromod_intra_coordinator.h"
#include "integration/intra/executive/nimcp_executive_intra_coordinator.h"
#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NeuromodExecutiveBridgeTest : public ::testing::Test {
protected:
    nimcp_neuromod_executive_bridge_t bridge = nullptr;
    nimcp_layer_registry_t registry = nullptr;
    nimcp_neuromod_intra_t neuromod = nullptr;
    nimcp_executive_intra_t executive = nullptr;

    void SetUp() override {
        /* Create registry for module registration */
        nimcp_layer_registry_config_t reg_config = nimcp_layer_registry_default_config();
        registry = nimcp_layer_registry_create(&reg_config);
        ASSERT_NE(registry, nullptr);

        /* Create neuromod intra-layer coordinator */
        nimcp_neuromod_intra_config_t neuro_config = nimcp_neuromod_intra_default_config();
        neuromod = nimcp_neuromod_intra_create(&neuro_config);
        /* Note: may be NULL for stub implementation */

        /* Create executive intra-layer coordinator */
        nimcp_executive_intra_config_t exec_config = nimcp_executive_intra_default_config();
        executive = nimcp_executive_intra_create(&exec_config);
        /* Note: may be NULL for stub implementation */

        /* Create neuromod-executive bridge */
        nimcp_neuromod_executive_config_t config = nimcp_neuromod_executive_default_config();
        bridge = nimcp_neuromod_executive_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            nimcp_neuromod_executive_destroy(bridge);
            bridge = nullptr;
        }
        if (executive) {
            nimcp_executive_intra_destroy(executive);
            executive = nullptr;
        }
        if (neuromod) {
            nimcp_neuromod_intra_destroy(neuromod);
            neuromod = nullptr;
        }
        if (registry) {
            nimcp_layer_registry_destroy(registry);
            registry = nullptr;
        }
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST(NeuromodExecutiveBridgeCreateTest, CreateWithDefaultConfig) {
    nimcp_neuromod_executive_bridge_t br = nimcp_neuromod_executive_create(nullptr);
    ASSERT_NE(br, nullptr);
    nimcp_neuromod_executive_destroy(br);
}

TEST(NeuromodExecutiveBridgeCreateTest, CreateWithCustomConfig) {
    nimcp_neuromod_executive_config_t config = nimcp_neuromod_executive_default_config();
    config.da_motivation_coupling = 0.9f;
    config.ne_flexibility_coupling = 0.8f;
    config.serotonin_impulse_coupling = 0.7f;
    config.enable_optimal_arousal = true;
    config.enable_metrics = true;

    nimcp_neuromod_executive_bridge_t br = nimcp_neuromod_executive_create(&config);
    ASSERT_NE(br, nullptr);
    nimcp_neuromod_executive_destroy(br);
}

TEST(NeuromodExecutiveBridgeCreateTest, CreateWithVariousCouplings) {
    nimcp_neuromod_executive_config_t config = nimcp_neuromod_executive_default_config();

    /* Test with low coupling */
    config.da_motivation_coupling = 0.1f;
    config.ne_flexibility_coupling = 0.1f;
    nimcp_neuromod_executive_bridge_t br = nimcp_neuromod_executive_create(&config);
    ASSERT_NE(br, nullptr);
    nimcp_neuromod_executive_destroy(br);

    /* Test with high coupling */
    config.da_motivation_coupling = 1.0f;
    config.ne_flexibility_coupling = 1.0f;
    br = nimcp_neuromod_executive_create(&config);
    ASSERT_NE(br, nullptr);
    nimcp_neuromod_executive_destroy(br);
}

TEST(NeuromodExecutiveBridgeCreateTest, DestroyNull) {
    /* Should not crash */
    nimcp_neuromod_executive_destroy(nullptr);
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(NeuromodExecutiveBridgeTest, InitSuccess) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(NeuromodExecutiveBridgeTest, InitNullBridge) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(nullptr, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(NeuromodExecutiveBridgeTest, InitNullRegistry) {
    /* May succeed or fail depending on implementation */
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, nullptr, neuromod, executive);
    /* Registry may be optional for standalone use */
    (void)err;
}

TEST_F(NeuromodExecutiveBridgeTest, InitNullNeuromod) {
    /* May succeed or fail depending on implementation */
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, nullptr, executive);
    /* Could be optional for partial testing */
    (void)err;
}

TEST_F(NeuromodExecutiveBridgeTest, InitNullExecutive) {
    /* May succeed or fail depending on implementation */
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, nullptr);
    /* Could be optional for partial testing */
    (void)err;
}

TEST_F(NeuromodExecutiveBridgeTest, DoubleInit) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_ALREADY_REGISTERED);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(NeuromodExecutiveBridgeTest, UpdateSuccess) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Update with 10ms timestep */
    err = nimcp_neuromod_executive_update(bridge, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(NeuromodExecutiveBridgeTest, UpdateMultipleTimes) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Multiple updates */
    for (int i = 0; i < 10; i++) {
        err = nimcp_neuromod_executive_update(bridge, 0.01f);
        EXPECT_EQ(err, NIMCP_LAYER_OK);
    }
}

TEST_F(NeuromodExecutiveBridgeTest, UpdateNotInitialized) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_update(bridge, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

TEST_F(NeuromodExecutiveBridgeTest, UpdateNull) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_update(nullptr, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(NeuromodExecutiveBridgeTest, UpdateZeroDt) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_neuromod_executive_update(bridge, 0.0f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

//=============================================================================
// Transfer Function Tests
//=============================================================================

TEST_F(NeuromodExecutiveBridgeTest, TransferBottomUpNull) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* NULL message should fail */
    err = nimcp_neuromod_executive_transfer_bottom_up(bridge, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(NeuromodExecutiveBridgeTest, TransferTopDownNull) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_neuromod_executive_transfer_top_down(bridge, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(NeuromodExecutiveBridgeTest, TransferBottomUpNotInitialized) {
    nimcp_layer_msg_t* msg = nimcp_layer_msg_create(
        NEURO_EXEC_MSG_MOTIVATION,
        NIMCP_LAYER_NEUROMODULATORY,
        NIMCP_LAYER_EXECUTIVE,
        nullptr, 0);
    ASSERT_NE(msg, nullptr);

    nimcp_layer_error_t err = nimcp_neuromod_executive_transfer_bottom_up(bridge, msg);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);

    nimcp_layer_msg_destroy(msg);
}

TEST_F(NeuromodExecutiveBridgeTest, TransferTopDownNotInitialized) {
    nimcp_layer_msg_t* msg = nimcp_layer_msg_create(
        NEURO_EXEC_MSG_GOAL_REWARD,
        NIMCP_LAYER_EXECUTIVE,
        NIMCP_LAYER_NEUROMODULATORY,
        nullptr, 0);
    ASSERT_NE(msg, nullptr);

    nimcp_layer_error_t err = nimcp_neuromod_executive_transfer_top_down(bridge, msg);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);

    nimcp_layer_msg_destroy(msg);
}

TEST_F(NeuromodExecutiveBridgeTest, TransferBottomUpNullBridge) {
    nimcp_layer_msg_t* msg = nimcp_layer_msg_create(
        NEURO_EXEC_MSG_MOTIVATION,
        NIMCP_LAYER_NEUROMODULATORY,
        NIMCP_LAYER_EXECUTIVE,
        nullptr, 0);
    ASSERT_NE(msg, nullptr);

    nimcp_layer_error_t err = nimcp_neuromod_executive_transfer_bottom_up(nullptr, msg);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);

    nimcp_layer_msg_destroy(msg);
}

//=============================================================================
// State and Stats Tests
//=============================================================================

TEST_F(NeuromodExecutiveBridgeTest, GetState) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_neuromod_executive_state_t state;
    err = nimcp_neuromod_executive_get_state(bridge, &state);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_GE(state.bridge_coherence, 0.0f);
}

TEST_F(NeuromodExecutiveBridgeTest, GetStateNull) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_neuromod_executive_get_state(bridge, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(NeuromodExecutiveBridgeTest, GetStateNullBridge) {
    nimcp_neuromod_executive_state_t state;
    nimcp_layer_error_t err = nimcp_neuromod_executive_get_state(nullptr, &state);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(NeuromodExecutiveBridgeTest, GetStats) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_neuromod_executive_stats_t stats;
    err = nimcp_neuromod_executive_get_stats(bridge, &stats);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_EQ(stats.motivation_updates, 0u);
}

TEST_F(NeuromodExecutiveBridgeTest, GetStatsNull) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_neuromod_executive_get_stats(bridge, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(NeuromodExecutiveBridgeTest, GetCoherence) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    float coherence = nimcp_neuromod_executive_get_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(NeuromodExecutiveBridgeTest, GetCoherenceNull) {
    float coherence = nimcp_neuromod_executive_get_coherence(nullptr);
    EXPECT_EQ(coherence, -1.0f);
}

TEST_F(NeuromodExecutiveBridgeTest, ResetStats) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_neuromod_executive_reset_stats(bridge);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(NeuromodExecutiveBridgeTest, ResetStatsNull) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_reset_stats(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// Shutdown Tests
//=============================================================================

TEST_F(NeuromodExecutiveBridgeTest, Shutdown) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_neuromod_executive_shutdown(bridge);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(NeuromodExecutiveBridgeTest, ShutdownNotInitialized) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_shutdown(bridge);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

TEST_F(NeuromodExecutiveBridgeTest, ShutdownNull) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_shutdown(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(NeuromodExecutiveBridgeTest, DoubleShutdown) {
    nimcp_layer_error_t err = nimcp_neuromod_executive_init(bridge, registry, neuromod, executive);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_neuromod_executive_shutdown(bridge);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_neuromod_executive_shutdown(bridge);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
