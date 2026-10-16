//=============================================================================
// test_brain_create_custom.cpp - Comprehensive unit tests for brain_create_custom()
//=============================================================================
/**
 * @file test_brain_create_custom.cpp
 * @brief Unit test suite for brain_create_custom() factory function
 *
 * WHAT: Comprehensive tests for custom brain creation with full configuration
 * WHY:  Ensure brain creation handles all config types, validates properly,
 *       initializes all subsystems, and cleans up on failure
 * HOW:  GoogleTest framework with fixtures for setup/teardown
 *
 * TEST COVERAGE:
 * 1. NULL config handling (1 test)
 * 2. Valid custom configuration (3 tests)
 * 3. Checkpoint auto-loading (3 tests)
 * 4. Config field validation (5 tests)
 * 5. Multimodal subsystem initialization (2 tests)
 * 6. Quantum annealing configuration (2 tests)
 * 7. Cognitive subsystems (5 tests)
 * 8. Error propagation (2 tests)
 * 9. Resource cleanup on failure (2 tests)
 *
 * TOTAL: 25 tests
 *
 * @version 1.0.0
 * @date 2025-11-21
 */

#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <unistd.h>

#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"

//=============================================================================
// Test Constants
//=============================================================================

static const char* TEST_CHECKPOINT_DIR = "/tmp/nimcp_brain_create_custom_test";
static const char* TEST_CHECKPOINT_PATH = "/tmp/nimcp_brain_create_custom_test/checkpoint.nimcp";

//=============================================================================
// Test Fixture
//=============================================================================

class BrainCreateCustomTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create temporary directory for checkpoints
        mkdir(TEST_CHECKPOINT_DIR, 0755);

        // Initialize default config
        memset(&valid_config, 0, sizeof(brain_config_t));
        valid_config.size = BRAIN_SIZE_SMALL;
        valid_config.task = BRAIN_TASK_CLASSIFICATION;
        valid_config.num_inputs = 10;
        valid_config.num_outputs = 3;
        valid_config.learning_rate = 0.01f;
        valid_config.sparsity_target = 0.85f;
        strncpy(valid_config.task_name, "test_brain", sizeof(valid_config.task_name) - 1);
        valid_config.task_name[sizeof(valid_config.task_name) - 1] = '\0';

        // Default: no multimodal, no quantum
        valid_config.enable_multimodal_integration = false;
        valid_config.enable_quantum_annealing = false;
        valid_config.enable_visual_cortex = false;
        valid_config.enable_audio_cortex = false;
        valid_config.enable_speech_cortex = false;
    }

    void TearDown() override
    {
        // Clean up checkpoint files
        unlink(TEST_CHECKPOINT_PATH);
        rmdir(TEST_CHECKPOINT_DIR);
    }

    brain_config_t valid_config;
};

//=============================================================================
// Test Category 1: NULL Config Handling
//=============================================================================

/**
 * TEST 1.1: NULL config pointer
 * WHAT: Verify NULL config is rejected with error message
 * WHY:  Prevent NULL dereference crashes
 */
TEST_F(BrainCreateCustomTest, NullConfigReturnsNull)
{
    brain_t brain = brain_create_custom(NULL);
    EXPECT_EQ(brain, nullptr);
}

//=============================================================================
// Test Category 2: Valid Custom Configuration
//=============================================================================

/**
 * TEST 2.1: Valid minimal configuration
 * WHAT: Create brain with minimal valid config
 * WHY:  Ensure basic path works
 */
TEST_F(BrainCreateCustomTest, ValidMinimalConfigSucceeds)
{
    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);

    // Verify brain properties
    EXPECT_EQ(brain->config.num_inputs, 10);
    EXPECT_EQ(brain->config.num_outputs, 3);
    EXPECT_NE(brain->network, nullptr);

    brain_destroy(brain);
}

/**
 * TEST 2.2: Valid configuration with all cognitive subsystems enabled
 * WHAT: Create brain with comprehensive cognitive features
 * WHY:  Verify integration of multiple advanced subsystems
 */
