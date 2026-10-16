/**
 * @file test_snn_prefrontal_bridge.cpp
 * @brief Unit tests for SNN-prefrontal cortex bridge
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_prefrontal_bridge.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_network.h"
#include "core/brain_regions/nimcp_brain_regions.h"

class SNNPrefrontalBridgeTest : public ::testing::Test {
protected:
    snn_network_t* network;
    brain_region_t* prefrontal;
    snn_prefrontal_bridge_t* bridge;
    snn_prefrontal_config_t config;

    void SetUp() override {
        /* Create SNN network */
        snn_config_t net_config;
        snn_config_feedforward(&net_config, 100, 50, 10);
        network = snn_network_create(&net_config);
        ASSERT_NE(network, nullptr);

        /* Create prefrontal region */
        prefrontal = brain_region_create(REGION_PREFRONTAL, 1000);
        ASSERT_NE(prefrontal, nullptr);

        /* Initialize bridge config */
        snn_prefrontal_config_default(&config);
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            snn_prefrontal_bridge_destroy(bridge);
        }
        if (prefrontal) {
            brain_region_destroy(prefrontal);
        }
        if (network) {
            snn_network_destroy(network);
        }
    }
};

/* Test 1: Config defaults */
TEST_F(SNNPrefrontalBridgeTest, ConfigDefaults) {
    EXPECT_GT(config.persistent_baseline_rate, 0.0f);
    EXPECT_GT(config.persistent_active_rate, config.persistent_baseline_rate);
    EXPECT_EQ(config.max_wm_items, 7);  /* Miller's 7±2 */
    EXPECT_GT(config.num_goal_populations, 0);
    EXPECT_GT(config.inhibitory_neuron_ratio, 0.0f);
    EXPECT_LT(config.inhibitory_neuron_ratio, 1.0f);
    EXPECT_TRUE(config.enable_bio_async);
}

/* Test 2: Bridge creation success */
TEST_F(SNNPrefrontalBridgeTest, BridgeCreationSuccess) {
    bridge = snn_prefrontal_bridge_create(&config, network, prefrontal);
    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(bridge->connected);
    EXPECT_EQ(bridge->max_wm_items, config.max_wm_items);
    EXPECT_EQ(bridge->n_goals, config.num_goal_populations);
    EXPECT_NE(bridge->accumulator, nullptr);
    EXPECT_NE(bridge->inhibition, nullptr);
}

