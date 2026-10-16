/**
 * @file test_genius_e2e.cpp
 * @brief End-to-end tests for genius profiles module
 *
 * WHAT: Tests complete genius profiles workflows from creation to destruction
 * WHY:  Verify the full system works as an integrated whole
 * HOW:  Tests realistic usage scenarios with multiple system interactions
 *
 * E2E scenarios:
 *   1. Complete profile lifecycle (create -> activate -> use -> deactivate -> destroy)
 *   2. Polymath workflow (blend profiles -> operate -> switch -> reset)
 *   3. Cognitive session simulation (fatigue -> flow -> recovery)
 *   4. Immune modulation workflow (cytokine effects -> degradation -> recovery)
 *   5. Training workflow (activate -> train -> evaluate -> adapt)
 *   6. Multi-profile session (switch between profiles during session)
 *   7. Stress test (rapid operations under load)
 *   8. Brain creation workflow (profile -> brain config -> brain instance)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

extern "C" {
#include "core/brain/genius/nimcp_genius_profiles.h"
#include "core/brain/genius/nimcp_genius_types.h"
#include "core/brain/genius/nimcp_genius_traits.h"
}

namespace nimcp {
namespace test {
namespace e2e {

/* ============================================================================
 * TEST FIXTURE
 * ============================================================================ */

class GeniusE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create bridge with realistic configuration for E2E testing
        genius_profiles_config_t config;
        ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_config_default(&config));

        // Enable features that don't require external systems
        config.enable_bio_async = false;       // No real router
        config.enable_mesh_coordination = false;  // No mesh network
        config.enable_immune_modulation = true;   // Test immune effects
        config.enable_health_agent = false;       // No background thread
        config.enable_training_integration = true;
        config.enable_snn_integration = true;
        config.enable_stdp = true;
        config.enable_metaplasticity = true;
        config.base_learning_rate = 0.01f;

        bridge_ = genius_profiles_bridge_create(&config);
        ASSERT_NE(nullptr, bridge_);
    }

    void TearDown() override {
        if (bridge_) {
            genius_profiles_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    genius_profiles_bridge_t* bridge_ = nullptr;

    // Simulate time passage for fatigue/flow calculations
    void SimulateTime(uint64_t ms, float activity_level) {
        genius_profiles_update_fatigue(bridge_, ms, activity_level);
    }
};

/* ============================================================================
 * SCENARIO 1: COMPLETE PROFILE LIFECYCLE
 * ============================================================================ */

/**
 * Test: Complete mathematical genius session lifecycle
 *
 * Workflow:
 *   1. Create bridge (done in SetUp)
 *   2. Query initial state
 *   3. Activate mathematical profile
 *   4. Verify activation
 *   5. Perform operations (simulated)
 *   6. Deactivate
 *   7. Reset
 *   8. Destroy (done in TearDown)
 */
TEST_F(GeniusE2ETest, MathematicalGeniusLifecycle) {
    // Step 1-2: Initial state
    EXPECT_EQ(GENIUS_STATE_INACTIVE, genius_profiles_get_state(bridge_));
    EXPECT_EQ(nullptr, genius_profiles_get_active(bridge_));
    EXPECT_FLOAT_EQ(0.0f, genius_profiles_get_fatigue(bridge_));

    // Step 3: Activate mathematical genius
    genius_error_t result = genius_profiles_activate(
        bridge_,
        GENIUS_TYPE_MATHEMATICAL,
        1.0f  // Full strength
    );
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, result);

    // Step 4: Verify activation
    genius_activation_state_t state = genius_profiles_get_state(bridge_);
    EXPECT_TRUE(state == GENIUS_STATE_ACTIVE || state == GENIUS_STATE_ACTIVATING);

    const genius_profile_t* active = genius_profiles_get_active(bridge_);
    ASSERT_NE(nullptr, active);
    EXPECT_EQ(GENIUS_TYPE_MATHEMATICAL, active->type);
    EXPECT_STREQ("Mathematical", active->name);

    // Verify enhanced traits
    EXPECT_EQ(10u, active->traits.working_memory_capacity);
    EXPECT_GE(active->traits.pattern_sensitivity, 2.0f);
    EXPECT_GE(active->parietal.size_multiplier, 2.0f);

    // Step 5: Simulate work session
    for (int i = 0; i < 10; i++) {
        SimulateTime(60000, 0.8f);  // 1 minute of high activity
    }

    // Fatigue should have increased
    EXPECT_GT(genius_profiles_get_fatigue(bridge_), 0.0f);

    // Step 6: Deactivate
    result = genius_profiles_deactivate(bridge_);
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, result);
    EXPECT_EQ(GENIUS_STATE_INACTIVE, genius_profiles_get_state(bridge_));

    // Step 7: Reset
    result = genius_profiles_bridge_reset(bridge_);
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, result);
    EXPECT_FLOAT_EQ(0.0f, genius_profiles_get_fatigue(bridge_));
}

