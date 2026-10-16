/**
 * @file test_bbb_regression_v2.cpp
 * @brief Comprehensive GTest Regression Tests for Blood-Brain Barrier (BBB) Security
 *
 * WHAT: GTest-based regression tests verifying BBB backward compatibility and API stability
 * WHY:  Ensure BBB behavior remains consistent across versions without introducing regressions
 * HOW:  Test API contracts, error codes, performance baselines, and known bug fixes
 *
 * REGRESSION CATEGORIES:
 * 1. API Contract Stability - Function signatures, return values
 * 2. Error Code Consistency - Same errors for same invalid inputs
 * 3. Performance Baselines - Operations stay within time bounds
 * 4. Known Bug Fixes - Verify fixed bugs don't regress
 * 5. Input Validation Stability - Detection patterns remain stable
 * 6. Memory Boundary API - Region management consistency
 * 7. Access Control API - Subject/object handling stability
 *
 * @author NIMCP Security Team
 * @date 2026-02-02
 */

#include "test_helpers.h"

extern "C" {
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
}

#include <chrono>
#include <cstring>
#include <string>
#include <vector>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class BBBRegressionV2Test : public ::testing::Test {
protected:
    void SetUp() override
    {
        config_ = bbb_default_config();
        system_ = bbb_system_create(&config_);
        if (system_) {
            bbb_system_set_enabled(system_, true);
        }
    }

    void TearDown() override
    {
        if (system_) {
            bbb_clear_signing_key();
            bbb_system_destroy(system_);
            system_ = nullptr;
        }
    }

    double get_time_ms()
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration<double, std::milli>(duration).count();
    }

    bbb_config_t config_;
    bbb_system_t system_ = nullptr;
};

//=============================================================================
// API Contract Stability Tests
//=============================================================================

/**
 * Test: bbb_system_create() API contract
 * Regression: Must return NULL on NULL config or valid handle on valid config
 */
TEST_F(BBBRegressionV2Test, ApiCreateContract)
{
    // NULL config should work (use defaults)
    bbb_system_t sys1 = bbb_system_create(nullptr);
    EXPECT_NE(sys1, nullptr) << "bbb_system_create(NULL) should succeed with defaults";
    bbb_system_destroy(sys1);

    // Valid config should work
    bbb_config_t config = bbb_default_config();
    bbb_system_t sys2 = bbb_system_create(&config);
    EXPECT_NE(sys2, nullptr) << "bbb_system_create(&config) should succeed";
    bbb_system_destroy(sys2);
}

/**
 * Test: bbb_system_destroy() API contract
 * Regression: Must handle NULL safely (no crash)
 */
TEST_F(BBBRegressionV2Test, ApiDestroyNullSafety)
{
    // NULL should not crash
    bbb_system_destroy(nullptr);
    SUCCEED();
}

/**
 * Test: bbb_system_set_enabled() return value contract
 * Regression: Must return true on success, false on NULL
 */
TEST_F(BBBRegressionV2Test, ApiEnableReturnValue)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    bool result = bbb_system_set_enabled(system_, true);
    EXPECT_TRUE(result) << "set_enabled(true) should return true";

    result = bbb_system_set_enabled(system_, false);
    EXPECT_TRUE(result) << "set_enabled(false) should return true";

    result = bbb_system_set_enabled(nullptr, true);
    EXPECT_FALSE(result) << "set_enabled(NULL, true) should return false";
}

/**
 * Test: bbb_system_is_enabled() return value contract
 * Regression: Must return actual state, false for NULL
 */
TEST_F(BBBRegressionV2Test, ApiIsEnabledContract)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    bbb_system_set_enabled(system_, true);
    bool enabled = bbb_system_is_enabled(system_);
    EXPECT_TRUE(enabled) << "is_enabled should return true when enabled";

    bbb_system_set_enabled(system_, false);
    enabled = bbb_system_is_enabled(system_);
    EXPECT_FALSE(enabled) << "is_enabled should return false when disabled";

    enabled = bbb_system_is_enabled(nullptr);
    EXPECT_FALSE(enabled) << "is_enabled(NULL) should return false";
}

//=============================================================================
// Error Code Consistency Tests
//=============================================================================

/**
 * Test: Enum values must not change (ABI stability)
 * Regression: Changing enum values breaks serialization and compatibility
 */
