/**
 * @file test_biological_timescales_fep_bridge.cpp
 * @brief Unit tests for Biological Timescales-FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Biological Timescales bidirectional integration
 * WHY:  Ensure temporal precision weighting and multi-scale predictions work correctly
 * HOW:  Test lifecycle, temporal prediction, precision modulation, and effects updates
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "async/nimcp_biological_timescales_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class BiologicalTimescalesFepBridgeTest : public ::testing::Test {
protected:
    biological_timescales_fep_bridge_t* bridge = nullptr;
    fep_system_t* fep = nullptr;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, 8, 4);
        ASSERT_NE(fep, nullptr);

        /* Create bridge */
        biological_timescales_fep_config_t config;
        biological_timescales_fep_default_config(&config);
        bridge = biological_timescales_fep_create(&config, fep);
    }

    void TearDown() override {
        if (bridge) {
            biological_timescales_fep_destroy(bridge);
            bridge = nullptr;
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

TEST_F(BiologicalTimescalesFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(BiologicalTimescalesFepBridgeTest, CreateWithNullConfig) {
    biological_timescales_fep_bridge_t* br =
        biological_timescales_fep_create(nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(BiologicalTimescalesFepBridgeTest, CreateWithNullFep) {
    biological_timescales_fep_config_t config;
    biological_timescales_fep_default_config(&config);
    biological_timescales_fep_bridge_t* br =
        biological_timescales_fep_create(&config, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(BiologicalTimescalesFepBridgeTest, DestroyNull) {
    biological_timescales_fep_destroy(nullptr);  /* Should not crash */
}

TEST_F(BiologicalTimescalesFepBridgeTest, DefaultConfig) {
    biological_timescales_fep_config_t config;
    int ret = biological_timescales_fep_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_hierarchical_timing);
    EXPECT_TRUE(config.enable_precision_learning);
    EXPECT_TRUE(config.enable_temporal_prediction);
    EXPECT_GT(config.learning_rate, 0.0f);
}

TEST_F(BiologicalTimescalesFepBridgeTest, DefaultConfigNullPtr) {
    int ret = biological_timescales_fep_default_config(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Effects Update Tests
 * ============================================================================ */

TEST_F(BiologicalTimescalesFepBridgeTest, UpdateEffects) {
    int ret = biological_timescales_fep_update_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(BiologicalTimescalesFepBridgeTest, UpdateEffectsNull) {
    EXPECT_NE(biological_timescales_fep_update_effects(nullptr), 0);
}

TEST_F(BiologicalTimescalesFepBridgeTest, UpdateEffectsSelectsBand) {
    int ret = biological_timescales_fep_update_effects(bridge);
    EXPECT_EQ(ret, 0);

    biological_timescales_fep_effects_t effects;
    ret = biological_timescales_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    /* Selected band should be valid */
    EXPECT_GE((int)effects.selected_band, 0);
    EXPECT_LE((int)effects.selected_band, 4);
}

TEST_F(BiologicalTimescalesFepBridgeTest, UpdateEffectsSetsBandPreferences) {
    int ret = biological_timescales_fep_update_effects(bridge);
    EXPECT_EQ(ret, 0);

    biological_timescales_fep_effects_t effects;
    ret = biological_timescales_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    /* Band preferences should be valid probabilities */
    for (int i = 0; i < 5; i++) {
        EXPECT_GE(effects.band_preferences[i], 0.0f);
        EXPECT_LE(effects.band_preferences[i], 1.0f);
    }
}

/* ============================================================================
 * Temporal Prediction Tests
 * ============================================================================ */

TEST_F(BiologicalTimescalesFepBridgeTest, PredictInterval) {
    float predicted_ms;
    float precision;

    int ret = biological_timescales_fep_predict_timing(bridge, BIO_OSC_ALPHA, &predicted_ms, &precision);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(predicted_ms, 0.0f);
    EXPECT_GE(precision, 0.0f);
}

TEST_F(BiologicalTimescalesFepBridgeTest, PredictIntervalNull) {
    float predicted_ms;
    float precision;

    EXPECT_NE(biological_timescales_fep_predict_timing(nullptr, BIO_OSC_ALPHA, &predicted_ms, &precision), 0);
    EXPECT_NE(biological_timescales_fep_predict_timing(bridge, BIO_OSC_ALPHA, nullptr, &precision), 0);
    EXPECT_NE(biological_timescales_fep_predict_timing(bridge, BIO_OSC_ALPHA, &predicted_ms, nullptr), 0);
}

TEST_F(BiologicalTimescalesFepBridgeTest, SelectBandForPrecision) {
    nimcp_oscillation_band_t band = biological_timescales_fep_select_band(bridge, 0.8f);
    /* High precision should select fast band (gamma) */
    EXPECT_EQ(band, BIO_OSC_GAMMA);
}

TEST_F(BiologicalTimescalesFepBridgeTest, SelectBandForLowPrecision) {
    nimcp_oscillation_band_t band = biological_timescales_fep_select_band(bridge, 0.2f);
    /* Band should be valid (any band 0-4 is valid) */
    EXPECT_GE((int)band, (int)BIO_OSC_DELTA);
    EXPECT_LE((int)band, (int)BIO_OSC_GAMMA);
}

/* ============================================================================
 * Timing Observation Tests
 * ============================================================================ */

TEST_F(BiologicalTimescalesFepBridgeTest, ObserveTiming) {
    int ret = biological_timescales_fep_observe_timing(bridge, 50.0f, BIO_OSC_ALPHA);
    EXPECT_EQ(ret, 0);
}

TEST_F(BiologicalTimescalesFepBridgeTest, ObserveTimingNull) {
    EXPECT_NE(biological_timescales_fep_observe_timing(nullptr, 50.0f, BIO_OSC_ALPHA), 0);
}

TEST_F(BiologicalTimescalesFepBridgeTest, ObserveTimingUpdatesEffects) {
    biological_timescales_fep_observe_timing(bridge, 50.0f, BIO_OSC_ALPHA);

    fep_biological_timescales_effects_t effects;
    int ret = biological_timescales_fep_get_timescales_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    EXPECT_FLOAT_EQ(effects.observed_interval_ms, 50.0f);
    EXPECT_EQ(effects.active_band, BIO_OSC_ALPHA);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(BiologicalTimescalesFepBridgeTest, InitiallyNotConnected) {
    EXPECT_FALSE(biological_timescales_fep_is_bio_async_connected(bridge));
}

TEST_F(BiologicalTimescalesFepBridgeTest, ConnectDisconnectBioAsync) {
    int ret = biological_timescales_fep_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(biological_timescales_fep_is_bio_async_connected(bridge));

    ret = biological_timescales_fep_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(biological_timescales_fep_is_bio_async_connected(bridge));
}

TEST_F(BiologicalTimescalesFepBridgeTest, ConnectNull) {
    EXPECT_NE(biological_timescales_fep_connect_bio_async(nullptr), 0);
}

TEST_F(BiologicalTimescalesFepBridgeTest, DisconnectNull) {
    EXPECT_NE(biological_timescales_fep_disconnect_bio_async(nullptr), 0);
}

TEST_F(BiologicalTimescalesFepBridgeTest, IsConnectedNull) {
    EXPECT_FALSE(biological_timescales_fep_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(BiologicalTimescalesFepBridgeTest, GetEffects) {
    biological_timescales_fep_effects_t effects;
    int ret = biological_timescales_fep_get_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.temporal_resolution_ms, 0.0f);
}

TEST_F(BiologicalTimescalesFepBridgeTest, GetEffectsNull) {
    biological_timescales_fep_effects_t effects;

    EXPECT_NE(biological_timescales_fep_get_effects(nullptr, &effects), 0);
    EXPECT_NE(biological_timescales_fep_get_effects(bridge, nullptr), 0);
}

TEST_F(BiologicalTimescalesFepBridgeTest, GetTimescalesEffects) {
    fep_biological_timescales_effects_t effects;
    int ret = biological_timescales_fep_get_timescales_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.observed_interval_ms, 0.0f);
}

TEST_F(BiologicalTimescalesFepBridgeTest, GetTimescalesEffectsNull) {
    fep_biological_timescales_effects_t effects;

    EXPECT_NE(biological_timescales_fep_get_timescales_effects(nullptr, &effects), 0);
    EXPECT_NE(biological_timescales_fep_get_timescales_effects(bridge, nullptr), 0);
}

TEST_F(BiologicalTimescalesFepBridgeTest, GetStats) {
    biological_timescales_fep_stats_t stats;
    int ret = biological_timescales_fep_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.avg_precision, 0.0f);
}

TEST_F(BiologicalTimescalesFepBridgeTest, GetStatsNull) {
    biological_timescales_fep_stats_t stats;

    EXPECT_NE(biological_timescales_fep_get_stats(nullptr, &stats), 0);
    EXPECT_NE(biological_timescales_fep_get_stats(bridge, nullptr), 0);
}

TEST_F(BiologicalTimescalesFepBridgeTest, ResetStats) {
    biological_timescales_fep_observe_timing(bridge, 50.0f, BIO_OSC_ALPHA);

    int ret = biological_timescales_fep_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    biological_timescales_fep_stats_t stats;
    biological_timescales_fep_get_stats(bridge, &stats);

    EXPECT_EQ(stats.timing_violations, 0u);
}

TEST_F(BiologicalTimescalesFepBridgeTest, ResetStatsNull) {
    EXPECT_NE(biological_timescales_fep_reset_stats(nullptr), 0);
}

/* ============================================================================
 * Integration Behavior Tests
 * ============================================================================ */

TEST_F(BiologicalTimescalesFepBridgeTest, PrecisionPerBandComputed) {
    biological_timescales_fep_update_effects(bridge);

    biological_timescales_fep_effects_t effects;
    biological_timescales_fep_get_effects(bridge, &effects);

    /* Each band should have non-negative precision */
    for (int i = 0; i < 5; i++) {
        EXPECT_GE(effects.precision_per_band[i], 0.0f);
    }
}

TEST_F(BiologicalTimescalesFepBridgeTest, TimingSurpriseComputed) {
    /* First make a prediction */
    biological_timescales_fep_update_effects(bridge);

    /* Then observe timing that differs from prediction */
    biological_timescales_fep_observe_timing(bridge, 100.0f, BIO_OSC_DELTA);

    fep_biological_timescales_effects_t effects;
    biological_timescales_fep_get_timescales_effects(bridge, &effects);

    /* Surprise should be computed */
    EXPECT_GE(effects.timing_surprise, 0.0f);
}

TEST_F(BiologicalTimescalesFepBridgeTest, HierarchicalLevelToBandMapping) {
    /* Update to compute mappings */
    biological_timescales_fep_update_effects(bridge);

    biological_timescales_fep_effects_t effects;
    biological_timescales_fep_get_effects(bridge, &effects);

    /* Temporal resolution should be positive */
    EXPECT_GT(effects.temporal_resolution_ms, 0.0f);
}
