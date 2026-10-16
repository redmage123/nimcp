/**
 * @file test_symbolic_logic_integration.cpp
 * @brief Integration tests for symbolic logic export/import round-trip
 *
 * COVERAGE:
 * - Export then import round-trip preserves fact count
 * - Export then import preserves salience annotations
 * - Import is additive (does not replace existing KB)
 * - Export format contains expected structure (header, facts section)
 * - Import from manually crafted KB file
 * - Multiple export/import cycles remain consistent
 * - Stats consistency through export/import lifecycle
 * - Export/import with custom config
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

#include "cognitive/reasoning/nimcp_symbolic_logic_brain_integration.h"
#include "core/brain/nimcp_brain.h"
#include "utils/logging/nimcp_logging.h"

// =============================================================================
// Test Fixture
// =============================================================================

class SymbolicLogicIntegrationTest : public ::testing::Test {
protected:
    brain_t brain_ = nullptr;
    std::string tmp_filepath_;
    std::string tmp_filepath2_;

    void SetUp() override {
        nimcp_log_set_level(NULL, LOG_LEVEL_ERROR);

        brain_ = brain_create("logic_integration_test", BRAIN_SIZE_SMALL,
                              BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain_, nullptr) << "Failed to create brain";

        tmp_filepath_ = makeTempFile();
        tmp_filepath2_ = makeTempFile();
    }

    void TearDown() override {
        if (brain_) {
            brain_destroy_symbolic_logic(brain_);
            brain_destroy(brain_);
            brain_ = nullptr;
        }
        cleanupFile(tmp_filepath_);
        cleanupFile(tmp_filepath2_);
    }

    bool initLogic(const logic_brain_config_t* config = nullptr) {
        return brain_create_symbolic_logic(brain_, config);
    }

    std::string makeTempFile() {
        char tmp_name[] = "/tmp/nimcp_logic_int_XXXXXX";
        int fd = mkstemp(tmp_name);
        if (fd < 0) return "";
        close(fd);
        return std::string(tmp_name);
    }

    void cleanupFile(const std::string& path) {
        if (!path.empty()) {
            std::remove(path.c_str());
        }
    }

    std::string readFile(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) return "";
        return std::string((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
    }

    void writeFile(const std::string& path, const std::string& content) {
        std::ofstream ofs(path);
        ASSERT_TRUE(ofs.is_open()) << "Failed to open file for writing: " << path;
        ofs << content;
        ofs.close();
    }

    brain_t createFreshBrain(const char* name) {
        brain_t b = brain_create(name, BRAIN_SIZE_SMALL,
                                 BRAIN_TASK_CLASSIFICATION, 10, 5);
        return b;
    }
};

// =============================================================================
// Round-Trip Integration Tests
// =============================================================================

// Test: Export then import preserves fact count
TEST_F(SymbolicLogicIntegrationTest, RoundTripPreservesFactCount) {
    ASSERT_TRUE(initLogic());

    // Add several diverse facts
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Penguin(opus)", 0.8f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Cat(garfield)", 0.7f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Dog(snoopy)", 0.6f));

    // Get stats before export
    logic_stats_t stats_before;
    ASSERT_TRUE(brain_get_logic_stats(brain_, &stats_before));
    EXPECT_EQ(stats_before.facts_stored, 4u);

    // Export
    ASSERT_TRUE(brain_export_knowledge_base(brain_, tmp_filepath_.c_str()));

    // Create fresh brain and import
    brain_t brain2 = createFreshBrain("roundtrip_count");
    ASSERT_NE(brain2, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain2, nullptr));

    ASSERT_TRUE(brain_import_knowledge_base(brain2, tmp_filepath_.c_str()));

    // Verify fact count matches
    logic_stats_t stats_after;
    ASSERT_TRUE(brain_get_logic_stats(brain2, &stats_after));
    EXPECT_EQ(stats_after.facts_stored, stats_before.facts_stored)
        << "Round-trip should preserve fact count";

    brain_destroy_symbolic_logic(brain2);
    brain_destroy(brain2);
}

// Test: Export preserves salience annotations in file format
TEST_F(SymbolicLogicIntegrationTest, ExportPreservesSalienceAnnotations) {
    ASSERT_TRUE(initLogic());

    ASSERT_TRUE(brain_add_logical_fact(brain_, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Cat(garfield)", 0.3f));

    ASSERT_TRUE(brain_export_knowledge_base(brain_, tmp_filepath_.c_str()));

    // Verify the file contains salience annotations
    std::string content = readFile(tmp_filepath_);
    EXPECT_NE(content.find("salience="), std::string::npos)
        << "Exported file should contain salience annotations";
}

// Test: Import is additive - does not replace existing facts
TEST_F(SymbolicLogicIntegrationTest, ImportIsAdditive) {
    ASSERT_TRUE(initLogic());

    // Add initial facts and export
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Cat(garfield)", 0.7f));
    ASSERT_TRUE(brain_export_knowledge_base(brain_, tmp_filepath_.c_str()));

    // Create brain with pre-existing facts
    brain_t brain2 = createFreshBrain("additive_test");
    ASSERT_NE(brain2, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain2, nullptr));

    // Add a fact before import
    ASSERT_TRUE(brain_add_logical_fact(brain2, "Dog(snoopy)", 0.6f));

    logic_stats_t stats_pre_import;
    ASSERT_TRUE(brain_get_logic_stats(brain2, &stats_pre_import));
    uint32_t pre_count = stats_pre_import.facts_stored;
    EXPECT_EQ(pre_count, 1u);

    // Import should add to existing
    ASSERT_TRUE(brain_import_knowledge_base(brain2, tmp_filepath_.c_str()));

    logic_stats_t stats_post_import;
    ASSERT_TRUE(brain_get_logic_stats(brain2, &stats_post_import));
    // Should have more facts than before import
    EXPECT_GT(stats_post_import.facts_stored, pre_count)
        << "Import should add to existing KB, not replace";

    brain_destroy_symbolic_logic(brain2);
    brain_destroy(brain2);
}

// Test: Export format contains expected structure
TEST_F(SymbolicLogicIntegrationTest, ExportFormatHasExpectedStructure) {
    ASSERT_TRUE(initLogic());

    ASSERT_TRUE(brain_add_logical_fact(brain_, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_export_knowledge_base(brain_, tmp_filepath_.c_str()));

    std::string content = readFile(tmp_filepath_);

    // Check for header
    EXPECT_NE(content.find("// NIMCP Knowledge Base Export"), std::string::npos)
        << "File should have NIMCP header comment";

    // Check for facts section header
    EXPECT_NE(content.find("// Facts"), std::string::npos)
        << "File should have facts section header";

    // Check that stats header shows correct count
    EXPECT_NE(content.find("Facts: 1"), std::string::npos)
        << "Header should show correct fact count";
}

// Test: Import from manually crafted KB file
TEST_F(SymbolicLogicIntegrationTest, ImportFromManuallyCreatedFile) {
    ASSERT_TRUE(initLogic());

    // Write a manually formatted KB file
    std::string manual_kb =
        "// NIMCP Knowledge Base Export\n"
        "// Facts: 3, Rules applied: 0\n"
        "\n"
        "// Facts\n"
        "Bird(tweety). [salience=0.90]\n"
        "Penguin(opus). [salience=0.80]\n"
        "Cat(garfield). [salience=0.70]\n"
        "\n";

    writeFile(tmp_filepath_, manual_kb);

    ASSERT_TRUE(brain_import_knowledge_base(brain_, tmp_filepath_.c_str()));

    logic_stats_t stats;
    ASSERT_TRUE(brain_get_logic_stats(brain_, &stats));
    EXPECT_EQ(stats.facts_stored, 3u)
        << "Should import all 3 facts from manual file";
}

// Test: Multiple export/import cycles remain consistent
TEST_F(SymbolicLogicIntegrationTest, MultipleRoundTripsAreConsistent) {
    ASSERT_TRUE(initLogic());

    // Add facts
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Cat(garfield)", 0.7f));

    // First export/import cycle
    ASSERT_TRUE(brain_export_knowledge_base(brain_, tmp_filepath_.c_str()));

    brain_t brain2 = createFreshBrain("cycle1");
    ASSERT_NE(brain2, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain2, nullptr));
    ASSERT_TRUE(brain_import_knowledge_base(brain2, tmp_filepath_.c_str()));

    logic_stats_t stats1;
    ASSERT_TRUE(brain_get_logic_stats(brain2, &stats1));

    // Second export/import cycle from brain2
    ASSERT_TRUE(brain_export_knowledge_base(brain2, tmp_filepath2_.c_str()));
    brain_destroy_symbolic_logic(brain2);
    brain_destroy(brain2);

    brain_t brain3 = createFreshBrain("cycle2");
    ASSERT_NE(brain3, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain3, nullptr));
    ASSERT_TRUE(brain_import_knowledge_base(brain3, tmp_filepath2_.c_str()));

    logic_stats_t stats2;
    ASSERT_TRUE(brain_get_logic_stats(brain3, &stats2));

    // Fact count should remain consistent across cycles
    EXPECT_EQ(stats1.facts_stored, stats2.facts_stored)
        << "Multiple round-trips should preserve fact count";

    brain_destroy_symbolic_logic(brain3);
    brain_destroy(brain3);
}

// Test: Stats consistency through export/import lifecycle
TEST_F(SymbolicLogicIntegrationTest, StatsConsistencyThroughLifecycle) {
    ASSERT_TRUE(initLogic());

    // Initial stats: should be empty
    logic_stats_t stats_initial;
    ASSERT_TRUE(brain_get_logic_stats(brain_, &stats_initial));
    EXPECT_EQ(stats_initial.facts_stored, 0u);

    // Add facts
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Cat(garfield)", 0.7f));

    logic_stats_t stats_after_add;
    ASSERT_TRUE(brain_get_logic_stats(brain_, &stats_after_add));
    EXPECT_EQ(stats_after_add.facts_stored, 2u);

    // Export doesn't change stats
    ASSERT_TRUE(brain_export_knowledge_base(brain_, tmp_filepath_.c_str()));

    logic_stats_t stats_after_export;
    ASSERT_TRUE(brain_get_logic_stats(brain_, &stats_after_export));
    EXPECT_EQ(stats_after_export.facts_stored, stats_after_add.facts_stored)
        << "Export should not change source brain stats";
}

// Test: Export/import with custom config
TEST_F(SymbolicLogicIntegrationTest, RoundTripWithCustomConfig) {
    logic_brain_config_t config = {
        .max_facts = 100,
        .max_rules = 50,
        .max_inference_depth = 5,
        .enable_forward_chaining = true,
        .enable_backward_chaining = true,
        .enable_wm_integration = false,
        .enable_exec_integration = false,
        .wm_inference_salience = 0.5f
    };

    ASSERT_TRUE(brain_create_symbolic_logic(brain_, &config));

    ASSERT_TRUE(brain_add_logical_fact(brain_, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Penguin(opus)", 0.8f));

    ASSERT_TRUE(brain_export_knowledge_base(brain_, tmp_filepath_.c_str()));

    // Import into a brain with different config but same capacity
    brain_t brain2 = createFreshBrain("custom_config");
    ASSERT_NE(brain2, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain2, &config));

    ASSERT_TRUE(brain_import_knowledge_base(brain2, tmp_filepath_.c_str()));

    logic_stats_t stats;
    ASSERT_TRUE(brain_get_logic_stats(brain2, &stats));
    EXPECT_EQ(stats.facts_stored, 2u);

    brain_destroy_symbolic_logic(brain2);
    brain_destroy(brain2);
}

// Test: Import file with mixed facts and rules via "->""
TEST_F(SymbolicLogicIntegrationTest, ImportMixedFactsAndRules) {
    ASSERT_TRUE(initLogic());

    // Write file with both facts and rules
    std::string mixed_kb =
        "// Mixed KB\n"
        "Bird(tweety). [salience=0.90]\n"
        "Penguin(opus). [salience=0.80]\n"
        "Bird(x) -> Fly(x). [priority=0.80]\n"
        "\n";

    writeFile(tmp_filepath_, mixed_kb);

    // Import may partially succeed: facts should import, rules may fail if
    // rule parsing is not fully implemented. The import function returns true
    // as long as at least one fact or rule imported successfully.
    bool result = brain_import_knowledge_base(brain_, tmp_filepath_.c_str());
    // Import returns true if any facts/rules were imported
    EXPECT_TRUE(result) << "Import should succeed with at least the facts";

    logic_stats_t stats;
    ASSERT_TRUE(brain_get_logic_stats(brain_, &stats));
    // Should have imported at least the 2 facts
    EXPECT_GE(stats.facts_stored, 2u)
        << "Should have imported at least the 2 facts";
}

// Test: get_last_error is set on export failure
TEST_F(SymbolicLogicIntegrationTest, GetLastErrorSetOnFailure) {
    // No logic engine initialized - export should fail
    bool result = brain_export_knowledge_base(brain_, tmp_filepath_.c_str());
    EXPECT_FALSE(result);

    const char* err = brain_logic_get_last_error();
    ASSERT_NE(err, nullptr);
    EXPECT_GT(strlen(err), 0u)
        << "Last error should be set after failed operation";
}

// Test: Multi-brain knowledge sharing via export/import
TEST_F(SymbolicLogicIntegrationTest, MultiBrainKnowledgeSharing) {
    ASSERT_TRUE(initLogic());

    // Brain 1 has bird knowledge
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain_, "Bird(polly)", 0.85f));
    ASSERT_TRUE(brain_export_knowledge_base(brain_, tmp_filepath_.c_str()));

    // Brain 2 starts with cat knowledge
    brain_t brain2 = createFreshBrain("cat_brain");
    ASSERT_NE(brain2, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain2, nullptr));
    ASSERT_TRUE(brain_add_logical_fact(brain2, "Cat(garfield)", 0.7f));

    // Brain 2 imports brain 1's knowledge
    ASSERT_TRUE(brain_import_knowledge_base(brain2, tmp_filepath_.c_str()));

    logic_stats_t stats;
    ASSERT_TRUE(brain_get_logic_stats(brain2, &stats));
    // Should have cat + bird facts
    EXPECT_GE(stats.facts_stored, 3u)
        << "Brain2 should have its own facts plus imported facts";

    brain_destroy_symbolic_logic(brain2);
    brain_destroy(brain2);
}
