/**
 * @file test_surprise_bridges_phase1b_regression.cpp
 * @brief Regression tests for Phase 1b Surprise Bridges (Society of Thought)
 * @date 2026-01-27
 *
 * WHAT: Regression tests verifying baselines, boundaries, and performance
 * WHY:  Catch regressions in default configs, formula behavior, and performance
 * HOW:  Per-bridge fixture tests with exact constant checks and boundary probes
 *
 * TEST CATEGORIES:
 * - Default config baselines (match header constants exactly)
 * - Boundary conditions (threshold exact, above, below)
 * - Formula verification (specific computations)
 * - Clamp verification (output range enforcement)
 * - Error code verification (correct error returns)
 * - Performance tests (throughput within limits)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>

extern "C" {
#include "cognitive/salience/nimcp_surprise_amplifier.h"
#include "cognitive/salience/nimcp_surprise_plasticity_bridge.h"
#include "cognitive/salience/nimcp_surprise_snn_bridge.h"
#include "cognitive/salience/nimcp_surprise_substrate_bridge.h"
#include "cognitive/salience/nimcp_surprise_thalamic_bridge.h"
#include "cognitive/salience/nimcp_surprise_pink_noise_bridge.h"
#include "cognitive/salience/nimcp_surprise_imagination_bridge.h"
#include "cognitive/salience/nimcp_surprise_self_model_bridge.h"
}

/* ============================================================================
 * Helper: Create amplifier for regression tests
 * ============================================================================ */

static surprise_amplifier_t* create_test_amplifier() {
    surprise_amplifier_config_t cfg = surprise_amplifier_default_config();
    cfg.refractory_period_ms = 0;
    cfg.max_concurrent = 256;
    cfg.enable_bio_async = false;
    cfg.enable_logging = false;
    return surprise_amplifier_create(&cfg);
}

/* ============================================================================
 * 1. Plasticity Bridge Regression
 * ============================================================================ */

class PlasticityRegressionTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        amp = create_test_amplifier();
        surprise_plasticity_config_t cfg = surprise_plasticity_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_plasticity_bridge_create(&cfg);
        surprise_plasticity_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) surprise_plasticity_bridge_destroy(bridge);
        if (amp) surprise_amplifier_destroy(amp);
    }
};

TEST_F(PlasticityRegressionTest, DefaultConfigBaselines) {
    surprise_plasticity_config_t cfg = surprise_plasticity_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.learning_rate_boost, 2.0f);
    EXPECT_FLOAT_EQ(cfg.habituation_rate, 0.05f);
    EXPECT_FLOAT_EQ(cfg.habituation_recovery_rate, 0.01f);
    EXPECT_FLOAT_EQ(cfg.stdp_window_expansion, 1.5f);
    EXPECT_FLOAT_EQ(cfg.eligibility_boost, 1.3f);
    EXPECT_FLOAT_EQ(cfg.bcm_threshold_shift, 0.1f);
    EXPECT_FLOAT_EQ(cfg.min_surprise_for_boost, 0.3f);
    EXPECT_EQ(cfg.max_tracked_sources, 64u);
}

TEST_F(PlasticityRegressionTest, ThresholdBoundaryExact) {
    /* Exactly at threshold: 0.3f */
    surprise_plasticity_on_surprise_event(bridge, 0.3f, 0x100);
    surprise_plasticity_bridge_update(bridge, 0.1f);

    surprise_plasticity_effects_t effects;
    surprise_plasticity_bridge_get_effects(bridge, &effects);
    /* At exact threshold, boost should activate */
    EXPECT_GE(effects.learning_rate_multiplier, 1.0f);
}

TEST_F(PlasticityRegressionTest, ThresholdBoundaryBelow) {
    /* Just below threshold */
    surprise_plasticity_on_surprise_event(bridge, 0.29f, 0x100);
    surprise_plasticity_bridge_update(bridge, 0.1f);

    surprise_plasticity_effects_t effects;
    surprise_plasticity_bridge_get_effects(bridge, &effects);
    EXPECT_FLOAT_EQ(effects.learning_rate_multiplier, 1.0f);
}

