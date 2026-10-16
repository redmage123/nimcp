/**
 * @file test_parahippocampal.cpp
 * @brief Unit tests for Parahippocampal Cortex
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/parahippocampal/nimcp_parahippocampal.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ParahippocampalTest : public ::testing::Test {
protected:
    nimcp_parahippocampal_t* ph = nullptr;

    void SetUp() override {
        parahipp_config_t config = parahipp_default_config();
        ph = parahipp_create(&config);
        ASSERT_NE(ph, nullptr);
    }

    void TearDown() override {
        if (ph) {
            parahipp_destroy(ph);
            ph = nullptr;
        }
    }

    void createTestFeatures(float* features, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base_value + sinf(i * 0.1f) * 0.5f;
        }
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, CreateWithDefaultConfig) {
    nimcp_parahippocampal_t* p = parahipp_create(nullptr);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->status, PARAHIPP_STATUS_READY);
    EXPECT_EQ(p->num_place_cells, PARAHIPP_DEFAULT_PLACE_CELLS);
    EXPECT_EQ(p->num_scene_cells, PARAHIPP_DEFAULT_SCENE_CELLS);
    parahipp_destroy(p);
}

TEST_F(ParahippocampalTest, CreateWithCustomConfig) {
    parahipp_config_t config = parahipp_default_config();
    config.num_place_cells = 128;
    config.num_scene_cells = 256;
    config.max_stored_scenes = 256;

    nimcp_parahippocampal_t* p = parahipp_create(&config);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->num_place_cells, 128u);
    EXPECT_EQ(p->num_scene_cells, 256u);
    EXPECT_EQ(p->max_stored_scenes, 256u);
    parahipp_destroy(p);
}

TEST_F(ParahippocampalTest, DestroyNull) {
    parahipp_destroy(nullptr);
    SUCCEED();
}

TEST_F(ParahippocampalTest, Reset) {
    ph->updates_processed = 100;
    ph->scenes_encoded = 50;
    ph->context_switches = 10;

    EXPECT_EQ(parahipp_reset(ph), 0);

    EXPECT_EQ(ph->updates_processed, 0u);
    EXPECT_EQ(ph->scenes_encoded, 0u);
    EXPECT_EQ(ph->context_switches, 0u);
    EXPECT_EQ(ph->status, PARAHIPP_STATUS_READY);
}

TEST_F(ParahippocampalTest, ResetNull) {
    EXPECT_EQ(parahipp_reset(nullptr), -1);
}

TEST_F(ParahippocampalTest, Update) {
    EXPECT_EQ(parahipp_update(ph, 0.01f), 0);
    EXPECT_EQ(ph->updates_processed, 1u);
}

TEST_F(ParahippocampalTest, UpdateNull) {
    EXPECT_EQ(parahipp_update(nullptr, 0.01f), -1);
}

TEST_F(ParahippocampalTest, UpdateMultiple) {
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(parahipp_update(ph, 0.01f), 0);
    }
    EXPECT_EQ(ph->updates_processed, 100u);
}

/*=============================================================================
 * SCENE ENCODING TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, EncodeScene) {
    float features[512];
    createTestFeatures(features, 512, 0.5f);
    float position[3] = {10.0f, 20.0f, 0.0f};

    uint32_t scene_id;
    EXPECT_EQ(parahipp_encode_scene(ph, features, 512, position, 0.0f, "TestScene", &scene_id), 0);
    EXPECT_LT(scene_id, ph->max_stored_scenes);
    EXPECT_EQ(ph->num_stored_scenes, 1u);
    EXPECT_EQ(ph->scenes_encoded, 1u);
}

TEST_F(ParahippocampalTest, EncodeSceneNull) {
    float features[512];
    float position[3] = {0};
    uint32_t scene_id;
    EXPECT_EQ(parahipp_encode_scene(nullptr, features, 512, position, 0.0f, "Test", &scene_id), -1);
    EXPECT_EQ(parahipp_encode_scene(ph, nullptr, 512, position, 0.0f, "Test", &scene_id), -1);
}

TEST_F(ParahippocampalTest, EncodeSceneZeroDim) {
    float features[512];
    float position[3] = {0};
    uint32_t scene_id;
    EXPECT_EQ(parahipp_encode_scene(ph, features, 0, position, 0.0f, "Test", &scene_id), -1);
}

TEST_F(ParahippocampalTest, EncodeMultipleScenes) {
    float features[512];
    float position[3] = {0};
    uint32_t ids[10];

    for (int i = 0; i < 10; i++) {
        createTestFeatures(features, 512, (float)i * 0.1f);
        position[0] = (float)i * 10.0f;
        char name[32];
        snprintf(name, sizeof(name), "Scene%d", i);
        EXPECT_EQ(parahipp_encode_scene(ph, features, 512, position, 0.0f, name, &ids[i]), 0);
    }

    EXPECT_EQ(ph->num_stored_scenes, 10u);

    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}

TEST_F(ParahippocampalTest, GetScene) {
    float features[512];
    createTestFeatures(features, 512, 0.5f);
    float position[3] = {10.0f, 20.0f, 0.0f};

    uint32_t scene_id;
    parahipp_encode_scene(ph, features, 512, position, 1.57f, "TestScene", &scene_id);

    const nimcp_stored_scene_t* scene = parahipp_get_scene(ph, scene_id);
    ASSERT_NE(scene, nullptr);
    EXPECT_EQ(scene->scene_id, scene_id);
    EXPECT_STREQ(scene->name, "TestScene");
    EXPECT_FLOAT_EQ(scene->heading, 1.57f);
    EXPECT_EQ(scene->visit_count, 1u);
}

TEST_F(ParahippocampalTest, GetSceneInvalid) {
    EXPECT_EQ(parahipp_get_scene(ph, 99999), nullptr);
    EXPECT_EQ(parahipp_get_scene(nullptr, 0), nullptr);
}

TEST_F(ParahippocampalTest, ForgetScene) {
    float features[512];
    createTestFeatures(features, 512, 0.5f);
    float position[3] = {0};

    uint32_t scene_id;
    parahipp_encode_scene(ph, features, 512, position, 0.0f, "Test", &scene_id);
    EXPECT_EQ(ph->num_stored_scenes, 1u);

    EXPECT_EQ(parahipp_forget_scene(ph, scene_id), 0);
    EXPECT_EQ(ph->num_stored_scenes, 0u);
    EXPECT_EQ(parahipp_get_scene(ph, scene_id), nullptr);
}

TEST_F(ParahippocampalTest, ForgetSceneInvalid) {
    EXPECT_EQ(parahipp_forget_scene(ph, 99999), -1);
    EXPECT_EQ(parahipp_forget_scene(nullptr, 0), -1);
}

TEST_F(ParahippocampalTest, AddSceneView) {
    float features[512];
    createTestFeatures(features, 512, 0.5f);
    float position[3] = {0};

    uint32_t scene_id;
    parahipp_encode_scene(ph, features, 512, position, 0.0f, "Test", &scene_id);

    float view2[512];
    createTestFeatures(view2, 512, 0.7f);
    EXPECT_EQ(parahipp_add_scene_view(ph, scene_id, view2, 512, 1.57f), 0);

    const nimcp_stored_scene_t* scene = parahipp_get_scene(ph, scene_id);
    EXPECT_EQ(scene->num_views, 2u);
}

TEST_F(ParahippocampalTest, AddSceneViewInvalid) {
    float features[512];
    EXPECT_EQ(parahipp_add_scene_view(ph, 99999, features, 512, 0.0f), -1);
    EXPECT_EQ(parahipp_add_scene_view(nullptr, 0, features, 512, 0.0f), -1);
}

TEST_F(ParahippocampalTest, UpdateSceneVisit) {
    float features[512];
    createTestFeatures(features, 512, 0.5f);
    float position[3] = {0};

    uint32_t scene_id;
    parahipp_encode_scene(ph, features, 512, position, 0.0f, "Test", &scene_id);

    const nimcp_stored_scene_t* scene = parahipp_get_scene(ph, scene_id);
    uint32_t initial_visits = scene->visit_count;
    float initial_familiarity = scene->familiarity;

    EXPECT_EQ(parahipp_update_scene_visit(ph, scene_id), 0);

    EXPECT_EQ(scene->visit_count, initial_visits + 1);
    EXPECT_GT(scene->familiarity, initial_familiarity);
}

/*=============================================================================
 * SCENE RECOGNITION TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, RecognizeScene) {
    float features[512];
    createTestFeatures(features, 512, 0.5f);
    float position[3] = {10.0f, 20.0f, 0.0f};

    uint32_t scene_id;
    parahipp_encode_scene(ph, features, 512, position, 0.0f, "Test", &scene_id);

    parahipp_recognition_result_t result;
    EXPECT_EQ(parahipp_recognize_scene(ph, features, 512, &result), 0);

    EXPECT_EQ(result.scene_id, scene_id);
    EXPECT_GT(result.match_confidence, 0.9f);
    EXPECT_FALSE(result.is_novel);
}

TEST_F(ParahippocampalTest, RecognizeSceneNull) {
    float features[512];
    parahipp_recognition_result_t result;
    EXPECT_EQ(parahipp_recognize_scene(nullptr, features, 512, &result), -1);
    EXPECT_EQ(parahipp_recognize_scene(ph, nullptr, 512, &result), -1);
    EXPECT_EQ(parahipp_recognize_scene(ph, features, 512, nullptr), -1);
}

TEST_F(ParahippocampalTest, RecognizeSimilarScene) {
    float features[512];
    createTestFeatures(features, 512, 0.5f);
    float position[3] = {0};

    uint32_t scene_id;
    parahipp_encode_scene(ph, features, 512, position, 0.0f, "Test", &scene_id);

    float similar[512];
    for (int i = 0; i < 512; i++) {
        similar[i] = features[i] + 0.05f * ((float)rand() / RAND_MAX - 0.5f);
    }

    parahipp_recognition_result_t result;
    parahipp_recognize_scene(ph, similar, 512, &result);

    EXPECT_EQ(result.scene_id, scene_id);
    EXPECT_GT(result.match_confidence, 0.5f);
}

TEST_F(ParahippocampalTest, RecognizeNovelScene) {
    float features[512];
    createTestFeatures(features, 512, 0.5f);

    parahipp_recognition_result_t result;
    parahipp_recognize_scene(ph, features, 512, &result);

    EXPECT_TRUE(result.is_novel);
    EXPECT_EQ(result.context_state, CONTEXT_STATE_NOVEL);
}

/*=============================================================================
 * PLACE CELL TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, UpdatePlaceCells) {
    float position[3] = {50.0f, 50.0f, 0.0f};
    EXPECT_EQ(parahipp_update_place_cells(ph, position, 3), 0);

    uint32_t active_ids[100];
    float activations[100];
    uint32_t num_active;
    parahipp_get_active_place_cells(ph, active_ids, activations, 100, &num_active);

    EXPECT_GT(num_active, 0u);
}

TEST_F(ParahippocampalTest, UpdatePlaceCellsNull) {
    float position[3] = {0};
    EXPECT_EQ(parahipp_update_place_cells(nullptr, position, 3), -1);
    EXPECT_EQ(parahipp_update_place_cells(ph, nullptr, 3), -1);
}

TEST_F(ParahippocampalTest, GetPlacePopulationVector) {
    float position[3] = {50.0f, 50.0f, 0.0f};
    parahipp_update_place_cells(ph, position, 3);

    float vector[3];
    uint32_t dim;
    EXPECT_EQ(parahipp_get_place_population_vector(ph, vector, &dim), 0);
    EXPECT_EQ(dim, 3u);
}

TEST_F(ParahippocampalTest, DecodePosition) {
    float position[3] = {50.0f, 50.0f, 0.0f};
    parahipp_update_place_cells(ph, position, 3);

    float decoded[3];
    float confidence;
    EXPECT_EQ(parahipp_decode_position(ph, decoded, &confidence), 0);
    EXPECT_GE(confidence, 0.0f);
}

TEST_F(ParahippocampalTest, GetActivePlaceCells) {
    float position[3] = {50.0f, 50.0f, 0.0f};
    parahipp_update_place_cells(ph, position, 3);

    uint32_t cell_ids[100];
    float activations[100];
    uint32_t num_active;
    EXPECT_EQ(parahipp_get_active_place_cells(ph, cell_ids, activations, 100, &num_active), 0);
}

/*=============================================================================
 * SPATIAL LAYOUT TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, ProcessLayout) {
    float boundaries[360];
    for (int i = 0; i < 360; i++) {
        boundaries[i] = 10.0f + 5.0f * cosf(i * 0.0174533f);
    }

    EXPECT_EQ(parahipp_process_layout(ph, boundaries, 360), 0);
}

TEST_F(ParahippocampalTest, ProcessLayoutNull) {
    float boundaries[360];
    EXPECT_EQ(parahipp_process_layout(nullptr, boundaries, 360), -1);
    EXPECT_EQ(parahipp_process_layout(ph, nullptr, 360), -1);
}

TEST_F(ParahippocampalTest, GetLayoutType) {
    float boundaries[360];
    for (int i = 0; i < 360; i++) {
        boundaries[i] = 10.0f;  // Circular
    }
    parahipp_process_layout(ph, boundaries, 360);

    layout_type_t type = parahipp_get_layout_type(ph);
    EXPECT_NE(type, LAYOUT_TYPE_UNKNOWN);
}

TEST_F(ParahippocampalTest, GetLayoutTypeNull) {
    EXPECT_EQ(parahipp_get_layout_type(nullptr), LAYOUT_TYPE_UNKNOWN);
}

TEST_F(ParahippocampalTest, GetOpenness) {
    float boundaries[360];
    for (int i = 0; i < 360; i++) {
        boundaries[i] = 50.0f;  // Large space
    }
    parahipp_process_layout(ph, boundaries, 360);

    float openness = parahipp_get_openness(ph);
    EXPECT_GT(openness, 0.0f);
}

TEST_F(ParahippocampalTest, GetOpennessNull) {
    EXPECT_FLOAT_EQ(parahipp_get_openness(nullptr), 0.0f);
}

TEST_F(ParahippocampalTest, GetNavigability) {
    float boundaries[360];
    for (int i = 0; i < 360; i++) {
        boundaries[i] = 10.0f;
    }
    parahipp_process_layout(ph, boundaries, 360);

    float nav = parahipp_get_navigability(ph);
    EXPECT_GE(nav, 0.0f);
}

TEST_F(ParahippocampalTest, GetLayoutFeatures) {
    float boundaries[360];
    for (int i = 0; i < 360; i++) {
        boundaries[i] = 10.0f;
    }
    parahipp_process_layout(ph, boundaries, 360);

    float features[128];
    int result = parahipp_get_layout_features(ph, features, 128);
    EXPECT_GE(result, 0);
}

/*=============================================================================
 * CONTEXT TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, GetCurrentContext) {
    float context[256];
    int result = parahipp_get_current_context(ph, context, 256);
    EXPECT_GE(result, 0);
}

TEST_F(ParahippocampalTest, GetCurrentContextNull) {
    float context[256];
    EXPECT_EQ(parahipp_get_current_context(nullptr, context, 256), -1);
    EXPECT_EQ(parahipp_get_current_context(ph, nullptr, 256), -1);
}

TEST_F(ParahippocampalTest, SetContext) {
    float context[256];
    for (int i = 0; i < 256; i++) {
        context[i] = sinf(i * 0.1f);
    }

    EXPECT_EQ(parahipp_set_context(ph, context, 256), 0);
}

TEST_F(ParahippocampalTest, SetContextNull) {
    float context[256];
    EXPECT_EQ(parahipp_set_context(nullptr, context, 256), -1);
    EXPECT_EQ(parahipp_set_context(ph, nullptr, 256), -1);
}

TEST_F(ParahippocampalTest, DetectContextChange) {
    bool change = parahipp_detect_context_change(ph);
    // Initial state should not detect change
    EXPECT_FALSE(change);
}

TEST_F(ParahippocampalTest, GetContextStability) {
    float stability = parahipp_get_context_stability(ph);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);
}

TEST_F(ParahippocampalTest, GetContextState) {
    context_state_t state = parahipp_get_context_state(ph);
    // Should return a valid state
    EXPECT_GE((int)state, 0);
    EXPECT_LT((int)state, 5);
}

TEST_F(ParahippocampalTest, BindContextToScene) {
    float features[512];
    createTestFeatures(features, 512, 0.5f);
    float position[3] = {0};

    uint32_t scene_id;
    parahipp_encode_scene(ph, features, 512, position, 0.0f, "Test", &scene_id);

    float context[256];
    for (int i = 0; i < 256; i++) {
        context[i] = sinf(i * 0.1f);
    }
    parahipp_set_context(ph, context, 256);

    EXPECT_EQ(parahipp_bind_context_to_scene(ph, scene_id), 0);
}

TEST_F(ParahippocampalTest, BindContextToSceneInvalid) {
    EXPECT_EQ(parahipp_bind_context_to_scene(ph, 99999), -1);
    EXPECT_EQ(parahipp_bind_context_to_scene(nullptr, 0), -1);
}

/*=============================================================================
 * LANDMARK TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, AddLandmark) {
    float features[128];
    createTestFeatures(features, 128, 0.5f);
    float position[3] = {100.0f, 100.0f, 0.0f};

    uint32_t landmark_id;
    EXPECT_EQ(parahipp_add_landmark(ph, features, 128, position, "Tower", &landmark_id), 0);
    EXPECT_LT(landmark_id, ph->max_landmarks);
    EXPECT_EQ(ph->num_stored_landmarks, 1u);
}

TEST_F(ParahippocampalTest, AddLandmarkNull) {
    float features[128];
    float position[3] = {0};
    uint32_t id;
    EXPECT_EQ(parahipp_add_landmark(nullptr, features, 128, position, "Test", &id), -1);
    EXPECT_EQ(parahipp_add_landmark(ph, nullptr, 128, position, "Test", &id), -1);
    EXPECT_EQ(parahipp_add_landmark(ph, features, 128, nullptr, "Test", &id), -1);
}

TEST_F(ParahippocampalTest, GetLandmark) {
    float features[128];
    createTestFeatures(features, 128, 0.5f);
    float position[3] = {100.0f, 100.0f, 0.0f};

    uint32_t landmark_id;
    parahipp_add_landmark(ph, features, 128, position, "Tower", &landmark_id);

    const nimcp_stored_landmark_t* lm = parahipp_get_landmark(ph, landmark_id);
    ASSERT_NE(lm, nullptr);
    EXPECT_EQ(lm->landmark_id, landmark_id);
    EXPECT_STREQ(lm->name, "Tower");
}

TEST_F(ParahippocampalTest, GetLandmarkInvalid) {
    EXPECT_EQ(parahipp_get_landmark(ph, 99999), nullptr);
    EXPECT_EQ(parahipp_get_landmark(nullptr, 0), nullptr);
}

TEST_F(ParahippocampalTest, RecognizeLandmarks) {
    float features[128];
    createTestFeatures(features, 128, 0.5f);
    float position[3] = {100.0f, 100.0f, 0.0f};

    uint32_t landmark_id;
    parahipp_add_landmark(ph, features, 128, position, "Tower", &landmark_id);

    uint32_t recognized_ids[10];
    float confidences[10];
    uint32_t num_recognized;

    EXPECT_EQ(parahipp_recognize_landmarks(ph, features, 128,
        recognized_ids, confidences, 10, &num_recognized), 0);
    EXPECT_EQ(num_recognized, 1u);
    EXPECT_EQ(recognized_ids[0], landmark_id);
}

TEST_F(ParahippocampalTest, GetLandmarkBearing) {
    float features[128];
    createTestFeatures(features, 128, 0.5f);
    float position[3] = {100.0f, 100.0f, 0.0f};

    uint32_t landmark_id;
    parahipp_add_landmark(ph, features, 128, position, "Tower", &landmark_id);

    float from_pos[3] = {0.0f, 0.0f, 0.0f};
    float bearing = parahipp_get_landmark_bearing(ph, landmark_id, from_pos);

    // Should be ~45 degrees (pi/4)
    EXPECT_GT(bearing, 0.0f);
    EXPECT_LT(bearing, 1.6f);
}

TEST_F(ParahippocampalTest, UpdateLandmarkCells) {
    float features[128];
    createTestFeatures(features, 128, 0.5f);
    float position[3] = {100.0f, 100.0f, 0.0f};

    uint32_t landmark_id;
    parahipp_add_landmark(ph, features, 128, position, "Tower", &landmark_id);

    // Assign landmark to a cell
    if (ph->num_landmark_cells > 0) {
        ph->landmark_cells[0].landmark_id = landmark_id;
        ph->stored_landmarks[landmark_id].visibility_range = 200.0f;
    }

    float current_pos[3] = {50.0f, 50.0f, 0.0f};
    EXPECT_EQ(parahipp_update_landmark_cells(ph, current_pos), 0);
}

TEST_F(ParahippocampalTest, UpdateLandmarkCellsNull) {
    float pos[3] = {0};
    EXPECT_EQ(parahipp_update_landmark_cells(nullptr, pos), -1);
    EXPECT_EQ(parahipp_update_landmark_cells(ph, nullptr), -1);
}

/*=============================================================================
 * SCENE-OBJECT BINDING TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, BindObjectsToScene) {
    float features[512];
    createTestFeatures(features, 512, 0.5f);
    float position[3] = {0};

    uint32_t scene_id;
    parahipp_encode_scene(ph, features, 512, position, 0.0f, "Test", &scene_id);

    uint32_t object_ids[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(parahipp_bind_objects_to_scene(ph, scene_id, object_ids, 5), 0);

    const nimcp_stored_scene_t* scene = parahipp_get_scene(ph, scene_id);
    EXPECT_EQ(scene->num_objects, 5u);
}

TEST_F(ParahippocampalTest, BindObjectsToSceneInvalid) {
    uint32_t objects[] = {1, 2, 3};
    EXPECT_EQ(parahipp_bind_objects_to_scene(ph, 99999, objects, 3), -1);
    EXPECT_EQ(parahipp_bind_objects_to_scene(nullptr, 0, objects, 3), -1);
}

TEST_F(ParahippocampalTest, GetSceneObjects) {
    float features[512];
    createTestFeatures(features, 512, 0.5f);
    float position[3] = {0};

    uint32_t scene_id;
    parahipp_encode_scene(ph, features, 512, position, 0.0f, "Test", &scene_id);

    uint32_t object_ids[] = {10, 20, 30};
    parahipp_bind_objects_to_scene(ph, scene_id, object_ids, 3);

    uint32_t retrieved[10];
    uint32_t num_objects;
    EXPECT_EQ(parahipp_get_scene_objects(ph, scene_id, retrieved, 10, &num_objects), 0);
    EXPECT_EQ(num_objects, 3u);
    EXPECT_EQ(retrieved[0], 10u);
}

TEST_F(ParahippocampalTest, FindScenesWithObject) {
    float features[512];
    float position[3] = {0};

    // Create multiple scenes with same object
    uint32_t scene1, scene2;
    createTestFeatures(features, 512, 0.5f);
    parahipp_encode_scene(ph, features, 512, position, 0.0f, "Scene1", &scene1);
    createTestFeatures(features, 512, 0.7f);
    parahipp_encode_scene(ph, features, 512, position, 0.0f, "Scene2", &scene2);

    uint32_t object_id = 42;
    uint32_t objects[] = {object_id};
    parahipp_bind_objects_to_scene(ph, scene1, objects, 1);
    parahipp_bind_objects_to_scene(ph, scene2, objects, 1);

    uint32_t found_scenes[10];
    uint32_t num_found;
    EXPECT_EQ(parahipp_find_scenes_with_object(ph, object_id, found_scenes, 10, &num_found), 0);
    EXPECT_EQ(num_found, 2u);
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, GetStatus) {
    EXPECT_EQ(parahipp_get_status(ph), PARAHIPP_STATUS_READY);
}

TEST_F(ParahippocampalTest, GetStatusNull) {
    EXPECT_EQ(parahipp_get_status(nullptr), PARAHIPP_STATUS_ERROR);
}

TEST_F(ParahippocampalTest, GetLastError) {
    EXPECT_EQ(parahipp_get_last_error(ph), PARAHIPP_ERROR_NONE);
}

TEST_F(ParahippocampalTest, GetLastErrorNull) {
    EXPECT_EQ(parahipp_get_last_error(nullptr), PARAHIPP_ERROR_INTERNAL);
}

TEST_F(ParahippocampalTest, ErrorString) {
    EXPECT_STREQ(parahipp_error_string(PARAHIPP_ERROR_NONE), "No error");
    EXPECT_STREQ(parahipp_error_string(PARAHIPP_ERROR_INVALID_INPUT), "Invalid input");
    EXPECT_STREQ(parahipp_error_string(PARAHIPP_ERROR_SCENE_NOT_FOUND), "Scene not found");
    EXPECT_STREQ(parahipp_error_string(PARAHIPP_ERROR_MEMORY_FULL), "Memory full");
}

TEST_F(ParahippocampalTest, StatusString) {
    EXPECT_STREQ(parahipp_status_string(PARAHIPP_STATUS_IDLE), "Idle");
    EXPECT_STREQ(parahipp_status_string(PARAHIPP_STATUS_READY), "Ready");
    EXPECT_STREQ(parahipp_status_string(PARAHIPP_STATUS_SCENE_ENCODING), "Encoding scene");
    EXPECT_STREQ(parahipp_status_string(PARAHIPP_STATUS_SCENE_RECOGNIZING), "Recognizing scene");
}

TEST_F(ParahippocampalTest, GetStats) {
    parahipp_stats_t stats;
    EXPECT_EQ(parahipp_get_stats(ph, &stats), 0);
    EXPECT_EQ(stats.total_stored_scenes, 0u);
}

TEST_F(ParahippocampalTest, GetStatsNull) {
    parahipp_stats_t stats;
    EXPECT_EQ(parahipp_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(parahipp_get_stats(ph, nullptr), -1);
}

TEST_F(ParahippocampalTest, GetConfig) {
    parahipp_config_t config;
    EXPECT_EQ(parahipp_get_config(ph, &config), 0);
    EXPECT_EQ(config.num_place_cells, PARAHIPP_DEFAULT_PLACE_CELLS);
}

TEST_F(ParahippocampalTest, GetConfigNull) {
    parahipp_config_t config;
    EXPECT_EQ(parahipp_get_config(nullptr, &config), -1);
    EXPECT_EQ(parahipp_get_config(ph, nullptr), -1);
}

TEST_F(ParahippocampalTest, GetHealthStatus) {
    float health = parahipp_get_health_status(ph);
    EXPECT_GT(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(ParahippocampalTest, GetHealthStatusNull) {
    EXPECT_FLOAT_EQ(parahipp_get_health_status(nullptr), 0.0f);
}

TEST_F(ParahippocampalTest, LogDiagnostics) {
    EXPECT_EQ(parahipp_log_diagnostics(ph), 0);
}

TEST_F(ParahippocampalTest, LogDiagnosticsNull) {
    EXPECT_EQ(parahipp_log_diagnostics(nullptr), -1);
}

/*=============================================================================
 * CELL ACTIVITY TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, GetPlaceCellActivity) {
    float activity[100];
    size_t count = parahipp_get_place_cell_activity(ph, activity, 100);
    EXPECT_GT(count, 0u);
    EXPECT_LE(count, 100u);
}

TEST_F(ParahippocampalTest, GetPlaceCellActivityNull) {
    float activity[100];
    EXPECT_EQ(parahipp_get_place_cell_activity(nullptr, activity, 100), 0u);
    EXPECT_EQ(parahipp_get_place_cell_activity(ph, nullptr, 100), 0u);
}

TEST_F(ParahippocampalTest, GetSceneCellActivity) {
    float activity[100];
    size_t count = parahipp_get_scene_cell_activity(ph, activity, 100);
    EXPECT_GT(count, 0u);
}

TEST_F(ParahippocampalTest, GetContextCellActivity) {
    float activity[100];
    size_t count = parahipp_get_context_cell_activity(ph, activity, 100);
    EXPECT_GT(count, 0u);
}

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, ProcessIncoming) {
    EXPECT_EQ(parahipp_process_incoming(ph), 0);
}

TEST_F(ParahippocampalTest, ProcessIncomingNull) {
    EXPECT_EQ(parahipp_process_incoming(nullptr), -1);
}

TEST_F(ParahippocampalTest, SendOutgoing) {
    EXPECT_EQ(parahipp_send_outgoing(ph), 0);
}

TEST_F(ParahippocampalTest, SendOutgoingNull) {
    EXPECT_EQ(parahipp_send_outgoing(nullptr), -1);
}

TEST_F(ParahippocampalTest, BidirectionalUpdate) {
    EXPECT_EQ(parahipp_bidirectional_update(ph, 0.01f), 0);
}

TEST_F(ParahippocampalTest, BidirectionalUpdateNull) {
    EXPECT_EQ(parahipp_bidirectional_update(nullptr, 0.01f), -1);
}

TEST_F(ParahippocampalTest, ProcessVisualInput) {
    float features[512];
    createTestFeatures(features, 512, 0.5f);

    EXPECT_EQ(parahipp_process_visual_input(ph, features, 512), 0);
}

TEST_F(ParahippocampalTest, ProcessVisualInputNull) {
    float features[512];
    EXPECT_EQ(parahipp_process_visual_input(nullptr, features, 512), -1);
    EXPECT_EQ(parahipp_process_visual_input(ph, nullptr, 512), -1);
}

/*=============================================================================
 * BRIDGE INTEGRATION TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, SyncEntorhinal) {
    EXPECT_EQ(parahipp_sync_entorhinal(ph), 0);
}

TEST_F(ParahippocampalTest, SyncEntorhinalNull) {
    EXPECT_EQ(parahipp_sync_entorhinal(nullptr), -1);
}

TEST_F(ParahippocampalTest, SyncPerirhinal) {
    EXPECT_EQ(parahipp_sync_perirhinal(ph), 0);
}

TEST_F(ParahippocampalTest, SyncPerirhinalNull) {
    EXPECT_EQ(parahipp_sync_perirhinal(nullptr), -1);
}

TEST_F(ParahippocampalTest, SendToEntorhinal) {
    EXPECT_EQ(parahipp_send_to_entorhinal(ph), 0);
}

TEST_F(ParahippocampalTest, SendToPerirhinal) {
    EXPECT_EQ(parahipp_send_to_perirhinal(ph), 0);
}

/*=============================================================================
 * BRIDGE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, InitEntorhinalBridge) {
    EXPECT_EQ(parahipp_init_entorhinal_bridge(ph, nullptr), 0);
    EXPECT_FLOAT_EQ(ph->entorhinal_bridge.grid_cell_input_weight, 0.5f);
}

TEST_F(ParahippocampalTest, InitPerirhinalBridge) {
    EXPECT_EQ(parahipp_init_perirhinal_bridge(ph, nullptr), 0);
    EXPECT_FLOAT_EQ(ph->perirhinal_bridge.object_context_weight, 0.5f);
}

TEST_F(ParahippocampalTest, InitSecurityBridge) {
    EXPECT_EQ(parahipp_init_security_bridge(ph, nullptr, nullptr), 0);
    EXPECT_EQ(ph->security_bridge.access_level, 1u);
}

TEST_F(ParahippocampalTest, InitImmuneBridge) {
    EXPECT_EQ(parahipp_init_immune_bridge(ph, nullptr), 0);
    EXPECT_FLOAT_EQ(ph->immune_bridge.health_score, 1.0f);
}

TEST_F(ParahippocampalTest, InitSnnBridge) {
    EXPECT_EQ(parahipp_init_snn_bridge(ph, nullptr), 0);
}

TEST_F(ParahippocampalTest, InitSubstrateBridge) {
    EXPECT_EQ(parahipp_init_substrate_bridge(ph, nullptr), 0);
    EXPECT_FLOAT_EQ(ph->substrate_bridge.atp_level, 1.0f);
}

TEST_F(ParahippocampalTest, InitHippocampusBridge) {
    EXPECT_EQ(parahipp_init_hippocampus_bridge(ph, nullptr), 0);
    EXPECT_FLOAT_EQ(ph->hippocampus_bridge.place_cell_coupling, 0.8f);
}

/*=============================================================================
 * SERIALIZATION TESTS
 *===========================================================================*/

TEST_F(ParahippocampalTest, GetSerializationSize) {
    size_t size = parahipp_get_serialization_size(ph);
    EXPECT_GT(size, 0u);
}

TEST_F(ParahippocampalTest, GetSerializationSizeNull) {
    EXPECT_EQ(parahipp_get_serialization_size(nullptr), 0u);
}

TEST_F(ParahippocampalTest, Serialize) {
    size_t size = parahipp_get_serialization_size(ph);
    uint8_t* buffer = new uint8_t[size];
    size_t written;

    EXPECT_EQ(parahipp_serialize(ph, buffer, size, &written), 0);
    EXPECT_GT(written, 0u);

    delete[] buffer;
}

TEST_F(ParahippocampalTest, SerializeNull) {
    uint8_t buffer[1024];
    size_t written;
    EXPECT_EQ(parahipp_serialize(nullptr, buffer, 1024, &written), -1);
    EXPECT_EQ(parahipp_serialize(ph, nullptr, 1024, &written), -1);
    EXPECT_EQ(parahipp_serialize(ph, buffer, 1024, nullptr), -1);
}

