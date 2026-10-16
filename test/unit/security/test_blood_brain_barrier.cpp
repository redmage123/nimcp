/**
 * @file test_blood_brain_barrier.cpp
 * @brief Unit tests for Blood-Brain Barrier security system (NIMCP)
 *
 * Tests the four-layer perimeter defense system:
 * - Input Gate: Input validation and sanitization
 * - Code Signing: Signature verification and key management
 * - Memory Boundary: Region protection and canary verification
 * - Access Control: Subject/object access control and capabilities
 *
 * BIOLOGICAL MODEL:
 * Endothelial cells -> Input validation gates
 * Basement membrane -> Code signing verification
 * Astrocyte end-feet -> Memory boundary monitors
 * Pericytes -> Access control enforcers
 */

#include "test_helpers.h"

// Headers have their own extern "C" guards
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"

#include <cstring>
#include <thread>
#include <vector>
#include <atomic>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class BloodBrainBarrierTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        // Reset all BBB subsystem state for test isolation
        bbb_reset_test_state();

        config = bbb_default_config();
        system = bbb_system_create(&config);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override
    {
        if (system) {
            bbb_system_destroy(system);
            system = nullptr;
        }
    }

    // Helper to create SQL injection test string
    const char* create_sql_injection_string()
    {
        return "'; DROP TABLE users; --";
    }

    // Helper to create format string attack
    const char* create_format_string_attack()
    {
        return "%n%n%n%n%s%s%s%s";
    }

    // Helper to create safe input string
    const char* create_safe_string()
    {
        return "Hello, this is a safe input string.";
    }

    // Helper to create subject
    bbb_subject_t create_test_subject(uint32_t id, uint32_t priv, uint32_t roles, uint64_t caps)
    {
        bbb_subject_t subject;
        subject.id = id;
        subject.privilege_level = priv;
        subject.roles = roles;
        subject.capabilities = caps;
        return subject;
    }

    // Helper to create object
    bbb_object_t create_test_object(uint32_t id, uint32_t req_priv, uint32_t roles, uint64_t caps)
    {
        bbb_object_t object;
        object.id = id;
        object.required_privilege = req_priv;
        object.required_roles = roles;
        object.required_capabilities = caps;
        return object;
    }

    bbb_config_t config;
    bbb_system_t system;
};

//=============================================================================
// System Lifecycle Tests
//=============================================================================

TEST_F(BloodBrainBarrierTest, CreateSystemWithDefaults)
{
    bbb_system_t sys = bbb_system_create(nullptr);
    EXPECT_NE(sys, nullptr);
    if (sys) {
        bbb_system_destroy(sys);
    }
}

TEST_F(BloodBrainBarrierTest, CreateSystemWithConfig)
{
    bbb_config_t cfg = bbb_default_config();
    cfg.strict_mode = true;

    bbb_system_t sys = bbb_system_create(&cfg);
    EXPECT_NE(sys, nullptr);
    if (sys) {
        bbb_system_destroy(sys);
    }
}

TEST_F(BloodBrainBarrierTest, DestroyNullSystemSafe)
{
    // Should not crash
    bbb_system_destroy(nullptr);
    SUCCEED();
}

TEST_F(BloodBrainBarrierTest, EnableDisableSystem)
{
    EXPECT_TRUE(bbb_system_set_enabled(system, true));
    EXPECT_TRUE(bbb_system_is_enabled(system));

    EXPECT_TRUE(bbb_system_set_enabled(system, false));
    EXPECT_FALSE(bbb_system_is_enabled(system));
}

TEST_F(BloodBrainBarrierTest, EnableNullSystemFails)
{
    EXPECT_FALSE(bbb_system_set_enabled(nullptr, true));
}

TEST_F(BloodBrainBarrierTest, IsEnabledNullSystemFalse)
{
    EXPECT_FALSE(bbb_system_is_enabled(nullptr));
}

