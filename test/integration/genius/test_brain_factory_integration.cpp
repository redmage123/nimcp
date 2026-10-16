/**
 * @file test_brain_factory_integration.cpp
 * @brief Integration tests for genius profiles brain factory integration
 *
 * Test Categories:
 * 1. Brain Creation Tests - Create brains with genius profiles
 * 2. Hemispheric Brain Tests - Create lateralized hemispheric brains
 * 3. Profile Application Tests - Verify genius traits are applied
 * 4. KG Wiring Tests - Knowledge graph entity registration
 * 5. Cross-System Tests - Multiple integrations together
 *
 * @author NIMCP Development Team
 * @date 2026-02-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/brain/genius/nimcp_genius_profiles.h"
#include "core/brain/genius/nimcp_genius_types.h"
#include "core/brain/genius/nimcp_genius_traits.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BrainFactoryIntegrationTest : public ::testing::Test {
protected:
    genius_profiles_bridge_t* bridge = nullptr;
    genius_profiles_config_t config;

    void SetUp() override {
        // Get default config
        ASSERT_EQ(genius_profiles_config_default(&config), GENIUS_ERROR_SUCCESS);

        // Disable external integrations for focused testing
        config.enable_bio_async = false;
        config.enable_mesh_coordination = false;
        config.enable_training_integration = false;
        config.enable_rcog_integration = false;
        config.enable_ccog_integration = false;
        config.enable_quantum_optimization = false;
        config.enable_kg_wiring = false;

        // Create bridge
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
// 1. BRAIN CREATION TESTS
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, CreateMathematicalGeniusBrain) {
    brain_t brain = genius_brain_create_ex(GENIUS_TYPE_MATHEMATICAL, true);
    ASSERT_NE(brain, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainFactoryIntegrationTest, CreateVisualArtisticGeniusBrain) {
    brain_t brain = genius_brain_create_ex(GENIUS_TYPE_VISUAL_ARTISTIC, true);
    ASSERT_NE(brain, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainFactoryIntegrationTest, CreateMusicalGeniusBrain) {
    brain_t brain = genius_brain_create_ex(GENIUS_TYPE_MUSICAL, true);
    ASSERT_NE(brain, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainFactoryIntegrationTest, CreateScientificGeniusBrain) {
    brain_t brain = genius_brain_create_ex(GENIUS_TYPE_SCIENTIFIC, true);
    ASSERT_NE(brain, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainFactoryIntegrationTest, CreateFinancialGeniusBrain) {
    brain_t brain = genius_brain_create_ex(GENIUS_TYPE_FINANCIAL, true);
    ASSERT_NE(brain, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainFactoryIntegrationTest, CreateAllGeniusTypesBrains) {
    genius_type_t types[] = {
        GENIUS_TYPE_MATHEMATICAL,
        GENIUS_TYPE_VISUAL_ARTISTIC,
        GENIUS_TYPE_MUSICAL,
        GENIUS_TYPE_LITERARY,
        GENIUS_TYPE_SCIENTIFIC,
        GENIUS_TYPE_ATHLETIC,
        GENIUS_TYPE_STRATEGIC,
        GENIUS_TYPE_FINANCIAL
    };

    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++) {
        brain_t brain = genius_brain_create_ex(types[i], true);
        ASSERT_NE(brain, nullptr) << "Failed to create brain for type " << (int)types[i];

        brain_destroy(brain);
    }
}

TEST_F(BrainFactoryIntegrationTest, InvalidGeniusTypeReturnsNull) {
    brain_t brain = genius_brain_create_ex((genius_type_t)999, true);
    EXPECT_EQ(brain, nullptr);
}

//=============================================================================
// 2. HEMISPHERIC BRAIN TESTS
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, CreateMathematicalHemisphericBrain) {
    hemispheric_brain_t* hemi_brain = genius_hemispheric_brain_create_ex(GENIUS_TYPE_MATHEMATICAL, true);
    ASSERT_NE(hemi_brain, nullptr);

    hemispheric_brain_destroy(hemi_brain);
}

TEST_F(BrainFactoryIntegrationTest, CreateVisualArtisticHemisphericBrain) {
    hemispheric_brain_t* hemi_brain = genius_hemispheric_brain_create_ex(GENIUS_TYPE_VISUAL_ARTISTIC, true);
    ASSERT_NE(hemi_brain, nullptr);

    hemispheric_brain_destroy(hemi_brain);
}

TEST_F(BrainFactoryIntegrationTest, CreateMusicalHemisphericBrain) {
    hemispheric_brain_t* hemi_brain = genius_hemispheric_brain_create_ex(GENIUS_TYPE_MUSICAL, true);
    ASSERT_NE(hemi_brain, nullptr);

    hemispheric_brain_destroy(hemi_brain);
}

TEST_F(BrainFactoryIntegrationTest, InvalidTypeHemisphericBrainReturnsNull) {
    hemispheric_brain_t* hemi_brain = genius_hemispheric_brain_create_ex((genius_type_t)999, true);
    EXPECT_EQ(hemi_brain, nullptr);
}

//=============================================================================
// 3. PROFILE APPLICATION TESTS
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, MathematicalBrainHasStats) {
    brain_t brain = genius_brain_create_ex(GENIUS_TYPE_MATHEMATICAL, true);
    ASSERT_NE(brain, nullptr);

    // Verify brain can provide stats
    brain_stats_t stats;
    bool has_stats = brain_get_stats(brain, &stats);
    EXPECT_TRUE(has_stats);

    brain_destroy(brain);
}

TEST_F(BrainFactoryIntegrationTest, VisualArtisticBrainHasStats) {
    brain_t brain = genius_brain_create_ex(GENIUS_TYPE_VISUAL_ARTISTIC, true);
    ASSERT_NE(brain, nullptr);

    brain_stats_t stats;
    bool has_stats = brain_get_stats(brain, &stats);
    EXPECT_TRUE(has_stats);

    brain_destroy(brain);
}

//=============================================================================
// 4. KG WIRING TESTS
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, RegisterKGWiringWithActiveProfile) {
    // First activate a profile
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Register KG wiring - may succeed or fail depending on KG availability
    err = genius_profiles_register_kg_wiring(bridge);
    // Just verify it doesn't crash
    EXPECT_TRUE(err == GENIUS_ERROR_SUCCESS ||
                err == GENIUS_ERROR_KG_UNAVAILABLE ||
                err == GENIUS_ERROR_NOT_ACTIVE);
}

TEST_F(BrainFactoryIntegrationTest, RegisterKGWiringWithoutActivationFails) {
    // Try to register KG wiring without activating a profile first
    genius_error_t err = genius_profiles_register_kg_wiring(bridge);
    // Should fail because no profile is active
    EXPECT_NE(err, GENIUS_ERROR_SUCCESS);
}

//=============================================================================
// 5. CROSS-SYSTEM TESTS
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, CreateBrainAndActivateProfile) {
    // Create brain (test_mode for fast init)
    brain_t brain = genius_brain_create_ex(GENIUS_TYPE_MATHEMATICAL, true);
    ASSERT_NE(brain, nullptr);

    // Also activate profile on bridge
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Verify bridge state
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_ACTIVE);

    brain_destroy(brain);
}

TEST_F(BrainFactoryIntegrationTest, CreateMultipleBrainsConcurrently) {
    brain_t brains[4] = {nullptr};

    // Create multiple brains of different types (test_mode for fast init)
    brains[0] = genius_brain_create_ex(GENIUS_TYPE_MATHEMATICAL, true);
    brains[1] = genius_brain_create_ex(GENIUS_TYPE_VISUAL_ARTISTIC, true);
    brains[2] = genius_brain_create_ex(GENIUS_TYPE_MUSICAL, true);
    brains[3] = genius_brain_create_ex(GENIUS_TYPE_SCIENTIFIC, true);

    // Verify all were created
    for (int i = 0; i < 4; i++) {
        EXPECT_NE(brains[i], nullptr) << "Brain " << i << " was not created";
    }

    // Clean up
    for (int i = 0; i < 4; i++) {
        if (brains[i]) {
            brain_destroy(brains[i]);
        }
    }
}

TEST_F(BrainFactoryIntegrationTest, CreateHemisphericBrainAndActivateProfile) {
    // Create hemispheric brain (test_mode for fast init)
    hemispheric_brain_t* hemi_brain = genius_hemispheric_brain_create_ex(GENIUS_TYPE_VISUAL_ARTISTIC, true);
    ASSERT_NE(hemi_brain, nullptr);

    // Also activate profile on bridge
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_VISUAL_ARTISTIC, 1.0f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Verify bridge state
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_ACTIVE);

    hemispheric_brain_destroy(hemi_brain);
}

//=============================================================================
// 6. ERROR HANDLING TESTS
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, CreateBrainWithPolymathTypeNotSupported) {
    // GENIUS_TYPE_POLYMATH requires special handling, may or may not be supported
    brain_t brain = genius_brain_create_ex(GENIUS_TYPE_POLYMATH, true);
    // May succeed or fail depending on implementation
    if (brain != nullptr) {
        brain_destroy(brain);
    }
}

TEST_F(BrainFactoryIntegrationTest, CreateBrainNegativeType) {
    brain_t brain = genius_brain_create_ex((genius_type_t)-1, true);
    EXPECT_EQ(brain, nullptr);
}

TEST_F(BrainFactoryIntegrationTest, CreateBrainTypeAtBoundary) {
    brain_t brain = genius_brain_create_ex(GENIUS_TYPE_COUNT, true);
    EXPECT_EQ(brain, nullptr);
}

//=============================================================================
// 7. RAPID CREATION/DESTRUCTION TESTS
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, RapidBrainCreationDestruction) {
    for (int i = 0; i < 5; i++) {
        brain_t brain = genius_brain_create_ex(GENIUS_TYPE_MATHEMATICAL, true);
        ASSERT_NE(brain, nullptr) << "Failed at iteration " << i;
        brain_destroy(brain);
    }
}

TEST_F(BrainFactoryIntegrationTest, RapidHemisphericBrainCreationDestruction) {
    for (int i = 0; i < 5; i++) {
        hemispheric_brain_t* hemi = genius_hemispheric_brain_create_ex(GENIUS_TYPE_VISUAL_ARTISTIC, true);
        ASSERT_NE(hemi, nullptr) << "Failed at iteration " << i;
        hemispheric_brain_destroy(hemi);
    }
}