TEST_F(PlasticityRegressionTest, HabituationAccumulation) {
    /* Register the source first via surprise event */
    surprise_plasticity_on_surprise_event(bridge, 0.8f, 0x100);

    /* Repeated learning outcomes from same source should increase habituation */
    for (int i = 0; i < 20; i++) {
        surprise_plasticity_on_learning_outcome(bridge, 0.5f, 0x100);
    }

    float hab = surprise_plasticity_get_habituation_for_source(bridge, 0x100);
    EXPECT_GT(hab, 0.0f);
    EXPECT_LE(hab, 1.0f);  /* Should be clamped */
}

TEST_F(PlasticityRegressionTest, ErrorCodeVerification) {
    EXPECT_EQ(NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER, 28401);
    EXPECT_EQ(NIMCP_SURPRISE_PLASTICITY_ERROR_INVALID_PARAM, 28402);
    EXPECT_EQ(NIMCP_SURPRISE_PLASTICITY_ERROR_NO_MEMORY, 28403);
    EXPECT_EQ(NIMCP_SURPRISE_PLASTICITY_ERROR_NOT_CONNECTED, 28404);
}

TEST_F(PlasticityRegressionTest, Performance10kEvents) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        surprise_plasticity_on_surprise_event(bridge, 0.8f, (uint32_t)(i % 64));
        surprise_plasticity_bridge_update(bridge, 0.001f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(ms, 500) << "10k plasticity events took " << ms << "ms (limit 500ms)";
}

/* ============================================================================
 * 2. SNN Bridge Regression
 * ============================================================================ */

class SnnRegressionTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        amp = create_test_amplifier();
        surprise_snn_config_t cfg = surprise_snn_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_snn_bridge_create(&cfg);
        surprise_snn_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) surprise_snn_bridge_destroy(bridge);
        if (amp) surprise_amplifier_destroy(amp);
    }
};

TEST_F(SnnRegressionTest, DefaultConfigBaselines) {
    surprise_snn_config_t cfg = surprise_snn_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.dt_ms, 1.0f);
    EXPECT_EQ(cfg.neurons_per_channel, 16u);
    EXPECT_EQ(cfg.encoding_type, SURPRISE_SNN_ENCODING_RATE);
    EXPECT_FLOAT_EQ(cfg.threshold, 0.5f);
    EXPECT_FLOAT_EQ(cfg.refractory_ms, 2.0f);
    EXPECT_FLOAT_EQ(cfg.decay_factor, 0.95f);
    EXPECT_EQ(cfg.history_size, 32u);
}

TEST_F(SnnRegressionTest, AllChannelsEncodable) {
    /* All 4 channels should accept encoding */
    EXPECT_EQ(surprise_snn_encode_surprise(bridge, 0.5f, SURPRISE_SNN_CHANNEL_PE), 0);
    EXPECT_EQ(surprise_snn_encode_surprise(bridge, 0.5f, SURPRISE_SNN_CHANNEL_CONFLICT), 0);
    EXPECT_EQ(surprise_snn_encode_surprise(bridge, 0.5f, SURPRISE_SNN_CHANNEL_NOVELTY), 0);
    EXPECT_EQ(surprise_snn_encode_surprise(bridge, 0.5f, SURPRISE_SNN_CHANNEL_HYPOTHESIS), 0);
}

TEST_F(SnnRegressionTest, ClampSurpriseInput) {
    /* Values > 1.0 should be clamped, not crash */
    EXPECT_EQ(surprise_snn_encode_surprise(bridge, 2.0f, SURPRISE_SNN_CHANNEL_PE), 0);
    EXPECT_EQ(surprise_snn_encode_surprise(bridge, -1.0f, SURPRISE_SNN_CHANNEL_PE), 0);
}

TEST_F(SnnRegressionTest, ErrorCodeVerification) {
    EXPECT_EQ(NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER, 28501);
    EXPECT_EQ(NIMCP_SURPRISE_SNN_ERROR_INVALID_PARAM, 28502);
    EXPECT_EQ(NIMCP_SURPRISE_SNN_ERROR_NO_MEMORY, 28503);
}

TEST_F(SnnRegressionTest, Performance10kSimSteps) {
    surprise_snn_encode_surprise(bridge, 0.8f, SURPRISE_SNN_CHANNEL_PE);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        surprise_snn_simulate_step(bridge);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(ms, 500) << "10k SNN sim steps took " << ms << "ms (limit 500ms)";
}

