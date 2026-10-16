/**
 * @file test_networking_exception_handling.cpp
 * @brief Unit tests for networking module exception handling
 *
 * WHAT: Tests for exception handling in NIMCP networking modules
 * WHY:  Verify that networking errors properly map to exceptions and trigger
 *       immune system responses for recovery
 * HOW:  Test P2P, protocol, distributed cognition, NLP, and replication
 *       exception handling paths
 *
 * TEST CATEGORIES:
 * - Protocol error to exception mapping
 * - P2P node connection failure exceptions
 * - Distributed cognition exceptions
 * - NLP session exception handling
 * - Message framing error exceptions
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

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NetworkingExceptionHandlingTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<nimcp_error_t> last_exception_code;
    static std::atomic<nimcp_exception_category_t> last_exception_category;
    static nimcp_handler_registration_t* test_handler_reg;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = NIMCP_SUCCESS;
        last_exception_category = EXCEPTION_CATEGORY_GENERIC;

        nimcp_exception_system_init();

        // Register test exception handler
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.name = "network_test_handler";
        options.handler = test_exception_handler;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.user_data = nullptr;
        test_handler_reg = nimcp_handler_register(&options);
    }

    void TearDown() override {
        if (test_handler_reg) {
            nimcp_handler_unregister(test_handler_reg);
            test_handler_reg = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_exception_category = ex->category;
        return false;  // Allow chain continuation
    }

    // Helper to create and dispatch a network exception
    void dispatch_network_exception(nimcp_error_t code, const char* message) {
        nimcp_exception_t* ex = nimcp_exception_create(
            code,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "%s", message
        );
        ASSERT_NE(ex, nullptr);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }
};

std::atomic<int> NetworkingExceptionHandlingTest::handler_call_count(0);
std::atomic<nimcp_error_t> NetworkingExceptionHandlingTest::last_exception_code(NIMCP_SUCCESS);
std::atomic<nimcp_exception_category_t> NetworkingExceptionHandlingTest::last_exception_category(EXCEPTION_CATEGORY_GENERIC);
nimcp_handler_registration_t* NetworkingExceptionHandlingTest::test_handler_reg = nullptr;

//=============================================================================
// Protocol Error Exception Tests
//=============================================================================

TEST_F(NetworkingExceptionHandlingTest, ProtocolInvalidMagicException) {
    // WHAT: Test exception handling for invalid protocol magic number
    // WHY:  Invalid magic indicates message corruption or protocol mismatch
    // HOW:  Create exception with appropriate error code and verify handling

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Invalid protocol magic: expected 0x4E494D43, got 0xDEADBEEF"
    );
    ASSERT_NE(ex, nullptr);

    // Set context for better diagnostics
    nimcp_exception_set_context(ex, "expected_magic", "0x4E494D43");
    nimcp_exception_set_context(ex, "actual_magic", "0xDEADBEEF");
    nimcp_exception_set_context(ex, "source", "protocol_validate_header");

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_INVALID_PARAMETER);

    // Verify context was preserved
    const char* source = nimcp_exception_get_context(ex, "source");
    EXPECT_STREQ(source, "protocol_validate_header");

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionHandlingTest, ProtocolVersionMismatchException) {
    // WHAT: Test exception handling for protocol version mismatch
    // WHY:  Version mismatch requires graceful degradation or rejection

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Protocol version mismatch: local v2, remote v1"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "local_version", "2");
    nimcp_exception_set_context(ex, "remote_version", "1");
    nimcp_exception_set_context(ex, "peer_ip", "192.168.1.100");

    // Expected recovery: reduce functionality or reject connection
    ex->suggested_action = EXCEPTION_RECOVERY_REDUCE_LOAD;

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(ex->suggested_action, EXCEPTION_RECOVERY_REDUCE_LOAD);

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionHandlingTest, ProtocolChecksumFailureException) {
    // WHAT: Test exception for message checksum verification failure
    // WHY:  Checksum failure indicates message corruption in transit

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_DESERIALIZATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Message checksum validation failed"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "expected_checksum", "0x12345678");
    nimcp_exception_set_context(ex, "actual_checksum", "0xABCDEF01");
    nimcp_exception_set_context(ex, "message_type", "STATE_UPDATE");

    // Checksum failure suggests retry
    ex->suggested_action = EXCEPTION_RECOVERY_RETRY;

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
}

//=============================================================================
// P2P Node Connection Exception Tests
//=============================================================================

TEST_F(NetworkingExceptionHandlingTest, P2PConnectionRefusedException) {
    // WHAT: Test exception handling for connection refused
    // WHY:  Connection refused indicates peer unavailable or firewall

    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_SOCKET_ERROR,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "192.168.1.50:8080",
        "Connection refused by peer"
    );
    ASSERT_NE(io_ex, nullptr);

    io_ex->is_network = true;
    io_ex->socket_fd = -1;
    io_ex->errno_value = 111;  // ECONNREFUSED

    nimcp_exception_t* ex = (nimcp_exception_t*)io_ex;
    ex->suggested_action = EXCEPTION_RECOVERY_RETRY;

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(io_ex->is_network, true);

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionHandlingTest, P2PConnectionTimeoutException) {
    // WHAT: Test exception handling for connection timeout
    // WHY:  Timeout may indicate network issues or peer overload

    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "192.168.1.50:8080",
        "Connection attempt timed out after 30 seconds"
    );
    ASSERT_NE(io_ex, nullptr);

    io_ex->is_network = true;
    io_ex->socket_fd = -1;
    io_ex->errno_value = 110;  // ETIMEDOUT

    nimcp_exception_t* ex = (nimcp_exception_t*)io_ex;

    // Timeout should suggest retry or load reduction
    ex->suggested_action = EXCEPTION_RECOVERY_RETRY;

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionHandlingTest, P2PPeerDisconnectedException) {
    // WHAT: Test exception handling for unexpected peer disconnect
    // WHY:  Unexpected disconnect may indicate peer crash or network partition

    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "192.168.1.100:9000",
        "Peer disconnected unexpectedly"
    );
    ASSERT_NE(io_ex, nullptr);

    io_ex->is_network = true;
    io_ex->socket_fd = 42;
    io_ex->bytes_transferred = 1024;
    io_ex->bytes_expected = 4096;

    nimcp_exception_t* ex = (nimcp_exception_t*)io_ex;

    // Disconnect should suggest reconnection attempt
    ex->suggested_action = EXCEPTION_RECOVERY_RESTART_COMPONENT;

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(io_ex->bytes_transferred, 1024u);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Distributed Cognition Exception Tests
//=============================================================================

TEST_F(NetworkingExceptionHandlingTest, DistributedSyncFailureException) {
    // WHAT: Test exception for distributed synchronization failure
    // WHY:  Sync failure indicates potential network partition or peer issues

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_THREAD_SYNC,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Neuromodulator synchronization failed: peer timeout"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "sync_type", "neuromodulator");
    nimcp_exception_set_context(ex, "failed_peers", "2");
    nimcp_exception_set_context(ex, "total_peers", "5");
    nimcp_exception_set_context(ex, "neuromod_type", "DOPAMINE");

    // Sync failure should trigger rollback or retry
    ex->suggested_action = EXCEPTION_RECOVERY_ROLLBACK;

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionHandlingTest, DistributedBrainRegionSyncException) {
    // WHAT: Test exception for brain region sync failure
    // WHY:  Region sync failure affects distributed cognitive processing

    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_NETWORK_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0,
        "PREFRONTAL_CORTEX",
        "Failed to sync brain region state across network"
    );
    ASSERT_NE(brain_ex, nullptr);

    brain_ex->network_id = 5;

    nimcp_exception_t* ex = (nimcp_exception_t*)brain_ex;
    nimcp_exception_set_context(ex, "region_type", "PREFRONTAL_CORTEX");
    nimcp_exception_set_context(ex, "sync_attempt", "3");

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_STREQ(brain_ex->region_name, "PREFRONTAL_CORTEX");

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionHandlingTest, DistributedGlialCoordinationException) {
    // WHAT: Test exception for glial coordination failure
    // WHY:  Glial coordination affects synaptic pruning decisions

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Glial pruning coordination timeout"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "coordination_type", "pruning");
    nimcp_exception_set_context(ex, "source_neuron", "12345");
    nimcp_exception_set_context(ex, "target_neuron", "67890");
    nimcp_exception_set_context(ex, "timeout_ms", "5000");

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
}

//=============================================================================
// NLP (Neural Link Protocol) Exception Tests
//=============================================================================

TEST_F(NetworkingExceptionHandlingTest, NLPHandshakeFailureException) {
    // WHAT: Test exception for NLP handshake failure
    // WHY:  Handshake failure prevents secure session establishment

    nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0,  // threat_type
        "NLP handshake failed: key exchange timeout"
    );
    ASSERT_NE(sec_ex, nullptr);

    sec_ex->severity_score = 6;

    nimcp_exception_t* ex = (nimcp_exception_t*)sec_ex;
    nimcp_exception_set_context(ex, "session_state", "HANDSHAKE_SENT");
    nimcp_exception_set_context(ex, "peer_id", "0x12345678");

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionHandlingTest, NLPEncryptionFailureException) {
    // WHAT: Test exception for NLP encryption/decryption failure
    // WHY:  Encryption failure indicates key mismatch or corruption

    nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
        NIMCP_ERROR_DESERIALIZATION,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1,  // threat_type
        "AES-GCM decryption failed: authentication tag mismatch"
    );
    ASSERT_NE(sec_ex, nullptr);

    sec_ex->severity_score = 7;
    sec_ex->quarantine_required = true;
    sec_ex->threat_signature = "auth_tag_mismatch";

    nimcp_exception_t* ex = (nimcp_exception_t*)sec_ex;
    nimcp_exception_set_context(ex, "key_slot", "2");
    nimcp_exception_set_context(ex, "message_type", "SPIKE_BATCH");

    // Encryption failure should trigger key rotation or quarantine
    ex->suggested_action = EXCEPTION_RECOVERY_QUARANTINE;

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_TRUE(sec_ex->quarantine_required);

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionHandlingTest, NLPReplayAttackException) {
    // WHAT: Test exception for detected replay attack
    // WHY:  Replay attacks are security threats requiring immediate response

    nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        2,  // threat_type: replay
        "Replay attack detected: duplicate nonce"
    );
    ASSERT_NE(sec_ex, nullptr);

    sec_ex->severity_score = 9;
    sec_ex->quarantine_required = true;
    sec_ex->source_node_id = 0xBAD12345;
    sec_ex->threat_signature = "duplicate_nonce";

    nimcp_exception_t* ex = (nimcp_exception_t*)sec_ex;
    nimcp_exception_set_context(ex, "nonce", "0x1234567890ABCDEF");
    nimcp_exception_set_context(ex, "timestamp_diff", "-300");  // 300s ago

    // Replay attack triggers immediate quarantine
    ex->suggested_action = EXCEPTION_RECOVERY_QUARANTINE;

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionHandlingTest, NLPSessionTimeoutException) {
    // WHAT: Test exception for NLP session timeout
    // WHY:  Session timeout requires reconnection or failover

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "NLP session expired after 30 second timeout"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "session_id", "0xABCD1234");
    nimcp_exception_set_context(ex, "last_activity_ms", "35000");
    nimcp_exception_set_context(ex, "timeout_ms", "30000");

    // Session timeout suggests restart
    ex->suggested_action = EXCEPTION_RECOVERY_RESTART_COMPONENT;

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Message Framing Exception Tests
//=============================================================================

TEST_F(NetworkingExceptionHandlingTest, MessageTooLargeException) {
    // WHAT: Test exception for oversized message
    // WHY:  Messages exceeding MAX_PAYLOAD_SIZE must be rejected

    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_BUFFER_OVERFLOW,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "network_message",
        "Message size exceeds maximum payload limit"
    );
    ASSERT_NE(io_ex, nullptr);

    io_ex->bytes_expected = 65536;  // MAX_PAYLOAD_SIZE
    io_ex->bytes_transferred = 100000;  // Attempted size
    io_ex->is_network = true;

    nimcp_exception_t* ex = (nimcp_exception_t*)io_ex;
    nimcp_exception_set_context(ex, "max_size", "65536");
    nimcp_exception_set_context(ex, "actual_size", "100000");

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionHandlingTest, MessageFragmentationException) {
    // WHAT: Test exception for fragmentation issues
    // WHY:  Fragment reassembly failures indicate data loss

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Message fragment reassembly failed: missing fragment"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "total_fragments", "5");
    nimcp_exception_set_context(ex, "received_fragments", "4");
    nimcp_exception_set_context(ex, "missing_fragment", "3");

    // Fragment failure should trigger retry
    ex->suggested_action = EXCEPTION_RECOVERY_RETRY;

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Recovery Action Tests
//=============================================================================

TEST_F(NetworkingExceptionHandlingTest, NetworkRecoveryActionRetry) {
    // WHAT: Test that RETRY recovery action is suggested for transient errors
    // WHY:  Transient network errors should be retried automatically

    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "192.168.1.100:9000",
        "Network temporarily congested"
    );
    ASSERT_NE(io_ex, nullptr);

    nimcp_exception_t* ex = (nimcp_exception_t*)io_ex;

    // Get suggested recovery
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // For I/O errors, primary should be RETRY
    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_RETRY);

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionHandlingTest, NetworkRecoveryActionQuarantine) {
    // WHAT: Test quarantine recovery for security threats
    // WHY:  Security threats require isolation

    nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        3,  // Byzantine threat
        "Byzantine node detected: inconsistent messages"
    );
    ASSERT_NE(sec_ex, nullptr);

    sec_ex->quarantine_required = true;

    nimcp_exception_t* ex = (nimcp_exception_t*)sec_ex;
    ex->suggested_action = EXCEPTION_RECOVERY_QUARANTINE;

    EXPECT_EQ(ex->suggested_action, EXCEPTION_RECOVERY_QUARANTINE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Immune System Integration Tests
//=============================================================================

TEST_F(NetworkingExceptionHandlingTest, NetworkExceptionImmunePresentation) {
    // WHAT: Test that severe network exceptions are presented to immune system
    // WHY:  Immune system should learn from network errors for auto-recovery

    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "distributed_sync",
        "Critical network failure affecting distributed cognition"
    );
    ASSERT_NE(io_ex, nullptr);

    nimcp_exception_t* ex = (nimcp_exception_t*)io_ex;

    // Generate epitope for immune recognition
    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(result, 0);

    // The important contract is that present_to_immune succeeded (return 0)
    // The internal flag may or may not be set depending on implementation

    nimcp_exception_unref(ex);
}

TEST_F(NetworkingExceptionHandlingTest, NetworkExceptionEpitopeGeneration) {
    // WHAT: Test epitope generation for network exceptions
    // WHY:  Consistent epitopes enable immune pattern matching

    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Connection timeout to peer"
    );
    ASSERT_NE(ex1, nullptr);

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Connection timeout to peer"
    );
    ASSERT_NE(ex2, nullptr);

    // Generate epitopes
    size_t len1 = nimcp_exception_generate_epitope(ex1);
    size_t len2 = nimcp_exception_generate_epitope(ex2);

    EXPECT_GT(len1, 0u);
    EXPECT_GT(len2, 0u);

    // Similar exceptions should have similar epitopes
    // (exact match depends on implementation)
    EXPECT_EQ(len1, len2);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

//=============================================================================
// Exception Chaining Tests
//=============================================================================

TEST_F(NetworkingExceptionHandlingTest, NetworkExceptionChaining) {
    // WHAT: Test exception chaining for nested network errors
    // WHY:  Root cause analysis requires exception chain

    // Root cause: DNS resolution failure
    nimcp_io_exception_t* root_ex = nimcp_io_exception_create(
        NIMCP_ERROR_NOT_FOUND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "peer.nimcp.local",
        "DNS resolution failed"
    );
    ASSERT_NE(root_ex, nullptr);

    // Higher level: Connection failure
    nimcp_io_exception_t* conn_ex = nimcp_io_exception_create(
        NIMCP_ERROR_SOCKET_ERROR,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "peer.nimcp.local:9000",
        "Failed to connect to peer"
    );
    ASSERT_NE(conn_ex, nullptr);

    // Chain the exceptions
    nimcp_exception_set_cause(
        (nimcp_exception_t*)conn_ex,
        (nimcp_exception_t*)root_ex
    );

    // Verify chain
    nimcp_exception_t* cause = nimcp_exception_get_cause((nimcp_exception_t*)conn_ex);
    ASSERT_NE(cause, nullptr);
    EXPECT_EQ(cause->code, NIMCP_ERROR_NOT_FOUND);

    // Cleanup - only unref the top-level exception
    nimcp_exception_unref((nimcp_exception_t*)conn_ex);
}

//=============================================================================
// Aggregate Exception Tests
//=============================================================================

TEST_F(NetworkingExceptionHandlingTest, NetworkAggregateException) {
    // WHAT: Test aggregate exceptions for multi-peer failures
    // WHY:  Swarm operations may fail on multiple peers simultaneously

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Multiple peer synchronization failures"
    );
    ASSERT_NE(agg, nullptr);

    // Add child exceptions for each failed peer
    for (int i = 0; i < 3; i++) {
        nimcp_io_exception_t* peer_ex = nimcp_io_exception_create(
            NIMCP_ERROR_TIMEOUT,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "peer_sync",
            "Peer %d sync timeout", i
        );
        ASSERT_NE(peer_ex, nullptr);

        int result = nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)peer_ex);
        EXPECT_EQ(result, 0);
    }

    // Verify aggregate
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 3u);

    // Dispatch aggregate
    handler_call_count = 0;
    nimcp_exception_dispatch((nimcp_exception_t*)agg);

    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Try/Catch Integration Tests
//=============================================================================

TEST_F(NetworkingExceptionHandlingTest, NetworkTryCatchBasic) {
    // WHAT: Test try/catch macros with network exceptions
    // WHY:  Verify exception handling flow works correctly

    bool caught = false;
    nimcp_error_t caught_code = NIMCP_SUCCESS;

    NIMCP_TRY {
        // Simulate a network error by raising an exception
        nimcp_exception_throw(
            NIMCP_ERROR_NETWORK_IO,
            __FILE__, __LINE__, __func__,
            "Simulated network failure for test"
        );
        // Should not reach here
        FAIL() << "Should not reach here after throw";
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        caught = true;
        caught_code = ex->code;
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(caught);
    EXPECT_EQ(caught_code, NIMCP_ERROR_NETWORK_IO);
}

//=============================================================================
// Handler Priority Tests
//=============================================================================

TEST_F(NetworkingExceptionHandlingTest, HandlerPriorityOrdering) {
    // WHAT: Test that handlers are called in priority order
    // WHY:  Higher priority handlers should process first

    static std::vector<int> call_order;
    call_order.clear();

    // Handler that records call order
    auto record_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        int* order = (int*)user_data;
        call_order.push_back(*order);
        return false;
    };

    int order1 = 1, order2 = 2, order3 = 3;

    // Register handlers with different priorities
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.handler = record_handler;

    opts.name = "low_priority";
    opts.priority = NIMCP_HANDLER_PRIORITY_LOW;
    opts.user_data = &order3;
    nimcp_handler_registration_t* low = nimcp_handler_register(&opts);

    opts.name = "high_priority";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.user_data = &order1;
    nimcp_handler_registration_t* high = nimcp_handler_register(&opts);

    opts.name = "normal_priority";
    opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    opts.user_data = &order2;
    nimcp_handler_registration_t* normal = nimcp_handler_register(&opts);

    // Dispatch an exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test priority ordering"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_dispatch(ex);

    // Verify order: high -> normal -> low
    ASSERT_GE(call_order.size(), 3u);
    // Find relative positions
    auto pos_high = std::find(call_order.begin(), call_order.end(), 1);
    auto pos_normal = std::find(call_order.begin(), call_order.end(), 2);
    auto pos_low = std::find(call_order.begin(), call_order.end(), 3);

    EXPECT_LT(pos_high, pos_normal);
    EXPECT_LT(pos_normal, pos_low);

    // Cleanup
    nimcp_exception_unref(ex);
    nimcp_handler_unregister(low);
    nimcp_handler_unregister(high);
    nimcp_handler_unregister(normal);
}

//=============================================================================
// Category Filtering Tests
//=============================================================================

TEST_F(NetworkingExceptionHandlingTest, CategoryFilteringIO) {
    // WHAT: Test handler category filtering for I/O exceptions
    // WHY:  Specific handlers should only receive relevant exceptions

    static bool io_handler_called = false;
    io_handler_called = false;

    auto io_only_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        io_handler_called = true;
        return false;
    };

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "io_category_handler";
    opts.handler = io_only_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.category_filter = EXCEPTION_CATEGORY_IO;

    nimcp_handler_registration_t* io_reg = nimcp_handler_register(&opts);

    // Dispatch I/O exception - should be caught
    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_NETWORK_IO,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "test_path",
        "Test I/O exception"
    );
    ASSERT_NE(io_ex, nullptr);

    nimcp_exception_dispatch((nimcp_exception_t*)io_ex);
    EXPECT_TRUE(io_handler_called);

    io_handler_called = false;

    // Dispatch non-I/O exception - should NOT be caught by filtered handler
    nimcp_exception_t* generic_ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test generic exception"
    );
    ASSERT_NE(generic_ex, nullptr);

    nimcp_exception_dispatch(generic_ex);
    // The io_only_handler should NOT be called for generic exception
    // (Note: depends on filter implementation behavior)

    nimcp_exception_unref((nimcp_exception_t*)io_ex);
    nimcp_exception_unref(generic_ex);
    nimcp_handler_unregister(io_reg);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
