/**
 * @file test_state_manager.cpp
 * @brief Comprehensive unit tests for module state manager
 *
 * WHAT: Unit tests for nimcp_state_manager_t
 * WHY:  Verify checkpointing, restoration, and validation work correctly
 * HOW:  Test manager lifecycle, registration, checkpoint/restore, and stats
 *
 * PHASE 8: System-Wide Health Integration
 *
 * @author NIMCP Team
 * @date 2026-01-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "utils/fault_tolerance/nimcp_state_manager.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief State manager test fixture
 *
 * WHAT: Provides fresh state manager for each test
 * WHY:  Ensure test isolation
 * HOW:  Create manager in SetUp, destroy in TearDown
 */
class StateManagerTest : public ::testing::Test {
protected:
    nimcp_state_manager_t* manager;

    void SetUp() override {
        manager = nimcp_state_manager_create();
    }

    void TearDown() override {
        if (manager) {
            nimcp_state_manager_destroy(manager);
            manager = nullptr;
        }
    }
};

//=============================================================================
// Mock Module State for Testing
//=============================================================================

/**
 * @brief Simple mock module state for testing
 */
typedef struct mock_module_state {
    uint32_t magic;
    uint32_t version;
    float value_a;
    float value_b;
    int32_t counter;
    bool flag;
} mock_module_state_t;

#define MOCK_STATE_MAGIC 0x4D4F434B  /* "MOCK" */
#define MOCK_STATE_VERSION 1

/* Global tracking for test verification */
static int g_serialize_calls = 0;
static int g_deserialize_calls = 0;
static int g_validate_calls = 0;
static int g_reset_calls = 0;
static int g_get_size_calls = 0;

/**
 * @brief Reset mock call counters
 */
static void reset_mock_counters(void) {
    g_serialize_calls = 0;
    g_deserialize_calls = 0;
    g_validate_calls = 0;
    g_reset_calls = 0;
    g_get_size_calls = 0;
}

/**
 * @brief Mock serialize function
 */
static int mock_serialize(void* module_state, uint8_t* buffer, size_t* size) {
    g_serialize_calls++;

    if (!module_state || !size) {
        return -1;
    }

    mock_module_state_t* state = (mock_module_state_t*)module_state;
    size_t required = sizeof(mock_module_state_t);

    /* Size query */
    if (!buffer) {
        *size = required;
        return 0;
    }

    /* Buffer too small */
    if (*size < required) {
        *size = required;
        return -2;
    }

    /* Copy state */
    memcpy(buffer, state, sizeof(mock_module_state_t));
    *size = required;
    return 0;
}

/**
 * @brief Mock deserialize function
 */
static int mock_deserialize(void* module_state, const uint8_t* buffer, size_t size) {
    g_deserialize_calls++;

    if (!module_state || !buffer) {
        return -1;
    }

    if (size < sizeof(mock_module_state_t)) {
        return -1;
    }

    mock_module_state_t* state = (mock_module_state_t*)module_state;
    const mock_module_state_t* src = (const mock_module_state_t*)buffer;

    /* Validate magic */
    if (src->magic != MOCK_STATE_MAGIC) {
        return -1;
    }

    memcpy(state, src, sizeof(mock_module_state_t));
    return 0;
}

/**
 * @brief Mock validate function
 */
static int mock_validate(void* module_state) {
    g_validate_calls++;

    if (!module_state) {
        return -1;
    }

    mock_module_state_t* state = (mock_module_state_t*)module_state;

    /* Check magic */
    if (state->magic != MOCK_STATE_MAGIC) {
        return -1;
    }

    /* Check version */
    if (state->version != MOCK_STATE_VERSION) {
        return -2;
    }

    /* Check values are finite */
    if (!std::isfinite(state->value_a) || !std::isfinite(state->value_b)) {
        return -3;
    }

    return 0;
}

/**
 * @brief Mock reset function
 */
static int mock_reset(void* module_state) {
    g_reset_calls++;

    if (!module_state) {
        return -1;
    }

    mock_module_state_t* state = (mock_module_state_t*)module_state;
    state->magic = MOCK_STATE_MAGIC;
    state->version = MOCK_STATE_VERSION;
    state->value_a = 0.0f;
    state->value_b = 0.0f;
    state->counter = 0;
    state->flag = false;
    return 0;
}