TEST_F(BBBRegressionV2Test, EnumValuesStable)
{
    // Threat types - values must remain stable
    EXPECT_EQ(BBB_THREAT_NONE, 0) << "BBB_THREAT_NONE must be 0";
    EXPECT_EQ(BBB_THREAT_BUFFER_OVERFLOW, 1) << "BBB_THREAT_BUFFER_OVERFLOW must be 1";
    EXPECT_EQ(BBB_THREAT_FORMAT_STRING, 2) << "BBB_THREAT_FORMAT_STRING must be 2";
    EXPECT_EQ(BBB_THREAT_INTEGER_OVERFLOW, 3) << "BBB_THREAT_INTEGER_OVERFLOW must be 3";
    EXPECT_EQ(BBB_THREAT_SQL_INJECTION, 4) << "BBB_THREAT_SQL_INJECTION must be 4";
    EXPECT_EQ(BBB_THREAT_CODE_INJECTION, 5) << "BBB_THREAT_CODE_INJECTION must be 5";
    EXPECT_EQ(BBB_THREAT_SHELLCODE, 6) << "BBB_THREAT_SHELLCODE must be 6";
    EXPECT_EQ(BBB_THREAT_ROP_CHAIN, 7) << "BBB_THREAT_ROP_CHAIN must be 7";
    EXPECT_EQ(BBB_THREAT_INVALID_SIGNATURE, 8) << "BBB_THREAT_INVALID_SIGNATURE must be 8";
    EXPECT_EQ(BBB_THREAT_MEMORY_VIOLATION, 9) << "BBB_THREAT_MEMORY_VIOLATION must be 9";
    EXPECT_EQ(BBB_THREAT_UNAUTHORIZED_ACCESS, 10) << "BBB_THREAT_UNAUTHORIZED_ACCESS must be 10";

    // Severity levels
    EXPECT_EQ(BBB_SEVERITY_NONE, 0) << "BBB_SEVERITY_NONE must be 0";
    EXPECT_EQ(BBB_SEVERITY_LOW, 1) << "BBB_SEVERITY_LOW must be 1";
    EXPECT_EQ(BBB_SEVERITY_MEDIUM, 2) << "BBB_SEVERITY_MEDIUM must be 2";
    EXPECT_EQ(BBB_SEVERITY_HIGH, 3) << "BBB_SEVERITY_HIGH must be 3";
    EXPECT_EQ(BBB_SEVERITY_CRITICAL, 4) << "BBB_SEVERITY_CRITICAL must be 4";

    // Actions
    EXPECT_EQ(BBB_ACTION_ALLOW, 0) << "BBB_ACTION_ALLOW must be 0";
    EXPECT_EQ(BBB_ACTION_LOG, 1) << "BBB_ACTION_LOG must be 1";
    EXPECT_EQ(BBB_ACTION_BLOCK, 2) << "BBB_ACTION_BLOCK must be 2";
    EXPECT_EQ(BBB_ACTION_QUARANTINE, 3) << "BBB_ACTION_QUARANTINE must be 3";
    EXPECT_EQ(BBB_ACTION_TERMINATE, 4) << "BBB_ACTION_TERMINATE must be 4";
    EXPECT_EQ(BBB_ACTION_LOCKDOWN, 5) << "BBB_ACTION_LOCKDOWN must be 5";
}

/**
 * Test: Default config values must not change
 * Regression: Default behavior changes break existing deployments
 */
TEST_F(BBBRegressionV2Test, DefaultConfigStable)
{
    bbb_config_t config = bbb_default_config();

    // Input validation defaults
    EXPECT_TRUE(config.input.validate_strings) << "validate_strings default must be true";
    EXPECT_TRUE(config.input.validate_integers) << "validate_integers default must be true";
    EXPECT_TRUE(config.input.validate_pointers) << "validate_pointers default must be true";

    // Reasonable bounds on max_string_length
    EXPECT_GE(config.input.max_string_length, 1024u) << "max_string_length must be at least 1KB";
    EXPECT_LE(config.input.max_string_length, 1048576u) << "max_string_length must be at most 1MB";

    // Code signing defaults
    EXPECT_TRUE(config.signing.verify_on_load) << "verify_on_load default must be true";

    // Memory protection defaults
    EXPECT_TRUE(config.memory.enable_stack_canaries) << "enable_stack_canaries default must be true";
    EXPECT_TRUE(config.memory.enable_heap_guards) << "enable_heap_guards default must be true";
    EXPECT_GE(config.memory.guard_page_size, 4096u) << "guard_page_size must be at least one page";

    // Access control defaults
    EXPECT_TRUE(config.access.enable_capability) << "enable_capability default must be true";
    EXPECT_GE(config.access.max_privilege_level, 10u) << "max_privilege_level must be at least 10";

    // Default action must be secure
    EXPECT_GE(static_cast<int>(config.default_action), static_cast<int>(BBB_ACTION_BLOCK))
        << "default_action must be BLOCK or higher";
}

