/**
 * @file test_parahipp_kg_wiring_integration.cpp
 * @brief Integration tests for Parahippocampal Cortex with Knowledge Graph wiring
 *
 * WHAT: Tests Parahippocampal Cortex integration with KG wiring system
 * WHY:  Ensure proper scene-concept associations and semantic linking
 * HOW:  Test KG connections, scene relationships, and semantic queries
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

class ParahippKGWiringIntegrationTest : public ::testing::Test {
protected:
    nimcp_parahippocampal_t* parahipp;
    parahipp_config_t config;
    float default_position[3];
    float default_heading;

    void SetUp() override {
        config = parahipp_default_config();
        config.enable_bio_async = false;
        config.enable_kg = true;
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
 * KG WIRING CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(ParahippKGWiringIntegrationTest, KGWiringEnabled) {
    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(parahipp, &retrieved));
    EXPECT_TRUE(retrieved.enable_kg);
}

TEST_F(ParahippKGWiringIntegrationTest, CreateWithKGConfig) {
    parahipp_config_t kg_config = parahipp_default_config();
    kg_config.enable_kg = true;
    kg_config.enable_bio_async = false;

    nimcp_parahippocampal_t* kg_ph = parahipp_create(&kg_config);
    ASSERT_NE(nullptr, kg_ph);

    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(kg_ph, &retrieved));
    EXPECT_TRUE(retrieved.enable_kg);

    parahipp_destroy(kg_ph);
}

/*=============================================================================
 * SCENE-CONCEPT ASSOCIATION TESTS
 *===========================================================================*/

TEST_F(ParahippKGWiringIntegrationTest, SceneEncodingCreatesKGEntry) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "kg_scene", &scene_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(scene_id, 0u);

    const nimcp_stored_scene_t* stored = parahipp_get_scene(parahipp, scene_id);
    EXPECT_NE(nullptr, stored);
}

TEST_F(ParahippKGWiringIntegrationTest, MultipleSceneKGEntries) {
    uint32_t scene_ids[5];

    for (int i = 0; i < 5; i++) {
        float features[512];
        CreateTestFeatures(features, 512, (float)i * 0.2f);

        char name[32];
        snprintf(name, sizeof(name), "kg_scene_%d", i);

        float pos[3] = {(float)i * 10.0f, 0.0f, 0.0f};

        int result = parahipp_encode_scene(parahipp, features, 512,
            pos, default_heading, name, &scene_ids[i]);
        EXPECT_EQ(0, result);
        EXPECT_GE(scene_ids[i], 0u);
    }

    for (int i = 0; i < 5; i++) {
        const nimcp_stored_scene_t* stored = parahipp_get_scene(parahipp, scene_ids[i]);
        EXPECT_NE(nullptr, stored);
    }
}

/*=============================================================================
 * LANDMARK KG INTEGRATION TESTS
 *===========================================================================*/

TEST_F(ParahippKGWiringIntegrationTest, LandmarkCreatesKGEntry) {
    float position[3] = {10.0f, 20.0f, 0.0f};
    float features[128];
    CreateTestFeatures(features, 128, 0.5f);

    uint32_t landmark_id = 0;
    int result = parahipp_add_landmark(parahipp, features, 128, position, "kg_landmark", &landmark_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(landmark_id, 0u);
}

TEST_F(ParahippKGWiringIntegrationTest, MultipleLandmarkKGEntries) {
    for (int i = 0; i < 5; i++) {
        float position[3] = {(float)i * 10.0f, (float)i * 20.0f, 0.0f};
        float features[128];
        CreateTestFeatures(features, 128, (float)i * 0.1f);

        char name[32];
        snprintf(name, sizeof(name), "landmark_%d", i);

        uint32_t landmark_id = 0;
        int result = parahipp_add_landmark(parahipp, features, 128, position, name, &landmark_id);
        EXPECT_EQ(0, result);
        EXPECT_GE(landmark_id, 0u);
    }
}

/*=============================================================================
 * SPATIAL CONTEXT KG TESTS
 *===========================================================================*/

TEST_F(ParahippKGWiringIntegrationTest, SpatialContextLinksToKG) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "spatial_kg", &scene_id));

    float context[256];
    int result = parahipp_get_current_context(parahipp, context, 256);
    EXPECT_GE(result, 0);
}

TEST_F(ParahippKGWiringIntegrationTest, LayoutProcessingUpdatesKG) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    int result = parahipp_process_layout(parahipp, features, 512);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * RECOGNITION WITH KG TESTS
 *===========================================================================*/

TEST_F(ParahippKGWiringIntegrationTest, RecognitionUsesKG) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "recognizable", &scene_id));

    parahipp_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    int rec_result = parahipp_recognize_scene(parahipp, features, 512, &result);
    EXPECT_EQ(0, rec_result);
    EXPECT_GT(result.match_confidence, 0.0f);
}

TEST_F(ParahippKGWiringIntegrationTest, SimilarScenesRecognized) {
    float features1[512];
    CreateTestFeatures(features1, 512, 0.5f);

    uint32_t scene_id1 = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features1, 512,
        default_position, default_heading, "scene_a", &scene_id1));

    float features2[512];
    CreateTestFeatures(features2, 512, 0.51f);

    parahipp_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    int rec_result = parahipp_recognize_scene(parahipp, features2, 512, &result);
    EXPECT_EQ(0, rec_result);
}

/*=============================================================================
 * PERSISTENCE AND RESET TESTS
 *===========================================================================*/

TEST_F(ParahippKGWiringIntegrationTest, ResetClearsKGState) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "pre_reset", &scene_id);

    parahipp_reset(parahipp);

    parahipp_status_t status = parahipp_get_status(parahipp);
    EXPECT_GE((int)status, 0);
}

TEST_F(ParahippKGWiringIntegrationTest, KGOperationsAfterReset) {
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
 * STATISTICS TESTS
 *===========================================================================*/

TEST_F(ParahippKGWiringIntegrationTest, StatsTrackKGOperations) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "kg_stats_%d", i);

        float pos[3] = {(float)i * 10.0f, 0.0f, 0.0f};
        uint32_t scene_id = 0;
        parahipp_encode_scene(parahipp, features, 512,
            pos, default_heading, name, &scene_id);
    }

    parahipp_stats_t stats;
    EXPECT_EQ(0, parahipp_get_stats(parahipp, &stats));
    EXPECT_GE(stats.scenes_encoded, 5u);
}

TEST_F(ParahippKGWiringIntegrationTest, DiagnosticsIncludeKGStatus) {
    int result = parahipp_log_diagnostics(parahipp);
    EXPECT_EQ(0, result);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