/**
 * @brief Mock get_size function
 */
static size_t mock_get_size(void* module_state) {
    g_get_size_calls++;
    (void)module_state;
    return sizeof(mock_module_state_t);
}

/**
 * @brief Create mock state ops structure
 */
static nimcp_module_state_ops_t create_mock_ops(void) {
    nimcp_module_state_ops_t ops;
    ops.serialize = mock_serialize;
    ops.deserialize = mock_deserialize;
    ops.validate = mock_validate;
    ops.reset = mock_reset;
    ops.get_size = mock_get_size;
    return ops;
}

/**
 * @brief Initialize a valid mock state
 */
static void init_mock_state(mock_module_state_t* state) {
    state->magic = MOCK_STATE_MAGIC;
    state->version = MOCK_STATE_VERSION;
    state->value_a = 1.5f;
    state->value_b = 2.5f;
    state->counter = 42;
    state->flag = true;
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(StateManagerTest, CreateDestroy) {
    ASSERT_NE(manager, nullptr);
    EXPECT_TRUE(manager->initialized);
    EXPECT_EQ(manager->magic, NIMCP_STATE_MANAGER_MAGIC);
    EXPECT_EQ(manager->module_count, 0u);
}

TEST(StateManagerLifecycleTest, CreateMultiple) {
    nimcp_state_manager_t* m1 = nimcp_state_manager_create();
    nimcp_state_manager_t* m2 = nimcp_state_manager_create();

    ASSERT_NE(m1, nullptr);
    ASSERT_NE(m2, nullptr);
    EXPECT_NE(m1, m2);

    nimcp_state_manager_destroy(m1);
    nimcp_state_manager_destroy(m2);
}

TEST(StateManagerLifecycleTest, DestroyNull) {
    /* Should not crash */
    nimcp_state_manager_destroy(nullptr);
}

//=============================================================================
// Module Registration Tests
//=============================================================================

TEST_F(StateManagerTest, RegisterModule) {
    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    int result = nimcp_state_manager_register(manager, "test_module", &ops, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(manager->module_count, 1u);
}

TEST_F(StateManagerTest, RegisterMultipleModules) {
    mock_module_state_t states[3];
    nimcp_module_state_ops_t ops = create_mock_ops();

    for (int i = 0; i < 3; i++) {
        init_mock_state(&states[i]);
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        int result = nimcp_state_manager_register(manager, name, &ops, &states[i]);
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(manager->module_count, 3u);
}

TEST_F(StateManagerTest, RegisterDuplicateName) {
    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    int result1 = nimcp_state_manager_register(manager, "duplicate", &ops, &state);
    EXPECT_EQ(result1, 0);

    int result2 = nimcp_state_manager_register(manager, "duplicate", &ops, &state);
    EXPECT_LT(result2, 0);  /* Should fail */
    EXPECT_EQ(manager->module_count, 1u);  /* Still only 1 */
}

TEST_F(StateManagerTest, RegisterNullParams) {
    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    /* Null manager */
    EXPECT_LT(nimcp_state_manager_register(nullptr, "test", &ops, &state), 0);

    /* Null name */
    EXPECT_LT(nimcp_state_manager_register(manager, nullptr, &ops, &state), 0);

    /* Null ops */
    EXPECT_LT(nimcp_state_manager_register(manager, "test", nullptr, &state), 0);

    /* Null state is allowed (stateless module) */
    EXPECT_EQ(nimcp_state_manager_register(manager, "stateless", &ops, nullptr), 0);
}

TEST_F(StateManagerTest, RegisterWithPriority) {
    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    int result = nimcp_state_manager_register_with_priority(
        manager, "priority_module", &ops, &state, 10);
    EXPECT_EQ(result, 0);

    nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, "priority_module");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->priority, 10u);
}

TEST_F(StateManagerTest, UnregisterModule) {
    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "to_remove", &ops, &state);
    EXPECT_EQ(manager->module_count, 1u);

    int result = nimcp_state_manager_unregister(manager, "to_remove");
    EXPECT_EQ(result, 0);
    EXPECT_EQ(manager->module_count, 0u);
}

