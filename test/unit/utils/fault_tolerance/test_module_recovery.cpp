/**
 * @file test_module_recovery.cpp
 * @brief Comprehensive unit tests for module recovery system
 *
 * WHAT: Unit tests for nimcp_module_recovery_manager_t
 * WHY:  Verify graduated recovery, health checks, and isolation work correctly
 * HOW:  Test manager lifecycle, registration, recovery levels, and escalation
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
#include "utils/fault_tolerance/nimcp_module_recovery.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Module recovery test fixture
 *
 * WHAT: Provides fresh recovery manager for each test
 * WHY:  Ensure test isolation
 * HOW:  Create manager in SetUp, destroy in TearDown
 */
class ModuleRecoveryTest : public ::testing::Test {
protected:
    nimcp_module_recovery_manager_t* manager;

    void SetUp() override {
        manager = nimcp_module_recovery_manager_create();
    }

    void TearDown() override {
        if (manager) {
            nimcp_module_recovery_manager_destroy(manager);
            manager = nullptr;
        }
    }
};

//=============================================================================
// Mock Module State for Testing
//=============================================================================

/**
 * @brief Mock recoverable module state
 */
typedef struct mock_recoverable_state {
    float value;
    float trace;
    int error_count;
    bool is_isolated;
    bool is_healthy;
    float health_score;
} mock_recoverable_state_t;

/* Global tracking for test verification */
static int g_recovery_calls = 0;
static int g_health_check_calls = 0;
static nimcp_module_recovery_level_t g_last_recovery_level = NIMCP_MODULE_RECOVERY_NONE;
static bool g_force_recovery_fail = false;
static bool g_force_escalate = false;

/**
 * @brief Reset mock counters
 */
static void reset_mock_recovery_counters(void) {
    g_recovery_calls = 0;
    g_health_check_calls = 0;
    g_last_recovery_level = NIMCP_MODULE_RECOVERY_NONE;
    g_force_recovery_fail = false;
    g_force_escalate = false;
}

/**
 * @brief Mock recovery function
 *
 * Simulates graduated recovery based on level.
 */
static nimcp_module_recovery_result_t mock_recovery(
    void* module_state,
    nimcp_module_recovery_level_t level,
    void* user_data
) {
    (void)user_data;
    g_recovery_calls++;
    g_last_recovery_level = level;

    if (!module_state) {
        return NIMCP_MODULE_RECOVERY_FAILED;
    }

    /* Force failure for testing */
    if (g_force_recovery_fail) {
        return NIMCP_MODULE_RECOVERY_FAILED;
    }

    /* Force escalation for testing */
    if (g_force_escalate) {
        return NIMCP_MODULE_RECOVERY_ESCALATE;
    }

    mock_recoverable_state_t* state = (mock_recoverable_state_t*)module_state;

    switch (level) {
        case NIMCP_MODULE_RECOVERY_NONE:
            return NIMCP_MODULE_RECOVERY_SUCCESS;

        case NIMCP_MODULE_RECOVERY_LIGHT:
            /* Light reset: clear trace only */
            state->trace = 0.0f;
            state->is_healthy = true;
            state->health_score = 0.9f;
            return NIMCP_MODULE_RECOVERY_SUCCESS;

        case NIMCP_MODULE_RECOVERY_PARTIAL:
            /* Partial reset: clear trace and error count */
            state->trace = 0.0f;
            state->error_count = 0;
            state->is_healthy = true;
            state->health_score = 0.95f;
            return NIMCP_MODULE_RECOVERY_SUCCESS;

        case NIMCP_MODULE_RECOVERY_FULL:
            /* Full reset: everything to defaults */
            state->value = 0.5f;
            state->trace = 0.0f;
            state->error_count = 0;
            state->is_isolated = false;
            state->is_healthy = true;
            state->health_score = 1.0f;
            return NIMCP_MODULE_RECOVERY_SUCCESS;

        case NIMCP_MODULE_RECOVERY_ISOLATE:
            /* Isolate: mark as isolated */
            state->is_isolated = true;
            state->is_healthy = false;
            return NIMCP_MODULE_RECOVERY_SUCCESS;

        default:
            return NIMCP_MODULE_RECOVERY_FAILED;
    }
}

/**
 * @brief Mock health check function
 */
static int mock_health_check(void* module_state, float* out_health) {
    g_health_check_calls++;

    if (!module_state || !out_health) {
        return -1;
    }

    mock_recoverable_state_t* state = (mock_recoverable_state_t*)module_state;

    /* Return pre-configured health score */
    *out_health = state->health_score;
    return 0;
}

/**
 * @brief Create mock recovery ops
 */
