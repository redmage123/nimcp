/**
 * @file test_brain_init_regression.cpp
 * @brief Regression tests for brain factory initialization stability
 *
 * Tests for:
 * - Backward compatibility with older initialization patterns
 * - Performance regression detection
 * - Memory leak detection
 * - Stability under stress
 * - Edge cases and corner cases
 * - Long-running initialization scenarios
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 * @date 2025-11-21
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cstring>

#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"

/**
 * @brief Test fixture for regression tests
 */
class BrainInitRegressionTest : public ::testing::Test {
protected:
    std::vector<brain_t> test_brains;

    void SetUp() override {
        test_brains.clear();
    }

    void TearDown() override {
        for (auto brain : test_brains) {
            if (brain) {
                nimcp_brain_destroy(brain);
            }
        }
        test_brains.clear();
    }

    brain_t CreateTestBrain(const char* name, brain_size_t size = BRAIN_SIZE_TINY) {
        brain_t brain = nimcp_brain_create(name, size, BRAIN_TASK_CLASSIFICATION, 5, 2);
        if (brain) {
            test_brains.push_back(brain);
        }
        return brain;
    }
};

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(BrainInitRegressionTest, BackwardCompat_BasicInitialization) {
    // Test that basic initialization still works as before
    brain_t brain = CreateTestBrain("compat_test_1");
    ASSERT_NE(brain, nullptr);

    // Old pattern: initialize subsystems individually
    EXPECT_TRUE(nimcp_brain_factory_init_introspection_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_ethics_engine_subsystem(brain));

    EXPECT_NE(brain->introspection, nullptr);
    EXPECT_NE(brain->ethics, nullptr);
}

TEST_F(BrainInitRegressionTest, BackwardCompat_ConfigFlags) {
    // Test that old config flag patterns still work
    brain_t brain = CreateTestBrain("compat_test_2");
    ASSERT_NE(brain, nullptr);

    // Old pattern: set config flags before initialization
    brain->config.enable_curiosity = true;
    brain->config.enable_salience = true;
    brain->config.enable_mental_health_monitoring = true;

    EXPECT_TRUE(nimcp_brain_factory_init_curiosity_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_salience_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain));

    EXPECT_NE(brain->curiosity, nullptr);
    EXPECT_NE(brain->salience, nullptr);
    EXPECT_NE(brain->mental_health_monitor, nullptr);
}

