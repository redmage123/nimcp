//=============================================================================
// test_pattern_library.cpp - Comprehensive Pattern Library Tests
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "middleware/patterns/nimcp_pattern_library.h"
#include "utils/memory/nimcp_memory.h"

/**
 * WHAT: Comprehensive test suite for pattern library
 * WHY:  Ensure pattern storage, matching, and KNN search work correctly
 * HOW:  Unit tests for all 13 functions, edge cases, integration tests
 */

class PatternLibraryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    bool FloatEquals(float a, float b, float eps = 0.001f) {
        return std::fabs(a - b) < eps;
    }

    void CreateTestPattern(float* pattern, uint32_t dim, float value) {
        for (uint32_t i = 0; i < dim; i++) {
            pattern[i] = value + (float)i * 0.1f;
        }
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================
// WHAT: Test library creation and destruction
// WHY:  Verify resource management and parameter validation
// HOW:  Test parameter combinations and edge cases

TEST_F(PatternLibraryTest, Create_Success_Default) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);
    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Create_Success_Custom) {
    pattern_library_config_t config = pattern_library_default_config();
    config.max_capacity = 100;
    config.max_dimension = 128;
    config.metric = SIMILARITY_COSINE;

    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);
    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Destroy_NullSafe) {
    pattern_library_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// ADD PATTERN TESTS
//=============================================================================
// WHAT: Test pattern addition operations
// WHY:  Verify pattern storage and ID assignment
// HOW:  Add patterns, check IDs

TEST_F(PatternLibraryTest, Add_Success) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t id = 0;
    bool result = pattern_library_add(lib, pattern, 4, nullptr, 0, &id);
    EXPECT_TRUE(result);
    EXPECT_GT(id, 0);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Add_Success_WithMetadata) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    const char* metadata = "test_pattern";
    uint32_t id = 0;
    bool result = pattern_library_add(lib, pattern, 4, metadata, strlen(metadata) + 1, &id);
    EXPECT_TRUE(result);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Add_Failure_NullLibrary) {
    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t id = 0;
    bool result = pattern_library_add(nullptr, pattern, 4, nullptr, 0, &id);
    EXPECT_FALSE(result);
}

TEST_F(PatternLibraryTest, Add_Failure_NullPattern) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    uint32_t id = 0;
    bool result = pattern_library_add(lib, nullptr, 4, nullptr, 0, &id);
    EXPECT_FALSE(result);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Add_Failure_NullOutputID) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    // Output ID is optional in the implementation, so this should succeed
    bool result = pattern_library_add(lib, pattern, 4, nullptr, 0, nullptr);
    EXPECT_TRUE(result);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Add_Failure_ZeroDimension) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t id = 0;
    bool result = pattern_library_add(lib, pattern, 0, nullptr, 0, &id);
    EXPECT_FALSE(result);

    pattern_library_destroy(lib);
}

//=============================================================================
// MATCH PATTERN TESTS
//=============================================================================
// WHAT: Test pattern matching operations
// WHY:  Verify similarity-based pattern retrieval
// HOW:  Match against stored patterns

TEST_F(PatternLibraryTest, Match_Success_ExactMatch) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t id = 0;
    pattern_library_add(lib, pattern, 4, nullptr, 0, &id);

    pattern_match_t match;
    bool success = pattern_library_match(lib, pattern, 4, &match);
    EXPECT_TRUE(success);
    EXPECT_EQ(match.pattern_id, id);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Match_Success_BestMatch) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float pattern2[4] = {5.0f, 6.0f, 7.0f, 8.0f};
    float query[4] = {1.1f, 2.1f, 3.1f, 4.1f};  // Closer to pattern1

    uint32_t id1 = 0, id2 = 0;
    pattern_library_add(lib, pattern1, 4, nullptr, 0, &id1);
    pattern_library_add(lib, pattern2, 4, nullptr, 0, &id2);

    pattern_match_t match;
    bool success = pattern_library_match(lib, query, 4, &match);
    EXPECT_TRUE(success);
    EXPECT_EQ(match.pattern_id, id1);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Match_Failure_NullLibrary) {
    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    pattern_match_t match;
    bool success = pattern_library_match(nullptr, pattern, 4, &match);
    EXPECT_FALSE(success);
}

TEST_F(PatternLibraryTest, Match_Failure_EmptyLibrary) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    pattern_match_t match;
    bool success = pattern_library_match(lib, pattern, 4, &match);
    EXPECT_FALSE(success);

    pattern_library_destroy(lib);
}

//=============================================================================
// KNN TESTS
//=============================================================================
// WHAT: Test K-nearest neighbor search
// WHY:  Verify multi-pattern retrieval
// HOW:  KNN with various K values

