/**
 * @file test_parahippocampal_backward_compat.cpp
 * @brief Backward compatibility regression tests for Parahippocampal Cortex
 *
 * WHAT: Tests Parahippocampal Cortex API stability and backward compatibility
 * WHY:  Ensure existing parahippocampal code continues to work after updates
 * HOW:  Test core API functions, data structures, and return values
 *
 * REGRESSION FOCUS:
 * - API function signatures unchanged
 * - Return value semantics preserved
 * - Default behaviors maintained
 * - Error codes consistent
 * - Configuration defaults stable
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/parahippocampal/nimcp_parahippocampal.h"
#include "utils/logging/nimcp_logging.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ParahippocampalBackwardCompatTest : public ::testing::Test {
protected:
    nimcp_parahippocampal_t* parahipp;
    parahipp_config_t config;

    static constexpr uint32_t FEATURE_DIM = 128;

    void SetUp() override {
        config = parahipp_default_config();
        config.enable_bio_async = false;
        parahipp = parahipp_create(&config);
        ASSERT_NE(nullptr, parahipp);
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
 * API FUNCTION SIGNATURE TESTS
 *===========================================================================*/

TEST_F(ParahippocampalBackwardCompatTest, API_parahipp_default_config_exists) {
    parahipp_config_t cfg = parahipp_default_config();
    EXPECT_TRUE(true);  /* Compilation success = function exists */
}

TEST_F(ParahippocampalBackwardCompatTest, API_parahipp_create_exists) {
    parahipp_config_t cfg = parahipp_default_config();
    cfg.enable_bio_async = false;
    nimcp_parahippocampal_t* test = parahipp_create(&cfg);
    ASSERT_NE(nullptr, test);
    parahipp_destroy(test);
}

TEST_F(ParahippocampalBackwardCompatTest, API_parahipp_destroy_exists) {
    parahipp_config_t cfg = parahipp_default_config();
    cfg.enable_bio_async = false;
    nimcp_parahippocampal_t* test = parahipp_create(&cfg);
    parahipp_destroy(test);
    parahipp_destroy(nullptr);  /* Should handle NULL safely */
}

TEST_F(ParahippocampalBackwardCompatTest, API_parahipp_encode_scene_exists) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);
    float position[3] = {1.0f, 2.0f, 0.0f};

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, features, FEATURE_DIM,
        position, 0.0f, nullptr, &scene_id);
    EXPECT_EQ(0, result);
}

TEST_F(ParahippocampalBackwardCompatTest, API_parahipp_recognize_scene_exists) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);
    float position[3] = {1.0f, 2.0f, 0.0f};

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, FEATURE_DIM, position, 0.0f, nullptr, &scene_id);

    parahipp_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    int ret = parahipp_recognize_scene(parahipp, features, FEATURE_DIM, &result);
    EXPECT_GE(ret, -1);
}

TEST_F(ParahippocampalBackwardCompatTest, API_parahipp_add_landmark_exists) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);
    float position[3] = {10.0f, 20.0f, 0.0f};

    uint32_t landmark_id = 0;
    int result = parahipp_add_landmark(parahipp, features, FEATURE_DIM,
        position, "test_landmark", &landmark_id);
    EXPECT_EQ(0, result);
}

TEST_F(ParahippocampalBackwardCompatTest, API_parahipp_process_layout_exists) {
    float boundary_distances[64];
    CreateTestFeatures(boundary_distances, 64, 0.5f);

    int result = parahipp_process_layout(parahipp, boundary_distances, 64);
    EXPECT_EQ(0, result);
}

TEST_F(ParahippocampalBackwardCompatTest, API_parahipp_reset_exists) {
    int result = parahipp_reset(parahipp);
    EXPECT_EQ(0, result);
}

TEST_F(ParahippocampalBackwardCompatTest, API_parahipp_get_status_exists) {
    parahipp_status_t status = parahipp_get_status(parahipp);
    EXPECT_GE((int)status, 0);
}

TEST_F(ParahippocampalBackwardCompatTest, API_parahipp_get_config_exists) {
    parahipp_config_t retrieved;
    int result = parahipp_get_config(parahipp, &retrieved);
    EXPECT_EQ(0, result);
}

TEST_F(ParahippocampalBackwardCompatTest, API_parahipp_get_stats_exists) {
    parahipp_stats_t stats;
    int result = parahipp_get_stats(parahipp, &stats);
    EXPECT_EQ(0, result);
}

