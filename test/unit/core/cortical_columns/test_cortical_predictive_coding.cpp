/**
 * @file test_cortical_predictive_coding.cpp
 * @brief Unit tests for hierarchical predictive coding in cortical columns
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_predictive_coding.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CorticalPredictiveCodingTest : public ::testing::Test {
protected:
    cortical_predictive_t* pc;
    predictive_config_t config;

    void SetUp() override {
        cortical_predictive_default_config(&config);
        config.hierarchy_depth = 3;
        pc = cortical_predictive_create(&config);
        ASSERT_NE(pc, nullptr);
    }

    void TearDown() override {
        if (pc) {
            cortical_predictive_destroy(pc);
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(CorticalPredictiveCodingTest, DefaultConfig) {
    predictive_config_t cfg;
    int result = cortical_predictive_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.prediction_learning_rate, 0.0f);
    EXPECT_GT(cfg.precision_learning_rate, 0.0f);
    EXPECT_GT(cfg.error_gain, 0.0f);
    EXPECT_GE(cfg.hierarchy_depth, 1u);
}

TEST_F(CorticalPredictiveCodingTest, DefaultConfigNullPointer) {
    int result = cortical_predictive_default_config(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(CorticalPredictiveCodingTest, CreateWithConfig) {
    predictive_config_t custom_config;
    cortical_predictive_default_config(&custom_config);
    custom_config.prediction_learning_rate = 0.05f;
    custom_config.enable_precision_weighting = true;

    cortical_predictive_t* system = cortical_predictive_create(&custom_config);
    ASSERT_NE(system, nullptr);

    cortical_predictive_destroy(system);
}

TEST_F(CorticalPredictiveCodingTest, CreateWithNullConfig) {
    cortical_predictive_t* system = cortical_predictive_create(nullptr);
    ASSERT_NE(system, nullptr);
    cortical_predictive_destroy(system);
}

/* ============================================================================
 * Hierarchy Construction Tests
 * ============================================================================ */

TEST_F(CorticalPredictiveCodingTest, AddLevel) {
    int result = cortical_predictive_add_level(pc, 64, 32);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalPredictiveCodingTest, AddMultipleLevels) {
    int result1 = cortical_predictive_add_level(pc, 128, 64);
    int result2 = cortical_predictive_add_level(pc, 64, 32);
    int result3 = cortical_predictive_add_level(pc, 32, 16);

    EXPECT_EQ(result1, 0);
    EXPECT_EQ(result2, 0);
    EXPECT_EQ(result3, 0);
}

TEST_F(CorticalPredictiveCodingTest, AddLevelNullSystem) {
    int result = cortical_predictive_add_level(nullptr, 64, 32);
    EXPECT_NE(result, 0);
}

/* ============================================================================
 * Prediction and Error Computation Tests
 * ============================================================================ */

TEST_F(CorticalPredictiveCodingTest, ComputePrediction) {
    cortical_predictive_add_level(pc, 64, 32);

    float predictions[64];
    int result = cortical_predictive_compute_prediction(pc, 0, predictions, 64);
    EXPECT_GE(result, 0);
}

TEST_F(CorticalPredictiveCodingTest, ComputeError) {
    cortical_predictive_add_level(pc, 64, 32);

    float observation[64] = {0};
    for (int i = 0; i < 64; i++) {
        observation[i] = 0.5f;
    }

    float errors[64];
    int result = cortical_predictive_compute_error(pc, 0, observation, 64, errors, 64);
    EXPECT_GE(result, 0);
}

TEST_F(CorticalPredictiveCodingTest, WeightByPrecision) {
    cortical_predictive_add_level(pc, 64, 32);

    float errors[64];
    float weighted[64];
    for (int i = 0; i < 64; i++) {
        errors[i] = 0.1f;
    }

    int result = cortical_predictive_weight_by_precision(pc, 0, errors, 64, weighted, 64);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Message Passing Tests
 * ============================================================================ */

TEST_F(CorticalPredictiveCodingTest, PropagateUp) {
    cortical_predictive_add_level(pc, 128, 64);
    cortical_predictive_add_level(pc, 64, 32);

    int result = cortical_predictive_propagate_up(pc, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalPredictiveCodingTest, PropagateDown) {
    cortical_predictive_add_level(pc, 128, 64);
    cortical_predictive_add_level(pc, 64, 32);

    int result = cortical_predictive_propagate_down(pc, 1);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Learning Tests
 * ============================================================================ */

TEST_F(CorticalPredictiveCodingTest, UpdatePredictions) {
    cortical_predictive_add_level(pc, 64, 32);

    int result = cortical_predictive_update_predictions(pc, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalPredictiveCodingTest, UpdatePrecisions) {
    cortical_predictive_add_level(pc, 64, 32);

    int result = cortical_predictive_update_precisions(pc, 0);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Free Energy Tests
 * ============================================================================ */

TEST_F(CorticalPredictiveCodingTest, ComputeFreeEnergy) {
    cortical_predictive_add_level(pc, 64, 32);

    float free_energy;
    int result = cortical_predictive_compute_free_energy(pc, &free_energy);
    EXPECT_EQ(result, 0);
    EXPECT_GE(free_energy, 0.0f);
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(CorticalPredictiveCodingTest, GetStats) {
    cortical_predictive_add_level(pc, 64, 32);
    cortical_predictive_update_predictions(pc, 0);

    predictive_stats_t stats;
    int result = cortical_predictive_get_stats(pc, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalPredictiveCodingTest, GetPredictions) {
    cortical_predictive_add_level(pc, 64, 32);

    float predictions[64];
    int result = cortical_predictive_get_predictions(pc, 0, predictions, 64);
    EXPECT_GE(result, 0);
}

TEST_F(CorticalPredictiveCodingTest, GetErrors) {
    cortical_predictive_add_level(pc, 64, 32);

    float errors[64];
    int result = cortical_predictive_get_errors(pc, 0, errors, 64);
    EXPECT_GE(result, 0);
}

TEST_F(CorticalPredictiveCodingTest, GetPrecisions) {
    cortical_predictive_add_level(pc, 64, 32);

    float precisions[64];
    int result = cortical_predictive_get_precisions(pc, 0, precisions, 64);
    EXPECT_GE(result, 0);
}

/* ============================================================================
 * Bio-async Tests
 * ============================================================================ */

TEST_F(CorticalPredictiveCodingTest, ConnectBioAsync) {
    int result = cortical_predictive_connect_bio_async(pc);
    EXPECT_TRUE(result == 0 || result < 0);
}

TEST_F(CorticalPredictiveCodingTest, IsBioAsyncConnected) {
    bool connected = cortical_predictive_is_bio_async_connected(pc);
    EXPECT_FALSE(connected);
}

TEST_F(CorticalPredictiveCodingTest, DisconnectBioAsync) {
    int result = cortical_predictive_disconnect_bio_async(pc);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(CorticalPredictiveCodingTest, DestroyNull) {
    cortical_predictive_destroy(nullptr);
}

TEST_F(CorticalPredictiveCodingTest, MultipleIterations) {
    cortical_predictive_add_level(pc, 64, 32);
    cortical_predictive_add_level(pc, 32, 16);

    for (int i = 0; i < 100; i++) {
        cortical_predictive_propagate_up(pc, 0);
        cortical_predictive_update_predictions(pc, 1);
        cortical_predictive_propagate_down(pc, 1);
        cortical_predictive_update_precisions(pc, 0);
    }

    float free_energy;
    cortical_predictive_compute_free_energy(pc, &free_energy);
    EXPECT_GE(free_energy, 0.0f);
}
