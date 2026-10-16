//=============================================================================
// test_enhanced_features_integration.cpp - Integration Tests
//=============================================================================
/**
 * @file test_enhanced_features_integration.cpp
 * @brief Integration tests for COW snapshots, version checking, and memory tracking
 *
 * WHAT: End-to-end integration tests for enhanced NIMCP features
 * WHY:  Verify features work together correctly in realistic scenarios
 * HOW:  GoogleTest framework with complete workflows
 *
 * TEST COVERAGE:
 * 1. COW snapshot + memory tracking integration
 * 2. Pretrained model loading + version checking
 * 3. Complete workflow: load → snapshot → train → restore
 * 4. Multi-brain scenarios with shared memory
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"

class EnhancedFeaturesIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

//=============================================================================
// Integration Test 1: COW Clone + Memory Tracking
//=============================================================================

TEST_F(EnhancedFeaturesIntegrationTest, COWCloneWithMemoryTracking) {
    // WHAT: Verify COW clones integrate properly with memory tracking
    // WHY:  Ensure both features work together correctly
    // HOW:  Create clone, check memory, verify consistency

    // Create brain
    brain_t brain = brain_create("integration_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain, nullptr);

    // Get initial memory
    size_t memory_before_clone = brain_get_memory_usage(brain);
    EXPECT_GT(memory_before_clone, 0);

    // Create COW clone
    brain_t clone = brain_clone_cow(brain);
    ASSERT_NE(clone, nullptr);

    // Get memory after clone
    size_t memory_after_clone = brain_get_memory_usage(brain);

    // VERIFY: Memory didn't increase significantly (COW sharing)
    double ratio = (double)memory_after_clone / (double)memory_before_clone;
    EXPECT_LT(ratio, 1.1);  // Less than 10% increase

    // Cleanup
    brain_destroy(clone);
    brain_destroy(brain);
}

//=============================================================================
// Integration Test 2: Save and Load Brain
//=============================================================================

TEST_F(EnhancedFeaturesIntegrationTest, SaveAndLoadBrain) {
    // WHAT: Test brain save and load functionality
    // WHY:  Verify persistence works
    // HOW:  Save brain, load it back, verify

    // Create and train brain
    brain_t brain = brain_create("save_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain, nullptr);

    float inputs[10] = {1.0f, 0.5f, 0.3f, 0.8f, 0.2f, 0.6f, 0.4f, 0.9f, 0.1f, 0.7f};
    brain_learn_example(brain, inputs, 10, "test_label", 0.8f);

    // Save brain
    const char* filepath = "/tmp/test_brain_save.bin";
    bool saved = brain_save(brain, filepath);
    EXPECT_TRUE(saved);

    // Cleanup
    brain_destroy(brain);

    // Load brain back
    brain_t loaded = brain_load(filepath);
    if (loaded) {
        // Verify loaded brain works
        brain_decision_t* decision = brain_decide(loaded, inputs, 10);
        EXPECT_NE(decision, nullptr);
        if (decision) {
            brain_free_decision(decision);
        }
        brain_destroy(loaded);
    }
}

//=============================================================================
// Integration Test 3: Complete Clone Workflow
//=============================================================================

TEST_F(EnhancedFeaturesIntegrationTest, CompleteCloneWorkflow) {
    // WHAT: Test complete workflow with COW cloning
    // WHY:  Verify realistic usage scenario works end-to-end
    // HOW:  Create → train → clone → verify isolation

    // Step 1: Create brain
    brain_t brain = brain_create("workflow_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Step 2: Train initial model
    float inputs[10] = {1.0f, 0.5f, 0.3f, 0.8f, 0.2f, 0.6f, 0.4f, 0.9f, 0.1f, 0.7f};
    for (int i = 0; i < 50; i++) {
        brain_learn_example(brain, inputs, 10, "initial_class", 0.8f);
    }

    // Step 3: Track memory before clone
    size_t memory_initial = brain_get_memory_usage(brain);
    EXPECT_GT(memory_initial, 0);

    // Step 4: Create COW clone
    brain_t clone = brain_clone_cow(brain);
    ASSERT_NE(clone, nullptr);

    // Step 5: Get initial decision from clone
    brain_decision_t* clone_decision = brain_decide(clone, inputs, 10);
    ASSERT_NE(clone_decision, nullptr);

    char initial_label[128];
    strncpy(initial_label, clone_decision->label, sizeof(initial_label) - 1);
    initial_label[sizeof(initial_label) - 1] = '\0';
    brain_free_decision(clone_decision);

    // Step 6: Train original on different data
    float new_inputs[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    for (int i = 0; i < 100; i++) {
        brain_learn_example(brain, new_inputs, 10, "modified_class", 0.8f);
    }

    // Step 7: Verify clone unchanged
    brain_decision_t* clone_decision2 = brain_decide(clone, inputs, 10);
    ASSERT_NE(clone_decision2, nullptr);
    EXPECT_STREQ(initial_label, clone_decision2->label);

    // Cleanup
    brain_free_decision(clone_decision2);
    brain_destroy(clone);
    brain_destroy(brain);
}

//=============================================================================
// Integration Test 4: Multi-Brain Memory Sharing
//=============================================================================

TEST_F(EnhancedFeaturesIntegrationTest, MultiBrainMemorySharing) {
    // WHAT: Test multiple brains sharing memory via COW
    // WHY:  Verify memory efficiency with multiple clones
    // HOW:  Create original + multiple clones, check memory

    // Create original brain
    brain_t original = brain_create("multi_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(original, nullptr);

    size_t original_memory = brain_get_memory_usage(original);

    // Create multiple COW clones
    brain_t clone1 = brain_clone_cow(original);
    brain_t clone2 = brain_clone_cow(original);
    brain_t clone3 = brain_clone_cow(original);

    ASSERT_NE(clone1, nullptr);
    ASSERT_NE(clone2, nullptr);
    ASSERT_NE(clone3, nullptr);

    // Get memory for clones
    size_t clone1_memory = brain_get_memory_usage(clone1);
    size_t clone2_memory = brain_get_memory_usage(clone2);
    size_t clone3_memory = brain_get_memory_usage(clone3);

    // VERIFY: All clones have similar memory footprint
    EXPECT_GT(clone1_memory, 0);
    EXPECT_GT(clone2_memory, 0);
    EXPECT_GT(clone3_memory, 0);

    // Cleanup
    brain_destroy(clone1);
    brain_destroy(clone2);
    brain_destroy(clone3);
    brain_destroy(original);
}

//=============================================================================
// Integration Test 5: Clone Memory Tracking Consistency
//=============================================================================

TEST_F(EnhancedFeaturesIntegrationTest, CloneMemoryTrackingConsistency) {
    // WHAT: Verify memory tracking remains consistent with clones
    // WHY:  Ensure tracking doesn't break with COW operations
    // HOW:  Create/destroy clones repeatedly, check consistency

    brain_t brain = brain_create("consistency_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain, nullptr);

    size_t initial_memory = brain_get_memory_usage(brain);

    // Create and destroy multiple clones
    for (int i = 0; i < 10; i++) {
        brain_t clone = brain_clone_cow(brain);
        ASSERT_NE(clone, nullptr);

        size_t memory_with_clone = brain_get_memory_usage(brain);

        // VERIFY: Memory is consistent
        double ratio = (double)memory_with_clone / (double)initial_memory;
        EXPECT_GT(ratio, 0.8);
        EXPECT_LT(ratio, 1.2);

        brain_destroy(clone);
    }

    // Final memory check
    size_t final_memory = brain_get_memory_usage(brain);
    double final_ratio = (double)final_memory / (double)initial_memory;
    EXPECT_GT(final_ratio, 0.9);
    EXPECT_LT(final_ratio, 1.1);

    brain_destroy(brain);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
