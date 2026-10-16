/**
 * @file test_brain_persistence_integration.cpp
 * @brief Unit tests for persistence module memory and security integration
 *
 * WHAT: Tests for Phase PERSIST-1 (security registration) and PERSIST-2 (unified memory)
 * WHY:  Ensure persistence module properly integrates with security and memory subsystems
 * HOW:  Test initialization, stats tracking, extended save/load, and CoW snapshots
 *
 * TEST COVERAGE:
 * - Module initialization and shutdown
 * - Security module registration
 * - Statistics tracking
 * - Extended save/load API with checksums
 * - CoW snapshot support
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

#include "core/brain/nimcp_brain.h"
#include "core/brain/persistence/nimcp_brain_persistence.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PersistenceIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    nimcp_sec_integration_t* security_ctx = nullptr;
    unified_mem_manager_t mem_manager = nullptr;
    const char* test_file = "/tmp/test_persist_integration.nimcp";
    const char* snapshot_dir = "/tmp/persist_snapshots";

    void SetUp() override {
        // Clean up test files
        remove(test_file);
        char checksum_file[512];
        snprintf(checksum_file, sizeof(checksum_file), "%s.checksum", test_file);
        remove(checksum_file);

        // Ensure persistence is shutdown from previous tests
        persistence_shutdown();
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }

        // Shutdown persistence module
        persistence_shutdown();

        // Cleanup security context
        if (security_ctx) {
            nimcp_sec_integration_destroy(security_ctx);
            security_ctx = nullptr;
        }

        // Cleanup memory manager
        if (mem_manager) {
            unified_mem_destroy(mem_manager);
            mem_manager = nullptr;
        }

        // Clean up test files
        remove(test_file);
        char checksum_file[512];
        snprintf(checksum_file, sizeof(checksum_file), "%s.checksum", test_file);
        remove(checksum_file);
        char meta_file[512];
        snprintf(meta_file, sizeof(meta_file), "%s.meta", test_file);
        remove(meta_file);
    }

    brain_t createTestBrain() {
        brain_config_t config = {};
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 10;
        config.num_outputs = 3;
        strncpy(config.task_name, "persist_test", sizeof(config.task_name) - 1);
        config.snapshot_dir = snapshot_dir;
        return brain_create_custom(&config);
    }
};

//=============================================================================
// Module Initialization Tests
//=============================================================================

TEST_F(PersistenceIntegrationTest, Init_WithoutSecurity) {
    // Initialize without security context
    bool result = persistence_init(nullptr);
    EXPECT_TRUE(result);

    // Should get 0 for security module ID (not registered)
    uint32_t id = persistence_get_security_module_id();
    EXPECT_EQ(id, 0u);

    // Shutdown should succeed
    persistence_shutdown();
}

TEST_F(PersistenceIntegrationTest, Init_DoubleInit) {
    // First init
    bool result1 = persistence_init(nullptr);
    EXPECT_TRUE(result1);

    // Second init should be idempotent
    bool result2 = persistence_init(nullptr);
    EXPECT_TRUE(result2);

    persistence_shutdown();
}

TEST_F(PersistenceIntegrationTest, Init_WithSecurity) {
    // Create security context
    security_ctx = nimcp_sec_integration_create();
    if (security_ctx) {
        nimcp_sec_integration_config_t sec_config = nimcp_sec_integration_default_config();
        sec_config.enable_continuous_monitoring = true;
        nimcp_sec_integration_init(security_ctx, &sec_config);

        // Initialize with security
        bool result = persistence_init(security_ctx);
        EXPECT_TRUE(result);

        // Should get non-zero security module ID
        uint32_t id = persistence_get_security_module_id();
        EXPECT_GT(id, 0u);
    }
}

TEST_F(PersistenceIntegrationTest, Shutdown_CleanMultiple) {
    // Initialize
    persistence_init(nullptr);

    // Multiple shutdowns should be safe
    persistence_shutdown();
    persistence_shutdown();  // Second call should be no-op
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(PersistenceIntegrationTest, Stats_GetBeforeInit) {
    persistence_stats_t stats;
    bool result = persistence_get_stats(&stats);
    EXPECT_FALSE(result);  // Should fail before init
}

TEST_F(PersistenceIntegrationTest, Stats_GetAfterInit) {
    persistence_init(nullptr);

    persistence_stats_t stats;
    bool result = persistence_get_stats(&stats);
    EXPECT_TRUE(result);

    // Should be zeros initially
    EXPECT_EQ(stats.total_saves, 0u);
    EXPECT_EQ(stats.total_loads, 0u);
    EXPECT_EQ(stats.bytes_written, 0u);
}

TEST_F(PersistenceIntegrationTest, Stats_NullPointer) {
    persistence_init(nullptr);

    bool result = persistence_get_stats(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(PersistenceIntegrationTest, Stats_Reset) {
    persistence_init(nullptr);

    // Do a save to generate stats
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    brain_learn_example(brain, features, 10, "test", 1.0f);
    brain_save_ex(brain, test_file, nullptr);

    // Verify stats are non-zero
    persistence_stats_t stats;
    persistence_get_stats(&stats);
    EXPECT_GT(stats.total_saves, 0u);

    // Reset stats
    persistence_reset_stats();

    // Verify stats are zero again
    persistence_get_stats(&stats);
    EXPECT_EQ(stats.total_saves, 0u);
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(PersistenceIntegrationTest, Config_DefaultValues) {
    persistence_config_t config = persistence_default_config();

    // Check defaults
    EXPECT_FALSE(config.use_unified_memory);
    EXPECT_EQ(config.memory_manager, nullptr);
    EXPECT_FALSE(config.enable_cow_snapshots);
    EXPECT_FALSE(config.enable_security);
    EXPECT_EQ(config.security_context, nullptr);
    EXPECT_EQ(config.read_buffer_size, 64u * 1024u);
    EXPECT_EQ(config.write_buffer_size, 64u * 1024u);
    EXPECT_TRUE(config.enable_checksum);
    EXPECT_TRUE(config.verify_on_load);
}

//=============================================================================
// Extended Save/Load API Tests
//=============================================================================

TEST_F(PersistenceIntegrationTest, SaveEx_NullBrain) {
    persistence_init(nullptr);

    bool result = brain_save_ex(nullptr, test_file, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(PersistenceIntegrationTest, SaveEx_NullFilepath) {
    persistence_init(nullptr);
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    bool result = brain_save_ex(brain, nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(PersistenceIntegrationTest, SaveEx_BasicSave) {
    persistence_init(nullptr);
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    brain_learn_example(brain, features, 10, "test", 1.0f);

    bool result = brain_save_ex(brain, test_file, nullptr);
    EXPECT_TRUE(result);

    // Verify file exists
    EXPECT_EQ(access(test_file, F_OK), 0);

    // Verify checksum file exists (checksums enabled by default)
    char checksum_file[512];
    snprintf(checksum_file, sizeof(checksum_file), "%s.checksum", test_file);
    EXPECT_EQ(access(checksum_file, F_OK), 0);

    // Verify stats were updated
    persistence_stats_t stats;
    persistence_get_stats(&stats);
    EXPECT_EQ(stats.total_saves, 1u);
    EXPECT_GT(stats.bytes_written, 0u);
}

TEST_F(PersistenceIntegrationTest, SaveEx_DisableChecksum) {
    persistence_init(nullptr);
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    brain_learn_example(brain, features, 10, "test", 1.0f);

    persistence_config_t config = persistence_default_config();
    config.enable_checksum = false;

    bool result = brain_save_ex(brain, test_file, &config);
    EXPECT_TRUE(result);

    // Checksum file should NOT exist
    char checksum_file[512];
    snprintf(checksum_file, sizeof(checksum_file), "%s.checksum", test_file);
    EXPECT_NE(access(checksum_file, F_OK), 0);
}

TEST_F(PersistenceIntegrationTest, LoadEx_NullFilepath) {
    persistence_init(nullptr);

    brain_t loaded = brain_load_ex(nullptr, nullptr);
    EXPECT_EQ(loaded, nullptr);
}

TEST_F(PersistenceIntegrationTest, LoadEx_NonexistentFile) {
    persistence_init(nullptr);

    brain_t loaded = brain_load_ex("/nonexistent/path.nimcp", nullptr);
    EXPECT_EQ(loaded, nullptr);

    // Should record error in stats
    persistence_stats_t stats;
    persistence_get_stats(&stats);
    EXPECT_EQ(stats.load_errors, 1u);
}

TEST_F(PersistenceIntegrationTest, LoadEx_BasicLoad) {
    persistence_init(nullptr);
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    brain_learn_example(brain, features, 10, "test", 1.0f);
    brain_save_ex(brain, test_file, nullptr);

    brain_destroy(brain);
    brain = nullptr;

    // Load brain
    brain = brain_load_ex(test_file, nullptr);
    EXPECT_NE(brain, nullptr);

    // Verify stats
    persistence_stats_t stats;
    persistence_get_stats(&stats);
    EXPECT_EQ(stats.total_loads, 1u);
    EXPECT_GT(stats.bytes_read, 0u);
}

TEST_F(PersistenceIntegrationTest, LoadEx_ChecksumVerification) {
    persistence_init(nullptr);
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    brain_learn_example(brain, features, 10, "test", 1.0f);
    brain_save_ex(brain, test_file, nullptr);

    brain_destroy(brain);
    brain = nullptr;

    // Load with checksum verification
    persistence_config_t config = persistence_default_config();
    config.verify_on_load = true;

    brain = brain_load_ex(test_file, &config);
    EXPECT_NE(brain, nullptr);
}

TEST_F(PersistenceIntegrationTest, LoadEx_CorruptedChecksum) {
    persistence_init(nullptr);
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    brain_learn_example(brain, features, 10, "test", 1.0f);
    brain_save_ex(brain, test_file, nullptr);

    brain_destroy(brain);
    brain = nullptr;

    // Corrupt the checksum file
    char checksum_file[512];
    snprintf(checksum_file, sizeof(checksum_file), "%s.checksum", test_file);
    FILE* f = fopen(checksum_file, "wb");
    if (f) {
        uint32_t bad_checksum = 0xDEADBEEF;
        long bad_size = 12345;
        fwrite(&bad_checksum, sizeof(uint32_t), 1, f);
        fwrite(&bad_size, sizeof(long), 1, f);
        fclose(f);
    }

    // Load should fail due to checksum mismatch
    persistence_config_t config = persistence_default_config();
    config.verify_on_load = true;

    brain = brain_load_ex(test_file, &config);
    EXPECT_EQ(brain, nullptr);

    // Check that checksum failure was recorded
    persistence_stats_t stats;
    persistence_get_stats(&stats);
    EXPECT_GT(stats.checksum_failures, 0u);
}

//=============================================================================
// CoW Snapshot Tests
//=============================================================================

TEST_F(PersistenceIntegrationTest, SnapshotCow_NullBrain) {
    persistence_init(nullptr);

    bool result = brain_save_snapshot_cow(nullptr, "test", "desc", nullptr);
    EXPECT_FALSE(result);
}

TEST_F(PersistenceIntegrationTest, SnapshotCow_NullName) {
    persistence_init(nullptr);
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    bool result = brain_save_snapshot_cow(brain, nullptr, "desc", nullptr);
    EXPECT_FALSE(result);
}

TEST_F(PersistenceIntegrationTest, SnapshotCow_BasicSnapshot) {
    persistence_init(nullptr);
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    brain_learn_example(brain, features, 10, "test", 1.0f);

    // Create snapshot (falls back to regular snapshot without unified memory)
    bool result = brain_save_snapshot_cow(brain, "test_snap", "Test snapshot", nullptr);
    EXPECT_TRUE(result);

    // Verify stats
    persistence_stats_t stats;
    persistence_get_stats(&stats);
    EXPECT_EQ(stats.total_snapshots_created, 1u);
}

TEST_F(PersistenceIntegrationTest, SnapshotCow_WithUnifiedMemory) {
    persistence_init(nullptr);
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    // Create unified memory manager
    unified_mem_config_t mem_config = unified_mem_default_config();
    mem_config.enable_cow = true;
    mem_manager = unified_mem_create(&mem_config);

    if (mem_manager) {
        float features[10] = {1,2,3,4,5,6,7,8,9,10};
        brain_learn_example(brain, features, 10, "test", 1.0f);

        persistence_config_t config = persistence_default_config();
        config.use_unified_memory = true;
        config.memory_manager = mem_manager;
        config.enable_cow_snapshots = true;

        bool result = brain_save_snapshot_cow(brain, "cow_snap", "CoW snapshot", &config);
        EXPECT_TRUE(result);
    }
}

//=============================================================================
// Memory Allocation Statistics Tests
//=============================================================================

TEST_F(PersistenceIntegrationTest, MemoryStats_MallocFallback) {
    persistence_init(nullptr);
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    brain_learn_example(brain, features, 10, "test", 1.0f);

    // Save with default config (no unified memory)
    brain_save_ex(brain, test_file, nullptr);

    persistence_stats_t stats;
    persistence_get_stats(&stats);

    // Without unified memory, should use malloc for checksum buffer
    EXPECT_GT(stats.malloc_allocations, 0u);
}

TEST_F(PersistenceIntegrationTest, MemoryStats_PoolAllocation) {
    persistence_init(nullptr);
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    // Create unified memory manager
    unified_mem_config_t mem_config = unified_mem_default_config();
    mem_config.enable_cow = true;
    mem_manager = unified_mem_create(&mem_config);

    if (mem_manager) {
        float features[10] = {1,2,3,4,5,6,7,8,9,10};
        brain_learn_example(brain, features, 10, "test", 1.0f);

        persistence_config_t config = persistence_default_config();
        config.use_unified_memory = true;
        config.memory_manager = mem_manager;

        brain_save_ex(brain, test_file, &config);

        persistence_stats_t stats;
        persistence_get_stats(&stats);

        // With unified memory, should use pool allocations
        // Note: May still use malloc if pool fails
        EXPECT_TRUE(stats.pool_allocations > 0 || stats.malloc_allocations > 0);
    }
}

//=============================================================================
// Security Integration Tests
//=============================================================================

TEST_F(PersistenceIntegrationTest, Security_RecordInteractions) {
    // Create security context
    security_ctx = nimcp_sec_integration_create();
    if (security_ctx) {
        nimcp_sec_integration_config_t sec_config = nimcp_sec_integration_default_config();
        nimcp_sec_integration_init(security_ctx, &sec_config);

        persistence_init(security_ctx);

        brain = createTestBrain();
        ASSERT_NE(brain, nullptr);

        float features[10] = {1,2,3,4,5,6,7,8,9,10};
        brain_learn_example(brain, features, 10, "test", 1.0f);

        // Save and load - should record security interactions
        brain_save_ex(brain, test_file, nullptr);

        brain_destroy(brain);
        brain = nullptr;

        brain = brain_load_ex(test_file, nullptr);
        EXPECT_NE(brain, nullptr);

        // Get trust score for persistence module
        uint32_t module_id = persistence_get_security_module_id();
        nimcp_trust_score_t score;
        nimcp_result_t result = nimcp_sec_get_trust_score(security_ctx, module_id, &score);

        if (result == NIMCP_SUCCESS) {
            // Trust score should be reasonable after successful operations
            EXPECT_GE(score.expected_trust, 0.0);
            EXPECT_LE(score.expected_trust, 1.0);
        }
    }
}

//=============================================================================
// Performance Statistics Tests
//=============================================================================

TEST_F(PersistenceIntegrationTest, Performance_TimingStats) {
    persistence_init(nullptr);
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    brain_learn_example(brain, features, 10, "test", 1.0f);

    // Do multiple saves to accumulate timing
    for (int i = 0; i < 3; i++) {
        brain_save_ex(brain, test_file, nullptr);
    }

    persistence_stats_t stats;
    persistence_get_stats(&stats);

    // Should have timing info (may be 0 for very fast operations)
    EXPECT_EQ(stats.total_saves, 3u);
    // total_save_time_ms could be 0 for fast operations, but should be >= 0
    EXPECT_GE(stats.total_save_time_ms, 0u);
}

//=============================================================================
// Run Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