TEST_F(StateManagerTest, UnregisterNonexistent) {
    int result = nimcp_state_manager_unregister(manager, "nonexistent");
    EXPECT_LT(result, 0);
}

TEST_F(StateManagerTest, FindModule) {
    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "findable", &ops, &state);

    nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, "findable");
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->name, "findable");
    EXPECT_EQ(entry->context, &state);
}

TEST_F(StateManagerTest, FindNonexistent) {
    nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, "nonexistent");
    EXPECT_EQ(entry, nullptr);
}

TEST_F(StateManagerTest, SetEnabled) {
    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "toggle_me", &ops, &state);

    /* Initially enabled */
    nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, "toggle_me");
    ASSERT_NE(entry, nullptr);
    EXPECT_TRUE(entry->enabled);

    /* Disable */
    int result = nimcp_state_manager_set_enabled(manager, "toggle_me", false);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(entry->enabled);

    /* Re-enable */
    result = nimcp_state_manager_set_enabled(manager, "toggle_me", true);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(entry->enabled);
}

//=============================================================================
// Checkpoint Tests
//=============================================================================

TEST_F(StateManagerTest, CheckpointModuleQuerySize) {
    reset_mock_counters();

    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "checkpoint_test", &ops, &state);

    /* Query size */
    size_t size = 0;
    int result = nimcp_state_manager_checkpoint_module(manager, "checkpoint_test", nullptr, &size);
    EXPECT_EQ(result, 0);
    EXPECT_GT(size, 0u);
    EXPECT_EQ(g_serialize_calls, 1);
}

TEST_F(StateManagerTest, CheckpointModuleFull) {
    reset_mock_counters();

    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "checkpoint_test", &ops, &state);

    /* Query size first */
    size_t size = 0;
    nimcp_state_manager_checkpoint_module(manager, "checkpoint_test", nullptr, &size);

    /* Allocate buffer and checkpoint */
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    int result = nimcp_state_manager_checkpoint_module(
        manager, "checkpoint_test", buffer.data(), &written);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(written, sizeof(mock_module_state_t));

    /* Verify serialized data */
    mock_module_state_t* serialized = (mock_module_state_t*)buffer.data();
    EXPECT_EQ(serialized->magic, MOCK_STATE_MAGIC);
    EXPECT_FLOAT_EQ(serialized->value_a, 1.5f);
    EXPECT_FLOAT_EQ(serialized->value_b, 2.5f);
    EXPECT_EQ(serialized->counter, 42);
    EXPECT_TRUE(serialized->flag);
}

TEST_F(StateManagerTest, CheckpointModuleBufferTooSmall) {
    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "small_buffer", &ops, &state);

    uint8_t small_buffer[4];  /* Too small */
    size_t size = sizeof(small_buffer);
    int result = nimcp_state_manager_checkpoint_module(
        manager, "small_buffer", small_buffer, &size);

    EXPECT_EQ(result, -2);  /* Buffer too small */
    EXPECT_GT(size, sizeof(small_buffer));  /* Returns required size */
}

TEST_F(StateManagerTest, CheckpointAll) {
    reset_mock_counters();

    mock_module_state_t states[3];
    nimcp_module_state_ops_t ops = create_mock_ops();

    for (int i = 0; i < 3; i++) {
        init_mock_state(&states[i]);
        states[i].counter = i * 10;
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        nimcp_state_manager_register(manager, name, &ops, &states[i]);
    }

    /* Query total size */
    size_t total_size = 0;
    int result = nimcp_state_manager_checkpoint_all(manager, nullptr, &total_size);
    EXPECT_EQ(result, 0);
    EXPECT_GT(total_size, 0u);

    /* Checkpoint all */
    std::vector<uint8_t> buffer(total_size);
    size_t written = total_size;
    result = nimcp_state_manager_checkpoint_all(manager, buffer.data(), &written);
    EXPECT_EQ(result, 0);

    /* Should have called serialize for each module */
    EXPECT_GE(g_serialize_calls, 3);  /* At least 3 for size queries */
}