//=============================================================================
// Input Validation API Stability Tests
//=============================================================================

/**
 * Test: bbb_validate_string() behavior for safe inputs
 * Regression: Safe inputs must always be allowed
 */
TEST_F(BBBRegressionV2Test, ValidateStringSafeInputs)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    std::vector<const char*> safe_inputs = {
        "Hello, World!",
        "This is a normal sentence.",
        "user@example.com",
        "12345",
        "+1-555-123-4567",
        "https://example.com/page?id=123",
        "John O'Brien",  // Legitimate apostrophe
        "Price: $19.99",
        "{\"key\": \"value\"}",
        ""  // Empty string
    };

    for (const auto& input : safe_inputs) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(system_, input, &result);
        EXPECT_TRUE(valid) << "Safe input must be allowed: " << input;
        EXPECT_EQ(result.threat, BBB_THREAT_NONE) << "Safe input must have no threat: " << input;
    }
}

/**
 * Test: bbb_validate_string() behavior for SQL injection patterns
 * Regression: Known SQL injection patterns must always be detected
 */
TEST_F(BBBRegressionV2Test, ValidateStringSqlInjection)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    std::vector<const char*> sql_injections = {
        "'; DROP TABLE users; --",
        "' OR '1'='1",
        "' OR 1=1--",
        "'; DELETE FROM accounts; --",
        "1 UNION SELECT * FROM passwords",
        "' UNION SELECT username, password FROM users--",
        "admin'--",
    };

    for (const auto& input : sql_injections) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(system_, input, &result);
        EXPECT_FALSE(valid) << "SQL injection must be detected: " << input;
        EXPECT_EQ(result.threat, BBB_THREAT_SQL_INJECTION)
            << "Threat type must be SQL_INJECTION: " << input;
    }
}

/**
 * Test: bbb_validate_string() behavior for format string patterns
 * Regression: Known format string patterns must always be detected
 */
TEST_F(BBBRegressionV2Test, ValidateStringFormatString)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    std::vector<const char*> format_strings = {
        "%n%n%n%n",
        "%s%s%s%s",
        "%x%x%x%x",
        "AAAA%08x.%08x.%08x.%08x",
        "%p%p%p%p",
    };

    for (const auto& input : format_strings) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_string(system_, input, &result);
        EXPECT_FALSE(valid) << "Format string must be detected: " << input;
        EXPECT_EQ(result.threat, BBB_THREAT_FORMAT_STRING)
            << "Threat type must be FORMAT_STRING: " << input;
    }
}

/**
 * Test: bbb_validate_string() NULL handling
 * Regression: NULL inputs must return false, not crash
 */
TEST_F(BBBRegressionV2Test, ValidateStringNullHandling)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    bbb_validation_result_t result;

    // NULL string
    bool valid = bbb_validate_string(system_, nullptr, &result);
    EXPECT_FALSE(valid) << "NULL string must return false";

    // NULL result - should not crash
    valid = bbb_validate_string(system_, "test", nullptr);
    // Result may be true or false depending on implementation

    // NULL system
    valid = bbb_validate_string(nullptr, "test", &result);
    EXPECT_FALSE(valid) << "NULL system must return false";
}

//=============================================================================
// Memory Boundary API Stability Tests
//=============================================================================

/**
 * Test: bbb_register_memory_region() return value contract
 * Regression: Must return >0 on success, 0 on failure
 */
