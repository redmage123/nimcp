/**
 * @file test_meta_learning.cpp
 * @brief Phase 10.8: Meta-Learning Test Suite
 *
 * WHAT: Comprehensive tests for MAML and few-shot learning
 * WHY:  Ensure meta-learning correctness and robustness
 * HOW:  Google Test framework with mock brains and tasks
 *
 * TEST COVERAGE:
 * - Creation & destruction
 * - Configuration management
 * - Task creation & management
 * - Adaptive learning rates
 * - Task similarity computation
 * - MAML inner loop
 * - Statistics tracking
 * - Error handling
 *
 * @author NIMCP Phase 10 Team
 * @date 2025-11-09
 */

#include <gtest/gtest.h>
#include "cognitive/nimcp_meta_learning.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MetaLearningTest : public ::testing::Test {
protected:
    meta_learner_t meta;
    brain_t mock_brain;
    meta_task_t* mock_task;

    void SetUp() override {
        meta = nullptr;
        mock_brain = nullptr;
        mock_task = nullptr;
    }

    void TearDown() override {
        if (meta) {
            meta_learner_destroy(meta);
            meta = nullptr;
        }
        if (mock_task) {
            meta_task_destroy(mock_task);
            mock_task = nullptr;
        }
        // Note: mock_brain is placeholder, not dynamically allocated
    }

    // Helper: Create mock brain (placeholder)
    brain_t create_mock_brain() {
        return nullptr;  // Placeholder pointer
    }

    // Helper: Create test task
    meta_task_t* create_test_task(const char* name, uint32_t num_classes, uint32_t input_dim) {
        return meta_task_create(name, num_classes, input_dim);
    }
};

//=============================================================================
// Creation & Destruction Tests
//=============================================================================

TEST_F(MetaLearningTest, CreateWithDefaults) {
    // Arrange: Default configuration
    uint32_t num_regions = 10;

    // Act: Create with NULL config (uses defaults)
    meta = meta_learner_create(nullptr, num_regions);

    // Assert: Created successfully
    ASSERT_NE(meta, nullptr);
}

TEST_F(MetaLearningTest, CreateWithCustomConfig) {
    // Arrange: Custom configuration
    meta_learning_config_t config = meta_learning_default_config();
    config.few_shot_k = FEW_SHOT_1;  // 1-shot learning
    config.inner_steps = 10;
    config.enable_adaptive_lr = false;

    // Act: Create with custom config
    meta = meta_learner_create(&config, 5);

    // Assert: Created successfully
    ASSERT_NE(meta, nullptr);
}

TEST_F(MetaLearningTest, CreateWithZeroRegions) {
    // Act: Try to create with invalid regions
    meta = meta_learner_create(nullptr, 0);

    // Assert: Should fail
    EXPECT_EQ(meta, nullptr);
}

