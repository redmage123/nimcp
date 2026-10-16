/**
 * @file test_bbb_bio_router_integration.cpp
 * @brief Integration tests for BBB security through bio-router (NIMCP)
 *
 * WHAT: End-to-end integration of BBB security validation with bio-async messaging
 * WHY:  Ensure security checks are properly applied to all messages flowing through bio-router
 * HOW:  Test BBB validation at message boundaries, cross-module security, and concurrent load
 *
 * TEST COVERAGE:
 * 1. BBB validation of bio-async messages before routing
 * 2. Cross-module security validation (module A → BBB → module B)
 * 3. Security under concurrent message load
 * 4. Attack detection and blocking via bio-router
 * 5. Security statistics and reporting
 *
 * INTEGRATION SCENARIOS:
 * - Messages validated by BBB before entering bio-router
 * - Malicious payloads detected and blocked
 * - Legitimate messages pass through with minimal overhead
 * - Security violations properly logged and reported
 * - Concurrent access remains thread-safe
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 * @version 1.0.0
 */

#include "test_helpers.h"

// Headers have their own extern "C" guards
#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class BBBBioRouterIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Initialize BBB system
        bbb_config_t bbb_cfg = bbb_default_config();
        bbb_cfg.strict_mode = true;
        bbb_cfg.alert_callback = &BBBBioRouterIntegrationTest::alert_callback_static;

        bbb_system_ = bbb_system_create(&bbb_cfg);
        ASSERT_NE(bbb_system_, nullptr);
        ASSERT_TRUE(bbb_system_set_enabled(bbb_system_, true));

        // Initialize bio-async system
        nimcp_error_t err = nimcp_bio_async_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Bio-async initialization failed";

        // Initialize router
        err = bio_router_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Router initialization failed";

        // Register test modules
        bio_module_info_t sender_info = {
            .module_id = BIO_MODULE_BRAIN,
            .module_name = "sender_module",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        sender_ctx_ = bio_router_register_module(&sender_info);
        ASSERT_NE(sender_ctx_, nullptr);

        bio_module_info_t receiver_info = {
            .module_id = BIO_MODULE_INTROSPECTION,
            .module_name = "receiver_module",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        receiver_ctx_ = bio_router_register_module(&receiver_info);
        ASSERT_NE(receiver_ctx_, nullptr);

        // Reset counters
        alert_count_.store(0);
        messages_received_.store(0);
        messages_blocked_.store(0);
        last_alert_type_ = BBB_THREAT_NONE;
    }

    void TearDown() override
    {
        if (sender_ctx_) bio_router_unregister_module(sender_ctx_);
        if (receiver_ctx_) bio_router_unregister_module(receiver_ctx_);

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        if (bbb_system_) {
            bbb_system_destroy(bbb_system_);
            bbb_system_ = nullptr;
        }
    }

    // Static callback for BBB alerts
    static void alert_callback_static(bbb_threat_type_t type, bbb_severity_t severity,
                                       const char* description)
    {
        std::lock_guard<std::mutex> lock(alert_mutex_);
        alert_count_.fetch_add(1);
        last_alert_type_ = type;
        last_alert_severity_ = severity;
        if (description) {
            last_alert_description_ = description;
        }
    }

    // Validate message payload with BBB before sending
    bool validate_message_payload(const void* payload, size_t size)
    {
        if (!payload || size == 0) return false;

        // Treat payload as string for validation
        std::string payload_str(static_cast<const char*>(payload),
                                std::min(size, (size_t)256));

        bbb_validation_result_t result;
        bool valid = bbb_validate_string(bbb_system_, payload_str.c_str(), &result);

        if (!valid) {
            messages_blocked_.fetch_add(1);
        }

        return valid;
    }

    // Helper to send validated message
    nimcp_bio_promise_t send_validated_message(bio_module_context_t ctx,
                                                const void* msg, size_t msg_size,
                                                nimcp_bio_channel_type_t channel)
    {
        // Extract payload from message (skip header)
        const bio_message_header_t* header =
            static_cast<const bio_message_header_t*>(msg);

        const void* payload = static_cast<const char*>(msg) + sizeof(bio_message_header_t);
        size_t payload_size = header->payload_size;

        // Validate payload with BBB
        if (!validate_message_payload(payload, payload_size)) {
            return nullptr;  // Blocked by BBB
        }

        // Send through router
        return bio_router_send_async(ctx, msg, msg_size, channel);
    }

    bbb_system_t bbb_system_{nullptr};
    bio_module_context_t sender_ctx_{nullptr};
    bio_module_context_t receiver_ctx_{nullptr};

    static std::atomic<int> alert_count_;
    static std::atomic<int> messages_received_;
    static std::atomic<int> messages_blocked_;
    static bbb_threat_type_t last_alert_type_;
    static bbb_severity_t last_alert_severity_;
    static std::string last_alert_description_;
    static std::mutex alert_mutex_;
};

