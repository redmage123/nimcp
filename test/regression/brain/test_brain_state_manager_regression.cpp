/**
 * @file test_brain_state_manager_regression.cpp
 * @brief Regression tests for Brain State Manager API
 *
 * WHAT: Regression tests for brain state manager API stability
 * WHY:  Catch regressions in API behavior and error handling
 * HOW:  Test edge cases, error conditions, and API contracts
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
 * API Contract Tests - NULL Handling
 *=============================================================================*/

TEST(BrainStateManagerAPIRegression, InitNullBrainReturnsFalse) {
    // API CONTRACT: brain_init_state_manager(NULL) must return false
    bool result = brain_init_state_manager(nullptr);
    EXPECT_FALSE(result);
}

TEST(BrainStateManagerAPIRegression, ShutdownNullBrainNoOp) {
    // API CONTRACT: brain_shutdown_state_manager(NULL) must not crash
    brain_shutdown_state_manager(nullptr);
    SUCCEED();
}

TEST(BrainStateManagerAPIRegression, CheckpointNullBrainReturnsNegative) {
    // API CONTRACT: brain_checkpoint_state(NULL, ...) must return -1
    size_t size = 100;
    int result = brain_checkpoint_state(nullptr, nullptr, &size);
    EXPECT_EQ(result, -1);
}

TEST(BrainStateManagerAPIRegression, CheckpointNullSizeReturnsNegative) {
    // API CONTRACT: brain_checkpoint_state(brain, buffer, NULL) must return -1
    brain_t brain = brain_create("test_brain", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);

    if (brain) {
        uint8_t buffer[64];
        int result = brain_checkpoint_state(brain, buffer, nullptr);
        EXPECT_EQ(result, -1);
        brain_destroy(brain);
    }
}

TEST(BrainStateManagerAPIRegression, RestoreNullBrainReturnsNegative) {
    // API CONTRACT: brain_restore_state(NULL, ...) must return -1
    uint8_t buffer[64] = {0};
    int result = brain_restore_state(nullptr, buffer, sizeof(buffer));
    EXPECT_EQ(result, -1);
}

TEST(BrainStateManagerAPIRegression, RestoreNullBufferReturnsNegative) {
    // API CONTRACT: brain_restore_state(brain, NULL, size) must return -1
    brain_t brain = brain_create("test_brain", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);

    if (brain) {
        int result = brain_restore_state(brain, nullptr, 64);
        EXPECT_EQ(result, -1);
        brain_destroy(brain);
    }
}

TEST(BrainStateManagerAPIRegression, ValidateNullBrainReturnsNegative) {
    // API CONTRACT: brain_validate_state(NULL) must return -1
    int result = brain_validate_state(nullptr);
    EXPECT_EQ(result, -1);
}

TEST(BrainStateManagerAPIRegression, ResetInvalidNullBrainReturnsNegative) {
    // API CONTRACT: brain_reset_invalid_state(NULL) must return -1
    int result = brain_reset_invalid_state(nullptr);
    EXPECT_EQ(result, -1);
}

/*=============================================================================
 * API Contract Tests - Disabled State Manager
 *=============================================================================*/

class DisabledStateManagerTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create("disabled_state_test", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 4, 2);

        if (brain && brain->state_manager) {
            brain_shutdown_state_manager(brain);
        }
        if (brain) {
            brain->state_manager_enabled = false;
        }
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

TEST_F(DisabledStateManagerTest, CheckpointReturnsZeroSizeWhenDisabled) {
    if (!brain) {
        GTEST_SKIP() << "Brain creation not available";
    }

    // API CONTRACT: When disabled, checkpoint should return 0 size
    size_t size = 999;
    int result = brain_checkpoint_state(brain, nullptr, &size);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(size, 0u);
}

TEST_F(DisabledStateManagerTest, RestoreReturnsSuccessWhenDisabled) {
    if (!brain) {
        GTEST_SKIP() << "Brain creation not available";
    }

    // API CONTRACT: When disabled, restore should be no-op and return 0
    uint8_t buffer[64] = {1, 2, 3, 4};
    int result = brain_restore_state(brain, buffer, sizeof(buffer));
    EXPECT_EQ(result, 0);
}

TEST_F(DisabledStateManagerTest, ValidateReturnsZeroWhenDisabled) {
    if (!brain) {
        GTEST_SKIP() << "Brain creation not available";
    }

    // API CONTRACT: When disabled, validate should return 0 (no modules)
    int result = brain_validate_state(brain);
    EXPECT_EQ(result, 0);
}

TEST_F(DisabledStateManagerTest, ResetReturnsZeroWhenDisabled) {
    if (!brain) {
        GTEST_SKIP() << "Brain creation not available";
    }

    // API CONTRACT: When disabled, reset should return 0 (no modules)
    int result = brain_reset_invalid_state(brain);
    EXPECT_EQ(result, 0);
}

/*=============================================================================
 * API Contract Tests - Return Values
 *=============================================================================*/

class StateManagerReturnValueTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create("return_value_test", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 4, 2);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

