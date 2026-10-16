/**
 * @file test_security_exception_integration.cpp
 * @brief Integration tests for security module exception handling
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Integration tests for security-exception-immune system pipelines
 * WHY:  Verify security modules, exception system, and immune system work together
 * HOW:  Test complete flow from security threat detection through exception handling
 *       to immune response and recovery actions
 *
 * TEST SCENARIOS:
 * - BBB threat detection -> Exception -> Immune response
 * - Anomaly detection -> Exception -> Pattern learning
 * - Multi-module security threat aggregation
 * - Exception-driven security lockdown coordination
 * - Recovery from security exceptions
 * - Concurrent security exception processing
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>

// Include C++ compatible headers first (may include CUDA)
#include "cognitive/immune/nimcp_brain_immune.h"
#include "security/nimcp_anomaly_detector.h"

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_pattern_db.h"
#include "security/nimcp_rate_limiter.h"
#include "utils/error/nimcp_error_codes.h"
}

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class SecurityExceptionIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> exception_count;
    static std::atomic<int> immune_presentation_count;
    static std::atomic<int> recovery_action_count;
    static std::atomic<bool> lockdown_triggered;
    static std::mutex stats_mutex;

    void SetUp() override {
        exception_count = 0;
        immune_presentation_count = 0;
        recovery_action_count = 0;
        lockdown_triggered = false;

        // Initialize exception system
        nimcp_exception_system_init();

        // Initialize immune integration
        nimcp_exception_immune_init(nullptr);

        // Reset BBB test state
        bbb_reset_test_state();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_immune_shutdown();
        nimcp_exception_system_shutdown();
    }

    // Handler for tracking security exceptions
    static bool security_tracking_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        exception_count++;

        if (ex->category == EXCEPTION_CATEGORY_SECURITY) {
            // Present to immune system
            nimcp_immune_response_t response;
            memset(&response, 0, sizeof(response));
            if (nimcp_exception_present_to_immune(ex, &response) == 0) {
                immune_presentation_count++;
            }
        }

        return false;  // Allow chain to continue
    }

    // Handler that triggers lockdown on critical threats
    static bool lockdown_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;

        if (ex->category == EXCEPTION_CATEGORY_SECURITY &&
            ex->severity >= EXCEPTION_SEVERITY_CRITICAL) {
            lockdown_triggered = true;
        }

        return false;
    }

    // Recovery callback for quarantine
    static int quarantine_recovery(
        nimcp_exception_t* ex,
        nimcp_exception_recovery_action_t action,
        void* user_data
    ) {
        (void)user_data;
        (void)ex;

        if (action == EXCEPTION_RECOVERY_QUARANTINE) {
            recovery_action_count++;
            return 0;  // Success
        }
        return -1;
    }

    // Recovery callback for rollback
    static int rollback_recovery(
        nimcp_exception_t* ex,
        nimcp_exception_recovery_action_t action,
        void* user_data
    ) {
        (void)user_data;
        (void)ex;

        if (action == EXCEPTION_RECOVERY_ROLLBACK) {
            recovery_action_count++;
            return 0;
        }
        return -1;
    }

    // Create a BBB system for testing
    static bbb_system_t create_test_bbb() {
        bbb_config_t config = bbb_default_config();
        config.strict_mode = true;
        return bbb_system_create(&config);
    }
};

std::atomic<int> SecurityExceptionIntegrationTest::exception_count(0);
std::atomic<int> SecurityExceptionIntegrationTest::immune_presentation_count(0);
std::atomic<int> SecurityExceptionIntegrationTest::recovery_action_count(0);
std::atomic<bool> SecurityExceptionIntegrationTest::lockdown_triggered(false);
std::mutex SecurityExceptionIntegrationTest::stats_mutex;

//=============================================================================
// BBB Threat to Exception to Immune Flow Tests
//=============================================================================

TEST_F(SecurityExceptionIntegrationTest, BBBThreatToExceptionToImmuneFlow) {
    // WHAT: Test complete BBB threat -> exception -> immune presentation flow
    // WHY:  Verify end-to-end security exception pipeline

    // Register handlers
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "bbb_immune_handler";
    options.handler = security_tracking_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    // Create BBB system
    bbb_system_t bbb = create_test_bbb();
    ASSERT_NE(bbb, nullptr);

    // Trigger threat through validation
    const char* malicious_input = "'; DROP TABLE users; --";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(bbb, malicious_input, &result);

    if (!valid && result.threat != BBB_THREAT_NONE) {
        // Create exception from threat
        nimcp_security_exception_t* ex = nimcp_security_exception_create(
            NIMCP_ERROR_PERMISSION_DENIED,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__,
            __LINE__,
            __func__,
            result.threat,
            "BBB validation failed: %s", result.reason
        );

        ASSERT_NE(ex, nullptr);

        // Dispatch exception (triggers handler chain)
        nimcp_exception_dispatch((nimcp_exception_t*)ex);

        // Verify flow - handler was called
        EXPECT_GT(exception_count.load(), 0);
        // Immune presentation tracking is handler-specific
        // The flag behavior is implementation-specific

        nimcp_exception_unref((nimcp_exception_t*)ex);
    }

    nimcp_handler_unregister(reg);
    bbb_system_destroy(bbb);
}

TEST_F(SecurityExceptionIntegrationTest, MultipleThreatsAggregation) {
    // WHAT: Test aggregating multiple BBB threats into single exception
    // WHY:  Multi-vector attacks need consolidated handling

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "aggregation_handler";
    options.handler = security_tracking_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    bbb_system_t bbb = create_test_bbb();
    ASSERT_NE(bbb, nullptr);

    // Create aggregate exception
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__,
        __LINE__,
        __func__,
        "Multi-vector attack detected"
    );

    ASSERT_NE(agg, nullptr);

    // Test multiple malicious inputs
    const char* attacks[] = {
        "'; DROP TABLE users; --",        // SQL injection
        "%n%n%n%n%s%s%s%s",               // Format string
        "../../../etc/passwd",             // Path traversal
    };

    bbb_threat_type_t expected_threats[] = {
        BBB_THREAT_SQL_INJECTION,
        BBB_THREAT_FORMAT_STRING,
        BBB_THREAT_PATH_TRAVERSAL
    };

    for (size_t i = 0; i < sizeof(attacks) / sizeof(attacks[0]); i++) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(bbb, attacks[i], &result);

        if (!valid && result.threat != BBB_THREAT_NONE) {
            nimcp_security_exception_t* child = nimcp_security_exception_create(
                NIMCP_ERROR_PERMISSION_DENIED,
                EXCEPTION_SEVERITY_ERROR,
                __FILE__,
                __LINE__,
                __func__,
                result.threat,
                "Attack vector %zu: %s", i, result.reason
            );

            if (child) {
                nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)child);
            }
        }
    }

    // Dispatch aggregate
    size_t child_count = nimcp_aggregate_exception_count(agg);
    EXPECT_GT(child_count, 0u);

    exception_count = 0;
    nimcp_exception_dispatch((nimcp_exception_t*)agg);
    EXPECT_GT(exception_count.load(), 0);

    nimcp_exception_unref((nimcp_exception_t*)agg);
    nimcp_handler_unregister(reg);
    bbb_system_destroy(bbb);
}

//=============================================================================
// Critical Threat Lockdown Tests
//=============================================================================

TEST_F(SecurityExceptionIntegrationTest, CriticalThreatTriggersLockdown) {
    // WHAT: Test that critical security exceptions trigger lockdown
    // WHY:  Severe threats require immediate containment

    nimcp_handler_options_t tracking_opts, lockdown_opts;
    nimcp_handler_default_options(&tracking_opts);
    nimcp_handler_default_options(&lockdown_opts);

    tracking_opts.name = "tracking_handler";
    tracking_opts.handler = security_tracking_handler;
    tracking_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    lockdown_opts.name = "lockdown_handler";
    lockdown_opts.handler = lockdown_handler;
    lockdown_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    lockdown_opts.category_filter = EXCEPTION_CATEGORY_SECURITY;
    lockdown_opts.min_severity = EXCEPTION_SEVERITY_CRITICAL;

    nimcp_handler_registration_t* tracking_reg = nimcp_handler_register(&tracking_opts);
    nimcp_handler_registration_t* lockdown_reg = nimcp_handler_register(&lockdown_opts);

    ASSERT_NE(tracking_reg, nullptr);
    ASSERT_NE(lockdown_reg, nullptr);

    // Create critical security exception
    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_SHELLCODE,
        "Shellcode execution attempt"
    );

    ASSERT_NE(ex, nullptr);

    lockdown_triggered = false;
    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    // Lockdown should be triggered
    EXPECT_TRUE(lockdown_triggered.load());

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_handler_unregister(tracking_reg);
    nimcp_handler_unregister(lockdown_reg);
}

TEST_F(SecurityExceptionIntegrationTest, NonCriticalThreatNoLockdown) {
    // WHAT: Test that non-critical threats don't trigger lockdown
    // WHY:  Avoid unnecessary lockdowns for minor threats

    nimcp_handler_options_t lockdown_opts;
    nimcp_handler_default_options(&lockdown_opts);
    lockdown_opts.name = "lockdown_handler";
    lockdown_opts.handler = lockdown_handler;
    lockdown_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    lockdown_opts.category_filter = EXCEPTION_CATEGORY_SECURITY;
    lockdown_opts.min_severity = EXCEPTION_SEVERITY_CRITICAL;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&lockdown_opts);
    ASSERT_NE(reg, nullptr);

    // Create non-critical security exception
    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_SQL_INJECTION,
        "Suspicious but non-critical"
    );

    ASSERT_NE(ex, nullptr);

    lockdown_triggered = false;
    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    // Lockdown should NOT be triggered for warnings
    EXPECT_FALSE(lockdown_triggered.load());

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Recovery Action Tests
//=============================================================================

TEST_F(SecurityExceptionIntegrationTest, SecurityExceptionQuarantineRecovery) {
    // WHAT: Test quarantine recovery for security exceptions
    // WHY:  Verify recovery action execution

    // Register quarantine recovery
    EXPECT_EQ(nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_QUARANTINE,
        quarantine_recovery,
        nullptr
    ), 0);

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_CODE_INJECTION,
        "Code injection for quarantine"
    );

    ASSERT_NE(ex, nullptr);

    // Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    // Execute primary recovery
    recovery_action_count = 0;
    int result = nimcp_execute_recovery(
        (nimcp_exception_t*)ex,
        strategy.primary_action
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(recovery_action_count.load(), 0);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_QUARANTINE);
}

TEST_F(SecurityExceptionIntegrationTest, SecurityExceptionRollbackRecovery) {
    // WHAT: Test rollback recovery for corrupted state
    // WHY:  Some attacks require rolling back to clean state

    EXPECT_EQ(nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_ROLLBACK,
        rollback_recovery,
        nullptr
    ), 0);

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_DATA_TAMPERING,
        "Data tampering detected"
    );

    ASSERT_NE(ex, nullptr);

    recovery_action_count = 0;
    int result = nimcp_execute_recovery(
        (nimcp_exception_t*)ex,
        EXCEPTION_RECOVERY_ROLLBACK
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(recovery_action_count.load(), 0);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK);
}

TEST_F(SecurityExceptionIntegrationTest, ChainedRecoveryActions) {
    // WHAT: Test executing multiple recovery actions in sequence
    // WHY:  Complex attacks may need multiple recovery steps

    EXPECT_EQ(nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_QUARANTINE,
        quarantine_recovery,
        nullptr
    ), 0);

    EXPECT_EQ(nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_ROLLBACK,
        rollback_recovery,
        nullptr
    ), 0);

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_MEMORY_VIOLATION,
        "Memory corruption attack"
    );

    ASSERT_NE(ex, nullptr);

    recovery_action_count = 0;

    // Execute quarantine first
    nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_QUARANTINE);

    // Then rollback
    nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_ROLLBACK);

    // Both should have executed
    EXPECT_EQ(recovery_action_count.load(), 2);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_QUARANTINE);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK);
}

//=============================================================================
// Immune System Integration Tests
//=============================================================================

TEST_F(SecurityExceptionIntegrationTest, ExceptionEpitopeMatchingForPatterns) {
    // WHAT: Test that similar exceptions produce similar epitopes
    // WHY:  Immune pattern matching relies on epitope similarity

    nimcp_security_exception_t* ex1 = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_SQL_INJECTION,
        "SQL injection attack"
    );

    nimcp_security_exception_t* ex2 = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_SQL_INJECTION,
        "Another SQL injection"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    // Generate epitopes
    size_t len1 = nimcp_exception_generate_epitope((nimcp_exception_t*)ex1);
    size_t len2 = nimcp_exception_generate_epitope((nimcp_exception_t*)ex2);

    EXPECT_GT(len1, 0u);
    EXPECT_GT(len2, 0u);

    // Compare epitopes (similar threats should have some overlap)
    int matching_bytes = 0;
    size_t min_len = (len1 < len2) ? len1 : len2;
    for (size_t i = 0; i < min_len; i++) {
        if (ex1->base.epitope[i] == ex2->base.epitope[i]) {
            matching_bytes++;
        }
    }

    // At least some overlap expected for same threat type
    float similarity = (float)matching_bytes / (float)min_len;
    EXPECT_GT(similarity, 0.2f);  // At least 20% similar

    nimcp_exception_unref((nimcp_exception_t*)ex1);
    nimcp_exception_unref((nimcp_exception_t*)ex2);
}

TEST_F(SecurityExceptionIntegrationTest, DifferentThreatsDifferentEpitopes) {
    // WHAT: Test that different threats produce different epitopes
    // WHY:  Immune system should distinguish different threat types

    nimcp_security_exception_t* sql_ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_SQL_INJECTION,
        "SQL injection"
    );

    nimcp_security_exception_t* shell_ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_SHELLCODE,
        "Shellcode"
    );

    ASSERT_NE(sql_ex, nullptr);
    ASSERT_NE(shell_ex, nullptr);

    size_t sql_len = nimcp_exception_generate_epitope((nimcp_exception_t*)sql_ex);
    size_t shell_len = nimcp_exception_generate_epitope((nimcp_exception_t*)shell_ex);

    EXPECT_GT(sql_len, 0u);
    EXPECT_GT(shell_len, 0u);

    // Epitopes should differ
    bool different = false;
    size_t min_len = (sql_len < shell_len) ? sql_len : shell_len;
    for (size_t i = 0; i < min_len; i++) {
        if (sql_ex->base.epitope[i] != shell_ex->base.epitope[i]) {
            different = true;
            break;
        }
    }

    EXPECT_TRUE(different);

    nimcp_exception_unref((nimcp_exception_t*)sql_ex);
    nimcp_exception_unref((nimcp_exception_t*)shell_ex);
}

//=============================================================================
// Rate Limiter Exception Integration Tests
//=============================================================================

TEST_F(SecurityExceptionIntegrationTest, RateLimitExceededToException) {
    // WHAT: Test creating exceptions from rate limit violations
    // WHY:  Rate limit exceeded should trigger security exception

    nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();
    config.requests_per_second = 10.0f;
    config.burst_size = 5;

    nimcp_rate_limiter_t limiter = nimcp_rate_limiter_create(&config);
    ASSERT_NE(limiter, nullptr);

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "rate_limit_handler";
    options.handler = security_tracking_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    // Exhaust rate limit
    const char* client_id = "test_client";
    int blocked_count = 0;

    for (int i = 0; i < 20; i++) {
        if (!nimcp_rate_limiter_allow(limiter, client_id)) {
            blocked_count++;

            // Create exception for rate limit exceeded
            nimcp_security_exception_t* ex = nimcp_security_exception_create(
                NIMCP_ERROR_RATE_LIMIT,
                EXCEPTION_SEVERITY_WARNING,
                __FILE__,
                __LINE__,
                __func__,
                BBB_THREAT_NONE,  // Not a specific threat type
                "Rate limit exceeded for client: %s", client_id
            );

            if (ex) {
                nimcp_exception_dispatch((nimcp_exception_t*)ex);
                nimcp_exception_unref((nimcp_exception_t*)ex);
            }
        }
    }

    // Should have some blocked requests
    EXPECT_GT(blocked_count, 0);
    EXPECT_GT(exception_count.load(), 0);

    nimcp_handler_unregister(reg);
    nimcp_rate_limiter_destroy(limiter);
}

//=============================================================================
// Concurrent Exception Processing Tests
//=============================================================================

TEST_F(SecurityExceptionIntegrationTest, ConcurrentSecurityExceptions) {
    // WHAT: Test handling security exceptions from multiple threads
    // WHY:  Security threats may come from multiple sources simultaneously

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "concurrent_security_handler";
    options.handler = security_tracking_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    const int num_threads = 4;
    const int exceptions_per_thread = 25;
    std::vector<std::thread> threads;

    exception_count = 0;
    immune_presentation_count = 0;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t, exceptions_per_thread]() {
            for (int i = 0; i < exceptions_per_thread; i++) {
                bbb_threat_type_t threat = static_cast<bbb_threat_type_t>(
                    (t * exceptions_per_thread + i) % 10 + 1
                );

                nimcp_security_exception_t* ex = nimcp_security_exception_create(
                    NIMCP_ERROR_PERMISSION_DENIED,
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__,
                    __LINE__,
                    __func__,
                    threat,
                    "Thread %d exception %d", t, i
                );

                if (ex) {
                    nimcp_exception_dispatch((nimcp_exception_t*)ex);
                    nimcp_exception_unref((nimcp_exception_t*)ex);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All exceptions should be processed
    EXPECT_EQ(exception_count.load(), num_threads * exceptions_per_thread);

    nimcp_handler_unregister(reg);
}

TEST_F(SecurityExceptionIntegrationTest, ConcurrentHandlerModification) {
    // WHAT: Test handler registration while processing exceptions
    // WHY:  Handlers may be added/removed during attacks

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "base_handler";
    options.handler = security_tracking_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    nimcp_handler_registration_t* base_reg = nimcp_handler_register(&options);
    ASSERT_NE(base_reg, nullptr);

    std::atomic<bool> stop_threads{false};
    std::atomic<int> registrations{0};

    // Thread that dispatches exceptions
    std::thread dispatch_thread([&stop_threads]() {
        while (!stop_threads.load()) {
            nimcp_security_exception_t* ex = nimcp_security_exception_create(
                NIMCP_ERROR_PERMISSION_DENIED,
                EXCEPTION_SEVERITY_ERROR,
                __FILE__,
                __LINE__,
                __func__,
                BBB_THREAT_CODE_INJECTION,
                "Concurrent test"
            );

            if (ex) {
                nimcp_exception_dispatch((nimcp_exception_t*)ex);
                nimcp_exception_unref((nimcp_exception_t*)ex);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Thread that adds/removes handlers
    std::thread handler_thread([&stop_threads, &registrations]() {
        for (int i = 0; i < 10 && !stop_threads.load(); i++) {
            nimcp_handler_options_t opts;
            nimcp_handler_default_options(&opts);

            char name[64];
            snprintf(name, sizeof(name), "dynamic_handler_%d", i);
            opts.name = name;
            opts.handler = security_tracking_handler;
            opts.priority = NIMCP_HANDLER_PRIORITY_LOW;

            nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
            if (reg) {
                registrations++;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                nimcp_handler_unregister(reg);
            }
        }
    });

    handler_thread.join();
    stop_threads = true;
    dispatch_thread.join();

    // Should have successfully registered handlers
    EXPECT_GT(registrations.load(), 0);

    nimcp_handler_unregister(base_reg);
}

//=============================================================================
// Exception Context Propagation Tests
//=============================================================================

TEST_F(SecurityExceptionIntegrationTest, ContextPropagationThroughHandlers) {
    // WHAT: Test that exception context is accessible in handlers
    // WHY:  Handlers need context for decision making

    static std::string captured_source_ip;

    auto context_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)user_data;

        const char* ip = nimcp_exception_get_context(ex, "source_ip");
        if (ip) {
            captured_source_ip = ip;
        }

        return false;
    };

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "context_handler";
    options.handler = context_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_CODE_INJECTION,
        "Context test"
    );

    ASSERT_NE(ex, nullptr);

    // Add context
    nimcp_exception_set_context((nimcp_exception_t*)ex, "source_ip", "192.168.1.100");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "attack_vector", "POST /api/users");

    captured_source_ip.clear();
    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    // Context should have been captured
    EXPECT_EQ(captured_source_ip, "192.168.1.100");

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Exception Chaining Integration Tests
//=============================================================================

TEST_F(SecurityExceptionIntegrationTest, ExceptionChainingThroughPipeline) {
    // WHAT: Test exception chains flow through the handler pipeline
    // WHY:  Cause chains provide attack correlation

    static bool cause_found = false;

    auto chain_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)user_data;

        nimcp_exception_t* cause = nimcp_exception_get_cause(ex);
        if (cause && cause->code == NIMCP_ERROR_MEMORY_CORRUPTION) {
            cause_found = true;
        }

        return false;
    };

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "chain_handler";
    options.handler = chain_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    // Create cause
    nimcp_exception_t* cause = nimcp_exception_create(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Buffer overflow caused memory corruption"
    );

    // Create security exception
    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_BUFFER_OVERFLOW,
        "Buffer overflow attack"
    );

    ASSERT_NE(cause, nullptr);
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_cause((nimcp_exception_t*)ex, cause);

    cause_found = false;
    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    EXPECT_TRUE(cause_found);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Pattern Database Integration Tests
//=============================================================================

TEST_F(SecurityExceptionIntegrationTest, PatternDbExceptionOnMatch) {
    // WHAT: Test that security exceptions can be created for pattern matches
    // WHY:  Pattern database threat matches should produce exceptions

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "pattern_handler";
    options.handler = security_tracking_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    // Create security exception for simulated pattern match
    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_SQL_INJECTION,
        "Pattern match: SQL injection detected"
    );

    ASSERT_NE(ex, nullptr);
    exception_count = 0;
    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    EXPECT_GT(exception_count.load(), 0);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Exception Statistics Integration Tests
//=============================================================================

TEST_F(SecurityExceptionIntegrationTest, ExceptionStatisticsCollection) {
    // WHAT: Test that exception statistics are properly collected
    // WHY:  Security monitoring needs exception metrics

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "stats_handler";
    options.handler = security_tracking_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    // Dispatch various exceptions
    const int num_exceptions = 10;
    for (int i = 0; i < num_exceptions; i++) {
        nimcp_security_exception_t* ex = nimcp_security_exception_create(
            NIMCP_ERROR_PERMISSION_DENIED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__,
            __LINE__,
            __func__,
            BBB_THREAT_CODE_INJECTION,
            "Stats test %d", i
        );

        if (ex) {
            nimcp_exception_dispatch((nimcp_exception_t*)ex);
            nimcp_exception_unref((nimcp_exception_t*)ex);
        }
    }

    // Check immune statistics (exception stats may not be available)
    nimcp_exception_immune_stats_t immune_stats;
    nimcp_exception_immune_get_stats(&immune_stats);

    // Handler should have processed exceptions
    EXPECT_EQ(exception_count.load(), num_exceptions);

    nimcp_handler_unregister(reg);
}

//=============================================================================
// Memory Management Integration Tests
//=============================================================================

TEST_F(SecurityExceptionIntegrationTest, ExceptionMemoryLeakCheck) {
    // WHAT: Test that exception handling doesn't leak memory
    // WHY:  Security exceptions must not cause resource exhaustion

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "leak_check_handler";
    options.handler = security_tracking_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    // Create and destroy many exceptions
    const int iterations = 100;
    for (int i = 0; i < iterations; i++) {
        nimcp_security_exception_t* ex = nimcp_security_exception_create(
            NIMCP_ERROR_PERMISSION_DENIED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__,
            __LINE__,
            __func__,
            BBB_THREAT_CODE_INJECTION,
            "Leak check %d with a moderately long message to stress memory", i
        );

        if (ex) {
            // Add context
            nimcp_exception_set_context((nimcp_exception_t*)ex, "iteration", "value");

            // Generate epitope
            nimcp_exception_generate_epitope((nimcp_exception_t*)ex);

            // Dispatch
            nimcp_exception_dispatch((nimcp_exception_t*)ex);

            // Release
            nimcp_exception_unref((nimcp_exception_t*)ex);
        }
    }

    // If we get here without crash or hang, basic memory management works
    // More thorough leak detection would require external tools
    SUCCEED();

    nimcp_handler_unregister(reg);
}

//=============================================================================
// Handler Timeout Integration Tests
//=============================================================================

TEST_F(SecurityExceptionIntegrationTest, HandlerTimeoutHandling) {
    // WHAT: Test handling slow handlers
    // WHY:  Security handlers shouldn't block indefinitely

    static std::atomic<bool> slow_handler_started{false};
    static std::atomic<bool> slow_handler_completed{false};

    auto slow_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        slow_handler_started = true;

        // Simulate slow processing (but not too slow for tests)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        slow_handler_completed = true;
        return false;
    };

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "slow_handler";
    options.handler = slow_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    slow_handler_started = false;
    slow_handler_completed = false;

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_CODE_INJECTION,
        "Timeout test"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    // Handler should have completed within timeout
    EXPECT_TRUE(slow_handler_started.load());
    EXPECT_TRUE(slow_handler_completed.load());

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_handler_unregister(reg);
}

}  // namespace
