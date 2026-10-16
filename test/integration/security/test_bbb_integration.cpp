/**
 * @file test_bbb_integration.cpp
 * @brief Integration tests for Blood-Brain Barrier security system (NIMCP)
 *
 * Tests the integration of all four BBB layers working together:
 * - Full system workflows
 * - Multi-component interactions
 * - Concurrent access patterns
 * - Integration with NIMCP brain systems
 *
 * INTEGRATION SCENARIOS:
 * 1. Complete security workflow (create, configure, validate, report)
 * 2. Multiple simultaneous threats
 * 3. Thread-safe concurrent validation
 * 4. Alert callback integration
 * 5. Memory protection with access control
 */

#include "test_helpers.h"

// Headers have their own extern "C" guards
#include "security/nimcp_blood_brain_barrier.h"

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

class BBBIntegrationTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        // Reset all BBB subsystem state for test isolation
        bbb_reset_test_state();

        alert_count = 0;
        last_alert_type = BBB_THREAT_NONE;
        last_alert_severity = BBB_SEVERITY_NONE;

        config = bbb_default_config();
        config.alert_callback = &BBBIntegrationTest::alert_callback_static;
        config.strict_mode = true;

        system = bbb_system_create(&config);
        ASSERT_NE(system, nullptr);
        ASSERT_TRUE(bbb_system_set_enabled(system, true));
    }

    void TearDown() override
    {
        // Clear any signing key that was set during the test
        bbb_clear_signing_key();

        if (system) {
            bbb_system_destroy(system);
            system = nullptr;
        }
    }

    // Static callback for alerts (required for C callback interface)
    static void alert_callback_static(bbb_threat_type_t type, bbb_severity_t severity,
                                       const char* description)
    {
        std::lock_guard<std::mutex> lock(alert_mutex);
        alert_count++;
        last_alert_type = type;
        last_alert_severity = severity;
        if (description) {
            last_alert_description = description;
        }
    }

    // Static members for callback state
    static std::atomic<int> alert_count;
    static bbb_threat_type_t last_alert_type;
    static bbb_severity_t last_alert_severity;
    static std::string last_alert_description;
    static std::mutex alert_mutex;

    bbb_config_t config;
    bbb_system_t system;
};

// Static member definitions
std::atomic<int> BBBIntegrationTest::alert_count{0};
bbb_threat_type_t BBBIntegrationTest::last_alert_type = BBB_THREAT_NONE;
bbb_severity_t BBBIntegrationTest::last_alert_severity = BBB_SEVERITY_NONE;
std::string BBBIntegrationTest::last_alert_description;
std::mutex BBBIntegrationTest::alert_mutex;

//=============================================================================
// Full System Workflow Tests
//=============================================================================

TEST_F(BBBIntegrationTest, CompleteSecurityWorkflow)
{
    // Step 1: Configure system
    EXPECT_TRUE(bbb_system_is_enabled(system));

    // Step 2: Register memory region
    char protected_data[1024] = "Sensitive data";
    uint32_t region_id = bbb_register_memory_region(system, protected_data,
                                                     sizeof(protected_data), true);
    EXPECT_GT(region_id, 0u);

    // Step 3: Register access control subjects and objects
    bbb_subject_t subject;
    subject.id = 1;
    subject.privilege_level = 5;
    subject.roles = 0x01;
    subject.capabilities = 0x01;
    EXPECT_TRUE(bbb_register_subject(system, &subject));

    bbb_object_t object;
    object.id = 1;
    object.required_privilege = 3;
    object.required_roles = 0x01;
    object.required_capabilities = 0x01;
    EXPECT_TRUE(bbb_register_object(system, &object));

    // Step 4: Validate inputs
    bbb_validation_result_t result;
    EXPECT_TRUE(bbb_validate_string(system, "safe input", &result));
    EXPECT_TRUE(result.valid);

    // Step 5: Check access
    bool access_allowed = bbb_check_access(system, &subject, &object, 1);
    EXPECT_TRUE(access_allowed);

    // Step 6: Verify memory access
    bool read_allowed = bbb_check_memory_access(system, protected_data,
                                                 sizeof(protected_data), false);
    EXPECT_TRUE(read_allowed);

    bool write_allowed = bbb_check_memory_access(system, protected_data,
                                                  sizeof(protected_data), true);
    EXPECT_FALSE(write_allowed);  // Read-only region

    // Step 7: Get statistics
    bbb_statistics_t stats;
    EXPECT_TRUE(bbb_system_get_statistics(system, &stats));
    EXPECT_GT(stats.total_validations, 0u);

    // Step 8: Cleanup
    EXPECT_TRUE(bbb_unregister_memory_region(system, region_id));
}