TEST_F(BrainInitRegressionTest, BackwardCompat_NullHandling) {
    // Ensure NULL handling remains consistent
    EXPECT_FALSE(nimcp_brain_factory_init_mental_health_subsystem(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_predictive_subsystem(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_mirror_neurons(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_consolidation_subsystem(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_curiosity_subsystem(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_salience_subsystem(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_introspection_subsystem(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_ethics_engine_subsystem(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_empathy_network_subsystem(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_empathetic_response_subsystem(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_autobiographical_memory_subsystem(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_self_model_subsystem(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_global_workspace_subsystem(nullptr));
}

TEST_F(BrainInitRegressionTest, BackwardCompat_DisabledSubsystems) {
    // Test that disabled subsystems still return true (not an error)
    brain_t brain = CreateTestBrain("compat_test_3");
    ASSERT_NE(brain, nullptr);

    brain->config.enable_curiosity = false;
    brain->config.enable_salience = false;
    brain->config.enable_predictive_processing = false;

    EXPECT_TRUE(nimcp_brain_factory_init_curiosity_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_salience_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_predictive_subsystem(brain));

    EXPECT_EQ(brain->curiosity, nullptr);
    EXPECT_EQ(brain->salience, nullptr);
    EXPECT_EQ(brain->predictive_network, nullptr);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(BrainInitRegressionTest, Performance_SingleSubsystemInit) {
    brain_t brain = CreateTestBrain("perf_test_1");
    ASSERT_NE(brain, nullptr);

    auto start = std::chrono::high_resolution_clock::now();
    nimcp_brain_factory_init_introspection_subsystem(brain);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Single subsystem init should be very fast (<10ms)
    EXPECT_LT(duration.count(), 10000);
}

TEST_F(BrainInitRegressionTest, Performance_AllSubsystemsInit) {
    brain_t brain = CreateTestBrain("perf_test_2", BRAIN_SIZE_SMALL);
    ASSERT_NE(brain, nullptr);

    // Enable all subsystems
    brain->config.enable_mental_health_monitoring = true;
    brain->config.enable_predictive_processing = true;
    brain->config.enable_mirror_neurons = true;
    brain->config.enable_consolidation = true;
    brain->config.enable_curiosity = true;
    brain->config.enable_salience = true;
    brain->config.enable_global_workspace = true;

    auto start = std::chrono::high_resolution_clock::now();

    // Initialize all subsystems
    nimcp_brain_factory_init_mental_health_subsystem(brain);
    nimcp_brain_factory_init_predictive_subsystem(brain);
    nimcp_brain_factory_init_mirror_neurons(brain);
    nimcp_brain_factory_init_consolidation_subsystem(brain);
    nimcp_brain_factory_init_curiosity_subsystem(brain);
    nimcp_brain_factory_init_salience_subsystem(brain);
    nimcp_brain_factory_init_introspection_subsystem(brain);
    nimcp_brain_factory_init_ethics_engine_subsystem(brain);
    nimcp_brain_factory_init_empathy_network_subsystem(brain);
    nimcp_brain_factory_init_empathetic_response_subsystem(brain);
    nimcp_brain_factory_init_autobiographical_memory_subsystem(brain);
    nimcp_brain_factory_init_self_model_subsystem(brain);
    nimcp_brain_factory_init_global_workspace_subsystem(brain);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // All subsystems should initialize in reasonable time (<2 seconds)
    EXPECT_LT(duration.count(), 2000);
}

TEST_F(BrainInitRegressionTest, Performance_RepeatedInitialization) {
    brain_t brain = CreateTestBrain("perf_test_3");
    ASSERT_NE(brain, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    // Initialize same subsystem multiple times (should be idempotent and fast)
    for (int i = 0; i < 100; i++) {
        nimcp_brain_factory_init_introspection_subsystem(brain);
        nimcp_brain_factory_init_self_model_subsystem(brain);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Repeated initialization should be very fast (<100ms for 100 iterations)
    EXPECT_LT(duration.count(), 100);
}

//=============================================================================
// Memory Leak Tests
//=============================================================================

TEST_F(BrainInitRegressionTest, MemoryLeak_SingleInitDestroy) {
    // Create and destroy multiple brains to check for leaks
    for (int i = 0; i < 10; i++) {
        char name[64];
        snprintf(name, sizeof(name), "leak_test_%d", i);

        brain_t brain = nimcp_brain_create(name, BRAIN_SIZE_TINY,
                                          BRAIN_TASK_CLASSIFICATION, 5, 2);
        ASSERT_NE(brain, nullptr);

        nimcp_brain_factory_init_introspection_subsystem(brain);
        nimcp_brain_factory_init_self_model_subsystem(brain);

        nimcp_brain_destroy(brain);
    }
    // If there are memory leaks, they'll be caught by valgrind/asan
}

TEST_F(BrainInitRegressionTest, MemoryLeak_AllSubsystemsInitDestroy) {
    for (int i = 0; i < 5; i++) {
        char name[64];
        snprintf(name, sizeof(name), "full_leak_test_%d", i);

        brain_t brain = nimcp_brain_create(name, BRAIN_SIZE_TINY,
                                          BRAIN_TASK_CLASSIFICATION, 5, 2);
        ASSERT_NE(brain, nullptr);

        brain->config.enable_curiosity = true;
        brain->config.enable_salience = true;
        brain->config.enable_predictive_processing = true;

        // Initialize multiple subsystems
        nimcp_brain_factory_init_curiosity_subsystem(brain);
        nimcp_brain_factory_init_salience_subsystem(brain);
        nimcp_brain_factory_init_predictive_subsystem(brain);
        nimcp_brain_factory_init_introspection_subsystem(brain);
        nimcp_brain_factory_init_ethics_engine_subsystem(brain);
        nimcp_brain_factory_init_self_model_subsystem(brain);

        nimcp_brain_destroy(brain);
    }
}

TEST_F(BrainInitRegressionTest, MemoryLeak_IdempotentInitialization) {
    brain_t brain = CreateTestBrain("idempotent_leak_test");
    ASSERT_NE(brain, nullptr);

    brain->config.enable_curiosity = true;

    // Initialize same subsystem many times
    for (int i = 0; i < 50; i++) {
        nimcp_brain_factory_init_curiosity_subsystem(brain);
        nimcp_brain_factory_init_introspection_subsystem(brain);
    }

    // Should not leak memory (valgrind/asan will catch this)
}

//=============================================================================
// Stability Tests
//=============================================================================

TEST_F(BrainInitRegressionTest, Stability_RandomInitializationOrder) {
    brain_t brain = CreateTestBrain("stability_test_1");
    ASSERT_NE(brain, nullptr);

    brain->config.enable_curiosity = true;
    brain->config.enable_salience = true;
    brain->config.enable_predictive_processing = true;

    // Initialize in random order (should still work)
    EXPECT_TRUE(nimcp_brain_factory_init_self_model_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_curiosity_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_ethics_engine_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_salience_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_introspection_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_predictive_subsystem(brain));

    EXPECT_NE(brain->self_model, nullptr);
    EXPECT_NE(brain->curiosity, nullptr);
    EXPECT_NE(brain->ethics, nullptr);
    EXPECT_NE(brain->salience, nullptr);
    EXPECT_NE(brain->introspection, nullptr);
    EXPECT_NE(brain->predictive_network, nullptr);
}

TEST_F(BrainInitRegressionTest, Stability_PartialInitialization) {
    brain_t brain = CreateTestBrain("stability_test_2");
    ASSERT_NE(brain, nullptr);

    brain->config.enable_curiosity = true;
    brain->config.enable_salience = false;

    // Initialize only some subsystems
    EXPECT_TRUE(nimcp_brain_factory_init_curiosity_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_salience_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_introspection_subsystem(brain));

    EXPECT_NE(brain->curiosity, nullptr);
    EXPECT_EQ(brain->salience, nullptr);  // Disabled
    EXPECT_NE(brain->introspection, nullptr);
}

TEST_F(BrainInitRegressionTest, Stability_MultipleConfigChanges) {
    brain_t brain = CreateTestBrain("stability_test_3");
    ASSERT_NE(brain, nullptr);

    // Change config multiple times
    brain->config.enable_curiosity = true;
    EXPECT_TRUE(nimcp_brain_factory_init_curiosity_subsystem(brain));
    EXPECT_NE(brain->curiosity, nullptr);

    // Changing config after init shouldn't break anything
    brain->config.enable_curiosity = false;
    EXPECT_TRUE(nimcp_brain_factory_init_curiosity_subsystem(brain));
    EXPECT_NE(brain->curiosity, nullptr);  // Still initialized
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(BrainInitRegressionTest, EdgeCase_TinyBrainAllSubsystems) {
    brain_t brain = CreateTestBrain("edge_test_1", BRAIN_SIZE_TINY);
    ASSERT_NE(brain, nullptr);

    // Enable all subsystems on tiny brain
    brain->config.enable_mental_health_monitoring = true;
    brain->config.enable_predictive_processing = true;
    brain->config.enable_mirror_neurons = true;
    brain->config.enable_consolidation = true;
    brain->config.enable_curiosity = true;
    brain->config.enable_salience = true;
    brain->config.enable_global_workspace = true;

    // Should still initialize successfully
    EXPECT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_predictive_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_mirror_neurons(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_consolidation_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_curiosity_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_salience_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_global_workspace_subsystem(brain));
}

TEST_F(BrainInitRegressionTest, EdgeCase_CustomMirrorNeuronConfig) {
    brain_t brain = CreateTestBrain("edge_test_2");
    ASSERT_NE(brain, nullptr);

    brain->config.enable_mirror_neurons = true;
    brain->config.mirror_neuron_count = 0;  // Edge case: zero count
    brain->config.mirror_max_actions = 0;
    brain->config.mirror_learning_rate = 0.0f;

    // Should still initialize (will use defaults)
    EXPECT_TRUE(nimcp_brain_factory_init_mirror_neurons(brain));
    EXPECT_NE(brain->mirror_neurons, nullptr);
}

TEST_F(BrainInitRegressionTest, EdgeCase_NoSubsystemsEnabled) {
    brain_t brain = CreateTestBrain("edge_test_3");
    ASSERT_NE(brain, nullptr);

    // Disable all optional subsystems
    brain->config.enable_mental_health_monitoring = false;
    brain->config.enable_predictive_processing = false;
    brain->config.enable_mirror_neurons = false;
    brain->config.enable_consolidation = false;
    brain->config.enable_curiosity = false;
    brain->config.enable_salience = false;
    brain->config.enable_global_workspace = false;

    // All should return true (not an error to skip)
    EXPECT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_predictive_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_mirror_neurons(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_consolidation_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_curiosity_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_salience_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_global_workspace_subsystem(brain));

    // All should be NULL
    EXPECT_EQ(brain->mental_health_monitor, nullptr);
    EXPECT_EQ(brain->predictive_network, nullptr);
    EXPECT_EQ(brain->mirror_neurons, nullptr);
    EXPECT_EQ(brain->consolidation, nullptr);
    EXPECT_EQ(brain->curiosity, nullptr);
    EXPECT_EQ(brain->salience, nullptr);
    EXPECT_EQ(brain->global_workspace, nullptr);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(BrainInitRegressionTest, Stress_ManyBrainsSequential) {
    // Create many brains sequentially
    for (int i = 0; i < 20; i++) {
        char name[64];
        snprintf(name, sizeof(name), "stress_brain_%d", i);

        brain_t brain = nimcp_brain_create(name, BRAIN_SIZE_TINY,
                                          BRAIN_TASK_CLASSIFICATION, 5, 2);
        ASSERT_NE(brain, nullptr);

        nimcp_brain_factory_init_introspection_subsystem(brain);
        nimcp_brain_factory_init_self_model_subsystem(brain);

        nimcp_brain_destroy(brain);
    }
}

TEST_F(BrainInitRegressionTest, Stress_RepeatedInitDestroyCycles) {
    // Repeated create/init/destroy cycles
    for (int i = 0; i < 15; i++) {
        brain_t brain = CreateTestBrain("cycle_test");
        ASSERT_NE(brain, nullptr);

        brain->config.enable_curiosity = true;
        brain->config.enable_salience = true;

        nimcp_brain_factory_init_curiosity_subsystem(brain);
        nimcp_brain_factory_init_salience_subsystem(brain);
        nimcp_brain_factory_init_introspection_subsystem(brain);

        // Will be destroyed in TearDown
    }
}

TEST_F(BrainInitRegressionTest, Stress_AllSubsystemsMultipleBrains) {
    // Create multiple brains with all subsystems
    for (int i = 0; i < 5; i++) {
        char name[64];
        snprintf(name, sizeof(name), "full_brain_%d", i);

        brain_t brain = CreateTestBrain(name);
        ASSERT_NE(brain, nullptr);

        brain->config.enable_mental_health_monitoring = true;
        brain->config.enable_predictive_processing = true;
        brain->config.enable_mirror_neurons = true;
        brain->config.enable_curiosity = true;
        brain->config.enable_salience = true;

        nimcp_brain_factory_init_mental_health_subsystem(brain);
        nimcp_brain_factory_init_predictive_subsystem(brain);
        nimcp_brain_factory_init_mirror_neurons(brain);
        nimcp_brain_factory_init_curiosity_subsystem(brain);
        nimcp_brain_factory_init_salience_subsystem(brain);
        nimcp_brain_factory_init_introspection_subsystem(brain);
        nimcp_brain_factory_init_ethics_engine_subsystem(brain);
        nimcp_brain_factory_init_self_model_subsystem(brain);
    }
}

//=============================================================================
// Regression Bug Tests
//=============================================================================

TEST_F(BrainInitRegressionTest, BugFix_DoubleInitDoesNotLeak) {
    // Regression test: double initialization should not leak memory
    brain_t brain = CreateTestBrain("bugfix_test_1");
    ASSERT_NE(brain, nullptr);

    brain->config.enable_curiosity = true;

    void* first_ptr = nullptr;

    // First init
    nimcp_brain_factory_init_curiosity_subsystem(brain);
    first_ptr = brain->curiosity;
    ASSERT_NE(first_ptr, nullptr);

    // Second init should be idempotent
    nimcp_brain_factory_init_curiosity_subsystem(brain);
    EXPECT_EQ(brain->curiosity, first_ptr);  // Same pointer
}

TEST_F(BrainInitRegressionTest, BugFix_NullConfigHandling) {
    // Regression test: NULL brain with valid config should fail gracefully
    EXPECT_FALSE(nimcp_brain_factory_init_introspection_subsystem(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_self_model_subsystem(nullptr));
}

TEST_F(BrainInitRegressionTest, BugFix_PartialDependencyInit) {
    // Regression test: empathetic response without ethics/empathy
    brain_t brain = CreateTestBrain("bugfix_test_3");
    ASSERT_NE(brain, nullptr);

    // Initialize empathetic response without prerequisites
    // Should handle gracefully (may fail or create with NULL refs)
    nimcp_brain_factory_init_empathetic_response_subsystem(brain);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
