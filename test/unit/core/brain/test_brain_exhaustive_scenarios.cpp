/**
 * @file test_brain_exhaustive_scenarios.cpp
 * @brief Exhaustive test scenarios to maximize brain.c coverage
 *
 * TARGET: Cover remaining error paths, task variations, and feature interactions
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <cmath>

#include "core/brain/nimcp_brain.h"

class BrainExhaustiveTest : public ::testing::Test {
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

// Test all size + task combinations
TEST_F(BrainExhaustiveTest, AllSizeTaskCombinations) {
    brain_size_t sizes[] = {BRAIN_SIZE_TINY, BRAIN_SIZE_SMALL, BRAIN_SIZE_MEDIUM, BRAIN_SIZE_LARGE};
    brain_task_t tasks[] = {BRAIN_TASK_CLASSIFICATION, BRAIN_TASK_REGRESSION,
                           BRAIN_TASK_PATTERN_MATCHING, BRAIN_TASK_SEQUENCE,
                           BRAIN_TASK_ASSOCIATION, BRAIN_TASK_CUSTOM};

    for (auto size : sizes) {
        for (auto task : tasks) {
            int inputs = (size == BRAIN_SIZE_TINY) ? 5 : (size == BRAIN_SIZE_SMALL) ? 10 : 20;
            int outputs = 3;

            brain = brain_create("combo_test", size, task, inputs, outputs);
            if (brain) {
                // Exercise it
                float* features = new float[inputs];
                for (int i = 0; i < inputs; i++) {
                    features[i] = (float)i / inputs;
                }

                brain_learn_example(brain, features, inputs, "test", 1.0f);
                brain_decision_t* decision = brain_decide(brain, features, inputs);
                if (decision) {
                    brain_free_decision(decision);
                }

                delete[] features;
                brain_destroy(brain);
                brain = nullptr;
            }
        }
    }
}

// Test intensive batch operations
TEST_F(BrainExhaustiveTest, LargeBatchOperations) {
    brain = brain_create("batch_large", BRAIN_SIZE_MEDIUM, BRAIN_TASK_CLASSIFICATION, 20, 5);
    ASSERT_NE(brain, nullptr);

    // Create 200 examples
    std::vector<brain_example_t> examples;
    for (int i = 0; i < 200; i++) {
        brain_example_t ex = {};
        ex.features = new float[20];
        for (int j = 0; j < 20; j++) {
            ex.features[j] = sinf(i * 0.1f + j * 0.05f);
        }
        ex.num_features = 20;
        int class_id = i % 5;
        snprintf(ex.label, sizeof(ex.label), "class_%d", class_id);
        ex.confidence = 0.7f + (i % 30) * 0.01f;
        examples.push_back(ex);
    }

    // Learn in large batch
    float loss = brain_learn_batch(brain, examples.data(), examples.size());
    EXPECT_GE(loss, 0.0f);

    // Batch decisions
    std::vector<const float*> inputs;
    for (const auto& ex : examples) {
        inputs.push_back(ex.features);
    }

    std::vector<brain_decision_t> decisions(examples.size());
    brain_decide_batch(brain, inputs.data(), inputs.size(), 20, decisions.data());

    // Cleanup
    for (auto& ex : examples) {
        delete[] ex.features;
    }
}

// Test learning with varying confidence levels
TEST_F(BrainExhaustiveTest, VaryingConfidenceLearning) {
    brain = brain_create("confidence_var", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features[10];
    for (int i = 0; i < 10; i++) {
        features[i] = (float)i / 10.0f;
    }

    // Learn with different confidence levels
    for (float conf = 0.1f; conf <= 1.0f; conf += 0.1f) {
        brain_learn_example(brain, features, 10, "test", conf);
    }

    // Learn with confidence = 0.0f (edge case)
    brain_learn_example(brain, features, 10, "test", 0.0f);

    // Learn with very low confidence
    brain_learn_example(brain, features, 10, "test", 0.01f);
}

// Test learning with many different labels
TEST_F(BrainExhaustiveTest, ManyDifferentLabels) {
    brain = brain_create("many_labels", BRAIN_SIZE_MEDIUM, BRAIN_TASK_CLASSIFICATION, 15, 20);
    ASSERT_NE(brain, nullptr);

    float features[15];

    // Learn 100 different labels
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 15; j++) {
            features[j] = sinf(i + j * 0.1f);
        }

        char label[64];
        snprintf(label, sizeof(label), "label_%d", i);
        brain_learn_example(brain, features, 15, label, 1.0f);
    }
}

// Test sequence learning extensively
TEST_F(BrainExhaustiveTest, ExtensiveSequenceLearning) {
    brain = brain_create("sequence", BRAIN_SIZE_SMALL, BRAIN_TASK_SEQUENCE, 10, 10);
    ASSERT_NE(brain, nullptr);

    // Learn sequences
    for (int seq = 0; seq < 20; seq++) {
        float features[10];
        for (int i = 0; i < 10; i++) {
            features[i] = sinf(seq * 1.0f + i * 0.5f);
        }
        brain_learn_example(brain, features, 10, "seq", 1.0f);
    }

    // Test sequence prediction
    float test[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    brain_decision_t* decision = brain_decide(brain, test, 10);
    if (decision) {
        brain_free_decision(decision);
    }
}

// Test association task
TEST_F(BrainExhaustiveTest, ExtensiveAssociationLearning) {
    brain = brain_create("association", BRAIN_SIZE_SMALL, BRAIN_TASK_ASSOCIATION, 12, 12);
    ASSERT_NE(brain, nullptr);

    // Learn associations
    for (int i = 0; i < 30; i++) {
        float features[12];
        for (int j = 0; j < 12; j++) {
            features[j] = (i % 2 == 0) ? cosf(j * 0.2f) : sinf(j * 0.2f);
        }
        brain_learn_example(brain, features, 12, "assoc", 1.0f);
    }
}

// Test pattern matching with varying patterns
TEST_F(BrainExhaustiveTest, ExtensivePatternMatching) {
    brain = brain_create("patterns", BRAIN_SIZE_MEDIUM, BRAIN_TASK_PATTERN_MATCHING, 25, 10);
    ASSERT_NE(brain, nullptr);

    // Learn various patterns
    for (int pattern = 0; pattern < 50; pattern++) {
        float features[25];
        for (int i = 0; i < 25; i++) {
            features[i] = (pattern % 3 == 0) ? sinf(i * 0.1f) :
                         (pattern % 3 == 1) ? cosf(i * 0.1f) :
                         tanf(i * 0.05f);
        }
        brain_learn_example(brain, features, 25, "pattern", 1.0f);
    }
}

// Test pruning at various thresholds
TEST_F(BrainExhaustiveTest, PruningVariousThresholds) {
    brain = brain_create("prune_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Train extensively
    float features[10];
    for (int i = 0; i < 50; i++) {
        for (int j = 0; j < 10; j++) {
            features[j] = (float)(i * 10 + j) / 500.0f;
        }
        brain_learn_example(brain, features, 10, "test", 1.0f);
    }

    // Test pruning at different thresholds
    float thresholds[] = {0.0001f, 0.001f, 0.01f, 0.1f, 0.5f};
    for (float thresh : thresholds) {
        uint32_t pruned = brain_prune(brain, thresh);
        // Just call it for coverage
    }

    // Test recommend pruning threshold
    for (float sparsity : {0.5f, 0.7f, 0.9f, 0.95f, 0.99f}) {
        float recommended = brain_recommend_pruning_threshold(brain, sparsity);
        EXPECT_GE(recommended, 0.0f);
    }
}

// Test optimization for inference
TEST_F(BrainExhaustiveTest, OptimizeForInference) {
    brain = brain_create("optimize", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Train
    float features[10];
    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 10; j++) {
            features[j] = (float)(i + j) / 30.0f;
        }
        brain_learn_example(brain, features, 10, "test", 1.0f);
    }

    // Optimize multiple times
    for (int i = 0; i < 3; i++) {
        bool result = brain_optimize_for_inference(brain);
    }
}

// Test top neurons with varying counts
TEST_F(BrainExhaustiveTest, TopNeuronsVaryingCounts) {
    brain = brain_create("top_neurons", BRAIN_SIZE_MEDIUM, BRAIN_TASK_CLASSIFICATION, 20, 5);
    ASSERT_NE(brain, nullptr);

    // Train
    float features[20];
    for (int i = 0; i < 30; i++) {
        for (int j = 0; j < 20; j++) {
            features[j] = (float)(i * j) / 600.0f;
        }
        brain_learn_example(brain, features, 20, "test", 1.0f);
    }

    // Get top neurons with different counts
    for (uint32_t count : {1, 5, 10, 20, 50, 100}) {
        uint32_t* neuron_ids = new uint32_t[count];
        float* importances = new float[count];

        uint32_t returned = brain_get_top_neurons(brain, count, neuron_ids, importances);
        EXPECT_LE(returned, count);

        delete[] neuron_ids;
        delete[] importances;
    }
}

// Test explanation with various inputs
TEST_F(BrainExhaustiveTest, ExplainVariousInputs) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "explain", sizeof(config.task_name) - 1);
    config.enable_explanations = true;
    config.enable_introspection = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Train
    float features[10];
    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 10; j++) {
            features[j] = (float)j / 10.0f;
        }
        brain_learn_example(brain, features, 10, "test", 1.0f);
    }

    // Explain various inputs
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            features[j] = (float)(i + j) / 20.0f;
        }

        char explanation[256];
        brain_explain_decision(brain, features, 10, explanation, sizeof(explanation));
    }
}

// Test save/load cycle with various scenarios
TEST_F(BrainExhaustiveTest, SaveLoadMultipleCycles) {
    const char* filepath = "/tmp/test_brain_exhaustive.bin";

    for (int cycle = 0; cycle < 3; cycle++) {
        brain = brain_create("save_load", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
        ASSERT_NE(brain, nullptr);

        // Train
        float features[10];
        for (int i = 0; i < 10 + cycle * 5; i++) {
            for (int j = 0; j < 10; j++) {
                features[j] = (float)(i + j + cycle) / 30.0f;
            }
            brain_learn_example(brain, features, 10, "test", 1.0f);
        }

        // Save
        bool saved = brain_save(brain, filepath);
        EXPECT_TRUE(saved);

        brain_destroy(brain);

        // Load
        brain = brain_load(filepath);
        EXPECT_NE(brain, nullptr);

        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
}

// Test memory usage tracking
TEST_F(BrainExhaustiveTest, MemoryUsageTracking) {
    std::vector<brain_size_t> sizes = {BRAIN_SIZE_TINY, BRAIN_SIZE_SMALL, BRAIN_SIZE_MEDIUM};

    for (auto size : sizes) {
        brain = brain_create("mem_track", size, BRAIN_TASK_CLASSIFICATION, 10, 3);
        if (brain) {
            size_t mem = brain_get_memory_usage(brain);
            EXPECT_GT(mem, 0u);

            brain_destroy(brain);
            brain = nullptr;
        }
    }
}

// Test stats collection extensively
TEST_F(BrainExhaustiveTest, StatsCollectionExtensive) {
    brain = brain_create("stats", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features[10];

    // Collect stats at various training stages
    for (int stage = 0; stage < 10; stage++) {
        // Train a bit
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                features[j] = (float)(stage * 10 + i + j) / 100.0f;
            }
            brain_learn_example(brain, features, 10, "test", 1.0f);
        }

        // Get stats
        brain_stats_t stats;
        bool result = brain_get_stats(brain, &stats);
        EXPECT_TRUE(result);
        EXPECT_GT(stats.total_learning_steps, 0u);
    }
}

// Test observe action extensively
TEST_F(BrainExhaustiveTest, ObserveActionExtensive) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "observe", sizeof(config.task_name) - 1);
    config.enable_mirror_neurons = true;
    config.mirror_neuron_count = 500;
    config.mirror_max_actions = 50;
    config.mirror_max_agents = 10;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    float features[10];

    // Observe many actions from multiple agents
    for (uint32_t agent_id = 1; agent_id <= 10; agent_id++) {
        for (int action = 0; action < 20; action++) {
            for (int j = 0; j < 10; j++) {
                features[j] = (float)(agent_id * action + j) / 200.0f;
            }
            brain_observe_action(brain, features, 10, agent_id);
        }
    }
}

// Test print info
TEST_F(BrainExhaustiveTest, PrintInfo) {
    brain = brain_create("print_info", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Just call it for coverage
    brain_print_info(brain);
}

// Test get network accessor
TEST_F(BrainExhaustiveTest, GetNetworkAccessor) {
    brain = brain_create("get_net", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    adaptive_network_t net = brain_get_network(brain);
    EXPECT_NE(net, nullptr);
}

// Test error message handling
TEST_F(BrainExhaustiveTest, ErrorMessageHandling) {
    // Trigger various errors
    brain_learn_example(nullptr, nullptr, 0, nullptr, 0.0f);

    const char* error1 = brain_get_last_error();
    EXPECT_NE(error1, nullptr);

    brain_clear_error();

    const char* error2 = brain_get_last_error();
    // After clear, error might be NULL or empty

    // Trigger more errors
    brain_decide(nullptr, nullptr, 0);
    brain_get_last_error();
    brain_clear_error();
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
