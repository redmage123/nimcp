//=============================================================================
// test_brain_init_thalamus.cpp - Brain Thalamus Init Unit Tests
//=============================================================================
/**
 * @file test_brain_init_thalamus.cpp
 * @brief Unit tests for brain factory thalamus initialization
 *
 * WHAT: Tests for thalamus subsystem initialization and brain-level API
 * WHY:  Ensure proper thalamus integration with brain factory
 * HOW:  GTest framework testing lifecycle, relay, and configuration
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/factory/init/nimcp_brain_init_thalamus.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Test fixture for brain thalamus initialization
 */
class BrainThalamusInitTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create a minimal brain for testing
        brain = brain_create("test_thalamus", BRAIN_SIZE_MICRO,
                             BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

/**
 * @brief Test fixture for thalamus with full initialization
 */
class BrainThalamusFullTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create a small brain with subsystems enabled
        brain_config_t config = {};
        config.size = BRAIN_SIZE_TINY;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 32;
        config.num_outputs = 8;
        config.minimal_mode = false;

        brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST(BrainThalamusConfigTest, DefaultConfigNullBrain) {
    thalamus_config_t config;
    memset(&config, 0xFF, sizeof(config));  // Fill with garbage

    nimcp_brain_thal_default_config(nullptr, &config);

    // Should still get default config values
    EXPECT_TRUE(config.enable_trn);
    EXPECT_TRUE(config.enable_mode_switching);
}

TEST(BrainThalamusConfigTest, DefaultConfigNullConfig) {
    // Should not crash with NULL config
    nimcp_brain_thal_default_config(nullptr, nullptr);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(BrainThalamusInitTest, InitSubsystem) {
    // Initialize thalamus subsystem
    bool result = nimcp_brain_factory_init_thalamus_subsystem(brain);
    EXPECT_TRUE(result);

    // Check that thalamus is enabled
    EXPECT_TRUE(nimcp_brain_thal_is_enabled(brain));
}

TEST_F(BrainThalamusInitTest, InitSubsystemTwice) {
    // First init should succeed
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    // Second init should also succeed (returns true for already initialized)
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    EXPECT_TRUE(nimcp_brain_thal_is_enabled(brain));
}

TEST(BrainThalamusInitTest, InitNullBrain) {
    EXPECT_FALSE(nimcp_brain_factory_init_thalamus_subsystem(nullptr));
}

TEST_F(BrainThalamusInitTest, Destroy) {
    // Initialize first
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_thal_is_enabled(brain));

    // Destroy
    nimcp_brain_thal_destroy(brain);
    EXPECT_FALSE(nimcp_brain_thal_is_enabled(brain));
}

TEST(BrainThalamusInitTest, DestroyNullBrain) {
    // Should not crash
    nimcp_brain_thal_destroy(nullptr);
}

//=============================================================================
// Step Tests
//=============================================================================

TEST_F(BrainThalamusInitTest, StepNotInitialized) {
    // Step without init should succeed (silently skips)
    int result = nimcp_brain_thal_step(brain, 1.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainThalamusInitTest, StepAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    // Step should succeed
    int result = nimcp_brain_thal_step(brain, 1.0f);
    EXPECT_EQ(result, 0);
}

TEST(BrainThalamusInitTest, StepNullBrain) {
    EXPECT_EQ(nimcp_brain_thal_step(nullptr, 1.0f), -1);
}

//=============================================================================
// Relay Tests
//=============================================================================

TEST_F(BrainThalamusInitTest, RelayVisualNotInitialized) {
    float input[16] = {0.5f};
    float output[16] = {0.0f};

    // Without thalamus init, should pass through directly
    int result = nimcp_brain_thal_relay_visual(brain, input, 16, output, 16);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(output[0], 0.5f);  // Pass-through
}

TEST_F(BrainThalamusInitTest, RelayVisualAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    float input[16];
    float output[16];

    // Fill input with test values
    for (int i = 0; i < 16; i++) {
        input[i] = (float)i / 16.0f;
    }
    memset(output, 0, sizeof(output));

    int result = nimcp_brain_thal_relay_visual(brain, input, 16, output, 16);
    EXPECT_EQ(result, 0);

    // Output should be modulated but non-zero
    bool has_output = false;
    for (int i = 0; i < 16; i++) {
        if (output[i] > 0.0f) has_output = true;
    }
    EXPECT_TRUE(has_output);
}

TEST_F(BrainThalamusInitTest, RelayMotorAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    float input[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float output[8] = {0.0f};

    int result = nimcp_brain_thal_relay_motor(brain, input, 8, output, 8);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainThalamusInitTest, RelayExecutiveAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    float input[16] = {0.5f};
    float output[16] = {0.0f};

    int result = nimcp_brain_thal_relay_executive(brain, input, 16, output, 16);
    EXPECT_EQ(result, 0);
}

TEST(BrainThalamusRelayTest, RelayNullParameters) {
    EXPECT_EQ(nimcp_brain_thal_relay_visual(nullptr, nullptr, 0, nullptr, 0), -1);
}

//=============================================================================
// Arousal Tests
//=============================================================================

TEST_F(BrainThalamusInitTest, SetArousalNotInitialized) {
    int result = nimcp_brain_thal_set_arousal(brain, 0.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainThalamusInitTest, SetArousalAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    int result = nimcp_brain_thal_set_arousal(brain, 0.75f);
    EXPECT_EQ(result, 0);

    float arousal = nimcp_brain_thal_get_arousal(brain);
    EXPECT_NEAR(arousal, 0.75f, 0.1f);  // Allow some tolerance
}

TEST_F(BrainThalamusInitTest, GetArousalNotInitialized) {
    float arousal = nimcp_brain_thal_get_arousal(brain);
    EXPECT_LT(arousal, 0.0f);  // Should return -1 for error
}

TEST(BrainThalamusArousalTest, SetArousalNullBrain) {
    EXPECT_EQ(nimcp_brain_thal_set_arousal(nullptr, 0.5f), -1);
}

//=============================================================================
// Attention Tests
//=============================================================================

TEST_F(BrainThalamusInitTest, SetAttentionAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    int result = nimcp_brain_thal_set_attention(brain, 0.8f);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainThalamusInitTest, SetNucleusAttentionAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    int result = nimcp_brain_thal_set_nucleus_attention(brain, THAL_NUCLEUS_LGN, 0.9f);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Firing Mode Tests
//=============================================================================

TEST_F(BrainThalamusInitTest, SetModeAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    int result = nimcp_brain_thal_set_mode(brain, THAL_NUCLEUS_LGN, THAL_MODE_BURST);
    EXPECT_EQ(result, 0);

    thal_firing_mode_t mode = nimcp_brain_thal_get_mode(brain, THAL_NUCLEUS_LGN);
    EXPECT_EQ(mode, THAL_MODE_BURST);
}

TEST_F(BrainThalamusInitTest, GetModeNotInitialized) {
    thal_firing_mode_t mode = nimcp_brain_thal_get_mode(brain, THAL_NUCLEUS_LGN);
    EXPECT_EQ(mode, THAL_MODE_TONIC);  // Default
}

TEST_F(BrainThalamusInitTest, TriggerBurstAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    int result = nimcp_brain_thal_trigger_burst(brain, THAL_NUCLEUS_MGN);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// TRN Inhibition Tests
//=============================================================================

TEST_F(BrainThalamusInitTest, ApplyTrnInhibitionAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    int result = nimcp_brain_thal_apply_trn_inhibition(brain, THAL_NUCLEUS_VA, 0.5f);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Callback Tests
//=============================================================================

TEST_F(BrainThalamusInitTest, OnArousalChangeAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    // Should not crash
    nimcp_brain_thal_on_arousal_change(brain, 0.2f);

    // Low arousal should trigger burst mode
    thal_firing_mode_t mode = nimcp_brain_thal_get_mode(brain, THAL_NUCLEUS_LGN);
    EXPECT_EQ(mode, THAL_MODE_BURST);
}

TEST_F(BrainThalamusInitTest, OnArousalChangeHighArousal) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    // Set to burst mode first
    nimcp_brain_thal_set_mode(brain, THAL_NUCLEUS_LGN, THAL_MODE_BURST);

    // High arousal should trigger tonic mode
    nimcp_brain_thal_on_arousal_change(brain, 0.9f);

    thal_firing_mode_t mode = nimcp_brain_thal_get_mode(brain, THAL_NUCLEUS_LGN);
    EXPECT_EQ(mode, THAL_MODE_TONIC);
}

TEST_F(BrainThalamusInitTest, OnSleepWakeChange) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    // Wake transition
    nimcp_brain_thal_on_sleep_wake_change(brain, true);
    EXPECT_EQ(nimcp_brain_thal_get_mode(brain, THAL_NUCLEUS_LGN), THAL_MODE_TONIC);

    // Sleep transition
    nimcp_brain_thal_on_sleep_wake_change(brain, false);
    EXPECT_EQ(nimcp_brain_thal_get_mode(brain, THAL_NUCLEUS_LGN), THAL_MODE_BURST);
}

TEST(BrainThalamusCallbackTest, OnArousalChangeNullBrain) {
    // Should not crash
    nimcp_brain_thal_on_arousal_change(nullptr, 0.5f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(BrainThalamusInitTest, GetStatsAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    thalamus_stats_t stats;
    int result = nimcp_brain_thal_get_stats(brain, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainThalamusInitTest, GetStatsNotInitialized) {
    thalamus_stats_t stats;
    int result = nimcp_brain_thal_get_stats(brain, &stats);
    EXPECT_EQ(result, -1);
}

TEST(BrainThalamusStatsTest, GetStatsNullParams) {
    EXPECT_EQ(nimcp_brain_thal_get_stats(nullptr, nullptr), -1);
}

//=============================================================================
// Direct Access Tests
//=============================================================================

TEST_F(BrainThalamusInitTest, GetHandleAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    thalamus_t* thal = nimcp_brain_thal_get_handle(brain);
    EXPECT_NE(thal, nullptr);
}

TEST_F(BrainThalamusInitTest, GetHandleNotInitialized) {
    thalamus_t* thal = nimcp_brain_thal_get_handle(brain);
    EXPECT_EQ(thal, nullptr);
}

TEST_F(BrainThalamusInitTest, GetNucleusAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    thal_nucleus_t* lgn = nimcp_brain_thal_get_nucleus(brain, THAL_NUCLEUS_LGN);
    EXPECT_NE(lgn, nullptr);

    thal_nucleus_t* mgn = nimcp_brain_thal_get_nucleus(brain, THAL_NUCLEUS_MGN);
    EXPECT_NE(mgn, nullptr);

    thal_nucleus_t* pulvinar = nimcp_brain_thal_get_nucleus(brain, THAL_NUCLEUS_PULVINAR);
    EXPECT_NE(pulvinar, nullptr);
}

//=============================================================================
// Query Function Tests
//=============================================================================

TEST_F(BrainThalamusInitTest, GetFiringRateAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    float rate = nimcp_brain_thal_get_firing_rate(brain, THAL_NUCLEUS_LGN);
    EXPECT_GE(rate, 0.0f);
}

TEST_F(BrainThalamusInitTest, GetTonicFractionAfterInit) {
    EXPECT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    float fraction = nimcp_brain_thal_get_tonic_fraction(brain, THAL_NUCLEUS_LGN);
    EXPECT_GE(fraction, 0.0f);
    EXPECT_LE(fraction, 1.0f);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(BrainThalamusFullTest, FullWorkflow) {
    // Initialize thalamus
    ASSERT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_thal_is_enabled(brain));

    // Set arousal high (awake)
    EXPECT_EQ(nimcp_brain_thal_set_arousal(brain, 0.9f), 0);

    // Set attention
    EXPECT_EQ(nimcp_brain_thal_set_attention(brain, 0.8f), 0);

    // Relay some signals
    float visual_input[32];
    float visual_output[32];
    for (int i = 0; i < 32; i++) visual_input[i] = (float)i / 32.0f;

    EXPECT_EQ(nimcp_brain_thal_relay_visual(brain, visual_input, 32, visual_output, 32), 0);

    // Step the system
    EXPECT_EQ(nimcp_brain_thal_step(brain, 10.0f), 0);

    // Get stats
    thalamus_stats_t stats;
    EXPECT_EQ(nimcp_brain_thal_get_stats(brain, &stats), 0);
    EXPECT_GT(stats.total_signals_relayed, (uint64_t)0);

    // Cleanup is automatic via TearDown
}

TEST_F(BrainThalamusFullTest, ArousalModeTransitions) {
    ASSERT_TRUE(nimcp_brain_factory_init_thalamus_subsystem(brain));

    // Start with high arousal (tonic mode)
    nimcp_brain_thal_on_arousal_change(brain, 0.9f);
    EXPECT_EQ(nimcp_brain_thal_get_mode(brain, THAL_NUCLEUS_LGN), THAL_MODE_TONIC);
    EXPECT_EQ(nimcp_brain_thal_get_mode(brain, THAL_NUCLEUS_MGN), THAL_MODE_TONIC);
    EXPECT_EQ(nimcp_brain_thal_get_mode(brain, THAL_NUCLEUS_MD), THAL_MODE_TONIC);

    // Drop to low arousal (burst mode)
    nimcp_brain_thal_on_arousal_change(brain, 0.1f);
    EXPECT_EQ(nimcp_brain_thal_get_mode(brain, THAL_NUCLEUS_LGN), THAL_MODE_BURST);
    EXPECT_EQ(nimcp_brain_thal_get_mode(brain, THAL_NUCLEUS_MGN), THAL_MODE_BURST);
    EXPECT_EQ(nimcp_brain_thal_get_mode(brain, THAL_NUCLEUS_MD), THAL_MODE_BURST);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
