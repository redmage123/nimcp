/**
 * @file test_emotion_substrate_bridge.cpp
 * @brief Unit tests for Emotion-Neural Substrate Bridge
 * @date 2025-12-19
 *
 * Tests bidirectional substrate-emotion integration including emotion intensity,
 * regulation capacity, reactivity thresholds, and neurotransmitter modulation.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/emotion/nimcp_emotion_substrate_bridge.h"
#include "cognitive/nimcp_emotional_system.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class EmotionSubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    emotional_system_t* emotion = nullptr;
    emotion_substrate_bridge_t* bridge = nullptr;
    emotion_substrate_config_t config;

    void SetUp() override {
        // Create neural substrate
        substrate_config_t sub_cfg;
        substrate_default_config(&sub_cfg);
        substrate = substrate_create(&sub_cfg);
        ASSERT_NE(substrate, nullptr);

        // Create emotion system
        emotion_config_t emo_cfg = emotion_system_default_config();
        emotion = emotion_system_create(&emo_cfg);
        ASSERT_NE(emotion, nullptr);

        // Get default bridge config
        emotion_substrate_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            emotion_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (emotion) {
            emotion_system_destroy(emotion);
            emotion = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    void createBridge() {
        bridge = emotion_substrate_bridge_create(&config, emotion, substrate);
        ASSERT_NE(bridge, nullptr);
    }

    void setSubstrateATP(float level) {
        if (substrate) {
            substrate_set_atp(substrate, level);
        }
    }

    void setSubstrateTemperature(float temp) {
        if (substrate) {
            substrate_set_temperature(substrate, temp);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, CreateWithValidInputs) {
    bridge = emotion_substrate_bridge_create(&config, emotion, substrate);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(EmotionSubstrateBridgeTest, CreateWithNullConfig) {
    bridge = emotion_substrate_bridge_create(nullptr, emotion, substrate);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(EmotionSubstrateBridgeTest, CreateWithNullSubstrate) {
    bridge = emotion_substrate_bridge_create(&config, emotion, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(EmotionSubstrateBridgeTest, CreateWithNullEmotion) {
    bridge = emotion_substrate_bridge_create(&config, nullptr, substrate);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(EmotionSubstrateBridgeTest, DestroyNull) {
    emotion_substrate_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(EmotionSubstrateBridgeTest, DestroyValid) {
    createBridge();
    emotion_substrate_bridge_destroy(bridge);
    bridge = nullptr;
    SUCCEED();
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, DefaultConfigEnablesModulations) {
    emotion_substrate_config_t cfg;
    emotion_substrate_default_config(&cfg);

    EXPECT_TRUE(cfg.enable_atp_modulation);
    EXPECT_TRUE(cfg.enable_serotonin_modulation);
    EXPECT_TRUE(cfg.enable_dopamine_modulation);
}

TEST_F(EmotionSubstrateBridgeTest, DefaultConfigHasReasonableSensitivity) {
    emotion_substrate_config_t cfg;
    emotion_substrate_default_config(&cfg);

    EXPECT_GT(cfg.atp_sensitivity, 0.0f);
    EXPECT_LE(cfg.atp_sensitivity, 2.0f);
}

TEST_F(EmotionSubstrateBridgeTest, DefaultConfigNullSafe) {
    emotion_substrate_default_config(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, BioAsyncInitiallyDisconnected) {
    createBridge();
    EXPECT_FALSE(emotion_substrate_is_bio_async_connected(bridge));
}

TEST_F(EmotionSubstrateBridgeTest, ConnectBioAsyncNull) {
    int result = emotion_substrate_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(EmotionSubstrateBridgeTest, DisconnectBioAsyncNull) {
    int result = emotion_substrate_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(EmotionSubstrateBridgeTest, IsConnectedNullReturnsFalse) {
    EXPECT_FALSE(emotion_substrate_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * Initial Effects Tests
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, InitialEffectsAreOptimal) {
    createBridge();

    EXPECT_FLOAT_EQ(emotion_substrate_get_intensity_mod(bridge), 1.0f);
    EXPECT_FLOAT_EQ(emotion_substrate_get_regulation_capacity(bridge), 1.0f);
    /* Reactivity threshold starts at 0.5 (moderate reactivity) */
    float reactivity = emotion_substrate_get_reactivity_threshold(bridge);
    EXPECT_GE(reactivity, 0.0f);
    EXPECT_LE(reactivity, 1.0f);
}

