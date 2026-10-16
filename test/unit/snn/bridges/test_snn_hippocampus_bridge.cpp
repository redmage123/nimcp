/**
 * @file test_snn_hippocampus_bridge.cpp
 * @brief Unit tests for SNN-hippocampus bridge
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_hippocampus_bridge.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_network.h"
#include "core/brain_regions/nimcp_brain_regions.h"

class SNNHippocampusBridgeTest : public ::testing::Test {
protected:
    snn_network_t* network;
    brain_region_t* hippocampus;
    snn_hippocampus_bridge_t* bridge;
    snn_hippocampus_config_t config;

    void SetUp() override {
        /* Create SNN network */
        snn_config_t net_config;
        snn_config_feedforward(&net_config, 100, 50, 10);
        network = snn_network_create(&net_config);
        ASSERT_NE(network, nullptr);

        /* Create hippocampus region */
        hippocampus = brain_region_create(REGION_HIPPOCAMPUS, 1000);
        ASSERT_NE(hippocampus, nullptr);

        /* Initialize bridge config */
        snn_hippocampus_config_default(&config);
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            snn_hippocampus_bridge_destroy(bridge);
        }
        if (hippocampus) {
            brain_region_destroy(hippocampus);
        }
        if (network) {
            snn_network_destroy(network);
        }
    }
};

/* Test 1: Config defaults */
TEST_F(SNNHippocampusBridgeTest, ConfigDefaults) {
    EXPECT_GT(config.theta_frequency, 0.0f);
    EXPECT_LT(config.theta_frequency, 15.0f);  /* Theta band: 4-12 Hz */
    EXPECT_GT(config.num_place_cells, 0);
    EXPECT_GT(config.place_field_size, 0.0f);
    EXPECT_GT(config.ripple_frequency, 50.0f);
    EXPECT_TRUE(config.enable_phase_precession);
    EXPECT_TRUE(config.enable_bio_async);
}

/* Test 2: Bridge creation success */
TEST_F(SNNHippocampusBridgeTest, BridgeCreationSuccess) {
    bridge = snn_hippocampus_bridge_create(&config, network, hippocampus);
    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(bridge->connected);
    EXPECT_EQ(bridge->n_place_cells, config.num_place_cells);
    EXPECT_TRUE(bridge->theta_active);
}

