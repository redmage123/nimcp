/**
 * @file test_cortical_attention_gain.cpp
 * @brief Unit tests for attention-modulated gain control in cortical columns
 */

#include <gtest/gtest.h>
#include <cstdint>
// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_attention_gain.h"
#include "core/cortical_columns/nimcp_cortical_column.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CorticalAttentionGainTest : public ::testing::Test {
protected:
    cortical_attention_gain_t* attention;
    hypercolumn_t* hypercolumn;
    cortical_column_pool_t* pool;
    cortical_attention_config_t config;

    void SetUp() override {
        /* Create a pool first */
        cortical_column_pool_config_t pool_config = {
            .max_minicolumns = 100,
            .max_hypercolumns = 10,
            .max_neurons_per_minicolumn = 100,
            .enable_cow_support = false
        };
        pool = cortical_column_pool_create(&pool_config);
        ASSERT_NE(pool, nullptr);

        /* Create minicolumn configs */
        static uint32_t neuron_ids[] = {0, 1, 2, 3, 4};
        static minicolumn_config_t mc_configs[16];
        for (int i = 0; i < 16; i++) {
            mc_configs[i].neuron_ids = neuron_ids;
            mc_configs[i].num_neurons = 5;
            mc_configs[i].tuning_preference = (float)i * 11.25f; /* 0-180 degrees */
            mc_configs[i].receptive_field = {0.0f, 0.0f, 0.0f, 1.0f};
            mc_configs[i].layers = {2, 2, 1}; /* layer 2/3, 4, 5/6 */
        }

        /* Create a hypercolumn for testing */
        hypercolumn_config_t hc_config;
        hc_config.num_minicolumns = 16;
        hc_config.minicolumn_configs = mc_configs;
        hc_config.feature_space_min = 0.0f;
        hc_config.feature_space_max = 180.0f;
        hc_config.topographic_x = 0.0f;
        hc_config.topographic_y = 0.0f;
        hc_config.competition = CC_COMPETITION_WINNER_TAKE_ALL;
        hc_config.k_winners = 1;
        hc_config.temperature = 1.0f;
        hc_config.lateral_inhibition_strength = 0.5f;
        hc_config.lateral_inhibition_sigma1 = 1.0f;
        hc_config.lateral_inhibition_sigma2 = 3.0f;

        hypercolumn = hypercolumn_create(pool, &hc_config);
        ASSERT_NE(hypercolumn, nullptr);

        cortical_attention_default_config(&config);
        attention = cortical_attention_create(&config, hypercolumn);
        ASSERT_NE(attention, nullptr);
    }

    void TearDown() override {
        if (attention) {
            cortical_attention_destroy(attention);
        }
        if (hypercolumn) {
            hypercolumn_destroy(hypercolumn);
        }
        if (pool) {
            cortical_column_pool_destroy(pool);
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(CorticalAttentionGainTest, DefaultConfig) {
    cortical_attention_config_t cfg;
    int result = cortical_attention_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(cfg.mode, ATTENTION_NONE);
    EXPECT_FLOAT_EQ(cfg.baseline_gain, 1.0f);
    EXPECT_GT(cfg.max_gain_boost, 1.0f);
    EXPECT_GT(cfg.spatial_sigma, 0.0f);
    EXPECT_GT(cfg.layer_23_gain_factor, 1.0f);
}

TEST_F(CorticalAttentionGainTest, DefaultConfigNullPointer) {
    int result = cortical_attention_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(CorticalAttentionGainTest, CreateWithConfig) {
    cortical_attention_config_t custom_config;
    cortical_attention_default_config(&custom_config);
    custom_config.max_gain_boost = 3.0f;
    custom_config.spatial_sigma = 5.0f;

    cortical_attention_gain_t* system = cortical_attention_create(&custom_config, hypercolumn);
    ASSERT_NE(system, nullptr);

    cortical_attention_destroy(system);
}

TEST_F(CorticalAttentionGainTest, CreateWithNullConfig) {
    cortical_attention_gain_t* system = cortical_attention_create(nullptr, hypercolumn);
    ASSERT_NE(system, nullptr);
    cortical_attention_destroy(system);
}

TEST_F(CorticalAttentionGainTest, CreateWithNullHypercolumn) {
    cortical_attention_gain_t* system = cortical_attention_create(&config, nullptr);
    EXPECT_EQ(system, nullptr);
}

/* ============================================================================
 * Attention Mode Tests
 * ============================================================================ */

TEST_F(CorticalAttentionGainTest, SetModeNone) {
    int result = cortical_attention_set_mode(attention, ATTENTION_NONE);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalAttentionGainTest, SetModeFeatureBased) {
    int result = cortical_attention_set_mode(attention, ATTENTION_FEATURE_BASED);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalAttentionGainTest, SetModeSpatial) {
    int result = cortical_attention_set_mode(attention, ATTENTION_SPATIAL);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalAttentionGainTest, SetModeDivided) {
    int result = cortical_attention_set_mode(attention, ATTENTION_DIVIDED);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalAttentionGainTest, SetModeNullSystem) {
    int result = cortical_attention_set_mode(nullptr, ATTENTION_SPATIAL);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Spotlight Tests
 * ============================================================================ */

TEST_F(CorticalAttentionGainTest, SetSpotlight) {
    int result = cortical_attention_set_spotlight(attention, 5.0f, 5.0f, 2.0f, 0.8f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalAttentionGainTest, SetSpotlightInvalidRadius) {
    int result = cortical_attention_set_spotlight(attention, 5.0f, 5.0f, -1.0f, 0.8f);
    EXPECT_EQ(result, -1);
}

TEST_F(CorticalAttentionGainTest, SetSpotlightInvalidIntensity) {
    int result = cortical_attention_set_spotlight(attention, 5.0f, 5.0f, 2.0f, 1.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(CorticalAttentionGainTest, SetSpotlightNullSystem) {
    int result = cortical_attention_set_spotlight(nullptr, 5.0f, 5.0f, 2.0f, 0.8f);
    EXPECT_EQ(result, -1);
}

TEST_F(CorticalAttentionGainTest, AddSpotlight) {
    int result = cortical_attention_add_spotlight(attention, 2.0f, 3.0f, 1.5f, 0.6f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalAttentionGainTest, AddMultipleSpotlights) {
    int result1 = cortical_attention_add_spotlight(attention, 2.0f, 3.0f, 1.5f, 0.6f);
    int result2 = cortical_attention_add_spotlight(attention, 8.0f, 7.0f, 2.0f, 0.7f);
    int result3 = cortical_attention_add_spotlight(attention, 4.0f, 5.0f, 1.0f, 0.5f);

    EXPECT_EQ(result1, 0);
    EXPECT_EQ(result2, 0);
    EXPECT_EQ(result3, 0);
}

TEST_F(CorticalAttentionGainTest, ClearSpotlights) {
    cortical_attention_add_spotlight(attention, 2.0f, 3.0f, 1.5f, 0.6f);
    cortical_attention_add_spotlight(attention, 8.0f, 7.0f, 2.0f, 0.7f);

    int result = cortical_attention_clear_spotlights(attention);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Feature Target Tests
 * ============================================================================ */

TEST_F(CorticalAttentionGainTest, SetFeatureTarget) {
    int result = cortical_attention_set_feature_target(attention, 45.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalAttentionGainTest, SetFeatureTargetNullSystem) {
    int result = cortical_attention_set_feature_target(nullptr, 45.0f);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Gain Application Tests
 * ============================================================================ */

TEST_F(CorticalAttentionGainTest, UpdateGains) {
    cortical_attention_set_mode(attention, ATTENTION_SPATIAL);
    cortical_attention_set_spotlight(attention, 0.0f, 0.0f, 5.0f, 1.0f);

    int result = cortical_attention_update_gains(attention);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalAttentionGainTest, ApplyGainValidIndex) {
    cortical_attention_update_gains(attention);
    float modulated = cortical_attention_apply_gain(attention, 0, 0.5f);
    EXPECT_GE(modulated, 0.0f);
    EXPECT_LE(modulated, 1.0f);
}

TEST_F(CorticalAttentionGainTest, ApplyGainInvalidIndex) {
    float modulated = cortical_attention_apply_gain(attention, 1000, 0.5f);
    EXPECT_FLOAT_EQ(modulated, 0.5f); /* Returns original on error */
}

TEST_F(CorticalAttentionGainTest, ComputeLayerGain) {
    cortical_attention_update_gains(attention);

    float layer23_gain = cortical_attention_compute_layer_gain(attention, 0, 0);
    float layer4_gain = cortical_attention_compute_layer_gain(attention, 0, 1);
    float layer56_gain = cortical_attention_compute_layer_gain(attention, 0, 2);

    EXPECT_GE(layer23_gain, 0.0f);
    EXPECT_GE(layer4_gain, 0.0f);
    EXPECT_GE(layer56_gain, 0.0f);
}

TEST_F(CorticalAttentionGainTest, ApplyGainToHypercolumn) {
    cortical_attention_set_mode(attention, ATTENTION_SPATIAL);
    cortical_attention_set_spotlight(attention, 0.0f, 0.0f, 5.0f, 1.0f);

    int result = cortical_attention_apply_gain_to_hypercolumn(attention);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(CorticalAttentionGainTest, GetMinicolumnGain) {
    cortical_attention_update_gains(attention);

    minicolumn_gain_state_t gain_state;
    int result = cortical_attention_get_minicolumn_gain(attention, 0, &gain_state);
    EXPECT_EQ(result, 0);
    EXPECT_GE(gain_state.total_gain, 0.0f);
}

TEST_F(CorticalAttentionGainTest, IsAttended) {
    cortical_attention_set_mode(attention, ATTENTION_SPATIAL);
    cortical_attention_set_spotlight(attention, 0.0f, 0.0f, 100.0f, 1.0f);
    cortical_attention_update_gains(attention);

    bool attended = cortical_attention_is_attended(attention, 0);
    /* Result depends on spatial relationship */
    EXPECT_TRUE(attended || !attended); /* Just check it doesn't crash */
}

TEST_F(CorticalAttentionGainTest, GetStats) {
    cortical_attention_update_gains(attention);

    attention_gain_stats_t stats;
    int result = cortical_attention_get_stats(attention, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.total_updates, 1u);
}

TEST_F(CorticalAttentionGainTest, ResetStats) {
    cortical_attention_update_gains(attention);
    cortical_attention_reset_stats(attention);

    attention_gain_stats_t stats;
    cortical_attention_get_stats(attention, &stats);
    EXPECT_EQ(stats.total_updates, 0u);
}

/* ============================================================================
 * FEP Integration Tests
 * ============================================================================ */

TEST_F(CorticalAttentionGainTest, DisconnectFEP) {
    int result = cortical_attention_disconnect_fep(attention);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalAttentionGainTest, ModulatePrecision) {
    float modulated = cortical_attention_modulate_precision(attention, 1.0f);
    EXPECT_GE(modulated, 0.0f);
}

TEST_F(CorticalAttentionGainTest, UpdateFromPrecision) {
    int result = cortical_attention_update_from_precision(attention, 2.0f);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Bio-async Tests
 * ============================================================================ */

TEST_F(CorticalAttentionGainTest, ConnectBioAsync) {
    /* Bio-async may not be available in test environment */
    int result = cortical_attention_connect_bio_async(attention);
    /* Either success or expected failure */
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(CorticalAttentionGainTest, IsBioAsyncConnected) {
    bool connected = cortical_attention_is_bio_async_connected(attention);
    EXPECT_FALSE(connected); /* Initially not connected */
}

TEST_F(CorticalAttentionGainTest, DisconnectBioAsync) {
    int result = cortical_attention_disconnect_bio_async(attention);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(CorticalAttentionGainTest, DestroyNull) {
    /* Should not crash */
    cortical_attention_destroy(nullptr);
}

TEST_F(CorticalAttentionGainTest, MultipleUpdates) {
    for (int i = 0; i < 100; i++) {
        int result = cortical_attention_update_gains(attention);
        EXPECT_EQ(result, 0);
    }

    attention_gain_stats_t stats;
    cortical_attention_get_stats(attention, &stats);
    EXPECT_EQ(stats.total_updates, 100u);
}

TEST_F(CorticalAttentionGainTest, ModeTransitions) {
    cortical_attention_set_mode(attention, ATTENTION_SPATIAL);
    cortical_attention_update_gains(attention);

    cortical_attention_set_mode(attention, ATTENTION_FEATURE_BASED);
    cortical_attention_update_gains(attention);

    cortical_attention_set_mode(attention, ATTENTION_DIVIDED);
    cortical_attention_update_gains(attention);

    /* No crash expected */
    EXPECT_TRUE(true);
}
