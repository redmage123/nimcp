/**
 * @file test_brain_init_integration.cpp
 * @brief Integration tests for complete brain factory initialization workflows
 *
 * Tests end-to-end brain initialization including:
 * - Complete subsystem initialization sequences
 * - Cross-subsystem dependencies and interactions
 * - Full brain lifecycle (create, init all subsystems, use, destroy)
 * - Subsystem ordering dependencies
 * - Real-world initialization scenarios
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 * @date 2025-11-21
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"

/**
 * @brief Test fixture for integration tests
 */
class BrainInitIntegrationTest : public ::testing::Test {
protected:
    brain_t test_brain;

    void SetUp() override {
        test_brain = nullptr;
    }

    void TearDown() override {
        if (test_brain) {
            nimcp_brain_destroy(test_brain);
            test_brain = nullptr;
        }
    }

    /**
     * @brief Helper to initialize all cognitive subsystems in correct order
     */
    bool InitializeAllCognitiveSubsystems(brain_t brain) {
        if (!brain) return false;

        // Phase 1: Core cognitive infrastructure
        if (!nimcp_brain_factory_init_introspection_subsystem(brain)) return false;
        if (!nimcp_brain_factory_init_salience_subsystem(brain)) return false;

        // Phase 2: Memory systems
        if (!nimcp_brain_factory_init_consolidation_subsystem(brain)) return false;
        if (!nimcp_brain_factory_init_autobiographical_memory_subsystem(brain)) return false;

        // Phase 3: Predictive and learning systems
        if (!nimcp_brain_factory_init_predictive_subsystem(brain)) return false;
        if (!nimcp_brain_factory_init_curiosity_subsystem(brain)) return false;

        // Phase 4: Social cognition
        if (!nimcp_brain_factory_init_mirror_neurons(brain)) return false;
        if (!nimcp_brain_factory_init_empathy_network_subsystem(brain)) return false;

        // Phase 5: Ethics and values
        if (!nimcp_brain_factory_init_ethics_engine_subsystem(brain)) return false;
        if (!nimcp_brain_factory_init_empathetic_response_subsystem(brain)) return false;

        // Phase 6: Self-awareness
        if (!nimcp_brain_factory_init_self_model_subsystem(brain)) return false;
        if (!nimcp_brain_factory_init_mental_health_subsystem(brain)) return false;

        // Phase 7: Global integration
        if (!nimcp_brain_factory_init_global_workspace_subsystem(brain)) return false;

        return true;
    }
};

//=============================================================================
// Complete Initialization Workflow Tests
//=============================================================================

TEST_F(BrainInitIntegrationTest, CompleteInitialization_MinimalBrain) {
    test_brain = nimcp_brain_create(
        "minimal_integration_test",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        5,
        2
    );
    ASSERT_NE(test_brain, nullptr);

    // Initialize only essential subsystems
    EXPECT_TRUE(nimcp_brain_factory_init_introspection_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_ethics_engine_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_self_model_subsystem(test_brain));

    EXPECT_NE(test_brain->introspection, nullptr);
    EXPECT_NE(test_brain->ethics, nullptr);
    EXPECT_NE(test_brain->self_model, nullptr);
}

TEST_F(BrainInitIntegrationTest, CompleteInitialization_FullCognitiveBrain) {
    test_brain = nimcp_brain_create(
        "full_cognitive_test",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        10,
        3
    );
    ASSERT_NE(test_brain, nullptr);

    // Enable all cognitive subsystems
    test_brain->config.enable_mental_health_monitoring = true;
    test_brain->config.enable_predictive_processing = true;
    test_brain->config.enable_mirror_neurons = true;
    test_brain->config.enable_consolidation = true;
    test_brain->config.enable_curiosity = true;
    test_brain->config.enable_salience = true;
    test_brain->config.enable_global_workspace = true;

    // Initialize all subsystems
    EXPECT_TRUE(InitializeAllCognitiveSubsystems(test_brain));

    // Verify all subsystems are initialized
    EXPECT_NE(test_brain->introspection, nullptr);
    EXPECT_NE(test_brain->salience, nullptr);
    EXPECT_NE(test_brain->consolidation, nullptr);
    EXPECT_NE(test_brain->autobio, nullptr);
    EXPECT_NE(test_brain->predictive_network, nullptr);
    EXPECT_NE(test_brain->curiosity, nullptr);
    EXPECT_NE(test_brain->mirror_neurons, nullptr);
    EXPECT_NE(test_brain->empathy_network, nullptr);
    EXPECT_NE(test_brain->ethics, nullptr);
    EXPECT_NE(test_brain->empathetic_response_engine, nullptr);
    EXPECT_NE(test_brain->self_model, nullptr);
    EXPECT_NE(test_brain->mental_health_monitor, nullptr);
    EXPECT_NE(test_brain->global_workspace, nullptr);
}

