/**
 * @file test_mirror_neurons_fep_bridge.cpp
 * @brief Unit tests for Mirror Neurons FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Mirror Neurons bidirectional integration
 * WHY:  Ensure action understanding, goal inference, and motor evidence work correctly
 * HOW:  Test lifecycle, connections, precision modulation, goal updates, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/mirror_neurons/nimcp_mirror_neurons_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class MirrorNeuronsFepBridgeTest : public ::testing::Test {
protected:
    mirror_neurons_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        mirror_neurons_fep_config_t config;
        mirror_neurons_fep_bridge_default_config(&config);
        bridge = mirror_neurons_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            mirror_neurons_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(MirrorNeuronsFepBridgeTest, CreateWithNullConfig) {
    mirror_neurons_fep_bridge_t* br = mirror_neurons_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    mirror_neurons_fep_bridge_destroy(br);
}

TEST_F(MirrorNeuronsFepBridgeTest, DestroyNull) {
    mirror_neurons_fep_bridge_destroy(nullptr);
}

TEST_F(MirrorNeuronsFepBridgeTest, DefaultConfig) {
    mirror_neurons_fep_config_t config;
    int ret = mirror_neurons_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.precision_gain_factor, 0.0f);
    EXPECT_GT(config.goal_belief_coupling_rate, 0.0f);
    EXPECT_TRUE(config.enable_precision_modulation);
    EXPECT_TRUE(config.enable_goal_belief_coupling);
}

TEST_F(MirrorNeuronsFepBridgeTest, DefaultConfigNullPtr) {
    int ret = mirror_neurons_fep_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = mirror_neurons_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(MirrorNeuronsFepBridgeTest, ConnectFepNull) {
    EXPECT_NE(mirror_neurons_fep_bridge_connect_fep(nullptr, nullptr), 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, ConnectMirrorNeurons) {
    mirror_hierarchy_t mirror = 0;
    int ret = mirror_neurons_fep_bridge_connect_mirror_neurons(bridge, mirror);
    EXPECT_EQ(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, ConnectMirrorNeuronsNull) {
    mirror_hierarchy_t mirror = 0;
    EXPECT_NE(mirror_neurons_fep_bridge_connect_mirror_neurons(nullptr, mirror), 0);
}

/* ============================================================================
 * FEP → Mirror Neurons Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsFepBridgeTest, ApplyPrecisionModulation) {
    int ret = mirror_neurons_fep_apply_precision_modulation(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, ApplyPrecisionModulationNull) {
    int ret = mirror_neurons_fep_apply_precision_modulation(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, UpdateGoalsFromBeliefs) {
    int ret = mirror_neurons_fep_update_goals_from_beliefs(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, UpdateGoalsFromBeliefsNull) {
    int ret = mirror_neurons_fep_update_goals_from_beliefs(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, PropagatePredictionErrors) {
    int ret = mirror_neurons_fep_propagate_prediction_errors(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, PropagatePredictionErrorsNull) {
    int ret = mirror_neurons_fep_propagate_prediction_errors(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Mirror Neurons → FEP Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsFepBridgeTest, TransferGoalsToBeliefs) {
    int ret = mirror_neurons_fep_transfer_goals_to_beliefs(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, TransferGoalsToBeliefsNull) {
    int ret = mirror_neurons_fep_transfer_goals_to_beliefs(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, ProvideMotorEvidence) {
    int ret = mirror_neurons_fep_provide_motor_evidence(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, ProvideMotorEvidenceNull) {
    int ret = mirror_neurons_fep_provide_motor_evidence(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, SetPrecisionFromResonance) {
    int ret = mirror_neurons_fep_set_precision_from_resonance(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, SetPrecisionFromResonanceNull) {
    int ret = mirror_neurons_fep_set_precision_from_resonance(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Update & State Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsFepBridgeTest, Update) {
    int ret = mirror_neurons_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, UpdateNull) {
    int ret = mirror_neurons_fep_bridge_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, GetState) {
    mirror_neurons_fep_state_t state;
    int ret = mirror_neurons_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, GetStateNull) {
    mirror_neurons_fep_state_t state;
    EXPECT_NE(mirror_neurons_fep_bridge_get_state(nullptr, &state), 0);
    EXPECT_NE(mirror_neurons_fep_bridge_get_state(bridge, nullptr), 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, GetStats) {
    mirror_neurons_fep_stats_t stats;
    int ret = mirror_neurons_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, GetStatsNull) {
    mirror_neurons_fep_stats_t stats;
    EXPECT_NE(mirror_neurons_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(mirror_neurons_fep_bridge_get_stats(bridge, nullptr), 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(MirrorNeuronsFepBridgeTest, ConnectBioAsync) {
    int ret = mirror_neurons_fep_bridge_connect_bio_async(bridge);
    (void)ret;
}

TEST_F(MirrorNeuronsFepBridgeTest, ConnectBioAsyncNull) {
    int ret = mirror_neurons_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, DisconnectBioAsync) {
    mirror_neurons_fep_bridge_connect_bio_async(bridge);
    int ret = mirror_neurons_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, DisconnectBioAsyncNull) {
    int ret = mirror_neurons_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(MirrorNeuronsFepBridgeTest, IsBioAsyncConnected) {
    bool connected = mirror_neurons_fep_bridge_is_bio_async_connected(bridge);
    (void)connected;
}

TEST_F(MirrorNeuronsFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = mirror_neurons_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}