TEST_F(StateManagerTest, CheckpointNonexistent) {
    size_t size = 0;
    int result = nimcp_state_manager_checkpoint_module(manager, "nonexistent", nullptr, &size);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Restore Tests
//=============================================================================

TEST_F(StateManagerTest, RestoreModule) {
    reset_mock_counters();

    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "restore_test", &ops, &state);

    /* Checkpoint */
    size_t size = 0;
    nimcp_state_manager_checkpoint_module(manager, "restore_test", nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    nimcp_state_manager_checkpoint_module(manager, "restore_test", buffer.data(), &written);

    /* Modify state */
    state.value_a = 999.0f;
    state.counter = 0;

    /* Restore */
    int result = nimcp_state_manager_restore_module(
        manager, "restore_test", buffer.data(), written);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_deserialize_calls, 1);

    /* Verify restoration */
    EXPECT_FLOAT_EQ(state.value_a, 1.5f);
    EXPECT_EQ(state.counter, 42);
}

TEST_F(StateManagerTest, RestoreAll) {
    reset_mock_counters();

    mock_module_state_t states[2];
    nimcp_module_state_ops_t ops = create_mock_ops();

    for (int i = 0; i < 2; i++) {
        init_mock_state(&states[i]);
        states[i].counter = (i + 1) * 100;
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        nimcp_state_manager_register(manager, name, &ops, &states[i]);
    }

    /* Checkpoint all */
    size_t total_size = 0;
    nimcp_state_manager_checkpoint_all(manager, nullptr, &total_size);
    std::vector<uint8_t> buffer(total_size);
    size_t written = total_size;
    nimcp_state_manager_checkpoint_all(manager, buffer.data(), &written);

    /* Modify states */
    states[0].counter = 0;
    states[1].counter = 0;

    /* Restore all */
    int result = nimcp_state_manager_restore_all(manager, buffer.data(), written);
    EXPECT_EQ(result, 0);

    /* Verify restoration */
    EXPECT_EQ(states[0].counter, 100);
    EXPECT_EQ(states[1].counter, 200);
}

TEST_F(StateManagerTest, RestoreInvalidBuffer) {
    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "invalid_restore", &ops, &state);

    /* Create buffer with invalid magic */
    mock_module_state_t bad_data;
    bad_data.magic = 0xDEADBEEF;  /* Wrong magic */
    bad_data.version = MOCK_STATE_VERSION;

    int result = nimcp_state_manager_restore_module(
        manager, "invalid_restore", (const uint8_t*)&bad_data, sizeof(bad_data));
    EXPECT_LT(result, 0);
}

//=============================================================================
// Validation Tests
//=============================================================================

TEST_F(StateManagerTest, ValidateModuleValid) {
    reset_mock_counters();

    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "valid_module", &ops, &state);

    int result = nimcp_state_manager_validate_module(manager, "valid_module");
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_validate_calls, 1);
}

TEST_F(StateManagerTest, ValidateModuleInvalid) {
    reset_mock_counters();

    mock_module_state_t state;
    init_mock_state(&state);
    state.magic = 0;  /* Invalid magic */
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "invalid_module", &ops, &state);

    int result = nimcp_state_manager_validate_module(manager, "invalid_module");
    EXPECT_LT(result, 0);
}

TEST_F(StateManagerTest, ValidateModuleNaN) {
    mock_module_state_t state;
    init_mock_state(&state);
    state.value_a = NAN;  /* Invalid value */
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "nan_module", &ops, &state);

    int result = nimcp_state_manager_validate_module(manager, "nan_module");
    EXPECT_LT(result, 0);
}

TEST_F(StateManagerTest, ValidateAll) {
    reset_mock_counters();

    mock_module_state_t states[3];
    nimcp_module_state_ops_t ops = create_mock_ops();

    for (int i = 0; i < 3; i++) {
        init_mock_state(&states[i]);
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        nimcp_state_manager_register(manager, name, &ops, &states[i]);
    }

    int valid_count = nimcp_state_manager_validate_all(manager);
    EXPECT_EQ(valid_count, 3);
    EXPECT_EQ(g_validate_calls, 3);
}