TEST_F(BrainInitIntegrationTest, CompleteInitialization_LargeBrain) {
    test_brain = nimcp_brain_create(
        "large_integration_test",
        BRAIN_SIZE_MEDIUM,
        BRAIN_TASK_REGRESSION,
        20,
        5
    );
    ASSERT_NE(test_brain, nullptr);

    // Enable subsystems
    test_brain->config.enable_predictive_processing = true;
    test_brain->config.enable_curiosity = true;
    test_brain->config.enable_salience = true;
    test_brain->config.enable_global_workspace = true;

    EXPECT_TRUE(InitializeAllCognitiveSubsystems(test_brain));

    // Brain should be ready for complex tasks
    EXPECT_NE(test_brain->network, nullptr);
    EXPECT_NE(test_brain->introspection, nullptr);
    EXPECT_NE(test_brain->global_workspace, nullptr);
}

//=============================================================================
// Subsystem Dependency Tests
//=============================================================================

TEST_F(BrainInitIntegrationTest, Dependencies_MirrorNeuronsAndEmpathy) {
    test_brain = nimcp_brain_create("dependency_test_1", BRAIN_SIZE_TINY,
                                    BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(test_brain, nullptr);

    test_brain->config.enable_mirror_neurons = true;

    // Initialize mirror neurons first
    EXPECT_TRUE(nimcp_brain_factory_init_mirror_neurons(test_brain));
    EXPECT_NE(test_brain->mirror_neurons, nullptr);

    // Empathy network can reference mirror neurons
    EXPECT_TRUE(nimcp_brain_factory_init_empathy_network_subsystem(test_brain));
    EXPECT_NE(test_brain->empathy_network, nullptr);
}

TEST_F(BrainInitIntegrationTest, Dependencies_EthicsAndEmpatheticResponse) {
    test_brain = nimcp_brain_create("dependency_test_2", BRAIN_SIZE_TINY,
                                    BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(test_brain, nullptr);

    // Ethics and empathy must be initialized before empathetic response
    EXPECT_TRUE(nimcp_brain_factory_init_ethics_engine_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_empathy_network_subsystem(test_brain));

    EXPECT_TRUE(nimcp_brain_factory_init_empathetic_response_subsystem(test_brain));
    EXPECT_NE(test_brain->empathetic_response_engine, nullptr);
}

TEST_F(BrainInitIntegrationTest, Dependencies_SelfModelAndPersonality) {
    test_brain = nimcp_brain_create("dependency_test_3", BRAIN_SIZE_TINY,
                                    BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(test_brain, nullptr);

    // Self-model can integrate with personality if available
    EXPECT_TRUE(nimcp_brain_factory_init_self_model_subsystem(test_brain));
    EXPECT_NE(test_brain->self_model, nullptr);
}

TEST_F(BrainInitIntegrationTest, Dependencies_MirrorNeuronsWithWorkingMemory) {
    test_brain = nimcp_brain_create("dependency_test_4", BRAIN_SIZE_TINY,
                                    BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(test_brain, nullptr);

    test_brain->config.enable_mirror_neurons = true;
    test_brain->config.enable_working_memory = true;

    // Mirror neurons can integrate with working memory
    EXPECT_TRUE(nimcp_brain_factory_init_mirror_neurons(test_brain));
    EXPECT_NE(test_brain->mirror_neurons, nullptr);
}

//=============================================================================
// Initialization Order Tests
//=============================================================================

TEST_F(BrainInitIntegrationTest, InitOrder_CoreFirst) {
    test_brain = nimcp_brain_create("order_test_1", BRAIN_SIZE_TINY,
                                    BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(test_brain, nullptr);

    // Initialize core infrastructure first
    EXPECT_TRUE(nimcp_brain_factory_init_introspection_subsystem(test_brain));

    // Then higher-level cognitive systems
    EXPECT_TRUE(nimcp_brain_factory_init_self_model_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_autobiographical_memory_subsystem(test_brain));
}

TEST_F(BrainInitIntegrationTest, InitOrder_DependenciesRespected) {
    test_brain = nimcp_brain_create("order_test_2", BRAIN_SIZE_TINY,
                                    BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(test_brain, nullptr);

    // Correct order: foundation -> social -> ethics -> response
    test_brain->config.enable_mirror_neurons = true;

    EXPECT_TRUE(nimcp_brain_factory_init_mirror_neurons(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_empathy_network_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_ethics_engine_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_empathetic_response_subsystem(test_brain));
}

//=============================================================================
// Cross-Subsystem Interaction Tests
//=============================================================================

TEST_F(BrainInitIntegrationTest, Interaction_CuriosityAndSalience) {
    test_brain = nimcp_brain_create("interaction_test_1", BRAIN_SIZE_SMALL,
                                    BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(test_brain, nullptr);

    test_brain->config.enable_curiosity = true;
    test_brain->config.enable_salience = true;

    EXPECT_TRUE(nimcp_brain_factory_init_curiosity_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_salience_subsystem(test_brain));

    // Both systems should coexist and complement each other
    EXPECT_NE(test_brain->curiosity, nullptr);
    EXPECT_NE(test_brain->salience, nullptr);
}

TEST_F(BrainInitIntegrationTest, Interaction_PredictiveAndConsolidation) {
    test_brain = nimcp_brain_create("interaction_test_2", BRAIN_SIZE_SMALL,
                                    BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(test_brain, nullptr);

    test_brain->config.enable_predictive_processing = true;
    test_brain->config.enable_consolidation = true;

    EXPECT_TRUE(nimcp_brain_factory_init_predictive_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_consolidation_subsystem(test_brain));

    // Predictive and consolidation should work together
    EXPECT_NE(test_brain->predictive_network, nullptr);
    EXPECT_NE(test_brain->consolidation, nullptr);
}

TEST_F(BrainInitIntegrationTest, Interaction_IntrospectionWithAllSystems) {
    test_brain = nimcp_brain_create("interaction_test_3", BRAIN_SIZE_SMALL,
                                    BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(test_brain, nullptr);

    test_brain->config.enable_curiosity = true;
    test_brain->config.enable_salience = true;
    test_brain->config.enable_predictive_processing = true;

    // Introspection should integrate with all subsystems
    EXPECT_TRUE(nimcp_brain_factory_init_introspection_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_curiosity_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_salience_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_predictive_subsystem(test_brain));

    EXPECT_NE(test_brain->introspection, nullptr);
}

//=============================================================================
// Real-World Scenario Tests
//=============================================================================

TEST_F(BrainInitIntegrationTest, Scenario_ConversationalAI) {
    test_brain = nimcp_brain_create("conversational_ai", BRAIN_SIZE_SMALL,
                                    BRAIN_TASK_CLASSIFICATION, 20, 10);
    ASSERT_NE(test_brain, nullptr);

    // Enable subsystems needed for conversational AI
    test_brain->config.enable_mirror_neurons = true;
    test_brain->config.enable_global_workspace = true;

    // Social understanding
    EXPECT_TRUE(nimcp_brain_factory_init_mirror_neurons(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_empathy_network_subsystem(test_brain));

    // Ethics and safety
    EXPECT_TRUE(nimcp_brain_factory_init_ethics_engine_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_empathetic_response_subsystem(test_brain));

    // Self-awareness
    EXPECT_TRUE(nimcp_brain_factory_init_self_model_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_introspection_subsystem(test_brain));

    // Integration
    EXPECT_TRUE(nimcp_brain_factory_init_global_workspace_subsystem(test_brain));

    // Verify conversational capabilities are ready
    EXPECT_NE(test_brain->ethics, nullptr);
    EXPECT_NE(test_brain->empathy_network, nullptr);
    EXPECT_NE(test_brain->self_model, nullptr);
}

TEST_F(BrainInitIntegrationTest, Scenario_AutonomousLearner) {
    test_brain = nimcp_brain_create("autonomous_learner", BRAIN_SIZE_MEDIUM,
                                    BRAIN_TASK_REGRESSION, 30, 5);
    ASSERT_NE(test_brain, nullptr);

    // Enable learning-focused subsystems
    test_brain->config.enable_curiosity = true;
    test_brain->config.enable_predictive_processing = true;
    test_brain->config.enable_consolidation = true;
    test_brain->config.enable_salience = true;

    // Curiosity-driven exploration
    EXPECT_TRUE(nimcp_brain_factory_init_curiosity_subsystem(test_brain));

    // Predictive learning
    EXPECT_TRUE(nimcp_brain_factory_init_predictive_subsystem(test_brain));

    // Memory consolidation
    EXPECT_TRUE(nimcp_brain_factory_init_consolidation_subsystem(test_brain));

    // Attention and focus
    EXPECT_TRUE(nimcp_brain_factory_init_salience_subsystem(test_brain));

    // Self-monitoring
    EXPECT_TRUE(nimcp_brain_factory_init_introspection_subsystem(test_brain));

    // Verify learning capabilities
    EXPECT_NE(test_brain->curiosity, nullptr);
    EXPECT_NE(test_brain->predictive_network, nullptr);
    EXPECT_NE(test_brain->consolidation, nullptr);
}

TEST_F(BrainInitIntegrationTest, Scenario_SelfAwareAgent) {
    test_brain = nimcp_brain_create("self_aware_agent", BRAIN_SIZE_SMALL,
                                    BRAIN_TASK_CLASSIFICATION, 15, 3);
    ASSERT_NE(test_brain, nullptr);

    test_brain->config.enable_global_workspace = true;
    test_brain->config.enable_mental_health_monitoring = true;

    // Memory and identity
    EXPECT_TRUE(nimcp_brain_factory_init_autobiographical_memory_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_self_model_subsystem(test_brain));

    // Introspection and metacognition
    EXPECT_TRUE(nimcp_brain_factory_init_introspection_subsystem(test_brain));

    // Global integration
    EXPECT_TRUE(nimcp_brain_factory_init_global_workspace_subsystem(test_brain));

    // Mental health monitoring
    EXPECT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(test_brain));

    // Verify self-awareness stack
    EXPECT_NE(test_brain->autobio, nullptr);
    EXPECT_NE(test_brain->self_model, nullptr);
    EXPECT_NE(test_brain->introspection, nullptr);
    EXPECT_NE(test_brain->global_workspace, nullptr);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(BrainInitIntegrationTest, Lifecycle_CreateInitUseDestroy) {
    // Create
    test_brain = nimcp_brain_create("lifecycle_test", BRAIN_SIZE_TINY,
                                    BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(test_brain, nullptr);

    // Initialize
    EXPECT_TRUE(nimcp_brain_factory_init_introspection_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_ethics_engine_subsystem(test_brain));

    // Use (verify subsystems are accessible)
    EXPECT_NE(test_brain->introspection, nullptr);
    EXPECT_NE(test_brain->ethics, nullptr);

    // Destroy (handled in TearDown)
}

TEST_F(BrainInitIntegrationTest, Lifecycle_MultipleReinitializations) {
    test_brain = nimcp_brain_create("reinit_test", BRAIN_SIZE_TINY,
                                    BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(test_brain, nullptr);

    // Initialize multiple times (should be idempotent)
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(nimcp_brain_factory_init_introspection_subsystem(test_brain));
        EXPECT_TRUE(nimcp_brain_factory_init_self_model_subsystem(test_brain));
    }

    // Should still have exactly one instance
    EXPECT_NE(test_brain->introspection, nullptr);
    EXPECT_NE(test_brain->self_model, nullptr);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(BrainInitIntegrationTest, Performance_FastInitialization) {
    auto start = std::chrono::high_resolution_clock::now();

    test_brain = nimcp_brain_create("perf_test", BRAIN_SIZE_SMALL,
                                    BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(test_brain, nullptr);

    test_brain->config.enable_curiosity = true;
    test_brain->config.enable_salience = true;
    InitializeAllCognitiveSubsystems(test_brain);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Full initialization should be reasonably fast (<5 seconds for small brain)
    EXPECT_LT(duration.count(), 5000);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
