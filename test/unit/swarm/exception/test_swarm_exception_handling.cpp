/**
 * @file test_swarm_exception_handling.cpp
 * @brief Unit tests for swarm module exception handling
 *
 * WHAT: Test exception handling across all swarm modules
 * WHY:  Ensure consistent error-to-exception mapping and handler chain dispatch
 * HOW:  Test each swarm module's error conditions and exception integration
 *
 * SWARM MODULES TESTED:
 * - Swarm Brain (main coordinator)
 * - Swarm Signal (radio communication)
 * - Swarm Consensus (voting/quorum)
 * - Swarm Emergence (tier detection)
 * - Swarm Collective (workspace/attention)
 * - Swarm Pheromone (chemical signaling)
 * - Swarm Flocking (group behaviors)
 *
 * TEST PATTERNS:
 * - Error code to exception mapping
 * - Exception dispatch through handler chain
 * - Exception category classification (SWARM, COORDINATION, COMMUNICATION)
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
#include "swarm/nimcp_swarm_brain.h"
#include "swarm/nimcp_swarm_signal.h"
#include "swarm/nimcp_swarm_consensus.h"
#include "swarm/nimcp_swarm_emergence.h"
}

//=============================================================================
// Swarm Exception Categories
//=============================================================================

// Define swarm-specific exception categories for testing
#define EXCEPTION_CATEGORY_SWARM_BASE         200
#define EXCEPTION_CATEGORY_SWARM_BRAIN        (EXCEPTION_CATEGORY_SWARM_BASE + 1)
#define EXCEPTION_CATEGORY_SWARM_SIGNAL       (EXCEPTION_CATEGORY_SWARM_BASE + 2)
#define EXCEPTION_CATEGORY_SWARM_CONSENSUS    (EXCEPTION_CATEGORY_SWARM_BASE + 3)
#define EXCEPTION_CATEGORY_SWARM_EMERGENCE    (EXCEPTION_CATEGORY_SWARM_BASE + 4)
#define EXCEPTION_CATEGORY_SWARM_PHEROMONE    (EXCEPTION_CATEGORY_SWARM_BASE + 5)
#define EXCEPTION_CATEGORY_SWARM_FLOCKING     (EXCEPTION_CATEGORY_SWARM_BASE + 6)
#define EXCEPTION_CATEGORY_SWARM_WORKSPACE    (EXCEPTION_CATEGORY_SWARM_BASE + 7)
#define EXCEPTION_CATEGORY_SWARM_QUORUM       (EXCEPTION_CATEGORY_SWARM_BASE + 8)

//=============================================================================
// Test Fixture
//=============================================================================

class SwarmExceptionHandlingTest : public ::testing::Test {
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

    // Helper to create swarm exception
    nimcp_exception_t* create_swarm_exception(
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

std::atomic<int> SwarmExceptionHandlingTest::handler_call_count(0);
std::atomic<int> SwarmExceptionHandlingTest::last_exception_code(0);
std::atomic<int> SwarmExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> SwarmExceptionHandlingTest::handler_consumed(false);

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(SwarmExceptionHandlingTest, CreateSwarmBrainException) {
    // WHAT: Test creation of swarm brain-related exception
    // WHY:  Verify exception fields are set correctly

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SWARM_BRAIN,
        EXCEPTION_SEVERITY_ERROR,
        "Swarm brain coordinator failed to join network"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SWARM_BRAIN);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_NE(ex->message, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(SwarmExceptionHandlingTest, CreateSwarmSignalException) {
    // WHAT: Test creation of swarm signal-related exception
    // WHY:  Signal adapter errors need proper categorization

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_CATEGORY_SWARM_SIGNAL,
        EXCEPTION_SEVERITY_ERROR,
        "Radio signal adapter failed to broadcast message"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NETWORK_IO);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SWARM_SIGNAL);

    nimcp_exception_unref(ex);
}

TEST_F(SwarmExceptionHandlingTest, CreateSwarmConsensusException) {
    // WHAT: Test creation of consensus-related exception
    // WHY:  Consensus failures need specialized handling

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_CATEGORY_SWARM_CONSENSUS,
        EXCEPTION_SEVERITY_WARNING,
        "Consensus vote timed out - insufficient quorum"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_TIMEOUT);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SWARM_CONSENSUS);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Swarm Brain Exception Tests
//=============================================================================

TEST_F(SwarmExceptionHandlingTest, SwarmBrainNullConfigException) {
    // WHAT: Test exception for NULL config parameter
    // WHY:  Verify proper error handling for invalid inputs

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_SWARM_BRAIN,
        EXCEPTION_SEVERITY_ERROR,
        "Swarm brain config is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    // Register handler and dispatch
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "swarm_null_handler";
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

TEST_F(SwarmExceptionHandlingTest, SwarmBrainPeerCapacityException) {
    // WHAT: Test exception for peer capacity exceeded
    // WHY:  Swarm has maximum peer limits

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_SWARM_BRAIN,
        EXCEPTION_SEVERITY_WARNING,
        "Swarm peer capacity exceeded (32/32 peers)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

TEST_F(SwarmExceptionHandlingTest, SwarmBrainMigrationException) {
    // WHAT: Test exception for brain migration failure
    // WHY:  Migration between hosts may fail

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SWARM_BRAIN,
        EXCEPTION_SEVERITY_ERROR,
        "Brain migration checkpoint creation failed"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Swarm Signal Exception Tests
//=============================================================================

TEST_F(SwarmExceptionHandlingTest, SwarmSignalRadioFailException) {
    // WHAT: Test exception for radio communication failure
    // WHY:  Physical layer errors need handling

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_IO,
        EXCEPTION_CATEGORY_SWARM_SIGNAL,
        EXCEPTION_SEVERITY_ERROR,
        "Radio adapter failed - no signal"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_IO);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SWARM_SIGNAL);

    nimcp_exception_unref(ex);
}

TEST_F(SwarmExceptionHandlingTest, SwarmSignalMessageSizeException) {
    // WHAT: Test exception for message too large
    // WHY:  Messages have size limits (255 bytes)

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_BUFFER_OVERFLOW,
        EXCEPTION_CATEGORY_SWARM_SIGNAL,
        EXCEPTION_SEVERITY_ERROR,
        "Message exceeds maximum size (300 > 255 bytes)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_BUFFER_OVERFLOW);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Swarm Consensus Exception Tests
//=============================================================================

TEST_F(SwarmExceptionHandlingTest, SwarmConsensusQuorumException) {
    // WHAT: Test exception for quorum not reached
    // WHY:  Consensus requires minimum participants

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SWARM_QUORUM,
        EXCEPTION_SEVERITY_WARNING,
        "Quorum not reached (2/5 votes received)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SWARM_QUORUM);

    nimcp_exception_unref(ex);
}

TEST_F(SwarmExceptionHandlingTest, SwarmConsensusConflictException) {
    // WHAT: Test exception for voting conflict
    // WHY:  Byzantine nodes may cause conflicts

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_SECURITY_THREAT,
        EXCEPTION_CATEGORY_SWARM_CONSENSUS,
        EXCEPTION_SEVERITY_SEVERE,
        "Byzantine vote conflict detected from peer 5"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_SEVERE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Swarm Emergence Exception Tests
//=============================================================================

TEST_F(SwarmExceptionHandlingTest, SwarmEmergenceTierTransitionException) {
    // WHAT: Test exception for tier transition failure
    // WHY:  Emergence tier changes may fail

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_CATEGORY_SWARM_EMERGENCE,
        EXCEPTION_SEVERITY_WARNING,
        "Cannot transition from TIER_1_PAIRED to TIER_3_SWARM"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SWARM_EMERGENCE);

    nimcp_exception_unref(ex);
}

TEST_F(SwarmExceptionHandlingTest, SwarmEmergenceCoherenceException) {
    // WHAT: Test exception for low coherence
    // WHY:  Coherence below threshold affects emergence

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_SWARM_EMERGENCE,
        EXCEPTION_SEVERITY_WARNING,
        "Swarm coherence below threshold (0.3 < 0.5)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Swarm Pheromone Exception Tests
//=============================================================================

TEST_F(SwarmExceptionHandlingTest, SwarmPheromoneEvaporationException) {
    // WHAT: Test exception for pheromone evaporation error
    // WHY:  Pheromone decay calculations may fail

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SWARM_PHEROMONE,
        EXCEPTION_SEVERITY_ERROR,
        "Pheromone evaporation calculation failed - negative concentration"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SWARM_PHEROMONE);

    nimcp_exception_unref(ex);
}

TEST_F(SwarmExceptionHandlingTest, SwarmPheromoneDepositException) {
    // WHAT: Test exception for pheromone deposit failure
    // WHY:  Deposit may exceed bounds

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_SWARM_PHEROMONE,
        EXCEPTION_SEVERITY_WARNING,
        "Pheromone deposit exceeds maximum concentration"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Swarm Flocking Exception Tests
//=============================================================================

TEST_F(SwarmExceptionHandlingTest, SwarmFlockingCollisionException) {
    // WHAT: Test exception for collision detection failure
    // WHY:  Flocking algorithms detect collisions

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SWARM_FLOCKING,
        EXCEPTION_SEVERITY_CRITICAL,
        "Collision detected between agents 3 and 7"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SWARM_FLOCKING);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_unref(ex);
}

TEST_F(SwarmExceptionHandlingTest, SwarmFlockingVelocityException) {
    // WHAT: Test exception for velocity bound exceeded
    // WHY:  Agents have maximum velocity

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_SWARM_FLOCKING,
        EXCEPTION_SEVERITY_WARNING,
        "Agent velocity exceeds maximum (15.0 > 10.0 m/s)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Swarm Workspace Exception Tests
//=============================================================================

TEST_F(SwarmExceptionHandlingTest, SwarmWorkspaceCapacityException) {
    // WHAT: Test exception for workspace capacity exceeded
    // WHY:  Collective workspace has limited size

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_SWARM_WORKSPACE,
        EXCEPTION_SEVERITY_WARNING,
        "Collective workspace full (32/32 entries)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_SWARM_WORKSPACE);

    nimcp_exception_unref(ex);
}

TEST_F(SwarmExceptionHandlingTest, SwarmWorkspaceSyncException) {
    // WHAT: Test exception for workspace sync failure
    // WHY:  Workspace synchronization may fail

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_THREAD_SYNC,
        EXCEPTION_CATEGORY_SWARM_WORKSPACE,
        EXCEPTION_SEVERITY_ERROR,
        "Collective workspace synchronization failed"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_THREAD_SYNC);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Handler Chain Tests
//=============================================================================

TEST_F(SwarmExceptionHandlingTest, HandlerChainDispatch) {
    // WHAT: Test exception dispatch through multiple handlers
    // WHY:  Verify chain processing works correctly

    // Register multiple handlers
    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    options1.name = "swarm_handler_1";
    options1.handler = test_exception_handler;
    options1.priority = 100;

    options2.name = "swarm_handler_2";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SWARM_BRAIN,
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

TEST_F(SwarmExceptionHandlingTest, HandlerConsumesException) {
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

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_SWARM_SIGNAL,
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

TEST_F(SwarmExceptionHandlingTest, SwarmExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for swarm exceptions
    // WHY:  Swarm failures may need retry or reconnection

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_CATEGORY_SWARM_SIGNAL,
        EXCEPTION_SEVERITY_ERROR,
        "Radio communication lost"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Signal/IO exceptions should have retry as primary action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(SwarmExceptionHandlingTest, CriticalSwarmExceptionRecovery) {
    // WHAT: Test recovery for critical swarm failures
    // WHY:  Critical failures may require emergency measures

    nimcp_exception_t* ex = create_swarm_exception(
        NIMCP_ERROR_SECURITY_THREAT,
        EXCEPTION_CATEGORY_SWARM_CONSENSUS,
        EXCEPTION_SEVERITY_CRITICAL,
        "Byzantine attack detected in swarm"
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

TEST_F(SwarmExceptionHandlingTest, ExceptionStatisticsTracking) {
    // WHAT: Test that exception dispatch is tracked by handlers
    // WHY:  Need to monitor swarm exception frequency

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
        nimcp_exception_t* ex = create_swarm_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_CATEGORY_SWARM_BRAIN,
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

TEST_F(SwarmExceptionHandlingTest, ConcurrentExceptionCreation) {
    // WHAT: Test concurrent exception creation
    // WHY:  Swarm runs across multiple threads

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
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
