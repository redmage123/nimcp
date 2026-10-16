/**
 * @file test_brain_medulla_integration.cpp
 * @brief Unit tests for brain-medulla subsystem integration
 *
 * Tests the integration between the brain factory and the medulla oblongata,
 * including initialization, updates, and state query functions.
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain/factory/init/nimcp_brain_init_medulla.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/medulla/nimcp_medulla.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainMedullaIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create a minimal brain for testing
        brain = static_cast<brain_t>(calloc(1, sizeof(struct brain_struct)));
        ASSERT_NE(brain, nullptr);

        // Initialize basic fields
        brain->current_time_us = 0;
        brain->bio_async_enabled = false;

        // Initialize medulla fields
        brain->medulla = nullptr;
        brain->medulla_enabled = false;
        brain->last_medulla_update_us = 0;
    }

    void TearDown() override {
        if (brain) {
            // Clean up medulla if initialized
            nimcp_brain_destroy_medulla_subsystem(brain);
            free(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(BrainMedullaIntegrationTest, InitWithNullBrain) {
    // Should handle null gracefully
    bool result = nimcp_brain_factory_init_medulla_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainMedullaIntegrationTest, InitMedullaSucceeds) {
    bool result = nimcp_brain_factory_init_medulla_subsystem(brain);
    EXPECT_TRUE(result);

    // Verify medulla was created and enabled
    EXPECT_NE(brain->medulla, nullptr);
    EXPECT_TRUE(brain->medulla_enabled);
    EXPECT_EQ(brain->last_medulla_update_us, brain->current_time_us);
}

TEST_F(BrainMedullaIntegrationTest, InitMedullaWithBioAsyncEnabled) {
    brain->bio_async_enabled = true;

    bool result = nimcp_brain_factory_init_medulla_subsystem(brain);
    EXPECT_TRUE(result);

    // Medulla should be created even with bio-async enabled
    EXPECT_NE(brain->medulla, nullptr);
    EXPECT_TRUE(brain->medulla_enabled);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(BrainMedullaIntegrationTest, UpdateWithNullBrain) {
    int result = nimcp_brain_update_medulla_subsystem(nullptr, 0.016f);
    EXPECT_NE(result, 0);  // Should return error code
}

TEST_F(BrainMedullaIntegrationTest, UpdateWithDisabledMedulla) {
    // Don't initialize medulla
    int result = nimcp_brain_update_medulla_subsystem(brain, 0.016f);
    EXPECT_EQ(result, 0);  // Should succeed silently
}

TEST_F(BrainMedullaIntegrationTest, UpdateSucceeds) {
    // Initialize medulla first
    ASSERT_TRUE(nimcp_brain_factory_init_medulla_subsystem(brain));

    // Update with 16ms delta (60 FPS equivalent)
    int result = nimcp_brain_update_medulla_subsystem(brain, 0.016f);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainMedullaIntegrationTest, MultipleUpdates) {
    ASSERT_TRUE(nimcp_brain_factory_init_medulla_subsystem(brain));

    // Perform multiple updates
    for (int i = 0; i < 100; i++) {
        brain->current_time_us += 16000;  // 16ms
        int result = nimcp_brain_update_medulla_subsystem(brain, 0.016f);
        EXPECT_EQ(result, 0);
    }
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(BrainMedullaIntegrationTest, GetArousalLevelWithNullBrain) {
    float arousal = nimcp_brain_get_arousal_level(nullptr);
    EXPECT_FLOAT_EQ(arousal, 0.5f);  // Default neutral
}

TEST_F(BrainMedullaIntegrationTest, GetArousalLevelWithDisabledMedulla) {
    float arousal = nimcp_brain_get_arousal_level(brain);
    EXPECT_FLOAT_EQ(arousal, 0.5f);  // Default neutral
}

TEST_F(BrainMedullaIntegrationTest, GetArousalLevelWithMedulla) {
    ASSERT_TRUE(nimcp_brain_factory_init_medulla_subsystem(brain));

    float arousal = nimcp_brain_get_arousal_level(brain);
    // Should be near baseline (0.5)
    EXPECT_GE(arousal, 0.1f);
    EXPECT_LE(arousal, 0.95f);
}

TEST_F(BrainMedullaIntegrationTest, GetCircadianPhaseWithNullBrain) {
    circadian_phase_t phase = nimcp_brain_get_circadian_phase(nullptr);
    EXPECT_EQ(phase, CIRCADIAN_PHASE_MORNING);  // Default
}

TEST_F(BrainMedullaIntegrationTest, GetCircadianPhaseWithDisabledMedulla) {
    circadian_phase_t phase = nimcp_brain_get_circadian_phase(brain);
    EXPECT_EQ(phase, CIRCADIAN_PHASE_MORNING);  // Default
}

TEST_F(BrainMedullaIntegrationTest, GetCircadianPhaseWithMedulla) {
    ASSERT_TRUE(nimcp_brain_factory_init_medulla_subsystem(brain));

    circadian_phase_t phase = nimcp_brain_get_circadian_phase(brain);
    // Should be a valid phase
    EXPECT_GE((int)phase, (int)CIRCADIAN_PHASE_EARLY_MORNING);
    EXPECT_LE((int)phase, (int)CIRCADIAN_PHASE_PRE_DAWN);
}

TEST_F(BrainMedullaIntegrationTest, GetProtectionLevelWithNullBrain) {
    protection_level_t level = nimcp_brain_get_protection_level(nullptr);
    EXPECT_EQ(level, PROTECTION_LEVEL_NORMAL);  // Default
}

TEST_F(BrainMedullaIntegrationTest, GetProtectionLevelWithDisabledMedulla) {
    protection_level_t level = nimcp_brain_get_protection_level(brain);
    EXPECT_EQ(level, PROTECTION_LEVEL_NORMAL);  // Default
}

TEST_F(BrainMedullaIntegrationTest, GetProtectionLevelWithMedulla) {
    ASSERT_TRUE(nimcp_brain_factory_init_medulla_subsystem(brain));

    protection_level_t level = nimcp_brain_get_protection_level(brain);
    // Should start at normal
    EXPECT_EQ(level, PROTECTION_LEVEL_NORMAL);
}

TEST_F(BrainMedullaIntegrationTest, IsMedullaEmergencyWithNullBrain) {
    bool emergency = nimcp_brain_is_medulla_emergency(nullptr);
    EXPECT_FALSE(emergency);
}

TEST_F(BrainMedullaIntegrationTest, IsMedullaEmergencyWithDisabledMedulla) {
    bool emergency = nimcp_brain_is_medulla_emergency(brain);
    EXPECT_FALSE(emergency);
}

TEST_F(BrainMedullaIntegrationTest, IsMedullaEmergencyWithMedulla) {
    ASSERT_TRUE(nimcp_brain_factory_init_medulla_subsystem(brain));

    bool emergency = nimcp_brain_is_medulla_emergency(brain);
    EXPECT_FALSE(emergency);  // Should not be in emergency by default
}

//=============================================================================
// Control Tests
//=============================================================================

TEST_F(BrainMedullaIntegrationTest, TriggerEmergencyWithNullBrain) {
    int result = nimcp_brain_trigger_emergency(nullptr, "test");
    EXPECT_NE(result, 0);  // Should fail
}

TEST_F(BrainMedullaIntegrationTest, TriggerEmergencyWithDisabledMedulla) {
    int result = nimcp_brain_trigger_emergency(brain, "test");
    EXPECT_NE(result, 0);  // Should fail
}

TEST_F(BrainMedullaIntegrationTest, TriggerEmergencyWithMedulla) {
    ASSERT_TRUE(nimcp_brain_factory_init_medulla_subsystem(brain));

    int result = nimcp_brain_trigger_emergency(brain, "test emergency");
    EXPECT_EQ(result, 0);

    // Should now be in emergency
    bool emergency = nimcp_brain_is_medulla_emergency(brain);
    EXPECT_TRUE(emergency);
}

TEST_F(BrainMedullaIntegrationTest, RequestStateChangeWithNullBrain) {
    int result = nimcp_brain_request_medulla_state(nullptr, MEDULLA_STATE_RUNNING);
    EXPECT_NE(result, 0);  // Should fail
}

TEST_F(BrainMedullaIntegrationTest, RequestStateChangeWithDisabledMedulla) {
    int result = nimcp_brain_request_medulla_state(brain, MEDULLA_STATE_RUNNING);
    EXPECT_NE(result, 0);  // Should fail
}

//=============================================================================
// Destruction Tests
//=============================================================================

TEST_F(BrainMedullaIntegrationTest, DestroyWithNullBrain) {
    // Should not crash
    nimcp_brain_destroy_medulla_subsystem(nullptr);
}

TEST_F(BrainMedullaIntegrationTest, DestroyWithDisabledMedulla) {
    // Should not crash
    nimcp_brain_destroy_medulla_subsystem(brain);
    EXPECT_FALSE(brain->medulla_enabled);
    EXPECT_EQ(brain->medulla, nullptr);
}

TEST_F(BrainMedullaIntegrationTest, DestroyWithMedulla) {
    ASSERT_TRUE(nimcp_brain_factory_init_medulla_subsystem(brain));
    EXPECT_NE(brain->medulla, nullptr);

    nimcp_brain_destroy_medulla_subsystem(brain);

    EXPECT_EQ(brain->medulla, nullptr);
    EXPECT_FALSE(brain->medulla_enabled);
    EXPECT_EQ(brain->last_medulla_update_us, 0);
}

TEST_F(BrainMedullaIntegrationTest, DoubleDestroy) {
    ASSERT_TRUE(nimcp_brain_factory_init_medulla_subsystem(brain));

    nimcp_brain_destroy_medulla_subsystem(brain);
    nimcp_brain_destroy_medulla_subsystem(brain);  // Should not crash

    EXPECT_EQ(brain->medulla, nullptr);
    EXPECT_FALSE(brain->medulla_enabled);
}

//=============================================================================
// Integration Lifecycle Tests
//=============================================================================

TEST_F(BrainMedullaIntegrationTest, FullLifecycle) {
    // Initialize
    ASSERT_TRUE(nimcp_brain_factory_init_medulla_subsystem(brain));
    EXPECT_NE(brain->medulla, nullptr);
    EXPECT_TRUE(brain->medulla_enabled);

    // Run some updates
    for (int i = 0; i < 50; i++) {
        brain->current_time_us += 50000;  // 50ms
        int result = nimcp_brain_update_medulla_subsystem(brain, 0.05f);
        EXPECT_EQ(result, 0);
    }

    // Query states
    float arousal = nimcp_brain_get_arousal_level(brain);
    EXPECT_GE(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);

    protection_level_t protection = nimcp_brain_get_protection_level(brain);
    EXPECT_GE((int)protection, 0);

    circadian_phase_t phase = nimcp_brain_get_circadian_phase(brain);
    EXPECT_GE((int)phase, 0);

    // Destroy
    nimcp_brain_destroy_medulla_subsystem(brain);
    EXPECT_EQ(brain->medulla, nullptr);
}

TEST_F(BrainMedullaIntegrationTest, ReinitializeAfterDestroy) {
    // First initialization
    ASSERT_TRUE(nimcp_brain_factory_init_medulla_subsystem(brain));
    EXPECT_NE(brain->medulla, nullptr);
    EXPECT_TRUE(brain->medulla_enabled);

    // Destroy
    nimcp_brain_destroy_medulla_subsystem(brain);
    EXPECT_EQ(brain->medulla, nullptr);
    EXPECT_FALSE(brain->medulla_enabled);

    // Reinitialize
    ASSERT_TRUE(nimcp_brain_factory_init_medulla_subsystem(brain));
    EXPECT_NE(brain->medulla, nullptr);
    EXPECT_TRUE(brain->medulla_enabled);
    // Note: New allocation may reuse same memory address, so we don't check address difference
}
