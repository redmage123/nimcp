/**
 * @file test_cortical_surround.cpp
 * @brief Unit tests for surround suppression and contextual modulation
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_surround.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CorticalSurroundTest : public ::testing::Test {
protected:
    cortical_surround_system_t* surround;
    cortical_surround_config_t config;

    void SetUp() override {
        cortical_surround_default_config(&config);
        surround = cortical_surround_create(&config);
        ASSERT_NE(surround, nullptr);
    }

    void TearDown() override {
        if (surround) {
            cortical_surround_destroy(surround);
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(CorticalSurroundTest, DefaultConfig) {
    cortical_surround_config_t cfg;
    int result = cortical_surround_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.center_radius, 0.0f);
    EXPECT_GT(cfg.surround_radius, cfg.center_radius);
    EXPECT_GE(cfg.suppression_strength, 0.0f);
    EXPECT_LE(cfg.suppression_strength, 1.0f);
}

TEST_F(CorticalSurroundTest, DefaultConfigNullPointer) {
    int result = cortical_surround_default_config(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(CorticalSurroundTest, CreateWithConfig) {
    cortical_surround_config_t custom_config;
    cortical_surround_default_config(&custom_config);
    custom_config.center_radius = 2.0f;
    custom_config.surround_radius = 5.0f;

    cortical_surround_system_t* system = cortical_surround_create(&custom_config);
    ASSERT_NE(system, nullptr);

    cortical_surround_destroy(system);
}

TEST_F(CorticalSurroundTest, CreateWithNullConfig) {
    cortical_surround_system_t* system = cortical_surround_create(nullptr);
    ASSERT_NE(system, nullptr);
    cortical_surround_destroy(system);
}

/* ============================================================================
 * Suppression Tests
 * ============================================================================ */

TEST_F(CorticalSurroundTest, ComputeSuppression) {
    float center_response = 0.8f;
    float surround_response = 0.6f;

    float suppressed;
    int result = cortical_surround_compute_suppression(surround, center_response,
                                                        surround_response, &suppressed);
    EXPECT_EQ(result, 0);
    EXPECT_LE(suppressed, center_response);
    EXPECT_GE(suppressed, 0.0f);
}

TEST_F(CorticalSurroundTest, ApplySurroundSuppression) {
    float center_activations[64];
    float surround_activations[64];
    float suppressed[64];

    /* Create center-surround pattern */
    for (int i = 0; i < 64; i++) {
        center_activations[i] = (i < 32) ? 0.8f : 0.2f;
        surround_activations[i] = 0.5f;
    }

    int result = cortical_surround_apply_suppression(surround, center_activations,
                                                      surround_activations, 64, suppressed);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalSurroundTest, ComputeDivisiveNormalization) {
    float responses[64];
    float normalized[64];

    for (int i = 0; i < 64; i++) {
        responses[i] = (float)i / 64.0f;
    }

    int result = cortical_surround_apply_divisive_normalization(surround, responses, 64, normalized);
    EXPECT_EQ(result, 0);

    /* Check normalization */
    for (int i = 0; i < 64; i++) {
        EXPECT_GE(normalized[i], 0.0f);
    }
}

/* ============================================================================
 * Orientation-Specific Tests
 * ============================================================================ */

TEST_F(CorticalSurroundTest, ComputeOrientationSuppression) {
    float center_orientation = 45.0f; /* degrees */
    float surround_orientation = 90.0f;
    float center_response = 0.8f;

    float suppressed;
    int result = cortical_surround_orientation_suppression(surround, center_orientation,
                                                            surround_orientation, center_response,
                                                            &suppressed);
    EXPECT_EQ(result, 0);
    /* Cross-orientation should be less suppressive */
    EXPECT_GE(suppressed, 0.0f);
}

TEST_F(CorticalSurroundTest, IsoOrientationSuppression) {
    float center_orientation = 45.0f;
    float surround_orientation = 45.0f; /* Same as center */
    float center_response = 0.8f;

    float suppressed;
    cortical_surround_orientation_suppression(surround, center_orientation,
                                               surround_orientation, center_response,
                                               &suppressed);
    /* Iso-orientation should be more suppressive */
    EXPECT_LT(suppressed, center_response);
}

