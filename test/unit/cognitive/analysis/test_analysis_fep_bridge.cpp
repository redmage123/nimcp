/**
 * @file test_analysis_fep_bridge.cpp
 * @brief Unit tests for Analysis-FEP Bridge
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/analysis/nimcp_analysis_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class AnalysisFEPBridgeTest : public ::testing::Test {
protected:
    analysis_fep_bridge_t* bridge;
    fep_system_t* fep;
    network_analyzer_t* analyzer;

    void SetUp() override {
        bridge = nullptr;
        fep = nullptr;
        analyzer = nullptr;
    }

    void TearDown() override {
        if (bridge) analysis_fep_bridge_destroy(bridge);
        if (fep) fep_destroy(fep);
        // analyzer would be destroyed by its own test infrastructure
    }
};

TEST_F(AnalysisFEPBridgeTest, DefaultConfig) {
    analysis_fep_config_t config;
    int result = analysis_fep_bridge_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(config.pe_exploration_threshold, ANALYSIS_FEP_HIGH_PE_THRESHOLD);
    EXPECT_TRUE(config.enable_pe_exploration);
    EXPECT_TRUE(config.enable_precision_weighting);
    EXPECT_TRUE(config.enable_topology_priors);
}

TEST_F(AnalysisFEPBridgeTest, CreateDestroy) {
    bridge = analysis_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    EXPECT_FALSE(bridge->base.bio_async_enabled);

    analysis_fep_bridge_destroy(bridge);
    bridge = nullptr; // Prevent double-free in TearDown
}

TEST_F(AnalysisFEPBridgeTest, ConnectFEP) {
    bridge = analysis_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Create minimal FEP system for testing
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_config.num_levels = 2;
    uint32_t dims[] = {16, 8};
    fep_config.level_dims = dims;

    fep = fep_create(&fep_config, 16, 4);
    ASSERT_NE(fep, nullptr);

    int result = analysis_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->fep_system, fep);
}

TEST_F(AnalysisFEPBridgeTest, TriggerExploration) {
    bridge = analysis_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Low PE - should not trigger
    int result = analysis_fep_trigger_exploration(bridge, 2.0f);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(bridge->state.exploration_active);

    // High PE - should trigger
    result = analysis_fep_trigger_exploration(bridge, 10.0f);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->state.exploration_active);
    EXPECT_GT(bridge->stats.exploration_events, 0);
}

TEST_F(AnalysisFEPBridgeTest, GetState) {
    bridge = analysis_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    analysis_fep_state_t state;
    int result = analysis_fep_bridge_get_state(bridge, &state);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.num_communities, 0);
}

TEST_F(AnalysisFEPBridgeTest, GetStats) {
    bridge = analysis_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    analysis_fep_stats_t stats;
    int result = analysis_fep_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.exploration_events, 0);
    EXPECT_EQ(stats.topology_updates, 0);
}

TEST_F(AnalysisFEPBridgeTest, BioAsyncConnection) {
    bridge = analysis_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(analysis_fep_bridge_is_bio_async_connected(bridge));

    int result = analysis_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(result, 0);

    // May or may not connect depending on router availability
    // Just verify no crash
}

TEST_F(AnalysisFEPBridgeTest, Update) {
    bridge = analysis_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = analysis_fep_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);
}

TEST_F(AnalysisFEPBridgeTest, NullPointerHandling) {
    EXPECT_EQ(analysis_fep_bridge_default_config(nullptr), -1);
    EXPECT_EQ(analysis_fep_bridge_connect_fep(nullptr, nullptr), -1);
    EXPECT_EQ(analysis_fep_trigger_exploration(nullptr, 1.0f), -1);
    EXPECT_EQ(analysis_fep_bridge_get_state(nullptr, nullptr), -1);
}
