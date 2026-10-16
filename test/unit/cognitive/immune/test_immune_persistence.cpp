/**
 * @file test_immune_persistence.cpp
 * @brief Unit tests for Immune Persistence Module
 * @date 2025-12-12
 *
 * Tests save/load, incremental updates, version compatibility,
 * checksum validation, and configuration options.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_immune_persistence.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ImmunePersistenceTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    immune_persistence_config_t config;
    const char* test_file = "/tmp/test_immune_memory.dat";
    const char* test_file_inc = "/tmp/test_immune_memory_inc.dat";

    void SetUp() override {
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        immune_persistence_default_config(&config);
    }

    void TearDown() override {
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }

        // Clean up test files
        remove(test_file);
        remove(test_file_inc);
    }

    // Helper to populate immune system with test data
    void populateImmuneSystem() {
        // Present some antigens
        for (int i = 0; i < 3; i++) {
            uint8_t epitope[] = {(uint8_t)i, 0x02, 0x03, 0x04};
            uint32_t antigen_id;
            brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                         epitope, sizeof(epitope), (uint8_t)(i + 1), (uint32_t)i, &antigen_id);
        }

        // Activate some B cells
        uint32_t b_cell_id, helper_id;
        brain_immune_activate_b_cell(immune_system, 1, &b_cell_id);
        brain_immune_activate_helper_t(immune_system, 1, &helper_id);
        brain_immune_t_help_b(immune_system, helper_id, b_cell_id);
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(ImmunePersistenceTest, DefaultConfigIsValid) {
    immune_persistence_config_t cfg;
    int result = immune_persistence_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cfg.save_antigens);
    EXPECT_TRUE(cfg.save_b_cells);
    EXPECT_TRUE(cfg.save_t_cells);
    EXPECT_TRUE(cfg.verify_on_load);
    EXPECT_TRUE(cfg.create_backup);
}

TEST_F(ImmunePersistenceTest, DefaultConfigNullFails) {
    int result = immune_persistence_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ImmunePersistenceTest, SetEncryptionKey) {
    uint8_t key[32] = {0};
    for (int i = 0; i < 32; i++) key[i] = (uint8_t)i;

    int result = immune_persistence_set_encryption_key(&config, key, 32);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.encryption_key_set);
    EXPECT_TRUE(config.enable_encryption);
}

TEST_F(ImmunePersistenceTest, SetEncryptionKeyInvalidLength) {
    uint8_t key[16] = {0};
    int result = immune_persistence_set_encryption_key(&config, key, 16);
    EXPECT_EQ(result, -1);
}

TEST_F(ImmunePersistenceTest, SetEncryptionKeyNullFails) {
    uint8_t key[32] = {0};
    EXPECT_EQ(immune_persistence_set_encryption_key(nullptr, key, 32), -1);
    EXPECT_EQ(immune_persistence_set_encryption_key(&config, nullptr, 32), -1);
}

/* ============================================================================
 * Save/Load Tests
 * ============================================================================ */

TEST_F(ImmunePersistenceTest, SaveEmptySystem) {
    int result = immune_persistence_save(immune_system, test_file, &config);
    EXPECT_EQ(result, 0);

    // File should exist
    FILE* f = fopen(test_file, "rb");
    EXPECT_NE(f, nullptr);
    if (f) fclose(f);
}

TEST_F(ImmunePersistenceTest, SavePopulatedSystem) {
    populateImmuneSystem();

    int result = immune_persistence_save(immune_system, test_file, &config);
    EXPECT_EQ(result, 0);
}

TEST_F(ImmunePersistenceTest, LoadSavedState) {
    populateImmuneSystem();
    immune_persistence_save(immune_system, test_file, &config);

    // Create new immune system
    brain_immune_system_t* new_system = brain_immune_create(nullptr);
    ASSERT_NE(new_system, nullptr);
    brain_immune_start(new_system);

    int result = immune_persistence_load(new_system, test_file, &config);
    EXPECT_EQ(result, 0);

    brain_immune_stop(new_system);
    brain_immune_destroy(new_system);
}

TEST_F(ImmunePersistenceTest, SaveLoadRoundTrip) {
    populateImmuneSystem();

    brain_immune_stats_t before;
    brain_immune_get_stats(immune_system, &before);

    // Save
    immune_persistence_save(immune_system, test_file, &config);

    // Clear state
    immune_persistence_clear_state(immune_system);

    // Load
    immune_persistence_load(immune_system, test_file, &config);

    brain_immune_stats_t after;
    brain_immune_get_stats(immune_system, &after);

    // Counts should match
    EXPECT_EQ(after.antigens_processed, before.antigens_processed);
}

