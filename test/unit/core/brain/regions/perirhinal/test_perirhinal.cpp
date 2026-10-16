/**
 * @file test_perirhinal.cpp
 * @brief Unit tests for Perirhinal Cortex
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/perirhinal/nimcp_perirhinal.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class PerirhinalTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* pr = nullptr;

    void SetUp() override {
        perirhinal_config_t config = perirhinal_default_config();
        pr = perirhinal_create(&config);
        ASSERT_NE(pr, nullptr);
    }

    void TearDown() override {
        if (pr) {
            perirhinal_destroy(pr);
            pr = nullptr;
        }
    }

    // Helper to create test visual features
    void createTestFeatures(float* features, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base_value + sinf(i * 0.1f) * 0.5f;
        }
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, CreateWithDefaultConfig) {
    nimcp_perirhinal_t* p = perirhinal_create(nullptr);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->status, PERIRHINAL_STATUS_READY);
    EXPECT_EQ(p->num_object_cells, PERIRHINAL_DEFAULT_OBJECT_CELLS);
    EXPECT_EQ(p->num_familiarity_cells, PERIRHINAL_DEFAULT_FAMILIARITY_CELLS);
    perirhinal_destroy(p);
}

TEST_F(PerirhinalTest, CreateWithCustomConfig) {
    perirhinal_config_t config = perirhinal_default_config();
    config.num_object_cells = 256;
    config.num_familiarity_cells = 128;
    config.max_stored_objects = 512;

    nimcp_perirhinal_t* p = perirhinal_create(&config);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->num_object_cells, 256u);
    EXPECT_EQ(p->num_familiarity_cells, 128u);
    EXPECT_EQ(p->max_stored_objects, 512u);
    perirhinal_destroy(p);
}

TEST_F(PerirhinalTest, DestroyNull) {
    perirhinal_destroy(nullptr);
    SUCCEED();
}

TEST_F(PerirhinalTest, Reset) {
    // Add some state
    pr->updates_processed = 100;
    pr->objects_encoded = 50;
    pr->current_familiarity = 0.8f;

    EXPECT_EQ(perirhinal_reset(pr), 0);

    EXPECT_EQ(pr->updates_processed, 0u);
    EXPECT_EQ(pr->objects_encoded, 0u);
    EXPECT_FLOAT_EQ(pr->current_familiarity, 0.0f);
    EXPECT_EQ(pr->status, PERIRHINAL_STATUS_READY);
}

TEST_F(PerirhinalTest, ResetNull) {
    EXPECT_EQ(perirhinal_reset(nullptr), -1);
}

TEST_F(PerirhinalTest, Update) {
    EXPECT_EQ(perirhinal_update(pr, 0.01f), 0);
    EXPECT_EQ(pr->updates_processed, 1u);
}

TEST_F(PerirhinalTest, UpdateNull) {
    EXPECT_EQ(perirhinal_update(nullptr, 0.01f), -1);
}

TEST_F(PerirhinalTest, UpdateMultiple) {
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(perirhinal_update(pr, 0.01f), 0);
    }
    EXPECT_EQ(pr->updates_processed, 100u);
}

/*=============================================================================
 * OBJECT ENCODING TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, EncodeObject) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    EXPECT_EQ(perirhinal_encode_object(pr, features, 256, "TestObject", &object_id), 0);
    EXPECT_LT(object_id, pr->max_stored_objects);
    EXPECT_EQ(pr->num_stored_objects, 1u);
    EXPECT_EQ(pr->objects_encoded, 1u);
}

TEST_F(PerirhinalTest, EncodeObjectNull) {
    float features[256];
    uint32_t object_id;
    EXPECT_EQ(perirhinal_encode_object(nullptr, features, 256, "Test", &object_id), -1);
    EXPECT_EQ(perirhinal_encode_object(pr, nullptr, 256, "Test", &object_id), -1);
}

TEST_F(PerirhinalTest, EncodeObjectZeroDim) {
    float features[256];
    uint32_t object_id;
    EXPECT_EQ(perirhinal_encode_object(pr, features, 0, "Test", &object_id), -1);
}

TEST_F(PerirhinalTest, EncodeMultipleObjects) {
    float features[256];
    uint32_t ids[10];

    for (int i = 0; i < 10; i++) {
        createTestFeatures(features, 256, (float)i * 0.1f);
        char name[32];
        snprintf(name, sizeof(name), "Object%d", i);
        EXPECT_EQ(perirhinal_encode_object(pr, features, 256, name, &ids[i]), 0);
    }

    EXPECT_EQ(pr->num_stored_objects, 10u);

    // Verify all IDs are unique
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}

TEST_F(PerirhinalTest, GetObject) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "TestObject", &object_id);

    const nimcp_stored_object_t* obj = perirhinal_get_object(pr, object_id);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->object_id, object_id);
    EXPECT_STREQ(obj->name, "TestObject");
    EXPECT_EQ(obj->encounter_count, 1u);
}

TEST_F(PerirhinalTest, GetObjectInvalid) {
    EXPECT_EQ(perirhinal_get_object(pr, 99999), nullptr);
    EXPECT_EQ(perirhinal_get_object(nullptr, 0), nullptr);
}

TEST_F(PerirhinalTest, ForgetObject) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);
    EXPECT_EQ(pr->num_stored_objects, 1u);

    EXPECT_EQ(perirhinal_forget_object(pr, object_id), 0);
    EXPECT_EQ(pr->num_stored_objects, 0u);
    EXPECT_EQ(perirhinal_get_object(pr, object_id), nullptr);
}

TEST_F(PerirhinalTest, ForgetObjectInvalid) {
    EXPECT_EQ(perirhinal_forget_object(pr, 99999), -1);
    EXPECT_EQ(perirhinal_forget_object(nullptr, 0), -1);
}

TEST_F(PerirhinalTest, AddObjectView) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    float view2[256];
    createTestFeatures(view2, 256, 0.7f);

    EXPECT_EQ(perirhinal_add_object_view(pr, object_id, view2, 256), 0);

    const nimcp_stored_object_t* obj = perirhinal_get_object(pr, object_id);
    EXPECT_EQ(obj->num_views, 2u);
}

TEST_F(PerirhinalTest, AddObjectViewInvalid) {
    float features[256];
    EXPECT_EQ(perirhinal_add_object_view(pr, 99999, features, 256), -1);
    EXPECT_EQ(perirhinal_add_object_view(nullptr, 0, features, 256), -1);
}

/*=============================================================================
 * OBJECT RECOGNITION TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, RecognizeObject) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    // Recognize with same features
    perirhinal_recognition_result_t result;
    EXPECT_EQ(perirhinal_recognize_object(pr, features, 256, &result), 0);

    EXPECT_EQ(result.object_id, object_id);
    EXPECT_GT(result.match_confidence, 0.9f);  // Should be high match
    EXPECT_GE(result.confidence_level, RECOGNITION_CONFIDENCE_HIGH);
}

TEST_F(PerirhinalTest, RecognizeObjectNull) {
    float features[256];
    perirhinal_recognition_result_t result;
    EXPECT_EQ(perirhinal_recognize_object(nullptr, features, 256, &result), -1);
    EXPECT_EQ(perirhinal_recognize_object(pr, nullptr, 256, &result), -1);
    EXPECT_EQ(perirhinal_recognize_object(pr, features, 256, nullptr), -1);
}

TEST_F(PerirhinalTest, RecognizeSimilarObject) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    // Recognize with slightly different features
    float similar[256];
    for (int i = 0; i < 256; i++) {
        similar[i] = features[i] + 0.05f * ((float)rand() / RAND_MAX - 0.5f);
    }

    perirhinal_recognition_result_t result;
    perirhinal_recognize_object(pr, similar, 256, &result);

    EXPECT_EQ(result.object_id, object_id);
    EXPECT_GT(result.match_confidence, 0.5f);  // Should still match
}

TEST_F(PerirhinalTest, RecognizeDifferentObject) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    // Try to recognize very different features
    float different[256];
    createTestFeatures(different, 256, -0.5f);  // Very different

    perirhinal_recognition_result_t result;
    perirhinal_recognize_object(pr, different, 256, &result);

    EXPECT_LT(result.match_confidence, 0.5f);  // Should be low match
}

TEST_F(PerirhinalTest, RecognizeNoObjects) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    perirhinal_recognition_result_t result;
    perirhinal_recognize_object(pr, features, 256, &result);

    EXPECT_EQ(result.confidence_level, RECOGNITION_CONFIDENCE_NONE);
}

/*=============================================================================
 * FAMILIARITY TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, ComputeFamiliarity) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    // Before encoding - should be unfamiliar
    float initial_fam = perirhinal_compute_familiarity(pr, features, 256);
    EXPECT_LT(initial_fam, 0.1f);

    // After encoding
    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    // Update familiarity multiple times
    for (int i = 0; i < 10; i++) {
        perirhinal_update_familiarity(pr, object_id);
    }

    float after_fam = perirhinal_compute_familiarity(pr, features, 256);
    EXPECT_GT(after_fam, initial_fam);
}

TEST_F(PerirhinalTest, ComputeFamiliarityNull) {
    float features[256];
    EXPECT_FLOAT_EQ(perirhinal_compute_familiarity(nullptr, features, 256), 0.0f);
    EXPECT_FLOAT_EQ(perirhinal_compute_familiarity(pr, nullptr, 256), 0.0f);
}

TEST_F(PerirhinalTest, ClassifyFamiliarity) {
    EXPECT_EQ(perirhinal_classify_familiarity(pr, 0.1f), FAMILIARITY_TYPE_NOVEL);
    EXPECT_EQ(perirhinal_classify_familiarity(pr, 0.3f), FAMILIARITY_TYPE_SEEN_BEFORE);
    EXPECT_EQ(perirhinal_classify_familiarity(pr, 0.5f), FAMILIARITY_TYPE_FAMILIAR);
    EXPECT_EQ(perirhinal_classify_familiarity(pr, 0.7f), FAMILIARITY_TYPE_VERY_FAMILIAR);
    EXPECT_EQ(perirhinal_classify_familiarity(pr, 0.9f), FAMILIARITY_TYPE_KNOWN);
}

TEST_F(PerirhinalTest, IsFamiliar) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    // Not familiar before encoding
    EXPECT_FALSE(perirhinal_is_familiar(pr, features, 256));

    // Encode and increase familiarity
    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    for (int i = 0; i < 20; i++) {
        perirhinal_update_familiarity(pr, object_id);
    }

    EXPECT_TRUE(perirhinal_is_familiar(pr, features, 256));
}

TEST_F(PerirhinalTest, GetObjectFamiliarity) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    float initial = perirhinal_get_object_familiarity(pr, object_id);
    EXPECT_GT(initial, 0.0f);

    perirhinal_update_familiarity(pr, object_id);

    float updated = perirhinal_get_object_familiarity(pr, object_id);
    EXPECT_GT(updated, initial);
}

TEST_F(PerirhinalTest, GetObjectFamiliarityInvalid) {
    EXPECT_FLOAT_EQ(perirhinal_get_object_familiarity(pr, 99999), 0.0f);
    EXPECT_FLOAT_EQ(perirhinal_get_object_familiarity(nullptr, 0), 0.0f);
}

TEST_F(PerirhinalTest, UpdateFamiliarity) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    const nimcp_stored_object_t* obj = perirhinal_get_object(pr, object_id);
    uint32_t initial_count = obj->encounter_count;

    EXPECT_EQ(perirhinal_update_familiarity(pr, object_id), 0);

    EXPECT_EQ(obj->encounter_count, initial_count + 1);
}

/*=============================================================================
 * NOVELTY TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, ComputeNovelty) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    // Novel before any encoding
    float novelty = perirhinal_compute_novelty(pr, features, 256);
    EXPECT_GT(novelty, 0.5f);

    // Encode the object
    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    // Update familiarity
    for (int i = 0; i < 10; i++) {
        perirhinal_update_familiarity(pr, object_id);
    }

    // Less novel after becoming familiar
    float novelty_after = perirhinal_compute_novelty(pr, features, 256);
    EXPECT_LT(novelty_after, novelty);
}

TEST_F(PerirhinalTest, ComputeNoveltyNull) {
    float features[256];
    EXPECT_FLOAT_EQ(perirhinal_compute_novelty(nullptr, features, 256), 1.0f);
    EXPECT_FLOAT_EQ(perirhinal_compute_novelty(pr, nullptr, 256), 1.0f);
}

TEST_F(PerirhinalTest, IsNovel) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    // Novel before encoding
    EXPECT_TRUE(perirhinal_is_novel(pr, features, 256));

    // Encode and familiarize
    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    for (int i = 0; i < 20; i++) {
        perirhinal_update_familiarity(pr, object_id);
    }

    // Should no longer be novel
    EXPECT_FALSE(perirhinal_is_novel(pr, features, 256));
}

TEST_F(PerirhinalTest, GetSurpriseSignal) {
    float surprise = perirhinal_get_surprise_signal(pr);
    EXPECT_GE(surprise, 0.0f);
}

TEST_F(PerirhinalTest, Habituate) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    EXPECT_EQ(perirhinal_habituate(pr, features, 256), 0);

    // Habituation should reduce novelty response
    float novelty_before = perirhinal_compute_novelty(pr, features, 256);

    for (int i = 0; i < 10; i++) {
        perirhinal_habituate(pr, features, 256);
    }

    // After habituation, novelty cells should have adapted
    for (uint32_t i = 0; i < pr->num_novelty_cells; i++) {
        EXPECT_GT(pr->novelty_cells[i].habituation_state, 0.0f);
    }
}

TEST_F(PerirhinalTest, HabituateNull) {
    float features[256];
    EXPECT_EQ(perirhinal_habituate(nullptr, features, 256), -1);
    EXPECT_EQ(perirhinal_habituate(pr, nullptr, 256), -1);
}

/*=============================================================================
 * RECENCY TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, UpdateRecency) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    EXPECT_EQ(perirhinal_update_recency(pr, object_id), 0);

    float recency = perirhinal_get_recency_signal(pr, object_id);
    EXPECT_FLOAT_EQ(recency, 1.0f);  // Just updated
}

TEST_F(PerirhinalTest, UpdateRecencyInvalid) {
    EXPECT_EQ(perirhinal_update_recency(pr, 99999), -1);
    EXPECT_EQ(perirhinal_update_recency(nullptr, 0), -1);
}

TEST_F(PerirhinalTest, GetRecencySignal) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    float recency = perirhinal_get_recency_signal(pr, object_id);
    EXPECT_FLOAT_EQ(recency, 1.0f);  // Just encoded
}

TEST_F(PerirhinalTest, GetRecencySignalInvalid) {
    EXPECT_FLOAT_EQ(perirhinal_get_recency_signal(pr, 99999), 0.0f);
    EXPECT_FLOAT_EQ(perirhinal_get_recency_signal(nullptr, 0), 0.0f);
}

TEST_F(PerirhinalTest, DecayRecency) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    float initial_recency = perirhinal_get_recency_signal(pr, object_id);

    // Decay over time
    for (int i = 0; i < 100; i++) {
        perirhinal_decay_recency(pr, 1.0f);  // 1 second per update
    }

    float decayed_recency = perirhinal_get_recency_signal(pr, object_id);
    EXPECT_LT(decayed_recency, initial_recency);
}

TEST_F(PerirhinalTest, DecayRecencyNull) {
    EXPECT_EQ(perirhinal_decay_recency(nullptr, 0.01f), -1);
}

TEST_F(PerirhinalTest, GetTimeSinceEncounter) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    uint64_t time_since = perirhinal_get_time_since_encounter(pr, object_id);
    EXPECT_LT(time_since, 1000u);  // Should be very recent
}

TEST_F(PerirhinalTest, GetTimeSinceEncounterInvalid) {
    EXPECT_EQ(perirhinal_get_time_since_encounter(pr, 99999), UINT64_MAX);
    EXPECT_EQ(perirhinal_get_time_since_encounter(nullptr, 0), UINT64_MAX);
}

/*=============================================================================
 * SEMANTIC ASSOCIATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, CreateAssociation) {
    float features1[256], features2[256];
    createTestFeatures(features1, 256, 0.5f);
    createTestFeatures(features2, 256, 0.7f);

    uint32_t id1, id2;
    perirhinal_encode_object(pr, features1, 256, "Object1", &id1);
    perirhinal_encode_object(pr, features2, 256, "Object2", &id2);

    EXPECT_EQ(perirhinal_create_association(pr, id1, id2, 0.8f), 0);

    const nimcp_stored_object_t* obj1 = perirhinal_get_object(pr, id1);
    EXPECT_EQ(obj1->num_associations, 1u);
}

TEST_F(PerirhinalTest, CreateAssociationInvalid) {
    EXPECT_EQ(perirhinal_create_association(pr, 99999, 0, 0.5f), -1);
    EXPECT_EQ(perirhinal_create_association(nullptr, 0, 0, 0.5f), -1);
}

TEST_F(PerirhinalTest, GetAssociations) {
    float features1[256], features2[256], features3[256];
    createTestFeatures(features1, 256, 0.5f);
    createTestFeatures(features2, 256, 0.7f);
    createTestFeatures(features3, 256, 0.9f);

    uint32_t id1, id2, id3;
    perirhinal_encode_object(pr, features1, 256, "Object1", &id1);
    perirhinal_encode_object(pr, features2, 256, "Object2", &id2);
    perirhinal_encode_object(pr, features3, 256, "Object3", &id3);

    perirhinal_create_association(pr, id1, id2, 0.8f);
    perirhinal_create_association(pr, id1, id3, 0.6f);

    uint32_t associated[10];
    float strengths[10];
    uint32_t num_found;

    EXPECT_EQ(perirhinal_get_associations(pr, id1, associated, strengths, 10, &num_found), 0);
    EXPECT_EQ(num_found, 2u);
}

TEST_F(PerirhinalTest, StrengthenAssociation) {
    float features1[256], features2[256];
    createTestFeatures(features1, 256, 0.5f);
    createTestFeatures(features2, 256, 0.7f);

    uint32_t id1, id2;
    perirhinal_encode_object(pr, features1, 256, "Object1", &id1);
    perirhinal_encode_object(pr, features2, 256, "Object2", &id2);

    perirhinal_create_association(pr, id1, id2, 0.5f);

    EXPECT_EQ(perirhinal_strengthen_association(pr, id1, id2, 0.2f), 0);

    uint32_t associated[10];
    float strengths[10];
    uint32_t num_found;
    perirhinal_get_associations(pr, id1, associated, strengths, 10, &num_found);

    EXPECT_FLOAT_EQ(strengths[0], 0.7f);
}

/*=============================================================================
 * CONTEXT BINDING TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, BindSpatialContext) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    float context[3] = {10.0f, 20.0f, 0.0f};
    EXPECT_EQ(perirhinal_bind_spatial_context(pr, object_id, context, 3), 0);

    const nimcp_stored_object_t* obj = perirhinal_get_object(pr, object_id);
    EXPECT_EQ(obj->spatial_dim, 3u);
    EXPECT_GT(obj->context_binding_strength, 0.0f);
}

TEST_F(PerirhinalTest, BindSpatialContextInvalid) {
    float context[3] = {0};
    EXPECT_EQ(perirhinal_bind_spatial_context(pr, 99999, context, 3), -1);
    EXPECT_EQ(perirhinal_bind_spatial_context(nullptr, 0, context, 3), -1);
    EXPECT_EQ(perirhinal_bind_spatial_context(pr, 0, nullptr, 3), -1);
}

TEST_F(PerirhinalTest, GetSpatialContext) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    uint32_t object_id;
    perirhinal_encode_object(pr, features, 256, "Test", &object_id);

    float context[3] = {10.0f, 20.0f, 30.0f};
    perirhinal_bind_spatial_context(pr, object_id, context, 3);

    float retrieved[3];
    int result = perirhinal_get_spatial_context(pr, object_id, retrieved, 3);
    EXPECT_EQ(result, 3);
    EXPECT_FLOAT_EQ(retrieved[0], 10.0f);
    EXPECT_FLOAT_EQ(retrieved[1], 20.0f);
    EXPECT_FLOAT_EQ(retrieved[2], 30.0f);
}

TEST_F(PerirhinalTest, FindByContext) {
    float features1[256], features2[256];
    createTestFeatures(features1, 256, 0.5f);
    createTestFeatures(features2, 256, 0.7f);

    uint32_t id1, id2;
    perirhinal_encode_object(pr, features1, 256, "Object1", &id1);
    perirhinal_encode_object(pr, features2, 256, "Object2", &id2);

    float context1[3] = {10.0f, 10.0f, 0.0f};
    float context2[3] = {50.0f, 50.0f, 0.0f};
    perirhinal_bind_spatial_context(pr, id1, context1, 3);
    perirhinal_bind_spatial_context(pr, id2, context2, 3);

    uint32_t found_ids[10];
    float strengths[10];
    uint32_t num_found;

    // Search near first context
    float query[3] = {11.0f, 11.0f, 0.0f};
    EXPECT_EQ(perirhinal_find_by_context(pr, query, 3, found_ids, strengths, 10, &num_found), 0);
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, GetStatus) {
    EXPECT_EQ(perirhinal_get_status(pr), PERIRHINAL_STATUS_READY);
}

TEST_F(PerirhinalTest, GetStatusNull) {
    EXPECT_EQ(perirhinal_get_status(nullptr), PERIRHINAL_STATUS_ERROR);
}

TEST_F(PerirhinalTest, GetLastError) {
    EXPECT_EQ(perirhinal_get_last_error(pr), PERIRHINAL_ERROR_NONE);
}

TEST_F(PerirhinalTest, GetLastErrorNull) {
    EXPECT_EQ(perirhinal_get_last_error(nullptr), PERIRHINAL_ERROR_INTERNAL);
}

TEST_F(PerirhinalTest, ErrorString) {
    EXPECT_STREQ(perirhinal_error_string(PERIRHINAL_ERROR_NONE), "No error");
    EXPECT_STREQ(perirhinal_error_string(PERIRHINAL_ERROR_INVALID_INPUT), "Invalid input");
    EXPECT_STREQ(perirhinal_error_string(PERIRHINAL_ERROR_OBJECT_NOT_FOUND), "Object not found");
    EXPECT_STREQ(perirhinal_error_string(PERIRHINAL_ERROR_MEMORY_FULL), "Memory full");
}

TEST_F(PerirhinalTest, StatusString) {
    EXPECT_STREQ(perirhinal_status_string(PERIRHINAL_STATUS_IDLE), "Idle");
    EXPECT_STREQ(perirhinal_status_string(PERIRHINAL_STATUS_READY), "Ready");
    EXPECT_STREQ(perirhinal_status_string(PERIRHINAL_STATUS_ENCODING), "Encoding");
    EXPECT_STREQ(perirhinal_status_string(PERIRHINAL_STATUS_RECOGNIZING), "Recognizing");
}

TEST_F(PerirhinalTest, GetStats) {
    perirhinal_stats_t stats;
    EXPECT_EQ(perirhinal_get_stats(pr, &stats), 0);
    EXPECT_EQ(stats.total_stored_objects, 0u);
}

TEST_F(PerirhinalTest, GetStatsNull) {
    perirhinal_stats_t stats;
    EXPECT_EQ(perirhinal_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(perirhinal_get_stats(pr, nullptr), -1);
}

TEST_F(PerirhinalTest, GetConfig) {
    perirhinal_config_t config;
    EXPECT_EQ(perirhinal_get_config(pr, &config), 0);
    EXPECT_EQ(config.num_object_cells, PERIRHINAL_DEFAULT_OBJECT_CELLS);
}

TEST_F(PerirhinalTest, GetConfigNull) {
    perirhinal_config_t config;
    EXPECT_EQ(perirhinal_get_config(nullptr, &config), -1);
    EXPECT_EQ(perirhinal_get_config(pr, nullptr), -1);
}

TEST_F(PerirhinalTest, GetHealthStatus) {
    float health = perirhinal_get_health_status(pr);
    EXPECT_GT(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(PerirhinalTest, GetHealthStatusNull) {
    EXPECT_FLOAT_EQ(perirhinal_get_health_status(nullptr), 0.0f);
}

TEST_F(PerirhinalTest, LogDiagnostics) {
    EXPECT_EQ(perirhinal_log_diagnostics(pr), 0);
}

TEST_F(PerirhinalTest, LogDiagnosticsNull) {
    EXPECT_EQ(perirhinal_log_diagnostics(nullptr), -1);
}

/*=============================================================================
 * CELL ACTIVITY TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, GetObjectCellActivity) {
    float activity[100];
    size_t count = perirhinal_get_object_cell_activity(pr, activity, 100);
    EXPECT_GT(count, 0u);
    EXPECT_LE(count, 100u);
}

TEST_F(PerirhinalTest, GetObjectCellActivityNull) {
    float activity[100];
    EXPECT_EQ(perirhinal_get_object_cell_activity(nullptr, activity, 100), 0u);
    EXPECT_EQ(perirhinal_get_object_cell_activity(pr, nullptr, 100), 0u);
}

TEST_F(PerirhinalTest, GetFamiliarityCellActivity) {
    float activity[100];
    size_t count = perirhinal_get_familiarity_cell_activity(pr, activity, 100);
    EXPECT_GT(count, 0u);
}

TEST_F(PerirhinalTest, GetCurrentFamiliarity) {
    float fam = perirhinal_get_current_familiarity(pr);
    EXPECT_GE(fam, 0.0f);
}

TEST_F(PerirhinalTest, GetCurrentNovelty) {
    float nov = perirhinal_get_current_novelty(pr);
    EXPECT_GE(nov, 0.0f);
}

/*=============================================================================
 * VISUAL INPUT PROCESSING TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, ProcessVisualInput) {
    float features[256];
    createTestFeatures(features, 256, 0.5f);

    EXPECT_EQ(perirhinal_process_visual_input(pr, features, 256), 0);

    // Should have updated current familiarity and novelty
    float fam = perirhinal_get_current_familiarity(pr);
    float nov = perirhinal_get_current_novelty(pr);
    EXPECT_GE(fam, 0.0f);
    EXPECT_GE(nov, 0.0f);
}

TEST_F(PerirhinalTest, ProcessVisualInputNull) {
    float features[256];
    EXPECT_EQ(perirhinal_process_visual_input(nullptr, features, 256), -1);
    EXPECT_EQ(perirhinal_process_visual_input(pr, nullptr, 256), -1);
}

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, ProcessIncoming) {
    EXPECT_EQ(perirhinal_process_incoming(pr), 0);
}

TEST_F(PerirhinalTest, ProcessIncomingNull) {
    EXPECT_EQ(perirhinal_process_incoming(nullptr), -1);
}

TEST_F(PerirhinalTest, SendOutgoing) {
    EXPECT_EQ(perirhinal_send_outgoing(pr), 0);
}

TEST_F(PerirhinalTest, SendOutgoingNull) {
    EXPECT_EQ(perirhinal_send_outgoing(nullptr), -1);
}

TEST_F(PerirhinalTest, BidirectionalUpdate) {
    EXPECT_EQ(perirhinal_bidirectional_update(pr, 0.01f), 0);
}

TEST_F(PerirhinalTest, BidirectionalUpdateNull) {
    EXPECT_EQ(perirhinal_bidirectional_update(nullptr, 0.01f), -1);
}

/*=============================================================================
 * SERIALIZATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, GetSerializationSize) {
    size_t size = perirhinal_get_serialization_size(pr);
    EXPECT_GT(size, 0u);
}

TEST_F(PerirhinalTest, GetSerializationSizeNull) {
    EXPECT_EQ(perirhinal_get_serialization_size(nullptr), 0u);
}

TEST_F(PerirhinalTest, Serialize) {
    size_t size = perirhinal_get_serialization_size(pr);
    uint8_t* buffer = new uint8_t[size];
    size_t written;

    EXPECT_EQ(perirhinal_serialize(pr, buffer, size, &written), 0);
    EXPECT_GT(written, 0u);

    delete[] buffer;
}

TEST_F(PerirhinalTest, SerializeNull) {
    uint8_t buffer[1024];
    size_t written;
    EXPECT_EQ(perirhinal_serialize(nullptr, buffer, 1024, &written), -1);
    EXPECT_EQ(perirhinal_serialize(pr, nullptr, 1024, &written), -1);
    EXPECT_EQ(perirhinal_serialize(pr, buffer, 1024, nullptr), -1);
}

/*=============================================================================
 * BRIDGE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalTest, InitEntorhinalBridge) {
    EXPECT_EQ(perirhinal_init_entorhinal_bridge(pr, nullptr), 0);
    EXPECT_FLOAT_EQ(pr->entorhinal_bridge.spatial_context_weight, 0.5f);
}

TEST_F(PerirhinalTest, InitEntorhinalBridgeNull) {
    EXPECT_EQ(perirhinal_init_entorhinal_bridge(nullptr, nullptr), -1);
}

TEST_F(PerirhinalTest, InitSecurityBridge) {
    EXPECT_EQ(perirhinal_init_security_bridge(pr, nullptr, nullptr), 0);
    EXPECT_EQ(pr->security_bridge.access_level, 1u);
}

TEST_F(PerirhinalTest, InitImmuneBridge) {
    EXPECT_EQ(perirhinal_init_immune_bridge(pr, nullptr), 0);
    EXPECT_FLOAT_EQ(pr->immune_bridge.health_score, 1.0f);
}

TEST_F(PerirhinalTest, InitBioAsyncBridge) {
    EXPECT_EQ(perirhinal_init_bio_async_bridge(pr, nullptr), 0);
}

TEST_F(PerirhinalTest, InitSnnBridge) {
    EXPECT_EQ(perirhinal_init_snn_bridge(pr, nullptr), 0);
}

TEST_F(PerirhinalTest, InitPlasticityBridge) {
    EXPECT_EQ(perirhinal_init_plasticity_bridge(pr, nullptr, nullptr), 0);
    EXPECT_FLOAT_EQ(pr->plasticity_bridge.learning_rate, pr->config.learning_rate);
}

TEST_F(PerirhinalTest, InitCognitiveBridge) {
    EXPECT_EQ(perirhinal_init_cognitive_bridge(pr, nullptr, nullptr, nullptr), 0);
}

TEST_F(PerirhinalTest, InitTrainingBridge) {
    EXPECT_EQ(perirhinal_init_training_bridge(pr, nullptr), 0);
}

TEST_F(PerirhinalTest, InitSubstrateBridge) {
    EXPECT_EQ(perirhinal_init_substrate_bridge(pr, nullptr), 0);
    EXPECT_FLOAT_EQ(pr->substrate_bridge.atp_level, 1.0f);
}

TEST_F(PerirhinalTest, InitResonanceBridge) {
    EXPECT_EQ(perirhinal_init_resonance_bridge(pr, nullptr), 0);
}

TEST_F(PerirhinalTest, InitThalamicBridge) {
    EXPECT_EQ(perirhinal_init_thalamic_bridge(pr, nullptr), 0);
    EXPECT_FLOAT_EQ(pr->thalamic_bridge.relay_gain, 1.0f);
}

TEST_F(PerirhinalTest, InitHippocampusBridge) {
    EXPECT_EQ(perirhinal_init_hippocampus_bridge(pr, nullptr), 0);
}

TEST_F(PerirhinalTest, InitPerceptionBridge) {
    EXPECT_EQ(perirhinal_init_perception_bridge(pr, nullptr), 0);
}

TEST_F(PerirhinalTest, SyncEntorhinal) {
    EXPECT_EQ(perirhinal_sync_entorhinal(pr), 0);
}

TEST_F(PerirhinalTest, SyncEntorhinalNull) {
    EXPECT_EQ(perirhinal_sync_entorhinal(nullptr), -1);
}

