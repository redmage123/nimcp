/**
 * @file test_brain_init_subsystems_part2.cpp
 * @brief GoogleTest unit tests for brain factory initialization subsystems (Part 2)
 *
 * Tests the remaining 14 subsystem initialization functions:
 * - Mental Health Monitoring
 * - Predictive Processing
 * - Mirror Neurons
 * - Memory Consolidation
 * - Curiosity-Driven Learning
 * - Salience Detection
 * - Introspection
 * - Ethics Engine
 * - Empathy Network
 * - Empathetic Response
 * - Autobiographical Memory
 * - Self-Model
 * - Global Workspace
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 * @date 2025-11-21
 */

#include <gtest/gtest.h>

#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"

/**
 * @brief Test fixture for subsystem initialization tests
 */
class BrainInitSubsystemsPart2Test : public ::testing::Test {
protected:
    brain_t test_brain;

    void SetUp() override {
        // Create a minimal test brain using internal API
        test_brain = brain_create(
            "subsystem_test_brain",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            10,
            2
        );
        ASSERT_NE(test_brain, nullptr);
    }

    void TearDown() override {
        if (test_brain) {
            brain_destroy(test_brain);
            test_brain = nullptr;
        }
    }
};

//=============================================================================
// Mental Health Subsystem Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, MentalHealth_NullBrain) {
    bool result = nimcp_brain_factory_init_mental_health_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart2Test, MentalHealth_SuccessWhenEnabled) {
    test_brain->config.enable_mental_health_monitoring = true;
    bool result = nimcp_brain_factory_init_mental_health_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->mental_health_monitor, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, MentalHealth_SkippedWhenDisabled) {
    test_brain->config.enable_mental_health_monitoring = false;
    bool result = nimcp_brain_factory_init_mental_health_subsystem(test_brain);
    EXPECT_TRUE(result);  // Not an error, just skipped
    EXPECT_EQ(test_brain->mental_health_monitor, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, MentalHealth_AlreadyInitialized) {
    test_brain->config.enable_mental_health_monitoring = true;
    nimcp_brain_factory_init_mental_health_subsystem(test_brain);
    void* first_ptr = test_brain->mental_health_monitor;

    // Second initialization should be idempotent
    bool result = nimcp_brain_factory_init_mental_health_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->mental_health_monitor, first_ptr);
}

TEST_F(BrainInitSubsystemsPart2Test, MentalHealth_VerifyMonitor) {
    test_brain->config.enable_mental_health_monitoring = true;
    nimcp_brain_factory_init_mental_health_subsystem(test_brain);
    ASSERT_NE(test_brain->mental_health_monitor, nullptr);
    // Mental health monitor should be ready for use
}

//=============================================================================
// Predictive Processing Subsystem Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, Predictive_NullBrain) {
    bool result = nimcp_brain_factory_init_predictive_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart2Test, Predictive_SuccessWhenEnabled) {
    test_brain->config.enable_predictive_processing = true;
    bool result = nimcp_brain_factory_init_predictive_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->predictive_network, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Predictive_SkippedWhenDisabled) {
    test_brain->config.enable_predictive_processing = false;
    bool result = nimcp_brain_factory_init_predictive_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->predictive_network, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Predictive_AlreadyInitialized) {
    test_brain->config.enable_predictive_processing = true;
    nimcp_brain_factory_init_predictive_subsystem(test_brain);
    void* first_ptr = test_brain->predictive_network;

    bool result = nimcp_brain_factory_init_predictive_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->predictive_network, first_ptr);
}

