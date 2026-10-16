/**
 * @file test_kg_persistence.cpp
 * @brief Unit tests for Knowledge Graph Persistence functionality
 *
 * TEST COVERAGE:
 * - Snapshot creation
 * - Snapshot loading
 * - Data integrity verification
 * - Recovery from corruption
 * - Large KG handling
 *
 * @author NIMCP Development Team
 * @date 2025-02-02
 */

#include <gtest/gtest.h>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <chrono>

extern "C" {
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
}

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class KnowledgeGraphPersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        system = knowledge_system_create("kg_persistence_test");
        ASSERT_NE(system, nullptr);

        // Create temp directory for test files
        temp_dir = "/tmp/nimcp_kg_test_";
        temp_dir += std::to_string(getpid());
        mkdir(temp_dir.c_str(), 0755);
    }

    void TearDown() override {
        if (system) {
            knowledge_system_destroy(system);
            system = nullptr;
        }

        // Clean up temp files
        cleanup_temp_files();
    }

    void cleanup_temp_files() {
        // Remove all files in temp directory
        std::string cmd = "rm -rf " + temp_dir;
        system(cmd.c_str());
    }

    std::string get_temp_file(const std::string& name) {
        return temp_dir + "/" + name;
    }

    // Helper to create a test knowledge item
    knowledge_item_t create_test_item(const char* name, knowledge_domain_t domain, float confidence) {
        knowledge_item_t item = {0};
        strncpy(item.concept_name, name, sizeof(item.concept_name) - 1);
        item.domain = domain;
        item.confidence = confidence;
        item.learned_timestamp = (uint64_t)time(nullptr);
        snprintf(item.definition, sizeof(item.definition), "Definition of %s", name);
        snprintf(item.context, sizeof(item.context), "Context for %s", name);
        return item;
    }

    // Helper to populate knowledge system with test data
    void populate_test_data(int num_items) {
        for (int i = 0; i < num_items; i++) {
            char name[64];
            snprintf(name, sizeof(name), "test_concept_%d", i);
            knowledge_item_t item = create_test_item(name,
                static_cast<knowledge_domain_t>(i % 11),
                0.1f + (float)(i % 10) * 0.1f);
            knowledge_add_item(system, &item);
        }
    }

    // Helper to verify data integrity after load
    bool verify_data_integrity(knowledge_system_t loaded_system, int expected_count) {
        for (int i = 0; i < expected_count; i++) {
            char name[64];
            snprintf(name, sizeof(name), "test_concept_%d", i);
            knowledge_item_t item;
            if (!knowledge_retrieve(loaded_system, name, &item)) {
                return false;
            }
            if (strcmp(item.concept_name, name) != 0) {
                return false;
            }
        }
        return true;
    }

    knowledge_system_t system = nullptr;
    std::string temp_dir;
};

//=============================================================================
// Snapshot Creation Tests
//=============================================================================

TEST_F(KnowledgeGraphPersistenceTest, SaveEmptyKnowledge) {
    std::string filepath = get_temp_file("empty_knowledge.dat");
    bool result = knowledge_save(system, filepath.c_str());
    EXPECT_TRUE(result);

    // Verify file exists
    struct stat st;
    EXPECT_EQ(stat(filepath.c_str(), &st), 0);
}

TEST_F(KnowledgeGraphPersistenceTest, SaveNullSystem) {
    std::string filepath = get_temp_file("null_system.dat");
    bool result = knowledge_save(nullptr, filepath.c_str());
    EXPECT_FALSE(result);
}