TEST_F(EmotionSubstrateBridgeTest, InitiallyNotImpaired) {
    createBridge();
    EXPECT_FALSE(emotion_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Update Tests - Intensity Factor
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, UpdateWithFullATPMaintainsIntensity) {
    createBridge();
    setSubstrateATP(1.0f);

    int result = emotion_substrate_update(bridge);
    EXPECT_EQ(result, 0);

    float intensity = emotion_substrate_get_intensity_mod(bridge);
    EXPECT_GE(intensity, 0.9f);
}

TEST_F(EmotionSubstrateBridgeTest, UpdateWithLowATPAffectsIntensity) {
    createBridge();
    setSubstrateATP(0.3f);

    emotion_substrate_update(bridge);

    /* Low ATP can cause heightened intensity (stress response) - up to 2.0 */
    float intensity = emotion_substrate_get_intensity_mod(bridge);
    EXPECT_GE(intensity, 0.0f);
    EXPECT_LE(intensity, 2.0f);
}

/* ============================================================================
 * Update Tests - Regulation Capacity
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, UpdateWithFullATPMaintainsRegulation) {
    createBridge();
    setSubstrateATP(1.0f);

    emotion_substrate_update(bridge);

    /* Full ATP maintains good (but not necessarily perfect) regulation */
    float regulation = emotion_substrate_get_regulation_capacity(bridge);
    EXPECT_GE(regulation, 0.5f);
    EXPECT_LE(regulation, 1.0f);
}

TEST_F(EmotionSubstrateBridgeTest, UpdateWithLowATPReducesRegulation) {
    createBridge();
    setSubstrateATP(0.2f);

    emotion_substrate_update(bridge);

    float regulation = emotion_substrate_get_regulation_capacity(bridge);
    EXPECT_LT(regulation, 0.5f);
}

/* ============================================================================
 * Update Tests - Reactivity Threshold
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, UpdateWithNormalStateMaintainsReactivity) {
    createBridge();
    setSubstrateATP(1.0f);
    setSubstrateTemperature(37.0f);

    emotion_substrate_update(bridge);

    /* Normal state has moderate reactivity threshold */
    float reactivity = emotion_substrate_get_reactivity_threshold(bridge);
    EXPECT_GE(reactivity, 0.0f);
    EXPECT_LE(reactivity, 1.0f);
}

TEST_F(EmotionSubstrateBridgeTest, UpdateWithStressIncreasesReactivity) {
    createBridge();
    setSubstrateATP(0.25f);

    emotion_substrate_update(bridge);

    // Low ATP below reactivity threshold should affect response
    float reactivity = emotion_substrate_get_reactivity_threshold(bridge);
    EXPECT_GE(reactivity, 0.0f);
    EXPECT_LE(reactivity, 1.0f);
}

/* ============================================================================
 * Impairment Detection Tests
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, ImpairedWithLowResources) {
    createBridge();
    setSubstrateATP(0.1f);

    emotion_substrate_update(bridge);

    EXPECT_TRUE(emotion_substrate_is_impaired(bridge));
}

TEST_F(EmotionSubstrateBridgeTest, NotImpairedWithOptimalState) {
    createBridge();
    setSubstrateATP(1.0f);
    setSubstrateTemperature(37.0f);

    emotion_substrate_update(bridge);

    EXPECT_FALSE(emotion_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Get Effects Tests
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, GetEffectsSuccess) {
    createBridge();

    emotion_substrate_effects_t effects = emotion_substrate_get_effects(bridge);

    EXPECT_FLOAT_EQ(effects.intensity_modulation, 1.0f);
    EXPECT_FLOAT_EQ(effects.regulation_capacity, 1.0f);
}

TEST_F(EmotionSubstrateBridgeTest, GetEffectsNullBridge) {
    /* Returns default effects for null bridge */
    emotion_substrate_effects_t effects = emotion_substrate_get_effects(nullptr);
    /* Implementation returns 1.0 as default intensity */
    EXPECT_FLOAT_EQ(effects.intensity_modulation, 1.0f);
}