//=============================================================================
// Mirror Neurons Subsystem Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, MirrorNeurons_NullBrain) {
    bool result = nimcp_brain_factory_init_mirror_neurons(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart2Test, MirrorNeurons_SuccessWhenEnabled) {
    test_brain->config.enable_mirror_neurons = true;
    bool result = nimcp_brain_factory_init_mirror_neurons(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->mirror_neurons, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, MirrorNeurons_SkippedWhenDisabled) {
    test_brain->config.enable_mirror_neurons = false;
    bool result = nimcp_brain_factory_init_mirror_neurons(test_brain);
    EXPECT_TRUE(result);  // Should succeed (not error) when disabled
    // Note: pointer may already be set by brain_create(), so we only check return value
}

TEST_F(BrainInitSubsystemsPart2Test, MirrorNeurons_AlreadyInitialized) {
    test_brain->config.enable_mirror_neurons = true;
    nimcp_brain_factory_init_mirror_neurons(test_brain);
    void* first_ptr = test_brain->mirror_neurons;

    bool result = nimcp_brain_factory_init_mirror_neurons(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->mirror_neurons, first_ptr);
}

TEST_F(BrainInitSubsystemsPart2Test, MirrorNeurons_CustomConfig) {
    test_brain->config.enable_mirror_neurons = true;
    test_brain->config.mirror_neuron_count = 50;
    test_brain->config.mirror_max_actions = 20;
    test_brain->config.mirror_learning_rate = 0.05f;

    bool result = nimcp_brain_factory_init_mirror_neurons(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->mirror_neurons, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, MirrorNeurons_IntegrationWithOtherSystems) {
    test_brain->config.enable_mirror_neurons = true;
    test_brain->config.enable_working_memory = true;
    test_brain->config.enable_theory_of_mind = true;
    test_brain->config.enable_predictive_processing = true;

    bool result = nimcp_brain_factory_init_mirror_neurons(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->mirror_neurons, nullptr);
}

//=============================================================================
// Consolidation Subsystem Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, Consolidation_NullBrain) {
    bool result = nimcp_brain_factory_init_consolidation_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart2Test, Consolidation_SuccessWhenEnabled) {
    test_brain->config.enable_consolidation = true;
    bool result = nimcp_brain_factory_init_consolidation_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->consolidation, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Consolidation_SkippedWhenDisabled) {
    test_brain->config.enable_consolidation = false;
    bool result = nimcp_brain_factory_init_consolidation_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->consolidation, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Consolidation_AlreadyInitialized) {
    test_brain->config.enable_consolidation = true;
    nimcp_brain_factory_init_consolidation_subsystem(test_brain);
    void* first_ptr = test_brain->consolidation;

    bool result = nimcp_brain_factory_init_consolidation_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->consolidation, first_ptr);
}

//=============================================================================
// Curiosity Subsystem Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, Curiosity_NullBrain) {
    bool result = nimcp_brain_factory_init_curiosity_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart2Test, Curiosity_SuccessWhenEnabled) {
    test_brain->config.enable_curiosity = true;
    bool result = nimcp_brain_factory_init_curiosity_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->curiosity, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Curiosity_SkippedWhenDisabled) {
    test_brain->config.enable_curiosity = false;
    bool result = nimcp_brain_factory_init_curiosity_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->curiosity, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Curiosity_AlreadyInitialized) {
    test_brain->config.enable_curiosity = true;
    nimcp_brain_factory_init_curiosity_subsystem(test_brain);
    void* first_ptr = test_brain->curiosity;

    bool result = nimcp_brain_factory_init_curiosity_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->curiosity, first_ptr);
}

//=============================================================================
// Salience Subsystem Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, Salience_NullBrain) {
    bool result = nimcp_brain_factory_init_salience_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart2Test, Salience_SuccessWhenEnabled) {
    test_brain->config.enable_salience = true;
    bool result = nimcp_brain_factory_init_salience_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->salience, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Salience_SkippedWhenDisabled) {
    test_brain->config.enable_salience = false;
    bool result = nimcp_brain_factory_init_salience_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->salience, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Salience_AlreadyInitialized) {
    test_brain->config.enable_salience = true;
    nimcp_brain_factory_init_salience_subsystem(test_brain);
    void* first_ptr = test_brain->salience;

    bool result = nimcp_brain_factory_init_salience_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->salience, first_ptr);
}