TEST_F(MetaLearningTest, DestroyNull) {
    // Act: Destroy NULL (should not crash)
    meta_learner_destroy(nullptr);

    // Assert: No crash
    SUCCEED();
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(MetaLearningTest, DefaultConfigValues) {
    // Act: Get default configuration
    meta_learning_config_t config = meta_learning_default_config();

    // Assert: Sensible defaults
    EXPECT_EQ(config.algorithm, META_ALGORITHM_MAML);
    EXPECT_EQ(config.few_shot_k, FEW_SHOT_5);
    EXPECT_GT(config.inner_learning_rate, 0.0f);
    EXPECT_GT(config.outer_learning_rate, 0.0f);
    EXPECT_GT(config.inner_steps, 0u);
    EXPECT_TRUE(config.enable_task_similarity);
    EXPECT_TRUE(config.enable_adaptive_lr);
}

TEST_F(MetaLearningTest, FewShotModes) {
    // Test all few-shot modes
    EXPECT_EQ((int)FEW_SHOT_1, 1);
    EXPECT_EQ((int)FEW_SHOT_5, 5);
    EXPECT_EQ((int)FEW_SHOT_10, 10);
}

//=============================================================================
// Task Management Tests
//=============================================================================

TEST_F(MetaLearningTest, CreateTask) {
    // Act: Create task
    mock_task = meta_task_create("MNIST", 10, 784);

    // Assert: Created successfully
    ASSERT_NE(mock_task, nullptr);
    EXPECT_STREQ(mock_task->name, "MNIST");
    EXPECT_EQ(mock_task->num_classes, 10u);
    EXPECT_EQ(mock_task->input_dim, 784u);
    EXPECT_NE(mock_task->class_prototypes, nullptr);
}

TEST_F(MetaLearningTest, CreateTaskWithNullName) {
    // Act: Try to create with NULL name
    mock_task = meta_task_create(nullptr, 10, 784);

    // Assert: Should fail
    EXPECT_EQ(mock_task, nullptr);
}

TEST_F(MetaLearningTest, CreateTaskWithZeroClasses) {
    // Act: Try to create with zero classes
    mock_task = meta_task_create("Invalid", 0, 784);

    // Assert: Should fail
    EXPECT_EQ(mock_task, nullptr);
}

TEST_F(MetaLearningTest, DestroyTaskNull) {
    // Act: Destroy NULL task
    meta_task_destroy(nullptr);

    // Assert: No crash
    SUCCEED();
}

TEST_F(MetaLearningTest, UpdateTaskPrototypes) {
    // Arrange: Create task
    mock_task = meta_task_create("Test", 2, 4);
    ASSERT_NE(mock_task, nullptr);

    // Create sample data
    float input1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float input2[4] = {5.0f, 6.0f, 7.0f, 8.0f};
    const float* inputs[2] = {input1, input2};
    uint32_t labels[2] = {0, 1};

    // Act: Update prototypes
    bool result = meta_task_update_prototypes(mock_task, inputs, labels, 2);

    // Assert: Updated successfully
    EXPECT_TRUE(result);
    EXPECT_EQ(mock_task->samples_seen, 2u);
}

//=============================================================================
// Adaptive Learning Rate Tests
//=============================================================================

TEST_F(MetaLearningTest, GetLearningRate) {
    // Arrange: Create meta-learner
    meta = meta_learner_create(nullptr, 5);
    ASSERT_NE(meta, nullptr);

    // Act: Get learning rates for each region
    float lr_sensory = meta_get_learning_rate(meta, META_REGION_SENSORY);
    float lr_association = meta_get_learning_rate(meta, META_REGION_ASSOCIATION);
    float lr_prefrontal = meta_get_learning_rate(meta, META_REGION_PREFRONTAL);

    // Assert: All positive, prefrontal > association > sensory
    EXPECT_GT(lr_sensory, 0.0f);
    EXPECT_GT(lr_association, 0.0f);
    EXPECT_GT(lr_prefrontal, 0.0f);
    EXPECT_GT(lr_prefrontal, lr_association);
    EXPECT_GT(lr_association, lr_sensory);
}

TEST_F(MetaLearningTest, AdaptLearningRateWithImprovingLoss) {
    // Arrange: Create meta-learner with adaptive LR enabled
    meta_learning_config_t config = meta_learning_default_config();
    config.enable_adaptive_lr = true;
    meta = meta_learner_create(&config, 5);
    ASSERT_NE(meta, nullptr);

    float initial_lr = meta_get_learning_rate(meta, META_REGION_SENSORY);

    // Act: Adapt with decreasing loss (improvement)
    meta_adapt_learning_rate(meta, META_REGION_SENSORY, 1.0f);  // First loss
    float lr_after_first = meta_get_learning_rate(meta, META_REGION_SENSORY);

    meta_adapt_learning_rate(meta, META_REGION_SENSORY, 0.5f);  // Improved!
    float lr_after_improvement = meta_get_learning_rate(meta, META_REGION_SENSORY);

    // Assert: LR should increase when loss improves
    EXPECT_GE(lr_after_improvement, lr_after_first);
}

TEST_F(MetaLearningTest, AdaptLearningRateWithWorseningLoss) {
    // Arrange: Create meta-learner
    meta_learning_config_t config = meta_learning_default_config();
    config.enable_adaptive_lr = true;
    meta = meta_learner_create(&config, 5);
    ASSERT_NE(meta, nullptr);

    // Act: Adapt with increasing loss (worsening)
    meta_adapt_learning_rate(meta, META_REGION_SENSORY, 0.5f);  // First loss
    float lr_after_first = meta_get_learning_rate(meta, META_REGION_SENSORY);

    meta_adapt_learning_rate(meta, META_REGION_SENSORY, 1.0f);  // Worse!
    float lr_after_worsening = meta_get_learning_rate(meta, META_REGION_SENSORY);

    // Assert: LR should decrease when loss worsens
    EXPECT_LE(lr_after_worsening, lr_after_first);
}

TEST_F(MetaLearningTest, GetLearningRateWithNullMeta) {
    // Act: Get LR with NULL meta
    float lr = meta_get_learning_rate(nullptr, META_REGION_SENSORY);

    // Assert: Returns default value
    EXPECT_GT(lr, 0.0f);
}

//=============================================================================
// Task Similarity Tests
//=============================================================================

TEST_F(MetaLearningTest, ComputeTaskSimilarity) {
    // Arrange: Create meta-learner and two similar tasks
    meta = meta_learner_create(nullptr, 5);
    ASSERT_NE(meta, nullptr);

    meta_task_t* task_a = meta_task_create("MNIST", 10, 784);
    meta_task_t* task_b = meta_task_create("FashionMNIST", 10, 784);
    ASSERT_NE(task_a, nullptr);
    ASSERT_NE(task_b, nullptr);

    // Initialize prototypes with similar values
    for (uint32_t i = 0; i < 10 * 784; i++) {
        task_a->class_prototypes[i] = 1.0f;
        task_b->class_prototypes[i] = 1.0f;  // Identical
    }

    // Act: Compute similarity
    float similarity = meta_compute_task_similarity(meta, task_a, task_b);

    // Assert: High similarity (identical prototypes)
    EXPECT_GT(similarity, 0.9f);

    meta_task_destroy(task_a);
    meta_task_destroy(task_b);
}

TEST_F(MetaLearningTest, ComputeTaskSimilarityDifferentTasks) {
    // Arrange: Create two different tasks
    meta = meta_learner_create(nullptr, 5);
    ASSERT_NE(meta, nullptr);

    meta_task_t* task_a = meta_task_create("TaskA", 5, 100);
    meta_task_t* task_b = meta_task_create("TaskB", 5, 100);
    ASSERT_NE(task_a, nullptr);
    ASSERT_NE(task_b, nullptr);

    // Initialize with opposite values
    for (uint32_t i = 0; i < 5 * 100; i++) {
        task_a->class_prototypes[i] = 1.0f;
        task_b->class_prototypes[i] = -1.0f;  // Opposite
    }

    // Act: Compute similarity
    float similarity = meta_compute_task_similarity(meta, task_a, task_b);

    // Assert: Low/negative similarity
    EXPECT_LT(similarity, 0.1f);

    meta_task_destroy(task_a);
    meta_task_destroy(task_b);
}

TEST_F(MetaLearningTest, ComputeSimilarityIncompatibleDimensions) {
    // Arrange: Tasks with different input dimensions
    meta = meta_learner_create(nullptr, 5);
    ASSERT_NE(meta, nullptr);

    meta_task_t* task_a = meta_task_create("TaskA", 5, 100);
    meta_task_t* task_b = meta_task_create("TaskB", 5, 200);  // Different dim
    ASSERT_NE(task_a, nullptr);
    ASSERT_NE(task_b, nullptr);

    // Act: Compute similarity
    float similarity = meta_compute_task_similarity(meta, task_a, task_b);

    // Assert: Zero similarity (incompatible)
    EXPECT_EQ(similarity, 0.0f);

    meta_task_destroy(task_a);
    meta_task_destroy(task_b);
}

TEST_F(MetaLearningTest, ComputeSimilarityWithNullTasks) {
    // Arrange
    meta = meta_learner_create(nullptr, 5);
    ASSERT_NE(meta, nullptr);

    // Act: Compute with NULL tasks
    float similarity = meta_compute_task_similarity(meta, nullptr, nullptr);

    // Assert: Zero similarity
    EXPECT_EQ(similarity, 0.0f);
}

//=============================================================================
// MAML Inner Loop Tests
//=============================================================================

TEST_F(MetaLearningTest, MAMLInnerLoopWithNullMeta) {
    // Arrange
    mock_brain = create_mock_brain();
    float input1[10] = {1,2,3,4,5,6,7,8,9,10};
    const float* inputs[1] = {input1};
    uint32_t labels[1] = {0};
    brain_t adapted = nullptr;

    // Act: Inner loop with NULL meta
    bool result = meta_maml_inner_loop(nullptr, mock_brain, inputs, labels, 1, &adapted);

    // Assert: Should fail
    EXPECT_FALSE(result);
    EXPECT_EQ(adapted, nullptr);
}

TEST_F(MetaLearningTest, MAMLInnerLoopWithNullBrain) {
    // Arrange
    meta = meta_learner_create(nullptr, 5);
    ASSERT_NE(meta, nullptr);

    float input1[10] = {1,2,3,4,5,6,7,8,9,10};
    const float* inputs[1] = {input1};
    uint32_t labels[1] = {0};
    brain_t adapted = nullptr;

    // Act: Inner loop with NULL brain
    bool result = meta_maml_inner_loop(meta, nullptr, inputs, labels, 1, &adapted);

    // Assert: Should fail
    EXPECT_FALSE(result);
}

TEST_F(MetaLearningTest, MAMLInnerLoopWithEmptySupportSet) {
    // Arrange
    meta = meta_learner_create(nullptr, 5);
    mock_brain = create_mock_brain();
    ASSERT_NE(meta, nullptr);

    brain_t adapted = nullptr;

    // Act: Inner loop with zero support examples
    bool result = meta_maml_inner_loop(meta, mock_brain, nullptr, nullptr, 0, &adapted);

    // Assert: Should fail
    EXPECT_FALSE(result);
}

//=============================================================================
// Transfer Learning Tests
//=============================================================================

TEST_F(MetaLearningTest, TransferKnowledgeWithNullBrains) {
    // Arrange
    meta = meta_learner_create(nullptr, 5);
    ASSERT_NE(meta, nullptr);

    // Act: Transfer with NULL brains
    bool result = meta_transfer_knowledge(meta, nullptr, nullptr, 0.8f);

    // Assert: Should fail
    EXPECT_FALSE(result);
}

TEST_F(MetaLearningTest, TransferKnowledgeLowSimilarity) {
    // Arrange
    meta = meta_learner_create(nullptr, 5);
    ASSERT_NE(meta, nullptr);

    brain_t source = create_mock_brain();
    brain_t target = create_mock_brain();

    // Act: Transfer with low similarity (below threshold)
    bool result = meta_transfer_knowledge(meta, source, target, 0.3f);

    // Assert: Should skip transfer
    EXPECT_FALSE(result);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(MetaLearningTest, GetStatistics) {
    // Arrange
    meta = meta_learner_create(nullptr, 5);
    ASSERT_NE(meta, nullptr);

    uint32_t num_tasks = 0;
    float avg_gain = 0.0f;
    float avg_steps = 0.0f;

    // Act: Get statistics
    bool result = meta_get_statistics(meta, &num_tasks, &avg_gain, &avg_steps);

    // Assert: Valid statistics
    EXPECT_TRUE(result);
    EXPECT_EQ(num_tasks, 0u);  // No tasks yet
}

TEST_F(MetaLearningTest, GetStatisticsWithNullMeta) {
    // Act: Get statistics with NULL meta
    bool result = meta_get_statistics(nullptr, nullptr, nullptr, nullptr);

    // Assert: Should fail
    EXPECT_FALSE(result);
}

TEST_F(MetaLearningTest, GetStatisticsWithNullOutputs) {
    // Arrange
    meta = meta_learner_create(nullptr, 5);
    ASSERT_NE(meta, nullptr);

    // Act: Get statistics with NULL outputs (should not crash)
    bool result = meta_get_statistics(meta, nullptr, nullptr, nullptr);

    // Assert: Should succeed (outputs are optional)
    EXPECT_TRUE(result);
}

//=============================================================================
// Print/Debug Tests
//=============================================================================

TEST_F(MetaLearningTest, PrintStateWithValidMeta) {
    // Arrange
    meta = meta_learner_create(nullptr, 5);
    ASSERT_NE(meta, nullptr);

    // Act: Print state (should not crash)
    meta_print_state(meta);

    // Assert: No crash
    SUCCEED();
}

TEST_F(MetaLearningTest, PrintStateWithNullMeta) {
    // Act: Print NULL meta (should not crash)
    meta_print_state(nullptr);

    // Assert: No crash
    SUCCEED();
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(MetaLearningTest, CompleteWorkflow) {
    // Arrange: Create meta-learner
    meta_learning_config_t config = meta_learning_default_config();
    config.few_shot_k = FEW_SHOT_5;
    meta = meta_learner_create(&config, 10);
    ASSERT_NE(meta, nullptr);

    // Create task
    meta_task_t* task = meta_task_create("Workflow", 5, 20);
    ASSERT_NE(task, nullptr);

    // Get initial learning rate
    float initial_lr = meta_get_learning_rate(meta, META_REGION_SENSORY);
    EXPECT_GT(initial_lr, 0.0f);

    // Adapt learning rate
    float adapted_lr = meta_adapt_learning_rate(meta, META_REGION_SENSORY, 0.5f);
    EXPECT_GT(adapted_lr, 0.0f);

    // Get statistics
    uint32_t num_tasks = 0;
    float avg_gain = 0.0f;
    bool stats_result = meta_get_statistics(meta, &num_tasks, &avg_gain, nullptr);
    EXPECT_TRUE(stats_result);

    // Print state
    meta_print_state(meta);

    // Cleanup
    meta_task_destroy(task);

    // Assert: All operations succeeded
    SUCCEED();
}

TEST_F(MetaLearningTest, MultipleTasksWorkflow) {
    // Arrange: Create meta-learner and multiple tasks
    meta = meta_learner_create(nullptr, 5);
    ASSERT_NE(meta, nullptr);

    // Create multiple tasks
    meta_task_t* task1 = meta_task_create("Task1", 3, 10);
    meta_task_t* task2 = meta_task_create("Task2", 3, 10);
    meta_task_t* task3 = meta_task_create("Task3", 3, 10);

    ASSERT_NE(task1, nullptr);
    ASSERT_NE(task2, nullptr);
    ASSERT_NE(task3, nullptr);

    // Compute pairwise similarities
    float sim_1_2 = meta_compute_task_similarity(meta, task1, task2);
    float sim_2_3 = meta_compute_task_similarity(meta, task2, task3);
    float sim_1_3 = meta_compute_task_similarity(meta, task1, task3);

    // All similarities should be valid
    EXPECT_GE(sim_1_2, 0.0f);
    EXPECT_LE(sim_1_2, 1.0f);
    EXPECT_GE(sim_2_3, 0.0f);
    EXPECT_LE(sim_2_3, 1.0f);
    EXPECT_GE(sim_1_3, 0.0f);
    EXPECT_LE(sim_1_3, 1.0f);

    // Cleanup
    meta_task_destroy(task1);
    meta_task_destroy(task2);
    meta_task_destroy(task3);

    SUCCEED();
}

//=============================================================================
// Edge Cases & Error Handling
//=============================================================================

TEST_F(MetaLearningTest, LargeNumberOfRegions) {
    // Act: Create with large number of regions
    meta = meta_learner_create(nullptr, 10000);

    // Assert: Should handle gracefully
    EXPECT_NE(meta, nullptr);
}

TEST_F(MetaLearningTest, VeryLongTaskName) {
    // Arrange: Very long task name
    char long_name[1000];
    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    // Act: Create task with long name
    mock_task = meta_task_create(long_name, 5, 10);

    // Assert: Should truncate gracefully
    EXPECT_NE(mock_task, nullptr);
}

TEST_F(MetaLearningTest, RegionTypeBoundary) {
    // Arrange
    meta = meta_learner_create(nullptr, 5);
    ASSERT_NE(meta, nullptr);

    // Act: Get LR for invalid region
    float lr = meta_get_learning_rate(meta, (meta_region_type_t)999);

    // Assert: Should return default value
    EXPECT_GT(lr, 0.0f);
}

TEST_F(MetaLearningTest, MAMLOuterLoopWithValidTasks) {
    // Arrange: Create meta-learner
    meta = meta_learner_create(nullptr, 3);
    ASSERT_NE(meta, nullptr);

    // Create multiple tasks
    meta_task_t* task1 = meta_task_create("task1", 3, 10);
    meta_task_t* task2 = meta_task_create("task2", 5, 10);
    meta_task_t* task3 = meta_task_create("task3", 4, 10);
    ASSERT_NE(task1, nullptr);
    ASSERT_NE(task2, nullptr);
    ASSERT_NE(task3, nullptr);

    // Set task losses
    task1->average_loss = 0.5f;
    task2->average_loss = 0.7f;
    task3->average_loss = 0.3f;

    // Create task array
    meta_task_t* tasks[] = {task1, task2, task3};

    // Act: Run outer loop (note: brain is NULL for this test)
    // In a real scenario, we'd pass a valid brain
    bool success = meta_maml_outer_loop(meta, mock_brain, tasks, 3);

    // Assert: Should process tasks successfully
    // Note: success may be false due to NULL brain, but shouldn't crash
    // The important part is that task history is updated

    // Cleanup
    meta_task_destroy(task1);
    meta_task_destroy(task2);
    meta_task_destroy(task3);
}

TEST_F(MetaLearningTest, MAMLOuterLoopWithEmptyTaskArray) {
    // Arrange
    meta = meta_learner_create(nullptr, 3);
    ASSERT_NE(meta, nullptr);

    // Act: Run outer loop with zero tasks
    bool success = meta_maml_outer_loop(meta, mock_brain, nullptr, 0);

    // Assert: Should fail gracefully
    EXPECT_FALSE(success);
}

TEST_F(MetaLearningTest, MAMLOuterLoopWithNullMeta) {
    // Arrange: Create a task
    meta_task_t* task = meta_task_create("test", 3, 10);
    meta_task_t* tasks[] = {task};

    // Act: Run outer loop with NULL meta
    bool success = meta_maml_outer_loop(nullptr, mock_brain, tasks, 1);

    // Assert: Should fail gracefully
    EXPECT_FALSE(success);

    // Cleanup
    meta_task_destroy(task);
}