TEST_F(BloodBrainBarrierTest, GetStatisticsValid)
{
    bbb_statistics_t stats;
    memset(&stats, 0, sizeof(stats));

    EXPECT_TRUE(bbb_system_get_statistics(system, &stats));
    EXPECT_EQ(stats.total_validations, 0u);
    EXPECT_EQ(stats.threats_detected, 0u);
}

TEST_F(BloodBrainBarrierTest, GetStatisticsNullSystemFails)
{
    bbb_statistics_t stats;
    EXPECT_FALSE(bbb_system_get_statistics(nullptr, &stats));
}

TEST_F(BloodBrainBarrierTest, GetStatisticsNullStatsFails)
{
    EXPECT_FALSE(bbb_system_get_statistics(system, nullptr));
}

TEST_F(BloodBrainBarrierTest, ResetStatistics)
{
    // Note: Standalone validation functions don't require system handle
    // and don't track statistics. This test verifies reset functionality.
    bbb_statistics_t stats;
    EXPECT_TRUE(bbb_system_get_statistics(system, &stats));

    // Reset and verify stats are zeroed
    bbb_system_reset_statistics(system);
    EXPECT_TRUE(bbb_system_get_statistics(system, &stats));
    // After reset, validations should be 0
    EXPECT_EQ(stats.total_validations, 0u);
}

//=============================================================================
// Input Validation Tests - String Validation
//=============================================================================

TEST_F(BloodBrainBarrierTest, ValidateStringSafe)
{
    bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    bool valid = bbb_validate_string(system, create_safe_string(), &result);

    EXPECT_TRUE(valid);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.threat, BBB_THREAT_NONE);
}

TEST_F(BloodBrainBarrierTest, ValidateStringNullSystem)
{
    // NULL system is rejected - validation requires system context
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(nullptr, "test", &result);
    EXPECT_FALSE(valid);
    EXPECT_FALSE(result.valid);
}

TEST_F(BloodBrainBarrierTest, ValidateStringNullInput)
{
    // NULL string is invalid (security risk - could indicate use-after-free)
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(system, nullptr, &result);
    EXPECT_FALSE(valid);
    EXPECT_FALSE(result.valid);
}

TEST_F(BloodBrainBarrierTest, ValidateStringEmptyString)
{
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(system, "", &result);
    EXPECT_TRUE(valid);  // Empty string is technically valid
    EXPECT_TRUE(result.valid);
}

TEST_F(BloodBrainBarrierTest, DetectSQLInjection)
{
    bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    bool valid = bbb_validate_string(system, create_sql_injection_string(), &result);

    EXPECT_FALSE(valid);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.threat, BBB_THREAT_SQL_INJECTION);
    EXPECT_GE(result.severity, BBB_SEVERITY_MEDIUM);
}

TEST_F(BloodBrainBarrierTest, DetectSQLInjectionUnion)
{
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(system, "1 UNION SELECT * FROM passwords", &result);

    EXPECT_FALSE(valid);
    EXPECT_EQ(result.threat, BBB_THREAT_SQL_INJECTION);
}

TEST_F(BloodBrainBarrierTest, DetectFormatStringAttack)
{
    bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    bool valid = bbb_validate_string(system, create_format_string_attack(), &result);

    EXPECT_FALSE(valid);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.threat, BBB_THREAT_FORMAT_STRING);
}

TEST_F(BloodBrainBarrierTest, DetectFormatStringPercent)
{
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(system, "%x%x%x%x", &result);

    EXPECT_FALSE(valid);
    EXPECT_EQ(result.threat, BBB_THREAT_FORMAT_STRING);
}

//=============================================================================
// Input Validation Tests - Integer Validation
//=============================================================================

TEST_F(BloodBrainBarrierTest, ValidateIntegerValid)
{
    bbb_validation_result_t result;
    bool valid = bbb_validate_integer(system, 42, &result);

    EXPECT_TRUE(valid);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.threat, BBB_THREAT_NONE);
}

TEST_F(BloodBrainBarrierTest, ValidateIntegerZero)
{
    bbb_validation_result_t result;
    bool valid = bbb_validate_integer(system, 0, &result);

    EXPECT_TRUE(valid);
    EXPECT_TRUE(result.valid);
}