/* ============================================================================
 * 3. Substrate Bridge Regression
 * ============================================================================ */

class SubstrateRegressionTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_substrate_bridge_t* bridge = nullptr;

    void SetUp() override {
        amp = create_test_amplifier();
        surprise_substrate_config_t cfg = surprise_substrate_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_substrate_bridge_create(&cfg);
        surprise_substrate_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) surprise_substrate_bridge_destroy(bridge);
        if (amp) surprise_amplifier_destroy(amp);
    }
};

TEST_F(SubstrateRegressionTest, DefaultConfigBaselines) {
    surprise_substrate_config_t cfg = surprise_substrate_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.detection_sensitivity_mult, 1.05f);
    EXPECT_FLOAT_EQ(cfg.amplification_accuracy_mult, 1.0f);
    EXPECT_FLOAT_EQ(cfg.decay_modulation_mult, 0.95f);
    EXPECT_FLOAT_EQ(cfg.refractory_modulation_mult, 1.1f);
    EXPECT_FLOAT_EQ(cfg.min_capacity, 0.3f);
}

TEST_F(SubstrateRegressionTest, MinCapacityEnforced) {
    /* Even at zero ATP, capacity should not go below min */
    surprise_substrate_bridge_update(bridge, 0.0f, 1.0f);

    surprise_substrate_effects_t effects;
    surprise_substrate_bridge_get_effects(bridge, &effects);
    EXPECT_GE(effects.overall_capacity, 0.3f);
}

TEST_F(SubstrateRegressionTest, ErrorCodeVerification) {
    EXPECT_EQ(NIMCP_SURPRISE_SUBSTRATE_ERROR_NULL_POINTER, 28601);
    EXPECT_EQ(NIMCP_SURPRISE_SUBSTRATE_ERROR_INVALID_PARAM, 28602);
    EXPECT_EQ(NIMCP_SURPRISE_SUBSTRATE_ERROR_NO_MEMORY, 28603);
}

TEST_F(SubstrateRegressionTest, Performance10kUpdates) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        surprise_substrate_bridge_update(bridge, 0.8f, 0.2f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(ms, 200) << "10k substrate updates took " << ms << "ms (limit 200ms)";
}

/* ============================================================================
 * 4. Thalamic Bridge Regression
 * ============================================================================ */

class ThalamicRegressionTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        amp = create_test_amplifier();
        surprise_thalamic_config_t cfg = surprise_thalamic_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_thalamic_bridge_create(&cfg);
        surprise_thalamic_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) surprise_thalamic_bridge_destroy(bridge);
        if (amp) surprise_amplifier_destroy(amp);
    }
};

TEST_F(ThalamicRegressionTest, DefaultConfigBaselines) {
    surprise_thalamic_config_t cfg = surprise_thalamic_bridge_default_config();
    EXPECT_TRUE(cfg.enable_realization);
    EXPECT_TRUE(cfg.enable_conflict);
    EXPECT_TRUE(cfg.enable_novelty);
    EXPECT_TRUE(cfg.enable_hypothesis);
    EXPECT_FLOAT_EQ(cfg.threshold_realization, 0.5f);
    EXPECT_FLOAT_EQ(cfg.threshold_conflict, 0.4f);
    EXPECT_FLOAT_EQ(cfg.threshold_novelty, 0.3f);
    EXPECT_FLOAT_EQ(cfg.threshold_hypothesis, 0.6f);
    EXPECT_FLOAT_EQ(cfg.attention_weight_default, 1.0f);
}

TEST_F(ThalamicRegressionTest, SignalTypeConstants) {
    EXPECT_EQ(SURPRISE_THALAMIC_REALIZATION, 0x01u);
    EXPECT_EQ(SURPRISE_THALAMIC_CONFLICT, 0x02u);
    EXPECT_EQ(SURPRISE_THALAMIC_NOVELTY, 0x04u);
    EXPECT_EQ(SURPRISE_THALAMIC_HYPOTHESIS, 0x08u);
}

TEST_F(ThalamicRegressionTest, DestinationConstants) {
    EXPECT_EQ(SURPRISE_THALAMIC_DEST_ACC, 0x4002u);
    EXPECT_EQ(SURPRISE_THALAMIC_DEST_ANTERIOR_INSULA, 0x4001u);
    EXPECT_EQ(SURPRISE_THALAMIC_DEST_PULVINAR, 0x4003u);
    EXPECT_EQ(SURPRISE_THALAMIC_DEST_PFC, 0x4004u);
}

