/**
 * @file test_api_end_to_end.cpp
 * @brief End-to-end integration tests for NIMCP API
 *
 * Tests complete workflows through the public API:
 * - Full learning pipeline (create → learn → predict → save → load → predict)
 * - Multiple brains with different configurations
 * - Brain with working memory integration
 * - Brain with global workspace integration
 * - Error recovery scenarios
 * - Resource management across multiple operations
 *
 * Estimated tests: 20
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <vector>
#include <memory>

class APIEndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize NIMCP library
        ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);
    }

    void TearDown() override {
        // Cleanup NIMCP library
        nimcp_shutdown();
    }

    // Helper to remove test files
    void cleanup_file(const char* filepath) {
        unlink(filepath);
    }
};

//=============================================================================
// Full Learning Pipeline Tests
//=============================================================================

TEST_F(APIEndToEndTest, FullLearningPipeline_Classification) {
    const char* save_path = "/tmp/test_brain_classification.nimcp";
    cleanup_file(save_path);

    // Step 1: Create brain
    nimcp_brain_t brain = nimcp_brain_create(
        "classifier",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        10,  // inputs
        3    // outputs (3 classes)
    );
    ASSERT_NE(brain, nullptr);

    // Step 2: Train with examples
    float features_class_a[] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    float features_class_b[] = {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    float features_class_c[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};

    // Learn multiple examples for each class
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(nimcp_brain_learn_example(brain, features_class_a, 10, "class_a", 1.0f), NIMCP_SUCCESS);
        ASSERT_EQ(nimcp_brain_learn_example(brain, features_class_b, 10, "class_b", 1.0f), NIMCP_SUCCESS);
        ASSERT_EQ(nimcp_brain_learn_example(brain, features_class_c, 10, "class_c", 1.0f), NIMCP_SUCCESS);
    }

    // Step 3: Predict before save
    char label[64];
    float confidence;
    ASSERT_EQ(nimcp_brain_predict(brain, features_class_a, 10, label, &confidence), NIMCP_SUCCESS);
    EXPECT_STREQ(label, "class_a");
    EXPECT_GT(confidence, 0.5f);

    // Step 4: Save brain
    ASSERT_EQ(nimcp_brain_save(brain, save_path), NIMCP_SUCCESS);

    // Step 5: Destroy original brain
    nimcp_brain_destroy(brain);

    // Step 6: Load brain from file
    nimcp_brain_t loaded_brain = nimcp_brain_load(save_path);
    ASSERT_NE(loaded_brain, nullptr);

    // Step 7: Predict with loaded brain (should match original)
    char loaded_label[64];
    float loaded_confidence;
    ASSERT_EQ(nimcp_brain_predict(loaded_brain, features_class_a, 10, loaded_label, &loaded_confidence), NIMCP_SUCCESS);
    EXPECT_STREQ(loaded_label, "class_a");
    EXPECT_NEAR(confidence, loaded_confidence, 0.1f);

    // Cleanup
    nimcp_brain_destroy(loaded_brain);
    cleanup_file(save_path);
}

TEST_F(APIEndToEndTest, FullLearningPipeline_Regression) {
    const char* save_path = "/tmp/test_brain_regression.nimcp";
    cleanup_file(save_path);

    // Create regression brain
    nimcp_brain_t brain = nimcp_brain_create(
        "regressor",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_REGRESSION,
        5,  // inputs
        1   // output (single value)
    );
    ASSERT_NE(brain, nullptr);

    // Train with regression examples (learn sum of inputs)
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float outputs[1];

    // Learn multiple times
    for (int i = 0; i < 20; i++) {
        ASSERT_EQ(nimcp_brain_learn_example(brain, features, 5, "15.0", 1.0f), NIMCP_SUCCESS);
    }

    // Get inference result
    ASSERT_EQ(nimcp_brain_infer(brain, features, 5, outputs, 1), NIMCP_SUCCESS);

    // Save and reload
    ASSERT_EQ(nimcp_brain_save(brain, save_path), NIMCP_SUCCESS);
    nimcp_brain_destroy(brain);

    nimcp_brain_t loaded_brain = nimcp_brain_load(save_path);
    ASSERT_NE(loaded_brain, nullptr);

    // Verify loaded brain produces similar output
    float loaded_outputs[1];
    ASSERT_EQ(nimcp_brain_infer(loaded_brain, features, 5, loaded_outputs, 1), NIMCP_SUCCESS);
    EXPECT_NEAR(outputs[0], loaded_outputs[0], 0.1f);

    nimcp_brain_destroy(loaded_brain);
    cleanup_file(save_path);
}

TEST_F(APIEndToEndTest, FullLearningPipeline_PatternMatching) {
    // Create pattern matching brain
    nimcp_brain_t brain = nimcp_brain_create(
        "pattern_matcher",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_PATTERN_MATCHING,
        16,  // 4x4 pattern
        2    // match/no-match
    );
    ASSERT_NE(brain, nullptr);

    // Train with pattern examples
    float pattern_cross[] = {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0};
    float pattern_square[] = {1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1};

    for (int i = 0; i < 15; i++) {
        ASSERT_EQ(nimcp_brain_learn_example(brain, pattern_cross, 16, "cross", 1.0f), NIMCP_SUCCESS);
        ASSERT_EQ(nimcp_brain_learn_example(brain, pattern_square, 16, "square", 1.0f), NIMCP_SUCCESS);
    }

    // Test recognition
    char label[64];
    float confidence;
    ASSERT_EQ(nimcp_brain_predict(brain, pattern_cross, 16, label, &confidence), NIMCP_SUCCESS);
    EXPECT_STREQ(label, "cross");

    nimcp_brain_destroy(brain);
}

//=============================================================================
// Multiple Brains with Different Configurations
//=============================================================================

TEST_F(APIEndToEndTest, MultipleBrains_DifferentSizes) {
    // Create brains of different sizes
    nimcp_brain_t tiny = nimcp_brain_create("tiny", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_brain_t small = nimcp_brain_create("small", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_brain_t medium = nimcp_brain_create("medium", NIMCP_BRAIN_MEDIUM, NIMCP_TASK_CLASSIFICATION, 5, 2);

    ASSERT_NE(tiny, nullptr);
    ASSERT_NE(small, nullptr);
    ASSERT_NE(medium, nullptr);

    // Verify neuron counts are different
    uint32_t tiny_count = nimcp_brain_get_neuron_count(tiny);
    uint32_t small_count = nimcp_brain_get_neuron_count(small);
    uint32_t medium_count = nimcp_brain_get_neuron_count(medium);

    EXPECT_LT(tiny_count, small_count);
    EXPECT_LT(small_count, medium_count);

    // Train all simultaneously
    float features[] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    ASSERT_EQ(nimcp_brain_learn_example(tiny, features, 5, "pattern_a", 1.0f), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_brain_learn_example(small, features, 5, "pattern_a", 1.0f), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_brain_learn_example(medium, features, 5, "pattern_a", 1.0f), NIMCP_SUCCESS);

    // Cleanup
    nimcp_brain_destroy(tiny);
    nimcp_brain_destroy(small);
    nimcp_brain_destroy(medium);
}

TEST_F(APIEndToEndTest, MultipleBrains_DifferentTasks) {
    // Create brains for different tasks
    std::vector<nimcp_brain_t> brains;

    brains.push_back(nimcp_brain_create("task1", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 3));
    brains.push_back(nimcp_brain_create("task2", NIMCP_BRAIN_SMALL, NIMCP_TASK_REGRESSION, 10, 1));
    brains.push_back(nimcp_brain_create("task3", NIMCP_BRAIN_SMALL, NIMCP_TASK_PATTERN_MATCHING, 16, 2));
    brains.push_back(nimcp_brain_create("task4", NIMCP_BRAIN_SMALL, NIMCP_TASK_SEQUENCE, 8, 4));
    brains.push_back(nimcp_brain_create("task5", NIMCP_BRAIN_SMALL, NIMCP_TASK_ASSOCIATION, 12, 5));

    // Verify all created successfully
    for (auto brain : brains) {
        ASSERT_NE(brain, nullptr);
    }

    // Cleanup
    for (auto brain : brains) {
        nimcp_brain_destroy(brain);
    }
}

TEST_F(APIEndToEndTest, MultipleBrains_Concurrent_Operations) {
    // Create multiple brains
    nimcp_brain_t brain1 = nimcp_brain_create("concurrent1", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_brain_t brain2 = nimcp_brain_create("concurrent2", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_brain_t brain3 = nimcp_brain_create("concurrent3", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);
    ASSERT_NE(brain3, nullptr);

    // Interleave operations
    float features1[] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    float features2[] = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f};

    ASSERT_EQ(nimcp_brain_learn_example(brain1, features1, 5, "A", 1.0f), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_brain_learn_example(brain2, features2, 5, "B", 1.0f), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_brain_learn_example(brain3, features1, 5, "A", 1.0f), NIMCP_SUCCESS);

    char label1[64], label2[64], label3[64];
    float conf1, conf2, conf3;

    ASSERT_EQ(nimcp_brain_predict(brain1, features1, 5, label1, &conf1), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_brain_predict(brain2, features2, 5, label2, &conf2), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_brain_predict(brain3, features1, 5, label3, &conf3), NIMCP_SUCCESS);

    // Each brain should maintain independent state
    EXPECT_STREQ(label1, "A");
    EXPECT_STREQ(label2, "B");
    EXPECT_STREQ(label3, "A");

    nimcp_brain_destroy(brain1);
    nimcp_brain_destroy(brain2);
    nimcp_brain_destroy(brain3);
}

//=============================================================================
// Brain with Working Memory Integration
//=============================================================================

TEST_F(APIEndToEndTest, WorkingMemory_AddAndRetrieve) {
    nimcp_brain_t brain = nimcp_brain_create("wm_brain", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    // Add items to working memory
    float item1[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float item2[] = {6.0f, 7.0f, 8.0f, 9.0f, 10.0f};

    nimcp_status_t status1 = nimcp_brain_working_memory_add(brain, item1, 5, 0.8f);
    nimcp_status_t status2 = nimcp_brain_working_memory_add(brain, item2, 5, 0.6f);

    // May not be enabled, but should not crash
    if (status1 == NIMCP_SUCCESS) {
        // Get statistics
        uint32_t size, capacity;
        ASSERT_EQ(nimcp_brain_working_memory_stats(brain, &size, &capacity), NIMCP_SUCCESS);
        EXPECT_GE(size, 1);
        EXPECT_LE(size, capacity);

        // Retrieve item
        uint32_t retrieved_size;
        const float* retrieved = nimcp_brain_working_memory_get(brain, 0, &retrieved_size);
        if (retrieved != nullptr) {
            EXPECT_EQ(retrieved_size, 5);
        }
    }

    nimcp_brain_destroy(brain);
}

TEST_F(APIEndToEndTest, WorkingMemory_CapacityLimits) {
    nimcp_brain_t brain = nimcp_brain_create("wm_capacity", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    // Try to add many items (should be limited by capacity)
    float item[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    for (int i = 0; i < 20; i++) {
        nimcp_brain_working_memory_add(brain, item, 5, 0.5f);
    }

    // Get capacity
    uint32_t size, capacity;
    if (nimcp_brain_working_memory_stats(brain, &size, &capacity) == NIMCP_SUCCESS) {
        // Size should not exceed capacity
        EXPECT_LE(size, capacity);
        // Capacity should be around Miller's 7±2
        EXPECT_LE(capacity, 10);
    }

    nimcp_brain_destroy(brain);
}

TEST_F(APIEndToEndTest, WorkingMemory_RefreshPreventsDecay) {
    nimcp_brain_t brain = nimcp_brain_create("wm_refresh", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    float item[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    nimcp_status_t status = nimcp_brain_working_memory_add(brain, item, 5, 0.9f);

    if (status == NIMCP_SUCCESS) {
        // Refresh item multiple times
        for (int i = 0; i < 5; i++) {
            nimcp_brain_working_memory_refresh(brain, 0);
        }

        // Item should still be accessible
        uint32_t size;
        const float* retrieved = nimcp_brain_working_memory_get(brain, 0, &size);
        EXPECT_NE(retrieved, nullptr);
    }

    nimcp_brain_destroy(brain);
}

//=============================================================================
// Brain with Global Workspace Integration
//=============================================================================

TEST_F(APIEndToEndTest, GlobalWorkspace_CompeteAndRead) {
    nimcp_brain_t brain = nimcp_brain_create("gw_brain", NIMCP_BRAIN_MEDIUM, NIMCP_TASK_CLASSIFICATION, 256, 10);
    ASSERT_NE(brain, nullptr);

    // Prepare content for workspace
    float content[256];
    for (int i = 0; i < 256; i++) {
        content[i] = static_cast<float>(i) / 256.0f;
    }

    // Compete for workspace access
    nimcp_status_t compete_status = nimcp_brain_workspace_compete(
        brain,
        NIMCP_MODULE_PERCEPTION,
        content,
        256,
        0.85f  // High strength
    );

    // May not be enabled, but should not crash
    if (compete_status == NIMCP_SUCCESS) {
        // Check if broadcast exists
        bool has_broadcast;
        ASSERT_EQ(nimcp_brain_workspace_has_broadcast(brain, &has_broadcast), NIMCP_SUCCESS);
        EXPECT_TRUE(has_broadcast);

        // Read broadcast
        float read_content[256];
        uint32_t dim;
        nimcp_cognitive_module_t source;
        ASSERT_EQ(nimcp_brain_workspace_read(brain, read_content, 256, &dim, &source), NIMCP_SUCCESS);
        EXPECT_EQ(source, NIMCP_MODULE_PERCEPTION);
        EXPECT_EQ(dim, 256);
    }

    nimcp_brain_destroy(brain);
}

TEST_F(APIEndToEndTest, GlobalWorkspace_SubscribeAndBroadcast) {
    nimcp_brain_t brain = nimcp_brain_create("gw_subscribe", NIMCP_BRAIN_MEDIUM, NIMCP_TASK_CLASSIFICATION, 256, 10);
    ASSERT_NE(brain, nullptr);

    // Subscribe modules
    nimcp_brain_workspace_subscribe(brain, NIMCP_MODULE_WORKING_MEMORY);
    nimcp_brain_workspace_subscribe(brain, NIMCP_MODULE_EXECUTIVE);

    // Prepare and broadcast content
    float content[256];
    for (int i = 0; i < 256; i++) {
        content[i] = 1.0f;
    }

    nimcp_brain_workspace_compete(brain, NIMCP_MODULE_ATTENTION, content, 256, 0.9f);

    // Unsubscribe
    nimcp_brain_workspace_unsubscribe(brain, NIMCP_MODULE_WORKING_MEMORY);

    nimcp_brain_destroy(brain);
}

TEST_F(APIEndToEndTest, GlobalWorkspace_Statistics) {
    nimcp_brain_t brain = nimcp_brain_create("gw_stats", NIMCP_BRAIN_MEDIUM, NIMCP_TASK_CLASSIFICATION, 256, 10);
    ASSERT_NE(brain, nullptr);

    float content[256] = {0};

    // Multiple competitions
    for (int i = 0; i < 5; i++) {
        nimcp_brain_workspace_compete(brain, NIMCP_MODULE_PERCEPTION, content, 256, 0.7f + i * 0.05f);
    }

    // Get statistics
    uint32_t broadcasts, competitions;
    float avg_strength;
    nimcp_status_t status = nimcp_brain_workspace_stats(brain, &broadcasts, &competitions, &avg_strength);

    if (status == NIMCP_SUCCESS) {
        EXPECT_GE(competitions, 5);
        EXPECT_LE(broadcasts, competitions);
    }

    nimcp_brain_destroy(brain);
}

//=============================================================================
// Error Recovery Scenarios
//=============================================================================

TEST_F(APIEndToEndTest, ErrorRecovery_InvalidLoad) {
    // Try to load non-existent file
    nimcp_brain_t brain = nimcp_brain_load("/nonexistent/path/brain.nimcp");
    EXPECT_EQ(brain, nullptr);

    // Error message should be set
    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);
    EXPECT_NE(strlen(error), 0);
}

TEST_F(APIEndToEndTest, ErrorRecovery_CorruptedFile) {
    const char* corrupt_path = "/tmp/corrupt_brain.nimcp";

    // Create a corrupted file
    FILE* f = fopen(corrupt_path, "wb");
    ASSERT_NE(f, nullptr);
    const char* garbage = "This is not a valid brain file";
    fwrite(garbage, 1, strlen(garbage), f);
    fclose(f);

    // Try to load corrupted file
    nimcp_brain_t brain = nimcp_brain_load(corrupt_path);
    EXPECT_EQ(brain, nullptr);

    cleanup_file(corrupt_path);
}

TEST_F(APIEndToEndTest, ErrorRecovery_AfterFailedSave) {
    nimcp_brain_t brain = nimcp_brain_create("recovery", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    // Try to save to invalid path
    nimcp_status_t status = nimcp_brain_save(brain, "/invalid/directory/brain.nimcp");
    EXPECT_EQ(status, NIMCP_ERROR_IO);

    // Brain should still be usable
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    ASSERT_EQ(nimcp_brain_learn_example(brain, features, 10, "test", 1.0f), NIMCP_SUCCESS);

    nimcp_brain_destroy(brain);
}

//=============================================================================
// Resource Management Tests
//=============================================================================

TEST_F(APIEndToEndTest, ResourceManagement_CreateDestroyMultipleTimes) {
    // Create and destroy brains multiple times
    for (int i = 0; i < 10; i++) {
        nimcp_brain_t brain = nimcp_brain_create("temp", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 2);
        ASSERT_NE(brain, nullptr);

        // Use the brain briefly
        float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
        nimcp_brain_learn_example(brain, features, 10, "test", 1.0f);

        nimcp_brain_destroy(brain);
    }
}

TEST_F(APIEndToEndTest, ResourceManagement_ProbeAfterOperations) {
    nimcp_brain_t brain = nimcp_brain_create("probe_test", NIMCP_BRAIN_MEDIUM, NIMCP_TASK_CLASSIFICATION, 20, 5);
    ASSERT_NE(brain, nullptr);

    // Perform operations
    float features[20];
    for (int i = 0; i < 20; i++) features[i] = static_cast<float>(i);

    for (int i = 0; i < 50; i++) {
        nimcp_brain_learn_example(brain, features, 20, "label", 1.0f);
    }

    // Probe brain state
    nimcp_brain_probe_t probe;
    ASSERT_EQ(nimcp_brain_probe(brain, &probe), NIMCP_SUCCESS);

    // Verify probe data is reasonable
    EXPECT_GT(probe.num_neurons, 0);
    EXPECT_GT(probe.num_synapses, 0);
    EXPECT_EQ(probe.num_inputs, 20);
    EXPECT_EQ(probe.num_outputs, 5);
    EXPECT_GE(probe.total_learning_steps, 50);

    nimcp_brain_destroy(brain);
}

TEST_F(APIEndToEndTest, ResourceManagement_ResizeOperations) {
    nimcp_brain_t brain = nimcp_brain_create("resize_test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    uint32_t initial_count = nimcp_brain_get_neuron_count(brain);
    EXPECT_GT(initial_count, 0);

    // Try manual resize
    bool resize_result = nimcp_brain_resize(brain, initial_count * 2);
    if (resize_result) {
        uint32_t new_count = nimcp_brain_get_neuron_count(brain);
        EXPECT_GT(new_count, initial_count);
    }

    // Try auto resize
    nimcp_brain_auto_resize(brain);

    // Brain should still be functional
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    ASSERT_EQ(nimcp_brain_learn_example(brain, features, 10, "test", 1.0f), NIMCP_SUCCESS);

    nimcp_brain_destroy(brain);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