TEST_F(StateManagerReturnValueTest, CheckpointQuerySizeReturnsZero) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // API CONTRACT: Query size (NULL buffer) returns 0 on success
    size_t size = 0;
    int result = brain_checkpoint_state(brain, nullptr, &size);
    EXPECT_EQ(result, 0);
}

TEST_F(StateManagerReturnValueTest, CheckpointWriteReturnsZero) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // API CONTRACT: Successful checkpoint returns 0
    size_t size = 0;
    brain_checkpoint_state(brain, nullptr, &size);

    if (size > 0) {
        uint8_t* buffer = (uint8_t*)malloc(size);
        size_t written = size;
        int result = brain_checkpoint_state(brain, buffer, &written);
        EXPECT_EQ(result, 0);
        free(buffer);
    }
}

TEST_F(StateManagerReturnValueTest, RestoreReturnsZeroOnSuccess) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // API CONTRACT: Successful restore returns 0
    size_t size = 0;
    brain_checkpoint_state(brain, nullptr, &size);

    if (size > 0) {
        uint8_t* buffer = (uint8_t*)malloc(size);
        size_t written = size;
        brain_checkpoint_state(brain, buffer, &written);

        int result = brain_restore_state(brain, buffer, written);
        EXPECT_EQ(result, 0);

        free(buffer);
    }
}

TEST_F(StateManagerReturnValueTest, ValidateReturnsPositiveOnSuccess) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // API CONTRACT: Validate returns number of valid modules (non-negative)
    int result = brain_validate_state(brain);
    EXPECT_GE(result, 0);
}

TEST_F(StateManagerReturnValueTest, ResetReturnsNonNegative) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // API CONTRACT: Reset returns number of modules reset (non-negative)
    int result = brain_reset_invalid_state(brain);
    EXPECT_GE(result, 0);
}

/*=============================================================================
 * Edge Cases
 *=============================================================================*/

TEST_F(StateManagerReturnValueTest, CheckpointWithSmallBufferReturnsSizeNeeded) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // Query actual size needed
    size_t actual_size = 0;
    brain_checkpoint_state(brain, nullptr, &actual_size);

    if (actual_size > 1) {
        // Try with buffer too small
        uint8_t small_buffer[1];
        size_t small_size = 1;
        int result = brain_checkpoint_state(brain, small_buffer, &small_size);

        // Should either fail or indicate size needed
        if (result != 0) {
            EXPECT_EQ(result, -2);  // Buffer too small
            EXPECT_EQ(small_size, actual_size);  // Should report needed size
        }
    }
}

TEST_F(StateManagerReturnValueTest, RestoreWithCorruptedDataHandled) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // Try restoring garbage data
    uint8_t garbage[128];
    memset(garbage, 0xFF, sizeof(garbage));

    // Should not crash, may return error
    int result = brain_restore_state(brain, garbage, sizeof(garbage));
    // Result depends on validation - may succeed if no validation
    // Main test is that it doesn't crash
    (void)result;
    SUCCEED();
}

TEST_F(StateManagerReturnValueTest, DoubleInitIdempotent) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    nimcp_state_manager_t* original = brain->state_manager;

    // Double init should be idempotent
    bool result = brain_init_state_manager(brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(brain->state_manager, original);  // Same manager
}

TEST_F(StateManagerReturnValueTest, DoubleShutdownSafe) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "State manager not available";
    }

    // Double shutdown should be safe
    brain_shutdown_state_manager(brain);
    brain_shutdown_state_manager(brain);  // Should not crash
    SUCCEED();
}

/*=============================================================================
 * Module Registration Regression Tests
 *=============================================================================*/

TEST_F(StateManagerReturnValueTest, BrainStatsModulePriority10) {
    if (!brain || !brain->state_manager_enabled || !brain->state_manager) {
        GTEST_SKIP() << "State manager not available";
    }

    // API CONTRACT: brain_stats registered with priority 10
    nimcp_state_module_entry_t* entry =
        nimcp_state_manager_find(brain->state_manager, "brain_stats");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->priority, 10u);
}

TEST_F(StateManagerReturnValueTest, WorkingMemoryModulePriority20) {
    if (!brain || !brain->state_manager_enabled || !brain->state_manager) {
        GTEST_SKIP() << "State manager not available";
    }

    // API CONTRACT: working_memory registered with priority 20
    nimcp_state_module_entry_t* entry =
        nimcp_state_manager_find(brain->state_manager, "working_memory");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->priority, 20u);
}

TEST_F(StateManagerReturnValueTest, ExecutiveModulePriority30) {
    if (!brain || !brain->state_manager_enabled || !brain->state_manager) {
        GTEST_SKIP() << "State manager not available";
    }

    // API CONTRACT: executive registered with priority 30
    nimcp_state_module_entry_t* entry =
        nimcp_state_manager_find(brain->state_manager, "executive");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->priority, 30u);
}

/*=============================================================================
 * Historical Bug Regression Tests
 *=============================================================================*/

// Add regression tests here for any bugs that are fixed in the future
// Example:
// TEST(BrainStateManagerBugRegression, Bug123_CheckpointOverflowFixed) { ... }
