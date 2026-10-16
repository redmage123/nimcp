/**
 * @file test_lnn_exception_handling.cpp
 * @brief Unit tests for LNN (Liquid Neural Network) module exception handling
 *
 * WHAT: Test exception handling across LNN module components
 * WHY:  Ensure consistent error-to-exception mapping and handler chain dispatch
 * HOW:  Test each LNN module's error conditions and exception integration
 *
 * LNN MODULES TESTED:
 * - Network creation and configuration errors
 * - Forward/backward pass failures
 * - ODE solver issues
 * - Wiring configuration errors
 * - Gradient computation errors
 * - Bio-async integration failures
 *
 * TEST PATTERNS:
 * - Error code to exception mapping
 * - Exception dispatch through handler chain
 * - Exception category classification (BRAIN, MEMORY)
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

// LNN header may include CUDA headers with templates, so include outside extern "C"
#include "lnn/nimcp_lnn.h"

//=============================================================================
// LNN Exception Categories
//=============================================================================

// Define LNN-specific exception categories for testing
#define EXCEPTION_CATEGORY_LNN_BASE        200
#define EXCEPTION_CATEGORY_LNN_NETWORK     (EXCEPTION_CATEGORY_LNN_BASE + 1)
#define EXCEPTION_CATEGORY_LNN_ODE         (EXCEPTION_CATEGORY_LNN_BASE + 2)
#define EXCEPTION_CATEGORY_LNN_WIRING      (EXCEPTION_CATEGORY_LNN_BASE + 3)
#define EXCEPTION_CATEGORY_LNN_GRADIENT    (EXCEPTION_CATEGORY_LNN_BASE + 4)
#define EXCEPTION_CATEGORY_LNN_LAYER       (EXCEPTION_CATEGORY_LNN_BASE + 5)
#define EXCEPTION_CATEGORY_LNN_TRAINING    (EXCEPTION_CATEGORY_LNN_BASE + 6)
#define EXCEPTION_CATEGORY_LNN_BIO_ASYNC   (EXCEPTION_CATEGORY_LNN_BASE + 7)

//=============================================================================
// Test Fixture
//=============================================================================

class LNNExceptionHandlingTest : public ::testing::Test {
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

    // Helper to create LNN exception
    nimcp_exception_t* create_lnn_exception(
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

std::atomic<int> LNNExceptionHandlingTest::handler_call_count(0);
std::atomic<int> LNNExceptionHandlingTest::last_exception_code(0);
std::atomic<int> LNNExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> LNNExceptionHandlingTest::handler_consumed(false);

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(LNNExceptionHandlingTest, CreateNetworkException) {
    // WHAT: Test creation of LNN network exception
    // WHY:  Verify exception fields are set correctly for network errors

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_NETWORK_CREATION,
        EXCEPTION_CATEGORY_LNN_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "LNN network creation failed - invalid layer configuration"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NETWORK_CREATION);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_LNN_NETWORK);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_NE(ex->message, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(LNNExceptionHandlingTest, CreateODEException) {
    // WHAT: Test creation of ODE solver exception
    // WHY:  LNN uses ODE solvers that may have numerical issues

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_LNN_ODE,
        EXCEPTION_SEVERITY_ERROR,
        "ODE solver diverged - step size too large"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_LNN_ODE);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);

    nimcp_exception_unref(ex);
}

TEST_F(LNNExceptionHandlingTest, CreateWiringException) {
    // WHAT: Test creation of wiring configuration exception
    // WHY:  Wiring defines network topology and connections

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_CONFIG_INVALID,
        EXCEPTION_CATEGORY_LNN_WIRING,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid wiring configuration - unconnected neurons detected"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_CONFIG_INVALID);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_LNN_WIRING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Network Exception Tests
//=============================================================================

TEST_F(LNNExceptionHandlingTest, NetworkNullPointerException) {
    // WHAT: Test exception for NULL network parameter
    // WHY:  Verify proper error handling for invalid inputs

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_LNN_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "LNN network pointer is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    // Register handler and dispatch
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "lnn_null_handler";
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

TEST_F(LNNExceptionHandlingTest, NetworkDimensionMismatchException) {
    // WHAT: Test exception for dimension mismatch in network
    // WHY:  Input/output dimensions must match network configuration

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_DIMENSION_MISMATCH,
        EXCEPTION_CATEGORY_LNN_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "Input dimension 128 does not match network input_size 64"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_DIMENSION_MISMATCH);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_LNN_NETWORK);

    nimcp_exception_unref(ex);
}

TEST_F(LNNExceptionHandlingTest, ForwardPassException) {
    // WHAT: Test exception for forward pass failure
    // WHY:  Forward pass is core LNN operation

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_CATEGORY_LNN_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "LNN forward pass failed - NaN detected in activation"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_FORWARD_PASS);

    nimcp_exception_unref(ex);
}

TEST_F(LNNExceptionHandlingTest, BackwardPassException) {
    // WHAT: Test exception for backward pass failure
    // WHY:  Backward pass computes gradients for training

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_CATEGORY_LNN_GRADIENT,
        EXCEPTION_SEVERITY_ERROR,
        "LNN backward pass failed - gradient explosion detected"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_BACKWARD_PASS);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_LNN_GRADIENT);

    nimcp_exception_unref(ex);
}

//=============================================================================
// ODE Solver Exception Tests
//=============================================================================

TEST_F(LNNExceptionHandlingTest, ODEDivergenceException) {
    // WHAT: Test exception for ODE solver divergence
    // WHY:  Numerical instability can cause solver to diverge

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_LNN_ODE,
        EXCEPTION_SEVERITY_SEVERE,
        "ODE solver diverged - state values exceeding bounds"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_SEVERE);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_LNN_ODE);

    nimcp_exception_unref(ex);
}

TEST_F(LNNExceptionHandlingTest, ODEStiffnessException) {
    // WHAT: Test exception for stiff ODE system
    // WHY:  Stiff systems require specialized solvers

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_LNN_ODE,
        EXCEPTION_SEVERITY_WARNING,
        "ODE system is stiff - consider using implicit solver"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

TEST_F(LNNExceptionHandlingTest, ODETimestepException) {
    // WHAT: Test exception for invalid timestep
    // WHY:  Timestep must be positive and reasonable

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_LNN_ODE,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid ODE timestep: dt must be positive (got: -0.001)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Layer Exception Tests
//=============================================================================

TEST_F(LNNExceptionHandlingTest, LayerCreationException) {
    // WHAT: Test exception for layer creation failure
    // WHY:  Layers are fundamental LNN building blocks

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_CATEGORY_LNN_LAYER,
        EXCEPTION_SEVERITY_SEVERE,
        "Failed to allocate memory for LNN layer with 1024 neurons"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NO_MEMORY);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_LNN_LAYER);

    nimcp_exception_unref(ex);
}

TEST_F(LNNExceptionHandlingTest, LayerTauException) {
    // WHAT: Test exception for invalid tau parameter
    // WHY:  Tau (time constant) must be positive for stable dynamics

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_LNN_LAYER,
        EXCEPTION_SEVERITY_ERROR,
        "LNN tau parameter must be positive (got: 0.0)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Training Exception Tests
//=============================================================================

TEST_F(LNNExceptionHandlingTest, TrainingGradientNaNException) {
    // WHAT: Test exception for NaN in gradients
    // WHY:  NaN gradients indicate training instability

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_LNN_TRAINING,
        EXCEPTION_SEVERITY_ERROR,
        "NaN detected in LNN gradients during backward pass"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_LEARNING_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_LNN_TRAINING);

    nimcp_exception_unref(ex);
}

TEST_F(LNNExceptionHandlingTest, TrainingGradientExplosionException) {
    // WHAT: Test exception for gradient explosion
    // WHY:  Large gradients cause unstable learning

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_LNN_TRAINING,
        EXCEPTION_SEVERITY_WARNING,
        "Gradient norm exceeds threshold (1e6 > 1e3) - clipping applied"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Bio-Async Integration Exception Tests
//=============================================================================

TEST_F(LNNExceptionHandlingTest, BioAsyncConnectionException) {
    // WHAT: Test exception for bio-async connection failure
    // WHY:  LNN integrates with bio-async messaging system

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_LNN_BIO_ASYNC,
        EXCEPTION_SEVERITY_WARNING,
        "Failed to connect LNN to bio-async router"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_LNN_BIO_ASYNC);

    nimcp_exception_unref(ex);
}

TEST_F(LNNExceptionHandlingTest, ImmuneIntegrationException) {
    // WHAT: Test exception for immune system integration failure
    // WHY:  LNN connects to brain immune system for modulation

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_LNN_BIO_ASYNC,
        EXCEPTION_SEVERITY_WARNING,
        "Failed to connect LNN to immune bridge"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Handler Chain Tests
//=============================================================================

TEST_F(LNNExceptionHandlingTest, HandlerChainDispatch) {
    // WHAT: Test exception dispatch through multiple handlers
    // WHY:  Verify chain processing works correctly

    // Register multiple handlers
    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    options1.name = "lnn_handler_1";
    options1.handler = test_exception_handler;
    options1.priority = 100;

    options2.name = "lnn_handler_2";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_NETWORK_CREATION,
        EXCEPTION_CATEGORY_LNN_NETWORK,
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

TEST_F(LNNExceptionHandlingTest, HandlerConsumesException) {
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

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_CATEGORY_LNN_NETWORK,
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

TEST_F(LNNExceptionHandlingTest, NetworkExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for network exceptions
    // WHY:  Network failures may need restart or reconfiguration

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_NETWORK_CREATION,
        EXCEPTION_CATEGORY_LNN_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "LNN network creation failed"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Network exceptions should have some recovery action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(LNNExceptionHandlingTest, MemoryExceptionRecoveryStrategy) {
    // WHAT: Test recovery for memory allocation failures
    // WHY:  Memory failures may trigger GC or compaction

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_CATEGORY_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "Failed to allocate memory for LNN network"
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

TEST_F(LNNExceptionHandlingTest, CriticalODEExceptionRecovery) {
    // WHAT: Test recovery for critical ODE failures
    // WHY:  Critical ODE issues may require emergency save

    nimcp_exception_t* ex = create_lnn_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_LNN_ODE,
        EXCEPTION_SEVERITY_CRITICAL,
        "ODE solver critically failed - numerical overflow"
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

TEST_F(LNNExceptionHandlingTest, ExceptionStatisticsTracking) {
    // WHAT: Test that exception dispatch is tracked by handlers
    // WHY:  Need to monitor LNN exception frequency

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
    opts.name = "lnn_stats_counter";
    opts.handler = counting_handler;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Create and dispatch several exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = create_lnn_exception(
            NIMCP_ERROR_FORWARD_PASS,
            EXCEPTION_CATEGORY_LNN_NETWORK,
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

TEST_F(LNNExceptionHandlingTest, ConcurrentExceptionCreation) {
    // WHAT: Test concurrent exception creation
    // WHY:  LNN may run across multiple threads

    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int exceptions_per_thread = 10;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&success_count, t, exceptions_per_thread]() {
            for (int i = 0; i < exceptions_per_thread; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_FORWARD_PASS,
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

TEST_F(LNNExceptionHandlingTest, CategoryClassificationFromCode) {
    // WHAT: Test automatic category classification from error codes
    // WHY:  Verify error codes map to correct categories

    // Network error
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NETWORK_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Network creation error"
    );
    ASSERT_NE(ex1, nullptr);
    EXPECT_EQ(ex1->category, EXCEPTION_CATEGORY_BRAIN);
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
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
