//=============================================================================
// test_hemispheric_memory_opt_regression.cpp - Regression for Opt 5+7
//=============================================================================
// WHAT: Regression tests for hemispheric module sharing stability
// WHY:  Ensure optimized hemispheric brains don't regress in functionality
// HOW:  Create, update, infer, and destroy hemispheric brains with optimizations

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

class HemisphericMemOptRegression : public ::testing::Test {
protected:
    static hemispheric_brain_t* s_brain;

    static void SetUpTestSuite() {
        nimcp_memory_init();

        hemispheric_brain_config_t cfg = hemispheric_brain_default_config();
        cfg.size = BRAIN_SIZE_MICRO;
        cfg.task = BRAIN_TASK_CLASSIFICATION;
        cfg.task_name = "hemi_reg_test";
        cfg.num_inputs = 4;
        cfg.num_outputs = 4;
        cfg.enable_shared_thalamus = true;
        cfg.enable_shared_immune = true;
        s_brain = hemispheric_brain_create(&cfg);
    }

    static void TearDownTestSuite() {
        if (s_brain) {
            hemispheric_brain_destroy(s_brain);
            s_brain = nullptr;
        }
    }
};

hemispheric_brain_t* HemisphericMemOptRegression::s_brain = nullptr;

//=============================================================================
// Lifecycle Regression
//=============================================================================

TEST_F(HemisphericMemOptRegression, Create_WithSharing) {
    ASSERT_NE(s_brain, nullptr) << "Hemispheric brain with sharing failed (OOM?)";
}

TEST_F(HemisphericMemOptRegression, IsActive_ReturnsTrue) {
    if (!s_brain) GTEST_SKIP();
    EXPECT_TRUE(hemispheric_brain_is_active(s_brain));
}

//=============================================================================
// Processing Regression
//=============================================================================

TEST_F(HemisphericMemOptRegression, Update_Stable_10Cycles) {
    if (!s_brain) GTEST_SKIP();

    for (int i = 0; i < 10; i++) {
        int rc = hemispheric_brain_update(s_brain, 0.01f);
        EXPECT_EQ(rc, 0) << "Update failed at cycle " << i;
    }
}

TEST_F(HemisphericMemOptRegression, Infer_ProducesOutput) {
    if (!s_brain) GTEST_SKIP();

    float input[] = {0.5f, 0.3f, 0.7f, 0.1f};
    float output[4] = {0};

    int rc = hemispheric_brain_infer(s_brain, input, 4, output, 4);
    EXPECT_EQ(rc, 0);
}

TEST_F(HemisphericMemOptRegression, Infer_MultipleCallsStable) {
    if (!s_brain) GTEST_SKIP();

    float input[] = {0.1f, 0.2f, 0.3f, 0.4f};
    float output[4] = {0};

    for (int i = 0; i < 5; i++) {
        int rc = hemispheric_brain_infer(s_brain, input, 4, output, 4);
        EXPECT_EQ(rc, 0) << "Infer failed at call " << i;
    }
}

//=============================================================================
// Bilateral Mode Regression
//=============================================================================

TEST_F(HemisphericMemOptRegression, BilateralMode_ToggleDoesNotCrash) {
    if (!s_brain) GTEST_SKIP();

    int rc1 = hemispheric_brain_set_bilateral_mode(s_brain, true);
    EXPECT_EQ(rc1, 0);
    EXPECT_TRUE(hemispheric_brain_is_bilateral_mode(s_brain));

    int rc2 = hemispheric_brain_set_bilateral_mode(s_brain, false);
    EXPECT_EQ(rc2, 0);
    EXPECT_FALSE(hemispheric_brain_is_bilateral_mode(s_brain));
}

//=============================================================================
// Lateralization Regression
//=============================================================================

TEST_F(HemisphericMemOptRegression, LateralizationShift_WithSharing) {
    if (!s_brain) GTEST_SKIP();

    int rc = hemispheric_brain_apply_lateralization_shift(s_brain, 0.1f);
    EXPECT_EQ(rc, 0);

    rc = hemispheric_brain_apply_lateralization_shift(s_brain, -0.1f);
    EXPECT_EQ(rc, 0);
}
