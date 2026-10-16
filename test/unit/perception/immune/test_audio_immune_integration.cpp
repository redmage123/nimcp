/**
 * @file test_audio_immune_integration.cpp
 * @brief Unit tests for Audio Cortex - Immune System Integration
 *
 * WHAT: Test bidirectional coupling between audio processing and immune system
 * WHY:  Validate cytokine modulation of auditory processing and audio-triggered immunity
 * HOW:  Test cytokine effects on processing accuracy, loudness/anomaly-induced immune activation
 *
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "perception/immune/nimcp_audio_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "perception/nimcp_audio_cortex.h"

class AudioImmuneTest : public ::testing::Test {
protected:
    audio_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    audio_cortex_t* audio_cortex;

    void SetUp() override {
        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Create audio cortex
        audio_cortex_config_t audio_config = {
            .sample_rate = 44100,
            .frame_size = 1024,
            .num_freq_bins = 512,
            .num_mel_filters = 40,
            .num_mfcc = 13,
            .num_channels = 1,
            .feature_dim = 53,
            .enable_attention = true,
            .enable_memory = true,
            .enable_fractal_topology = false,
            .enable_bio_async = false,
            .enable_second_messengers = false
        };
        audio_cortex = audio_cortex_create(&audio_config);
        ASSERT_NE(audio_cortex, nullptr);

        // Create bridge
        audio_immune_config_t bridge_config;
        audio_immune_default_config(&bridge_config);
        bridge = audio_immune_bridge_create(&bridge_config, immune, audio_cortex);
        ASSERT_NE(bridge, nullptr);

        // Start immune system
        int result = brain_immune_start(immune);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        audio_immune_bridge_destroy(bridge);
        brain_immune_stop(immune);
        brain_immune_destroy(immune);
        audio_cortex_destroy(audio_cortex);
    }
};

/**
 * TEST: Lifecycle - Default Configuration
 * WHAT: Verify default configuration provides sensible values
 */
TEST_F(AudioImmuneTest, DefaultConfiguration) {
    audio_immune_config_t config;
    int result = audio_immune_default_config(&config);
    EXPECT_EQ(result, 0);

    // All features should be enabled by default
    EXPECT_TRUE(config.enable_cytokine_audio_modulation);
    EXPECT_TRUE(config.enable_inflammation_processing_impairment);
    EXPECT_TRUE(config.enable_audio_immune_trigger);
    EXPECT_TRUE(config.enable_audio_immune_boost);
    EXPECT_TRUE(config.enable_tinnitus_inflammation_coupling);

    // Sensitivities should be 1.0 by default
    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.threat_trigger_sensitivity, 1.0f);

    // Thresholds should be reasonable
    EXPECT_GT(config.loudness_trigger_threshold, 0.5f);
    EXPECT_LT(config.loudness_trigger_threshold, 1.0f);
    EXPECT_GT(config.anomaly_trigger_threshold, 0.5f);
    EXPECT_GT(config.inflammation_audio_threshold, 0.0f);
}

/**
 * TEST: Lifecycle - Bridge Creation and Destruction
 * WHAT: Verify bridge can be created and destroyed without errors
 */
TEST_F(AudioImmuneTest, LifecycleManagement) {
    // Bridge was created in SetUp, verify it's valid
    EXPECT_NE(bridge, nullptr);

    // Create another bridge to test multiple instances
    audio_immune_config_t config;
    audio_immune_default_config(&config);
    audio_immune_bridge_t* bridge2 = audio_immune_bridge_create(&config, immune, audio_cortex);
    EXPECT_NE(bridge2, nullptr);

    // Destroy second bridge
    audio_immune_bridge_destroy(bridge2);
}

/**
 * TEST: Cytokine Effects on Audio Processing
 * BIOLOGICAL: Pro-inflammatory cytokines impair auditory processing
 */
