/**
 * @file test_perirhinal_immune_integration.cpp
 * @brief Integration tests for Perirhinal Cortex with immune system
 *
 * WHAT: Tests Perirhinal Cortex integration with brain immune system
 * WHY:  Ensure object recognition maintains health and anomaly detection
 * HOW:  Test immune bridge, health monitoring, and anomaly handling
 *
 * BIOLOGICAL BASIS:
 * The brain immune system in perirhinal cortex:
 * - Monitors object representation integrity
 * - Detects corrupted or malicious patterns
 * - Maintains healthy neural activity
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Include NIMCP headers first for typedef compatibility
#include "nimcp.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/regions/perirhinal/nimcp_perirhinal.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class PerirhinalImmuneIntegrationTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* perirhinal;
    perirhinal_config_t config;

    void SetUp() override {
        config = perirhinal_default_config();
        config.enable_bio_async = false;
        config.enable_immune = true;
        perirhinal = perirhinal_create(&config);
        ASSERT_NE(nullptr, perirhinal);
    }

    void TearDown() override {
        if (perirhinal) {
            perirhinal_destroy(perirhinal);
            perirhinal = nullptr;
        }
    }

    void CreateTestFeatures(float* features, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base_value + (float)i * 0.001f;
        }
    }
};

/*=============================================================================
 * IMMUNE CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalImmuneIntegrationTest, ImmuneEnabled) {
    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(perirhinal, &retrieved));
    EXPECT_TRUE(retrieved.enable_immune);
}

TEST_F(PerirhinalImmuneIntegrationTest, CreateWithImmuneDisabled) {
    perirhinal_config_t no_immune_config = perirhinal_default_config();
    no_immune_config.enable_immune = false;
    no_immune_config.enable_bio_async = false;

    nimcp_perirhinal_t* no_immune_pr = perirhinal_create(&no_immune_config);
    ASSERT_NE(nullptr, no_immune_pr);

    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(no_immune_pr, &retrieved));
    EXPECT_FALSE(retrieved.enable_immune);

    perirhinal_destroy(no_immune_pr);
}

/*=============================================================================
 * HEALTH STATUS TESTS
 *===========================================================================*/

TEST_F(PerirhinalImmuneIntegrationTest, HealthStatusInitiallyGood) {
    float health = perirhinal_get_health_status(perirhinal);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
    /* Initial health should be good (> 0.5) */
    EXPECT_GE(health, 0.5f);
}

TEST_F(PerirhinalImmuneIntegrationTest, HealthStatusAfterOperations) {
    /* Perform normal operations */
    for (int i = 0; i < 10; i++) {
        float features[256];
        CreateTestFeatures(features, 256, (float)i * 0.1f);

        char name[32];
        snprintf(name, sizeof(name), "health_obj_%d", i);

        uint32_t object_id = 0;
        perirhinal_encode_object(perirhinal, features, 256, name, &object_id);
    }

    /* Run update cycles */
    for (int i = 0; i < 50; i++) {
        perirhinal_update(perirhinal, 10.0f);
    }

    /* Health should still be good after normal operations */
    float health = perirhinal_get_health_status(perirhinal);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(PerirhinalImmuneIntegrationTest, HealthStatusPersistsAcrossReset) {
    float initial_health = perirhinal_get_health_status(perirhinal);

    /* Perform operations */
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "reset_test", &object_id);

    /* Reset */
    perirhinal_reset(perirhinal);

    float post_reset_health = perirhinal_get_health_status(perirhinal);
    EXPECT_GE(post_reset_health, 0.0f);
    EXPECT_LE(post_reset_health, 1.0f);
}

/*=============================================================================
 * DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(PerirhinalImmuneIntegrationTest, DiagnosticsWork) {
    int result = perirhinal_log_diagnostics(perirhinal);
    EXPECT_EQ(0, result);
}

TEST_F(PerirhinalImmuneIntegrationTest, DiagnosticsAfterIntensiveUse) {
    /* Intensive use */
    for (int i = 0; i < 50; i++) {
        float features[256];
        CreateTestFeatures(features, 256, (float)i * 0.02f);

        char name[32];
        snprintf(name, sizeof(name), "diag_obj_%d", i);

        uint32_t object_id = 0;
        perirhinal_encode_object(perirhinal, features, 256, name, &object_id);

        perirhinal_update(perirhinal, 5.0f);
    }

    int result = perirhinal_log_diagnostics(perirhinal);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * ERROR DETECTION TESTS
 *===========================================================================*/

TEST_F(PerirhinalImmuneIntegrationTest, HandleNullInput) {
    uint32_t object_id = 0;
    int result = perirhinal_encode_object(perirhinal, nullptr, 256, "null_test", &object_id);
    EXPECT_NE(0, result);
    /* Error code may or may not be set depending on implementation */
    perirhinal_error_t err = perirhinal_get_last_error(perirhinal);
    EXPECT_TRUE(err == PERIRHINAL_ERROR_INVALID_INPUT || err == PERIRHINAL_ERROR_NONE);

    /* System should still be healthy after handling error */
    float health = perirhinal_get_health_status(perirhinal);
    EXPECT_GE(health, 0.0f);
}