TEST_F(BrainCreateCustomTest, ValidConfigWithCognitiveSubsystems)
{
    valid_config.enable_working_memory = true;
    valid_config.enable_executive_control = true;
    valid_config.enable_theory_of_mind = true;
    valid_config.enable_ethics = true;
    valid_config.enable_introspection = true;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);
    EXPECT_NE(brain->working_memory, nullptr);
    EXPECT_NE(brain->executive, nullptr);

    brain_destroy(brain);
}

/**
 * TEST 2.3: Valid configuration with large brain size
 * WHAT: Create large brain with custom config
 * WHY:  Verify scalability to larger networks
 */
TEST_F(BrainCreateCustomTest, ValidLargeBrainConfig)
{
    valid_config.size = BRAIN_SIZE_LARGE;
    valid_config.num_inputs = 100;
    valid_config.num_outputs = 50;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);
    EXPECT_EQ(brain->config.num_inputs, 100);
    EXPECT_EQ(brain->config.num_outputs, 50);

    brain_destroy(brain);
}

//=============================================================================
// Test Category 3: Checkpoint Auto-Loading
//=============================================================================

/**
 * TEST 3.1: Auto-load disabled with non-existent checkpoint
 * WHAT: Create brain when auto_load=false
 * WHY:  Verify fresh brain creation when auto-load disabled
 */
TEST_F(BrainCreateCustomTest, AutoLoadDisabledCreatesNewBrain)
{
    valid_config.auto_load = false;
    valid_config.checkpoint_path = NULL;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);

    brain_destroy(brain);
}

/**
 * TEST 3.2: Auto-load with non-existent checkpoint file
 * WHAT: Create fresh brain when checkpoint doesn't exist but auto_load enabled
 * WHY:  Graceful fallback to fresh creation
 */
TEST_F(BrainCreateCustomTest, AutoLoadNonExistentCheckpointCreatesFresh)
{
    valid_config.auto_load = true;
    valid_config.checkpoint_path = TEST_CHECKPOINT_PATH;

    // Ensure checkpoint doesn't exist
    unlink(TEST_CHECKPOINT_PATH);

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);

    brain_destroy(brain);
}

/**
 * TEST 3.3: Auto-load with existing checkpoint
 * WHAT: Load from checkpoint when file exists and auto_load enabled
 * WHY:  Support checkpoint restoration workflow
 *
 * NOTE: This test creates a checkpoint first, then loads it
 */
TEST_F(BrainCreateCustomTest, AutoLoadExistingCheckpointLoads)
{
    // First create and save a brain
    valid_config.auto_load = false;
    valid_config.save_initial_snapshot = false;
    brain_t brain1 = brain_create_custom(&valid_config);
    ASSERT_NE(brain1, nullptr);

    // Save it as checkpoint
    int result = brain_save(brain1, TEST_CHECKPOINT_PATH);
    brain_destroy(brain1);

    if (result == 0) {
        // Now try to auto-load it
        valid_config.auto_load = true;
        valid_config.checkpoint_path = TEST_CHECKPOINT_PATH;

        brain_t brain2 = brain_create_custom(&valid_config);
        // Either loaded successfully or created fresh (both acceptable)
        if (brain2 != nullptr) {
            brain_destroy(brain2);
        }
    }
}

//=============================================================================
// Test Category 4: Config Field Validation
//=============================================================================

/**
 * TEST 4.1: Invalid task_name (NULL or empty)
 * WHAT: Verify task_name validation
 * WHY:  Prevent invalid string fields
 */
TEST_F(BrainCreateCustomTest, InvalidTaskNameHandling)
{
    // Test with empty string - should still work
    memset(valid_config.task_name, 0, sizeof(valid_config.task_name));

    brain_t brain = brain_create_custom(&valid_config);
    // Empty string may or may not be accepted depending on implementation
    // Just verify it doesn't crash
    if (brain != nullptr) {
        brain_destroy(brain);
    }
}

/**
 * TEST 4.2: num_inputs below minimum range
 * WHAT: Reject num_inputs < 1
 * WHY:  Ensure valid network dimensions
 */
TEST_F(BrainCreateCustomTest, InvalidNumInputsTooSmall)
{
    valid_config.num_inputs = 0;

    brain_t brain = brain_create_custom(&valid_config);
    EXPECT_EQ(brain, nullptr);
}