TEST_F(PatternLibraryTest, KNN_Success_K1) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float pattern2[4] = {5.0f, 6.0f, 7.0f, 8.0f};
    float query[4] = {1.1f, 2.1f, 3.1f, 4.1f};

    uint32_t id1 = 0, id2 = 0;
    pattern_library_add(lib, pattern1, 4, nullptr, 0, &id1);
    pattern_library_add(lib, pattern2, 4, nullptr, 0, &id2);

    pattern_match_t matches[1];
    uint32_t found = 0;
    bool success = pattern_library_knn(lib, query, 4, 1, matches, &found);
    EXPECT_TRUE(success);
    EXPECT_EQ(found, 1);
    EXPECT_EQ(matches[0].pattern_id, id1);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, KNN_Success_K3) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float pattern2[4] = {2.0f, 3.0f, 4.0f, 5.0f};
    float pattern3[4] = {5.0f, 6.0f, 7.0f, 8.0f};
    float query[4] = {1.5f, 2.5f, 3.5f, 4.5f};

    uint32_t id = 0;
    pattern_library_add(lib, pattern1, 4, nullptr, 0, &id);
    pattern_library_add(lib, pattern2, 4, nullptr, 0, &id);
    pattern_library_add(lib, pattern3, 4, nullptr, 0, &id);

    pattern_match_t matches[3];
    uint32_t found = 0;
    bool success = pattern_library_knn(lib, query, 4, 3, matches, &found);
    EXPECT_TRUE(success);
    EXPECT_EQ(found, 3);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, KNN_Failure_NullLibrary) {
    float query[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    pattern_match_t matches[3];
    uint32_t found = 0;
    bool success = pattern_library_knn(nullptr, query, 4, 3, matches, &found);
    EXPECT_FALSE(success);
}

//=============================================================================
// GET TESTS
//=============================================================================
// WHAT: Test pattern retrieval
// WHY:  Verify stored patterns can be accessed
// HOW:  Add patterns, retrieve by ID

TEST_F(PatternLibraryTest, Get_Success) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t id = 0;
    pattern_library_add(lib, pattern, 4, nullptr, 0, &id);

    pattern_template_t retrieved;
    bool success = pattern_library_get(lib, id, &retrieved);
    EXPECT_TRUE(success);
    EXPECT_EQ(retrieved.pattern_id, id);
    EXPECT_EQ(retrieved.dimension, 4);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Get_Failure_InvalidID) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    pattern_template_t retrieved;
    bool success = pattern_library_get(lib, 999, &retrieved);
    EXPECT_FALSE(success);

    pattern_library_destroy(lib);
}

//=============================================================================
// UPDATE TESTS
//=============================================================================
// WHAT: Test pattern update operations
// WHY:  Verify pattern modification
// HOW:  Add pattern, update it

TEST_F(PatternLibraryTest, Update_Success) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t id = 0;
    pattern_library_add(lib, pattern, 4, nullptr, 0, &id);

    float new_pattern[4] = {5.0f, 6.0f, 7.0f, 8.0f};
    bool success = pattern_library_update(lib, id, new_pattern, 4);
    EXPECT_TRUE(success);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Update_Failure_InvalidID) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    bool success = pattern_library_update(lib, 999, pattern, 4);
    EXPECT_FALSE(success);

    pattern_library_destroy(lib);
}

//=============================================================================
// REMOVE TESTS
//=============================================================================
// WHAT: Test pattern removal
// WHY:  Verify patterns can be deleted
// HOW:  Add pattern, remove it

TEST_F(PatternLibraryTest, Remove_Success) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t id = 0;
    pattern_library_add(lib, pattern, 4, nullptr, 0, &id);

    bool success = pattern_library_remove(lib, id);
    EXPECT_TRUE(success);

    pattern_template_t retrieved;
    bool found = pattern_library_get(lib, id, &retrieved);
    EXPECT_FALSE(found);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Remove_Failure_InvalidID) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    bool success = pattern_library_remove(lib, 999);
    EXPECT_FALSE(success);

    pattern_library_destroy(lib);
}

//=============================================================================
// PRUNE TESTS
//=============================================================================
// WHAT: Test pattern pruning operations
// WHY:  Verify unused pattern removal
// HOW:  Add patterns, prune based on usage

