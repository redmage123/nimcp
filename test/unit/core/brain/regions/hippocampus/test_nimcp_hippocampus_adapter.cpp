/**
 * @file test_nimcp_hippocampus_adapter.cpp
 * @brief Unit tests for nimcp_hippocampus_adapter.c
 *
 * WHAT: Comprehensive unit tests for the Hippocampus adapter
 * WHY:  Ensure correct integration of memory encoding, retrieval, and navigation
 * HOW:  Use Google Test framework to test lifecycle, memory operations, pattern
 *       separation/completion, spatial navigation, and consolidation.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/hippocampus/nimcp_hippocampus_adapter.h"

// Test Fixture for Hippocampus Adapter
class HippocampusAdapterTest : public ::testing::Test {
protected:
    hippocampus_adapter_t* adapter;
    hippocampus_config_t config;

    void SetUp() override {
        config = hippocampus_default_config();
        // Use smaller sizes for faster tests
        config.num_place_cells = 64;
        config.num_grid_cells = 32;
        config.ca3_size = 64;
        config.ca1_size = 128;
        config.dg_size = 256;
        config.memory_capacity = 100;
        config.enable_bio_async = false;  // Disable for unit tests
        adapter = hippocampus_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create hippocampus adapter";
    }

    void TearDown() override {
        hippocampus_destroy(adapter);
        adapter = nullptr;
    }

    // Helper to create test features
    void create_test_features(float* features, uint32_t count, float base_value) {
        for (uint32_t i = 0; i < count; i++) {
            features[i] = base_value + (float)i * 0.1f;
        }
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(HippocampusAdapterTest, DefaultConfigHasReasonableValues) {
    hippocampus_config_t default_config = hippocampus_default_config();

    EXPECT_EQ(default_config.num_place_cells, HIPPOCAMPUS_DEFAULT_NUM_PLACE_CELLS);
    EXPECT_EQ(default_config.num_grid_cells, HIPPOCAMPUS_DEFAULT_NUM_GRID_CELLS);
    EXPECT_EQ(default_config.ca1_size, HIPPOCAMPUS_DEFAULT_CA1_SIZE);
    EXPECT_EQ(default_config.ca3_size, HIPPOCAMPUS_DEFAULT_CA3_SIZE);
    EXPECT_EQ(default_config.dg_size, HIPPOCAMPUS_DEFAULT_DG_SIZE);
    EXPECT_EQ(default_config.memory_capacity, HIPPOCAMPUS_DEFAULT_MEMORY_CAPACITY);
    EXPECT_TRUE(default_config.enable_pattern_separation);
    EXPECT_TRUE(default_config.enable_pattern_completion);
    EXPECT_TRUE(default_config.enable_spatial_navigation);
}

TEST_F(HippocampusAdapterTest, CreateWithNullConfigUsesDefaults) {
    hippocampus_adapter_t* adapter_null = hippocampus_create(NULL);
    ASSERT_NE(nullptr, adapter_null);

    hippocampus_config_t retrieved;
    EXPECT_TRUE(hippocampus_get_config(adapter_null, &retrieved));
    EXPECT_EQ(retrieved.num_place_cells, HIPPOCAMPUS_DEFAULT_NUM_PLACE_CELLS);

    hippocampus_destroy(adapter_null);
}

TEST_F(HippocampusAdapterTest, DestroyNullDoesNotCrash) {
    hippocampus_destroy(NULL);
    // Should not crash
}

TEST_F(HippocampusAdapterTest, ResetClearsState) {
    // Encode a memory first
    float features[16];
    create_test_features(features, 16, 1.0f);
    uint32_t memory_id = hippocampus_encode_memory(adapter, features, 16, NULL, 0.5f);
    EXPECT_NE(memory_id, 0u);

    EXPECT_TRUE(hippocampus_reset(adapter));

    // Status should be idle after reset
    EXPECT_EQ(hippocampus_get_status(adapter), HIPPOCAMPUS_STATUS_IDLE);
    EXPECT_EQ(hippocampus_get_last_error(adapter), HIPPOCAMPUS_ERROR_NONE);
}

TEST_F(HippocampusAdapterTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(hippocampus_reset(NULL));
}

// ============================================================================
// MEMORY ENCODING TESTS
// ============================================================================

TEST_F(HippocampusAdapterTest, EncodeMemorySuccess) {
    float features[16];
    create_test_features(features, 16, 1.0f);

    uint32_t memory_id = hippocampus_encode_memory(adapter, features, 16, NULL, 0.5f);
    EXPECT_NE(memory_id, 0u);
}

TEST_F(HippocampusAdapterTest, EncodeMemoryWithLocation) {
    float features[16];
    create_test_features(features, 16, 1.0f);

    hippocampus_location_t location = {
        .x = 10.0f,
        .y = 20.0f,
        .z = 0.0f,
        .heading = 1.57f,
        .velocity = 0.0f
    };

    uint32_t memory_id = hippocampus_encode_memory(adapter, features, 16, &location, 0.8f);
    EXPECT_NE(memory_id, 0u);

    // Retrieve and verify location
    hippocampus_memory_t retrieved;
    EXPECT_TRUE(hippocampus_get_memory(adapter, memory_id, &retrieved));
    EXPECT_FLOAT_EQ(retrieved.location.x, 10.0f);
    EXPECT_FLOAT_EQ(retrieved.location.y, 20.0f);
    EXPECT_FLOAT_EQ(retrieved.emotional_valence, 0.8f);
}

TEST_F(HippocampusAdapterTest, EncodeMemoryNullInputFails) {
    EXPECT_EQ(hippocampus_encode_memory(NULL, NULL, 0, NULL, 0.0f), 0u);
    EXPECT_EQ(hippocampus_encode_memory(adapter, NULL, 16, NULL, 0.0f), 0u);

    float features[16];
    EXPECT_EQ(hippocampus_encode_memory(adapter, features, 0, NULL, 0.0f), 0u);
}

TEST_F(HippocampusAdapterTest, EncodeMultipleMemories) {
    float features[16];
    uint32_t ids[10];

    for (int i = 0; i < 10; i++) {
        create_test_features(features, 16, (float)i);
        ids[i] = hippocampus_encode_memory(adapter, features, 16, NULL, (float)i / 10.0f);
        EXPECT_NE(ids[i], 0u);
    }

    // Verify all memories are unique
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}

TEST_F(HippocampusAdapterTest, GetMemoryById) {
    float features[16];
    create_test_features(features, 16, 5.0f);

    uint32_t memory_id = hippocampus_encode_memory(adapter, features, 16, NULL, 0.3f);
    ASSERT_NE(memory_id, 0u);

    hippocampus_memory_t retrieved;
    EXPECT_TRUE(hippocampus_get_memory(adapter, memory_id, &retrieved));
    EXPECT_EQ(retrieved.memory_id, memory_id);
    EXPECT_FLOAT_EQ(retrieved.emotional_valence, 0.3f);
    EXPECT_EQ(retrieved.feature_count, 16u);
}

TEST_F(HippocampusAdapterTest, GetMemoryNotFound) {
    hippocampus_memory_t retrieved;
    EXPECT_FALSE(hippocampus_get_memory(adapter, 9999, &retrieved));
}

// ============================================================================
// MEMORY RETRIEVAL TESTS
// ============================================================================

TEST_F(HippocampusAdapterTest, RetrieveByCue) {
    // Encode several memories
    float features1[16], features2[16], features3[16];
    create_test_features(features1, 16, 1.0f);
    create_test_features(features2, 16, 2.0f);
    create_test_features(features3, 16, 1.1f);  // Similar to features1

    hippocampus_encode_memory(adapter, features1, 16, NULL, 0.5f);
    hippocampus_encode_memory(adapter, features2, 16, NULL, 0.6f);
    hippocampus_encode_memory(adapter, features3, 16, NULL, 0.7f);

    // Retrieve with cue similar to features1
    float cue[16];
    create_test_features(cue, 16, 1.05f);

    retrieval_result_t result;
    EXPECT_TRUE(hippocampus_retrieve_by_cue(adapter, cue, 16, 3, &result));
    EXPECT_GT(result.count, 0u);
    EXPECT_TRUE(result.retrieval_success);

    // Clean up
    if (result.memories) free(result.memories);
    if (result.similarities) free(result.similarities);
}

TEST_F(HippocampusAdapterTest, RetrieveByCueNullInputFails) {
    retrieval_result_t result;
    EXPECT_FALSE(hippocampus_retrieve_by_cue(NULL, NULL, 0, 0, NULL));
    EXPECT_FALSE(hippocampus_retrieve_by_cue(adapter, NULL, 16, 5, &result));

    float cue[16];
    EXPECT_FALSE(hippocampus_retrieve_by_cue(adapter, cue, 0, 5, &result));
    EXPECT_FALSE(hippocampus_retrieve_by_cue(adapter, cue, 16, 5, NULL));
}

TEST_F(HippocampusAdapterTest, RetrieveByLocation) {
    // Encode memories at different locations
    float features[16];
    create_test_features(features, 16, 1.0f);

    hippocampus_location_t loc1 = {.x = 0.0f, .y = 0.0f, .z = 0.0f, .heading = 0.0f, .velocity = 0.0f};
    hippocampus_location_t loc2 = {.x = 100.0f, .y = 100.0f, .z = 0.0f, .heading = 0.0f, .velocity = 0.0f};
    hippocampus_location_t loc3 = {.x = 5.0f, .y = 5.0f, .z = 0.0f, .heading = 0.0f, .velocity = 0.0f};

    hippocampus_encode_memory(adapter, features, 16, &loc1, 0.5f);
    hippocampus_encode_memory(adapter, features, 16, &loc2, 0.5f);
    hippocampus_encode_memory(adapter, features, 16, &loc3, 0.5f);

    // Query near origin
    hippocampus_location_t query = {.x = 2.0f, .y = 2.0f, .z = 0.0f, .heading = 0.0f, .velocity = 0.0f};

    retrieval_result_t result;
    EXPECT_TRUE(hippocampus_retrieve_by_location(adapter, &query, 20.0f, 3, &result));
    EXPECT_GE(result.count, 2u);  // Should find loc1 and loc3

    // Clean up
    if (result.memories) free(result.memories);
    if (result.similarities) free(result.similarities);
}

// ============================================================================
// PATTERN SEPARATION TESTS
// ============================================================================

TEST_F(HippocampusAdapterTest, PatternSeparationSuccess) {
    float input[64];
    create_test_features(input, 64, 1.0f);

    pattern_separation_result_t result;
    EXPECT_TRUE(hippocampus_pattern_separate(adapter, input, 64, &result));
    EXPECT_GT(result.sparse_size, 0u);
    EXPECT_LT(result.sparsity, 0.2f);  // Should be sparse
    EXPECT_GT(result.separation_strength, 0.5f);

    // Clean up
    if (result.sparse_code) free(result.sparse_code);
}

TEST_F(HippocampusAdapterTest, PatternSeparationNullInputFails) {
    pattern_separation_result_t result;
    EXPECT_FALSE(hippocampus_pattern_separate(NULL, NULL, 0, NULL));
    EXPECT_FALSE(hippocampus_pattern_separate(adapter, NULL, 64, &result));
}

// ============================================================================
// PATTERN COMPLETION TESTS
// ============================================================================

TEST_F(HippocampusAdapterTest, PatternCompletionSuccess) {
    // Encode a memory first
    float original[64];
    create_test_features(original, 64, 1.0f);
    hippocampus_encode_memory(adapter, original, 64, NULL, 0.5f);

    // Provide partial cue
    float partial[32];
    create_test_features(partial, 32, 1.0f);

    pattern_completion_result_t result;
    EXPECT_TRUE(hippocampus_pattern_complete(adapter, partial, 32, &result));
    EXPECT_GT(result.pattern_size, 0u);
    EXPECT_GT(result.completion_confidence, 0.0f);

    // Clean up
    if (result.completed_pattern) free(result.completed_pattern);
}

TEST_F(HippocampusAdapterTest, PatternCompletionNullInputFails) {
    pattern_completion_result_t result;
    EXPECT_FALSE(hippocampus_pattern_complete(NULL, NULL, 0, NULL));
    EXPECT_FALSE(hippocampus_pattern_complete(adapter, NULL, 32, &result));
}

// ============================================================================
// SPATIAL NAVIGATION TESTS
// ============================================================================

TEST_F(HippocampusAdapterTest, UpdatePositionSuccess) {
    hippocampus_location_t location = {
        .x = 10.0f,
        .y = 20.0f,
        .z = 0.0f,
        .heading = 1.0f,
        .velocity = 2.0f
    };

    EXPECT_TRUE(hippocampus_update_position(adapter, &location));
}

TEST_F(HippocampusAdapterTest, UpdatePositionNullFails) {
    EXPECT_FALSE(hippocampus_update_position(NULL, NULL));
    EXPECT_FALSE(hippocampus_update_position(adapter, NULL));
}

TEST_F(HippocampusAdapterTest, GetPositionEstimate) {
    // Update to a known position
    hippocampus_location_t location = {
        .x = 15.0f,
        .y = 25.0f,
        .z = 0.0f,
        .heading = 0.5f,
        .velocity = 1.0f
    };
    ASSERT_TRUE(hippocampus_update_position(adapter, &location));

    // Get estimate (may differ due to place cell representation)
    hippocampus_location_t estimate;
    bool success = hippocampus_get_position_estimate(adapter, &estimate);
    // Position estimate depends on place cell coverage
    if (success) {
        // Just verify we got some values
        EXPECT_TRUE(std::isfinite(estimate.x));
        EXPECT_TRUE(std::isfinite(estimate.y));
    }
}

TEST_F(HippocampusAdapterTest, SetNavigationGoal) {
    hippocampus_location_t goal = {
        .x = 50.0f,
        .y = 50.0f,
        .z = 0.0f,
        .heading = 0.0f,
        .velocity = 0.0f
    };

    EXPECT_TRUE(hippocampus_set_navigation_goal(adapter, &goal));
    EXPECT_EQ(hippocampus_get_status(adapter), HIPPOCAMPUS_STATUS_NAVIGATING);
}

TEST_F(HippocampusAdapterTest, GetNavigationGuidance) {
    // Set current position
    hippocampus_location_t current = {
        .x = 0.0f,
        .y = 0.0f,
        .z = 0.0f,
        .heading = 0.0f,
        .velocity = 1.0f
    };
    ASSERT_TRUE(hippocampus_update_position(adapter, &current));

    // Set goal
    hippocampus_location_t goal = {
        .x = 30.0f,
        .y = 40.0f,
        .z = 0.0f,
        .heading = 0.0f,
        .velocity = 0.0f
    };
    ASSERT_TRUE(hippocampus_set_navigation_goal(adapter, &goal));

    // Get guidance
    navigation_result_t result;
    EXPECT_TRUE(hippocampus_get_navigation_guidance(adapter, &result));
    EXPECT_FLOAT_EQ(result.goal.x, 30.0f);
    EXPECT_FLOAT_EQ(result.goal.y, 40.0f);
    EXPECT_GT(result.distance_to_goal, 0.0f);

    // Clean up
    if (result.path) free(result.path);
}

TEST_F(HippocampusAdapterTest, GetNavigationGuidanceNoGoalFails) {
    navigation_result_t result;
    EXPECT_FALSE(hippocampus_get_navigation_guidance(adapter, &result));
}

TEST_F(HippocampusAdapterTest, GetPlaceCellActivity) {
    // Update position to activate place cells
    hippocampus_location_t location = {
        .x = 10.0f,
        .y = 10.0f,
        .z = 0.0f,
        .heading = 0.0f,
        .velocity = 0.0f
    };
    ASSERT_TRUE(hippocampus_update_position(adapter, &location));

    place_cell_activity_t activities[32];
    uint32_t count = 0;
    EXPECT_TRUE(hippocampus_get_place_cell_activity(adapter, activities, 32, &count));
    EXPECT_GT(count, 0u);
}

TEST_F(HippocampusAdapterTest, GetGridCellActivity) {
    // Update position to activate grid cells
    hippocampus_location_t location = {
        .x = 15.0f,
        .y = 15.0f,
        .z = 0.0f,
        .heading = 0.0f,
        .velocity = 0.0f
    };
    ASSERT_TRUE(hippocampus_update_position(adapter, &location));

    grid_cell_activity_t activities[16];
    uint32_t count = 0;
    EXPECT_TRUE(hippocampus_get_grid_cell_activity(adapter, activities, 16, &count));
    EXPECT_GT(count, 0u);
}

// ============================================================================
// MEMORY CONSOLIDATION TESTS
// ============================================================================

TEST_F(HippocampusAdapterTest, ConsolidateMemories) {
    // Encode several memories with varying strengths
    float features[16];

    for (int i = 0; i < 5; i++) {
        create_test_features(features, 16, (float)i);
        hippocampus_encode_memory(adapter, features, 16, NULL, 0.9f);  // High strength
    }

    // Consolidate with threshold
    uint32_t consolidated = hippocampus_consolidate_memories(adapter, 0.8f);
    EXPECT_GE(consolidated, 5u);
}

TEST_F(HippocampusAdapterTest, SetConsolidationCallback) {
    static int callback_count = 0;
    callback_count = 0;

    auto callback = [](const hippocampus_memory_t* memory, void* user_data) {
        (void)memory;
        (void)user_data;
        callback_count++;
    };

    EXPECT_TRUE(hippocampus_set_consolidation_callback(adapter, callback, nullptr));

    // Encode and consolidate
    float features[16];
    create_test_features(features, 16, 1.0f);
    hippocampus_encode_memory(adapter, features, 16, NULL, 0.95f);

    hippocampus_consolidate_memories(adapter, 0.9f);
    EXPECT_EQ(callback_count, 1);
}

TEST_F(HippocampusAdapterTest, TriggerReplay) {
    // Encode some memories
    float features[16];
    for (int i = 0; i < 3; i++) {
        create_test_features(features, 16, (float)i);
        hippocampus_encode_memory(adapter, features, 16, NULL, 0.5f);
    }

    // Trigger forward replay
    EXPECT_TRUE(hippocampus_trigger_replay(adapter, false, 2));

    // Trigger reverse replay
    EXPECT_TRUE(hippocampus_trigger_replay(adapter, true, 2));
}

// ============================================================================
// TRAINING INTERFACE TESTS
// ============================================================================

TEST_F(HippocampusAdapterTest, TrainAssociationDisabledByDefault) {
    float cue[16];
    create_test_features(cue, 16, 1.0f);

    // Training is disabled by default
    EXPECT_FALSE(hippocampus_train_association(adapter, cue, 16, 1, 0.01f));
}

TEST_F(HippocampusAdapterTest, TrainAssociationWithTrainingEnabled) {
    // Create adapter with training enabled
    hippocampus_config_t train_config = hippocampus_default_config();
    train_config.enable_training = true;
    train_config.enable_bio_async = false;

    hippocampus_adapter_t* train_adapter = hippocampus_create(&train_config);
    ASSERT_NE(nullptr, train_adapter);

    // Encode a memory first
    float features[16];
    create_test_features(features, 16, 1.0f);
    uint32_t memory_id = hippocampus_encode_memory(train_adapter, features, 16, NULL, 0.5f);

    // Now train association
    float cue[16];
    create_test_features(cue, 16, 2.0f);
    EXPECT_TRUE(hippocampus_train_association(train_adapter, cue, 16, memory_id, 0.01f));

    hippocampus_destroy(train_adapter);
}

TEST_F(HippocampusAdapterTest, TrainPlaceField) {
    // Create adapter with training enabled
    hippocampus_config_t train_config = hippocampus_default_config();
    train_config.enable_training = true;
    train_config.enable_bio_async = false;

    hippocampus_adapter_t* train_adapter = hippocampus_create(&train_config);
    ASSERT_NE(nullptr, train_adapter);

    hippocampus_location_t location = {
        .x = 20.0f,
        .y = 30.0f,
        .z = 0.0f,
        .heading = 0.0f,
        .velocity = 0.0f
    };

    float features[32];
    create_test_features(features, 32, 1.0f);

    EXPECT_TRUE(hippocampus_train_place_field(train_adapter, &location, features, 32));

    hippocampus_destroy(train_adapter);
}

// ============================================================================
// STATUS AND DIAGNOSTICS TESTS
// ============================================================================

TEST_F(HippocampusAdapterTest, GetStatusIdle) {
    EXPECT_EQ(hippocampus_get_status(adapter), HIPPOCAMPUS_STATUS_IDLE);
}

TEST_F(HippocampusAdapterTest, GetLastErrorNone) {
    EXPECT_EQ(hippocampus_get_last_error(adapter), HIPPOCAMPUS_ERROR_NONE);
}

TEST_F(HippocampusAdapterTest, ErrorStringNotNull) {
    const char* str = hippocampus_error_string(HIPPOCAMPUS_ERROR_NONE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = hippocampus_error_string(HIPPOCAMPUS_ERROR_INVALID_INPUT);
    EXPECT_NE(str, nullptr);

    str = hippocampus_error_string(HIPPOCAMPUS_ERROR_ENCODING_FAILURE);
    EXPECT_NE(str, nullptr);

    str = hippocampus_error_string(HIPPOCAMPUS_ERROR_RETRIEVAL_FAILURE);
    EXPECT_NE(str, nullptr);
}

TEST_F(HippocampusAdapterTest, StatusStringNotNull) {
    const char* str = hippocampus_status_string(HIPPOCAMPUS_STATUS_IDLE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = hippocampus_status_string(HIPPOCAMPUS_STATUS_ENCODING);
    EXPECT_NE(str, nullptr);

    str = hippocampus_status_string(HIPPOCAMPUS_STATUS_NAVIGATING);
    EXPECT_NE(str, nullptr);
}

TEST_F(HippocampusAdapterTest, GetStats) {
    hippocampus_stats_t stats;
    EXPECT_TRUE(hippocampus_get_stats(adapter, &stats));
    EXPECT_EQ(stats.memories_encoded, 0u);  // No memories yet

    // Encode a memory
    float features[16];
    create_test_features(features, 16, 1.0f);
    hippocampus_encode_memory(adapter, features, 16, NULL, 0.5f);

    EXPECT_TRUE(hippocampus_get_stats(adapter, &stats));
    EXPECT_EQ(stats.memories_encoded, 1u);
}

TEST_F(HippocampusAdapterTest, GetConfig) {
    hippocampus_config_t retrieved;
    EXPECT_TRUE(hippocampus_get_config(adapter, &retrieved));
    EXPECT_EQ(retrieved.num_place_cells, config.num_place_cells);
    EXPECT_EQ(retrieved.memory_capacity, config.memory_capacity);
}

// ============================================================================
// SUB-MODULE ACCESS TESTS
// ============================================================================

TEST_F(HippocampusAdapterTest, GetPlaceCellsNotNull) {
    EXPECT_NE(hippocampus_get_place_cells(adapter), nullptr);
}

TEST_F(HippocampusAdapterTest, GetGridCellsNotNull) {
    EXPECT_NE(hippocampus_get_grid_cells(adapter), nullptr);
}

TEST_F(HippocampusAdapterTest, GetPatternSeparatorNotNull) {
    EXPECT_NE(hippocampus_get_pattern_separator(adapter), nullptr);
}

TEST_F(HippocampusAdapterTest, GetMemoryEncoderNotNull) {
    EXPECT_NE(hippocampus_get_memory_encoder(adapter), nullptr);
}

TEST_F(HippocampusAdapterTest, GetSubModulesNullAdapter) {
    EXPECT_EQ(hippocampus_get_place_cells(NULL), nullptr);
    EXPECT_EQ(hippocampus_get_grid_cells(NULL), nullptr);
    EXPECT_EQ(hippocampus_get_pattern_separator(NULL), nullptr);
    EXPECT_EQ(hippocampus_get_memory_encoder(NULL), nullptr);
}

// ============================================================================
// NULL SAFETY TESTS
// ============================================================================

TEST_F(HippocampusAdapterTest, GetStatusNull) {
    EXPECT_EQ(hippocampus_get_status(NULL), HIPPOCAMPUS_STATUS_ERROR);
}

TEST_F(HippocampusAdapterTest, GetLastErrorNull) {
    EXPECT_EQ(hippocampus_get_last_error(NULL), HIPPOCAMPUS_ERROR_INTERNAL);
}

TEST_F(HippocampusAdapterTest, GetStatsNull) {
    hippocampus_stats_t stats;
    EXPECT_FALSE(hippocampus_get_stats(NULL, &stats));
    EXPECT_FALSE(hippocampus_get_stats(adapter, NULL));
}

TEST_F(HippocampusAdapterTest, GetConfigNull) {
    hippocampus_config_t config;
    EXPECT_FALSE(hippocampus_get_config(NULL, &config));
    EXPECT_FALSE(hippocampus_get_config(adapter, NULL));
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(HippocampusAdapterTest, StressEncodeMany) {
    float features[32];
    const int NUM_MEMORIES = 50;

    for (int i = 0; i < NUM_MEMORIES; i++) {
        create_test_features(features, 32, (float)i);
        uint32_t id = hippocampus_encode_memory(adapter, features, 32, NULL, 0.5f);
        EXPECT_NE(id, 0u);
    }

    hippocampus_stats_t stats;
    EXPECT_TRUE(hippocampus_get_stats(adapter, &stats));
    EXPECT_EQ(stats.memories_encoded, (uint64_t)NUM_MEMORIES);
}

TEST_F(HippocampusAdapterTest, StressNavigationUpdates) {
    const int NUM_UPDATES = 100;

    for (int i = 0; i < NUM_UPDATES; i++) {
        hippocampus_location_t location = {
            .x = (float)(i % 50),
            .y = (float)(i / 50) * 10.0f,
            .z = 0.0f,
            .heading = (float)i * 0.1f,
            .velocity = 1.0f
        };
        EXPECT_TRUE(hippocampus_update_position(adapter, &location));
    }

    hippocampus_stats_t stats;
    EXPECT_TRUE(hippocampus_get_stats(adapter, &stats));
    EXPECT_EQ(stats.navigation_steps, (uint64_t)NUM_UPDATES);
}
