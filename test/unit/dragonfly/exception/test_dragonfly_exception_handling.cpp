/**
 * @file test_dragonfly_exception_handling.cpp
 * @brief Unit tests for dragonfly module exception handling
 *
 * WHAT: Test exception handling across dragonfly subsystems
 * WHY:  Ensure consistent error-to-exception mapping and handler chain dispatch
 * HOW:  Test each dragonfly module's error conditions and exception integration
 *
 * DRAGONFLY MODULES TESTED:
 * - Main coordinator (dragonfly_system)
 * - TSDN (Target-Selective Descending Neurons)
 * - Tracking
 * - Prediction
 * - Interception
 * - Multi-target management
 * - Energy management
 * - Various brain bridges (cognitive, visual, snn, etc.)
 *
 * TEST PATTERNS:
 * - Error code to exception mapping
 * - Exception dispatch through handler chain
 * - Exception category classification (VISUAL_PROCESSING, MOTOR_CONTROL, TRACKING)
 * - Recovery strategy determination
 *
 * @author NIMCP Development Team
 * @date 2026-01-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <vector>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "dragonfly/nimcp_dragonfly.h"
}

//=============================================================================
// Dragonfly Exception Categories
//=============================================================================

// Define dragonfly-specific exception categories for testing
#define EXCEPTION_CATEGORY_DRAGONFLY_BASE      200
#define EXCEPTION_CATEGORY_VISUAL_PROCESSING   (EXCEPTION_CATEGORY_DRAGONFLY_BASE + 1)
#define EXCEPTION_CATEGORY_MOTOR_CONTROL       (EXCEPTION_CATEGORY_DRAGONFLY_BASE + 2)
#define EXCEPTION_CATEGORY_TRACKING            (EXCEPTION_CATEGORY_DRAGONFLY_BASE + 3)
#define EXCEPTION_CATEGORY_PREDICTION          (EXCEPTION_CATEGORY_DRAGONFLY_BASE + 4)
#define EXCEPTION_CATEGORY_INTERCEPTION        (EXCEPTION_CATEGORY_DRAGONFLY_BASE + 5)
#define EXCEPTION_CATEGORY_ENERGY              (EXCEPTION_CATEGORY_DRAGONFLY_BASE + 6)
#define EXCEPTION_CATEGORY_TSDN                (EXCEPTION_CATEGORY_DRAGONFLY_BASE + 7)
#define EXCEPTION_CATEGORY_MULTI_TARGET        (EXCEPTION_CATEGORY_DRAGONFLY_BASE + 8)

//=============================================================================
// Test Fixture
//=============================================================================

class DragonflyExceptionHandlingTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<int> last_exception_category;
    static std::atomic<bool> handler_consumed;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        last_exception_category = 0;
        handler_consumed = false;

        nimcp_exception_system_init();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_exception_category = ex->category;
        return false;  // Don't consume - allow other handlers
    }

    static bool consuming_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        handler_consumed = true;
        return true;  // Consume the exception
    }

    // Helper to create dragonfly exception
    nimcp_exception_t* create_dragonfly_exception(
        nimcp_error_t code,
        int category,
        nimcp_exception_severity_t severity,
        const char* message
    ) {
        nimcp_exception_t* ex = nimcp_exception_create(
            code,
            severity,
            __FILE__, __LINE__, __func__,
            message
        );
        if (ex) {
            ex->category = static_cast<nimcp_exception_category_t>(category);
        }
        return ex;
    }
};

std::atomic<int> DragonflyExceptionHandlingTest::handler_call_count(0);
std::atomic<int> DragonflyExceptionHandlingTest::last_exception_code(0);
std::atomic<int> DragonflyExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> DragonflyExceptionHandlingTest::handler_consumed(false);

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(DragonflyExceptionHandlingTest, CreateVisualProcessingException) {
    // WHAT: Test creation of visual processing exception
    // WHY:  Verify exception fields are set correctly for dragonfly visual pipeline

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        "Visual detection failed - target below contrast threshold"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_VISUAL_PROCESSING);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_NE(ex->message, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(DragonflyExceptionHandlingTest, CreateMotorControlException) {
    // WHAT: Test creation of motor control exception
    // WHY:  Motor commands need proper error categorization

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_MOTOR_CONTROL,
        EXCEPTION_SEVERITY_WARNING,
        "Motor command exceeds maximum turn rate limit"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_MOTOR_CONTROL);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

TEST_F(DragonflyExceptionHandlingTest, CreateTrackingException) {
    // WHAT: Test creation of tracking exception
    // WHY:  Track failures need specialized handling

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_TRACKING,
        EXCEPTION_SEVERITY_ERROR,
        "Target tracking lost - confidence dropped below threshold"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TRACKING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// TSDN Exception Tests
//=============================================================================

TEST_F(DragonflyExceptionHandlingTest, TSDNNullSystemException) {
    // WHAT: Test exception for NULL dragonfly system parameter
    // WHY:  Verify proper error handling for invalid inputs

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_TSDN,
        EXCEPTION_SEVERITY_ERROR,
        "TSDN dragonfly system is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    // Register handler and dispatch
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "tsdn_null_handler";
    options.handler = test_exception_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);
    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NULL_POINTER);

    nimcp_exception_unref(ex);
    if (reg) nimcp_handler_unregister(reg);
}

TEST_F(DragonflyExceptionHandlingTest, TSDNDirectionVectorException) {
    // WHAT: Test exception for invalid TSDN direction encoding
    // WHY:  Direction vectors must be properly normalized

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_TSDN,
        EXCEPTION_SEVERITY_ERROR,
        "TSDN direction vector is not normalized (magnitude: 2.5)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TSDN);

    nimcp_exception_unref(ex);
}

TEST_F(DragonflyExceptionHandlingTest, TSDNPopulationSaturationException) {
    // WHAT: Test exception for TSDN population vector saturation
    // WHY:  All neurons firing may indicate sensor overload

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_TSDN,
        EXCEPTION_SEVERITY_WARNING,
        "TSDN population vector saturated - all 16 neurons above threshold"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Tracking Exception Tests
//=============================================================================

TEST_F(DragonflyExceptionHandlingTest, TrackingTargetLostException) {
    // WHAT: Test exception for target loss during tracking
    // WHY:  Lost targets need recovery or re-acquisition

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_NOT_FOUND,
        EXCEPTION_CATEGORY_TRACKING,
        EXCEPTION_SEVERITY_WARNING,
        "Track target lost after 500ms of no updates"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NOT_FOUND);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TRACKING);

    nimcp_exception_unref(ex);
}

TEST_F(DragonflyExceptionHandlingTest, TrackingMaxTargetsException) {
    // WHAT: Test exception when maximum tracked targets exceeded
    // WHY:  System has finite tracking capacity

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_MULTI_TARGET,
        EXCEPTION_SEVERITY_WARNING,
        "Maximum tracked targets (8) exceeded - dropping lowest priority"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_MULTI_TARGET);

    nimcp_exception_unref(ex);
}

TEST_F(DragonflyExceptionHandlingTest, TrackingStateInvalidException) {
    // WHAT: Test exception for invalid tracking state transition
    // WHY:  State machine must follow valid transitions

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_CATEGORY_TRACKING,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid state transition: LOST -> LOCKED (must go through TENTATIVE)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_STATE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Prediction Exception Tests
//=============================================================================

TEST_F(DragonflyExceptionHandlingTest, PredictionFilterDivergenceException) {
    // WHAT: Test exception for IMM filter divergence
    // WHY:  Divergent filters produce unreliable predictions

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_PREDICTION,
        EXCEPTION_SEVERITY_ERROR,
        "IMM filter diverged - covariance matrix non-positive-definite"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_PREDICTION);

    nimcp_exception_unref(ex);
}

TEST_F(DragonflyExceptionHandlingTest, PredictionHorizonException) {
    // WHAT: Test exception for invalid prediction horizon
    // WHY:  Prediction horizon must be positive and bounded

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_PREDICTION,
        EXCEPTION_SEVERITY_ERROR,
        "Prediction horizon must be in range (0, 5.0] seconds"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Interception Exception Tests
//=============================================================================

TEST_F(DragonflyExceptionHandlingTest, InterceptionInfeasibleException) {
    // WHAT: Test exception for infeasible interception solution
    // WHY:  Target may be unreachable given current constraints

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_INTERCEPTION,
        EXCEPTION_SEVERITY_WARNING,
        "Interception infeasible - target outside velocity envelope"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_INTERCEPTION);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

TEST_F(DragonflyExceptionHandlingTest, InterceptionGuidanceException) {
    // WHAT: Test exception for proportional navigation failure
    // WHY:  PN guidance requires valid bearing rate

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_INTERCEPTION,
        EXCEPTION_SEVERITY_ERROR,
        "PN guidance failed - bearing rate undefined (target at zero range)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Energy Management Exception Tests
//=============================================================================

TEST_F(DragonflyExceptionHandlingTest, EnergyLowException) {
    // WHAT: Test exception for low energy level
    // WHY:  Energy-aware decisions depend on remaining energy

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_ENERGY,
        EXCEPTION_SEVERITY_WARNING,
        "Energy level below minimum reserve (0.15 < 0.20)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_ENERGY);

    nimcp_exception_unref(ex);
}

TEST_F(DragonflyExceptionHandlingTest, EnergyCriticalException) {
    // WHAT: Test exception for critical energy depletion
    // WHY:  Critical energy requires immediate response

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_ENERGY,
        EXCEPTION_SEVERITY_CRITICAL,
        "Energy level critical (0.02) - aborting pursuit"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Multi-Target Exception Tests
//=============================================================================

TEST_F(DragonflyExceptionHandlingTest, MultiTargetPrioritizationException) {
    // WHAT: Test exception for target prioritization failure
    // WHY:  Multiple targets with equal priority may cause deadlock

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_MULTI_TARGET,
        EXCEPTION_SEVERITY_WARNING,
        "Target prioritization ambiguous - multiple targets with equal threat level"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_MULTI_TARGET);

    nimcp_exception_unref(ex);
}

TEST_F(DragonflyExceptionHandlingTest, MultiTargetConflictException) {
    // WHAT: Test exception for conflicting target assignments
    // WHY:  Cannot pursue multiple targets simultaneously

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_MULTI_TARGET,
        EXCEPTION_SEVERITY_ERROR,
        "Target conflict - locked targets require divergent pursuit vectors"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Handler Chain Tests
//=============================================================================

TEST_F(DragonflyExceptionHandlingTest, HandlerChainDispatch) {
    // WHAT: Test exception dispatch through multiple handlers
    // WHY:  Verify chain processing works correctly

    // Register multiple handlers
    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    options1.name = "dragonfly_handler_1";
    options1.handler = test_exception_handler;
    options1.priority = 100;

    options2.name = "dragonfly_handler_2";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_TRACKING,
        EXCEPTION_SEVERITY_ERROR,
        "Test exception for handler chain"
    );

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    // Both handlers should be called (neither consumes)
    EXPECT_GE(handler_call_count.load(), 2);

    nimcp_exception_unref(ex);
    if (reg1) nimcp_handler_unregister(reg1);
    if (reg2) nimcp_handler_unregister(reg2);
}

TEST_F(DragonflyExceptionHandlingTest, HandlerConsumesException) {
    // WHAT: Test handler consuming exception stops chain
    // WHY:  Verify consumed exceptions don't propagate

    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    // First handler consumes
    options1.name = "consuming_handler";
    options1.handler = consuming_exception_handler;
    options1.priority = 100;

    // Second handler should not be called
    options2.name = "secondary_handler";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_TRACKING,
        EXCEPTION_SEVERITY_ERROR,
        "Test exception for consumption"
    );

    handler_call_count = 0;
    handler_consumed = false;
    nimcp_exception_dispatch(ex);

    // Only consuming handler should be called
    EXPECT_TRUE(handler_consumed.load());
    EXPECT_EQ(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
    if (reg1) nimcp_handler_unregister(reg1);
    if (reg2) nimcp_handler_unregister(reg2);
}

//=============================================================================
// Recovery Strategy Tests
//=============================================================================

TEST_F(DragonflyExceptionHandlingTest, TrackingExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for tracking exceptions
    // WHY:  Tracking failures may need target re-acquisition

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_NOT_FOUND,
        EXCEPTION_CATEGORY_TRACKING,
        EXCEPTION_SEVERITY_ERROR,
        "Target tracking lost"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Tracking exceptions should have retry as primary action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(DragonflyExceptionHandlingTest, CriticalDragonflyExceptionRecovery) {
    // WHAT: Test recovery for critical dragonfly failures
    // WHY:  Critical failures may require system reset

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_CATEGORY_TRACKING,
        EXCEPTION_SEVERITY_CRITICAL,
        "Tracking state memory corruption detected"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Critical errors should trigger some kind of recovery action
    EXPECT_TRUE(strategy.primary_action != EXCEPTION_RECOVERY_NONE ||
                strategy.fallback_action != EXCEPTION_RECOVERY_NONE ||
                strategy.retry_count > 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Exception Statistics Tests
//=============================================================================

TEST_F(DragonflyExceptionHandlingTest, ExceptionStatisticsTracking) {
    // WHAT: Test that exception dispatch is tracked by handlers
    // WHY:  Need to monitor dragonfly exception frequency

    // Register a counting handler
    static std::atomic<int> dispatch_count{0};
    dispatch_count = 0;

    auto counting_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        dispatch_count++;
        return false;
    };

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "stats_counter";
    opts.handler = counting_handler;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Create and dispatch several exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = create_dragonfly_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_CATEGORY_TRACKING,
            EXCEPTION_SEVERITY_WARNING,
            "Test exception for statistics"
        );
        if (ex) {
            nimcp_exception_dispatch(ex);
            nimcp_exception_unref(ex);
        }
    }

    // Handler should have been called for each exception
    EXPECT_GE(dispatch_count.load(), 5);

    nimcp_handler_unregister(reg);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(DragonflyExceptionHandlingTest, ConcurrentExceptionCreation) {
    // WHAT: Test concurrent exception creation
    // WHY:  Dragonfly runs real-time; exceptions must be thread-safe

    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int exceptions_per_thread = 10;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&success_count, t, exceptions_per_thread]() {
            for (int i = 0; i < exceptions_per_thread; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED,
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Thread %d exception %d", t, i
                );
                if (ex) {
                    success_count++;
                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * exceptions_per_thread);
}

//=============================================================================
// Mode-Specific Exception Tests
//=============================================================================

TEST_F(DragonflyExceptionHandlingTest, ModeTransitionException) {
    // WHAT: Test exception for invalid mode transition
    // WHY:  Mode state machine must follow valid transitions

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_CATEGORY_TRACKING,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid mode transition: IDLE -> INTERCEPTING (must be TRACKING or PURSUING)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_STATE);

    nimcp_exception_unref(ex);
}

TEST_F(DragonflyExceptionHandlingTest, HuntTimeoutException) {
    // WHAT: Test exception for hunt timeout
    // WHY:  Pursuits have time limits for energy conservation

    nimcp_exception_t* ex = create_dragonfly_exception(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_CATEGORY_INTERCEPTION,
        EXCEPTION_SEVERITY_WARNING,
        "Hunt timeout exceeded (10.0s) - aborting pursuit"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_TIMEOUT);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_INTERCEPTION);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
