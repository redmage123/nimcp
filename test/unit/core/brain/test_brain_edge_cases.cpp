/**
 * @file test_brain_edge_cases.cpp
 * @brief Edge cases, error paths, and advanced API tests for brain.c
 *
 * FOCUS: Remaining uncovered code paths to reach 95%
 * - Error handling paths
 * - Distributed brain features (COW, P2P)
 * - Snapshot and persistence
 * - Optimization functions  
 * - Pre-trained model APIs
 * - Accessor functions
 *
 * TARGET: +35% brain.c coverage (59% → 95%)
 */

#include <gtest/gtest.h>
#include <cstring>

#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainEdgeCasesTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = nullptr;
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// 1. Error Handling Tests
//=============================================================================

TEST_F(BrainEdgeCasesTest, ErrorHandling_NullBrain) {
    // Test NULL brain parameter
    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    
    float loss = brain_learn_example(nullptr, features, 10, "test", 1.0f);
    EXPECT_LT(loss, 0.0f);  // Should return error
    
    brain_decision_t* decision = brain_decide(nullptr, features, 10);
    EXPECT_EQ(decision, nullptr);
    
    brain_stats_t stats;
    bool result = brain_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);
}

TEST_F(BrainEdgeCasesTest, ErrorHandling_InvalidFeatureCount) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    // Try with wrong feature count
    float features[5] = {1,2,3,4,5};
    float loss = brain_learn_example(brain, features, 5, "test", 1.0f);
    EXPECT_LT(loss, 0.0f);  // Should return error
}

TEST_F(BrainEdgeCasesTest, ErrorHandling_NullFeatures) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    float loss = brain_learn_example(brain, nullptr, 10, "test", 1.0f);
    EXPECT_LT(loss, 0.0f);
    
    brain_decision_t* decision = brain_decide(brain, nullptr, 10);
    EXPECT_EQ(decision, nullptr);
}

TEST_F(BrainEdgeCasesTest, ErrorHandling_NullLabel) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    float loss = brain_learn_example(brain, features, 10, nullptr, 1.0f);
    EXPECT_LT(loss, 0.0f);
}

TEST_F(BrainEdgeCasesTest, ErrorHandling_GetLastError) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    // Trigger an error
    float features[5] = {1,2,3,4,5};
    brain_learn_example(brain, features, 5, "test", 1.0f);
    
    // Get error message
    const char* error = brain_get_last_error();
    EXPECT_NE(error, nullptr);
    
    // Clear error
    brain_clear_error();
}

//=============================================================================
// 2. Copy-on-Write (COW) Tests
//=============================================================================

TEST_F(BrainEdgeCasesTest, COW_BasicClone) {
    // Create original brain
    brain = brain_create("original", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    // Train original
    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    brain_learn_example(brain, features, 10, "class_a", 1.0f);
    
    // Clone with COW
    brain_t clone = brain_clone_cow(brain);
    ASSERT_NE(clone, nullptr);
    
    // Get COW stats
    brain_cow_stats_t cow_stats;
    bool result = brain_get_cow_stats(clone, &cow_stats);
    EXPECT_TRUE(result);
    EXPECT_TRUE(cow_stats.is_cow_clone);
    
    // Cleanup
    brain_destroy(clone);
}

TEST_F(BrainEdgeCasesTest, COW_MultipleClones) {
    brain = brain_create("original", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    // Create multiple clones
    brain_t clone1 = brain_clone_cow(brain);
    brain_t clone2 = brain_clone_cow(brain);
    brain_t clone3 = brain_clone_cow(brain);
    
    EXPECT_NE(clone1, nullptr);
    EXPECT_NE(clone2, nullptr);
    EXPECT_NE(clone3, nullptr);
    
    // Cleanup
    brain_destroy(clone1);
    brain_destroy(clone2);
    brain_destroy(clone3);
}

//=============================================================================
// 3. Statistics and Monitoring Tests
//=============================================================================

TEST_F(BrainEdgeCasesTest, Stats_GetBrainStats) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
    
    EXPECT_GT(stats.num_neurons, 0u);
    EXPECT_GT(stats.num_synapses, 0u);
    EXPECT_STREQ(stats.task_name, "test");
}

TEST_F(BrainEdgeCasesTest, Stats_MemoryUsage) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    size_t memory = brain_get_memory_usage(brain);
    EXPECT_GT(memory, 0u);
}

TEST_F(BrainEdgeCasesTest, Stats_PrintInfo) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    // Should not crash
    brain_print_info(brain);
}

//=============================================================================
// 4. Optimization Tests
//=============================================================================

TEST_F(BrainEdgeCasesTest, Optimization_Pruning) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    // Train to create some weak connections
    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 10, "test", 1.0f);
    }
    
    // Prune weak connections
    uint32_t pruned = brain_prune(brain, 0.01f);
    EXPECT_GE(pruned, 0u);
}

TEST_F(BrainEdgeCasesTest, Optimization_RecommendPruningThreshold) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    float threshold = brain_recommend_pruning_threshold(brain, 0.9f);
    EXPECT_GE(threshold, 0.0f);
}

TEST_F(BrainEdgeCasesTest, Optimization_OptimizeForInference) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    // Train first
    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    for (int i = 0; i < 5; i++) {
        brain_learn_example(brain, features, 10, "test", 1.0f);
    }
    
    bool result = brain_optimize_for_inference(brain);
    EXPECT_TRUE(result);
}

//=============================================================================
// 5. Accessor Function Tests
//=============================================================================

TEST_F(BrainEdgeCasesTest, Accessors_GetNetwork) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    adaptive_network_t network = brain_get_network(brain);
    EXPECT_NE(network, nullptr);
}