static nimcp_module_recovery_ops_t create_mock_recovery_ops(void) {
    nimcp_module_recovery_ops_t ops;
    ops.recover = mock_recovery;
    ops.health_check = mock_health_check;
    ops.user_data = nullptr;
    return ops;
}

/**
 * @brief Initialize mock recoverable state
 */
static void init_mock_recoverable_state(mock_recoverable_state_t* state) {
    state->value = 1.0f;
    state->trace = 0.1f;
    state->error_count = 0;
    state->is_isolated = false;
    state->is_healthy = true;
    state->health_score = 1.0f;
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ModuleRecoveryTest, CreateDestroy) {
    ASSERT_NE(manager, nullptr);
    EXPECT_TRUE(manager->initialized);
    EXPECT_EQ(manager->magic, NIMCP_MODULE_RECOVERY_MAGIC);
    EXPECT_EQ(manager->module_count, 0u);
}

TEST(ModuleRecoveryLifecycleTest, CreateMultiple) {
    nimcp_module_recovery_manager_t* m1 = nimcp_module_recovery_manager_create();
    nimcp_module_recovery_manager_t* m2 = nimcp_module_recovery_manager_create();

    ASSERT_NE(m1, nullptr);
    ASSERT_NE(m2, nullptr);
    EXPECT_NE(m1, m2);

    nimcp_module_recovery_manager_destroy(m1);
    nimcp_module_recovery_manager_destroy(m2);
}

TEST(ModuleRecoveryLifecycleTest, DestroyNull) {
    /* Should not crash */
    nimcp_module_recovery_manager_destroy(nullptr);
}

TEST_F(ModuleRecoveryTest, DefaultConfiguration) {
    /* Verify default configuration values */
    EXPECT_GT(manager->health_threshold, 0.0f);
    EXPECT_LE(manager->health_threshold, 1.0f);
    EXPECT_TRUE(manager->auto_escalate);
    EXPECT_GT(manager->max_escalation_level, 0u);
}

//=============================================================================
// Registration Tests
//=============================================================================

TEST_F(ModuleRecoveryTest, RegisterModule) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    int result = nimcp_module_recovery_register(manager, "test_module", &ops, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(manager->module_count, 1u);
}

TEST_F(ModuleRecoveryTest, RegisterMultipleModules) {
    mock_recoverable_state_t states[3];
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    for (int i = 0; i < 3; i++) {
        init_mock_recoverable_state(&states[i]);
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        int result = nimcp_module_recovery_register(manager, name, &ops, &states[i]);
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(manager->module_count, 3u);
}

TEST_F(ModuleRecoveryTest, RegisterDuplicateName) {
    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    int result1 = nimcp_module_recovery_register(manager, "duplicate", &ops, &state);
    EXPECT_EQ(result1, 0);

    int result2 = nimcp_module_recovery_register(manager, "duplicate", &ops, &state);
    EXPECT_LT(result2, 0);  /* Should fail */
    EXPECT_EQ(manager->module_count, 1u);
}

TEST_F(ModuleRecoveryTest, RegisterNullParams) {
    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    /* Null manager */
    EXPECT_LT(nimcp_module_recovery_register(nullptr, "test", &ops, &state), 0);

    /* Null name */
    EXPECT_LT(nimcp_module_recovery_register(manager, nullptr, &ops, &state), 0);

    /* Null ops */
    EXPECT_LT(nimcp_module_recovery_register(manager, "test", nullptr, &state), 0);
}

TEST_F(ModuleRecoveryTest, UnregisterModule) {
    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "to_remove", &ops, &state);
    EXPECT_EQ(manager->module_count, 1u);

    int result = nimcp_module_recovery_unregister(manager, "to_remove");
    EXPECT_EQ(result, 0);
    EXPECT_EQ(manager->module_count, 0u);
}

TEST_F(ModuleRecoveryTest, UnregisterNonexistent) {
    int result = nimcp_module_recovery_unregister(manager, "nonexistent");
    EXPECT_LT(result, 0);
}

TEST_F(ModuleRecoveryTest, SetEnabled) {
    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "toggle_me", &ops, &state);

    /* Disable */
    int result = nimcp_module_recovery_set_enabled(manager, "toggle_me", false);
    EXPECT_EQ(result, 0);

    /* Find and verify */
    for (uint32_t i = 0; i < manager->module_count; i++) {
        if (strcmp(manager->modules[i].name, "toggle_me") == 0) {
            EXPECT_FALSE(manager->modules[i].enabled);
            break;
        }
    }

    /* Re-enable */
    result = nimcp_module_recovery_set_enabled(manager, "toggle_me", true);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Recovery Level Tests