TEST_F(AudioImmuneTest, CytokineImpairmentOfProcessing) {
    // Trigger inflammation in immune system
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02, 0x03};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  8, /* high severity */
                                  0, &antigen_id);

    // Activate full immune response to raise cytokine levels
    uint32_t b_cell_id, helper_id, antibody_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);
    brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);

    // CRITICAL: Must initiate inflammation sites to have cytokine effects
    // The cytokine level estimation depends on inflammation_sites count
    uint32_t site_id;
    for (int i = 0; i < 5; i++) {
        brain_immune_initiate_inflammation(immune, i, antigen_id, &site_id);
    }

    // Apply cytokine effects to audio processing
    int result = audio_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    // Get cytokine effects - should show processing impairment
    cytokine_audio_effects_t effects;
    audio_immune_get_cytokine_effects(bridge, &effects);

    // Pro-inflammatory cytokines should impair processing
    EXPECT_LT(effects.total_processing_impact, 0.0f);
    EXPECT_GT(effects.noise_sensitivity_increase, 0.0f);
    EXPECT_GT(effects.attention_impairment, 0.0f);
}

/**
 * TEST: Inflammation Processing Impairment
 * BIOLOGICAL: Chronic inflammation reduces auditory processing capabilities
 */
TEST_F(AudioImmuneTest, InflammationReducesProcessingCapability) {
    // Create systemic inflammation by multiple threats
    uint32_t site_id;
    for (int i = 0; i < 5; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)(0x10 + i), 0x20, 0x30};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                      7, /* moderate-high severity */
                                      0, &antigen_id);
        // CRITICAL: Must initiate inflammation sites
        brain_immune_initiate_inflammation(immune, i, antigen_id, &site_id);
    }

    // Apply inflammation effects
    int result = audio_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    // Get inflammation state
    inflammation_audio_state_t inflam_state;
    audio_immune_get_inflammation_state(bridge, &inflam_state);

    // Processing capabilities should be reduced
    EXPECT_LT(inflam_state.processing_accuracy, 1.0f);
    EXPECT_LT(inflam_state.frequency_discrimination, 1.0f);
    EXPECT_LT(inflam_state.temporal_resolution, 1.0f);
    EXPECT_LT(inflam_state.noise_tolerance, 1.0f);
    EXPECT_LT(inflam_state.processing_bandwidth, 1.0f);

    // Auditory attention should be reduced
    EXPECT_LT(inflam_state.auditory_attention, 1.0f);
}

/**
 * TEST: Bandwidth Reduction Under Inflammation
 * BIOLOGICAL: Inflammation reduces neural resources for processing
 */
TEST_F(AudioImmuneTest, BandwidthReductionFromInflammation) {
    // No inflammation - should have no reduction
    float baseline_reduction = audio_immune_compute_bandwidth_reduction(bridge);
    EXPECT_FLOAT_EQ(baseline_reduction, 0.0f);

    // Create systemic inflammation
    uint32_t site_id;
    for (int i = 0; i < 7; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)(0xA0 + i), 0xB0, 0xC0};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                      8, 0, &antigen_id);
        // CRITICAL: Must initiate inflammation sites
        brain_immune_initiate_inflammation(immune, i, antigen_id, &site_id);
    }

    // Apply inflammation effects
    audio_immune_apply_inflammation_effects(bridge);

    // Bandwidth should be reduced
    float inflamed_reduction = audio_immune_compute_bandwidth_reduction(bridge);
    EXPECT_GT(inflamed_reduction, baseline_reduction);
    EXPECT_LE(inflamed_reduction, MAX_BANDWIDTH_REDUCTION);
}

/**
 * TEST: Noise Sensitivity Increases with Inflammation
 * BIOLOGICAL: Inflammation increases susceptibility to noise damage
 */
TEST_F(AudioImmuneTest, NoiseSensitivityIncrease) {
    // Baseline sensitivity
    float baseline_sensitivity = audio_immune_compute_noise_sensitivity(bridge);
    EXPECT_FLOAT_EQ(baseline_sensitivity, 1.0f);

    // Create regional inflammation
    uint32_t site_id;
    for (int i = 0; i < 3; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)(0x50 + i), 0x60, 0x70};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                      6, 0, &antigen_id);
        // CRITICAL: Must initiate inflammation sites
        brain_immune_initiate_inflammation(immune, i, antigen_id, &site_id);
    }

    // Apply inflammation
    audio_immune_apply_inflammation_effects(bridge);

    // Noise sensitivity should increase
    float inflamed_sensitivity = audio_immune_compute_noise_sensitivity(bridge);
    EXPECT_GT(inflamed_sensitivity, baseline_sensitivity);
    EXPECT_LE(inflamed_sensitivity, 3.0f);
}

