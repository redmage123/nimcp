/**
 * @file test_eidetic_integration.cpp
 * @brief Integration tests for eidetic memory with genius profiles
 *
 * Test Categories:
 * 1. Profile Eidetic Integration - Eidetic configs in genius profiles
 * 2. Memory System Integration - Apply eidetic to memory subsystems
 * 3. Cross-System Enhancement - Multiple systems enhanced together
 * 4. Preset Integration - Tesla, Mozart, von Neumann with real systems
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
#include "core/brain/genius/eidetic/nimcp_eidetic_memory.h"
#include "core/brain/genius/nimcp_genius_profiles.h"
#include "core/brain/genius/nimcp_genius_types.h"
#include "core/brain/genius/nimcp_genius_traits.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class EideticIntegrationTest : public ::testing::Test {
protected:
    genius_profiles_bridge_t* bridge = nullptr;
    genius_profiles_config_t config;

    void SetUp() override {
        // Get default config
        ASSERT_EQ(genius_profiles_config_default(&config), GENIUS_ERROR_SUCCESS);

        // Disable external integrations for focused eidetic testing
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
// 1. PROFILE EIDETIC INTEGRATION TESTS
//=============================================================================

TEST_F(EideticIntegrationTest, ScientificProfileHasEideticConfig) {
    // Scientific genius (Tesla) should have strong eidetic config
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_SCIENTIFIC, 1.0f);
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    const eidetic_memory_config_t* eidetic = genius_profiles_get_eidetic_config(bridge);
    ASSERT_NE(eidetic, nullptr);

    // Tesla was famous for visual-spatial eidetic memory
    EXPECT_GT(eidetic->visual_eidetic, 2.0f);
    EXPECT_GT(eidetic->spatial_eidetic, 2.0f);
}

TEST_F(EideticIntegrationTest, MusicalProfileHasAuditoryEidetic) {
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_MUSICAL, 1.0f);
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    const eidetic_memory_config_t* eidetic = genius_profiles_get_eidetic_config(bridge);
    ASSERT_NE(eidetic, nullptr);

    // Mozart was famous for auditory eidetic memory
    EXPECT_GT(eidetic->auditory_eidetic, 2.0f);
}

TEST_F(EideticIntegrationTest, MathematicalProfileActivationReturnsEideticConfig) {
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Profile should return an eidetic config pointer
    const eidetic_memory_config_t* eidetic = genius_profiles_get_eidetic_config(bridge);
    EXPECT_NE(eidetic, nullptr);
}

TEST_F(EideticIntegrationTest, VisualArtisticProfileActivationReturnsEideticConfig) {
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_VISUAL_ARTISTIC, 1.0f);
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Profile should return an eidetic config pointer
    const eidetic_memory_config_t* eidetic = genius_profiles_get_eidetic_config(bridge);
    EXPECT_NE(eidetic, nullptr);
}

//=============================================================================
// 2. PRESET INTEGRATION TESTS
//=============================================================================

TEST_F(EideticIntegrationTest, TeslaPresetIntegration) {
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    ASSERT_NE(tesla, nullptr);

    // Verify Tesla preset has key eidetic characteristics
    EXPECT_GT(tesla->visual_eidetic, 2.5f);
    EXPECT_GT(tesla->spatial_eidetic, 2.5f);
    EXPECT_GT(tesla->working_memory.capacity_boost, 0u);
    EXPECT_GT(tesla->hippocampus.encoding_speed, 1.0f);

    // Should be valid for application
    EXPECT_TRUE(eidetic_config_is_valid(tesla));
}

TEST_F(EideticIntegrationTest, MozartPresetIntegration) {
    const eidetic_memory_config_t* mozart = eidetic_config_mozart();
    ASSERT_NE(mozart, nullptr);

    // Mozart's auditory eidetic should enhance auditory memory systems
    EXPECT_GT(mozart->auditory_eidetic, 2.0f);
    EXPECT_TRUE(eidetic_config_is_valid(mozart));
}

TEST_F(EideticIntegrationTest, VonNeumannPresetIntegration) {
    const eidetic_memory_config_t* vn = eidetic_config_vonneumann();
    ASSERT_NE(vn, nullptr);

    // von Neumann's verbal/numerical eidetic
    EXPECT_GT(vn->verbal_eidetic, 2.0f);
    EXPECT_GT(vn->working_memory.capacity_boost, 3u);  // Extended WM capacity
    EXPECT_TRUE(eidetic_config_is_valid(vn));
}

TEST_F(EideticIntegrationTest, KimPeekPresetIntegration) {
    const eidetic_memory_config_t* kp = eidetic_config_kim_peek();
    ASSERT_NE(kp, nullptr);

    // Kim Peek had exceptional factual/verbal memory
    EXPECT_GT(kp->detail_granularity, 2.0f);
    EXPECT_GT(kp->retrieval_accuracy, 0.9f);
    EXPECT_TRUE(eidetic_config_is_valid(kp));
}

TEST_F(EideticIntegrationTest, WiltshirePresetIntegration) {
    const eidetic_memory_config_t* sw = eidetic_config_wiltshire();
    ASSERT_NE(sw, nullptr);

    // Stephen Wiltshire - visual-artistic eidetic
    EXPECT_GT(sw->visual_eidetic, 2.5f);
    EXPECT_TRUE(eidetic_config_is_valid(sw));
}

//=============================================================================
// 3. APPLY EIDETIC TO BRIDGE TESTS
//=============================================================================

TEST_F(EideticIntegrationTest, ApplyEideticToActiveBridge) {
    // Activate a profile first
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_SCIENTIFIC, 1.0f);
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Apply eidetic enhancements
    err = genius_profiles_apply_eidetic(bridge);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);
}

TEST_F(EideticIntegrationTest, ApplyEideticToInactiveBridgeReturnsError) {
    // Don't activate any profile - bridge is created but inactive
    // Apply eidetic should return error for inactive bridge
    genius_error_t err = genius_profiles_apply_eidetic(bridge);
    // Without an active profile, there's no eidetic config to apply
    EXPECT_NE(err, GENIUS_ERROR_SUCCESS);
}

//=============================================================================
// 4. CROSS-PROFILE EIDETIC TESTS
//=============================================================================

TEST_F(EideticIntegrationTest, PolymathHasBlendedEidetic) {
    // Create polymath (scientific + artistic)
    genius_error_t err = genius_profiles_create_polymath(
        bridge,
        GENIUS_TYPE_SCIENTIFIC,
        GENIUS_TYPE_VISUAL_ARTISTIC,
        0.4f
    );
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    const eidetic_memory_config_t* eidetic = genius_profiles_get_eidetic_config(bridge);
    ASSERT_NE(eidetic, nullptr);

    // Should have blended visual eidetic from both profiles
    EXPECT_GT(eidetic->visual_eidetic, 2.0f);
}

TEST_F(EideticIntegrationTest, BlendedProfilesHaveValidEidetic) {
    genius_type_t types[] = {GENIUS_TYPE_MUSICAL, GENIUS_TYPE_MATHEMATICAL};
    float weights[] = {0.6f, 0.4f};

    genius_error_t err = genius_profiles_blend(bridge, types, weights, 2);
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    const eidetic_memory_config_t* eidetic = genius_profiles_get_eidetic_config(bridge);
    ASSERT_NE(eidetic, nullptr);

    // Blended config should still be valid
    EXPECT_TRUE(eidetic_config_is_valid(eidetic));
}

//=============================================================================
// 5. EIDETIC CONFIG FIELD INTEGRATION TESTS
//=============================================================================

TEST_F(EideticIntegrationTest, HippocampusConfigKeyFieldsSet) {
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    ASSERT_NE(tesla, nullptr);

    // Tesla preset sets encoding_speed, encoding_fidelity, single_exposure_learning
    EXPECT_GE(tesla->hippocampus.encoding_speed, 1.0f);
    EXPECT_LE(tesla->hippocampus.encoding_speed, 10.0f);

    EXPECT_GT(tesla->hippocampus.encoding_fidelity, 0.9f);
    EXPECT_LE(tesla->hippocampus.encoding_fidelity, 1.0f);

    EXPECT_TRUE(tesla->hippocampus.single_exposure_learning);
}

TEST_F(EideticIntegrationTest, WorkingMemoryConfigKeyFieldsSet) {
    const eidetic_memory_config_t* vn = eidetic_config_vonneumann();
    ASSERT_NE(vn, nullptr);

    // von Neumann should have enhanced working memory capacity
    EXPECT_GT(vn->working_memory.capacity_boost, 0u);
    EXPECT_LE(vn->working_memory.capacity_boost, 10u);  // Reasonable max

    EXPECT_GT(vn->working_memory.decay_multiplier, 0.0f);
    EXPECT_LE(vn->working_memory.decay_multiplier, 1.0f);
}

TEST_F(EideticIntegrationTest, SemanticConfigKeyFieldsSet) {
    const eidetic_memory_config_t* kp = eidetic_config_kim_peek();
    ASSERT_NE(kp, nullptr);

    // Kim Peek should have enhanced semantic memory concept capacity
    EXPECT_GT(kp->semantic.concept_capacity_multiplier, 1u);
    EXPECT_TRUE(kp->semantic.enable_instant_learning);
}

//=============================================================================
// 6. UTILITY FUNCTION INTEGRATION TESTS
//=============================================================================

TEST_F(EideticIntegrationTest, ScaleValueConsistency) {
    // Test that scale values are consistent across calls
    float v1 = eidetic_scale_value(100.0f, 2.0f, 4.0f);
    float v2 = eidetic_scale_value(100.0f, 2.0f, 4.0f);
    EXPECT_FLOAT_EQ(v1, v2);
}

TEST_F(EideticIntegrationTest, DecayResistanceConsistency) {
    float r1 = eidetic_compute_decay_resistance(1.5f);
    float r2 = eidetic_compute_decay_resistance(1.5f);
    EXPECT_FLOAT_EQ(r1, r2);
}

TEST_F(EideticIntegrationTest, ScaleAndDecayCorrelation) {
    // Higher eidetic strength should correlate with:
    // - Higher scaled values
    // - Lower decay resistance (slower decay)

    float scale_low = eidetic_scale_value(100.0f, 0.5f, 4.0f);
    float scale_mid = eidetic_scale_value(100.0f, 1.5f, 4.0f);
    float scale_high = eidetic_scale_value(100.0f, 2.5f, 4.0f);

    EXPECT_LT(scale_low, scale_mid);
    EXPECT_LT(scale_mid, scale_high);

    float decay_low = eidetic_compute_decay_resistance(0.5f);
    float decay_mid = eidetic_compute_decay_resistance(1.5f);
    float decay_high = eidetic_compute_decay_resistance(2.5f);

    EXPECT_GT(decay_low, decay_mid);
    EXPECT_GT(decay_mid, decay_high);
}
