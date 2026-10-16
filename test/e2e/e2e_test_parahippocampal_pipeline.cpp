/**
 * @file e2e_test_parahippocampal_pipeline.cpp
 * @brief End-to-end tests for Parahippocampal Cortex Pipeline
 *
 * WHAT: Full pipeline tests for scene recognition and spatial context
 * WHY:  Verify complete parahippocampal workflows with navigation integration
 * HOW:  Test scene encoding, context processing, landmark navigation
 *
 * TEST COVERAGE:
 * - Scene Encoding Pipeline (4 tests)
 * - Context Processing (3 tests)
 * - Landmark Navigation (4 tests)
 * - Layout Recognition (3 tests)
 * - Performance Benchmarks (2 tests)
 *
 * TOTAL: 16 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Parahippocampal cortex critical for scene/context recognition
 * - Place recognition and viewpoint-invariant representations
 * - Spatial layout processing for navigation
 * - Bidirectional connections with hippocampus and entorhinal cortex
 *
 * @author NIMCP Development Team
 * @date 2026-01-14
 */

#include "e2e_test_framework.h"
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <cstring>

#include "core/brain/regions/parahippocampal/nimcp_parahippocampal.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

/*=============================================================================
 * Test Configuration
 *===========================================================================*/

constexpr double MAX_ENCODING_TIME_MS = 50.0;
constexpr double MAX_RECOGNITION_TIME_MS = 30.0;
constexpr float MIN_MATCH_CONFIDENCE = 0.5f;
constexpr uint32_t FEATURE_DIM = 512;
constexpr uint32_t CONTEXT_DIM = 256;
constexpr uint32_t NUM_TEST_SCENES = 10;

/*=============================================================================
 * Helper Functions
 *===========================================================================*/

static void CreateTestFeatures(float* features, uint32_t dim, float base_value) {
    for (uint32_t i = 0; i < dim; i++) {
        features[i] = base_value + (float)i * 0.001f;
    }
}

/*=============================================================================
 * Scene Encoding Pipeline Tests
 *===========================================================================*/

class E2EParahippSceneEncodingTest : public ::testing::Test {
protected:
    nimcp_parahippocampal_t* parahipp = nullptr;
    float default_position[3];
    float default_heading;

    void SetUp() override {
        parahipp_config_t config = parahipp_default_config();
        config.enable_bio_async = false;
        config.enable_training = true;
        parahipp = parahipp_create(&config);
        ASSERT_NE(parahipp, nullptr);

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
};

TEST_F(E2EParahippSceneEncodingTest, SingleSceneEncoding) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t scene_id = 0;
    auto start = std::chrono::high_resolution_clock::now();
    int result = parahipp_encode_scene(parahipp, features, FEATURE_DIM,
        default_position, default_heading, "test_scene", &scene_id);
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(0, result);
    EXPECT_GE(scene_id, 0u);

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(elapsed_ms, MAX_ENCODING_TIME_MS);
}

TEST_F(E2EParahippSceneEncodingTest, MultipleScenesEncoding) {
    std::vector<uint32_t> scene_ids(NUM_TEST_SCENES);

    for (uint32_t i = 0; i < NUM_TEST_SCENES; i++) {
        float features[FEATURE_DIM];
        CreateTestFeatures(features, FEATURE_DIM, (float)i * 0.1f);

        float pos[3] = {(float)i * 10.0f, 0.0f, 0.0f};
        char name[32];
        snprintf(name, sizeof(name), "scene_%u", i);

        int result = parahipp_encode_scene(parahipp, features, FEATURE_DIM,
            pos, default_heading, name, &scene_ids[i]);
        EXPECT_EQ(0, result);
    }

    parahipp_stats_t stats;
    parahipp_get_stats(parahipp, &stats);
    EXPECT_GE(stats.scenes_encoded, NUM_TEST_SCENES);
}

TEST_F(E2EParahippSceneEncodingTest, SceneWithSpatialContext) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    float position[3] = {100.0f, 200.0f, 0.0f};
    float heading = 1.57f; /* 90 degrees */

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, features, FEATURE_DIM,
        position, heading, "spatial_scene", &scene_id);
    EXPECT_EQ(0, result);

    const nimcp_stored_scene_t* stored = parahipp_get_scene(parahipp, scene_id);
    EXPECT_NE(nullptr, stored);
}