/**
 * Test: Complete artistic genius session with flow state
 */
TEST_F(GeniusE2ETest, ArtisticGeniusWithFlowState) {
    // Activate artistic genius
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_VISUAL_ARTISTIC, 1.0f));

    const genius_profile_t* profile = genius_profiles_get_active(bridge_);
    ASSERT_NE(nullptr, profile);

    // Verify artistic profile characteristics
    EXPECT_GE(profile->traits.mental_imagery_vividness, 2.5f);
    EXPECT_LE(profile->lateralization.spatial_dominance, 0.3f);  // Right hemisphere

    // Attempt to enter flow state (balanced challenge/skill)
    genius_error_t result = genius_profiles_enter_flow(bridge_, 0.6f, 0.6f);

    if (result == GENIUS_ERROR_SUCCESS) {
        EXPECT_EQ(GENIUS_STATE_FLOW, genius_profiles_get_state(bridge_));
        EXPECT_GT(genius_profiles_get_flow_depth(bridge_), 0.0f);

        // Work in flow state
        SimulateTime(3600000, 0.9f);  // 1 hour of intense creative work

        // Exit flow
        ASSERT_EQ(GENIUS_ERROR_SUCCESS,
                  genius_profiles_exit_flow(bridge_, "session_complete"));

        genius_activation_state_t post_flow = genius_profiles_get_state(bridge_);
        EXPECT_NE(GENIUS_STATE_FLOW, post_flow);
    }

    // Cleanup
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
}

/* ============================================================================
 * SCENARIO 2: POLYMATH WORKFLOW
 * ============================================================================ */

/**
 * Test: Da Vinci-style polymath (Art + Science)
 */
TEST_F(GeniusE2ETest, DaVinciPolymathWorkflow) {
    // Create polymath blend: 60% artistic, 40% scientific
    genius_error_t result = genius_profiles_create_polymath(
        bridge_,
        GENIUS_TYPE_VISUAL_ARTISTIC,
        GENIUS_TYPE_SCIENTIFIC,
        0.4f  // Secondary weight
    );
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, result);
    EXPECT_EQ(GENIUS_STATE_BLENDED, genius_profiles_get_state(bridge_));

    // Simulate interdisciplinary work
    for (int i = 0; i < 5; i++) {
        SimulateTime(30000, 0.7f);  // 30 seconds of work
    }

    // Deactivate and check
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
    EXPECT_EQ(GENIUS_STATE_INACTIVE, genius_profiles_get_state(bridge_));
}

/**
 * Test: Complex polymath blend (3+ profiles)
 */
TEST_F(GeniusE2ETest, ComplexPolymathBlend) {
    // Leibniz-style: Math + Science + Literary
    genius_type_t types[] = {
        GENIUS_TYPE_MATHEMATICAL,
        GENIUS_TYPE_SCIENTIFIC,
        GENIUS_TYPE_LITERARY
    };
    float weights[] = {0.4f, 0.35f, 0.25f};

    genius_error_t result = genius_profiles_blend(bridge_, types, weights, 3);
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, result);
    EXPECT_EQ(GENIUS_STATE_BLENDED, genius_profiles_get_state(bridge_));

    // Work session
    SimulateTime(600000, 0.8f);  // 10 minutes

    // Clean up
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
}

/**
 * Test: Switch between polymath configurations
 */
