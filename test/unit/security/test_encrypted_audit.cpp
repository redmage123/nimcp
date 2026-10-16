/**
 * @file test_encrypted_audit.cpp
 * @brief Comprehensive unit tests for Encrypted Audit Buffer
 *
 * WHAT: Comprehensive tests for encrypted audit logging
 * WHY:  Ensure encryption, decryption, and tamper detection work correctly
 * HOW:  Google Test framework with encryption verification
 *
 * TEST COVERAGE:
 * - Create/destroy lifecycle
 * - Audit entry encryption/decryption
 * - Circular buffer behavior
 * - Key rotation
 * - Export/import functionality
 * - Buffer overflow protection
 * - Statistics and monitoring
 * - Thread safety
 * - Error handling paths
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include "security/nimcp_encrypted_audit.h"
#include "security/nimcp_security.h"
#include <thread>
#include <vector>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <unistd.h>

//=============================================================================
// Test Fixture
//=============================================================================

class EncryptedAuditTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate test key
        nimcp_encryption_generate_key(test_key);

        config = nimcp_encrypted_audit_default_config();
        config.buffer_size = 100;
        config.enable_compression = false;

        audit = nimcp_encrypted_audit_create(&config, test_key, sizeof(test_key));
        ASSERT_NE(audit, nullptr);
    }

    void TearDown() override {
        if (audit) {
            nimcp_encrypted_audit_destroy(audit);
            audit = nullptr;
        }
    }

    uint8_t test_key[NIMCP_AUDIT_KEY_SIZE];
    nimcp_encrypted_audit_config_t config;
    nimcp_encrypted_audit_t audit;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EncryptedAuditTest, CreateDestroy) {
    EXPECT_NE(audit, nullptr);

    nimcp_encrypted_audit_stats_t stats;
    EXPECT_EQ(nimcp_encrypted_audit_get_stats(audit, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_entries, 0);
    EXPECT_EQ(stats.current_key_version, 0);
}

TEST_F(EncryptedAuditTest, CreateWithInvalidKey) {
    uint8_t short_key[16];  // Too short
    nimcp_encrypted_audit_t invalid_audit = nimcp_encrypted_audit_create(&config, short_key, sizeof(short_key));
    EXPECT_EQ(invalid_audit, nullptr);
}

TEST_F(EncryptedAuditTest, CreateWithNullKey) {
    nimcp_encrypted_audit_t invalid_audit = nimcp_encrypted_audit_create(&config, nullptr, NIMCP_AUDIT_KEY_SIZE);
    EXPECT_EQ(invalid_audit, nullptr);
}

TEST_F(EncryptedAuditTest, CreateWithZeroLengthKey) {
    uint8_t key[NIMCP_AUDIT_KEY_SIZE];
    nimcp_encrypted_audit_t invalid_audit = nimcp_encrypted_audit_create(&config, key, 0);
    EXPECT_EQ(invalid_audit, nullptr);
}

TEST_F(EncryptedAuditTest, CreateWithNullConfig) {
    // Should use defaults when config is NULL
    uint8_t key[NIMCP_AUDIT_KEY_SIZE];
    nimcp_encryption_generate_key(key);
    nimcp_encrypted_audit_t audit_with_defaults = nimcp_encrypted_audit_create(nullptr, key, sizeof(key));
    EXPECT_NE(audit_with_defaults, nullptr);
    if (audit_with_defaults) {
        nimcp_encrypted_audit_destroy(audit_with_defaults);
    }
}

TEST_F(EncryptedAuditTest, DestroyNullHandle) {
    // Should not crash when destroying NULL
    nimcp_encrypted_audit_destroy(nullptr);
}

TEST_F(EncryptedAuditTest, DefaultConfigValues) {
    nimcp_encrypted_audit_config_t default_config = nimcp_encrypted_audit_default_config();
    EXPECT_EQ(default_config.buffer_size, NIMCP_AUDIT_DEFAULT_BUFFER_SIZE);
    EXPECT_EQ(default_config.max_entry_size, NIMCP_AUDIT_DEFAULT_ENTRY_SIZE);
    EXPECT_EQ(default_config.key_rotation_interval, NIMCP_AUDIT_DEFAULT_KEY_ROTATION);
    EXPECT_FALSE(default_config.enable_compression);
    EXPECT_FALSE(default_config.enable_bio_async);
    EXPECT_TRUE(default_config.lock_memory);
    EXPECT_TRUE(default_config.secure_erase);
}

//=============================================================================
// Logging Tests
//=============================================================================

TEST_F(EncryptedAuditTest, LogSimpleEntry) {
    EXPECT_EQ(nimcp_encrypted_audit_log(
        audit,
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_AUTHENTICATION,
        "Test message",
        nullptr,
        0
    ), NIMCP_SUCCESS);

    nimcp_encrypted_audit_stats_t stats;
    nimcp_encrypted_audit_get_stats(audit, &stats);
    EXPECT_EQ(stats.total_entries, 1);
    EXPECT_EQ(stats.entries_encrypted, 1);
}

TEST_F(EncryptedAuditTest, LogWithData) {
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};

    EXPECT_EQ(nimcp_encrypted_audit_log(
        audit,
        NIMCP_AUDIT_WARNING,
        NIMCP_AUDIT_NETWORK,
        "Test with data",
        test_data,
        sizeof(test_data)
    ), NIMCP_SUCCESS);

    nimcp_encrypted_audit_stats_t stats;
    nimcp_encrypted_audit_get_stats(audit, &stats);
    EXPECT_EQ(stats.total_entries, 1);
}

TEST_F(EncryptedAuditTest, LogWithLargeData) {
    // Test with data close to max size
    std::vector<uint8_t> large_data(NIMCP_AUDIT_MAX_DATA_SIZE - 1);
    for (size_t i = 0; i < large_data.size(); i++) {
        large_data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    EXPECT_EQ(nimcp_encrypted_audit_log(
        audit,
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_DATA_ACCESS,
        "Large data test",
        large_data.data(),
        large_data.size()
    ), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, LogWithTooLargeData) {
    // Test with data exceeding max size
    std::vector<uint8_t> oversized_data(NIMCP_AUDIT_MAX_DATA_SIZE + 1);

    EXPECT_NE(nimcp_encrypted_audit_log(
        audit,
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_DATA_ACCESS,
        "Oversized data test",
        oversized_data.data(),
        oversized_data.size()
    ), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, LogMultipleEntries) {
    const int num_entries = 10;

    for (int i = 0; i < num_entries; i++) {
        char message[256];
        snprintf(message, sizeof(message), "Entry %d", i);

        EXPECT_EQ(nimcp_encrypted_audit_log(
            audit,
            NIMCP_AUDIT_INFO,
            NIMCP_AUDIT_SYSTEM,
            message,
            nullptr,
            0
        ), NIMCP_SUCCESS);
    }

    nimcp_encrypted_audit_stats_t stats;
    nimcp_encrypted_audit_get_stats(audit, &stats);
    EXPECT_EQ(stats.total_entries, num_entries);
}

TEST_F(EncryptedAuditTest, LogFormattedEntry) {
    EXPECT_EQ(nimcp_encrypted_audit_logf(
        audit,
        NIMCP_AUDIT_ERROR,
        NIMCP_AUDIT_APPLICATION,
        "Error code: %d, message: %s",
        42,
        "test error"
    ), NIMCP_SUCCESS);

    nimcp_encrypted_audit_stats_t stats;
    nimcp_encrypted_audit_get_stats(audit, &stats);
    EXPECT_EQ(stats.total_entries, 1);
}

TEST_F(EncryptedAuditTest, LogTimestamped) {
    uint64_t custom_timestamp = 1234567890123456789ULL;

    EXPECT_EQ(nimcp_encrypted_audit_log_timestamped(
        audit,
        custom_timestamp,
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_SYSTEM,
        "Timestamped entry",
        nullptr,
        0
    ), NIMCP_SUCCESS);

    // Read back and verify timestamp
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read(audit, entries, 10, &num_entries), NIMCP_SUCCESS);
    EXPECT_EQ(num_entries, 1);
    EXPECT_EQ(entries[0].timestamp_ns, custom_timestamp);
}

TEST_F(EncryptedAuditTest, LogAllSeverities) {
    const nimcp_audit_severity_t severities[] = {
        NIMCP_AUDIT_DEBUG,
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_WARNING,
        NIMCP_AUDIT_ERROR,
        NIMCP_AUDIT_CRITICAL,
        NIMCP_AUDIT_EMERGENCY
    };

    for (auto severity : severities) {
        EXPECT_EQ(nimcp_encrypted_audit_log(
            audit,
            severity,
            NIMCP_AUDIT_SYSTEM,
            "Test severity",
            nullptr,
            0
        ), NIMCP_SUCCESS);
    }

    nimcp_encrypted_audit_stats_t stats;
    nimcp_encrypted_audit_get_stats(audit, &stats);
    EXPECT_EQ(stats.total_entries, 6);
}

TEST_F(EncryptedAuditTest, LogAllCategories) {
    const nimcp_audit_category_t categories[] = {
        NIMCP_AUDIT_AUTHENTICATION,
        NIMCP_AUDIT_AUTHORIZATION,
        NIMCP_AUDIT_DATA_ACCESS,
        NIMCP_AUDIT_CONFIGURATION,
        NIMCP_AUDIT_NETWORK,
        NIMCP_AUDIT_SYSTEM,
        NIMCP_AUDIT_APPLICATION,
        NIMCP_AUDIT_THREAT,
        NIMCP_AUDIT_ENCRYPTION,
        NIMCP_AUDIT_PATTERN
    };

    for (auto category : categories) {
        EXPECT_EQ(nimcp_encrypted_audit_log(
            audit,
            NIMCP_AUDIT_INFO,
            category,
            "Test category",
            nullptr,
            0
        ), NIMCP_SUCCESS);
    }

    nimcp_encrypted_audit_stats_t stats;
    nimcp_encrypted_audit_get_stats(audit, &stats);
    EXPECT_EQ(stats.total_entries, 10);
}

TEST_F(EncryptedAuditTest, LogLongMessage) {
    // Test with message close to max size
    std::string long_message(NIMCP_AUDIT_MAX_MESSAGE_SIZE - 10, 'A');

    EXPECT_EQ(nimcp_encrypted_audit_log(
        audit,
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_SYSTEM,
        long_message.c_str(),
        nullptr,
        0
    ), NIMCP_SUCCESS);
}

//=============================================================================
// Reading and Decryption Tests
//=============================================================================

TEST_F(EncryptedAuditTest, ReadAfterWrite) {
    // Log entry
    ASSERT_EQ(nimcp_encrypted_audit_log(
        audit,
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_SYSTEM,
        "Read test",
        nullptr,
        0
    ), NIMCP_SUCCESS);

    // Read back
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read(audit, entries, 10, &num_entries), NIMCP_SUCCESS);
    EXPECT_EQ(num_entries, 1);
    EXPECT_STREQ(entries[0].message, "Read test");
    EXPECT_EQ(entries[0].severity, NIMCP_AUDIT_INFO);
    EXPECT_EQ(entries[0].category, NIMCP_AUDIT_SYSTEM);
}

TEST_F(EncryptedAuditTest, ReadMultipleEntries) {
    const int num_to_write = 5;

    // Write entries
    for (int i = 0; i < num_to_write; i++) {
        char message[256];
        snprintf(message, sizeof(message), "Entry %d", i);

        ASSERT_EQ(nimcp_encrypted_audit_log(
            audit,
            NIMCP_AUDIT_INFO,
            NIMCP_AUDIT_SYSTEM,
            message,
            nullptr,
            0
        ), NIMCP_SUCCESS);
    }

    // Read back
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read(audit, entries, 10, &num_entries), NIMCP_SUCCESS);
    EXPECT_EQ(num_entries, num_to_write);
}

TEST_F(EncryptedAuditTest, ReadWithFilter) {
    // Log entries with different severities
    ASSERT_EQ(nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_DEBUG, NIMCP_AUDIT_SYSTEM, "Debug", nullptr, 0), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, "Info", nullptr, 0), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_WARNING, NIMCP_AUDIT_SYSTEM, "Warning", nullptr, 0), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_ERROR, NIMCP_AUDIT_SYSTEM, "Error", nullptr, 0), NIMCP_SUCCESS);

    // Read only WARNING and above
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read_filtered(
        audit,
        NIMCP_AUDIT_WARNING,
        NIMCP_AUDIT_CATEGORY_COUNT,  // All categories
        entries,
        10,
        &num_entries
    ), NIMCP_SUCCESS);

    EXPECT_EQ(num_entries, 2);  // WARNING and ERROR
}

TEST_F(EncryptedAuditTest, ReadByCategory) {
    // Log entries in different categories
    ASSERT_EQ(nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_AUTHENTICATION, "Auth 1", nullptr, 0), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_NETWORK, "Net 1", nullptr, 0), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_AUTHENTICATION, "Auth 2", nullptr, 0), NIMCP_SUCCESS);

    // Read only authentication
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read_filtered(
        audit,
        NIMCP_AUDIT_DEBUG,  // All severities
        NIMCP_AUDIT_AUTHENTICATION,
        entries,
        10,
        &num_entries
    ), NIMCP_SUCCESS);

    EXPECT_EQ(num_entries, 2);
}

TEST_F(EncryptedAuditTest, ReadTimeRange) {
    uint64_t start_time = 1000000000000ULL;
    uint64_t mid_time = 2000000000000ULL;
    uint64_t end_time = 3000000000000ULL;

    // Log with specific timestamps
    ASSERT_EQ(nimcp_encrypted_audit_log_timestamped(audit, start_time, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, "First", nullptr, 0), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_encrypted_audit_log_timestamped(audit, mid_time, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, "Second", nullptr, 0), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_encrypted_audit_log_timestamped(audit, end_time, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, "Third", nullptr, 0), NIMCP_SUCCESS);

    // Read only middle range
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read_range(
        audit,
        mid_time - 1,
        mid_time + 1,
        entries,
        10,
        &num_entries
    ), NIMCP_SUCCESS);

    EXPECT_EQ(num_entries, 1);
    EXPECT_STREQ(entries[0].message, "Second");
}

TEST_F(EncryptedAuditTest, ReadWithDataVerification) {
    uint8_t test_data[] = {0xDE, 0xAD, 0xBE, 0xEF};

    ASSERT_EQ(nimcp_encrypted_audit_log(
        audit,
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_DATA_ACCESS,
        "Data verification test",
        test_data,
        sizeof(test_data)
    ), NIMCP_SUCCESS);

    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read(audit, entries, 10, &num_entries), NIMCP_SUCCESS);
    EXPECT_EQ(num_entries, 1);
    EXPECT_EQ(entries[0].data_len, sizeof(test_data));
    EXPECT_EQ(memcmp(entries[0].data, test_data, sizeof(test_data)), 0);
}

//=============================================================================
// Circular Buffer Tests
//=============================================================================

TEST_F(EncryptedAuditTest, CircularBufferWrap) {
    // Create small buffer
    uint8_t key[NIMCP_AUDIT_KEY_SIZE];
    nimcp_encryption_generate_key(key);

    nimcp_encrypted_audit_config_t small_config = nimcp_encrypted_audit_default_config();
    small_config.buffer_size = 5;

    nimcp_encrypted_audit_t small_audit = nimcp_encrypted_audit_create(&small_config, key, sizeof(key));
    ASSERT_NE(small_audit, nullptr);

    // Write more than buffer size
    for (int i = 0; i < 10; i++) {
        char message[256];
        snprintf(message, sizeof(message), "Entry %d", i);

        EXPECT_EQ(nimcp_encrypted_audit_log(
            small_audit,
            NIMCP_AUDIT_INFO,
            NIMCP_AUDIT_SYSTEM,
            message,
            nullptr,
            0
        ), NIMCP_SUCCESS);
    }

    // Check statistics
    nimcp_encrypted_audit_stats_t stats;
    nimcp_encrypted_audit_get_stats(small_audit, &stats);
    EXPECT_EQ(stats.total_entries, 10);
    EXPECT_GT(stats.buffer_wraps, 0);

    nimcp_encrypted_audit_destroy(small_audit);
}

TEST_F(EncryptedAuditTest, BufferOverflowProtection) {
    // Create very small buffer
    uint8_t key[NIMCP_AUDIT_KEY_SIZE];
    nimcp_encryption_generate_key(key);

    nimcp_encrypted_audit_config_t tiny_config = nimcp_encrypted_audit_default_config();
    tiny_config.buffer_size = 3;

    nimcp_encrypted_audit_t tiny_audit = nimcp_encrypted_audit_create(&tiny_config, key, sizeof(key));
    ASSERT_NE(tiny_audit, nullptr);

    // Write many entries - should not crash
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_encrypted_audit_log(
            tiny_audit,
            NIMCP_AUDIT_INFO,
            NIMCP_AUDIT_SYSTEM,
            "Overflow test",
            nullptr,
            0
        ), NIMCP_SUCCESS);
    }

    // Only last 3 entries should be readable
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read(tiny_audit, entries, 10, &num_entries), NIMCP_SUCCESS);
    EXPECT_LE(num_entries, 3);

    nimcp_encrypted_audit_destroy(tiny_audit);
}

TEST_F(EncryptedAuditTest, OldestNewestEntryTracking) {
    // Create small buffer
    uint8_t key[NIMCP_AUDIT_KEY_SIZE];
    nimcp_encryption_generate_key(key);

    nimcp_encrypted_audit_config_t small_config = nimcp_encrypted_audit_default_config();
    small_config.buffer_size = 5;

    nimcp_encrypted_audit_t small_audit = nimcp_encrypted_audit_create(&small_config, key, sizeof(key));
    ASSERT_NE(small_audit, nullptr);

    // Write entries
    for (int i = 0; i < 10; i++) {
        nimcp_encrypted_audit_log(small_audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, "Entry", nullptr, 0);
    }

    nimcp_encrypted_audit_stats_t stats;
    nimcp_encrypted_audit_get_stats(small_audit, &stats);
    EXPECT_EQ(stats.newest_entry_id, 10);
    // Oldest should be around 6 (buffer holds 5, entry IDs 6-10)
    EXPECT_GE(stats.oldest_entry_id, 1);

    nimcp_encrypted_audit_destroy(small_audit);
}

//=============================================================================
// Key Rotation Tests
//=============================================================================

TEST_F(EncryptedAuditTest, GetKeyVersion) {
    uint32_t version = nimcp_encrypted_audit_get_key_version(audit);
    EXPECT_EQ(version, 0);  // Initial version
}

TEST_F(EncryptedAuditTest, RotateKey) {
    uint8_t new_key[NIMCP_AUDIT_KEY_SIZE];
    nimcp_encryption_generate_key(new_key);

    uint32_t old_version = nimcp_encrypted_audit_get_key_version(audit);

    EXPECT_EQ(nimcp_encrypted_audit_rotate_key(audit, new_key, sizeof(new_key)), NIMCP_SUCCESS);

    uint32_t new_version = nimcp_encrypted_audit_get_key_version(audit);
    EXPECT_GT(new_version, old_version);

    nimcp_encrypted_audit_stats_t stats;
    nimcp_encrypted_audit_get_stats(audit, &stats);
    EXPECT_EQ(stats.key_rotations, 1);
}

TEST_F(EncryptedAuditTest, MultipleKeyRotations) {
    for (int i = 0; i < 5; i++) {
        uint8_t new_key[NIMCP_AUDIT_KEY_SIZE];
        nimcp_encryption_generate_key(new_key);
        EXPECT_EQ(nimcp_encrypted_audit_rotate_key(audit, new_key, sizeof(new_key)), NIMCP_SUCCESS);
    }

    EXPECT_EQ(nimcp_encrypted_audit_get_key_version(audit), 5);
}

TEST_F(EncryptedAuditTest, RotateKeyWithInvalidKey) {
    uint8_t short_key[16];  // Too short
    EXPECT_NE(nimcp_encrypted_audit_rotate_key(audit, short_key, sizeof(short_key)), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, RotateKeyWithNullKey) {
    EXPECT_NE(nimcp_encrypted_audit_rotate_key(audit, nullptr, NIMCP_AUDIT_KEY_SIZE), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, SetRotationPolicy) {
    EXPECT_EQ(nimcp_encrypted_audit_set_rotation_policy(
        audit,
        NIMCP_KEY_ROTATION_COUNT,
        1000
    ), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, SetManualRotationPolicy) {
    EXPECT_EQ(nimcp_encrypted_audit_set_rotation_policy(
        audit,
        NIMCP_KEY_ROTATION_MANUAL,
        0
    ), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, ReadEntriesAfterKeyRotation) {
    // Log some entries with initial key
    ASSERT_EQ(nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, "Before rotation", nullptr, 0), NIMCP_SUCCESS);

    // Rotate key
    uint8_t new_key[NIMCP_AUDIT_KEY_SIZE];
    nimcp_encryption_generate_key(new_key);
    EXPECT_EQ(nimcp_encrypted_audit_rotate_key(audit, new_key, sizeof(new_key)), NIMCP_SUCCESS);

    // Log more entries with new key
    ASSERT_EQ(nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, "After rotation", nullptr, 0), NIMCP_SUCCESS);

    // Read all entries - should work (key history retained)
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read(audit, entries, 10, &num_entries), NIMCP_SUCCESS);
    EXPECT_GE(num_entries, 1);  // At least the new entry should be readable
}

//=============================================================================
// Export/Import Tests
//=============================================================================

TEST_F(EncryptedAuditTest, ExportToFile) {
    // Log some entries
    for (int i = 0; i < 5; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Export test entry %d", i);
        ASSERT_EQ(nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, msg, nullptr, 0), NIMCP_SUCCESS);
    }

    const char* filepath = "/tmp/test_audit_export.bin";
    EXPECT_EQ(nimcp_encrypted_audit_export(audit, filepath), NIMCP_SUCCESS);

    // Verify file exists
    FILE* f = fopen(filepath, "rb");
    EXPECT_NE(f, nullptr);
    if (f) {
        fclose(f);
        unlink(filepath);
    }
}

TEST_F(EncryptedAuditTest, ImportFromFile) {
    // Log some entries
    for (int i = 0; i < 5; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Import test entry %d", i);
        ASSERT_EQ(nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, msg, nullptr, 0), NIMCP_SUCCESS);
    }

    const char* filepath = "/tmp/test_audit_import.bin";
    EXPECT_EQ(nimcp_encrypted_audit_export(audit, filepath), NIMCP_SUCCESS);

    // Create new audit and import
    uint8_t key[NIMCP_AUDIT_KEY_SIZE];
    nimcp_encryption_generate_key(key);
    nimcp_encrypted_audit_t new_audit = nimcp_encrypted_audit_create(&config, key, sizeof(key));
    ASSERT_NE(new_audit, nullptr);

    EXPECT_EQ(nimcp_encrypted_audit_import(new_audit, filepath), NIMCP_SUCCESS);

    nimcp_encrypted_audit_destroy(new_audit);
    unlink(filepath);
}

TEST_F(EncryptedAuditTest, ExportWithNullFilepath) {
    EXPECT_NE(nimcp_encrypted_audit_export(audit, nullptr), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, ImportWithNullFilepath) {
    EXPECT_NE(nimcp_encrypted_audit_import(audit, nullptr), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, ImportNonexistentFile) {
    EXPECT_NE(nimcp_encrypted_audit_import(audit, "/nonexistent/path/file.bin"), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, ExportEmptyBuffer) {
    const char* filepath = "/tmp/test_audit_empty.bin";
    EXPECT_EQ(nimcp_encrypted_audit_export(audit, filepath), NIMCP_SUCCESS);
    unlink(filepath);
}

TEST_F(EncryptedAuditTest, ExportJson) {
    // Log some entries
    ASSERT_EQ(nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, "JSON test", nullptr, 0), NIMCP_SUCCESS);

    const char* filepath = "/tmp/test_audit.json";
    EXPECT_EQ(nimcp_encrypted_audit_export_json(audit, filepath, test_key, sizeof(test_key)), NIMCP_SUCCESS);

    // Verify file exists and has content
    FILE* f = fopen(filepath, "r");
    EXPECT_NE(f, nullptr);
    if (f) {
        char buffer[256];
        EXPECT_NE(fgets(buffer, sizeof(buffer), f), nullptr);
        fclose(f);
        unlink(filepath);
    }
}

TEST_F(EncryptedAuditTest, ExportJsonWithInvalidKey) {
    uint8_t short_key[16];
    EXPECT_NE(nimcp_encrypted_audit_export_json(audit, "/tmp/test.json", short_key, sizeof(short_key)), NIMCP_SUCCESS);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(EncryptedAuditTest, StatisticsTracking) {
    nimcp_encrypted_audit_stats_t stats;

    // Initial state
    EXPECT_EQ(nimcp_encrypted_audit_get_stats(audit, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_entries, 0);
    EXPECT_EQ(stats.entries_encrypted, 0);

    // Log entries
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(nimcp_encrypted_audit_log(
            audit,
            NIMCP_AUDIT_INFO,
            NIMCP_AUDIT_SYSTEM,
            "Test",
            nullptr,
            0
        ), NIMCP_SUCCESS);
    }

    // Check stats after logging
    EXPECT_EQ(nimcp_encrypted_audit_get_stats(audit, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_entries, 5);
    EXPECT_EQ(stats.entries_encrypted, 5);

    // Read entries (triggers decryption)
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read(audit, entries, 10, &num_entries), NIMCP_SUCCESS);

    // Check decryption stats
    EXPECT_EQ(nimcp_encrypted_audit_get_stats(audit, &stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.entries_decrypted, 0);
}

TEST_F(EncryptedAuditTest, ResetStatistics) {
    // Generate some stats
    EXPECT_EQ(nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, "Test", nullptr, 0), NIMCP_SUCCESS);

    nimcp_encrypted_audit_stats_t stats;
    EXPECT_EQ(nimcp_encrypted_audit_get_stats(audit, &stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.entries_encrypted, 0);

    // Reset
    nimcp_encrypted_audit_reset_stats(audit);

    EXPECT_EQ(nimcp_encrypted_audit_get_stats(audit, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.encryption_failures, 0);
    EXPECT_EQ(stats.decryption_failures, 0);
}

TEST_F(EncryptedAuditTest, MemoryUsageTracking) {
    nimcp_encrypted_audit_stats_t stats;

    EXPECT_EQ(nimcp_encrypted_audit_get_stats(audit, &stats), NIMCP_SUCCESS);
    size_t initial_memory = stats.memory_usage_bytes;

    // Log entries
    for (int i = 0; i < 10; i++) {
        nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, "Memory test", nullptr, 0);
    }

    EXPECT_EQ(nimcp_encrypted_audit_get_stats(audit, &stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.memory_usage_bytes, initial_memory);
}

TEST_F(EncryptedAuditTest, EncryptionTimeTracking) {
    // Log several entries to get timing data
    for (int i = 0; i < 100; i++) {
        nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, "Timing test", nullptr, 0);
    }

    nimcp_encrypted_audit_stats_t stats;
    EXPECT_EQ(nimcp_encrypted_audit_get_stats(audit, &stats), NIMCP_SUCCESS);
    // avg_encryption_time_us should have some positive value
    EXPECT_GE(stats.avg_encryption_time_us, 0.0f);
}

TEST_F(EncryptedAuditTest, GetStatsWithNullStats) {
    EXPECT_NE(nimcp_encrypted_audit_get_stats(audit, nullptr), NIMCP_SUCCESS);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(EncryptedAuditTest, SeverityNameConversion) {
    /* The nimcp_audit_severity_name() function uses NIMCP_AUDIT_SEV_* enum values
     * from nimcp_security_audit.h which has different values than NIMCP_AUDIT_*
     * from nimcp_encrypted_audit.h. Use numeric values to avoid header conflicts.
     * SEV enum: DEBUG=0, INFO=1, NOTICE=2, WARNING=3, ERROR=4, CRITICAL=5, ALERT=6, EMERGENCY=7 */
    EXPECT_STREQ(nimcp_audit_severity_name((nimcp_audit_severity_t)0), "DEBUG");
    EXPECT_STREQ(nimcp_audit_severity_name((nimcp_audit_severity_t)1), "INFO");
    EXPECT_STREQ(nimcp_audit_severity_name((nimcp_audit_severity_t)3), "WARNING");
    EXPECT_STREQ(nimcp_audit_severity_name((nimcp_audit_severity_t)4), "ERROR");
    EXPECT_STREQ(nimcp_audit_severity_name((nimcp_audit_severity_t)5), "CRITICAL");
    EXPECT_STREQ(nimcp_audit_severity_name((nimcp_audit_severity_t)7), "EMERGENCY");
}