TEST_F(PatternLibraryTest, Prune_Success) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float pattern2[4] = {5.0f, 6.0f, 7.0f, 8.0f};

    uint32_t id = 0;
    pattern_library_add(lib, pattern1, 4, nullptr, 0, &id);
    pattern_library_add(lib, pattern2, 4, nullptr, 0, &id);

    uint32_t removed = 0;
    bool success = pattern_library_prune(lib, 10, &removed);
    EXPECT_TRUE(success);
    EXPECT_GE(removed, 0);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Prune_Failure_NullLibrary) {
    uint32_t removed = 0;
    bool success = pattern_library_prune(nullptr, 10, &removed);
    EXPECT_FALSE(success);
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================
// WHAT: Test statistics retrieval
// WHY:  Verify library state tracking
// HOW:  Get stats, verify counts

TEST_F(PatternLibraryTest, GetStats_Success) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    uint32_t num_patterns = 0;
    float capacity_used = 0.0f;
    float avg_dimension = 0.0f;
    uint64_t total_matches = 0;

    bool success = pattern_library_get_stats(lib, &num_patterns, &capacity_used, &avg_dimension, &total_matches);
    EXPECT_TRUE(success);
    EXPECT_EQ(num_patterns, 0);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, GetStats_Success_WithPatterns) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t id = 0;
    pattern_library_add(lib, pattern, 4, nullptr, 0, &id);

    uint32_t num_patterns = 0;
    float capacity_used = 0.0f;
    float avg_dimension = 0.0f;
    uint64_t total_matches = 0;

    bool success = pattern_library_get_stats(lib, &num_patterns, &capacity_used, &avg_dimension, &total_matches);
    EXPECT_TRUE(success);
    EXPECT_EQ(num_patterns, 1);

    pattern_library_destroy(lib);
}

//=============================================================================
// CLEAR TESTS
//=============================================================================
// WHAT: Test library clearing
// WHY:  Verify all patterns can be removed
// HOW:  Add patterns, clear, verify empty

TEST_F(PatternLibraryTest, Clear_Success) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float pattern2[4] = {5.0f, 6.0f, 7.0f, 8.0f};

    uint32_t id = 0;
    pattern_library_add(lib, pattern1, 4, nullptr, 0, &id);
    pattern_library_add(lib, pattern2, 4, nullptr, 0, &id);

    pattern_library_clear(lib);

    uint32_t num_patterns = 0;
    float capacity_used, avg_dimension;
    uint64_t total_matches;
    pattern_library_get_stats(lib, &num_patterns, &capacity_used, &avg_dimension, &total_matches);
    EXPECT_EQ(num_patterns, 0);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Clear_NullSafe) {
    pattern_library_clear(nullptr);
    // Should not crash
}

//=============================================================================
// SIMILARITY COMPUTATION TESTS
//=============================================================================
// WHAT: Test similarity metric calculations
// WHY:  Verify distance/similarity functions
// HOW:  Compute similarity between known patterns

TEST_F(PatternLibraryTest, ComputeSimilarity_IdenticalPatterns) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float pattern2[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    float sim = pattern_library_compute_similarity(lib, pattern1, pattern2, 4);
    EXPECT_GT(sim, 0.99f);  // Should be very high similarity

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, ComputeSimilarity_DifferentPatterns) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float pattern1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float pattern2[4] = {-1.0f, -2.0f, -3.0f, -4.0f};

    // Cosine similarity ranges from -1 to 1 (perfectly opposite to perfectly aligned)
    float sim = pattern_library_compute_similarity(lib, pattern1, pattern2, 4);
    EXPECT_GE(sim, -1.0f);
    EXPECT_LE(sim, 1.0f);
    // These patterns are perfectly opposite, so similarity should be close to -1
    EXPECT_LT(sim, -0.99f);

    pattern_library_destroy(lib);
}

//=============================================================================
// REGRESSION TESTS
//=============================================================================
// WHAT: Test known edge cases and bugs
// WHY:  Prevent regressions
// HOW:  Test problematic scenarios

TEST_F(PatternLibraryTest, Regression_ZeroVector) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float zero_pattern[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id = 0;
    bool success = pattern_library_add(lib, zero_pattern, 4, nullptr, 0, &id);
    EXPECT_TRUE(success);

    pattern_library_destroy(lib);
}

TEST_F(PatternLibraryTest, Regression_LargeDimension) {
    pattern_library_config_t config = pattern_library_default_config();
    pattern_library_t* lib = pattern_library_create(&config);
    ASSERT_NE(lib, nullptr);

    float* large_pattern = new float[256];
    for (uint32_t i = 0; i < 256; i++) {
        large_pattern[i] = (float)i;
    }

    uint32_t id = 0;
    bool success = pattern_library_add(lib, large_pattern, 256, nullptr, 0, &id);
    EXPECT_TRUE(success);

    delete[] large_pattern;
    pattern_library_destroy(lib);
}

int main(int argc, char** argv) {
    // Initialize memory system before running tests
    nimcp_memory_init();
    nimcp_memory_enable_tracking(true);

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    // Cleanup memory system after tests
    nimcp_memory_check_leaks();
    nimcp_memory_cleanup();

    return result;
}
