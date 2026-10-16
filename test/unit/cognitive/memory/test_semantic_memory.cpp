/**
 * @file test_semantic_memory.cpp
 * @brief Unit tests for Phase M4 Semantic Memory Network
 *
 * WHAT: Comprehensive unit tests for semantic memory system
 * WHY:  Ensure 100% test coverage and NIMCP compliance
 * HOW:  Test all API functions with normal, edge, and error cases
 *
 * @version Phase M4
 * @date 2025-11-13
 */

#include <gtest/gtest.h>

#include "cognitive/memory/nimcp_semantic_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
// Test Fixtures
//=============================================================================

class SemanticMemoryTest : public ::testing::Test {
protected:
    semantic_memory_system_t* system;

    void SetUp() override {
        system = semantic_memory_create();
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        semantic_memory_destroy(system);
    }

    // Helper: Create test features
    void create_test_features(float* features, uint32_t dim, float value) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = value + (i * 0.1f);
        }
    }
};

//=============================================================================
// System Management Tests
//=============================================================================

TEST_F(SemanticMemoryTest, CreateSystem) {
    // System already created in SetUp
    EXPECT_NE(system, nullptr);
}

TEST_F(SemanticMemoryTest, DestroyNullSystem) {
    // Should not crash
    semantic_memory_destroy(nullptr);
    SUCCEED();
}

TEST_F(SemanticMemoryTest, ResetSystem) {
    // Create some concepts
    float features[32];
    create_test_features(features, 32, 0.5f);

    uint64_t id1 = semantic_memory_create_concept(
        system, features, 32, "test1", CONCEPT_OBJECT
    );
    EXPECT_NE(id1, 0);

    // Reset
    semantic_memory_reset(system);

    // Verify cleared
    semantic_memory_stats_t stats;
    semantic_memory_get_statistics(system, &stats);
    EXPECT_EQ(stats.concept_count, 0);
    EXPECT_EQ(stats.relation_count, 0);
}

TEST_F(SemanticMemoryTest, ResetNullSystem) {
    // Should not crash
    semantic_memory_reset(nullptr);
    SUCCEED();
}

//=============================================================================
// Integration API Tests
//=============================================================================

TEST_F(SemanticMemoryTest, SetConsolidation) {
    void* dummy_consolidation = (void*)0x12345678;
    semantic_memory_set_consolidation(system, dummy_consolidation);
    // No crash = success
    SUCCEED();
}

TEST_F(SemanticMemoryTest, SetConsolidationNull) {
    semantic_memory_set_consolidation(nullptr, nullptr);
    // Should not crash
    SUCCEED();
}

//=============================================================================
// Concept Operations Tests
//=============================================================================

TEST_F(SemanticMemoryTest, CreateConcept) {
    float features[32];
    create_test_features(features, 32, 0.8f);

    uint64_t id = semantic_memory_create_concept(
        system,
        features,
        32,
        "apple",
        CONCEPT_OBJECT
    );

    EXPECT_NE(id, 0);

    // Verify stats updated
    semantic_memory_stats_t stats;
    semantic_memory_get_statistics(system, &stats);
    EXPECT_EQ(stats.concept_count, 1);
    EXPECT_EQ(stats.total_concepts_formed, 1);
}

TEST_F(SemanticMemoryTest, CreateConceptNullSystem) {
    float features[32];
    create_test_features(features, 32, 0.5f);

    uint64_t id = semantic_memory_create_concept(
        nullptr, features, 32, "test", CONCEPT_OBJECT
    );
    EXPECT_EQ(id, 0);
}

TEST_F(SemanticMemoryTest, CreateConceptNullFeatures) {
    uint64_t id = semantic_memory_create_concept(
        system, nullptr, 32, "test", CONCEPT_OBJECT
    );
    EXPECT_EQ(id, 0);
}

TEST_F(SemanticMemoryTest, CreateConceptZeroDim) {
    float features[32];
    uint64_t id = semantic_memory_create_concept(
        system, features, 0, "test", CONCEPT_OBJECT
    );
    EXPECT_EQ(id, 0);
}

TEST_F(SemanticMemoryTest, CreateConceptNoLabel) {
    float features[32];
    create_test_features(features, 32, 0.5f);

    uint64_t id = semantic_memory_create_concept(
        system, features, 32, nullptr, CONCEPT_OBJECT
    );
    EXPECT_NE(id, 0); // Should succeed even without label
}

TEST_F(SemanticMemoryTest, GetConcept) {
    float features[32];
    create_test_features(features, 32, 0.7f);

    uint64_t id = semantic_memory_create_concept(
        system, features, 32, "dog", CONCEPT_OBJECT
    );
    EXPECT_NE(id, 0);

    const semantic_concept_t* retrieved = semantic_memory_get_concept(system, id);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id, id);
    EXPECT_EQ(retrieved->category, CONCEPT_OBJECT);
    EXPECT_STREQ(retrieved->label, "dog");
}

