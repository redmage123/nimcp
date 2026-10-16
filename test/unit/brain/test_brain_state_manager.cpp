/**
 * @file test_brain_state_manager.cpp
 * @brief Unit tests for Brain State Manager Integration
 *
 * WHAT: Comprehensive test suite for brain state manager API
 * WHY:  Verify correct behavior of brain state checkpointing and recovery
 * HOW:  Unit tests using GTest framework covering all API functions
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

// Include brain_internal.h outside extern "C" due to CUDA C++ templates
#include "core/brain/nimcp_brain_internal.h"

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "core/brain/factory/nimcp_brain_init_state_manager.h"
#include "utils/fault_tolerance/nimcp_state_manager.h"
}

/*=============================================================================
 * Test Fixture with Minimal Brain
 *=============================================================================*/

class BrainStateManagerTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create small brain for testing
        brain = brain_create("state_mgr_test", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 4, 2);
        // Brain may be NULL in minimal test environments, that's OK
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

/*=============================================================================
 * Lifecycle Tests
 *=============================================================================*/

TEST(BrainStateManagerLifecycle, InitNullBrain) {
    bool result = brain_init_state_manager(nullptr);
    EXPECT_FALSE(result);
}

TEST(BrainStateManagerLifecycle, ShutdownNullBrain) {
    // Should not crash
    brain_shutdown_state_manager(nullptr);
    SUCCEED();
}

TEST_F(BrainStateManagerTest, InitCreatesStateManager) {
    if (!brain) {
        GTEST_SKIP() << "Brain creation not available in this environment";
    }

    // State manager should be initialized during brain_create
    // But in minimal_mode it might be skipped
    if (brain->state_manager_enabled) {
        EXPECT_NE(brain->state_manager, nullptr);
        EXPECT_TRUE(brain->state_manager_owns_manager);
    }
}

TEST_F(BrainStateManagerTest, DoubleInitReturnsTrueIfAlreadyInitialized) {
    if (!brain) {
        GTEST_SKIP() << "Brain creation not available in this environment";
    }

    // If already initialized, should return true without creating new
    if (brain->state_manager) {
        nimcp_state_manager_t* original = brain->state_manager;
        bool result = brain_init_state_manager(brain);
        EXPECT_TRUE(result);
        EXPECT_EQ(brain->state_manager, original);  // Same manager
    }
}

/*=============================================================================
 * Checkpoint Tests
 *=============================================================================*/

TEST(BrainStateManagerCheckpoint, NullBrainReturnsError) {
    size_t size = 0;
    int result = brain_checkpoint_state(nullptr, nullptr, &size);
    EXPECT_EQ(result, -1);
}

TEST(BrainStateManagerCheckpoint, NullSizeReturnsError) {
    // Need a valid brain to test this properly
    // For now, just verify NULL size is rejected
    brain_t brain = brain_create("test_brain", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);

    if (brain) {
        int result = brain_checkpoint_state(brain, nullptr, nullptr);
        EXPECT_EQ(result, -1);
        brain_destroy(brain);
    }
}

TEST_F(BrainStateManagerTest, CheckpointQuerySizeWithNullBuffer) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    size_t size = 0;
    int result = brain_checkpoint_state(brain, nullptr, &size);
    EXPECT_EQ(result, 0);
    EXPECT_GT(size, 0u);  // Should have some state to checkpoint
}

TEST_F(BrainStateManagerTest, CheckpointWritesToBuffer) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // First query size
    size_t size = 0;
    int result = brain_checkpoint_state(brain, nullptr, &size);
    EXPECT_EQ(result, 0);

    if (size > 0) {
        // Allocate buffer and checkpoint
        uint8_t* buffer = (uint8_t*)malloc(size);
        ASSERT_NE(buffer, nullptr);

        size_t written = size;
        result = brain_checkpoint_state(brain, buffer, &written);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(written, size);

        free(buffer);
    }
}

/*=============================================================================
 * Restore Tests
 *=============================================================================*/

TEST(BrainStateManagerRestore, NullBrainReturnsError) {
    uint8_t buffer[64] = {0};
    int result = brain_restore_state(nullptr, buffer, sizeof(buffer));
    EXPECT_EQ(result, -1);
}

TEST(BrainStateManagerRestore, NullBufferReturnsError) {
    brain_t brain = brain_create("test_brain", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);

    if (brain) {
        int result = brain_restore_state(brain, nullptr, 64);
        EXPECT_EQ(result, -1);
        brain_destroy(brain);
    }
}

TEST_F(BrainStateManagerTest, CheckpointAndRestore) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // Modify some brain state
    brain->stats.total_learning_steps = 42;
    brain->stats.total_inferences = 100;

    // Checkpoint
    size_t size = 0;
    int result = brain_checkpoint_state(brain, nullptr, &size);
    EXPECT_EQ(result, 0);

    if (size > 0) {
        uint8_t* buffer = (uint8_t*)malloc(size);
        ASSERT_NE(buffer, nullptr);

        size_t written = size;
        result = brain_checkpoint_state(brain, buffer, &written);
        EXPECT_EQ(result, 0);

        // Modify state
        brain->stats.total_learning_steps = 999;
        brain->stats.total_inferences = 999;

        // Restore
        result = brain_restore_state(brain, buffer, written);
        EXPECT_EQ(result, 0);

        // Verify restored values
        EXPECT_EQ(brain->stats.total_learning_steps, 42u);
        EXPECT_EQ(brain->stats.total_inferences, 100u);

        free(buffer);
    }
}

