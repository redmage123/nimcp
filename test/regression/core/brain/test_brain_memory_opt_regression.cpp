//=============================================================================
// test_brain_memory_opt_regression.cpp - Regression for Brain Memory Opts
//=============================================================================
// WHAT: Regression tests for Opt 2+3+6+8 brain config changes
// WHY:  Ensure optimized brain creation doesn't break existing functionality
// HOW:  Create brains with optimized configs, verify they still operate correctly

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_tier_optimization.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BrainMemOptRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }
};

//=============================================================================
// Opt 2: Tier-Based Enable - Brain Still Functions After Disabling Subsystems
//=============================================================================

TEST_F(BrainMemOptRegressionTest, DisabledSubsystems_BrainStillCreates) {
    brain_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.size = BRAIN_SIZE_MICRO;
    cfg.task = BRAIN_TASK_CLASSIFICATION;
    strncpy(cfg.task_name, "reg_disabled", sizeof(cfg.task_name) - 1);
    cfg.num_inputs = 4;
    cfg.num_outputs = 4;

    // Disable heavy subsystems (simulating CONSTRAINED tier)
    cfg.enable_visual_cortex = false;
    cfg.enable_audio_cortex = false;
    cfg.enable_speech_cortex = false;
    cfg.enable_working_memory = false;
    cfg.enable_theory_of_mind = false;
    cfg.enable_global_workspace = false;

    brain_t brain = brain_create_custom(&cfg);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}

TEST_F(BrainMemOptRegressionTest, DisabledSubsystems_DecideStillWorks) {
    brain_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.size = BRAIN_SIZE_MICRO;
    cfg.task = BRAIN_TASK_CLASSIFICATION;
    strncpy(cfg.task_name, "reg_decide", sizeof(cfg.task_name) - 1);
    cfg.num_inputs = 4;
    cfg.num_outputs = 4;
    cfg.enable_visual_cortex = false;
    cfg.enable_audio_cortex = false;

    brain_t brain = brain_create_custom(&cfg);
    ASSERT_NE(brain, nullptr);

    float features[] = {0.1f, 0.2f, 0.3f, 0.4f};
    brain_decision_t* dec = brain_decide(brain, features, 4);
    if (dec) {
        brain_free_decision(dec);
    }

    brain_destroy(brain);
}

//=============================================================================
// Opt 3: Lazy Init - Brain Functions With Lazy Subsystems
//=============================================================================

TEST_F(BrainMemOptRegressionTest, LazyInit_BrainStillCreates) {
    brain_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.size = BRAIN_SIZE_MICRO;
    cfg.task = BRAIN_TASK_CLASSIFICATION;
    strncpy(cfg.task_name, "reg_lazy", sizeof(cfg.task_name) - 1);
    cfg.num_inputs = 4;
    cfg.num_outputs = 4;

    cfg.lazy_init_mode = true;
    cfg.lazy_dendrite_init = true;
    cfg.lazy_axon_init = true;
    cfg.lazy_visual_init = true;
    cfg.lazy_audio_init = true;
    cfg.lazy_speech_init = true;
    cfg.lazy_working_memory_init = true;
    cfg.lazy_theory_of_mind_init = true;
    cfg.lazy_global_workspace_init = true;
    cfg.lazy_ethics_init = true;
    cfg.lazy_mirror_neurons_init = true;

    brain_t brain = brain_create_custom(&cfg);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}

//=============================================================================
// Opt 6: Default Lazy on FULL Tier
//=============================================================================

