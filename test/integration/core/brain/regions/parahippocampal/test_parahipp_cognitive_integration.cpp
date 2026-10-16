/**
 * @file test_parahipp_cognitive_integration.cpp
 * @brief Integration tests for Parahippocampal Cortex with cognitive systems
 *
 * WHAT: Tests Parahippocampal Cortex integration with working memory and attention
 * WHY:  Ensure scene processing integrates with cognitive processing
 * HOW:  Test spatial context, scene binding, and navigation support
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

class ParahippCognitiveIntegrationTest : public ::testing::Test {
protected:
    nimcp_parahippocampal_t* parahipp;
    parahipp_config_t config;
    float default_position[3];
    float default_heading;

    void SetUp() override {
        config = parahipp_default_config();
        config.enable_bio_async = false;
        config.enable_cognitive = true;
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
 * COGNITIVE CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(ParahippCognitiveIntegrationTest, CognitiveEnabled) {
    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(parahipp, &retrieved));
    EXPECT_TRUE(retrieved.enable_cognitive);
}

TEST_F(ParahippCognitiveIntegrationTest, CreateWithFullCognitiveConfig) {
    parahipp_config_t full_config = parahipp_default_config();
    full_config.enable_cognitive = true;
    full_config.enable_training = true;
    full_config.enable_perception = true;
    full_config.enable_bio_async = false;

    nimcp_parahippocampal_t* full_ph = parahipp_create(&full_config);
    ASSERT_NE(nullptr, full_ph);

    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(full_ph, &retrieved));
    EXPECT_TRUE(retrieved.enable_cognitive);

    parahipp_destroy(full_ph);
}

/*=============================================================================
 * SCENE ENCODING TESTS
 *===========================================================================*/

TEST_F(ParahippCognitiveIntegrationTest, EncodeSceneForWorkingMemory) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "wm_scene", &scene_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(scene_id, 0u);

    const nimcp_stored_scene_t* stored = parahipp_get_scene(parahipp, scene_id);
    EXPECT_NE(nullptr, stored);
}

TEST_F(ParahippCognitiveIntegrationTest, MultipleScenesInMemory) {
    uint32_t scene_ids[5];

    for (int i = 0; i < 5; i++) {
        float features[512];
        CreateTestFeatures(features, 512, (float)i * 0.2f);

        char name[32];
        snprintf(name, sizeof(name), "wm_scene_%d", i);

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
 * RECOGNITION AND RECALL TESTS
 *===========================================================================*/

TEST_F(ParahippCognitiveIntegrationTest, RecognizeEncodedScene) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "recall_test", &scene_id));

    parahipp_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    int rec_result = parahipp_recognize_scene(parahipp, features, 512, &result);
    EXPECT_EQ(0, rec_result);
    EXPECT_GT(result.match_confidence, 0.0f);
}

/*=============================================================================
 * SPATIAL CONTEXT TESTS
 *===========================================================================*/

TEST_F(ParahippCognitiveIntegrationTest, ComputeSpatialContext) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "context_test", &scene_id));

    float context[256];
    int result = parahipp_get_current_context(parahipp, context, 256);
    EXPECT_GE(result, 0);
}

TEST_F(ParahippCognitiveIntegrationTest, LayoutProcessing) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    int result = parahipp_process_layout(parahipp, features, 512);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * LANDMARK TESTS
 *===========================================================================*/

TEST_F(ParahippCognitiveIntegrationTest, AddLandmark) {
    float position[3] = {10.0f, 20.0f, 0.0f};
    float features[128];
    CreateTestFeatures(features, 128, 0.5f);

    uint32_t landmark_id = 0;
    int result = parahipp_add_landmark(parahipp, features, 128, position, "landmark1", &landmark_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(landmark_id, 0u);
}

TEST_F(ParahippCognitiveIntegrationTest, RetrieveLandmarks) {
    float position[3] = {10.0f, 20.0f, 0.0f};
    float features[128];
    CreateTestFeatures(features, 128, 0.5f);

    uint32_t landmark_id = 0;
    ASSERT_EQ(0, parahipp_add_landmark(parahipp, features, 128, position, "test_landmark", &landmark_id));

    const nimcp_stored_landmark_t* landmark = parahipp_get_landmark(parahipp, landmark_id);
    EXPECT_NE(nullptr, landmark);
}

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(ParahippCognitiveIntegrationTest, StatsTrackCognitiveActivity) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "stats_test", &scene_id);

    parahipp_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    parahipp_recognize_scene(parahipp, features, 512, &result);

    parahipp_stats_t stats;
    EXPECT_EQ(0, parahipp_get_stats(parahipp, &stats));
    EXPECT_GE(stats.scenes_encoded, 1u);
}

TEST_F(ParahippCognitiveIntegrationTest, CurrentContextAccessible) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "context", &scene_id);
    parahipp_process_visual_input(parahipp, features, 512);

    context_state_t state = parahipp_get_context_state(parahipp);
    EXPECT_GE((int)state, 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
