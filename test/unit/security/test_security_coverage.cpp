/**
 * @file test_security_coverage.cpp
 * @brief Unit tests for Security Coverage Framework (Phase SC-1)
 *
 * Tests all new security modules:
 *   - Security Coverage
 *   - Continuous Monitor
 *   - Control Flow Integrity (CFI)
 *   - Shadow Stack
 *   - Capability-Based Access Control
 *   - Fractal Security
 *   - Security Audit
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#include <gtest/gtest.h>
#include <string.h>
#include <string>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "security/nimcp_security_coverage.h"
#include "security/nimcp_continuous_monitor.h"
#include "security/nimcp_cfi.h"
#include "security/nimcp_shadow_stack.h"
#include "security/nimcp_capability.h"
#include "security/nimcp_security_fractal.h"
#include "security/nimcp_security_audit.h"
#include "security/nimcp_security.h"  // For nimcp_threat_level_t

//=============================================================================
// Security Coverage Tests
//=============================================================================

class SecurityCoverageTest : public ::testing::Test {
protected:
    nimcp_security_coverage_t* coverage;

    void SetUp() override {
        coverage = nimcp_security_coverage_create();
        ASSERT_NE(coverage, nullptr);

        ASSERT_EQ(nimcp_security_coverage_init(coverage), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (coverage) {
            nimcp_security_coverage_destroy(coverage);
            coverage = nullptr;
        }
    }
};

TEST_F(SecurityCoverageTest, CreateAndInit) {
    EXPECT_NE(coverage, nullptr);
}

TEST_F(SecurityCoverageTest, RegisterMemoryRegion) {
    int test_data[100];
    int32_t region_id = nimcp_coverage_register_region(
        coverage, test_data, sizeof(test_data),
        NIMCP_PROTECTION_HASH_VERIFIED, "test_data");
    EXPECT_GE(region_id, 0);
}

TEST_F(SecurityCoverageTest, VerifyMemoryRegion) {
    int test_data[100] = {0};

    int32_t region_id = nimcp_coverage_register_region(
        coverage, test_data, sizeof(test_data),
        NIMCP_PROTECTION_HASH_VERIFIED, "test_data");
    ASSERT_GE(region_id, 0);

    // Should pass verification when unchanged
    EXPECT_TRUE(nimcp_coverage_verify_region(coverage, region_id));
}

TEST_F(SecurityCoverageTest, VerifyAllRegions) {
    int data1[10] = {0}, data2[10] = {0};

    nimcp_coverage_register_region(coverage, data1, sizeof(data1),
        NIMCP_PROTECTION_HASH_VERIFIED, "data1");
    nimcp_coverage_register_region(coverage, data2, sizeof(data2),
        NIMCP_PROTECTION_HASH_VERIFIED, "data2");

    // Should pass when all regions unchanged
    EXPECT_TRUE(nimcp_coverage_verify_all_regions(coverage));
}

TEST_F(SecurityCoverageTest, CodePathRegistration) {
    void* entry_point = (void*)0x1000;
    nimcp_result_t result = nimcp_coverage_register_code_path(
        coverage, entry_point, 1);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SecurityCoverageTest, InputChannelRegistration) {
    int32_t channel_id = nimcp_coverage_register_input_channel(
        coverage, "stdin", true);
    EXPECT_GE(channel_id, 0);
}

TEST_F(SecurityCoverageTest, OutputChannelRegistration) {
    int32_t channel_id = nimcp_coverage_register_output_channel(
        coverage, "stdout", true, true);
    EXPECT_GE(channel_id, 0);
}

TEST_F(SecurityCoverageTest, IPCEndpointRegistration) {
    int32_t endpoint_id = nimcp_coverage_register_ipc_endpoint(
        coverage, "main_bus", true, true, 0xFF);
    EXPECT_GE(endpoint_id, 0);
}

TEST_F(SecurityCoverageTest, CoverageVerifyAll) {
    // Register some coverage items
    int data1[10], data2[10];
    nimcp_coverage_register_region(coverage, data1, sizeof(data1),
        NIMCP_PROTECTION_HASH_VERIFIED, "data1");
    nimcp_coverage_register_input_channel(coverage, "input1", true);

    // Check coverage report
    nimcp_coverage_report_t report;
    nimcp_result_t result = nimcp_coverage_verify_all(coverage, &report);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SecurityCoverageTest, CoveragePercentage) {
    // Register some items (use HASH_VERIFIED, not FULL, since FULL uses mprotect
    // which requires page-aligned memory)
    int data[10] = {0};
    nimcp_coverage_register_region(coverage, data, sizeof(data),
        NIMCP_PROTECTION_HASH_VERIFIED, "data");

    float percentage = nimcp_coverage_get_percentage(coverage);
    EXPECT_GE(percentage, 0.0f);
    EXPECT_LE(percentage, 100.0f);
}

TEST_F(SecurityCoverageTest, Heartbeat) {
    nimcp_result_t result = nimcp_coverage_heartbeat(coverage);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SecurityCoverageTest, TemporalCoverage) {
    nimcp_coverage_heartbeat(coverage);
    bool temporal_ok = nimcp_coverage_check_temporal(coverage, 1000);
    EXPECT_TRUE(temporal_ok);
}

//=============================================================================
// Continuous Monitor Tests
//=============================================================================

class ContinuousMonitorTest : public ::testing::Test {
protected:
    nimcp_security_coverage_t* coverage;
    nimcp_continuous_monitor_t* monitor;

    void SetUp() override {
        coverage = nimcp_security_coverage_create();
        ASSERT_NE(coverage, nullptr);
        ASSERT_EQ(nimcp_security_coverage_init(coverage), NIMCP_SUCCESS);

        monitor = nimcp_monitor_create(coverage);
        ASSERT_NE(monitor, nullptr);

        nimcp_monitor_config_t config;
        nimcp_monitor_get_default_config(&config);
        config.interval_ms = 100;  // Fast interval for testing
        ASSERT_EQ(nimcp_monitor_init(monitor, &config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (monitor) {
            nimcp_monitor_stop(monitor);
            nimcp_monitor_destroy(monitor);
            monitor = nullptr;
        }
        if (coverage) {
            nimcp_security_coverage_destroy(coverage);
            coverage = nullptr;
        }
    }
};

TEST_F(ContinuousMonitorTest, CreateAndInit) {
    EXPECT_NE(monitor, nullptr);
}

TEST_F(ContinuousMonitorTest, GetState) {
    nimcp_monitor_state_t state = nimcp_monitor_get_state(monitor);
    EXPECT_EQ(state, NIMCP_MONITOR_STATE_STOPPED);
}

TEST_F(ContinuousMonitorTest, StartStop) {
    nimcp_result_t result = nimcp_monitor_start(monitor);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_monitor_get_state(monitor), NIMCP_MONITOR_STATE_RUNNING);

    result = nimcp_monitor_stop(monitor);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_monitor_get_state(monitor), NIMCP_MONITOR_STATE_STOPPED);
}

TEST_F(ContinuousMonitorTest, PauseResume) {
    nimcp_monitor_start(monitor);

    nimcp_result_t result = nimcp_monitor_pause(monitor);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_monitor_get_state(monitor), NIMCP_MONITOR_STATE_PAUSED);

    result = nimcp_monitor_resume(monitor);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_monitor_get_state(monitor), NIMCP_MONITOR_STATE_RUNNING);
}

TEST_F(ContinuousMonitorTest, ManualCycle) {
    nimcp_result_t result = nimcp_monitor_run_cycle(monitor);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ContinuousMonitorTest, Statistics) {
    nimcp_monitor_stats_t stats;
    nimcp_result_t result = nimcp_monitor_get_stats(monitor, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ContinuousMonitorTest, AlertCount) {
    uint32_t count = nimcp_monitor_get_alert_count(monitor);
    // Initially should be 0
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// Control Flow Integrity Tests
//=============================================================================

class CFITest : public ::testing::Test {
protected:
    nimcp_cfi_context_t* cfi;

    void SetUp() override {
        cfi = nimcp_cfi_create();
        ASSERT_NE(cfi, nullptr);

        ASSERT_EQ(nimcp_cfi_init(cfi, NIMCP_CFI_MODE_ENFORCE), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (cfi) {
            nimcp_cfi_destroy(cfi);
            cfi = nullptr;
        }
    }
};

// Dummy function for testing
static void test_callback(void) {}

TEST_F(CFITest, CreateAndInit) {
    EXPECT_NE(cfi, nullptr);
}

TEST_F(CFITest, RegisterTarget) {
    int32_t target_id = nimcp_cfi_register_target(
        cfi, (void*)test_callback, 0x1001, NIMCP_FUNC_TYPE_CALLBACK, "test_callback");
    EXPECT_GE(target_id, 0);
}

TEST_F(CFITest, IsValidTarget) {
    nimcp_cfi_register_target(cfi, (void*)test_callback, 0x1001,
        NIMCP_FUNC_TYPE_CALLBACK, "test_callback");

    // Registered target should be valid
    EXPECT_TRUE(nimcp_cfi_is_valid_target(cfi, (void*)test_callback));
}

TEST_F(CFITest, ValidatePtr) {
    nimcp_cfi_register_target(cfi, (void*)test_callback, 0x1001,
        NIMCP_FUNC_TYPE_CALLBACK, "test_callback");

    // Should validate registered target with matching type
    EXPECT_TRUE(nimcp_cfi_validate_ptr(cfi, (void*)test_callback, 0x1001));
}

TEST_F(CFITest, RejectUnregisteredTarget) {
    // Unregistered target should fail validation
    EXPECT_FALSE(nimcp_cfi_is_valid_target(cfi, (void*)0xDEADBEEF));
}

TEST_F(CFITest, CheckCall) {
    nimcp_cfi_register_target(cfi, (void*)test_callback, 0x1001,
        NIMCP_FUNC_TYPE_CALLBACK, "test_callback");

    nimcp_cfi_result_t result = nimcp_cfi_check_call(cfi, (void*)test_callback, 0x1001);
    EXPECT_EQ(result, NIMCP_CFI_VALID);
}

TEST_F(CFITest, RejectNullTarget) {
    nimcp_cfi_result_t result = nimcp_cfi_check_call(cfi, nullptr, 0x1001);
    EXPECT_EQ(result, NIMCP_CFI_NULL_TARGET);
}

TEST_F(CFITest, SetMode) {
    nimcp_result_t result = nimcp_cfi_set_mode(cfi, NIMCP_CFI_MODE_DETECT);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_cfi_get_mode(cfi), NIMCP_CFI_MODE_DETECT);
}

TEST_F(CFITest, Statistics) {
    nimcp_cfi_stats_t stats;
    nimcp_result_t result = nimcp_cfi_get_stats(cfi, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CFITest, ViolationCount) {
    uint64_t violations = nimcp_cfi_get_violation_count(cfi);
    EXPECT_EQ(violations, 0u);  // Initially zero
}

//=============================================================================
// Shadow Stack Tests
//=============================================================================

class ShadowStackTest : public ::testing::Test {
protected:
    nimcp_shadow_stack_t* ss;

    void SetUp() override {
        ss = nimcp_shadow_stack_create(0);  // Default size
        ASSERT_NE(ss, nullptr);

        ASSERT_EQ(nimcp_shadow_stack_init(ss, NIMCP_SS_MODE_ENFORCE), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (ss) {
            nimcp_shadow_stack_destroy(ss);
            ss = nullptr;
        }
    }
};

TEST_F(ShadowStackTest, CreateAndInit) {
    EXPECT_NE(ss, nullptr);
}

TEST_F(ShadowStackTest, PushPop) {
    void* ret_addr = (void*)0x12345678;

    nimcp_ss_result_t result = nimcp_shadow_stack_push(ss, ret_addr);
    EXPECT_EQ(result, NIMCP_SS_OK);
    EXPECT_FALSE(nimcp_shadow_stack_is_empty(ss));

    result = nimcp_shadow_stack_pop(ss, ret_addr);
    EXPECT_EQ(result, NIMCP_SS_OK);
    EXPECT_TRUE(nimcp_shadow_stack_is_empty(ss));
}

TEST_F(ShadowStackTest, MismatchDetection) {
    void* ret_addr1 = (void*)0x12345678;
    void* ret_addr2 = (void*)0xDEADBEEF;

    nimcp_shadow_stack_push(ss, ret_addr1);

    // Mismatched return should be detected
    nimcp_ss_result_t result = nimcp_shadow_stack_pop(ss, ret_addr2);
    EXPECT_EQ(result, NIMCP_SS_MISMATCH);
}

TEST_F(ShadowStackTest, Depth) {
    void* addr1 = (void*)0x1000;
    void* addr2 = (void*)0x2000;
    void* addr3 = (void*)0x3000;

    nimcp_shadow_stack_push(ss, addr1);
    nimcp_shadow_stack_push(ss, addr2);
    nimcp_shadow_stack_push(ss, addr3);

    EXPECT_EQ(nimcp_shadow_stack_depth(ss), 3u);
}

TEST_F(ShadowStackTest, Peek) {
    void* ret_addr = (void*)0xABCDEF00;
    nimcp_shadow_stack_push(ss, ret_addr);

    void* peeked;
    nimcp_ss_result_t result = nimcp_shadow_stack_peek(ss, &peeked);
    EXPECT_EQ(result, NIMCP_SS_OK);
    EXPECT_EQ(peeked, ret_addr);

    // Stack should still have the entry
    EXPECT_EQ(nimcp_shadow_stack_depth(ss), 1u);
}

TEST_F(ShadowStackTest, Verify) {
    void* addr1 = (void*)0x1000;
    void* addr2 = (void*)0x2000;

    nimcp_shadow_stack_push(ss, addr1);
    nimcp_shadow_stack_push(ss, addr2);

    EXPECT_TRUE(nimcp_shadow_stack_verify(ss));
}

TEST_F(ShadowStackTest, Clear) {
    void* addr1 = (void*)0x1000;
    nimcp_shadow_stack_push(ss, addr1);
    nimcp_shadow_stack_push(ss, addr1);

    nimcp_result_t result = nimcp_shadow_stack_clear(ss);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(nimcp_shadow_stack_is_empty(ss));
}

TEST_F(ShadowStackTest, Statistics) {
    nimcp_ss_stats_t stats;
    nimcp_result_t result = nimcp_shadow_stack_get_stats(ss, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ShadowStackTest, SetMode) {
    nimcp_result_t result = nimcp_shadow_stack_set_mode(ss, NIMCP_SS_MODE_DETECT);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_shadow_stack_get_mode(ss), NIMCP_SS_MODE_DETECT);
}

//=============================================================================
// Capability Tests
//=============================================================================

class CapabilityTest : public ::testing::Test {
protected:
    nimcp_capability_system_t* caps;

    void SetUp() override {
        caps = nimcp_capability_system_create();
        ASSERT_NE(caps, nullptr);

        ASSERT_EQ(nimcp_capability_system_init(caps), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (caps) {
            nimcp_capability_system_destroy(caps);
            caps = nullptr;
        }
    }
};

TEST_F(CapabilityTest, CreateAndInit) {
    EXPECT_NE(caps, nullptr);
}

TEST_F(CapabilityTest, CreateCapability) {
    nimcp_capability_t cap;
    nimcp_result_t result = nimcp_capability_create(
        caps, NIMCP_RES_MEMORY, nullptr, NIMCP_PERM_READ | NIMCP_PERM_WRITE, &cap);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(nimcp_capability_is_null(cap));
}

TEST_F(CapabilityTest, CheckPermission) {
    nimcp_capability_t cap;
    nimcp_capability_create(caps, NIMCP_RES_GENERIC, nullptr,
                           NIMCP_PERM_READ | NIMCP_PERM_WRITE, &cap);

    EXPECT_TRUE(nimcp_capability_check(caps, cap, NIMCP_PERM_READ));
    EXPECT_TRUE(nimcp_capability_check(caps, cap, NIMCP_PERM_WRITE));
    EXPECT_FALSE(nimcp_capability_check(caps, cap, NIMCP_PERM_EXECUTE));
}

TEST_F(CapabilityTest, IsValid) {
    nimcp_capability_t cap;
    nimcp_capability_create(caps, NIMCP_RES_GENERIC, nullptr, NIMCP_PERM_READ, &cap);

    EXPECT_TRUE(nimcp_capability_is_valid(caps, cap));
}

TEST_F(CapabilityTest, Delegate) {
    nimcp_capability_t parent;
    nimcp_capability_create(caps, NIMCP_RES_GENERIC, nullptr,
                           NIMCP_PERM_READ | NIMCP_PERM_WRITE | NIMCP_PERM_DELEGATE, &parent);

    nimcp_capability_t child;
    nimcp_result_t result = nimcp_capability_delegate(
        caps, parent, NIMCP_PERM_READ, &child);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Child should only have READ
    EXPECT_TRUE(nimcp_capability_check(caps, child, NIMCP_PERM_READ));
    EXPECT_FALSE(nimcp_capability_check(caps, child, NIMCP_PERM_WRITE));
}

TEST_F(CapabilityTest, Revoke) {
    nimcp_capability_t cap;
    nimcp_capability_create(caps, NIMCP_RES_GENERIC, nullptr, NIMCP_PERM_READ, &cap);

    EXPECT_TRUE(nimcp_capability_is_valid(caps, cap));

    nimcp_result_t result = nimcp_capability_revoke(caps, cap);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_FALSE(nimcp_capability_is_valid(caps, cap));
}

TEST_F(CapabilityTest, RegisterHolder) {
    uint32_t holder_id;
    nimcp_result_t result = nimcp_capability_register_holder(caps, "test_module", &holder_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(holder_id, 0u);
}

TEST_F(CapabilityTest, GetPermissions) {
    nimcp_capability_t cap;
    nimcp_capability_create(caps, NIMCP_RES_GENERIC, nullptr,
                           NIMCP_PERM_READ | NIMCP_PERM_WRITE, &cap);

    uint32_t perms = nimcp_capability_get_permissions(caps, cap);
    EXPECT_EQ(perms, NIMCP_PERM_READ | NIMCP_PERM_WRITE);
}

TEST_F(CapabilityTest, Statistics) {
    nimcp_cap_stats_t stats;
    nimcp_result_t result = nimcp_capability_get_stats(caps, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Fractal Security Tests
//=============================================================================

class FractalSecurityTest : public ::testing::Test {
protected:
    nimcp_fractal_security_t* fsc;

    void SetUp() override {
        fsc = nimcp_fractal_security_create();
        ASSERT_NE(fsc, nullptr);

        nimcp_fsc_config_t config = nimcp_fractal_security_default_config();
        ASSERT_EQ(nimcp_fractal_security_init(fsc, &config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (fsc) {
            nimcp_fractal_security_destroy(fsc);
            fsc = nullptr;
        }
    }
};

TEST_F(FractalSecurityTest, CreateAndInit) {
    EXPECT_NE(fsc, nullptr);
}

TEST_F(FractalSecurityTest, ProtectData) {
    int test_data[50] = {1, 2, 3, 4, 5};

    nimcp_fsc_node_t* node = nullptr;
    nimcp_result_t result = nimcp_fractal_security_protect(
        fsc, test_data, sizeof(test_data), &node);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NE(node, nullptr);
}

TEST_F(FractalSecurityTest, VerifyData) {
    int test_data[50] = {1, 2, 3, 4, 5};

    nimcp_fractal_security_protect(fsc, test_data, sizeof(test_data), nullptr);

    nimcp_fsc_result_t result = nimcp_fractal_security_verify_data(fsc, test_data);
    EXPECT_EQ(result, NIMCP_FSC_INTACT);
}

TEST_F(FractalSecurityTest, DetectTampering) {
    int test_data[50] = {1, 2, 3, 4, 5};

    nimcp_fractal_security_protect(fsc, test_data, sizeof(test_data), nullptr);

    // Tamper with data
    test_data[25] = 999;

    // Should detect tampering
    nimcp_fsc_result_t result = nimcp_fractal_security_verify_data(fsc, test_data);
    EXPECT_EQ(result, NIMCP_FSC_HASH_MISMATCH);
}

TEST_F(FractalSecurityTest, UpdateHash) {
    int test_data[50] = {1, 2, 3, 4, 5};

    nimcp_fractal_security_protect(fsc, test_data, sizeof(test_data), nullptr);

    // Legitimately modify and update
    test_data[25] = 999;
    nimcp_result_t update_result = nimcp_fractal_security_update_hash(fsc, test_data);
    EXPECT_EQ(update_result, NIMCP_SUCCESS);

    // Should now pass verification
    nimcp_fsc_result_t verify_result = nimcp_fractal_security_verify_data(fsc, test_data);
    EXPECT_EQ(verify_result, NIMCP_FSC_INTACT);
}

TEST_F(FractalSecurityTest, ComputeDimension) {
    // Add some data to create tree structure
    int data1[10], data2[10], data3[10];
    nimcp_fractal_security_protect(fsc, data1, sizeof(data1), nullptr);
    nimcp_fractal_security_protect(fsc, data2, sizeof(data2), nullptr);
    nimcp_fractal_security_protect(fsc, data3, sizeof(data3), nullptr);

    float dimension;
    nimcp_result_t result = nimcp_fractal_security_compute_dimension(fsc, &dimension);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(dimension, 0.0f);
}

TEST_F(FractalSecurityTest, TrustLevel) {
    int test_data[50];
    nimcp_fsc_node_t* node = nullptr;

    nimcp_fractal_security_protect(fsc, test_data, sizeof(test_data), &node);

    // Initially verified
    float trust = nimcp_fractal_security_trust_level(fsc, node);
    EXPECT_GT(trust, 0.0f);
}

TEST_F(FractalSecurityTest, VerifyAll) {
    int data1[10], data2[10];
    nimcp_fractal_security_protect(fsc, data1, sizeof(data1), nullptr);
    nimcp_fractal_security_protect(fsc, data2, sizeof(data2), nullptr);

    nimcp_fsc_result_t result = nimcp_fractal_security_verify_all(fsc);
    EXPECT_EQ(result, NIMCP_FSC_INTACT);
}

TEST_F(FractalSecurityTest, GuardianSentinels) {
    int test_data[50];
    nimcp_fsc_node_t* node = nullptr;
    nimcp_fractal_security_protect(fsc, test_data, sizeof(test_data), &node);

    nimcp_result_t result = nimcp_fractal_security_place_guardian(fsc, node);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(FractalSecurityTest, Statistics) {
    nimcp_fsc_stats_t stats;
    nimcp_result_t result = nimcp_fractal_security_get_stats(fsc, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Audit Log Tests
//=============================================================================

class AuditLogTest : public ::testing::Test {
protected:
    nimcp_audit_log_t* audit;

    void SetUp() override {
        audit = nimcp_audit_create();
        ASSERT_NE(audit, nullptr);

        nimcp_audit_config_t config = nimcp_audit_default_config();
        ASSERT_EQ(nimcp_audit_init(audit, &config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (audit) {
            nimcp_audit_destroy(audit);
            audit = nullptr;
        }
    }
};

TEST_F(AuditLogTest, CreateAndInit) {
    EXPECT_NE(audit, nullptr);
}

TEST_F(AuditLogTest, LogEvent) {
    nimcp_result_t result = nimcp_audit_log(
        audit,
        NIMCP_AUDIT_CAT_SYSTEM,
        NIMCP_AUDIT_SEV_INFO,
        NIMCP_AUDIT_OUTCOME_SUCCESS,
        "test",
        "Test event message"
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(AuditLogTest, LogFormattedEvent) {
    nimcp_result_t result = nimcp_audit_logf(
        audit,
        NIMCP_AUDIT_CAT_ACCESS,
        NIMCP_AUDIT_SEV_WARNING,
        NIMCP_AUDIT_OUTCOME_DENIED,
        "access_test",
        "Access denied for user %d on resource %s",
        42, "protected_data"
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(AuditLogTest, LogAccessEvent) {
    nimcp_result_t result = nimcp_audit_log_access(
        audit, 100, 200, "read", true, "Permission granted");
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(AuditLogTest, LogThreatEvent) {
    nimcp_result_t result = nimcp_audit_log_threat(
        audit, NIMCP_THREAT_HIGH, "buffer_overflow", "Stack smash detected");
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(AuditLogTest, LogConfigChange) {
    nimcp_result_t result = nimcp_audit_log_config_change(
        audit, "security", "level", "medium", "high");
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(AuditLogTest, GetRecentEvents) {
    // Log some events
    for (int i = 0; i < 10; i++) {
        nimcp_audit_logf(audit, NIMCP_AUDIT_CAT_SYSTEM, NIMCP_AUDIT_SEV_INFO,
                        NIMCP_AUDIT_OUTCOME_SUCCESS, "test", "Event %d", i);
    }

    nimcp_audit_event_t* events = nullptr;
    uint32_t count = 0;
    nimcp_result_t result = nimcp_audit_get_recent(audit, 5, &events, &count);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(count, 5u);

    if (events) free(events);
}

TEST_F(AuditLogTest, VerifyChain) {
    // Log some events to build chain
    for (int i = 0; i < 5; i++) {
        nimcp_audit_logf(audit, NIMCP_AUDIT_CAT_SYSTEM, NIMCP_AUDIT_SEV_INFO,
                        NIMCP_AUDIT_OUTCOME_SUCCESS, "chain_test", "Event %d", i);
    }

    uint64_t first_broken;
    EXPECT_TRUE(nimcp_audit_verify_chain(audit, &first_broken));
}

TEST_F(AuditLogTest, GenerateReport) {
    // Log various events
    nimcp_audit_log_access(audit, 1, 10, "read", true, "ok");
    nimcp_audit_log_threat(audit, NIMCP_THREAT_LOW, "scan", "Port scan detected");
    nimcp_audit_log_config_change(audit, "security", "level", "medium", "high");

    char buffer[4096];
    int written = nimcp_audit_generate_report(audit, nullptr, buffer, sizeof(buffer));
    EXPECT_GT(written, 0);
}

TEST_F(AuditLogTest, SecuritySummary) {
    // Log some events
    nimcp_audit_log_threat(audit, NIMCP_THREAT_HIGH, "attack", "Attack detected");

    char buffer[1024];
    int written = nimcp_audit_security_summary(audit, buffer, sizeof(buffer));
    EXPECT_GT(written, 0);
}

TEST_F(AuditLogTest, ClearLog) {
    nimcp_audit_log(audit, NIMCP_AUDIT_CAT_SYSTEM, NIMCP_AUDIT_SEV_INFO,
                   NIMCP_AUDIT_OUTCOME_SUCCESS, "test", "message");

    nimcp_result_t result = nimcp_audit_clear(audit);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(AuditLogTest, Statistics) {
    nimcp_audit_stats_t stats;
    nimcp_result_t result = nimcp_audit_get_stats(audit, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Utility Name Tests
//=============================================================================

TEST(UtilityTest, AuditCategoryNames) {
    EXPECT_STREQ(nimcp_audit_category_name(NIMCP_AUDIT_CAT_ACCESS), "ACCESS");
    EXPECT_STREQ(nimcp_audit_category_name(NIMCP_AUDIT_CAT_THREAT), "THREAT");
    EXPECT_STREQ(nimcp_audit_category_name(NIMCP_AUDIT_CAT_SYSTEM), "SYSTEM");
}

TEST(UtilityTest, AuditSeverityNames) {
    EXPECT_STREQ(nimcp_audit_severity_name(NIMCP_AUDIT_SEV_INFO), "INFO");
    EXPECT_STREQ(nimcp_audit_severity_name(NIMCP_AUDIT_SEV_CRITICAL), "CRITICAL");
}

TEST(UtilityTest, FractalResultNames) {
    EXPECT_STREQ(nimcp_fsc_result_name(NIMCP_FSC_INTACT), "INTACT");
    EXPECT_STREQ(nimcp_fsc_result_name(NIMCP_FSC_HASH_MISMATCH), "HASH_MISMATCH");
}

TEST(UtilityTest, ShadowStackResultNames) {
    EXPECT_STREQ(nimcp_ss_result_name(NIMCP_SS_OK), "OK");
    EXPECT_STREQ(nimcp_ss_result_name(NIMCP_SS_MISMATCH), "Mismatch");
}

TEST(UtilityTest, CFIResultNames) {
    EXPECT_STREQ(nimcp_cfi_result_name(NIMCP_CFI_VALID), "Valid");
    EXPECT_STREQ(nimcp_cfi_result_name(NIMCP_CFI_INVALID_TARGET), "Invalid Target");
}

TEST(UtilityTest, MonitorStateNames) {
    EXPECT_STREQ(nimcp_monitor_state_name(NIMCP_MONITOR_STATE_RUNNING), "Running");
    EXPECT_STREQ(nimcp_monitor_state_name(NIMCP_MONITOR_STATE_STOPPED), "Stopped");
}

//=============================================================================
// Integration Tests
//=============================================================================

class SecurityIntegrationTest : public ::testing::Test {
protected:
    nimcp_security_coverage_t* coverage;
    nimcp_capability_system_t* caps;
    nimcp_audit_log_t* audit;

    void SetUp() override {
        coverage = nimcp_security_coverage_create();
        caps = nimcp_capability_system_create();
        audit = nimcp_audit_create();

        ASSERT_NE(coverage, nullptr);
        ASSERT_NE(caps, nullptr);
        ASSERT_NE(audit, nullptr);

        ASSERT_EQ(nimcp_security_coverage_init(coverage), NIMCP_SUCCESS);
        ASSERT_EQ(nimcp_capability_system_init(caps), NIMCP_SUCCESS);

        nimcp_audit_config_t audit_config = nimcp_audit_default_config();
        ASSERT_EQ(nimcp_audit_init(audit, &audit_config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (coverage) nimcp_security_coverage_destroy(coverage);
        if (caps) nimcp_capability_system_destroy(caps);
        if (audit) nimcp_audit_destroy(audit);
    }
};

TEST_F(SecurityIntegrationTest, CapabilityProtectedAccess) {
    // Create capability for memory access
    nimcp_capability_t cap;
    nimcp_capability_create(caps, NIMCP_RES_MEMORY, nullptr,
                           NIMCP_PERM_READ | NIMCP_PERM_WRITE, &cap);

    // Check permission and log
    if (nimcp_capability_check(caps, cap, NIMCP_PERM_READ)) {
        nimcp_audit_log_access(audit, 1, 100, "read", true, "Capability check passed");
    }

    // Verify audit has the event
    nimcp_audit_stats_t stats;
    nimcp_audit_get_stats(audit, &stats);
    EXPECT_GT(stats.total_events, 0u);
}

TEST_F(SecurityIntegrationTest, MemoryProtectionWithAudit) {
    int protected_data[100] = {0};

    // Register for coverage
    int32_t region_id = nimcp_coverage_register_region(coverage, protected_data,
        sizeof(protected_data), NIMCP_PROTECTION_HASH_VERIFIED, "protected_data");
    ASSERT_GE(region_id, 0);

    // Log registration
    nimcp_audit_log(audit, NIMCP_AUDIT_CAT_CONFIGURATION, NIMCP_AUDIT_SEV_INFO,
                   NIMCP_AUDIT_OUTCOME_SUCCESS, "memory_protection",
                   "Memory region registered for protection");

    // Verify
    EXPECT_TRUE(nimcp_coverage_verify_region(coverage, region_id));

    // Log verification result
    nimcp_audit_log(audit, NIMCP_AUDIT_CAT_INTEGRITY, NIMCP_AUDIT_SEV_INFO,
                   NIMCP_AUDIT_OUTCOME_SUCCESS, "memory_protection",
                   "Memory verification passed");
}

TEST_F(SecurityIntegrationTest, CFIWithShadowStack) {
    // Create CFI and shadow stack
    nimcp_cfi_context_t* cfi = nimcp_cfi_create();
    nimcp_shadow_stack_t* ss = nimcp_shadow_stack_create(0);

    ASSERT_NE(cfi, nullptr);
    ASSERT_NE(ss, nullptr);

    ASSERT_EQ(nimcp_cfi_init(cfi, NIMCP_CFI_MODE_ENFORCE), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_shadow_stack_init(ss, NIMCP_SS_MODE_ENFORCE), NIMCP_SUCCESS);

    // Register a function target
    nimcp_cfi_register_target(cfi, (void*)test_callback, 0x1001,
        NIMCP_FUNC_TYPE_CALLBACK, "test_callback");

    // Simulate function entry - push return address
    void* fake_return = (void*)0x12345678;
    EXPECT_EQ(nimcp_shadow_stack_push(ss, fake_return), NIMCP_SS_OK);

    // Validate the call target
    EXPECT_TRUE(nimcp_cfi_validate_ptr(cfi, (void*)test_callback, 0x1001));

    // Simulate function return - verify return address
    EXPECT_EQ(nimcp_shadow_stack_pop(ss, fake_return), NIMCP_SS_OK);

    // Log the successful CFI-protected call
    nimcp_audit_log(audit, NIMCP_AUDIT_CAT_ACCESS, NIMCP_AUDIT_SEV_INFO,
                   NIMCP_AUDIT_OUTCOME_SUCCESS, "cfi_call",
                   "CFI-protected function call completed successfully");

    nimcp_shadow_stack_destroy(ss);
    nimcp_cfi_destroy(cfi);
}

//=============================================================================
// Phase SC-2: Security Recovery Bridge Tests
//=============================================================================

// Include security recovery bridge header
// Headers have their own extern "C" guards
#include "security/nimcp_security_recovery_bridge.h"

class SecurityRecoveryBridgeTest : public ::testing::Test {
protected:
    nimcp_security_recovery_bridge_t* bridge;

    void SetUp() override {
        bridge = nimcp_srb_create();
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            nimcp_srb_destroy(bridge);
            bridge = nullptr;
        }
    }
};

// Test bridge creation
TEST_F(SecurityRecoveryBridgeTest, CreateBridge) {
    // Bridge was created in SetUp
    EXPECT_NE(bridge, nullptr);
}

// Test bridge initialization with default config
TEST_F(SecurityRecoveryBridgeTest, InitWithDefaults) {
    EXPECT_EQ(nimcp_srb_init(bridge, nullptr), NIMCP_SUCCESS);
}

// Test bridge initialization with custom config
TEST_F(SecurityRecoveryBridgeTest, InitWithConfig) {
    nimcp_srb_config_t config = {
        .mode = NIMCP_SRB_MODE_AUTO_REPAIR,
        .enable_auto_checkpoint = true,
        .checkpoint_interval_ms = 30000,
        .enable_fractal_verification = true,
        .verification_interval_ms = 5000,
        .cooldown_ms = 500,
        .max_repairs_per_minute = 30,
        .notify_brain = true,
        .log_to_audit = true
    };

    EXPECT_EQ(nimcp_srb_init(bridge, &config), NIMCP_SUCCESS);
}

// Test default config helper
TEST_F(SecurityRecoveryBridgeTest, DefaultConfig) {
    nimcp_srb_config_t config = nimcp_srb_default_config();

    // Default is AUTO_REPAIR mode with checkpoints enabled for safety
    EXPECT_EQ(config.mode, NIMCP_SRB_MODE_AUTO_REPAIR);
    EXPECT_TRUE(config.enable_auto_checkpoint);
    EXPECT_TRUE(config.enable_fractal_verification);
    EXPECT_TRUE(config.log_to_audit);
}

// Test security module connection - coverage
TEST_F(SecurityRecoveryBridgeTest, ConnectCoverage) {
    ASSERT_EQ(nimcp_srb_init(bridge, nullptr), NIMCP_SUCCESS);

    // Create coverage module
    nimcp_security_coverage_t* coverage = nimcp_security_coverage_create();
    ASSERT_NE(coverage, nullptr);
    ASSERT_EQ(nimcp_security_coverage_init(coverage), NIMCP_SUCCESS);

    // Connect coverage to bridge
    EXPECT_EQ(nimcp_srb_connect_coverage(bridge, coverage), NIMCP_SUCCESS);

    nimcp_security_coverage_destroy(coverage);
}

// Test security module connection - fractal
TEST_F(SecurityRecoveryBridgeTest, ConnectFractal) {
    ASSERT_EQ(nimcp_srb_init(bridge, nullptr), NIMCP_SUCCESS);

    // Create fractal security module with default config
    nimcp_fractal_security_t* fsc = nimcp_fractal_security_create();
    ASSERT_NE(fsc, nullptr);

    nimcp_fsc_config_t fsc_config = {
        .fractal_dimension = 1.8f,
        .hierarchy_depth = 8,
        .branching_factor = 4,
        .anomaly_threshold = 0.1f,
        .detect_mode = NIMCP_FSC_DETECT_STRUCTURAL,
        .enable_guardians = false,
        .guardian_interval = 0,
        .verification_interval_ms = 0
    };
    ASSERT_EQ(nimcp_fractal_security_init(fsc, &fsc_config), NIMCP_SUCCESS);

    // Connect fractal security to bridge
    EXPECT_EQ(nimcp_srb_connect_fractal(bridge, fsc), NIMCP_SUCCESS);

    nimcp_fractal_security_destroy(fsc);
}

// Test security module connection - audit
TEST_F(SecurityRecoveryBridgeTest, ConnectAudit) {
    ASSERT_EQ(nimcp_srb_init(bridge, nullptr), NIMCP_SUCCESS);

    // Create audit module
    nimcp_audit_log_t* audit = nimcp_audit_create();
    ASSERT_NE(audit, nullptr);

    nimcp_audit_config_t audit_config = {
        .destinations = 0,  // No output (memory only)
        .log_file_path = nullptr,
        .rotation_size = 0,
        .max_memory_entries = 1000,
        .min_severity = NIMCP_AUDIT_SEV_INFO,
        .enable_chain_verification = true,
        .enable_timestamps = true,
        .enable_stack_traces = false,
        .synchronous_write = false
    };
    ASSERT_EQ(nimcp_audit_init(audit, &audit_config), NIMCP_SUCCESS);

    // Connect audit to bridge
    EXPECT_EQ(nimcp_srb_connect_audit(bridge, audit), NIMCP_SUCCESS);

    nimcp_audit_destroy(audit);
}

// Test violation reporting
TEST_F(SecurityRecoveryBridgeTest, ReportViolation) {
    nimcp_srb_config_t config = {
        .mode = NIMCP_SRB_MODE_AUTO_REPAIR,
        .enable_auto_checkpoint = false,
        .checkpoint_interval_ms = 0,
        .enable_fractal_verification = false,
        .verification_interval_ms = 0,
        .cooldown_ms = 0,
        .max_repairs_per_minute = 100,
        .notify_brain = false,
        .log_to_audit = false
    };
    ASSERT_EQ(nimcp_srb_init(bridge, &config), NIMCP_SUCCESS);

    // Create a test violation
    nimcp_security_violation_t violation = {
        .type = NIMCP_SV_MEMORY_HASH_MISMATCH,
        .severity = NIMCP_SV_SEVERITY_MEDIUM,
        .timestamp = 12345678,
        .affected_address = (void*)0x1000,
        .affected_size = 256,
        .region_name = "test_region",
        .affected_brain = nullptr,
        .details = "Test violation"
    };

    nimcp_security_recovery_result_t result;
    EXPECT_EQ(nimcp_srb_report_violation(bridge, &violation, &result), NIMCP_SUCCESS);

    // The result should indicate some action was taken
    EXPECT_NE(result.action_taken, NIMCP_SRA_NONE);
}

// Test action determination
TEST_F(SecurityRecoveryBridgeTest, DetermineAction) {
    ASSERT_EQ(nimcp_srb_init(bridge, nullptr), NIMCP_SUCCESS);

    // Test different violation types and severities
    // Note: nimcp_srb_determine_action is a pure function that doesn't need bridge
    EXPECT_NE(nimcp_srb_determine_action(NIMCP_SV_MEMORY_HASH_MISMATCH,
        NIMCP_SV_SEVERITY_LOW), NIMCP_SRA_HALT);

    EXPECT_EQ(nimcp_srb_determine_action(NIMCP_SV_MEMORY_CORRUPTION,
        NIMCP_SV_SEVERITY_CRITICAL), NIMCP_SRA_RESTORE_CHECKPOINT);

    EXPECT_EQ(nimcp_srb_determine_action(NIMCP_SV_SHADOW_STACK_MISMATCH,
        NIMCP_SV_SEVERITY_CRITICAL), NIMCP_SRA_HALT);
}

// Test statistics tracking
TEST_F(SecurityRecoveryBridgeTest, Statistics) {
    ASSERT_EQ(nimcp_srb_init(bridge, nullptr), NIMCP_SUCCESS);

    nimcp_srb_stats_t stats;
    EXPECT_EQ(nimcp_srb_get_stats(bridge, &stats), NIMCP_SUCCESS);

    // Initially all stats should be zero
    EXPECT_EQ(stats.violations_detected, 0u);
    EXPECT_EQ(stats.recoveries_attempted, 0u);
    EXPECT_EQ(stats.recoveries_successful, 0u);
}

// Test verification cycle
TEST_F(SecurityRecoveryBridgeTest, VerificationCycle) {
    nimcp_srb_config_t config = {
        .mode = NIMCP_SRB_MODE_MONITOR,
        .enable_auto_checkpoint = false,
        .checkpoint_interval_ms = 0,
        .enable_fractal_verification = false,
        .verification_interval_ms = 0,
        .cooldown_ms = 0,
        .max_repairs_per_minute = 100,
        .notify_brain = false,
        .log_to_audit = false
    };
    ASSERT_EQ(nimcp_srb_init(bridge, &config), NIMCP_SUCCESS);

    // Run verification cycle (should succeed with no modules connected)
    EXPECT_EQ(nimcp_srb_run_verification_cycle(bridge), NIMCP_SUCCESS);
}

//=============================================================================
// Phase SC-3: Mathematical Security Tests
//=============================================================================

// Headers have their own extern "C" guards
#include "security/nimcp_security_math.h"

//-----------------------------------------------------------------------------
// Entropy Analyzer Tests
//-----------------------------------------------------------------------------

class EntropyAnalyzerTest : public ::testing::Test {
protected:
    nimcp_entropy_analyzer_t* analyzer;

    void SetUp() override {
        analyzer = nimcp_entropy_create();
        ASSERT_NE(analyzer, nullptr);
        EXPECT_EQ(nimcp_entropy_init(analyzer, nullptr), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (analyzer) {
            nimcp_entropy_destroy(analyzer);
            analyzer = nullptr;
        }
    }
};

TEST_F(EntropyAnalyzerTest, CalculateUniformEntropy) {
    // Uniform random data should have entropy near 8 bits/byte
    uint8_t uniform_data[1024];
    for (int i = 0; i < 1024; i++) {
        uniform_data[i] = i % 256;
    }

    double entropy = nimcp_entropy_calculate(uniform_data, sizeof(uniform_data));
    EXPECT_GE(entropy, 7.9);  // Near maximum for 256 symbols
    EXPECT_LE(entropy, 8.0);
}

TEST_F(EntropyAnalyzerTest, CalculateLowEntropy) {
    // Repetitive data should have low entropy
    uint8_t low_entropy[1024];
    memset(low_entropy, 'A', sizeof(low_entropy));

    double entropy = nimcp_entropy_calculate(low_entropy, sizeof(low_entropy));
    EXPECT_EQ(entropy, 0.0);  // Single symbol = 0 entropy
}

TEST_F(EntropyAnalyzerTest, AnalyzeData) {
    uint8_t test_data[512];
    for (int i = 0; i < 512; i++) {
        test_data[i] = i % 256;
    }

    nimcp_entropy_result_t result;
    EXPECT_EQ(nimcp_entropy_analyze(analyzer, test_data, sizeof(test_data), &result),
              NIMCP_SUCCESS);

    EXPECT_GT(result.entropy, 0.0);
    EXPECT_EQ(result.total_bytes, 512u);
}

TEST_F(EntropyAnalyzerTest, MutualInformation) {
    // Identical data should have high mutual information
    uint8_t data1[256];
    uint8_t data2[256];
    for (int i = 0; i < 256; i++) {
        data1[i] = i;
        data2[i] = i;  // Same as data1
    }

    double mi = nimcp_entropy_mutual_information(data1, data2, 256);
    EXPECT_GT(mi, 7.0);  // High mutual information for identical data
}

TEST_F(EntropyAnalyzerTest, BaselineTracking) {
    uint8_t baseline_data[256];
    for (int i = 0; i < 256; i++) baseline_data[i] = i;

    // Set baseline
    EXPECT_EQ(nimcp_entropy_set_baseline(analyzer, 1, baseline_data, 256), NIMCP_SUCCESS);

    // Check against baseline (should match)
    nimcp_entropy_result_t result;
    EXPECT_EQ(nimcp_entropy_check_baseline(analyzer, 1, baseline_data, 256, &result),
              NIMCP_SUCCESS);
    EXPECT_FALSE(result.is_anomaly);
}

//-----------------------------------------------------------------------------
// Bayesian Trust Tests
//-----------------------------------------------------------------------------

class BayesianTrustTest : public ::testing::Test {
protected:
    nimcp_trust_network_t* network;

    void SetUp() override {
        network = nimcp_trust_create();
        ASSERT_NE(network, nullptr);
        EXPECT_EQ(nimcp_trust_init(network, nullptr), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (network) {
            nimcp_trust_destroy(network);
            network = nullptr;
        }
    }
};

TEST_F(BayesianTrustTest, RegisterEntity) {
    EXPECT_EQ(nimcp_trust_register_entity(network, 1, "test_entity"), NIMCP_SUCCESS);

    nimcp_trust_score_t score;
    EXPECT_EQ(nimcp_trust_get_score(network, 1, &score), NIMCP_SUCCESS);

    // Initial trust should be based on prior (1,1) = 0.5
    EXPECT_NEAR(score.expected_trust, 0.5, 0.01);
}

TEST_F(BayesianTrustTest, BayesianUpdate) {
    ASSERT_EQ(nimcp_trust_register_entity(network, 1, "entity1"), NIMCP_SUCCESS);

    // Record successful interactions - trust should increase
    for (int i = 0; i < 9; i++) {
        EXPECT_EQ(nimcp_trust_record_interaction(network, 1, true, 1.0), NIMCP_SUCCESS);
    }

    nimcp_trust_score_t score;
    EXPECT_EQ(nimcp_trust_get_score(network, 1, &score), NIMCP_SUCCESS);

    // After 9 successes + prior (1,1): alpha=10, beta=1, trust = 10/11 ~ 0.91
    EXPECT_GT(score.expected_trust, 0.85);
    EXPECT_EQ(score.observations, 9u);
}

TEST_F(BayesianTrustTest, TrustDecreaseOnFailure) {
    ASSERT_EQ(nimcp_trust_register_entity(network, 1, "entity1"), NIMCP_SUCCESS);

    // Record failures - trust should decrease
    for (int i = 0; i < 9; i++) {
        EXPECT_EQ(nimcp_trust_record_interaction(network, 1, false, 1.0), NIMCP_SUCCESS);
    }

    nimcp_trust_score_t score;
    EXPECT_EQ(nimcp_trust_get_score(network, 1, &score), NIMCP_SUCCESS);

    // After 9 failures + prior (1,1): alpha=1, beta=10, trust = 1/11 ~ 0.09
    EXPECT_LT(score.expected_trust, 0.15);
}

TEST_F(BayesianTrustTest, VoucherPropagation) {
    // Register two entities
    ASSERT_EQ(nimcp_trust_register_entity(network, 1, "trusted_entity"), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_trust_register_entity(network, 2, "new_entity"), NIMCP_SUCCESS);

    // Make entity 1 very trusted
    for (int i = 0; i < 19; i++) {
        nimcp_trust_record_interaction(network, 1, true, 1.0);
    }

    // Entity 1 vouches for entity 2
    EXPECT_EQ(nimcp_trust_add_voucher(network, 1, 2), NIMCP_SUCCESS);

    // Propagate trust
    EXPECT_EQ(nimcp_trust_propagate(network), NIMCP_SUCCESS);

    // Entity 2 should have higher trust now due to voucher
    nimcp_trust_score_t score;
    EXPECT_EQ(nimcp_trust_get_score(network, 2, &score), NIMCP_SUCCESS);
    EXPECT_GT(score.expected_trust, 0.5);  // Higher than initial 0.5
}

TEST_F(BayesianTrustTest, ProbabilityAboveThreshold) {
    nimcp_trust_score_t score = {
        .alpha = 10.0,
        .beta = 2.0,
        .expected_trust = 10.0 / 12.0,
        .variance = 0.0,
        .confidence = 0.0,
        .observations = 10
    };

    // P(Trust >= 0.5) should be high for alpha=10, beta=2
    double prob = nimcp_trust_probability_above(&score, 0.5);
    EXPECT_GT(prob, 0.9);

    // P(Trust >= 0.95) should be low
    prob = nimcp_trust_probability_above(&score, 0.95);
    EXPECT_LT(prob, 0.3);
}

//-----------------------------------------------------------------------------
// Differential Privacy Tests
//-----------------------------------------------------------------------------

class DifferentialPrivacyTest : public ::testing::Test {
protected:
    nimcp_dp_context_t* dp;

    void SetUp() override {
        dp = nimcp_dp_create();
        ASSERT_NE(dp, nullptr);
        EXPECT_EQ(nimcp_dp_init(dp, nullptr), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (dp) {
            nimcp_dp_destroy(dp);
            dp = nullptr;
        }
    }
};

TEST_F(DifferentialPrivacyTest, LaplaceNoise) {
    nimcp_dp_result_t result;
    EXPECT_EQ(nimcp_dp_add_laplace_noise(dp, 100.0, 1.0, &result), NIMCP_SUCCESS);

    // Noisy value should be different from true value
    EXPECT_EQ(result.true_value, 100.0);
    // With epsilon=1.0, sensitivity=1.0, noise scale=1.0
    // Most noise values should be within a few scale units
    EXPECT_GT(result.accuracy_bound, 0.0);
}

TEST_F(DifferentialPrivacyTest, CountQuery) {
    nimcp_dp_result_t result;
    EXPECT_EQ(nimcp_dp_count(dp, 1000, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.true_value, 1000.0);
    // Count has sensitivity 1
    EXPECT_GT(result.epsilon_spent, 0.0);
}

TEST_F(DifferentialPrivacyTest, BudgetTracking) {
    double initial_budget = nimcp_dp_remaining_budget(dp);
    EXPECT_GT(initial_budget, 0.0);

    // Spend some budget
    nimcp_dp_result_t result;
    nimcp_dp_count(dp, 100, &result);

    double remaining = nimcp_dp_remaining_budget(dp);
    EXPECT_LT(remaining, initial_budget);
}

TEST_F(DifferentialPrivacyTest, BudgetReset) {
    // Spend some budget
    nimcp_dp_result_t result;
    nimcp_dp_count(dp, 100, &result);
    nimcp_dp_count(dp, 200, &result);

    // Reset
    EXPECT_EQ(nimcp_dp_reset_budget(dp), NIMCP_SUCCESS);

    double remaining = nimcp_dp_remaining_budget(dp);
    EXPECT_EQ(remaining, 10.0);  // Default total budget
}

TEST_F(DifferentialPrivacyTest, MeanQuery) {
    nimcp_dp_result_t result;
    EXPECT_EQ(nimcp_dp_mean(dp, 50.0, 100, 100.0, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.true_value, 50.0);
    // Mean sensitivity = max_value/n = 100/100 = 1
}

TEST_F(DifferentialPrivacyTest, HistogramPrivatization) {
    uint64_t histogram[10] = {100, 200, 300, 150, 50, 75, 125, 225, 175, 100};
    double noisy_histogram[10];

    EXPECT_EQ(nimcp_dp_histogram(dp, histogram, 10, noisy_histogram), NIMCP_SUCCESS);

    // Noisy values should be non-negative (we clamp at 0)
    for (int i = 0; i < 10; i++) {
        EXPECT_GE(noisy_histogram[i], 0.0);
    }
}

//-----------------------------------------------------------------------------
// Utility Function Tests
//-----------------------------------------------------------------------------

TEST(SecurityMathUtilTest, SafeLog2) {
    EXPECT_EQ(nimcp_safe_log2(0.0), 0.0);
    EXPECT_EQ(nimcp_safe_log2(-1.0), 0.0);
    EXPECT_NEAR(nimcp_safe_log2(1.0), 0.0, 0.001);
    EXPECT_NEAR(nimcp_safe_log2(2.0), 1.0, 0.001);
    EXPECT_NEAR(nimcp_safe_log2(256.0), 8.0, 0.001);
}

TEST(SecurityMathUtilTest, BetaIncomplete) {
    // I_0.5(1,1) = 0.5 (symmetric case)
    EXPECT_NEAR(nimcp_beta_incomplete(0.5, 1.0, 1.0), 0.5, 0.01);

    // I_0(a,b) = 0
    EXPECT_EQ(nimcp_beta_incomplete(0.0, 2.0, 3.0), 0.0);

    // I_1(a,b) = 1
    EXPECT_EQ(nimcp_beta_incomplete(1.0, 2.0, 3.0), 1.0);
}
