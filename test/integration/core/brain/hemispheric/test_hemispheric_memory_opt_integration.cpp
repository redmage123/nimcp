//=============================================================================
// test_hemispheric_memory_opt_integration.cpp - Integration for Opt 5+7
//=============================================================================
// WHAT: Integration tests for hemispheric module sharing configurations
// WHY:  Verify Opt 5 and 7 work together with the full hemispheric pipeline
// HOW:  Create hemispheric brains with various configurations and verify operation

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "core/brain/hemispheric/nimcp_brain_hemisphere.h"
#include "utils/memory/nimcp_memory.h"
}

//=============================================================================
// Test Fixture - shared brain to avoid OOM
//=============================================================================

class HemisphericMemOptIntegration : public ::testing::Test {
protected:
    static hemispheric_brain_t* s_shared;
    static hemispheric_brain_t* s_noshare;

    static void SetUpTestSuite() {
        nimcp_memory_init();

        // Brain 1: sharing enabled
        {
            hemispheric_brain_config_t cfg = hemispheric_brain_default_config();
            cfg.size = BRAIN_SIZE_MICRO;
            cfg.task = BRAIN_TASK_CLASSIFICATION;
            cfg.task_name = "shared_int";
            cfg.num_inputs = 4;
            cfg.num_outputs = 4;
            cfg.enable_shared_thalamus = true;
            cfg.enable_shared_immune = true;
            s_shared = hemispheric_brain_create(&cfg);
        }

        // Brain 2: sharing disabled
        {
            hemispheric_brain_config_t cfg = hemispheric_brain_default_config();
            cfg.size = BRAIN_SIZE_MICRO;
            cfg.task = BRAIN_TASK_CLASSIFICATION;
            cfg.task_name = "noshare_int";
            cfg.num_inputs = 4;
            cfg.num_outputs = 4;
            cfg.enable_shared_thalamus = false;
            cfg.enable_shared_immune = false;
            s_noshare = hemispheric_brain_create(&cfg);
        }
    }

    static void TearDownTestSuite() {
        if (s_shared) {
            hemispheric_brain_destroy(s_shared);
            s_shared = nullptr;
        }
        if (s_noshare) {
            hemispheric_brain_destroy(s_noshare);
            s_noshare = nullptr;
        }
    }
};

hemispheric_brain_t* HemisphericMemOptIntegration::s_shared = nullptr;
hemispheric_brain_t* HemisphericMemOptIntegration::s_noshare = nullptr;

//=============================================================================
// Lifecycle
//=============================================================================

TEST_F(HemisphericMemOptIntegration, Shared_CreationSucceeds) {
    ASSERT_NE(s_shared, nullptr) << "Shared brain creation failed (OOM?)";
    EXPECT_TRUE(hemispheric_brain_is_active(s_shared));
}

TEST_F(HemisphericMemOptIntegration, NoShare_CreationSucceeds) {
    ASSERT_NE(s_noshare, nullptr) << "NoShare brain creation failed (OOM?)";
    EXPECT_TRUE(hemispheric_brain_is_active(s_noshare));
}

//=============================================================================
// Processing Works With Both Configurations
//=============================================================================

TEST_F(HemisphericMemOptIntegration, Shared_UpdateSucceeds) {
    if (!s_shared) GTEST_SKIP();
    int rc = hemispheric_brain_update(s_shared, 0.01f);
    EXPECT_EQ(rc, 0);
}

TEST_F(HemisphericMemOptIntegration, Shared_InferSucceeds) {
    if (!s_shared) GTEST_SKIP();
    float input[] = {0.1f, 0.2f, 0.3f, 0.4f};
    float output[4] = {0};
    int rc = hemispheric_brain_infer(s_shared, input, 4, output, 4);
    EXPECT_EQ(rc, 0);
}

TEST_F(HemisphericMemOptIntegration, NoShare_UpdateSucceeds) {
    if (!s_noshare) GTEST_SKIP();
    int rc = hemispheric_brain_update(s_noshare, 0.01f);
    EXPECT_EQ(rc, 0);
}

TEST_F(HemisphericMemOptIntegration, NoShare_InferSucceeds) {
    if (!s_noshare) GTEST_SKIP();
    float input[] = {0.1f, 0.2f, 0.3f, 0.4f};
    float output[4] = {0};
    int rc = hemispheric_brain_infer(s_noshare, input, 4, output, 4);
    EXPECT_EQ(rc, 0);
}

//=============================================================================
// Left Hemisphere Accessible
//=============================================================================

TEST_F(HemisphericMemOptIntegration, Shared_LeftHemisphereAccessible) {
    if (!s_shared) GTEST_SKIP();
    brain_hemisphere_t* left = hemispheric_brain_get_left(s_shared);
    EXPECT_NE(left, nullptr);
}

TEST_F(HemisphericMemOptIntegration, NoShare_LeftHemisphereAccessible) {
    if (!s_noshare) GTEST_SKIP();
    brain_hemisphere_t* left = hemispheric_brain_get_left(s_noshare);
    EXPECT_NE(left, nullptr);
}