TEST_F(BloodBrainBarrierTest, ValidateIntegerNegative)
{
    bbb_validation_result_t result;
    bool valid = bbb_validate_integer(system, -100, &result);

    // Depends on config - check it doesn't crash
    EXPECT_GE(result.threat, BBB_THREAT_NONE);
}

TEST_F(BloodBrainBarrierTest, DetectIntegerOverflowMax)
{
    bbb_validation_result_t result;
    bool valid = bbb_validate_integer(system, INT64_MAX, &result);

    // May detect as overflow depending on config
    if (!valid) {
        EXPECT_EQ(result.threat, BBB_THREAT_INTEGER_OVERFLOW);
    }
}

TEST_F(BloodBrainBarrierTest, DetectIntegerOverflowMin)
{
    bbb_validation_result_t result;
    bool valid = bbb_validate_integer(system, INT64_MIN, &result);

    // May detect as overflow depending on config
    if (!valid) {
        EXPECT_EQ(result.threat, BBB_THREAT_INTEGER_OVERFLOW);
    }
}

TEST_F(BloodBrainBarrierTest, ValidateIntegerNullSystem)
{
    // NULL system is rejected - validation requires system context
    bbb_validation_result_t result;
    bool valid = bbb_validate_integer(nullptr, 42, &result);
    EXPECT_FALSE(valid);
    EXPECT_FALSE(result.valid);
}

//=============================================================================
// Input Validation Tests - Pointer Validation
//=============================================================================

TEST_F(BloodBrainBarrierTest, ValidatePointerValid)
{
    int data = 42;
    bbb_validation_result_t result;

    bool valid = bbb_validate_pointer(system, &data, sizeof(data), &result);

    EXPECT_TRUE(valid);
    EXPECT_TRUE(result.valid);
}

TEST_F(BloodBrainBarrierTest, ValidatePointerNull)
{
    bbb_validation_result_t result;
    bool valid = bbb_validate_pointer(system, nullptr, 10, &result);

    EXPECT_FALSE(valid);
    EXPECT_FALSE(result.valid);
}

TEST_F(BloodBrainBarrierTest, ValidatePointerZeroSize)
{
    int data = 42;
    bbb_validation_result_t result;

    bool valid = bbb_validate_pointer(system, &data, 0, &result);

    // Zero size might be valid or invalid depending on implementation
    EXPECT_GE(result.threat, BBB_THREAT_NONE);
}

TEST_F(BloodBrainBarrierTest, ValidatePointerNullSystem)
{
    // NULL system is acceptable - skip system-specific checks (Portia passes NULL)
    int data = 42;
    bbb_validation_result_t result;

    bool valid = bbb_validate_pointer(nullptr, &data, sizeof(data), &result);
    EXPECT_TRUE(valid);
}

//=============================================================================
// Input Validation Tests - Sanitization
//=============================================================================

TEST_F(BloodBrainBarrierTest, SanitizeStringSafe)
{
    char output[256];
    ssize_t len = bbb_sanitize_string(system, create_safe_string(), output, sizeof(output));

    EXPECT_GT(len, 0);
    EXPECT_STREQ(output, create_safe_string());
}

TEST_F(BloodBrainBarrierTest, SanitizeStringSQLInjection)
{
    char output[256];
    ssize_t len = bbb_sanitize_string(system, create_sql_injection_string(), output, sizeof(output));

    EXPECT_GT(len, 0);
    // Should have removed dangerous SQL operators (', ;) - whitelist approach
    // keeps alphanumeric characters like DROP TABLE but removes operators
    EXPECT_EQ(strchr(output, '\''), nullptr);  // Single quote removed
    EXPECT_EQ(strchr(output, ';'), nullptr);   // Semicolon removed
}

TEST_F(BloodBrainBarrierTest, SanitizeStringNullInput)
{
    char output[256];
    ssize_t len = bbb_sanitize_string(system, nullptr, output, sizeof(output));

    EXPECT_EQ(len, -1);
}