TEST_F(ParahippocampalBackwardCompatTest, API_parahipp_update_exists) {
    int result = parahipp_update(parahipp, 10.0f);
    EXPECT_EQ(0, result);
}

TEST_F(ParahippocampalBackwardCompatTest, API_parahipp_get_current_context_exists) {
    float context[128];
    int result = parahipp_get_current_context(parahipp, context, 128);
    EXPECT_GE(result, -1);
}

/*=============================================================================
 * RETURN VALUE SEMANTICS TESTS
 *===========================================================================*/

TEST_F(ParahippocampalBackwardCompatTest, ReturnSemantics_EncodeReturnsZeroOnSuccess) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);
    float position[3] = {1.0f, 2.0f, 0.0f};

    uint32_t scene_id = 0;
    EXPECT_EQ(0, parahipp_encode_scene(parahipp, features, FEATURE_DIM,
        position, 0.0f, nullptr, &scene_id));
}

TEST_F(ParahippocampalBackwardCompatTest, ReturnSemantics_NullHandledGracefully) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);
    float position[3] = {1.0f, 2.0f, 0.0f};

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(nullptr, features, FEATURE_DIM,
        position, 0.0f, nullptr, &scene_id);
    EXPECT_NE(0, result);
}

/*=============================================================================
 * DEFAULT BEHAVIOR TESTS
 *===========================================================================*/

TEST_F(ParahippocampalBackwardCompatTest, DefaultConfig_HasReasonableValues) {
    parahipp_config_t cfg = parahipp_default_config();

    EXPECT_GT(cfg.max_stored_scenes, 0u);
    EXPECT_GT(cfg.max_landmarks, 0u);
}

TEST_F(ParahippocampalBackwardCompatTest, DefaultStats_AreZero) {
    parahipp_stats_t stats;
    parahipp_get_stats(parahipp, &stats);

    EXPECT_EQ(0u, stats.total_stored_scenes);
    EXPECT_EQ(0u, stats.total_stored_landmarks);
}

/*=============================================================================
 * STRUCTURE SIZE TESTS
 *===========================================================================*/

TEST_F(ParahippocampalBackwardCompatTest, StructSize_parahipp_recognition_result_t) {
    EXPECT_GT(sizeof(parahipp_recognition_result_t), sizeof(uint32_t) * 2);
}

TEST_F(ParahippocampalBackwardCompatTest, StructSize_parahipp_config_t) {
    EXPECT_GT(sizeof(parahipp_config_t), sizeof(uint32_t) * 2);
}

TEST_F(ParahippocampalBackwardCompatTest, StructSize_parahipp_stats_t) {
    EXPECT_GT(sizeof(parahipp_stats_t), sizeof(uint32_t) * 2);
}

/*=============================================================================
 * LIFECYCLE COMPATIBILITY TESTS
 *===========================================================================*/

TEST_F(ParahippocampalBackwardCompatTest, Lifecycle_CreateDestroyMultiple) {
    for (int i = 0; i < 5; i++) {
        parahipp_config_t cfg = parahipp_default_config();
        cfg.enable_bio_async = false;
        nimcp_parahippocampal_t* test = parahipp_create(&cfg);
        EXPECT_NE(nullptr, test);
        parahipp_destroy(test);
    }
}

TEST_F(ParahippocampalBackwardCompatTest, Lifecycle_ResetMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        float features[FEATURE_DIM];
        CreateTestFeatures(features, FEATURE_DIM, (float)i * 0.1f);
        float position[3] = {(float)i, (float)i, 0.0f};

        uint32_t scene_id = 0;
        parahipp_encode_scene(parahipp, features, FEATURE_DIM, position, 0.0f, nullptr, &scene_id);
        parahipp_reset(parahipp);
    }
}

TEST_F(ParahippocampalBackwardCompatTest, Lifecycle_OperationsAfterReset) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);
    float position[3] = {1.0f, 2.0f, 0.0f};

    uint32_t id1 = 0;
    parahipp_encode_scene(parahipp, features, FEATURE_DIM, position, 0.0f, nullptr, &id1);
    parahipp_reset(parahipp);

    uint32_t id2 = 0;
    int result = parahipp_encode_scene(parahipp, features, FEATURE_DIM, position, 0.0f, nullptr, &id2);
    EXPECT_EQ(0, result);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
