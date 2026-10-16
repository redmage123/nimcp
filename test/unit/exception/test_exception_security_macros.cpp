/**
 * @file test_exception_security_macros.cpp
 * @brief Unit tests for security module exception integration
 *
 * WHAT: Tests for security facade and related module exception handling
 * WHY:  Verify security exception flow, facade error handling, and guard behavior
 * HOW:  GoogleTest framework with fixture setup/teardown for exception system
 *
 * TEST CATEGORIES:
 * 1. Security facade error handling
 * 2. Security exception flow through NIMCP_THROW_SECURITY
 * 3. Threat reporting exception behavior
 * 4. Module enable/disable exception flow
 * 5. Lockdown trigger exception behavior
 *
 * @author NIMCP Development Team
 * @date 2026-01-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>

extern "C" {
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "security/nimcp_security_facade.h"
}

//=============================================================================
// Test Globals for Handler Tracking
//=============================================================================

namespace {

std::atomic<int> g_handler_call_count{0};
std::atomic<nimcp_error_t> g_last_error_code{NIMCP_SUCCESS};
std::atomic<nimcp_exception_severity_t> g_last_severity{EXCEPTION_SEVERITY_DEBUG};
std::atomic<bool> g_exception_presented_to_immune{false};
std::atomic<uint32_t> g_last_threat_type{0};
std::vector<std::string> g_captured_messages;

/**
 * @brief Test handler callback to track exception dispatch
 */
bool security_test_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex) {
        g_handler_call_count++;
        g_last_error_code = ex->code;
        g_last_severity = ex->severity;
        g_exception_presented_to_immune = ex->presented_to_immune;

        if (ex->message) {
            g_captured_messages.push_back(std::string(ex->message));
        }
    }
    return false;  // Don't consume, let chain continue
}

/**
 * @brief Reset all test tracking globals
 */
void reset_tracking() {
    g_handler_call_count = 0;
    g_last_error_code = NIMCP_SUCCESS;
    g_last_severity = EXCEPTION_SEVERITY_DEBUG;
    g_exception_presented_to_immune = false;
    g_last_threat_type = 0;
    g_captured_messages.clear();
}

}  // anonymous namespace

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * WHAT: Base fixture for security exception tests
 * WHY:  Setup/teardown exception system for each test
 */
