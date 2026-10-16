//=============================================================================
// test_brain_tier_config.cpp - Unit Tests for Tier-Based Brain Config (Opt 2,3,6)
//=============================================================================
// WHAT: Tests tier-based subsystem enable/disable flags and lazy init flags
// WHY:  Verify brain_config_t fields exist and can be set
// HOW:  Create configs and verify enable/lazy flag states

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BrainTierConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }
};

//=============================================================================
// Opt 2: Enable Flags Exist in brain_config_t
//=============================================================================

TEST_F(BrainTierConfigTest, EnableFlags_CanBeSet) {
    brain_config_t config;
    memset(&config, 0, sizeof(config));

    config.enable_visual_cortex = true;
    config.enable_audio_cortex = true;
    config.enable_speech_cortex = true;
    config.enable_introspection = true;
    config.enable_ethics = true;
    config.enable_salience = true;
    config.enable_consolidation = true;
    config.enable_curiosity = true;
    config.enable_knowledge = true;
    config.enable_logic = true;

    EXPECT_TRUE(config.enable_visual_cortex);
    EXPECT_TRUE(config.enable_audio_cortex);
    EXPECT_TRUE(config.enable_speech_cortex);
    EXPECT_TRUE(config.enable_introspection);
    EXPECT_TRUE(config.enable_ethics);
    EXPECT_TRUE(config.enable_salience);
    EXPECT_TRUE(config.enable_consolidation);
    EXPECT_TRUE(config.enable_curiosity);
    EXPECT_TRUE(config.enable_knowledge);
    EXPECT_TRUE(config.enable_logic);
}

TEST_F(BrainTierConfigTest, EnableFlags_CanBeDisabled) {
    brain_config_t config;
    memset(&config, 0, sizeof(config));

    config.enable_visual_cortex = false;
    config.enable_audio_cortex = false;
    config.enable_working_memory = false;
    config.enable_theory_of_mind = false;
    config.enable_mirror_neurons = false;
    config.enable_global_workspace = false;
    config.enable_brain_regions = false;
    config.enable_cortical_columns = false;
    config.enable_pink_noise = false;
    config.enable_dendrites = false;

    EXPECT_FALSE(config.enable_visual_cortex);
    EXPECT_FALSE(config.enable_audio_cortex);
    EXPECT_FALSE(config.enable_working_memory);
    EXPECT_FALSE(config.enable_theory_of_mind);
    EXPECT_FALSE(config.enable_mirror_neurons);
    EXPECT_FALSE(config.enable_global_workspace);
    EXPECT_FALSE(config.enable_brain_regions);
    EXPECT_FALSE(config.enable_cortical_columns);
    EXPECT_FALSE(config.enable_pink_noise);
    EXPECT_FALSE(config.enable_dendrites);
}

//=============================================================================
// Opt 3: Lazy Init Flags Exist in brain_config_t
//=============================================================================

TEST_F(BrainTierConfigTest, LazyInitFlags_Exist) {
    brain_config_t config;
    memset(&config, 0, sizeof(config));

    // Master lazy flag
    config.lazy_init_mode = true;
    EXPECT_TRUE(config.lazy_init_mode);

    // Specific subsystem lazy flags
    config.lazy_dendrite_init = true;
    config.lazy_axon_init = true;
    config.lazy_visual_init = true;
    config.lazy_audio_init = true;
    config.lazy_speech_init = true;
    config.lazy_working_memory_init = true;
    config.lazy_theory_of_mind_init = true;
    config.lazy_global_workspace_init = true;
    config.lazy_ethics_init = true;
    config.lazy_mirror_neurons_init = true;

    EXPECT_TRUE(config.lazy_dendrite_init);
    EXPECT_TRUE(config.lazy_axon_init);
    EXPECT_TRUE(config.lazy_visual_init);
    EXPECT_TRUE(config.lazy_audio_init);
    EXPECT_TRUE(config.lazy_speech_init);
    EXPECT_TRUE(config.lazy_working_memory_init);
    EXPECT_TRUE(config.lazy_theory_of_mind_init);
    EXPECT_TRUE(config.lazy_global_workspace_init);
    EXPECT_TRUE(config.lazy_ethics_init);
    EXPECT_TRUE(config.lazy_mirror_neurons_init);
}

TEST_F(BrainTierConfigTest, LazyInitFlags_IndependentFromEnable) {
    brain_config_t config;
    memset(&config, 0, sizeof(config));

    // Can enable subsystem but lazy-init it
    config.enable_visual_cortex = true;
    config.lazy_visual_init = true;
    EXPECT_TRUE(config.enable_visual_cortex);
    EXPECT_TRUE(config.lazy_visual_init);

    // Can disable subsystem and also mark lazy
    config.enable_visual_cortex = false;
    config.lazy_visual_init = false;
    EXPECT_FALSE(config.enable_visual_cortex);
    EXPECT_FALSE(config.lazy_visual_init);
}

//=============================================================================
// Opt 6: Default Lazy for Rarely-Accessed Subsystems
//=============================================================================

TEST_F(BrainTierConfigTest, Opt6_ExtendedLazyFlags) {
    brain_config_t config;
    memset(&config, 0, sizeof(config));

    // Additional lazy flags for less-used subsystems
    config.lazy_executive_init = true;
    config.lazy_consolidation_init = true;
    config.lazy_meta_learning_init = true;
    config.lazy_neuromod_init = true;
    config.lazy_glial_init = true;
    config.lazy_cortical_init = true;
    config.lazy_topographic_init = true;
    config.lazy_pr_memory_init = true;

    EXPECT_TRUE(config.lazy_executive_init);
    EXPECT_TRUE(config.lazy_consolidation_init);
    EXPECT_TRUE(config.lazy_meta_learning_init);
    EXPECT_TRUE(config.lazy_neuromod_init);
    EXPECT_TRUE(config.lazy_glial_init);
    EXPECT_TRUE(config.lazy_cortical_init);
    EXPECT_TRUE(config.lazy_topographic_init);
    EXPECT_TRUE(config.lazy_pr_memory_init);
}

//=============================================================================
// Struct Size Verification
//=============================================================================

TEST_F(BrainTierConfigTest, ConfigStruct_ReasonableSize) {
    EXPECT_LT(sizeof(brain_config_t), 16384u);
    EXPECT_GT(sizeof(brain_config_t), 100u);
}