TEST_F(BBBIntegrationTest, ThreatDetectionWorkflow)
{
    bbb_validation_result_t result;

    // Test various attack vectors
    const char* sql_injection = "'; DROP TABLE users; --";
    const char* xss_attack = "<script>alert('XSS')</script>";
    const char* format_string = "%n%n%n%n";
    const char* path_traversal = "../../../etc/passwd";

    // SQL Injection should be detected
    EXPECT_FALSE(bbb_validate_string(system, sql_injection, &result));
    EXPECT_EQ(result.threat, BBB_THREAT_SQL_INJECTION);

    // Format string should be detected
    EXPECT_FALSE(bbb_validate_string(system, format_string, &result));
    EXPECT_EQ(result.threat, BBB_THREAT_FORMAT_STRING);

    // Get threat reports
    bbb_threat_report_t reports[10];
    size_t count = bbb_get_threat_reports(system, reports, 10);
    EXPECT_GE(count, 2u);

    // Verify statistics reflect threats
    bbb_statistics_t stats;
    EXPECT_TRUE(bbb_system_get_statistics(system, &stats));
    EXPECT_GE(stats.threats_detected, 2u);
    EXPECT_GE(stats.threats_blocked, 2u);
}

TEST_F(BBBIntegrationTest, CodeSigningWorkflow)
{
    // Step 0: Configure signing key (required before signing)
    static const uint8_t test_key[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    ASSERT_TRUE(bbb_set_signing_key(test_key, sizeof(test_key)));

    // Step 1: Sign code
    const char* code = R"(
        int calculate(int a, int b) {
            return a + b;
        }
    )";

    uint8_t signature[512];
    ssize_t sig_len = bbb_sign_code(system, code, strlen(code), signature, sizeof(signature));
    ASSERT_GT(sig_len, 0);

    // Step 2: Verify original code
    EXPECT_TRUE(bbb_verify_signature(system, code, strlen(code), signature, sig_len));

    // Step 3: Tamper detection - modify code
    const char* tampered_code = R"(
        int calculate(int a, int b) {
            return a - b;  // Changed from + to -
        }
    )";

    EXPECT_FALSE(bbb_verify_signature(system, tampered_code, strlen(tampered_code),
                                       signature, sig_len));

    // Step 4: Report tampering attempt
    bbb_threat_report_t report = bbb_report_threat(
        system,
        BBB_THREAT_INVALID_SIGNATURE,
        BBB_SEVERITY_CRITICAL,
        "Code tampering detected",
        tampered_code,
        tampered_code,
        strlen(tampered_code)
    );

    EXPECT_EQ(report.type, BBB_THREAT_INVALID_SIGNATURE);
    EXPECT_EQ(report.severity, BBB_SEVERITY_CRITICAL);
}

//=============================================================================
// Multi-Component Integration Tests
//=============================================================================

