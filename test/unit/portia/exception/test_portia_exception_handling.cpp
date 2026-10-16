/**
 * @file test_portia_exception_handling.cpp
 * @brief Unit tests for portia module exception handling
 *
 * WHAT: Test exception handling across portia subsystems
 * WHY:  Ensure consistent error-to-exception mapping and handler chain dispatch
 * HOW:  Test each portia module's error conditions and exception integration
 *
 * PORTIA MODULES TESTED:
 * - Main coordinator (portia context)
 * - Tier Manager
 * - Power Monitor
 * - Resource Tracker
 * - Degradation Controller
 * - Accelerator Detector
 * - Sensor Fusion
 * - Planning Engine
 * - Target Classifier
 *
 * TEST PATTERNS:
 * - Error code to exception mapping
 * - Exception dispatch through handler chain
 * - Exception category classification (RESOURCE, POWER, THERMAL, ADAPTATION)
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
#include "portia/nimcp_portia.h"
}

//=============================================================================
// Portia Exception Categories
//=============================================================================

// Define portia-specific exception categories for testing
#define EXCEPTION_CATEGORY_PORTIA_BASE         300
#define EXCEPTION_CATEGORY_RESOURCE            (EXCEPTION_CATEGORY_PORTIA_BASE + 1)
#define EXCEPTION_CATEGORY_POWER               (EXCEPTION_CATEGORY_PORTIA_BASE + 2)
#define EXCEPTION_CATEGORY_THERMAL             (EXCEPTION_CATEGORY_PORTIA_BASE + 3)
#define EXCEPTION_CATEGORY_ADAPTATION          (EXCEPTION_CATEGORY_PORTIA_BASE + 4)
#define EXCEPTION_CATEGORY_TIER                (EXCEPTION_CATEGORY_PORTIA_BASE + 5)
#define EXCEPTION_CATEGORY_DEGRADATION         (EXCEPTION_CATEGORY_PORTIA_BASE + 6)
#define EXCEPTION_CATEGORY_ACCELERATOR         (EXCEPTION_CATEGORY_PORTIA_BASE + 7)
#define EXCEPTION_CATEGORY_SENSOR_FUSION       (EXCEPTION_CATEGORY_PORTIA_BASE + 8)
#define EXCEPTION_CATEGORY_PLANNING            (EXCEPTION_CATEGORY_PORTIA_BASE + 9)

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaExceptionHandlingTest : public ::testing::Test {
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

    // Helper to create portia exception
    nimcp_exception_t* create_portia_exception(
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

std::atomic<int> PortiaExceptionHandlingTest::handler_call_count(0);
std::atomic<int> PortiaExceptionHandlingTest::last_exception_code(0);
std::atomic<int> PortiaExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> PortiaExceptionHandlingTest::handler_consumed(false);

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(PortiaExceptionHandlingTest, CreateResourceException) {
    // WHAT: Test creation of resource monitoring exception
    // WHY:  Verify exception fields are set correctly for portia resource tracking

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_RESOURCE,
        EXCEPTION_SEVERITY_WARNING,
        "CPU usage exceeded threshold (95% > 90%)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_RESOURCE);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_NE(ex->message, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, CreatePowerException) {
    // WHAT: Test creation of power monitoring exception
    // WHY:  Power-related errors need proper categorization

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_PORTIA_ERROR_POWER_CRITICAL,
        EXCEPTION_CATEGORY_POWER,
        EXCEPTION_SEVERITY_CRITICAL,
        "Battery level critical (3%) - initiating emergency shutdown"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_POWER);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, CreateThermalException) {
    // WHAT: Test creation of thermal monitoring exception
    // WHY:  Thermal issues need specialized handling

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_PORTIA_ERROR_THERMAL_THROTTLE,
        EXCEPTION_CATEGORY_THERMAL,
        EXCEPTION_SEVERITY_ERROR,
        "System thermal throttling active - CPU at 92C"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_PORTIA_ERROR_THERMAL_THROTTLE);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_THERMAL);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Tier Manager Exception Tests
//=============================================================================

TEST_F(PortiaExceptionHandlingTest, TierManagerNullContextException) {
    // WHAT: Test exception for NULL portia context parameter
    // WHY:  Verify proper error handling for invalid inputs

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_TIER,
        EXCEPTION_SEVERITY_ERROR,
        "Portia context is NULL - system not initialized"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    // Register handler and dispatch
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "tier_null_handler";
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

TEST_F(PortiaExceptionHandlingTest, TierLockedSwitchException) {
    // WHAT: Test exception for tier switch when locked
    // WHY:  Tier switching may be disabled by configuration

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_PORTIA_ERROR_TIER_LOCKED,
        EXCEPTION_CATEGORY_TIER,
        EXCEPTION_SEVERITY_WARNING,
        "Tier switch rejected - tier is locked to PLATFORM_TIER_FULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_PORTIA_ERROR_TIER_LOCKED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TIER);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, TierInvalidTransitionException) {
    // WHAT: Test exception for invalid tier transition
    // WHY:  Some tier transitions may not be valid

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_TIER,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid tier value (5) - must be in range [0, 3]"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Power Monitor Exception Tests
//=============================================================================

TEST_F(PortiaExceptionHandlingTest, PowerBatteryLowException) {
    // WHAT: Test exception for low battery
    // WHY:  Low battery triggers degradation

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_POWER,
        EXCEPTION_SEVERITY_WARNING,
        "Battery level low (15%) - enabling power conservation"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_POWER);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, PowerBatteryCriticalException) {
    // WHAT: Test exception for critical battery
    // WHY:  Critical battery requires immediate response

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_PORTIA_ERROR_POWER_CRITICAL,
        EXCEPTION_CATEGORY_POWER,
        EXCEPTION_SEVERITY_CRITICAL,
        "Battery level critical (2%) - emergency shutdown imminent"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_PORTIA_ERROR_POWER_CRITICAL);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, PowerStateUnknownException) {
    // WHAT: Test exception for unknown power state
    // WHY:  Some systems may not report power state

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_NOT_FOUND,
        EXCEPTION_CATEGORY_POWER,
        EXCEPTION_SEVERITY_INFO,
        "Power state unknown - assuming AC power"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NOT_FOUND);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Resource Tracker Exception Tests
//=============================================================================

TEST_F(PortiaExceptionHandlingTest, ResourceMemoryHighException) {
    // WHAT: Test exception for high memory usage
    // WHY:  Memory pressure triggers degradation

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_RESOURCE,
        EXCEPTION_SEVERITY_WARNING,
        "Memory usage high (88%) - consider reducing batch size"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_RESOURCE);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, ResourceCPUThrottlingException) {
    // WHAT: Test exception for CPU throttling
    // WHY:  CPU throttling affects performance

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_RESOURCE,
        EXCEPTION_SEVERITY_WARNING,
        "CPU frequency reduced due to thermal throttling"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Degradation Controller Exception Tests
//=============================================================================

TEST_F(PortiaExceptionHandlingTest, DegradationFailedException) {
    // WHAT: Test exception for degradation failure
    // WHY:  Degradation may fail if already at minimum

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_PORTIA_ERROR_DEGRADATION_FAILED,
        EXCEPTION_CATEGORY_DEGRADATION,
        EXCEPTION_SEVERITY_ERROR,
        "Cannot degrade further - already at EMERGENCY level"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_PORTIA_ERROR_DEGRADATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_DEGRADATION);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, DegradationLevelInvalidException) {
    // WHAT: Test exception for invalid degradation level
    // WHY:  Degradation levels must be valid

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_DEGRADATION,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid degradation level (5) - must be in range [0, 4]"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, DegradationMaxReachedException) {
    // WHAT: Test exception when max degradation reached
    // WHY:  System may need emergency action when at max degradation

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_DEGRADATION,
        EXCEPTION_SEVERITY_SEVERE,
        "Maximum degradation level reached - system in survival mode"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_SEVERE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Accelerator Detector Exception Tests
//=============================================================================

TEST_F(PortiaExceptionHandlingTest, AcceleratorNotAvailableException) {
    // WHAT: Test exception for accelerator not available
    // WHY:  GPU/NPU may not be present on all systems

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_PORTIA_ERROR_NO_ACCELERATOR,
        EXCEPTION_CATEGORY_ACCELERATOR,
        EXCEPTION_SEVERITY_INFO,
        "No GPU accelerator detected - using CPU fallback"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_PORTIA_ERROR_NO_ACCELERATOR);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_ACCELERATOR);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, AcceleratorDetectionTimeoutException) {
    // WHAT: Test exception for accelerator detection timeout
    // WHY:  Detection may timeout on slow systems

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_CATEGORY_ACCELERATOR,
        EXCEPTION_SEVERITY_WARNING,
        "Accelerator detection timed out after 5000ms"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_TIMEOUT);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Sensor Fusion Exception Tests
//=============================================================================

TEST_F(PortiaExceptionHandlingTest, SensorFusionConflictException) {
    // WHAT: Test exception for conflicting sensor readings
    // WHY:  Multiple sensors may report conflicting data

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SENSOR_FUSION,
        EXCEPTION_SEVERITY_WARNING,
        "Sensor fusion conflict - CPU and thermal sensors disagree"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SENSOR_FUSION);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, SensorFusionMissingSensorException) {
    // WHAT: Test exception for missing sensor data
    // WHY:  Some sensors may not be available

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_NOT_FOUND,
        EXCEPTION_CATEGORY_SENSOR_FUSION,
        EXCEPTION_SEVERITY_INFO,
        "Thermal sensor data unavailable - using estimated values"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NOT_FOUND);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Planning Engine Exception Tests
//=============================================================================

TEST_F(PortiaExceptionHandlingTest, PlanningResourceConflictException) {
    // WHAT: Test exception for resource allocation conflict
    // WHY:  Planning may fail due to conflicting resource requests

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_PLANNING,
        EXCEPTION_SEVERITY_ERROR,
        "Resource planning conflict - cannot satisfy all constraints"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_PLANNING);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, PlanningTimeoutException) {
    // WHAT: Test exception for planning timeout
    // WHY:  Planning must complete within time budget

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_CATEGORY_PLANNING,
        EXCEPTION_SEVERITY_WARNING,
        "Planning engine timeout - using previous plan"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_TIMEOUT);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Handler Chain Tests
//=============================================================================

TEST_F(PortiaExceptionHandlingTest, HandlerChainDispatch) {
    // WHAT: Test exception dispatch through multiple handlers
    // WHY:  Verify chain processing works correctly

    // Register multiple handlers
    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    options1.name = "portia_handler_1";
    options1.handler = test_exception_handler;
    options1.priority = 100;

    options2.name = "portia_handler_2";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_RESOURCE,
        EXCEPTION_SEVERITY_WARNING,
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

TEST_F(PortiaExceptionHandlingTest, HandlerConsumesException) {
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

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_RESOURCE,
        EXCEPTION_SEVERITY_WARNING,
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

TEST_F(PortiaExceptionHandlingTest, ResourceExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for resource exceptions
    // WHY:  Resource issues may need load reduction

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_RESOURCE,
        EXCEPTION_SEVERITY_WARNING,
        "Memory usage high"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Resource exceptions should suggest some recovery action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, CriticalPortiaExceptionRecovery) {
    // WHAT: Test recovery for critical portia failures
    // WHY:  Critical failures may require emergency action

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_PORTIA_ERROR_POWER_CRITICAL,
        EXCEPTION_CATEGORY_POWER,
        EXCEPTION_SEVERITY_CRITICAL,
        "Critical power failure detected"
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

TEST_F(PortiaExceptionHandlingTest, ExceptionStatisticsTracking) {
    // WHAT: Test that exception dispatch is tracked by handlers
    // WHY:  Need to monitor portia exception frequency

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
        nimcp_exception_t* ex = create_portia_exception(
            NIMCP_ERROR_OUT_OF_RANGE,
            EXCEPTION_CATEGORY_RESOURCE,
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

TEST_F(PortiaExceptionHandlingTest, ConcurrentExceptionCreation) {
    // WHAT: Test concurrent exception creation
    // WHY:  Portia monitors run across multiple threads

    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int exceptions_per_thread = 10;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&success_count, t, exceptions_per_thread]() {
            for (int i = 0; i < exceptions_per_thread; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OUT_OF_RANGE,
                    EXCEPTION_SEVERITY_WARNING,
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
// Portia-Specific Error Code Tests
//=============================================================================

TEST_F(PortiaExceptionHandlingTest, PortiaNotInitializedException) {
    // WHAT: Test exception for uninitialized portia
    // WHY:  Operations require initialization

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_PORTIA_ERROR_NOT_INITIALIZED,
        EXCEPTION_CATEGORY_ADAPTATION,
        EXCEPTION_SEVERITY_ERROR,
        "Portia system not initialized - call portia_init() first"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_PORTIA_ERROR_NOT_INITIALIZED);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, PortiaAlreadyInitializedException) {
    // WHAT: Test exception for double initialization
    // WHY:  Double initialization should be prevented

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_PORTIA_ERROR_ALREADY_INITIALIZED,
        EXCEPTION_CATEGORY_ADAPTATION,
        EXCEPTION_SEVERITY_WARNING,
        "Portia already initialized - ignoring duplicate init"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_PORTIA_ERROR_ALREADY_INITIALIZED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Workload Classification Exception Tests
//=============================================================================

TEST_F(PortiaExceptionHandlingTest, WorkloadUnknownException) {
    // WHAT: Test exception for unknown workload type
    // WHY:  Workload classification may fail

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_NOT_FOUND,
        EXCEPTION_CATEGORY_ADAPTATION,
        EXCEPTION_SEVERITY_INFO,
        "Unknown workload type - defaulting to INFERENCE"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NOT_FOUND);

    nimcp_exception_unref(ex);
}

TEST_F(PortiaExceptionHandlingTest, WorkloadMismatchException) {
    // WHAT: Test exception for workload/tier mismatch
    // WHY:  Heavy workloads on constrained tiers need handling

    nimcp_exception_t* ex = create_portia_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_ADAPTATION,
        EXCEPTION_SEVERITY_WARNING,
        "Workload TRAINING not supported on PLATFORM_TIER_MINIMAL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
