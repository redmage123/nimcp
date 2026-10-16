/**
 * @file test_parietal_training_regression.cpp
 * @brief Regression tests for Parietal-Training Bridge API stability
 * @date 2026-01-20
 *
 * These tests verify that the parietal-training bridge API remains stable
 * and that changes don't break existing functionality.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/parietal/nimcp_parietal_training_bridge.h"
}

class ParietalTrainingRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/* ============================================================================
 * API Signature Regression Tests
 * ============================================================================ */

TEST_F(ParietalTrainingRegressionTest, DefaultConfigFunctionSignature) {
    // Verify default_config accepts pointer and returns int
    parietal_training_config_t config;
    int result = parietal_training_default_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(ParietalTrainingRegressionTest, CreateFunctionSignature) {
    // Verify create accepts config, parietal, and bio_async pointers
    parietal_training_bridge_t* bridge = parietal_training_create(nullptr, nullptr, nullptr);
    // Should fail gracefully with null parietal
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(ParietalTrainingRegressionTest, DestroyFunctionSignature) {
    // Verify destroy accepts bridge pointer and handles null
    parietal_training_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Config Structure Regression Tests
 * ============================================================================ */

TEST_F(ParietalTrainingRegressionTest, ConfigStructureHasRequiredFields) {
    parietal_training_config_t config;
    parietal_training_default_config(&config);

    // Verify expected fields exist and are accessible
    EXPECT_GE(config.base_learning_rate, 0.0f);
    EXPECT_LE(config.base_learning_rate, 1.0f);

    // Boolean flags
    (void)config.register_with_training;
    (void)config.connect_to_plasticity;
    (void)config.enable_stdp_learning;
    (void)config.enable_batch_updates;
    (void)config.verbose_logging;
}

TEST_F(ParietalTrainingRegressionTest, DomainConfigArrayExists) {
    parietal_training_config_t config;
    parietal_training_default_config(&config);

    // Verify domains array is accessible
    for (int i = 0; i < PARIETAL_DOMAIN_COUNT; i++) {
        EXPECT_TRUE(config.domains[i].enabled || !config.domains[i].enabled);
        EXPECT_GE(config.domains[i].learning_rate, 0.0f);
    }
}

/* ============================================================================
 * Enum Value Regression Tests
 * ============================================================================ */

TEST_F(ParietalTrainingRegressionTest, DomainEnumValues) {
    // Verify domain enum values haven't changed
    EXPECT_EQ(PARIETAL_DOMAIN_COORDINATE_TRANSFORM, 0);
    EXPECT_EQ(PARIETAL_DOMAIN_SPATIAL_ATTENTION, 1);
    EXPECT_EQ(PARIETAL_DOMAIN_BODY_SCHEMA, 2);
    EXPECT_EQ(PARIETAL_DOMAIN_MOTOR_PLANNING, 3);
    EXPECT_EQ(PARIETAL_DOMAIN_OBJECT_MANIPULATION, 4);
    EXPECT_EQ(PARIETAL_DOMAIN_NUMBER_PROCESSING, 5);
    EXPECT_EQ(PARIETAL_DOMAIN_SPATIAL_MEMORY, 6);
    EXPECT_EQ(PARIETAL_DOMAIN_VISUOMOTOR, 7);
    EXPECT_EQ(PARIETAL_DOMAIN_COUNT, 8);
}

TEST_F(ParietalTrainingRegressionTest, StateEnumValues) {
    // Verify state enum values are stable
    EXPECT_EQ(PARIETAL_TRAIN_STATE_IDLE, 0);
    EXPECT_EQ(PARIETAL_TRAIN_STATE_ACTIVE, 1);
    EXPECT_EQ(PARIETAL_TRAIN_STATE_PAUSED, 2);
    EXPECT_EQ(PARIETAL_TRAIN_STATE_ERROR, 3);
}

TEST_F(ParietalTrainingRegressionTest, ResponseEnumValues) {
    // Verify response enum values are stable
    EXPECT_EQ(PARIETAL_TRAIN_RESPONSE_NONE, 0);
    EXPECT_EQ(PARIETAL_TRAIN_RESPONSE_UPDATE_WEIGHTS, 1);
    EXPECT_EQ(PARIETAL_TRAIN_RESPONSE_ADJUST_THRESHOLD, 2);
    EXPECT_EQ(PARIETAL_TRAIN_RESPONSE_MODULATE_GAIN, 3);
    EXPECT_EQ(PARIETAL_TRAIN_RESPONSE_CONSOLIDATE, 4);
}

/* ============================================================================
 * Signal Structure Regression Tests
 * ============================================================================ */

TEST_F(ParietalTrainingRegressionTest, SignalStructureFields) {
    parietal_learning_signal_t signal = {};

    // Verify all expected fields are accessible
    signal.domain = PARIETAL_DOMAIN_COORDINATE_TRANSFORM;
    signal.signal_strength = 0.5f;
    signal.error_gradient = 0.1f;
    signal.timestamp_us = 1000;
    signal.source_id = 1;
    signal.target_id = 2;

    EXPECT_EQ(signal.domain, PARIETAL_DOMAIN_COORDINATE_TRANSFORM);
    EXPECT_FLOAT_EQ(signal.signal_strength, 0.5f);
    EXPECT_FLOAT_EQ(signal.error_gradient, 0.1f);
}

/* ============================================================================
 * Stats Structure Regression Tests
 * ============================================================================ */

TEST_F(ParietalTrainingRegressionTest, StatsStructureFields) {
    parietal_training_stats_t stats = {};

    // Verify all expected fields are accessible
    stats.signals_processed = 10;
    stats.weight_updates = 5;
    stats.batch_flushes = 2;
    stats.total_learning_time_us = 1000;

    EXPECT_EQ(stats.signals_processed, 10u);
    EXPECT_EQ(stats.weight_updates, 5u);
    EXPECT_EQ(stats.batch_flushes, 2u);
}

/* ============================================================================
 * Utility Function Regression Tests
 * ============================================================================ */

TEST_F(ParietalTrainingRegressionTest, DomainNameFunction) {
    // Verify domain name function returns expected strings
    EXPECT_STREQ(parietal_training_domain_name(PARIETAL_DOMAIN_COORDINATE_TRANSFORM),
                 "coordinate_transform");
    EXPECT_STREQ(parietal_training_domain_name(PARIETAL_DOMAIN_SPATIAL_ATTENTION),
                 "spatial_attention");
    EXPECT_STREQ(parietal_training_domain_name(PARIETAL_DOMAIN_BODY_SCHEMA),
                 "body_schema");
    EXPECT_STREQ(parietal_training_domain_name(PARIETAL_DOMAIN_MOTOR_PLANNING),
                 "motor_planning");
}

TEST_F(ParietalTrainingRegressionTest, ResponseNameFunction) {
    // Verify response name function returns expected strings
    EXPECT_STREQ(parietal_training_response_name(PARIETAL_TRAIN_RESPONSE_NONE), "none");
    EXPECT_STREQ(parietal_training_response_name(PARIETAL_TRAIN_RESPONSE_UPDATE_WEIGHTS),
                 "update_weights");
    EXPECT_STREQ(parietal_training_response_name(PARIETAL_TRAIN_RESPONSE_ADJUST_THRESHOLD),
                 "adjust_threshold");
    EXPECT_STREQ(parietal_training_response_name(PARIETAL_TRAIN_RESPONSE_MODULATE_GAIN),
                 "modulate_gain");
    EXPECT_STREQ(parietal_training_response_name(PARIETAL_TRAIN_RESPONSE_CONSOLIDATE),
                 "consolidate");
}

TEST_F(ParietalTrainingRegressionTest, StateNameFunction) {
    // Verify state name function returns expected strings
    EXPECT_STREQ(parietal_training_state_name(PARIETAL_TRAIN_STATE_IDLE), "idle");
    EXPECT_STREQ(parietal_training_state_name(PARIETAL_TRAIN_STATE_ACTIVE), "active");
    EXPECT_STREQ(parietal_training_state_name(PARIETAL_TRAIN_STATE_PAUSED), "paused");
    EXPECT_STREQ(parietal_training_state_name(PARIETAL_TRAIN_STATE_ERROR), "error");
}

TEST_F(ParietalTrainingRegressionTest, VersionFunction) {
    const char* version = parietal_training_bridge_version();
    EXPECT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);

    // Version should contain a dot (e.g., "1.0.0")
    EXPECT_NE(strchr(version, '.'), nullptr);
}

/* ============================================================================
 * Error Handling Regression Tests
 * ============================================================================ */

TEST_F(ParietalTrainingRegressionTest, NullHandlingConsistency) {
    // All functions should handle null gracefully and return consistent error codes

    // Config functions
    EXPECT_EQ(parietal_training_default_config(nullptr), -1);

    // Query functions with null bridge
    EXPECT_EQ(parietal_training_get_state(nullptr), PARIETAL_TRAIN_STATE_ERROR);
    EXPECT_FALSE(parietal_training_is_connected(nullptr));

    // Domain functions with null bridge
    EXPECT_EQ(parietal_training_set_domain_lr(nullptr, PARIETAL_DOMAIN_BODY_SCHEMA, 0.01f), -1);
    EXPECT_EQ(parietal_training_set_domain_enabled(nullptr, PARIETAL_DOMAIN_BODY_SCHEMA, true), -1);

    // Callback functions with null bridge
    EXPECT_EQ(parietal_training_set_learning_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(parietal_training_set_update_callback(nullptr, nullptr, nullptr), -1);

    // Stats functions with null bridge
    parietal_training_stats_t stats;
    EXPECT_EQ(parietal_training_get_stats(nullptr, &stats), -1);

    // Connection functions with null bridge
    EXPECT_EQ(parietal_training_connect(nullptr, nullptr), -1);
    EXPECT_EQ(parietal_training_disconnect(nullptr), -1);
    EXPECT_EQ(parietal_training_connect_plasticity(nullptr, nullptr), -1);
    EXPECT_EQ(parietal_training_connect_bio_async(nullptr, nullptr), -1);

    // Processing functions with null
    EXPECT_EQ(parietal_training_process_signal(nullptr, nullptr), PARIETAL_TRAIN_RESPONSE_NONE);
    EXPECT_EQ(parietal_training_flush_batch(nullptr), -1);
    EXPECT_EQ(parietal_training_update_weights(nullptr, PARIETAL_DOMAIN_BODY_SCHEMA, 0.01f), -1);
}

/* ============================================================================
 * Default Value Regression Tests
 * ============================================================================ */

TEST_F(ParietalTrainingRegressionTest, DefaultConfigValues) {
    parietal_training_config_t config;
    parietal_training_default_config(&config);

    // Base learning rate should be reasonable
    EXPECT_GT(config.base_learning_rate, 0.0f);
    EXPECT_LT(config.base_learning_rate, 0.5f);

    // Features should be enabled by default
    EXPECT_TRUE(config.register_with_training);
    EXPECT_TRUE(config.connect_to_plasticity);
    EXPECT_TRUE(config.enable_stdp_learning);
}

TEST_F(ParietalTrainingRegressionTest, AllDomainsEnabledByDefault) {
    parietal_training_config_t config;
    parietal_training_default_config(&config);

    for (int i = 0; i < PARIETAL_DOMAIN_COUNT; i++) {
        EXPECT_TRUE(config.domains[i].enabled)
            << "Domain " << parietal_training_domain_name((parietal_learning_domain_t)i)
            << " should be enabled by default";
    }
}

TEST_F(ParietalTrainingRegressionTest, DomainLearningRatesPositive) {
    parietal_training_config_t config;
    parietal_training_default_config(&config);

    for (int i = 0; i < PARIETAL_DOMAIN_COUNT; i++) {
        EXPECT_GT(config.domains[i].learning_rate, 0.0f)
            << "Domain " << parietal_training_domain_name((parietal_learning_domain_t)i)
            << " should have positive learning rate";
    }
}

/* ============================================================================
 * Boundary Condition Regression Tests
 * ============================================================================ */

TEST_F(ParietalTrainingRegressionTest, InvalidDomainHandling) {
    // Invalid domain values should be handled gracefully
    const char* name = parietal_training_domain_name((parietal_learning_domain_t)100);
    EXPECT_STREQ(name, "unknown");

    name = parietal_training_domain_name((parietal_learning_domain_t)-1);
    EXPECT_STREQ(name, "unknown");
}

TEST_F(ParietalTrainingRegressionTest, InvalidStateHandling) {
    const char* name = parietal_training_state_name((parietal_train_state_t)100);
    EXPECT_STREQ(name, "unknown");
}

TEST_F(ParietalTrainingRegressionTest, InvalidResponseHandling) {
    const char* name = parietal_training_response_name((parietal_train_response_t)100);
    EXPECT_STREQ(name, "unknown");
}

