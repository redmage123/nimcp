/**
 * @file test_state_manager.cpp
 * @brief Unit tests for State Manager module registration
 *
 * WHAT: Tests for nimcp_state_manager_* API
 * WHY:  Phase 8 health integration requires state serialization
 * HOW:  Test registration, serialization, deserialization, validation
 *
 * @author NIMCP Team
 * @date 2026-01-25
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <vector>

extern "C" {
#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "utils/memory/nimcp_memory.h"
}

/**
 * @brief Mock module state for testing
 */
typedef struct {
    int value1;
    float value2;
    char name[32];
    bool valid;
} mock_module_state_t;

/**
 * @brief Serialize mock module state
 */
static int mock_serialize(void* module_state, uint8_t* buffer, size_t* size) {
    mock_module_state_t* state = (mock_module_state_t*)module_state;
    size_t required = sizeof(mock_module_state_t);

    if (!buffer) {
        *size = required;
        return 0;
    }

    if (*size < required) {
        *size = required;
        return -2;
    }

    memcpy(buffer, state, required);
    *size = required;
    return 0;
}

/**
 * @brief Deserialize mock module state
 */
static int mock_deserialize(void* module_state, const uint8_t* buffer, size_t size) {
    mock_module_state_t* state = (mock_module_state_t*)module_state;

    if (size < sizeof(mock_module_state_t)) {
        return -1;
    }

    memcpy(state, buffer, sizeof(mock_module_state_t));
    return 0;
}

/**
 * @brief Validate mock module state
 */
static int mock_validate(void* module_state) {
    mock_module_state_t* state = (mock_module_state_t*)module_state;
    return state->valid ? 0 : -1;
}

/**
 * @brief Reset mock module state
 */
static int mock_reset(void* module_state) {
    mock_module_state_t* state = (mock_module_state_t*)module_state;
    state->value1 = 0;
    state->value2 = 0.0f;
    memset(state->name, 0, sizeof(state->name));
    state->valid = true;
    return 0;
}

/**
 * @brief Get mock module state size
 */
static size_t mock_get_size(void* module_state) {
    (void)module_state;
    return sizeof(mock_module_state_t);
}

/**
 * @brief Test fixture for State Manager tests
 */
class StateManagerTest : public ::testing::Test {
protected:
    nimcp_state_manager_t* manager = nullptr;
    mock_module_state_t module_state;
    nimcp_module_state_ops_t ops;

    void SetUp() override {
        manager = nimcp_state_manager_create();

        // Initialize module state
        module_state.value1 = 42;
        module_state.value2 = 3.14f;
        strncpy(module_state.name, "test_module", sizeof(module_state.name) - 1);
        module_state.valid = true;

        // Set up operations
        ops.serialize = mock_serialize;
        ops.deserialize = mock_deserialize;
        ops.validate = mock_validate;
        ops.reset = mock_reset;
        ops.get_size = mock_get_size;
    }

    void TearDown() override {
        if (manager) {
            nimcp_state_manager_destroy(manager);
            manager = nullptr;
        }
    }
};

/**
 * @test Verify state manager creation
 */
TEST_F(StateManagerTest, CreateDestroy) {
    ASSERT_NE(manager, nullptr);
    EXPECT_TRUE(manager->initialized);
    EXPECT_EQ(manager->magic, NIMCP_STATE_MANAGER_MAGIC);
}

/**
 * @test Verify module registration
 */
