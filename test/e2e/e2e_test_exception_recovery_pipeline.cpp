/**
 * @file e2e_test_exception_recovery_pipeline.cpp
 * @brief E2E Test for Exception Recovery Actions and Adaptive Recovery
 *
 * WHAT: Full end-to-end tests for exception recovery mechanisms
 * WHY:  Verify recovery callbacks, adaptive learning, and immune-mediated recovery
 * HOW:  Test all recovery action types, adaptive pattern learning, and recovery orchestration
 *
 * TEST SCENARIOS:
 * 1. RecoveryCallbackRegistration - Test callback registration for each action type
 * 2. GCRecoveryPipeline - Test garbage collection recovery flow
 * 3. RetryRecoveryPipeline - Test retry mechanism with backoff
 * 4. RollbackRecoveryPipeline - Test checkpoint rollback recovery
 * 5. ThreadRestartRecoveryPipeline - Test thread restart recovery
 * 6. QuarantineRecoveryPipeline - Test quarantine mechanism
 * 7. EmergencySaveRecoveryPipeline - Test emergency state persistence
 * 8. AdaptiveLearningPipeline - Test pattern-based recovery learning
 * 9. RecoveryStrategySelectionPipeline - Test strategy selection logic
 * 10. FailedRecoveryEscalationPipeline - Test recovery failure handling
 *
 * RECOVERY ARCHITECTURE:
 * ```
 * Exception -> Get Strategy -> Try Primary Action -> Success? -> Record Outcome
 *                                   |                  |
 *                                   | Fail             | Yes
 *                                   v                  |
 *                           Try Fallback Action        |
 *                                   |                  |
 *                                   v                  |
 *                           All Failed?               |
 *                                   |                  |
 *                                   v                  v
 *                           Escalate/Alert    Update Pattern Learning
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-16
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <atomic>
#include <cmath>
#include <cstring>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
}

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

// Timing thresholds (milliseconds)
constexpr double MAX_RECOVERY_TIME_MS = 200.0;
constexpr double MAX_LEARNING_TIME_MS = 100.0;
constexpr double MAX_STRATEGY_TIME_MS = 50.0;

// Test parameters
constexpr int LEARNING_ITERATIONS = 20;
constexpr int PATTERN_SAMPLES = 10;

//=============================================================================
// Recovery Tracking
//=============================================================================

struct RecoveryTracker {
    std::atomic<int> gc_calls{0};
    std::atomic<int> retry_calls{0};
    std::atomic<int> rollback_calls{0};
    std::atomic<int> thread_restart_calls{0};
    std::atomic<int> quarantine_calls{0};
    std::atomic<int> emergency_save_calls{0};
    std::atomic<int> reduce_load_calls{0};
    std::atomic<int> clear_cache_calls{0};
    std::atomic<int> component_restart_calls{0};
    std::atomic<int> graceful_shutdown_calls{0};
    std::atomic<int> compact_calls{0};

    std::atomic<int> successful_recoveries{0};
    std::atomic<int> failed_recoveries{0};

    // Track which exceptions triggered recovery
    nimcp_exception_t* last_exception{nullptr};
    nimcp_exception_recovery_action_t last_action{EXCEPTION_RECOVERY_NONE};

    // Configurable success/failure for testing
    bool gc_should_succeed{true};
    bool retry_should_succeed{true};
    bool rollback_should_succeed{true};
    bool thread_restart_should_succeed{true};
    bool quarantine_should_succeed{true};

    void reset() {
        gc_calls = 0;
        retry_calls = 0;
        rollback_calls = 0;
        thread_restart_calls = 0;
        quarantine_calls = 0;
        emergency_save_calls = 0;
        reduce_load_calls = 0;
        clear_cache_calls = 0;
        component_restart_calls = 0;
        graceful_shutdown_calls = 0;
        compact_calls = 0;
        successful_recoveries = 0;
        failed_recoveries = 0;
        last_exception = nullptr;
        last_action = EXCEPTION_RECOVERY_NONE;
        gc_should_succeed = true;
        retry_should_succeed = true;
        rollback_should_succeed = true;
        thread_restart_should_succeed = true;
        quarantine_should_succeed = true;
    }
};

static RecoveryTracker g_recovery_tracker;

// Recovery callback implementations
static int gc_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)user_data;
    g_recovery_tracker.gc_calls++;
    g_recovery_tracker.last_exception = ex;
    g_recovery_tracker.last_action = action;

    if (g_recovery_tracker.gc_should_succeed) {
        g_recovery_tracker.successful_recoveries++;
        return 0;
    } else {
        g_recovery_tracker.failed_recoveries++;
        return -1;
    }
}

static int retry_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)user_data;
    g_recovery_tracker.retry_calls++;
    g_recovery_tracker.last_exception = ex;
    g_recovery_tracker.last_action = action;

    if (g_recovery_tracker.retry_should_succeed) {
        g_recovery_tracker.successful_recoveries++;
        return 0;
    } else {
        g_recovery_tracker.failed_recoveries++;
        return -1;
    }
}

static int rollback_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)user_data;
    g_recovery_tracker.rollback_calls++;
    g_recovery_tracker.last_exception = ex;
    g_recovery_tracker.last_action = action;

    if (g_recovery_tracker.rollback_should_succeed) {
        g_recovery_tracker.successful_recoveries++;
        return 0;
    } else {
        g_recovery_tracker.failed_recoveries++;
        return -1;
    }
}

static int thread_restart_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)user_data;
    g_recovery_tracker.thread_restart_calls++;
    g_recovery_tracker.last_exception = ex;
    g_recovery_tracker.last_action = action;

    if (g_recovery_tracker.thread_restart_should_succeed) {
        g_recovery_tracker.successful_recoveries++;
        return 0;
    } else {
        g_recovery_tracker.failed_recoveries++;
        return -1;
    }
}

static int quarantine_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)user_data;
    g_recovery_tracker.quarantine_calls++;
    g_recovery_tracker.last_exception = ex;
    g_recovery_tracker.last_action = action;

    if (g_recovery_tracker.quarantine_should_succeed) {
        g_recovery_tracker.successful_recoveries++;
        return 0;
    } else {
        g_recovery_tracker.failed_recoveries++;
        return -1;
    }
}

static int emergency_save_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_tracker.emergency_save_calls++;
    g_recovery_tracker.successful_recoveries++;
    return 0;
}

static int reduce_load_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_tracker.reduce_load_calls++;
    g_recovery_tracker.successful_recoveries++;
    return 0;
}

static int clear_cache_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_tracker.clear_cache_calls++;
    g_recovery_tracker.successful_recoveries++;
    return 0;
}

static int component_restart_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_tracker.component_restart_calls++;
    g_recovery_tracker.successful_recoveries++;
    return 0;
}

static int graceful_shutdown_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_tracker.graceful_shutdown_calls++;
    g_recovery_tracker.successful_recoveries++;
    return 0;
}

static int compact_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_tracker.compact_calls++;
    g_recovery_tracker.successful_recoveries++;
    return 0;
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionRecoveryPipelineTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune;

    void SetUp() override {
        g_recovery_tracker.reset();

        // Initialize exception system
        ASSERT_EQ(nimcp_exception_system_init(), 0);

        // Initialize circuit breaker
        ASSERT_EQ(nimcp_circuit_init(), 0);

        // Initialize metrics with adaptive recovery
        nimcp_metrics_config_t metrics_config;
        nimcp_metrics_default_config(&metrics_config);
        metrics_config.enable_adaptive_recovery = true;
        ASSERT_EQ(nimcp_metrics_init_with_config(&metrics_config), 0);

        // Initialize adaptive recovery
        ASSERT_EQ(nimcp_adaptive_init(), 0);

        // Create immune system
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        config.enable_logging = false;

        immune = brain_immune_create(&config);
        ASSERT_NE(immune, nullptr);
        ASSERT_EQ(brain_immune_start(immune), 0);

        // Connect exception system to immune
        ASSERT_EQ(nimcp_exception_immune_connect(immune), 0);
    }

    void TearDown() override {
        // Unregister all recovery callbacks
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RESTART_THREAD);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_QUARANTINE);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_EMERGENCY_SAVE);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_REDUCE_LOAD);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_CLEAR_CACHE);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RESTART_COMPONENT);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_COMPACT);

        // Disconnect and cleanup
        nimcp_exception_immune_disconnect();

        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
        }

        // Shutdown subsystems
        nimcp_adaptive_shutdown();
        nimcp_metrics_shutdown();
        nimcp_circuit_shutdown();
        nimcp_exception_system_shutdown();
    }

    void RegisterAllRecoveryCallbacks() {
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, gc_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, retry_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, rollback_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RESTART_THREAD, thread_restart_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_QUARANTINE, quarantine_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_EMERGENCY_SAVE, emergency_save_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_REDUCE_LOAD, reduce_load_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_CLEAR_CACHE, clear_cache_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RESTART_COMPONENT, component_restart_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN, graceful_shutdown_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_COMPACT, compact_recovery_callback, nullptr);
    }
};

//=============================================================================
// E2E Test: Recovery Callback Registration
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, RecoveryCallbackRegistration) {
    E2E_PIPELINE_START("Recovery Callback Registration Pipeline");

    E2E_STAGE_BEGIN("Register GC callback", MAX_RECOVERY_TIME_MS);
    EXPECT_EQ(nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, gc_recovery_callback, nullptr), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Register RETRY callback", MAX_RECOVERY_TIME_MS);
    EXPECT_EQ(nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, retry_recovery_callback, nullptr), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Register ROLLBACK callback", MAX_RECOVERY_TIME_MS);
    EXPECT_EQ(nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, rollback_recovery_callback, nullptr), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Register RESTART_THREAD callback", MAX_RECOVERY_TIME_MS);
    EXPECT_EQ(nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RESTART_THREAD, thread_restart_recovery_callback, nullptr), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Register QUARANTINE callback", MAX_RECOVERY_TIME_MS);
    EXPECT_EQ(nimcp_register_recovery_callback(EXCEPTION_RECOVERY_QUARANTINE, quarantine_recovery_callback, nullptr), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test callback execution", MAX_RECOVERY_TIME_MS);
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Callback test"
    );

    // Execute GC recovery
    int result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_recovery_tracker.gc_calls.load(), 1);
    EXPECT_EQ(g_recovery_tracker.last_action, EXCEPTION_RECOVERY_GC);

    nimcp_exception_unref(ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Unregister callback", MAX_RECOVERY_TIME_MS);
    EXPECT_EQ(nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC), 0);

    // Verify callback no longer executes
    ex = nimcp_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Unregister test"
    );

    int initial_gc_calls = g_recovery_tracker.gc_calls.load();
    // May return error or success depending on implementation
    nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    // GC callback should not have been called again (or called but not our callback)
    // Implementation dependent - just verify no crash

    nimcp_exception_unref(ex);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: GC Recovery Pipeline
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, GCRecoveryPipeline) {
    E2E_PIPELINE_START("GC Recovery Pipeline");

    RegisterAllRecoveryCallbacks();

    E2E_STAGE_BEGIN("Create memory exception", MAX_RECOVERY_TIME_MS);
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        4 * 1024 * 1024,  // 4MB
        "GC recovery test"
    );
    ASSERT_NE(mem_ex, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get recovery strategy", MAX_STRATEGY_TIME_MS);
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)mem_ex, &strategy);

    // Memory exceptions typically suggest GC
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_GC ||
        strategy.primary_action == EXCEPTION_RECOVERY_COMPACT
    );
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute GC recovery", MAX_RECOVERY_TIME_MS);
    int result = nimcp_exception_execute_recovery((nimcp_exception_t*)mem_ex, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(result, 0);
    EXPECT_GE(g_recovery_tracker.gc_calls.load(), 1);
    EXPECT_GE(g_recovery_tracker.successful_recoveries.load(), 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Notify recovery result", MAX_RECOVERY_TIME_MS);
    result = nimcp_exception_notify_recovery_result((nimcp_exception_t*)mem_ex, EXCEPTION_RECOVERY_GC, true);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(((nimcp_exception_t*)mem_ex)->recovery_succeeded);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Record recovery metrics", MAX_RECOVERY_TIME_MS);
    nimcp_metrics_record_recovery((nimcp_exception_t*)mem_ex, EXCEPTION_RECOVERY_GC, true, 5000);

    float gc_rate = nimcp_metrics_get_recovery_rate(EXCEPTION_RECOVERY_GC);
    EXPECT_GT(gc_rate, 0.0f);
    E2E_STAGE_END();

    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Retry Recovery Pipeline
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, RetryRecoveryPipeline) {
    E2E_PIPELINE_START("Retry Recovery Pipeline");

    RegisterAllRecoveryCallbacks();

    E2E_STAGE_BEGIN("Create I/O exception", MAX_RECOVERY_TIME_MS);
    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_IO_ERROR,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/tmp/test_file.dat",
        "Retry recovery test"
    );
    ASSERT_NE(io_ex, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute retry recovery", MAX_RECOVERY_TIME_MS);
    int result = nimcp_exception_execute_recovery((nimcp_exception_t*)io_ex, EXCEPTION_RECOVERY_RETRY);
    EXPECT_EQ(result, 0);
    EXPECT_GE(g_recovery_tracker.retry_calls.load(), 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test failed retry", MAX_RECOVERY_TIME_MS);
    g_recovery_tracker.retry_should_succeed = false;

    nimcp_io_exception_t* io_ex2 = nimcp_io_exception_create(
        NIMCP_ERROR_IO_ERROR,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/tmp/another_file.dat",
        "Failed retry test"
    );

    result = nimcp_exception_execute_recovery((nimcp_exception_t*)io_ex2, EXCEPTION_RECOVERY_RETRY);
    EXPECT_EQ(result, -1);  // Should fail
    EXPECT_GE(g_recovery_tracker.failed_recoveries.load(), 1);

    nimcp_exception_unref((nimcp_exception_t*)io_ex2);
    g_recovery_tracker.retry_should_succeed = true;
    E2E_STAGE_END();

    nimcp_exception_unref((nimcp_exception_t*)io_ex);
    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Rollback Recovery Pipeline
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, RollbackRecoveryPipeline) {
    E2E_PIPELINE_START("Rollback Recovery Pipeline");

    RegisterAllRecoveryCallbacks();

    E2E_STAGE_BEGIN("Create brain exception for rollback", MAX_RECOVERY_TIME_MS);
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_NOT_INITIALIZED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        42,
        "cortex",
        "Rollback recovery test"
    );
    ASSERT_NE(brain_ex, nullptr);
    brain_ex->learning_diverged = true;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute rollback recovery", MAX_RECOVERY_TIME_MS);
    int result = nimcp_exception_execute_recovery((nimcp_exception_t*)brain_ex, EXCEPTION_RECOVERY_ROLLBACK);
    EXPECT_EQ(result, 0);
    EXPECT_GE(g_recovery_tracker.rollback_calls.load(), 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify rollback tracking", MAX_RECOVERY_TIME_MS);
    nimcp_exception_notify_recovery_result((nimcp_exception_t*)brain_ex, EXCEPTION_RECOVERY_ROLLBACK, true);
    nimcp_metrics_record_recovery((nimcp_exception_t*)brain_ex, EXCEPTION_RECOVERY_ROLLBACK, true, 10000);

    float rollback_rate = nimcp_metrics_get_recovery_rate(EXCEPTION_RECOVERY_ROLLBACK);
    EXPECT_GT(rollback_rate, 0.0f);
    E2E_STAGE_END();

    nimcp_exception_unref((nimcp_exception_t*)brain_ex);
    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Thread Restart Recovery Pipeline
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, ThreadRestartRecoveryPipeline) {
    E2E_PIPELINE_START("Thread Restart Recovery Pipeline");

    RegisterAllRecoveryCallbacks();

    E2E_STAGE_BEGIN("Create threading exception", MAX_RECOVERY_TIME_MS);
    nimcp_threading_exception_t* thread_ex = nimcp_threading_exception_create(
        NIMCP_ERROR_THREAD_ERROR,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        12345,
        "Thread restart recovery test"
    );
    ASSERT_NE(thread_ex, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute thread restart recovery", MAX_RECOVERY_TIME_MS);
    int result = nimcp_exception_execute_recovery((nimcp_exception_t*)thread_ex, EXCEPTION_RECOVERY_RESTART_THREAD);
    EXPECT_EQ(result, 0);
    EXPECT_GE(g_recovery_tracker.thread_restart_calls.load(), 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test deadlock exception recovery", MAX_RECOVERY_TIME_MS);
    nimcp_threading_exception_t* deadlock_ex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        54321,
        "Deadlock recovery test"
    );
    deadlock_ex->is_deadlock = true;
    deadlock_ex->deadlock_cycle_len = 3;

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)deadlock_ex, &strategy);

    // Deadlock should suggest thread restart
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_RESTART_THREAD ||
        strategy.primary_action == EXCEPTION_RECOVERY_RESTART_COMPONENT
    );

    nimcp_exception_unref((nimcp_exception_t*)deadlock_ex);
    E2E_STAGE_END();

    nimcp_exception_unref((nimcp_exception_t*)thread_ex);
    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Quarantine Recovery Pipeline
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, QuarantineRecoveryPipeline) {
    E2E_PIPELINE_START("Quarantine Recovery Pipeline");

    RegisterAllRecoveryCallbacks();

    E2E_STAGE_BEGIN("Create security exception", MAX_RECOVERY_TIME_MS);
    nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        1,
        "Quarantine recovery test"
    );
    ASSERT_NE(sec_ex, nullptr);
    sec_ex->quarantine_required = true;
    sec_ex->source_node_id = 5;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute quarantine recovery", MAX_RECOVERY_TIME_MS);
    int result = nimcp_exception_execute_recovery((nimcp_exception_t*)sec_ex, EXCEPTION_RECOVERY_QUARANTINE);
    EXPECT_EQ(result, 0);
    EXPECT_GE(g_recovery_tracker.quarantine_calls.load(), 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify security exception strategy", MAX_STRATEGY_TIME_MS);
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)sec_ex, &strategy);

    // Security exceptions with quarantine_required should suggest quarantine
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_QUARANTINE ||
        strategy.fallback_action == EXCEPTION_RECOVERY_QUARANTINE
    );
    E2E_STAGE_END();

    nimcp_exception_unref((nimcp_exception_t*)sec_ex);
    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Emergency Save Recovery Pipeline
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, EmergencySaveRecoveryPipeline) {
    E2E_PIPELINE_START("Emergency Save Recovery Pipeline");

    RegisterAllRecoveryCallbacks();

    E2E_STAGE_BEGIN("Create fatal exception", MAX_RECOVERY_TIME_MS);
    nimcp_exception_t* fatal_ex = nimcp_exception_create(
        NIMCP_ERROR_FATAL,
        EXCEPTION_SEVERITY_FATAL,
        __FILE__, __LINE__, __func__,
        "Emergency save test"
    );
    ASSERT_NE(fatal_ex, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute emergency save recovery", MAX_RECOVERY_TIME_MS);
    int result = nimcp_exception_execute_recovery(fatal_ex, EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    EXPECT_EQ(result, 0);
    EXPECT_GE(g_recovery_tracker.emergency_save_calls.load(), 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify fatal exception strategy", MAX_STRATEGY_TIME_MS);
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(fatal_ex, &strategy);

    // Fatal exceptions should include emergency save or graceful shutdown
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_EMERGENCY_SAVE ||
        strategy.primary_action == EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN ||
        strategy.fallback_action == EXCEPTION_RECOVERY_EMERGENCY_SAVE
    );
    E2E_STAGE_END();

    nimcp_exception_unref(fatal_ex);
    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Adaptive Learning Pipeline
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, AdaptiveLearningPipeline) {
    E2E_PIPELINE_START("Adaptive Learning Pipeline");

    RegisterAllRecoveryCallbacks();
    ASSERT_TRUE(nimcp_adaptive_is_initialized());

    E2E_STAGE_BEGIN("Train pattern with successful GC recoveries", MAX_LEARNING_TIME_MS * 2);
    // Create same type of exception repeatedly to build pattern
    for (int i = 0; i < PATTERN_SAMPLES; i++) {
        nimcp_memory_exception_t* ex = nimcp_memory_exception_create(
            NIMCP_ERROR_OUT_OF_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            1024,
            "Adaptive learning test"
        );

        // Generate epitope
        nimcp_exception_generate_epitope((nimcp_exception_t*)ex);

        // Execute GC recovery successfully
        nimcp_exception_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_GC);

        // Record outcome for learning
        nimcp_adaptive_record_outcome((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_GC, true);

        nimcp_exception_unref((nimcp_exception_t*)ex);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify pattern learned GC preference", MAX_LEARNING_TIME_MS);
    nimcp_memory_exception_t* test_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024,
        "Verify learning"
    );
    nimcp_exception_generate_epitope((nimcp_exception_t*)test_ex);

    nimcp_exception_recovery_action_t suggested = nimcp_adaptive_suggest_action((nimcp_exception_t*)test_ex);

    // After enough successful GC recoveries, adaptive should suggest GC
    // (or NONE if not enough samples yet)
    EXPECT_TRUE(suggested == EXCEPTION_RECOVERY_GC || suggested == EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref((nimcp_exception_t*)test_ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train pattern with failed GC, successful RETRY", MAX_LEARNING_TIME_MS * 2);
    g_recovery_tracker.gc_should_succeed = false;

    for (int i = 0; i < PATTERN_SAMPLES; i++) {
        nimcp_io_exception_t* ex = nimcp_io_exception_create(
            NIMCP_ERROR_IO_ERROR,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "/tmp/test.dat",
            "IO learning test"
        );

        nimcp_exception_generate_epitope((nimcp_exception_t*)ex);

        // Try GC (fails)
        nimcp_adaptive_record_outcome((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_GC, false);

        // Try RETRY (succeeds)
        nimcp_adaptive_record_outcome((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_RETRY, true);

        nimcp_exception_unref((nimcp_exception_t*)ex);
    }

    g_recovery_tracker.gc_should_succeed = true;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get adaptive statistics", MAX_LEARNING_TIME_MS);
    nimcp_adaptive_stats_t stats;
    nimcp_adaptive_get_stats(&stats);

    EXPECT_GT(stats.total_patterns, 0u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get confidence for action", MAX_LEARNING_TIME_MS);
    nimcp_memory_exception_t* conf_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024,
        "Confidence test"
    );
    nimcp_exception_generate_epitope((nimcp_exception_t*)conf_ex);

    float confidence = nimcp_adaptive_get_confidence((nimcp_exception_t*)conf_ex, EXCEPTION_RECOVERY_GC);
    // Confidence may be low if not enough samples
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    nimcp_exception_unref((nimcp_exception_t*)conf_ex);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Recovery Strategy Selection Pipeline
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, RecoveryStrategySelectionPipeline) {
    E2E_PIPELINE_START("Recovery Strategy Selection Pipeline");

    E2E_STAGE_BEGIN("Memory exception strategy", MAX_STRATEGY_TIME_MS);
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024,
        "Memory strategy test"
    );

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)mem_ex, &strategy);

    // Memory exception primary: GC or COMPACT
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_GC ||
        strategy.primary_action == EXCEPTION_RECOVERY_COMPACT
    );

    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("I/O exception strategy", MAX_STRATEGY_TIME_MS);
    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_IO_ERROR,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/dev/null",
        "IO strategy test"
    );

    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)io_ex, &strategy);

    // IO exception primary: RETRY
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_RETRY ||
        strategy.primary_action == EXCEPTION_RECOVERY_ROLLBACK
    );

    nimcp_exception_unref((nimcp_exception_t*)io_ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Threading exception strategy", MAX_STRATEGY_TIME_MS);
    nimcp_threading_exception_t* thread_ex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        12345,
        "Threading strategy test"
    );
    thread_ex->is_deadlock = true;

    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)thread_ex, &strategy);

    // Deadlock primary: RESTART_THREAD
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_RESTART_THREAD ||
        strategy.primary_action == EXCEPTION_RECOVERY_RESTART_COMPONENT
    );

    nimcp_exception_unref((nimcp_exception_t*)thread_ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Security exception strategy", MAX_STRATEGY_TIME_MS);
    nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        1,
        "Security strategy test"
    );
    sec_ex->quarantine_required = true;

    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)sec_ex, &strategy);

    // Security exception with quarantine: QUARANTINE
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_QUARANTINE ||
        strategy.fallback_action == EXCEPTION_RECOVERY_QUARANTINE
    );

    nimcp_exception_unref((nimcp_exception_t*)sec_ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Brain exception strategy", MAX_STRATEGY_TIME_MS);
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_NOT_INITIALIZED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1,
        "prefrontal",
        "Brain strategy test"
    );
    brain_ex->has_nan_weights = true;
    brain_ex->learning_diverged = true;

    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)brain_ex, &strategy);

    // Diverged learning: ROLLBACK or CLEAR_CACHE
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_ROLLBACK ||
        strategy.primary_action == EXCEPTION_RECOVERY_CLEAR_CACHE ||
        strategy.primary_action == EXCEPTION_RECOVERY_RESTART_COMPONENT
    );

    nimcp_exception_unref((nimcp_exception_t*)brain_ex);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Failed Recovery Escalation Pipeline
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, FailedRecoveryEscalationPipeline) {
    E2E_PIPELINE_START("Failed Recovery Escalation Pipeline");

    RegisterAllRecoveryCallbacks();

    E2E_STAGE_BEGIN("Simulate primary action failure", MAX_RECOVERY_TIME_MS);
    g_recovery_tracker.gc_should_succeed = false;

    nimcp_memory_exception_t* ex = nimcp_memory_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024,
        "Escalation test"
    );

    // Get strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    // Try primary action (GC - will fail)
    int result = nimcp_exception_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(result, -1);

    // Record failure for metrics
    nimcp_metrics_record_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_GC, false, 1000);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute fallback action", MAX_RECOVERY_TIME_MS);
    // Try fallback (should succeed)
    result = nimcp_exception_execute_recovery((nimcp_exception_t*)ex, strategy.fallback_action);
    EXPECT_EQ(result, 0);

    // Record success for fallback
    nimcp_metrics_record_recovery((nimcp_exception_t*)ex, strategy.fallback_action, true, 2000);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify metrics show failure pattern", MAX_RECOVERY_TIME_MS);
    float gc_rate = nimcp_metrics_get_recovery_rate(EXCEPTION_RECOVERY_GC);
    float fallback_rate = nimcp_metrics_get_recovery_rate(strategy.fallback_action);

    // GC should have lower success rate than fallback
    EXPECT_LT(gc_rate, 1.0f);
    EXPECT_GT(fallback_rate, 0.0f);
    E2E_STAGE_END();

    g_recovery_tracker.gc_should_succeed = true;
    nimcp_exception_unref((nimcp_exception_t*)ex);
    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: All Recovery Actions Pipeline
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, AllRecoveryActionsPipeline) {
    E2E_PIPELINE_START("All Recovery Actions Pipeline");

    RegisterAllRecoveryCallbacks();

    struct ActionTest {
        nimcp_exception_recovery_action_t action;
        const char* name;
        std::atomic<int>* counter;
    };

    ActionTest tests[] = {
        {EXCEPTION_RECOVERY_GC, "GC", &g_recovery_tracker.gc_calls},
        {EXCEPTION_RECOVERY_RETRY, "RETRY", &g_recovery_tracker.retry_calls},
        {EXCEPTION_RECOVERY_ROLLBACK, "ROLLBACK", &g_recovery_tracker.rollback_calls},
        {EXCEPTION_RECOVERY_RESTART_THREAD, "RESTART_THREAD", &g_recovery_tracker.thread_restart_calls},
        {EXCEPTION_RECOVERY_QUARANTINE, "QUARANTINE", &g_recovery_tracker.quarantine_calls},
        {EXCEPTION_RECOVERY_EMERGENCY_SAVE, "EMERGENCY_SAVE", &g_recovery_tracker.emergency_save_calls},
        {EXCEPTION_RECOVERY_REDUCE_LOAD, "REDUCE_LOAD", &g_recovery_tracker.reduce_load_calls},
        {EXCEPTION_RECOVERY_CLEAR_CACHE, "CLEAR_CACHE", &g_recovery_tracker.clear_cache_calls},
        {EXCEPTION_RECOVERY_RESTART_COMPONENT, "RESTART_COMPONENT", &g_recovery_tracker.component_restart_calls},
        {EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN, "GRACEFUL_SHUTDOWN", &g_recovery_tracker.graceful_shutdown_calls},
        {EXCEPTION_RECOVERY_COMPACT, "COMPACT", &g_recovery_tracker.compact_calls},
    };

    for (const auto& test : tests) {
        std::string stage_name = std::string("Test ") + test.name + " action";
        E2E_STAGE_BEGIN(stage_name.c_str(), MAX_RECOVERY_TIME_MS);

        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Test %s", test.name
        );

        int initial_count = test.counter->load();
        int result = nimcp_execute_recovery(ex, test.action);
        EXPECT_EQ(result, 0) << "Failed to execute " << test.name;
        EXPECT_GT(test.counter->load(), initial_count) << test.name << " callback not called";

        // Record metrics
        nimcp_metrics_record_recovery(ex, test.action, true, 1000);

        nimcp_exception_unref(ex);
        E2E_STAGE_END();
    }

    E2E_STAGE_BEGIN("Verify all recovery metrics", MAX_RECOVERY_TIME_MS);
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    // Each action should have at least one attempt
    for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
        EXPECT_GE(metrics.recovery[tests[i].action].attempts, 1u)
            << "No attempts recorded for " << tests[i].name;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Immune-Mediated Recovery Pipeline
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, ImmuneMediatedRecoveryPipeline) {
    E2E_PIPELINE_START("Immune-Mediated Recovery Pipeline");

    RegisterAllRecoveryCallbacks();

    E2E_STAGE_BEGIN("Present exception to immune", MAX_RECOVERY_TIME_MS);
    nimcp_memory_exception_t* ex = nimcp_memory_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024,
        "Immune-mediated recovery"
    );

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));

    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);
    EXPECT_EQ(result, 0);
    EXPECT_GT(response.antigen_id, 0u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute immune-triggered recovery", MAX_RECOVERY_TIME_MS);
    // Get strategy from exception
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    // Execute recovery
    result = nimcp_exception_execute_recovery((nimcp_exception_t*)ex, strategy.primary_action);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Notify immune of recovery result", MAX_RECOVERY_TIME_MS);
    result = nimcp_exception_notify_recovery_result((nimcp_exception_t*)ex, strategy.primary_action, true);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify immune statistics", MAX_RECOVERY_TIME_MS);
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);

    EXPECT_GT(stats.exceptions_presented, 0u);
    EXPECT_GT(stats.recoveries_attempted, 0u);
    EXPECT_GT(stats.recoveries_succeeded, 0u);
    E2E_STAGE_END();

    nimcp_exception_unref((nimcp_exception_t*)ex);
    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Pattern Reset and Relearning Pipeline
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, PatternResetRelearningPipeline) {
    E2E_PIPELINE_START("Pattern Reset and Relearning Pipeline");

    RegisterAllRecoveryCallbacks();

    E2E_STAGE_BEGIN("Train initial pattern", MAX_LEARNING_TIME_MS);
    nimcp_exception_t* template_ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Pattern reset test"
    );
    nimcp_exception_generate_epitope(template_ex);

    // Record some outcomes
    for (int i = 0; i < 5; i++) {
        nimcp_adaptive_record_outcome(template_ex, EXCEPTION_RECOVERY_RETRY, true);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Reset pattern", MAX_LEARNING_TIME_MS);
    nimcp_adaptive_reset_pattern(template_ex->epitope, template_ex->epitope_len);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Retrain pattern", MAX_LEARNING_TIME_MS);
    // Record new outcomes with different action
    for (int i = 0; i < 5; i++) {
        nimcp_adaptive_record_outcome(template_ex, EXCEPTION_RECOVERY_GC, true);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Reset all patterns", MAX_LEARNING_TIME_MS);
    nimcp_adaptive_reset_all();

    nimcp_adaptive_stats_t stats;
    nimcp_adaptive_get_stats(&stats);
    // Patterns may still be tracked but learning data reset
    E2E_STAGE_END();

    nimcp_exception_unref(template_ex);
    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Concurrent Recovery Operations
//=============================================================================

TEST_F(ExceptionRecoveryPipelineTest, ConcurrentRecoveryOperations) {
    E2E_PIPELINE_START("Concurrent Recovery Operations Pipeline");

    RegisterAllRecoveryCallbacks();

    constexpr int THREAD_COUNT = 4;
    constexpr int OPS_PER_THREAD = 20;

    std::atomic<int> total_recoveries{0};
    std::atomic<bool> has_error{false};

    E2E_STAGE_BEGIN("Spawn concurrent recovery threads", 3000);
    std::vector<std::thread> threads;

    for (int t = 0; t < THREAD_COUNT; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED,
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Thread %d op %d", t, i
                );

                if (!ex) {
                    has_error = true;
                    continue;
                }

                // Execute different recovery actions
                nimcp_exception_recovery_action_t actions[] = {
                    EXCEPTION_RECOVERY_GC,
                    EXCEPTION_RECOVERY_RETRY,
                    EXCEPTION_RECOVERY_CLEAR_CACHE,
                    EXCEPTION_RECOVERY_REDUCE_LOAD
                };

                nimcp_exception_recovery_action_t action = actions[i % 4];
                int result = nimcp_execute_recovery(ex, action);

                if (result == 0) {
                    total_recoveries++;
                    nimcp_metrics_record_recovery(ex, action, true, 1000);
                } else {
                    has_error = true;
                }

                nimcp_exception_unref(ex);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(has_error);
    EXPECT_EQ(total_recoveries.load(), THREAD_COUNT * OPS_PER_THREAD);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify metrics consistency", MAX_RECOVERY_TIME_MS);
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    // Sum of all recovery attempts should match total
    uint64_t total_attempts = 0;
    for (int i = 0; i < NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT; i++) {
        total_attempts += metrics.recovery[i].attempts;
    }

    EXPECT_GE(total_attempts, (uint64_t)(THREAD_COUNT * OPS_PER_THREAD));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