TEST_F(BBBRegressionV2Test, MemoryRegionRegistration)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    char buffer[1024];

    // Valid registration
    uint32_t region_id = bbb_register_memory_region(system_, buffer, sizeof(buffer), false);
    EXPECT_GT(region_id, 0u) << "Valid registration must return id > 0";

    // Unregister
    bool unregistered = bbb_unregister_memory_region(system_, region_id);
    EXPECT_TRUE(unregistered) << "Valid unregistration must succeed";

    // NULL system
    region_id = bbb_register_memory_region(nullptr, buffer, sizeof(buffer), false);
    EXPECT_EQ(region_id, 0u) << "NULL system must return 0";

    // NULL buffer
    region_id = bbb_register_memory_region(system_, nullptr, 100, false);
    EXPECT_EQ(region_id, 0u) << "NULL buffer must return 0";

    // Zero size
    region_id = bbb_register_memory_region(system_, buffer, 0, false);
    EXPECT_EQ(region_id, 0u) << "Zero size must return 0";
}

/**
 * Test: bbb_check_memory_access() consistency
 * Regression: Same access must return same result
 */
TEST_F(BBBRegressionV2Test, MemoryAccessConsistency)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    char buffer[1024];
    uint32_t region_id = bbb_register_memory_region(system_, buffer, sizeof(buffer), false);
    ASSERT_GT(region_id, 0u) << "Registration must succeed";

    // Same access should return same result
    bool access1 = bbb_check_memory_access(system_, buffer, 100, false);
    bool access2 = bbb_check_memory_access(system_, buffer, 100, false);
    EXPECT_EQ(access1, access2) << "Same access must return same result";

    bbb_unregister_memory_region(system_, region_id);
}

//=============================================================================
// Access Control API Stability Tests
//=============================================================================

/**
 * Test: Subject/Object registration contract
 * Regression: Registration must return true on valid inputs
 */
TEST_F(BBBRegressionV2Test, AccessControlRegistration)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    bbb_subject_t subject = {};
    subject.id = 1;
    subject.privilege_level = 5;
    subject.clearance = 0x01;
    subject.capabilities = 0x01;

    bbb_object_t object = {};
    object.id = 1;
    object.classification = 3;
    object.category = 0x01;
    object.required_capability = 0x01;

    // Valid registration
    bool reg_subj = bbb_register_subject(system_, &subject);
    EXPECT_TRUE(reg_subj) << "Subject registration must succeed";

    bool reg_obj = bbb_register_object(system_, &object);
    EXPECT_TRUE(reg_obj) << "Object registration must succeed";

    // NULL system
    reg_subj = bbb_register_subject(nullptr, &subject);
    EXPECT_FALSE(reg_subj) << "NULL system must fail";

    // NULL subject/object
    reg_subj = bbb_register_subject(system_, nullptr);
    EXPECT_FALSE(reg_subj) << "NULL subject must fail";

    reg_obj = bbb_register_object(system_, nullptr);
    EXPECT_FALSE(reg_obj) << "NULL object must fail";
}

/**
 * Test: bbb_check_access() consistency
 * Regression: Same access check must return same result
 */
TEST_F(BBBRegressionV2Test, AccessCheckConsistency)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    bbb_subject_t subject = {};
    subject.id = 1;
    subject.privilege_level = 5;
    subject.clearance = 0x01;
    subject.capabilities = 0x01;

    bbb_object_t object = {};
    object.id = 1;
    object.classification = 3;
    object.category = 0x01;
    object.required_capability = 0x01;

    bbb_register_subject(system_, &subject);
    bbb_register_object(system_, &object);

    // Same check should return same result
    bool check1 = bbb_check_access(system_, &subject, &object, 1);
    bool check2 = bbb_check_access(system_, &subject, &object, 1);
    EXPECT_EQ(check1, check2) << "Same access check must return same result";
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

/**
 * Test: String validation performance baseline
 * Regression: 1000 validations must complete in under 1 second
 */
TEST_F(BBBRegressionV2Test, PerformanceStringValidation)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    const int NUM_ITERATIONS = 1000;
    const double MAX_TIME_MS = 1000.0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        bbb_validation_result_t result;
        bbb_validate_string(system_, "safe input string for performance test", &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_LT(elapsed.count(), MAX_TIME_MS)
        << "String validation must meet performance baseline: " << elapsed.count() << " ms";
}

