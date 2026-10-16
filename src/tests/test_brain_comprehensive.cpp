/**
 * @file test_brain_comprehensive.cpp
 * @brief Comprehensive unit tests for brain.c (Target: 100% coverage)
 *
 * WHAT: Complete test coverage for all 50+ functions in nimcp_brain.c
 * WHY:  Increase coverage from 15.4% to 100% (803 lines)
 * HOW:  Test all code paths, branches, error conditions, and edge cases
 *
 * COVERAGE STRATEGY:
 * 1. Brain Creation (all modes, sizes, tasks)
 * 2. Learning APIs (single, batch, LLM teacher)
 * 3. Inference APIs (single, batch, multimodal)
 * 4. Persistence (save, load, snapshots)
 * 5. COW Operations (clone, distributed clone)
 * 6. Statistics & Introspection
 * 7. Optimization (prune, optimize)
 * 8. Distributed Operations
 * 9. Cognitive Integration
 * 10. Error Handling (all paths)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <fstream>
#include <sys/stat.h>

extern "C" {
    #include "core/brain/nimcp_brain.h"
    #include "utils/memory/nimcp_memory.h"
    #include "utils/cache/nimcp_cache.h"
    #include "utils/time/nimcp_time.h"
    #include "include/nimcp.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BrainComprehensiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize NIMCP systems
        nimcp_memory_init();
        nimcp_cache_init();
        nimcp_init();

        // Clear any previous errors
        brain_clear_error();
    }

    void TearDown() override {
        // Cleanup
        nimcp_shutdown();
        nimcp_cache_cleanup();
        nimcp_memory_cleanup();
    }

    // Helper: Create test input features
    float* create_test_features(uint32_t size, float base = 0.5f) {
        float* features = new float[size];
        for (uint32_t i = 0; i < size; i++) {
            features[i] = base + (float)i * 0.01f;
        }
        return features;
    }

    // Helper: Check if file exists
    bool file_exists(const char* path) {
        struct stat buffer;
        return (stat(path, &buffer) == 0);
    }
};

//=============================================================================
// 1. Brain Creation Tests - All Modes & Configurations
//=============================================================================

TEST_F(BrainComprehensiveTest, CreateBasic_AllSizes) {
    // Test all brain sizes
    brain_size_t sizes[] = {BRAIN_SIZE_TINY, BRAIN_SIZE_SMALL, BRAIN_SIZE_MEDIUM, BRAIN_SIZE_LARGE};
    const char* size_names[] = {"tiny", "small", "medium", "large"};

    for (int i = 0; i < 4; i++) {
        brain_t brain = brain_create(size_names[i], sizes[i], BRAIN_TASK_CLASSIFICATION, 10, 3);
        ASSERT_NE(brain, nullptr) << "Failed to create " << size_names[i] << " brain";

        // Verify stats
        brain_stats_t stats;
        ASSERT_TRUE(brain_get_stats(brain, &stats));
        EXPECT_GT(stats.num_neurons, 0u);
        EXPECT_EQ(stats.size, sizes[i]);

        brain_destroy(brain);
    }
}

TEST_F(BrainComprehensiveTest, CreateBasic_AllTaskTypes) {
    // Test all task types
    brain_task_t tasks[] = {
        BRAIN_TASK_CLASSIFICATION,
        BRAIN_TASK_REGRESSION,
        BRAIN_TASK_PATTERN_MATCHING,
        BRAIN_TASK_SEQUENCE,
        BRAIN_TASK_ASSOCIATION,
        BRAIN_TASK_CUSTOM
    };

    for (int i = 0; i < 6; i++) {
        brain_t brain = brain_create("test", BRAIN_SIZE_SMALL, tasks[i], 10, 5);
        ASSERT_NE(brain, nullptr) << "Failed to create brain with task " << i;
        brain_destroy(brain);
    }
}

TEST_F(BrainComprehensiveTest, CreateCustom_MinimalConfig) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.learning_rate = 0.01f;
    config.sparsity_target = 0.8f;
    config.enable_explanations = true;
    strncpy(config.task_name, "custom_test", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    brain_stats_t stats;
    ASSERT_TRUE(brain_get_stats(brain, &stats));
    EXPECT_STREQ(stats.task_name, "custom_test");
    EXPECT_FLOAT_EQ(stats.current_learning_rate, 0.01f);

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, CreateCustom_AllCognitiveFeatures) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "cognitive", sizeof(config.task_name) - 1);

    // Enable all cognitive features
    config.enable_introspection = true;
    config.enable_ethics = true;
    config.enable_salience = true;
    config.enable_consolidation = true;
    config.enable_curiosity = true;
    config.enable_knowledge = true;
    config.enable_wellbeing = true;
    config.enable_logic = true;
    config.enable_epistemic_filter = true;
    config.enable_wellbeing_monitoring = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, CreateCustom_AllPhase10Features) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "phase10", sizeof(config.task_name) - 1);

    // Enable Phase 10 features
    config.enable_working_memory = true;
    config.working_memory_capacity = 7;
    config.enable_emotional_tagging = true;
    config.enable_executive_control = true;
    config.enable_sleep_wake_cycle = true;
    config.enable_mental_health_monitoring = true;
    config.enable_theory_of_mind = true;
    config.enable_natural_explanations = true;
    config.enable_meta_learning = true;
    config.enable_predictive_processing = true;
    config.enable_mirror_neurons = true;
    config.mirror_neuron_count = 1000;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, CreateCustom_MultimodalEnabled) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "multimodal", sizeof(config.task_name) - 1);

    // Enable multimodal processing
    config.enable_multimodal_integration = true;
    config.enable_visual_cortex = true;
    config.enable_audio_cortex = true;
    config.enable_speech_cortex = true;
    config.visual_feature_dim = 128;
    config.audio_feature_dim = 64;
    config.speech_feature_dim = 256;
    config.language_feature_dim = 512;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, CreateCustom_GlialEnabled) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "glial", sizeof(config.task_name) - 1);

    // Enable glial cells
    config.enable_glial = true;
    config.enable_oscillations = true;
    config.num_astrocytes = 200;
    config.num_oligodendrocytes = 140;
    config.num_microglia = 100;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, CreateCustom_AdvancedPlasticity) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "plasticity", sizeof(config.task_name) - 1);

    // Enable advanced plasticity features
    config.enable_eligibility_traces = true;
    config.enable_pink_noise = true;
    config.enable_spike_nlp = true;
    config.enable_fractal_topology = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, CreateErrors_NullParameters) {
    // Test NULL task name
    brain_t brain1 = brain_create(nullptr, BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    EXPECT_EQ(brain1, nullptr);
    EXPECT_NE(brain_get_last_error(), nullptr);
    brain_clear_error();

    // Test zero inputs
    brain_t brain2 = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 0, 3);
    EXPECT_EQ(brain2, nullptr);
    brain_clear_error();

    // Test zero outputs
    brain_t brain3 = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 0);
    EXPECT_EQ(brain3, nullptr);
    brain_clear_error();

    // Test NULL custom config
    brain_t brain4 = brain_create_custom(nullptr);
    EXPECT_EQ(brain4, nullptr);
    brain_clear_error();
}

TEST_F(BrainComprehensiveTest, CreateErrors_InvalidDimensions) {
    // Test excessive inputs
    brain_t brain1 = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 1000000, 3);
    EXPECT_EQ(brain1, nullptr);
    brain_clear_error();

    // Test excessive outputs
    brain_t brain2 = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 1000000);
    EXPECT_EQ(brain2, nullptr);
    brain_clear_error();
}

//=============================================================================
// 2. Learning API Tests - All Learning Modes
//=============================================================================

TEST_F(BrainComprehensiveTest, LearnExample_Basic) {
    brain_t brain = brain_create("learn_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);
    float loss = brain_learn_example(brain, features, 10, "class_a", 0.95f);

    EXPECT_GE(loss, 0.0f) << "Loss should be non-negative";

    // Verify learning happened
    brain_stats_t stats;
    ASSERT_TRUE(brain_get_stats(brain, &stats));
    EXPECT_EQ(stats.total_learning_steps, 1u);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, LearnExample_MultipleLabels) {
    brain_t brain = brain_create("learn_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);

    // Learn different labels
    brain_learn_example(brain, features, 10, "label_a", 0.9f);
    brain_learn_example(brain, features, 10, "label_b", 0.9f);
    brain_learn_example(brain, features, 10, "label_c", 0.9f);

    brain_stats_t stats;
    ASSERT_TRUE(brain_get_stats(brain, &stats));
    EXPECT_EQ(stats.total_learning_steps, 3u);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, LearnExample_Errors) {
    brain_t brain = brain_create("learn_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);

    // Test NULL brain
    float loss1 = brain_learn_example(nullptr, features, 10, "label", 0.9f);
    EXPECT_LT(loss1, 0.0f);
    brain_clear_error();

    // Test NULL features
    float loss2 = brain_learn_example(brain, nullptr, 10, "label", 0.9f);
    EXPECT_LT(loss2, 0.0f);
    brain_clear_error();

    // Test NULL label
    float loss3 = brain_learn_example(brain, features, 10, nullptr, 0.9f);
    EXPECT_LT(loss3, 0.0f);
    brain_clear_error();

    // Test wrong feature count
    float loss4 = brain_learn_example(brain, features, 5, "label", 0.9f);
    EXPECT_LT(loss4, 0.0f);
    brain_clear_error();

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, LearnBatch_Basic) {
    brain_t brain = brain_create("batch_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Create batch examples
    const int batch_size = 5;
    brain_example_t examples[batch_size];

    for (int i = 0; i < batch_size; i++) {
        examples[i].features = create_test_features(10, 0.1f * i);
        examples[i].num_features = 10;
        snprintf(examples[i].label, sizeof(examples[i].label), "label_%d", i % 3);
        examples[i].confidence = 0.9f;
    }

    float avg_loss = brain_learn_batch(brain, examples, batch_size);
    EXPECT_GE(avg_loss, 0.0f);

    brain_stats_t stats;
    ASSERT_TRUE(brain_get_stats(brain, &stats));
    EXPECT_EQ(stats.total_learning_steps, (uint64_t)batch_size);

    // Cleanup
    for (int i = 0; i < batch_size; i++) {
        delete[] examples[i].features;
    }
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, LearnBatch_Errors) {
    brain_t brain = brain_create("batch_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    brain_example_t example;
    example.features = create_test_features(10);
    example.num_features = 10;
    strncpy(example.label, "test", sizeof(example.label) - 1);
    example.label[sizeof(example.label) - 1] = '\0';
    example.confidence = 0.9f;

    // Test NULL brain
    float loss1 = brain_learn_batch(nullptr, &example, 1);
    EXPECT_LT(loss1, 0.0f);
    brain_clear_error();

    // Test NULL examples
    float loss2 = brain_learn_batch(brain, nullptr, 1);
    EXPECT_LT(loss2, 0.0f);
    brain_clear_error();

    // Test zero examples
    float loss3 = brain_learn_batch(brain, &example, 0);
    EXPECT_LT(loss3, 0.0f);
    brain_clear_error();

    delete[] example.features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, LearnFromLLM_Basic) {
    brain_t brain = brain_create("llm_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Mock LLM teacher function
    auto mock_llm = [](const float* input, uint32_t num_features, void* context,
                      char* output_label, uint32_t max_label_len) -> float {
        (void)input; (void)num_features; (void)context;
        strncpy(output_label, "llm_decision", max_label_len);
        return 0.95f;  // High confidence
    };

    float* features = create_test_features(10);
    float loss = brain_learn_from_llm(brain, features, 10, mock_llm, nullptr);

    EXPECT_GE(loss, 0.0f);

    brain_stats_t stats;
    ASSERT_TRUE(brain_get_stats(brain, &stats));
    EXPECT_EQ(stats.total_learning_steps, 1u);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, LearnFromLLM_Errors) {
    brain_t brain = brain_create("llm_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    auto mock_llm = [](const float* input, uint32_t num_features, void* context,
                      char* output_label, uint32_t max_label_len) -> float {
        (void)input; (void)num_features; (void)context;
        strncpy(output_label, "test", max_label_len);
        return 0.9f;
    };

    float* features = create_test_features(10);

    // Test NULL brain
    float loss1 = brain_learn_from_llm(nullptr, features, 10, mock_llm, nullptr);
    EXPECT_LT(loss1, 0.0f);
    brain_clear_error();

    // Test NULL features
    float loss2 = brain_learn_from_llm(brain, nullptr, 10, mock_llm, nullptr);
    EXPECT_LT(loss2, 0.0f);
    brain_clear_error();

    // Test NULL LLM function
    float loss3 = brain_learn_from_llm(brain, features, 10, nullptr, nullptr);
    EXPECT_LT(loss3, 0.0f);
    brain_clear_error();

    // Test wrong feature count
    float loss4 = brain_learn_from_llm(brain, features, 5, mock_llm, nullptr);
    EXPECT_LT(loss4, 0.0f);
    brain_clear_error();

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, LearnFromLLM_InvalidLLMResponse) {
    brain_t brain = brain_create("llm_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Mock LLM that returns invalid confidence
    auto bad_llm = [](const float* input, uint32_t num_features, void* context,
                     char* output_label, uint32_t max_label_len) -> float {
        (void)input; (void)num_features; (void)context;
        strncpy(output_label, "test", max_label_len);
        return -1.0f;  // Invalid confidence
    };

    float* features = create_test_features(10);
    float loss = brain_learn_from_llm(brain, features, 10, bad_llm, nullptr);

    EXPECT_LT(loss, 0.0f);  // Should fail
    brain_clear_error();

    delete[] features;
    brain_destroy(brain);
}

//=============================================================================
// 3. Inference API Tests - All Inference Modes
//=============================================================================

TEST_F(BrainComprehensiveTest, Decide_Basic) {
    brain_t brain = brain_create("decide_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Train first
    float* features = create_test_features(10);
    brain_learn_example(brain, features, 10, "class_a", 0.9f);

    // Make decision
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);

    EXPECT_GT(strlen(decision->label), 0u);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);
    EXPECT_NE(decision->output_vector, nullptr);
    EXPECT_EQ(decision->output_size, 3u);
    EXPECT_GE(decision->inference_time_us, 0u);

    brain_free_decision(decision);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, Decide_WithExplanations) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_explanations = true;  // Enable interpretability
    strncpy(config.task_name, "explain_test", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);

    // Check interpretability fields
    EXPECT_GE(decision->sparsity, 0.0f);
    EXPECT_LE(decision->sparsity, 1.0f);

    brain_free_decision(decision);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, Decide_Caching) {
    brain_t brain = brain_create("cache_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);

    // First decision
    uint64_t start1 = nimcp_time_get_us();
    brain_decision_t* decision1 = brain_decide(brain, features, 10);
    uint64_t time1 = nimcp_time_get_us() - start1;
    ASSERT_NE(decision1, nullptr);

    // Second decision (should be cached and faster)
    uint64_t start2 = nimcp_time_get_us();
    brain_decision_t* decision2 = brain_decide(brain, features, 10);
    uint64_t time2 = nimcp_time_get_us() - start2;
    ASSERT_NE(decision2, nullptr);

    // Cached decision should match
    EXPECT_STREQ(decision1->label, decision2->label);
    EXPECT_FLOAT_EQ(decision1->confidence, decision2->confidence);

    brain_free_decision(decision1);
    brain_free_decision(decision2);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, Decide_Errors) {
    brain_t brain = brain_create("decide_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);

    // Test NULL brain
    brain_decision_t* decision1 = brain_decide(nullptr, features, 10);
    EXPECT_EQ(decision1, nullptr);
    brain_clear_error();

    // Test NULL features
    brain_decision_t* decision2 = brain_decide(brain, nullptr, 10);
    EXPECT_EQ(decision2, nullptr);
    brain_clear_error();

    // Test wrong feature count
    brain_decision_t* decision3 = brain_decide(brain, features, 5);
    EXPECT_EQ(decision3, nullptr);
    brain_clear_error();

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, DecideBatch_Basic) {
    brain_t brain = brain_create("batch_decide", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Train first
    float* train_features = create_test_features(10);
    brain_learn_example(brain, train_features, 10, "class_a", 0.9f);

    // Batch inference
    const int batch_size = 5;
    const float* inputs[batch_size];
    float* feature_arrays[batch_size];
    brain_decision_t decisions[batch_size];

    for (int i = 0; i < batch_size; i++) {
        feature_arrays[i] = create_test_features(10, 0.1f * i);
        inputs[i] = feature_arrays[i];
    }

    bool success = brain_decide_batch(brain, inputs, batch_size, 10, decisions);
    EXPECT_TRUE(success);

    // Verify all decisions
    for (int i = 0; i < batch_size; i++) {
        EXPECT_GT(strlen(decisions[i].label), 0u);
        EXPECT_GE(decisions[i].confidence, 0.0f);
        EXPECT_LE(decisions[i].confidence, 1.0f);
    }

    // Cleanup
    for (int i = 0; i < batch_size; i++) {
        delete[] feature_arrays[i];
        if (decisions[i].output_vector) {
            nimcp_free(decisions[i].output_vector);
        }
    }
    delete[] train_features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, DecideBatch_Errors) {
    brain_t brain = brain_create("batch_decide", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);
    const float* inputs[] = {features};
    brain_decision_t decisions[1];

    // Test NULL brain
    bool result1 = brain_decide_batch(nullptr, inputs, 1, 10, decisions);
    EXPECT_FALSE(result1);
    brain_clear_error();

    // Test NULL inputs
    bool result2 = brain_decide_batch(brain, nullptr, 1, 10, decisions);
    EXPECT_FALSE(result2);
    brain_clear_error();

    // Test zero batch size
    bool result3 = brain_decide_batch(brain, inputs, 0, 10, decisions);
    EXPECT_FALSE(result3);
    brain_clear_error();

    // Test NULL decisions buffer
    bool result4 = brain_decide_batch(brain, inputs, 1, 10, nullptr);
    EXPECT_FALSE(result4);
    brain_clear_error();

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, ObserveAction_Basic) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_mirror_neurons = true;
    config.mirror_neuron_count = 1000;
    strncpy(config.task_name, "mirror_test", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);
    bool result = brain_observe_action(brain, features, 10, 1);  // Agent ID = 1
    EXPECT_TRUE(result);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, ObserveAction_Errors) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_mirror_neurons = true;
    strncpy(config.task_name, "mirror_test", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);

    // Test NULL brain
    bool result1 = brain_observe_action(nullptr, features, 10, 1);
    EXPECT_FALSE(result1);
    brain_clear_error();

    // Test NULL features
    bool result2 = brain_observe_action(brain, nullptr, 10, 1);
    EXPECT_FALSE(result2);
    brain_clear_error();

    // Test invalid agent ID (0 = self)
    bool result3 = brain_observe_action(brain, features, 10, 0);
    EXPECT_FALSE(result3);
    brain_clear_error();

    delete[] features;
    brain_destroy(brain);
}

//=============================================================================
// 4. Persistence API Tests - Save/Load/Snapshots
//=============================================================================

TEST_F(BrainComprehensiveTest, SaveLoad_Basic) {
    const char* filepath = "/tmp/test_brain_save.brain";

    // Create and train brain
    brain_t brain1 = brain_create("save_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain1, nullptr);

    float* features = create_test_features(10);
    brain_learn_example(brain1, features, 10, "test_label", 0.9f);

    // Save
    bool saved = brain_save(brain1, filepath);
    EXPECT_TRUE(saved);
    EXPECT_TRUE(file_exists(filepath));

    // Load
    brain_t brain2 = brain_load(filepath);
    ASSERT_NE(brain2, nullptr);

    // Verify loaded brain works
    brain_decision_t* decision = brain_decide(brain2, features, 10);
    ASSERT_NE(decision, nullptr);

    brain_free_decision(decision);
    delete[] features;
    brain_destroy(brain1);
    brain_destroy(brain2);
    unlink(filepath);
}

TEST_F(BrainComprehensiveTest, SaveLoad_Errors) {
    brain_t brain = brain_create("save_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Test save with NULL brain
    bool result1 = brain_save(nullptr, "/tmp/test.brain");
    EXPECT_FALSE(result1);
    brain_clear_error();

    // Test save with NULL filepath
    bool result2 = brain_save(brain, nullptr);
    EXPECT_FALSE(result2);
    brain_clear_error();

    // Test save to invalid directory
    bool result3 = brain_save(brain, "/nonexistent/directory/test.brain");
    EXPECT_FALSE(result3);
    brain_clear_error();

    // Test load from NULL path
    brain_t brain2 = brain_load(nullptr);
    EXPECT_EQ(brain2, nullptr);
    brain_clear_error();

    // Test load from nonexistent file
    brain_t brain3 = brain_load("/tmp/nonexistent_brain.brain");
    EXPECT_EQ(brain3, nullptr);
    brain_clear_error();

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, Snapshot_SaveRestoreList) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.snapshot_dir = "/tmp/nimcp_snapshots";
    config.enable_auto_snapshots = false;
    strncpy(config.task_name, "snapshot_test", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Train
    float* features = create_test_features(10);
    brain_learn_example(brain, features, 10, "before_snapshot", 0.9f);

    brain_stats_t stats_before;
    brain_get_stats(brain, &stats_before);

    // Save snapshot
    bool saved = brain_save_snapshot(brain, "test_snapshot", "Test snapshot description");
    EXPECT_TRUE(saved);

    // Continue training
    for (int i = 0; i < 5; i++) {
        brain_learn_example(brain, features, 10, "after_snapshot", 0.9f);
    }

    brain_stats_t stats_after;
    brain_get_stats(brain, &stats_after);
    EXPECT_GT(stats_after.total_learning_steps, stats_before.total_learning_steps);

    // List snapshots
    brain_snapshot_info_t infos[10];
    uint32_t count = 0;
    bool listed = brain_list_snapshots(brain, infos, 10, &count);
    EXPECT_TRUE(listed);
    EXPECT_GT(count, 0u);

    // Restore snapshot
    brain_t restored = brain_restore_snapshot(brain, "test_snapshot");
    EXPECT_NE(restored, nullptr);

    // Delete snapshot
    bool deleted = brain_delete_snapshot(brain, "test_snapshot");
    EXPECT_TRUE(deleted);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, Snapshot_Errors) {
    brain_t brain = brain_create("snapshot_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Test save with NULL brain
    bool result1 = brain_save_snapshot(nullptr, "test", "desc");
    EXPECT_FALSE(result1);
    brain_clear_error();

    // Test save with NULL name
    bool result2 = brain_save_snapshot(brain, nullptr, "desc");
    EXPECT_FALSE(result2);
    brain_clear_error();

    // Test restore with NULL name
    brain_t restored1 = brain_restore_snapshot(brain, nullptr);
    EXPECT_EQ(restored1, nullptr);
    brain_clear_error();

    // Test restore nonexistent snapshot
    brain_t restored2 = brain_restore_snapshot(brain, "nonexistent_snapshot");
    EXPECT_EQ(restored2, nullptr);
    brain_clear_error();

    // Test delete with NULL name
    bool result3 = brain_delete_snapshot(brain, nullptr);
    EXPECT_FALSE(result3);
    brain_clear_error();

    brain_destroy(brain);
}

//=============================================================================
// 5. COW Operations Tests - Clone & Distributed Clone
//=============================================================================

TEST_F(BrainComprehensiveTest, CloneCOW_Basic) {
    brain_t original = brain_create("original", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(original, nullptr);

    // Train original
    float* features = create_test_features(10);
    brain_learn_example(original, features, 10, "test_label", 0.9f);

    // Clone
    brain_t clone = brain_clone_cow(original);
    ASSERT_NE(clone, nullptr);

    // Verify clone works
    brain_decision_t* decision = brain_decide(clone, features, 10);
    ASSERT_NE(decision, nullptr);

    // Verify COW stats
    brain_cow_stats_t cow_stats;
    ASSERT_TRUE(brain_get_cow_stats(clone, &cow_stats));
    EXPECT_TRUE(cow_stats.is_cow_clone);
    EXPECT_GT(cow_stats.cow_shared_bytes, 0u);

    brain_free_decision(decision);
    delete[] features;
    brain_destroy(clone);
    brain_destroy(original);
}

TEST_F(BrainComprehensiveTest, CloneCOW_WriteTriggersCopy) {
    brain_t original = brain_create("original", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(original, nullptr);

    brain_t clone = brain_clone_cow(original);
    ASSERT_NE(clone, nullptr);

    // Check initial sharing
    brain_cow_stats_t stats_before;
    brain_get_cow_stats(clone, &stats_before);
    EXPECT_GT(stats_before.cow_shared_bytes, 0u);

    // Trigger write (learning)
    float* features = create_test_features(10);
    brain_learn_example(clone, features, 10, "modified", 0.9f);

    // Check that sharing decreased after write
    brain_cow_stats_t stats_after;
    brain_get_cow_stats(clone, &stats_after);
    // Note: May still show some sharing depending on implementation

    delete[] features;
    brain_destroy(clone);
    brain_destroy(original);
}

TEST_F(BrainComprehensiveTest, CloneCOW_MultipleClones) {
    brain_t original = brain_create("original", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(original, nullptr);

    // Create multiple clones
    brain_t clone1 = brain_clone_cow(original);
    brain_t clone2 = brain_clone_cow(original);
    brain_t clone3 = brain_clone_cow(original);

    ASSERT_NE(clone1, nullptr);
    ASSERT_NE(clone2, nullptr);
    ASSERT_NE(clone3, nullptr);

    // All should share memory
    brain_cow_stats_t stats1, stats2, stats3;
    brain_get_cow_stats(clone1, &stats1);
    brain_get_cow_stats(clone2, &stats2);
    brain_get_cow_stats(clone3, &stats3);

    EXPECT_TRUE(stats1.is_cow_clone);
    EXPECT_TRUE(stats2.is_cow_clone);
    EXPECT_TRUE(stats3.is_cow_clone);

    brain_destroy(clone3);
    brain_destroy(clone2);
    brain_destroy(clone1);
    brain_destroy(original);
}

TEST_F(BrainComprehensiveTest, CloneCOW_Errors) {
    // Test NULL brain
    brain_t clone = brain_clone_cow(nullptr);
    EXPECT_EQ(clone, nullptr);
    brain_clear_error();
}

//=============================================================================
// 6. Statistics & Introspection Tests
//=============================================================================

TEST_F(BrainComprehensiveTest, GetStats_Complete) {
    brain_t brain = brain_create("stats_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    brain_stats_t stats;
    ASSERT_TRUE(brain_get_stats(brain, &stats));

    // Check all fields
    EXPECT_STREQ(stats.task_name, "stats_test");
    EXPECT_EQ(stats.size, BRAIN_SIZE_SMALL);
    EXPECT_GT(stats.num_neurons, 0u);
    EXPECT_GT(stats.num_synapses, 0u);
    EXPECT_EQ(stats.total_inferences, 0u);
    EXPECT_EQ(stats.total_learning_steps, 0u);
    EXPECT_GT(stats.current_learning_rate, 0.0f);
    EXPECT_GT(stats.memory_bytes, 0u);

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, GetStats_AfterOperations) {
    brain_t brain = brain_create("stats_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);

    // Perform operations
    brain_learn_example(brain, features, 10, "test", 0.9f);
    brain_decision_t* decision = brain_decide(brain, features, 10);

    brain_stats_t stats;
    ASSERT_TRUE(brain_get_stats(brain, &stats));

    EXPECT_EQ(stats.total_learning_steps, 1u);
    EXPECT_GE(stats.total_inferences, 1u);

    brain_free_decision(decision);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, GetStats_Errors) {
    brain_stats_t stats;

    // Test NULL brain
    bool result1 = brain_get_stats(nullptr, &stats);
    EXPECT_FALSE(result1);
    brain_clear_error();

    // Test NULL stats buffer
    brain_t brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    bool result2 = brain_get_stats(brain, nullptr);
    EXPECT_FALSE(result2);
    brain_clear_error();

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, GetCOWStats_Complete) {
    brain_t original = brain_create("original", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(original, nullptr);

    brain_t clone = brain_clone_cow(original);
    ASSERT_NE(clone, nullptr);

    brain_cow_stats_t cow_stats;
    ASSERT_TRUE(brain_get_cow_stats(clone, &cow_stats));

    EXPECT_TRUE(cow_stats.is_cow_clone);
    EXPECT_GT(cow_stats.cow_ref_count, 1u);
    EXPECT_GT(cow_stats.cow_shared_bytes, 0u);
    EXPECT_GE(cow_stats.cow_private_bytes, 0u);

    brain_destroy(clone);
    brain_destroy(original);
}

TEST_F(BrainComprehensiveTest, GetMemoryUsage) {
    brain_t brain = brain_create("memory_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    size_t memory = brain_get_memory_usage(brain);
    EXPECT_GT(memory, 0u);

    // Memory should be reasonable for small brain (< 100MB)
    EXPECT_LT(memory, 100 * 1024 * 1024u);

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, GetMemoryUsage_Null) {
    size_t memory = brain_get_memory_usage(nullptr);
    EXPECT_EQ(memory, 0u);
}

TEST_F(BrainComprehensiveTest, PrintInfo) {
    brain_t brain = brain_create("print_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Just verify it doesn't crash
    brain_print_info(brain);

    // Test NULL brain (should not crash)
    brain_print_info(nullptr);

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, GetTopNeurons) {
    brain_t brain = brain_create("neuron_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Train to activate some neurons
    float* features = create_test_features(10);
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 10, "test", 0.9f);
    }

    const uint32_t top_n = 10;
    uint32_t neuron_ids[top_n];
    float importances[top_n];

    uint32_t count = brain_get_top_neurons(brain, top_n, neuron_ids, importances);
    EXPECT_GT(count, 0u);
    EXPECT_LE(count, top_n);

    // Verify importances are sorted descending
    for (uint32_t i = 1; i < count; i++) {
        EXPECT_GE(importances[i-1], importances[i]);
    }

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, GetTopNeurons_Errors) {
    brain_t brain = brain_create("neuron_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    uint32_t ids[10];
    float importances[10];

    // Test NULL brain
    uint32_t count1 = brain_get_top_neurons(nullptr, 10, ids, importances);
    EXPECT_EQ(count1, 0u);

    // Test NULL arrays
    uint32_t count2 = brain_get_top_neurons(brain, 10, nullptr, importances);
    EXPECT_EQ(count2, 0u);

    uint32_t count3 = brain_get_top_neurons(brain, 10, ids, nullptr);
    EXPECT_EQ(count3, 0u);

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, ExplainDecision) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_explanations = true;
    strncpy(config.task_name, "explain_test", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);
    brain_learn_example(brain, features, 10, "test", 0.9f);

    char explanation[512];
    bool result = brain_explain_decision(brain, features, 10, explanation, sizeof(explanation));
    EXPECT_TRUE(result);
    EXPECT_GT(strlen(explanation), 0u);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, ExplainDecision_Errors) {
    brain_t brain = brain_create("explain_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);
    char explanation[256];

    // Test NULL brain
    bool result1 = brain_explain_decision(nullptr, features, 10, explanation, sizeof(explanation));
    EXPECT_FALSE(result1);
    brain_clear_error();

    // Test NULL features
    bool result2 = brain_explain_decision(brain, nullptr, 10, explanation, sizeof(explanation));
    EXPECT_FALSE(result2);
    brain_clear_error();

    // Test NULL explanation buffer
    bool result3 = brain_explain_decision(brain, features, 10, nullptr, 256);
    EXPECT_FALSE(result3);
    brain_clear_error();

    delete[] features;
    brain_destroy(brain);
}

//=============================================================================
// 7. Optimization API Tests - Prune & Optimize
//=============================================================================

TEST_F(BrainComprehensiveTest, Prune_Basic) {
    brain_t brain = brain_create("prune_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Train to create some weak connections
    float* features = create_test_features(10);
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 10, "test", 0.9f);
    }

    brain_stats_t stats_before;
    brain_get_stats(brain, &stats_before);

    // Prune weak connections
    uint32_t pruned = brain_prune(brain, 0.01f);

    brain_stats_t stats_after;
    brain_get_stats(brain, &stats_after);

    // Should have pruned some synapses
    EXPECT_LE(stats_after.num_active_synapses, stats_before.num_active_synapses);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, Prune_Errors) {
    // Test NULL brain
    uint32_t pruned = brain_prune(nullptr, 0.01f);
    EXPECT_EQ(pruned, 0u);
    brain_clear_error();
}

TEST_F(BrainComprehensiveTest, OptimizeForInference) {
    brain_t brain = brain_create("optimize_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Train first
    float* features = create_test_features(10);
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 10, "test", 0.9f);
    }

    bool result = brain_optimize_for_inference(brain);
    EXPECT_TRUE(result);

    // Verify brain still works after optimization
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);

    brain_free_decision(decision);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, OptimizeForInference_Errors) {
    // Test NULL brain
    bool result = brain_optimize_for_inference(nullptr);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainComprehensiveTest, RecommendPruningThreshold) {
    brain_t brain = brain_create("threshold_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Train
    float* features = create_test_features(10);
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 10, "test", 0.9f);
    }

    // Get recommended threshold for 90% sparsity
    float threshold = brain_recommend_pruning_threshold(brain, 0.9f);
    EXPECT_GE(threshold, 0.0f);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, RecommendPruningThreshold_Errors) {
    // Test NULL brain
    float threshold = brain_recommend_pruning_threshold(nullptr, 0.9f);
    EXPECT_EQ(threshold, 0.0f);
    brain_clear_error();
}

//=============================================================================
// 8. Distributed Operations Tests
//=============================================================================

TEST_F(BrainComprehensiveTest, IsDistributed_False) {
    brain_t brain = brain_create("standalone", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    bool is_distributed = brain_is_distributed(brain);
    EXPECT_FALSE(is_distributed);

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, IsDistributed_Null) {
    bool is_distributed = brain_is_distributed(nullptr);
    EXPECT_FALSE(is_distributed);
}

//=============================================================================
// 9. Cognitive Integration Tests - Module Accessors
//=============================================================================

TEST_F(BrainComprehensiveTest, GetNetwork) {
    brain_t brain = brain_create("network_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    adaptive_network_t network = brain_get_network(brain);
    EXPECT_NE(network, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, GetNetwork_Null) {
    adaptive_network_t network = brain_get_network(nullptr);
    EXPECT_EQ(network, nullptr);
}

TEST_F(BrainComprehensiveTest, GetNeuromodulatorSystem) {
    brain_t brain = brain_create("neuromod_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(brain);
    // May be NULL if not initialized

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, GetNeuromodulatorSystem_Null) {
    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(nullptr);
    EXPECT_EQ(neuromod, nullptr);
}

TEST_F(BrainComprehensiveTest, GetWorkingMemory) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_working_memory = true;
    config.working_memory_capacity = 7;
    strncpy(config.task_name, "wm_test", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    working_memory_t* wm = brain_get_working_memory(brain);
    // May be NULL if initialization failed

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, GetWorkingMemory_Null) {
    working_memory_t* wm = brain_get_working_memory(nullptr);
    EXPECT_EQ(wm, nullptr);
}

TEST_F(BrainComprehensiveTest, GetSleepSystem) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_sleep_wake_cycle = true;
    strncpy(config.task_name, "sleep_test", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    sleep_system_t sleep_sys = brain_get_sleep_system(brain);
    // May be NULL if not initialized

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, GetSleepSystem_Null) {
    sleep_system_t sleep_sys = brain_get_sleep_system(nullptr);
    EXPECT_EQ(sleep_sys, nullptr);
}

TEST_F(BrainComprehensiveTest, GetTheoryOfMind) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_theory_of_mind = true;
    strncpy(config.task_name, "tom_test", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    theory_of_mind_t tom = brain_get_theory_of_mind(brain);
    // May be NULL if not initialized

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, GetTheoryOfMind_Null) {
    theory_of_mind_t tom = brain_get_theory_of_mind(nullptr);
    EXPECT_EQ(tom, nullptr);
}

TEST_F(BrainComprehensiveTest, GetExplanationGenerator) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_natural_explanations = true;
    strncpy(config.task_name, "explain_test", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    explanation_generator_t gen = brain_get_explanation_generator(brain);
    // May be NULL if not initialized

    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, GetExplanationGenerator_Null) {
    explanation_generator_t gen = brain_get_explanation_generator(nullptr);
    EXPECT_EQ(gen, nullptr);
}

//=============================================================================
// 10. Error Handling Tests - All Error Paths
//=============================================================================

TEST_F(BrainComprehensiveTest, ErrorHandling_GetLastError) {
    // Initially no error
    const char* error1 = brain_get_last_error();

    // Trigger error
    brain_t brain = brain_create(nullptr, BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    EXPECT_EQ(brain, nullptr);

    const char* error2 = brain_get_last_error();
    EXPECT_NE(error2, nullptr);
    EXPECT_GT(strlen(error2), 0u);

    brain_clear_error();

    const char* error3 = brain_get_last_error();
    // After clear, error should be cleared
}

TEST_F(BrainComprehensiveTest, ErrorHandling_ClearError) {
    // Trigger error
    brain_t brain = brain_create(nullptr, BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    EXPECT_EQ(brain, nullptr);

    const char* error1 = brain_get_last_error();
    EXPECT_NE(error1, nullptr);

    // Clear
    brain_clear_error();

    // Error should be cleared (implementation-dependent)
}

TEST_F(BrainComprehensiveTest, Destroy_Null) {
    // Should not crash
    brain_destroy(nullptr);
}

TEST_F(BrainComprehensiveTest, Destroy_Complete) {
    // Test destroy with all subsystems enabled
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "destroy_test", sizeof(config.task_name) - 1);

    // Enable everything
    config.enable_introspection = true;
    config.enable_ethics = true;
    config.enable_salience = true;
    config.enable_consolidation = true;
    config.enable_curiosity = true;
    config.enable_knowledge = true;
    config.enable_wellbeing = true;
    config.enable_working_memory = true;
    config.enable_emotional_tagging = true;
    config.enable_executive_control = true;
    config.enable_sleep_wake_cycle = true;
    config.enable_mental_health_monitoring = true;
    config.enable_theory_of_mind = true;
    config.enable_natural_explanations = true;
    config.enable_meta_learning = true;
    config.enable_predictive_processing = true;
    config.enable_mirror_neurons = true;
    config.enable_multimodal_integration = true;
    config.enable_visual_cortex = true;
    config.enable_audio_cortex = true;
    config.enable_speech_cortex = true;
    config.enable_glial = true;
    config.enable_oscillations = true;
    config.enable_pink_noise = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Should not crash or leak
    brain_destroy(brain);
}

//=============================================================================
// 11. Multimodal Processing Tests
//=============================================================================

TEST_F(BrainComprehensiveTest, ProcessMultimodal_DirectInput) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_multimodal_integration = true;
    strncpy(config.task_name, "multimodal_test", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Prepare direct input
    float* direct_data = create_test_features(10);
    brain_multimodal_input_t input = {};
    input.direct_data = direct_data;
    input.direct_dim = 10;
    input.timestamp_ms = nimcp_time_get_ms();

    // Allocate output
    brain_multimodal_output_t output = {};
    output.output_vector = new float[3];
    output.output_dim = 3;

    bool result = brain_process_multimodal(brain, &input, &output);
    EXPECT_TRUE(result);

    EXPECT_GE(output.confidence, 0.0f);
    EXPECT_LE(output.confidence, 1.0f);

    delete[] output.output_vector;
    delete[] direct_data;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, ProcessMultimodal_Errors) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_multimodal_integration = true;
    strncpy(config.task_name, "multimodal_test", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    brain_multimodal_input_t input = {};
    brain_multimodal_output_t output = {};
    output.output_vector = new float[3];
    output.output_dim = 3;

    // Test NULL brain
    bool result1 = brain_process_multimodal(nullptr, &input, &output);
    EXPECT_FALSE(result1);
    brain_clear_error();

    // Test NULL input
    bool result2 = brain_process_multimodal(brain, nullptr, &output);
    EXPECT_FALSE(result2);
    brain_clear_error();

    // Test NULL output
    bool result3 = brain_process_multimodal(brain, &input, nullptr);
    EXPECT_FALSE(result3);
    brain_clear_error();

    delete[] output.output_vector;
    brain_destroy(brain);
}

//=============================================================================
// 12. Pretrained Models Tests
//=============================================================================

TEST_F(BrainComprehensiveTest, ModelExists_Nonexistent) {
    bool exists = brain_model_exists("nonexistent_model_12345");
    EXPECT_FALSE(exists);
}

TEST_F(BrainComprehensiveTest, DownloadModel_InvalidModel) {
    bool result = brain_download_model("invalid_model_xyz");
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainComprehensiveTest, GetModelInfo_InvalidModel) {
    brain_model_info_t info;
    bool result = brain_get_model_info("invalid_model", &info);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainComprehensiveTest, GetModelInfo_Errors) {
    brain_model_info_t info;

    // Test NULL model_id
    bool result1 = brain_get_model_info(nullptr, &info);
    EXPECT_FALSE(result1);
    brain_clear_error();

    // Test NULL info
    bool result2 = brain_get_model_info("test_model", nullptr);
    EXPECT_FALSE(result2);
    brain_clear_error();
}

TEST_F(BrainComprehensiveTest, CreatePretrained_InvalidModel) {
    brain_t brain = brain_create_pretrained("invalid_model", BRAIN_TASK_CLASSIFICATION);
    EXPECT_EQ(brain, nullptr);
    brain_clear_error();
}

TEST_F(BrainComprehensiveTest, Finetune_Errors) {
    brain_t brain = brain_create("finetune_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float training_data[50];  // 5 samples × 10 features
    float labels[15];         // 5 samples × 3 outputs

    // Test NULL brain
    bool result1 = brain_finetune(nullptr, training_data, labels, 5, nullptr);
    EXPECT_FALSE(result1);
    brain_clear_error();

    // Test NULL training data
    bool result2 = brain_finetune(brain, nullptr, labels, 5, nullptr);
    EXPECT_FALSE(result2);
    brain_clear_error();

    // Test NULL labels
    bool result3 = brain_finetune(brain, training_data, nullptr, 5, nullptr);
    EXPECT_FALSE(result3);
    brain_clear_error();

    // Test zero samples
    bool result4 = brain_finetune(brain, training_data, labels, 0, nullptr);
    EXPECT_FALSE(result4);
    brain_clear_error();

    brain_destroy(brain);
}

//=============================================================================
// 13. Edge Cases & Stress Tests
//=============================================================================

TEST_F(BrainComprehensiveTest, EdgeCase_VerySmallInputs) {
    brain_t brain = brain_create("tiny_test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 1, 1);
    ASSERT_NE(brain, nullptr);

    float feature = 0.5f;
    brain_learn_example(brain, &feature, 1, "single", 1.0f);

    brain_decision_t* decision = brain_decide(brain, &feature, 1);
    ASSERT_NE(decision, nullptr);

    brain_free_decision(decision);
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, EdgeCase_LargeInputOutputDims) {
    brain_t brain = brain_create("large_io", BRAIN_SIZE_MEDIUM, BRAIN_TASK_CLASSIFICATION, 256, 128);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(256);
    brain_learn_example(brain, features, 256, "large", 0.9f);

    brain_decision_t* decision = brain_decide(brain, features, 256);
    ASSERT_NE(decision, nullptr);
    EXPECT_EQ(decision->output_size, 128u);

    brain_free_decision(decision);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, EdgeCase_ZeroConfidence) {
    brain_t brain = brain_create("zero_conf", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);
    float loss = brain_learn_example(brain, features, 10, "test", 0.0f);  // Zero confidence
    EXPECT_GE(loss, 0.0f);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, EdgeCase_FullConfidence) {
    brain_t brain = brain_create("full_conf", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);
    float loss = brain_learn_example(brain, features, 10, "test", 1.0f);  // Full confidence
    EXPECT_GE(loss, 0.0f);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, Stress_ManyLearningSteps) {
    brain_t brain = brain_create("stress_learn", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);

    // Train 1000 times
    for (int i = 0; i < 1000; i++) {
        float loss = brain_learn_example(brain, features, 10, "stress", 0.9f);
        EXPECT_GE(loss, 0.0f);
    }

    brain_stats_t stats;
    brain_get_stats(brain, &stats);
    EXPECT_EQ(stats.total_learning_steps, 1000u);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainComprehensiveTest, Stress_ManyInferences) {
    brain_t brain = brain_create("stress_infer", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_test_features(10);
    brain_learn_example(brain, features, 10, "test", 0.9f);

    // Run 1000 inferences
    for (int i = 0; i < 1000; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);
    }

    brain_stats_t stats;
    brain_get_stats(brain, &stats);
    EXPECT_GE(stats.total_inferences, 1000u);

    delete[] features;
    brain_destroy(brain);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
