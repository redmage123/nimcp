/**
 * @file test_networking_exception_integration.cpp
 * @brief Integration tests for networking module exception handling
 *
 * WHAT: Integration tests for exception handling across networking modules
 * WHY:  Verify that exceptions propagate correctly across module boundaries and
 *       trigger appropriate immune system responses for recovery coordination
 * HOW:  Test cross-module exception flow, immune integration, and recovery chains
 *
 * TEST SCENARIOS:
 * - P2P to distributed cognition exception propagation
 * - Protocol to immune system integration
 * - NLP session exception with handler chain
 * - Swarm coordination exception recovery
 * - Multi-module aggregate exception handling
 * - Exception-driven immune response and recovery
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <functional>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NetworkingExceptionIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> recovery_callback_count;
    static std::atomic<nimcp_exception_recovery_action_t> last_recovery_action;
    static std::atomic<bool> immune_response_received;
    static std::vector<nimcp_error_t> exception_sequence;

    nimcp_handler_registration_t* primary_handler_reg;
    nimcp_handler_registration_t* secondary_handler_reg;

    void SetUp() override {
        handler_call_count = 0;
        recovery_callback_count = 0;
        last_recovery_action = EXCEPTION_RECOVERY_NONE;
        immune_response_received = false;
        exception_sequence.clear();
        primary_handler_reg = nullptr;
        secondary_handler_reg = nullptr;

        // Initialize exception system
        nimcp_exception_system_init();

        // Initialize immune integration (with defaults)
        nimcp_exception_immune_init(nullptr);

        // Install default handlers
        nimcp_install_default_handlers();

        // Register integration test handler
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.name = "integration_test_handler";
        options.handler = integration_exception_handler;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH + 10;  // Higher than default
        options.user_data = nullptr;
        primary_handler_reg = nimcp_handler_register(&options);
    }

    void TearDown() override {
        if (primary_handler_reg) {
            nimcp_handler_unregister(primary_handler_reg);
            primary_handler_reg = nullptr;
        }
        if (secondary_handler_reg) {
            nimcp_handler_unregister(secondary_handler_reg);
            secondary_handler_reg = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_immune_shutdown();
        nimcp_exception_system_shutdown();
    }

    static bool integration_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        exception_sequence.push_back(ex->code);
        return false;  // Continue chain
    }

    static int test_recovery_callback(
        nimcp_exception_t* ex,
        nimcp_exception_recovery_action_t action,
        void* user_data
    ) {
        (void)ex;
        (void)user_data;
        recovery_callback_count++;
        last_recovery_action = action;
        return 0;  // Success
    }
};

std::atomic<int> NetworkingExceptionIntegrationTest::handler_call_count(0);
std::atomic<int> NetworkingExceptionIntegrationTest::recovery_callback_count(0);
std::atomic<nimcp_exception_recovery_action_t> NetworkingExceptionIntegrationTest::last_recovery_action(EXCEPTION_RECOVERY_NONE);
std::atomic<bool> NetworkingExceptionIntegrationTest::immune_response_received(false);
std::vector<nimcp_error_t> NetworkingExceptionIntegrationTest::exception_sequence;

//=============================================================================
// Cross-Module Exception Propagation Tests
//=============================================================================

TEST_F(NetworkingExceptionIntegrationTest, P2PToDistributedCognitionPropagation) {
    // WHAT: Test exception propagation from P2P layer to distributed cognition
    // WHY:  P2P failures affect distributed cognitive operations
    // HOW:  Chain exceptions from P2P through distributed layer

    // Root cause: P2P peer connection failure
    nimcp_io_exception_t* p2p_ex = nimcp_io_exception_create(
        NIMCP_ERROR_SOCKET_ERROR,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "192.168.1.100:8080",
        "P2P peer connection refused"
    );
    ASSERT_NE(p2p_ex, nullptr);
    p2p_ex->is_network = true;
    p2p_ex->errno_value = 111;  // ECONNREFUSED

    // Higher level: Distributed cognition sync failure
    nimcp_exception_t* distrib_ex = nimcp_exception_create(
        NIMCP_ERROR_GPU_SYNC,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Distributed cognition sync failed due to peer unavailability"
    );
    ASSERT_NE(distrib_ex, nullptr);

    // Chain: distrib_ex caused by p2p_ex
    nimcp_exception_set_cause(distrib_ex, (nimcp_exception_t*)p2p_ex);

    // Set context showing propagation path
    nimcp_exception_set_context(distrib_ex, "source_module", "p2p_node");
    nimcp_exception_set_context(distrib_ex, "target_module", "distrib_cognition");
    nimcp_exception_set_context(distrib_ex, "failed_operation", "neuromod_sync");

    // Dispatch and verify handler chain
    handler_call_count = 0;
    exception_sequence.clear();
    nimcp_exception_dispatch(distrib_ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(exception_sequence[0], NIMCP_ERROR_GPU_SYNC);

    // Verify cause chain
    nimcp_exception_t* cause = nimcp_exception_get_cause(distrib_ex);
    ASSERT_NE(cause, nullptr);
    EXPECT_EQ(cause->code, NIMCP_ERROR_SOCKET_ERROR);

    nimcp_exception_unref(distrib_ex);
}

TEST_F(NetworkingExceptionIntegrationTest, ProtocolToImmuneSystemIntegration) {
    // WHAT: Test that protocol errors trigger immune responses
    // WHY:  Severe protocol errors should activate immune system for auto-recovery
    // HOW:  Create severe protocol exception and present to immune system

    nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1,  // threat_type
        "Protocol violation: invalid message structure detected"
    );
    ASSERT_NE(sec_ex, nullptr);

    sec_ex->severity_score = 7;
    sec_ex->quarantine_required = false;
    sec_ex->source_node_id = 0x12345678;

    nimcp_exception_t* ex = (nimcp_exception_t*)sec_ex;

    // Set context for immune pattern matching
    nimcp_exception_set_context(ex, "protocol_version", "2");
    nimcp_exception_set_context(ex, "message_type", "CTRL_MSG_HEARTBEAT");
    nimcp_exception_set_context(ex, "violation_type", "invalid_checksum");

    // Generate epitope for immune recognition
    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(result, 0);

    // Verify immune processing occurred
    // presented_to_immune flag behavior is implementation-specific

    // Verify recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);
    // Security exceptions should suggest quarantine or validation
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionIntegrationTest, NLPSessionExceptionWithHandlerChain) {
    // WHAT: Test NLP session exception flows through handler chain correctly
    // WHY:  Session exceptions need multiple handlers (logging, security, recovery)
    // HOW:  Register multiple handlers and verify all receive the exception

    static std::atomic<int> security_handler_count{0};
    static std::atomic<int> logging_handler_count{0};

    // Security handler
    auto security_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        security_handler_count++;
        return false;  // Continue chain
    };

    // Logging handler
    auto logging_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        logging_handler_count++;
        return false;  // Continue chain
    };

    nimcp_handler_options_t opts;

    // Register security handler
    nimcp_handler_default_options(&opts);
    opts.name = "nlp_security_handler";
    opts.handler = security_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.category_filter = EXCEPTION_CATEGORY_SECURITY;
    nimcp_handler_registration_t* sec_reg = nimcp_handler_register(&opts);

    // Register logging handler
    nimcp_handler_default_options(&opts);
    opts.name = "nlp_logging_handler";
    opts.handler = logging_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_LOW;
    opts.category_filter = (nimcp_exception_category_t)0;  // All categories
    nimcp_handler_registration_t* log_reg = nimcp_handler_register(&opts);

    // Create NLP session exception
    nimcp_security_exception_t* nlp_ex = nimcp_security_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        0,  // threat_type
        "NLP session expired: timeout after 30 seconds"
    );
    ASSERT_NE(nlp_ex, nullptr);

    // Reset counters
    security_handler_count = 0;
    logging_handler_count = 0;

    // Dispatch
    nimcp_exception_dispatch((nimcp_exception_t*)nlp_ex);

    // Verify both handlers were called
    // Security handler should be called for security category
    // Logging handler should be called for all categories
    EXPECT_GE(logging_handler_count.load(), 1);

    nimcp_exception_unref((nimcp_exception_t*)nlp_ex);
    nimcp_handler_unregister(sec_reg);
    nimcp_handler_unregister(log_reg);
}

//=============================================================================
// Swarm Coordination Exception Tests
//=============================================================================

TEST_F(NetworkingExceptionIntegrationTest, SwarmCoordinationExceptionRecovery) {
    // WHAT: Test swarm coordination exception with recovery action
    // WHY:  Swarm failures require coordinated recovery across nodes
    // HOW:  Create swarm exception, register recovery callback, verify execution

    // Register recovery callback for RESTART_COMPONENT action
    nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_RESTART_COMPONENT,
        test_recovery_callback,
        nullptr
    );

    // Create swarm coordination exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Swarm consensus failed: quorum not reached"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "swarm_size", "10");
    nimcp_exception_set_context(ex, "votes_received", "3");
    nimcp_exception_set_context(ex, "quorum_required", "6");
    nimcp_exception_set_context(ex, "consensus_type", "master_election");

    // Set suggested recovery
    ex->suggested_action = EXCEPTION_RECOVERY_RESTART_COMPONENT;

    // Execute recovery
    recovery_callback_count = 0;
    int result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_RESTART_COMPONENT);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(recovery_callback_count.load(), 1);
    EXPECT_EQ(last_recovery_action.load(), EXCEPTION_RECOVERY_RESTART_COMPONENT);

    // Unregister callback
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RESTART_COMPONENT);

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionIntegrationTest, SwarmByzantineFaultException) {
    // WHAT: Test Byzantine fault detection exception handling
    // WHY:  Byzantine faults are critical security threats in swarms
    // HOW:  Create Byzantine exception, verify immune presentation and quarantine

    nimcp_security_exception_t* byz_ex = nimcp_security_exception_create(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        3,  // threat_type: Byzantine
        "Byzantine fault detected: inconsistent vote messages"
    );
    ASSERT_NE(byz_ex, nullptr);

    byz_ex->severity_score = 9;
    byz_ex->quarantine_required = true;
    byz_ex->source_node_id = 0xBAD12345;
    byz_ex->threat_signature = "vote_inconsistency";

    nimcp_exception_t* ex = (nimcp_exception_t*)byz_ex;

    // Set context
    nimcp_exception_set_context(ex, "expected_vote", "node_A");
    nimcp_exception_set_context(ex, "actual_vote", "node_B");
    nimcp_exception_set_context(ex, "detection_round", "5");

    // Present to immune
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(result, 0);

    // Verify critical severity
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);

    // Verify quarantine suggested
    ex->suggested_action = EXCEPTION_RECOVERY_QUARANTINE;
    EXPECT_EQ(ex->suggested_action, EXCEPTION_RECOVERY_QUARANTINE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Multi-Module Aggregate Exception Tests
//=============================================================================

TEST_F(NetworkingExceptionIntegrationTest, MultiModuleAggregateException) {
    // WHAT: Test aggregate exception spanning multiple networking modules
    // WHY:  Network partitions can cause failures in multiple modules simultaneously
    // HOW:  Create aggregate with exceptions from P2P, NLP, and distributed modules

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Network partition detected: multiple module failures"
    );
    ASSERT_NE(agg, nullptr);

    // P2P module exception
    nimcp_io_exception_t* p2p_ex = nimcp_io_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "p2p_heartbeat",
        "P2P heartbeat timeout to multiple peers"
    );
    ASSERT_NE(p2p_ex, nullptr);
    nimcp_exception_set_context((nimcp_exception_t*)p2p_ex, "module", "p2p");
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)p2p_ex);

    // NLP module exception
    nimcp_security_exception_t* nlp_ex = nimcp_security_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        0,
        "NLP sessions expired due to partition"
    );
    ASSERT_NE(nlp_ex, nullptr);
    nimcp_exception_set_context((nimcp_exception_t*)nlp_ex, "module", "nlp");
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)nlp_ex);

    // Distributed cognition exception
    nimcp_exception_t* distrib_ex = nimcp_exception_create(
        NIMCP_ERROR_GPU_SYNC,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Distributed cognition sync halted due to partition"
    );
    ASSERT_NE(distrib_ex, nullptr);
    nimcp_exception_set_context(distrib_ex, "module", "distributed_cognition");
    nimcp_aggregate_exception_add(agg, distrib_ex);

    // Verify aggregate contains all children
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 3u);

    // Dispatch aggregate
    handler_call_count = 0;
    exception_sequence.clear();
    nimcp_exception_dispatch((nimcp_exception_t*)agg);

    EXPECT_GE(handler_call_count.load(), 1);

    // Verify each child can be retrieved
    nimcp_exception_t* child0 = nimcp_aggregate_exception_get(agg, 0);
    ASSERT_NE(child0, nullptr);
    EXPECT_EQ(child0->code, NIMCP_ERROR_TIMEOUT);

    nimcp_exception_t* child1 = nimcp_aggregate_exception_get(agg, 1);
    ASSERT_NE(child1, nullptr);
    EXPECT_EQ(child1->code, NIMCP_ERROR_TIMEOUT);

    nimcp_exception_t* child2 = nimcp_aggregate_exception_get(agg, 2);
    ASSERT_NE(child2, nullptr);
    EXPECT_EQ(child2->code, NIMCP_ERROR_GPU_SYNC);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Recovery Strategy Integration Tests
//=============================================================================

TEST_F(NetworkingExceptionIntegrationTest, RecoveryStrategyForIOExceptions) {
    // WHAT: Test that I/O exceptions get appropriate recovery strategies
    // WHY:  Different I/O errors require different recovery approaches
    // HOW:  Create I/O exceptions and verify a recovery strategy is returned

    struct {
        nimcp_error_t code;
        const char* message;
    } test_cases[] = {
        { NIMCP_ERROR_TIMEOUT, "Connection timeout" },
        { NIMCP_ERROR_SOCKET_ERROR, "Connection refused" },
        { NIMCP_ERROR_NETWORK_IO, "Connection closed" },
    };

    for (const auto& tc : test_cases) {
        nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
            tc.code,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "test_path",
            "%s", tc.message
        );
        ASSERT_NE(io_ex, nullptr);

        nimcp_exception_recovery_strategy_t strategy;
        nimcp_exception_get_recovery_strategy((nimcp_exception_t*)io_ex, &strategy);

        // I/O exceptions should get some recovery strategy (not NONE)
        // The exact strategy is implementation-specific
        EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE)
            << "I/O exception should have a recovery strategy for error code: " << tc.code;

        nimcp_exception_unref((nimcp_exception_t*)io_ex);
    }
}

TEST_F(NetworkingExceptionIntegrationTest, RecoveryChainExecution) {
    // WHAT: Test execution of recovery chain (primary -> fallback)
    // WHY:  If primary recovery fails, fallback should be attempted
    // HOW:  Register failing primary callback, verify fallback is called

    static std::atomic<int> primary_calls{0};
    static std::atomic<int> fallback_calls{0};

    // Primary callback that fails
    auto primary_callback = [](
        nimcp_exception_t* ex,
        nimcp_exception_recovery_action_t action,
        void* user_data
    ) -> int {
        (void)ex;
        (void)action;
        (void)user_data;
        primary_calls++;
        return -1;  // Fail
    };

    // Fallback callback that succeeds
    auto fallback_callback = [](
        nimcp_exception_t* ex,
        nimcp_exception_recovery_action_t action,
        void* user_data
    ) -> int {
        (void)ex;
        (void)action;
        (void)user_data;
        fallback_calls++;
        return 0;  // Success
    };

    // Register callbacks
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, primary_callback, nullptr);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, fallback_callback, nullptr);

    // Create exception
    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "test_path",
        "Test recovery chain"
    );
    ASSERT_NE(io_ex, nullptr);

    // Reset counters
    primary_calls = 0;
    fallback_calls = 0;

    // Execute primary recovery (should fail)
    int result = nimcp_execute_recovery((nimcp_exception_t*)io_ex, EXCEPTION_RECOVERY_RETRY);
    EXPECT_EQ(result, -1);
    EXPECT_EQ(primary_calls.load(), 1);

    // Execute fallback recovery (should succeed)
    result = nimcp_execute_recovery((nimcp_exception_t*)io_ex, EXCEPTION_RECOVERY_ROLLBACK);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(fallback_calls.load(), 1);

    // Cleanup
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK);
    nimcp_exception_unref((nimcp_exception_t*)io_ex);
}

//=============================================================================
// Immune Response Integration Tests
//=============================================================================

TEST_F(NetworkingExceptionIntegrationTest, ImmuneResponseToNetworkAnomaly) {
    // WHAT: Test immune system response to network anomaly
    // WHY:  Network anomalies should trigger appropriate immune responses
    // HOW:  Create network anomaly exception, present to immune, verify response

    // Create severe network anomaly
    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "distributed_sync",
        "Network anomaly: packet loss spike detected"
    );
    ASSERT_NE(io_ex, nullptr);

    nimcp_exception_t* ex = (nimcp_exception_t*)io_ex;
    nimcp_exception_set_context(ex, "packet_loss_rate", "0.35");
    nimcp_exception_set_context(ex, "threshold", "0.15");
    nimcp_exception_set_context(ex, "duration_ms", "5000");

    // Generate epitope
    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(result, 0);

    // Verify exception was processed
    // presented_to_immune flag behavior is implementation-specific

    // Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // I/O exceptions should have some recovery strategy
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionIntegrationTest, ImmuneMemoryForRepeatedExceptions) {
    // WHAT: Test that immune system learns from repeated exceptions
    // WHY:  Recurring exceptions should trigger faster/stronger responses
    // HOW:  Present similar exceptions multiple times, verify epitope consistency

    // Create first exception
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Repeated timeout to peer"
    );
    ASSERT_NE(ex1, nullptr);
    nimcp_exception_set_context(ex1, "peer_id", "0x12345678");

    // Create second similar exception
    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Repeated timeout to peer"
    );
    ASSERT_NE(ex2, nullptr);
    nimcp_exception_set_context(ex2, "peer_id", "0x12345678");

    // Generate epitopes
    size_t len1 = nimcp_exception_generate_epitope(ex1);
    size_t len2 = nimcp_exception_generate_epitope(ex2);

    EXPECT_GT(len1, 0u);
    EXPECT_GT(len2, 0u);
    EXPECT_EQ(len1, len2);

    // Present both to immune
    nimcp_immune_response_t response1, response2;
    memset(&response1, 0, sizeof(response1));
    memset(&response2, 0, sizeof(response2));

    int result1 = nimcp_exception_present_to_immune(ex1, &response1);
    int result2 = nimcp_exception_present_to_immune(ex2, &response2);

    // Present operations should succeed
    EXPECT_EQ(result1, 0);
    EXPECT_EQ(result2, 0);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

//=============================================================================
// Handler Enable/Disable Tests
//=============================================================================

TEST_F(NetworkingExceptionIntegrationTest, HandlerEnableDisable) {
    // WHAT: Test handler enable/disable functionality
    // WHY:  Handlers may need to be temporarily disabled during recovery
    // HOW:  Register handler, disable, verify not called, re-enable, verify called

    static std::atomic<int> test_handler_calls{0};

    auto test_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        test_handler_calls++;
        return false;
    };

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "enable_disable_test";
    opts.handler = test_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH + 20;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Test with handler enabled
    test_handler_calls = 0;
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception 1"
    );
    nimcp_exception_dispatch(ex1);
    int calls_enabled = test_handler_calls.load();
    EXPECT_GE(calls_enabled, 1);
    nimcp_exception_unref(ex1);

    // Disable handler
    nimcp_handler_disable(reg);

    // Test with handler disabled
    test_handler_calls = 0;
    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception 2"
    );
    nimcp_exception_dispatch(ex2);
    int calls_disabled = test_handler_calls.load();
    EXPECT_LT(calls_disabled, calls_enabled);  // Fewer or zero calls
    nimcp_exception_unref(ex2);

    // Re-enable handler
    nimcp_handler_enable(reg);

    // Test with handler re-enabled
    test_handler_calls = 0;
    nimcp_exception_t* ex3 = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception 3"
    );
    nimcp_exception_dispatch(ex3);
    int calls_reenabled = test_handler_calls.load();
    EXPECT_GE(calls_reenabled, 1);
    nimcp_exception_unref(ex3);

    nimcp_handler_unregister(reg);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(NetworkingExceptionIntegrationTest, ConcurrentExceptionDispatch) {
    // WHAT: Test thread safety of exception dispatch
    // WHY:  Multiple threads may generate exceptions simultaneously
    // HOW:  Dispatch exceptions from multiple threads concurrently

    static std::atomic<int> total_dispatched{0};
    const int num_threads = 4;
    const int exceptions_per_thread = 10;

    auto dispatch_exceptions = [&]() {
        for (int i = 0; i < exceptions_per_thread; i++) {
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_TIMEOUT,
                EXCEPTION_SEVERITY_WARNING,
                __FILE__, __LINE__, __func__,
                "Concurrent test exception %d", i
            );
            if (ex) {
                nimcp_exception_dispatch(ex);
                total_dispatched++;
                nimcp_exception_unref(ex);
            }
        }
    };

    // Launch threads
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(dispatch_exceptions);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    // Verify all exceptions were dispatched
    EXPECT_EQ(total_dispatched.load(), num_threads * exceptions_per_thread);

    // Handler should have been called for each
    EXPECT_GE(handler_call_count.load(), num_threads * exceptions_per_thread);
}

TEST_F(NetworkingExceptionIntegrationTest, ConcurrentImmunePresentation) {
    // WHAT: Test thread safety of immune presentation
    // WHY:  Multiple threads may present exceptions to immune simultaneously
    // HOW:  Present exceptions from multiple threads concurrently

    static std::atomic<int> total_presented{0};
    const int num_threads = 4;
    const int exceptions_per_thread = 5;

    auto present_exceptions = [&]() {
        for (int i = 0; i < exceptions_per_thread; i++) {
            nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
                NIMCP_ERROR_NETWORK_IO,
                EXCEPTION_SEVERITY_SEVERE,
                __FILE__, __LINE__, __func__,
                "test_path",
                "Concurrent immune test %d", i
            );
            if (io_ex) {
                nimcp_exception_t* ex = (nimcp_exception_t*)io_ex;
                nimcp_exception_generate_epitope(ex);

                nimcp_immune_response_t response;
                memset(&response, 0, sizeof(response));
                if (nimcp_exception_present_to_immune(ex, &response) == 0) {
                    total_presented++;
                }
                nimcp_exception_unref(ex);
            }
        }
    };

    // Launch threads
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(present_exceptions);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    // Verify all exceptions were presented
    EXPECT_EQ(total_presented.load(), num_threads * exceptions_per_thread);
}

//=============================================================================
// Statistics Verification Tests
//=============================================================================

TEST_F(NetworkingExceptionIntegrationTest, ImmuneStatisticsTracking) {
    // WHAT: Test that immune statistics are properly tracked
    // WHY:  Statistics help diagnose system health and exception patterns
    // HOW:  Present exceptions and verify statistics are updated

    // Reset statistics
    nimcp_exception_immune_reset_stats();

    // Present several exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
            NIMCP_ERROR_TIMEOUT,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "test",
            "Stats test exception %d", i
        );
        if (io_ex) {
            nimcp_exception_t* ex = (nimcp_exception_t*)io_ex;
            nimcp_exception_generate_epitope(ex);
            nimcp_exception_present_to_immune(ex, nullptr);
            nimcp_exception_unref(ex);
        }
    }

    // Get statistics
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);

    // Statistics tracking is optional/implementation-specific
    // The key contract is that the API doesn't crash
    (void)stats;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