TEST_F(StateManagerTest, RegisterModule) {
    ASSERT_NE(manager, nullptr);

    int result = nimcp_state_manager_register(manager, "test_module", &ops, &module_state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(manager->module_count, 1u);
}

/**
 * @test Verify duplicate module registration fails
 */
TEST_F(StateManagerTest, RegisterDuplicate) {
    ASSERT_NE(manager, nullptr);

    int result = nimcp_state_manager_register(manager, "test_module", &ops, &module_state);
    EXPECT_EQ(result, 0);

    // Register with same name should fail
    result = nimcp_state_manager_register(manager, "test_module", &ops, &module_state);
    EXPECT_NE(result, 0);
}

/**
 * @test Verify multiple module registration
 */
TEST_F(StateManagerTest, RegisterMultipleModules) {
    ASSERT_NE(manager, nullptr);

    mock_module_state_t states[5];
    for (int i = 0; i < 5; i++) {
        states[i].value1 = i;
        states[i].value2 = (float)i * 1.5f;
        states[i].valid = true;

        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);

        int result = nimcp_state_manager_register(manager, name, &ops, &states[i]);
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(manager->module_count, 5u);
}

/**
 * @test Verify NULL parameter handling
 */
TEST_F(StateManagerTest, NullParameters) {
    // NULL manager
    int result = nimcp_state_manager_register(nullptr, "test", &ops, &module_state);
    EXPECT_NE(result, 0);

    // NULL name
    result = nimcp_state_manager_register(manager, nullptr, &ops, &module_state);
    EXPECT_NE(result, 0);

    // NULL ops
    result = nimcp_state_manager_register(manager, "test", nullptr, &module_state);
    EXPECT_NE(result, 0);
}

/**
 * @test Verify state validation
 */
TEST_F(StateManagerTest, ValidateState) {
    ASSERT_NE(manager, nullptr);

    int result = nimcp_state_manager_register(manager, "test_module", &ops, &module_state);
    ASSERT_EQ(result, 0);

    // Validate all - should pass (returns count of validated modules)
    result = nimcp_state_manager_validate_all(manager);
    EXPECT_GE(result, 1);  // At least 1 module validated successfully

    // Make state invalid
    module_state.valid = false;

    // Validate all - should fail (returns 0 when validation fails)
    result = nimcp_state_manager_validate_all(manager);
    EXPECT_EQ(result, 0);  // No modules validated successfully
}

/**
 * @test Verify state reset
 */
TEST_F(StateManagerTest, ResetState) {
    ASSERT_NE(manager, nullptr);

    int result = nimcp_state_manager_register(manager, "test_module", &ops, &module_state);
    ASSERT_EQ(result, 0);

    // Reset all (returns count of reset modules)
    result = nimcp_state_manager_reset_all(manager);
    EXPECT_GE(result, 1);  // At least 1 module reset successfully

    // State should be reset
    EXPECT_EQ(module_state.value1, 0);
    EXPECT_EQ(module_state.value2, 0.0f);
    EXPECT_TRUE(module_state.valid);
}

/**
 * @test Verify checkpoint (serialize all)
 */
TEST_F(StateManagerTest, CheckpointAll) {
    ASSERT_NE(manager, nullptr);

    int result = nimcp_state_manager_register(manager, "test_module", &ops, &module_state);
    ASSERT_EQ(result, 0);

    // Get required size
    size_t size = 0;
    result = nimcp_state_manager_checkpoint_all(manager, nullptr, &size);
    EXPECT_EQ(result, 0);
    EXPECT_GT(size, 0u);

    // Allocate and checkpoint
    std::vector<uint8_t> buffer(size);
    result = nimcp_state_manager_checkpoint_all(manager, buffer.data(), &size);
    EXPECT_EQ(result, 0);
}

/**
 * @test Verify restore (deserialize all)
 */
TEST_F(StateManagerTest, RestoreAll) {
    ASSERT_NE(manager, nullptr);

    int result = nimcp_state_manager_register(manager, "test_module", &ops, &module_state);
    ASSERT_EQ(result, 0);

    // Save original values
    int original_value1 = module_state.value1;
    float original_value2 = module_state.value2;

    // Checkpoint
    size_t size = 0;
    nimcp_state_manager_checkpoint_all(manager, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    nimcp_state_manager_checkpoint_all(manager, buffer.data(), &size);

    // Modify state
    module_state.value1 = 999;
    module_state.value2 = 999.0f;

    // Restore
    result = nimcp_state_manager_restore_all(manager, buffer.data(), size);
    EXPECT_EQ(result, 0);

    // Values should be restored
    EXPECT_EQ(module_state.value1, original_value1);
    EXPECT_EQ(module_state.value2, original_value2);
}

/**
 * @test Verify module unregistration
 */
TEST_F(StateManagerTest, UnregisterModule) {
    ASSERT_NE(manager, nullptr);

    int result = nimcp_state_manager_register(manager, "test_module", &ops, &module_state);
    ASSERT_EQ(result, 0);
    EXPECT_EQ(manager->module_count, 1u);

    result = nimcp_state_manager_unregister(manager, "test_module");
    EXPECT_EQ(result, 0);
    EXPECT_EQ(manager->module_count, 0u);
}

/**
 * @test Verify unregister nonexistent module
 */
TEST_F(StateManagerTest, UnregisterNonexistent) {
    ASSERT_NE(manager, nullptr);

    int result = nimcp_state_manager_unregister(manager, "nonexistent");
    EXPECT_NE(result, 0);
}

/**
 * @test Verify module lookup
 */
TEST_F(StateManagerTest, FindModule) {
    ASSERT_NE(manager, nullptr);

    int result = nimcp_state_manager_register(manager, "test_module", &ops, &module_state);
    ASSERT_EQ(result, 0);

    nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, "test_module");
    EXPECT_NE(entry, nullptr);
    EXPECT_STREQ(entry->name, "test_module");

    entry = nimcp_state_manager_find(manager, "nonexistent");
    EXPECT_EQ(entry, nullptr);
}

/**
 * @test Verify statistics tracking
 */
TEST_F(StateManagerTest, Statistics) {
    ASSERT_NE(manager, nullptr);

    int result = nimcp_state_manager_register(manager, "test_module", &ops, &module_state);
    ASSERT_EQ(result, 0);

    // Checkpoint
    size_t size = 0;
    nimcp_state_manager_checkpoint_all(manager, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    nimcp_state_manager_checkpoint_all(manager, buffer.data(), &size);

    // Restore
    nimcp_state_manager_restore_all(manager, buffer.data(), size);

    // Validate
    nimcp_state_manager_validate_all(manager);

    // Check statistics
    EXPECT_GE(manager->total_checkpoints, 1u);
    EXPECT_GE(manager->total_restores, 1u);
    EXPECT_GE(manager->total_validations, 1u);
}

/**
 * @test Verify max module limit
 */
TEST_F(StateManagerTest, MaxModuleLimit) {
    ASSERT_NE(manager, nullptr);

    mock_module_state_t states[NIMCP_STATE_MANAGER_MAX_MODULES + 10];

    // Register up to max
    for (int i = 0; i < NIMCP_STATE_MANAGER_MAX_MODULES; i++) {
        states[i].valid = true;
        char name[64];
        snprintf(name, sizeof(name), "module_%d", i);

        int result = nimcp_state_manager_register(manager, name, &ops, &states[i]);
        EXPECT_EQ(result, 0) << "Failed to register module " << i;
    }

    EXPECT_EQ(manager->module_count, (uint32_t)NIMCP_STATE_MANAGER_MAX_MODULES);

    // Try to register one more - should fail
    states[NIMCP_STATE_MANAGER_MAX_MODULES].valid = true;
    int result = nimcp_state_manager_register(manager, "overflow", &ops,
                                               &states[NIMCP_STATE_MANAGER_MAX_MODULES]);
    EXPECT_NE(result, 0);
}

/**
 * @brief Main entry point
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