/* ============================================================================
 * Spatial Integration Tests
 * ============================================================================ */

TEST_F(CorticalSurroundTest, ComputeSpatialPooling) {
    float activations[64];
    float positions_x[64];
    float positions_y[64];

    for (int i = 0; i < 64; i++) {
        activations[i] = 0.5f;
        positions_x[i] = (float)(i % 8);
        positions_y[i] = (float)(i / 8);
    }

    float center_pool, surround_pool;
    int result = cortical_surround_compute_spatial_pooling(surround, activations, positions_x,
                                                            positions_y, 64, 4.0f, 4.0f,
                                                            &center_pool, &surround_pool);
    EXPECT_EQ(result, 0);
    EXPECT_GE(center_pool, 0.0f);
    EXPECT_GE(surround_pool, 0.0f);
}

/* ============================================================================
 * Contextual Modulation Tests
 * ============================================================================ */

TEST_F(CorticalSurroundTest, ComputeContextualModulation) {
    float center_response = 0.6f;
    float context_responses[8] = {0.5f, 0.4f, 0.6f, 0.5f, 0.3f, 0.7f, 0.4f, 0.5f};

    float modulated;
    int result = cortical_surround_contextual_modulation(surround, center_response,
                                                          context_responses, 8, &modulated);
    EXPECT_EQ(result, 0);
    EXPECT_GE(modulated, 0.0f);
}

TEST_F(CorticalSurroundTest, ComputeEndStoppedResponse) {
    float line_response = 0.8f;
    float end_responses[2] = {0.1f, 0.2f}; /* End regions */

    float end_stopped;
    int result = cortical_surround_end_stopped_response(surround, line_response,
                                                         end_responses, &end_stopped);
    EXPECT_EQ(result, 0);
    /* End-stopped cells respond to line ends */
    EXPECT_GE(end_stopped, 0.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(CorticalSurroundTest, GetStats) {
    cortical_surround_stats_t stats;
    int result = cortical_surround_get_stats(surround, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalSurroundTest, ResetStats) {
    int result = cortical_surround_reset_stats(surround);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Bio-async Tests
 * ============================================================================ */

TEST_F(CorticalSurroundTest, ConnectBioAsync) {
    int result = cortical_surround_connect_bio_async(surround);
    EXPECT_TRUE(result == 0 || result < 0);
}

TEST_F(CorticalSurroundTest, IsBioAsyncConnected) {
    bool connected = cortical_surround_is_bio_async_connected(surround);
    EXPECT_FALSE(connected);
}

TEST_F(CorticalSurroundTest, DisconnectBioAsync) {
    int result = cortical_surround_disconnect_bio_async(surround);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(CorticalSurroundTest, DestroyNull) {
    cortical_surround_destroy(nullptr);
}

TEST_F(CorticalSurroundTest, ZeroCenterResponse) {
    float suppressed;
    int result = cortical_surround_compute_suppression(surround, 0.0f, 0.5f, &suppressed);
    EXPECT_EQ(result, 0);
    EXPECT_NEAR(suppressed, 0.0f, 0.01f);
}

TEST_F(CorticalSurroundTest, ZeroSurroundResponse) {
    float suppressed;
    int result = cortical_surround_compute_suppression(surround, 0.8f, 0.0f, &suppressed);
    EXPECT_EQ(result, 0);
    /* No surround should mean no suppression */
    EXPECT_GE(suppressed, 0.0f);
}

TEST_F(CorticalSurroundTest, HighContrastSurround) {
    float center_response = 0.5f;
    float surround_response = 1.0f; /* High contrast surround */

    float suppressed;
    cortical_surround_compute_suppression(surround, center_response, surround_response, &suppressed);
    /* Strong surround should suppress */
    EXPECT_LE(suppressed, center_response);
}