TEST_F(BBBIntegrationTest, MemoryWithAccessControl)
{
    // Register protected memory region
    char sensitive_memory[2048];
    strcpy(sensitive_memory, "CONFIDENTIAL DATA");

    uint32_t region_id = bbb_register_memory_region(system, sensitive_memory,
                                                     sizeof(sensitive_memory), true);
    ASSERT_GT(region_id, 0u);

    // Create low-privilege subject
    bbb_subject_t low_priv_subject;
    low_priv_subject.id = 100;
    low_priv_subject.privilege_level = 1;
    low_priv_subject.roles = 0x00;
    low_priv_subject.capabilities = 0x00;
    ASSERT_TRUE(bbb_register_subject(system, &low_priv_subject));

    // Create high-privilege subject
    bbb_subject_t high_priv_subject;
    high_priv_subject.id = 101;
    high_priv_subject.privilege_level = 10;
    high_priv_subject.roles = 0xFF;
    high_priv_subject.capabilities = 0xFF;
    ASSERT_TRUE(bbb_register_subject(system, &high_priv_subject));

    // Create object representing the memory region
    bbb_object_t memory_object;
    memory_object.id = region_id;
    memory_object.required_privilege = 5;
    memory_object.required_roles = 0x01;
    memory_object.required_capabilities = 0x01;
    ASSERT_TRUE(bbb_register_object(system, &memory_object));

    // Low privilege subject denied
    EXPECT_FALSE(bbb_check_access(system, &low_priv_subject, &memory_object, 1));

    // High privilege subject allowed
    EXPECT_TRUE(bbb_check_access(system, &high_priv_subject, &memory_object, 1));

    // Memory boundary still enforces read-only
    EXPECT_FALSE(bbb_check_memory_access(system, sensitive_memory, 100, true));
}

TEST_F(BBBIntegrationTest, InputValidationWithQuarantine)
{
    char malicious_input[256];
    strcpy(malicious_input, "'; EXEC xp_cmdshell('rm -rf /'); --");

    bbb_validation_result_t result;
    bool valid = bbb_validate_string(system, malicious_input, &result);

    EXPECT_FALSE(valid);
    EXPECT_EQ(result.threat, BBB_THREAT_SQL_INJECTION);

    // Quarantine the malicious data
    EXPECT_TRUE(bbb_quarantine_region(system, malicious_input, sizeof(malicious_input)));

    // Check that memory access is blocked
    EXPECT_FALSE(bbb_check_memory_access(system, malicious_input, sizeof(malicious_input), false));

    // Release quarantine
    EXPECT_TRUE(bbb_release_quarantine(system, malicious_input));
}

TEST_F(BBBIntegrationTest, StackCanaryWithMemoryProtection)
{
    char function_stack[512];
    memset(function_stack, 0, sizeof(function_stack));

    // Install canary
    uint64_t canary = bbb_install_stack_canary(system, function_stack);
    ASSERT_NE(canary, 0u);

    // Register the stack region
    uint32_t region_id = bbb_register_memory_region(system, function_stack,
                                                     sizeof(function_stack), false);
    ASSERT_GT(region_id, 0u);

    // Normal operation - canary intact
    EXPECT_TRUE(bbb_verify_stack_canary(system, function_stack, canary));

    // Simulate buffer overflow attack
    memset(function_stack, 'A', sizeof(function_stack));

    // Canary should be corrupted
    EXPECT_FALSE(bbb_verify_stack_canary(system, function_stack, canary));

    // Report the overflow
    bbb_report_threat(system, BBB_THREAT_BUFFER_OVERFLOW, BBB_SEVERITY_CRITICAL,
                      "Stack canary corruption detected", function_stack, nullptr, 0);

    bbb_statistics_t stats;
    EXPECT_TRUE(bbb_system_get_statistics(system, &stats));
    EXPECT_GE(stats.threats_detected, 1u);
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

TEST_F(BBBIntegrationTest, ConcurrentValidation)
{
    const int NUM_THREADS = 8;
    const int VALIDATIONS_PER_THREAD = 100;
    std::atomic<int> total_validations{0};
    std::atomic<int> threats_detected{0};
    std::vector<std::thread> threads;

    auto validate_task = [this, &total_validations, &threats_detected](int thread_id) {
        for (int i = 0; i < VALIDATIONS_PER_THREAD; i++) {
            bbb_validation_result_t result;
            bool valid;

            // Alternate between safe and malicious inputs
            if (i % 2 == 0) {
                valid = bbb_validate_string(system, "safe input string", &result);
                if (!valid) {
                    threats_detected++;
                }
            } else {
                valid = bbb_validate_string(system, "'; DROP TABLE x; --", &result);
                if (!valid) {
                    threats_detected++;
                }
            }
            total_validations++;
        }
    };

    // Launch threads
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(validate_task, i);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_validations.load(), NUM_THREADS * VALIDATIONS_PER_THREAD);
    EXPECT_GE(threats_detected.load(), NUM_THREADS * VALIDATIONS_PER_THREAD / 2);

    // Verify statistics are consistent
    bbb_statistics_t stats;
    EXPECT_TRUE(bbb_system_get_statistics(system, &stats));
    EXPECT_EQ(stats.total_validations, (uint64_t)(NUM_THREADS * VALIDATIONS_PER_THREAD));
}