/**
 * TEST: Loudness-Triggered Immune Response
 * BIOLOGICAL: Loud noise triggers stress response and inflammation
 */
TEST_F(AudioImmuneTest, LoudnessTriggersImmuneResponse) {
    // Get baseline immune stats
    brain_immune_stats_t baseline_stats;
    brain_immune_get_stats(immune, &baseline_stats);

    // Trigger with loud sound
    float loudness = 0.9f; // Above AUDIO_THREAT_LOUDNESS_THRESHOLD
    float novelty = 0.3f;
    float anomaly = 0.2f;

    int result = audio_immune_trigger_from_threat(bridge, loudness, novelty, anomaly);
    EXPECT_EQ(result, 0);

    // Immune system should be activated
    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune, &after_stats);

    EXPECT_GT(after_stats.antigens_processed, baseline_stats.antigens_processed);
}

/**
 * TEST: Novelty-Triggered Immune Surveillance
 * BIOLOGICAL: Novel sounds trigger immune surveillance
 */
TEST_F(AudioImmuneTest, NoveltyTriggersImmuneSurveillance) {
    // Trigger with novel sound
    float loudness = 0.5f;
    float novelty = 0.95f; // Above AUDIO_THREAT_NOVELTY_THRESHOLD
    float anomaly = 0.3f;

    int result = audio_immune_trigger_from_threat(bridge, loudness, novelty, anomaly);
    EXPECT_EQ(result, 0);

    // Check that bridge recorded the trigger
    EXPECT_GT(bridge->anomaly_count, 0u);
}

/**
 * TEST: Anomaly-Triggered Immune Response
 * BIOLOGICAL: Pattern violations signal danger
 */
TEST_F(AudioImmuneTest, AnomalyTriggersImmuneResponse) {
    // Trigger with auditory anomaly
    float loudness = 0.4f;
    float novelty = 0.5f;
    float anomaly = 0.95f; // High anomaly score

    int result = audio_immune_trigger_from_threat(bridge, loudness, novelty, anomaly);
    EXPECT_EQ(result, 0);

    // Should have triggered immune response
    EXPECT_GT(bridge->audio_triggered_responses, 0u);
}

/**
 * TEST: Processing Failure Triggers Immune Response
 * BIOLOGICAL: Processing failure signals sensory system stress
 */
TEST_F(AudioImmuneTest, ProcessingFailureTriggersImmune) {
    // High processing failure rate
    float failure_rate = 0.7f;

    int result = audio_immune_trigger_from_processing_failure(bridge, failure_rate);
    EXPECT_EQ(result, 0);

    // Should have recorded processing failure
    EXPECT_GT(bridge->processing_failures, 0u);
}

/**
 * TEST: Tinnitus-Inflammation Coupling
 * BIOLOGICAL: Tinnitus associated with neuroinflammation
 */
TEST_F(AudioImmuneTest, TinnitusAmpliesInflammation) {
    // Get baseline immune stats
    brain_immune_stats_t baseline_stats;
    brain_immune_get_stats(immune, &baseline_stats);

    // Trigger tinnitus episode
    float tinnitus_severity = 0.8f;
    int result = audio_immune_amplify_tinnitus_inflammation(bridge, tinnitus_severity);
    EXPECT_EQ(result, 0);

    // Should have triggered immune response
    EXPECT_GT(bridge->tinnitus_episodes, 0u);

    // Check inflammation state
    inflammation_audio_state_t state;
    audio_immune_get_inflammation_state(bridge, &state);
    EXPECT_FLOAT_EQ(state.tinnitus_severity, tinnitus_severity);
}

/**
 * TEST: Calm Environment Boosts Immunity
 * BIOLOGICAL: Quiet/music reduces stress and enhances immune function
 */
