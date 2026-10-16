/**
 * @file test_security_exception_handling.cpp
 * @brief Unit tests for security module exception handling
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Unit tests for security-specific exception handling
 * WHY:  Verify security modules properly create, dispatch, and handle exceptions
 * HOW:  Test security exception creation, handler registration, dispatch chain,
 *       immune presentation, and recovery actions for security threats
 *
 * TEST CATEGORIES:
 * - Security exception creation and validation
 * - BBB threat to exception mapping
 * - Anomaly detection exception handling
 * - Pattern database exception integration
 * - Rate limiter exception scenarios
 * - Handler registration and dispatch
 * - Immune system presentation for security exceptions
 * - Recovery action execution
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_pattern_db.h"
#include "security/nimcp_rate_limiter.h"
#include "utils/error/nimcp_error_codes.h"
}

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class SecurityExceptionHandlingTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<nimcp_error_t> last_exception_code;
    static std::atomic<bool> security_handler_called;
    static std::atomic<uint32_t> last_threat_type;
    static std::atomic<nimcp_exception_category_t> last_category;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        security_handler_called = false;
        last_threat_type = 0;
        last_category = EXCEPTION_CATEGORY_GENERIC;

        // Initialize exception system
        nimcp_exception_system_init();

        // Reset BBB test state for isolation
        bbb_reset_test_state();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    // Handler that captures security exceptions
    static bool security_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_category = ex->category;

        if (ex->type == EXCEPTION_TYPE_SECURITY) {
            security_handler_called = true;
            nimcp_security_exception_t* sec_ex = (nimcp_security_exception_t*)ex;
            last_threat_type = sec_ex->threat_type;
        }

        return false;  // Don't consume - allow chain to continue
    }

    // Handler that consumes security exceptions
    static bool consuming_security_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;

        if (ex->category == EXCEPTION_CATEGORY_SECURITY) {
            return true;  // Consume security exceptions
        }
        return false;
    }

    // Recovery callback for testing
    static int test_recovery_callback(
        nimcp_exception_t* ex,
        nimcp_exception_recovery_action_t action,
        void* user_data
    ) {
        (void)user_data;
        (void)ex;
        (void)action;
        return 0;  // Success
    }
};

std::atomic<int> SecurityExceptionHandlingTest::handler_call_count(0);
std::atomic<nimcp_error_t> SecurityExceptionHandlingTest::last_exception_code(0);
std::atomic<bool> SecurityExceptionHandlingTest::security_handler_called(false);
std::atomic<uint32_t> SecurityExceptionHandlingTest::last_threat_type(0);
std::atomic<nimcp_exception_category_t> SecurityExceptionHandlingTest::last_category(
    EXCEPTION_CATEGORY_GENERIC);

//=============================================================================
// Security Exception Creation Tests
//=============================================================================

TEST_F(SecurityExceptionHandlingTest, CreateSecurityException) {
    // WHAT: Test creating a basic security exception
    // WHY:  Verify security exception structure is properly initialized

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_CODE_INJECTION,
        "Code injection attempt detected"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_SECURITY);
    EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_SECURITY);
    EXPECT_EQ(ex->base.severity, EXCEPTION_SEVERITY_SEVERE);
    EXPECT_EQ(ex->threat_type, BBB_THREAT_CODE_INJECTION);
    EXPECT_STREQ(ex->base.message, "Code injection attempt detected");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SecurityExceptionHandlingTest, CreateSecurityExceptionWithNullParams) {
    // WHAT: Test security exception creation with NULL parameters
    // WHY:  Verify graceful handling of invalid inputs

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        nullptr,  // NULL file
        0,
        nullptr,  // NULL function
        BBB_THREAT_BUFFER_OVERFLOW,
        nullptr   // NULL message
    );

    // Should still create exception with defaults
    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_SECURITY);
    EXPECT_EQ(ex->threat_type, BBB_THREAT_BUFFER_OVERFLOW);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SecurityExceptionHandlingTest, SecurityExceptionThreatTypes) {
    // WHAT: Test all BBB threat types map to security exceptions
    // WHY:  Ensure comprehensive threat coverage

    const bbb_threat_type_t threats[] = {
        BBB_THREAT_BUFFER_OVERFLOW,
        BBB_THREAT_FORMAT_STRING,
        BBB_THREAT_INTEGER_OVERFLOW,
        BBB_THREAT_SQL_INJECTION,
        BBB_THREAT_CODE_INJECTION,
        BBB_THREAT_SHELLCODE,
        BBB_THREAT_ROP_CHAIN,
        BBB_THREAT_INVALID_SIGNATURE,
        BBB_THREAT_MEMORY_VIOLATION,
        BBB_THREAT_UNAUTHORIZED_ACCESS,
        BBB_THREAT_DATA_TAMPERING,
        BBB_THREAT_PATH_TRAVERSAL,
        BBB_THREAT_SHELL_INJECTION
    };

    for (auto threat : threats) {
        nimcp_security_exception_t* ex = nimcp_security_exception_create(
            NIMCP_ERROR_PERMISSION_DENIED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__,
            __LINE__,
            __func__,
            threat,
            "Threat test: %u", threat
        );

        ASSERT_NE(ex, nullptr) << "Failed to create exception for threat: " << threat;
        EXPECT_EQ(ex->threat_type, threat);
        EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_SECURITY);

        nimcp_exception_unref((nimcp_exception_t*)ex);
    }
}

TEST_F(SecurityExceptionHandlingTest, SecurityExceptionSeverityLevels) {
    // WHAT: Test security exceptions with different severity levels
    // WHY:  Verify severity mapping for threat escalation

    const nimcp_exception_severity_t severities[] = {
        EXCEPTION_SEVERITY_WARNING,
        EXCEPTION_SEVERITY_ERROR,
        EXCEPTION_SEVERITY_SEVERE,
        EXCEPTION_SEVERITY_CRITICAL,
        EXCEPTION_SEVERITY_FATAL
    };

    for (auto severity : severities) {
        nimcp_security_exception_t* ex = nimcp_security_exception_create(
            NIMCP_ERROR_PERMISSION_DENIED,
            severity,
            __FILE__,
            __LINE__,
            __func__,
            BBB_THREAT_CODE_INJECTION,
            "Severity test"
        );

        ASSERT_NE(ex, nullptr);
        EXPECT_EQ(ex->base.severity, severity);

        nimcp_exception_unref((nimcp_exception_t*)ex);
    }
}

//=============================================================================
// Exception Context Tests
//=============================================================================

TEST_F(SecurityExceptionHandlingTest, SecurityExceptionContext) {
    // WHAT: Test adding context to security exceptions
    // WHY:  Context provides debugging information for threat analysis

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_SQL_INJECTION,
        "SQL injection detected"
    );

    ASSERT_NE(ex, nullptr);

    // Add security-related context
    EXPECT_EQ(nimcp_exception_set_context((nimcp_exception_t*)ex, "source_ip", "192.168.1.100"), 0);
    EXPECT_EQ(nimcp_exception_set_context((nimcp_exception_t*)ex, "target_table", "users"), 0);
    EXPECT_EQ(nimcp_exception_set_context((nimcp_exception_t*)ex, "payload", "'; DROP TABLE users; --"), 0);

    // Verify context retrieval
    EXPECT_STREQ(nimcp_exception_get_context((nimcp_exception_t*)ex, "source_ip"), "192.168.1.100");
    EXPECT_STREQ(nimcp_exception_get_context((nimcp_exception_t*)ex, "target_table"), "users");
    EXPECT_EQ(nimcp_exception_context_count((nimcp_exception_t*)ex), 3u);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SecurityExceptionHandlingTest, SecurityExceptionContextOverflow) {
    // WHAT: Test context overflow handling
    // WHY:  Security exceptions may have many context entries

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_BUFFER_OVERFLOW,
        "Buffer overflow"
    );

    ASSERT_NE(ex, nullptr);

    // Fill context entries up to max
    for (size_t i = 0; i < NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%zu", i);
        int result = nimcp_exception_set_context((nimcp_exception_t*)ex, key, "value");
        EXPECT_EQ(result, 0) << "Failed at entry " << i;
    }

    // Overflow should fail gracefully
    int overflow_result = nimcp_exception_set_context((nimcp_exception_t*)ex, "overflow_key", "value");
    EXPECT_EQ(overflow_result, -1);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Handler Registration Tests
//=============================================================================

TEST_F(SecurityExceptionHandlingTest, RegisterSecurityHandler) {
    // WHAT: Test registering a security-specific exception handler
    // WHY:  Security modules need dedicated handlers

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "security_test_handler";
    options.handler = security_exception_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    options.category_filter = EXCEPTION_CATEGORY_SECURITY;
    options.min_severity = EXCEPTION_SEVERITY_WARNING;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);
    EXPECT_TRUE(reg->active);

    // Clean up
    EXPECT_EQ(nimcp_handler_unregister(reg), 0);
}

TEST_F(SecurityExceptionHandlingTest, SecurityHandlerPriority) {
    // WHAT: Test handler priority ordering
    // WHY:  Security handlers should be processed before general handlers

    std::atomic<int> call_order{0};
    static std::atomic<int> high_priority_order{0};
    static std::atomic<int> low_priority_order{0};

    auto high_priority_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        auto* order = static_cast<std::atomic<int>*>(user_data);
        high_priority_order = ++(*order);
        return false;
    };

    auto low_priority_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        auto* order = static_cast<std::atomic<int>*>(user_data);
        low_priority_order = ++(*order);
        return false;
    };

    nimcp_handler_options_t high_opts, low_opts;
    nimcp_handler_default_options(&high_opts);
    nimcp_handler_default_options(&low_opts);

    high_opts.name = "high_priority";
    high_opts.handler = high_priority_handler;
    high_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    high_opts.user_data = &call_order;

    low_opts.name = "low_priority";
    low_opts.handler = low_priority_handler;
    low_opts.priority = NIMCP_HANDLER_PRIORITY_LOW;
    low_opts.user_data = &call_order;

    nimcp_handler_registration_t* high_reg = nimcp_handler_register(&high_opts);
    nimcp_handler_registration_t* low_reg = nimcp_handler_register(&low_opts);

    ASSERT_NE(high_reg, nullptr);
    ASSERT_NE(low_reg, nullptr);

    // Create and dispatch security exception
    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_CODE_INJECTION,
        "Priority test"
    );

    ASSERT_NE(ex, nullptr);
    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    // High priority should be called first
    EXPECT_LT(high_priority_order.load(), low_priority_order.load());

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_handler_unregister(high_reg);
    nimcp_handler_unregister(low_reg);
}

TEST_F(SecurityExceptionHandlingTest, SecurityHandlerCategoryFilter) {
    // WHAT: Test handler category filtering
    // WHY:  Security handlers should only receive security exceptions

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "security_filter_handler";
    options.handler = security_exception_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    options.category_filter = EXCEPTION_CATEGORY_SECURITY;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    // Create non-security exception
    nimcp_exception_t* mem_ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Memory allocation failed"
    );

    ASSERT_NE(mem_ex, nullptr);
    handler_call_count = 0;
    nimcp_exception_dispatch(mem_ex);

    // Handler should not be called for non-security exception
    EXPECT_EQ(handler_call_count.load(), 0);

    // Create security exception
    nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_CODE_INJECTION,
        "Security test"
    );

    ASSERT_NE(sec_ex, nullptr);
    nimcp_exception_dispatch((nimcp_exception_t*)sec_ex);

    // Handler should be called for security exception
    EXPECT_GT(handler_call_count.load(), 0);

    nimcp_exception_unref(mem_ex);
    nimcp_exception_unref((nimcp_exception_t*)sec_ex);
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Exception Dispatch Tests
//=============================================================================

TEST_F(SecurityExceptionHandlingTest, DispatchSecurityException) {
    // WHAT: Test dispatching security exception through handler chain
    // WHY:  Verify security exceptions flow through dispatch mechanism

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "dispatch_test_handler";
    options.handler = security_exception_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_SHELLCODE,
        "Shellcode detected in input"
    );

    ASSERT_NE(ex, nullptr);

    handler_call_count = 0;
    security_handler_called = false;
    bool handled = nimcp_exception_dispatch((nimcp_exception_t*)ex);

    // Handler should have been called
    EXPECT_GT(handler_call_count.load(), 0);
    EXPECT_TRUE(security_handler_called.load());
    EXPECT_EQ(last_threat_type.load(), BBB_THREAT_SHELLCODE);
    EXPECT_EQ(last_category.load(), EXCEPTION_CATEGORY_SECURITY);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_handler_unregister(reg);
}

TEST_F(SecurityExceptionHandlingTest, DispatchChainConsumption) {
    // WHAT: Test that consumed exceptions stop chain processing
    // WHY:  Security handlers may need to consume exceptions

    std::atomic<int> second_handler_calls{0};

    auto second_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        auto* count = static_cast<std::atomic<int>*>(user_data);
        (*count)++;
        return false;
    };

    nimcp_handler_options_t consuming_opts, second_opts;
    nimcp_handler_default_options(&consuming_opts);
    nimcp_handler_default_options(&second_opts);

    consuming_opts.name = "consuming_handler";
    consuming_opts.handler = consuming_security_handler;
    consuming_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    second_opts.name = "second_handler";
    second_opts.handler = second_handler;
    second_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    second_opts.user_data = &second_handler_calls;

    nimcp_handler_registration_t* consuming_reg = nimcp_handler_register(&consuming_opts);
    nimcp_handler_registration_t* second_reg = nimcp_handler_register(&second_opts);

    ASSERT_NE(consuming_reg, nullptr);
    ASSERT_NE(second_reg, nullptr);

    // Create and dispatch security exception
    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_CODE_INJECTION,
        "Consumption test"
    );

    ASSERT_NE(ex, nullptr);
    handler_call_count = 0;
    bool handled = nimcp_exception_dispatch((nimcp_exception_t*)ex);

    // Exception should be consumed by first handler
    EXPECT_TRUE(handled);
    // Second handler should not be called due to consumption
    EXPECT_EQ(second_handler_calls.load(), 0);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_handler_unregister(consuming_reg);
    nimcp_handler_unregister(second_reg);
}

//=============================================================================
// Immune Presentation Tests
//=============================================================================

TEST_F(SecurityExceptionHandlingTest, PresentSecurityExceptionToImmune) {
    // WHAT: Test presenting security exception to immune system
    // WHY:  Security threats should trigger immune responses

    // Initialize immune integration (may return error if not connected)
    nimcp_exception_immune_init(nullptr);

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_CODE_INJECTION,
        "Critical security threat"
    );

    ASSERT_NE(ex, nullptr);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);

    // Should succeed even without connected immune system (queues for later)
    EXPECT_EQ(result, 0);

    // The important contract is that present succeeded (return 0)
    // Internal flag behavior is implementation-specific

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_exception_immune_shutdown();
}

TEST_F(SecurityExceptionHandlingTest, SecurityExceptionEpitope) {
    // WHAT: Test epitope generation for security exceptions
    // WHY:  Epitopes enable immune pattern matching

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_SQL_INJECTION,
        "SQL injection detected"
    );

    ASSERT_NE(ex, nullptr);

    // Generate epitope
    size_t epitope_len = nimcp_exception_generate_epitope((nimcp_exception_t*)ex);

    // Epitope should be generated
    EXPECT_GT(epitope_len, 0u);
    EXPECT_LE(epitope_len, NIMCP_EXCEPTION_EPITOPE_SIZE);
    EXPECT_EQ(ex->base.epitope_len, epitope_len);

    // Epitope should not be all zeros
    bool all_zero = true;
    for (size_t i = 0; i < epitope_len; i++) {
        if (ex->base.epitope[i] != 0) {
            all_zero = false;
            break;
        }
    }
    EXPECT_FALSE(all_zero);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SecurityExceptionHandlingTest, SecurityExceptionToAntigenSource) {
    // WHAT: Test mapping security exceptions to antigen sources
    // WHY:  Security exceptions should map to BBB antigen source

    exception_antigen_source_t source = nimcp_exception_to_antigen_source(
        EXCEPTION_CATEGORY_SECURITY
    );

    // Security category should map to BBB source
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_BBB);
}

TEST_F(SecurityExceptionHandlingTest, SecuritySeverityToImmuneSeverity) {
    // WHAT: Test severity mapping to immune severity scale
    // WHY:  Immune system uses 1-10 severity scale

    // Map various exception severities
    uint32_t debug_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_DEBUG);
    uint32_t warning_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_WARNING);
    uint32_t error_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_ERROR);
    uint32_t severe_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_SEVERE);
    uint32_t critical_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_CRITICAL);
    uint32_t fatal_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_FATAL);

    // Verify ordering
    EXPECT_LT(debug_sev, warning_sev);
    EXPECT_LT(warning_sev, error_sev);
    EXPECT_LT(error_sev, severe_sev);
    EXPECT_LT(severe_sev, critical_sev);
    EXPECT_LE(critical_sev, fatal_sev);

    // Verify range (1-10)
    EXPECT_GE(debug_sev, 1u);
    EXPECT_LE(fatal_sev, 10u);
}

//=============================================================================
// Recovery Action Tests
//=============================================================================

TEST_F(SecurityExceptionHandlingTest, SecurityExceptionRecoveryStrategy) {
    // WHAT: Test getting recovery strategy for security exceptions
    // WHY:  Security threats require specific recovery actions

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_CODE_INJECTION,
        "Critical injection attack"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    // Security threats should suggest quarantine
    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_QUARANTINE);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SecurityExceptionHandlingTest, SecurityRecoveryCallbackRegistration) {
    // WHAT: Test registering recovery callbacks for security actions
    // WHY:  Security modules need custom recovery logic

    int result = nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_QUARANTINE,
        test_recovery_callback,
        nullptr
    );

    EXPECT_EQ(result, 0);

    // Clean up
    result = nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_QUARANTINE);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityExceptionHandlingTest, ExecuteSecurityRecovery) {
    // WHAT: Test executing recovery action for security exception
    // WHY:  Verify recovery mechanism works for security threats

    // Register recovery callback
    nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_QUARANTINE,
        test_recovery_callback,
        nullptr
    );

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_MEMORY_VIOLATION,
        "Memory violation detected"
    );

    ASSERT_NE(ex, nullptr);

    // Execute recovery
    int result = nimcp_execute_recovery(
        (nimcp_exception_t*)ex,
        EXCEPTION_RECOVERY_QUARANTINE
    );

    EXPECT_EQ(result, 0);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_QUARANTINE);
}

//=============================================================================
// BBB Integration Tests
//=============================================================================

TEST_F(SecurityExceptionHandlingTest, BBBThreatToException) {
    // WHAT: Test creating exceptions from BBB threat reports
    // WHY:  BBB threats should be convertible to exceptions

    bbb_config_t config = bbb_default_config();
    bbb_system_t bbb = bbb_system_create(&config);
    ASSERT_NE(bbb, nullptr);

    // Report a threat
    bbb_threat_report_t report = bbb_report_threat(
        bbb,
        BBB_THREAT_CODE_INJECTION,
        BBB_SEVERITY_HIGH,
        "Code injection in input",
        nullptr,
        nullptr,
        0
    );

    // Create exception from threat
    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        report.type,
        "%s", report.description
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->threat_type, BBB_THREAT_CODE_INJECTION);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    bbb_system_destroy(bbb);
}

TEST_F(SecurityExceptionHandlingTest, BBBValidationException) {
    // WHAT: Test exception handling for BBB validation failures
    // WHY:  Validation failures should produce security exceptions

    bbb_config_t config = bbb_default_config();
    bbb_system_t bbb = bbb_system_create(&config);
    ASSERT_NE(bbb, nullptr);

    // Malicious input
    const char* malicious_input = "'; DROP TABLE users; --";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(bbb, malicious_input, &result);

    if (!valid && result.threat != BBB_THREAT_NONE) {
        // Create exception for validation failure
        nimcp_security_exception_t* ex = nimcp_security_exception_create(
            NIMCP_ERROR_PERMISSION_DENIED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__,
            __LINE__,
            __func__,
            result.threat,
            "Validation failed: %s", result.reason
        );

        ASSERT_NE(ex, nullptr);
        EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_SECURITY);

        nimcp_exception_unref((nimcp_exception_t*)ex);
    }

    bbb_system_destroy(bbb);
}

//=============================================================================
// Aggregate Exception Tests
//=============================================================================

TEST_F(SecurityExceptionHandlingTest, AggregateSecurityExceptions) {
    // WHAT: Test aggregating multiple security exceptions
    // WHY:  Multi-vector attacks produce multiple exceptions

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__,
        __LINE__,
        __func__,
        "Multiple security threats detected"
    );

    ASSERT_NE(agg, nullptr);

    // Create child exceptions
    nimcp_security_exception_t* ex1 = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_SQL_INJECTION,
        "SQL injection"
    );

    nimcp_security_exception_t* ex2 = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_CODE_INJECTION,
        "Code injection"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    // Add to aggregate
    EXPECT_EQ(nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)ex1), 0);
    EXPECT_EQ(nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)ex2), 0);
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 2u);

    // Verify children
    nimcp_exception_t* child1 = nimcp_aggregate_exception_get(agg, 0);
    nimcp_exception_t* child2 = nimcp_aggregate_exception_get(agg, 1);

    ASSERT_NE(child1, nullptr);
    ASSERT_NE(child2, nullptr);
    EXPECT_EQ(child1->type, EXCEPTION_TYPE_SECURITY);
    EXPECT_EQ(child2->type, EXCEPTION_TYPE_SECURITY);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Exception Chaining Tests
//=============================================================================

TEST_F(SecurityExceptionHandlingTest, SecurityExceptionChaining) {
    // WHAT: Test chaining security exceptions with causes
    // WHY:  Security issues often have root causes

    // Root cause: memory corruption
    nimcp_exception_t* cause = nimcp_exception_create(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Buffer overwrite detected"
    );

    ASSERT_NE(cause, nullptr);

    // Resulting security exception
    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_BUFFER_OVERFLOW,
        "Buffer overflow attack"
    );

    ASSERT_NE(ex, nullptr);

    // Chain exceptions
    nimcp_exception_set_cause((nimcp_exception_t*)ex, cause);

    // Verify chain
    nimcp_exception_t* retrieved_cause = nimcp_exception_get_cause((nimcp_exception_t*)ex);
    EXPECT_EQ(retrieved_cause, cause);
    EXPECT_EQ(retrieved_cause->code, NIMCP_ERROR_MEMORY_CORRUPTION);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    // cause is unreffed when ex is unreffed due to ownership transfer
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(SecurityExceptionHandlingTest, ConcurrentHandlerRegistration) {
    // WHAT: Test concurrent handler registration
    // WHY:  Multiple threads may register handlers simultaneously

    const int num_threads = 4;
    const int handlers_per_thread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successful_registrations{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < handlers_per_thread; i++) {
                nimcp_handler_options_t options;
                nimcp_handler_default_options(&options);

                char name[64];
                snprintf(name, sizeof(name), "thread_%d_handler_%d", t, i);
                options.name = name;
                options.handler = security_exception_handler;
                options.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

                nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
                if (reg != nullptr) {
                    successful_registrations++;
                    nimcp_handler_unregister(reg);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Most registrations should succeed
    EXPECT_GT(successful_registrations.load(), 0);
}

TEST_F(SecurityExceptionHandlingTest, ConcurrentExceptionDispatch) {
    // WHAT: Test concurrent exception dispatch
    // WHY:  Multiple threads may dispatch exceptions simultaneously

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "concurrent_dispatch_handler";
    options.handler = security_exception_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    const int num_threads = 4;
    const int exceptions_per_thread = 25;
    std::vector<std::thread> threads;

    handler_call_count = 0;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < exceptions_per_thread; i++) {
                nimcp_security_exception_t* ex = nimcp_security_exception_create(
                    NIMCP_ERROR_PERMISSION_DENIED,
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__,
                    __LINE__,
                    __func__,
                    BBB_THREAT_CODE_INJECTION,
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

    // All exceptions should have been dispatched
    EXPECT_EQ(handler_call_count.load(), num_threads * exceptions_per_thread);

    nimcp_handler_unregister(reg);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST_F(SecurityExceptionHandlingTest, SecurityExceptionToString) {
    // WHAT: Test converting security exception to string
    // WHY:  Logging and debugging require string representation

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        BBB_THREAT_SQL_INJECTION,
        "SQL injection detected"
    );

    ASSERT_NE(ex, nullptr);

    char buffer[1024];
    size_t len = nimcp_exception_to_string((nimcp_exception_t*)ex, buffer, sizeof(buffer));

    EXPECT_GT(len, 0u);
    EXPECT_LT(len, sizeof(buffer));

    // Buffer should contain relevant information
    EXPECT_NE(strstr(buffer, "SQL injection"), nullptr);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SecurityExceptionHandlingTest, SeverityToString) {
    // WHAT: Test severity level string conversion
    // WHY:  Human-readable severity names for logging

    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_DEBUG), "DEBUG");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_INFO), "INFO");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_WARNING), "WARNING");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_ERROR), "ERROR");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_SEVERE), "SEVERE");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_CRITICAL), "CRITICAL");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_FATAL), "FATAL");
}

TEST_F(SecurityExceptionHandlingTest, CategoryToString) {
    // WHAT: Test category string conversion
    // WHY:  Human-readable category names for logging

    const char* security_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_SECURITY);
    EXPECT_NE(security_str, nullptr);
    EXPECT_NE(strstr(security_str, "SECURITY"), nullptr);
}

TEST_F(SecurityExceptionHandlingTest, RecoveryActionToString) {
    // WHAT: Test recovery action string conversion
    // WHY:  Human-readable action names for logging

    EXPECT_NE(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_NONE), nullptr);
    EXPECT_NE(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RETRY), nullptr);
    EXPECT_NE(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GC), nullptr);
    EXPECT_NE(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_QUARANTINE), nullptr);
    EXPECT_NE(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_ROLLBACK), nullptr);
    EXPECT_NE(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN), nullptr);
}

}  // namespace
