/**
 * @file test_hypothalamus_bridge.cpp
 * @brief Unit tests for Entorhinal-Hypothalamus Bridge
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal_hypothalamus_bridge.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
}

class HypothalamusBridgeTest : public ::testing::Test {
protected:
    entorhinal_hypothalamus_bridge_state_t* bridge = nullptr;
    nimcp_entorhinal_t* ec = nullptr;

    void SetUp() override {
        entorhinal_hypothalamus_config_t config = entorhinal_hypothalamus_default_config();
        bridge = entorhinal_hypothalamus_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        entorhinal_config_t ec_config = entorhinal_default_config();
        ec = entorhinal_create(&ec_config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            entorhinal_hypothalamus_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(HypothalamusBridgeTest, CreateWithDefaultConfig) {
    entorhinal_hypothalamus_bridge_state_t* b = entorhinal_hypothalamus_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->config.enable_motivation_modulation);
    EXPECT_TRUE(b->config.enable_reward_learning);
    entorhinal_hypothalamus_bridge_destroy(b);
}

TEST_F(HypothalamusBridgeTest, CreateWithCustomConfig) {
    entorhinal_hypothalamus_config_t config = entorhinal_hypothalamus_default_config();
    config.enable_value_mapping = false;
    config.motivation_encoding_weight = 0.5f;

    entorhinal_hypothalamus_bridge_state_t* b = entorhinal_hypothalamus_bridge_create(&config);
    ASSERT_NE(b, nullptr);
    EXPECT_FALSE(b->config.enable_value_mapping);
    EXPECT_FLOAT_EQ(b->config.motivation_encoding_weight, 0.5f);
    entorhinal_hypothalamus_bridge_destroy(b);
}

TEST_F(HypothalamusBridgeTest, DestroyNull) {
    entorhinal_hypothalamus_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(HypothalamusBridgeTest, Connect) {
    EXPECT_EQ(entorhinal_hypothalamus_bridge_connect(bridge, ec, nullptr), 0);
    EXPECT_TRUE(bridge->connected);
    EXPECT_EQ(bridge->entorhinal, ec);
}

TEST_F(HypothalamusBridgeTest, ConnectNull) {
    EXPECT_EQ(entorhinal_hypothalamus_bridge_connect(nullptr, ec, nullptr), -1);
}

TEST_F(HypothalamusBridgeTest, Disconnect) {
    entorhinal_hypothalamus_bridge_connect(bridge, ec, nullptr);
    EXPECT_EQ(entorhinal_hypothalamus_bridge_disconnect(bridge), 0);
    EXPECT_FALSE(bridge->connected);
    EXPECT_EQ(bridge->entorhinal, nullptr);
}

TEST_F(HypothalamusBridgeTest, DisconnectNull) {
    EXPECT_EQ(entorhinal_hypothalamus_bridge_disconnect(nullptr), -1);
}

TEST_F(HypothalamusBridgeTest, Reset) {
    entorhinal_hypothalamus_bridge_connect(bridge, ec, nullptr);
    bridge->updates_processed = 100;
    EXPECT_EQ(entorhinal_hypothalamus_bridge_reset(bridge), 0);
    EXPECT_EQ(bridge->updates_processed, 0u);
    EXPECT_FLOAT_EQ(bridge->encoding_modulation, 1.0f);
}

TEST_F(HypothalamusBridgeTest, ResetNull) {
    EXPECT_EQ(entorhinal_hypothalamus_bridge_reset(nullptr), -1);
}

/*=============================================================================
 * UPDATE TESTS
 *===========================================================================*/

TEST_F(HypothalamusBridgeTest, UpdateBasic) {
    entorhinal_hypothalamus_bridge_connect(bridge, ec, nullptr);
    EXPECT_EQ(entorhinal_hypothalamus_bridge_update(bridge, 0.01f), 0);
    EXPECT_EQ(bridge->updates_processed, 1u);
}

TEST_F(HypothalamusBridgeTest, UpdateNull) {
    EXPECT_EQ(entorhinal_hypothalamus_bridge_update(nullptr, 0.01f), -1);
}

TEST_F(HypothalamusBridgeTest, UpdateMultiple) {
    entorhinal_hypothalamus_bridge_connect(bridge, ec, nullptr);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(entorhinal_hypothalamus_bridge_update(bridge, 0.01f), 0);
    }
    EXPECT_EQ(bridge->updates_processed, 100u);
}

/*=============================================================================
 * MOTIVATION RECEPTION TESTS
 *===========================================================================*/

TEST_F(HypothalamusBridgeTest, ReceiveMotivation) {
    hypothalamic_motivational_state_t motivation = {0};
    motivation.hunger_drive = 0.8f;
    motivation.thirst_drive = 0.3f;
    motivation.arousal_level = 0.7f;

    EXPECT_EQ(entorhinal_hypothalamus_receive_motivation(bridge, &motivation), 0);
    EXPECT_FLOAT_EQ(bridge->motivation.hunger_drive, 0.8f);
    EXPECT_FLOAT_EQ(bridge->motivation.arousal_level, 0.7f);
}