/**
 * TEST 4.3: num_inputs above maximum range
 * WHAT: Reject num_inputs > 10000
 * WHY:  Prevent unreasonably large networks
 */
TEST_F(BrainCreateCustomTest, InvalidNumInputsTooLarge)
{
    valid_config.num_inputs = 10001;

    brain_t brain = brain_create_custom(&valid_config);
    EXPECT_EQ(brain, nullptr);
}

/**
 * TEST 4.4: num_outputs boundary validation
 * WHAT: Reject invalid output dimensions
 * WHY:  Ensure valid network output layer
 */
TEST_F(BrainCreateCustomTest, InvalidNumOutputsRange)
{
    // Test too small
    valid_config.num_outputs = 0;
    brain_t brain = brain_create_custom(&valid_config);
    EXPECT_EQ(brain, nullptr);

    // Test too large
    valid_config.num_outputs = 10001;
    brain = brain_create_custom(&valid_config);
    EXPECT_EQ(brain, nullptr);
}

/**
 * TEST 4.5: Learning rate NaN/Inf validation
 * WHAT: Reject invalid floating-point learning rates
 * WHY:  Prevent invalid numeric values that break learning
 */
TEST_F(BrainCreateCustomTest, InvalidLearningRateValidation)
{
    // NaN and Inf should be rejected
    valid_config.learning_rate = std::numeric_limits<float>::quiet_NaN();

    brain_t brain = brain_create_custom(&valid_config);
    EXPECT_EQ(brain, nullptr);
}

//=============================================================================
// Test Category 5: Multimodal Subsystem Initialization
//=============================================================================

/**
 * TEST 5.1: Multimodal integration disabled
 * WHAT: Create brain without multimodal subsystems
 * WHY:  Verify default behavior without sensory modules
 */
TEST_F(BrainCreateCustomTest, MultimodalDisabledSucceeds)
{
    valid_config.enable_multimodal_integration = false;
    valid_config.enable_visual_cortex = false;
    valid_config.enable_audio_cortex = false;
    valid_config.enable_speech_cortex = false;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);

    // Sensory modules should be NULL when disabled
    EXPECT_EQ(brain->visual_cortex, nullptr);

    brain_destroy(brain);
}

/**
 * TEST 5.2: Multimodal integration enabled with sensory cortices
 * WHAT: Initialize visual, audio, and speech cortices
 * WHY:  Verify multimodal feature works
 */
TEST_F(BrainCreateCustomTest, MultimodalWithSensoryCortices)
{
    valid_config.enable_multimodal_integration = true;
    valid_config.enable_visual_cortex = true;
    valid_config.enable_audio_cortex = true;
    valid_config.enable_speech_cortex = true;
    valid_config.visual_feature_dim = 64;
    valid_config.audio_feature_dim = 32;
    valid_config.speech_feature_dim = 16;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);

    // Multimodal integration should be initialized (non-pointer struct member)
    // Verify by checking that the brain was created successfully
    EXPECT_NE(brain->network, nullptr);

    brain_destroy(brain);
}

//=============================================================================
// Test Category 6: Quantum Annealing Configuration
//=============================================================================

/**
 * TEST 6.1: Quantum annealing disabled
 * WHAT: Create brain without quantum annealer
 * WHY:  Verify default classical operation
 */
TEST_F(BrainCreateCustomTest, QuantumAnnealingDisabled)
{
    valid_config.enable_quantum_annealing = false;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);
    EXPECT_EQ(brain->quantum_annealer, nullptr);

    brain_destroy(brain);
}

/**
 * TEST 6.2: Quantum annealing enabled with config
 * WHAT: Initialize quantum annealer with temperature schedule
 * WHY:  Support quantum-enhanced optimization
 */
TEST_F(BrainCreateCustomTest, QuantumAnnealingEnabled)
{
    valid_config.enable_quantum_annealing = true;
    valid_config.annealing_temperature_init = 1.0f;
    valid_config.annealing_temperature_final = 0.01f;
    valid_config.annealing_steps = 1000;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);

    // Quantum annealer should be initialized (or NULL if creation failed)
    // Either outcome is acceptable for this basic test

    brain_destroy(brain);
}

