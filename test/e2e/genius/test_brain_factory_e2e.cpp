/**
 * @file test_brain_factory_e2e.cpp
 * @brief End-to-end tests for genius profiles brain factory workflow
 *
 * @author NIMCP Development Team
 * @date 2026-02-03
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/brain/genius/nimcp_genius_profiles.h"
#include "core/brain/genius/nimcp_genius_types.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BrainFactoryE2ETest : public ::testing::Test {
protected:
    genius_profiles_bridge_t* bridge = nullptr;
    genius_profiles_config_t config;

    void SetUp() override {
        ASSERT_EQ(genius_profiles_config_default(&config), GENIUS_ERROR_SUCCESS);
        config.enable_bio_async = false;
        config.enable_mesh_coordination = false;
        config.enable_training_integration = false;
        config.enable_rcog_integration = false;
        config.enable_ccog_integration = false;
        config.enable_quantum_optimization = false;
        config.enable_kg_wiring = false;

        bridge = genius_profiles_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            genius_profiles_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// 1. COMPLETE BRAIN CREATION WORKFLOW
//=============================================================================

TEST_F(BrainFactoryE2ETest, CompleteMathematicalBrainWorkflow) {
    brain_t brain = genius_brain_create(GENIUS_TYPE_MATHEMATICAL);
    ASSERT_NE(brain, nullptr);

    brain_stats_t stats;
    brain_get_stats(brain, &stats);

    brain_destroy(brain);
}

TEST_F(BrainFactoryE2ETest, CompleteAllTypesWorkflow) {
    genius_type_t types[] = {
        GENIUS_TYPE_MATHEMATICAL,
        GENIUS_TYPE_VISUAL_ARTISTIC,
        GENIUS_TYPE_MUSICAL,
        GENIUS_TYPE_SCIENTIFIC
    };

    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++) {
        brain_t brain = genius_brain_create(types[i]);
        ASSERT_NE(brain, nullptr);
        brain_destroy(brain);
    }
}

//=============================================================================
// 2. HEMISPHERIC BRAIN WORKFLOW
//=============================================================================

TEST_F(BrainFactoryE2ETest, HemisphericBrainWorkflow) {
    hemispheric_brain_t* hemi = genius_hemispheric_brain_create(GENIUS_TYPE_VISUAL_ARTISTIC);
    ASSERT_NE(hemi, nullptr);

    hemispheric_brain_destroy(hemi);
}

TEST_F(BrainFactoryE2ETest, AllHemisphericTypesWorkflow) {
    genius_type_t types[] = {
        GENIUS_TYPE_MATHEMATICAL,
        GENIUS_TYPE_VISUAL_ARTISTIC,
        GENIUS_TYPE_MUSICAL,
        GENIUS_TYPE_SCIENTIFIC
    };

    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++) {
        hemispheric_brain_t* hemi = genius_hemispheric_brain_create(types[i]);
        ASSERT_NE(hemi, nullptr);
        hemispheric_brain_destroy(hemi);
    }
}

//=============================================================================
// 3. PROFILE SWITCHING WORKFLOW
//=============================================================================

TEST_F(BrainFactoryE2ETest, CreateBrainAndSwitchProfiles) {
    brain_t brain = genius_brain_create(GENIUS_TYPE_MATHEMATICAL);
    ASSERT_NE(brain, nullptr);

    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    err = genius_profiles_activate(bridge, GENIUS_TYPE_VISUAL_ARTISTIC, 1.0f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_ACTIVE);

    brain_destroy(brain);
}

TEST_F(BrainFactoryE2ETest, ActivateDeactivateCycle) {
    for (int i = 0; i < 3; i++) {
        genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_SCIENTIFIC, 1.0f);
        EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

        genius_profiles_deactivate(bridge);
        EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_INACTIVE);
    }
}

//=============================================================================
// 4. MULTI-BRAIN WORKFLOW
//=============================================================================

TEST_F(BrainFactoryE2ETest, MultipleBrainsConcurrently) {
    brain_t brains[4] = {nullptr};

    brains[0] = genius_brain_create(GENIUS_TYPE_MATHEMATICAL);
    brains[1] = genius_brain_create(GENIUS_TYPE_VISUAL_ARTISTIC);
    brains[2] = genius_brain_create(GENIUS_TYPE_MUSICAL);
    brains[3] = genius_brain_create(GENIUS_TYPE_SCIENTIFIC);

    for (int i = 0; i < 4; i++) {
        ASSERT_NE(brains[i], nullptr);
    }

    for (int i = 3; i >= 0; i--) {
        brain_destroy(brains[i]);
    }
}

TEST_F(BrainFactoryE2ETest, MixedBrainTypes) {
    brain_t regular = genius_brain_create(GENIUS_TYPE_MATHEMATICAL);
    ASSERT_NE(regular, nullptr);

    hemispheric_brain_t* hemi = genius_hemispheric_brain_create(GENIUS_TYPE_VISUAL_ARTISTIC);
    ASSERT_NE(hemi, nullptr);

    hemispheric_brain_destroy(hemi);
    brain_destroy(regular);
}

//=============================================================================
// 5. FULL WORKFLOW
//=============================================================================

TEST_F(BrainFactoryE2ETest, FullProfileBrainWorkflow) {
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_SCIENTIFIC, 1.0f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    const eidetic_memory_config_t* eidetic = genius_profiles_get_eidetic_config(bridge);
    EXPECT_NE(eidetic, nullptr);

    brain_t brain = genius_brain_create(GENIUS_TYPE_SCIENTIFIC);
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_ACTIVE);

    err = genius_profiles_enter_flow(bridge, 0.7f, 0.8f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    genius_profiles_exit_flow(bridge, "test_complete");
    genius_profiles_deactivate(bridge);
    brain_destroy(brain);
}

//=============================================================================
// 6. STRESS TESTS
//=============================================================================

TEST_F(BrainFactoryE2ETest, RapidBrainCreation) {
    for (int i = 0; i < 10; i++) {
        brain_t brain = genius_brain_create(GENIUS_TYPE_MATHEMATICAL);
        ASSERT_NE(brain, nullptr);
        brain_destroy(brain);
    }
}

TEST_F(BrainFactoryE2ETest, RapidProfileSwitching) {
    genius_type_t types[] = {
        GENIUS_TYPE_MATHEMATICAL,
        GENIUS_TYPE_VISUAL_ARTISTIC,
        GENIUS_TYPE_MUSICAL,
        GENIUS_TYPE_SCIENTIFIC
    };

    for (int iter = 0; iter < 3; iter++) {
        for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++) {
            genius_profiles_activate(bridge, types[i], 1.0f);
        }
    }

    genius_profiles_deactivate(bridge);
}