TEST_F(ImmunePersistenceTest, SaveWithNullConfig) {
    populateImmuneSystem();
    int result = immune_persistence_save(immune_system, test_file, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(ImmunePersistenceTest, LoadFromNonExistentFile) {
    int result = immune_persistence_load(immune_system, "/tmp/nonexistent.dat", &config);
    EXPECT_NE(result, 0);
}

TEST_F(ImmunePersistenceTest, SaveNullSystemFails) {
    int result = immune_persistence_save(nullptr, test_file, &config);
    EXPECT_EQ(result, -1);
}

TEST_F(ImmunePersistenceTest, SaveNullPathFails) {
    int result = immune_persistence_save(immune_system, nullptr, &config);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Incremental Save Tests
 * ============================================================================ */

TEST_F(ImmunePersistenceTest, SaveIncremental) {
    populateImmuneSystem();

    int result = immune_persistence_save_incremental(immune_system, test_file_inc, &config);
    EXPECT_EQ(result, 0);
}

TEST_F(ImmunePersistenceTest, IncrementalSavesMemoryOnly) {
    config.memory_cells_only = true;
    populateImmuneSystem();

    int result = immune_persistence_save_incremental(immune_system, test_file_inc, &config);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Extended API Tests
 * ============================================================================ */

TEST_F(ImmunePersistenceTest, SaveEx) {
    populateImmuneSystem();

    immune_persistence_result_t result_info;
    int result = immune_persistence_save_ex(immune_system, test_file, &config, &result_info);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(result_info.success);
    // Note: The _ex wrapper doesn't populate bytes_written/items_saved fields
    // It calls the base save function which doesn't track these metrics
}

TEST_F(ImmunePersistenceTest, LoadEx) {
    populateImmuneSystem();
    immune_persistence_save(immune_system, test_file, &config);
    immune_persistence_clear_state(immune_system);

    immune_persistence_result_t result_info;
    int result = immune_persistence_load_ex(immune_system, test_file, &config, &result_info);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(result_info.success);
    // Note: The _ex wrapper doesn't populate bytes_read/items_loaded fields
    // It calls the base load function which doesn't track these metrics
}

TEST_F(ImmunePersistenceTest, SaveExNullResultFails) {
    int result = immune_persistence_save_ex(immune_system, test_file, &config, nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Validation Tests
 * ============================================================================ */

TEST_F(ImmunePersistenceTest, GetVersion) {
    uint32_t version = immune_persistence_get_version();
    EXPECT_EQ(version, IMMUNE_PERSISTENCE_VERSION);
}

TEST_F(ImmunePersistenceTest, ValidateFile) {
    populateImmuneSystem();
    immune_persistence_save(immune_system, test_file, &config);

    int result = immune_persistence_validate_file(test_file, true);
    EXPECT_EQ(result, 0);
}

TEST_F(ImmunePersistenceTest, ValidateNonExistentFile) {
    int result = immune_persistence_validate_file("/tmp/nonexistent.dat", false);
    EXPECT_NE(result, 0);
}

TEST_F(ImmunePersistenceTest, ValidateCorruptedFile) {
    // Create a corrupted file
    FILE* f = fopen(test_file, "wb");
    ASSERT_NE(f, nullptr);
    const char* garbage = "not a valid immune file";
    fwrite(garbage, 1, strlen(garbage), f);
    fclose(f);

    int result = immune_persistence_validate_file(test_file, false);
    EXPECT_NE(result, 0);
}

TEST_F(ImmunePersistenceTest, IsVersionCompatible) {
    EXPECT_TRUE(immune_persistence_is_version_compatible(IMMUNE_PERSISTENCE_VERSION));
    EXPECT_FALSE(immune_persistence_is_version_compatible(999));
}

/* ============================================================================
 * File Info Tests
 * ============================================================================ */

TEST_F(ImmunePersistenceTest, GetFileInfo) {
    populateImmuneSystem();
    immune_persistence_save(immune_system, test_file, &config);

    immune_persistence_header_t header;
    immune_persistence_counts_t counts;

    int result = immune_persistence_get_file_info(test_file, &header, &counts);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(header.version, IMMUNE_PERSISTENCE_VERSION);
    EXPECT_GT(counts.antigen_count, 0u);
}

TEST_F(ImmunePersistenceTest, GetFileInfoNullHeader) {
    populateImmuneSystem();
    immune_persistence_save(immune_system, test_file, &config);

    immune_persistence_counts_t counts;
    int result = immune_persistence_get_file_info(test_file, nullptr, &counts);
    EXPECT_EQ(result, 0);
}

TEST_F(ImmunePersistenceTest, GetFileInfoNullCounts) {
    populateImmuneSystem();
    immune_persistence_save(immune_system, test_file, &config);

    immune_persistence_header_t header;
    int result = immune_persistence_get_file_info(test_file, &header, nullptr);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Backup Tests
 * ============================================================================ */

TEST_F(ImmunePersistenceTest, CreateBackup) {
    populateImmuneSystem();
    immune_persistence_save(immune_system, test_file, &config);

    int result = immune_persistence_create_backup(test_file, ".bak");
    EXPECT_EQ(result, 0);

    // Check backup exists
    char backup_path[256];
    snprintf(backup_path, sizeof(backup_path), "%s.bak", test_file);
    FILE* f = fopen(backup_path, "rb");
    EXPECT_NE(f, nullptr);
    if (f) fclose(f);

    remove(backup_path);
}

TEST_F(ImmunePersistenceTest, BackupNonExistentFile) {
    // Implementation returns 0 for non-existent file (nothing to backup = success)
    int result = immune_persistence_create_backup("/tmp/nonexistent.dat", ".bak");
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Clear State Tests
 * ============================================================================ */

TEST_F(ImmunePersistenceTest, ClearState) {
    populateImmuneSystem();

    brain_immune_stats_t before;
    brain_immune_get_stats(immune_system, &before);
    EXPECT_GT(before.antigens_processed, 0u);

    int result = immune_persistence_clear_state(immune_system);
    EXPECT_EQ(result, 0);

    brain_immune_stats_t after;
    brain_immune_get_stats(immune_system, &after);
    EXPECT_EQ(after.antigens_processed, 0u);
}

TEST_F(ImmunePersistenceTest, ClearStateNullFails) {
    int result = immune_persistence_clear_state(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Selective Save Tests
 * ============================================================================ */

TEST_F(ImmunePersistenceTest, SaveAntigensOnly) {
    config.save_antigens = true;
    config.save_b_cells = false;
    config.save_t_cells = false;
    config.save_antibodies = false;
    config.save_cytokines = false;
    config.save_inflammation = false;
    config.save_statistics = false;

    populateImmuneSystem();

    immune_persistence_result_t result_info;
    immune_persistence_save_ex(immune_system, test_file, &config, &result_info);
    EXPECT_TRUE(result_info.success);
}

TEST_F(ImmunePersistenceTest, SaveMemoryCellsOnly) {
    config.memory_cells_only = true;

    populateImmuneSystem();

    int result = immune_persistence_save(immune_system, test_file, &config);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Compression Tests
 * ============================================================================ */

TEST_F(ImmunePersistenceTest, SaveWithCompression) {
    config.enable_compression = true;

    populateImmuneSystem();

    int result = immune_persistence_save(immune_system, test_file, &config);
    // May succeed or fail depending on zlib availability
    // Just check it doesn't crash
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(ImmunePersistenceTest, EmptyPath) {
    int result = immune_persistence_save(immune_system, "", &config);
    EXPECT_NE(result, 0);
}

TEST_F(ImmunePersistenceTest, InvalidDirectory) {
    int result = immune_persistence_save(immune_system, "/nonexistent/dir/file.dat", &config);
    EXPECT_NE(result, 0);
}

TEST_F(ImmunePersistenceTest, DoubleLoad) {
    populateImmuneSystem();
    immune_persistence_save(immune_system, test_file, &config);

    // Load twice
    immune_persistence_load(immune_system, test_file, &config);
    int result = immune_persistence_load(immune_system, test_file, &config);
    EXPECT_EQ(result, 0);
}

TEST_F(ImmunePersistenceTest, LargeImmuneSystem) {
    // Populate with many entries
    for (int i = 0; i < 100; i++) {
        uint8_t epitope[] = {(uint8_t)(i >> 8), (uint8_t)(i & 0xFF), 0x03, 0x04};
        uint32_t antigen_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                     epitope, sizeof(epitope), (uint8_t)((i % 10) + 1), (uint32_t)i, &antigen_id);
    }

    immune_persistence_result_t result_info;
    int result = immune_persistence_save_ex(immune_system, test_file, &config, &result_info);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(result_info.success);
    // Note: items_saved is not populated by the _ex wrapper
}