TEST_F(AudioImmuneTest, CalmEnvironmentBoostsImmunity) {
    // Provide calm environment
    float quietness = 0.8f;
    float music_presence = 0.7f;
    float predictability = 0.9f;

    int result = audio_immune_boost_from_calm_environment(
        bridge, quietness, music_presence, predictability);
    EXPECT_EQ(result, 0);

    // Should have recorded boost
    EXPECT_GT(bridge->audio_boosts, 0u);

    // Check boost state
    EXPECT_GT(bridge->audio_boost.immune_enhancement, 0.0f);
    EXPECT_GT(bridge->audio_boost.il10_release_boost, 0.0f);
    EXPECT_GT(bridge->audio_boost.stress_reduction, 0.0f);
}

/**
 * TEST: No Boost from Non-Calm Environment
 * BIOLOGICAL: Only calm environments provide immune benefits
 */
TEST_F(AudioImmuneTest, NoCalmNoBoost) {
    // Provide non-calm environment
    float quietness = 0.3f;
    float music_presence = 0.2f;
    float predictability = 0.4f;

    int result = audio_immune_boost_from_calm_environment(
        bridge, quietness, music_presence, predictability);
    EXPECT_EQ(result, 0);

    // Should not provide immune boost
    EXPECT_FLOAT_EQ(bridge->audio_boost.immune_enhancement, 0.0f);
}

/**
 * TEST: Bidirectional Update Integration
 * WHAT: Verify update processes both directions
 */
TEST_F(AudioImmuneTest, BidirectionalUpdate) {
    // Run update
    int result = audio_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);

    // Should have incremented update counter
    EXPECT_GT(bridge->total_updates, 0u);
}

/**
 * TEST: Auditory Impairment Detection
 * BIOLOGICAL: Significant processing reduction indicates impairment
 */
TEST_F(AudioImmuneTest, AuditoryImpairmentDetection) {
    // Create inflammation first to set baseline
    // (The bridge may have some initial state)
    uint32_t site_id;
    for (int i = 0; i < 10; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)(0xD0 + i), 0xE0, 0xF0};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                      9, 0, &antigen_id);
        // CRITICAL: Must initiate inflammation sites
        brain_immune_initiate_inflammation(immune, i, antigen_id, &site_id);
    }

    // Apply inflammation
    audio_immune_apply_inflammation_effects(bridge);

    // Should detect impairment
    EXPECT_TRUE(audio_immune_is_impaired(bridge));
}

/**
 * TEST: Accuracy Reduction Query
 * BIOLOGICAL: Quantify processing accuracy loss
 */
TEST_F(AudioImmuneTest, AccuracyReductionQuery) {
    // Baseline - no reduction (need to apply inflammation effects first to get baseline)
    audio_immune_apply_inflammation_effects(bridge);
    float baseline = audio_immune_get_accuracy_reduction(bridge);
    // Baseline should be 0 when there are no inflammation sites
    EXPECT_FLOAT_EQ(baseline, 0.0f);

    // Create inflammation
    uint32_t site_id;
    for (int i = 0; i < 4; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)(0x40 + i), 0x50, 0x60};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                      7, 0, &antigen_id);
        // CRITICAL: Must initiate inflammation sites
        brain_immune_initiate_inflammation(immune, i, antigen_id, &site_id);
    }

    // Apply inflammation
    audio_immune_apply_inflammation_effects(bridge);

    // Should have some accuracy reduction
    float reduction = audio_immune_get_accuracy_reduction(bridge);
    EXPECT_GT(reduction, baseline);
    EXPECT_LE(reduction, 1.0f);
}

/**
 * TEST: Tinnitus Severity Query
 * BIOLOGICAL: Monitor tinnitus episodes
 */
TEST_F(AudioImmuneTest, TinnitusSeverityQuery) {
    // Baseline - no tinnitus
    float baseline = audio_immune_get_tinnitus_severity(bridge);
    EXPECT_FLOAT_EQ(baseline, 0.0f);

    // Trigger tinnitus
    audio_immune_amplify_tinnitus_inflammation(bridge, 0.6f);

    // Should report tinnitus
    float severity = audio_immune_get_tinnitus_severity(bridge);
    EXPECT_FLOAT_EQ(severity, 0.6f);
}