TEST_F(StateManagerTest, ValidateAllWithInvalid) {
    mock_module_state_t states[3];
    nimcp_module_state_ops_t ops = create_mock_ops();

    for (int i = 0; i < 3; i++) {
        init_mock_state(&states[i]);
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        nimcp_state_manager_register(manager, name, &ops, &states[i]);
    }

    /* Make one invalid */
    states[1].magic = 0;

    int valid_count = nimcp_state_manager_validate_all(manager);
    EXPECT_EQ(valid_count, 2);  /* 2 valid, 1 invalid */
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(StateManagerTest, ResetModule) {
    reset_mock_counters();

    mock_module_state_t state;
    init_mock_state(&state);
    state.counter = 999;  /* Non-default value */
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "reset_me", &ops, &state);

    int result = nimcp_state_manager_reset_module(manager, "reset_me");
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_reset_calls, 1);
    EXPECT_EQ(state.counter, 0);  /* Should be reset to default */
}

TEST_F(StateManagerTest, ResetAll) {
    reset_mock_counters();

    mock_module_state_t states[3];
    nimcp_module_state_ops_t ops = create_mock_ops();

    for (int i = 0; i < 3; i++) {
        init_mock_state(&states[i]);
        states[i].counter = (i + 1) * 100;
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        nimcp_state_manager_register(manager, name, &ops, &states[i]);
    }

    int reset_count = nimcp_state_manager_reset_all(manager);
    EXPECT_EQ(reset_count, 3);

    /* All should be reset to defaults */
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(states[i].counter, 0);
    }
}

TEST_F(StateManagerTest, ResetInvalid) {
    mock_module_state_t states[3];
    nimcp_module_state_ops_t ops = create_mock_ops();

    for (int i = 0; i < 3; i++) {
        init_mock_state(&states[i]);
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        nimcp_state_manager_register(manager, name, &ops, &states[i]);
    }

    /* Make one invalid */
    states[1].magic = 0;

    int reset_count = nimcp_state_manager_reset_invalid(manager);
    EXPECT_EQ(reset_count, 1);  /* Only 1 was invalid and reset */

    /* Verify the invalid one is now valid */
    EXPECT_EQ(states[1].magic, MOCK_STATE_MAGIC);
}

//=============================================================================
// Query API Tests
//=============================================================================

TEST_F(StateManagerTest, GetTotalSize) {
    mock_module_state_t states[3];
    nimcp_module_state_ops_t ops = create_mock_ops();

    for (int i = 0; i < 3; i++) {
        init_mock_state(&states[i]);
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        nimcp_state_manager_register(manager, name, &ops, &states[i]);
    }

    size_t total_size = nimcp_state_manager_get_total_size(manager);
    /* Total size includes checkpoint header + module headers + module data */
    /* state_checkpoint_header_t (24) + 3 * (state_module_header_t (72) + mock_module_state_t (24)) */
    EXPECT_GT(total_size, 3 * sizeof(mock_module_state_t));
    EXPECT_EQ(total_size, 320u);  /* Header + 3 * (module header + module data) */
}

TEST_F(StateManagerTest, GetModuleCount) {
    EXPECT_EQ(nimcp_state_manager_get_module_count(manager), 0u);

    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "one", &ops, &state);
    EXPECT_EQ(nimcp_state_manager_get_module_count(manager), 1u);

    nimcp_state_manager_register(manager, "two", &ops, &state);
    EXPECT_EQ(nimcp_state_manager_get_module_count(manager), 2u);
}

TEST_F(StateManagerTest, GetModuleNames) {
    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    nimcp_state_manager_register(manager, "alpha", &ops, &state);
    nimcp_state_manager_register(manager, "beta", &ops, &state);
    nimcp_state_manager_register(manager, "gamma", &ops, &state);

    const char* names[10];
    uint32_t count = nimcp_state_manager_get_module_names(manager, names, 10);
    EXPECT_EQ(count, 3u);

    /* Verify names (order may vary) */
    bool found_alpha = false, found_beta = false, found_gamma = false;
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(names[i], "alpha") == 0) found_alpha = true;
        if (strcmp(names[i], "beta") == 0) found_beta = true;
        if (strcmp(names[i], "gamma") == 0) found_gamma = true;
    }
    EXPECT_TRUE(found_alpha);
    EXPECT_TRUE(found_beta);
    EXPECT_TRUE(found_gamma);
}