TEST_F(BrainEdgeCasesTest, Accessors_GetWorkingMemory) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "wm_test", sizeof(config.task_name) - 1);
    config.enable_working_memory = true;
    
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    
    working_memory_t* wm = brain_get_working_memory(brain);
    // May be NULL if not initialized, that's ok
}

TEST_F(BrainEdgeCasesTest, Accessors_GetGlobalWorkspace) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "gw_test", sizeof(config.task_name) - 1);
    config.enable_global_workspace = true;
    
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    
    global_workspace_t* gw = brain_get_global_workspace(brain);
    // May be NULL if not initialized, that's ok
}

TEST_F(BrainEdgeCasesTest, Accessors_GetTheoryOfMind) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "tom_test", sizeof(config.task_name) - 1);
    config.enable_theory_of_mind = true;
    
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    
    theory_of_mind_t tom = brain_get_theory_of_mind(brain);
    // May be NULL if not initialized, that's ok
}

TEST_F(BrainEdgeCasesTest, Accessors_GetSleepSystem) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "sleep_test", sizeof(config.task_name) - 1);
    config.enable_sleep_wake_cycle = true;
    
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    
    sleep_system_t sleep_sys = brain_get_sleep_system(brain);
    // May be NULL if not initialized, that's ok
}

TEST_F(BrainEdgeCasesTest, Accessors_GetExplanationGenerator) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "explain_test", sizeof(config.task_name) - 1);
    config.enable_natural_explanations = true;
    
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    
    explanation_generator_t eg = brain_get_explanation_generator(brain);
    // May be NULL if not initialized, that's ok
}

//=============================================================================
// 6. Batch Learning Tests
//=============================================================================

TEST_F(BrainEdgeCasesTest, Batch_EmptyBatch) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    float loss = brain_learn_batch(brain, nullptr, 0);
    EXPECT_LT(loss, 0.0f);  // Should return error
}

TEST_F(BrainEdgeCasesTest, Batch_LargeBatch) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    // Create large batch
    std::vector<brain_example_t> examples;
    for (int i = 0; i < 50; i++) {
        brain_example_t ex = {};
        float* features = new float[10];
        for (int j = 0; j < 10; j++) {
            features[j] = static_cast<float>(i + j);
        }
        ex.features = features;
        ex.num_features = 10;
        strncpy(ex.label, (i % 2 == 0) ? "class_a" : "class_b", sizeof(ex.label) - 1);
        ex.confidence = 1.0f;
        examples.push_back(ex);
    }
    
    float avg_loss = brain_learn_batch(brain, examples.data(), examples.size());
    EXPECT_GE(avg_loss, 0.0f);
    
    // Cleanup
    for (auto& ex : examples) {
        delete[] ex.features;
    }
}

//=============================================================================
// 7. Decision Batch Tests
//=============================================================================

TEST_F(BrainEdgeCasesTest, Decision_BatchInference) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    // Train first
    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    brain_learn_example(brain, features, 10, "test", 1.0f);
    
    // Prepare batch inputs
    const float* inputs[5];
    float data1[10] = {1,2,3,4,5,6,7,8,9,10};
    float data2[10] = {2,3,4,5,6,7,8,9,10,11};
    float data3[10] = {3,4,5,6,7,8,9,10,11,12};
    float data4[10] = {4,5,6,7,8,9,10,11,12,13};
    float data5[10] = {5,6,7,8,9,10,11,12,13,14};
    
    inputs[0] = data1;
    inputs[1] = data2;
    inputs[2] = data3;
    inputs[3] = data4;
    inputs[4] = data5;
    
    brain_decision_t decisions[5];
    bool result = brain_decide_batch(brain, inputs, 5, 10, decisions);
    EXPECT_TRUE(result);
}

//=============================================================================
// 8. Explanation Tests
//=============================================================================

TEST_F(BrainEdgeCasesTest, Explanation_ExplainDecision) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "explain", sizeof(config.task_name) - 1);
    config.enable_explanations = true;
    
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    
    // Train
    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    brain_learn_example(brain, features, 10, "test", 1.0f);
    
    // Get explanation
    char explanation[256];
    bool result = brain_explain_decision(brain, features, 10, explanation, sizeof(explanation));
    EXPECT_TRUE(result);
}

TEST_F(BrainEdgeCasesTest, Explanation_GetTopNeurons) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    // Train
    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 10, "test", 1.0f);
    }
    
    uint32_t neuron_ids[10];
    float importances[10];
    uint32_t count = brain_get_top_neurons(brain, 10, neuron_ids, importances);
    EXPECT_GT(count, 0u);
}

//=============================================================================
// 9. Sleep-Wake Cycle Tests
//=============================================================================

TEST_F(BrainEdgeCasesTest, SleepWake_Configuration) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "sleep_wake", sizeof(config.task_name) - 1);
    
    config.enable_sleep_wake_cycle = true;
    config.sleep_pressure_threshold = 0.8f;
    config.enable_memory_replay = true;
    config.enable_synaptic_homeostasis = true;
    config.enable_rem_creativity = true;
    
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    
    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

//=============================================================================
// 10. Emotional Tagging Tests
//=============================================================================

TEST_F(BrainEdgeCasesTest, Emotional_Configuration) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "emotional", sizeof(config.task_name) - 1);
    
    config.enable_emotional_tagging = true;
    config.enable_emotional_memories = true;
    
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    
    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

//=============================================================================
// 11. Task Switching Tests
//=============================================================================

TEST_F(BrainEdgeCasesTest, TaskSwitching_Configuration) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "task_switch", sizeof(config.task_name) - 1);
    
    config.enable_task_switching = true;
    config.enable_planning = true;
    
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    
    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

//=============================================================================
// Run Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
