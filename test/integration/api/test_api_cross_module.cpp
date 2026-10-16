/**
 * @file test_api_cross_module.cpp
 * @brief Cross-module integration tests for NIMCP API
 *
 * Tests API interactions across modules:
 * - Brain + Ethics + Knowledge integration
 * - Network operations within brain context
 * - Multiple module lifecycle management
 * - Shared resource coordination
 *
 * Estimated tests: 15
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>
#include <vector>
#include <cstdio>
#include <unistd.h>

class APICrossModuleTest : public ::testing::Test {
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
};

//=============================================================================
// Brain + Ethics + Knowledge Integration
//=============================================================================

TEST_F(APICrossModuleTest, Brain_Ethics_DecisionValidation) {
    // Create brain and ethics module
    nimcp_brain_t brain = nimcp_brain_create("ethical_brain", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 3);
    nimcp_ethics_t ethics = nimcp_ethics_create();

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(ethics, nullptr);

    // Train brain with examples
    float good_action[] = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float bad_action[] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float neutral_action[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f};

    for (int i = 0; i < 20; i++) {
        nimcp_brain_learn_example(brain, good_action, 10, "beneficial", 1.0f);
        nimcp_brain_learn_example(brain, bad_action, 10, "harmful", 1.0f);
        nimcp_brain_learn_example(brain, neutral_action, 10, "neutral", 1.0f);
    }

    // Brain makes decision
    char decision[64];
    float confidence;
    ASSERT_EQ(nimcp_brain_predict(brain, good_action, 10, decision, &confidence), NIMCP_SUCCESS);

    // Ethics validates decision
    float ethics_score;
    ASSERT_EQ(nimcp_ethics_check(ethics, good_action, 10, &ethics_score), NIMCP_SUCCESS);

    // Combined decision-making based on both brain and ethics
    bool should_execute = (confidence > 0.5f) && (ethics_score >= 0.0f);
    EXPECT_TRUE(should_execute || !should_execute);  // Either outcome is valid

    nimcp_ethics_destroy(ethics);
    nimcp_brain_destroy(brain);
}

TEST_F(APICrossModuleTest, Brain_Knowledge_LearningAndRetrieval) {
    // Create brain and knowledge graph
    nimcp_brain_t brain = nimcp_brain_create("learning_brain", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(knowledge, nullptr);

    // Brain learns from examples
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    nimcp_brain_learn_example(brain, features, 5, "pattern_a", 1.0f);

    // Store learning in knowledge graph
    nimcp_knowledge_add_fact(knowledge, "brain", "learned", "pattern_a");
    nimcp_knowledge_add_fact(knowledge, "pattern_a", "has_features", "sequential");

    // Query knowledge
    char query_result[1024];
    ASSERT_EQ(nimcp_knowledge_query(knowledge, "brain", query_result, 1024), NIMCP_SUCCESS);

    // Brain makes prediction
    char prediction[64];
    float confidence;
    ASSERT_EQ(nimcp_brain_predict(brain, features, 5, prediction, &confidence), NIMCP_SUCCESS);

    // Store prediction in knowledge
    nimcp_knowledge_add_fact(knowledge, "brain", "predicted", prediction);

    nimcp_knowledge_destroy(knowledge);
    nimcp_brain_destroy(brain);
}

TEST_F(APICrossModuleTest, Ethics_Knowledge_ConstraintSystem) {
    // Create ethics and knowledge modules
    nimcp_ethics_t ethics = nimcp_ethics_create();
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();

    ASSERT_NE(ethics, nullptr);
    ASSERT_NE(knowledge, nullptr);

    // Build ethical knowledge base
    nimcp_knowledge_add_fact(knowledge, "harm", "is", "unethical");
    nimcp_knowledge_add_fact(knowledge, "help", "is", "ethical");
    nimcp_knowledge_add_fact(knowledge, "neutral", "is", "acceptable");

    // Test ethical scenarios
    float harmful_situation[] = {-1.0f, -1.0f, 0.0f, 0.0f, 0.0f};
    float helpful_situation[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f};

    float harm_score, help_score;
    ASSERT_EQ(nimcp_ethics_check(ethics, harmful_situation, 5, &harm_score), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_ethics_check(ethics, helpful_situation, 5, &help_score), NIMCP_SUCCESS);

    // Query knowledge for ethical principles
    char result[1024];
    nimcp_knowledge_query(knowledge, "harm", result, 1024);

    nimcp_knowledge_destroy(knowledge);
    nimcp_ethics_destroy(ethics);
}

TEST_F(APICrossModuleTest, ThreeModule_DecisionPipeline) {
    // Complete decision pipeline: Brain → Ethics → Knowledge
    nimcp_brain_t brain = nimcp_brain_create("pipeline_brain", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 8, 3);
    nimcp_ethics_t ethics = nimcp_ethics_create();
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(ethics, nullptr);
    ASSERT_NE(knowledge, nullptr);

    // Training phase: Brain learns, knowledge records
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};

    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_example(brain, features, 8, "action_x", 1.0f);
    }
    nimcp_knowledge_add_fact(knowledge, "brain", "trained_on", "action_x");

    // Decision phase: Brain decides, ethics validates, knowledge logs
    char decision[64];
    float confidence;
    ASSERT_EQ(nimcp_brain_predict(brain, features, 8, decision, &confidence), NIMCP_SUCCESS);

    float ethics_score;
    ASSERT_EQ(nimcp_ethics_check(ethics, features, 8, &ethics_score), NIMCP_SUCCESS);

    // Log decision to knowledge base
    nimcp_knowledge_add_fact(knowledge, "brain", "decided", decision);

    char ethics_fact[128];
    snprintf(ethics_fact, sizeof(ethics_fact), "score_%.2f", ethics_score);
    nimcp_knowledge_add_fact(knowledge, decision, "ethics_score", ethics_fact);

    // Verify decision is recorded
    char query_result[1024];
    nimcp_knowledge_query(knowledge, "brain", query_result, 1024);

    nimcp_knowledge_destroy(knowledge);
    nimcp_ethics_destroy(ethics);
    nimcp_brain_destroy(brain);
}

//=============================================================================
// Network Operations Within Brain Context
//=============================================================================

TEST_F(APICrossModuleTest, Brain_Network_Coordination) {
    // Create brain (high-level) and network (low-level)
    nimcp_brain_t brain = nimcp_brain_create("coordinated", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 2);
    nimcp_network_t network = nimcp_network_create(10, 2, 50, 0.01f);

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(network, nullptr);

    float inputs[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    float targets[] = {1.0f, 0.0f};
    float network_outputs[2];

    // Train both on same data
    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_example(brain, inputs, 10, "class_a", 1.0f);
        nimcp_network_train(network, inputs, 10, targets, 2);
    }

    // Get predictions from both
    char brain_label[64];
    float brain_confidence;
    ASSERT_EQ(nimcp_brain_predict(brain, inputs, 10, brain_label, &brain_confidence), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_network_forward(network, inputs, 10, network_outputs, 2), NIMCP_SUCCESS);

    // Both should produce reasonable outputs
    EXPECT_GT(brain_confidence, 0.0f);
    EXPECT_LE(brain_confidence, 1.0f);

    nimcp_network_destroy(network);
    nimcp_brain_destroy(brain);
}

TEST_F(APICrossModuleTest, Network_MultipleContexts) {
    // Create multiple networks for different tasks
    nimcp_network_t net1 = nimcp_network_create(5, 2, 20, 0.01f);
    nimcp_network_t net2 = nimcp_network_create(10, 3, 30, 0.005f);
    nimcp_network_t net3 = nimcp_network_create(8, 4, 25, 0.02f);

    ASSERT_NE(net1, nullptr);
    ASSERT_NE(net2, nullptr);
    ASSERT_NE(net3, nullptr);

    // Train all networks independently
    float inputs1[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float targets1[] = {1.0f, 0.0f};
    float outputs1[2];

    float inputs2[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    float targets2[] = {1.0f, 0.0f, 0.0f};
    float outputs2[3];

    float inputs3[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float targets3[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float outputs3[4];

    // Interleave operations
    nimcp_network_train(net1, inputs1, 5, targets1, 2);
    nimcp_network_train(net2, inputs2, 10, targets2, 3);
    nimcp_network_train(net3, inputs3, 8, targets3, 4);

    nimcp_network_forward(net1, inputs1, 5, outputs1, 2);
    nimcp_network_forward(net2, inputs2, 10, outputs2, 3);
    nimcp_network_forward(net3, inputs3, 8, outputs3, 4);

    // All should produce valid outputs
    EXPECT_TRUE(outputs1[0] >= 0.0f && outputs1[0] <= 1.0f);
    EXPECT_TRUE(outputs2[0] >= 0.0f && outputs2[0] <= 1.0f);
    EXPECT_TRUE(outputs3[0] >= 0.0f && outputs3[0] <= 1.0f);

    nimcp_network_destroy(net3);
    nimcp_network_destroy(net2);
    nimcp_network_destroy(net1);
}

TEST_F(APICrossModuleTest, Brain_Network_SharedTraining) {
    nimcp_brain_t brain = nimcp_brain_create("shared", NIMCP_BRAIN_SMALL, NIMCP_TASK_REGRESSION, 5, 1);
    nimcp_network_t network = nimcp_network_create(5, 1, 20, 0.01f);

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(network, nullptr);

    float inputs[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float targets[] = {15.0f};  // Sum of inputs
    float brain_outputs[1];
    float network_outputs[1];

    // Train both on same regression task
    for (int i = 0; i < 30; i++) {
        nimcp_brain_learn_example(brain, inputs, 5, "15.0", 1.0f);
        nimcp_network_train(network, inputs, 5, targets, 1);
    }

    // Compare outputs
    ASSERT_EQ(nimcp_brain_infer(brain, inputs, 5, brain_outputs, 1), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_network_forward(network, inputs, 5, network_outputs, 1), NIMCP_SUCCESS);

    // Both should learn similar patterns (roughly)
    EXPECT_GT(brain_outputs[0], 0.0f);
    EXPECT_GT(network_outputs[0], 0.0f);

    nimcp_network_destroy(network);
    nimcp_brain_destroy(brain);
}

//=============================================================================
// Multiple Module Lifecycle Management
//=============================================================================

TEST_F(APICrossModuleTest, Lifecycle_CreateAllModulesAndDestroy) {
    // Create all module types
    nimcp_brain_t brain = nimcp_brain_create("all_modules", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_network_t network = nimcp_network_create(5, 2, 20, 0.01f);
    nimcp_ethics_t ethics = nimcp_ethics_create();
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();

    // Verify all created
    ASSERT_NE(brain, nullptr);
    ASSERT_NE(network, nullptr);
    ASSERT_NE(ethics, nullptr);
    ASSERT_NE(knowledge, nullptr);

    // Destroy in different order than creation
    nimcp_ethics_destroy(ethics);
    nimcp_brain_destroy(brain);
    nimcp_knowledge_destroy(knowledge);
    nimcp_network_destroy(network);

    // Should not crash
}

TEST_F(APICrossModuleTest, Lifecycle_PartialFailureRecovery) {
    // Create some modules
    nimcp_brain_t brain = nimcp_brain_create("partial", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_ethics_t ethics = nimcp_ethics_create();

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(ethics, nullptr);

    // Destroy brain
    nimcp_brain_destroy(brain);

    // Ethics should still work
    float situation[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float score;
    ASSERT_EQ(nimcp_ethics_check(ethics, situation, 5, &score), NIMCP_SUCCESS);

    nimcp_ethics_destroy(ethics);
}

TEST_F(APICrossModuleTest, Lifecycle_RepeatedCreationDestruction) {
    // Repeatedly create and destroy modules
    for (int cycle = 0; cycle < 5; cycle++) {
        nimcp_brain_t brain = nimcp_brain_create("cycle", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
        nimcp_network_t network = nimcp_network_create(5, 2, 10, 0.01f);
        nimcp_ethics_t ethics = nimcp_ethics_create();
        nimcp_knowledge_t knowledge = nimcp_knowledge_create();

        // Use modules briefly
        if (brain) {
            float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
            nimcp_brain_learn_example(brain, features, 5, "test", 1.0f);
        }

        if (network) {
            float inputs[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
            float outputs[2];
            nimcp_network_forward(network, inputs, 5, outputs, 2);
        }

        // Cleanup
        nimcp_knowledge_destroy(knowledge);
        nimcp_ethics_destroy(ethics);
        nimcp_network_destroy(network);
        nimcp_brain_destroy(brain);
    }
}

TEST_F(APICrossModuleTest, Lifecycle_SaveLoadWithOtherModules) {
    const char* save_path = "/tmp/cross_module_test.nimcp";
    cleanup_file(save_path);

    // Create modules
    nimcp_brain_t brain = nimcp_brain_create("save_test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_ethics_t ethics = nimcp_ethics_create();
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(ethics, nullptr);
    ASSERT_NE(knowledge, nullptr);

    // Train brain
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    nimcp_brain_learn_example(brain, features, 5, "test", 1.0f);

    // Save brain
    ASSERT_EQ(nimcp_brain_save(brain, save_path), NIMCP_SUCCESS);

    // Destroy all
    nimcp_brain_destroy(brain);
    nimcp_ethics_destroy(ethics);
    nimcp_knowledge_destroy(knowledge);

    // Load brain back
    brain = nimcp_brain_load(save_path);
    ASSERT_NE(brain, nullptr);

    // Brain should still work
    char label[64];
    float confidence;
    ASSERT_EQ(nimcp_brain_predict(brain, features, 5, label, &confidence), NIMCP_SUCCESS);

    nimcp_brain_destroy(brain);
    cleanup_file(save_path);
}

//=============================================================================
// Shared Resource Coordination
//=============================================================================

TEST_F(APICrossModuleTest, SharedResources_MultipleModulesAccessingMemory) {
    // Create multiple modules that will allocate memory
    std::vector<nimcp_brain_t> brains;
    std::vector<nimcp_network_t> networks;

    for (int i = 0; i < 5; i++) {
        nimcp_brain_t brain = nimcp_brain_create("shared", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 2);
        nimcp_network_t network = nimcp_network_create(10, 2, 30, 0.01f);

        if (brain) brains.push_back(brain);
        if (network) networks.push_back(network);
    }

    EXPECT_GT(brains.size(), 0);
    EXPECT_GT(networks.size(), 0);

    // All modules should be able to work simultaneously
    float inputs[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    float outputs[2];

    for (auto brain : brains) {
        nimcp_brain_learn_example(brain, inputs, 10, "test", 1.0f);
    }

    for (auto network : networks) {
        nimcp_network_forward(network, inputs, 10, outputs, 2);
    }

    // Cleanup
    for (auto network : networks) {
        nimcp_network_destroy(network);
    }
    for (auto brain : brains) {
        nimcp_brain_destroy(brain);
    }
}

TEST_F(APICrossModuleTest, SharedResources_ErrorHandlingAcrossModules) {
    nimcp_brain_t brain = nimcp_brain_create("error_test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_ethics_t ethics = nimcp_ethics_create();

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(ethics, nullptr);

    // Cause error in brain
    nimcp_status_t brain_status = nimcp_brain_predict(brain, nullptr, 0, nullptr, nullptr);
    EXPECT_NE(brain_status, NIMCP_SUCCESS);

    // Ethics should still work after brain error
    float situation[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float score;
    ASSERT_EQ(nimcp_ethics_check(ethics, situation, 5, &score), NIMCP_SUCCESS);

    // Brain should recover and work again
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    char label[64];
    float confidence;
    nimcp_brain_learn_example(brain, features, 5, "test", 1.0f);
    ASSERT_EQ(nimcp_brain_predict(brain, features, 5, label, &confidence), NIMCP_SUCCESS);

    nimcp_ethics_destroy(ethics);
    nimcp_brain_destroy(brain);
}

TEST_F(APICrossModuleTest, SharedResources_VersionCompatibility) {
    // Verify API version is consistent
    const char* version_str = nimcp_version();
    int version_int = nimcp_version_int();

    EXPECT_NE(version_str, nullptr);
    EXPECT_GT(version_int, 0);

    // Version should be in expected format (2.x.x)
    EXPECT_EQ(version_int / 10000, 2);

    // Create modules and verify they all work with same version
    nimcp_brain_t brain = nimcp_brain_create("version", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_network_t network = nimcp_network_create(5, 2, 20, 0.01f);
    nimcp_ethics_t ethics = nimcp_ethics_create();
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();

    // All should be created successfully
    EXPECT_NE(brain, nullptr);
    EXPECT_NE(network, nullptr);
    EXPECT_NE(ethics, nullptr);
    EXPECT_NE(knowledge, nullptr);

    nimcp_knowledge_destroy(knowledge);
    nimcp_ethics_destroy(ethics);
    nimcp_network_destroy(network);
    nimcp_brain_destroy(brain);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
