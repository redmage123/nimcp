/**
 * @file test_dragonfly_cortical_bridge.cpp
 * @brief Unit tests for Dragonfly Cortical Column Bridge
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_cortical_bridge.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class CorticalBridgeTest : public ::testing::Test {
protected:
    dragonfly_cortical_bridge_t* bridge = nullptr;
    dragonfly_cortical_config_t config;

    void SetUp() override {
        ASSERT_EQ(dragonfly_cortical_bridge_default_config(&config), 0);
    }

    void TearDown() override {
        if (bridge) {
            dragonfly_cortical_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(CorticalBridgeTest, DefaultConfig) {
    EXPECT_EQ(config.mapping_mode, CORTICAL_MAP_DIRECT);
    EXPECT_EQ(config.neurons_per_minicolumn, DRAGONFLY_CORTICAL_NEURONS_PER_MINICOLUMN);
    EXPECT_EQ(config.competition_mode, CORTICAL_COMPETE_SOFTMAX);
    EXPECT_FLOAT_EQ(config.temperature, 1.0f);
    EXPECT_EQ(config.k_winners, 3u);
    EXPECT_TRUE(config.enable_lateral_inhibition);
    EXPECT_FLOAT_EQ(config.inhibition_strength, 0.5f);
    EXPECT_FLOAT_EQ(config.gain_modulation, 1.0f);
    EXPECT_TRUE(config.enable_adaptation);
    EXPECT_TRUE(config.interpolate_output);
}

TEST_F(CorticalBridgeTest, ValidateConfig) {
    EXPECT_EQ(dragonfly_cortical_bridge_validate_config(&config), 0);
}

TEST_F(CorticalBridgeTest, ValidateNullConfig) {
    EXPECT_EQ(dragonfly_cortical_bridge_validate_config(nullptr), -1);
}

TEST_F(CorticalBridgeTest, ValidateInvalidTemperature) {
    config.temperature = 0;
    EXPECT_EQ(dragonfly_cortical_bridge_validate_config(&config), -1);

    config.temperature = -1.0f;
    EXPECT_EQ(dragonfly_cortical_bridge_validate_config(&config), -1);
}

TEST_F(CorticalBridgeTest, ValidateInvalidKWinners) {
    config.k_winners = 0;
    EXPECT_EQ(dragonfly_cortical_bridge_validate_config(&config), -1);

    config.k_winners = DRAGONFLY_CORTICAL_MINICOLUMN_COUNT + 1;
    EXPECT_EQ(dragonfly_cortical_bridge_validate_config(&config), -1);
}

TEST_F(CorticalBridgeTest, ValidateInvalidGain) {
    config.gain_modulation = 0.05f;  // Below 0.1
    EXPECT_EQ(dragonfly_cortical_bridge_validate_config(&config), -1);

    config.gain_modulation = 15.0f;  // Above 10
    EXPECT_EQ(dragonfly_cortical_bridge_validate_config(&config), -1);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(CorticalBridgeTest, CreateDestroy) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    dragonfly_cortical_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(CorticalBridgeTest, CreateWithConfig) {
    config.competition_mode = CORTICAL_COMPETE_WTA;
    config.temperature = 0.5f;

    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(bridge, nullptr);

    dragonfly_cortical_config_t out_config;
    EXPECT_EQ(dragonfly_cortical_bridge_get_config(bridge, &out_config), 0);
    EXPECT_EQ(out_config.competition_mode, CORTICAL_COMPETE_WTA);
    EXPECT_FLOAT_EQ(out_config.temperature, 0.5f);
}

TEST_F(CorticalBridgeTest, CreateWithInvalidConfig) {
    config.temperature = 0;
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, &config);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(CorticalBridgeTest, Reset) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Process something first */
    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    firing_rates[0] = 1.0f;
    cortical_direction_t direction;
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &direction), 0);

    /* Reset */
    EXPECT_EQ(dragonfly_cortical_bridge_reset(bridge), 0);

    /* Get current direction should be zeroed */
    cortical_direction_t reset_dir;
    EXPECT_EQ(dragonfly_cortical_get_direction(bridge, &reset_dir), 0);
    EXPECT_FLOAT_EQ(reset_dir.winner_activation, 0);
}