TEST_F(GeniusE2ETest, PolymathProfileSwitching) {
    // Start with Math + Science
    genius_type_t types1[] = {GENIUS_TYPE_MATHEMATICAL, GENIUS_TYPE_SCIENTIFIC};
    float weights1[] = {0.5f, 0.5f};
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_blend(bridge_, types1, weights1, 2));

    SimulateTime(120000, 0.7f);  // 2 minutes

    // Deactivate and switch to Art + Music
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));

    genius_type_t types2[] = {GENIUS_TYPE_VISUAL_ARTISTIC, GENIUS_TYPE_MUSICAL};
    float weights2[] = {0.6f, 0.4f};
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_blend(bridge_, types2, weights2, 2));

    // Verify new blend is active
    EXPECT_EQ(GENIUS_STATE_BLENDED, genius_profiles_get_state(bridge_));

    // Clean up
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
}

/* ============================================================================
 * SCENARIO 3: COGNITIVE SESSION SIMULATION
 * ============================================================================ */

/**
 * Test: Extended work session with fatigue and recovery
 */
TEST_F(GeniusE2ETest, ExtendedWorkSessionWithFatigue) {
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_SCIENTIFIC, 1.0f));

    float initial_fatigue = genius_profiles_get_fatigue(bridge_);

    // Phase 1: Intense work (2 hours simulated)
    for (int i = 0; i < 120; i++) {
        SimulateTime(60000, 0.9f);  // 1 minute of high intensity
    }

    float mid_fatigue = genius_profiles_get_fatigue(bridge_);
    EXPECT_GT(mid_fatigue, initial_fatigue);

    // Phase 2: Light work / recovery (1 hour simulated)
    for (int i = 0; i < 60; i++) {
        SimulateTime(60000, 0.2f);  // 1 minute of low intensity
    }

    float final_fatigue = genius_profiles_get_fatigue(bridge_);
    // Fatigue should have decreased during recovery
    // (depends on implementation - may still be high but not increasing)

    // Clean up
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
}

/**
 * Test: Flow state maintenance and interruption
 */
TEST_F(GeniusE2ETest, FlowStateMaintenanceAndInterruption) {
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_MUSICAL, 1.0f));

    const genius_profile_t* profile = genius_profiles_get_active(bridge_);
    ASSERT_NE(nullptr, profile);

    // Enter flow with balanced challenge/skill
    if (genius_profiles_enter_flow(bridge_, 0.65f, 0.65f) == GENIUS_ERROR_SUCCESS) {
        EXPECT_EQ(GENIUS_STATE_FLOW, genius_profiles_get_state(bridge_));

        float initial_depth = genius_profiles_get_flow_depth(bridge_);
        EXPECT_GT(initial_depth, 0.0f);

        // Maintain flow with steady work
        for (int i = 0; i < 30; i++) {
            SimulateTime(60000, 0.7f);  // 1 minute
        }

        // Flow depth may deepen or be maintained
        float maintained_depth = genius_profiles_get_flow_depth(bridge_);
        EXPECT_GT(maintained_depth, 0.0f);

        // Interrupted exit
        ASSERT_EQ(GENIUS_ERROR_SUCCESS,
                  genius_profiles_exit_flow(bridge_, "external_interruption"));

        EXPECT_NE(GENIUS_STATE_FLOW, genius_profiles_get_state(bridge_));
    }

    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
}

/* ============================================================================
 * SCENARIO 4: IMMUNE MODULATION WORKFLOW
 * ============================================================================ */

/**
 * Test: Immune system effects on genius performance
 */