//=============================================================================

TEST_F(ModuleRecoveryTest, RecoveryLightLevel) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    state.trace = 0.5f;  /* Non-zero trace to verify reset */
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "light_test", &ops, &state);

    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        manager, "light_test", NIMCP_MODULE_RECOVERY_LIGHT);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);
    EXPECT_EQ(g_recovery_calls, 1);
    EXPECT_EQ(g_last_recovery_level, NIMCP_MODULE_RECOVERY_LIGHT);
    EXPECT_FLOAT_EQ(state.trace, 0.0f);  /* Trace should be reset */
    EXPECT_FLOAT_EQ(state.value, 1.0f);  /* Value should NOT be reset */
}

TEST_F(ModuleRecoveryTest, RecoveryPartialLevel) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    state.trace = 0.5f;
    state.error_count = 10;
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "partial_test", &ops, &state);

    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        manager, "partial_test", NIMCP_MODULE_RECOVERY_PARTIAL);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);
    EXPECT_EQ(g_last_recovery_level, NIMCP_MODULE_RECOVERY_PARTIAL);
    EXPECT_FLOAT_EQ(state.trace, 0.0f);
    EXPECT_EQ(state.error_count, 0);
}

TEST_F(ModuleRecoveryTest, RecoveryFullLevel) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    state.value = 0.9f;
    state.trace = 0.5f;
    state.error_count = 10;
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "full_test", &ops, &state);

    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        manager, "full_test", NIMCP_MODULE_RECOVERY_FULL);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);
    EXPECT_EQ(g_last_recovery_level, NIMCP_MODULE_RECOVERY_FULL);
    EXPECT_FLOAT_EQ(state.value, 0.5f);  /* Reset to default */
    EXPECT_FLOAT_EQ(state.trace, 0.0f);
    EXPECT_EQ(state.error_count, 0);
}

TEST_F(ModuleRecoveryTest, RecoveryIsolateLevel) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    state.is_isolated = false;
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "isolate_test", &ops, &state);

    /* Max escalation level must include ISOLATE for it to be attempted */
    manager->max_escalation_level = NIMCP_MODULE_RECOVERY_ISOLATE;

    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        manager, "isolate_test", NIMCP_MODULE_RECOVERY_ISOLATE);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);
    EXPECT_EQ(g_last_recovery_level, NIMCP_MODULE_RECOVERY_ISOLATE);
    EXPECT_TRUE(state.is_isolated);
}

TEST_F(ModuleRecoveryTest, RecoveryNonexistentModule) {
    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        manager, "nonexistent", NIMCP_MODULE_RECOVERY_LIGHT);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_FAILED);
}

//=============================================================================
// Auto-Escalation Tests
//=============================================================================

TEST_F(ModuleRecoveryTest, AutoEscalationOnFailure) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "escalate_test", &ops, &state);

    /* Enable auto-escalation and force escalate result */
    manager->auto_escalate = true;
    g_force_escalate = true;

    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        manager, "escalate_test", NIMCP_MODULE_RECOVERY_LIGHT);

    /* Should have escalated through multiple levels */
    EXPECT_GT(g_recovery_calls, 1);
    /* Final result depends on implementation - either success at higher level
       or escalate if all levels tried */
}

TEST_F(ModuleRecoveryTest, NoAutoEscalation) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "no_escalate_test", &ops, &state);

    /* Disable auto-escalation - this only affects FAILED results, not ESCALATE results */
    manager->auto_escalate = false;

    /* Force FAILED result (not ESCALATE) to test auto_escalate flag */
    g_force_recovery_fail = true;

    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        manager, "no_escalate_test", NIMCP_MODULE_RECOVERY_LIGHT);

    /* Should have tried only once since auto_escalate is false */
    EXPECT_EQ(g_recovery_calls, 1);
    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_FAILED);
}

TEST_F(ModuleRecoveryTest, MaxEscalationLevel) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "max_escalate_test", &ops, &state);

    /* Set max escalation to PARTIAL (level 2) */
    manager->auto_escalate = true;
    manager->max_escalation_level = NIMCP_MODULE_RECOVERY_PARTIAL;
    g_force_escalate = true;

    nimcp_module_recovery_attempt(manager, "max_escalate_test", NIMCP_MODULE_RECOVERY_LIGHT);

    /* Should not have escalated beyond PARTIAL */
    EXPECT_LE(g_last_recovery_level, NIMCP_MODULE_RECOVERY_PARTIAL);
}

//=============================================================================
// Health Check Tests
//=============================================================================

