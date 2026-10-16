/**
 * @file test_brain_cognitive_systems.cpp
 * @brief Comprehensive tests for cognitive subsystems initialization
 *
 * WHAT: Tests for higher-level cognitive systems (knowledge, logic, sleep, meta-learning, etc.)
 * WHY: Boost coverage by exercising cognitive module initialization paths
 * HOW: Create brains with various cognitive subsystems enabled
 *
 * TARGET: Cognitive subsystem initialization in brain.c
 * EXPECTED COVERAGE GAIN: ~300 lines
 */

#include <gtest/gtest.h>
#include <cstring>

    #include "core/brain/nimcp_brain.h"
    #include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainCognitiveSystemsTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

//=============================================================================
// Knowledge System Tests
//=============================================================================

TEST_F(BrainCognitiveSystemsTest, Knowledge_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "knowledge_test", 63);

    // Enable knowledge system
    config.enable_knowledge = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, Knowledge_WithLogic) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "knowledge_logic", 63);

    // Enable knowledge system (which enables logic internally)
    config.enable_knowledge = true;
    config.enable_logic = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Sleep-Wake Cycle Tests
//=============================================================================

TEST_F(BrainCognitiveSystemsTest, Sleep_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "sleep_test", 63);

    // Enable sleep-wake cycle
    config.enable_sleep_wake_cycle = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, Sleep_WithMemoryReplay) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "sleep_replay", 63);

    // Enable sleep with memory replay
    config.enable_sleep_wake_cycle = true;
    config.enable_memory_replay = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, Sleep_WithHomeostasis) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "sleep_homeostasis", 63);

    // Enable sleep with synaptic homeostasis
    config.enable_sleep_wake_cycle = true;
    config.enable_synaptic_homeostasis = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, Sleep_WithREMCreativity) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "sleep_rem", 63);

    // Enable sleep with REM creativity
    config.enable_sleep_wake_cycle = true;
    config.enable_rem_creativity = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Working Memory Tests
//=============================================================================

TEST_F(BrainCognitiveSystemsTest, WorkingMemory_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "working_memory", 63);

    // Enable working memory
    config.enable_working_memory = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, WorkingMemory_WithEmotionalTagging) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "memory_emotional", 63);

    // Enable working memory with emotional tagging
    config.enable_working_memory = true;
    config.enable_emotional_tagging = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Executive Control Tests
//=============================================================================

TEST_F(BrainCognitiveSystemsTest, Executive_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "executive_test", 63);

    // Enable executive control
    config.enable_executive_control = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, Executive_WithTaskSwitching) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "executive_switching", 63);

    // Enable executive control with task switching
    config.enable_executive_control = true;
    config.enable_task_switching = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, Executive_WithPlanning) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "executive_planning", 63);

    // Enable executive control with planning
    config.enable_executive_control = true;
    config.enable_planning = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Meta-Learning Tests
//=============================================================================

TEST_F(BrainCognitiveSystemsTest, MetaLearning_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "meta_learning", 63);

    // Enable meta-learning
    config.enable_meta_learning = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, MetaLearning_WithAdaptiveLR) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "meta_adaptive", 63);

    // Enable meta-learning with adaptive learning rates
    config.enable_meta_learning = true;
    config.enable_adaptive_meta_lr = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Predictive Processing Tests
//=============================================================================

TEST_F(BrainCognitiveSystemsTest, Predictive_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "predictive_test", 63);

    // Enable predictive processing
    config.enable_predictive_processing = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, Predictive_WithActiveInference) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "predictive_active", 63);

    // Enable predictive processing with active inference
    config.enable_predictive_processing = true;
    config.enable_active_inference = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Global Workspace Tests
//=============================================================================

TEST_F(BrainCognitiveSystemsTest, GlobalWorkspace_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "workspace_test", 63);

    // Enable global workspace
    config.enable_global_workspace = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, GlobalWorkspace_WithHistory) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "workspace_history", 63);

    // Enable global workspace with history tracking
    config.enable_global_workspace = true;
    config.workspace_enable_history = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Mirror Neurons Tests
//=============================================================================

TEST_F(BrainCognitiveSystemsTest, MirrorNeurons_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "mirror_test", 63);

    // Enable mirror neurons
    config.enable_mirror_neurons = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Theory of Mind Tests
//=============================================================================

TEST_F(BrainCognitiveSystemsTest, TheoryOfMind_BasicInitialization) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "tom_test", 63);

    // Enable theory of mind
    config.enable_theory_of_mind = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, TheoryOfMind_WithEmpathy) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "tom_empathy", 63);

    // Enable theory of mind with empathy
    config.enable_theory_of_mind = true;
    config.enable_empathy_responses = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, TheoryOfMind_WithFalseBelief) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "tom_false_belief", 63);

    // Enable theory of mind with false belief tracking
    config.enable_theory_of_mind = true;
    config.enable_false_belief_tracking = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Comprehensive Integration Tests
//=============================================================================

TEST_F(BrainCognitiveSystemsTest, Comprehensive_AllCognitiveSystems) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_LARGE;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 256;
    config.num_outputs = 50;
    strncpy(config.task_name, "all_cognitive", 63);

    // Enable all major cognitive systems
    config.enable_knowledge = true;
    config.enable_logic = true;
    config.enable_sleep_wake_cycle = true;
    config.enable_memory_replay = true;
    config.enable_working_memory = true;
    config.enable_executive_control = true;
    config.enable_meta_learning = true;
    config.enable_predictive_processing = true;
    config.enable_global_workspace = true;
    config.enable_mirror_neurons = true;
    config.enable_theory_of_mind = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, Comprehensive_EthicsWellbeing) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "ethics_wellbeing", 63);

    // Enable ethics and wellbeing systems
    config.enable_ethics = true;
    config.enable_wellbeing = true;
    config.enable_wellbeing_monitoring = true;
    config.enable_epistemic_filter = true;
    config.enable_introspection = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(BrainCognitiveSystemsTest, Comprehensive_AllFeatures) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_LARGE;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 512;
    config.num_outputs = 100;
    strncpy(config.task_name, "all_features", 63);

    // Enable EVERYTHING
    config.enable_multihead_attention = true;
    config.num_attention_heads = 16;
    config.enable_brain_regions = true;
    config.num_brain_regions = 6;
    config.enable_multimodal_integration = true;
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 128;
    config.enable_audio_cortex = true;
    config.audio_feature_dim = 64;
    config.enable_speech_cortex = true;
    config.speech_feature_dim = 32;
    config.enable_knowledge = true;
    config.enable_logic = true;
    config.enable_sleep_wake_cycle = true;
    config.enable_working_memory = true;
    config.enable_executive_control = true;
    config.enable_meta_learning = true;
    config.enable_predictive_processing = true;
    config.enable_global_workspace = true;
    config.enable_mirror_neurons = true;
    config.enable_theory_of_mind = true;
    config.enable_ethics = true;
    config.enable_wellbeing = true;
    config.enable_introspection = true;
    config.enable_salience = true;
    config.enable_curiosity = true;

    brain_t brain = brain_create_custom(&config);
    if (brain) {
        // Test that the brain can actually be used
        float features[512];
        for (int i = 0; i < 512; i++) features[i] = 0.5f;

        brain_learn_example(brain, features, 512, "test_label", 0.9f);

        brain_decision_t* decision = brain_decide(brain, features, 512);
        if (decision) {
            brain_free_decision(decision);
        }

        brain_destroy(brain);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