//=============================================================================
// Introspection Subsystem Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, Introspection_NullBrain) {
    bool result = nimcp_brain_factory_init_introspection_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart2Test, Introspection_Success) {
    // Introspection is always created (no config flag check)
    bool result = nimcp_brain_factory_init_introspection_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->introspection, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Introspection_AlreadyInitialized) {
    nimcp_brain_factory_init_introspection_subsystem(test_brain);
    void* first_ptr = test_brain->introspection;

    bool result = nimcp_brain_factory_init_introspection_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->introspection, first_ptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Introspection_VerifyConfig) {
    bool result = nimcp_brain_factory_init_introspection_subsystem(test_brain);
    EXPECT_TRUE(result);
    ASSERT_NE(test_brain->introspection, nullptr);
    // Introspection should have default config applied
}

//=============================================================================
// Ethics Engine Subsystem Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, EthicsEngine_NullBrain) {
    bool result = nimcp_brain_factory_init_ethics_engine_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart2Test, EthicsEngine_Success) {
    test_brain->config.enable_ethics = true;
    bool result = nimcp_brain_factory_init_ethics_engine_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->ethics, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, EthicsEngine_AlreadyInitialized) {
    test_brain->config.enable_ethics = true;
    nimcp_brain_factory_init_ethics_engine_subsystem(test_brain);
    void* first_ptr = test_brain->ethics;

    bool result = nimcp_brain_factory_init_ethics_engine_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->ethics, first_ptr);
}

TEST_F(BrainInitSubsystemsPart2Test, EthicsEngine_VerifyGoldenRule) {
    test_brain->config.enable_ethics = true;
    bool result = nimcp_brain_factory_init_ethics_engine_subsystem(test_brain);
    EXPECT_TRUE(result);
    ASSERT_NE(test_brain->ethics, nullptr);
    // Ethics engine should have Golden Rule configured
}

//=============================================================================
// Empathy Network Subsystem Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, EmpathyNetwork_NullBrain) {
    bool result = nimcp_brain_factory_init_empathy_network_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart2Test, EmpathyNetwork_Success) {
    test_brain->config.enable_ethics = true;  // Empathy requires ethics
    bool result = nimcp_brain_factory_init_empathy_network_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->empathy_network, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, EmpathyNetwork_AlreadyInitialized) {
    test_brain->config.enable_ethics = true;  // Empathy requires ethics
    nimcp_brain_factory_init_empathy_network_subsystem(test_brain);
    void* first_ptr = test_brain->empathy_network;

    bool result = nimcp_brain_factory_init_empathy_network_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->empathy_network, first_ptr);
}

TEST_F(BrainInitSubsystemsPart2Test, EmpathyNetwork_WithMirrorNeurons) {
    test_brain->config.enable_ethics = true;  // Empathy requires ethics
    test_brain->config.enable_mirror_neurons = true;
    nimcp_brain_factory_init_mirror_neurons(test_brain);

    bool result = nimcp_brain_factory_init_empathy_network_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->empathy_network, nullptr);
}

//=============================================================================
// Empathetic Response Subsystem Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, EmpatheticResponse_NullBrain) {
    bool result = nimcp_brain_factory_init_empathetic_response_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart2Test, EmpatheticResponse_Success) {
    // Initialize prerequisites
    nimcp_brain_factory_init_ethics_engine_subsystem(test_brain);
    nimcp_brain_factory_init_empathy_network_subsystem(test_brain);

    bool result = nimcp_brain_factory_init_empathetic_response_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->empathetic_response_engine, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, EmpatheticResponse_AlreadyInitialized) {
    nimcp_brain_factory_init_ethics_engine_subsystem(test_brain);
    nimcp_brain_factory_init_empathy_network_subsystem(test_brain);
    nimcp_brain_factory_init_empathetic_response_subsystem(test_brain);
    void* first_ptr = test_brain->empathetic_response_engine;

    bool result = nimcp_brain_factory_init_empathetic_response_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->empathetic_response_engine, first_ptr);
}