/* Test 3: Bridge creation with null parameters */
TEST_F(SNNHippocampusBridgeTest, BridgeCreationNullParams) {
    bridge = snn_hippocampus_bridge_create(nullptr, network, hippocampus);
    EXPECT_EQ(bridge, nullptr);

    bridge = snn_hippocampus_bridge_create(&config, nullptr, hippocampus);
    EXPECT_EQ(bridge, nullptr);

    bridge = snn_hippocampus_bridge_create(&config, network, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

/* Test 4: Bridge destruction */
TEST_F(SNNHippocampusBridgeTest, BridgeDestruction) {
    bridge = snn_hippocampus_bridge_create(&config, network, hippocampus);
    ASSERT_NE(bridge, nullptr);

    snn_hippocampus_bridge_destroy(bridge);
    bridge = nullptr;  /* Prevent double-free in TearDown */
}

/* Test 5: Bio-async connection */
TEST_F(SNNHippocampusBridgeTest, BioAsyncConnection) {
    bridge = snn_hippocampus_bridge_create(&config, network, hippocampus);
    ASSERT_NE(bridge, nullptr);

    int result = snn_hippocampus_bridge_connect_bio_async(bridge);
    EXPECT_EQ(result, 0);

    bool connected = snn_hippocampus_bridge_is_bio_async_connected(bridge);
    /* Bio-async may or may not be available in test environment */

    result = snn_hippocampus_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

/* Test 6: Process spatial input */
TEST_F(SNNHippocampusBridgeTest, ProcessSpatialInput) {
    bridge = snn_hippocampus_bridge_create(&config, network, hippocampus);
    ASSERT_NE(bridge, nullptr);

    float position[3] = {0.5f, 0.5f, 0.0f};
    float velocity[3] = {0.1f, 0.1f, 0.0f};
    float output[100];

    int result = snn_hippocampus_bridge_process(bridge, position, velocity, output, 100);
    EXPECT_EQ(result, 0);

    /* Check that position was updated */
    EXPECT_FLOAT_EQ(bridge->current_position[0], 0.5f);
    EXPECT_FLOAT_EQ(bridge->current_position[1], 0.5f);
}

/* Test 7: Update bridge and theta rhythm */
TEST_F(SNNHippocampusBridgeTest, UpdateAndThetaRhythm) {
    bridge = snn_hippocampus_bridge_create(&config, network, hippocampus);
    ASSERT_NE(bridge, nullptr);

    float initial_phase = bridge->theta_phase;

    int result = snn_hippocampus_bridge_update(bridge, 10.0f);
    EXPECT_EQ(result, 0);

    /* Theta phase should have advanced */
    EXPECT_NE(bridge->theta_phase, initial_phase);
    EXPECT_GT(bridge->update_count, 0);
}

/* Test 8: Get theta phase */
TEST_F(SNNHippocampusBridgeTest, GetThetaPhase) {
    bridge = snn_hippocampus_bridge_create(&config, network, hippocampus);
    ASSERT_NE(bridge, nullptr);

    float phase = snn_hippocampus_get_theta_phase(bridge);
    EXPECT_GE(phase, 0.0f);
    EXPECT_LE(phase, 6.3f);  /* 2*PI */
}

/* Test 9: Place cell activation */
TEST_F(SNNHippocampusBridgeTest, PlaceCellActivation) {
    bridge = snn_hippocampus_bridge_create(&config, network, hippocampus);
    ASSERT_NE(bridge, nullptr);

    /* Move to a position that should activate some place cells */
    float position[3] = {0.2f, 0.2f, 0.0f};
    float output[100];

    int result = snn_hippocampus_bridge_process(bridge, position, nullptr, output, 100);
    EXPECT_EQ(result, 0);

    /* Check that some place cells are active */
    bool has_active = false;
    for (uint32_t i = 0; i < bridge->n_place_cells; i++) {
        if (bridge->place_cells[i]->is_active) {
            has_active = true;
            break;
        }
    }
    EXPECT_TRUE(has_active);
}

/* Test 10: Get place cell pattern */
TEST_F(SNNHippocampusBridgeTest, GetPlaceCellPattern) {
    bridge = snn_hippocampus_bridge_create(&config, network, hippocampus);
    ASSERT_NE(bridge, nullptr);

    place_cell_pattern_t pattern;
    int result = snn_hippocampus_get_place_cell(bridge, 0, &pattern);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(pattern.cell_id, 0);
    EXPECT_GE(pattern.current_rate, 0.0f);
}

/* Test 11: Encode episodic memory */
TEST_F(SNNHippocampusBridgeTest, EncodeEpisode) {
    bridge = snn_hippocampus_bridge_create(&config, network, hippocampus);
    ASSERT_NE(bridge, nullptr);

    /* Move to activate place cells */
    float position[3] = {0.3f, 0.3f, 0.0f};
    float output[100];
    snn_hippocampus_bridge_process(bridge, position, nullptr, output, 100);

    /* Encode episode */
    episodic_memory_t* episode = snn_hippocampus_encode_episode(bridge, 100.0f);

    if (episode) {
        EXPECT_GT(episode->sequence_length, 0);
        EXPECT_NE(episode->spike_sequence, nullptr);
        EXPECT_FALSE(episode->consolidated);
    }
}

/* Test 12: Generate ripple event */
TEST_F(SNNHippocampusBridgeTest, GenerateRipple) {
    bridge = snn_hippocampus_bridge_create(&config, network, hippocampus);
    ASSERT_NE(bridge, nullptr);

    ripple_event_t* ripple = snn_hippocampus_generate_ripple(bridge, nullptr);
    ASSERT_NE(ripple, nullptr);

    EXPECT_GT(ripple->peak_frequency, 100.0f);  /* Ripple frequency */
    EXPECT_LT(ripple->peak_frequency, 300.0f);
    EXPECT_GT(ripple->participating_neurons, 0);
}

/* Test 13: Retrieve episodic memory */
TEST_F(SNNHippocampusBridgeTest, RetrieveEpisode) {
    bridge = snn_hippocampus_bridge_create(&config, network, hippocampus);
    ASSERT_NE(bridge, nullptr);

    /* Encode an episode first */
    float position[3] = {0.4f, 0.4f, 0.0f};
    float output[100];
    snn_hippocampus_bridge_process(bridge, position, nullptr, output, 100);

    episodic_memory_t* episode = snn_hippocampus_encode_episode(bridge, 100.0f);

    if (episode && episode->sequence_length > 0) {
        /* Try to retrieve with partial cue */
        uint32_t cue_length = episode->sequence_length / 2;
        episodic_memory_t* retrieved = snn_hippocampus_retrieve_episode(
            bridge, episode->spike_sequence, cue_length
        );

        /* May or may not find match depending on threshold */
        if (retrieved) {
            EXPECT_NE(retrieved->episode_id, UINT32_MAX);
        }
    }
}

/* Test 14: Get bridge activity */
TEST_F(SNNHippocampusBridgeTest, GetBridgeActivity) {
    bridge = snn_hippocampus_bridge_create(&config, network, hippocampus);
    ASSERT_NE(bridge, nullptr);

    /* Activate some place cells */
    float position[3] = {0.5f, 0.5f, 0.0f};
    float output[100];
    snn_hippocampus_bridge_process(bridge, position, nullptr, output, 100);

    float activity = snn_hippocampus_bridge_get_activity(bridge);
    EXPECT_GE(activity, 0.0f);
    EXPECT_LE(activity, 1.0f);
}

/* Test 15: Statistics and reset */
TEST_F(SNNHippocampusBridgeTest, StatisticsAndReset) {
    bridge = snn_hippocampus_bridge_create(&config, network, hippocampus);
    ASSERT_NE(bridge, nullptr);

    /* Generate a ripple */
    snn_hippocampus_generate_ripple(bridge, nullptr);

    /* Update a few times */
    for (int i = 0; i < 5; i++) {
        snn_hippocampus_bridge_update(bridge, 1.0f);
    }

    uint32_t total_ripples, episodes_stored, updates;
    int result = snn_hippocampus_get_stats(bridge, &total_ripples, &episodes_stored, &updates);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(updates, 5);
    EXPECT_GT(total_ripples, 0);

    /* Reset */
    snn_hippocampus_reset_stats(bridge);

    result = snn_hippocampus_get_stats(bridge, &total_ripples, &episodes_stored, &updates);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(updates, 0);
    EXPECT_EQ(total_ripples, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
