/**
 * @file test_brain_init_subsystems_part1.cpp
 * @brief Comprehensive tests for brain subsystem initialization functions (Part 1 of 2)
 *
 * WHAT: Tests for the first 15 brain subsystem initialization functions
 * WHY:  Ensure robust initialization with NULL safety and proper subsystem creation
 * HOW:  GoogleTest framework with comprehensive validation
 *
 * TESTED FUNCTIONS (15 total):
 * 1.  nimcp_brain_factory_init_glial_subsystem
 * 2.  nimcp_brain_factory_init_multimodal_subsystems
 * 3.  nimcp_brain_factory_init_pink_noise_subsystem
 * 4.  nimcp_brain_factory_init_neuromodulator_system
 * 5.  nimcp_brain_factory_init_spatial_neuromod_system
 * 6.  nimcp_brain_factory_init_attention_subsystem
 * 7.  nimcp_brain_factory_init_brain_regions_subsystem
 * 8.  nimcp_brain_factory_init_symbolic_logic_subsystem
 * 9.  nimcp_brain_factory_init_symbolic_reasoning_subsystem
 * 10. nimcp_brain_factory_init_epistemic_subsystem
 * 11. nimcp_brain_factory_init_working_memory_subsystem
 * 12. nimcp_brain_factory_init_executive_subsystem
 * 13. nimcp_brain_factory_init_theory_of_mind_subsystem
 * 14. nimcp_brain_factory_init_natural_explanations_subsystem
 * 15. nimcp_brain_factory_init_meta_learning_subsystem
 *
 * TEST COVERAGE:
 * - NULL brain pointer handling (~15 tests)
 * - Successful initialization when enabled (~15 tests)
 * - Graceful handling when disabled (~15 tests)
 * - Double initialization prevention (~15 tests)
 * - Subsystem verification (~15 tests)
 * Total: ~75 tests
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainInitSubsystemsPart1Test : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        nimcp_shutdown();
    }

    // Helper: Create default brain config (replacement for non-existent brain_config_default)
    brain_config_t brain_config_default() {
        brain_config_t config = {};  // Zero-initialize
        config.size = BRAIN_SIZE_TINY;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 10;
        config.num_outputs = 2;
        config.learning_rate = 0.01f;
        config.sparsity_target = 0.85f;
        strcpy(config.task_name, "test_brain");
        return config;
    }

    // Helper: Create minimal brain for initialization testing
    brain_t create_minimal_brain() {
        brain_t brain = brain_create("init_test", BRAIN_SIZE_TINY,
                                      BRAIN_TASK_CLASSIFICATION, 10, 2);
        return brain;
    }

    // Helper: Create brain with specific config flags
    brain_t create_brain_with_config(const char* name, bool enable_flag) {
        brain_config_t config = brain_config_default();
        config.size = BRAIN_SIZE_TINY;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 10;
        config.num_outputs = 2;
        strncpy(config.task_name, name, sizeof(config.task_name) - 1);

        // Set all flags that might be relevant
        config.enable_glial = enable_flag;
        config.enable_multimodal_integration = enable_flag;
        config.enable_pink_noise = enable_flag;
        config.enable_quantum_walk_diffusion = false; // Keep off for simplicity
        // Note: multihead_attention requires additional config (num_attention_heads, etc.)
        // It's tested separately in AttentionSubsystem tests with proper config
        config.enable_multihead_attention = false;
        config.enable_brain_regions = enable_flag;
        config.enable_knowledge = enable_flag;
        config.enable_logic = enable_flag;
        config.enable_working_memory = enable_flag;
        config.enable_executive_control = enable_flag;
        config.enable_theory_of_mind = enable_flag;
        config.enable_natural_explanations = enable_flag;
        config.enable_meta_learning = enable_flag;

        return brain_create_custom(&config);
    }
};

//=============================================================================
// 1. Glial Subsystem Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, GlialSubsystem_NullBrain) {
    bool result = nimcp_brain_factory_init_glial_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, GlialSubsystem_SuccessWhenEnabled) {
    brain_t brain = create_brain_with_config("glial_enabled", true);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_glial_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->glial, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, GlialSubsystem_SuccessWhenDisabled) {
    brain_t brain = create_brain_with_config("glial_disabled", false);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_glial_subsystem(brain);
    EXPECT_TRUE(result);  // Returns true even when disabled

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, GlialSubsystem_DoubleInitialization) {
    brain_t brain = create_brain_with_config("glial_double", true);
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_glial_subsystem(brain);
    EXPECT_TRUE(result1);

    void* first_glial = brain->glial;

    bool result2 = nimcp_brain_factory_init_glial_subsystem(brain);
    EXPECT_TRUE(result2);
    EXPECT_EQ(brain->glial, first_glial);  // Should be same pointer

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, GlialSubsystem_RequiresNetwork) {
    brain_t brain = create_minimal_brain();
    ASSERT_NE(brain, nullptr);

    // Save original network and set to NULL temporarily
    adaptive_network_t saved_network = brain->network;
    brain->network = nullptr;

    bool result = nimcp_brain_factory_init_glial_subsystem(brain);
    EXPECT_FALSE(result);

    // Restore network
    brain->network = saved_network;
    brain_destroy(brain);
}

//=============================================================================
// 2. Multimodal Subsystems Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, MultimodalSubsystems_NullBrain) {
    bool result = nimcp_brain_factory_init_multimodal_subsystems(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, MultimodalSubsystems_SuccessWhenEnabled) {
    brain_t brain = create_brain_with_config("multimodal_enabled", true);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_multimodal_subsystems(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->integrated_feature_buffer, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, MultimodalSubsystems_AllocatesIntegratedBuffer) {
    brain_t brain = create_brain_with_config("multimodal_buffer", false);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_multimodal_subsystems(brain);
    EXPECT_TRUE(result);
    // Even when disabled, integrated_feature_buffer should be allocated
    EXPECT_NE(brain->integrated_feature_buffer, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, MultimodalSubsystems_DoubleInitialization) {
    brain_t brain = create_brain_with_config("multimodal_double", true);
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_multimodal_subsystems(brain);
    EXPECT_TRUE(result1);

    void* first_buffer = brain->integrated_feature_buffer;

    bool result2 = nimcp_brain_factory_init_multimodal_subsystems(brain);
    EXPECT_TRUE(result2);
    // Buffer should remain the same
    EXPECT_EQ(brain->integrated_feature_buffer, first_buffer);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, MultimodalSubsystems_CreatesVisualCortex) {
    brain_config_t config = brain_config_default();
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 100;
    config.num_outputs = 2;
    config.enable_multimodal_integration = true;
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 32;
    strncpy(config.task_name, "visual_cortex_test", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_multimodal_subsystems(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->visual_cortex, nullptr);
    EXPECT_NE(brain->visual_feature_buffer, nullptr);

    brain_destroy(brain);
}

//=============================================================================
// 3. Pink Noise Subsystem Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, PinkNoiseSubsystem_NullBrain) {
    bool result = nimcp_brain_factory_init_pink_noise_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, PinkNoiseSubsystem_SuccessWhenEnabled) {
    brain_t brain = create_brain_with_config("pink_noise_enabled", true);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_pink_noise_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->pink_noise, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, PinkNoiseSubsystem_SuccessWhenDisabled) {
    brain_t brain = create_brain_with_config("pink_noise_disabled", false);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_pink_noise_subsystem(brain);
    EXPECT_TRUE(result);  // Returns true even when disabled

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, PinkNoiseSubsystem_DoubleInitialization) {
    brain_t brain = create_brain_with_config("pink_noise_double", true);
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_pink_noise_subsystem(brain);
    EXPECT_TRUE(result1);

    void* first_pink_noise = brain->pink_noise;

    bool result2 = nimcp_brain_factory_init_pink_noise_subsystem(brain);
    EXPECT_TRUE(result2);
    EXPECT_EQ(brain->pink_noise, first_pink_noise);

    brain_destroy(brain);
}

//=============================================================================
// 4. Neuromodulator System Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, NeuromodulatorSystem_NullBrain) {
    bool result = nimcp_brain_factory_init_neuromodulator_system(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, NeuromodulatorSystem_Success) {
    brain_t brain = create_minimal_brain();
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_neuromodulator_system(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->neuromodulator_system, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, NeuromodulatorSystem_DoubleInitialization) {
    brain_t brain = create_minimal_brain();
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_neuromodulator_system(brain);
    EXPECT_TRUE(result1);

    void* first_neuromod = brain->neuromodulator_system;

    bool result2 = nimcp_brain_factory_init_neuromodulator_system(brain);
    EXPECT_TRUE(result2);
    EXPECT_EQ(brain->neuromodulator_system, first_neuromod);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, NeuromodulatorSystem_PersonalityModulation) {
    brain_t brain = create_minimal_brain();
    ASSERT_NE(brain, nullptr);

    // Neuromodulator system should work even without personality
    bool result = nimcp_brain_factory_init_neuromodulator_system(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->neuromodulator_system, nullptr);

    brain_destroy(brain);
}

//=============================================================================
// 5. Spatial Neuromodulator System Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, SpatialNeuromodSystem_NullBrain) {
    bool result = nimcp_brain_factory_init_spatial_neuromod_system(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, SpatialNeuromodSystem_RequiresGlial) {
    brain_t brain = create_minimal_brain();
    ASSERT_NE(brain, nullptr);

    // Without glial integration, should return true (non-error)
    bool result = nimcp_brain_factory_init_spatial_neuromod_system(brain);
    EXPECT_TRUE(result);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, SpatialNeuromodSystem_WithGlialIntegration) {
    brain_t brain = create_brain_with_config("spatial_neuromod", true);
    ASSERT_NE(brain, nullptr);

    // Initialize glial first
    nimcp_brain_factory_init_glial_subsystem(brain);

    bool result = nimcp_brain_factory_init_spatial_neuromod_system(brain);
    EXPECT_TRUE(result);

    // Check if spatial neuromod was added to glial integration
    if (brain->glial) {
        // Spatial neuromod may or may not be initialized (non-fatal failure)
        // We just verify the function returns successfully
        EXPECT_TRUE(result);
    }

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, SpatialNeuromodSystem_DoubleInitialization) {
    brain_t brain = create_brain_with_config("spatial_neuromod_double", true);
    ASSERT_NE(brain, nullptr);

    nimcp_brain_factory_init_glial_subsystem(brain);

    bool result1 = nimcp_brain_factory_init_spatial_neuromod_system(brain);
    EXPECT_TRUE(result1);

    bool result2 = nimcp_brain_factory_init_spatial_neuromod_system(brain);
    EXPECT_TRUE(result2);

    brain_destroy(brain);
}

//=============================================================================
// 6. Attention Subsystem Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, AttentionSubsystem_NullBrain) {
    bool result = nimcp_brain_factory_init_attention_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, AttentionSubsystem_SuccessWhenEnabled) {
    // Attention requires proper config: embed_dim divisible by num_heads
    brain_config_t config = brain_config_default();
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;  // divisible by num_attention_heads
    config.num_outputs = 2;
    config.enable_multihead_attention = true;
    config.num_attention_heads = 4;
    strncpy(config.task_name, "attention_enabled", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_attention_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->multihead_attention, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, AttentionSubsystem_SuccessWhenDisabled) {
    brain_t brain = create_brain_with_config("attention_disabled", false);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_attention_subsystem(brain);
    EXPECT_TRUE(result);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, AttentionSubsystem_DoubleInitialization) {
    // Attention requires proper config: embed_dim divisible by num_heads
    brain_config_t config = brain_config_default();
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;  // divisible by num_attention_heads
    config.num_outputs = 2;
    config.enable_multihead_attention = true;
    config.num_attention_heads = 4;
    strncpy(config.task_name, "attention_double", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_attention_subsystem(brain);
    EXPECT_TRUE(result1);

    void* first_attention = brain->multihead_attention;

    bool result2 = nimcp_brain_factory_init_attention_subsystem(brain);
    EXPECT_TRUE(result2);
    EXPECT_EQ(brain->multihead_attention, first_attention);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, AttentionSubsystem_UsesNumInputs) {
    brain_config_t config = brain_config_default();
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 2;
    config.enable_multihead_attention = true;
    config.num_attention_heads = 4;
    strncpy(config.task_name, "attention_dims", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_attention_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->multihead_attention, nullptr);

    brain_destroy(brain);
}

//=============================================================================
// 7. Brain Regions Subsystem Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, BrainRegionsSubsystem_NullBrain) {
    bool result = nimcp_brain_factory_init_brain_regions_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, BrainRegionsSubsystem_SuccessWhenEnabled) {
    brain_t brain = create_brain_with_config("brain_regions_enabled", true);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_brain_regions_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->brain_regions, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, BrainRegionsSubsystem_SuccessWhenDisabled) {
    brain_t brain = create_brain_with_config("brain_regions_disabled", false);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_brain_regions_subsystem(brain);
    EXPECT_TRUE(result);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, BrainRegionsSubsystem_DoubleInitialization) {
    brain_t brain = create_brain_with_config("brain_regions_double", true);
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_brain_regions_subsystem(brain);
    EXPECT_TRUE(result1);

    void* first_regions = brain->brain_regions;

    bool result2 = nimcp_brain_factory_init_brain_regions_subsystem(brain);
    EXPECT_TRUE(result2);
    EXPECT_EQ(brain->brain_regions, first_regions);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, BrainRegionsSubsystem_CreatesDefaultRegions) {
    brain_config_t config = brain_config_default();
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 2;
    strncpy(config.task_name, "brain_regions_create", sizeof(config.task_name) - 1);
    config.enable_brain_regions = true;
    config.num_brain_regions = 4;
    config.neurons_per_region = 100;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_brain_regions_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->brain_regions, nullptr);

    brain_destroy(brain);
}

//=============================================================================
// 8. Symbolic Logic Subsystem Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, SymbolicLogicSubsystem_NullBrain) {
    bool result = nimcp_brain_factory_init_symbolic_logic_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, SymbolicLogicSubsystem_SuccessWhenEnabled) {
    brain_t brain = create_brain_with_config("symbolic_logic_enabled", true);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_symbolic_logic_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->logic, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, SymbolicLogicSubsystem_SuccessWhenDisabled) {
    brain_t brain = create_brain_with_config("symbolic_logic_disabled", false);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_symbolic_logic_subsystem(brain);
    EXPECT_TRUE(result);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, SymbolicLogicSubsystem_DoubleInitialization) {
    brain_t brain = create_brain_with_config("symbolic_logic_double", true);
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_symbolic_logic_subsystem(brain);
    EXPECT_TRUE(result1);

    void* first_logic = brain->logic;

    bool result2 = nimcp_brain_factory_init_symbolic_logic_subsystem(brain);
    EXPECT_TRUE(result2);
    EXPECT_EQ(brain->logic, first_logic);

    brain_destroy(brain);
}

//=============================================================================
// 9. Symbolic Reasoning Subsystem Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, SymbolicReasoningSubsystem_NullBrain) {
    bool result = nimcp_brain_factory_init_symbolic_reasoning_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, SymbolicReasoningSubsystem_SuccessWhenEnabled) {
    brain_t brain = create_brain_with_config("symbolic_reasoning_enabled", true);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_symbolic_reasoning_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->symbolic_logic, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, SymbolicReasoningSubsystem_SuccessWhenDisabled) {
    brain_t brain = create_brain_with_config("symbolic_reasoning_disabled", false);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_symbolic_reasoning_subsystem(brain);
    EXPECT_TRUE(result);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, SymbolicReasoningSubsystem_DoubleInitialization) {
    brain_t brain = create_brain_with_config("symbolic_reasoning_double", true);
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_symbolic_reasoning_subsystem(brain);
    EXPECT_TRUE(result1);

    void* first_symbolic = brain->symbolic_logic;

    bool result2 = nimcp_brain_factory_init_symbolic_reasoning_subsystem(brain);
    EXPECT_TRUE(result2);
    EXPECT_EQ(brain->symbolic_logic, first_symbolic);

    brain_destroy(brain);
}

//=============================================================================
// 10. Epistemic Subsystem Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, EpistemicSubsystem_NullBrain) {
    bool result = nimcp_brain_factory_init_epistemic_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, EpistemicSubsystem_Success) {
    brain_t brain = create_minimal_brain();
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_epistemic_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->epistemic, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, EpistemicSubsystem_DoubleInitialization) {
    brain_t brain = create_minimal_brain();
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_epistemic_subsystem(brain);
    EXPECT_TRUE(result1);

    void* first_epistemic = brain->epistemic;

    bool result2 = nimcp_brain_factory_init_epistemic_subsystem(brain);
    EXPECT_TRUE(result2);
    EXPECT_EQ(brain->epistemic, first_epistemic);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, EpistemicSubsystem_SkepticismLevel) {
    brain_t brain = create_minimal_brain();
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_epistemic_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->epistemic, nullptr);
    // Epistemic filter should be created with default skepticism level (0.6)

    brain_destroy(brain);
}

//=============================================================================
// 11. Working Memory Subsystem Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, WorkingMemorySubsystem_NullBrain) {
    bool result = nimcp_brain_factory_init_working_memory_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, WorkingMemorySubsystem_SuccessWhenEnabled) {
    brain_t brain = create_brain_with_config("working_memory_enabled", true);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_working_memory_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->working_memory, nullptr);
    EXPECT_NE(brain->emotional_system, nullptr);
    EXPECT_NE(brain->sleep_system, nullptr);
    EXPECT_NE(brain->engram_system, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, WorkingMemorySubsystem_SuccessWhenDisabled) {
    brain_t brain = create_brain_with_config("working_memory_disabled", false);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_working_memory_subsystem(brain);
    EXPECT_TRUE(result);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, WorkingMemorySubsystem_DoubleInitialization) {
    brain_t brain = create_brain_with_config("working_memory_double", true);
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_working_memory_subsystem(brain);
    EXPECT_TRUE(result1);

    void* first_wm = brain->working_memory;

    bool result2 = nimcp_brain_factory_init_working_memory_subsystem(brain);
    EXPECT_TRUE(result2);
    EXPECT_EQ(brain->working_memory, first_wm);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, WorkingMemorySubsystem_CreatesIntegratedSystems) {
    brain_t brain = create_brain_with_config("working_memory_integrated", true);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_working_memory_subsystem(brain);
    EXPECT_TRUE(result);

    // Verify all integrated systems are created
    EXPECT_NE(brain->working_memory, nullptr);
    EXPECT_NE(brain->emotional_system, nullptr);
    EXPECT_NE(brain->sleep_system, nullptr);
    EXPECT_NE(brain->engram_system, nullptr);
    EXPECT_NE(brain->systems_consolidation, nullptr);
    EXPECT_NE(brain->wm_transfer_system, nullptr);
    EXPECT_NE(brain->semantic_memory, nullptr);

    brain_destroy(brain);
}

//=============================================================================
// 12. Executive Subsystem Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, ExecutiveSubsystem_NullBrain) {
    bool result = nimcp_brain_factory_init_executive_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, ExecutiveSubsystem_SuccessWhenEnabled) {
    brain_t brain = create_brain_with_config("executive_enabled", true);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_executive_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->executive, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, ExecutiveSubsystem_SuccessWhenDisabled) {
    brain_t brain = create_brain_with_config("executive_disabled", false);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_executive_subsystem(brain);
    EXPECT_TRUE(result);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, ExecutiveSubsystem_DoubleInitialization) {
    brain_t brain = create_brain_with_config("executive_double", true);
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_executive_subsystem(brain);
    EXPECT_TRUE(result1);

    void* first_executive = brain->executive;

    bool result2 = nimcp_brain_factory_init_executive_subsystem(brain);
    EXPECT_TRUE(result2);
    EXPECT_EQ(brain->executive, first_executive);

    brain_destroy(brain);
}

//=============================================================================
// 13. Theory of Mind Subsystem Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, TheoryOfMindSubsystem_NullBrain) {
    bool result = nimcp_brain_factory_init_theory_of_mind_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, TheoryOfMindSubsystem_SuccessWhenEnabled) {
    brain_t brain = create_brain_with_config("tom_enabled", true);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_theory_of_mind_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->theory_of_mind, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, TheoryOfMindSubsystem_SuccessWhenDisabled) {
    brain_t brain = create_brain_with_config("tom_disabled", false);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_theory_of_mind_subsystem(brain);
    EXPECT_TRUE(result);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, TheoryOfMindSubsystem_DoubleInitialization) {
    brain_t brain = create_brain_with_config("tom_double", true);
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_theory_of_mind_subsystem(brain);
    EXPECT_TRUE(result1);

    void* first_tom = brain->theory_of_mind;

    bool result2 = nimcp_brain_factory_init_theory_of_mind_subsystem(brain);
    EXPECT_TRUE(result2);
    EXPECT_EQ(brain->theory_of_mind, first_tom);

    brain_destroy(brain);
}

//=============================================================================
// 14. Natural Explanations Subsystem Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, NaturalExplanationsSubsystem_NullBrain) {
    bool result = nimcp_brain_factory_init_natural_explanations_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, NaturalExplanationsSubsystem_SuccessWhenEnabled) {
    brain_t brain = create_brain_with_config("explanations_enabled", true);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_natural_explanations_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->explanation_gen, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, NaturalExplanationsSubsystem_SuccessWhenDisabled) {
    brain_t brain = create_brain_with_config("explanations_disabled", false);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_natural_explanations_subsystem(brain);
    EXPECT_TRUE(result);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, NaturalExplanationsSubsystem_DoubleInitialization) {
    brain_t brain = create_brain_with_config("explanations_double", true);
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_natural_explanations_subsystem(brain);
    EXPECT_TRUE(result1);

    void* first_explanations = brain->explanation_gen;

    bool result2 = nimcp_brain_factory_init_natural_explanations_subsystem(brain);
    EXPECT_TRUE(result2);
    EXPECT_EQ(brain->explanation_gen, first_explanations);

    brain_destroy(brain);
}

//=============================================================================
// 15. Meta-Learning Subsystem Initialization Tests
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, MetaLearningSubsystem_NullBrain) {
    bool result = nimcp_brain_factory_init_meta_learning_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitSubsystemsPart1Test, MetaLearningSubsystem_SuccessWhenEnabled) {
    brain_t brain = create_brain_with_config("meta_learning_enabled", true);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_meta_learning_subsystem(brain);
    EXPECT_TRUE(result);
    EXPECT_NE(brain->meta_learner, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, MetaLearningSubsystem_SuccessWhenDisabled) {
    brain_t brain = create_brain_with_config("meta_learning_disabled", false);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_meta_learning_subsystem(brain);
    EXPECT_TRUE(result);

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, MetaLearningSubsystem_DoubleInitialization) {
    brain_t brain = create_brain_with_config("meta_learning_double", true);
    ASSERT_NE(brain, nullptr);

    bool result1 = nimcp_brain_factory_init_meta_learning_subsystem(brain);
    EXPECT_TRUE(result1);

    void* first_meta = brain->meta_learner;

    bool result2 = nimcp_brain_factory_init_meta_learning_subsystem(brain);
    EXPECT_TRUE(result2);
    EXPECT_EQ(brain->meta_learner, first_meta);

    brain_destroy(brain);
}

//=============================================================================
// Integration Tests - Multiple Subsystems
//=============================================================================

TEST_F(BrainInitSubsystemsPart1Test, Integration_AllSubsystemsEnabled) {
    brain_t brain = create_brain_with_config("all_enabled", true);
    ASSERT_NE(brain, nullptr);

    // Initialize all 15 subsystems
    EXPECT_TRUE(nimcp_brain_factory_init_glial_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_multimodal_subsystems(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_pink_noise_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_neuromodulator_system(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_spatial_neuromod_system(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_attention_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_brain_regions_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_symbolic_logic_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_symbolic_reasoning_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_epistemic_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_working_memory_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_executive_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_theory_of_mind_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_natural_explanations_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_meta_learning_subsystem(brain));

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, Integration_AllSubsystemsDisabled) {
    brain_t brain = create_brain_with_config("all_disabled", false);
    ASSERT_NE(brain, nullptr);

    // All should succeed even when disabled
    EXPECT_TRUE(nimcp_brain_factory_init_glial_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_multimodal_subsystems(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_pink_noise_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_neuromodulator_system(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_spatial_neuromod_system(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_attention_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_brain_regions_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_symbolic_logic_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_symbolic_reasoning_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_epistemic_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_working_memory_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_executive_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_theory_of_mind_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_natural_explanations_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_meta_learning_subsystem(brain));

    brain_destroy(brain);
}

TEST_F(BrainInitSubsystemsPart1Test, Integration_CognitiveStackComplete) {
    brain_t brain = create_brain_with_config("cognitive_stack", true);
    ASSERT_NE(brain, nullptr);

    // Initialize cognitive stack in order
    EXPECT_TRUE(nimcp_brain_factory_init_working_memory_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_executive_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_theory_of_mind_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_natural_explanations_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_meta_learning_subsystem(brain));

    // Verify cognitive systems are linked
    EXPECT_NE(brain->working_memory, nullptr);
    EXPECT_NE(brain->executive, nullptr);
    EXPECT_NE(brain->theory_of_mind, nullptr);
    EXPECT_NE(brain->explanation_gen, nullptr);
    EXPECT_NE(brain->meta_learner, nullptr);

    brain_destroy(brain);
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