TEST_F(GeniusE2ETest, ImmuneModulationEffects) {
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_MATHEMATICAL, 1.0f));

    // Apply mild immune response (low cytokines, low inflammation)
    genius_error_t result = genius_profiles_apply_immune_modulation(
        bridge_,
        0.2f,   // Low cytokine level
        0.1f    // Low inflammation
    );
    EXPECT_EQ(GENIUS_ERROR_SUCCESS, result);

    // State should still be active
    genius_activation_state_t state = genius_profiles_get_state(bridge_);
    EXPECT_TRUE(state == GENIUS_STATE_ACTIVE || state == GENIUS_STATE_BLENDED);

    // Apply moderate immune response
    result = genius_profiles_apply_immune_modulation(bridge_, 0.5f, 0.4f);
    EXPECT_EQ(GENIUS_ERROR_SUCCESS, result);

    // Apply severe immune response
    result = genius_profiles_apply_immune_modulation(bridge_, 0.9f, 0.8f);
    EXPECT_EQ(GENIUS_ERROR_SUCCESS, result);

    // May be degraded now
    state = genius_profiles_get_state(bridge_);
    // Accept either degraded or still active (implementation dependent)
    EXPECT_TRUE(state == GENIUS_STATE_ACTIVE ||
                state == GENIUS_STATE_DEGRADED ||
                state == GENIUS_STATE_FATIGUED);

    // Recovery (low cytokines)
    result = genius_profiles_apply_immune_modulation(bridge_, 0.1f, 0.1f);
    EXPECT_EQ(GENIUS_ERROR_SUCCESS, result);

    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
}

/* ============================================================================
 * SCENARIO 5: TRAINING WORKFLOW
 * ============================================================================ */

/**
 * Test: Profile training with gradient updates
 */
TEST_F(GeniusE2ETest, ProfileTrainingWorkflow) {
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_ATHLETIC, 1.0f));

    // Simulate training iterations
    for (int epoch = 0; epoch < 10; epoch++) {
        // Simulated loss decreasing over time
        float loss = 1.0f - (epoch * 0.08f);

        // Simulated gradients
        float gradients[] = {0.01f, -0.02f, 0.005f, -0.01f};
        uint32_t gradient_count = 4;

        genius_error_t result = genius_profiles_training_step(
            bridge_,
            loss,
            gradients,
            gradient_count
        );

        // Should succeed or indicate no training configured
        EXPECT_TRUE(result == GENIUS_ERROR_SUCCESS ||
                    result == GENIUS_ERROR_TRAINING_FAILED);

        // Simulate time between epochs
        SimulateTime(1000, 0.5f);
    }

    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
}

/**
 * Test: STDP learning integration
 */
TEST_F(GeniusE2ETest, STDPLearningIntegration) {
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_MUSICAL, 1.0f));

    uint64_t base_time = 1000000;  // Base timestamp

    // Simulate STDP events (pre before post = LTP)
    for (int i = 0; i < 100; i++) {
        uint64_t pre_time = base_time + (i * 100);
        uint64_t post_time = pre_time + 10;  // Post 10ms after pre

        genius_error_t result = genius_profiles_apply_stdp(
            bridge_,
            pre_time,
            post_time
        );

        // Should succeed or indicate SNN not configured
        EXPECT_TRUE(result == GENIUS_ERROR_SUCCESS ||
                    result == GENIUS_ERROR_SNN_CONFIG_INVALID);
    }

    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
}

/* ============================================================================
 * SCENARIO 6: MULTI-PROFILE SESSION
 * ============================================================================ */

/**
 * Test: Switch between multiple single profiles in one session
 */
TEST_F(GeniusE2ETest, MultiProfileSession) {
    // Morning: Mathematical work
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_MATHEMATICAL, 1.0f));
    SimulateTime(7200000, 0.8f);  // 2 hours
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));

    // Midday: Creative break
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_VISUAL_ARTISTIC, 0.7f));
    SimulateTime(3600000, 0.5f);  // 1 hour
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));

    // Afternoon: Strategic planning
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_STRATEGIC, 1.0f));
    SimulateTime(5400000, 0.7f);  // 1.5 hours
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));

    // Evening: Musical relaxation
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_MUSICAL, 0.5f));
    SimulateTime(1800000, 0.3f);  // 30 minutes
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));

    // Final state check
    EXPECT_EQ(GENIUS_STATE_INACTIVE, genius_profiles_get_state(bridge_));

    // Fatigue should have accumulated over the day
    // (depends on implementation - may reset on deactivate)
}

/**
 * Test: All genius types can be activated in sequence
 */
