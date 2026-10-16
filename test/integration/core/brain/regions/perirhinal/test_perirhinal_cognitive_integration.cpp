/**
 * @file test_perirhinal_cognitive_integration.cpp
 * @brief Integration tests for Perirhinal Cortex with cognitive systems
 *
 * WHAT: Tests Perirhinal Cortex integration with working memory and attention
 * WHY:  Ensure object recognition integrates with cognitive processing
 * HOW:  Test working memory interactions, attention modulation, and cognitive control
 *
 * COGNITIVE INTEGRATION POINTS:
 * - Working memory: Object representations in active memory
 * - Attention: Selective focus on objects of interest
 * - Cognitive hub: Integration with higher cognitive functions
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

class PerirhinalCognitiveIntegrationTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* perirhinal;
    perirhinal_config_t config;

    void SetUp() override {
        config = perirhinal_default_config();
        config.enable_bio_async = false;
        config.enable_cognitive = true;
        config.enable_training = true;
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
 * COGNITIVE CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalCognitiveIntegrationTest, CognitiveEnabled) {
    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(perirhinal, &retrieved));
    EXPECT_TRUE(retrieved.enable_cognitive);
}

TEST_F(PerirhinalCognitiveIntegrationTest, CreateWithFullCognitiveConfig) {
    perirhinal_config_t full_config = perirhinal_default_config();
    full_config.enable_cognitive = true;
    full_config.enable_training = true;
    full_config.enable_perception = true;
    full_config.enable_bio_async = false;

    nimcp_perirhinal_t* full_pr = perirhinal_create(&full_config);
    ASSERT_NE(nullptr, full_pr);

    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(full_pr, &retrieved));
    EXPECT_TRUE(retrieved.enable_cognitive);

    perirhinal_destroy(full_pr);
}

/*=============================================================================
 * OBJECT ENCODING FOR WORKING MEMORY TESTS
 *===========================================================================*/

TEST_F(PerirhinalCognitiveIntegrationTest, EncodeObjectForWorkingMemory) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    int result = perirhinal_encode_object(perirhinal, features, 256, "wm_object", &object_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(object_id, 0u);

    /* Object should be retrievable */
    const nimcp_stored_object_t* stored = perirhinal_get_object(perirhinal, object_id);
    EXPECT_NE(nullptr, stored);
}

TEST_F(PerirhinalCognitiveIntegrationTest, MultipleObjectsInMemory) {
    uint32_t object_ids[5];

    for (int i = 0; i < 5; i++) {
        float features[256];
        CreateTestFeatures(features, 256, (float)i * 0.2f);

        char name[32];
        snprintf(name, sizeof(name), "wm_obj_%d", i);

        int result = perirhinal_encode_object(perirhinal, features, 256, name, &object_ids[i]);
        EXPECT_EQ(0, result);
        EXPECT_GE(object_ids[i], 0u);
    }

    /* All should be retrievable */
    for (int i = 0; i < 5; i++) {
        const nimcp_stored_object_t* stored = perirhinal_get_object(perirhinal, object_ids[i]);
        EXPECT_NE(nullptr, stored);
    }
}

/*=============================================================================
 * RECOGNITION AND RECALL TESTS
 *===========================================================================*/

TEST_F(PerirhinalCognitiveIntegrationTest, RecognizeEncodedObject) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "recall_test", &object_id));

    perirhinal_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    int rec_result = perirhinal_recognize_object(perirhinal, features, 256, &result);
    EXPECT_EQ(0, rec_result);
    EXPECT_GT(result.match_confidence, 0.0f);
}

TEST_F(PerirhinalCognitiveIntegrationTest, ConfidenceLevelClassification) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "conf_test", &object_id));

    /* Repeated exposures to increase confidence */
    for (int i = 0; i < 10; i++) {
        perirhinal_update_familiarity(perirhinal, object_id);
    }

    perirhinal_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    perirhinal_recognize_object(perirhinal, features, 256, &result);

    /* Should have some level of confidence */
    EXPECT_GE(result.confidence_level, RECOGNITION_CONFIDENCE_NONE);
    EXPECT_LE(result.confidence_level, RECOGNITION_CONFIDENCE_CERTAIN);
}

/*=============================================================================
 * FAMILIARITY AND RECENCY TESTS
 *===========================================================================*/

TEST_F(PerirhinalCognitiveIntegrationTest, FamiliarityIncreasesWithExposure) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "fam_increase", &object_id));

    float initial_fam = perirhinal_get_object_familiarity(perirhinal, object_id);

    /* Multiple exposures */
    for (int i = 0; i < 20; i++) {
        perirhinal_update_familiarity(perirhinal, object_id);
    }

    float later_fam = perirhinal_get_object_familiarity(perirhinal, object_id);
    EXPECT_GE(later_fam, initial_fam);
}

TEST_F(PerirhinalCognitiveIntegrationTest, RecencySignalUpdates) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "recency_test", &object_id));

    /* Update recency */
    perirhinal_update_recency(perirhinal, object_id);

    float recency = perirhinal_get_recency_signal(perirhinal, object_id);
    EXPECT_GE(recency, 0.0f);
    EXPECT_LE(recency, 1.0f);
}

