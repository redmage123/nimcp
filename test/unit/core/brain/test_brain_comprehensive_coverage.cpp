/**
 * @file test_brain_comprehensive_coverage.cpp  
 * @brief Comprehensive tests targeting remaining uncovered brain.c code
 *
 * TARGET: Reach 95% brain.c coverage by exercising all major code paths
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

#include "core/brain/nimcp_brain.h"

class BrainComprehensiveCoverageTest : public ::testing::Test {
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

// Test with ALL features enabled and exercised
TEST_F(BrainComprehensiveCoverageTest, MaximalFeatureExercise) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 300;
    config.num_outputs = 10;
    config.learning_rate = 0.01f;
    config.sparsity_target = 0.9f;
    strncpy(config.task_name, "comprehensive", sizeof(config.task_name) - 1);
    
    // Enable EVERYTHING
    config.enable_explanations = true;
    config.enable_distributed = false;  // Skip P2P for now
    config.enable_glial = true;
    config.enable_oscillations = true;
    config.num_astrocytes = 200;
    config.num_oligodendrocytes = 100;
    config.num_microglia = 50;
    
    // Sensory
    config.enable_visual_cortex = true;
    config.enable_audio_cortex = true;
    config.enable_speech_cortex = true;
    config.enable_multimodal_integration = true;
    config.visual_feature_dim = 100;
    config.audio_feature_dim = 50;
    config.speech_feature_dim = 50;
    
    // Cognitive
    config.enable_introspection = true;
    config.enable_ethics = true;
    config.enable_salience = true;
    config.enable_consolidation = true;
    config.enable_curiosity = true;
    config.enable_knowledge = true;
    config.enable_wellbeing = true;
    config.enable_logic = true;
    
    // Advanced plasticity
    config.enable_eligibility_traces = true;
    config.enable_pink_noise = true;
    config.enable_spike_nlp = true;
    config.enable_fractal_topology = true;
    
    // Attention
    config.enable_multihead_attention = false;  // Causes issues
    config.enable_thalamic_gate = true;
    config.enable_salience_weighting = true;
    
    // Multi-modal
    config.language_feature_dim = 100;
    
    // Epistemic & wellbeing
    config.enable_epistemic_filter = true;
    config.enable_wellbeing_monitoring = true;
    config.wellbeing_check_interval_ms = 5000;
    
    // Phase 10 features
    config.enable_working_memory = true;
    config.working_memory_capacity = 7;
    config.working_memory_decay_tau_ms = 1000.0f;
    
    config.enable_emotional_tagging = true;
    config.enable_emotional_memories = true;
    
    config.enable_executive_control = true;
    config.enable_task_switching = true;
    config.enable_planning = true;
    
    config.enable_sleep_wake_cycle = true;
    config.sleep_pressure_threshold = 0.8f;
    config.enable_memory_replay = true;
    config.enable_synaptic_homeostasis = true;
    config.enable_rem_creativity = true;
    
    config.enable_mental_health_monitoring = true;
    config.enable_auto_intervention = true;
    config.shutdown_on_critical_disorder = false;
    
    config.enable_theory_of_mind = true;
    config.enable_empathy_responses = true;
    config.enable_false_belief_tracking = true;
    
    config.enable_natural_explanations = true;
    config.enable_causal_explanations = true;
    
    config.enable_meta_learning = true;
    config.enable_adaptive_meta_lr = true;
    config.meta_task_batch_size = 4;
    config.meta_k_shot = 5;
    
    config.enable_predictive_processing = true;
    config.enable_active_inference = true;
    
    config.enable_mirror_neurons = true;
    config.mirror_neuron_count = 1000;
    config.mirror_max_actions = 100;
    config.mirror_max_agents = 10;
    config.mirror_learning_rate = 0.01f;
    config.mirror_match_threshold = 0.7f;
    
    config.enable_global_workspace = true;
    config.workspace_capacity_dim = 256;
    config.workspace_ignition_threshold = 0.6f;
    config.workspace_refractory_ms = 50;
    config.workspace_enable_history = true;
    config.workspace_history_depth = 10;
    
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    
    // Exercise learning with many patterns
    for (int trial = 0; trial < 50; trial++) {
        float features[300];
        for (int i = 0; i < 300; i++) {
            features[i] = sinf(trial * 0.1f + i * 0.01f);
        }
        
        char label[32];
        snprintf(label, sizeof(label), "class_%d", trial % 10);
        
        float loss = brain_learn_example(brain, features, 300, label, 1.0f);
        EXPECT_GE(loss, 0.0f);
        
        // Periodically make decisions
        if (trial % 5 == 0) {
            brain_decision_t* decision = brain_decide(brain, features, 300);
            if (decision) {
                EXPECT_GE(decision->confidence, 0.0f);
                EXPECT_LE(decision->confidence, 1.0f);
                brain_free_decision(decision);
            }
        }
        
        // Observe other agents
        if (trial % 3 == 0) {
            brain_observe_action(brain, features, 300, (trial % 5) + 1);
        }
    }
    
    // Get statistics
    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
    EXPECT_GT(stats.total_learning_steps, 0u);
    EXPECT_GT(stats.num_neurons, 0u);
    
    // Test explanations
    float test_features[300];
    for (int i = 0; i < 300; i++) {
        test_features[i] = 0.5f;
    }
    
    char explanation[256];
    brain_explain_decision(brain, test_features, 300, explanation, sizeof(explanation));
    
    // Test top neurons
    uint32_t neuron_ids[20];
    float importances[20];
    uint32_t count = brain_get_top_neurons(brain, 20, neuron_ids, importances);
    EXPECT_GT(count, 0u);
    
    // Test optimization
    float threshold = brain_recommend_pruning_threshold(brain, 0.95f);
    EXPECT_GE(threshold, 0.0f);
    
    uint32_t pruned = brain_prune(brain, 0.001f);
    
    // Memory usage
    size_t memory = brain_get_memory_usage(brain);
    EXPECT_GT(memory, 0u);
    
    // COW stats (even if not COW)
    brain_cow_stats_t cow_stats;
    brain_get_cow_stats(brain, &cow_stats);
}

// Test all task types thoroughly
TEST_F(BrainComprehensiveCoverageTest, AllTaskTypesExtensive) {
    brain_task_t tasks[] = {
        BRAIN_TASK_CLASSIFICATION,
        BRAIN_TASK_REGRESSION,
        BRAIN_TASK_PATTERN_MATCHING,
        BRAIN_TASK_SEQUENCE,
        BRAIN_TASK_ASSOCIATION,
        BRAIN_TASK_CUSTOM
    };
    
    for (auto task : tasks) {
        brain = brain_create("task_test", BRAIN_SIZE_SMALL, task, 20, 5);
        ASSERT_NE(brain, nullptr);
        
        // Train extensively
        for (int i = 0; i < 20; i++) {
            float features[20];
            for (int j = 0; j < 20; j++) {
                features[j] = (float)(i * 20 + j) / 400.0f;
            }
            
            brain_learn_example(brain, features, 20, "test", 1.0f);
        }
        
        // Test decision making
        float test[20] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f,
                         0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
        brain_decision_t* decision = brain_decide(brain, test, 20);
        if (decision) {
            brain_free_decision(decision);
        }
        
        // Test batch decision
        const float* inputs[3] = {test, test, test};
        brain_decision_t decisions[3];
        brain_decide_batch(brain, inputs, 3, 20, decisions);
        
        brain_destroy(brain);
        brain = nullptr;
    }
}

// Test all brain sizes
TEST_F(BrainComprehensiveCoverageTest, AllBrainSizesExtensive) {
    brain_size_t sizes[] = {
        BRAIN_SIZE_TINY,
        BRAIN_SIZE_SMALL,
        BRAIN_SIZE_MEDIUM,
        BRAIN_SIZE_LARGE
    };
    
    for (auto size : sizes) {
        int inputs = (size == BRAIN_SIZE_TINY) ? 5 : 
                    (size == BRAIN_SIZE_SMALL) ? 20 :
                    (size == BRAIN_SIZE_MEDIUM) ? 50 : 100;
        
        brain = brain_create("size_test", size, BRAIN_TASK_CLASSIFICATION, inputs, 5);
        ASSERT_NE(brain, nullptr);
        
        // Train
        float* features = new float[inputs];
        for (int i = 0; i < inputs; i++) {
            features[i] = (float)i / inputs;
        }
        
        for (int trial = 0; trial < 10; trial++) {
            brain_learn_example(brain, features, inputs, "test", 1.0f);
        }
        
        // Decide
        brain_decision_t* decision = brain_decide(brain, features, inputs);
        if (decision) {
            brain_free_decision(decision);
        }
        
        delete[] features;
        brain_destroy(brain);
        brain = nullptr;
    }
}

// Test error paths extensively
TEST_F(BrainComprehensiveCoverageTest, ExtensiveErrorPaths) {
    brain = brain_create("error_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    
    // Test various error conditions
    brain_learn_example(nullptr, features, 10, "test", 1.0f);  // NULL brain
    brain_learn_example(brain, nullptr, 10, "test", 1.0f);    // NULL features
    brain_learn_example(brain, features, 5, "test", 1.0f);    // Wrong size
    brain_learn_example(brain, features, 10, nullptr, 1.0f);  // NULL label
    
    brain_decide(nullptr, features, 10);  // NULL brain
    brain_decide(brain, nullptr, 10);     // NULL features
    brain_decide(brain, features, 5);     // Wrong size
    
    brain_get_stats(nullptr, nullptr);
    brain_stats_t stats;
    brain_get_stats(nullptr, &stats);
    brain_get_stats(brain, nullptr);
    
    brain_get_network(nullptr);
    brain_get_memory_usage(nullptr);
    brain_print_info(nullptr);
    
    brain_prune(nullptr, 0.01f);
    brain_recommend_pruning_threshold(nullptr, 0.9f);
    brain_optimize_for_inference(nullptr);
    
    brain_save(nullptr, "/tmp/test.brain");
    brain_save(brain, nullptr);
    
    brain_explain_decision(nullptr, features, 10, nullptr, 256);
    
    brain_get_top_neurons(nullptr, 10, nullptr, nullptr);
    
    brain_clone_cow(nullptr);
    
    const char* error = brain_get_last_error();
    brain_clear_error();
}

// Test batch operations extensively
TEST_F(BrainComprehensiveCoverageTest, ExtensiveBatchOperations) {
    brain = brain_create("batch_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    
    // Large batch learning
    std::vector<brain_example_t> examples;
    for (int i = 0; i < 100; i++) {
        brain_example_t ex = {};
        ex.features = new float[10];
        for (int j = 0; j < 10; j++) {
            ex.features[j] = (float)(i * 10 + j) / 1000.0f;
        }
        ex.num_features = 10;
        strncpy(ex.label, (i % 3 == 0) ? "class_a" : (i % 3 == 1) ? "class_b" : "class_c", 
                sizeof(ex.label) - 1);
        ex.confidence = 0.8f + (i % 10) * 0.02f;
        examples.push_back(ex);
    }
    
    float avg_loss = brain_learn_batch(brain, examples.data(), examples.size());
    EXPECT_GE(avg_loss, 0.0f);
    
    // Batch inference
    std::vector<float*> input_ptrs;
    for (auto& ex : examples) {
        input_ptrs.push_back(ex.features);
    }
    
    std::vector<brain_decision_t> decisions(examples.size());
    brain_decide_batch(brain, (const float**)input_ptrs.data(), examples.size(), 10, decisions.data());
    
    // Cleanup
    for (auto& ex : examples) {
        delete[] ex.features;
    }
}

// Test accessors
TEST_F(BrainComprehensiveCoverageTest, AllAccessors) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "accessor_test", sizeof(config.task_name) - 1);
    
    config.enable_working_memory = true;
    config.enable_global_workspace = true;
    config.enable_theory_of_mind = true;
    config.enable_sleep_wake_cycle = true;
    config.enable_natural_explanations = true;
    
    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);
    
    // Call all accessors
    adaptive_network_t net = brain_get_network(brain);
    EXPECT_NE(net, nullptr);
    
    working_memory_t* wm = brain_get_working_memory(brain);
    global_workspace_t* gw = brain_get_global_workspace(brain);
    theory_of_mind_t tom = brain_get_theory_of_mind(brain);
    sleep_system_t sleep = brain_get_sleep_system(brain);
    explanation_generator_t eg = brain_get_explanation_generator(brain);
    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(brain);
    
    // Distributed checks
    bool is_dist = brain_is_distributed(brain);
    EXPECT_FALSE(is_dist);
    
    bool is_cow = brain_is_distributed_cow(brain);
    EXPECT_FALSE(is_cow);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
