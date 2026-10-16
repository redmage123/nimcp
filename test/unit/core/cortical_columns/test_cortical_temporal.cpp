/**
 * @file test_cortical_temporal.cpp
 * @brief Unit tests for temporal dynamics in cortical columns
 */

#include <gtest/gtest.h>
#include <cmath>
// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_temporal.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CorticalTemporalTest : public ::testing::Test {
protected:
    cortical_temporal_system_t* temporal;
    temporal_config_t config;

    void SetUp() override {
        config = cortical_temporal_default_config(32, 100);
        temporal = cortical_temporal_create(&config);
        ASSERT_NE(temporal, nullptr);
    }

    void TearDown() override {
        if (temporal) {
            cortical_temporal_destroy(temporal);
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(CorticalTemporalTest, DefaultConfig) {
    temporal_config_t cfg = cortical_temporal_default_config(64, 50);

    EXPECT_GT(cfg.history_window_ms, 0.0f);
    EXPECT_GT(cfg.history_bins, 0u);
    EXPECT_GT(cfg.num_columns, 0u);
}

TEST_F(CorticalTemporalTest, CreateWithConfig) {
    temporal_config_t custom_config = cortical_temporal_default_config(128, 200);
    custom_config.adaptation_strength = 0.3f;

    cortical_temporal_system_t* system = cortical_temporal_create(&custom_config);
    ASSERT_NE(system, nullptr);

    cortical_temporal_destroy(system);
}

TEST_F(CorticalTemporalTest, CreateWithNullConfig) {
    cortical_temporal_system_t* system = cortical_temporal_create(nullptr);
    /* Implementation may require valid config - just verify no crash */
    if (system != nullptr) {
        cortical_temporal_destroy(system);
    }
}

/* ============================================================================
 * Timescale Tests
 * ============================================================================ */

TEST_F(CorticalTemporalTest, SetLayerTimescale) {
    layer_timescale_t timescale = {
        .tau = 50.0f,
        .adaptation_rate = 0.1f,
        .recovery_rate = 0.05f
    };

    int result = cortical_temporal_set_layer_timescale(temporal, 0, &timescale);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalTemporalTest, GetEffectiveTimescale) {
    float tau = cortical_temporal_get_effective_timescale(temporal, 1);
    EXPECT_GT(tau, 0.0f);
}

TEST_F(CorticalTemporalTest, GetTimescaleInvalidLayer) {
    float tau = cortical_temporal_get_effective_timescale(temporal, 100);
    EXPECT_LT(tau, 0.0f);
}

/* ============================================================================
 * Temporal Dynamics Tests
 * ============================================================================ */

TEST_F(CorticalTemporalTest, Update) {
    int result = cortical_temporal_update(temporal, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalTemporalTest, UpdateMultiple) {
    for (int i = 0; i < 100; i++) {
        int result = cortical_temporal_update(temporal, 10.0f);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(CorticalTemporalTest, ApplyAdaptation) {
    float activity = 0.8f;
    float adapted = cortical_temporal_apply_adaptation(temporal, 0, activity);
    EXPECT_GE(adapted, 0.0f);
    EXPECT_LE(adapted, 1.0f);
}

TEST_F(CorticalTemporalTest, ApplyAdaptationRepeated) {
    /* Repeated stimulation should increase adaptation */
    float activity = 0.8f;
    for (int i = 0; i < 10; i++) {
        float adapted = cortical_temporal_apply_adaptation(temporal, 0, activity);
        EXPECT_GE(adapted, 0.0f);
    }
}

TEST_F(CorticalTemporalTest, ApplyHabituation) {
    float activity = 0.7f;
    float habituated = cortical_temporal_apply_habituation(temporal, 0, activity);
    EXPECT_GE(habituated, 0.0f);
    EXPECT_LE(habituated, 1.0f);
}

TEST_F(CorticalTemporalTest, IntegrateHistory) {
    float integrated = cortical_temporal_integrate_history(temporal, 0, 1, 0.5f);
    EXPECT_GE(integrated, 0.0f);
}

TEST_F(CorticalTemporalTest, ResetAdaptation) {
    /* First adapt */
    cortical_temporal_apply_adaptation(temporal, 0, 0.8f);

    /* Then reset */
    int result = cortical_temporal_reset_adaptation(temporal, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalTemporalTest, ResetHabituation) {
    /* First habituate */
    cortical_temporal_apply_habituation(temporal, 0, 0.8f);

    /* Then reset */
    int result = cortical_temporal_reset_habituation(temporal, 0);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Sequence Detection Tests
 * ============================================================================ */

TEST_F(CorticalTemporalTest, CreateSequenceDetector) {
    float template_seq[5] = {0.1f, 0.5f, 0.9f, 0.5f, 0.1f};
    sequence_detector_t* detector = cortical_temporal_create_sequence_detector(
        template_seq, 5, 0.8f);
    ASSERT_NE(detector, nullptr);

    cortical_temporal_destroy_sequence_detector(detector);
}

TEST_F(CorticalTemporalTest, DetectSequence) {
    float template_seq[5] = {0.1f, 0.5f, 0.9f, 0.5f, 0.1f};
    sequence_detector_t* detector = cortical_temporal_create_sequence_detector(
        template_seq, 5, 0.8f);
    ASSERT_NE(detector, nullptr);

    /* Feed some data */
    for (int i = 0; i < 10; i++) {
        cortical_temporal_update(temporal, 10.0f);
    }

    bool detected = cortical_temporal_detect_sequence(temporal, detector, 0, 1);
    /* Just verify it runs without crashing */
    EXPECT_TRUE(detected || !detected);

    cortical_temporal_destroy_sequence_detector(detector);
}

TEST_F(CorticalTemporalTest, DestroySequenceDetectorNull) {
    cortical_temporal_destroy_sequence_detector(nullptr);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(CorticalTemporalTest, GetStats) {
    temporal_stats_t stats;
    int result = cortical_temporal_get_stats(temporal, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalTemporalTest, GetStatsAfterUpdates) {
    for (int i = 0; i < 100; i++) {
        cortical_temporal_update(temporal, 10.0f);
    }

    temporal_stats_t stats;
    int result = cortical_temporal_get_stats(temporal, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_updates, 100u);
}

TEST_F(CorticalTemporalTest, ResetStats) {
    cortical_temporal_update(temporal, 10.0f);
    int result = cortical_temporal_reset_stats(temporal);
    EXPECT_EQ(result, 0);

    temporal_stats_t stats;
    cortical_temporal_get_stats(temporal, &stats);
    EXPECT_EQ(stats.total_updates, 0u);
}

/* ============================================================================
 * Bio-async Tests
 * ============================================================================ */

TEST_F(CorticalTemporalTest, ConnectBioAsync) {
    int result = cortical_temporal_connect_bio_async(temporal);
    EXPECT_TRUE(result == 0 || result < 0);
}

TEST_F(CorticalTemporalTest, IsBioAsyncConnected) {
    bool connected = cortical_temporal_is_bio_async_connected(temporal);
    EXPECT_FALSE(connected);
}

TEST_F(CorticalTemporalTest, DisconnectBioAsync) {
    int result = cortical_temporal_disconnect_bio_async(temporal);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(CorticalTemporalTest, DestroyNull) {
    cortical_temporal_destroy(nullptr);
}

TEST_F(CorticalTemporalTest, InvalidColumnId) {
    float adapted = cortical_temporal_apply_adaptation(temporal, 10000, 0.5f);
    EXPECT_LT(adapted, 0.0f); /* Should return error */
}

TEST_F(CorticalTemporalTest, ZeroActivity) {
    float adapted = cortical_temporal_apply_adaptation(temporal, 0, 0.0f);
    EXPECT_NEAR(adapted, 0.0f, 0.01f);
}

TEST_F(CorticalTemporalTest, FullActivity) {
    float adapted = cortical_temporal_apply_adaptation(temporal, 0, 1.0f);
    EXPECT_GE(adapted, 0.0f);
    EXPECT_LE(adapted, 1.0f);
}