//=============================================================================
// TSDN to Cortical Conversion Tests
//=============================================================================

TEST_F(CorticalBridgeTest, TSDNToColumnSingleActive) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    firing_rates[0] = 1.0f;

    cortical_direction_t direction;
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &direction), 0);

    /* Winner should be minicolumn 0 */
    EXPECT_EQ(direction.winner_index, 0u);
    EXPECT_GT(direction.winner_activation, 0);
    EXPECT_GT(direction.confidence, 0);
}

TEST_F(CorticalBridgeTest, TSDNToColumnMultipleActive) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Activate two adjacent minicolumns */
    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    firing_rates[3] = 0.8f;
    firing_rates[4] = 1.0f;
    firing_rates[5] = 0.6f;

    cortical_direction_t direction;
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &direction), 0);

    /* Winner should be minicolumn 4 (highest activation) */
    EXPECT_EQ(direction.winner_index, 4u);

    /* Interpolated direction should be between 3 and 5 */
    float expected_angle = 4 * DRAGONFLY_CORTICAL_ANGULAR_SPACING;
    EXPECT_NEAR(direction.azimuth, expected_angle, M_PI / 8);
}

TEST_F(CorticalBridgeTest, TSDNToColumnNullInputs) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    cortical_direction_t direction;

    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(nullptr, firing_rates, &direction), -1);
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, nullptr, &direction), -1);
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, firing_rates, nullptr), -1);
}

TEST_F(CorticalBridgeTest, TSDNToColumnAllZeros) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    cortical_direction_t direction;

    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &direction), 0);

    /* With softmax, all-zero inputs produce uniform distribution: 1/16 each
     * This is mathematically correct - softmax(0,0,...,0) = (1/n, 1/n, ..., 1/n) */
    float expected_uniform = 1.0f / DRAGONFLY_CORTICAL_MINICOLUMN_COUNT;
    EXPECT_NEAR(direction.winner_activation, expected_uniform, 0.01f);

    /* Confidence comes from center-of-mass interpolation, non-zero for uniform */
    EXPECT_GT(direction.confidence, 0);
}

//=============================================================================
// Competition Mode Tests
//=============================================================================

TEST_F(CorticalBridgeTest, CompetitionWTA) {
    config.competition_mode = CORTICAL_COMPETE_WTA;
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(bridge, nullptr);

    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    firing_rates[3] = 0.5f;
    firing_rates[4] = 1.0f;
    firing_rates[5] = 0.5f;

    cortical_direction_t direction;
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &direction), 0);

    /* Only winner should have non-zero activation */
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        if (i == 4) {
            EXPECT_GT(direction.activations[i], 0);
        } else {
            EXPECT_FLOAT_EQ(direction.activations[i], 0);
        }
    }
}

TEST_F(CorticalBridgeTest, CompetitionSoftmax) {
    config.competition_mode = CORTICAL_COMPETE_SOFTMAX;
    config.temperature = 1.0f;
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(bridge, nullptr);

    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    firing_rates[4] = 1.0f;
    firing_rates[5] = 0.8f;

    cortical_direction_t direction;
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &direction), 0);

    /* Multiple minicolumns should have activation, but winner has most */
    EXPECT_GT(direction.activations[4], direction.activations[5]);
    EXPECT_GT(direction.activations[5], 0);

    /* Sum should be approximately 1 (probability distribution) */
    float sum = 0;
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        sum += direction.activations[i];
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);
}

TEST_F(CorticalBridgeTest, CompetitionKWinners) {
    config.competition_mode = CORTICAL_COMPETE_K_WINNERS;
    config.k_winners = 3;
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(bridge, nullptr);

    /* Create input with 5 active minicolumns */
    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    firing_rates[2] = 0.3f;
    firing_rates[3] = 0.6f;
    firing_rates[4] = 1.0f;
    firing_rates[5] = 0.8f;
    firing_rates[6] = 0.4f;

    cortical_direction_t direction;
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &direction), 0);

    /* Count non-zero activations */
    int active_count = 0;
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        if (direction.activations[i] > 0) active_count++;
    }
    EXPECT_LE(active_count, 3);
}