TEST_F(BrainMemOptRegressionTest, DefaultLazy_ExtendedSubsystems) {
    brain_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.size = BRAIN_SIZE_MICRO;
    cfg.task = BRAIN_TASK_CLASSIFICATION;
    strncpy(cfg.task_name, "reg_opt6", sizeof(cfg.task_name) - 1);
    cfg.num_inputs = 4;
    cfg.num_outputs = 4;
    cfg.enable_introspection = true;
    cfg.enable_pink_noise = true;
    cfg.lazy_executive_init = true;
    cfg.lazy_consolidation_init = true;
    cfg.lazy_meta_learning_init = true;
    cfg.lazy_neuromod_init = true;
    cfg.lazy_glial_init = true;

    brain_t brain = brain_create_custom(&cfg);
    ASSERT_NE(brain, nullptr);

    float features[] = {0.1f, 0.2f, 0.3f, 0.4f};
    brain_decision_t* dec = brain_decide(brain, features, 4);
    if (dec) {
        brain_free_decision(dec);
    }

    brain_destroy(brain);
}

//=============================================================================
// Opt 8: Tier Scaling Constants Valid
//=============================================================================

TEST_F(BrainMemOptRegressionTest, TierScaling_HelperFunctionsWork) {
    size_t budget = nimcp_tier_memory_budget_bytes();
    EXPECT_GT(budget, 0u);

    uint32_t threads = nimcp_tier_thread_count();
    EXPECT_GT(threads, 0u);

    size_t scaled = nimcp_tier_scale_size(1024);
    EXPECT_GT(scaled, 0u);
    EXPECT_LE(scaled, 1024u);

    uint32_t count = nimcp_tier_scale_count(256);
    EXPECT_GT(count, 0u);
    EXPECT_LE(count, 256u);
}

TEST_F(BrainMemOptRegressionTest, JepaConstants_WithinExpectedRanges) {
    EXPECT_GE(NIMCP_JEPA_LATENT_DIM, 64u);
    EXPECT_LE(NIMCP_JEPA_LATENT_DIM, 512u);
    EXPECT_GE(NIMCP_JEPA_NUM_PATCHES, 4u);
    EXPECT_LE(NIMCP_JEPA_NUM_PATCHES, 49u);
}

//=============================================================================
// Combined: All Opts Together
//=============================================================================

TEST_F(BrainMemOptRegressionTest, AllOpts_CombinedBrainCreation) {
    brain_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.size = BRAIN_SIZE_MICRO;
    cfg.task = BRAIN_TASK_CLASSIFICATION;
    strncpy(cfg.task_name, "reg_all_opts", sizeof(cfg.task_name) - 1);
    cfg.num_inputs = 4;
    cfg.num_outputs = 4;

    // Opt 2: disable non-essential
    cfg.enable_visual_cortex = false;
    cfg.enable_audio_cortex = false;
    cfg.enable_speech_cortex = false;

    // Opt 3+6: lazy everything
    cfg.lazy_init_mode = true;
    cfg.lazy_dendrite_init = true;
    cfg.lazy_axon_init = true;
    cfg.lazy_working_memory_init = true;
    cfg.lazy_executive_init = true;
    cfg.lazy_consolidation_init = true;
    cfg.lazy_meta_learning_init = true;
    cfg.lazy_neuromod_init = true;
    cfg.lazy_glial_init = true;

    brain_t brain = brain_create_custom(&cfg);
    ASSERT_NE(brain, nullptr);

    float features[] = {0.1f, 0.2f, 0.3f, 0.4f};
    brain_decision_t* dec = brain_decide(brain, features, 4);
    if (dec) {
        brain_free_decision(dec);
    }

    brain_destroy(brain);
}

//=============================================================================
// Backward Compat: Default Config Still Works
//=============================================================================

TEST_F(BrainMemOptRegressionTest, DefaultConfig_BackwardCompatible) {
    brain_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.size = BRAIN_SIZE_MICRO;
    cfg.task = BRAIN_TASK_CLASSIFICATION;
    strncpy(cfg.task_name, "reg_compat", sizeof(cfg.task_name) - 1);
    cfg.num_inputs = 4;
    cfg.num_outputs = 4;

    brain_t brain = brain_create_custom(&cfg);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}