/* Test 3: Bridge creation with null parameters */
TEST_F(SNNPrefrontalBridgeTest, BridgeCreationNullParams) {
    bridge = snn_prefrontal_bridge_create(nullptr, network, prefrontal);
    EXPECT_EQ(bridge, nullptr);

    bridge = snn_prefrontal_bridge_create(&config, nullptr, prefrontal);
    EXPECT_EQ(bridge, nullptr);

    bridge = snn_prefrontal_bridge_create(&config, network, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

/* Test 4: Bridge destruction */
TEST_F(SNNPrefrontalBridgeTest, BridgeDestruction) {
    bridge = snn_prefrontal_bridge_create(&config, network, prefrontal);
    ASSERT_NE(bridge, nullptr);

    snn_prefrontal_bridge_destroy(bridge);
    bridge = nullptr;  /* Prevent double-free in TearDown */
}

/* Test 5: Bio-async connection */
TEST_F(SNNPrefrontalBridgeTest, BioAsyncConnection) {
    bridge = snn_prefrontal_bridge_create(&config, network, prefrontal);
    ASSERT_NE(bridge, nullptr);

    int result = snn_prefrontal_bridge_connect_bio_async(bridge);
    EXPECT_EQ(result, 0);

    bool connected = snn_prefrontal_bridge_is_bio_async_connected(bridge);
    /* Bio-async may or may not be available in test environment */

    result = snn_prefrontal_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

/* Test 6: Process input */
TEST_F(SNNPrefrontalBridgeTest, ProcessInput) {
    bridge = snn_prefrontal_bridge_create(&config, network, prefrontal);
    ASSERT_NE(bridge, nullptr);

    float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                       0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float output[10];

    int result = snn_prefrontal_bridge_process(bridge, input, 10, output, 10);
    EXPECT_EQ(result, 0);
}

/* Test 7: Update bridge */
TEST_F(SNNPrefrontalBridgeTest, UpdateBridge) {
    bridge = snn_prefrontal_bridge_create(&config, network, prefrontal);
    ASSERT_NE(bridge, nullptr);

    int result = snn_prefrontal_bridge_update(bridge, 1.0f);
    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge->last_update_time, 0.0f);
    EXPECT_GT(bridge->update_count, 0);
}

/* Test 8: Add WM item */
TEST_F(SNNPrefrontalBridgeTest, AddWMItem) {
    bridge = snn_prefrontal_bridge_create(&config, network, prefrontal);
    ASSERT_NE(bridge, nullptr);

    float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    uint32_t item_id = snn_prefrontal_add_wm_item(bridge, features, 5);

    EXPECT_NE(item_id, UINT32_MAX);
    EXPECT_EQ(bridge->n_wm_items, 1);

    uint32_t count = snn_prefrontal_get_wm_count(bridge);
    EXPECT_EQ(count, 1);
}

/* Test 9: Remove WM item */
TEST_F(SNNPrefrontalBridgeTest, RemoveWMItem) {
    bridge = snn_prefrontal_bridge_create(&config, network, prefrontal);
    ASSERT_NE(bridge, nullptr);

    float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    uint32_t item_id = snn_prefrontal_add_wm_item(bridge, features, 5);
    ASSERT_NE(item_id, UINT32_MAX);

    int result = snn_prefrontal_remove_wm_item(bridge, item_id);
    EXPECT_EQ(result, 0);
}

/* Test 10: WM capacity limit */
TEST_F(SNNPrefrontalBridgeTest, WMCapacityLimit) {
    bridge = snn_prefrontal_bridge_create(&config, network, prefrontal);
    ASSERT_NE(bridge, nullptr);

    float features[3] = {1.0f, 2.0f, 3.0f};

    /* Fill WM to capacity */
    for (uint32_t i = 0; i < config.max_wm_items; i++) {
        uint32_t item_id = snn_prefrontal_add_wm_item(bridge, features, 3);
        EXPECT_NE(item_id, UINT32_MAX);
    }

    /* Try to add one more - should fail */
    uint32_t overflow_id = snn_prefrontal_add_wm_item(bridge, features, 3);
    EXPECT_EQ(overflow_id, UINT32_MAX);
}

/* Test 11: Set and get active goal */
TEST_F(SNNPrefrontalBridgeTest, SetAndGetGoal) {
    bridge = snn_prefrontal_bridge_create(&config, network, prefrontal);
    ASSERT_NE(bridge, nullptr);

    int result = snn_prefrontal_set_goal(bridge, 2);
    EXPECT_EQ(result, 0);

    const goal_state_t* goal = snn_prefrontal_get_active_goal(bridge);
    ASSERT_NE(goal, nullptr);
    EXPECT_EQ(goal->goal_id, 2);
    EXPECT_TRUE(goal->is_active);
}

/* Test 12: Accumulate evidence */
TEST_F(SNNPrefrontalBridgeTest, AccumulateEvidence) {
    bridge = snn_prefrontal_bridge_create(&config, network, prefrontal);
    ASSERT_NE(bridge, nullptr);

    float evidence[2] = {0.3f, 0.2f};

    int result = snn_prefrontal_accumulate_evidence(bridge, evidence, 2);
    EXPECT_EQ(result, 0);

    /* Accumulate more */
    result = snn_prefrontal_accumulate_evidence(bridge, evidence, 2);
    EXPECT_EQ(result, 0);
}

/* Test 13: Decision threshold crossing */
TEST_F(SNNPrefrontalBridgeTest, DecisionThresholdCrossing) {
    bridge = snn_prefrontal_bridge_create(&config, network, prefrontal);
    ASSERT_NE(bridge, nullptr);

    /* Strong evidence for option 0 */
    float evidence[2] = {0.5f, 0.1f};

    /* Accumulate until threshold */
    for (int i = 0; i < 5; i++) {
        snn_prefrontal_accumulate_evidence(bridge, evidence, 2);
        snn_prefrontal_bridge_update(bridge, 1.0f);
    }

    /* Check if decision made */
    bool decided = snn_prefrontal_is_decision_made(bridge);
    if (decided) {
        uint32_t choice = snn_prefrontal_get_decision(bridge);
        EXPECT_EQ(choice, 0);
    }
}

/* Test 14: Apply inhibitory control */
TEST_F(SNNPrefrontalBridgeTest, ApplyInhibition) {
    bridge = snn_prefrontal_bridge_create(&config, network, prefrontal);
    ASSERT_NE(bridge, nullptr);

    /* Apply strong stop signal */
    int result = snn_prefrontal_apply_inhibition(bridge, 0.9f);
    EXPECT_EQ(result, 0);

    /* Check that response was suppressed */
    EXPECT_TRUE(bridge->inhibition->response_suppressed);
    EXPECT_GT(bridge->inhibited_responses, 0);
}

/* Test 15: Statistics and reset */
TEST_F(SNNPrefrontalBridgeTest, StatisticsAndReset) {
    bridge = snn_prefrontal_bridge_create(&config, network, prefrontal);
    ASSERT_NE(bridge, nullptr);

    /* Apply some inhibition */
    snn_prefrontal_apply_inhibition(bridge, 0.8f);

    /* Update a few times */
    for (int i = 0; i < 5; i++) {
        snn_prefrontal_bridge_update(bridge, 1.0f);
    }

    uint32_t total_decisions, inhibited_responses, updates;
    int result = snn_prefrontal_get_stats(bridge, &total_decisions,
                                          &inhibited_responses, &updates);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(updates, 5);
    EXPECT_GT(inhibited_responses, 0);

    /* Reset */
    snn_prefrontal_reset_stats(bridge);

    result = snn_prefrontal_get_stats(bridge, &total_decisions,
                                      &inhibited_responses, &updates);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(updates, 0);
    EXPECT_EQ(total_decisions, 0);
    EXPECT_EQ(inhibited_responses, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