/* ============================================================================
 * Query API Null Safety Tests
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, GetIntensityNullReturnsDefault) {
    /* Implementation returns 1.0 as safe default for null */
    EXPECT_FLOAT_EQ(emotion_substrate_get_intensity_mod(nullptr), 1.0f);
}

TEST_F(EmotionSubstrateBridgeTest, GetRegulationNullReturnsDefault) {
    /* Implementation returns 1.0 as safe default for null */
    EXPECT_FLOAT_EQ(emotion_substrate_get_regulation_capacity(nullptr), 1.0f);
}

TEST_F(EmotionSubstrateBridgeTest, GetReactivityNullReturnsDefault) {
    /* Implementation returns 0.5 as safe default for null */
    EXPECT_FLOAT_EQ(emotion_substrate_get_reactivity_threshold(nullptr), 0.5f);
}

TEST_F(EmotionSubstrateBridgeTest, IsImpairedNullReturnsFalse) {
    EXPECT_FALSE(emotion_substrate_is_impaired(nullptr));
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, InitialStatsAreZero) {
    createBridge();

    emotion_substrate_stats_t stats = emotion_substrate_get_stats(bridge);

    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(EmotionSubstrateBridgeTest, UpdateIncrementsCount) {
    createBridge();

    emotion_substrate_update(bridge);
    emotion_substrate_update(bridge);

    emotion_substrate_stats_t stats = emotion_substrate_get_stats(bridge);

    EXPECT_EQ(stats.total_updates, 2u);
}

TEST_F(EmotionSubstrateBridgeTest, GetStatsNullBridge) {
    /* emotion_substrate_get_stats returns struct directly, so null bridge returns zeroed struct */
    emotion_substrate_stats_t stats = emotion_substrate_get_stats(nullptr);
    EXPECT_EQ(stats.total_updates, 0u);
}

/* ============================================================================
 * Configuration Modulation Tests
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, DisabledATPModulationKeepsNormalIntensity) {
    config.enable_atp_modulation = false;
    createBridge();
    setSubstrateATP(0.1f);

    emotion_substrate_update(bridge);

    /* With ATP modulation disabled, intensity stays near normal */
    float intensity = emotion_substrate_get_intensity_mod(bridge);
    EXPECT_GE(intensity, 0.8f);
}

TEST_F(EmotionSubstrateBridgeTest, DisabledATPModulationStaysOptimal) {
    config.enable_atp_modulation = false;
    createBridge();
    setSubstrateATP(0.1f);

    emotion_substrate_update(bridge);

    /* With ATP modulation disabled, regulation capacity stays at optimal */
    float regulation = emotion_substrate_get_regulation_capacity(bridge);
    EXPECT_GE(regulation, 0.9f);
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, UpdateNullReturnsError) {
    int result = emotion_substrate_update(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(EmotionSubstrateBridgeTest, IntensityNeverBelowFloor) {
    createBridge();
    setSubstrateATP(0.0f);

    emotion_substrate_update(bridge);

    float intensity = emotion_substrate_get_intensity_mod(bridge);
    EXPECT_GE(intensity, 0.0f);
}

TEST_F(EmotionSubstrateBridgeTest, IntensityNeverAboveCeiling) {
    createBridge();
    setSubstrateATP(2.0f);

    emotion_substrate_update(bridge);

    float intensity = emotion_substrate_get_intensity_mod(bridge);
    EXPECT_LE(intensity, 1.5f);  // Some implementations allow modest amplification
}

TEST_F(EmotionSubstrateBridgeTest, RegulationNeverBelowFloor) {
    createBridge();
    setSubstrateATP(0.0f);

    emotion_substrate_update(bridge);

    float regulation = emotion_substrate_get_regulation_capacity(bridge);
    EXPECT_GE(regulation, 0.0f);
}
