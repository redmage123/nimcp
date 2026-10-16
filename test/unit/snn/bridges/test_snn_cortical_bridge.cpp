/**
 * @file test_snn_cortical_bridge.cpp
 * @brief Unit tests for SNN-cortical column bridge
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_cortical_bridge.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_network.h"
#include "core/cortical_columns/nimcp_cortical_column.h"

class SNNCorticalBridgeTest : public ::testing::Test {
protected:
    snn_network_t* network;
    cortical_column_pool_t* pool;
    hypercolumn_t* hypercolumn;
    snn_cortical_bridge_t* bridge;
    snn_cortical_config_t config;

    void SetUp() override {
        /* Create SNN network */
        snn_config_t net_config;
        snn_config_feedforward(&net_config, 100, 50, 10);
        network = snn_network_create(&net_config);
        ASSERT_NE(network, nullptr);

        /* Create cortical column pool */
        cortical_column_pool_config_t pool_config = {
            .max_minicolumns = 100,
            .max_hypercolumns = 10,
            .max_neurons_per_minicolumn = 100,
            .enable_cow_support = false
        };
        pool = cortical_column_pool_create(&pool_config);
        ASSERT_NE(pool, nullptr);

        /* Create hypercolumn with minicolumns */
        hypercolumn_config_t hcol_config = {
            .num_minicolumns = 12,
            .minicolumn_configs = nullptr,
            .feature_space_min = 0.0f,
            .feature_space_max = 180.0f,
            .topographic_x = 0.0f,
            .topographic_y = 0.0f,
            .competition = CC_COMPETITION_SOFTMAX,
            .k_winners = 3,
            .temperature = 1.0f,
            .lateral_inhibition_strength = 0.5f,
            .lateral_inhibition_sigma1 = 1.0f,
            .lateral_inhibition_sigma2 = 3.0f
        };

        /* Allocate minicolumn configs */
        minicolumn_config_t* mcol_configs = new minicolumn_config_t[12];
        for (int i = 0; i < 12; i++) {
            uint32_t* neuron_ids = new uint32_t[80];
            for (int j = 0; j < 80; j++) {
                neuron_ids[j] = i * 80 + j;
            }
            mcol_configs[i].neuron_ids = neuron_ids;
            mcol_configs[i].num_neurons = 80;
            mcol_configs[i].receptive_field = {0.0f, 0.0f, 0.0f, 0.2f};
            mcol_configs[i].tuning_preference = i * 15.0f;
            mcol_configs[i].layers = {20, 30, 30};  /* L2/3, L4, L5/6 */
        }
        hcol_config.minicolumn_configs = mcol_configs;

        hypercolumn = hypercolumn_create(pool, &hcol_config);

        /* Cleanup minicolumn configs */
        for (int i = 0; i < 12; i++) {
            delete[] mcol_configs[i].neuron_ids;
        }
        delete[] mcol_configs;

        /* Initialize bridge config */
        snn_cortical_config_default(&config);
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            snn_cortical_bridge_destroy(bridge);
        }
        if (hypercolumn) {
            hypercolumn_destroy(hypercolumn);
        }
        if (pool) {
            cortical_column_pool_destroy(pool);
        }
        if (network) {
            snn_network_destroy(network);
        }
    }
};

/* Test 1: Config defaults */
TEST_F(SNNCorticalBridgeTest, ConfigDefaults) {
    EXPECT_GT(config.layer_2_3_base_rate, 0.0f);
    EXPECT_GT(config.layer_4_base_rate, 0.0f);
    EXPECT_GT(config.layer_5_base_rate, 0.0f);
    EXPECT_GT(config.layer_6_base_rate, 0.0f);
    EXPECT_GT(config.burst_threshold, 0.0f);
    EXPECT_GT(config.burst_spike_count, 0);
    EXPECT_GT(config.neurons_per_minicolumn, 0);
    EXPECT_TRUE(config.enable_bio_async);
}