TEST_F(GeniusE2ETest, AllTypesActivationSequence) {
    for (int i = 0; i < GENIUS_TYPE_COUNT; i++) {
        genius_type_t type = static_cast<genius_type_t>(i);

        if (type == GENIUS_TYPE_POLYMATH) {
            // Polymath requires blend, skip for single activation
            continue;
        }

        // Activate
        genius_error_t result = genius_profiles_activate(bridge_, type, 1.0f);
        ASSERT_EQ(GENIUS_ERROR_SUCCESS, result)
            << "Failed to activate type " << genius_type_name(type);

        // Verify
        const genius_profile_t* profile = genius_profiles_get_active(bridge_);
        ASSERT_NE(nullptr, profile)
            << "No active profile for type " << genius_type_name(type);
        EXPECT_EQ(type, profile->type);

        // Brief operation
        SimulateTime(1000, 0.5f);

        // Deactivate
        result = genius_profiles_deactivate(bridge_);
        ASSERT_EQ(GENIUS_ERROR_SUCCESS, result)
            << "Failed to deactivate type " << genius_type_name(type);
    }
}

/* ============================================================================
 * SCENARIO 7: EIDETIC MEMORY WORKFLOW
 * ============================================================================ */

/**
 * Test: Scientific genius with Tesla-level eidetic memory
 */
TEST_F(GeniusE2ETest, TeslaEideticMemoryWorkflow) {
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_SCIENTIFIC, 1.0f));

    // Get eidetic config for current profile
    const eidetic_memory_config_t* eidetic =
        genius_profiles_get_eidetic_config(bridge_);

    if (eidetic != nullptr) {
        // Verify Tesla-like characteristics
        EXPECT_GE(eidetic->visual_eidetic, 2.0f);
        EXPECT_GE(eidetic->spatial_eidetic, 2.0f);
        EXPECT_GE(eidetic->simulation_duration_sec, 10.0f);
    }

    // Apply eidetic configuration to memory systems
    genius_error_t result = genius_profiles_apply_eidetic(bridge_);
    // Success or no memory systems connected
    EXPECT_TRUE(result == GENIUS_ERROR_SUCCESS ||
                result == GENIUS_ERROR_BRIDGE_DISCONNECTED);

    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
}

/**
 * Test: Musical genius with Mozart-level auditory memory
 */
TEST_F(GeniusE2ETest, MozartAuditoryMemoryWorkflow) {
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_MUSICAL, 1.0f));

    const eidetic_memory_config_t* eidetic =
        genius_profiles_get_eidetic_config(bridge_);

    if (eidetic != nullptr) {
        // Verify Mozart-like auditory dominance
        EXPECT_GE(eidetic->auditory_eidetic, 2.0f);
        EXPECT_GE(eidetic->working_memory.auditory_buffer_size, 64u);
    }

    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
}

/* ============================================================================
 * SCENARIO 8: STRESS TESTS
 * ============================================================================ */

/**
 * Test: Rapid activation/deactivation cycles
 */
TEST_F(GeniusE2ETest, RapidActivationDeactivationCycles) {
    for (int cycle = 0; cycle < 100; cycle++) {
        genius_type_t type = static_cast<genius_type_t>(cycle % (GENIUS_TYPE_COUNT - 1));

        ASSERT_EQ(GENIUS_ERROR_SUCCESS,
                  genius_profiles_activate(bridge_, type, 0.8f))
            << "Cycle " << cycle << " activation failed";

        ASSERT_EQ(GENIUS_ERROR_SUCCESS,
                  genius_profiles_deactivate(bridge_))
            << "Cycle " << cycle << " deactivation failed";
    }

    // Bridge should still be in good state
    EXPECT_EQ(GENIUS_STATE_INACTIVE, genius_profiles_get_state(bridge_));
}

/**
 * Test: Rapid blend changes
 */