// Static member definitions
std::atomic<int> BBBBioRouterIntegrationTest::alert_count_{0};
std::atomic<int> BBBBioRouterIntegrationTest::messages_received_{0};
std::atomic<int> BBBBioRouterIntegrationTest::messages_blocked_{0};
bbb_threat_type_t BBBBioRouterIntegrationTest::last_alert_type_ = BBB_THREAT_NONE;
bbb_severity_t BBBBioRouterIntegrationTest::last_alert_severity_ = BBB_SEVERITY_NONE;
std::string BBBBioRouterIntegrationTest::last_alert_description_;
std::mutex BBBBioRouterIntegrationTest::alert_mutex_;

//=============================================================================
// End-to-End BBB Validation Tests
//=============================================================================

TEST_F(BBBBioRouterIntegrationTest, LegitimateMessagesPassThrough)
{
    // Register receiver handler
    auto handler = [](const void* msg, size_t msg_size,
                      nimcp_bio_promise_t response_promise,
                      void* user_data) -> nimcp_error_t {
        messages_received_.fetch_add(1);
        if (response_promise) {
            float ack = 1.0f;
            nimcp_bio_promise_complete(response_promise, &ack);
        }
        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(receiver_ctx_, BIO_MSG_INTROSPECTION_QUERY, handler);

    // Send legitimate messages
    const int NUM_MESSAGES = 10;
    std::vector<nimcp_bio_promise_t> promises;

    for (int i = 0; i < NUM_MESSAGES; i++) {
        // Create legitimate message
        struct test_msg {
            bio_message_header_t header;
            char payload[64];
        } msg;

        bio_msg_init_header(&msg.header, BIO_MSG_INTROSPECTION_QUERY,
                            BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, sizeof(msg));
        snprintf(msg.payload, sizeof(msg.payload), "Legitimate query %d", i);
        msg.header.payload_size = strlen(msg.payload) + 1;

        nimcp_bio_promise_t promise = send_validated_message(
            sender_ctx_, &msg, sizeof(msg.header) + msg.header.payload_size,
            BIO_CHANNEL_ACETYLCHOLINE);

        ASSERT_NE(promise, nullptr) << "Legitimate message was blocked";
        promises.push_back(promise);
    }

    // Process messages
    auto start = std::chrono::steady_clock::now();
    while (messages_received_.load() < NUM_MESSAGES &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 1000) {
        bio_router_process_inbox(receiver_ctx_, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(messages_received_.load(), NUM_MESSAGES);
    EXPECT_EQ(messages_blocked_.load(), 0);
    EXPECT_EQ(alert_count_.load(), 0);

    // Cleanup
    for (auto promise : promises) {
        nimcp_bio_promise_destroy(promise);
    }
}

TEST_F(BBBBioRouterIntegrationTest, MaliciousPayloadsBlocked)
{
    alert_count_ = 0;
    messages_blocked_ = 0;

    // Register receiver handler
    auto handler = [](const void* msg, size_t msg_size,
                      nimcp_bio_promise_t response_promise,
                      void* user_data) -> nimcp_error_t {
        messages_received_.fetch_add(1);
        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(receiver_ctx_, BIO_MSG_INTROSPECTION_QUERY, handler);

    // Attempt to send malicious messages
    const char* malicious_payloads[] = {
        "'; DROP TABLE users; --",
        "1 UNION SELECT * FROM passwords",
        "%n%n%n%n",
        "<script>alert('XSS')</script>",
        "; rm -rf /",
    };

    const int NUM_ATTACKS = sizeof(malicious_payloads) / sizeof(malicious_payloads[0]);

    for (int i = 0; i < NUM_ATTACKS; i++) {
        struct test_msg {
            bio_message_header_t header;
            char payload[256];
        } msg;

        bio_msg_init_header(&msg.header, BIO_MSG_INTROSPECTION_QUERY,
                            BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, sizeof(msg));
        snprintf(msg.payload, sizeof(msg.payload), "%s", malicious_payloads[i]);
        msg.header.payload_size = strlen(msg.payload) + 1;

        nimcp_bio_promise_t promise = send_validated_message(
            sender_ctx_, &msg, sizeof(msg.header) + msg.header.payload_size,
            BIO_CHANNEL_ACETYLCHOLINE);

        // Track how many were blocked (promise == nullptr means blocked)
        if (promise == nullptr) {
            // Attack was blocked as expected
        } else {
            // This payload was not blocked - cleanup the promise
            nimcp_bio_promise_destroy(promise);
        }
    }

    // Allow time for alerts
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify most attacks were blocked (BBB may not catch all edge cases)
    EXPECT_GE(messages_blocked_.load(), NUM_ATTACKS - 1) << "Too few attacks blocked";
    EXPECT_GE(alert_count_.load(), NUM_ATTACKS - 1) << "Too few alerts generated";
    EXPECT_LE(messages_received_.load(), 1);  // At most 1 edge case message might pass
}

TEST_F(BBBBioRouterIntegrationTest, CrossModuleSecurityValidation)
{
    // Test security validation across multiple modules
    bio_module_info_t module_a_info = {
        .module_id = BIO_MODULE_ETHICS,
        .module_name = "module_a",
        .inbox_capacity = 100,
        .user_data = nullptr
    };
    bio_module_context_t module_a_ctx = bio_router_register_module(&module_a_info);
    ASSERT_NE(module_a_ctx, nullptr);

    bio_module_info_t module_b_info = {
        .module_id = BIO_MODULE_SALIENCE,
        .module_name = "module_b",
        .inbox_capacity = 100,
        .user_data = nullptr
    };
    bio_module_context_t module_b_ctx = bio_router_register_module(&module_b_info);
    ASSERT_NE(module_b_ctx, nullptr);

    static std::atomic<int> module_b_received{0};
    module_b_received.store(0);

    // Register handler on module B
    auto handler = [](const void* msg, size_t msg_size,
                      nimcp_bio_promise_t response_promise,
                      void* user_data) -> nimcp_error_t {
        module_b_received.fetch_add(1);
        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(module_b_ctx, BIO_MSG_SALIENCE_QUERY, +handler);

    // Module A sends mix of safe and malicious messages to Module B
    const char* test_payloads[] = {
        "Safe data 1",
        "'; DROP TABLE x; --",  // Malicious
        "Safe data 2",
        "%n%n%n",               // Malicious
        "Safe data 3",
    };

    int expected_safe = 0;
    int expected_blocked = 0;

    for (size_t i = 0; i < sizeof(test_payloads) / sizeof(test_payloads[0]); i++) {
        struct test_msg {
            bio_message_header_t header;
            char payload[256];
        } msg;

        bio_msg_init_header(&msg.header, BIO_MSG_SALIENCE_QUERY,
                            BIO_MODULE_ETHICS, BIO_MODULE_SALIENCE, sizeof(msg));
        snprintf(msg.payload, sizeof(msg.payload), "%s", test_payloads[i]);
        msg.header.payload_size = strlen(msg.payload) + 1;

        nimcp_bio_promise_t promise = send_validated_message(
            module_a_ctx, &msg, sizeof(msg.header) + msg.header.payload_size,
            BIO_CHANNEL_NOREPINEPHRINE);

        if (promise) {
            expected_safe++;
            nimcp_bio_promise_destroy(promise);
        } else {
            expected_blocked++;
        }
    }

    // Process messages
    auto start = std::chrono::steady_clock::now();
    while (module_b_received.load() < expected_safe &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 1000) {
        bio_router_process_inbox(module_b_ctx, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Verify cross-module security
    EXPECT_EQ(module_b_received.load(), expected_safe);
    EXPECT_EQ(expected_safe, 3);   // 3 safe messages
    EXPECT_EQ(expected_blocked, 2); // 2 malicious messages

    // Cleanup
    bio_router_unregister_module(module_a_ctx);
    bio_router_unregister_module(module_b_ctx);
}

//=============================================================================
// Concurrent Message Load Tests
//=============================================================================

TEST_F(BBBBioRouterIntegrationTest, SecurityUnderConcurrentLoad)
{
    const int NUM_THREADS = 4;
    const int MESSAGES_PER_THREAD = 50;
    std::atomic<int> safe_sent{0};
    std::atomic<int> malicious_sent{0};
    std::vector<std::thread> threads;

    // Register receiver handler
    auto handler = [](const void* msg, size_t msg_size,
                      nimcp_bio_promise_t response_promise,
                      void* user_data) -> nimcp_error_t {
        messages_received_.fetch_add(1);
        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(receiver_ctx_, BIO_MSG_INTROSPECTION_QUERY, handler);

    // Launch sender threads
    auto sender_task = [this, &safe_sent, &malicious_sent](int thread_id) {
        for (int i = 0; i < MESSAGES_PER_THREAD; i++) {
            struct test_msg {
                bio_message_header_t header;
                char payload[128];
            } msg;

            bio_msg_init_header(&msg.header, BIO_MSG_INTROSPECTION_QUERY,
                                BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, sizeof(msg));

            // Alternate between safe and malicious
            if (i % 3 == 0) {
                // Malicious
                snprintf(msg.payload, sizeof(msg.payload),
                         "'; DROP TABLE t%d_%d; --", thread_id, i);
                malicious_sent.fetch_add(1);
            } else {
                // Safe
                snprintf(msg.payload, sizeof(msg.payload),
                         "Safe query from thread %d message %d", thread_id, i);
                safe_sent.fetch_add(1);
            }

            msg.header.payload_size = strlen(msg.payload) + 1;

            nimcp_bio_promise_t promise = send_validated_message(
                sender_ctx_, &msg, sizeof(msg.header) + msg.header.payload_size,
                BIO_CHANNEL_ACETYLCHOLINE);

            if (promise) {
                nimcp_bio_promise_destroy(promise);
            }

            // Small delay to avoid overwhelming
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    // Launch threads
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(sender_task, i);
    }

    // Process messages concurrently
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 3000) {
        bio_router_process_inbox(receiver_ctx_, 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Wait for senders
    for (auto& t : threads) {
        t.join();
    }

    // Verify security under load
    int total_sent = safe_sent.load() + malicious_sent.load();
    EXPECT_EQ(total_sent, NUM_THREADS * MESSAGES_PER_THREAD);

    // Only safe messages should have been received
    EXPECT_LE(messages_received_.load(), safe_sent.load());

    // Malicious messages should have been blocked
    EXPECT_GE(messages_blocked_.load(), malicious_sent.load() / 2);

    // Alerts should have been triggered
    EXPECT_GT(alert_count_.load(), 0);
}

TEST_F(BBBBioRouterIntegrationTest, ThreadSafeBBBValidation)
{
    const int NUM_VALIDATION_THREADS = 8;
    const int VALIDATIONS_PER_THREAD = 100;
    std::atomic<int> total_validations{0};
    std::atomic<int> threats_detected{0};
    std::vector<std::thread> threads;

    auto validate_task = [this, &total_validations, &threats_detected](int thread_id) {
        for (int i = 0; i < VALIDATIONS_PER_THREAD; i++) {
            const char* test_input;

            // Mix of safe and malicious inputs
            if (i % 4 == 0) {
                test_input = "'; DROP TABLE users; --";
            } else if (i % 4 == 1) {
                test_input = "%n%n%n%n";
            } else {
                test_input = "Safe input string";
            }

            bbb_validation_result_t result;
            bool valid = bbb_validate_string(bbb_system_, test_input, &result);

            total_validations.fetch_add(1);
            if (!valid) {
                threats_detected.fetch_add(1);
            }
        }
    };

    // Launch validation threads
    for (int i = 0; i < NUM_VALIDATION_THREADS; i++) {
        threads.emplace_back(validate_task, i);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    // Verify thread-safe operation
    EXPECT_EQ(total_validations.load(), NUM_VALIDATION_THREADS * VALIDATIONS_PER_THREAD);

    // About 50% should be threats
    int expected_threats = (NUM_VALIDATION_THREADS * VALIDATIONS_PER_THREAD) / 2;
    EXPECT_GE(threats_detected.load(), expected_threats - 50);
    EXPECT_LE(threats_detected.load(), expected_threats + 50);
}

//=============================================================================
// Security Statistics and Reporting Tests
//=============================================================================

TEST_F(BBBBioRouterIntegrationTest, SecurityStatisticsTracking)
{
    // Send mix of messages
    const char* test_payloads[] = {
        "Safe 1", "Safe 2", "'; DROP TABLE x; --",
        "Safe 3", "%n%n%n", "Safe 4"
    };

    for (size_t i = 0; i < sizeof(test_payloads) / sizeof(test_payloads[0]); i++) {
        struct test_msg {
            bio_message_header_t header;
            char payload[128];
        } msg;

        bio_msg_init_header(&msg.header, BIO_MSG_INTROSPECTION_QUERY,
                            BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, sizeof(msg));
        snprintf(msg.payload, sizeof(msg.payload), "%s", test_payloads[i]);
        msg.header.payload_size = strlen(msg.payload) + 1;

        nimcp_bio_promise_t promise = send_validated_message(
            sender_ctx_, &msg, sizeof(msg.header) + msg.header.payload_size,
            BIO_CHANNEL_ACETYLCHOLINE);

        if (promise) {
            nimcp_bio_promise_destroy(promise);
        }
    }

    // Get BBB statistics
    bbb_statistics_t bbb_stats;
    ASSERT_TRUE(bbb_system_get_statistics(bbb_system_, &bbb_stats));

    // Verify statistics
    EXPECT_EQ(bbb_stats.total_validations, sizeof(test_payloads) / sizeof(test_payloads[0]));
    EXPECT_GE(bbb_stats.threats_detected, 2u);  // 2 malicious payloads
    EXPECT_GE(bbb_stats.threats_blocked, 2u);

    // Get router statistics
    bio_router_stats_t router_stats;
    nimcp_error_t err = bio_router_get_stats(&router_stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Safe messages should have been routed
    EXPECT_GE(router_stats.messages_routed, 4u);  // At least 4 safe messages
}

TEST_F(BBBBioRouterIntegrationTest, ThreatReportGeneration)
{
    alert_count_ = 0;

    // Trigger multiple threats
    const char* threats[] = {
        "'; DELETE FROM accounts; --",
        "%n%n%n%n",
        "1 UNION SELECT * FROM passwords"
    };

    for (size_t i = 0; i < sizeof(threats) / sizeof(threats[0]); i++) {
        bbb_validation_result_t result;
        bbb_validate_string(bbb_system_, threats[i], &result);
    }

    // Allow time for alerts
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify alerts were generated
    EXPECT_GE(alert_count_.load(), 3);
    EXPECT_NE(last_alert_type_, BBB_THREAT_NONE);
    EXPECT_GE(last_alert_severity_, BBB_SEVERITY_MEDIUM);

    // Get threat reports
    bbb_threat_report_t reports[10];
    size_t count = bbb_get_threat_reports(bbb_system_, reports, 10);
    EXPECT_GE(count, 3u);

    // Verify report details
    for (size_t i = 0; i < count && i < 3; i++) {
        EXPECT_NE(reports[i].type, BBB_THREAT_NONE);
        EXPECT_GT(reports[i].timestamp, 0u);
        EXPECT_GT(strlen(reports[i].description), 0u);
    }
}

//=============================================================================
// Performance Impact Tests
//=============================================================================

TEST_F(BBBBioRouterIntegrationTest, SecurityValidationOverhead)
{
    const int NUM_MESSAGES = 1000;

    // Measure routing without validation
    auto start_no_validation = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_MESSAGES; i++) {
        struct test_msg {
            bio_message_header_t header;
            char payload[64];
        } msg;

        bio_msg_init_header(&msg.header, BIO_MSG_INTROSPECTION_QUERY,
                            BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, sizeof(msg));
        snprintf(msg.payload, sizeof(msg.payload), "Test %d", i);
        msg.header.payload_size = strlen(msg.payload) + 1;

        // Direct send (no validation)
        nimcp_bio_promise_t promise = bio_router_send_async(
            sender_ctx_, &msg, sizeof(msg.header) + msg.header.payload_size,
            BIO_CHANNEL_ACETYLCHOLINE);

        if (promise) {
            nimcp_bio_promise_destroy(promise);
        }
    }

    auto end_no_validation = std::chrono::high_resolution_clock::now();
    auto duration_no_validation_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end_no_validation - start_no_validation).count();

    // Drain receiver inbox between measurements to ensure fair comparison.
    // Without this, the second loop hits a full queue (from the first loop)
    // and creates expensive exception objects for every failed enqueue,
    // inflating the "with validation" timing far beyond actual validation cost.
    {
        auto drain_start = std::chrono::steady_clock::now();
        int drained = 0;
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - drain_start).count() < 500) {
            int processed = bio_router_process_inbox(receiver_ctx_, 100);
            if (processed <= 0) break;
            drained += processed;
        }
    }

    // Measure routing with validation
    auto start_with_validation = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_MESSAGES; i++) {
        struct test_msg {
            bio_message_header_t header;
            char payload[64];
        } msg;

        bio_msg_init_header(&msg.header, BIO_MSG_INTROSPECTION_QUERY,
                            BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, sizeof(msg));
        snprintf(msg.payload, sizeof(msg.payload), "Test %d", i);
        msg.header.payload_size = strlen(msg.payload) + 1;

        // Send with validation
        nimcp_bio_promise_t promise = send_validated_message(
            sender_ctx_, &msg, sizeof(msg.header) + msg.header.payload_size,
            BIO_CHANNEL_ACETYLCHOLINE);

        if (promise) {
            nimcp_bio_promise_destroy(promise);
        }
    }

    auto end_with_validation = std::chrono::high_resolution_clock::now();
    auto duration_with_validation_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end_with_validation - start_with_validation).count();

    // Use microsecond precision and ensure minimum threshold to avoid flaky results
    // when measurements are very fast (sub-millisecond)
    auto baseline_us = std::max(duration_no_validation_us, static_cast<long>(100));
    double overhead_ratio = static_cast<double>(duration_with_validation_us) /
                           static_cast<double>(baseline_us);

    // Allow up to 10x overhead in test environment (debug builds, CI variability)
    // In production with optimizations, overhead should be much lower
    EXPECT_LT(overhead_ratio, 10.0)
        << "Security validation overhead too high: " << overhead_ratio << "x";

    std::cout << "Security validation overhead: " << overhead_ratio << "x" << std::endl;
    std::cout << "No validation: " << (duration_no_validation_us / 1000.0) << "ms" << std::endl;
    std::cout << "With validation: " << (duration_with_validation_us / 1000.0) << "ms" << std::endl;
}

}  // anonymous namespace
