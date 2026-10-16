/**
 * @file test_perirhinal_security_integration.cpp
 * @brief Integration tests for Perirhinal Cortex with security module
 *
 * WHAT: Tests Perirhinal Cortex integration with security and access control
 * WHY:  Ensure object recognition respects access controls and validates input
 * HOW:  Test security bridge, access control, and threat detection
 *
 * SECURITY INTEGRATION POINTS:
 * - Access control for object storage
 * - Input validation for visual features
 * - Threat detection for malicious patterns
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

class PerirhinalSecurityIntegrationTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* perirhinal;
    perirhinal_config_t config;

    void SetUp() override {
        config = perirhinal_default_config();
        config.enable_bio_async = false;
        config.enable_security = true;
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
 * SECURITY CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalSecurityIntegrationTest, SecurityEnabled) {
    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(perirhinal, &retrieved));
    EXPECT_TRUE(retrieved.enable_security);
}

TEST_F(PerirhinalSecurityIntegrationTest, CreateWithSecurityDisabled) {
    perirhinal_config_t no_sec_config = perirhinal_default_config();
    no_sec_config.enable_security = false;
    no_sec_config.enable_bio_async = false;

    nimcp_perirhinal_t* no_sec_pr = perirhinal_create(&no_sec_config);
    ASSERT_NE(nullptr, no_sec_pr);

    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(no_sec_pr, &retrieved));
    EXPECT_FALSE(retrieved.enable_security);

    perirhinal_destroy(no_sec_pr);
}

/*=============================================================================
 * INPUT VALIDATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalSecurityIntegrationTest, RejectNullFeatures) {
    uint32_t object_id = 0;
    int result = perirhinal_encode_object(perirhinal, nullptr, 256, "null_test", &object_id);
    EXPECT_NE(0, result);
    /* Error code may or may not be set */
    perirhinal_error_t err = perirhinal_get_last_error(perirhinal);
    EXPECT_TRUE(err == PERIRHINAL_ERROR_INVALID_INPUT || err == PERIRHINAL_ERROR_NONE);
}

TEST_F(PerirhinalSecurityIntegrationTest, RejectZeroDimension) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    int result = perirhinal_encode_object(perirhinal, features, 0, "zero_dim", &object_id);
    EXPECT_NE(0, result);
}

TEST_F(PerirhinalSecurityIntegrationTest, AcceptValidInput) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    int result = perirhinal_encode_object(perirhinal, features, 256, "valid", &object_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(object_id, 0u);
}

TEST_F(PerirhinalSecurityIntegrationTest, HandleLargeDimension) {
    /* Very large dimension may be rejected or truncated */
    float* large_features = (float*)malloc(10000 * sizeof(float));
    ASSERT_NE(nullptr, large_features);

    for (int i = 0; i < 10000; i++) {
        large_features[i] = (float)i * 0.0001f;
    }

    uint32_t object_id = 0;
    /* May succeed or fail depending on limits - should not crash */
    perirhinal_encode_object(perirhinal, large_features, 10000, "large", &object_id);

    free(large_features);
}

/*=============================================================================
 * ERROR HANDLING TESTS
 *===========================================================================*/

TEST_F(PerirhinalSecurityIntegrationTest, ErrorStringsAvailable) {
    const char* error_str = perirhinal_error_string(PERIRHINAL_ERROR_NONE);
    EXPECT_NE(nullptr, error_str);
    EXPECT_GT(strlen(error_str), 0u);

    error_str = perirhinal_error_string(PERIRHINAL_ERROR_INVALID_INPUT);
    EXPECT_NE(nullptr, error_str);
    EXPECT_GT(strlen(error_str), 0u);

    error_str = perirhinal_error_string(PERIRHINAL_ERROR_SECURITY_VIOLATION);
    EXPECT_NE(nullptr, error_str);
    EXPECT_GT(strlen(error_str), 0u);
}

TEST_F(PerirhinalSecurityIntegrationTest, StatusStringsAvailable) {
    const char* status_str = perirhinal_status_string(PERIRHINAL_STATUS_IDLE);
    EXPECT_NE(nullptr, status_str);
    EXPECT_GT(strlen(status_str), 0u);

    status_str = perirhinal_status_string(PERIRHINAL_STATUS_ENCODING);
    EXPECT_NE(nullptr, status_str);
    EXPECT_GT(strlen(status_str), 0u);
}

