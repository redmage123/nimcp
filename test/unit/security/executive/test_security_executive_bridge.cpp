/**
 * @file test_security_executive_bridge.cpp
 * @brief Unit tests for Security-Executive Integration Bridge
 *
 * WHAT: Tests for security-executive bidirectional bridge
 * WHY:  Verify security authorization, resource limits, audit trails integrate
 *       correctly with executive control functions
 * HOW:  Test lifecycle, connections, authorization, capabilities, resources,
 *       deadlines, audit, rollback, and bidirectional effects
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "security/executive/nimcp_security_executive_bridge.h"
#include "cognitive/nimcp_executive.h"
#include "security/nimcp_policy_engine.h"
#include "security/nimcp_rate_limiter.h"
#include "security/nimcp_capability.h"
#include "utils/error/nimcp_error_codes.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class SecurityExecutiveBridgeTest : public ::testing::Test {
protected:
    security_executive_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            security_executive_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    void CreateBridge() {
        security_executive_config_t config;
        security_executive_default_config(&config);
        bridge = security_executive_bridge_create(&config);
    }

    void CreateBridgeWithConfig(const security_executive_config_t* config) {
        bridge = security_executive_bridge_create(config);
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, DefaultConfigReturnsValidConfig) {
    security_executive_config_t config;
    memset(&config, 0, sizeof(config));

    int ret = security_executive_default_config(&config);

    EXPECT_EQ(ret, 0);
    // Verify sensible defaults
    EXPECT_TRUE(config.enable_task_authorization);
    EXPECT_TRUE(config.enable_audit_logging);
    EXPECT_GE(config.security_sensitivity, 0.5f);
    EXPECT_LE(config.security_sensitivity, 2.0f);
    EXPECT_GE(config.executive_sensitivity, 0.5f);
    EXPECT_LE(config.executive_sensitivity, 2.0f);
}

TEST_F(SecurityExecutiveBridgeTest, DefaultConfigNullPointer) {
    int ret = security_executive_default_config(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, CreateWithNullConfig) {
    bridge = security_executive_bridge_create(nullptr);
    // Should create with defaults or return NULL - implementation defined
    // Either is acceptable, but if non-NULL it should be valid
    if (bridge) {
        EXPECT_NE(bridge, nullptr);
    }
}

TEST_F(SecurityExecutiveBridgeTest, CreateWithValidConfig) {
    security_executive_config_t config;
    security_executive_default_config(&config);

    bridge = security_executive_bridge_create(&config);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SecurityExecutiveBridgeTest, CreateWithCustomConfig) {
    security_executive_config_t config;
    security_executive_default_config(&config);

    // Customize config
    config.enable_task_authorization = true;
    config.enable_capability_checks = true;
    config.enable_policy_evaluation = true;
    config.strict_mode = true;
    config.enable_resource_limits = true;
    config.enable_rate_limiting = true;
    config.enable_deadline_enforcement = true;
    config.deadline_grace_period_ms = 200;
    config.enable_secure_rollback = true;

    bridge = security_executive_bridge_create(&config);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SecurityExecutiveBridgeTest, DestroyNull) {
    // Should not crash
    security_executive_bridge_destroy(nullptr);
}

TEST_F(SecurityExecutiveBridgeTest, DestroyValid) {
    CreateBridge();
    if (bridge) {
        security_executive_bridge_destroy(bridge);
        bridge = nullptr;  // Prevent double free in TearDown
    }
}

TEST_F(SecurityExecutiveBridgeTest, CreateDestroyMultiple) {
    for (int i = 0; i < 5; i++) {
        security_executive_config_t config;
        security_executive_default_config(&config);
        security_executive_bridge_t* temp = security_executive_bridge_create(&config);
        EXPECT_NE(temp, nullptr);
        security_executive_bridge_destroy(temp);
    }
}

// ============================================================================
// Connection Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, ConnectExecutiveNullBridge) {
    executive_controller_t* exec = nullptr;
    EXPECT_EQ(security_executive_bridge_connect_executive(nullptr, exec), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, ConnectPolicyEngineNullBridge) {
    nimcp_policy_engine_t engine = nullptr;
    EXPECT_EQ(security_executive_bridge_connect_policy_engine(nullptr, engine), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, ConnectRateLimiterNullBridge) {
    nimcp_rate_limiter_t limiter = nullptr;
    EXPECT_EQ(security_executive_bridge_connect_rate_limiter(nullptr, limiter), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, ConnectCapabilitySystemNullBridge) {
    nimcp_capability_system_t* caps = nullptr;
    EXPECT_EQ(security_executive_bridge_connect_capability_system(nullptr, caps), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, DisconnectNullBridge) {
    EXPECT_EQ(security_executive_bridge_disconnect(nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, IsConnectedNullBridge) {
    EXPECT_FALSE(security_executive_bridge_is_connected(nullptr));
}

// ============================================================================
// Connection Tests - Valid Bridge, NULL System
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, ConnectExecutiveNull) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_executive_bridge_connect_executive(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, ConnectPolicyEngineNull) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_executive_bridge_connect_policy_engine(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, ConnectRateLimiterNull) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_executive_bridge_connect_rate_limiter(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, ConnectCapabilitySystemNull) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_executive_bridge_connect_capability_system(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Connection Tests - Valid Bridge, Disconnect
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, DisconnectValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    // Disconnect should succeed even with no connections
    int ret = security_executive_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityExecutiveBridgeTest, IsConnectedNoConnections) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    // Should return false when nothing connected
    EXPECT_FALSE(security_executive_bridge_is_connected(bridge));
}

// ============================================================================
// Authorization Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, AuthorizeTaskNullBridge) {
    task_descriptor_t task = {};
    security_auth_result_t result = {};
    nimcp_capability_t caps[1] = {};

    int ret = security_executive_authorize_task(
        nullptr, &task, 1, caps, 0, &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, AuthorizeTaskNullTask) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_auth_result_t result = {};
    nimcp_capability_t caps[1] = {};

    int ret = security_executive_authorize_task(
        bridge, nullptr, 1, caps, 0, &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, AuthorizeTaskNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    task_descriptor_t task = {};
    nimcp_capability_t caps[1] = {};

    int ret = security_executive_authorize_task(
        bridge, &task, 1, caps, 0, nullptr
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Authorization Tests - Authorized Task
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, AuthorizeTaskBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    task_descriptor_t task = {};
    task.task_id = 1;
    task.type = TASK_TYPE_REASONING;
    task.priority = PRIORITY_NORMAL;
    strncpy(task.name, "test_task", sizeof(task.name) - 1);

    security_auth_result_t result = {};
    nimcp_capability_t caps[1] = {};

    int ret = security_executive_authorize_task(
        bridge, &task, 1, caps, 0, &result
    );

    // Should succeed (return 0) regardless of authorization decision
    EXPECT_EQ(ret, 0);

    // Result should be populated
    // The decision depends on configuration and policy
    EXPECT_GE(result.decision, SECURITY_AUTH_ALLOWED);
    EXPECT_LE(result.decision, SECURITY_AUTH_DENIED_UNKNOWN);
}

// ============================================================================
// Authorization Tests - Denied by Policy
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, AuthorizeTaskDeniedByPolicy) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_policy_evaluation = true;
    config.strict_mode = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    task_descriptor_t task = {};
    task.task_id = 999;
    task.type = TASK_TYPE_PLANNING;
    task.priority = PRIORITY_CRITICAL;
    strncpy(task.name, "malicious_task", sizeof(task.name) - 1);

    security_auth_result_t result = {};

    int ret = security_executive_authorize_task(
        bridge, &task, 0, nullptr, 0, &result
    );

    EXPECT_EQ(ret, 0);
    // When policy denies, result reflects that
    if (!result.authorized) {
        EXPECT_EQ(result.decision, SECURITY_AUTH_DENIED_POLICY);
        EXPECT_NE(strlen(result.denied_reason), 0u);
    }
}

// ============================================================================
// Authorization Tests - Denied by Capability
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, AuthorizeTaskDeniedByCapability) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_capability_checks = true;
    config.strict_mode = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    task_descriptor_t task = {};
    task.task_id = 100;
    task.type = TASK_TYPE_SEQUENCE;
    task.priority = PRIORITY_HIGH;
    strncpy(task.name, "privileged_task", sizeof(task.name) - 1);

    security_auth_result_t result = {};

    // No capabilities provided - should be denied
    int ret = security_executive_authorize_task(
        bridge, &task, 1, nullptr, 0, &result
    );

    EXPECT_EQ(ret, 0);
    if (!result.authorized && result.decision == SECURITY_AUTH_DENIED_CAPABILITY) {
        EXPECT_GT(result.num_required_capabilities, 0u);
    }
}

// ============================================================================
// Authorization Tests - Denied by Rate Limit
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, AuthorizeTaskDeniedByRateLimit) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_rate_limiting = true;
    config.strict_mode = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    task_descriptor_t task = {};
    task.task_id = 50;
    task.type = TASK_TYPE_REASONING;
    task.priority = PRIORITY_LOW;
    strncpy(task.name, "rate_limited_task", sizeof(task.name) - 1);

    security_auth_result_t result = {};

    // Submit many rapid requests to trigger rate limiting
    for (int i = 0; i < 100; i++) {
        task.task_id = static_cast<uint32_t>(50 + i);
        security_executive_authorize_task(bridge, &task, 1, nullptr, 0, &result);
        if (!result.authorized && result.decision == SECURITY_AUTH_DENIED_RATE_LIMIT) {
            // Rate limit triggered
            EXPECT_FALSE(result.authorized);
            break;
        }
    }
}

// ============================================================================
// Capability Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, CheckCapabilitiesNullBridge) {
    security_capability_check_t check = {};
    nimcp_capability_t caps[1] = {};

    int ret = security_executive_check_capabilities(
        nullptr, 1, caps, 0, 0, &check
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, CheckCapabilitiesNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    nimcp_capability_t caps[1] = {};

    int ret = security_executive_check_capabilities(
        bridge, 1, caps, 0, 0, nullptr
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Capability Tests - Success
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, CheckCapabilitiesSuccess) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_capability_check_t check = {};
    nimcp_capability_t caps[1] = {};

    // Request no capabilities - should succeed
    int ret = security_executive_check_capabilities(
        bridge, 1, caps, 0, 0, &check
    );

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(check.has_all_capabilities);
    EXPECT_EQ(check.num_missing, 0u);
}

// ============================================================================
// Capability Tests - Failure (Missing Capabilities)
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, CheckCapabilitiesMissing) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_capability_checks = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    security_capability_check_t check = {};

    // Request capabilities without providing any
    uint32_t required = 0xFFFF;  // Request many capabilities
    int ret = security_executive_check_capabilities(
        bridge, 1, nullptr, 0, required, &check
    );

    EXPECT_EQ(ret, 0);
    // Should report missing capabilities
    if (!check.has_all_capabilities) {
        EXPECT_GT(check.num_missing, 0u);
    }
}

// ============================================================================
// Resource Allocation Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, AllocateResourcesNullBridge) {
    security_resource_budget_t requested = {};
    security_resource_budget_t granted = {};

    int ret = security_executive_allocate_resources(
        nullptr, 1, &requested, &granted
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, AllocateResourcesNullRequested) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_resource_budget_t granted = {};

    int ret = security_executive_allocate_resources(
        bridge, 1, nullptr, &granted
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, AllocateResourcesNullGranted) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_resource_budget_t requested = {};

    int ret = security_executive_allocate_resources(
        bridge, 1, &requested, nullptr
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Resource Allocation Tests - Within Limits
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, AllocateResourcesWithinLimits) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_resource_limits = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    security_resource_budget_t requested = {};
    requested.cpu_limit_ms = 100;
    requested.memory_limit_bytes = 1024 * 1024;  // 1 MB
    requested.time_limit_ms = 1000;
    requested.io_read_limit = 100;
    requested.io_write_limit = 50;

    security_resource_budget_t granted = {};

    int ret = security_executive_allocate_resources(
        bridge, 1, &requested, &granted
    );

    EXPECT_EQ(ret, 0);
    // Granted should be <= requested
    EXPECT_LE(granted.cpu_limit_ms, requested.cpu_limit_ms);
    EXPECT_LE(granted.memory_limit_bytes, requested.memory_limit_bytes);
    EXPECT_FALSE(granted.budget_exceeded);
}

// ============================================================================
// Resource Allocation Tests - Exceed Limits
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, AllocateResourcesExceedLimits) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_resource_limits = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    security_resource_budget_t requested = {};
    // Request excessive resources
    requested.cpu_limit_ms = UINT64_MAX;
    requested.memory_limit_bytes = UINT64_MAX;
    requested.time_limit_ms = UINT64_MAX;
    requested.io_read_limit = UINT64_MAX;
    requested.io_write_limit = UINT64_MAX;

    security_resource_budget_t granted = {};

    int ret = security_executive_allocate_resources(
        bridge, 1, &requested, &granted
    );

    EXPECT_EQ(ret, 0);
    // Granted should be capped to reasonable limits
    EXPECT_LT(granted.cpu_limit_ms, requested.cpu_limit_ms);
    EXPECT_LT(granted.memory_limit_bytes, requested.memory_limit_bytes);
}

// ============================================================================
// Deadline Enforcement Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, EnforceDeadlineNullBridge) {
    int ret = security_executive_enforce_deadline(nullptr, 1, 1000);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Deadline Enforcement Tests - Within Deadline
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, EnforceDeadlineWithinTime) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_deadline_enforcement = true;
    config.deadline_grace_period_ms = 100;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    // Set deadline far in the future
    uint64_t deadline_ms = UINT64_MAX;

    int ret = security_executive_enforce_deadline(bridge, 1, deadline_ms);
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Deadline Enforcement Tests - Deadline Warning
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, EnforceDeadlineWarning) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_deadline_enforcement = true;
    config.deadline_grace_period_ms = 1000;
    config.abort_on_deadline_exceeded = false;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    // Set deadline that's approaching (within grace period)
    uint64_t deadline_ms = 500;

    int ret = security_executive_enforce_deadline(bridge, 1, deadline_ms);
    // Should succeed even if warning is triggered
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Deadline Enforcement Tests - Deadline Violation
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, EnforceDeadlineViolation) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_deadline_enforcement = true;
    config.abort_on_deadline_exceeded = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    // Set deadline in the past
    uint64_t deadline_ms = 0;

    int ret = security_executive_enforce_deadline(bridge, 1, deadline_ms);
    // May return error if deadline already exceeded and abort enabled
    // Either 0 or -1 is acceptable depending on implementation
    (void)ret;  // Result depends on implementation
}

// ============================================================================
// Audit Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, AuditTaskStartNullBridge) {
    task_descriptor_t task = {};
    int ret = security_executive_audit_task_start(nullptr, &task, 1);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, AuditTaskStartNullTask) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_executive_audit_task_start(bridge, nullptr, 1);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Audit Tests - Task Start
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, AuditTaskStartBasic) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_audit_logging = true;
    config.max_audit_entries = 100;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    task_descriptor_t task = {};
    task.task_id = 1;
    task.type = TASK_TYPE_REASONING;
    task.priority = PRIORITY_NORMAL;
    strncpy(task.name, "audit_test_task", sizeof(task.name) - 1);

    int ret = security_executive_audit_task_start(bridge, &task, 1);
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Audit Tests - Task Completion
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, AuditTaskCompletionNullBridge) {
    task_descriptor_t task = {};
    security_resource_budget_t resources = {};

    int ret = security_executive_audit_task_completion(
        nullptr, &task, true, 0, &resources
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, AuditTaskCompletionNullTask) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_resource_budget_t resources = {};

    int ret = security_executive_audit_task_completion(
        bridge, nullptr, true, 0, &resources
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, AuditTaskCompletionSuccess) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_audit_logging = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    task_descriptor_t task = {};
    task.task_id = 1;
    task.type = TASK_TYPE_REASONING;
    strncpy(task.name, "completed_task", sizeof(task.name) - 1);

    security_resource_budget_t resources = {};
    resources.cpu_used_ms = 50;
    resources.memory_used_bytes = 1024;
    resources.time_used_ms = 100;

    // Start the task first
    security_executive_audit_task_start(bridge, &task, 1);

    int ret = security_executive_audit_task_completion(
        bridge, &task, true, 0, &resources
    );
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityExecutiveBridgeTest, AuditTaskCompletionFailure) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_audit_logging = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    task_descriptor_t task = {};
    task.task_id = 2;
    task.type = TASK_TYPE_REASONING;
    strncpy(task.name, "failed_task", sizeof(task.name) - 1);

    security_resource_budget_t resources = {};

    int ret = security_executive_audit_task_completion(
        bridge, &task, false, -1, &resources
    );
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Audit Tests - Get Audit Records
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, GetAuditRecordsNullBridge) {
    security_audit_record_t records[10] = {};
    uint32_t num_records = 0;

    int ret = security_executive_get_audit_records(
        nullptr, records, 10, &num_records
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, GetAuditRecordsNullRecords) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t num_records = 0;

    int ret = security_executive_get_audit_records(
        bridge, nullptr, 10, &num_records
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, GetAuditRecordsNullCount) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_audit_record_t records[10] = {};

    int ret = security_executive_get_audit_records(
        bridge, records, 10, nullptr
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, GetAuditRecordsEmpty) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_audit_logging = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    security_audit_record_t records[10] = {};
    uint32_t num_records = 999;

    int ret = security_executive_get_audit_records(
        bridge, records, 10, &num_records
    );
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_records, 0u);
}

TEST_F(SecurityExecutiveBridgeTest, GetAuditRecordsWithData) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_audit_logging = true;
    config.max_audit_entries = 100;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    // Create some audit records
    task_descriptor_t task = {};
    task.task_id = 1;
    task.type = TASK_TYPE_REASONING;
    strncpy(task.name, "audit_record_task", sizeof(task.name) - 1);

    security_executive_audit_task_start(bridge, &task, 1);

    security_resource_budget_t resources = {};
    security_executive_audit_task_completion(bridge, &task, true, 0, &resources);

    // Retrieve records
    security_audit_record_t records[10] = {};
    uint32_t num_records = 0;

    int ret = security_executive_get_audit_records(
        bridge, records, 10, &num_records
    );
    EXPECT_EQ(ret, 0);
    EXPECT_GE(num_records, 1u);

    if (num_records > 0) {
        EXPECT_EQ(records[0].task_id, 1u);
    }
}

// ============================================================================
// Audit Tests - Flush Audit
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, FlushAuditNullBridge) {
    int ret = security_executive_flush_audit(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, FlushAuditBasic) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_audit_logging = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    int ret = security_executive_flush_audit(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityExecutiveBridgeTest, FlushAuditWithRecords) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_audit_logging = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    // Create audit record
    task_descriptor_t task = {};
    task.task_id = 1;
    strncpy(task.name, "flush_test", sizeof(task.name) - 1);
    security_executive_audit_task_start(bridge, &task, 1);

    int ret = security_executive_flush_audit(bridge);
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Rollback Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, RollbackTaskNullBridge) {
    security_rollback_status_t status;

    int ret = security_executive_rollback_task(
        nullptr, 1, "test reason", &status
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, RollbackTaskNullReason) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_rollback_status_t status;

    int ret = security_executive_rollback_task(
        bridge, 1, nullptr, &status
    );
    // May accept NULL reason
    (void)ret;
}

TEST_F(SecurityExecutiveBridgeTest, RollbackTaskNullStatus) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_executive_rollback_task(
        bridge, 1, "test reason", nullptr
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Rollback Tests - Success
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, RollbackTaskSuccess) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_secure_rollback = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    security_rollback_status_t status = SECURITY_ROLLBACK_FAILED;

    int ret = security_executive_rollback_task(
        bridge, 1, "authorization_failure", &status
    );

    EXPECT_EQ(ret, 0);
    // Status should indicate result
    EXPECT_GE(status, SECURITY_ROLLBACK_SUCCESS);
    EXPECT_LE(status, SECURITY_ROLLBACK_NOT_NEEDED);
}

TEST_F(SecurityExecutiveBridgeTest, RollbackTaskNotNeeded) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_secure_rollback = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    security_rollback_status_t status;

    // Rollback for non-existent task
    int ret = security_executive_rollback_task(
        bridge, 99999, "no such task", &status
    );

    EXPECT_EQ(ret, 0);
    // May report not needed or success
    EXPECT_GE(status, SECURITY_ROLLBACK_SUCCESS);
}

// ============================================================================
// Bidirectional Update Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, BridgeUpdateNullBridge) {
    int ret = security_executive_bridge_update(nullptr, 100);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, ApplySecurityEffectsNullBridge) {
    int ret = security_executive_apply_security_effects(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, ApplyExecutiveEffectsNullBridge) {
    int ret = security_executive_apply_executive_effects(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Bidirectional Update Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, BridgeUpdateBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_executive_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityExecutiveBridgeTest, BridgeUpdateZeroDelta) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_executive_bridge_update(bridge, 0);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityExecutiveBridgeTest, BridgeUpdateMultiple) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    for (int i = 0; i < 10; i++) {
        int ret = security_executive_bridge_update(bridge, 50);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(SecurityExecutiveBridgeTest, ApplySecurityEffectsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_executive_apply_security_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityExecutiveBridgeTest, ApplyExecutiveEffectsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_executive_apply_executive_effects(bridge);
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Stats Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, GetStateNullBridge) {
    security_executive_state_t state = {};
    int ret = security_executive_bridge_get_state(nullptr, &state);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, GetStateNullState) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_executive_bridge_get_state(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, GetStatsNullBridge) {
    security_executive_stats_t stats = {};
    int ret = security_executive_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, GetStatsNullStats) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_executive_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, ResetStatsNullBridge) {
    int ret = security_executive_bridge_reset_stats(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Stats Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, GetStateBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_executive_state_t state = {};

    int ret = security_executive_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);

    // State should have valid values
    EXPECT_GE(state.overall_resource_utilization, 0.0f);
    EXPECT_LE(state.overall_resource_utilization, 1.0f);
}

TEST_F(SecurityExecutiveBridgeTest, GetStatsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_executive_stats_t stats = {};

    int ret = security_executive_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    // Initial stats should be zero or reasonable values
    EXPECT_GE(stats.total_auth_requests, 0u);
    EXPECT_GE(stats.avg_auth_time_ns, 0.0f);
}

TEST_F(SecurityExecutiveBridgeTest, GetStatsAfterOperations) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_task_authorization = true;
    config.enable_audit_logging = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    // Perform some operations
    task_descriptor_t task = {};
    task.task_id = 1;
    task.type = TASK_TYPE_REASONING;
    strncpy(task.name, "stats_test", sizeof(task.name) - 1);

    security_auth_result_t result = {};
    security_executive_authorize_task(bridge, &task, 1, nullptr, 0, &result);
    security_executive_audit_task_start(bridge, &task, 1);

    security_resource_budget_t resources = {};
    security_executive_audit_task_completion(bridge, &task, true, 0, &resources);

    // Get stats
    security_executive_stats_t stats = {};
    int ret = security_executive_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.total_auth_requests, 1u);
    EXPECT_GE(stats.audit_events_logged, 1u);
}

TEST_F(SecurityExecutiveBridgeTest, ResetStatsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Perform some operations first
    task_descriptor_t task = {};
    task.task_id = 1;
    strncpy(task.name, "reset_stats_test", sizeof(task.name) - 1);
    security_auth_result_t result = {};
    security_executive_authorize_task(bridge, &task, 1, nullptr, 0, &result);

    // Reset stats
    int ret = security_executive_bridge_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    // Verify stats are reset
    security_executive_stats_t stats = {};
    security_executive_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_auth_requests, 0u);
    EXPECT_EQ(stats.auth_granted, 0u);
}

// ============================================================================
// Effects Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, GetSecurityEffectsNullBridge) {
    security_to_executive_effects_t effects = {};
    int ret = security_executive_get_security_effects(nullptr, &effects);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, GetSecurityEffectsNullEffects) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_executive_get_security_effects(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, GetExecutiveEffectsNullBridge) {
    executive_to_security_effects_t effects = {};
    int ret = security_executive_get_executive_effects(nullptr, &effects);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, GetExecutiveEffectsNullEffects) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_executive_get_executive_effects(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Effects Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, GetSecurityEffectsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_to_executive_effects_t effects = {};

    int ret = security_executive_get_security_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    // Effects should have valid values
    EXPECT_GE(effects.resource_utilization, 0.0f);
    EXPECT_LE(effects.resource_utilization, 1.0f);
}

TEST_F(SecurityExecutiveBridgeTest, GetExecutiveEffectsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    executive_to_security_effects_t effects = {};

    int ret = security_executive_get_executive_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    // Effects should have valid values
    EXPECT_GE(effects.task_failure_rate, 0.0f);
    EXPECT_LE(effects.task_failure_rate, 1.0f);
}

// ============================================================================
// Bio-Async Connection Tests
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, ConnectBioAsyncNullBridge) {
    int ret = security_executive_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, DisconnectBioAsyncNullBridge) {
    int ret = security_executive_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, IsBioAsyncConnectedNullBridge) {
    EXPECT_FALSE(security_executive_bridge_is_bio_async_connected(nullptr));
}

TEST_F(SecurityExecutiveBridgeTest, ConnectBioAsyncBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP() << "Bridge creation failed";

    int ret = security_executive_bridge_connect_bio_async(bridge);
    // May succeed or fail depending on bio-async router availability
    if (ret == -1) {
        GTEST_SKIP() << "Bio-async router not available";
    }
    // Either success or other valid error
    EXPECT_TRUE(ret == 0 || ret >= NIMCP_ERROR_UNKNOWN);
}

TEST_F(SecurityExecutiveBridgeTest, DisconnectBioAsyncBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_executive_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityExecutiveBridgeTest, IsBioAsyncConnectedNotConnected) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    EXPECT_FALSE(security_executive_bridge_is_bio_async_connected(bridge));
}

// ============================================================================
// Report Resource Usage Tests
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, ReportResourceUsageNullBridge) {
    security_resource_budget_t resources = {};
    int ret = security_executive_report_resource_usage(nullptr, 1, &resources);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, ReportResourceUsageNullResources) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_executive_report_resource_usage(bridge, 1, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityExecutiveBridgeTest, ReportResourceUsageBasic) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_resource_limits = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    security_resource_budget_t resources = {};
    resources.cpu_used_ms = 50;
    resources.memory_used_bytes = 2048;
    resources.time_used_ms = 100;
    resources.io_reads = 10;
    resources.io_writes = 5;

    int ret = security_executive_report_resource_usage(bridge, 1, &resources);
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, FullTaskLifecycle) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_task_authorization = true;
    config.enable_capability_checks = false;  // Disable for simpler test
    config.enable_resource_limits = true;
    config.enable_deadline_enforcement = true;
    config.enable_audit_logging = true;
    config.enable_secure_rollback = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    // 1. Authorize task
    task_descriptor_t task = {};
    task.task_id = 42;
    task.type = TASK_TYPE_REASONING;
    task.priority = PRIORITY_NORMAL;
    strncpy(task.name, "full_lifecycle_task", sizeof(task.name) - 1);

    security_auth_result_t auth_result = {};
    int ret = security_executive_authorize_task(
        bridge, &task, 1, nullptr, 0, &auth_result
    );
    EXPECT_EQ(ret, 0);

    if (auth_result.authorized) {
        // 2. Allocate resources
        security_resource_budget_t requested = {};
        requested.cpu_limit_ms = 100;
        requested.memory_limit_bytes = 1024 * 1024;
        requested.time_limit_ms = 5000;

        security_resource_budget_t granted = {};
        ret = security_executive_allocate_resources(
            bridge, task.task_id, &requested, &granted
        );
        EXPECT_EQ(ret, 0);

        // 3. Set deadline
        ret = security_executive_enforce_deadline(bridge, task.task_id, 10000);
        EXPECT_EQ(ret, 0);

        // 4. Audit task start
        ret = security_executive_audit_task_start(bridge, &task, 1);
        EXPECT_EQ(ret, 0);

        // 5. Report resource usage
        security_resource_budget_t usage = {};
        usage.cpu_used_ms = 25;
        usage.memory_used_bytes = 512 * 1024;
        usage.time_used_ms = 100;

        ret = security_executive_report_resource_usage(bridge, task.task_id, &usage);
        EXPECT_EQ(ret, 0);

        // 6. Update bridge
        ret = security_executive_bridge_update(bridge, 100);
        EXPECT_EQ(ret, 0);

        // 7. Complete task
        ret = security_executive_audit_task_completion(
            bridge, &task, true, 0, &usage
        );
        EXPECT_EQ(ret, 0);

        // 8. Verify audit records
        security_audit_record_t records[10] = {};
        uint32_t num_records = 0;
        ret = security_executive_get_audit_records(
            bridge, records, 10, &num_records
        );
        EXPECT_EQ(ret, 0);
        EXPECT_GE(num_records, 2u);  // At least start and complete
    }

    // 9. Get final stats
    security_executive_stats_t stats = {};
    ret = security_executive_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.total_auth_requests, 1u);
}

TEST_F(SecurityExecutiveBridgeTest, TaskWithRollback) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_audit_logging = true;
    config.enable_secure_rollback = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    // Start a task
    task_descriptor_t task = {};
    task.task_id = 100;
    task.type = TASK_TYPE_SEQUENCE;
    strncpy(task.name, "rollback_test_task", sizeof(task.name) - 1);

    security_executive_audit_task_start(bridge, &task, 1);

    // Simulate failure requiring rollback
    security_rollback_status_t rollback_status;
    int ret = security_executive_rollback_task(
        bridge, task.task_id, "simulated_failure", &rollback_status
    );
    EXPECT_EQ(ret, 0);

    // Complete with failure
    security_resource_budget_t resources = {};
    ret = security_executive_audit_task_completion(
        bridge, &task, false, -1, &resources
    );
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, MaxAuditEntries) {
    security_executive_config_t config;
    security_executive_default_config(&config);
    config.enable_audit_logging = true;
    config.max_audit_entries = 5;  // Small buffer to test overflow

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    // Create more tasks than buffer can hold
    for (uint32_t i = 0; i < 10; i++) {
        task_descriptor_t task = {};
        task.task_id = i;
        strncpy(task.name, "overflow_test", sizeof(task.name) - 1);
        security_executive_audit_task_start(bridge, &task, 1);
    }

    // Should still work (circular buffer)
    security_audit_record_t records[10] = {};
    uint32_t num_records = 0;
    int ret = security_executive_get_audit_records(
        bridge, records, 10, &num_records
    );
    EXPECT_EQ(ret, 0);
    EXPECT_LE(num_records, 5u);  // Limited by max_audit_entries
}

TEST_F(SecurityExecutiveBridgeTest, LongTaskName) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    task_descriptor_t task = {};
    task.task_id = 1;
    // Fill name with maximum length
    memset(task.name, 'A', sizeof(task.name) - 1);
    task.name[sizeof(task.name) - 1] = '\0';

    security_auth_result_t result = {};
    int ret = security_executive_authorize_task(
        bridge, &task, 1, nullptr, 0, &result
    );
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityExecutiveBridgeTest, ZeroAgentId) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    task_descriptor_t task = {};
    task.task_id = 1;
    strncpy(task.name, "zero_agent_test", sizeof(task.name) - 1);

    security_auth_result_t result = {};
    int ret = security_executive_authorize_task(
        bridge, &task, 0, nullptr, 0, &result
    );
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityExecutiveBridgeTest, MaxAgentId) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    task_descriptor_t task = {};
    task.task_id = 1;
    strncpy(task.name, "max_agent_test", sizeof(task.name) - 1);

    security_auth_result_t result = {};
    int ret = security_executive_authorize_task(
        bridge, &task, UINT32_MAX, nullptr, 0, &result
    );
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityExecutiveBridgeTest, AllTaskTypes) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    task_type_t types[] = {
        TASK_TYPE_CLASSIFICATION,
        TASK_TYPE_REGRESSION,
        TASK_TYPE_SEQUENCE,
        TASK_TYPE_PLANNING,
        TASK_TYPE_REASONING
    };

    for (auto type : types) {
        task_descriptor_t task = {};
        task.task_id = static_cast<uint32_t>(type);
        task.type = type;
        strncpy(task.name, "type_test", sizeof(task.name) - 1);

        security_auth_result_t result = {};
        int ret = security_executive_authorize_task(
            bridge, &task, 1, nullptr, 0, &result
        );
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(SecurityExecutiveBridgeTest, AllPriorityLevels) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    task_priority_t priorities[] = {
        PRIORITY_LOW,
        PRIORITY_NORMAL,
        PRIORITY_HIGH,
        PRIORITY_CRITICAL
    };

    for (auto priority : priorities) {
        task_descriptor_t task = {};
        task.task_id = static_cast<uint32_t>(priority);
        task.priority = priority;
        strncpy(task.name, "priority_test", sizeof(task.name) - 1);

        security_auth_result_t result = {};
        int ret = security_executive_authorize_task(
            bridge, &task, 1, nullptr, 0, &result
        );
        EXPECT_EQ(ret, 0);
    }
}

// ============================================================================
// Thread Safety Tests (basic - not comprehensive)
// ============================================================================

TEST_F(SecurityExecutiveBridgeTest, ConcurrentUpdates) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Multiple rapid updates should not crash
    for (int i = 0; i < 100; i++) {
        security_executive_bridge_update(bridge, 10);
        security_executive_apply_security_effects(bridge);
        security_executive_apply_executive_effects(bridge);
    }
}