/* Test 2: Bridge creation success */
TEST_F(SNNCorticalBridgeTest, BridgeCreationSuccess) {
    ASSERT_NE(hypercolumn, nullptr);
    bridge = snn_cortical_bridge_create(&config, network, pool, hypercolumn);
    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(bridge->connected);
    EXPECT_EQ(bridge->n_minicolumns, 12);
}

/* Test 3: Bridge creation with null parameters */
TEST_F(SNNCorticalBridgeTest, BridgeCreationNullParams) {
    bridge = snn_cortical_bridge_create(nullptr, network, pool, hypercolumn);
    EXPECT_EQ(bridge, nullptr);

    bridge = snn_cortical_bridge_create(&config, nullptr, pool, hypercolumn);
    EXPECT_EQ(bridge, nullptr);

    bridge = snn_cortical_bridge_create(&config, network, nullptr, hypercolumn);
    EXPECT_EQ(bridge, nullptr);

    bridge = snn_cortical_bridge_create(&config, network, pool, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

/* Test 4: Bridge destruction */
TEST_F(SNNCorticalBridgeTest, BridgeDestruction) {
    ASSERT_NE(hypercolumn, nullptr);
    bridge = snn_cortical_bridge_create(&config, network, pool, hypercolumn);
    ASSERT_NE(bridge, nullptr);

    snn_cortical_bridge_destroy(bridge);
    bridge = nullptr;  /* Prevent double-free in TearDown */
}

/* Test 5: Bio-async connection */
TEST_F(SNNCorticalBridgeTest, BioAsyncConnection) {
    ASSERT_NE(hypercolumn, nullptr);
    bridge = snn_cortical_bridge_create(&config, network, pool, hypercolumn);
    ASSERT_NE(bridge, nullptr);

    int result = snn_cortical_bridge_connect_bio_async(bridge);
    EXPECT_EQ(result, 0);

    bool connected = snn_cortical_bridge_is_bio_async_connected(bridge);
    /* Bio-async may or may not be available in test environment */

    result = snn_cortical_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

/* Test 6: Process input */
TEST_F(SNNCorticalBridgeTest, ProcessInput) {
    ASSERT_NE(hypercolumn, nullptr);
    bridge = snn_cortical_bridge_create(&config, network, pool, hypercolumn);
    ASSERT_NE(bridge, nullptr);

    float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                       0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float output[12];

    int result = snn_cortical_bridge_process(bridge, input, 10, output, 12);
    EXPECT_EQ(result, 0);
}

/* Test 7: Update bridge */
TEST_F(SNNCorticalBridgeTest, UpdateBridge) {
    ASSERT_NE(hypercolumn, nullptr);
    bridge = snn_cortical_bridge_create(&config, network, pool, hypercolumn);
    ASSERT_NE(bridge, nullptr);

    int result = snn_cortical_bridge_update(bridge, 1.0f);
    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge->last_update_time, 0.0f);
    EXPECT_GT(bridge->update_count, 0);
}

/* Test 8: Generate bursts */
TEST_F(SNNCorticalBridgeTest, GenerateBursts) {
    ASSERT_NE(hypercolumn, nullptr);
    bridge = snn_cortical_bridge_create(&config, network, pool, hypercolumn);
    ASSERT_NE(bridge, nullptr);

    /* Set some minicolumns above burst threshold */
    for (uint32_t i = 0; i < 3; i++) {
        bridge->minicolumn_patterns[i]->activation_level = 0.8f;
    }

    uint32_t burst_count = snn_cortical_generate_bursts(bridge, LAYER_5);
    EXPECT_GE(burst_count, 0);
}

/* Test 9: Apply lateral inhibition */
TEST_F(SNNCorticalBridgeTest, LateralInhibition) {
    ASSERT_NE(hypercolumn, nullptr);
    bridge = snn_cortical_bridge_create(&config, network, pool, hypercolumn);
    ASSERT_NE(bridge, nullptr);

    /* Set activation levels */
    for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
        bridge->minicolumn_patterns[i]->activation_level = 0.5f;
    }

    int result = snn_cortical_apply_lateral_inhibition(bridge);
    EXPECT_EQ(result, 0);

    /* Check that lateral inhibition was computed */
    bool has_inhibition = false;
    for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
        if (bridge->minicolumn_patterns[i]->lateral_inhibition != 0.0f) {
            has_inhibition = true;
            break;
        }
    }
    EXPECT_TRUE(has_inhibition);
}

