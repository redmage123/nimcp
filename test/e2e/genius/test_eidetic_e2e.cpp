/**
 * @file test_eidetic_e2e.cpp
 * @brief End-to-end tests for eidetic memory integration workflow
 *
 * Test Scenarios:
 * 1. Tesla Workflow - Visual-spatial eidetic genius
 * 2. Mozart Workflow - Auditory eidetic genius
 * 3. von Neumann Workflow - Verbal-numerical eidetic genius
 * 4. Polymath Workflow - Combined eidetic profiles
 * 5. Full System Workflow - Bridge creation through eidetic application
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
// Include profiles first to establish typedefs
#include "core/brain/genius/nimcp_genius_profiles.h"
#include "core/brain/genius/nimcp_genius_types.h"
#include "core/brain/genius/nimcp_genius_traits.h"
// Eidetic header last - uses forward declarations compatible with profiles
#include "core/brain/genius/eidetic/nimcp_eidetic_memory.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class EideticE2ETest : public ::testing::Test {
protected:
    genius_profiles_bridge_t* bridge = nullptr;
    genius_profiles_config_t config;

    void SetUp() override {
        // Get default config
        ASSERT_EQ(genius_profiles_config_default(&config), GENIUS_ERROR_SUCCESS);

        // Minimal external integrations for focused eidetic testing
        config.enable_bio_async = false;
        config.enable_mesh_coordination = false;
        config.enable_training_integration = false;
        config.enable_rcog_integration = false;
        config.enable_ccog_integration = false;
        config.enable_quantum_optimization = false;
        config.enable_kg_wiring = false;
    }

    void TearDown() override {
        if (bridge) {
            genius_profiles_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Helper for floating point comparison
    static bool FloatEqual(float a, float b, float epsilon = 0.001f) {
        return std::fabs(a - b) < epsilon;
    }
};

//=============================================================================
// 1. TESLA WORKFLOW - Visual-Spatial Eidetic
//=============================================================================

TEST_F(EideticE2ETest, TeslaWorkflowComplete) {
    // Step 1: Create bridge
    bridge = genius_profiles_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Step 2: Verify Tesla preset is available and valid
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    ASSERT_NE(tesla, nullptr);
    EXPECT_TRUE(eidetic_config_is_valid(tesla));

    // Step 3: Activate scientific profile (Tesla's domain)
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_SCIENTIFIC, 1.0f);
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Step 4: Verify profile state
    genius_activation_state_t state = genius_profiles_get_state(bridge);
    EXPECT_EQ(state, GENIUS_STATE_ACTIVE);

    // Step 5: Get eidetic config from active profile
    const eidetic_memory_config_t* eidetic = genius_profiles_get_eidetic_config(bridge);
    ASSERT_NE(eidetic, nullptr);

    // Step 6: Verify Tesla-like eidetic characteristics
    EXPECT_GT(eidetic->visual_eidetic, 2.0f);
    EXPECT_GT(eidetic->spatial_eidetic, 2.0f);

    // Step 7: Apply eidetic enhancements
    err = genius_profiles_apply_eidetic(bridge);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Step 8: Enter flow state for optimal performance
    err = genius_profiles_enter_flow(bridge, 0.8f, 0.9f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    state = genius_profiles_get_state(bridge);
    EXPECT_EQ(state, GENIUS_STATE_FLOW);

    // Step 9: Exit flow and deactivate
    err = genius_profiles_exit_flow(bridge, "test_complete");
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    err = genius_profiles_deactivate(bridge);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);
}

//=============================================================================
// 2. MOZART WORKFLOW - Auditory Eidetic
//=============================================================================

TEST_F(EideticE2ETest, MozartWorkflowComplete) {
    bridge = genius_profiles_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Verify Mozart preset
    const eidetic_memory_config_t* mozart = eidetic_config_mozart();
    ASSERT_NE(mozart, nullptr);
    EXPECT_GT(mozart->auditory_eidetic, 2.5f);

    // Activate musical profile
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_MUSICAL, 1.0f);
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Get eidetic config
    const eidetic_memory_config_t* eidetic = genius_profiles_get_eidetic_config(bridge);
    ASSERT_NE(eidetic, nullptr);

    // Musical genius should have auditory eidetic
    EXPECT_GT(eidetic->auditory_eidetic, 2.0f);

    // Apply and verify
    err = genius_profiles_apply_eidetic(bridge);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Cleanup
    err = genius_profiles_deactivate(bridge);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);
}

//=============================================================================
// 3. VON NEUMANN WORKFLOW - Verbal-Numerical Eidetic
//=============================================================================

TEST_F(EideticE2ETest, VonNeumannWorkflowComplete) {
    bridge = genius_profiles_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Verify von Neumann preset
    const eidetic_memory_config_t* vn = eidetic_config_vonneumann();
    ASSERT_NE(vn, nullptr);
    EXPECT_GT(vn->verbal_eidetic, 2.5f);

    // Activate mathematical profile
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Get eidetic config
    const eidetic_memory_config_t* eidetic = genius_profiles_get_eidetic_config(bridge);
    ASSERT_NE(eidetic, nullptr);

    // Mathematical genius should have enhanced working memory
    EXPECT_GT(vn->working_memory.capacity_boost, 2u);

    // Apply eidetic
    err = genius_profiles_apply_eidetic(bridge);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Enter flow for calculation performance
    err = genius_profiles_enter_flow(bridge, 0.9f, 0.95f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Exit and cleanup
    err = genius_profiles_exit_flow(bridge, "test_complete");
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    err = genius_profiles_deactivate(bridge);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);
}

//=============================================================================
// 4. POLYMATH WORKFLOW - Combined Eidetic
//=============================================================================

TEST_F(EideticE2ETest, PolymathWorkflowComplete) {
    bridge = genius_profiles_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Create Da Vinci-style polymath: scientific + artistic
    genius_error_t err = genius_profiles_create_polymath(
        bridge,
        GENIUS_TYPE_SCIENTIFIC,
        GENIUS_TYPE_VISUAL_ARTISTIC,
        0.4f
    );
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    // State should be BLENDED
    genius_activation_state_t state = genius_profiles_get_state(bridge);
    EXPECT_EQ(state, GENIUS_STATE_BLENDED);

    // Get blended eidetic config
    const eidetic_memory_config_t* eidetic = genius_profiles_get_eidetic_config(bridge);
    ASSERT_NE(eidetic, nullptr);

    // Should have strong visual eidetic from both profiles
    EXPECT_GT(eidetic->visual_eidetic, 2.0f);

    // Validate blended config
    EXPECT_TRUE(eidetic_config_is_valid(eidetic));

    // Apply eidetic
    err = genius_profiles_apply_eidetic(bridge);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Enter flow
    err = genius_profiles_enter_flow(bridge, 0.85f, 0.9f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Cleanup
    err = genius_profiles_exit_flow(bridge, "test_complete");
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    err = genius_profiles_deactivate(bridge);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);
}

//=============================================================================
// 5. FULL SYSTEM WORKFLOW
//=============================================================================

TEST_F(EideticE2ETest, FullSystemWorkflow) {
    // Test complete lifecycle with all genius types

    for (int type = GENIUS_TYPE_MATHEMATICAL; type < GENIUS_TYPE_POLYMATH; type++) {
        genius_type_t gtype = static_cast<genius_type_t>(type);

        // Create fresh bridge for each type
        bridge = genius_profiles_bridge_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed to create bridge for type " << type;

        // Activate profile
        genius_error_t err = genius_profiles_activate(bridge, gtype, 1.0f);
        ASSERT_EQ(err, GENIUS_ERROR_SUCCESS)
            << "Failed to activate type " << genius_type_name(gtype);

        // Get eidetic config
        const eidetic_memory_config_t* eidetic = genius_profiles_get_eidetic_config(bridge);
        ASSERT_NE(eidetic, nullptr)
            << "No eidetic config for type " << genius_type_name(gtype);

        // Validate config
        EXPECT_TRUE(eidetic_config_is_valid(eidetic))
            << "Invalid eidetic config for type " << genius_type_name(gtype);

        // Apply eidetic
        err = genius_profiles_apply_eidetic(bridge);
        EXPECT_EQ(err, GENIUS_ERROR_SUCCESS)
            << "Failed to apply eidetic for type " << genius_type_name(gtype);

        // Cleanup
        genius_profiles_deactivate(bridge);
        genius_profiles_bridge_destroy(bridge);
        bridge = nullptr;
    }
}

//=============================================================================
// 6. PROFILE SWITCHING WORKFLOW
//=============================================================================

TEST_F(EideticE2ETest, ProfileSwitchingWorkflow) {
    bridge = genius_profiles_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Start with mathematical profile
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    const eidetic_memory_config_t* math_eidetic = genius_profiles_get_eidetic_config(bridge);
    ASSERT_NE(math_eidetic, nullptr);
    float math_spatial = math_eidetic->spatial_eidetic;

    // Apply eidetic
    err = genius_profiles_apply_eidetic(bridge);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Deactivate and switch to musical
    err = genius_profiles_deactivate(bridge);
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    err = genius_profiles_activate(bridge, GENIUS_TYPE_MUSICAL, 1.0f);
    ASSERT_EQ(err, GENIUS_ERROR_SUCCESS);

    const eidetic_memory_config_t* music_eidetic = genius_profiles_get_eidetic_config(bridge);
    ASSERT_NE(music_eidetic, nullptr);
    float music_auditory = music_eidetic->auditory_eidetic;

    // Musical should have higher auditory than mathematical
    EXPECT_GT(music_auditory, math_eidetic->auditory_eidetic);

    // Apply new eidetic
    err = genius_profiles_apply_eidetic(bridge);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);

    // Cleanup
    genius_profiles_deactivate(bridge);
}

//=============================================================================
// 7. UTILITY FUNCTION E2E TESTS
//=============================================================================

TEST_F(EideticE2ETest, ScaleValueWorkflow) {
    // Test scale value across typical use cases
    float base_capacity = 7.0f;  // Normal working memory

    // At eidetic strength 2.0, with max multiplier 2.0
    float enhanced_capacity = eidetic_scale_value(base_capacity, 2.0f, 2.0f);

    // Should be significantly enhanced
    EXPECT_GT(enhanced_capacity, base_capacity);
    EXPECT_LT(enhanced_capacity, base_capacity * 2.0f);
}

TEST_F(EideticE2ETest, DecayResistanceWorkflow) {
    // Test decay resistance for memory retention
    float normal_decay = 0.1f;  // Normal decay rate

    // At eidetic strength 2.5
    float resistance = eidetic_compute_decay_resistance(2.5f);

    // Effective decay should be much lower
    float effective_decay = normal_decay * resistance;
    EXPECT_LT(effective_decay, normal_decay * 0.3f);  // At least 70% reduction
}

//=============================================================================
// 8. ERROR HANDLING WORKFLOW
//=============================================================================

TEST_F(EideticE2ETest, ErrorHandlingWorkflow) {
    // Test graceful error handling

    // Apply to all with no systems should handle gracefully
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    eidetic_error_t err = eidetic_apply_to_all(tesla, nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);

    // Apply with null config
    err = eidetic_apply_to_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);

    // Invalid config
    eidetic_memory_config_t invalid_config;
    memcpy(&invalid_config, tesla, sizeof(invalid_config));
    invalid_config.visual_eidetic = -1.0f;

    err = eidetic_apply_to_all(&invalid_config, nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(err, EIDETIC_ERROR_INVALID_CONFIG);
}

//=============================================================================
// 9. PRESET COMPARISON WORKFLOW
//=============================================================================

TEST_F(EideticE2ETest, PresetComparisonWorkflow) {
    // Compare all presets to verify they have distinct characteristics

    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    const eidetic_memory_config_t* mozart = eidetic_config_mozart();
    const eidetic_memory_config_t* vn = eidetic_config_vonneumann();
    const eidetic_memory_config_t* kp = eidetic_config_kim_peek();
    const eidetic_memory_config_t* sw = eidetic_config_wiltshire();

    // Tesla should have highest visual among most
    EXPECT_GT(tesla->visual_eidetic, mozart->visual_eidetic);

    // Mozart should have highest auditory
    EXPECT_GT(mozart->auditory_eidetic, tesla->auditory_eidetic);
    EXPECT_GT(mozart->auditory_eidetic, vn->auditory_eidetic);

    // von Neumann should have high verbal
    EXPECT_GT(vn->verbal_eidetic, tesla->verbal_eidetic);

    // Kim Peek should have exceptional detail granularity (>= 2.5)
    EXPECT_GE(kp->detail_granularity, 2.5f);

    // Wiltshire should have exceptional visual
    EXPECT_GE(sw->visual_eidetic, 2.5f);
}