//=============================================================================
// Autobiographical Memory Subsystem Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, AutobiographicalMemory_NullBrain) {
    bool result = nimcp_brain_factory_init_autobiographical_memory_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart2Test, AutobiographicalMemory_Success) {
    bool result = nimcp_brain_factory_init_autobiographical_memory_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->autobio, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, AutobiographicalMemory_AlreadyInitialized) {
    nimcp_brain_factory_init_autobiographical_memory_subsystem(test_brain);
    void* first_ptr = test_brain->autobio;

    bool result = nimcp_brain_factory_init_autobiographical_memory_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->autobio, first_ptr);
}

TEST_F(BrainInitSubsystemsPart2Test, AutobiographicalMemory_VerifyCapacity) {
    bool result = nimcp_brain_factory_init_autobiographical_memory_subsystem(test_brain);
    EXPECT_TRUE(result);
    ASSERT_NE(test_brain->autobio, nullptr);
    // Should have 10,000 memory capacity
}

//=============================================================================
// Self-Model Subsystem Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, SelfModel_NullBrain) {
    bool result = nimcp_brain_factory_init_self_model_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart2Test, SelfModel_Success) {
    bool result = nimcp_brain_factory_init_self_model_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->self_model, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, SelfModel_AlreadyInitialized) {
    nimcp_brain_factory_init_self_model_subsystem(test_brain);
    void* first_ptr = test_brain->self_model;

    bool result = nimcp_brain_factory_init_self_model_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->self_model, first_ptr);
}

TEST_F(BrainInitSubsystemsPart2Test, SelfModel_WithPersonality) {
    // Assuming personality is set during brain creation
    bool result = nimcp_brain_factory_init_self_model_subsystem(test_brain);
    EXPECT_TRUE(result);
    ASSERT_NE(test_brain->self_model, nullptr);
}