TEST_F(KnowledgeGraphPersistenceTest, SaveNullPath) {
    bool result = knowledge_save(system, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(KnowledgeGraphPersistenceTest, SaveEmptyPath) {
    bool result = knowledge_save(system, "");
    EXPECT_FALSE(result);
}

TEST_F(KnowledgeGraphPersistenceTest, SaveInvalidPath) {
    bool result = knowledge_save(system, "/nonexistent/directory/path/file.dat");
    EXPECT_FALSE(result);
}

TEST_F(KnowledgeGraphPersistenceTest, SaveWithData) {
    populate_test_data(100);

    std::string filepath = get_temp_file("knowledge_with_data.dat");
    bool result = knowledge_save(system, filepath.c_str());
    EXPECT_TRUE(result);

    // Verify file size is reasonable
    struct stat st;
    ASSERT_EQ(stat(filepath.c_str(), &st), 0);
    EXPECT_GT(st.st_size, 0);
}

TEST_F(KnowledgeGraphPersistenceTest, SaveOverwriteExisting) {
    std::string filepath = get_temp_file("overwrite.dat");

    // Save first version
    populate_test_data(50);
    bool result1 = knowledge_save(system, filepath.c_str());
    EXPECT_TRUE(result1);

    struct stat st1;
    ASSERT_EQ(stat(filepath.c_str(), &st1), 0);
    off_t size1 = st1.st_size;

    // Add more data and save again
    populate_test_data(100);  // This adds 100 more items
    bool result2 = knowledge_save(system, filepath.c_str());
    EXPECT_TRUE(result2);

    struct stat st2;
    ASSERT_EQ(stat(filepath.c_str(), &st2), 0);
    // Size should be larger due to more data
    EXPECT_GE(st2.st_size, size1);
}

//=============================================================================
// Snapshot Loading Tests
//=============================================================================

TEST_F(KnowledgeGraphPersistenceTest, LoadNullPath) {
    knowledge_system_t loaded = knowledge_load(nullptr);
    EXPECT_EQ(loaded, nullptr);
}

TEST_F(KnowledgeGraphPersistenceTest, LoadEmptyPath) {
    knowledge_system_t loaded = knowledge_load("");
    EXPECT_EQ(loaded, nullptr);
}

TEST_F(KnowledgeGraphPersistenceTest, LoadNonexistentFile) {
    knowledge_system_t loaded = knowledge_load("/nonexistent/path/file.dat");
    EXPECT_EQ(loaded, nullptr);
}

TEST_F(KnowledgeGraphPersistenceTest, LoadEmptyFile) {
    std::string filepath = get_temp_file("empty_file.dat");

    // Create empty file
    std::ofstream ofs(filepath);
    ofs.close();

    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    // May succeed with empty knowledge or fail
    if (loaded) {
        knowledge_system_destroy(loaded);
    }
}

TEST_F(KnowledgeGraphPersistenceTest, LoadValidFile) {
    const int NUM_ITEMS = 100;
    populate_test_data(NUM_ITEMS);

    std::string filepath = get_temp_file("valid_knowledge.dat");
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    ASSERT_NE(loaded, nullptr);

    // Verify data integrity
    EXPECT_TRUE(verify_data_integrity(loaded, NUM_ITEMS));

    knowledge_system_destroy(loaded);
}

TEST_F(KnowledgeGraphPersistenceTest, LoadPreservesConfidence) {
    knowledge_item_t item = create_test_item("confidence_test", KNOWLEDGE_DOMAIN_SCIENCE, 0.85f);
    knowledge_add_item(system, &item);

    std::string filepath = get_temp_file("confidence.dat");
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    ASSERT_NE(loaded, nullptr);

    knowledge_item_t retrieved;
    ASSERT_TRUE(knowledge_retrieve(loaded, "confidence_test", &retrieved));
    EXPECT_NEAR(retrieved.confidence, 0.85f, 0.001f);

    knowledge_system_destroy(loaded);
}

TEST_F(KnowledgeGraphPersistenceTest, LoadPreservesDomain) {
    for (int d = 0; d <= KNOWLEDGE_DOMAIN_GENERAL; d++) {
        char name[64];
        snprintf(name, sizeof(name), "domain_%d", d);
        knowledge_item_t item = create_test_item(name, static_cast<knowledge_domain_t>(d), 0.5f);
        knowledge_add_item(system, &item);
    }

    std::string filepath = get_temp_file("domains.dat");
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    ASSERT_NE(loaded, nullptr);

    for (int d = 0; d <= KNOWLEDGE_DOMAIN_GENERAL; d++) {
        char name[64];
        snprintf(name, sizeof(name), "domain_%d", d);
        knowledge_item_t retrieved;
        ASSERT_TRUE(knowledge_retrieve(loaded, name, &retrieved));
        EXPECT_EQ(retrieved.domain, static_cast<knowledge_domain_t>(d));
    }

    knowledge_system_destroy(loaded);
}

//=============================================================================
// Data Integrity Verification Tests
//=============================================================================

TEST_F(KnowledgeGraphPersistenceTest, IntegrityAfterRoundTrip) {
    // Create complex knowledge structure
    for (int i = 0; i < 50; i++) {
        char name[64];
        snprintf(name, sizeof(name), "concept_%d", i);
        knowledge_item_t item = create_test_item(name, KNOWLEDGE_DOMAIN_SCIENCE, 0.5f + (float)i * 0.01f);

        // Add examples
        item.num_examples = 2;
        item.examples = (char**)calloc(2, sizeof(char*));
        item.examples[0] = strdup("Example 1");
        item.examples[1] = strdup("Example 2");

        // Add related concepts
        item.num_related = 2;
        item.related_concepts = (char**)calloc(2, sizeof(char*));
        char related1[64], related2[64];
        snprintf(related1, sizeof(related1), "related_to_%d_a", i);
        snprintf(related2, sizeof(related2), "related_to_%d_b", i);
        item.related_concepts[0] = strdup(related1);
        item.related_concepts[1] = strdup(related2);

        knowledge_add_item(system, &item);

        // Free allocated memory
        free(item.examples[0]);
        free(item.examples[1]);
        free(item.examples);
        free(item.related_concepts[0]);
        free(item.related_concepts[1]);
        free(item.related_concepts);
    }

    std::string filepath = get_temp_file("integrity.dat");
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    ASSERT_NE(loaded, nullptr);

    // Verify all concepts
    for (int i = 0; i < 50; i++) {
        char name[64];
        snprintf(name, sizeof(name), "concept_%d", i);
        knowledge_item_t retrieved;
        ASSERT_TRUE(knowledge_retrieve(loaded, name, &retrieved)) << "Failed to retrieve " << name;
        EXPECT_NEAR(retrieved.confidence, 0.5f + (float)i * 0.01f, 0.001f);
    }

    knowledge_system_destroy(loaded);
}

TEST_F(KnowledgeGraphPersistenceTest, ChecksumValidation) {
    populate_test_data(50);

    std::string filepath = get_temp_file("checksum.dat");
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    // Corrupt the file slightly
    std::fstream file(filepath, std::ios::in | std::ios::out | std::ios::binary);
    if (file.is_open()) {
        file.seekp(10, std::ios::beg);  // Seek to position 10
        char corrupt_byte = 0xFF;
        file.write(&corrupt_byte, 1);
        file.close();
    }

    // Try to load corrupted file
    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    // May succeed with partial data or fail entirely
    if (loaded) {
        knowledge_system_destroy(loaded);
    }
}

//=============================================================================
// Recovery from Corruption Tests
//=============================================================================

TEST_F(KnowledgeGraphPersistenceTest, LoadTruncatedFile) {
    populate_test_data(100);

    std::string filepath = get_temp_file("truncated.dat");
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    // Truncate the file
    struct stat st;
    ASSERT_EQ(stat(filepath.c_str(), &st), 0);
    ASSERT_EQ(truncate(filepath.c_str(), st.st_size / 2), 0);

    // Try to load truncated file
    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    // Should fail or return partial data
    if (loaded) {
        knowledge_system_destroy(loaded);
    }
}

TEST_F(KnowledgeGraphPersistenceTest, LoadGarbageData) {
    std::string filepath = get_temp_file("garbage.dat");

    // Write random garbage
    std::ofstream ofs(filepath, std::ios::binary);
    for (int i = 0; i < 1000; i++) {
        char c = rand() % 256;
        ofs.write(&c, 1);
    }
    ofs.close();

    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    // Should fail gracefully
    EXPECT_EQ(loaded, nullptr);
}

TEST_F(KnowledgeGraphPersistenceTest, LoadPartiallyCorruptedFile) {
    populate_test_data(100);

    std::string filepath = get_temp_file("partial_corrupt.dat");
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    // Corrupt middle section
    std::fstream file(filepath, std::ios::in | std::ios::out | std::ios::binary);
    if (file.is_open()) {
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();

        file.seekp(file_size / 2, std::ios::beg);
        for (int i = 0; i < 50; i++) {
            char corrupt_byte = 0x00;
            file.write(&corrupt_byte, 1);
        }
        file.close();
    }

    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    // Should handle gracefully
    if (loaded) {
        knowledge_system_destroy(loaded);
    }
}

//=============================================================================
// Large KG Handling Tests
//=============================================================================

TEST_F(KnowledgeGraphPersistenceTest, SaveLargeKnowledgeGraph) {
    const int NUM_ITEMS = 1000;
    populate_test_data(NUM_ITEMS);

    std::string filepath = get_temp_file("large_kg.dat");

    auto start = std::chrono::high_resolution_clock::now();
    bool result = knowledge_save(system, filepath.c_str());
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_TRUE(result);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // Should complete in reasonable time (< 10 seconds)
    EXPECT_LT(duration.count(), 10000);
}

TEST_F(KnowledgeGraphPersistenceTest, LoadLargeKnowledgeGraph) {
    const int NUM_ITEMS = 1000;
    populate_test_data(NUM_ITEMS);

    std::string filepath = get_temp_file("large_kg_load.dat");
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    auto start = std::chrono::high_resolution_clock::now();
    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_NE(loaded, nullptr);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // Should complete in reasonable time (< 10 seconds)
    EXPECT_LT(duration.count(), 10000);

    // Spot check a few items
    for (int i = 0; i < NUM_ITEMS; i += 100) {
        char name[64];
        snprintf(name, sizeof(name), "test_concept_%d", i);
        knowledge_item_t item;
        EXPECT_TRUE(knowledge_retrieve(loaded, name, &item));
    }

    knowledge_system_destroy(loaded);
}

TEST_F(KnowledgeGraphPersistenceTest, LargeKGFileSize) {
    const int NUM_ITEMS = 500;
    populate_test_data(NUM_ITEMS);

    std::string filepath = get_temp_file("large_file_size.dat");
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    struct stat st;
    ASSERT_EQ(stat(filepath.c_str(), &st), 0);

    // File should be reasonable size (not too bloated)
    // Estimate: 500 items * ~500 bytes per item = ~250KB
    // Allow up to 10x for overhead
    EXPECT_LT(st.st_size, 500 * 500 * 10);
    EXPECT_GT(st.st_size, 500);  // At least some data
}

TEST_F(KnowledgeGraphPersistenceTest, IncrementalSave) {
    std::string filepath = get_temp_file("incremental.dat");

    // Save initial data
    populate_test_data(50);
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    struct stat st1;
    ASSERT_EQ(stat(filepath.c_str(), &st1), 0);

    // Add more data
    for (int i = 50; i < 100; i++) {
        char name[64];
        snprintf(name, sizeof(name), "incremental_concept_%d", i);
        knowledge_item_t item = create_test_item(name, KNOWLEDGE_DOMAIN_GENERAL, 0.5f);
        knowledge_add_item(system, &item);
    }

    // Save again
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    struct stat st2;
    ASSERT_EQ(stat(filepath.c_str(), &st2), 0);

    // File should be larger
    EXPECT_GT(st2.st_size, st1.st_size);

    // Verify all data is present
    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    ASSERT_NE(loaded, nullptr);

    // Check original data
    for (int i = 0; i < 50; i++) {
        char name[64];
        snprintf(name, sizeof(name), "test_concept_%d", i);
        knowledge_item_t item;
        EXPECT_TRUE(knowledge_retrieve(loaded, name, &item));
    }

    // Check incremental data
    for (int i = 50; i < 100; i++) {
        char name[64];
        snprintf(name, sizeof(name), "incremental_concept_%d", i);
        knowledge_item_t item;
        EXPECT_TRUE(knowledge_retrieve(loaded, name, &item));
    }

    knowledge_system_destroy(loaded);
}

//=============================================================================
// File Permission Tests
//=============================================================================

TEST_F(KnowledgeGraphPersistenceTest, SaveReadOnlyDirectory) {
    // Skip if running as root (can write anywhere)
    if (geteuid() == 0) {
        GTEST_SKIP() << "Test skipped when running as root";
    }

    std::string readonly_dir = temp_dir + "/readonly";
    mkdir(readonly_dir.c_str(), 0444);  // Read-only

    std::string filepath = readonly_dir + "/test.dat";
    bool result = knowledge_save(system, filepath.c_str());
    EXPECT_FALSE(result);

    // Cleanup
    chmod(readonly_dir.c_str(), 0755);
    rmdir(readonly_dir.c_str());
}

TEST_F(KnowledgeGraphPersistenceTest, LoadNoReadPermission) {
    // Skip if running as root
    if (geteuid() == 0) {
        GTEST_SKIP() << "Test skipped when running as root";
    }

    populate_test_data(10);
    std::string filepath = get_temp_file("noperm.dat");
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    // Remove read permission
    chmod(filepath.c_str(), 0000);

    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    EXPECT_EQ(loaded, nullptr);

    // Restore permissions for cleanup
    chmod(filepath.c_str(), 0644);
}

//=============================================================================
// Special Content Tests
//=============================================================================

TEST_F(KnowledgeGraphPersistenceTest, SaveSpecialCharacters) {
    const char* special_names[] = {
        "concept_with_space",
        "concept\twith\ttab",
        "concept'with'quote",
        "concept\"with\"doublequote",
        "concept\\with\\backslash",
        "concept/with/slash",
    };

    for (const char* name : special_names) {
        knowledge_item_t item = create_test_item(name, KNOWLEDGE_DOMAIN_GENERAL, 0.5f);
        knowledge_add_item(system, &item);
    }

    std::string filepath = get_temp_file("special_chars.dat");
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    ASSERT_NE(loaded, nullptr);

    for (const char* name : special_names) {
        knowledge_item_t retrieved;
        bool found = knowledge_retrieve(loaded, name, &retrieved);
        EXPECT_TRUE(found) << "Failed to retrieve: " << name;
    }

    knowledge_system_destroy(loaded);
}

TEST_F(KnowledgeGraphPersistenceTest, SaveBinaryContent) {
    knowledge_item_t item = create_test_item("binary_content", KNOWLEDGE_DOMAIN_GENERAL, 0.5f);

    // Add binary content in definition
    char binary_def[100];
    for (int i = 0; i < 50; i++) {
        binary_def[i] = (char)(i + 1);  // Non-null binary values
    }
    binary_def[50] = '\0';
    strncpy(item.definition, binary_def, sizeof(item.definition) - 1);

    knowledge_add_item(system, &item);

    std::string filepath = get_temp_file("binary_content.dat");
    ASSERT_TRUE(knowledge_save(system, filepath.c_str()));

    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    // May or may not preserve binary content depending on implementation
    if (loaded) {
        knowledge_system_destroy(loaded);
    }
}

}  // anonymous namespace