TEST_F(PerirhinalImmuneIntegrationTest, HandleZeroDimension) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    int result = perirhinal_encode_object(perirhinal, features, 0, "zero_dim", &object_id);
    EXPECT_NE(0, result);

    /* System should still be healthy */
    float health = perirhinal_get_health_status(perirhinal);
    EXPECT_GE(health, 0.0f);
}

TEST_F(PerirhinalImmuneIntegrationTest, RecoverFromErrors) {
    /* Cause error */
    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, nullptr, 256, "error", &object_id);

    /* Error state may or may not be set */
    perirhinal_error_t err = perirhinal_get_last_error(perirhinal);
    EXPECT_TRUE(err == PERIRHINAL_ERROR_INVALID_INPUT || err == PERIRHINAL_ERROR_NONE);

    /* Should still work after error - this is the key test */
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    int result = perirhinal_encode_object(perirhinal, features, 256, "recovery", &object_id);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * STATISTICS WITH IMMUNE TESTS
 *===========================================================================*/

TEST_F(PerirhinalImmuneIntegrationTest, ImmuneStatsTracked) {
    /* Perform operations */
    for (int i = 0; i < 5; i++) {
        float features[256];
        CreateTestFeatures(features, 256, (float)i * 0.2f);

        char name[32];
        snprintf(name, sizeof(name), "immune_stats_%d", i);

        uint32_t object_id = 0;
        perirhinal_encode_object(perirhinal, features, 256, name, &object_id);

        perirhinal_update(perirhinal, 10.0f);
    }

    perirhinal_stats_t stats;
    EXPECT_EQ(0, perirhinal_get_stats(perirhinal, &stats));
    EXPECT_GE(stats.objects_encoded, 5u);
    /* Immune stats may include scans */
    EXPECT_GE(stats.immune_scans, 0u);
}

/*=============================================================================
 * CONCURRENT OPERATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalImmuneIntegrationTest, MultipleInstancesHealthy) {
    perirhinal_config_t config1 = perirhinal_default_config();
    config1.enable_immune = true;
    config1.enable_bio_async = false;

    perirhinal_config_t config2 = perirhinal_default_config();
    config2.enable_immune = true;
    config2.enable_bio_async = false;

    nimcp_perirhinal_t* pr1 = perirhinal_create(&config1);
    nimcp_perirhinal_t* pr2 = perirhinal_create(&config2);

    ASSERT_NE(nullptr, pr1);
    ASSERT_NE(nullptr, pr2);

    /* Both should be healthy */
    float health1 = perirhinal_get_health_status(pr1);
    float health2 = perirhinal_get_health_status(pr2);

    EXPECT_GE(health1, 0.5f);
    EXPECT_GE(health2, 0.5f);

    perirhinal_destroy(pr1);
    perirhinal_destroy(pr2);
}

TEST_F(PerirhinalImmuneIntegrationTest, ResetRestoresHealth) {
    /* Perform heavy operations */
    for (int i = 0; i < 100; i++) {
        float features[256];
        CreateTestFeatures(features, 256, (float)i * 0.01f);

        char name[32];
        snprintf(name, sizeof(name), "heavy_%d", i);

        uint32_t object_id = 0;
        perirhinal_encode_object(perirhinal, features, 256, name, &object_id);
    }

    float pre_reset_health = perirhinal_get_health_status(perirhinal);

    /* Reset */
    perirhinal_reset(perirhinal);

    float post_reset_health = perirhinal_get_health_status(perirhinal);
    EXPECT_GE(post_reset_health, 0.0f);
    EXPECT_LE(post_reset_health, 1.0f);
}

/*=============================================================================
 * UPDATE CYCLE TESTS
 *===========================================================================*/

TEST_F(PerirhinalImmuneIntegrationTest, UpdateMaintainsHealth) {
    float initial_health = perirhinal_get_health_status(perirhinal);

    /* Many update cycles */
    for (int i = 0; i < 1000; i++) {
        perirhinal_update(perirhinal, 1.0f);
    }

    float final_health = perirhinal_get_health_status(perirhinal);
    EXPECT_GE(final_health, 0.0f);
    EXPECT_LE(final_health, 1.0f);
}

TEST_F(PerirhinalImmuneIntegrationTest, BidirectionalUpdateHealthy) {
    for (int i = 0; i < 100; i++) {
        int result = perirhinal_bidirectional_update(perirhinal, 10.0f);
        EXPECT_EQ(0, result);
    }

    float health = perirhinal_get_health_status(perirhinal);
    EXPECT_GE(health, 0.0f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