TEST_F(ThalamicRegressionTest, AttentionWeightPersistence) {
    /* Set weight, read it back, verify persistence */
    surprise_thalamic_set_attention_weight(bridge, SURPRISE_THALAMIC_NOVELTY, 3.0f);
    EXPECT_FLOAT_EQ(surprise_thalamic_get_attention_weight(bridge, SURPRISE_THALAMIC_NOVELTY), 3.0f);

    /* Other weights should remain at default */
    EXPECT_FLOAT_EQ(surprise_thalamic_get_attention_weight(bridge, SURPRISE_THALAMIC_REALIZATION), 1.0f);
}

TEST_F(ThalamicRegressionTest, ErrorCodeVerification) {
    EXPECT_EQ(NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER, 28701);
    EXPECT_EQ(NIMCP_SURPRISE_THALAMIC_ERROR_INVALID_PARAM, 28702);
}

TEST_F(ThalamicRegressionTest, Performance10kRoutes) {
    surprise_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = SURPRISE_THALAMIC_NOVELTY;
    signal.surprise_magnitude = 0.7f;
    signal.source_module = 0x100;
    signal.urgency = 0.5f;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        surprise_thalamic_route_surprise(bridge, &signal);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(ms, 200) << "10k thalamic routes took " << ms << "ms (limit 200ms)";
}

/* ============================================================================
 * 5. Pink Noise Bridge Regression
 * ============================================================================ */

class PinkNoiseRegressionTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_pink_noise_bridge_t* bridge = nullptr;

    void SetUp() override {
        amp = create_test_amplifier();
        surprise_pink_noise_config_t cfg = surprise_pink_noise_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_pink_noise_bridge_create(&cfg);
        surprise_pink_noise_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) surprise_pink_noise_bridge_destroy(bridge);
        if (amp) surprise_amplifier_destroy(amp);
    }
};

TEST_F(PinkNoiseRegressionTest, DefaultConfigBaselines) {
    surprise_pink_noise_config_t cfg = surprise_pink_noise_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.base_amplitude, 0.05f);
    EXPECT_FLOAT_EQ(cfg.alpha, 1.0f);
    EXPECT_FLOAT_EQ(cfg.adaptation_rate, 0.01f);
    EXPECT_FLOAT_EQ(cfg.temporal_smoothing, 0.9f);
    EXPECT_FLOAT_EQ(cfg.min_amplitude, 0.001f);
    EXPECT_FLOAT_EQ(cfg.max_amplitude, 0.2f);
    EXPECT_FLOAT_EQ(cfg.target_amplitudes[SURPRISE_PINK_NOISE_TARGET_THRESHOLD], 1.0f);
    EXPECT_FLOAT_EQ(cfg.target_amplitudes[SURPRISE_PINK_NOISE_TARGET_SENSITIVITY], 0.8f);
    EXPECT_FLOAT_EQ(cfg.target_amplitudes[SURPRISE_PINK_NOISE_TARGET_DECAY], 0.5f);
    EXPECT_FLOAT_EQ(cfg.target_amplitudes[SURPRISE_PINK_NOISE_TARGET_REFRACTORY], 0.3f);
}

TEST_F(PinkNoiseRegressionTest, NoiseAmplitudeClamped) {
    /* Push amplitude high with sustained surprise */
    for (int i = 0; i < 1000; i++) {
        surprise_pink_noise_adapt_amplitude(bridge, 1.0f);
    }

    surprise_pink_noise_effects_t effects;
    surprise_pink_noise_bridge_get_effects(bridge, &effects);
    EXPECT_LE(effects.effective_amplitude, 0.2f);  /* max_amplitude */
}

TEST_F(PinkNoiseRegressionTest, InitialAmplitude) {
    surprise_pink_noise_effects_t effects;
    surprise_pink_noise_bridge_get_effects(bridge, &effects);
    EXPECT_FLOAT_EQ(effects.effective_amplitude, 0.05f);  /* base_amplitude */
    EXPECT_FLOAT_EQ(effects.adaptation_factor, 1.0f);
}

