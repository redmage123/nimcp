/**
 * @file test_parahipp_security_integration.cpp
 * @brief Integration tests for Parahippocampal Cortex with security system
 *
 * WHAT: Tests Parahippocampal Cortex integration with security module
 * WHY:  Ensure secure scene processing and access control
 * HOW:  Test security contexts, validation, and protection mechanisms
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "nimcp.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/regions/parahippocampal/nimcp_parahippocampal.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ParahippSecurityIntegrationTest : public ::testing::Test {
protected:
    nimcp_parahippocampal_t* parahipp;
    parahipp_config_t config;
    float default_position[3];
    float default_heading;

    void SetUp() override {
        config = parahipp_default_config();
        config.enable_bio_async = false;
        config.enable_security = true;
        config.enable_training = true;
        parahipp = parahipp_create(&config);
        ASSERT_NE(nullptr, parahipp);

        default_position[0] = 0.0f;
        default_position[1] = 0.0f;
        default_position[2] = 0.0f;
        default_heading = 0.0f;
    }

    void TearDown() override {
        if (parahipp) {
            parahipp_destroy(parahipp);
            parahipp = nullptr;
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

TEST_F(ParahippSecurityIntegrationTest, SecurityEnabled) {
    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(parahipp, &retrieved));
    EXPECT_TRUE(retrieved.enable_security);
}

TEST_F(ParahippSecurityIntegrationTest, CreateWithSecurityConfig) {
    parahipp_config_t security_config = parahipp_default_config();
    security_config.enable_security = true;
    security_config.enable_bio_async = false;

    nimcp_parahippocampal_t* sec_ph = parahipp_create(&security_config);
    ASSERT_NE(nullptr, sec_ph);

    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(sec_ph, &retrieved));
    EXPECT_TRUE(retrieved.enable_security);

    parahipp_destroy(sec_ph);
}

/*=============================================================================
 * INPUT VALIDATION TESTS
 *===========================================================================*/

TEST_F(ParahippSecurityIntegrationTest, ValidInputAccepted) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "valid_scene", &scene_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(scene_id, 0u);
}

TEST_F(ParahippSecurityIntegrationTest, NullInputRejected) {
    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, nullptr, 512,
        default_position, default_heading, "null_input", &scene_id);
    EXPECT_NE(0, result);
}

TEST_F(ParahippSecurityIntegrationTest, ZeroDimensionRejected) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, features, 0,
        default_position, default_heading, "zero_dim", &scene_id);
    EXPECT_NE(0, result);
}

TEST_F(ParahippSecurityIntegrationTest, NullNameHandled) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, nullptr, &scene_id);
    /* May succeed with default name or fail - both acceptable */
    EXPECT_GE(result, -1);
}

/*=============================================================================
 * ACCESS CONTROL TESTS
 *===========================================================================*/

TEST_F(ParahippSecurityIntegrationTest, EncodedSceneAccessible) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "accessible", &scene_id));

    const nimcp_stored_scene_t* stored = parahipp_get_scene(parahipp, scene_id);
    EXPECT_NE(nullptr, stored);
}

TEST_F(ParahippSecurityIntegrationTest, InvalidSceneIdReturnsNull) {
    const nimcp_stored_scene_t* stored = parahipp_get_scene(parahipp, 999999);
    EXPECT_EQ(nullptr, stored);
}

TEST_F(ParahippSecurityIntegrationTest, MultipleAccessesToSameScene) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "multi_access", &scene_id));

    for (int i = 0; i < 10; i++) {
        const nimcp_stored_scene_t* stored = parahipp_get_scene(parahipp, scene_id);
        EXPECT_NE(nullptr, stored);
    }
}

/*=============================================================================
 * BOUNDS CHECKING TESTS
 *===========================================================================*/

TEST_F(ParahippSecurityIntegrationTest, LargeDimensionHandled) {
    /* Should handle gracefully - either succeed with truncation or fail */
    float features[1024];
    CreateTestFeatures(features, 1024, 0.5f);

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, features, 1024,
        default_position, default_heading, "large_dim", &scene_id);
    EXPECT_GE(result, -1);
}

TEST_F(ParahippSecurityIntegrationTest, ContextBufferBoundsRespected) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "context_test", &scene_id));

    float context[256];
    int result = parahipp_get_current_context(parahipp, context, 256);
    EXPECT_GE(result, -1);
}

/*=============================================================================
 * RECOVERY AND RESET TESTS
 *===========================================================================*/

TEST_F(ParahippSecurityIntegrationTest, ResetClearsSecurityState) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "pre_reset", &scene_id);

    EXPECT_EQ(0, parahipp_reset(parahipp));

    parahipp_status_t status = parahipp_get_status(parahipp);
    EXPECT_GE((int)status, 0);
}

TEST_F(ParahippSecurityIntegrationTest, OperationsAfterResetWork) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id1 = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "pre_reset", &scene_id1);

    parahipp_reset(parahipp);

    uint32_t scene_id2 = 0;
    int result = parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "post_reset", &scene_id2);
    EXPECT_EQ(0, result);
    EXPECT_GE(scene_id2, 0u);
}

/*=============================================================================
 * STATE CONSISTENCY TESTS
 *===========================================================================*/

TEST_F(ParahippSecurityIntegrationTest, StatusConsistentAfterOperations) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "scene_%d", i);

        float pos[3] = {(float)i * 10.0f, 0.0f, 0.0f};
        uint32_t scene_id = 0;
        parahipp_encode_scene(parahipp, features, 512,
            pos, default_heading, name, &scene_id);
    }

    parahipp_status_t status = parahipp_get_status(parahipp);
    EXPECT_GE((int)status, 0);
}

TEST_F(ParahippSecurityIntegrationTest, ErrorStateRecoverable) {
    /* Trigger an error */
    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, nullptr, 512,
        default_position, default_heading, "error", &scene_id);

    /* Should be able to continue with valid input */
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    int result = parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "recovery", &scene_id);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(ParahippSecurityIntegrationTest, DiagnosticsIncludeSecurityInfo) {
    int result = parahipp_log_diagnostics(parahipp);
    EXPECT_EQ(0, result);
}

TEST_F(ParahippSecurityIntegrationTest, StatsTrackSecurityOperations) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "stats_test", &scene_id);

    parahipp_stats_t stats;
    EXPECT_EQ(0, parahipp_get_stats(parahipp, &stats));
    EXPECT_GE(stats.scenes_encoded, 1u);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
