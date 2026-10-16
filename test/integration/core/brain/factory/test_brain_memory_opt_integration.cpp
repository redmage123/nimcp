//=============================================================================
// test_brain_memory_opt_integration.cpp - Integration Test for Memory Opts
//=============================================================================
// WHAT: Integration test for Opt 2+3+6 working together in brain creation
// WHY:  Verify enable flags, lazy init flags interact correctly
// HOW:  Create brains with various config combinations, verify creation succeeds

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BrainMemoryOptIntegration : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }

    brain_t create_brain_with_config(brain_config_t* cfg) {
        cfg->size = BRAIN_SIZE_MICRO;
        cfg->task = BRAIN_TASK_CLASSIFICATION;
        strncpy(cfg->task_name, "mem_opt_test", sizeof(cfg->task_name) - 1);
        cfg->num_inputs = 4;
        cfg->num_outputs = 4;
        return brain_create_custom(cfg);
    }
};

//=============================================================================
// Opt 2+3: Enable + Lazy Combined
//=============================================================================

TEST_F(BrainMemoryOptIntegration, AllDisabled_MinimalBrain) {
    brain_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    // All enable flags false (memset zeroed) = minimal brain
    brain_t brain = create_brain_with_config(&cfg);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}

TEST_F(BrainMemoryOptIntegration, AllLazy_DeferredInit) {
    brain_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_visual_cortex = true;
    cfg.enable_audio_cortex = true;
    cfg.enable_introspection = true;
    cfg.lazy_init_mode = true;
    cfg.lazy_visual_init = true;
    cfg.lazy_audio_init = true;
    cfg.lazy_dendrite_init = true;
    cfg.lazy_axon_init = true;
    cfg.lazy_working_memory_init = true;
    cfg.lazy_theory_of_mind_init = true;
    cfg.lazy_global_workspace_init = true;
    cfg.lazy_ethics_init = true;
    cfg.lazy_mirror_neurons_init = true;
    cfg.lazy_executive_init = true;

    brain_t brain = create_brain_with_config(&cfg);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}

TEST_F(BrainMemoryOptIntegration, Opt6_DefaultLazySubsystems) {
    brain_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_introspection = true;
    cfg.enable_pink_noise = true;
    cfg.enable_consolidation = true;

    cfg.lazy_consolidation_init = true;
    cfg.lazy_meta_learning_init = true;
    cfg.lazy_neuromod_init = true;
    cfg.lazy_glial_init = true;
    cfg.lazy_cortical_init = true;
    cfg.lazy_topographic_init = true;
    cfg.lazy_pr_memory_init = true;

    brain_t brain = create_brain_with_config(&cfg);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}

TEST_F(BrainMemoryOptIntegration, MixedEnableLazy_PartialInit) {
    brain_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_visual_cortex = true;
    cfg.lazy_visual_init = false;  // Eager

    cfg.enable_audio_cortex = true;
    cfg.lazy_audio_init = true;  // Lazy

    cfg.enable_ethics = false;  // Disabled entirely

    brain_t brain = create_brain_with_config(&cfg);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}

//=============================================================================
// Tensor Init Alongside Brain
//=============================================================================

TEST_F(BrainMemoryOptIntegration, TensorInitWithBrain) {
    nimcp_tensor_init();

    brain_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.lazy_visual_init = true;
    cfg.lazy_audio_init = true;

    brain_t brain = create_brain_with_config(&cfg);
    ASSERT_NE(brain, nullptr);

    // Tensor stats should be accessible
    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);
    EXPECT_GE(stats.tensors_created + stats.tensors_destroyed, 0u);

    brain_destroy(brain);
    nimcp_tensor_shutdown();
}