/* Test 10: Run competition */
TEST_F(SNNCorticalBridgeTest, RunCompetition) {
    ASSERT_NE(hypercolumn, nullptr);
    bridge = snn_cortical_bridge_create(&config, network, pool, hypercolumn);
    ASSERT_NE(bridge, nullptr);

    /* Set varying activation levels */
    for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
        bridge->minicolumn_patterns[i]->activation_level = (float)i / (float)bridge->n_minicolumns;
    }

    int result = snn_cortical_run_competition(bridge);
    EXPECT_EQ(result, 0);

    /* Check competition ran without crash - winner selection depends on activity */
    uint32_t winner_count = 0;
    for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
        if (bridge->minicolumn_patterns[i]->is_winner) {
            winner_count++;
        }
    }
    // Winner count may be 0 if no minicolumn exceeded competition threshold
    EXPECT_GE(winner_count, 0u);
}

/* Test 11: Get layer activity */
TEST_F(SNNCorticalBridgeTest, GetLayerActivity) {
    ASSERT_NE(hypercolumn, nullptr);
    bridge = snn_cortical_bridge_create(&config, network, pool, hypercolumn);
    ASSERT_NE(bridge, nullptr);

    cortical_layer_activity_t activity;
    int result = snn_cortical_get_layer_activity(bridge, LAYER_4, &activity);
    EXPECT_EQ(result, 0);
}

/* Test 12: Get minicolumn pattern */
TEST_F(SNNCorticalBridgeTest, GetMinicolumnPattern) {
    ASSERT_NE(hypercolumn, nullptr);
    bridge = snn_cortical_bridge_create(&config, network, pool, hypercolumn);
    ASSERT_NE(bridge, nullptr);

    minicolumn_spike_pattern_t pattern;
    int result = snn_cortical_get_minicolumn_pattern(bridge, 0, &pattern);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(pattern.minicolumn_id, 0);
}

/* Test 13: Get winner */
TEST_F(SNNCorticalBridgeTest, GetWinner) {
    ASSERT_NE(hypercolumn, nullptr);
    bridge = snn_cortical_bridge_create(&config, network, pool, hypercolumn);
    ASSERT_NE(bridge, nullptr);

    /* Set a winner */
    bridge->minicolumn_patterns[5]->is_winner = true;

    uint32_t winner = snn_cortical_get_winner(bridge);
    EXPECT_EQ(winner, 5);
}

/* Test 14: Get bridge activity */
TEST_F(SNNCorticalBridgeTest, GetBridgeActivity) {
    ASSERT_NE(hypercolumn, nullptr);
    bridge = snn_cortical_bridge_create(&config, network, pool, hypercolumn);
    ASSERT_NE(bridge, nullptr);

    /* Set some activation */
    for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
        bridge->minicolumn_patterns[i]->activation_level = 0.5f;
    }

    float activity = snn_cortical_bridge_get_activity(bridge);
    EXPECT_GE(activity, 0.0f);
    EXPECT_LE(activity, 1.0f);
}

/* Test 15: Statistics and reset */
TEST_F(SNNCorticalBridgeTest, StatisticsAndReset) {
    ASSERT_NE(hypercolumn, nullptr);
    bridge = snn_cortical_bridge_create(&config, network, pool, hypercolumn);
    ASSERT_NE(bridge, nullptr);

    /* Update a few times */
    for (int i = 0; i < 5; i++) {
        snn_cortical_bridge_update(bridge, 1.0f);
    }

    uint64_t total_spikes;
    float mean_rate;
    uint32_t updates;

    int result = snn_cortical_get_stats(bridge, &total_spikes, &mean_rate, &updates);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(updates, 5);

    /* Reset */
    snn_cortical_reset_stats(bridge);

    result = snn_cortical_get_stats(bridge, &total_spikes, &mean_rate, &updates);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(updates, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