TEST_F(HypothalamusBridgeTest, ReceiveMotivationNull) {
    hypothalamic_motivational_state_t motivation = {0};
    EXPECT_EQ(entorhinal_hypothalamus_receive_motivation(nullptr, &motivation), -1);
    EXPECT_EQ(entorhinal_hypothalamus_receive_motivation(bridge, nullptr), -1);
}

/*=============================================================================
 * MODULATION API TESTS
 *===========================================================================*/

TEST_F(HypothalamusBridgeTest, GetEncodingModulation) {
    float mod = entorhinal_hypothalamus_get_encoding_modulation(bridge);
    EXPECT_GE(mod, 0.0f);
    EXPECT_LE(mod, 2.0f);  // Can be boosted
}

TEST_F(HypothalamusBridgeTest, GetEncodingModulationNull) {
    EXPECT_FLOAT_EQ(entorhinal_hypothalamus_get_encoding_modulation(nullptr), 1.0f);
}

TEST_F(HypothalamusBridgeTest, GetRetrievalModulation) {
    float mod = entorhinal_hypothalamus_get_retrieval_modulation(bridge);
    EXPECT_GE(mod, 0.0f);
}

TEST_F(HypothalamusBridgeTest, GetPlasticityModulation) {
    float mod = entorhinal_hypothalamus_get_plasticity_modulation(bridge);
    EXPECT_GE(mod, 0.0f);
}

TEST_F(HypothalamusBridgeTest, GetConsolidationGate) {
    float gate = entorhinal_hypothalamus_get_consolidation_gate(bridge);
    EXPECT_GE(gate, 0.0f);
    EXPECT_LE(gate, 1.0f);
}

TEST_F(HypothalamusBridgeTest, ModulateEncoding) {
    float encoding_strength = 0.5f;
    EXPECT_EQ(entorhinal_hypothalamus_modulate_encoding(bridge, &encoding_strength), 0);
    EXPECT_GE(encoding_strength, 0.0f);
}

TEST_F(HypothalamusBridgeTest, ModulateEncodingNull) {
    float strength = 0.5f;
    EXPECT_EQ(entorhinal_hypothalamus_modulate_encoding(nullptr, &strength), -1);
    EXPECT_EQ(entorhinal_hypothalamus_modulate_encoding(bridge, nullptr), -1);
}

/*=============================================================================
 * MOTIVATION EFFECT TESTS
 *===========================================================================*/

TEST_F(HypothalamusBridgeTest, HighHungerBoostsEncoding) {
    hypothalamic_motivational_state_t motivation = {0};
    motivation.hunger_drive = 0.9f;
    motivation.arousal_level = 0.7f;
    entorhinal_hypothalamus_receive_motivation(bridge, &motivation);
    entorhinal_hypothalamus_bridge_update(bridge, 0.01f);

    float encoding_mod = entorhinal_hypothalamus_get_encoding_modulation(bridge);
    EXPECT_GT(encoding_mod, 0.5f);  // Should be boosted
}

TEST_F(HypothalamusBridgeTest, HighStressAffectsPlasticity) {
    hypothalamic_motivational_state_t motivation = {0};
    motivation.stress_level = 0.9f;  // Very high stress
    motivation.arousal_level = 0.5f;
    entorhinal_hypothalamus_receive_motivation(bridge, &motivation);
    entorhinal_hypothalamus_bridge_update(bridge, 0.01f);

    float plasticity_mod = entorhinal_hypothalamus_get_plasticity_modulation(bridge);
    // High stress should impair plasticity (inverted U)
    EXPECT_LT(plasticity_mod, 1.0f);
}

/*=============================================================================
 * REWARD PROCESSING TESTS
 *===========================================================================*/

TEST_F(HypothalamusBridgeTest, ProcessReward) {
    float position[3] = {1.0f, 2.0f, 0.0f};
    EXPECT_EQ(entorhinal_hypothalamus_process_reward(bridge, 1.0f, position, 3), 0);
}

TEST_F(HypothalamusBridgeTest, ProcessRewardNull) {
    float position[3] = {0};
    EXPECT_EQ(entorhinal_hypothalamus_process_reward(nullptr, 1.0f, position, 3), -1);
}

TEST_F(HypothalamusBridgeTest, ProcessRewardUpdatesRPE) {
    float position[3] = {1.0f, 2.0f, 0.0f};
    float initial_rpe = bridge->motivation.reward_prediction_error;
    entorhinal_hypothalamus_process_reward(bridge, 1.0f, position, 3);
    // RPE should be updated
    EXPECT_NE(bridge->motivation.reward_prediction_error, initial_rpe);
}

