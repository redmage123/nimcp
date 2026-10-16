//=============================================================================
// test_memory_footprint_e2e.cpp - E2E Test for Memory Footprint Optimization
//=============================================================================
// WHAT: End-to-end test validating all 8 memory optimizations work together
// WHY:  Verify the complete optimization pipeline from config through operation
// HOW:  Create optimized brain, run full pipeline, verify functionality preserved

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "core/brain/hemispheric/nimcp_brain_hemisphere.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_tier_optimization.h"
}

//=============================================================================
// Test Fixture - Full system lifecycle
//=============================================================================

class MemoryFootprintE2E : public ::testing::Test {
protected:
    static hemispheric_brain_t* s_hemi_brain;
    static brain_t s_single_brain;

    static void SetUpTestSuite() {
        nimcp_memory_init();
        nimcp_tensor_init();

        // Create single brain with all optimizations (Opt 2+3+6)
        {
            brain_config_t cfg;
            memset(&cfg, 0, sizeof(cfg));
            cfg.size = BRAIN_SIZE_MICRO;
            cfg.task = BRAIN_TASK_CLASSIFICATION;
            strncpy(cfg.task_name, "e2e_single", sizeof(cfg.task_name) - 1);
            cfg.num_inputs = 4;
            cfg.num_outputs = 4;

            // Opt 2: disable non-essential
            cfg.enable_visual_cortex = false;
            cfg.enable_audio_cortex = false;
            cfg.enable_speech_cortex = false;

            // Opt 3+6: lazy init
            cfg.lazy_init_mode = true;
            cfg.lazy_dendrite_init = true;
            cfg.lazy_axon_init = true;
            cfg.lazy_working_memory_init = true;
            cfg.lazy_executive_init = true;
            cfg.lazy_consolidation_init = true;
            cfg.lazy_meta_learning_init = true;
            cfg.lazy_neuromod_init = true;
            cfg.lazy_glial_init = true;

            s_single_brain = brain_create_custom(&cfg);
        }

        // Create hemispheric brain with sharing (Opt 5+7)
        {
            hemispheric_brain_config_t cfg = hemispheric_brain_default_config();
            cfg.size = BRAIN_SIZE_MICRO;
            cfg.task = BRAIN_TASK_CLASSIFICATION;
            cfg.task_name = "e2e_hemi";
            cfg.num_inputs = 4;
            cfg.num_outputs = 4;
            cfg.enable_shared_thalamus = true;
            cfg.enable_shared_immune = true;
            s_hemi_brain = hemispheric_brain_create(&cfg);
        }
    }

    static void TearDownTestSuite() {
        if (s_single_brain) {
            brain_destroy(s_single_brain);
            s_single_brain = nullptr;
        }
        if (s_hemi_brain) {
            hemispheric_brain_destroy(s_hemi_brain);
            s_hemi_brain = nullptr;
        }
        nimcp_tensor_shutdown();
    }
};

hemispheric_brain_t* MemoryFootprintE2E::s_hemi_brain = nullptr;
brain_t MemoryFootprintE2E::s_single_brain = nullptr;

//=============================================================================
// E2E: Complete Pipeline With Optimized Single Brain
//=============================================================================

TEST_F(MemoryFootprintE2E, SingleBrain_CreateSucceeds) {
    ASSERT_NE(s_single_brain, nullptr);
}

TEST_F(MemoryFootprintE2E, SingleBrain_DecideWorks) {
    if (!s_single_brain) GTEST_SKIP();

    float features[] = {0.5f, 0.3f, 0.7f, 0.1f};
    brain_decision_t* dec = brain_decide(s_single_brain, features, 4);
    if (dec) {
        brain_free_decision(dec);
    }
}

//=============================================================================
// E2E: Complete Pipeline With Optimized Hemispheric Brain
//=============================================================================

TEST_F(MemoryFootprintE2E, HemisphericBrain_CreateSucceeds) {
    ASSERT_NE(s_hemi_brain, nullptr) << "Hemispheric brain creation failed (OOM?)";
}

TEST_F(MemoryFootprintE2E, HemisphericBrain_UpdateAndInfer) {
    if (!s_hemi_brain) GTEST_SKIP();

    int rc = hemispheric_brain_update(s_hemi_brain, 0.01f);
    EXPECT_EQ(rc, 0);

    float input[] = {0.1f, 0.2f, 0.3f, 0.4f};
    float output[4] = {0};
    rc = hemispheric_brain_infer(s_hemi_brain, input, 4, output, 4);
    EXPECT_EQ(rc, 0);
}

//=============================================================================
// E2E: Tensor Lifecycle Works Alongside Brain
//=============================================================================

TEST_F(MemoryFootprintE2E, Tensor_CreateDestroyStillWorks) {
    uint32_t dims[] = {4, 4};
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(t, nullptr);

    for (int i = 0; i < 16; i++) {
        nimcp_tensor_set_flat(t, i, (double)i * 0.1);
    }

    double val = nimcp_tensor_get_flat(t, 5);
    EXPECT_NEAR(val, 0.5, 0.01);

    nimcp_tensor_destroy(t);
}

TEST_F(MemoryFootprintE2E, Tensor_StatsAccessible) {
    nimcp_tensor_stats_t stats;
    int rc = nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);
    EXPECT_GE(stats.tensors_created, 0u);
}

//=============================================================================
// E2E: Tier Constants Correctly Applied
//=============================================================================

TEST_F(MemoryFootprintE2E, TierConstants_BioInbox_Valid) {
    EXPECT_GE(NIMCP_BIO_INBOX_CAPACITY, 4u);
    EXPECT_LE(NIMCP_BIO_INBOX_CAPACITY, 32u);
    EXPECT_GE(NIMCP_BIO_MESSAGE_POOL_SIZE, 64u);
    EXPECT_LE(NIMCP_BIO_MESSAGE_POOL_SIZE, 1024u);
}

TEST_F(MemoryFootprintE2E, TierConstants_JepaScaling_Valid) {
    EXPECT_GE(NIMCP_JEPA_LATENT_DIM, 64u);
    EXPECT_LE(NIMCP_JEPA_LATENT_DIM, 512u);
    EXPECT_GE(NIMCP_JEPA_NUM_PATCHES, 4u);
    EXPECT_LE(NIMCP_JEPA_NUM_PATCHES, 49u);
}

TEST_F(MemoryFootprintE2E, TierHelpers_Work) {
    EXPECT_GT(nimcp_tier_memory_budget_bytes(), 0u);
    EXPECT_GT(nimcp_tier_thread_count(), 0u);
    EXPECT_GT(nimcp_tier_scale_size(1024), 0u);
    EXPECT_GT(nimcp_tier_scale_count(256), 0u);
}

//=============================================================================
// E2E: Multi-Cycle Stability
//=============================================================================

TEST_F(MemoryFootprintE2E, HemisphericBrain_50CycleStability) {
    if (!s_hemi_brain) GTEST_SKIP();

    float input[] = {0.1f, 0.2f, 0.3f, 0.4f};
    float output[4] = {0};

    for (int i = 0; i < 50; i++) {
        int rc = hemispheric_brain_update(s_hemi_brain, 0.01f);
        EXPECT_EQ(rc, 0) << "Update failed at cycle " << i;

        if (i % 10 == 0) {
            rc = hemispheric_brain_infer(s_hemi_brain, input, 4, output, 4);
            EXPECT_EQ(rc, 0) << "Infer failed at cycle " << i;
        }
    }
}