TEST_F(SemanticMemoryTest, GetConceptNotFound) {
    const semantic_concept_t* retrieved = semantic_memory_get_concept(system, 999999);
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(SemanticMemoryTest, GetConceptNullSystem) {
    const semantic_concept_t* retrieved = semantic_memory_get_concept(nullptr, 1);
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(SemanticMemoryTest, FindSimilarConcepts) {
    // Create similar concepts
    float features1[32];
    create_test_features(features1, 32, 0.8f);
    uint64_t id1 = semantic_memory_create_concept(
        system, features1, 32, "cat", CONCEPT_OBJECT
    );

    float features2[32];
    create_test_features(features2, 32, 0.82f); // Very similar
    uint64_t id2 = semantic_memory_create_concept(
        system, features2, 32, "dog", CONCEPT_OBJECT
    );

    float features3[32];
    create_test_features(features3, 32, 0.1f); // Dissimilar
    uint64_t id3 = semantic_memory_create_concept(
        system, features3, 32, "rock", CONCEPT_OBJECT
    );

    // Query with features similar to features1
    float query_features[32];
    create_test_features(query_features, 32, 0.81f);

    semantic_query_result_t* result = semantic_memory_find_similar(
        system, query_features, 32, 10, 0.5f
    );

    ASSERT_NE(result, nullptr);
    EXPECT_GT(result->count, 0);

    semantic_memory_free_result(result);
}

TEST_F(SemanticMemoryTest, FindSimilarNoMatches) {
    // Create concept
    float features1[32];
    create_test_features(features1, 32, 0.9f);
    semantic_memory_create_concept(system, features1, 32, "test", CONCEPT_OBJECT);

    // Query with very different features
    float query_features[32];
    create_test_features(query_features, 32, 0.1f);

    semantic_query_result_t* result = semantic_memory_find_similar(
        system, query_features, 32, 10, 0.99f  // Very high threshold
    );

    // May return NULL or have few/no matches
    if (result) {
        // With very high threshold, expect 0 or very few matches
        EXPECT_LE(result->count, 1);
        semantic_memory_free_result(result);
    }
}

TEST_F(SemanticMemoryTest, FindSimilarNullSystem) {
    float features[32];
    semantic_query_result_t* result = semantic_memory_find_similar(
        nullptr, features, 32, 10, 0.5f
    );
    EXPECT_EQ(result, nullptr);
}

//=============================================================================
// Relation Operations Tests
//=============================================================================

TEST_F(SemanticMemoryTest, CreateRelation) {
    // Create two concepts
    float features1[32], features2[32];
    create_test_features(features1, 32, 0.5f);
    create_test_features(features2, 32, 0.6f);

    uint64_t id1 = semantic_memory_create_concept(
        system, features1, 32, "dog", CONCEPT_OBJECT
    );
    uint64_t id2 = semantic_memory_create_concept(
        system, features2, 32, "animal", CONCEPT_CATEGORY
    );

    // Create IS-A relation: dog is-a animal
    uint64_t rel_id = semantic_memory_create_relation(
        system, id1, id2, RELATION_IS_A, 0.9f
    );

    EXPECT_NE(rel_id, 0);

    // Verify stats
    semantic_memory_stats_t stats;
    semantic_memory_get_statistics(system, &stats);
    EXPECT_EQ(stats.relation_count, 1);
    EXPECT_EQ(stats.total_relations_formed, 1);
}

TEST_F(SemanticMemoryTest, CreateRelationInvalidConcepts) {
    uint64_t rel_id = semantic_memory_create_relation(
        system, 999, 888, RELATION_IS_A, 0.5f
    );
    EXPECT_EQ(rel_id, 0);
}

TEST_F(SemanticMemoryTest, CreateRelationNullSystem) {
    uint64_t rel_id = semantic_memory_create_relation(
        nullptr, 1, 2, RELATION_IS_A, 0.5f
    );
    EXPECT_EQ(rel_id, 0);
}

TEST_F(SemanticMemoryTest, GetRelations) {
    // Create concepts
    float features[32];
    create_test_features(features, 32, 0.5f);
    uint64_t id1 = semantic_memory_create_concept(
        system, features, 32, "car", CONCEPT_OBJECT
    );
    uint64_t id2 = semantic_memory_create_concept(
        system, features, 32, "wheel", CONCEPT_OBJECT
    );

    // Create relation
    uint64_t rel_id = semantic_memory_create_relation(
        system, id1, id2, RELATION_HAS_A, 0.8f
    );
    EXPECT_NE(rel_id, 0);

    // Get relations for id1
    uint64_t relation_ids[10];
    uint32_t count = semantic_memory_get_relations(
        system, id1, relation_ids, 10
    );

    EXPECT_GE(count, 1);
}

TEST_F(SemanticMemoryTest, GetRelationsNullSystem) {
    uint64_t relation_ids[10];
    uint32_t count = semantic_memory_get_relations(
        nullptr, 1, relation_ids, 10
    );
    EXPECT_EQ(count, 0);
}

//=============================================================================
// Spreading Activation Tests
//=============================================================================

TEST_F(SemanticMemoryTest, ActivateConcept) {
    // Create concept
    float features[32];
    create_test_features(features, 32, 0.7f);
    uint64_t id = semantic_memory_create_concept(
        system, features, 32, "test", CONCEPT_OBJECT
    );

    // Activate
    semantic_query_result_t* result = semantic_memory_activate(
        system, id, 1.0f
    );

    ASSERT_NE(result, nullptr);
    EXPECT_GT(result->count, 0);

    semantic_memory_free_result(result);
}

TEST_F(SemanticMemoryTest, ActivateWithRelations) {
    // Create network: A -> B -> C
    float features[32];
    create_test_features(features, 32, 0.5f);

    uint64_t id_a = semantic_memory_create_concept(
        system, features, 32, "A", CONCEPT_OBJECT
    );
    uint64_t id_b = semantic_memory_create_concept(
        system, features, 32, "B", CONCEPT_OBJECT
    );
    uint64_t id_c = semantic_memory_create_concept(
        system, features, 32, "C", CONCEPT_OBJECT
    );

    semantic_memory_create_relation(system, id_a, id_b, RELATION_ASSOCIATED, 0.9f);
    semantic_memory_create_relation(system, id_b, id_c, RELATION_ASSOCIATED, 0.9f);

    // Activate A, should spread to B and possibly C
    semantic_query_result_t* result = semantic_memory_activate(
        system, id_a, 1.0f
    );

    ASSERT_NE(result, nullptr);
    // Should activate at least A, possibly more
    EXPECT_GE(result->count, 1);

    semantic_memory_free_result(result);
}

TEST_F(SemanticMemoryTest, ActivateNullSystem) {
    semantic_query_result_t* result = semantic_memory_activate(
        nullptr, 1, 1.0f
    );
    EXPECT_EQ(result, nullptr);
}

TEST_F(SemanticMemoryTest, ActivateInvalidConcept) {
    semantic_query_result_t* result = semantic_memory_activate(
        system, 999999, 1.0f
    );
    EXPECT_EQ(result, nullptr);
}

TEST_F(SemanticMemoryTest, QuerySemanticMemory) {
    // Create concept
    float features[32];
    create_test_features(features, 32, 0.6f);
    semantic_memory_create_concept(system, features, 32, "test", CONCEPT_OBJECT);

    // Query with similar features
    float query_features[32];
    create_test_features(query_features, 32, 0.61f);

    semantic_query_result_t* result = semantic_memory_query(
        system, query_features, 32
    );

    // May or may not find match depending on threshold
    if (result) {
        semantic_memory_free_result(result);
    }
    SUCCEED();
}

TEST_F(SemanticMemoryTest, QueryNullSystem) {
    float features[32];
    semantic_query_result_t* result = semantic_memory_query(
        nullptr, features, 32
    );
    EXPECT_EQ(result, nullptr);
}

TEST_F(SemanticMemoryTest, FreeNullResult) {
    // Should not crash
    semantic_memory_free_result(nullptr);
    SUCCEED();
}

//=============================================================================
// Knowledge Extraction Tests
//=============================================================================

TEST_F(SemanticMemoryTest, ExtractFromConsolidationNoSystem) {
    uint32_t count = semantic_memory_extract_from_consolidation(system);
    EXPECT_EQ(count, 0); // No consolidation system set
}

TEST_F(SemanticMemoryTest, ExtractFromConsolidationNull) {
    uint32_t count = semantic_memory_extract_from_consolidation(nullptr);
    EXPECT_EQ(count, 0);
}

//=============================================================================
// Configuration API Tests
//=============================================================================

TEST_F(SemanticMemoryTest, SetSpreadParams) {
    spreading_activation_params_t params;
    params.decay_rate = 0.5f;
    params.threshold = 0.2f;
    params.max_hops = 5;
    params.min_activation = 0.05f;

    semantic_memory_set_spread_params(system, &params);

    spreading_activation_params_t retrieved;
    semantic_memory_get_spread_params(system, &retrieved);

    EXPECT_FLOAT_EQ(retrieved.decay_rate, 0.5f);
    EXPECT_FLOAT_EQ(retrieved.threshold, 0.2f);
    EXPECT_EQ(retrieved.max_hops, 5);
    EXPECT_FLOAT_EQ(retrieved.min_activation, 0.05f);
}

TEST_F(SemanticMemoryTest, SetSpreadParamsNull) {
    semantic_memory_set_spread_params(nullptr, nullptr);
    // Should not crash
    SUCCEED();
}

TEST_F(SemanticMemoryTest, GetSpreadParamsNull) {
    spreading_activation_params_t params;
    semantic_memory_get_spread_params(nullptr, &params);
    // Should not crash
    SUCCEED();
}

TEST_F(SemanticMemoryTest, DefaultSpreadParams) {
    spreading_activation_params_t params =
        semantic_memory_get_default_spread_params();

    EXPECT_GT(params.decay_rate, 0.0f);
    EXPECT_LT(params.decay_rate, 1.0f);
    EXPECT_GT(params.threshold, 0.0f);
    EXPECT_GT(params.max_hops, 0);
    EXPECT_GT(params.min_activation, 0.0f);
}

//=============================================================================
// Statistics API Tests
//=============================================================================

TEST_F(SemanticMemoryTest, GetStatistics) {
    semantic_memory_stats_t stats;
    semantic_memory_get_statistics(system, &stats);

    EXPECT_EQ(stats.concept_count, 0);
    EXPECT_EQ(stats.relation_count, 0);
    EXPECT_EQ(stats.total_retrievals, 0);
}

TEST_F(SemanticMemoryTest, StatisticsUpdatedOnCreate) {
    float features[32];
    create_test_features(features, 32, 0.5f);

    semantic_memory_create_concept(system, features, 32, "test", CONCEPT_OBJECT);

    semantic_memory_stats_t stats;
    semantic_memory_get_statistics(system, &stats);

    EXPECT_EQ(stats.concept_count, 1);
    EXPECT_EQ(stats.total_concepts_formed, 1);
}

TEST_F(SemanticMemoryTest, GetStatisticsNull) {
    semantic_memory_stats_t stats;
    semantic_memory_get_statistics(nullptr, &stats);
    // Should not crash
    SUCCEED();
}

//=============================================================================
// Complex Integration Tests
//=============================================================================

TEST_F(SemanticMemoryTest, BuildSemanticNetwork) {
    // Build a small semantic network
    float features[32];

    create_test_features(features, 32, 0.5f);
    uint64_t animal = semantic_memory_create_concept(
        system, features, 32, "animal", CONCEPT_CATEGORY
    );

    create_test_features(features, 32, 0.6f);
    uint64_t dog = semantic_memory_create_concept(
        system, features, 32, "dog", CONCEPT_OBJECT
    );

    create_test_features(features, 32, 0.7f);
    uint64_t cat = semantic_memory_create_concept(
        system, features, 32, "cat", CONCEPT_OBJECT
    );

    // Create IS-A relations
    semantic_memory_create_relation(system, dog, animal, RELATION_IS_A, 0.9f);
    semantic_memory_create_relation(system, cat, animal, RELATION_IS_A, 0.9f);

    // Verify network
    semantic_memory_stats_t stats;
    semantic_memory_get_statistics(system, &stats);
    EXPECT_EQ(stats.concept_count, 3);
    EXPECT_EQ(stats.relation_count, 2);
}

TEST_F(SemanticMemoryTest, MultipleConceptCategories) {
    float features[32];
    create_test_features(features, 32, 0.5f);

    uint64_t id1 = semantic_memory_create_concept(
        system, features, 32, "object", CONCEPT_OBJECT
    );
    uint64_t id2 = semantic_memory_create_concept(
        system, features, 32, "action", CONCEPT_ACTION
    );
    uint64_t id3 = semantic_memory_create_concept(
        system, features, 32, "property", CONCEPT_PROPERTY
    );

    EXPECT_NE(id1, 0);
    EXPECT_NE(id2, 0);
    EXPECT_NE(id3, 0);
}

TEST_F(SemanticMemoryTest, MultipleRelationTypes) {
    float features[32];
    create_test_features(features, 32, 0.5f);

    uint64_t id1 = semantic_memory_create_concept(
        system, features, 32, "A", CONCEPT_OBJECT
    );
    uint64_t id2 = semantic_memory_create_concept(
        system, features, 32, "B", CONCEPT_OBJECT
    );

    uint64_t rel1 = semantic_memory_create_relation(
        system, id1, id2, RELATION_IS_A, 0.9f
    );
    uint64_t rel2 = semantic_memory_create_relation(
        system, id1, id2, RELATION_HAS_A, 0.8f
    );
    uint64_t rel3 = semantic_memory_create_relation(
        system, id1, id2, RELATION_CAUSES, 0.7f
    );

    EXPECT_NE(rel1, 0);
    EXPECT_NE(rel2, 0);
    EXPECT_NE(rel3, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
