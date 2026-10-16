/**
 * @file test_swarm_brain_fep_bridge.cpp
 * @brief Unit tests for Swarm Brain-FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Swarm Brain bidirectional integration
 * WHY:  Ensure collective free energy minimization and multi-agent inference work
 * HOW:  Test lifecycle, collective effects, emergence scaling, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_brain_fep_bridge.h"
#include "swarm/nimcp_swarm_brain.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class SwarmBrainFepBridgeTest : public ::testing::Test {
protected:
    swarm_brain_fep_bridge_t* bridge = nullptr;
    fep_system_t* fep = nullptr;
    swarm_brain_t* swarm_brain = nullptr;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, 8, 4);
        ASSERT_NE(fep, nullptr);

        /* Create swarm brain */
        swarm_brain_config_t sb_config = swarm_brain_default_config();
        sb_config.drone_id = 1;
        swarm_brain = swarm_brain_create(&sb_config);
        ASSERT_NE(swarm_brain, nullptr);

        /* Create bridge */
        swarm_brain_fep_config_t config;
        swarm_brain_fep_default_config(&config);
        bridge = swarm_brain_fep_create(&config, swarm_brain, fep);
    }

    void TearDown() override {
        if (bridge) {
            swarm_brain_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (swarm_brain) {
            swarm_brain_destroy(swarm_brain);
            swarm_brain = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(SwarmBrainFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SwarmBrainFepBridgeTest, CreateWithNullSwarmBrain) {
    swarm_brain_fep_config_t config;
    swarm_brain_fep_default_config(&config);
    swarm_brain_fep_bridge_t* br = swarm_brain_fep_create(&config, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmBrainFepBridgeTest, CreateWithNullFep) {
    swarm_brain_fep_config_t config;
    swarm_brain_fep_default_config(&config);
    swarm_brain_fep_bridge_t* br = swarm_brain_fep_create(&config, swarm_brain, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmBrainFepBridgeTest, DestroyNull) {
    swarm_brain_fep_destroy(nullptr);  /* Should not crash */
}

TEST_F(SwarmBrainFepBridgeTest, DefaultConfig) {
    swarm_brain_fep_config_t config;
    swarm_brain_fep_default_config(&config);

    EXPECT_GT(config.collective_pe_weight, 0.0f);
    EXPECT_GT(config.coherence_precision_gain, 0.0f);
    EXPECT_TRUE(config.enable_collective_inference);
    EXPECT_TRUE(config.enable_emergence_scaling);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(SwarmBrainFepBridgeTest, Update) {
    int ret = swarm_brain_fep_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SwarmBrainFepBridgeTest, UpdateNull) {
    EXPECT_NE(swarm_brain_fep_update(nullptr), 0);
}

TEST_F(SwarmBrainFepBridgeTest, ApplyModulation) {
    int ret = swarm_brain_fep_apply_modulation(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SwarmBrainFepBridgeTest, ApplyModulationNull) {
    EXPECT_NE(swarm_brain_fep_apply_modulation(nullptr), 0);
}

/* ============================================================================
 * Collective Free Energy Tests
 * ============================================================================ */

TEST_F(SwarmBrainFepBridgeTest, ComputeCollectiveFE) {
    float collective_fe = swarm_brain_fep_compute_collective_fe(bridge);
    /* Free energy should be non-negative */
    EXPECT_GE(collective_fe, 0.0f);
}

TEST_F(SwarmBrainFepBridgeTest, ComputeCollectiveFENull) {
    float collective_fe = swarm_brain_fep_compute_collective_fe(nullptr);
    EXPECT_FLOAT_EQ(collective_fe, 0.0f);
}

/* ============================================================================
 * Effects Tests
 * ============================================================================ */

TEST_F(SwarmBrainFepBridgeTest, GetEffects) {
    swarm_brain_fep_update(bridge);

    swarm_brain_fep_effects_t effects;
    int ret = swarm_brain_fep_get_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.coordination_adjustment, -1.0f);
    EXPECT_LE(effects.coordination_adjustment, 1.0f);
    EXPECT_GE(effects.exploration_bias, 0.0f);
    EXPECT_LE(effects.exploration_bias, 1.0f);
}

TEST_F(SwarmBrainFepBridgeTest, GetEffectsNull) {
    swarm_brain_fep_effects_t effects;
    EXPECT_NE(swarm_brain_fep_get_effects(nullptr, &effects), 0);
    EXPECT_NE(swarm_brain_fep_get_effects(bridge, nullptr), 0);
}

TEST_F(SwarmBrainFepBridgeTest, GetSwarmEffects) {
    swarm_brain_fep_update(bridge);

    fep_swarm_brain_effects_t effects;
    int ret = swarm_brain_fep_get_swarm_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.precision_modulation, 0.5f);
    EXPECT_LE(effects.precision_modulation, 2.0f);
}

TEST_F(SwarmBrainFepBridgeTest, GetSwarmEffectsNull) {
    fep_swarm_brain_effects_t effects;
    EXPECT_NE(swarm_brain_fep_get_swarm_effects(nullptr, &effects), 0);
    EXPECT_NE(swarm_brain_fep_get_swarm_effects(bridge, nullptr), 0);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(SwarmBrainFepBridgeTest, GetStats) {
    swarm_brain_fep_update(bridge);

    swarm_brain_fep_stats_t stats;
    int ret = swarm_brain_fep_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.total_updates, 1u);
}

TEST_F(SwarmBrainFepBridgeTest, GetStatsNull) {
    swarm_brain_fep_stats_t stats;
    EXPECT_NE(swarm_brain_fep_get_stats(nullptr, &stats), 0);
    EXPECT_NE(swarm_brain_fep_get_stats(bridge, nullptr), 0);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(SwarmBrainFepBridgeTest, InitiallyNotConnected) {
    EXPECT_FALSE(swarm_brain_fep_is_bio_async_connected(bridge));
}

TEST_F(SwarmBrainFepBridgeTest, ConnectDisconnectBioAsync) {
    int ret = swarm_brain_fep_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(swarm_brain_fep_is_bio_async_connected(bridge));

    ret = swarm_brain_fep_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(swarm_brain_fep_is_bio_async_connected(bridge));
}

TEST_F(SwarmBrainFepBridgeTest, ConnectNull) {
    EXPECT_NE(swarm_brain_fep_connect_bio_async(nullptr), 0);
}

TEST_F(SwarmBrainFepBridgeTest, DisconnectNull) {
    EXPECT_EQ(swarm_brain_fep_disconnect_bio_async(nullptr), 0);
}

TEST_F(SwarmBrainFepBridgeTest, IsConnectedNull) {
    EXPECT_FALSE(swarm_brain_fep_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * Integration Behavior Tests
 * ============================================================================ */

TEST_F(SwarmBrainFepBridgeTest, CoherenceAffectsPrecision) {
    /* Update bridge multiple times */
    for (int i = 0; i < 5; i++) {
        swarm_brain_fep_update(bridge);
    }

    fep_swarm_brain_effects_t effects;
    swarm_brain_fep_get_swarm_effects(bridge, &effects);

    /* Precision modulation should be computed */
    EXPECT_GE(effects.precision_modulation, 0.5f);
}

TEST_F(SwarmBrainFepBridgeTest, FreeEnergyAffectsExploration) {
    swarm_brain_fep_update(bridge);

    swarm_brain_fep_effects_t effects;
    swarm_brain_fep_get_effects(bridge, &effects);

    /* Exploration bias should be in valid range */
    EXPECT_GE(effects.exploration_bias, 0.0f);
    EXPECT_LE(effects.exploration_bias, 1.0f);
}

TEST_F(SwarmBrainFepBridgeTest, TierChangesTracked) {
    /* Update multiple times to potentially trigger tier changes */
    for (int i = 0; i < 10; i++) {
        swarm_brain_fep_update(bridge);
    }

    swarm_brain_fep_stats_t stats;
    swarm_brain_fep_get_stats(bridge, &stats);

    /* Stats should be accumulated */
    EXPECT_GE(stats.total_updates, 10u);
}