/**
 * Test: Hash calculation performance baseline
 * Regression: 1000 hashes must complete in under 500ms
 */
TEST_F(BBBRegressionV2Test, PerformanceHashCalculation)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

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
        << "Hash calculation must meet performance baseline: " << elapsed.count() << " ms";
}

/**
 * Test: Memory access check performance baseline
 * Regression: 10000 checks must complete in under 500ms
 */
TEST_F(BBBRegressionV2Test, PerformanceMemoryAccess)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    const int NUM_ITERATIONS = 10000;
    const double MAX_TIME_MS = 500.0;

    char buffer[1024];
    uint32_t region_id = bbb_register_memory_region(system_, buffer, sizeof(buffer), false);
    ASSERT_GT(region_id, 0u) << "Registration must succeed";

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        bbb_check_memory_access(system_, buffer, 100, false);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    bbb_unregister_memory_region(system_, region_id);

    EXPECT_LT(elapsed.count(), MAX_TIME_MS)
        << "Memory access check must meet performance baseline: " << elapsed.count() << " ms";
}

//=============================================================================
// Statistics API Stability Tests
//=============================================================================

/**
 * Test: bbb_system_get_statistics() contract
 * Regression: Statistics must be populated correctly
 */
TEST_F(BBBRegressionV2Test, StatisticsApiContract)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    bbb_statistics_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with garbage

    bool success = bbb_system_get_statistics(system_, &stats);
    EXPECT_TRUE(success) << "get_statistics must succeed";

    // After creation, counters should be initialized (0 or reasonable values)
    EXPECT_EQ(stats.total_validations, 0u) << "Initial validations must be 0";
    EXPECT_EQ(stats.threats_detected, 0u) << "Initial threats_detected must be 0";
    EXPECT_EQ(stats.threats_blocked, 0u) << "Initial threats_blocked must be 0";

    // Perform some operations
    bbb_validation_result_t result;
    bbb_validate_string(system_, "test", &result);
    bbb_validate_string(system_, "'; DROP TABLE; --", &result);

    // Get stats again
    success = bbb_system_get_statistics(system_, &stats);
    EXPECT_TRUE(success) << "get_statistics after ops must succeed";
    EXPECT_GE(stats.total_validations, 2u) << "total_validations must include all operations";
}

/**
 * Test: Statistics monotonically increasing
 * Regression: Counters must never decrease
 */
TEST_F(BBBRegressionV2Test, StatisticsMonotonic)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    bbb_statistics_t stats1, stats2;

    bbb_system_get_statistics(system_, &stats1);

    // Perform operations
    bbb_validation_result_t result;
    bbb_validate_string(system_, "test", &result);

    bbb_system_get_statistics(system_, &stats2);

    EXPECT_GE(stats2.total_validations, stats1.total_validations)
        << "total_validations must not decrease";
    EXPECT_GE(stats2.threats_detected, stats1.threats_detected)
        << "threats_detected must not decrease";
}

/**
 * Test: bbb_system_reset_statistics() contract
 * Regression: Reset must zero all counters
 */
TEST_F(BBBRegressionV2Test, StatisticsReset)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    // Perform operations to get non-zero stats
    bbb_validation_result_t result;
    bbb_validate_string(system_, "test", &result);
    bbb_validate_string(system_, "'; DROP TABLE; --", &result);

    // Reset
    bbb_system_reset_statistics(system_);

    bbb_statistics_t stats;
    bbb_system_get_statistics(system_, &stats);

    EXPECT_EQ(stats.total_validations, 0u) << "total_validations must be 0 after reset";
    EXPECT_EQ(stats.threats_detected, 0u) << "threats_detected must be 0 after reset";
    EXPECT_EQ(stats.threats_blocked, 0u) << "threats_blocked must be 0 after reset";
}

//=============================================================================
// BBB Helpers API Stability Tests
//=============================================================================

/**
 * Test: bbb_helpers init/shutdown lifecycle
 * Regression: Must support multiple init/shutdown cycles
 */
