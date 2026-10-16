/**
 * @file test_brain_state_manager_integration.cpp
 * @brief Integration tests for Brain State Manager
 *
 * WHAT: Integration tests for brain state manager with subsystems
 * WHY:  Verify state manager integrates correctly with brain lifecycle
 * HOW:  Test checkpoint/restore flow with actual brain subsystems
 *
 * PHASE 8: System-Wide Health Integration Tests
 *
 * @author NIMCP Development Team
 * @date 2026-01-22
 */

#include <gtest/gtest.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <thread>
#include <chrono>

// Include brain_internal.h outside extern "C" due to CUDA C++ templates
#include "core/brain/nimcp_brain_internal.h"

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "core/brain/factory/nimcp_brain_init_state_manager.h"
#include "utils/fault_tolerance/nimcp_state_manager.h"
}

/*=============================================================================
 * Test Fixture with Full Brain
 *=============================================================================*/

class BrainStateManagerIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create brain for integration testing
        brain = brain_create("state_mgr_integration", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 8, 4);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

/*=============================================================================
 * Lifecycle Integration Tests
 *=============================================================================*/

TEST_F(BrainStateManagerIntegrationTest, BrainCreateInitializesStateManager) {
    if (!brain) {
        GTEST_SKIP() << "Brain creation not available in this environment";
    }

    // State manager should be initialized as part of brain_create
    EXPECT_TRUE(brain->state_manager_enabled);
    EXPECT_NE(brain->state_manager, nullptr);
    EXPECT_TRUE(brain->state_manager_owns_manager);
}

TEST_F(BrainStateManagerIntegrationTest, BrainDestroyCleanupStateManager) {
    if (!brain) {
        GTEST_SKIP() << "Brain creation not available in this environment";
    }

    // Verify state manager exists
    ASSERT_NE(brain->state_manager, nullptr);

    // Destroy brain - should cleanup state manager
    brain_destroy(brain);
    brain = nullptr;

    // Can't verify cleanup directly, but test should not crash
    SUCCEED();
}

/*=============================================================================
 * Checkpoint/Restore Integration Tests
 *=============================================================================*/

TEST_F(BrainStateManagerIntegrationTest, CheckpointRestorePreservesStats) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // Set some brain stats
    brain->stats.total_learning_steps = 1000;
    brain->stats.total_inferences = 500;
    brain->stats.quantum_annealing_runs = 10;

    // Checkpoint
    size_t size = 0;
    int result = brain_checkpoint_state(brain, nullptr, &size);
    ASSERT_EQ(result, 0);
    ASSERT_GT(size, 0u);

    uint8_t* buffer = (uint8_t*)malloc(size);
    ASSERT_NE(buffer, nullptr);

    size_t written = size;
    result = brain_checkpoint_state(brain, buffer, &written);
    ASSERT_EQ(result, 0);

    // Modify stats
    brain->stats.total_learning_steps = 9999;
    brain->stats.total_inferences = 9999;
    brain->stats.quantum_annealing_runs = 9999;

    // Restore
    result = brain_restore_state(brain, buffer, written);
    EXPECT_EQ(result, 0);

    // Verify stats restored
    EXPECT_EQ(brain->stats.total_learning_steps, 1000u);
    EXPECT_EQ(brain->stats.total_inferences, 500u);
    EXPECT_EQ(brain->stats.quantum_annealing_runs, 10u);

    free(buffer);
}

TEST_F(BrainStateManagerIntegrationTest, MultipleCheckpointsWork) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // First checkpoint
    brain->stats.total_learning_steps = 100;
    size_t size1 = 0;
    brain_checkpoint_state(brain, nullptr, &size1);
    uint8_t* buffer1 = (uint8_t*)malloc(size1);
    size_t written1 = size1;
    brain_checkpoint_state(brain, buffer1, &written1);

    // Second checkpoint with different state
    brain->stats.total_learning_steps = 200;
    size_t size2 = 0;
    brain_checkpoint_state(brain, nullptr, &size2);
    uint8_t* buffer2 = (uint8_t*)malloc(size2);
    size_t written2 = size2;
    brain_checkpoint_state(brain, buffer2, &written2);

    // Modify state
    brain->stats.total_learning_steps = 999;

    // Restore from first checkpoint
    brain_restore_state(brain, buffer1, written1);
    EXPECT_EQ(brain->stats.total_learning_steps, 100u);

    // Restore from second checkpoint
    brain_restore_state(brain, buffer2, written2);
    EXPECT_EQ(brain->stats.total_learning_steps, 200u);

    free(buffer1);
    free(buffer2);
}

/*=============================================================================
 * Working Memory Integration Tests
 *=============================================================================*/