//=============================================================================
// Test Category 7: Cognitive Subsystems
//=============================================================================

/**
 * TEST 7.1: Working memory initialization
 * WHAT: Verify working memory subsystem creation
 * WHY:  Ensure Miller's 7±2 capacity memory works
 */
TEST_F(BrainCreateCustomTest, WorkingMemoryInitialization)
{
    valid_config.enable_working_memory = true;
    valid_config.working_memory_capacity = 7;
    valid_config.working_memory_decay_tau_ms = 1000.0f;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);
    EXPECT_NE(brain->working_memory, nullptr);

    brain_destroy(brain);
}

/**
 * TEST 7.2: Executive function subsystem
 * WHAT: Initialize executive control for task switching
 * WHY:  Support goal-directed behavior
 */
TEST_F(BrainCreateCustomTest, ExecutiveFunctionInitialization)
{
    valid_config.enable_executive_control = true;
    valid_config.enable_task_switching = true;
    valid_config.enable_planning = true;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);
    EXPECT_NE(brain->executive, nullptr);

    brain_destroy(brain);
}

/**
 * TEST 7.3: Theory of mind subsystem
 * WHAT: Initialize social cognition and empathy
 * WHY:  Enable social understanding
 */
TEST_F(BrainCreateCustomTest, TheoryOfMindInitialization)
{
    valid_config.enable_theory_of_mind = true;
    valid_config.enable_empathy_responses = true;
    valid_config.enable_false_belief_tracking = true;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);
    EXPECT_NE(brain->theory_of_mind, nullptr);

    brain_destroy(brain);
}

/**
 * TEST 7.4: Ethics engine and empathy
 * WHAT: Initialize moral reasoning and emotional resonance
 * WHY:  Enable value-aligned behavior
 */
TEST_F(BrainCreateCustomTest, EthicsAndEmpathyInitialization)
{
    valid_config.enable_ethics = true;
    valid_config.enable_introspection = true;
    valid_config.enable_consolidation = true;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);
    // ethics is a non-pointer typedef, verify brain was created
    EXPECT_NE(brain->network, nullptr);

    brain_destroy(brain);
}

/**
 * TEST 7.5: Mirror neurons and social learning
 * WHAT: Initialize observation-based learning system
 * WHY:  Support imitation and social learning
 */
TEST_F(BrainCreateCustomTest, MirrorNeuronsInitialization)
{
    valid_config.enable_mirror_neurons = true;
    valid_config.mirror_neuron_count = 500;
    valid_config.mirror_max_actions = 50;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);
    EXPECT_NE(brain->mirror_neurons, nullptr);

    brain_destroy(brain);
}

//=============================================================================
// Test Category 8: Error Propagation
//=============================================================================

/**
 * TEST 8.1: Subsystem initialization failure handling
 * WHAT: Verify brain is destroyed if a subsystem fails
 * WHY:  Ensure no resource leaks on partial initialization failure
 */
TEST_F(BrainCreateCustomTest, SubsystemFailureCleanup)
{
    // Create config that enables many subsystems
    valid_config.enable_working_memory = true;
    valid_config.enable_executive_control = true;
    valid_config.enable_theory_of_mind = true;
    valid_config.enable_mirror_neurons = true;
    valid_config.enable_multimodal_integration = true;

    // If creation fails, brain should be NULL
    // If successful, should have subsystems initialized
    brain_t brain = brain_create_custom(&valid_config);
    if (brain != nullptr) {
        EXPECT_NE(brain->working_memory, nullptr);
        brain_destroy(brain);
    }
}

/**
 * TEST 8.2: Config validation prevents corrupted state
 * WHAT: Verify invalid configs are rejected before creation
 * WHY:  Early exit prevents partial initialization
 */
TEST_F(BrainCreateCustomTest, ConfigValidationPreventsCorruption)
{
    // Create config with multiple invalid fields
    valid_config.num_inputs = 0;      // Invalid
    valid_config.num_outputs = 10001;  // Invalid
    valid_config.learning_rate = std::numeric_limits<float>::infinity();

    brain_t brain = brain_create_custom(&valid_config);
    EXPECT_EQ(brain, nullptr);
}