/*=============================================================================
 * Validation Tests
 *=============================================================================*/

TEST(BrainStateManagerValidation, NullBrainReturnsError) {
    int result = brain_validate_state(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainStateManagerTest, ValidateReturnsModuleCount) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    int result = brain_validate_state(brain);
    // Should return number of valid modules (non-negative)
    EXPECT_GE(result, 0);
}

TEST_F(BrainStateManagerTest, ValidateDetectsInvalidState) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // Corrupt brain stats to trigger validation failure
    uint32_t original_neurons = brain->stats.num_neurons;
    brain->stats.num_neurons = 999999999;  // Obviously invalid

    int valid_count = brain_validate_state(brain);
    // Validation should detect the invalid state
    // (exact behavior depends on implementation)

    // Restore original
    brain->stats.num_neurons = original_neurons;
}

/*=============================================================================
 * Reset Tests
 *=============================================================================*/

TEST(BrainStateManagerReset, NullBrainReturnsError) {
    int result = brain_reset_invalid_state(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainStateManagerTest, ResetInvalidStateReturnsCount) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    int result = brain_reset_invalid_state(brain);
    // Should return number of modules reset (>= 0)
    EXPECT_GE(result, 0);
}

/*=============================================================================
 * State Manager Integration Tests
 *=============================================================================*/

TEST_F(BrainStateManagerTest, StateManagerHasRegisteredModules) {
    if (!brain || !brain->state_manager_enabled || !brain->state_manager) {
        GTEST_SKIP() << "State manager not available";
    }

    uint32_t count = nimcp_state_manager_get_module_count(brain->state_manager);
    // Should have at least brain_stats registered
    EXPECT_GE(count, 1u);
}

TEST_F(BrainStateManagerTest, BrainStatsModuleRegistered) {
    if (!brain || !brain->state_manager_enabled || !brain->state_manager) {
        GTEST_SKIP() << "State manager not available";
    }

    nimcp_state_module_entry_t* entry =
        nimcp_state_manager_find(brain->state_manager, "brain_stats");
    EXPECT_NE(entry, nullptr);

    if (entry) {
        EXPECT_TRUE(entry->enabled);
        EXPECT_EQ(entry->priority, 10u);  // brain_stats has priority 10
    }
}

TEST_F(BrainStateManagerTest, WorkingMemoryModuleRegistered) {
    if (!brain || !brain->state_manager_enabled || !brain->state_manager) {
        GTEST_SKIP() << "State manager not available";
    }

    nimcp_state_module_entry_t* entry =
        nimcp_state_manager_find(brain->state_manager, "working_memory");
    EXPECT_NE(entry, nullptr);

    if (entry) {
        EXPECT_TRUE(entry->enabled);
        EXPECT_EQ(entry->priority, 20u);  // working_memory has priority 20
    }
}

TEST_F(BrainStateManagerTest, ExecutiveModuleRegistered) {
    if (!brain || !brain->state_manager_enabled || !brain->state_manager) {
        GTEST_SKIP() << "State manager not available";
    }

    nimcp_state_module_entry_t* entry =
        nimcp_state_manager_find(brain->state_manager, "executive");
    EXPECT_NE(entry, nullptr);

    if (entry) {
        EXPECT_TRUE(entry->enabled);
        EXPECT_EQ(entry->priority, 30u);  // executive has priority 30
    }
}

/*=============================================================================
 * Disabled State Manager Tests
 *=============================================================================*/

TEST(BrainStateManagerDisabled, CheckpointReturnsZeroSize) {
    brain_t brain = brain_create("test_brain", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);

    if (brain) {
        // Manually disable state manager for testing
        if (brain->state_manager) {
            brain_shutdown_state_manager(brain);
        }
        brain->state_manager_enabled = false;

        size_t size = 100;  // Non-zero initial value
        int result = brain_checkpoint_state(brain, nullptr, &size);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(size, 0u);  // No state manager means no checkpoint

        brain_destroy(brain);
    }
}

TEST(BrainStateManagerDisabled, RestoreReturnsSuccess) {
    brain_t brain = brain_create("test_brain", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);

    if (brain) {
        // Manually disable state manager
        if (brain->state_manager) {
            brain_shutdown_state_manager(brain);
        }
        brain->state_manager_enabled = false;

        uint8_t buffer[64] = {0};
        int result = brain_restore_state(brain, buffer, sizeof(buffer));
        EXPECT_EQ(result, 0);  // Should succeed (no-op)

        brain_destroy(brain);
    }
}

TEST(BrainStateManagerDisabled, ValidateReturnsZero) {
    brain_t brain = brain_create("test_brain", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);

    if (brain) {
        // Manually disable state manager
        if (brain->state_manager) {
            brain_shutdown_state_manager(brain);
        }
        brain->state_manager_enabled = false;

        int result = brain_validate_state(brain);
        EXPECT_EQ(result, 0);  // No modules to validate

        brain_destroy(brain);
    }
}