TEST_F(ModuleRecoveryTest, CheckHealthSingleModule) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    state.health_score = 0.85f;
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "health_test", &ops, &state);

    float health;
    int result = nimcp_module_recovery_check_health(manager, "health_test", &health);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(health, 0.85f);
    EXPECT_EQ(g_health_check_calls, 1);
}

TEST_F(ModuleRecoveryTest, CheckHealthAllModules) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t states[3];
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    float expected_avg = 0.0f;
    for (int i = 0; i < 3; i++) {
        init_mock_recoverable_state(&states[i]);
        states[i].health_score = 0.7f + (i * 0.1f);  /* 0.7, 0.8, 0.9 */
        expected_avg += states[i].health_score;

        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        nimcp_module_recovery_register(manager, name, &ops, &states[i]);
    }
    expected_avg /= 3.0f;

    float avg_health = nimcp_module_recovery_check_all_health(manager);

    EXPECT_NEAR(avg_health, expected_avg, 0.01f);
    EXPECT_EQ(g_health_check_calls, 3);
}

TEST_F(ModuleRecoveryTest, CheckHealthNonexistent) {
    float health;
    int result = nimcp_module_recovery_check_health(manager, "nonexistent", &health);
    EXPECT_LT(result, 0);
}

TEST_F(ModuleRecoveryTest, CheckHealthNullOutput) {
    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "null_test", &ops, &state);

    int result = nimcp_module_recovery_check_health(manager, "null_test", nullptr);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Recovery All Unhealthy Tests
//=============================================================================

TEST_F(ModuleRecoveryTest, AttemptAllUnhealthy) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t states[4];
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    /* Create 4 modules: 2 healthy, 2 unhealthy */
    for (int i = 0; i < 4; i++) {
        init_mock_recoverable_state(&states[i]);
        states[i].health_score = (i < 2) ? 0.95f : 0.3f;  /* First 2 healthy, last 2 not */

        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        nimcp_module_recovery_register(manager, name, &ops, &states[i]);
    }

    /* Set threshold */
    manager->health_threshold = 0.5f;

    int recovered = nimcp_module_recovery_attempt_all_unhealthy(manager);

    /* Should have recovered the 2 unhealthy modules */
    EXPECT_EQ(recovered, 2);
}

TEST_F(ModuleRecoveryTest, AttemptAllUnhealthyNoneNeeded) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t states[3];
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    /* All healthy */
    for (int i = 0; i < 3; i++) {
        init_mock_recoverable_state(&states[i]);
        states[i].health_score = 0.9f;

        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        nimcp_module_recovery_register(manager, name, &ops, &states[i]);
    }

    manager->health_threshold = 0.5f;

    int recovered = nimcp_module_recovery_attempt_all_unhealthy(manager);

    EXPECT_EQ(recovered, 0);
}

//=============================================================================
// Isolation Tests
//=============================================================================

TEST_F(ModuleRecoveryTest, IsolateModule) {
    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "isolate_me", &ops, &state);

    int result = nimcp_module_recovery_isolate(manager, "isolate_me");
    EXPECT_EQ(result, 0);

    bool is_isolated = nimcp_module_recovery_is_isolated(manager, "isolate_me");
    EXPECT_TRUE(is_isolated);
}

TEST_F(ModuleRecoveryTest, RestoreIsolatedModule) {
    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "restore_me", &ops, &state);

    /* Isolate then restore */
    nimcp_module_recovery_isolate(manager, "restore_me");
    EXPECT_TRUE(nimcp_module_recovery_is_isolated(manager, "restore_me"));

    int result = nimcp_module_recovery_restore(manager, "restore_me");
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(nimcp_module_recovery_is_isolated(manager, "restore_me"));
}

TEST_F(ModuleRecoveryTest, IsolateNonexistent) {
    int result = nimcp_module_recovery_isolate(manager, "nonexistent");
    EXPECT_LT(result, 0);
}

TEST_F(ModuleRecoveryTest, IsIsolatedNonexistent) {
    bool is_isolated = nimcp_module_recovery_is_isolated(manager, "nonexistent");
    /* Fail-safe behavior: treat unknown modules as isolated */
    EXPECT_TRUE(is_isolated);
}

