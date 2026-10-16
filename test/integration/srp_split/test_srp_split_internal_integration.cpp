/**
 * @file test_srp_split_internal_integration.cpp
 * @brief Integration tests for SRP-split module internals
 *
 * Tests internal APIs that span multiple part files within SRP-split modules.
 * Validates that the #include-based SRP split preserves correct behavior
 * at the internal function level.
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include "core/brain/nimcp_brain.h"
#include "async/nimcp_bio_router.h"
#include <cstring>

class SRPSplitInternalTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_status_t status = nimcp_init();
        ASSERT_EQ(status, NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

//=============================================================================
// Brain internal module (nimcp_brain.c, 7 parts)
//=============================================================================

TEST_F(SRPSplitInternalTest, BrainInternalCreateAndDestroy) {
    brain_t brain = brain_create(
        "srp_internal_test", BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain, nullptr) << "Internal brain_create failed after SRP split";
    brain_destroy(brain);
}

//=============================================================================
// Bio router module (nimcp_bio_router.c, 6 parts)
//=============================================================================

TEST_F(SRPSplitInternalTest, BioRouterInitialized) {
    bool initialized = bio_router_is_initialized();
    EXPECT_TRUE(initialized) << "Bio router should be initialized after nimcp_init()";
}

//=============================================================================
// Cross-module internal interaction
//=============================================================================

TEST_F(SRPSplitInternalTest, InternalBrainWithPublicAPI) {
    // Create through public API (routes through split API module)
    nimcp_brain_t pub_brain = nimcp_brain_create(
        "srp_pub_internal", NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(pub_brain, nullptr);

    // Create through internal API (routes through split brain module)
    brain_t int_brain = brain_create(
        "srp_int_internal", BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION, 5, 2
    );
    ASSERT_NE(int_brain, nullptr);

    // Both should coexist
    float features[10] = {0.5f};
    char label[NIMCP_MAX_LABEL_SIZE] = {0};
    float confidence = 0.0f;
    nimcp_status_t status = nimcp_brain_predict(
        pub_brain, features, 10, label, &confidence
    );
    EXPECT_EQ(status, NIMCP_SUCCESS);

    brain_destroy(int_brain);
    nimcp_brain_destroy(pub_brain);
}

//=============================================================================
// Multiple internal brains (test split brain module shared state)
//=============================================================================

TEST_F(SRPSplitInternalTest, MultipleInternalBrains) {
    brain_t brain1 = brain_create(
        "srp_multi_1", BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION, 10, 3
    );
    brain_t brain2 = brain_create(
        "srp_multi_2", BRAIN_SIZE_TINY,
        BRAIN_TASK_REGRESSION, 5, 1
    );

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);
    EXPECT_NE(brain1, brain2);

    brain_destroy(brain2);
    brain_destroy(brain1);
}

//=============================================================================
// Sequential create-destroy (test split lifecycle parts)
//=============================================================================

TEST_F(SRPSplitInternalTest, SequentialInternalLifecycles) {
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "srp_seq_%d", i);

        brain_t brain = brain_create(
            name, BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION, 10, 3
        );
        ASSERT_NE(brain, nullptr) << "Create failed on iteration " << i;
        brain_destroy(brain);
    }
}