TEST_F(CorticalBridgeTest, CompetitionNone) {
    config.competition_mode = CORTICAL_COMPETE_NONE;
    config.enable_lateral_inhibition = false;
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(bridge, nullptr);

    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        firing_rates[i] = (float)i / DRAGONFLY_CORTICAL_MINICOLUMN_COUNT;
    }

    cortical_direction_t direction;
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &direction), 0);

    /* Activations should match input (no competition) */
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        EXPECT_FLOAT_EQ(direction.activations[i], firing_rates[i]);
    }
}

TEST_F(CorticalBridgeTest, SetCompetition) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cortical_set_competition(bridge, CORTICAL_COMPETE_WTA, 1.0f), 0);

    dragonfly_cortical_config_t out_config;
    EXPECT_EQ(dragonfly_cortical_bridge_get_config(bridge, &out_config), 0);
    EXPECT_EQ(out_config.competition_mode, CORTICAL_COMPETE_WTA);
}

//=============================================================================
// Lateral Inhibition Tests
//=============================================================================

TEST_F(CorticalBridgeTest, LateralInhibitionEnhancesContrast) {
    config.enable_lateral_inhibition = true;
    config.inhibition_strength = 0.5f;
    config.competition_mode = CORTICAL_COMPETE_NONE;  // Test inhibition alone
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(bridge, nullptr);

    /* Create a broad activation pattern */
    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        firing_rates[i] = 0.5f;  // Uniform
    }
    firing_rates[4] = 1.0f;  // One peak

    cortical_direction_t direction;
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &direction), 0);

    /* Peak should remain high, neighbors should be suppressed */
    EXPECT_GT(direction.activations[4], direction.activations[0]);
    EXPECT_GT(direction.activations[4], direction.activations[8]);
}

TEST_F(CorticalBridgeTest, LateralInhibitionDisabled) {
    config.enable_lateral_inhibition = false;
    config.competition_mode = CORTICAL_COMPETE_NONE;
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(bridge, nullptr);

    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        firing_rates[i] = 0.5f;
    }

    cortical_direction_t direction;
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &direction), 0);

    /* All activations should be equal (no inhibition) */
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        EXPECT_FLOAT_EQ(direction.activations[i], 0.5f);
    }
}

//=============================================================================
// Angle Conversion Tests
//=============================================================================

TEST_F(CorticalBridgeTest, AngleToTSDN) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];

    /* Test angle = 0 (should activate minicolumn 0) */
    EXPECT_EQ(dragonfly_cortical_angle_to_tsdn(bridge, 0, 0, firing_rates), 0);
    EXPECT_GT(firing_rates[0], firing_rates[8]);  /* 0° vs 180° */

    /* Test angle = π (should activate minicolumn 8) */
    EXPECT_EQ(dragonfly_cortical_angle_to_tsdn(bridge, M_PI, 0, firing_rates), 0);
    EXPECT_GT(firing_rates[8], firing_rates[0]);
}

TEST_F(CorticalBridgeTest, AngleToTSDNIntermediate) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];

    /* Test angle between minicolumns 2 and 3 */
    float angle = 2.5f * DRAGONFLY_CORTICAL_ANGULAR_SPACING;
    EXPECT_EQ(dragonfly_cortical_angle_to_tsdn(bridge, angle, 0, firing_rates), 0);

    /* Both 2 and 3 should be active */
    EXPECT_GT(firing_rates[2], 0);
    EXPECT_GT(firing_rates[3], 0);

    /* Opposite direction should be low */
    EXPECT_LT(firing_rates[10], firing_rates[2]);
}

TEST_F(CorticalBridgeTest, ColumnToTSDN) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    cortical_direction_t direction;
    memset(&direction, 0, sizeof(direction));
    direction.activations[4] = 0.8f;
    direction.activations[5] = 0.4f;

    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];
    EXPECT_EQ(dragonfly_cortical_column_to_tsdn(bridge, &direction, firing_rates), 0);

    EXPECT_FLOAT_EQ(firing_rates[4], 0.8f);
    EXPECT_FLOAT_EQ(firing_rates[5], 0.4f);
    EXPECT_FLOAT_EQ(firing_rates[0], 0);
}