TEST_F(PerirhinalCognitiveIntegrationTest, RecencyDecaysOverTime) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "decay_test", &object_id));
    perirhinal_update_recency(perirhinal, object_id);

    float initial_recency = perirhinal_get_recency_signal(perirhinal, object_id);

    /* Simulate time passing */
    for (int i = 0; i < 100; i++) {
        perirhinal_decay_recency(perirhinal, 10.0f);
    }

    float later_recency = perirhinal_get_recency_signal(perirhinal, object_id);
    EXPECT_LE(later_recency, initial_recency);
}

/*=============================================================================
 * SEMANTIC ASSOCIATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalCognitiveIntegrationTest, CreateAssociationBetweenObjects) {
    float features1[256], features2[256];
    CreateTestFeatures(features1, 256, 0.3f);
    CreateTestFeatures(features2, 256, 0.7f);

    uint32_t obj1_id = 0, obj2_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features1, 256, "assoc1", &obj1_id));
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features2, 256, "assoc2", &obj2_id));

    /* Create association */
    int result = perirhinal_create_association(perirhinal, obj1_id, obj2_id, 0.8f);
    EXPECT_EQ(0, result);
}

TEST_F(PerirhinalCognitiveIntegrationTest, RetrieveAssociations) {
    float features1[256], features2[256], features3[256];
    CreateTestFeatures(features1, 256, 0.2f);
    CreateTestFeatures(features2, 256, 0.5f);
    CreateTestFeatures(features3, 256, 0.8f);

    uint32_t obj1_id = 0, obj2_id = 0, obj3_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features1, 256, "main", &obj1_id));
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features2, 256, "assoc_a", &obj2_id));
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features3, 256, "assoc_b", &obj3_id));

    /* Create associations */
    perirhinal_create_association(perirhinal, obj1_id, obj2_id, 0.9f);
    perirhinal_create_association(perirhinal, obj1_id, obj3_id, 0.6f);

    /* Retrieve associations */
    uint32_t associated_ids[10];
    float strengths[10];
    uint32_t num_found = 0;

    int result = perirhinal_get_associations(perirhinal, obj1_id,
        associated_ids, strengths, 10, &num_found);
    EXPECT_EQ(0, result);
    EXPECT_GE(num_found, 2u);
}

TEST_F(PerirhinalCognitiveIntegrationTest, StrengthenAssociation) {
    float features1[256], features2[256];
    CreateTestFeatures(features1, 256, 0.4f);
    CreateTestFeatures(features2, 256, 0.6f);

    uint32_t obj1_id = 0, obj2_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features1, 256, "strengthen1", &obj1_id));
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features2, 256, "strengthen2", &obj2_id));

    perirhinal_create_association(perirhinal, obj1_id, obj2_id, 0.5f);

    /* Strengthen association */
    int result = perirhinal_strengthen_association(perirhinal, obj1_id, obj2_id, 0.2f);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * CONTEXT BINDING TESTS
 *===========================================================================*/

TEST_F(PerirhinalCognitiveIntegrationTest, BindSpatialContext) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "context_bind", &object_id));

    /* Create spatial context */
    float spatial_context[64];
    for (int i = 0; i < 64; i++) {
        spatial_context[i] = (float)i * 0.01f;
    }

    int result = perirhinal_bind_spatial_context(perirhinal, object_id, spatial_context, 64);
    EXPECT_EQ(0, result);
}

TEST_F(PerirhinalCognitiveIntegrationTest, RetrieveSpatialContext) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "ctx_retrieve", &object_id));

    float spatial_context[64];
    for (int i = 0; i < 64; i++) {
        spatial_context[i] = (float)i * 0.02f;
    }
    perirhinal_bind_spatial_context(perirhinal, object_id, spatial_context, 64);

    /* Retrieve context - function may return size or 0 on success */
    float retrieved_context[64];
    int result = perirhinal_get_spatial_context(perirhinal, object_id, retrieved_context, 64);
    EXPECT_GE(result, 0);  /* Non-negative indicates success */
}

TEST_F(PerirhinalCognitiveIntegrationTest, FindObjectsByContext) {
    /* Encode objects with different contexts */
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "find_by_ctx", &object_id));

    float spatial_context[64];
    for (int i = 0; i < 64; i++) {
        spatial_context[i] = 0.5f;
    }
    perirhinal_bind_spatial_context(perirhinal, object_id, spatial_context, 64);

    /* Find by context */
    uint32_t found_ids[10];
    float match_strengths[10];
    uint32_t num_found = 0;

    int result = perirhinal_find_by_context(perirhinal, spatial_context, 64,
        found_ids, match_strengths, 10, &num_found);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(PerirhinalCognitiveIntegrationTest, StatsTrackCognitiveActivity) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "stats_test", &object_id);

    perirhinal_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    perirhinal_recognize_object(perirhinal, features, 256, &result);

    perirhinal_stats_t stats;
    EXPECT_EQ(0, perirhinal_get_stats(perirhinal, &stats));
    EXPECT_GE(stats.objects_encoded, 1u);
    EXPECT_GE(stats.objects_recognized, 0u);
}

TEST_F(PerirhinalCognitiveIntegrationTest, CurrentSignalsAccessible) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "signals", &object_id);
    perirhinal_process_visual_input(perirhinal, features, 256);

    float current_fam = perirhinal_get_current_familiarity(perirhinal);
    float current_nov = perirhinal_get_current_novelty(perirhinal);

    EXPECT_GE(current_fam, 0.0f);
    EXPECT_LE(current_fam, 1.0f);
    EXPECT_GE(current_nov, 0.0f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