TEST_F(E2EParahippSceneEncodingTest, SceneEncodingPersistence) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t scene_id = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features, FEATURE_DIM,
        default_position, default_heading, "persistent", &scene_id));

    const nimcp_stored_scene_t* stored = parahipp_get_scene(parahipp, scene_id);
    EXPECT_NE(nullptr, stored);
}

/*=============================================================================
 * Context Processing Tests
 *===========================================================================*/

class E2EParahippContextTest : public ::testing::Test {
protected:
    nimcp_parahippocampal_t* parahipp = nullptr;
    float default_position[3];
    float default_heading;

    void SetUp() override {
        parahipp_config_t config = parahipp_default_config();
        config.enable_bio_async = false;
        config.enable_training = true;
        parahipp = parahipp_create(&config);
        ASSERT_NE(parahipp, nullptr);

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
};

TEST_F(E2EParahippContextTest, ContextRetrieval) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t scene_id = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features, FEATURE_DIM,
        default_position, default_heading, "context_test", &scene_id));

    float context[CONTEXT_DIM];
    int result = parahipp_get_current_context(parahipp, context, CONTEXT_DIM);
    EXPECT_GE(result, 0);
}

TEST_F(E2EParahippContextTest, ContextStateTracking) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, FEATURE_DIM,
        default_position, default_heading, "context", &scene_id);
    parahipp_process_visual_input(parahipp, features, FEATURE_DIM);

    context_state_t state = parahipp_get_context_state(parahipp);
    EXPECT_GE((int)state, 0);
}

TEST_F(E2EParahippContextTest, ContextAfterMultipleScenes) {
    /* Encode multiple scenes */
    for (int i = 0; i < 5; i++) {
        float features[FEATURE_DIM];
        CreateTestFeatures(features, FEATURE_DIM, (float)i * 0.2f);

        float pos[3] = {(float)i * 10.0f, 0.0f, 0.0f};
        uint32_t scene_id = 0;
        parahipp_encode_scene(parahipp, features, FEATURE_DIM,
            pos, default_heading, nullptr, &scene_id);
    }

    float context[CONTEXT_DIM];
    int result = parahipp_get_current_context(parahipp, context, CONTEXT_DIM);
    EXPECT_GE(result, 0);
}

/*=============================================================================
 * Landmark Navigation Tests
 *===========================================================================*/

class E2EParahippLandmarkTest : public ::testing::Test {
protected:
    nimcp_parahippocampal_t* parahipp = nullptr;

    void SetUp() override {
        parahipp_config_t config = parahipp_default_config();
        config.enable_bio_async = false;
        config.enable_training = true;
        parahipp = parahipp_create(&config);
        ASSERT_NE(parahipp, nullptr);
    }

    void TearDown() override {
        if (parahipp) {
            parahipp_destroy(parahipp);
            parahipp = nullptr;
        }
    }
};

TEST_F(E2EParahippLandmarkTest, LandmarkAddition) {
    float position[3] = {10.0f, 20.0f, 0.0f};
    float features[128];
    CreateTestFeatures(features, 128, 0.5f);

    uint32_t landmark_id = 0;
    int result = parahipp_add_landmark(parahipp, features, 128, position, "landmark1", &landmark_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(landmark_id, 0u);
}

TEST_F(E2EParahippLandmarkTest, MultipleLandmarks) {
    for (int i = 0; i < 5; i++) {
        float position[3] = {(float)i * 10.0f, (float)i * 20.0f, 0.0f};
        float features[128];
        CreateTestFeatures(features, 128, (float)i * 0.1f);

        char name[32];
        snprintf(name, sizeof(name), "landmark_%d", i);

        uint32_t landmark_id = 0;
        int result = parahipp_add_landmark(parahipp, features, 128, position, name, &landmark_id);
        EXPECT_EQ(0, result);
    }
}

TEST_F(E2EParahippLandmarkTest, LandmarkRetrieval) {
    float position[3] = {10.0f, 20.0f, 0.0f};
    float features[128];
    CreateTestFeatures(features, 128, 0.5f);

    uint32_t landmark_id = 0;
    ASSERT_EQ(0, parahipp_add_landmark(parahipp, features, 128, position, "retrievable", &landmark_id));

    const nimcp_stored_landmark_t* landmark = parahipp_get_landmark(parahipp, landmark_id);
    EXPECT_NE(nullptr, landmark);
}

TEST_F(E2EParahippLandmarkTest, LandmarkBasedNavigation) {
    /* Add multiple landmarks */
    for (int i = 0; i < 3; i++) {
        float position[3] = {(float)i * 50.0f, 0.0f, 0.0f};
        float features[128];
        CreateTestFeatures(features, 128, (float)i * 0.3f);

        uint32_t id = 0;
        parahipp_add_landmark(parahipp, features, 128, position, nullptr, &id);
    }

    /* Process layout to integrate landmarks */
    float layout_features[FEATURE_DIM];
    CreateTestFeatures(layout_features, FEATURE_DIM, 0.5f);
    int result = parahipp_process_layout(parahipp, layout_features, FEATURE_DIM);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * Layout Recognition Tests
 *===========================================================================*/

class E2EParahippLayoutTest : public ::testing::Test {
protected:
    nimcp_parahippocampal_t* parahipp = nullptr;
    float default_position[3];
    float default_heading;

    void SetUp() override {
        parahipp_config_t config = parahipp_default_config();
        config.enable_bio_async = false;
        config.enable_training = true;
        parahipp = parahipp_create(&config);
        ASSERT_NE(parahipp, nullptr);

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
};

TEST_F(E2EParahippLayoutTest, LayoutProcessing) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    int result = parahipp_process_layout(parahipp, features, FEATURE_DIM);
    EXPECT_EQ(0, result);
}

TEST_F(E2EParahippLayoutTest, SceneRecognition) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t scene_id = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features, FEATURE_DIM,
        default_position, default_heading, "recognizable", &scene_id));

    parahipp_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    int rec_result = parahipp_recognize_scene(parahipp, features, FEATURE_DIM, &result);
    EXPECT_EQ(0, rec_result);
    EXPECT_GT(result.match_confidence, MIN_MATCH_CONFIDENCE);
}