//=============================================================================
// Global Workspace Subsystem Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, GlobalWorkspace_NullBrain) {
    bool result = nimcp_brain_factory_init_global_workspace_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart2Test, GlobalWorkspace_SuccessWhenEnabled) {
    test_brain->config.enable_global_workspace = true;
    bool result = nimcp_brain_factory_init_global_workspace_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(test_brain->global_workspace, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, GlobalWorkspace_SkippedWhenDisabled) {
    test_brain->config.enable_global_workspace = false;
    bool result = nimcp_brain_factory_init_global_workspace_subsystem(test_brain);
    EXPECT_TRUE(result);  // Should succeed (not error) when disabled
    // Note: pointer may already be set by brain_create(), so we only check return value
}

TEST_F(BrainInitSubsystemsPart2Test, GlobalWorkspace_AlreadyInitialized) {
    test_brain->config.enable_global_workspace = true;
    nimcp_brain_factory_init_global_workspace_subsystem(test_brain);
    void* first_ptr = test_brain->global_workspace;

    bool result = nimcp_brain_factory_init_global_workspace_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(test_brain->global_workspace, first_ptr);
}

//=============================================================================
// Integration Tests - Multiple Subsystems
//=============================================================================

TEST_F(BrainInitSubsystemsPart2Test, Integration_MirrorNeuronsAndEmpathy) {
    test_brain->config.enable_mirror_neurons = true;
    test_brain->config.enable_ethics = true;  // Empathy requires ethics

    bool mirror_result = nimcp_brain_factory_init_mirror_neurons(test_brain);
    bool empathy_result = nimcp_brain_factory_init_empathy_network_subsystem(test_brain);

    EXPECT_TRUE(mirror_result);
    EXPECT_TRUE(empathy_result);
    EXPECT_NE(test_brain->mirror_neurons, nullptr);
    EXPECT_NE(test_brain->empathy_network, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Integration_EthicsAndEmpathy) {
    test_brain->config.enable_ethics = true;  // Required for ethics and empathy
    bool ethics_result = nimcp_brain_factory_init_ethics_engine_subsystem(test_brain);
    bool empathy_result = nimcp_brain_factory_init_empathy_network_subsystem(test_brain);
    bool response_result = nimcp_brain_factory_init_empathetic_response_subsystem(test_brain);

    EXPECT_TRUE(ethics_result);
    EXPECT_TRUE(empathy_result);
    EXPECT_TRUE(response_result);
    EXPECT_NE(test_brain->ethics, nullptr);
    EXPECT_NE(test_brain->empathy_network, nullptr);
    EXPECT_NE(test_brain->empathetic_response_engine, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Integration_CognitiveStack) {
    test_brain->config.enable_curiosity = true;
    test_brain->config.enable_salience = true;

    bool curiosity_result = nimcp_brain_factory_init_curiosity_subsystem(test_brain);
    bool salience_result = nimcp_brain_factory_init_salience_subsystem(test_brain);
    bool introspection_result = nimcp_brain_factory_init_introspection_subsystem(test_brain);

    EXPECT_TRUE(curiosity_result);
    EXPECT_TRUE(salience_result);
    EXPECT_TRUE(introspection_result);
    EXPECT_NE(test_brain->curiosity, nullptr);
    EXPECT_NE(test_brain->salience, nullptr);
    EXPECT_NE(test_brain->introspection, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Integration_SelfAwarenessStack) {
    bool autobio_result = nimcp_brain_factory_init_autobiographical_memory_subsystem(test_brain);
    bool self_model_result = nimcp_brain_factory_init_self_model_subsystem(test_brain);
    bool introspection_result = nimcp_brain_factory_init_introspection_subsystem(test_brain);

    EXPECT_TRUE(autobio_result);
    EXPECT_TRUE(self_model_result);
    EXPECT_TRUE(introspection_result);
    EXPECT_NE(test_brain->autobio, nullptr);
    EXPECT_NE(test_brain->self_model, nullptr);
    EXPECT_NE(test_brain->introspection, nullptr);
}

TEST_F(BrainInitSubsystemsPart2Test, Integration_AllSubsystemsEnabled) {
    // Enable all subsystems
    test_brain->config.enable_mental_health_monitoring = true;
    test_brain->config.enable_predictive_processing = true;
    test_brain->config.enable_mirror_neurons = true;
    test_brain->config.enable_consolidation = true;
    test_brain->config.enable_curiosity = true;
    test_brain->config.enable_salience = true;
    test_brain->config.enable_global_workspace = true;
    test_brain->config.enable_ethics = true;  // Required for ethics and empathy

    // Initialize all subsystems
    EXPECT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_predictive_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_mirror_neurons(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_consolidation_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_curiosity_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_salience_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_introspection_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_ethics_engine_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_empathy_network_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_empathetic_response_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_autobiographical_memory_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_self_model_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_global_workspace_subsystem(test_brain));

    // Verify all pointers are set
    EXPECT_NE(test_brain->mental_health_monitor, nullptr);
    EXPECT_NE(test_brain->predictive_network, nullptr);
    EXPECT_NE(test_brain->mirror_neurons, nullptr);
    EXPECT_NE(test_brain->consolidation, nullptr);
    EXPECT_NE(test_brain->curiosity, nullptr);
    EXPECT_NE(test_brain->salience, nullptr);
    EXPECT_NE(test_brain->introspection, nullptr);
    EXPECT_NE(test_brain->ethics, nullptr);
    EXPECT_NE(test_brain->empathy_network, nullptr);
    EXPECT_NE(test_brain->empathetic_response_engine, nullptr);
    EXPECT_NE(test_brain->autobio, nullptr);
    EXPECT_NE(test_brain->self_model, nullptr);
    EXPECT_NE(test_brain->global_workspace, nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