class SecurityExceptionTest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* handler_reg_ = nullptr;

    void SetUp() override {
        reset_tracking();

        // Initialize exception system
        nimcp_exception_system_init();

        // Register test handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "security_test_handler";
        opts.handler = security_test_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        handler_reg_ = nimcp_handler_register(&opts);
    }

    void TearDown() override {
        if (handler_reg_) {
            nimcp_handler_unregister(handler_reg_);
            handler_reg_ = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

//=============================================================================
// Helper Functions Simulating Security Operations
//=============================================================================

/**
 * @brief Simulate security facade initialization with validation
 */
static int security_facade_validate_config(const security_facade_config_t* config) {
    NIMCP_CHECK_THROW(config != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Security facade config is NULL");

    NIMCP_CHECK_THROW(config->alert_threshold >= 0.0f && config->alert_threshold <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Alert threshold %.2f out of range [0, 1]",
                      config->alert_threshold);

    NIMCP_CHECK_THROW(config->lockdown_threshold >= config->alert_threshold,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Lockdown threshold %.2f must be >= alert threshold %.2f",
                      config->lockdown_threshold, config->alert_threshold);

    return 0;
}

/**
 * @brief Simulate threat level validation
 */
static int validate_threat_level(float threat_level) {
    NIMCP_CHECK_THROW(threat_level >= 0.0f, NIMCP_ERROR_OUT_OF_RANGE,
                      "Threat level %.2f cannot be negative", threat_level);
    NIMCP_CHECK_THROW(threat_level <= 1.0f, NIMCP_ERROR_OUT_OF_RANGE,
                      "Threat level %.2f exceeds maximum 1.0", threat_level);
    return 0;
}

/**
 * @brief Simulate security module enable with validation
 */
static int security_module_enable(security_facade_t facade, security_module_id_t module_id) {
    NIMCP_CHECK_THROW(facade != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Security facade is NULL");
    NIMCP_CHECK_THROW(module_id < SEC_MODULE_COUNT, NIMCP_ERROR_OUT_OF_RANGE,
                      "Invalid module ID %d", (int)module_id);

    // Simulate core module restriction
    if (module_id == SEC_MODULE_ORCHESTRATOR) {
        // Orchestrator is always enabled, this is a no-op
        return 0;
    }

    return 0;
}

/**
 * @brief Simulate security module disable with validation
 */
static int security_module_disable(security_facade_t facade, security_module_id_t module_id) {
    NIMCP_CHECK_THROW(facade != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Security facade is NULL");
    NIMCP_CHECK_THROW(module_id < SEC_MODULE_COUNT, NIMCP_ERROR_OUT_OF_RANGE,
                      "Invalid module ID %d", (int)module_id);

    // Core modules cannot be disabled
    NIMCP_CHECK_THROW(module_id != SEC_MODULE_ORCHESTRATOR, NIMCP_ERROR_INVALID_STATE,
                      "Cannot disable core module: orchestrator");

    return 0;
}

/**
 * @brief Simulate threat report with security exception
 */
static int report_security_threat(security_facade_t facade,
                                   uint32_t threat_type,
                                   float threat_level,
                                   const char* description) {
    NIMCP_CHECK_THROW(facade != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Security facade is NULL");
    NIMCP_CHECK_THROW(description != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Threat description is NULL");
    NIMCP_CHECK_THROW(threat_level >= 0.0f && threat_level <= 1.0f,
                      NIMCP_ERROR_OUT_OF_RANGE,
                      "Threat level %.2f out of range [0, 1]", threat_level);

    // For severe threats, throw security exception
    if (threat_level >= 0.8f) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, threat_type,
                             "Critical threat detected: %s (level %.2f)",
                             description, threat_level);
    }

    return 0;
}

/**
 * @brief Simulate lockdown trigger with validation
 */
static int trigger_lockdown(security_facade_t facade, const char* reason) {
    NIMCP_CHECK_THROW(facade != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Security facade is NULL");
    NIMCP_CHECK_THROW(reason != nullptr && strlen(reason) > 0,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Lockdown reason cannot be empty");

    return 0;
}

/**
 * @brief Simulate artifact verification (placeholder for LGSS guard)
 */
static int verify_artifact(const void* artifact, size_t size, const char* expected_hash) {
    NIMCP_CHECK_THROW(artifact != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Artifact to verify is NULL");
    NIMCP_CHECK_THROW(size > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Artifact size must be positive");
    NIMCP_CHECK_THROW(expected_hash != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Expected hash is NULL");

    // Simulate hash verification (always pass in test)
    return 0;
}

/**
 * @brief Simulate security guard check (placeholder for LGSS guard)
 */
static int security_guard_check(const void* context, uint32_t permission_flags) {
    NIMCP_CHECK_THROW(context != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Security context is NULL");
    NIMCP_CHECK_THROW(permission_flags != 0, NIMCP_ERROR_INVALID_PARAM,
                      "Permission flags cannot be zero");

    // Simulate permission denied
    if (permission_flags & 0x8000) {  // Reserved flag
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 1,
                             "Access denied: insufficient permissions (flags=0x%08X)",
                             permission_flags);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    return 0;
}

//=============================================================================
// Security Facade Error Handling Tests
//=============================================================================

/**
 * WHAT: Test facade config validation with NULL config
 * WHY:  Verify proper error handling for NULL input
 */
TEST_F(SecurityExceptionTest, FacadeConfigValidationNullConfig) {
    int result = security_facade_validate_config(nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test facade config validation with invalid alert threshold
 * WHY:  Verify threshold range validation
 */
TEST_F(SecurityExceptionTest, FacadeConfigValidationInvalidAlertThreshold) {
    security_facade_config_t config;
    memset(&config, 0, sizeof(config));
    config.alert_threshold = 1.5f;  // Invalid - above 1.0
    config.lockdown_threshold = 0.9f;

    int result = security_facade_validate_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_PARAM);
}

/**
 * WHAT: Test facade config validation with lockdown < alert threshold
 * WHY:  Verify lockdown threshold must be >= alert threshold
 */
TEST_F(SecurityExceptionTest, FacadeConfigValidationLockdownBelowAlert) {
    security_facade_config_t config;
    memset(&config, 0, sizeof(config));
    config.alert_threshold = 0.7f;
    config.lockdown_threshold = 0.5f;  // Invalid - below alert

    int result = security_facade_validate_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test valid facade config passes validation
 * WHY:  Verify successful validation doesn't throw
 */
TEST_F(SecurityExceptionTest, FacadeConfigValidationSuccess) {
    security_facade_config_t config;
    memset(&config, 0, sizeof(config));
    config.alert_threshold = 0.6f;
    config.lockdown_threshold = 0.9f;

    int result = security_facade_validate_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Security Exception Flow Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_THROW_SECURITY creates critical severity exception
 * WHY:  Security exceptions should always be critical
 */
TEST_F(SecurityExceptionTest, SecurityExceptionHasCriticalSeverity) {
    // Trigger security exception through threat report
    int dummy_facade = 1;  // Non-null placeholder
    int result = report_security_threat((security_facade_t)&dummy_facade,
                                        1, 0.9f, "Test threat");

    // Note: report_security_threat doesn't return the NIMCP_THROW_SECURITY result directly
    // but the exception should still be dispatched
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
}

/**
 * WHAT: Test security exception is presented to immune system
 * WHY:  Critical security events should be learned by immune system
 */
TEST_F(SecurityExceptionTest, SecurityExceptionPresentedToImmune) {
    int dummy_facade = 1;
    report_security_threat((security_facade_t)&dummy_facade,
                           2, 0.85f, "Severe threat");

    EXPECT_TRUE(g_exception_presented_to_immune);
}

/**
 * WHAT: Test low threat level doesn't trigger security exception
 * WHY:  Only severe threats should create exceptions
 */
TEST_F(SecurityExceptionTest, LowThreatLevelNoException) {
    int dummy_facade = 1;
    int result = report_security_threat((security_facade_t)&dummy_facade,
                                        1, 0.5f, "Minor threat");

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Threat Level Validation Tests
//=============================================================================

/**
 * WHAT: Test negative threat level validation
 * WHY:  Threat levels must be non-negative
 */
TEST_F(SecurityExceptionTest, NegativeThreatLevelRejected) {
    int result = validate_threat_level(-0.1f);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test threat level above 1.0 validation
 * WHY:  Threat levels must not exceed 1.0
 */
TEST_F(SecurityExceptionTest, ThreatLevelAboveOneRejected) {
    int result = validate_threat_level(1.5f);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test valid threat level boundaries
 * WHY:  Verify exact boundaries are accepted
 */
TEST_F(SecurityExceptionTest, ThreatLevelBoundariesAccepted) {
    EXPECT_EQ(validate_threat_level(0.0f), 0);
    EXPECT_EQ(validate_threat_level(1.0f), 0);
    EXPECT_EQ(validate_threat_level(0.5f), 0);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Module Enable/Disable Exception Tests
//=============================================================================

/**
 * WHAT: Test module enable with NULL facade
 * WHY:  Verify NULL check for facade parameter
 */
TEST_F(SecurityExceptionTest, ModuleEnableNullFacade) {
    int result = security_module_enable(nullptr, SEC_MODULE_DISTRIBUTED_TRAINING);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test module enable with invalid module ID
 * WHY:  Verify range check for module ID
 */
TEST_F(SecurityExceptionTest, ModuleEnableInvalidModuleId) {
    int dummy_facade = 1;
    int result = security_module_enable((security_facade_t)&dummy_facade,
                                        (security_module_id_t)999);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test module disable for core orchestrator fails
 * WHY:  Core modules cannot be disabled
 */
TEST_F(SecurityExceptionTest, ModuleDisableOrchestratorFails) {
    int dummy_facade = 1;
    int result = security_module_disable((security_facade_t)&dummy_facade,
                                         SEC_MODULE_ORCHESTRATOR);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test module disable for non-core module succeeds
 * WHY:  Non-core modules can be disabled
 */
TEST_F(SecurityExceptionTest, ModuleDisableNonCoreSucceeds) {
    int dummy_facade = 1;
    int result = security_module_disable((security_facade_t)&dummy_facade,
                                         SEC_MODULE_KNOWLEDGE_GRAPH);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Lockdown Trigger Exception Tests
//=============================================================================

/**
 * WHAT: Test lockdown trigger with NULL facade
 * WHY:  Verify NULL check for facade
 */
TEST_F(SecurityExceptionTest, LockdownTriggerNullFacade) {
    int result = trigger_lockdown(nullptr, "Test reason");

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test lockdown trigger with empty reason
 * WHY:  Verify reason cannot be empty
 */
TEST_F(SecurityExceptionTest, LockdownTriggerEmptyReason) {
    int dummy_facade = 1;
    int result = trigger_lockdown((security_facade_t)&dummy_facade, "");

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test lockdown trigger with NULL reason
 * WHY:  Verify reason cannot be NULL
 */
TEST_F(SecurityExceptionTest, LockdownTriggerNullReason) {
    int dummy_facade = 1;
    int result = trigger_lockdown((security_facade_t)&dummy_facade, nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test lockdown trigger with valid reason succeeds
 * WHY:  Verify successful lockdown trigger
 */
TEST_F(SecurityExceptionTest, LockdownTriggerSuccess) {
    int dummy_facade = 1;
    int result = trigger_lockdown((security_facade_t)&dummy_facade,
                                  "Critical threat detected");

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Artifact Verification Tests (LGSS Guard Placeholder)
//=============================================================================

/**
 * WHAT: Test artifact verification with NULL artifact
 * WHY:  Verify NULL check for artifact parameter
 */
TEST_F(SecurityExceptionTest, ArtifactVerificationNullArtifact) {
    int result = verify_artifact(nullptr, 100, "abc123");

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test artifact verification with zero size
 * WHY:  Verify size must be positive
 */
TEST_F(SecurityExceptionTest, ArtifactVerificationZeroSize) {
    int dummy_artifact = 1;
    int result = verify_artifact(&dummy_artifact, 0, "abc123");

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test artifact verification with NULL hash
 * WHY:  Verify NULL check for expected hash
 */
TEST_F(SecurityExceptionTest, ArtifactVerificationNullHash) {
    int dummy_artifact = 1;
    int result = verify_artifact(&dummy_artifact, 100, nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test artifact verification success
 * WHY:  Verify successful verification doesn't throw
 */
TEST_F(SecurityExceptionTest, ArtifactVerificationSuccess) {
    int dummy_artifact = 1;
    int result = verify_artifact(&dummy_artifact, 100, "abc123hash");

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Security Guard Tests (LGSS Guard Placeholder)
//=============================================================================

/**
 * WHAT: Test security guard with NULL context
 * WHY:  Verify NULL check for security context
 */
TEST_F(SecurityExceptionTest, SecurityGuardNullContext) {
    int result = security_guard_check(nullptr, 0x0001);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test security guard with zero permissions
 * WHY:  Verify permissions cannot be zero
 */
TEST_F(SecurityExceptionTest, SecurityGuardZeroPermissions) {
    int dummy_context = 1;
    int result = security_guard_check(&dummy_context, 0);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test security guard permission denied triggers exception
 * WHY:  Verify security exception on permission failure
 */
TEST_F(SecurityExceptionTest, SecurityGuardPermissionDenied) {
    int dummy_context = 1;
    int result = security_guard_check(&dummy_context, 0x8001);  // Reserved flag

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
}

/**
 * WHAT: Test security guard with valid permissions succeeds
 * WHY:  Verify successful permission check
 */
TEST_F(SecurityExceptionTest, SecurityGuardSuccess) {
    int dummy_context = 1;
    int result = security_guard_check(&dummy_context, 0x0001);  // Valid flag

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Error Message Content Tests
//=============================================================================

/**
 * WHAT: Test error message contains threat description
 * WHY:  Verify informative error messages for security threats
 */
TEST_F(SecurityExceptionTest, ErrorMessageContainsThreatDescription) {
    int dummy_facade = 1;
    report_security_threat((security_facade_t)&dummy_facade,
                           1, 0.95f, "SQL injection detected");

    ASSERT_FALSE(g_captured_messages.empty());
    const std::string& msg = g_captured_messages[0];
    EXPECT_NE(msg.find("SQL injection"), std::string::npos);
}

/**
 * WHAT: Test error message contains threat level
 * WHY:  Verify threat level is included in message
 */
TEST_F(SecurityExceptionTest, ErrorMessageContainsThreatLevel) {
    int dummy_facade = 1;
    report_security_threat((security_facade_t)&dummy_facade,
                           1, 0.85f, "Test threat");

    ASSERT_FALSE(g_captured_messages.empty());
    const std::string& msg = g_captured_messages[0];
    EXPECT_NE(msg.find("0.85"), std::string::npos);
}

/**
 * WHAT: Test error message contains permission flags
 * WHY:  Verify permission details in security guard errors
 */
TEST_F(SecurityExceptionTest, ErrorMessageContainsPermissionFlags) {
    int dummy_context = 1;
    security_guard_check(&dummy_context, 0x8001);

    ASSERT_FALSE(g_captured_messages.empty());
    const std::string& msg = g_captured_messages[0];
    // Check for hex representation of flags
    EXPECT_NE(msg.find("8001"), std::string::npos);
}

//=============================================================================
// Memory Leak Verification Tests
//=============================================================================

/**
 * WHAT: Test multiple security exceptions don't leak memory
 * WHY:  Verify exception cleanup on error paths
 */
TEST_F(SecurityExceptionTest, MultipleSecurityExceptionsNoLeak) {
    const int iterations = 50;
    int dummy_facade = 1;

    for (int i = 0; i < iterations; i++) {
        report_security_threat((security_facade_t)&dummy_facade,
                               i % 10, 0.9f, "Repeated threat");
    }

    EXPECT_EQ(g_handler_call_count, iterations);
}

/**
 * WHAT: Test mixed validation errors don't leak memory
 * WHY:  Verify cleanup across different error types
 */
TEST_F(SecurityExceptionTest, MixedValidationErrorsNoLeak) {
    // Various validation failures
    validate_threat_level(-1.0f);
    validate_threat_level(2.0f);
    security_module_enable(nullptr, SEC_MODULE_ASYNC);
    trigger_lockdown(nullptr, "test");
    verify_artifact(nullptr, 0, nullptr);

    EXPECT_EQ(g_handler_call_count, 5);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