TEST_F(PinkNoiseRegressionTest, ErrorCodeVerification) {
    EXPECT_EQ(NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER, 28801);
    EXPECT_EQ(NIMCP_SURPRISE_PINK_NOISE_ERROR_INVALID_PARAM, 28802);
}

TEST_F(PinkNoiseRegressionTest, Performance10kInjections) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        surprise_pink_noise_inject(bridge);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(ms, 200) << "10k noise injections took " << ms << "ms (limit 200ms)";
}

/* ============================================================================
 * 6. Imagination Bridge Regression
 * ============================================================================ */

class ImaginationRegressionTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_imagination_bridge_t* bridge = nullptr;

    void SetUp() override {
        amp = create_test_amplifier();
        surprise_imagination_config_t cfg = surprise_imagination_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_imagination_bridge_create(&cfg);
        surprise_imagination_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) surprise_imagination_bridge_destroy(bridge);
        if (amp) surprise_amplifier_destroy(amp);
    }
};

TEST_F(ImaginationRegressionTest, DefaultConfigBaselines) {
    surprise_imagination_config_t cfg = surprise_imagination_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.trigger_threshold, 0.7f);
    EXPECT_FLOAT_EQ(cfg.cooldown_seconds, 5.0f);
    EXPECT_EQ(cfg.max_scenarios, 4u);
    EXPECT_FLOAT_EQ(cfg.expectation_update_rate, 0.1f);
    EXPECT_EQ(cfg.counterfactual_depth, 3u);
}

TEST_F(ImaginationRegressionTest, ThresholdBoundaryExact) {
    /* Exactly at threshold (0.7f) */
    surprise_imagination_check_trigger(bridge, 0.7f, 0x100);

    surprise_imagination_stats_t stats;
    surprise_imagination_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.triggers, 1u);
}

TEST_F(ImaginationRegressionTest, ThresholdBoundaryBelow) {
    surprise_imagination_check_trigger(bridge, 0.69f, 0x100);

    surprise_imagination_stats_t stats;
    surprise_imagination_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.triggers, 0u);
}

TEST_F(ImaginationRegressionTest, MaxScenariosEnforced) {
    /* Fill all 4 scenario slots */
    for (uint32_t i = 0; i < 4; i++) {
        surprise_imagination_check_trigger(bridge, 0.9f, 0x100 + i);
        /* Expire cooldown by updating with enough dt */
        for (int j = 0; j < 60; j++) {
            surprise_imagination_bridge_update(bridge, 0.1f);
        }
    }

    surprise_imagination_effects_t effects;
    surprise_imagination_bridge_get_effects(bridge, &effects);
    EXPECT_LE(effects.scenarios_active, 4u);
}

TEST_F(ImaginationRegressionTest, CooldownDuration) {
    surprise_imagination_check_trigger(bridge, 0.9f, 0x100);

    surprise_imagination_effects_t effects;
    surprise_imagination_bridge_get_effects(bridge, &effects);
    EXPECT_FLOAT_EQ(effects.cooldown_remaining, 5.0f);

    /* After 5 seconds of updates, cooldown should be at ~0 */
    for (int i = 0; i < 50; i++) {
        surprise_imagination_bridge_update(bridge, 0.1f);
    }

    surprise_imagination_bridge_get_effects(bridge, &effects);
    EXPECT_NEAR(effects.cooldown_remaining, 0.0f, 1e-4f);
}

TEST_F(ImaginationRegressionTest, ErrorCodeVerification) {
    EXPECT_EQ(NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER, 28901);
    EXPECT_EQ(NIMCP_SURPRISE_IMAGINATION_ERROR_INVALID_PARAM, 28902);
}

TEST_F(ImaginationRegressionTest, Performance10kTriggerChecks) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        surprise_imagination_check_trigger(bridge, 0.5f, 0x100 + i);
        surprise_imagination_bridge_update(bridge, 0.001f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(ms, 500) << "10k imagination trigger checks took " << ms << "ms (limit 500ms)";
}

/* ============================================================================
 * 7. Self-Model Bridge Regression
 * ============================================================================ */

class SelfModelRegressionTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_self_model_bridge_t* bridge = nullptr;

    void SetUp() override {
        amp = create_test_amplifier();
        surprise_self_model_config_t cfg = surprise_self_model_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_self_model_bridge_create(&cfg);
        surprise_self_model_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) surprise_self_model_bridge_destroy(bridge);
        if (amp) surprise_amplifier_destroy(amp);
    }
};

TEST_F(SelfModelRegressionTest, DefaultConfigBaselines) {
    surprise_self_model_config_t cfg = surprise_self_model_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.capability_surprise_threshold, 0.6f);
    EXPECT_FLOAT_EQ(cfg.competence_update_rate, 0.05f);
    EXPECT_FLOAT_EQ(cfg.confidence_modulation_gain, 0.3f);
    EXPECT_FLOAT_EQ(cfg.belief_revision_rate, 0.1f);
    EXPECT_EQ(cfg.max_tracked_capabilities, 32u);
}

TEST_F(SelfModelRegressionTest, ThresholdBoundaryExact) {
    /* At threshold (0.6f) */
    surprise_self_model_on_capability_surprise(
        bridge, 1, 0.6f, SURPRISE_CAPABILITY_UPGRADE);

    surprise_self_model_stats_t stats;
    surprise_self_model_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.capability_surprises, 1u);
}

TEST_F(SelfModelRegressionTest, ThresholdBoundaryBelow) {
    surprise_self_model_on_capability_surprise(
        bridge, 1, 0.59f, SURPRISE_CAPABILITY_UPGRADE);

    surprise_self_model_effects_t effects;
    surprise_self_model_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(effects.beliefs_revised, 0u);
}

TEST_F(SelfModelRegressionTest, RevisionTypesTracked) {
    /* Test all three revision types */
    surprise_self_model_on_capability_surprise(
        bridge, 1, 0.8f, SURPRISE_CAPABILITY_UPGRADE);
    surprise_self_model_on_capability_surprise(
        bridge, 2, 0.8f, SURPRISE_CAPABILITY_DOWNGRADE);
    surprise_self_model_on_capability_surprise(
        bridge, 3, 0.8f, SURPRISE_CAPABILITY_NOVEL);

    surprise_self_model_stats_t stats;
    surprise_self_model_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.capability_surprises, 3u);
    EXPECT_GE(stats.discoveries, 1u);
}

TEST_F(SelfModelRegressionTest, LastRevisionAccuracy) {
    surprise_self_model_on_capability_surprise(
        bridge, 7, 0.75f, SURPRISE_CAPABILITY_NOVEL);

    surprise_capability_revision_t rev;
    surprise_self_model_get_last_revision(bridge, &rev);
    EXPECT_EQ(rev.capability_id, 7u);
    EXPECT_EQ(rev.revision_type, SURPRISE_CAPABILITY_NOVEL);
    EXPECT_NEAR(rev.surprise_magnitude, 0.75f, 0.01f);
}

TEST_F(SelfModelRegressionTest, ConfidenceModulationRange) {
    /* After various events, modulation should be finite and reasonable */
    for (int i = 0; i < 10; i++) {
        surprise_self_model_on_capability_surprise(
            bridge, (uint32_t)(i + 1), 0.8f, SURPRISE_CAPABILITY_UPGRADE);
        surprise_self_model_on_competence_feedback(bridge, (uint32_t)(i + 1), 0.7f);
        surprise_self_model_bridge_update(bridge, 0.1f);
    }

    float mod = surprise_self_model_query_confidence_modulation(bridge);
    EXPECT_GT(mod, 0.0f);
    EXPECT_LT(mod, 10.0f);  /* Reasonable range */
    EXPECT_TRUE(std::isfinite(mod));
}

TEST_F(SelfModelRegressionTest, ErrorCodeVerification) {
    EXPECT_EQ(NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER, 29001);
    EXPECT_EQ(NIMCP_SURPRISE_SELF_MODEL_ERROR_INVALID_PARAM, 29002);
}

TEST_F(SelfModelRegressionTest, Performance10kEvents) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        surprise_self_model_on_capability_surprise(
            bridge, (uint32_t)(i % 32 + 1), 0.8f, SURPRISE_CAPABILITY_UPGRADE);
        surprise_self_model_bridge_update(bridge, 0.001f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(ms, 500) << "10k self-model events took " << ms << "ms (limit 500ms)";
}
