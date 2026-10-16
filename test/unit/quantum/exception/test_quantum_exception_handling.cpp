/**
 * @file test_quantum_exception_handling.cpp
 * @brief Unit tests for Quantum module exception handling
 *
 * WHAT: Test exception handling across quantum annealing and quantum-inspired components
 * WHY:  Ensure consistent error-to-exception mapping and handler chain dispatch
 * HOW:  Test each quantum module's error conditions and exception integration
 *
 * QUANTUM MODULES TESTED:
 * - Quantum annealer creation and configuration errors
 * - Energy function evaluation failures
 * - Temperature/cooling schedule issues
 * - Monte Carlo integration errors
 * - Quantum tunneling failures
 * - Partition function estimation errors
 *
 * TEST PATTERNS:
 * - Error code to exception mapping
 * - Exception dispatch through handler chain
 * - Exception category classification
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
}

// Module header may include CUDA headers with templates, so include outside extern "C"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"

//=============================================================================
// Quantum Exception Categories
//=============================================================================

// Define quantum-specific exception categories for testing
#define EXCEPTION_CATEGORY_QUANTUM_BASE       400
#define EXCEPTION_CATEGORY_QUANTUM_ANNEALER   (EXCEPTION_CATEGORY_QUANTUM_BASE + 1)
#define EXCEPTION_CATEGORY_QUANTUM_ENERGY     (EXCEPTION_CATEGORY_QUANTUM_BASE + 2)
#define EXCEPTION_CATEGORY_QUANTUM_COOLING    (EXCEPTION_CATEGORY_QUANTUM_BASE + 3)
#define EXCEPTION_CATEGORY_QUANTUM_TUNNELING  (EXCEPTION_CATEGORY_QUANTUM_BASE + 4)
#define EXCEPTION_CATEGORY_QUANTUM_MC         (EXCEPTION_CATEGORY_QUANTUM_BASE + 5)
#define EXCEPTION_CATEGORY_QUANTUM_PARTITION  (EXCEPTION_CATEGORY_QUANTUM_BASE + 6)
#define EXCEPTION_CATEGORY_QUANTUM_NUMERICAL  (EXCEPTION_CATEGORY_QUANTUM_BASE + 7)

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumExceptionHandlingTest : public ::testing::Test {
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

    // Helper to create quantum exception
    nimcp_exception_t* create_quantum_exception(
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

std::atomic<int> QuantumExceptionHandlingTest::handler_call_count(0);
std::atomic<int> QuantumExceptionHandlingTest::last_exception_code(0);
std::atomic<int> QuantumExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> QuantumExceptionHandlingTest::handler_consumed(false);

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(QuantumExceptionHandlingTest, CreateAnnealerException) {
    // WHAT: Test creation of quantum annealer exception
    // WHY:  Verify exception fields are set correctly for annealer errors

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_QUANTUM_ANNEALER,
        EXCEPTION_SEVERITY_ERROR,
        "Quantum annealer creation failed - invalid configuration"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_QUANTUM_ANNEALER);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_NE(ex->message, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, CreateEnergyFunctionException) {
    // WHAT: Test creation of energy function exception
    // WHY:  Energy functions may have evaluation issues

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_QUANTUM_ENERGY,
        EXCEPTION_SEVERITY_ERROR,
        "Energy function evaluation failed - returned NaN"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_QUANTUM_ENERGY);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, CreateCoolingScheduleException) {
    // WHAT: Test creation of cooling schedule exception
    // WHY:  Cooling schedule parameters must be valid

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_QUANTUM_COOLING,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid cooling schedule - final_temperature > initial_temperature"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_QUANTUM_COOLING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Annealer Exception Tests
//=============================================================================

TEST_F(QuantumExceptionHandlingTest, AnnealerNullPointerException) {
    // WHAT: Test exception for NULL annealer parameter
    // WHY:  Verify proper error handling for invalid inputs

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_QUANTUM_ANNEALER,
        EXCEPTION_SEVERITY_ERROR,
        "Quantum annealer pointer is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    // Register handler and dispatch
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "quantum_null_handler";
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

TEST_F(QuantumExceptionHandlingTest, AnnealerConfigException) {
    // WHAT: Test exception for invalid annealer configuration
    // WHY:  Configuration validation is critical

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_CONFIG_INVALID,
        EXCEPTION_CATEGORY_QUANTUM_ANNEALER,
        EXCEPTION_SEVERITY_ERROR,
        "Quantum annealer config invalid - num_iterations must be > 0"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_CONFIG_INVALID);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_QUANTUM_ANNEALER);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, AnnealerMemoryException) {
    // WHAT: Test exception for memory allocation failure
    // WHY:  Annealer needs memory for state vectors

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_CATEGORY_QUANTUM_ANNEALER,
        EXCEPTION_SEVERITY_SEVERE,
        "Failed to allocate memory for quantum state vector"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_SEVERE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Temperature/Cooling Exception Tests
//=============================================================================

TEST_F(QuantumExceptionHandlingTest, TemperatureRangeException) {
    // WHAT: Test exception for invalid temperature range
    // WHY:  Temperature must be positive

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_QUANTUM_COOLING,
        EXCEPTION_SEVERITY_ERROR,
        "Temperature out of range - must be positive (got: -0.5)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_QUANTUM_COOLING);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, CoolingExponentialException) {
    // WHAT: Test exception for exponential cooling issues
    // WHY:  Exponential cooling may underflow

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_QUANTUM_COOLING,
        EXCEPTION_SEVERITY_WARNING,
        "Exponential cooling underflow - temperature approaching zero"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, AdaptiveCoolingException) {
    // WHAT: Test exception for adaptive cooling failure
    // WHY:  Adaptive cooling adjusts based on acceptance rate

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_QUANTUM_COOLING,
        EXCEPTION_SEVERITY_WARNING,
        "Adaptive cooling failed - acceptance rate is zero"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Energy Function Exception Tests
//=============================================================================

TEST_F(QuantumExceptionHandlingTest, EnergyFunctionNullException) {
    // WHAT: Test exception for NULL energy function
    // WHY:  Energy function is required for annealing

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_QUANTUM_ENERGY,
        EXCEPTION_SEVERITY_ERROR,
        "Energy function pointer is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_QUANTUM_ENERGY);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, EnergyFunctionNaNException) {
    // WHAT: Test exception for NaN energy value
    // WHY:  NaN indicates numerical instability

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_QUANTUM_ENERGY,
        EXCEPTION_SEVERITY_ERROR,
        "Energy function returned NaN"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, EnergyFunctionInfinityException) {
    // WHAT: Test exception for infinite energy value
    // WHY:  Infinity can indicate invalid state

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_QUANTUM_ENERGY,
        EXCEPTION_SEVERITY_WARNING,
        "Energy function returned infinity - state may be invalid"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Quantum Tunneling Exception Tests
//=============================================================================

TEST_F(QuantumExceptionHandlingTest, TunnelingStrengthException) {
    // WHAT: Test exception for invalid tunneling strength
    // WHY:  Quantum strength must be in [0, 1]

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_QUANTUM_TUNNELING,
        EXCEPTION_SEVERITY_ERROR,
        "Quantum tunneling strength must be in [0, 1] (got: 1.5)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_QUANTUM_TUNNELING);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, TunnelingDisabledException) {
    // WHAT: Test exception when tunneling is disabled but expected
    // WHY:  Some algorithms require tunneling

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_CATEGORY_QUANTUM_TUNNELING,
        EXCEPTION_SEVERITY_WARNING,
        "Quantum tunneling is disabled - pure simulated annealing mode"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_STATE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Monte Carlo Exception Tests
//=============================================================================

TEST_F(QuantumExceptionHandlingTest, MCConvergenceException) {
    // WHAT: Test exception for Monte Carlo convergence failure
    // WHY:  MC sampling may fail to converge

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_QUANTUM_MC,
        EXCEPTION_SEVERITY_WARNING,
        "Monte Carlo sampling failed to converge after max iterations"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_QUANTUM_MC);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, MCVarianceException) {
    // WHAT: Test exception for excessive MC variance
    // WHY:  High variance indicates poor sampling

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_QUANTUM_MC,
        EXCEPTION_SEVERITY_WARNING,
        "Monte Carlo variance exceeds threshold - more samples needed"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, MCAcceptanceRateException) {
    // WHAT: Test exception for abnormal acceptance rate
    // WHY:  Acceptance rate indicates optimization health

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_QUANTUM_MC,
        EXCEPTION_SEVERITY_WARNING,
        "Monte Carlo acceptance rate is 0% - temperature may be too low"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Partition Function Exception Tests
//=============================================================================

TEST_F(QuantumExceptionHandlingTest, PartitionFunctionOverflowException) {
    // WHAT: Test exception for partition function overflow
    // WHY:  Z = sum(exp(-E/T)) may overflow

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_QUANTUM_PARTITION,
        EXCEPTION_SEVERITY_ERROR,
        "Partition function overflow - temperature too low"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_QUANTUM_PARTITION);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, PartitionFunctionUnderflowException) {
    // WHAT: Test exception for partition function underflow
    // WHY:  Z may underflow with high temperature

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_QUANTUM_PARTITION,
        EXCEPTION_SEVERITY_WARNING,
        "Partition function underflow - all Boltzmann weights near zero"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Numerical Exception Tests
//=============================================================================

TEST_F(QuantumExceptionHandlingTest, NumericalInstabilityException) {
    // WHAT: Test exception for numerical instability
    // WHY:  Quantum annealing involves exponentials that may be unstable

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_QUANTUM_NUMERICAL,
        EXCEPTION_SEVERITY_ERROR,
        "Numerical instability detected in Metropolis criterion"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_QUANTUM_NUMERICAL);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, DimensionZeroException) {
    // WHAT: Test exception for zero dimensionality
    // WHY:  State dimension must be positive

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_QUANTUM_ANNEALER,
        EXCEPTION_SEVERITY_ERROR,
        "State dimensionality must be > 0 (got: 0)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Handler Chain Tests
//=============================================================================

TEST_F(QuantumExceptionHandlingTest, HandlerChainDispatch) {
    // WHAT: Test exception dispatch through multiple handlers
    // WHY:  Verify chain processing works correctly

    // Register multiple handlers
    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    options1.name = "quantum_handler_1";
    options1.handler = test_exception_handler;
    options1.priority = 100;

    options2.name = "quantum_handler_2";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_QUANTUM_ANNEALER,
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

TEST_F(QuantumExceptionHandlingTest, HandlerConsumesException) {
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

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_QUANTUM_ANNEALER,
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

TEST_F(QuantumExceptionHandlingTest, AnnealerExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for annealer exceptions
    // WHY:  Annealer failures may need restart with different config

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_QUANTUM_ANNEALER,
        EXCEPTION_SEVERITY_ERROR,
        "Quantum annealer optimization failed"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Annealer exceptions should have some recovery action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, MemoryExceptionRecoveryStrategy) {
    // WHAT: Test recovery for memory allocation failures
    // WHY:  Memory failures may trigger GC or compaction

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_CATEGORY_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "Failed to allocate memory for quantum state"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_SEVERE);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Memory errors should suggest GC or retry
    EXPECT_TRUE(strategy.primary_action != EXCEPTION_RECOVERY_NONE ||
                strategy.fallback_action != EXCEPTION_RECOVERY_NONE ||
                strategy.retry_count > 0);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, CriticalNumericalExceptionRecovery) {
    // WHAT: Test recovery for critical numerical failures
    // WHY:  Critical numerical issues may require emergency handling

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_QUANTUM_NUMERICAL,
        EXCEPTION_SEVERITY_CRITICAL,
        "Critical numerical overflow in quantum annealing"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Critical errors should trigger some recovery
    EXPECT_TRUE(strategy.primary_action != EXCEPTION_RECOVERY_NONE ||
                strategy.fallback_action != EXCEPTION_RECOVERY_NONE ||
                strategy.retry_count > 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Exception Statistics Tests
//=============================================================================

TEST_F(QuantumExceptionHandlingTest, ExceptionStatisticsTracking) {
    // WHAT: Test that exception dispatch is tracked by handlers
    // WHY:  Need to monitor quantum exception frequency

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
    opts.name = "quantum_stats_counter";
    opts.handler = counting_handler;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Create and dispatch several exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = create_quantum_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_CATEGORY_QUANTUM_ANNEALER,
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

TEST_F(QuantumExceptionHandlingTest, ConcurrentExceptionCreation) {
    // WHAT: Test concurrent exception creation
    // WHY:  Quantum operations may run in parallel

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
// Category Classification Tests
//=============================================================================

TEST_F(QuantumExceptionHandlingTest, CategoryClassificationFromCode) {
    // WHAT: Test automatic category classification from error codes
    // WHY:  Verify error codes map to correct categories

    // Generic error
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Operation failed"
    );
    ASSERT_NE(ex1, nullptr);
    EXPECT_EQ(ex1->category, EXCEPTION_CATEGORY_GENERIC);
    nimcp_exception_unref(ex1);

    // Memory error
    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Memory allocation error"
    );
    ASSERT_NE(ex2, nullptr);
    EXPECT_EQ(ex2->category, EXCEPTION_CATEGORY_MEMORY);
    nimcp_exception_unref(ex2);

    // Configuration error
    nimcp_exception_t* ex3 = nimcp_exception_create(
        NIMCP_ERROR_CONFIG_INVALID,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Configuration error"
    );
    ASSERT_NE(ex3, nullptr);
    EXPECT_EQ(ex3->category, EXCEPTION_CATEGORY_CONFIG);
    nimcp_exception_unref(ex3);
}

//=============================================================================
// Boltzmann Sampling Exception Tests
//=============================================================================

TEST_F(QuantumExceptionHandlingTest, BoltzmannSamplingException) {
    // WHAT: Test exception for Boltzmann sampling failure
    // WHY:  Importance sampling may fail with extreme weights

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_QUANTUM_MC,
        EXCEPTION_SEVERITY_WARNING,
        "Boltzmann sampling failed - all weights are zero"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_QUANTUM_MC);

    nimcp_exception_unref(ex);
}

TEST_F(QuantumExceptionHandlingTest, BoltzmannTemperatureException) {
    // WHAT: Test exception for invalid Boltzmann temperature
    // WHY:  Temperature affects Boltzmann distribution

    nimcp_exception_t* ex = create_quantum_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_QUANTUM_MC,
        EXCEPTION_SEVERITY_ERROR,
        "Boltzmann sampling temperature must be positive"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