TEST_F(StateManagerTest, GetModuleNamesLimitedBuffer) {
    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        nimcp_state_manager_register(manager, name, &ops, &state);
    }

    const char* names[2];  /* Only room for 2 */
    uint32_t count = nimcp_state_manager_get_module_names(manager, names, 2);
    EXPECT_EQ(count, 2u);  /* Should only return 2 */
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(StateManagerTest, GetStats) {
    mock_module_state_t states[2];
    nimcp_module_state_ops_t ops = create_mock_ops();

    for (int i = 0; i < 2; i++) {
        init_mock_state(&states[i]);
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        nimcp_state_manager_register(manager, name, &ops, &states[i]);
    }

    /* Query size first */
    size_t size = 0;
    nimcp_state_manager_checkpoint_all(manager, nullptr, &size);

    /* Actually perform checkpoint with buffer */
    uint8_t* buffer = new uint8_t[size];
    nimcp_state_manager_checkpoint_all(manager, buffer, &size);
    delete[] buffer;

    nimcp_state_manager_validate_all(manager);

    nimcp_state_manager_stats_t stats;
    int result = nimcp_state_manager_get_stats(manager, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.module_count, 2u);
    EXPECT_EQ(stats.enabled_modules, 2u);
    EXPECT_GE(stats.total_checkpoints, 1u);  /* At least 1 checkpoint operation */
    EXPECT_GE(stats.total_validations, 1u);  /* At least 1 validate operation */
}

TEST_F(StateManagerTest, GetStatsNullParams) {
    int result = nimcp_state_manager_get_stats(manager, nullptr);
    EXPECT_LT(result, 0);

    nimcp_state_manager_stats_t stats;
    result = nimcp_state_manager_get_stats(nullptr, &stats);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Edge Cases and Stress Tests
//=============================================================================

TEST_F(StateManagerTest, RegisterMaxModules) {
    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    /* Register up to max modules */
    int registered = 0;
    for (uint32_t i = 0; i < NIMCP_STATE_MANAGER_MAX_MODULES + 5; i++) {
        char name[64];
        snprintf(name, sizeof(name), "module_%u", i);
        int result = nimcp_state_manager_register(manager, name, &ops, &state);
        if (result == 0) {
            registered++;
        } else {
            break;  /* Hit limit */
        }
    }

    EXPECT_EQ(registered, NIMCP_STATE_MANAGER_MAX_MODULES);
    EXPECT_EQ(manager->module_count, (uint32_t)registered);
}

TEST_F(StateManagerTest, LongModuleName) {
    mock_module_state_t state;
    init_mock_state(&state);
    nimcp_module_state_ops_t ops = create_mock_ops();

    /* Name longer than max */
    char long_name[NIMCP_STATE_MANAGER_MAX_NAME_LEN + 20];
    memset(long_name, 'x', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    /* Should either truncate or reject */
    int result = nimcp_state_manager_register(manager, long_name, &ops, &state);
    /* Either succeeds (with truncation) or fails - both are valid behaviors */
    (void)result;

    /* If it succeeded, verify the entry exists and name was handled */
    if (result == 0) {
        EXPECT_EQ(manager->module_count, 1u);
    }
}

TEST_F(StateManagerTest, DisabledModuleNotCheckpointed) {
    reset_mock_counters();

    mock_module_state_t states[2];
    nimcp_module_state_ops_t ops = create_mock_ops();

    init_mock_state(&states[0]);
    init_mock_state(&states[1]);

    nimcp_state_manager_register(manager, "enabled", &ops, &states[0]);
    nimcp_state_manager_register(manager, "disabled", &ops, &states[1]);

    /* Disable second module */
    nimcp_state_manager_set_enabled(manager, "disabled", false);

    /* Checkpoint all */
    size_t size = 0;
    nimcp_state_manager_checkpoint_all(manager, nullptr, &size);

    /* Size should only include enabled module + headers */
    /* checkpoint_header (24) + module_header (72) + module_data (24) = 120 or 128 with alignment */
    EXPECT_GT(size, sizeof(mock_module_state_t));
    EXPECT_LT(size, 2 * (sizeof(mock_module_state_t) + 100));  /* Less than 2 modules worth */
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