TEST_F(E2EParahippLayoutTest, SimilarSceneRecognition) {
    float features1[FEATURE_DIM];
    CreateTestFeatures(features1, FEATURE_DIM, 0.5f);

    uint32_t scene_id = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features1, FEATURE_DIM,
        default_position, default_heading, "original", &scene_id));

    /* Slightly different features */
    float features2[FEATURE_DIM];
    CreateTestFeatures(features2, FEATURE_DIM, 0.51f);

    parahipp_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    parahipp_recognize_scene(parahipp, features2, FEATURE_DIM, &result);
    EXPECT_GE(result.match_confidence, 0.0f);
}

/*=============================================================================
 * Performance Benchmark Tests
 *===========================================================================*/

class E2EParahippBenchmarkTest : public ::testing::Test {
protected:
    nimcp_parahippocampal_t* parahipp = nullptr;
    float default_position[3];
    float default_heading;

    void SetUp() override {
        parahipp_config_t config = parahipp_default_config();
        config.enable_bio_async = false;
        parahipp = parahipp_create(&config);
        ASSERT_NE(parahipp, nullptr);

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
};

TEST_F(E2EParahippBenchmarkTest, SceneEncodingThroughput) {
    const uint32_t BENCHMARK_COUNT = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < BENCHMARK_COUNT; i++) {
        float features[FEATURE_DIM];
        CreateTestFeatures(features, FEATURE_DIM, (float)i * 0.01f);

        float pos[3] = {(float)i, 0.0f, 0.0f};
        uint32_t id = 0;
        parahipp_encode_scene(parahipp, features, FEATURE_DIM, pos, default_heading, nullptr, &id);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    double scenes_per_second = (BENCHMARK_COUNT * 1000.0) / elapsed_ms;
    EXPECT_GT(scenes_per_second, 50.0); /* At least 50 scenes/second */
}

TEST_F(E2EParahippBenchmarkTest, RecognitionLatency) {
    /* Pre-encode scenes */
    for (int i = 0; i < 50; i++) {
        float features[FEATURE_DIM];
        CreateTestFeatures(features, FEATURE_DIM, (float)i * 0.02f);

        float pos[3] = {(float)i * 10.0f, 0.0f, 0.0f};
        uint32_t id = 0;
        parahipp_encode_scene(parahipp, features, FEATURE_DIM, pos, default_heading, nullptr, &id);
    }

    /* Benchmark recognition */
    float query_features[FEATURE_DIM];
    CreateTestFeatures(query_features, FEATURE_DIM, 0.5f);

    auto start = std::chrono::high_resolution_clock::now();

    parahipp_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    parahipp_recognize_scene(parahipp, query_features, FEATURE_DIM, &result);

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_LT(elapsed_ms, MAX_RECOGNITION_TIME_MS);
}

/*=============================================================================
 * Main
 *===========================================================================*/

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
