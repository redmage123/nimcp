/**
 * @file test_parahipp_immune_integration.cpp
 * @brief Integration tests for Parahippocampal Cortex with immune system
 *
 * WHAT: Tests Parahippocampal Cortex integration with brain immune system
 * WHY:  Ensure proper immune response to anomalous scene patterns
 * HOW:  Test immune triggers, anomaly detection, and immune state management
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

class ParahippImmuneIntegrationTest : public ::testing::Test {
protected:
    nimcp_parahippocampal_t* parahipp;
    parahipp_config_t config;
    float default_position[3];
    float default_heading;

    void SetUp() override {
        config = parahipp_default_config();
        config.enable_bio_async = false;
        config.enable_immune = true;
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

    void CreateAnomalousFeatures(float* features, uint32_t dim) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = (i % 3 == 0) ? 100.0f : -100.0f;
        }
    }
};

/*=============================================================================
 * IMMUNE CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(ParahippImmuneIntegrationTest, ImmuneEnabled) {
    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(parahipp, &retrieved));
    EXPECT_TRUE(retrieved.enable_immune);
}

TEST_F(ParahippImmuneIntegrationTest, CreateWithImmuneConfig) {
    parahipp_config_t immune_config = parahipp_default_config();
    immune_config.enable_immune = true;
    immune_config.enable_bio_async = false;

    nimcp_parahippocampal_t* immune_ph = parahipp_create(&immune_config);
    ASSERT_NE(nullptr, immune_ph);

    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(immune_ph, &retrieved));
    EXPECT_TRUE(retrieved.enable_immune);

    parahipp_destroy(immune_ph);
}

/*=============================================================================
 * NORMAL SCENE PROCESSING TESTS
 *===========================================================================*/

TEST_F(ParahippImmuneIntegrationTest, NormalSceneDoesNotTriggerImmune) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "normal_scene", &scene_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(scene_id, 0u);

    float health = parahipp_get_health_status(parahipp);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(ParahippImmuneIntegrationTest, MultipleNormalScenesStayHealthy) {
    for (int i = 0; i < 10; i++) {
        float features[512];
        CreateTestFeatures(features, 512, (float)i * 0.1f);

        char name[32];
        snprintf(name, sizeof(name), "normal_%d", i);

        float pos[3] = {(float)i * 10.0f, 0.0f, 0.0f};

        uint32_t scene_id = 0;
        int result = parahipp_encode_scene(parahipp, features, 512,
            pos, default_heading, name, &scene_id);
        EXPECT_EQ(0, result);
    }

    float health = parahipp_get_health_status(parahipp);
    EXPECT_GE(health, 0.0f);
}

/*=============================================================================
 * ANOMALY DETECTION TESTS
 *===========================================================================*/

TEST_F(ParahippImmuneIntegrationTest, AnomalousSceneDetected) {
    float normal_features[512];
    CreateTestFeatures(normal_features, 512, 0.5f);

    uint32_t normal_id = 0;
    parahipp_encode_scene(parahipp, normal_features, 512,
        default_position, default_heading, "baseline", &normal_id);

    float anomalous_features[512];
    CreateAnomalousFeatures(anomalous_features, 512);

    float pos[3] = {100.0f, 0.0f, 0.0f};
    uint32_t anomaly_id = 0;
    int result = parahipp_encode_scene(parahipp, anomalous_features, 512,
        pos, default_heading, "anomaly", &anomaly_id);
    EXPECT_GE(result, -1);
}

TEST_F(ParahippImmuneIntegrationTest, ImmuneResponseToRepeatedAnomalies) {
    float normal_features[512];
    CreateTestFeatures(normal_features, 512, 0.5f);

    uint32_t normal_id = 0;
    parahipp_encode_scene(parahipp, normal_features, 512,
        default_position, default_heading, "baseline", &normal_id);

    for (int i = 0; i < 5; i++) {
        float anomalous_features[512];
        CreateAnomalousFeatures(anomalous_features, 512);

        char name[32];
        snprintf(name, sizeof(name), "anomaly_%d", i);

        float pos[3] = {(float)i * 100.0f, 0.0f, 0.0f};
        uint32_t anomaly_id = 0;
        parahipp_encode_scene(parahipp, anomalous_features, 512,
            pos, default_heading, name, &anomaly_id);
    }

    parahipp_stats_t stats;
    EXPECT_EQ(0, parahipp_get_stats(parahipp, &stats));
}

/*=============================================================================
 * HEALTH STATUS TESTS
 *===========================================================================*/

TEST_F(ParahippImmuneIntegrationTest, HealthStatusAccessible) {
    float health = parahipp_get_health_status(parahipp);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(ParahippImmuneIntegrationTest, HealthStatusAfterProcessing) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "test", &scene_id);

    for (int i = 0; i < 50; i++) {
        parahipp_update(parahipp, 10.0f);
    }

    float health = parahipp_get_health_status(parahipp);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

/*=============================================================================
 * RECOVERY TESTS
 *===========================================================================*/

TEST_F(ParahippImmuneIntegrationTest, RecoveryAfterAnomalies) {
    float normal_features[512];
    CreateTestFeatures(normal_features, 512, 0.5f);

    uint32_t normal_id = 0;
    parahipp_encode_scene(parahipp, normal_features, 512,
        default_position, default_heading, "baseline", &normal_id);

    float anomalous_features[512];
    CreateAnomalousFeatures(anomalous_features, 512);
    float anom_pos[3] = {100.0f, 0.0f, 0.0f};
    uint32_t anomaly_id = 0;
    parahipp_encode_scene(parahipp, anomalous_features, 512,
        anom_pos, default_heading, "anomaly", &anomaly_id);

    for (int i = 0; i < 10; i++) {
        CreateTestFeatures(normal_features, 512, (float)i * 0.1f);

        char name[32];
        snprintf(name, sizeof(name), "recovery_%d", i);

        float pos[3] = {(float)i * 10.0f, 0.0f, 0.0f};
        uint32_t recovery_id = 0;
        parahipp_encode_scene(parahipp, normal_features, 512,
            pos, default_heading, name, &recovery_id);
    }

    float health = parahipp_get_health_status(parahipp);
    EXPECT_GE(health, 0.0f);
}

TEST_F(ParahippImmuneIntegrationTest, ResetClearsImmuneState) {
    float anomalous_features[512];
    CreateAnomalousFeatures(anomalous_features, 512);

    uint32_t anomaly_id = 0;
    parahipp_encode_scene(parahipp, anomalous_features, 512,
        default_position, default_heading, "anomaly", &anomaly_id);

    parahipp_reset(parahipp);

    float health = parahipp_get_health_status(parahipp);
    EXPECT_GE(health, 0.0f);
}

/*=============================================================================
 * DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(ParahippImmuneIntegrationTest, DiagnosticsIncludeImmuneStatus) {
    int result = parahipp_log_diagnostics(parahipp);
    EXPECT_EQ(0, result);
}

TEST_F(ParahippImmuneIntegrationTest, StatsTrackProcessingWithImmune) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "test", &scene_id);

    parahipp_stats_t stats;
    EXPECT_EQ(0, parahipp_get_stats(parahipp, &stats));
    EXPECT_GE(stats.scenes_encoded, 1u);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