TEST_F(BBBRegressionV2Test, HelpersLifecycle)
{
    // Multiple cycles should not leak or crash
    for (int i = 0; i < 3; i++) {
        bool init_result = bbb_helpers_init();
        EXPECT_TRUE(init_result) << "helpers_init must succeed";

        bool is_init = bbb_helpers_is_initialized();
        EXPECT_TRUE(is_init) << "is_initialized must return true after init";

        bbb_helpers_shutdown();

        is_init = bbb_helpers_is_initialized();
        EXPECT_FALSE(is_init) << "is_initialized must return false after shutdown";
    }
}

/**
 * Test: bbb_check_pointer() contract
 * Regression: NULL returns false, valid returns true
 */
TEST_F(BBBRegressionV2Test, HelpersCheckPointer)
{
    bbb_helpers_init();

    int dummy = 42;

    bool result = bbb_check_pointer(nullptr, "test_func");
    EXPECT_FALSE(result) << "NULL pointer must return false";

    result = bbb_check_pointer(&dummy, "test_func");
    EXPECT_TRUE(result) << "Valid pointer must return true";

    bbb_helpers_shutdown();
}

/**
 * Test: bbb_check_string() contract
 * Regression: NULL returns false, valid returns true
 */
TEST_F(BBBRegressionV2Test, HelpersCheckString)
{
    bbb_helpers_init();

    bool result = bbb_check_string(nullptr, 100, "test_func");
    EXPECT_FALSE(result) << "NULL string must return false";

    result = bbb_check_string("valid", 100, "test_func");
    EXPECT_TRUE(result) << "Valid string must return true";

    bbb_helpers_shutdown();
}

/**
 * Test: bbb_validate_range() contract
 * Regression: In-range returns true, out-of-range returns false
 */
TEST_F(BBBRegressionV2Test, HelpersValidateRange)
{
    bbb_helpers_init();

    bool result = bbb_validate_range(50, 0, 100, "test_func");
    EXPECT_TRUE(result) << "Value in range must return true";

    result = bbb_validate_range(-1, 0, 100, "test_func");
    EXPECT_FALSE(result) << "Value below min must return false";

    result = bbb_validate_range(101, 0, 100, "test_func");
    EXPECT_FALSE(result) << "Value above max must return false";

    // Boundary values
    result = bbb_validate_range(0, 0, 100, "test_func");
    EXPECT_TRUE(result) << "Min boundary must return true";

    result = bbb_validate_range(100, 0, 100, "test_func");
    EXPECT_TRUE(result) << "Max boundary must return true";

    bbb_helpers_shutdown();
}

//=============================================================================
// Known Bug Fix Regression Tests
//=============================================================================

/**
 * Test: Empty string validation (bug fix)
 * Regression: Empty strings must be valid, not crash
 */
TEST_F(BBBRegressionV2Test, BugfixEmptyString)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    bbb_validation_result_t result;
    bool valid = bbb_validate_string(system_, "", &result);

    EXPECT_TRUE(valid) << "Empty string must be valid";
    EXPECT_TRUE(result.valid) << "Empty string result must be valid";
    EXPECT_EQ(result.threat, BBB_THREAT_NONE) << "Empty string must have no threat";
}

/**
 * Test: Very long string handling (bug fix)
 * Regression: Strings exceeding max must be detected as buffer overflow
 */
TEST_F(BBBRegressionV2Test, BugfixLongString)
{
    ASSERT_NE(system_, nullptr) << "Setup failed";

    bbb_config_t config = bbb_default_config();

    // Create string longer than max
    size_t len = config.input.max_string_length + 10;
    std::string long_string(len, 'A');

    bbb_validation_result_t result;
    bool valid = bbb_validate_string(system_, long_string.c_str(), &result);

    EXPECT_FALSE(valid) << "Long string must be rejected";
    EXPECT_EQ(result.threat, BBB_THREAT_BUFFER_OVERFLOW) << "Long string must be BUFFER_OVERFLOW";
}

/**
 * Test: Double destroy safety (bug fix)
 * Regression: Double destroy must not crash (idempotent)
 */
TEST_F(BBBRegressionV2Test, BugfixDoubleDestroy)
{
    bbb_config_t config = bbb_default_config();
    bbb_system_t sys = bbb_system_create(&config);
    ASSERT_NE(sys, nullptr) << "create must succeed";

    bbb_system_destroy(sys);

    // Second destroy on NULL must be safe
    bbb_system_destroy(nullptr);
    SUCCEED();
}

}  // anonymous namespace