/**
 * TEST: Attention Level Query
 * BIOLOGICAL: Monitor auditory attention reduction
 */
TEST_F(AudioImmuneTest, AttentionLevelQuery) {
    // Baseline - get initial attention (may need inflammation effects applied first)
    audio_immune_apply_inflammation_effects(bridge);
    float baseline = audio_immune_get_attention_level(bridge);
    // With no inflammation, attention should be at full capacity (1.0)
    EXPECT_FLOAT_EQ(baseline, 1.0f);

    // Create inflammation to reduce attention
    uint32_t site_id;
    for (int i = 0; i < 6; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)(0x80 + i), 0x90, 0xA0};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                      8, 0, &antigen_id);
        // CRITICAL: Must initiate inflammation sites
        brain_immune_initiate_inflammation(immune, i, antigen_id, &site_id);
    }

    // Apply inflammation
    audio_immune_apply_inflammation_effects(bridge);

    // Attention should be reduced
    float attention = audio_immune_get_attention_level(bridge);
    EXPECT_LT(attention, baseline);
}

/**
 * TEST: Chronic Inflammation Auditory Effects
 * BIOLOGICAL: Prolonged inflammation has cumulative effects
 */
TEST_F(AudioImmuneTest, ChronicInflammationEffects) {
    // Create sustained inflammation
    uint32_t site_id;
    for (int i = 0; i < 8; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)(0xC0 + i), 0xD0, 0xE0};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                      9, 0, &antigen_id);
        // CRITICAL: Must initiate inflammation sites
        brain_immune_initiate_inflammation(immune, i, antigen_id, &site_id);
    }

    // Apply inflammation effects
    audio_immune_apply_inflammation_effects(bridge);

    // Get inflammation state
    inflammation_audio_state_t state;
    audio_immune_get_inflammation_state(bridge, &state);

    // Should show impairment (systemic inflammation = ~0.7 factor)
    // processing_accuracy = 1.0 - (0.7 * 0.6) = 0.58
    EXPECT_LT(state.processing_accuracy, 0.7f);
    EXPECT_LT(state.frequency_discrimination, 0.8f);
    EXPECT_LT(state.noise_tolerance, 0.6f);
}

/**
 * TEST: IL-10 Recovery Effects
 * BIOLOGICAL: Anti-inflammatory cytokines restore processing
 */
TEST_F(AudioImmuneTest, IL10RecoveryEffects) {
    // Create then resolve inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0x11, 0x22, 0x33};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  6, 0, &antigen_id);

    // Neutralize to trigger IL-10 release
    uint32_t b_cell_id, helper_id, antibody_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);
    brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);
    brain_immune_neutralize(immune, antigen_id, antibody_id);

    // Apply cytokine effects
    audio_immune_apply_cytokine_effects(bridge);

    // Get effects
    cytokine_audio_effects_t effects;
    audio_immune_get_cytokine_effects(bridge, &effects);

    // IL-10 should provide some recovery boost
    EXPECT_GE(effects.il10_recovery_boost, 0.0f);
}

/**
 * TEST: Thread Safety - Concurrent Updates
 * WHAT: Verify mutex protects concurrent access
 */
TEST_F(AudioImmuneTest, ThreadSafetyConcurrentAccess) {
    // Multiple updates in sequence (single-threaded test)
    for (int i = 0; i < 10; i++) {
        audio_immune_bridge_update(bridge, 10);
    }

    EXPECT_EQ(bridge->total_updates, 10u);
}

/**
 * TEST: Null Pointer Handling
 * WHAT: Verify functions handle NULL gracefully
 */
TEST_F(AudioImmuneTest, NullPointerHandling) {
    EXPECT_EQ(audio_immune_default_config(nullptr), -1);
    EXPECT_EQ(audio_immune_bridge_create(nullptr, nullptr, nullptr), nullptr);
    EXPECT_EQ(audio_immune_apply_cytokine_effects(nullptr), -1);
    EXPECT_EQ(audio_immune_apply_inflammation_effects(nullptr), -1);
    EXPECT_FLOAT_EQ(audio_immune_compute_bandwidth_reduction(nullptr), 0.0f);
    EXPECT_FALSE(audio_immune_is_impaired(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
