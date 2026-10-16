/**
 * @file test_training_state_manager.cpp
 * @brief Unit tests for Training State Manager
 *
 * WHAT: Tests for training state manager registration and serialization
 * WHY:  Phase 8 requires checkpointing for training modules
 * HOW:  Test registry creation, module registration, checkpoint/restore
 *
 * @author NIMCP Team
 * @date 2026-01-25
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <vector>

extern "C" {
#include "training/integration/nimcp_training_state_manager.h"
#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "utils/memory/nimcp_memory.h"
}

/**
 * @brief Test fixture for Training State Manager tests
 */
class TrainingStateManagerTest : public ::testing::Test {
protected:
    training_state_registry_t* registry = nullptr;
    nimcp_state_manager_t* state_manager = nullptr;

    void SetUp() override {
        registry = training_state_registry_create();
        state_manager = nimcp_state_manager_create();
    }

    void TearDown() override {
        if (registry) {
            training_state_registry_destroy(registry);
            registry = nullptr;
        }
        if (state_manager) {
            nimcp_state_manager_destroy(state_manager);
            state_manager = nullptr;
        }
    }
};

/**
 * @test Verify registry creation and destruction
 */
TEST_F(TrainingStateManagerTest, CreateDestroy) {
    ASSERT_NE(registry, nullptr);
    EXPECT_TRUE(registry->initialized);
    EXPECT_EQ(registry->magic, 0x54535247);  // "TSRG"
    EXPECT_EQ(registry->module_count, 0u);
}

/**
 * @test Verify NULL registry creation handling
 */
TEST_F(TrainingStateManagerTest, NullDestroy) {
    // Should not crash
    training_state_registry_destroy(nullptr);
}

/**
 * @test Verify distributed training registration
 */