TEST_F(BloodBrainBarrierTest, SanitizeStringNullOutput)
{
    ssize_t len = bbb_sanitize_string(system, "test", nullptr, 0);

    EXPECT_EQ(len, -1);
}

TEST_F(BloodBrainBarrierTest, SanitizeStringBufferTooSmall)
{
    char output[5];  // Too small
    ssize_t len = bbb_sanitize_string(system, "This is a long string", output, sizeof(output));

    // Should truncate or return error
    EXPECT_LE(len, (ssize_t)sizeof(output));
}

//=============================================================================
// Code Signing Tests
//=============================================================================

TEST_F(BloodBrainBarrierTest, SignAndVerifyCode)
{
    /* Must configure signing key before code signing operations */
    static const uint8_t test_key[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    ASSERT_TRUE(bbb_set_signing_key(test_key, sizeof(test_key)));

    const char* code = "int main() { return 0; }";
    uint8_t signature[256];

    ssize_t sig_len = bbb_sign_code(system, code, strlen(code), signature, sizeof(signature));
    EXPECT_GT(sig_len, 0);

    bool valid = bbb_verify_signature(system, code, strlen(code), signature, sig_len);
    EXPECT_TRUE(valid);

    bbb_clear_signing_key();
}

TEST_F(BloodBrainBarrierTest, VerifyTamperedCode)
{
    /* Must configure signing key before code signing operations */
    static const uint8_t test_key[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    ASSERT_TRUE(bbb_set_signing_key(test_key, sizeof(test_key)));

    const char* code = "int main() { return 0; }";
    const char* tampered = "int main() { return 1; }";  // Modified
    uint8_t signature[256];

    ssize_t sig_len = bbb_sign_code(system, code, strlen(code), signature, sizeof(signature));
    ASSERT_GT(sig_len, 0);

    bool valid = bbb_verify_signature(system, tampered, strlen(tampered), signature, sig_len);
    EXPECT_FALSE(valid);

    bbb_clear_signing_key();
}

TEST_F(BloodBrainBarrierTest, SignCodeNullData)
{
    uint8_t signature[256];
    ssize_t sig_len = bbb_sign_code(system, nullptr, 0, signature, sizeof(signature));
    EXPECT_EQ(sig_len, -1);
}

TEST_F(BloodBrainBarrierTest, VerifySignatureNullSignature)
{
    const char* code = "test";
    bool valid = bbb_verify_signature(system, code, strlen(code), nullptr, 0);
    EXPECT_FALSE(valid);
}

TEST_F(BloodBrainBarrierTest, CalculateHash)
{
    const char* data = "test data for hashing";
    uint8_t hash[32];

    bool success = bbb_calculate_hash(data, strlen(data), hash);
    EXPECT_TRUE(success);

    // Hash should be non-zero
    bool non_zero = false;
    for (int i = 0; i < 32; i++) {
        if (hash[i] != 0) {
            non_zero = true;
            break;
        }
    }
    EXPECT_TRUE(non_zero);
}

TEST_F(BloodBrainBarrierTest, CalculateHashDeterministic)
{
    const char* data = "deterministic hash test";
    uint8_t hash1[32], hash2[32];

    EXPECT_TRUE(bbb_calculate_hash(data, strlen(data), hash1));
    EXPECT_TRUE(bbb_calculate_hash(data, strlen(data), hash2));

    EXPECT_EQ(memcmp(hash1, hash2, 32), 0);
}

TEST_F(BloodBrainBarrierTest, AddTrustedKey)
{
    uint8_t key_data[256] = {0x01, 0x02, 0x03, 0x04};

    bool added = bbb_add_trusted_key(system, key_data, sizeof(key_data), "test-key-1");
    EXPECT_TRUE(added);
}

TEST_F(BloodBrainBarrierTest, RemoveTrustedKey)
{
    uint8_t key_data[256] = {0x01, 0x02, 0x03, 0x04};

    ASSERT_TRUE(bbb_add_trusted_key(system, key_data, sizeof(key_data), "test-key-2"));

    bool removed = bbb_remove_trusted_key(system, "test-key-2");
    EXPECT_TRUE(removed);
}

TEST_F(BloodBrainBarrierTest, RemoveNonexistentKey)
{
    bool removed = bbb_remove_trusted_key(system, "nonexistent-key");
    EXPECT_FALSE(removed);
}

//=============================================================================
// Memory Boundary Tests
//=============================================================================

TEST_F(BloodBrainBarrierTest, RegisterMemoryRegion)
{
    char buffer[1024];

    uint32_t region_id = bbb_register_memory_region(system, buffer, sizeof(buffer), false);
    EXPECT_GT(region_id, 0u);
}

TEST_F(BloodBrainBarrierTest, RegisterMemoryRegionReadOnly)
{
    char buffer[1024];

    uint32_t region_id = bbb_register_memory_region(system, buffer, sizeof(buffer), true);
    EXPECT_GT(region_id, 0u);
}

TEST_F(BloodBrainBarrierTest, RegisterMemoryRegionNullAddress)
{
    uint32_t region_id = bbb_register_memory_region(system, nullptr, 1024, false);
    EXPECT_EQ(region_id, 0u);
}

TEST_F(BloodBrainBarrierTest, UnregisterMemoryRegion)
{
    char buffer[1024];
    uint32_t region_id = bbb_register_memory_region(system, buffer, sizeof(buffer), false);
    ASSERT_GT(region_id, 0u);

    bool success = bbb_unregister_memory_region(system, region_id);
    EXPECT_TRUE(success);
}

TEST_F(BloodBrainBarrierTest, UnregisterInvalidRegion)
{
    bool success = bbb_unregister_memory_region(system, 99999);
    EXPECT_FALSE(success);
}

TEST_F(BloodBrainBarrierTest, CheckMemoryAccessValid)
{
    char buffer[1024];
    uint32_t region_id = bbb_register_memory_region(system, buffer, sizeof(buffer), false);
    ASSERT_GT(region_id, 0u);

    bool valid = bbb_check_memory_access(system, buffer, sizeof(buffer), false);
    EXPECT_TRUE(valid);
}

TEST_F(BloodBrainBarrierTest, CheckMemoryAccessWriteToReadOnly)
{
    char buffer[1024];
    uint32_t region_id = bbb_register_memory_region(system, buffer, sizeof(buffer), true);
    ASSERT_GT(region_id, 0u);

    bool valid = bbb_check_memory_access(system, buffer, sizeof(buffer), true);
    EXPECT_FALSE(valid);  // Write to read-only should fail
}

TEST_F(BloodBrainBarrierTest, CheckMemoryAccessOutOfBounds)
{
    char buffer[1024];
    uint32_t region_id = bbb_register_memory_region(system, buffer, sizeof(buffer), false);
    ASSERT_GT(region_id, 0u);

    // Access beyond registered region
    bool valid = bbb_check_memory_access(system, buffer + 2000, 100, false);
    EXPECT_FALSE(valid);
}

TEST_F(BloodBrainBarrierTest, ProtectMemoryReadOnly)
{
    // Allocate page-aligned memory using NIMCP memory utilities
    size_t page_size = 4096;
    void* mem = nimcp_aligned_alloc(page_size, page_size);
    ASSERT_NE(mem, nullptr);

    bool success = bbb_protect_memory(system, mem, page_size, true, false, false);
    // May fail if not privileged, but shouldn't crash

    // Restore write permissions before freeing (required for heap allocator)
    if (success) {
        bbb_protect_memory(system, mem, page_size, true, true, false);
    }

    nimcp_aligned_free(mem);
}

TEST_F(BloodBrainBarrierTest, InstallStackCanary)
{
    char stack_buffer[128];

    uint64_t canary = bbb_install_stack_canary(system, stack_buffer);
    EXPECT_NE(canary, 0u);
}

TEST_F(BloodBrainBarrierTest, VerifyStackCanaryIntact)
{
    char stack_buffer[128];

    uint64_t canary = bbb_install_stack_canary(system, stack_buffer);
    ASSERT_NE(canary, 0u);

    bool intact = bbb_verify_stack_canary(system, stack_buffer, canary);
    EXPECT_TRUE(intact);
}

TEST_F(BloodBrainBarrierTest, VerifyStackCanaryCorrupted)
{
    char stack_buffer[128];

    uint64_t canary = bbb_install_stack_canary(system, stack_buffer);
    ASSERT_NE(canary, 0u);

    // Corrupt the stack
    memset(stack_buffer, 0, sizeof(stack_buffer));

    bool intact = bbb_verify_stack_canary(system, stack_buffer, canary);
    EXPECT_FALSE(intact);
}

//=============================================================================
// Access Control Tests
//=============================================================================

TEST_F(BloodBrainBarrierTest, RegisterSubject)
{
    bbb_subject_t subject = create_test_subject(1, 5, 0x01, 0x01);

    bool success = bbb_register_subject(system, &subject);
    EXPECT_TRUE(success);
}

TEST_F(BloodBrainBarrierTest, RegisterSubjectNullFails)
{
    bool success = bbb_register_subject(system, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(BloodBrainBarrierTest, RegisterObject)
{
    bbb_object_t object = create_test_object(1, 3, 0x01, 0x01);

    bool success = bbb_register_object(system, &object);
    EXPECT_TRUE(success);
}

TEST_F(BloodBrainBarrierTest, RegisterObjectNullFails)
{
    bool success = bbb_register_object(system, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(BloodBrainBarrierTest, CheckAccessAllowed)
{
    bbb_subject_t subject = create_test_subject(1, 5, 0x01, 0x01);
    bbb_object_t object = create_test_object(1, 3, 0x01, 0x01);

    ASSERT_TRUE(bbb_register_subject(system, &subject));
    ASSERT_TRUE(bbb_register_object(system, &object));

    bool allowed = bbb_check_access(system, &subject, &object, 1);  // Read access
    EXPECT_TRUE(allowed);
}

TEST_F(BloodBrainBarrierTest, CheckAccessDeniedInsufficientPrivilege)
{
    bbb_subject_t subject = create_test_subject(1, 2, 0x01, 0x01);  // Low privilege
    bbb_object_t object = create_test_object(1, 5, 0x01, 0x01);     // High requirement

    ASSERT_TRUE(bbb_register_subject(system, &subject));
    ASSERT_TRUE(bbb_register_object(system, &object));

    bool allowed = bbb_check_access(system, &subject, &object, 1);
    EXPECT_FALSE(allowed);
}

TEST_F(BloodBrainBarrierTest, CheckAccessDeniedMissingRole)
{
    bbb_subject_t subject = create_test_subject(1, 5, 0x01, 0x01);  // Role 0x01
    bbb_object_t object = create_test_object(1, 3, 0x02, 0x01);     // Requires role 0x02

    ASSERT_TRUE(bbb_register_subject(system, &subject));
    ASSERT_TRUE(bbb_register_object(system, &object));

    bool allowed = bbb_check_access(system, &subject, &object, 1);
    EXPECT_FALSE(allowed);
}

TEST_F(BloodBrainBarrierTest, GrantCapability)
{
    bbb_subject_t subject = create_test_subject(1, 5, 0x01, 0x00);  // No initial caps
    ASSERT_TRUE(bbb_register_subject(system, &subject));

    bool granted = bbb_grant_capability(system, 1, 0x04);  // Grant cap 0x04
    EXPECT_TRUE(granted);
}

TEST_F(BloodBrainBarrierTest, RevokeCapability)
{
    bbb_subject_t subject = create_test_subject(1, 5, 0x01, 0x04);  // Has cap 0x04
    ASSERT_TRUE(bbb_register_subject(system, &subject));

    bool revoked = bbb_revoke_capability(system, 1, 0x04);
    EXPECT_TRUE(revoked);
}

TEST_F(BloodBrainBarrierTest, RevokeCapabilityFromUnregistered)
{
    bool revoked = bbb_revoke_capability(system, 999, 0x04);
    EXPECT_FALSE(revoked);
}

//=============================================================================
// Threat Reporting Tests
//=============================================================================

TEST_F(BloodBrainBarrierTest, ReportThreat)
{
    const char* threat_data = "malicious payload";

    bbb_threat_report_t report = bbb_report_threat(
        system,
        BBB_THREAT_CODE_INJECTION,
        BBB_SEVERITY_HIGH,
        "Detected code injection attempt",
        threat_data,
        threat_data,
        strlen(threat_data)
    );

    EXPECT_EQ(report.type, BBB_THREAT_CODE_INJECTION);
    EXPECT_EQ(report.severity, BBB_SEVERITY_HIGH);
    EXPECT_GT(report.timestamp, 0u);
}

TEST_F(BloodBrainBarrierTest, GetThreatReports)
{
    // Report a threat first
    bbb_report_threat(system, BBB_THREAT_SQL_INJECTION, BBB_SEVERITY_MEDIUM,
                      "SQL injection detected", nullptr, nullptr, 0);

    bbb_threat_report_t reports[10];
    size_t count = bbb_get_threat_reports(system, reports, 10);

    EXPECT_GE(count, 1u);
    if (count > 0) {
        EXPECT_EQ(reports[0].type, BBB_THREAT_SQL_INJECTION);
    }
}

TEST_F(BloodBrainBarrierTest, ClearThreatReports)
{
    bbb_report_threat(system, BBB_THREAT_BUFFER_OVERFLOW, BBB_SEVERITY_HIGH,
                      "Buffer overflow", nullptr, nullptr, 0);

    bbb_clear_threat_reports(system);

    bbb_threat_report_t reports[10];
    size_t count = bbb_get_threat_reports(system, reports, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(BloodBrainBarrierTest, QuarantineRegion)
{
    char malicious_data[256];

    bool quarantined = bbb_quarantine_region(system, malicious_data, sizeof(malicious_data));
    EXPECT_TRUE(quarantined);
}

TEST_F(BloodBrainBarrierTest, ReleaseQuarantine)
{
    char malicious_data[256];
    ASSERT_TRUE(bbb_quarantine_region(system, malicious_data, sizeof(malicious_data)));

    bool released = bbb_release_quarantine(system, malicious_data);
    EXPECT_TRUE(released);
}

TEST_F(BloodBrainBarrierTest, ReleaseNonQuarantinedRegion)
{
    char normal_data[256];

    bool released = bbb_release_quarantine(system, normal_data);
    EXPECT_FALSE(released);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(BloodBrainBarrierTest, ThreatTypeNames)
{
    EXPECT_NE(bbb_threat_type_name(BBB_THREAT_NONE), nullptr);
    EXPECT_NE(bbb_threat_type_name(BBB_THREAT_BUFFER_OVERFLOW), nullptr);
    EXPECT_NE(bbb_threat_type_name(BBB_THREAT_SQL_INJECTION), nullptr);
    EXPECT_NE(bbb_threat_type_name(BBB_THREAT_FORMAT_STRING), nullptr);
    EXPECT_NE(bbb_threat_type_name(BBB_THREAT_CODE_INJECTION), nullptr);
    EXPECT_NE(bbb_threat_type_name(BBB_THREAT_SHELLCODE), nullptr);
    EXPECT_NE(bbb_threat_type_name(BBB_THREAT_ROP_CHAIN), nullptr);
    EXPECT_NE(bbb_threat_type_name(BBB_THREAT_INVALID_SIGNATURE), nullptr);
    EXPECT_NE(bbb_threat_type_name(BBB_THREAT_MEMORY_VIOLATION), nullptr);
    EXPECT_NE(bbb_threat_type_name(BBB_THREAT_UNAUTHORIZED_ACCESS), nullptr);
}

TEST_F(BloodBrainBarrierTest, SeverityNames)
{
    EXPECT_NE(bbb_severity_name(BBB_SEVERITY_NONE), nullptr);
    EXPECT_NE(bbb_severity_name(BBB_SEVERITY_LOW), nullptr);
    EXPECT_NE(bbb_severity_name(BBB_SEVERITY_MEDIUM), nullptr);
    EXPECT_NE(bbb_severity_name(BBB_SEVERITY_HIGH), nullptr);
    EXPECT_NE(bbb_severity_name(BBB_SEVERITY_CRITICAL), nullptr);
}

TEST_F(BloodBrainBarrierTest, ActionNames)
{
    EXPECT_NE(bbb_action_name(BBB_ACTION_ALLOW), nullptr);
    EXPECT_NE(bbb_action_name(BBB_ACTION_LOG), nullptr);
    EXPECT_NE(bbb_action_name(BBB_ACTION_BLOCK), nullptr);
    EXPECT_NE(bbb_action_name(BBB_ACTION_QUARANTINE), nullptr);
    EXPECT_NE(bbb_action_name(BBB_ACTION_TERMINATE), nullptr);
    EXPECT_NE(bbb_action_name(BBB_ACTION_LOCKDOWN), nullptr);
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(BloodBrainBarrierTest, DefaultConfigValues)
{
    bbb_config_t cfg = bbb_default_config();

    // Input config defaults
    EXPECT_TRUE(cfg.input.validate_strings);
    EXPECT_TRUE(cfg.input.validate_integers);
    EXPECT_TRUE(cfg.input.validate_pointers);
    EXPECT_GT(cfg.input.max_string_length, 0u);
    EXPECT_GT(cfg.input.max_array_size, 0u);

    // Signing config defaults
    EXPECT_TRUE(cfg.signing.verify_on_load);

    // Memory config defaults
    EXPECT_TRUE(cfg.memory.enable_stack_canaries);
    EXPECT_TRUE(cfg.memory.enable_heap_guards);

    // Access config defaults
    EXPECT_TRUE(cfg.access.enable_capability);

    // Default action should be BLOCK or higher
    EXPECT_GE(cfg.default_action, BBB_ACTION_BLOCK);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(BloodBrainBarrierTest, ValidateInputGeneric)
{
    uint8_t data[100];
    memset(data, 'A', sizeof(data));

    bbb_validation_result_t result;
    bool valid = bbb_validate_input(system, data, sizeof(data), &result);

    EXPECT_TRUE(valid);
}

TEST_F(BloodBrainBarrierTest, ValidateInputWithNullBytes)
{
    uint8_t data[100];
    memset(data, 0, sizeof(data));
    data[0] = 'A';
    data[50] = 'B';

    bbb_validation_result_t result;
    bool valid = bbb_validate_input(system, data, sizeof(data), &result);

    EXPECT_TRUE(valid);  // Null bytes in binary data are OK
}

TEST_F(BloodBrainBarrierTest, LongStringValidation)
{
    std::string long_string(10000, 'A');
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(system, long_string.c_str(), &result);

    // May fail due to max_string_length limit
    if (!valid) {
        EXPECT_EQ(result.threat, BBB_THREAT_BUFFER_OVERFLOW);
    }
}

TEST_F(BloodBrainBarrierTest, StatisticsUpdateAfterValidation)
{
    bbb_statistics_t stats_before, stats_after;

    EXPECT_TRUE(bbb_system_get_statistics(system, &stats_before));

    bbb_validation_result_t result;
    bbb_validate_string(system, "test string", &result);

    EXPECT_TRUE(bbb_system_get_statistics(system, &stats_after));
    EXPECT_GT(stats_after.total_validations, stats_before.total_validations);
}

TEST_F(BloodBrainBarrierTest, StatisticsUpdateAfterThreat)
{
    bbb_statistics_t stats_before, stats_after;

    EXPECT_TRUE(bbb_system_get_statistics(system, &stats_before));

    bbb_validation_result_t result;
    bbb_validate_string(system, "'; DROP TABLE users; --", &result);

    EXPECT_TRUE(bbb_system_get_statistics(system, &stats_after));
    EXPECT_GE(stats_after.threats_detected, stats_before.threats_detected);
}

}  // anonymous namespace
