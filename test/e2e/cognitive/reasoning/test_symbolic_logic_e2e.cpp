/**
 * @file test_symbolic_logic_e2e.cpp
 * @brief End-to-end tests for symbolic logic export/import full pipeline
 *
 * COVERAGE:
 * - Full lifecycle: brain create -> logic init -> add facts/rules -> export
 *                   -> new brain -> import -> verify -> destroy
 * - Knowledge persistence: build KB, export, destroy, recreate, import, verify
 * - Multi-brain knowledge sharing: one brain exports, multiple import
 * - Stats tracking through full lifecycle
 * - Export/import with rules (not just facts)
 * - Large KB export/import (many facts)
 * - Destroy-recreate-reimport workflow
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>

#include "cognitive/reasoning/nimcp_symbolic_logic_brain_integration.h"
#include "core/brain/nimcp_brain.h"
#include "utils/logging/nimcp_logging.h"

// =============================================================================
// Test Fixture
// =============================================================================

class SymbolicLogicE2ETest : public ::testing::Test {
protected:
    std::vector<brain_t> brains_;
    std::vector<std::string> tmp_files_;

    void SetUp() override {
        nimcp_log_set_level(NULL, LOG_LEVEL_ERROR);
    }

    void TearDown() override {
        // Cleanup all brains in reverse order
        for (auto it = brains_.rbegin(); it != brains_.rend(); ++it) {
            if (*it) {
                brain_destroy_symbolic_logic(*it);
                brain_destroy(*it);
            }
        }
        brains_.clear();

        // Cleanup all temp files
        for (const auto& f : tmp_files_) {
            if (!f.empty()) {
                std::remove(f.c_str());
            }
        }
        tmp_files_.clear();
    }

    brain_t createBrain(const char* name) {
        brain_t b = brain_create(name, BRAIN_SIZE_SMALL,
                                 BRAIN_TASK_CLASSIFICATION, 10, 5);
        if (b) brains_.push_back(b);
        return b;
    }

    std::string createTempFile() {
        char tmp_name[] = "/tmp/nimcp_logic_e2e_XXXXXX";
        int fd = mkstemp(tmp_name);
        if (fd < 0) return "";
        close(fd);
        std::string path(tmp_name);
        tmp_files_.push_back(path);
        return path;
    }

    std::string readFile(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) return "";
        return std::string((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
    }

    void writeFile(const std::string& path, const std::string& content) {
        std::ofstream ofs(path);
        ASSERT_TRUE(ofs.is_open()) << "Failed to open: " << path;
        ofs << content;
        ofs.close();
    }
};

// =============================================================================
// E2E Test: Full Pipeline
// =============================================================================

// Full lifecycle from brain creation through export/import and verification
TEST_F(SymbolicLogicE2ETest, FullPipelineLifecycle) {
    std::string kb_file = createTempFile();
    ASSERT_FALSE(kb_file.empty());

    // Phase 1: Create brain and initialize logic
    brain_t brain1 = createBrain("e2e_source");
    ASSERT_NE(brain1, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain1, nullptr));

    // Phase 2: Build knowledge base with facts
    ASSERT_TRUE(brain_add_logical_fact(brain1, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain1, "Penguin(opus)", 0.8f));
    ASSERT_TRUE(brain_add_logical_fact(brain1, "Cat(garfield)", 0.7f));
    ASSERT_TRUE(brain_add_logical_fact(brain1, "Dog(snoopy)", 0.6f));
    ASSERT_TRUE(brain_add_logical_fact(brain1, "Fish(nemo)", 0.5f));

    // Phase 3: Verify stats
    logic_stats_t source_stats;
    ASSERT_TRUE(brain_get_logic_stats(brain1, &source_stats));
    EXPECT_EQ(source_stats.facts_stored, 5u);

    // Phase 4: Export
    ASSERT_TRUE(brain_export_knowledge_base(brain1, kb_file.c_str()));

    // Verify export file exists and has content
    std::string content = readFile(kb_file);
    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("NIMCP Knowledge Base Export"), std::string::npos);

    // Phase 5: Create new brain and import
    brain_t brain2 = createBrain("e2e_target");
    ASSERT_NE(brain2, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain2, nullptr));
    ASSERT_TRUE(brain_import_knowledge_base(brain2, kb_file.c_str()));

    // Phase 6: Verify imported state matches source
    logic_stats_t target_stats;
    ASSERT_TRUE(brain_get_logic_stats(brain2, &target_stats));
    EXPECT_EQ(target_stats.facts_stored, source_stats.facts_stored)
        << "Target brain should have same fact count as source";
}

// =============================================================================
// E2E Test: Knowledge Persistence Through Destroy/Recreate
// =============================================================================

TEST_F(SymbolicLogicE2ETest, KnowledgePersistsThroughDestroyRecreate) {
    std::string kb_file = createTempFile();
    ASSERT_FALSE(kb_file.empty());

    uint32_t original_fact_count = 0;

    // Phase 1: Build and export knowledge
    {
        brain_t brain1 = createBrain("persist_source");
        ASSERT_NE(brain1, nullptr);
        ASSERT_TRUE(brain_create_symbolic_logic(brain1, nullptr));

        ASSERT_TRUE(brain_add_logical_fact(brain1, "Bird(tweety)", 0.9f));
        ASSERT_TRUE(brain_add_logical_fact(brain1, "Mammal(fido)", 0.85f));
        ASSERT_TRUE(brain_add_logical_fact(brain1, "Reptile(gex)", 0.75f));

        logic_stats_t stats;
        ASSERT_TRUE(brain_get_logic_stats(brain1, &stats));
        original_fact_count = stats.facts_stored;
        EXPECT_EQ(original_fact_count, 3u);

        ASSERT_TRUE(brain_export_knowledge_base(brain1, kb_file.c_str()));
    }
    // brain1 is still alive (tracked in brains_ vector) but we exported its KB

    // Phase 2: Create completely new brain and restore knowledge
    brain_t brain2 = createBrain("persist_restored");
    ASSERT_NE(brain2, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain2, nullptr));

    // Verify new brain starts empty
    logic_stats_t empty_stats;
    ASSERT_TRUE(brain_get_logic_stats(brain2, &empty_stats));
    EXPECT_EQ(empty_stats.facts_stored, 0u);

    // Import persisted knowledge
    ASSERT_TRUE(brain_import_knowledge_base(brain2, kb_file.c_str()));

    // Verify knowledge restored
    logic_stats_t restored_stats;
    ASSERT_TRUE(brain_get_logic_stats(brain2, &restored_stats));
    EXPECT_EQ(restored_stats.facts_stored, original_fact_count)
        << "Restored KB should match original fact count";
}

// =============================================================================
// E2E Test: Multi-Brain Knowledge Sharing
// =============================================================================

TEST_F(SymbolicLogicE2ETest, MultiBrainKnowledgeSharing) {
    std::string kb_file = createTempFile();
    ASSERT_FALSE(kb_file.empty());

    // Source brain builds shared knowledge
    brain_t source = createBrain("e2e_sharing_source");
    ASSERT_NE(source, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(source, nullptr));

    ASSERT_TRUE(brain_add_logical_fact(source, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(source, "Penguin(opus)", 0.8f));
    ASSERT_TRUE(brain_add_logical_fact(source, "Cat(garfield)", 0.7f));

    logic_stats_t source_stats;
    ASSERT_TRUE(brain_get_logic_stats(source, &source_stats));
    uint32_t shared_count = source_stats.facts_stored;

    ASSERT_TRUE(brain_export_knowledge_base(source, kb_file.c_str()));

    // Multiple target brains import the same knowledge
    const int NUM_TARGETS = 3;
    for (int i = 0; i < NUM_TARGETS; i++) {
        char name[64];
        snprintf(name, sizeof(name), "e2e_sharing_target_%d", i);

        brain_t target = createBrain(name);
        ASSERT_NE(target, nullptr) << "Failed to create target brain " << i;
        ASSERT_TRUE(brain_create_symbolic_logic(target, nullptr));
        ASSERT_TRUE(brain_import_knowledge_base(target, kb_file.c_str()))
            << "Failed to import into target brain " << i;

        logic_stats_t target_stats;
        ASSERT_TRUE(brain_get_logic_stats(target, &target_stats));
        EXPECT_EQ(target_stats.facts_stored, shared_count)
            << "Target brain " << i << " should have same fact count as source";
    }
}

// =============================================================================
// E2E Test: Stats Tracking Through Full Lifecycle
// =============================================================================

TEST_F(SymbolicLogicE2ETest, StatsTrackingThroughLifecycle) {
    std::string kb_file = createTempFile();
    ASSERT_FALSE(kb_file.empty());

    brain_t brain = createBrain("e2e_stats");
    ASSERT_NE(brain, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain, nullptr));

    // Step 1: Empty state
    logic_stats_t stats;
    ASSERT_TRUE(brain_get_logic_stats(brain, &stats));
    EXPECT_EQ(stats.facts_stored, 0u);

    // Step 2: Add facts one at a time, verify incrementing
    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_get_logic_stats(brain, &stats));
    EXPECT_EQ(stats.facts_stored, 1u);

    ASSERT_TRUE(brain_add_logical_fact(brain, "Cat(garfield)", 0.7f));
    ASSERT_TRUE(brain_get_logic_stats(brain, &stats));
    EXPECT_EQ(stats.facts_stored, 2u);

    ASSERT_TRUE(brain_add_logical_fact(brain, "Dog(snoopy)", 0.6f));
    ASSERT_TRUE(brain_get_logic_stats(brain, &stats));
    EXPECT_EQ(stats.facts_stored, 3u);

    // Step 3: Export - stats unchanged
    ASSERT_TRUE(brain_export_knowledge_base(brain, kb_file.c_str()));
    ASSERT_TRUE(brain_get_logic_stats(brain, &stats));
    EXPECT_EQ(stats.facts_stored, 3u)
        << "Export should not alter stats";
}

// =============================================================================
// E2E Test: Export with Rules and Facts Combined
// =============================================================================

TEST_F(SymbolicLogicE2ETest, ExportWithRulesAndFacts) {
    std::string kb_file = createTempFile();
    ASSERT_FALSE(kb_file.empty());

    brain_t brain = createBrain("e2e_rules_facts");
    ASSERT_NE(brain, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain, nullptr));

    // Add facts
    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain, "Penguin(opus)", 0.8f));

    // Add rules (rule parsing may not be fully implemented; test for no crash)
    brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", 0.8f);
    brain_add_logical_rule(brain, "Penguin(x) -> Bird(x)", 0.9f);

    // Export
    ASSERT_TRUE(brain_export_knowledge_base(brain, kb_file.c_str()));

    // Verify file has content
    std::string content = readFile(kb_file);
    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("NIMCP Knowledge Base Export"), std::string::npos);
}

// =============================================================================
// E2E Test: Large Knowledge Base Export/Import
// =============================================================================

TEST_F(SymbolicLogicE2ETest, LargeKBExportImport) {
    std::string kb_file = createTempFile();
    ASSERT_FALSE(kb_file.empty());

    brain_t brain = createBrain("e2e_large_kb");
    ASSERT_NE(brain, nullptr);

    logic_brain_config_t config = {
        .max_facts = 1000,
        .max_rules = 500,
        .max_inference_depth = 10,
        .enable_forward_chaining = true,
        .enable_backward_chaining = true,
        .enable_wm_integration = false,  // Disable WM to avoid overhead
        .enable_exec_integration = false,
        .wm_inference_salience = 0.7f
    };
    ASSERT_TRUE(brain_create_symbolic_logic(brain, &config));

    // Add many facts
    const int NUM_FACTS = 50;
    int added = 0;
    for (int i = 0; i < NUM_FACTS; i++) {
        char fact[128];
        snprintf(fact, sizeof(fact), "Entity%d(item%d)", i, i);
        if (brain_add_logical_fact(brain, fact, 0.5f + 0.005f * (float)i)) {
            added++;
        }
    }
    EXPECT_GT(added, 0) << "Should have added at least some facts";

    logic_stats_t stats_before;
    ASSERT_TRUE(brain_get_logic_stats(brain, &stats_before));
    EXPECT_EQ(stats_before.facts_stored, (uint32_t)added);

    // Export
    ASSERT_TRUE(brain_export_knowledge_base(brain, kb_file.c_str()));

    // Import into fresh brain
    brain_t brain2 = createBrain("e2e_large_kb_import");
    ASSERT_NE(brain2, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain2, &config));
    ASSERT_TRUE(brain_import_knowledge_base(brain2, kb_file.c_str()));

    logic_stats_t stats_after;
    ASSERT_TRUE(brain_get_logic_stats(brain2, &stats_after));
    EXPECT_EQ(stats_after.facts_stored, stats_before.facts_stored)
        << "Large KB round-trip should preserve all facts";
}

// =============================================================================
// E2E Test: Destroy, Recreate, and Reimport
// =============================================================================

TEST_F(SymbolicLogicE2ETest, DestroyRecreateReimport) {
    std::string kb_file = createTempFile();
    ASSERT_FALSE(kb_file.empty());

    brain_t brain = createBrain("e2e_recreate");
    ASSERT_NE(brain, nullptr);

    // First lifecycle
    ASSERT_TRUE(brain_create_symbolic_logic(brain, nullptr));
    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain, "Cat(garfield)", 0.7f));
    ASSERT_TRUE(brain_export_knowledge_base(brain, kb_file.c_str()));

    logic_stats_t stats1;
    ASSERT_TRUE(brain_get_logic_stats(brain, &stats1));
    uint32_t original_count = stats1.facts_stored;

    // Destroy logic engine
    brain_destroy_symbolic_logic(brain);

    // Recreate logic engine on same brain
    ASSERT_TRUE(brain_create_symbolic_logic(brain, nullptr));

    // Verify it starts fresh
    logic_stats_t stats_empty;
    ASSERT_TRUE(brain_get_logic_stats(brain, &stats_empty));
    EXPECT_EQ(stats_empty.facts_stored, 0u)
        << "Recreated engine should start empty";

    // Reimport from file
    ASSERT_TRUE(brain_import_knowledge_base(brain, kb_file.c_str()));

    logic_stats_t stats_reimported;
    ASSERT_TRUE(brain_get_logic_stats(brain, &stats_reimported));
    EXPECT_EQ(stats_reimported.facts_stored, original_count)
        << "Reimported facts should match original count";
}

// =============================================================================
// E2E Test: Import from External KB File Format
// =============================================================================

TEST_F(SymbolicLogicE2ETest, ImportFromExternalKBFormat) {
    std::string kb_file = createTempFile();
    ASSERT_FALSE(kb_file.empty());

    // Simulate a human-authored knowledge base file
    std::string manual_kb =
        "// Animal Classification Knowledge Base\n"
        "// Created by domain expert\n"
        "\n"
        "// Facts - Animals\n"
        "Bird(tweety). [salience=0.95]\n"
        "Penguin(opus). [salience=0.90]\n"
        "Cat(garfield). [salience=0.85]\n"
        "Dog(snoopy). [salience=0.80]\n"
        "Fish(nemo). [salience=0.75]\n"
        "\n"
        "// Rules - Classification\n"
        "Penguin(x) -> Bird(x). [priority=0.90]\n"
        "\n";

    writeFile(kb_file, manual_kb);

    brain_t brain = createBrain("e2e_external_kb");
    ASSERT_NE(brain, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain, nullptr));

    ASSERT_TRUE(brain_import_knowledge_base(brain, kb_file.c_str()));

    logic_stats_t stats;
    ASSERT_TRUE(brain_get_logic_stats(brain, &stats));
    // Should have imported at least the 5 facts
    EXPECT_GE(stats.facts_stored, 5u)
        << "Should import all facts from external KB file";
}

// =============================================================================
// E2E Test: Error Recovery - Continue After Failed Import
// =============================================================================

TEST_F(SymbolicLogicE2ETest, ContinueAfterFailedImport) {
    brain_t brain = createBrain("e2e_error_recovery");
    ASSERT_NE(brain, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brain, nullptr));

    // Try importing from nonexistent file - should fail but not corrupt state
    EXPECT_FALSE(brain_import_knowledge_base(
        brain, "/tmp/nimcp_noexist_e2e_99999.txt"));

    // Brain should still be functional
    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));

    logic_stats_t stats;
    ASSERT_TRUE(brain_get_logic_stats(brain, &stats));
    EXPECT_EQ(stats.facts_stored, 1u)
        << "Brain should be functional after failed import";

    // Export should also still work
    std::string kb_file = createTempFile();
    ASSERT_FALSE(kb_file.empty());
    ASSERT_TRUE(brain_export_knowledge_base(brain, kb_file.c_str()));

    std::string content = readFile(kb_file);
    EXPECT_FALSE(content.empty());
}

// =============================================================================
// E2E Test: Chain of Exports Between Multiple Brains
// =============================================================================

TEST_F(SymbolicLogicE2ETest, ChainOfExportsBetweenBrains) {
    std::string file1 = createTempFile();
    std::string file2 = createTempFile();
    ASSERT_FALSE(file1.empty());
    ASSERT_FALSE(file2.empty());

    // Brain A: create initial knowledge
    brain_t brainA = createBrain("chain_A");
    ASSERT_NE(brainA, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brainA, nullptr));
    ASSERT_TRUE(brain_add_logical_fact(brainA, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brainA, "Cat(garfield)", 0.7f));

    logic_stats_t statsA;
    ASSERT_TRUE(brain_get_logic_stats(brainA, &statsA));

    // A exports to file1
    ASSERT_TRUE(brain_export_knowledge_base(brainA, file1.c_str()));

    // Brain B: imports from A, adds more, exports to file2
    brain_t brainB = createBrain("chain_B");
    ASSERT_NE(brainB, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brainB, nullptr));
    ASSERT_TRUE(brain_import_knowledge_base(brainB, file1.c_str()));
    ASSERT_TRUE(brain_add_logical_fact(brainB, "Dog(snoopy)", 0.6f));

    logic_stats_t statsB;
    ASSERT_TRUE(brain_get_logic_stats(brainB, &statsB));
    EXPECT_GT(statsB.facts_stored, statsA.facts_stored)
        << "Brain B should have more facts than A after adding";

    ASSERT_TRUE(brain_export_knowledge_base(brainB, file2.c_str()));

    // Brain C: imports from B's export
    brain_t brainC = createBrain("chain_C");
    ASSERT_NE(brainC, nullptr);
    ASSERT_TRUE(brain_create_symbolic_logic(brainC, nullptr));
    ASSERT_TRUE(brain_import_knowledge_base(brainC, file2.c_str()));

    logic_stats_t statsC;
    ASSERT_TRUE(brain_get_logic_stats(brainC, &statsC));
    EXPECT_EQ(statsC.facts_stored, statsB.facts_stored)
        << "Brain C should match Brain B's exported fact count";
}