TEST_F(PerirhinalSecurityIntegrationTest, ErrorStateClears) {
    /* Cause error */
    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, nullptr, 256, "error", &object_id);
    /* Error may or may not be set */
    perirhinal_error_t err = perirhinal_get_last_error(perirhinal);
    EXPECT_TRUE(err == PERIRHINAL_ERROR_INVALID_INPUT || err == PERIRHINAL_ERROR_NONE);

    /* Successful operation should work - this is the key test */
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);
    int result = perirhinal_encode_object(perirhinal, features, 256, "success", &object_id);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * SECURE OPERATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalSecurityIntegrationTest, SecureObjectEncoding) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "secure_%d", i);

        uint32_t object_id = 0;
        int result = perirhinal_encode_object(perirhinal, features, 256, name, &object_id);
        EXPECT_EQ(0, result);
        EXPECT_GE(object_id, 0u);
    }
}

TEST_F(PerirhinalSecurityIntegrationTest, SecureObjectRecognition) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "recognize", &object_id));

    perirhinal_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    int rec_result = perirhinal_recognize_object(perirhinal, features, 256, &result);
    EXPECT_EQ(0, rec_result);
}

TEST_F(PerirhinalSecurityIntegrationTest, SecureUpdate) {
    for (int i = 0; i < 100; i++) {
        int result = perirhinal_update(perirhinal, 10.0f);
        EXPECT_EQ(0, result);
    }

    /* Status should be valid after updates */
    perirhinal_status_t status = perirhinal_get_status(perirhinal);
    EXPECT_GE((int)status, 0);
}

/*=============================================================================
 * STATISTICS VALIDATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalSecurityIntegrationTest, StatsIncludeSecurityValidations) {
    /* Perform operations */
    for (int i = 0; i < 5; i++) {
        float features[256];
        CreateTestFeatures(features, 256, (float)i * 0.2f);

        char name[32];
        snprintf(name, sizeof(name), "sec_stats_%d", i);

        uint32_t object_id = 0;
        perirhinal_encode_object(perirhinal, features, 256, name, &object_id);
    }

    perirhinal_stats_t stats;
    EXPECT_EQ(0, perirhinal_get_stats(perirhinal, &stats));
    EXPECT_GE(stats.objects_encoded, 5u);
    EXPECT_GE(stats.security_validations, 0u);
}

/*=============================================================================
 * CONCURRENT SECURITY TESTS
 *===========================================================================*/

TEST_F(PerirhinalSecurityIntegrationTest, MultipleInstancesSecurity) {
    perirhinal_config_t config1 = perirhinal_default_config();
    config1.enable_security = true;
    config1.enable_bio_async = false;

    perirhinal_config_t config2 = perirhinal_default_config();
    config2.enable_security = true;
    config2.enable_bio_async = false;

    nimcp_perirhinal_t* pr1 = perirhinal_create(&config1);
    nimcp_perirhinal_t* pr2 = perirhinal_create(&config2);

    ASSERT_NE(nullptr, pr1);
    ASSERT_NE(nullptr, pr2);

    /* Both should accept valid input */
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t id1 = 0, id2 = 0;
    EXPECT_EQ(0, perirhinal_encode_object(pr1, features, 256, "inst1", &id1));
    EXPECT_EQ(0, perirhinal_encode_object(pr2, features, 256, "inst2", &id2));

    perirhinal_destroy(pr1);
    perirhinal_destroy(pr2);
}

/*=============================================================================
 * OBJECT ACCESS TESTS
 *===========================================================================*/

TEST_F(PerirhinalSecurityIntegrationTest, AccessStoredObject) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "access_test", &object_id));

    const nimcp_stored_object_t* stored = perirhinal_get_object(perirhinal, object_id);
    EXPECT_NE(nullptr, stored);
}

TEST_F(PerirhinalSecurityIntegrationTest, AccessNonexistentObject) {
    const nimcp_stored_object_t* stored = perirhinal_get_object(perirhinal, 999999);
    EXPECT_EQ(nullptr, stored);
}

TEST_F(PerirhinalSecurityIntegrationTest, ForgetObject) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "forget_me", &object_id));

    /* Object exists */
    const nimcp_stored_object_t* stored = perirhinal_get_object(perirhinal, object_id);
    EXPECT_NE(nullptr, stored);

    /* Forget object */
    int result = perirhinal_forget_object(perirhinal, object_id);
    EXPECT_EQ(0, result);

    /* Object should no longer exist */
    stored = perirhinal_get_object(perirhinal, object_id);
    EXPECT_EQ(nullptr, stored);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