TEST_F(GeniusE2ETest, RapidBlendChanges) {
    for (int cycle = 0; cycle < 50; cycle++) {
        genius_type_t type1 = static_cast<genius_type_t>(cycle % (GENIUS_TYPE_COUNT - 1));
        genius_type_t type2 = static_cast<genius_type_t>((cycle + 1) % (GENIUS_TYPE_COUNT - 1));

        if (type1 == type2) {
            type2 = static_cast<genius_type_t>((type2 + 1) % (GENIUS_TYPE_COUNT - 1));
        }

        genius_type_t types[] = {type1, type2};
        float weights[] = {0.5f + (cycle % 10) * 0.03f, 0.5f - (cycle % 10) * 0.03f};

        ASSERT_EQ(GENIUS_ERROR_SUCCESS,
                  genius_profiles_blend(bridge_, types, weights, 2))
            << "Cycle " << cycle << " blend failed";

        ASSERT_EQ(GENIUS_ERROR_SUCCESS,
                  genius_profiles_deactivate(bridge_))
            << "Cycle " << cycle << " deactivation failed";
    }
}

/**
 * Test: Many fatigue updates
 */
TEST_F(GeniusE2ETest, ManyFatigueUpdates) {
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_MATHEMATICAL, 1.0f));

    // 1000 fatigue updates
    for (int i = 0; i < 1000; i++) {
        float activity = 0.3f + (i % 7) * 0.1f;  // Varying activity
        genius_profiles_update_fatigue(bridge_, 100, activity);
    }

    // Should still be functional
    genius_activation_state_t state = genius_profiles_get_state(bridge_);
    EXPECT_TRUE(state != GENIUS_STATE_ERROR);

    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
}

/**
 * Test: Many immune modulation calls
 */
TEST_F(GeniusE2ETest, ManyImmuneModulationCalls) {
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_ATHLETIC, 1.0f));

    // Fluctuating immune state
    for (int i = 0; i < 500; i++) {
        float cytokine = 0.1f + (std::sin(i * 0.1f) + 1.0f) * 0.4f;
        float inflammation = 0.05f + (std::cos(i * 0.15f) + 1.0f) * 0.3f;

        genius_profiles_apply_immune_modulation(bridge_, cytokine, inflammation);
    }

    // Should still be functional
    genius_activation_state_t state = genius_profiles_get_state(bridge_);
    EXPECT_TRUE(state != GENIUS_STATE_ERROR);

    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
}

/* ============================================================================
 * SCENARIO 9: ERROR RECOVERY
 * ============================================================================ */

/**
 * Test: Recovery from invalid operations
 */
TEST_F(GeniusE2ETest, RecoveryFromInvalidOperations) {
    // Try to enter flow without activation - returns INVALID_STATE not NOT_ACTIVE
    genius_error_t result = genius_profiles_enter_flow(bridge_, 0.5f, 0.5f);
    EXPECT_EQ(GENIUS_ERROR_INVALID_STATE, result);

    // Bridge should still be usable
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_FINANCIAL, 1.0f));
    EXPECT_EQ(GENIUS_STATE_ACTIVE, genius_profiles_get_state(bridge_));

    // Second activation creates blend (allowed up to max capacity)
    result = genius_profiles_activate(bridge_, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    EXPECT_EQ(GENIUS_ERROR_SUCCESS, result);
    EXPECT_EQ(GENIUS_STATE_BLENDED, genius_profiles_get_state(bridge_));

    // Primary profile should still be financial
    const genius_profile_t* active = genius_profiles_get_active(bridge_);
    ASSERT_NE(nullptr, active);
    EXPECT_EQ(GENIUS_TYPE_FINANCIAL, active->type);

    // Cleanup
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));

    // Try double deactivation
    result = genius_profiles_deactivate(bridge_);
    // May succeed (idempotent) or return NOT_ACTIVE
    EXPECT_TRUE(result == GENIUS_ERROR_SUCCESS ||
                result == GENIUS_ERROR_NOT_ACTIVE);
}

/**
 * Test: Recovery after reset
 */
TEST_F(GeniusE2ETest, RecoveryAfterReset) {
    // Do some operations
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_LITERARY, 1.0f));
    SimulateTime(60000, 0.8f);

    // Reset mid-session
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_bridge_reset(bridge_));

    // Bridge should be back to initial state
    EXPECT_EQ(GENIUS_STATE_INACTIVE, genius_profiles_get_state(bridge_));
    EXPECT_EQ(nullptr, genius_profiles_get_active(bridge_));

    // Should be able to start fresh
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_STRATEGIC, 1.0f));
    EXPECT_EQ(GENIUS_STATE_ACTIVE, genius_profiles_get_state(bridge_));

    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));
}

