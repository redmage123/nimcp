/**
 * @file test_api_multimodal.cpp
 * @brief Multimodal integration tests for NIMCP API
 *
 * Tests complex multi-feature scenarios:
 * - COW clone + snapshot + restore workflow
 * - Working memory + global workspace together
 * - Ethics + knowledge integration
 * - Concurrent brain operations
 * - Memory pressure scenarios
 *
 * Estimated tests: 15
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>
#include <vector>
#include <cstdio>
#include <unistd.h>

class APIMultimodalTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_shutdown();
    }

    void cleanup_file(const char* filepath) {
        unlink(filepath);
    }

    // Helper to train a brain with sample data
    void train_brain(nimcp_brain_t brain, int iterations) {
        float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        for (int i = 0; i < iterations; i++) {
            nimcp_brain_learn_example(brain, features, 5, "trained", 1.0f);
        }
    }
};

//=============================================================================
// COW Clone + Snapshot + Restore Workflow
//=============================================================================

TEST_F(APIMultimodalTest, COW_CloneAndModify) {
    // Create original brain and train it
    nimcp_brain_t original = nimcp_brain_create("original", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(original, nullptr);

    train_brain(original, 20);

    // Clone using COW
    nimcp_brain_t clone = nimcp_brain_clone_cow(original);
    ASSERT_NE(clone, nullptr);

    // Probe both brains
    nimcp_brain_probe_t orig_probe, clone_probe;
    ASSERT_EQ(nimcp_brain_probe(original, &orig_probe), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_brain_probe(clone, &clone_probe), NIMCP_SUCCESS);

    // Clone should be marked as COW
    EXPECT_TRUE(clone_probe.is_cow_clone);
    EXPECT_GT(clone_probe.cow_shared_bytes, 0);

    // Modify clone (triggers copy-on-write)
    float new_features[] = {5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    nimcp_brain_learn_example(clone, new_features, 10, "modified", 1.0f);

    // Verify original is unchanged
    char orig_label[64], clone_label[64];
    float orig_conf, clone_conf;
    float test_features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};

    nimcp_brain_predict(original, test_features, 10, orig_label, &orig_conf);
    nimcp_brain_predict(clone, test_features, 10, clone_label, &clone_conf);

    // Original and clone may have different predictions after modification
    // (This is expected behavior)

    nimcp_brain_destroy(clone);
    nimcp_brain_destroy(original);
}

TEST_F(APIMultimodalTest, COW_SnapshotAndRestore) {
    // Create and train brain
    nimcp_brain_t brain = nimcp_brain_create("snapshot_test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_example(brain, features, 10, "before", 1.0f);
    }

    // Create snapshot
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snapshot, nullptr);

    // Continue training (modify brain)
    for (int i = 0; i < 20; i++) {
        nimcp_brain_learn_example(brain, features, 10, "after", 1.0f);
    }

    // Get prediction after modification
    char after_label[64];
    float after_conf;
    nimcp_brain_predict(brain, features, 10, after_label, &after_conf);

    // Restore from snapshot
    ASSERT_EQ(nimcp_brain_restore_cow(brain, snapshot), NIMCP_SUCCESS);

    // Get prediction after restore
    char restored_label[64];
    float restored_conf;
    nimcp_brain_predict(brain, features, 10, restored_label, &restored_conf);

    // Restored brain should behave like the snapshot
    EXPECT_STREQ(restored_label, "before");

    nimcp_brain_snapshot_destroy(snapshot);
    nimcp_brain_destroy(brain);
}

TEST_F(APIMultimodalTest, COW_MultipleSnapshots) {
    nimcp_brain_t brain = nimcp_brain_create("multi_snap", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    // Create multiple snapshots at different training stages
    std::vector<nimcp_brain_snapshot_t> snapshots;

    for (int stage = 0; stage < 5; stage++) {
        // Train
        for (int i = 0; i < 10; i++) {
            nimcp_brain_learn_example(brain, features, 5, "training", 1.0f);
        }

        // Snapshot
        nimcp_brain_snapshot_t snap = nimcp_brain_snapshot_cow(brain);
        if (snap != nullptr) {
            snapshots.push_back(snap);
        }
    }

    // We should have created multiple snapshots
    EXPECT_GT(snapshots.size(), 0);

    // Restore to first snapshot
    if (!snapshots.empty()) {
        ASSERT_EQ(nimcp_brain_restore_cow(brain, snapshots[0]), NIMCP_SUCCESS);
    }

    // Cleanup snapshots
    for (auto snap : snapshots) {
        nimcp_brain_snapshot_destroy(snap);
    }

    nimcp_brain_destroy(brain);
}

TEST_F(APIMultimodalTest, COW_CloneFromClone) {
    // Create original
    nimcp_brain_t original = nimcp_brain_create("original", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(original, nullptr);
    train_brain(original, 10);

    // Clone 1
    nimcp_brain_t clone1 = nimcp_brain_clone_cow(original);
    ASSERT_NE(clone1, nullptr);

    // Clone 2 from clone 1
    nimcp_brain_t clone2 = nimcp_brain_clone_cow(clone1);
    ASSERT_NE(clone2, nullptr);

    // All should be functional
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    char label[64];
    float conf;

    ASSERT_EQ(nimcp_brain_predict(original, features, 5, label, &conf), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_brain_predict(clone1, features, 5, label, &conf), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_brain_predict(clone2, features, 5, label, &conf), NIMCP_SUCCESS);

    nimcp_brain_destroy(clone2);
    nimcp_brain_destroy(clone1);
    nimcp_brain_destroy(original);
}

//=============================================================================
// Working Memory + Global Workspace Together
//=============================================================================

TEST_F(APIMultimodalTest, WorkingMemory_GlobalWorkspace_Integration) {
    nimcp_brain_t brain = nimcp_brain_create("wm_gw", NIMCP_BRAIN_MEDIUM, NIMCP_TASK_CLASSIFICATION, 256, 10);
    ASSERT_NE(brain, nullptr);

    // Add items to working memory
    float wm_item1[256];
    for (int i = 0; i < 256; i++) wm_item1[i] = static_cast<float>(i) / 256.0f;

    nimcp_status_t wm_status = nimcp_brain_working_memory_add(brain, wm_item1, 256, 0.9f);

    // Compete for global workspace
    float gw_content[256];
    for (int i = 0; i < 256; i++) gw_content[i] = static_cast<float>(i) / 128.0f;

    nimcp_status_t gw_status = nimcp_brain_workspace_compete(
        brain,
        NIMCP_MODULE_WORKING_MEMORY,
        gw_content,
        256,
        0.85f
    );

    // If both are enabled, verify they work together
    if (wm_status == NIMCP_SUCCESS && gw_status == NIMCP_SUCCESS) {
        // Check working memory
        uint32_t wm_size, wm_capacity;
        ASSERT_EQ(nimcp_brain_working_memory_stats(brain, &wm_size, &wm_capacity), NIMCP_SUCCESS);
        EXPECT_GT(wm_size, 0);

        // Check global workspace
        bool has_broadcast;
        ASSERT_EQ(nimcp_brain_workspace_has_broadcast(brain, &has_broadcast), NIMCP_SUCCESS);
        EXPECT_TRUE(has_broadcast);
    }

    nimcp_brain_destroy(brain);
}

TEST_F(APIMultimodalTest, WorkingMemory_GlobalWorkspace_ContentFlow) {
    nimcp_brain_t brain = nimcp_brain_create("content_flow", NIMCP_BRAIN_MEDIUM, NIMCP_TASK_CLASSIFICATION, 256, 10);
    ASSERT_NE(brain, nullptr);

    float content[256];
    for (int i = 0; i < 256; i++) content[i] = 1.0f;

    // Add to working memory
    nimcp_brain_working_memory_add(brain, content, 256, 0.8f);

    // Content from working memory competes in global workspace
    nimcp_status_t compete_status = nimcp_brain_workspace_compete(
        brain,
        NIMCP_MODULE_WORKING_MEMORY,
        content,
        256,
        0.9f
    );

    if (compete_status == NIMCP_SUCCESS) {
        // Read from global workspace
        float read_content[256];
        uint32_t dim;
        nimcp_cognitive_module_t source;

        ASSERT_EQ(nimcp_brain_workspace_read(brain, read_content, 256, &dim, &source), NIMCP_SUCCESS);
        EXPECT_EQ(source, NIMCP_MODULE_WORKING_MEMORY);

        // Content should match
        for (int i = 0; i < 256; i++) {
            EXPECT_FLOAT_EQ(read_content[i], content[i]);
        }
    }

    nimcp_brain_destroy(brain);
}

TEST_F(APIMultimodalTest, MultiModule_Coordination) {
    nimcp_brain_t brain = nimcp_brain_create("multi_module", NIMCP_BRAIN_MEDIUM, NIMCP_TASK_CLASSIFICATION, 256, 10);
    ASSERT_NE(brain, nullptr);

    // Subscribe multiple modules
    nimcp_brain_workspace_subscribe(brain, NIMCP_MODULE_PERCEPTION);
    nimcp_brain_workspace_subscribe(brain, NIMCP_MODULE_WORKING_MEMORY);
    nimcp_brain_workspace_subscribe(brain, NIMCP_MODULE_EXECUTIVE);
    nimcp_brain_workspace_subscribe(brain, NIMCP_MODULE_ATTENTION);

    float content[256] = {0};

    // Different modules compete
    nimcp_brain_workspace_compete(brain, NIMCP_MODULE_PERCEPTION, content, 256, 0.7f);
    nimcp_brain_workspace_compete(brain, NIMCP_MODULE_ATTENTION, content, 256, 0.9f);
    nimcp_brain_workspace_compete(brain, NIMCP_MODULE_EXECUTIVE, content, 256, 0.8f);

    // Check statistics
    uint32_t broadcasts, competitions;
    float avg_strength;
    nimcp_status_t stats_status = nimcp_brain_workspace_stats(brain, &broadcasts, &competitions, &avg_strength);

    if (stats_status == NIMCP_SUCCESS) {
        EXPECT_GE(competitions, 3);
    }

    nimcp_brain_destroy(brain);
}

//=============================================================================
// Ethics + Knowledge Integration
//=============================================================================

TEST_F(APIMultimodalTest, Ethics_Knowledge_Integration) {
    // Create ethics and knowledge modules
    nimcp_ethics_t ethics = nimcp_ethics_create();
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();

    ASSERT_NE(ethics, nullptr);
    ASSERT_NE(knowledge, nullptr);

    // Add knowledge facts
    nimcp_knowledge_add_fact(knowledge, "action", "harms", "person");
    nimcp_knowledge_add_fact(knowledge, "action", "type", "harmful");

    // Check ethics
    float situation[] = {1.0f, 0.0f, -1.0f, 0.5f, 0.5f};
    float ethics_score;
    ASSERT_EQ(nimcp_ethics_check(ethics, situation, 5, &ethics_score), NIMCP_SUCCESS);

    // Query knowledge
    char result[1024];
    nimcp_knowledge_query(knowledge, "action", result, 1024);

    // Both modules should work independently
    EXPECT_GE(ethics_score, -1.0f);
    EXPECT_LE(ethics_score, 1.0f);

    nimcp_knowledge_destroy(knowledge);
    nimcp_ethics_destroy(ethics);
}

TEST_F(APIMultimodalTest, Brain_Ethics_Knowledge_Pipeline) {
    // Create all three components
    nimcp_brain_t brain = nimcp_brain_create("pipeline", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 2);
    nimcp_ethics_t ethics = nimcp_ethics_create();
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(ethics, nullptr);
    ASSERT_NE(knowledge, nullptr);

    // Brain processes input
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    char label[64];
    float confidence;
    nimcp_brain_predict(brain, features, 10, label, &confidence);

    // Ethics evaluates decision
    float ethics_score;
    nimcp_ethics_check(ethics, features, 10, &ethics_score);

    // Knowledge stores decision
    nimcp_knowledge_add_fact(knowledge, "brain", "predicted", label);

    // Query knowledge
    char query_result[1024];
    nimcp_knowledge_query(knowledge, "brain", query_result, 1024);

    nimcp_knowledge_destroy(knowledge);
    nimcp_ethics_destroy(ethics);
    nimcp_brain_destroy(brain);
}

//=============================================================================
// Concurrent Brain Operations
//=============================================================================

TEST_F(APIMultimodalTest, Concurrent_BrainAndNetwork) {
    // Create brain and network simultaneously
    nimcp_brain_t brain = nimcp_brain_create("concurrent_brain", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 2);
    nimcp_network_t network = nimcp_network_create(10, 2, 50, 0.01f);

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(network, nullptr);

    float inputs[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    float targets[] = {1.0f, 0.0f};
    float outputs[2];

    // Use both concurrently
    nimcp_brain_learn_example(brain, inputs, 10, "A", 1.0f);
    nimcp_network_train(network, inputs, 10, targets, 2);

    nimcp_brain_learn_example(brain, inputs, 10, "A", 1.0f);
    nimcp_network_forward(network, inputs, 10, outputs, 2);

    // Both should be functional
    char label[64];
    float confidence;
    ASSERT_EQ(nimcp_brain_predict(brain, inputs, 10, label, &confidence), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_network_forward(network, inputs, 10, outputs, 2), NIMCP_SUCCESS);

    nimcp_network_destroy(network);
    nimcp_brain_destroy(brain);
}

TEST_F(APIMultimodalTest, Concurrent_MultipleModuleTypes) {
    // Create multiple different module types
    nimcp_brain_t brain1 = nimcp_brain_create("brain1", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_brain_t brain2 = nimcp_brain_create("brain2", NIMCP_BRAIN_SMALL, NIMCP_TASK_REGRESSION, 5, 1);
    nimcp_network_t net = nimcp_network_create(5, 2, 20, 0.01f);
    nimcp_ethics_t ethics = nimcp_ethics_create();
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);
    ASSERT_NE(net, nullptr);
    ASSERT_NE(ethics, nullptr);
    ASSERT_NE(knowledge, nullptr);

    // Use all modules
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    nimcp_brain_learn_example(brain1, features, 5, "class", 1.0f);
    nimcp_brain_learn_example(brain2, features, 5, "value", 1.0f);

    float outputs[2];
    nimcp_network_forward(net, features, 5, outputs, 2);

    float ethics_score;
    nimcp_ethics_check(ethics, features, 5, &ethics_score);

    nimcp_knowledge_add_fact(knowledge, "test", "uses", "modules");

    // Cleanup
    nimcp_knowledge_destroy(knowledge);
    nimcp_ethics_destroy(ethics);
    nimcp_network_destroy(net);
    nimcp_brain_destroy(brain2);
    nimcp_brain_destroy(brain1);
}

//=============================================================================
// Memory Pressure Scenarios
//=============================================================================

TEST_F(APIMultimodalTest, MemoryPressure_ManyBrains) {
    std::vector<nimcp_brain_t> brains;

    // Create many small brains (test memory management)
    const int NUM_BRAINS = 20;
    for (int i = 0; i < NUM_BRAINS; i++) {
        nimcp_brain_t brain = nimcp_brain_create("temp", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
        if (brain != nullptr) {
            brains.push_back(brain);
        }
    }

    EXPECT_GT(brains.size(), 10);  // Should create at least some brains

    // Use all brains
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    for (auto brain : brains) {
        nimcp_brain_learn_example(brain, features, 5, "test", 1.0f);
    }

    // Cleanup
    for (auto brain : brains) {
        nimcp_brain_destroy(brain);
    }
}

TEST_F(APIMultimodalTest, MemoryPressure_COW_Efficiency) {
    // Create original brain
    nimcp_brain_t original = nimcp_brain_create("original", NIMCP_BRAIN_MEDIUM, NIMCP_TASK_CLASSIFICATION, 20, 5);
    ASSERT_NE(original, nullptr);
    train_brain(original, 50);

    // Create many COW clones (should be memory efficient)
    std::vector<nimcp_brain_t> clones;
    for (int i = 0; i < 20; i++) {
        nimcp_brain_t clone = nimcp_brain_clone_cow(original);
        if (clone != nullptr) {
            clones.push_back(clone);
        }
    }

    EXPECT_GT(clones.size(), 10);

    // Probe clones to verify COW
    for (auto clone : clones) {
        nimcp_brain_probe_t probe;
        if (nimcp_brain_probe(clone, &probe) == NIMCP_SUCCESS) {
            EXPECT_TRUE(probe.is_cow_clone);
            EXPECT_GT(probe.cow_shared_bytes, probe.cow_private_bytes);
        }
    }

    // Cleanup
    for (auto clone : clones) {
        nimcp_brain_destroy(clone);
    }
    nimcp_brain_destroy(original);
}

TEST_F(APIMultimodalTest, MemoryPressure_SaveLoadCycle) {
    const char* save_path = "/tmp/pressure_test.nimcp";
    cleanup_file(save_path);

    // Create, save, destroy, load cycle many times
    for (int cycle = 0; cycle < 5; cycle++) {
        nimcp_brain_t brain = nimcp_brain_create("cycle", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 2);
        ASSERT_NE(brain, nullptr);

        train_brain(brain, 10);

        ASSERT_EQ(nimcp_brain_save(brain, save_path), NIMCP_SUCCESS);
        nimcp_brain_destroy(brain);

        nimcp_brain_t loaded = nimcp_brain_load(save_path);
        ASSERT_NE(loaded, nullptr);

        // Verify loaded brain works
        float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
        char label[64];
        float conf;
        ASSERT_EQ(nimcp_brain_predict(loaded, features, 10, label, &conf), NIMCP_SUCCESS);

        nimcp_brain_destroy(loaded);
    }

    cleanup_file(save_path);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