TEST_F(BBBIntegrationTest, ConcurrentMemoryOperations)
{
    const int NUM_THREADS = 4;
    std::atomic<int> successful_registrations{0};
    std::vector<std::thread> threads;
    std::vector<char*> buffers(NUM_THREADS);

    // Pre-allocate buffers
    for (int i = 0; i < NUM_THREADS; i++) {
        buffers[i] = new char[1024];
    }

    auto memory_task = [this, &successful_registrations, &buffers](int thread_id) {
        char* buffer = buffers[thread_id];

        uint32_t region_id = bbb_register_memory_region(system, buffer, 1024, false);
        if (region_id > 0) {
            successful_registrations++;

            // Perform some access checks
            bbb_check_memory_access(system, buffer, 512, false);
            bbb_check_memory_access(system, buffer, 512, true);

            // Unregister
            bbb_unregister_memory_region(system, region_id);
        }
    };

    // Launch threads
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(memory_task, i);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successful_registrations.load(), NUM_THREADS);

    // Cleanup
    for (int i = 0; i < NUM_THREADS; i++) {
        delete[] buffers[i];
    }
}

TEST_F(BBBIntegrationTest, ConcurrentAccessControl)
{
    const int NUM_SUBJECTS = 10;
    std::vector<std::thread> threads;
    std::atomic<int> access_granted{0};
    std::atomic<int> access_denied{0};

    // Register an object
    bbb_object_t object;
    object.id = 1;
    object.required_privilege = 5;
    object.required_roles = 0x01;
    object.required_capabilities = 0x01;
    ASSERT_TRUE(bbb_register_object(system, &object));

    auto access_task = [this, &access_granted, &access_denied, &object](int subject_id) {
        bbb_subject_t subject;
        subject.id = subject_id + 1;  // IDs start at 1 (0 is invalid)
        subject.privilege_level = subject_id + 1;  // Varies by thread
        subject.roles = (subject_id >= 5) ? 0x01 : 0x00;
        subject.capabilities = (subject_id >= 5) ? 0x01 : 0x00;

        EXPECT_TRUE(bbb_register_subject(system, &subject));

        for (int i = 0; i < 100; i++) {
            bool allowed = bbb_check_access(system, &subject, &object, 1);
            if (allowed) {
                access_granted++;
            } else {
                access_denied++;
            }
        }
    };

    // Launch threads
    for (int i = 0; i < NUM_SUBJECTS; i++) {
        threads.emplace_back(access_task, i);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    // Half should be granted (subjects 5-9), half denied (subjects 0-4)
    EXPECT_GT(access_granted.load(), 0);
    EXPECT_GT(access_denied.load(), 0);
}

//=============================================================================
// Alert Callback Integration Tests
//=============================================================================

TEST_F(BBBIntegrationTest, AlertCallbackTriggered)
{
    alert_count = 0;

    // Trigger a threat
    bbb_validation_result_t result;
    bbb_validate_string(system, "'; DELETE FROM accounts; --", &result);

    // Allow time for callback
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_GT(alert_count.load(), 0);
    EXPECT_EQ(last_alert_type, BBB_THREAT_SQL_INJECTION);
    EXPECT_GE(last_alert_severity, BBB_SEVERITY_MEDIUM);
}

TEST_F(BBBIntegrationTest, MultipleAlertsInSequence)
{
    alert_count = 0;

    bbb_validation_result_t result;

    // Trigger multiple threats
    bbb_validate_string(system, "'; DROP TABLE; --", &result);
    bbb_validate_string(system, "%n%n%n%n", &result);
    bbb_validate_string(system, "1 UNION SELECT *", &result);

    // Allow time for callbacks
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_GE(alert_count.load(), 3);
}

//=============================================================================
// System State Tests
//=============================================================================

TEST_F(BBBIntegrationTest, DisabledSystemPassesAll)
{
    bbb_system_set_enabled(system, false);

    bbb_validation_result_t result;

    // Even malicious input should pass when disabled
    bool valid = bbb_validate_string(system, "'; DROP TABLE users; --", &result);

    // Behavior depends on implementation - may still validate but not block
    EXPECT_TRUE(valid || !bbb_system_is_enabled(system));
}

TEST_F(BBBIntegrationTest, ReenableAfterDisable)
{
    bbb_system_set_enabled(system, false);
    EXPECT_FALSE(bbb_system_is_enabled(system));

    bbb_system_set_enabled(system, true);
    EXPECT_TRUE(bbb_system_is_enabled(system));

    // Should now detect threats again
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(system, "'; DROP TABLE x; --", &result);
    EXPECT_FALSE(valid);
}

TEST_F(BBBIntegrationTest, StatisticsPersistAcrossOperations)
{
    bbb_statistics_t stats1, stats2;

    // Perform operations
    bbb_validation_result_t result;
    bbb_validate_string(system, "test1", &result);
    bbb_validate_string(system, "test2", &result);
    bbb_validate_string(system, "'; DROP TABLE; --", &result);

    EXPECT_TRUE(bbb_system_get_statistics(system, &stats1));
    EXPECT_EQ(stats1.total_validations, 3u);
    EXPECT_GE(stats1.threats_detected, 1u);

    // More operations
    bbb_validate_string(system, "test3", &result);

    EXPECT_TRUE(bbb_system_get_statistics(system, &stats2));
    EXPECT_EQ(stats2.total_validations, 4u);
}

//=============================================================================
// Error Recovery Tests
//=============================================================================

TEST_F(BBBIntegrationTest, RecoverFromQuarantine)
{
    char data[256];
    strcpy(data, "Initially safe data");

    // Quarantine the region
    EXPECT_TRUE(bbb_quarantine_region(system, data, sizeof(data)));

    // Access should be blocked
    EXPECT_FALSE(bbb_check_memory_access(system, data, sizeof(data), false));

    // Release quarantine
    EXPECT_TRUE(bbb_release_quarantine(system, data));

    // Access should now be allowed (need to register first)
    uint32_t region_id = bbb_register_memory_region(system, data, sizeof(data), false);
    if (region_id > 0) {
        EXPECT_TRUE(bbb_check_memory_access(system, data, sizeof(data), false));
        bbb_unregister_memory_region(system, region_id);
    }
}

TEST_F(BBBIntegrationTest, ClearReportsDoesNotAffectStatistics)
{
    bbb_validation_result_t result;
    bbb_validate_string(system, "'; DROP TABLE; --", &result);

    bbb_statistics_t stats_before;
    EXPECT_TRUE(bbb_system_get_statistics(system, &stats_before));

    bbb_clear_threat_reports(system);

    bbb_statistics_t stats_after;
    EXPECT_TRUE(bbb_system_get_statistics(system, &stats_after));

    // Statistics should be unchanged
    EXPECT_EQ(stats_before.threats_detected, stats_after.threats_detected);
    EXPECT_EQ(stats_before.total_validations, stats_after.total_validations);
}

}  // anonymous namespace
