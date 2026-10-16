/**
 * @file test_distributed_exception_handling.cpp
 * @brief Unit tests for distributed module exception handling
 *
 * WHAT: Test exception handling across all distributed cognition modules
 * WHY:  Ensure consistent error-to-exception mapping and handler chain dispatch
 * HOW:  Test each distributed module's error conditions and exception integration
 *
 * DISTRIBUTED MODULES TESTED:
 * - Distributed Cognition (main coordinator)
 * - P2P Networking (peer-to-peer communication)
 * - Neuromodulator Sync (chemical state sharing)
 * - Glial Coordination (microglia/astrocyte sync)
 * - Brain Region Sync (regional state sharing)
 * - Distributed Training (federated learning)
 *
 * TEST PATTERNS:
 * - Error code to exception mapping
 * - Exception dispatch through handler chain
 * - Exception category classification (DISTRIBUTED, NETWORK, SYNC)
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
#include "networking/distributed/nimcp_distributed_cognition.h"
}

//=============================================================================
// Distributed Exception Categories
//=============================================================================

// Define distributed-specific exception categories for testing
#define EXCEPTION_CATEGORY_DISTRIBUTED_BASE     300
#define EXCEPTION_CATEGORY_DISTRIBUTED_COGNITION (EXCEPTION_CATEGORY_DISTRIBUTED_BASE + 1)
#define EXCEPTION_CATEGORY_P2P_NETWORK          (EXCEPTION_CATEGORY_DISTRIBUTED_BASE + 2)
#define EXCEPTION_CATEGORY_NEUROMOD_SYNC        (EXCEPTION_CATEGORY_DISTRIBUTED_BASE + 3)
#define EXCEPTION_CATEGORY_GLIAL_SYNC           (EXCEPTION_CATEGORY_DISTRIBUTED_BASE + 4)
#define EXCEPTION_CATEGORY_REGION_SYNC          (EXCEPTION_CATEGORY_DISTRIBUTED_BASE + 5)
#define EXCEPTION_CATEGORY_DISTRIBUTED_TRAINING (EXCEPTION_CATEGORY_DISTRIBUTED_BASE + 6)
#define EXCEPTION_CATEGORY_PRUNING_COORD        (EXCEPTION_CATEGORY_DISTRIBUTED_BASE + 7)
#define EXCEPTION_CATEGORY_CALCIUM_WAVE         (EXCEPTION_CATEGORY_DISTRIBUTED_BASE + 8)

//=============================================================================
// Test Fixture
//=============================================================================

class DistributedExceptionHandlingTest : public ::testing::Test {
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

    // Helper to create distributed exception
    nimcp_exception_t* create_distributed_exception(
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

std::atomic<int> DistributedExceptionHandlingTest::handler_call_count(0);
std::atomic<int> DistributedExceptionHandlingTest::last_exception_code(0);
std::atomic<int> DistributedExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> DistributedExceptionHandlingTest::handler_consumed(false);

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(DistributedExceptionHandlingTest, CreateDistributedCognitionException) {
    // WHAT: Test creation of distributed cognition-related exception
    // WHY:  Verify exception fields are set correctly

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_DISTRIBUTED_COGNITION,
        EXCEPTION_SEVERITY_ERROR,
        "Distributed cognition coordinator failed to start"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_DISTRIBUTED_COGNITION);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_NE(ex->message, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(DistributedExceptionHandlingTest, CreateP2PNetworkException) {
    // WHAT: Test creation of P2P network-related exception
    // WHY:  Network errors need proper categorization

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_CATEGORY_P2P_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "P2P node failed to connect to peer"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NETWORK_IO);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_P2P_NETWORK);

    nimcp_exception_unref(ex);
}

TEST_F(DistributedExceptionHandlingTest, CreateNeuromodSyncException) {
    // WHAT: Test creation of neuromodulator sync exception
    // WHY:  Sync failures need specialized handling

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_CATEGORY_NEUROMOD_SYNC,
        EXCEPTION_SEVERITY_WARNING,
        "Neuromodulator broadcast timed out - peers unreachable"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_TIMEOUT);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_NEUROMOD_SYNC);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Distributed Cognition Exception Tests
//=============================================================================

TEST_F(DistributedExceptionHandlingTest, DistributedCognitionNullConfigException) {
    // WHAT: Test exception for NULL config parameter
    // WHY:  Verify proper error handling for invalid inputs

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_DISTRIBUTED_COGNITION,
        EXCEPTION_SEVERITY_ERROR,
        "Distributed cognition config is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    // Register handler and dispatch
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "distributed_null_handler";
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

TEST_F(DistributedExceptionHandlingTest, DistributedCognitionSyncModeException) {
    // WHAT: Test exception for invalid sync mode
    // WHY:  Sync mode must be valid

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_DISTRIBUTED_COGNITION,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid sync mode specified"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

TEST_F(DistributedExceptionHandlingTest, DistributedCognitionNotConnectedException) {
    // WHAT: Test exception for not connected state
    // WHY:  Operations require P2P connection

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_CATEGORY_DISTRIBUTED_COGNITION,
        EXCEPTION_SEVERITY_ERROR,
        "Distributed cognition not connected to P2P network"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_STATE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// P2P Network Exception Tests
//=============================================================================

TEST_F(DistributedExceptionHandlingTest, P2PConnectionRefusedException) {
    // WHAT: Test exception for connection refused
    // WHY:  Network connection may be refused

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_CATEGORY_P2P_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "P2P connection refused by peer"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NETWORK_IO);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_P2P_NETWORK);

    nimcp_exception_unref(ex);
}

TEST_F(DistributedExceptionHandlingTest, P2PMessageQueueFullException) {
    // WHAT: Test exception for message queue full
    // WHY:  Async message queue has limits

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_BUFFER_OVERFLOW,
        EXCEPTION_CATEGORY_P2P_NETWORK,
        EXCEPTION_SEVERITY_WARNING,
        "P2P message queue full - dropping messages"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_BUFFER_OVERFLOW);

    nimcp_exception_unref(ex);
}

TEST_F(DistributedExceptionHandlingTest, P2PProtocolVersionException) {
    // WHAT: Test exception for protocol version mismatch
    // WHY:  Peers must use compatible protocol

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_P2P_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "Protocol version mismatch (2.0 vs 1.5)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Neuromodulator Sync Exception Tests
//=============================================================================

TEST_F(DistributedExceptionHandlingTest, NeuromodPoolNotRegisteredException) {
    // WHAT: Test exception for unregistered neuromod pool
    // WHY:  Pool must be registered before sync

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_NOT_INITIALIZED,
        EXCEPTION_CATEGORY_NEUROMOD_SYNC,
        EXCEPTION_SEVERITY_ERROR,
        "Neuromodulator pool not registered for sync"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NOT_INITIALIZED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_NEUROMOD_SYNC);

    nimcp_exception_unref(ex);
}

TEST_F(DistributedExceptionHandlingTest, NeuromodConcentrationOutOfRangeException) {
    // WHAT: Test exception for invalid concentration
    // WHY:  Concentration must be in [0.0, 1.0]

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_NEUROMOD_SYNC,
        EXCEPTION_SEVERITY_WARNING,
        "Neuromodulator concentration out of range (1.5 > 1.0)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

TEST_F(DistributedExceptionHandlingTest, NeuromodBroadcastFailedException) {
    // WHAT: Test exception for broadcast failure
    // WHY:  Broadcast may fail due to network

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_NEUROMOD_SYNC,
        EXCEPTION_SEVERITY_ERROR,
        "Dopamine broadcast failed - no peers connected"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Glial Sync Exception Tests
//=============================================================================

TEST_F(DistributedExceptionHandlingTest, GlialSystemNotRegisteredException) {
    // WHAT: Test exception for unregistered glial system
    // WHY:  Glial system must be registered before coordination

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_NOT_INITIALIZED,
        EXCEPTION_CATEGORY_GLIAL_SYNC,
        EXCEPTION_SEVERITY_ERROR,
        "Glial integration system not registered"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NOT_INITIALIZED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_GLIAL_SYNC);

    nimcp_exception_unref(ex);
}

TEST_F(DistributedExceptionHandlingTest, PruningCoordinationFailedException) {
    // WHAT: Test exception for pruning coordination failure
    // WHY:  Distributed pruning consensus may fail

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_PRUNING_COORD,
        EXCEPTION_SEVERITY_WARNING,
        "Cross-node pruning coordination failed - no consensus"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_PRUNING_COORD);

    nimcp_exception_unref(ex);
}

TEST_F(DistributedExceptionHandlingTest, CalciumWavePropagationException) {
    // WHAT: Test exception for calcium wave propagation failure
    // WHY:  Calcium waves may fail to propagate across nodes

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_CATEGORY_CALCIUM_WAVE,
        EXCEPTION_SEVERITY_WARNING,
        "Calcium wave propagation timed out"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_TIMEOUT);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_CALCIUM_WAVE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Brain Region Sync Exception Tests
//=============================================================================

TEST_F(DistributedExceptionHandlingTest, RegionNotRegisteredException) {
    // WHAT: Test exception for unregistered brain region
    // WHY:  Region must be registered before sync

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_NOT_INITIALIZED,
        EXCEPTION_CATEGORY_REGION_SYNC,
        EXCEPTION_SEVERITY_ERROR,
        "Brain region not registered for network sync"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NOT_INITIALIZED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_REGION_SYNC);

    nimcp_exception_unref(ex);
}

TEST_F(DistributedExceptionHandlingTest, RegionActivityBroadcastException) {
    // WHAT: Test exception for region activity broadcast failure
    // WHY:  Activity broadcast may fail

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_REGION_SYNC,
        EXCEPTION_SEVERITY_WARNING,
        "Brain region activity broadcast failed"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Distributed Training Exception Tests
//=============================================================================

TEST_F(DistributedExceptionHandlingTest, FederatedAveragingException) {
    // WHAT: Test exception for federated averaging failure
    // WHY:  Distributed training may fail to average weights

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_DISTRIBUTED_TRAINING,
        EXCEPTION_SEVERITY_ERROR,
        "Federated averaging failed - weight divergence detected"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_DISTRIBUTED_TRAINING);

    nimcp_exception_unref(ex);
}

TEST_F(DistributedExceptionHandlingTest, GradientSyncException) {
    // WHAT: Test exception for gradient synchronization failure
    // WHY:  Gradient sync is critical for distributed training

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_THREAD_SYNC,
        EXCEPTION_CATEGORY_DISTRIBUTED_TRAINING,
        EXCEPTION_SEVERITY_ERROR,
        "Gradient synchronization across nodes failed"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_THREAD_SYNC);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Handler Chain Tests
//=============================================================================

TEST_F(DistributedExceptionHandlingTest, HandlerChainDispatch) {
    // WHAT: Test exception dispatch through multiple handlers
    // WHY:  Verify chain processing works correctly

    // Register multiple handlers
    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    options1.name = "distributed_handler_1";
    options1.handler = test_exception_handler;
    options1.priority = 100;

    options2.name = "distributed_handler_2";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_CATEGORY_DISTRIBUTED_COGNITION,
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

TEST_F(DistributedExceptionHandlingTest, HandlerConsumesException) {
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

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_P2P_NETWORK,
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

TEST_F(DistributedExceptionHandlingTest, DistributedExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for distributed exceptions
    // WHY:  Distributed failures may need retry or reconnection

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_CATEGORY_P2P_NETWORK,
        EXCEPTION_SEVERITY_ERROR,
        "Network connection lost to peer"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Network exceptions should have retry as primary action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(DistributedExceptionHandlingTest, CriticalDistributedExceptionRecovery) {
    // WHAT: Test recovery for critical distributed failures
    // WHY:  Critical failures may require emergency measures

    nimcp_exception_t* ex = create_distributed_exception(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_CATEGORY_DISTRIBUTED_TRAINING,
        EXCEPTION_SEVERITY_CRITICAL,
        "Weight corruption detected during distributed training"
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

TEST_F(DistributedExceptionHandlingTest, ExceptionStatisticsTracking) {
    // WHAT: Test that exception dispatch is tracked by handlers
    // WHY:  Need to monitor distributed exception frequency

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
        nimcp_exception_t* ex = create_distributed_exception(
            NIMCP_ERROR_NETWORK_IO,
            EXCEPTION_CATEGORY_P2P_NETWORK,
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

TEST_F(DistributedExceptionHandlingTest, ConcurrentExceptionCreation) {
    // WHAT: Test concurrent exception creation
    // WHY:  Distributed systems run across multiple threads

    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int exceptions_per_thread = 10;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&success_count, t, exceptions_per_thread]() {
            for (int i = 0; i < exceptions_per_thread; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_NETWORK_IO,
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