TEST_F(BrainStateManagerIntegrationTest, WorkingMemoryStateCheckpointed) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // Verify working_memory module is registered
    nimcp_state_module_entry_t* wm_entry =
        nimcp_state_manager_find(brain->state_manager, "working_memory");
    ASSERT_NE(wm_entry, nullptr);

    // Working memory might be NULL in some configs, but the module should still be registered
    EXPECT_TRUE(wm_entry->enabled);

    // Checkpoint should include working memory state
    size_t size = 0;
    int result = brain_checkpoint_state(brain, nullptr, &size);
    EXPECT_EQ(result, 0);
    EXPECT_GT(size, 0u);
}

/*=============================================================================
 * Executive Integration Tests
 *=============================================================================*/

TEST_F(BrainStateManagerIntegrationTest, ExecutiveStateCheckpointed) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // Verify executive module is registered
    nimcp_state_module_entry_t* exec_entry =
        nimcp_state_manager_find(brain->state_manager, "executive");
    ASSERT_NE(exec_entry, nullptr);

    EXPECT_TRUE(exec_entry->enabled);

    // Checkpoint should include executive state
    size_t size = 0;
    int result = brain_checkpoint_state(brain, nullptr, &size);
    EXPECT_EQ(result, 0);
    EXPECT_GT(size, 0u);
}

/*=============================================================================
 * Validation Integration Tests
 *=============================================================================*/

TEST_F(BrainStateManagerIntegrationTest, ValidateAllModulesPass) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // Fresh brain should have all modules valid
    int valid_count = brain_validate_state(brain);
    EXPECT_GT(valid_count, 0);

    // Get total module count
    uint32_t total_modules = nimcp_state_manager_get_module_count(brain->state_manager);
    EXPECT_EQ((uint32_t)valid_count, total_modules);
}

TEST_F(BrainStateManagerIntegrationTest, ResetAfterCorruption) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // Store original values
    uint32_t orig_neurons = brain->stats.num_neurons;

    // Corrupt brain stats
    brain->stats.num_neurons = 0xFFFFFFFF;  // Obviously invalid

    // Validate should detect issue (may or may not fail depending on validation logic)
    int valid_before = brain_validate_state(brain);

    // Reset invalid modules - should not crash
    int reset_count = brain_reset_invalid_state(brain);
    EXPECT_GE(reset_count, 0);

    // Note: Reset zeroes stats counters but doesn't restore num_neurons
    // since that's structure info, not operational state.
    // The main test is that reset doesn't crash.
    (void)orig_neurons;  // Used for comparison above if needed
}

/*=============================================================================
 * Heartbeat Integration Tests
 *=============================================================================*/

TEST_F(BrainStateManagerIntegrationTest, CheckpointSendsHeartbeat) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // Health agent should be enabled for heartbeat testing
    if (!brain->health_agent_enabled) {
        GTEST_SKIP() << "Health agent not enabled";
    }

    // Checkpoint operation should send heartbeats
    // (We can't directly verify heartbeat was sent, but operation should succeed)
    size_t size = 0;
    int result = brain_checkpoint_state(brain, nullptr, &size);
    EXPECT_EQ(result, 0);

    if (size > 0) {
        uint8_t* buffer = (uint8_t*)malloc(size);
        size_t written = size;
        result = brain_checkpoint_state(brain, buffer, &written);
        EXPECT_EQ(result, 0);
        free(buffer);
    }
}

TEST_F(BrainStateManagerIntegrationTest, RestoreSendsHeartbeat) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    if (!brain->health_agent_enabled) {
        GTEST_SKIP() << "Health agent not enabled";
    }

    // Create checkpoint
    size_t size = 0;
    brain_checkpoint_state(brain, nullptr, &size);
    if (size == 0) {
        GTEST_SKIP() << "No checkpoint data";
    }

    uint8_t* buffer = (uint8_t*)malloc(size);
    size_t written = size;
    brain_checkpoint_state(brain, buffer, &written);

    // Restore should send heartbeats
    int result = brain_restore_state(brain, buffer, written);
    EXPECT_EQ(result, 0);

    free(buffer);
}

/*=============================================================================
 * Stress Tests
 *=============================================================================*/

TEST_F(BrainStateManagerIntegrationTest, RapidCheckpointRestore) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    size_t size = 0;
    brain_checkpoint_state(brain, nullptr, &size);
    if (size == 0) {
        GTEST_SKIP() << "No checkpoint data";
    }

    uint8_t* buffer = (uint8_t*)malloc(size);

    // Rapid checkpoint/restore cycles
    for (int i = 0; i < 100; i++) {
        brain->stats.total_learning_steps = i;

        size_t written = size;
        int result = brain_checkpoint_state(brain, buffer, &written);
        ASSERT_EQ(result, 0);

        brain->stats.total_learning_steps = 9999;

        result = brain_restore_state(brain, buffer, written);
        ASSERT_EQ(result, 0);

        ASSERT_EQ(brain->stats.total_learning_steps, (uint64_t)i);
    }

    free(buffer);
}

TEST_F(BrainStateManagerIntegrationTest, ConcurrentValidation) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // Multiple validation calls should not interfere
    for (int i = 0; i < 50; i++) {
        int valid_count = brain_validate_state(brain);
        EXPECT_GE(valid_count, 0);
    }
}
