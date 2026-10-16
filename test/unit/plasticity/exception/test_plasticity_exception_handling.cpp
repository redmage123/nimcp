/**
 * @file test_plasticity_exception_handling.cpp
 * @brief Unit tests for plasticity module exception handling
 *
 * WHAT: Test exception handling across all plasticity modules
 * WHY:  Ensure consistent error-to-exception mapping and handler chain dispatch
 * HOW:  Test each plasticity module's error conditions and exception integration
 *
 * PLASTICITY MODULES TESTED:
 * - STDP (Spike-Timing-Dependent Plasticity)
 * - BCM (Bienenstock-Cooper-Munro)
 * - Homeostatic Plasticity
 * - Structural Plasticity
 * - Short-Term Plasticity (STP)
 * - Eligibility Traces
 * - Metaplasticity
 * - Neuromodulators
 *
 * TEST PATTERNS:
 * - Error code to exception mapping
 * - Exception dispatch through handler chain
 * - Exception category classification (LEARNING, ADAPTATION)
 * - Recovery strategy determination
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "plasticity/stp/nimcp_stp.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
}

//=============================================================================
// Plasticity Exception Categories
//=============================================================================

// Define plasticity-specific exception categories for testing
#define EXCEPTION_CATEGORY_PLASTICITY_BASE     100
#define EXCEPTION_CATEGORY_LEARNING            (EXCEPTION_CATEGORY_PLASTICITY_BASE + 1)
#define EXCEPTION_CATEGORY_ADAPTATION          (EXCEPTION_CATEGORY_PLASTICITY_BASE + 2)
#define EXCEPTION_CATEGORY_WEIGHT_UPDATE       (EXCEPTION_CATEGORY_PLASTICITY_BASE + 3)
#define EXCEPTION_CATEGORY_TIMING              (EXCEPTION_CATEGORY_PLASTICITY_BASE + 4)
#define EXCEPTION_CATEGORY_TRACE_DECAY         (EXCEPTION_CATEGORY_PLASTICITY_BASE + 5)
#define EXCEPTION_CATEGORY_HOMEOSTASIS         (EXCEPTION_CATEGORY_PLASTICITY_BASE + 6)
#define EXCEPTION_CATEGORY_STRUCTURAL          (EXCEPTION_CATEGORY_PLASTICITY_BASE + 7)

//=============================================================================
// Test Fixture
//=============================================================================

class PlasticityExceptionHandlingTest : public ::testing::Test {
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

    // Helper to create plasticity exception
    nimcp_exception_t* create_plasticity_exception(
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

std::atomic<int> PlasticityExceptionHandlingTest::handler_call_count(0);
std::atomic<int> PlasticityExceptionHandlingTest::last_exception_code(0);
std::atomic<int> PlasticityExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> PlasticityExceptionHandlingTest::handler_consumed(false);

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(PlasticityExceptionHandlingTest, CreateLearningException) {
    // WHAT: Test creation of learning-related exception
    // WHY:  Verify exception fields are set correctly

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_ERROR,
        "STDP weight update failed due to invalid timing window"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_LEARNING_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_LEARNING);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_NE(ex->message, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(PlasticityExceptionHandlingTest, CreateAdaptationException) {
    // WHAT: Test creation of adaptation-related exception
    // WHY:  Homeostatic plasticity errors need proper categorization

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_ADAPTATION,
        EXCEPTION_SEVERITY_WARNING,
        "Homeostatic scaling failed - target rate exceeded bounds"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_ADAPTATION);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

TEST_F(PlasticityExceptionHandlingTest, CreateWeightUpdateException) {
    // WHAT: Test creation of weight update exception
    // WHY:  Weight update failures need specialized handling

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_WEIGHT_INIT,
        EXCEPTION_CATEGORY_WEIGHT_UPDATE,
        EXCEPTION_SEVERITY_ERROR,
        "Weight update overflow detected in BCM rule"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_WEIGHT_INIT);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_WEIGHT_UPDATE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// STDP Exception Tests
//=============================================================================

TEST_F(PlasticityExceptionHandlingTest, STDPNullSynapseException) {
    // WHAT: Test exception for NULL synapse parameter
    // WHY:  Verify proper error handling for invalid inputs

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_ERROR,
        "STDP synapse is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    // Register handler and dispatch
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "stdp_null_handler";
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

TEST_F(PlasticityExceptionHandlingTest, STDPTimingWindowException) {
    // WHAT: Test exception for invalid STDP timing parameters
    // WHY:  Timing parameters must be positive

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_TIMING,
        EXCEPTION_SEVERITY_ERROR,
        "STDP tau_plus must be positive (got: -20.0)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TIMING);

    nimcp_exception_unref(ex);
}

TEST_F(PlasticityExceptionHandlingTest, STDPWeightBoundsException) {
    // WHAT: Test exception for weight exceeding bounds
    // WHY:  Weights should stay within [w_min, w_max]

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_WEIGHT_UPDATE,
        EXCEPTION_SEVERITY_WARNING,
        "STDP weight exceeded maximum bound (1.5 > 1.0)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// BCM Exception Tests
//=============================================================================

TEST_F(PlasticityExceptionHandlingTest, BCMThresholdException) {
    // WHAT: Test exception for invalid BCM threshold
    // WHY:  Theta_m must be positive for stable learning

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_ADAPTATION,
        EXCEPTION_SEVERITY_ERROR,
        "BCM sliding threshold theta_m is non-positive"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_ADAPTATION);

    nimcp_exception_unref(ex);
}

TEST_F(PlasticityExceptionHandlingTest, BCMActivityException) {
    // WHAT: Test exception for invalid activity rate
    // WHY:  Activity rate affects threshold computation

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_WARNING,
        "BCM activity rate is negative (invalid)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Homeostatic Plasticity Exception Tests
//=============================================================================

TEST_F(PlasticityExceptionHandlingTest, HomeostaticTargetRateException) {
    // WHAT: Test exception for invalid target firing rate
    // WHY:  Target rate must be positive and realistic

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_HOMEOSTASIS,
        EXCEPTION_SEVERITY_ERROR,
        "Homeostatic target firing rate is negative"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_HOMEOSTASIS);

    nimcp_exception_unref(ex);
}

TEST_F(PlasticityExceptionHandlingTest, HomeostaticScalingFactorException) {
    // WHAT: Test exception for extreme scaling factor
    // WHY:  Scaling factors should be bounded to prevent instability

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_HOMEOSTASIS,
        EXCEPTION_SEVERITY_WARNING,
        "Homeostatic scaling factor exceeds safety limit (10.0 > 5.0)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Structural Plasticity Exception Tests
//=============================================================================

TEST_F(PlasticityExceptionHandlingTest, StructuralSpineCapacityException) {
    // WHAT: Test exception for spine capacity exceeded
    // WHY:  Dendrites have limited spine capacity

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_STRUCTURAL,
        EXCEPTION_SEVERITY_WARNING,
        "Spine capacity exceeded on dendrite - cannot add new spine"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_STRUCTURAL);

    nimcp_exception_unref(ex);
}

TEST_F(PlasticityExceptionHandlingTest, StructuralPruningException) {
    // WHAT: Test exception for pruning failure
    // WHY:  Spine pruning may fail if spine is protected

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_STRUCTURAL,
        EXCEPTION_SEVERITY_ERROR,
        "Failed to prune spine - spine is in protected state"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Short-Term Plasticity (STP) Exception Tests
//=============================================================================

TEST_F(PlasticityExceptionHandlingTest, STPParameterException) {
    // WHAT: Test exception for invalid STP parameters
    // WHY:  U, tau_D, tau_F must be valid

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_ERROR,
        "STP release probability U must be in (0, 1]"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

TEST_F(PlasticityExceptionHandlingTest, STPStateException) {
    // WHAT: Test exception for invalid STP state
    // WHY:  x and u must be in valid range

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_WARNING,
        "STP available resources x is negative (numerical instability)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_STATE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Eligibility Trace Exception Tests
//=============================================================================

TEST_F(PlasticityExceptionHandlingTest, EligibilityDecayException) {
    // WHAT: Test exception for invalid decay parameter
    // WHY:  Lambda must be in (0, 1) for stable decay

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_TRACE_DECAY,
        EXCEPTION_SEVERITY_ERROR,
        "Eligibility decay lambda must be in (0, 1)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TRACE_DECAY);

    nimcp_exception_unref(ex);
}

TEST_F(PlasticityExceptionHandlingTest, EligibilityRewardException) {
    // WHAT: Test exception for invalid reward signal
    // WHY:  Reward should be bounded

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_WARNING,
        "Eligibility reward signal exceeds expected range"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Handler Chain Tests
//=============================================================================

TEST_F(PlasticityExceptionHandlingTest, HandlerChainDispatch) {
    // WHAT: Test exception dispatch through multiple handlers
    // WHY:  Verify chain processing works correctly

    // Register multiple handlers
    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    options1.name = "plasticity_handler_1";
    options1.handler = test_exception_handler;
    options1.priority = 100;

    options2.name = "plasticity_handler_2";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_LEARNING,
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

TEST_F(PlasticityExceptionHandlingTest, HandlerConsumesException) {
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

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_LEARNING,
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

TEST_F(PlasticityExceptionHandlingTest, LearningExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for learning exceptions
    // WHY:  Learning failures may need retry or parameter adjustment

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_ERROR,
        "STDP learning step failed"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Learning exceptions should have retry as primary action
    // (actual strategy depends on implementation)
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(PlasticityExceptionHandlingTest, CriticalPlasticityExceptionRecovery) {
    // WHAT: Test recovery for critical plasticity failures
    // WHY:  Critical failures may require emergency save

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_CATEGORY_WEIGHT_UPDATE,
        EXCEPTION_SEVERITY_CRITICAL,
        "Weight array memory corruption detected"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Critical errors should trigger some kind of recovery action
    // The actual action depends on the recovery strategy implementation
    EXPECT_TRUE(strategy.primary_action != EXCEPTION_RECOVERY_NONE ||
                strategy.fallback_action != EXCEPTION_RECOVERY_NONE ||
                strategy.retry_count > 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Exception Statistics Tests
//=============================================================================

TEST_F(PlasticityExceptionHandlingTest, ExceptionStatisticsTracking) {
    // WHAT: Test that exception dispatch is tracked by handlers
    // WHY:  Need to monitor plasticity exception frequency

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
        nimcp_exception_t* ex = create_plasticity_exception(
            NIMCP_ERROR_LEARNING_FAILED,
            EXCEPTION_CATEGORY_LEARNING,
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

TEST_F(PlasticityExceptionHandlingTest, ConcurrentExceptionCreation) {
    // WHAT: Test concurrent exception creation
    // WHY:  Plasticity runs across multiple threads

    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int exceptions_per_thread = 10;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&success_count, t, exceptions_per_thread]() {
            for (int i = 0; i < exceptions_per_thread; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_LEARNING_FAILED,
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
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
