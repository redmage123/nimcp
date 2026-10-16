/**
 * @file test_snn_exception_handling.cpp
 * @brief Unit tests for SNN (Spiking Neural Network) module exception handling
 *
 * WHAT: Test exception handling across SNN module components
 * WHY:  Ensure consistent error-to-exception mapping and handler chain dispatch
 * HOW:  Test each SNN module's error conditions and exception integration
 *
 * SNN MODULES TESTED:
 * - Network creation and configuration errors
 * - Spike encoding/decoding failures
 * - Training algorithm errors
 * - Bridge integration failures
 * - Bio-async integration failures
 * - Immune system integration errors
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

// Module header may include CUDA headers with templates, so include outside extern "C"
#include "snn/nimcp_snn.h"

//=============================================================================
// SNN Exception Categories
//=============================================================================

// Define SNN-specific exception categories for testing
#define EXCEPTION_CATEGORY_SNN_BASE        300
#define EXCEPTION_CATEGORY_SNN_NETWORK     (EXCEPTION_CATEGORY_SNN_BASE + 1)
#define EXCEPTION_CATEGORY_SNN_ENCODING    (EXCEPTION_CATEGORY_SNN_BASE + 2)
#define EXCEPTION_CATEGORY_SNN_TRAINING    (EXCEPTION_CATEGORY_SNN_BASE + 3)
#define EXCEPTION_CATEGORY_SNN_BRIDGE      (EXCEPTION_CATEGORY_SNN_BASE + 4)
#define EXCEPTION_CATEGORY_SNN_SPIKE       (EXCEPTION_CATEGORY_SNN_BASE + 5)
#define EXCEPTION_CATEGORY_SNN_POPULATION  (EXCEPTION_CATEGORY_SNN_BASE + 6)
#define EXCEPTION_CATEGORY_SNN_BIO_ASYNC   (EXCEPTION_CATEGORY_SNN_BASE + 7)
#define EXCEPTION_CATEGORY_SNN_IMMUNE      (EXCEPTION_CATEGORY_SNN_BASE + 8)

//=============================================================================
// Test Fixture
//=============================================================================

class SNNExceptionHandlingTest : public ::testing::Test {
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

    // Helper to create SNN exception
    nimcp_exception_t* create_snn_exception(
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

std::atomic<int> SNNExceptionHandlingTest::handler_call_count(0);
std::atomic<int> SNNExceptionHandlingTest::last_exception_code(0);
std::atomic<int> SNNExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> SNNExceptionHandlingTest::handler_consumed(false);

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(SNNExceptionHandlingTest, CreateNetworkException) {
    // WHAT: Test creation of SNN network exception
    // WHY:  Verify exception fields are set correctly for network errors

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_NETWORK_CREATION,
        EXCEPTION_CATEGORY_SNN_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "SNN network creation failed - invalid neuron configuration"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NETWORK_CREATION);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SNN_NETWORK);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_NE(ex->message, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, CreateEncodingException) {
    // WHAT: Test creation of spike encoding exception
    // WHY:  SNN uses spike encoding that may have parameter issues

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SNN_ENCODING,
        EXCEPTION_SEVERITY_ERROR,
        "Spike encoding failed - invalid rate parameter"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SNN_ENCODING);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, CreateTrainingException) {
    // WHAT: Test creation of training exception
    // WHY:  SNN training (STDP, etc.) may encounter errors

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_SNN_TRAINING,
        EXCEPTION_SEVERITY_ERROR,
        "SNN training failed - STDP update diverged"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_LEARNING_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SNN_TRAINING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Network Exception Tests
//=============================================================================

TEST_F(SNNExceptionHandlingTest, NetworkNullPointerException) {
    // WHAT: Test exception for NULL network parameter
    // WHY:  Verify proper error handling for invalid inputs

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_SNN_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "SNN network pointer is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    // Register handler and dispatch
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "snn_null_handler";
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

TEST_F(SNNExceptionHandlingTest, NetworkConfigException) {
    // WHAT: Test exception for invalid network configuration
    // WHY:  Configuration errors should be caught early

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_CONFIG_INVALID,
        EXCEPTION_CATEGORY_SNN_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "SNN configuration invalid - neuron count must be > 0"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_CONFIG_INVALID);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SNN_NETWORK);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, ForwardPassException) {
    // WHAT: Test exception for forward pass failure
    // WHY:  Forward pass is core SNN operation

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_CATEGORY_SNN_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "SNN forward pass failed - spike buffer overflow"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_FORWARD_PASS);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, DimensionMismatchException) {
    // WHAT: Test exception for dimension mismatch
    // WHY:  Input dimensions must match network configuration

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_DIMENSION_MISMATCH,
        EXCEPTION_CATEGORY_SNN_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "Input dimension 784 does not match expected 256"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_DIMENSION_MISMATCH);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Spike Encoding Exception Tests
//=============================================================================

TEST_F(SNNExceptionHandlingTest, RateEncodingException) {
    // WHAT: Test exception for rate encoding failure
    // WHY:  Rate encoding converts values to spike rates

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_SNN_ENCODING,
        EXCEPTION_SEVERITY_ERROR,
        "Rate encoding failed - max_rate must be positive"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SNN_ENCODING);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, TemporalEncodingException) {
    // WHAT: Test exception for temporal encoding failure
    // WHY:  Temporal encoding uses spike timing

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_SNN_ENCODING,
        EXCEPTION_SEVERITY_WARNING,
        "Temporal encoding: value exceeds encoding range [0, 1]"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, PopulationEncodingException) {
    // WHAT: Test exception for population encoding failure
    // WHY:  Population encoding uses multiple neurons

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_SNN_ENCODING,
        EXCEPTION_SEVERITY_ERROR,
        "Population encoding failed - population size must be >= 2"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Training Exception Tests
//=============================================================================

TEST_F(SNNExceptionHandlingTest, STDPTrainingException) {
    // WHAT: Test exception for STDP training failure
    // WHY:  STDP is primary SNN learning rule

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_SNN_TRAINING,
        EXCEPTION_SEVERITY_ERROR,
        "STDP learning failed - timing window exceeded"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_LEARNING_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SNN_TRAINING);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, SurrogateGradientException) {
    // WHAT: Test exception for surrogate gradient failure
    // WHY:  Surrogate gradients enable backprop through spikes

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_BACKWARD_PASS,
        EXCEPTION_CATEGORY_SNN_TRAINING,
        EXCEPTION_SEVERITY_ERROR,
        "Surrogate gradient computation failed - gradient explosion"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_BACKWARD_PASS);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, WeightBoundsException) {
    // WHAT: Test exception for weight out of bounds
    // WHY:  Weights should stay within valid range

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_SNN_TRAINING,
        EXCEPTION_SEVERITY_WARNING,
        "SNN weight exceeds bounds (2.5 > 1.0) - clipping applied"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Bridge Exception Tests
//=============================================================================

TEST_F(SNNExceptionHandlingTest, BridgeConnectionException) {
    // WHAT: Test exception for bridge connection failure
    // WHY:  SNN has many bridges to other modules

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SNN_BRIDGE,
        EXCEPTION_SEVERITY_WARNING,
        "SNN attention bridge connection failed"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SNN_BRIDGE);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, BridgeSyncException) {
    // WHAT: Test exception for bridge synchronization failure
    // WHY:  Bridges must synchronize data between modules

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_THREAD_SYNC,
        EXCEPTION_CATEGORY_SNN_BRIDGE,
        EXCEPTION_SEVERITY_ERROR,
        "SNN bridge synchronization timeout"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_THREAD_SYNC);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, HippocampusBridgeException) {
    // WHAT: Test exception for hippocampus bridge failure
    // WHY:  Hippocampus bridge handles memory consolidation

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SNN_BRIDGE,
        EXCEPTION_SEVERITY_ERROR,
        "SNN-Hippocampus bridge: memory encoding failed"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Bio-Async Integration Exception Tests
//=============================================================================

TEST_F(SNNExceptionHandlingTest, BioAsyncConnectionException) {
    // WHAT: Test exception for bio-async connection failure
    // WHY:  SNN integrates with bio-async messaging system

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SNN_BIO_ASYNC,
        EXCEPTION_SEVERITY_WARNING,
        "Failed to connect SNN to bio-async router"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SNN_BIO_ASYNC);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, BioAsyncMessageException) {
    // WHAT: Test exception for bio-async message failure
    // WHY:  Messages may fail to send/receive

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_CATEGORY_SNN_BIO_ASYNC,
        EXCEPTION_SEVERITY_WARNING,
        "Bio-async message timeout - no response from target"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_TIMEOUT);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Immune System Integration Exception Tests
//=============================================================================

TEST_F(SNNExceptionHandlingTest, ImmuneConnectionException) {
    // WHAT: Test exception for immune system connection failure
    // WHY:  SNN connects to brain immune system

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SNN_IMMUNE,
        EXCEPTION_SEVERITY_WARNING,
        "Failed to connect SNN to immune system"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SNN_IMMUNE);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, ImmuneModulationException) {
    // WHAT: Test exception for immune modulation failure
    // WHY:  Immune system modulates learning

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SNN_IMMUNE,
        EXCEPTION_SEVERITY_WARNING,
        "Immune modulation failed - invalid inflammatory state"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Handler Chain Tests
//=============================================================================

TEST_F(SNNExceptionHandlingTest, HandlerChainDispatch) {
    // WHAT: Test exception dispatch through multiple handlers
    // WHY:  Verify chain processing works correctly

    // Register multiple handlers
    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    options1.name = "snn_handler_1";
    options1.handler = test_exception_handler;
    options1.priority = 100;

    options2.name = "snn_handler_2";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_NETWORK_CREATION,
        EXCEPTION_CATEGORY_SNN_NETWORK,
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

TEST_F(SNNExceptionHandlingTest, HandlerConsumesException) {
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

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_CATEGORY_SNN_NETWORK,
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

TEST_F(SNNExceptionHandlingTest, NetworkExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for network exceptions
    // WHY:  Network failures may need restart or reconfiguration

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_NETWORK_CREATION,
        EXCEPTION_CATEGORY_SNN_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "SNN network creation failed"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Network exceptions should have some recovery action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, MemoryExceptionRecoveryStrategy) {
    // WHAT: Test recovery for memory allocation failures
    // WHY:  Memory failures may trigger GC or compaction

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_CATEGORY_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "Failed to allocate memory for SNN spike buffer"
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

TEST_F(SNNExceptionHandlingTest, CriticalTrainingExceptionRecovery) {
    // WHAT: Test recovery for critical training failures
    // WHY:  Critical training issues may require checkpoint rollback

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_SNN_TRAINING,
        EXCEPTION_SEVERITY_CRITICAL,
        "SNN training critically diverged - all weights NaN"
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

TEST_F(SNNExceptionHandlingTest, ExceptionStatisticsTracking) {
    // WHAT: Test that exception dispatch is tracked by handlers
    // WHY:  Need to monitor SNN exception frequency

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
    opts.name = "snn_stats_counter";
    opts.handler = counting_handler;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Create and dispatch several exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = create_snn_exception(
            NIMCP_ERROR_FORWARD_PASS,
            EXCEPTION_CATEGORY_SNN_NETWORK,
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

TEST_F(SNNExceptionHandlingTest, ConcurrentExceptionCreation) {
    // WHAT: Test concurrent exception creation
    // WHY:  SNN may run across multiple threads

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

TEST_F(SNNExceptionHandlingTest, CategoryClassificationFromCode) {
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

    // Threading error
    nimcp_exception_t* ex3 = nimcp_exception_create(
        NIMCP_ERROR_THREAD_SYNC,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Thread sync error"
    );
    ASSERT_NE(ex3, nullptr);
    EXPECT_EQ(ex3->category, EXCEPTION_CATEGORY_THREADING);
    nimcp_exception_unref(ex3);
}

//=============================================================================
// Spike-specific Exception Tests
//=============================================================================

TEST_F(SNNExceptionHandlingTest, SpikeBufferOverflowException) {
    // WHAT: Test exception for spike buffer overflow
    // WHY:  Spike buffers have limited capacity

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_BUFFER_OVERFLOW,
        EXCEPTION_CATEGORY_SNN_SPIKE,
        EXCEPTION_SEVERITY_ERROR,
        "Spike buffer overflow - exceeded maximum spike count"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_BUFFER_OVERFLOW);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SNN_SPIKE);

    nimcp_exception_unref(ex);
}

TEST_F(SNNExceptionHandlingTest, SpikeTimingException) {
    // WHAT: Test exception for invalid spike timing
    // WHY:  Spike times must be valid

    nimcp_exception_t* ex = create_snn_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_SNN_SPIKE,
        EXCEPTION_SEVERITY_WARNING,
        "Spike time is negative - invalid timing"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