//=============================================================================
// Interpolation Tests
//=============================================================================

TEST_F(CorticalBridgeTest, InterpolateDirectionCentered) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    float activations[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    activations[4] = 1.0f;

    float azimuth, confidence;
    EXPECT_EQ(dragonfly_cortical_interpolate_direction(bridge, activations, &azimuth, &confidence), 0);

    /* Should be close to minicolumn 4's preferred direction */
    float expected = 4 * DRAGONFLY_CORTICAL_ANGULAR_SPACING;
    EXPECT_NEAR(azimuth, expected, 0.01f);
    EXPECT_GT(confidence, 0.9f);
}

TEST_F(CorticalBridgeTest, InterpolateDirectionBetween) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    float activations[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    activations[4] = 0.5f;
    activations[5] = 0.5f;

    float azimuth, confidence;
    EXPECT_EQ(dragonfly_cortical_interpolate_direction(bridge, activations, &azimuth, &confidence), 0);

    /* Should be between minicolumns 4 and 5 */
    float expected = 4.5f * DRAGONFLY_CORTICAL_ANGULAR_SPACING;
    EXPECT_NEAR(azimuth, expected, 0.1f);
}

//=============================================================================
// Entropy and Sparseness Tests
//=============================================================================

TEST_F(CorticalBridgeTest, ComputeEntropySingleActive) {
    float activations[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    activations[4] = 1.0f;

    float entropy = dragonfly_cortical_compute_entropy(activations, DRAGONFLY_CORTICAL_MINICOLUMN_COUNT);
    EXPECT_FLOAT_EQ(entropy, 0);  /* Only one active = zero entropy */
}

TEST_F(CorticalBridgeTest, ComputeEntropyUniform) {
    float activations[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        activations[i] = 1.0f;
    }

    float entropy = dragonfly_cortical_compute_entropy(activations, DRAGONFLY_CORTICAL_MINICOLUMN_COUNT);
    float max_entropy = log2f(DRAGONFLY_CORTICAL_MINICOLUMN_COUNT);
    EXPECT_NEAR(entropy, max_entropy, 0.01f);  /* Uniform = max entropy */
}

TEST_F(CorticalBridgeTest, ComputeSparseness) {
    /* Single active = maximally sparse */
    float sparse[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    sparse[4] = 1.0f;
    float sp = dragonfly_cortical_compute_sparseness(sparse, DRAGONFLY_CORTICAL_MINICOLUMN_COUNT);
    EXPECT_GT(sp, 0.9f);

    /* Uniform = not sparse */
    float uniform[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        uniform[i] = 1.0f;
    }
    sp = dragonfly_cortical_compute_sparseness(uniform, DRAGONFLY_CORTICAL_MINICOLUMN_COUNT);
    EXPECT_NEAR(sp, 0, 0.01f);
}

//=============================================================================
// Gain and Adaptation Tests
//=============================================================================

TEST_F(CorticalBridgeTest, SetGetGain) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cortical_set_gain(bridge, 2.0f), 0);
    EXPECT_FLOAT_EQ(dragonfly_cortical_get_gain(bridge), 2.0f);
}

TEST_F(CorticalBridgeTest, SetInvalidGain) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cortical_set_gain(bridge, 0.05f), -1);  /* Too low */
    EXPECT_EQ(dragonfly_cortical_set_gain(bridge, 15.0f), -1);  /* Too high */
}

TEST_F(CorticalBridgeTest, GainAffectsOutput) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    firing_rates[4] = 0.5f;

    cortical_direction_t dir1, dir2;

    /* Default gain (1.0) */
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &dir1), 0);

    /* Higher gain (2.0) */
    dragonfly_cortical_set_gain(bridge, 2.0f);
    EXPECT_EQ(dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &dir2), 0);

    /* Higher gain should give higher winner activation (capped at 1.0) */
    EXPECT_GE(dir2.winner_activation, dir1.winner_activation);
}