TEST_F(TrainingStateManagerTest, RegisterDistributed) {
    ASSERT_NE(registry, nullptr);

    // Register with NULL context (valid - context may be created later)
    int result = training_state_register_distributed(registry, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(registry->module_count, 1u);
    EXPECT_TRUE(training_state_is_registered(registry, "distributed_training"));
}

/**
 * @test Verify meta-learning registration
 */
TEST_F(TrainingStateManagerTest, RegisterMetaLearning) {
    ASSERT_NE(registry, nullptr);

    int result = training_state_register_meta_learning(registry, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(registry->module_count, 1u);
    EXPECT_TRUE(training_state_is_registered(registry, "meta_learning"));
}

/**
 * @test Verify adversarial training registration
 */
TEST_F(TrainingStateManagerTest, RegisterAdversarial) {
    ASSERT_NE(registry, nullptr);

    int result = training_state_register_adversarial(registry, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(registry->module_count, 1u);
    EXPECT_TRUE(training_state_is_registered(registry, "adversarial_training"));
}

/**
 * @test Verify HPO registration
 */
TEST_F(TrainingStateManagerTest, RegisterHPO) {
    ASSERT_NE(registry, nullptr);

    int result = training_state_register_hpo(registry, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(registry->module_count, 1u);
    EXPECT_TRUE(training_state_is_registered(registry, "hyperparameter_optimization"));
}

/**
 * @test Verify multiple module registration
 */
TEST_F(TrainingStateManagerTest, RegisterMultipleModules) {
    ASSERT_NE(registry, nullptr);

    EXPECT_EQ(training_state_register_distributed(registry, nullptr), 0);
    EXPECT_EQ(training_state_register_meta_learning(registry, nullptr), 0);
    EXPECT_EQ(training_state_register_adversarial(registry, nullptr), 0);
    EXPECT_EQ(training_state_register_hpo(registry, nullptr), 0);

    EXPECT_EQ(registry->module_count, 4u);
    EXPECT_EQ(training_state_get_module_count(registry), 4u);
}

/**
 * @test Verify duplicate registration fails
 */
TEST_F(TrainingStateManagerTest, RegisterDuplicate) {
    ASSERT_NE(registry, nullptr);

    EXPECT_EQ(training_state_register_distributed(registry, nullptr), 0);
    EXPECT_NE(training_state_register_distributed(registry, nullptr), 0);
    EXPECT_EQ(registry->module_count, 1u);
}

/**
 * @test Verify module unregistration
 */
TEST_F(TrainingStateManagerTest, UnregisterModule) {
    ASSERT_NE(registry, nullptr);

    EXPECT_EQ(training_state_register_distributed(registry, nullptr), 0);
    EXPECT_EQ(training_state_register_meta_learning(registry, nullptr), 0);
    EXPECT_EQ(registry->module_count, 2u);

    EXPECT_EQ(training_state_unregister(registry, "distributed_training"), 0);
    EXPECT_EQ(registry->module_count, 1u);
    EXPECT_FALSE(training_state_is_registered(registry, "distributed_training"));
    EXPECT_TRUE(training_state_is_registered(registry, "meta_learning"));
}

/**
 * @test Verify unregister non-existent module
 */
TEST_F(TrainingStateManagerTest, UnregisterNonexistent) {
    ASSERT_NE(registry, nullptr);

    EXPECT_NE(training_state_unregister(registry, "nonexistent"), 0);
}

/**
 * @test Verify state manager linking
 */
TEST_F(TrainingStateManagerTest, LinkToStateManager) {
    ASSERT_NE(registry, nullptr);
    ASSERT_NE(state_manager, nullptr);

    // Register some modules first
    training_state_register_distributed(registry, nullptr);
    training_state_register_meta_learning(registry, nullptr);

    // Link to state manager
    int result = training_state_link_to_manager(registry, state_manager);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(registry->state_manager, state_manager);
}

/**
 * @test Verify state manager unlinking
 */
TEST_F(TrainingStateManagerTest, UnlinkFromStateManager) {
    ASSERT_NE(registry, nullptr);
    ASSERT_NE(state_manager, nullptr);

    training_state_register_distributed(registry, nullptr);
    training_state_link_to_manager(registry, state_manager);

    int result = training_state_unlink_from_manager(registry);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(registry->state_manager, nullptr);
}

/**
 * @test Verify checkpoint size calculation
 */
TEST_F(TrainingStateManagerTest, CheckpointSizeCalculation) {
    ASSERT_NE(registry, nullptr);

    // No modules = no size
    EXPECT_EQ(training_state_get_total_size(registry), 0u);

    // Add modules
    training_state_register_distributed(registry, nullptr);
    size_t size1 = training_state_get_total_size(registry);
    EXPECT_GT(size1, 0u);

    training_state_register_meta_learning(registry, nullptr);
    size_t size2 = training_state_get_total_size(registry);
    EXPECT_GT(size2, size1);
}

/**
 * @test Verify direct checkpoint/restore without state manager
 */
TEST_F(TrainingStateManagerTest, DirectCheckpointRestore) {
    ASSERT_NE(registry, nullptr);

    training_state_register_distributed(registry, nullptr);
    training_state_register_meta_learning(registry, nullptr);

    // Get size
    size_t size = 0;
    int result = training_state_checkpoint_all(registry, nullptr, &size);
    EXPECT_EQ(result, 0);
    EXPECT_GT(size, 0u);

    // Allocate and checkpoint
    std::vector<uint8_t> buffer(size);
    result = training_state_checkpoint_all(registry, buffer.data(), &size);
    EXPECT_EQ(result, 0);

    // Restore
    result = training_state_restore_all(registry, buffer.data(), size);
    EXPECT_EQ(result, 0);
}

/**
 * @test Verify validation
 */
TEST_F(TrainingStateManagerTest, ValidateAll) {
    ASSERT_NE(registry, nullptr);

    training_state_register_distributed(registry, nullptr);
    training_state_register_meta_learning(registry, nullptr);

    // All modules should validate (return count >= 0)
    int result = training_state_validate_all(registry);
    EXPECT_GE(result, 0);
}

/**
 * @test Verify reset
 */
TEST_F(TrainingStateManagerTest, ResetAll) {
    ASSERT_NE(registry, nullptr);

    training_state_register_distributed(registry, nullptr);
    training_state_register_adversarial(registry, nullptr);

    int result = training_state_reset_all(registry);
    EXPECT_GE(result, 0);
}

/**
 * @test Verify statistics retrieval
 */
TEST_F(TrainingStateManagerTest, GetStatistics) {
    ASSERT_NE(registry, nullptr);

    training_state_register_distributed(registry, nullptr);
    training_state_register_hpo(registry, nullptr);

    // Perform some operations
    size_t size = 0;
    training_state_checkpoint_all(registry, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    training_state_checkpoint_all(registry, buffer.data(), &size);
    training_state_restore_all(registry, buffer.data(), size);

    training_state_stats_t stats;
    int result = training_state_get_stats(registry, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(stats.registered_modules, 2u);
    EXPECT_GE(stats.total_checkpoints, 1u);
    EXPECT_GE(stats.total_restores, 1u);
    EXPECT_GT(stats.total_state_size, 0u);
}

/**
 * @test Verify NULL parameter handling
 */
TEST_F(TrainingStateManagerTest, NullParameters) {
    // NULL registry
    EXPECT_NE(training_state_register_distributed(nullptr, nullptr), 0);
    EXPECT_NE(training_state_register_meta_learning(nullptr, nullptr), 0);
    EXPECT_NE(training_state_register_adversarial(nullptr, nullptr), 0);
    EXPECT_NE(training_state_register_hpo(nullptr, nullptr), 0);

    // NULL name for unregister
    EXPECT_NE(training_state_unregister(registry, nullptr), 0);

    // NULL for link
    EXPECT_NE(training_state_link_to_manager(nullptr, state_manager), 0);
    EXPECT_NE(training_state_link_to_manager(registry, nullptr), 0);

    // NULL for checkpoint
    size_t size = 0;
    EXPECT_NE(training_state_checkpoint_all(nullptr, nullptr, &size), 0);
    EXPECT_NE(training_state_checkpoint_all(registry, nullptr, nullptr), 0);

    // NULL for restore
    EXPECT_NE(training_state_restore_all(nullptr, nullptr, 0), 0);
    EXPECT_NE(training_state_restore_all(registry, nullptr, 0), 0);

    // NULL for stats
    training_state_stats_t stats;
    EXPECT_NE(training_state_get_stats(nullptr, &stats), 0);
    EXPECT_NE(training_state_get_stats(registry, nullptr), 0);
}

/**
 * @test Verify buffer too small handling
 */
TEST_F(TrainingStateManagerTest, BufferTooSmall) {
    ASSERT_NE(registry, nullptr);

    training_state_register_distributed(registry, nullptr);

    // Get required size
    size_t required_size = 0;
    training_state_checkpoint_all(registry, nullptr, &required_size);

    // Try with too small buffer
    std::vector<uint8_t> small_buffer(1);
    size_t small_size = 1;
    int result = training_state_checkpoint_all(registry, small_buffer.data(), &small_size);
    EXPECT_EQ(result, -2);  // Buffer too small
    EXPECT_EQ(small_size, required_size);  // Should report required size
}

/**
 * @test Verify checkpoint via state manager
 */
TEST_F(TrainingStateManagerTest, CheckpointViaStateManager) {
    ASSERT_NE(registry, nullptr);
    ASSERT_NE(state_manager, nullptr);

    training_state_register_distributed(registry, nullptr);
    training_state_register_meta_learning(registry, nullptr);
    training_state_link_to_manager(registry, state_manager);

    // Checkpoint via state manager
    size_t size = 0;
    int result = nimcp_state_manager_checkpoint_all(state_manager, nullptr, &size);
    EXPECT_EQ(result, 0);

    if (size > 0) {
        std::vector<uint8_t> buffer(size);
        result = nimcp_state_manager_checkpoint_all(state_manager, buffer.data(), &size);
        EXPECT_EQ(result, 0);

        // Restore
        result = nimcp_state_manager_restore_all(state_manager, buffer.data(), size);
        EXPECT_EQ(result, 0);
    }
}

/**
 * @test Verify module count queries
 */
TEST_F(TrainingStateManagerTest, ModuleCountQueries) {
    ASSERT_NE(registry, nullptr);

    EXPECT_EQ(training_state_get_module_count(registry), 0u);

    training_state_register_distributed(registry, nullptr);
    EXPECT_EQ(training_state_get_module_count(registry), 1u);

    training_state_register_meta_learning(registry, nullptr);
    EXPECT_EQ(training_state_get_module_count(registry), 2u);

    training_state_register_adversarial(registry, nullptr);
    EXPECT_EQ(training_state_get_module_count(registry), 3u);

    training_state_register_hpo(registry, nullptr);
    EXPECT_EQ(training_state_get_module_count(registry), 4u);

    training_state_unregister(registry, "distributed_training");
    EXPECT_EQ(training_state_get_module_count(registry), 3u);
}

/**
 * @test Verify is_registered queries
 */
TEST_F(TrainingStateManagerTest, IsRegisteredQueries) {
    ASSERT_NE(registry, nullptr);

    // Nothing registered
    EXPECT_FALSE(training_state_is_registered(registry, "distributed_training"));
    EXPECT_FALSE(training_state_is_registered(registry, "meta_learning"));

    training_state_register_distributed(registry, nullptr);
    EXPECT_TRUE(training_state_is_registered(registry, "distributed_training"));
    EXPECT_FALSE(training_state_is_registered(registry, "meta_learning"));

    training_state_register_meta_learning(registry, nullptr);
    EXPECT_TRUE(training_state_is_registered(registry, "distributed_training"));
    EXPECT_TRUE(training_state_is_registered(registry, "meta_learning"));
}

/**
 * @brief Main entry point
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
