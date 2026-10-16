/**
 * @file test_symbolic_logic_export_import.cpp
 * @brief Unit tests for symbolic logic KB export/import functions
 *
 * COVERAGE:
 * - brain_export_knowledge_base: text format KB export
 * - brain_import_knowledge_base: text format KB import
 * - Round-trip: export then import preserves facts
 * - Error cases: NULL parameters, nonexistent file
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include "cognitive/reasoning/nimcp_symbolic_logic_brain_integration.h"
#include "core/brain/nimcp_brain.h"
#include "utils/logging/nimcp_logging.h"

// =============================================================================
// Test Fixture
// =============================================================================

class SymbolicLogicExportImportTest : public ::testing::Test {
protected:
    brain_t brain_ = nullptr;
    std::string tmp_filepath_;

    void SetUp() override {
        nimcp_log_set_level(NULL, LOG_LEVEL_ERROR);

        brain_ = brain_create("export_import_test", BRAIN_SIZE_SMALL,
                              BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain_, nullptr) << "Failed to create brain";

        /* Generate a unique temp file path */
        char tmp_name[] = "/tmp/nimcp_kb_test_XXXXXX";
        int fd = mkstemp(tmp_name);
        ASSERT_GE(fd, 0) << "Failed to create temp file";
        close(fd);
        tmp_filepath_ = std::string(tmp_name);
    }

    void TearDown() override {
        if (brain_) {
            brain_destroy(brain_);
            brain_ = nullptr;
        }
        /* Clean up temp file */
        if (!tmp_filepath_.empty()) {
            std::remove(tmp_filepath_.c_str());
        }
    }

    bool initLogic() {
        return brain_create_symbolic_logic(brain_, nullptr);
    }

    /* Read file contents as string for inspection */
    std::string readFile(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) return "";
        return std::string((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
    }
};

// =============================================================================
// Export Tests
// =============================================================================

// --- Export to file creates valid file ---

TEST_F(SymbolicLogicExportImportTest, ExportCreatesValidFile) {
    ASSERT_TRUE(initLogic());

    /* Add some facts */
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Penguin(opus)", 0.8f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Cat(garfield)", 0.7f));

    /* Export */
    bool result = brain_export_knowledge_base(brain_, tmp_filepath_.c_str());
    EXPECT_TRUE(result);

    /* Verify file was created and has content */
    std::string content = readFile(tmp_filepath_);
    EXPECT_FALSE(content.empty()) << "Exported file should not be empty";

    /* Should contain the NIMCP header */
    EXPECT_NE(content.find("NIMCP Knowledge Base Export"), std::string::npos)
        << "File should contain export header";

    /* Should contain facts section */
    EXPECT_NE(content.find("Facts"), std::string::npos)
        << "File should contain facts section header";
}

// --- Export with no facts produces header-only file ---

TEST_F(SymbolicLogicExportImportTest, ExportNoFactsProducesHeaderOnly) {
    ASSERT_TRUE(initLogic());

    /* Export without adding any facts */
    bool result = brain_export_knowledge_base(brain_, tmp_filepath_.c_str());
    EXPECT_TRUE(result);

    /* File should exist and contain at least the header */
    std::string content = readFile(tmp_filepath_);
    EXPECT_FALSE(content.empty()) << "File should have at least a header";
    EXPECT_NE(content.find("NIMCP Knowledge Base Export"), std::string::npos);

    /* Facts count should be 0 */
    EXPECT_NE(content.find("Facts: 0"), std::string::npos)
        << "Header should show 0 facts";
}

// =============================================================================
// Import Tests
// =============================================================================

// --- Import from valid file succeeds ---

TEST_F(SymbolicLogicExportImportTest, ImportFromValidFileSucceeds) {
    ASSERT_TRUE(initLogic());

    /* Add facts and export */
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Mammal(fido)", 0.85f));
    ASSERT_TRUE(brain_export_knowledge_base(brain_, tmp_filepath_.c_str()));

    /* Create a fresh brain for import */
    brain_t brain2 = brain_create("import_test", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain2, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain2, nullptr));

    /* Import */
    bool result = brain_import_knowledge_base(brain2, tmp_filepath_.c_str());
    EXPECT_TRUE(result);

    brain_destroy(brain2);
}

// --- Import from nonexistent file returns error ---

TEST_F(SymbolicLogicExportImportTest, ImportNonexistentFileReturnsError) {
    ASSERT_TRUE(initLogic());

    bool result = brain_import_knowledge_base(
        brain_, "/tmp/nimcp_nonexistent_file_99999.txt");
    EXPECT_FALSE(result);
}

// =============================================================================
// Round-trip Tests
// =============================================================================

// --- Round-trip: export then import preserves facts ---

TEST_F(SymbolicLogicExportImportTest, RoundTripPreservesFacts) {
    ASSERT_TRUE(initLogic());

    /* Add several facts */
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Penguin(opus)", 0.8f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Cat(garfield)", 0.7f));

    /* Get stats before export */
    logic_stats_t stats_before;
    ASSERT_TRUE(brain_get_logic_stats(brain_, &stats_before));
    uint32_t facts_before = stats_before.facts_stored;
    EXPECT_EQ(facts_before, 3u);

    /* Export */
    ASSERT_TRUE(brain_export_knowledge_base(brain_, tmp_filepath_.c_str()));

    /* Create fresh brain and import */
    brain_t brain2 = brain_create("roundtrip_test", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain2, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain2, nullptr));

    ASSERT_TRUE(brain_import_knowledge_base(brain2, tmp_filepath_.c_str()));

    /* Verify imported brain has same number of facts */
    logic_stats_t stats_after;
    ASSERT_TRUE(brain_get_logic_stats(brain2, &stats_after));
    EXPECT_EQ(stats_after.facts_stored, facts_before)
        << "Round-trip should preserve fact count";

    brain_destroy(brain2);
}

// =============================================================================
// NULL Parameter Tests
// =============================================================================

TEST_F(SymbolicLogicExportImportTest, ExportNullBrainReturnsFalse) {
    bool result = brain_export_knowledge_base(nullptr, tmp_filepath_.c_str());
    EXPECT_FALSE(result);
}

TEST_F(SymbolicLogicExportImportTest, ExportNullFilepathReturnsFalse) {
    ASSERT_TRUE(initLogic());
    bool result = brain_export_knowledge_base(brain_, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SymbolicLogicExportImportTest, ExportUninitializedLogicReturnsFalse) {
    /* Brain created but symbolic logic NOT initialized */
    bool result = brain_export_knowledge_base(brain_, tmp_filepath_.c_str());
    EXPECT_FALSE(result);
}

TEST_F(SymbolicLogicExportImportTest, ImportNullBrainReturnsFalse) {
    bool result = brain_import_knowledge_base(nullptr, tmp_filepath_.c_str());
    EXPECT_FALSE(result);
}

TEST_F(SymbolicLogicExportImportTest, ImportNullFilepathReturnsFalse) {
    ASSERT_TRUE(initLogic());
    bool result = brain_import_knowledge_base(brain_, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SymbolicLogicExportImportTest, ImportUninitializedLogicReturnsFalse) {
    /* Brain created but symbolic logic NOT initialized */
    bool result = brain_import_knowledge_base(brain_, tmp_filepath_.c_str());
    EXPECT_FALSE(result);
}
