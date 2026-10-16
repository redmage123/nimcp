/**
 * @file test_curiosity_fep_bridge.cpp
 * @brief Unit tests for Curiosity FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Curiosity bidirectional integration
 * WHY:  Ensure epistemic value computation and exploration triggering work correctly
 * HOW:  Test lifecycle, connections, epistemic value, knowledge gaps, exploration, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/curiosity/nimcp_curiosity_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class CuriosityFepBridgeTest : public ::testing::Test {
protected:
    curiosity_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        curiosity_fep_config_t config;
        curiosity_fep_bridge_default_config(&config);
        bridge = curiosity_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            curiosity_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(CuriosityFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(CuriosityFepBridgeTest, CreateWithNullConfig) {
    curiosity_fep_bridge_t* br = curiosity_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    curiosity_fep_bridge_destroy(br);
}

TEST_F(CuriosityFepBridgeTest, DestroyNull) {
    curiosity_fep_bridge_destroy(nullptr);
}

TEST_F(CuriosityFepBridgeTest, DefaultConfig) {
    curiosity_fep_config_t config;
    int ret = curiosity_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.epistemic_value_weight, 0.0f);
    EXPECT_GT(config.uncertainty_sensitivity, 0.0f);
    EXPECT_TRUE(config.enable_epistemic_curiosity);
    EXPECT_TRUE(config.enable_knowledge_gap_detection);
}

TEST_F(CuriosityFepBridgeTest, DefaultConfigNullPtr) {
    int ret = curiosity_fep_bridge_default_config(nullptr);
    EXPECT_NE(ret, 0);  /* Returns error code for NULL */
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(CuriosityFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_config.num_levels = 2;
    fep_system_t* fep = fep_create(&fep_config, 4, 4);
    ASSERT_NE(fep, nullptr);

    int ret = curiosity_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(CuriosityFepBridgeTest, ConnectFepNull) {
    EXPECT_NE(curiosity_fep_bridge_connect_fep(nullptr, nullptr), 0);
}

TEST_F(CuriosityFepBridgeTest, ConnectCuriosity) {
    /* Passing NULL/0 curiosity engine should fail */
    curiosity_engine_t curiosity = 0;
    int ret = curiosity_fep_bridge_connect_curiosity(bridge, curiosity);
    EXPECT_NE(ret, 0);  /* NULL curiosity engine should be rejected */
}

TEST_F(CuriosityFepBridgeTest, ConnectCuriosityNull) {
    curiosity_engine_t curiosity = 0;
    EXPECT_NE(curiosity_fep_bridge_connect_curiosity(nullptr, curiosity), 0);
}

/* ============================================================================
 * FEP → Curiosity Tests
 * ============================================================================ */

TEST_F(CuriosityFepBridgeTest, ComputeEpistemicValue) {
    /* Must connect FEP system before computing epistemic value */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_config.num_levels = 2;
    fep_system_t* fep = fep_create(&fep_config, 4, 4);
    ASSERT_NE(fep, nullptr);

    curiosity_fep_bridge_connect_fep(bridge, fep);
    int ret = curiosity_fep_compute_epistemic_value(bridge);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(CuriosityFepBridgeTest, ComputeEpistemicValueNull) {
    int ret = curiosity_fep_compute_epistemic_value(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityFepBridgeTest, DetectKnowledgeGaps) {
    /* Must connect FEP system before detecting knowledge gaps */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_config.num_levels = 2;
    fep_system_t* fep = fep_create(&fep_config, 4, 4);
    ASSERT_NE(fep, nullptr);

    curiosity_fep_bridge_connect_fep(bridge, fep);
    int ret = curiosity_fep_detect_knowledge_gaps(bridge);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(CuriosityFepBridgeTest, DetectKnowledgeGapsNull) {
    int ret = curiosity_fep_detect_knowledge_gaps(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityFepBridgeTest, TriggerExploration) {
    int ret = curiosity_fep_trigger_exploration(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityFepBridgeTest, TriggerExplorationNull) {
    int ret = curiosity_fep_trigger_exploration(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Curiosity → FEP Tests
 * ============================================================================ */

TEST_F(CuriosityFepBridgeTest, UpdateModelFromLearning) {
    /* Requires both FEP and curiosity systems to be connected */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_config.num_levels = 2;
    fep_system_t* fep = fep_create(&fep_config, 4, 4);
    ASSERT_NE(fep, nullptr);

    curiosity_fep_bridge_connect_fep(bridge, fep);
    int ret = curiosity_fep_update_model_from_learning(bridge);
    /* Without connected curiosity engine, this operation correctly fails */
    EXPECT_NE(ret, 0);

    fep_destroy(fep);
}

TEST_F(CuriosityFepBridgeTest, UpdateModelFromLearningNull) {
    int ret = curiosity_fep_update_model_from_learning(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Update & State Tests
 * ============================================================================ */

TEST_F(CuriosityFepBridgeTest, Update) {
    int ret = curiosity_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityFepBridgeTest, UpdateNull) {
    int ret = curiosity_fep_bridge_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityFepBridgeTest, GetState) {
    curiosity_fep_state_t state;
    int ret = curiosity_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityFepBridgeTest, GetStateNull) {
    curiosity_fep_state_t state;
    EXPECT_NE(curiosity_fep_bridge_get_state(nullptr, &state), 0);
    EXPECT_NE(curiosity_fep_bridge_get_state(bridge, nullptr), 0);
}

TEST_F(CuriosityFepBridgeTest, GetStats) {
    curiosity_fep_stats_t stats;
    int ret = curiosity_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityFepBridgeTest, GetStatsNull) {
    curiosity_fep_stats_t stats;
    EXPECT_NE(curiosity_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(curiosity_fep_bridge_get_stats(bridge, nullptr), 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(CuriosityFepBridgeTest, ConnectBioAsync) {
    int ret = curiosity_fep_bridge_connect_bio_async(bridge);
    (void)ret;
}

TEST_F(CuriosityFepBridgeTest, ConnectBioAsyncNull) {
    int ret = curiosity_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityFepBridgeTest, DisconnectBioAsync) {
    curiosity_fep_bridge_connect_bio_async(bridge);
    int ret = curiosity_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityFepBridgeTest, DisconnectBioAsyncNull) {
    int ret = curiosity_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityFepBridgeTest, IsBioAsyncConnected) {
    bool connected = curiosity_fep_bridge_is_bio_async_connected(bridge);
    (void)connected;
}

TEST_F(CuriosityFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = curiosity_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Epistemic Value Computation Tests
 * ============================================================================ */

TEST_F(CuriosityFepBridgeTest, EpistemicValueWithFEP) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_config.num_levels = 2;
    fep_system_t* fep = fep_create(&fep_config, 4, 4);
    ASSERT_NE(fep, nullptr);

    curiosity_fep_bridge_connect_fep(bridge, fep);

    /* Set prediction error magnitude */
    fep->levels[0].errors.magnitude = 0.5f;
    fep->levels[1].errors.magnitude = 0.3f;

    int ret = curiosity_fep_compute_epistemic_value(bridge);
    EXPECT_EQ(ret, 0);

    /* Should have computed epistemic value */
    EXPECT_GT(bridge->state.current_epistemic_value, 0.0f);
    EXPECT_LE(bridge->state.current_epistemic_value, EPISTEMIC_VALUE_MAX);

    fep_destroy(fep);
}

TEST_F(CuriosityFepBridgeTest, EpistemicValueDisabled) {
    bridge->config.enable_epistemic_curiosity = false;
    int ret = curiosity_fep_compute_epistemic_value(bridge);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Knowledge Gap Detection Tests
 * ============================================================================ */

TEST_F(CuriosityFepBridgeTest, KnowledgeGapsWithUncertainty) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_config.num_levels = 2;
    fep_system_t* fep = fep_create(&fep_config, 4, 4);
    ASSERT_NE(fep, nullptr);

    curiosity_fep_bridge_connect_fep(bridge, fep);

    /* Set low precision (high uncertainty) */
    for (uint32_t i = 0; i < fep->levels[0].errors.dim; i++) {
        fep->levels[0].errors.precision[i] = 0.05f;
    }

    int ret = curiosity_fep_detect_knowledge_gaps(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(bridge->state.current_uncertainty, 0.0f);

    fep_destroy(fep);
}

TEST_F(CuriosityFepBridgeTest, KnowledgeGapsDisabled) {
    bridge->config.enable_knowledge_gap_detection = false;
    int ret = curiosity_fep_detect_knowledge_gaps(bridge);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Exploration Trigger Tests
 * ============================================================================ */

TEST_F(CuriosityFepBridgeTest, ExplorationWithHighEpistemic) {
    bridge->state.current_epistemic_value = 0.8f;
    bridge->state.current_uncertainty = 0.6f;

    int ret = curiosity_fep_trigger_exploration(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(bridge->effects.exploration_motivation, 0.0f);
    EXPECT_GT(bridge->effects.curiosity_boost, 0.0f);
}

TEST_F(CuriosityFepBridgeTest, ExplorationDisabled) {
    bridge->config.enable_exploration_feedback = false;
    int ret = curiosity_fep_trigger_exploration(bridge);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Update Integration Tests
 * ============================================================================ */

TEST_F(CuriosityFepBridgeTest, UpdateWithConnectedFEP) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_config.num_levels = 2;
    fep_system_t* fep = fep_create(&fep_config, 4, 4);
    ASSERT_NE(fep, nullptr);

    curiosity_fep_bridge_connect_fep(bridge, fep);

    /* Set prediction errors */
    fep->levels[0].errors.magnitude = 0.5f;
    fep->levels[1].errors.magnitude = 0.3f;

    int ret = curiosity_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);

    /* Should have updated stats */
    EXPECT_GT(bridge->stats.total_epistemic_triggers, 0);

    fep_destroy(fep);
}

TEST_F(CuriosityFepBridgeTest, UpdateMultipleTimes) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_config.num_levels = 2;
    fep_system_t* fep = fep_create(&fep_config, 4, 4);
    ASSERT_NE(fep, nullptr);

    curiosity_fep_bridge_connect_fep(bridge, fep);

    /* Update multiple times */
    for (int i = 0; i < 5; i++) {
        fep->levels[0].errors.magnitude = 0.5f + (float)i * 0.1f;
        curiosity_fep_bridge_update(bridge, 100);
    }

    /* Should have accumulated statistics */
    EXPECT_GE(bridge->stats.total_epistemic_triggers, 5);

    fep_destroy(fep);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