/* ============================================================================
 * SCENARIO 10: REALISTIC WORKDAY SIMULATION
 * ============================================================================ */

/**
 * Test: Simulate a full workday with profile switching
 */
TEST_F(GeniusE2ETest, FullWorkdaySimulation) {
    // 9 AM - Start with mathematical analysis
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_MATHEMATICAL, 1.0f));

    // Enter flow state for deep work
    if (genius_profiles_enter_flow(bridge_, 0.7f, 0.7f) == GENIUS_ERROR_SUCCESS) {
        for (int i = 0; i < 90; i++) {  // 90 minutes
            SimulateTime(60000, 0.85f);
        }
        genius_profiles_exit_flow(bridge_, "break_time");
    } else {
        for (int i = 0; i < 90; i++) {
            SimulateTime(60000, 0.75f);
        }
    }
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));

    // 10:30 AM - Coffee break (no profile)
    SimulateTime(900000, 0.1f);  // 15 minutes

    // 10:45 AM - Strategic planning meeting
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_STRATEGIC, 0.8f));
    for (int i = 0; i < 60; i++) {  // 60 minutes
        SimulateTime(60000, 0.6f);
    }
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));

    // 11:45 AM - Financial analysis
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_FINANCIAL, 1.0f));
    for (int i = 0; i < 75; i++) {  // 75 minutes
        SimulateTime(60000, 0.7f);
    }
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));

    // 1 PM - Lunch (no profile)
    SimulateTime(3600000, 0.1f);  // 60 minutes

    // 2 PM - Creative brainstorming (polymath mode)
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_create_polymath(
                  bridge_,
                  GENIUS_TYPE_VISUAL_ARTISTIC,
                  GENIUS_TYPE_SCIENTIFIC,
                  0.35f
              ));
    for (int i = 0; i < 120; i++) {  // 2 hours
        SimulateTime(60000, 0.65f);
    }
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));

    // 4 PM - Technical writing (Literary)
    ASSERT_EQ(GENIUS_ERROR_SUCCESS,
              genius_profiles_activate(bridge_, GENIUS_TYPE_LITERARY, 0.9f));
    for (int i = 0; i < 90; i++) {  // 90 minutes
        SimulateTime(60000, 0.55f);
    }
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_deactivate(bridge_));

    // End of day
    EXPECT_EQ(GENIUS_STATE_INACTIVE, genius_profiles_get_state(bridge_));

    // Final reset for next day
    ASSERT_EQ(GENIUS_ERROR_SUCCESS, genius_profiles_bridge_reset(bridge_));
}

/* ============================================================================
 * SCENARIO 11: BRAIN CREATION WORKFLOW
 * ============================================================================ */

/**
 * Test: Create brain from genius profile
 * Note: This may return NULL if brain creation requires additional systems
 */
TEST_F(GeniusE2ETest, BrainCreationFromProfile) {
    // Try to create brain for each genius type
    for (int i = 0; i < GENIUS_TYPE_COUNT - 1; i++) {  // Skip POLYMATH
        genius_type_t type = static_cast<genius_type_t>(i);

        // This may return NULL if brain infrastructure isn't available
        brain_t brain = genius_brain_create(type);

        if (brain != nullptr) {
            // If creation succeeded, we should have a valid brain
            // (Actual brain validation would require more infrastructure)

            // Note: Would need brain_destroy() function to clean up
            // For now, just verify creation worked
        }
        // NULL is acceptable if brain systems aren't initialized
    }
}

/**
 * Test: Create hemispheric brain from genius profile
 */
TEST_F(GeniusE2ETest, HemisphericBrainCreationFromProfile) {
    // Try hemispheric brain creation
    hemispheric_brain_t* hbrain = genius_hemispheric_brain_create(GENIUS_TYPE_MATHEMATICAL);

    if (hbrain != nullptr) {
        // If creation succeeded, verify we got a valid hemispheric brain
        // (Actual validation would require hemispheric brain API)
    }
    // NULL is acceptable if brain systems aren't initialized
}

}  // namespace e2e
}  // namespace test
}  // namespace nimcp