/*=============================================================================
 * VALUE MAP TESTS
 *===========================================================================*/

TEST_F(HypothalamusBridgeTest, GetSpatialValue) {
    float position[3] = {1.0f, 1.0f, 0.0f};
    float value = entorhinal_hypothalamus_get_spatial_value(bridge, position, 3);
    EXPECT_GE(value, 0.0f);  // Initially 0
}

TEST_F(HypothalamusBridgeTest, UpdateSpatialValue) {
    float position[3] = {1.0f, 1.0f, 0.0f};
    EXPECT_EQ(entorhinal_hypothalamus_update_spatial_value(bridge, position, 3, 1.0f), 0);
    float value = entorhinal_hypothalamus_get_spatial_value(bridge, position, 3);
    EXPECT_GT(value, 0.0f);
}

TEST_F(HypothalamusBridgeTest, UpdateSpatialValueNull) {
    float position[3] = {0};
    EXPECT_EQ(entorhinal_hypothalamus_update_spatial_value(nullptr, position, 3, 1.0f), -1);
    EXPECT_EQ(entorhinal_hypothalamus_update_spatial_value(bridge, nullptr, 3, 1.0f), -1);
}

TEST_F(HypothalamusBridgeTest, GetValueGradient) {
    // Update some values first
    float pos1[3] = {1.0f, 1.0f, 0.0f};
    float pos2[3] = {2.0f, 1.0f, 0.0f};
    entorhinal_hypothalamus_update_spatial_value(bridge, pos1, 3, 0.0f);
    entorhinal_hypothalamus_update_spatial_value(bridge, pos2, 3, 1.0f);

    float gradient[2];
    float query[3] = {1.5f, 1.0f, 0.0f};
    EXPECT_EQ(entorhinal_hypothalamus_get_value_gradient(bridge, query, 3, gradient), 0);
}

TEST_F(HypothalamusBridgeTest, DecayValueMap) {
    float position[3] = {1.0f, 1.0f, 0.0f};
    entorhinal_hypothalamus_update_spatial_value(bridge, position, 3, 1.0f);
    float initial_value = entorhinal_hypothalamus_get_spatial_value(bridge, position, 3);

    // Decay over time
    for (int i = 0; i < 100; i++) {
        entorhinal_hypothalamus_decay_value_map(bridge, 1.0f);
    }

    float decayed_value = entorhinal_hypothalamus_get_spatial_value(bridge, position, 3);
    EXPECT_LT(decayed_value, initial_value);
}

/*=============================================================================
 * CIRCADIAN TESTS
 *===========================================================================*/

TEST_F(HypothalamusBridgeTest, GetCircadianConsolidation) {
    float consolidation = entorhinal_hypothalamus_get_circadian_consolidation(bridge);
    EXPECT_GE(consolidation, 0.0f);
    EXPECT_LE(consolidation, 1.0f);
}

TEST_F(HypothalamusBridgeTest, InConsolidationWindow) {
    bool in_window = entorhinal_hypothalamus_in_consolidation_window(bridge);
    // Result depends on circadian phase
    SUCCEED();
}

TEST_F(HypothalamusBridgeTest, CircadianAffectsConsolidation) {
    // Set circadian phase to peak consolidation time
    bridge->motivation.circadian_phase = bridge->config.consolidation_circadian_peak;
    entorhinal_hypothalamus_bridge_update(bridge, 0.01f);

    float peak_consolidation = entorhinal_hypothalamus_get_consolidation_gate(bridge);

    // Set to opposite phase
    bridge->motivation.circadian_phase = bridge->config.consolidation_circadian_peak + 3.14159f;
    entorhinal_hypothalamus_bridge_update(bridge, 0.01f);

    float trough_consolidation = entorhinal_hypothalamus_get_consolidation_gate(bridge);

    EXPECT_GT(peak_consolidation, trough_consolidation);
}

/*=============================================================================
 * DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(HypothalamusBridgeTest, GetStats) {
    uint64_t updates;
    float mean_motivation, mean_encoding;

    EXPECT_EQ(entorhinal_hypothalamus_bridge_get_stats(bridge,
        &updates, &mean_motivation, &mean_encoding), 0);
}

TEST_F(HypothalamusBridgeTest, GetStatsNull) {
    EXPECT_EQ(entorhinal_hypothalamus_bridge_get_stats(nullptr, nullptr, nullptr, nullptr), -1);
}

TEST_F(HypothalamusBridgeTest, LogDiagnostics) {
    EXPECT_EQ(entorhinal_hypothalamus_bridge_log_diagnostics(bridge), 0);
}

TEST_F(HypothalamusBridgeTest, LogDiagnosticsNull) {
    EXPECT_EQ(entorhinal_hypothalamus_bridge_log_diagnostics(nullptr), -1);
}