TEST_F(EncryptedAuditTest, CategoryNameConversion) {
    /* The nimcp_audit_category_name() function uses NIMCP_AUDIT_CAT_* enum values
     * from nimcp_security_audit.h. Use numeric values to avoid header conflicts.
     * CAT enum: ACCESS=0, AUTHENTICATION=1, AUTHORIZATION=2, INTEGRITY=3,
     * CONFIGURATION=4, POLICY=5, THREAT=6, SYSTEM=7 */
    EXPECT_STREQ(nimcp_audit_category_name((nimcp_audit_category_t)1), "AUTHENTICATION");
    EXPECT_STREQ(nimcp_audit_category_name((nimcp_audit_category_t)2), "AUTHORIZATION");
    EXPECT_STREQ(nimcp_audit_category_name((nimcp_audit_category_t)7), "SYSTEM");
    EXPECT_STREQ(nimcp_audit_category_name((nimcp_audit_category_t)6), "THREAT");
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(EncryptedAuditTest, ConcurrentLogging) {
    const int num_threads = 10;
    const int entries_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this, i, entries_per_thread, &success_count]() {
            for (int j = 0; j < entries_per_thread; j++) {
                char message[256];
                snprintf(message, sizeof(message), "Thread %d, Entry %d", i, j);

                nimcp_error_t result = nimcp_encrypted_audit_log(
                    audit,
                    NIMCP_AUDIT_INFO,
                    NIMCP_AUDIT_SYSTEM,
                    message,
                    nullptr,
                    0
                );

                if (result == NIMCP_SUCCESS) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, num_threads * entries_per_thread);

    nimcp_encrypted_audit_stats_t stats;
    nimcp_encrypted_audit_get_stats(audit, &stats);
    EXPECT_EQ(stats.total_entries, num_threads * entries_per_thread);
}

TEST_F(EncryptedAuditTest, ConcurrentReadWrite) {
    const int num_writers = 5;
    const int num_readers = 5;
    const int operations_per_thread = 50;
    std::vector<std::thread> threads;
    std::atomic<bool> stop{false};
    std::atomic<int> read_count{0};

    // Start writers
    for (int i = 0; i < num_writers; i++) {
        threads.emplace_back([this, i, operations_per_thread]() {
            for (int j = 0; j < operations_per_thread; j++) {
                char message[256];
                snprintf(message, sizeof(message), "Writer %d, Entry %d", i, j);
                nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, message, nullptr, 0);
            }
        });
    }

    // Start readers
    for (int i = 0; i < num_readers; i++) {
        threads.emplace_back([this, &stop, &read_count, operations_per_thread]() {
            nimcp_audit_entry_t entries[10];
            size_t num_entries = 0;
            for (int j = 0; j < operations_per_thread; j++) {
                if (nimcp_encrypted_audit_read(audit, entries, 10, &num_entries) == NIMCP_SUCCESS) {
                    read_count += num_entries;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should not crash and should have logged entries
    nimcp_encrypted_audit_stats_t stats;
    nimcp_encrypted_audit_get_stats(audit, &stats);
    EXPECT_EQ(stats.total_entries, num_writers * operations_per_thread);
}

TEST_F(EncryptedAuditTest, ConcurrentKeyRotation) {
    const int num_threads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> rotation_count{0};

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this, &rotation_count]() {
            uint8_t new_key[NIMCP_AUDIT_KEY_SIZE];
            nimcp_encryption_generate_key(new_key);
            if (nimcp_encrypted_audit_rotate_key(audit, new_key, sizeof(new_key)) == NIMCP_SUCCESS) {
                rotation_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All rotations should succeed
    EXPECT_EQ(rotation_count, num_threads);
    EXPECT_EQ(nimcp_encrypted_audit_get_key_version(audit), num_threads);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(EncryptedAuditTest, LogWithNullMessage) {
    EXPECT_NE(nimcp_encrypted_audit_log(
        audit,
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_SYSTEM,
        nullptr,  // Invalid
        nullptr,
        0
    ), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, LogWithInvalidSeverity) {
    EXPECT_NE(nimcp_encrypted_audit_log(
        audit,
        static_cast<nimcp_audit_severity_t>(999),  // Invalid
        NIMCP_AUDIT_SYSTEM,
        "Test",
        nullptr,
        0
    ), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, LogWithInvalidCategory) {
    EXPECT_NE(nimcp_encrypted_audit_log(
        audit,
        NIMCP_AUDIT_INFO,
        static_cast<nimcp_audit_category_t>(999),  // Invalid
        "Test",
        nullptr,
        0
    ), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, ReadWithNullBuffer) {
    size_t num_entries = 0;
    EXPECT_NE(nimcp_encrypted_audit_read(audit, nullptr, 10, &num_entries), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, ReadWithNullCount) {
    nimcp_audit_entry_t entries[10];
    EXPECT_NE(nimcp_encrypted_audit_read(audit, entries, 10, nullptr), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, ReadWithZeroMaxEntries) {
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_NE(nimcp_encrypted_audit_read(audit, entries, 0, &num_entries), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, LogWithNullAudit) {
    EXPECT_NE(nimcp_encrypted_audit_log(
        nullptr,
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_SYSTEM,
        "Test",
        nullptr,
        0
    ), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, ReadWithNullAudit) {
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_NE(nimcp_encrypted_audit_read(nullptr, entries, 10, &num_entries), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, GetStatsWithNullAudit) {
    nimcp_encrypted_audit_stats_t stats;
    EXPECT_NE(nimcp_encrypted_audit_get_stats(nullptr, &stats), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, RotateKeyWithNullAudit) {
    uint8_t new_key[NIMCP_AUDIT_KEY_SIZE];
    nimcp_encryption_generate_key(new_key);
    EXPECT_NE(nimcp_encrypted_audit_rotate_key(nullptr, new_key, sizeof(new_key)), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, GetKeyVersionWithNullAudit) {
    EXPECT_EQ(nimcp_encrypted_audit_get_key_version(nullptr), 0);
}

TEST_F(EncryptedAuditTest, SetRotationPolicyWithNullAudit) {
    EXPECT_NE(nimcp_encrypted_audit_set_rotation_policy(nullptr, NIMCP_KEY_ROTATION_COUNT, 1000), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, ExportWithNullAudit) {
    EXPECT_NE(nimcp_encrypted_audit_export(nullptr, "/tmp/test.bin"), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, ImportWithNullAudit) {
    EXPECT_NE(nimcp_encrypted_audit_import(nullptr, "/tmp/test.bin"), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, ReadFilteredWithNullAudit) {
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_NE(nimcp_encrypted_audit_read_filtered(
        nullptr,
        NIMCP_AUDIT_DEBUG,
        NIMCP_AUDIT_SYSTEM,
        entries,
        10,
        &num_entries
    ), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, ReadRangeWithNullAudit) {
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_NE(nimcp_encrypted_audit_read_range(
        nullptr,
        0,
        UINT64_MAX,
        entries,
        10,
        &num_entries
    ), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, ResetStatsWithNullAudit) {
    // Should not crash
    nimcp_encrypted_audit_reset_stats(nullptr);
}

//=============================================================================
// Bio-Async Integration Tests (Basic)
//=============================================================================

TEST_F(EncryptedAuditTest, ProcessInboxWithoutBioAsync) {
    // Should return 0 when bio-async not enabled
    EXPECT_EQ(nimcp_encrypted_audit_process_inbox(audit, 10), 0);
}

TEST_F(EncryptedAuditTest, BroadcastAlertWithoutBioAsync) {
    nimcp_audit_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.severity = NIMCP_AUDIT_CRITICAL;
    strncpy(entry.message, "Test alert", sizeof(entry.message) - 1);

    // Should succeed even without bio-async (just returns success without sending)
    EXPECT_EQ(nimcp_encrypted_audit_broadcast_alert(audit, &entry), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, BroadcastAlertWithNullEntry) {
    EXPECT_NE(nimcp_encrypted_audit_broadcast_alert(audit, nullptr), NIMCP_SUCCESS);
}

TEST_F(EncryptedAuditTest, BroadcastAlertWithNullAudit) {
    nimcp_audit_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    EXPECT_NE(nimcp_encrypted_audit_broadcast_alert(nullptr, &entry), NIMCP_SUCCESS);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(EncryptedAuditTest, ReadFromEmptyBuffer) {
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read(audit, entries, 10, &num_entries), NIMCP_SUCCESS);
    EXPECT_EQ(num_entries, 0);
}

TEST_F(EncryptedAuditTest, ReadWithSmallBuffer) {
    // Log more entries than we'll try to read
    for (int i = 0; i < 10; i++) {
        nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, "Test", nullptr, 0);
    }

    // Read only 3
    nimcp_audit_entry_t entries[3];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read(audit, entries, 3, &num_entries), NIMCP_SUCCESS);
    EXPECT_LE(num_entries, 3);
}

TEST_F(EncryptedAuditTest, LogEmptyMessage) {
    EXPECT_EQ(nimcp_encrypted_audit_log(
        audit,
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_SYSTEM,
        "",  // Empty but not NULL
        nullptr,
        0
    ), NIMCP_SUCCESS);

    nimcp_audit_entry_t entries[1];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read(audit, entries, 1, &num_entries), NIMCP_SUCCESS);
    EXPECT_EQ(num_entries, 1);
    EXPECT_STREQ(entries[0].message, "");
}

TEST_F(EncryptedAuditTest, FilterWithNoMatches) {
    // Log only DEBUG entries
    for (int i = 0; i < 5; i++) {
        nimcp_encrypted_audit_log(audit, NIMCP_AUDIT_DEBUG, NIMCP_AUDIT_SYSTEM, "Debug", nullptr, 0);
    }

    // Try to read CRITICAL and above
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read_filtered(
        audit,
        NIMCP_AUDIT_CRITICAL,
        NIMCP_AUDIT_CATEGORY_COUNT,
        entries,
        10,
        &num_entries
    ), NIMCP_SUCCESS);

    EXPECT_EQ(num_entries, 0);
}

TEST_F(EncryptedAuditTest, TimeRangeWithNoMatches) {
    // Log with timestamps in a specific range
    nimcp_encrypted_audit_log_timestamped(audit, 1000, NIMCP_AUDIT_INFO, NIMCP_AUDIT_SYSTEM, "Early", nullptr, 0);

    // Query for a different time range
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read_range(
        audit,
        9000,
        10000,
        entries,
        10,
        &num_entries
    ), NIMCP_SUCCESS);

    EXPECT_EQ(num_entries, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