TEST_F(CorticalBridgeTest, UpdateAdaptation) {
    config.enable_adaptation = true;
    config.adaptation_tau = 100.0f;
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(bridge, nullptr);

    float initial_gain = dragonfly_cortical_get_gain(bridge);

    /* Process high activity */
    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        firing_rates[i] = 1.0f;
    }
    cortical_direction_t direction;
    dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &direction);

    /* Update adaptation */
    EXPECT_EQ(dragonfly_cortical_update_adaptation(bridge, 50.0f), 0);

    /* Gain should decrease with high activity */
    float adapted_gain = dragonfly_cortical_get_gain(bridge);
    EXPECT_LT(adapted_gain, initial_gain);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CorticalBridgeTest, GetStats) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    cortical_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_cortical_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.tsdn_to_cortical_count, 0u);
}

TEST_F(CorticalBridgeTest, StatsUpdated) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    firing_rates[4] = 1.0f;
    cortical_direction_t direction;

    for (int i = 0; i < 10; i++) {
        dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &direction);
    }

    cortical_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_cortical_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.tsdn_to_cortical_count, 10u);
    EXPECT_GT(stats.avg_processing_time_us, 0);
    EXPECT_GT(stats.avg_winner_activation, 0);
}

TEST_F(CorticalBridgeTest, ResetStats) {
    bridge = dragonfly_cortical_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT] = {0};
    firing_rates[4] = 1.0f;
    cortical_direction_t direction;
    dragonfly_cortical_tsdn_to_column(bridge, firing_rates, &direction);

    EXPECT_EQ(dragonfly_cortical_bridge_reset_stats(bridge), 0);

    cortical_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_cortical_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.tsdn_to_cortical_count, 0u);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(CorticalBridgeTest, CompetitionName) {
    EXPECT_STREQ(dragonfly_cortical_competition_name(CORTICAL_COMPETE_WTA), "winner-take-all");
    EXPECT_STREQ(dragonfly_cortical_competition_name(CORTICAL_COMPETE_SOFTMAX), "softmax");
    EXPECT_STREQ(dragonfly_cortical_competition_name(CORTICAL_COMPETE_K_WINNERS), "k-winners");
    EXPECT_STREQ(dragonfly_cortical_competition_name(CORTICAL_COMPETE_NONE), "none");
}

TEST_F(CorticalBridgeTest, MappingName) {
    EXPECT_STREQ(dragonfly_cortical_mapping_name(CORTICAL_MAP_DIRECT), "direct");
    EXPECT_STREQ(dragonfly_cortical_mapping_name(CORTICAL_MAP_INTERPOLATED), "interpolated");
    EXPECT_STREQ(dragonfly_cortical_mapping_name(CORTICAL_MAP_HIERARCHICAL), "hierarchical");
}

TEST_F(CorticalBridgeTest, HypercolumnName) {
    EXPECT_STREQ(dragonfly_cortical_hypercolumn_name(HYPERCOLUMN_AZIMUTH), "azimuth");
    EXPECT_STREQ(dragonfly_cortical_hypercolumn_name(HYPERCOLUMN_ELEVATION), "elevation");
    EXPECT_STREQ(dragonfly_cortical_hypercolumn_name(HYPERCOLUMN_HEADING), "heading");
}

//=============================================================================
// Null Pointer Handling
//=============================================================================

TEST_F(CorticalBridgeTest, NullPointerHandling) {
    EXPECT_EQ(dragonfly_cortical_bridge_default_config(nullptr), -1);
    EXPECT_EQ(dragonfly_cortical_bridge_reset(nullptr), -1);

    EXPECT_EQ(dragonfly_cortical_set_gain(nullptr, 1.0f), -1);
    EXPECT_FLOAT_EQ(dragonfly_cortical_get_gain(nullptr), 1.0f);

    EXPECT_EQ(dragonfly_cortical_update_adaptation(nullptr, 10.0f), -1);

    cortical_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_cortical_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(dragonfly_cortical_bridge_reset_stats(nullptr), -1);

    dragonfly_cortical_config_t cfg;
    EXPECT_EQ(dragonfly_cortical_bridge_get_config(nullptr, &cfg), -1);
    EXPECT_EQ(dragonfly_cortical_bridge_set_config(nullptr, &cfg), -1);
}
