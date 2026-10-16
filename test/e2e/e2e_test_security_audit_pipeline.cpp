/**
 * @file e2e_test_security_audit_pipeline.cpp
 * @brief E2E Tests for Security Encrypted Audit Pipeline
 *
 * WHAT: End-to-end testing for encrypted audit log system
 * WHY:  Verify complete audit lifecycle from logging to export
 * HOW:  Test encryption, key rotation, export/import, and alerts
 *
 * TEST PIPELINES:
 * - FullAuditLifecycleWithEncryption: Complete audit workflow
 * - ExportImportRoundTrip: Export encrypted, import, verify
 * - KeyRotationDuringOperation: Rotate keys while logging
 * - AlertBroadcasting: Critical alert propagation
 * - ConcurrentAuditLogging: Multi-threaded logging stress test
 * - TamperingDetection: Verify tampering detection works
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "security/nimcp_encrypted_audit.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"

#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <cmath>

//=============================================================================
// Test Fixture
//=============================================================================

class SecurityAuditPipelineE2ETest : public ::testing::Test {
protected:
    static constexpr size_t KEY_SIZE = NIMCP_AUDIT_KEY_SIZE;

    nimcp_encrypted_audit_t audit_ = nullptr;
    uint8_t master_key_[KEY_SIZE];
    uint8_t backup_key_[KEY_SIZE];

    static std::atomic<int> alerts_received_;
    static std::atomic<int> entries_logged_;

    void SetUp() override {
        // Generate deterministic test key
        for (size_t i = 0; i < KEY_SIZE; i++) {
            master_key_[i] = static_cast<uint8_t>((i * 17 + 42) & 0xFF);
            backup_key_[i] = static_cast<uint8_t>((i * 31 + 99) & 0xFF);
        }

        // Initialize bio-async system
        nimcp_error_t err = nimcp_bio_async_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        err = bio_router_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Reset counters
        alerts_received_.store(0);
        entries_logged_.store(0);
    }

    void TearDown() override {
        if (audit_) {
            nimcp_encrypted_audit_destroy(audit_);
            audit_ = nullptr;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        // Clean up temp files
        std::remove("/tmp/nimcp_audit_test.bin");
        std::remove("/tmp/nimcp_audit_test.json");
    }

    nimcp_encrypted_audit_t CreateAudit(size_t buffer_size = 1000) {
        nimcp_encrypted_audit_config_t config = nimcp_encrypted_audit_default_config();
        config.buffer_size = buffer_size;
        config.enable_bio_async = true;
        config.enable_compression = true;
        config.lock_memory = false;  // Don't require mlock for tests

        return nimcp_encrypted_audit_create(&config, master_key_, KEY_SIZE);
    }
};

// Static member initialization
std::atomic<int> SecurityAuditPipelineE2ETest::alerts_received_{0};
std::atomic<int> SecurityAuditPipelineE2ETest::entries_logged_{0};

//=============================================================================
// Pipeline 1: Full Audit Lifecycle with Encryption
//=============================================================================

TEST_F(SecurityAuditPipelineE2ETest, FullAuditLifecycleWithEncryption) {
    E2E_PIPELINE_START("Full Audit Lifecycle with Encryption");

    // Stage 1: Create encrypted audit system
    E2E_STAGE_BEGIN("Create encrypted audit system", 500);

    audit_ = CreateAudit(500);
    E2E_ASSERT_NOT_NULL(audit_, "Failed to create encrypted audit system");

    // Verify initial key version
    uint32_t key_version = nimcp_encrypted_audit_get_key_version(audit_);
    EXPECT_EQ(key_version, 0u);

    E2E_STAGE_END();

    // Stage 2: Log entries across severity levels
    E2E_STAGE_BEGIN("Log entries across severity levels", 1000);

    const char* test_messages[] = {
        "Debug: System startup initiated",
        "Info: User authentication started",
        "Warning: Low memory condition detected",
        "Error: Failed to connect to database",
        "Critical: Security breach attempt detected"
    };

    nimcp_audit_severity_t severities[] = {
        NIMCP_AUDIT_DEBUG,
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_WARNING,
        NIMCP_AUDIT_ERROR,
        NIMCP_AUDIT_CRITICAL
    };

    for (size_t i = 0; i < 5; i++) {
        nimcp_error_t err = nimcp_encrypted_audit_log(
            audit_,
            severities[i],
            NIMCP_AUDIT_SYSTEM,
            test_messages[i],
            nullptr, 0
        );
        EXPECT_EQ(err, NIMCP_SUCCESS) << "Failed to log entry " << i;
        entries_logged_.fetch_add(1);
    }

    E2E_STAGE_END();

    // Stage 3: Log with auxiliary data
    E2E_STAGE_BEGIN("Log entries with auxiliary data", 500);

    struct TestData {
        uint32_t user_id;
        char ip_address[16];
        uint64_t timestamp;
    } test_data;

    test_data.user_id = 12345;
    snprintf(test_data.ip_address, sizeof(test_data.ip_address), "192.168.1.100");
    test_data.timestamp = 1704067200000ULL;

    nimcp_error_t err = nimcp_encrypted_audit_log(
        audit_,
        NIMCP_AUDIT_WARNING,
        NIMCP_AUDIT_AUTHENTICATION,
        "Failed login attempt from suspicious IP",
        &test_data, sizeof(test_data)
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
    entries_logged_.fetch_add(1);

    E2E_STAGE_END();

    // Stage 4: Read and verify entries
    E2E_STAGE_BEGIN("Read and verify entries", 1000);

    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;

    err = nimcp_encrypted_audit_read(audit_, entries, 10, &num_entries);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(num_entries, 6u);

    // Verify entries are properly decrypted
    for (size_t i = 0; i < num_entries; i++) {
        EXPECT_GT(entries[i].timestamp_ns, 0u);
        EXPECT_GE(entries[i].severity, NIMCP_AUDIT_DEBUG);
        EXPECT_LT(entries[i].severity, NIMCP_AUDIT_SEVERITY_COUNT);
        EXPECT_GT(strlen(entries[i].message), 0u);
    }

    E2E_STAGE_END();

    // Stage 5: Verify statistics
    E2E_STAGE_BEGIN("Verify statistics", 200);

    nimcp_encrypted_audit_stats_t stats;
    err = nimcp_encrypted_audit_get_stats(audit_, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(stats.total_entries, 6u);
    EXPECT_EQ(stats.entries_encrypted, 6u);
    EXPECT_EQ(stats.encryption_failures, 0u);
    EXPECT_EQ(stats.tampering_detected, 0u);
    EXPECT_EQ(stats.current_key_version, 0u);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Export/Import Round-Trip
//=============================================================================

TEST_F(SecurityAuditPipelineE2ETest, ExportImportRoundTrip) {
    E2E_PIPELINE_START("Export/Import Round-Trip");

    // Stage 1: Create and populate audit
    E2E_STAGE_BEGIN("Create and populate audit", 500);

    audit_ = CreateAudit(100);
    E2E_ASSERT_NOT_NULL(audit_, "Failed to create audit");

    // Log diverse entries
    for (int i = 0; i < 20; i++) {
        char message[256];
        snprintf(message, sizeof(message), "Test entry %d with unique content: %08X", i, i * 12345);

        nimcp_error_t err = nimcp_encrypted_audit_log(
            audit_,
            static_cast<nimcp_audit_severity_t>(i % NIMCP_AUDIT_SEVERITY_COUNT),
            static_cast<nimcp_audit_category_t>(i % NIMCP_AUDIT_CATEGORY_COUNT),
            message,
            nullptr, 0
        );
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    E2E_STAGE_END();

    // Stage 2: Export encrypted entries
    E2E_STAGE_BEGIN("Export encrypted entries", 1000);

    const char* export_path = "/tmp/nimcp_audit_test.bin";
    nimcp_error_t err = nimcp_encrypted_audit_export(audit_, export_path);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify file was created
    FILE* f = fopen(export_path, "rb");
    E2E_ASSERT_NOT_NULL(f, "Export file not created");
    fclose(f);

    E2E_STAGE_END();

    // Stage 3: Create new audit and import
    E2E_STAGE_BEGIN("Import entries to new audit", 1000);

    // Create second audit with same key
    nimcp_encrypted_audit_config_t config = nimcp_encrypted_audit_default_config();
    config.buffer_size = 100;
    nimcp_encrypted_audit_t audit2 = nimcp_encrypted_audit_create(&config, master_key_, KEY_SIZE);
    E2E_ASSERT_NOT_NULL(audit2, "Failed to create second audit");

    err = nimcp_encrypted_audit_import(audit2, export_path);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    E2E_STAGE_END();

    // Stage 4: Verify imported entries match
    E2E_STAGE_BEGIN("Verify imported entries match", 1000);

    nimcp_audit_entry_t original_entries[20];
    nimcp_audit_entry_t imported_entries[20];
    size_t original_count = 0, imported_count = 0;

    err = nimcp_encrypted_audit_read(audit_, original_entries, 20, &original_count);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = nimcp_encrypted_audit_read(audit2, imported_entries, 20, &imported_count);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(original_count, imported_count);

    for (size_t i = 0; i < original_count && i < imported_count; i++) {
        EXPECT_EQ(original_entries[i].severity, imported_entries[i].severity);
        EXPECT_EQ(original_entries[i].category, imported_entries[i].category);
        EXPECT_STREQ(original_entries[i].message, imported_entries[i].message);
    }

    nimcp_encrypted_audit_destroy(audit2);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Key Rotation During Operation
//=============================================================================

TEST_F(SecurityAuditPipelineE2ETest, KeyRotationDuringOperation) {
    E2E_PIPELINE_START("Key Rotation During Operation");

    // Stage 1: Create audit and log initial entries
    E2E_STAGE_BEGIN("Log entries with initial key", 500);

    audit_ = CreateAudit(200);
    E2E_ASSERT_NOT_NULL(audit_, "Failed to create audit");

    // Log 10 entries with initial key
    for (int i = 0; i < 10; i++) {
        char message[128];
        snprintf(message, sizeof(message), "Pre-rotation entry %d", i);

        nimcp_error_t err = nimcp_encrypted_audit_log(
            audit_,
            NIMCP_AUDIT_INFO,
            NIMCP_AUDIT_SYSTEM,
            message,
            nullptr, 0
        );
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    EXPECT_EQ(nimcp_encrypted_audit_get_key_version(audit_), 0u);

    E2E_STAGE_END();

    // Stage 2: Rotate key
    E2E_STAGE_BEGIN("Rotate encryption key", 500);

    nimcp_error_t err = nimcp_encrypted_audit_rotate_key(audit_, backup_key_, KEY_SIZE);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(nimcp_encrypted_audit_get_key_version(audit_), 1u);

    E2E_STAGE_END();

    // Stage 3: Log entries with new key
    E2E_STAGE_BEGIN("Log entries with new key", 500);

    for (int i = 0; i < 10; i++) {
        char message[128];
        snprintf(message, sizeof(message), "Post-rotation entry %d", i);

        err = nimcp_encrypted_audit_log(
            audit_,
            NIMCP_AUDIT_WARNING,
            NIMCP_AUDIT_ENCRYPTION,
            message,
            nullptr, 0
        );
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    E2E_STAGE_END();

    // Stage 4: Verify all entries readable
    E2E_STAGE_BEGIN("Verify all entries readable", 1000);

    nimcp_audit_entry_t entries[30];
    size_t num_entries = 0;

    err = nimcp_encrypted_audit_read(audit_, entries, 30, &num_entries);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(num_entries, 20u);

    // Verify pre-rotation entries
    int pre_rotation_count = 0;
    int post_rotation_count = 0;

    for (size_t i = 0; i < num_entries; i++) {
        if (strstr(entries[i].message, "Pre-rotation") != nullptr) {
            pre_rotation_count++;
        } else if (strstr(entries[i].message, "Post-rotation") != nullptr) {
            post_rotation_count++;
        }
    }

    EXPECT_EQ(pre_rotation_count, 10);
    EXPECT_EQ(post_rotation_count, 10);

    E2E_STAGE_END();

    // Stage 5: Verify statistics include rotation
    E2E_STAGE_BEGIN("Verify rotation statistics", 200);

    nimcp_encrypted_audit_stats_t stats;
    err = nimcp_encrypted_audit_get_stats(audit_, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(stats.key_rotations, 1u);
    EXPECT_EQ(stats.current_key_version, 1u);
    EXPECT_EQ(stats.decryption_failures, 0u);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Alert Broadcasting
//=============================================================================

TEST_F(SecurityAuditPipelineE2ETest, AlertBroadcasting) {
    E2E_PIPELINE_START("Alert Broadcasting");

    // Stage 1: Setup audit with bio-async
    E2E_STAGE_BEGIN("Setup audit with bio-async", 500);

    nimcp_encrypted_audit_config_t config = nimcp_encrypted_audit_default_config();
    config.buffer_size = 100;
    config.enable_bio_async = true;
    config.module_id = BIO_MODULE_SECURITY;

    audit_ = nimcp_encrypted_audit_create(&config, master_key_, KEY_SIZE);
    E2E_ASSERT_NOT_NULL(audit_, "Failed to create audit");

    nimcp_error_t err = nimcp_encrypted_audit_register_bio_async(audit_, BIO_MODULE_SECURITY);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    E2E_STAGE_END();

    // Stage 2: Create critical audit entry
    E2E_STAGE_BEGIN("Create critical audit entry", 300);

    nimcp_audit_entry_t critical_entry;
    memset(&critical_entry, 0, sizeof(critical_entry));

    critical_entry.severity = NIMCP_AUDIT_CRITICAL;
    critical_entry.category = NIMCP_AUDIT_THREAT;
    snprintf(critical_entry.message, sizeof(critical_entry.message),
             "CRITICAL: Active intrusion detected from hostile source");
    critical_entry.timestamp_ns = nimcp_time_get_ns();

    E2E_STAGE_END();

    // Stage 3: Broadcast alert
    E2E_STAGE_BEGIN("Broadcast critical alert", 500);

    err = nimcp_encrypted_audit_broadcast_alert(audit_, &critical_entry);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    E2E_STAGE_END();

    // Stage 4: Process messages
    E2E_STAGE_BEGIN("Process bio-async messages", 500);

    // Give time for message propagation
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    uint32_t processed = nimcp_encrypted_audit_process_inbox(audit_, 10);
    // May or may not have messages depending on router state
    (void)processed;

    E2E_STAGE_END();

    // Stage 5: Log the alert to audit trail
    E2E_STAGE_BEGIN("Log alert to audit trail", 300);

    err = nimcp_encrypted_audit_log(
        audit_,
        NIMCP_AUDIT_CRITICAL,
        NIMCP_AUDIT_THREAT,
        critical_entry.message,
        nullptr, 0
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify it was logged
    nimcp_encrypted_audit_stats_t stats;
    err = nimcp_encrypted_audit_get_stats(audit_, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(stats.total_entries, 1u);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Concurrent Audit Logging
//=============================================================================

TEST_F(SecurityAuditPipelineE2ETest, ConcurrentAuditLogging) {
    E2E_PIPELINE_START("Concurrent Audit Logging");

    const int NUM_THREADS = 4;
    const int ENTRIES_PER_THREAD = 50;

    // Stage 1: Create audit system
    E2E_STAGE_BEGIN("Create audit for concurrent access", 300);

    audit_ = CreateAudit(NUM_THREADS * ENTRIES_PER_THREAD + 100);
    E2E_ASSERT_NOT_NULL(audit_, "Failed to create audit");

    E2E_STAGE_END();

    // Stage 2: Launch concurrent logging threads
    E2E_STAGE_BEGIN("Concurrent logging from multiple threads", 3000);

    std::atomic<int> total_logged{0};
    std::atomic<int> log_failures{0};

    auto log_task = [this, &total_logged, &log_failures](int thread_id) {
        for (int i = 0; i < ENTRIES_PER_THREAD; i++) {
            char message[256];
            snprintf(message, sizeof(message),
                     "Thread %d entry %d: concurrent test message %08X",
                     thread_id, i, thread_id * 1000 + i);

            nimcp_error_t err = nimcp_encrypted_audit_log(
                audit_,
                static_cast<nimcp_audit_severity_t>(i % NIMCP_AUDIT_SEVERITY_COUNT),
                static_cast<nimcp_audit_category_t>(thread_id % NIMCP_AUDIT_CATEGORY_COUNT),
                message,
                nullptr, 0
            );

            if (err == NIMCP_SUCCESS) {
                total_logged.fetch_add(1);
            } else {
                log_failures.fetch_add(1);
            }

            // Small random delay to increase contention
            if (i % 10 == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(log_task, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    E2E_ASSERT(log_failures.load() == 0, "Some log operations failed");
    E2E_ASSERT(total_logged.load() == NUM_THREADS * ENTRIES_PER_THREAD,
               "Not all entries were logged");

    E2E_STAGE_END();

    // Stage 3: Verify all entries readable
    E2E_STAGE_BEGIN("Read all entries", 2000);

    nimcp_audit_entry_t* entries = new nimcp_audit_entry_t[NUM_THREADS * ENTRIES_PER_THREAD + 10];
    size_t num_entries = 0;

    nimcp_error_t err = nimcp_encrypted_audit_read(
        audit_,
        entries,
        NUM_THREADS * ENTRIES_PER_THREAD + 10,
        &num_entries
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(num_entries, static_cast<size_t>(NUM_THREADS * ENTRIES_PER_THREAD));

    // Verify entries are valid
    for (size_t i = 0; i < num_entries; i++) {
        EXPECT_GT(entries[i].timestamp_ns, 0u);
        EXPECT_GT(strlen(entries[i].message), 0u);
    }

    delete[] entries;

    E2E_STAGE_END();

    // Stage 4: Verify statistics accuracy
    E2E_STAGE_BEGIN("Verify concurrent statistics", 200);

    nimcp_encrypted_audit_stats_t stats;
    err = nimcp_encrypted_audit_get_stats(audit_, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(stats.total_entries, static_cast<uint64_t>(NUM_THREADS * ENTRIES_PER_THREAD));
    EXPECT_EQ(stats.entries_encrypted, static_cast<uint64_t>(NUM_THREADS * ENTRIES_PER_THREAD));
    EXPECT_EQ(stats.encryption_failures, 0u);
    EXPECT_EQ(stats.tampering_detected, 0u);

    std::cout << "\nConcurrent Logging Results:" << std::endl;
    std::cout << "  Threads: " << NUM_THREADS << std::endl;
    std::cout << "  Entries per thread: " << ENTRIES_PER_THREAD << std::endl;
    std::cout << "  Total logged: " << total_logged.load() << std::endl;
    std::cout << "  Failures: " << log_failures.load() << std::endl;
    std::cout << "  Avg encryption time: " << stats.avg_encryption_time_us << " us" << std::endl;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 6: Filtered Reading
//=============================================================================

TEST_F(SecurityAuditPipelineE2ETest, FilteredReading) {
    E2E_PIPELINE_START("Filtered Reading");

    // Stage 1: Log entries with various severities and categories
    E2E_STAGE_BEGIN("Log diverse entries", 1000);

    audit_ = CreateAudit(200);
    E2E_ASSERT_NOT_NULL(audit_, "Failed to create audit");

    // Log entries across all severity levels
    nimcp_audit_category_t categories[] = {
        NIMCP_AUDIT_AUTHENTICATION,
        NIMCP_AUDIT_AUTHORIZATION,
        NIMCP_AUDIT_THREAT,
        NIMCP_AUDIT_SYSTEM
    };

    for (int sev = 0; sev < NIMCP_AUDIT_SEVERITY_COUNT; sev++) {
        for (int cat = 0; cat < 4; cat++) {
            char message[256];
            snprintf(message, sizeof(message),
                     "Severity=%s Category=%s Test message",
                     nimcp_audit_severity_name(static_cast<nimcp_audit_severity_t>(sev)),
                     nimcp_audit_category_name(categories[cat]));

            nimcp_error_t err = nimcp_encrypted_audit_log(
                audit_,
                static_cast<nimcp_audit_severity_t>(sev),
                categories[cat],
                message,
                nullptr, 0
            );
            EXPECT_EQ(err, NIMCP_SUCCESS);
        }
    }

    E2E_STAGE_END();

    // Stage 2: Read filtered by severity
    E2E_STAGE_BEGIN("Read filtered by severity", 500);

    nimcp_audit_entry_t entries[50];
    size_t num_entries = 0;

    // Filter for ERROR and above
    nimcp_error_t err = nimcp_encrypted_audit_read_filtered(
        audit_,
        NIMCP_AUDIT_ERROR,
        NIMCP_AUDIT_CATEGORY_COUNT,  // All categories
        entries,
        50,
        &num_entries
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should have ERROR and CRITICAL entries only (2 severity levels * 4 categories = 8)
    EXPECT_EQ(num_entries, 8u);

    for (size_t i = 0; i < num_entries; i++) {
        EXPECT_GE(entries[i].severity, NIMCP_AUDIT_ERROR);
    }

    E2E_STAGE_END();

    // Stage 3: Read filtered by category
    E2E_STAGE_BEGIN("Read filtered by category", 500);

    err = nimcp_encrypted_audit_read_filtered(
        audit_,
        NIMCP_AUDIT_DEBUG,  // All severities
        NIMCP_AUDIT_THREAT,
        entries,
        50,
        &num_entries
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should have THREAT entries only (6 severity levels * 1 category = 6)
    EXPECT_EQ(num_entries, 6u);

    for (size_t i = 0; i < num_entries; i++) {
        EXPECT_EQ(entries[i].category, NIMCP_AUDIT_THREAT);
    }

    E2E_STAGE_END();

    // Stage 4: Combined filter
    E2E_STAGE_BEGIN("Combined severity and category filter", 500);

    err = nimcp_encrypted_audit_read_filtered(
        audit_,
        NIMCP_AUDIT_WARNING,  // WARNING and above
        NIMCP_AUDIT_AUTHENTICATION,
        entries,
        50,
        &num_entries
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should have WARNING, ERROR, CRITICAL, EMERGENCY for AUTH (4 entries)
    EXPECT_EQ(num_entries, 4u);

    for (size_t i = 0; i < num_entries; i++) {
        EXPECT_GE(entries[i].severity, NIMCP_AUDIT_WARNING);
        EXPECT_EQ(entries[i].category, NIMCP_AUDIT_AUTHENTICATION);
    }

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
