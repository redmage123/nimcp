/**
 * @file test_brain_persistence_integration.cpp
 * @brief Integration tests for persistence module with security and memory systems
 *
 * WHAT: End-to-end integration tests for persistence with external subsystems
 * WHY:  Verify persistence works correctly with security and unified memory in realistic scenarios
 * HOW:  Test multi-component workflows with real data and subsystem interactions
 *
 * TEST COVERAGE:
 * - Full save/load cycles with security tracking
 * - Checksum integrity verification
 * - Statistics accumulation across operations
 * - CoW snapshot fallback behavior
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

class PersistenceSecurityIntegration : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    nimcp_sec_integration_t* security_ctx = nullptr;
    unified_mem_manager_t mem_manager = nullptr;
    const char* test_file = "/tmp/test_persist_sec_int.nimcp";
    const char* snapshot_dir = "/tmp/persist_sec_snapshots";

    void SetUp() override {
        // Clean up test files
        cleanupFiles();

        // Shutdown persistence from previous tests
        persistence_shutdown();

        // Create security context
        security_ctx = nimcp_sec_integration_create();
        if (security_ctx) {
            nimcp_sec_integration_config_t sec_config = nimcp_sec_integration_default_config();
            sec_config.enable_continuous_monitoring = true;
            nimcp_sec_integration_init(security_ctx, &sec_config);
        }

        // Create unified memory manager
        unified_mem_config_t mem_config = unified_mem_default_config();
        mem_config.enable_cow = true;
        mem_manager = unified_mem_create(&mem_config);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }

        persistence_shutdown();

        if (security_ctx) {
            nimcp_sec_integration_destroy(security_ctx);
            security_ctx = nullptr;
        }

        if (mem_manager) {
            unified_mem_destroy(mem_manager);
            mem_manager = nullptr;
        }

        cleanupFiles();
    }

    void cleanupFiles() {
        remove(test_file);
        char path[512];
        snprintf(path, sizeof(path), "%s.checksum", test_file);
        remove(path);
        snprintf(path, sizeof(path), "%s.meta", test_file);
        remove(path);
    }

    brain_t createTestBrain() {
        brain_config_t config = {};
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 10;
        config.num_outputs = 3;
        strncpy(config.task_name, "integration_test", sizeof(config.task_name) - 1);
        config.snapshot_dir = snapshot_dir;
        return brain_create_custom(&config);
    }

    void trainBrain(brain_t b, int num_examples) {
        float features[10];
        for (int i = 0; i < num_examples; i++) {
            for (int j = 0; j < 10; j++) {
                features[j] = static_cast<float>(i * 10 + j) / 100.0f;
            }
            const char* label = (i % 3 == 0) ? "class_a" :
                               (i % 3 == 1) ? "class_b" : "class_c";
            brain_learn_example(b, features, 10, label, 1.0f);
        }
    }
};

//=============================================================================
// Full Pipeline Integration Tests
//=============================================================================

TEST_F(PersistenceSecurityIntegration, FullPipeline_SaveLoadWithSecurity) {
    if (!security_ctx) {
        GTEST_SKIP() << "Security context not available";
    }

    // Initialize persistence with security
    ASSERT_TRUE(persistence_init(security_ctx));

    // Verify security registration
    uint32_t module_id = persistence_get_security_module_id();
    EXPECT_GT(module_id, 0u);

    // Create and train brain
    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);
    trainBrain(brain, 50);

    // Save brain
    bool save_result = brain_save_ex(brain, test_file, nullptr);
    EXPECT_TRUE(save_result);

    // Get original stats for comparison
    brain_stats_t orig_stats;
    brain_get_stats(brain, &orig_stats);

    brain_destroy(brain);
    brain = nullptr;

    // Load brain
    brain = brain_load_ex(test_file, nullptr);
    ASSERT_NE(brain, nullptr);

    // Verify loaded brain has same stats
    brain_stats_t loaded_stats;
    brain_get_stats(brain, &loaded_stats);
    EXPECT_EQ(orig_stats.total_learning_steps, loaded_stats.total_learning_steps);

    // Verify persistence stats
    persistence_stats_t persist_stats;
    persistence_get_stats(&persist_stats);
    EXPECT_EQ(persist_stats.total_saves, 1u);
    EXPECT_EQ(persist_stats.total_loads, 1u);
    EXPECT_GT(persist_stats.bytes_written, 0u);
    EXPECT_GT(persist_stats.bytes_read, 0u);
}

TEST_F(PersistenceSecurityIntegration, FullPipeline_MultipleSaveLoad) {
    if (!security_ctx) {
        GTEST_SKIP() << "Security context not available";
    }

    ASSERT_TRUE(persistence_init(security_ctx));

    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    // Multiple save/load cycles
    for (int cycle = 0; cycle < 3; cycle++) {
        trainBrain(brain, 10);
        EXPECT_TRUE(brain_save_ex(brain, test_file, nullptr));

        brain_destroy(brain);
        brain = brain_load_ex(test_file, nullptr);
        ASSERT_NE(brain, nullptr);
    }

    // Verify cumulative stats
    persistence_stats_t stats;
    persistence_get_stats(&stats);
    EXPECT_EQ(stats.total_saves, 3u);
    EXPECT_EQ(stats.total_loads, 3u);
}

//=============================================================================
// Checksum Integrity Tests
//=============================================================================

TEST_F(PersistenceSecurityIntegration, Checksum_IntegrityPreserved) {
    ASSERT_TRUE(persistence_init(security_ctx));

    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);
    trainBrain(brain, 20);

    // Save with checksums enabled
    persistence_config_t config = persistence_default_config();
    config.enable_checksum = true;

    EXPECT_TRUE(brain_save_ex(brain, test_file, &config));

    brain_destroy(brain);
    brain = nullptr;

    // Load and verify checksum
    config.verify_on_load = true;
    brain = brain_load_ex(test_file, &config);
    EXPECT_NE(brain, nullptr);

    // No checksum failures
    persistence_stats_t stats;
    persistence_get_stats(&stats);
    EXPECT_EQ(stats.checksum_failures, 0u);
}

TEST_F(PersistenceSecurityIntegration, Checksum_DetectsCorruption) {
    ASSERT_TRUE(persistence_init(security_ctx));

    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);
    trainBrain(brain, 20);

    persistence_config_t config = persistence_default_config();
    config.enable_checksum = true;

    EXPECT_TRUE(brain_save_ex(brain, test_file, &config));

    brain_destroy(brain);
    brain = nullptr;

    // Corrupt the main file
    FILE* f = fopen(test_file, "r+b");
    if (f) {
        fseek(f, 100, SEEK_SET);  // Skip to middle of file
        char garbage[] = "CORRUPTED";
        fwrite(garbage, 1, sizeof(garbage), f);
        fclose(f);
    }

    // Load should fail due to checksum mismatch
    config.verify_on_load = true;
    brain = brain_load_ex(test_file, &config);
    EXPECT_EQ(brain, nullptr);

    persistence_stats_t stats;
    persistence_get_stats(&stats);
    EXPECT_GT(stats.checksum_failures, 0u);
}

//=============================================================================
// Unified Memory Integration Tests
//=============================================================================

TEST_F(PersistenceSecurityIntegration, UnifiedMemory_SaveWithPool) {
    if (!mem_manager) {
        GTEST_SKIP() << "Unified memory manager not available";
    }

    ASSERT_TRUE(persistence_init(security_ctx));

    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);
    trainBrain(brain, 20);

    persistence_config_t config = persistence_default_config();
    config.use_unified_memory = true;
    config.memory_manager = mem_manager;
    config.enable_checksum = true;

    EXPECT_TRUE(brain_save_ex(brain, test_file, &config));

    // Check that pool allocations were attempted
    persistence_stats_t stats;
    persistence_get_stats(&stats);
    // Either pool or malloc should be used
    EXPECT_TRUE(stats.pool_allocations > 0 || stats.malloc_allocations > 0);
}

TEST_F(PersistenceSecurityIntegration, UnifiedMemory_LoadWithPool) {
    if (!mem_manager) {
        GTEST_SKIP() << "Unified memory manager not available";
    }

    ASSERT_TRUE(persistence_init(security_ctx));

    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);
    trainBrain(brain, 20);

    // Save first
    EXPECT_TRUE(brain_save_ex(brain, test_file, nullptr));

    brain_destroy(brain);
    brain = nullptr;

    // Load with unified memory
    persistence_config_t config = persistence_default_config();
    config.use_unified_memory = true;
    config.memory_manager = mem_manager;

    brain = brain_load_ex(test_file, &config);
    EXPECT_NE(brain, nullptr);
}

//=============================================================================
// CoW Snapshot Integration Tests
//=============================================================================

TEST_F(PersistenceSecurityIntegration, CowSnapshot_FallbackBehavior) {
    if (!mem_manager) {
        GTEST_SKIP() << "Unified memory manager not available";
    }

    ASSERT_TRUE(persistence_init(security_ctx));

    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);
    trainBrain(brain, 30);

    persistence_config_t config = persistence_default_config();
    config.use_unified_memory = true;
    config.memory_manager = mem_manager;
    config.enable_cow_snapshots = true;

    // Create CoW snapshot - should fall back to regular snapshot
    EXPECT_TRUE(brain_save_snapshot_cow(brain, "cow_test", "Test CoW snapshot", &config));

    persistence_stats_t stats;
    persistence_get_stats(&stats);
    EXPECT_EQ(stats.total_snapshots_created, 1u);
}

TEST_F(PersistenceSecurityIntegration, CowSnapshot_WithoutUnifiedMemory) {
    ASSERT_TRUE(persistence_init(security_ctx));

    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);
    trainBrain(brain, 30);

    // Create snapshot without unified memory
    EXPECT_TRUE(brain_save_snapshot_cow(brain, "fallback_test", "Fallback test", nullptr));

    persistence_stats_t stats;
    persistence_get_stats(&stats);
    EXPECT_EQ(stats.total_snapshots_created, 1u);
    // No CoW snapshots since no unified memory
    EXPECT_EQ(stats.cow_snapshots, 0u);
}

//=============================================================================
// Security Trust Tracking Tests
//=============================================================================

TEST_F(PersistenceSecurityIntegration, TrustTracking_SuccessfulOps) {
    if (!security_ctx) {
        GTEST_SKIP() << "Security context not available";
    }

    ASSERT_TRUE(persistence_init(security_ctx));

    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);
    trainBrain(brain, 20);

    // Multiple successful operations
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(brain_save_ex(brain, test_file, nullptr));
        brain_destroy(brain);
        brain = brain_load_ex(test_file, nullptr);
        ASSERT_NE(brain, nullptr);
    }

    // Get trust score
    uint32_t module_id = persistence_get_security_module_id();
    nimcp_trust_score_t score;
    nimcp_result_t result = nimcp_sec_get_trust_score(security_ctx, module_id, &score);

    if (result == NIMCP_SUCCESS) {
        // After successful operations, trust should be reasonable
        EXPECT_GE(score.expected_trust, 0.0);
    }
}

//=============================================================================
// Error Recovery Tests
//=============================================================================

TEST_F(PersistenceSecurityIntegration, ErrorRecovery_InvalidPath) {
    ASSERT_TRUE(persistence_init(security_ctx));

    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);

    // Try to save to invalid path
    bool result = brain_save_ex(brain, "/nonexistent/directory/file.nimcp", nullptr);
    EXPECT_FALSE(result);

    persistence_stats_t stats;
    persistence_get_stats(&stats);
    EXPECT_GT(stats.save_errors, 0u);
}

TEST_F(PersistenceSecurityIntegration, ErrorRecovery_ContinueAfterError) {
    ASSERT_TRUE(persistence_init(security_ctx));

    brain = createTestBrain();
    ASSERT_NE(brain, nullptr);
    trainBrain(brain, 10);

    // Failed save
    brain_save_ex(brain, "/nonexistent/path.nimcp", nullptr);

    // Successful save should still work
    EXPECT_TRUE(brain_save_ex(brain, test_file, nullptr));

    persistence_stats_t stats;
    persistence_get_stats(&stats);
    EXPECT_EQ(stats.total_saves, 2u);  // All save attempts counted
    EXPECT_EQ(stats.save_errors, 1u);
    // Successful saves = total_saves - save_errors
    EXPECT_EQ(stats.total_saves - stats.save_errors, 1u);
}

//=============================================================================
// Run Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