TEST_F(ModuleRecoveryTest, IsolatedModuleNotRecovered) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    state.health_score = 0.1f;  /* Very unhealthy */
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "isolated_skip", &ops, &state);
    nimcp_module_recovery_isolate(manager, "isolated_skip");

    manager->health_threshold = 0.5f;

    /* Attempt all unhealthy - should skip isolated module */
    int recovered = nimcp_module_recovery_attempt_all_unhealthy(manager);

    EXPECT_EQ(recovered, 0);  /* Isolated module should be skipped */
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ModuleRecoveryTest, GetStats) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t states[3];
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    for (int i = 0; i < 3; i++) {
        init_mock_recoverable_state(&states[i]);
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        nimcp_module_recovery_register(manager, name, &ops, &states[i]);
    }

    /* Perform some operations */
    nimcp_module_recovery_attempt(manager, "module_0", NIMCP_MODULE_RECOVERY_LIGHT);
    nimcp_module_recovery_isolate(manager, "module_2");
    nimcp_module_recovery_check_all_health(manager);

    nimcp_module_recovery_stats_t stats;
    int result = nimcp_module_recovery_get_stats(manager, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.module_count, 3u);
    EXPECT_EQ(stats.isolated_modules, 1u);
    EXPECT_GE(stats.total_recoveries, 1u);
}

TEST_F(ModuleRecoveryTest, GetStatsNullParams) {
    int result = nimcp_module_recovery_get_stats(manager, nullptr);
    EXPECT_LT(result, 0);

    nimcp_module_recovery_stats_t stats;
    result = nimcp_module_recovery_get_stats(nullptr, &stats);
    EXPECT_LT(result, 0);
}

TEST_F(ModuleRecoveryTest, StatsTrackSuccessesAndFailures) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "stats_test", &ops, &state);

    /* Perform successful recovery */
    nimcp_module_recovery_attempt(manager, "stats_test", NIMCP_MODULE_RECOVERY_LIGHT);

    /* Perform failed recovery - disable auto-escalation so it fails once */
    manager->auto_escalate = false;
    g_force_recovery_fail = true;
    nimcp_module_recovery_attempt(manager, "stats_test", NIMCP_MODULE_RECOVERY_LIGHT);

    nimcp_module_recovery_stats_t stats;
    nimcp_module_recovery_get_stats(manager, &stats);

    /* total_recoveries only counts successful operations */
    EXPECT_GE(stats.total_recoveries, 1u);
    EXPECT_GE(stats.total_successes, 1u);
    EXPECT_GE(stats.total_failures, 1u);
}

//=============================================================================
// Edge Cases and Stress Tests
//=============================================================================

TEST_F(ModuleRecoveryTest, RegisterMaxModules) {
    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    int registered = 0;
    for (uint32_t i = 0; i < NIMCP_MODULE_RECOVERY_MAX_MODULES + 5; i++) {
        char name[64];
        snprintf(name, sizeof(name), "module_%u", i);
        int result = nimcp_module_recovery_register(manager, name, &ops, &state);
        if (result == 0) {
            registered++;
        } else {
            break;
        }
    }

    EXPECT_EQ(registered, NIMCP_MODULE_RECOVERY_MAX_MODULES);
}

TEST_F(ModuleRecoveryTest, RecoveryWithNullState) {
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    /* Register with null state */
    nimcp_module_recovery_register(manager, "null_state", &ops, nullptr);

    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        manager, "null_state", NIMCP_MODULE_RECOVERY_LIGHT);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_FAILED);
}

TEST_F(ModuleRecoveryTest, DisabledModuleNotRecovered) {
    reset_mock_recovery_counters();

    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    state.health_score = 0.1f;
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "disabled_module", &ops, &state);
    nimcp_module_recovery_set_enabled(manager, "disabled_module", false);

    manager->health_threshold = 0.5f;

    int recovered = nimcp_module_recovery_attempt_all_unhealthy(manager);

    /* Disabled module should not be recovered automatically */
    EXPECT_EQ(recovered, 0);
}

TEST_F(ModuleRecoveryTest, MultipleRecoveriesTrackLastLevel) {
    mock_recoverable_state_t state;
    init_mock_recoverable_state(&state);
    nimcp_module_recovery_ops_t ops = create_mock_recovery_ops();

    nimcp_module_recovery_register(manager, "level_track", &ops, &state);

    /* Perform recoveries at different levels */
    nimcp_module_recovery_attempt(manager, "level_track", NIMCP_MODULE_RECOVERY_LIGHT);
    nimcp_module_recovery_attempt(manager, "level_track", NIMCP_MODULE_RECOVERY_FULL);
    nimcp_module_recovery_attempt(manager, "level_track", NIMCP_MODULE_RECOVERY_PARTIAL);

    /* Find module and verify last level is tracked */
    for (uint32_t i = 0; i < manager->module_count; i++) {
        if (strcmp(manager->modules[i].name, "level_track") == 0) {
            EXPECT_EQ(manager->modules[i].last_level, NIMCP_MODULE_RECOVERY_PARTIAL);
            break;
        }
    }
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