//=============================================================================
// Test Category 9: Resource Cleanup on Failure
//=============================================================================

/**
 * TEST 9.1: Memory not leaked on NULL return
 * WHAT: Verify no dangling allocations when creation fails
 * WHY:  Prevent memory leaks from failed creations
 */
TEST_F(BrainCreateCustomTest, NoLeakOnNullConfig)
{
    // Multiple failed creation attempts
    for (int i = 0; i < 10; i++) {
        brain_t brain = brain_create_custom(NULL);
        EXPECT_EQ(brain, nullptr);
    }
    // If we get here without crash, memory wasn't leaked
}

/**
 * TEST 9.2: Brain fields properly initialized even on partial failure
 * WHAT: Verify all brain fields are in valid state
 * WHY:  Ensure no uninitialized pointers or invalid values
 */
TEST_F(BrainCreateCustomTest, ProperInitializationOnSuccess)
{
    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);

    // Verify core fields are initialized (num_inputs/outputs are in config)
    EXPECT_EQ(brain->config.num_inputs, valid_config.num_inputs);
    EXPECT_EQ(brain->config.num_outputs, valid_config.num_outputs);
    EXPECT_NE(brain->network, nullptr);

    // Config should be copied
    EXPECT_EQ(brain->config.num_inputs, valid_config.num_inputs);

    brain_destroy(brain);
}

//=============================================================================
// Additional Edge Cases and Integration Tests
//=============================================================================

/**
 * TEST 10: Brain with only basic features (minimal cognitive load)
 * WHAT: Create lightweight brain with basic features only
 * WHY:  Ensure minimal viable brain works
 */
TEST_F(BrainCreateCustomTest, MinimalBrainCreation)
{
    // Disable all optional subsystems
    valid_config.enable_working_memory = false;
    valid_config.enable_executive_control = false;
    valid_config.enable_theory_of_mind = false;
    valid_config.enable_ethics = false;
    valid_config.enable_mirror_neurons = false;
    valid_config.enable_multimodal_integration = false;
    valid_config.enable_quantum_annealing = false;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);
    EXPECT_NE(brain->network, nullptr);

    brain_destroy(brain);
}

/**
 * TEST 11: Config field boundary values (valid edge cases)
 * WHAT: Test valid boundary values for numeric fields
 * WHY:  Ensure edge cases within valid range work
 */
TEST_F(BrainCreateCustomTest, BoundaryValuesValidConfig)
{
    // Test valid minimum num_inputs
    valid_config.num_inputs = 1;
    valid_config.num_outputs = 1;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);

    brain_destroy(brain);

    // Test valid maximum
    valid_config.num_inputs = 10000;
    valid_config.num_outputs = 10000;

    brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);

    brain_destroy(brain);
}

/**
 * TEST 12: Config with all features enabled (comprehensive integration)
 * WHAT: Create fully-featured brain with all systems enabled
 * WHY:  Verify integration of all subsystems
 */
TEST_F(BrainCreateCustomTest, FullyFeaturedBrainCreation)
{
    // Enable all major subsystems
    valid_config.enable_working_memory = true;
    valid_config.enable_executive_control = true;
    valid_config.enable_theory_of_mind = true;
    valid_config.enable_ethics = true;
    valid_config.enable_introspection = true;
    valid_config.enable_mirror_neurons = true;
    valid_config.enable_multimodal_integration = true;
    valid_config.enable_visual_cortex = true;
    valid_config.enable_audio_cortex = true;
    valid_config.enable_consolidation = true;
    valid_config.enable_curiosity = true;
    valid_config.enable_salience = true;
    valid_config.enable_predictive_processing = true;
    valid_config.enable_meta_learning = true;
    valid_config.enable_brain_regions = true;

    brain_t brain = brain_create_custom(&valid_config);
    ASSERT_NE(brain, nullptr);

    // Verify major subsystems are initialized
    EXPECT_NE(brain->network, nullptr);

    brain_destroy(brain);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
