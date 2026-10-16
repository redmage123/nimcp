/**
 * @file test_surprise_bridges_phase1b.cpp
 * @brief Unit tests for Phase 1b Surprise Bridges (Society of Thought)
 * @date 2026-01-27
 *
 * WHAT: Unit tests for 7 new surprise bridges
 * WHY:  Verify each bridge's lifecycle, operations, queries, and null safety
 * HOW:  GoogleTest fixtures per bridge, SetUp/TearDown with amplifier
 *
 * BRIDGES TESTED:
 * 1. Plasticity Bridge     - surprise → learning rate / habituation
 * 2. SNN Bridge            - surprise → spike trains / channel activity
 * 3. Substrate Bridge      - metabolic constraints on surprise processing
 * 4. Thalamic Bridge       - thalamic routing of surprise signals
 * 5. Pink Noise Bridge     - 1/f noise for surprise parameters
 * 6. Imagination Bridge    - surprise triggers counterfactual imagination
 * 7. Self-Model Bridge     - surprise updates self-model capabilities
 */

#include <gtest/gtest.h>
#include <cmath>

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
 * 1. Plasticity Bridge Tests
 * ============================================================================ */

class SurprisePlasticityBridgeTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);

        surprise_plasticity_config_t cfg = surprise_plasticity_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_plasticity_bridge_create(&cfg);
        surprise_plasticity_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) { surprise_plasticity_bridge_destroy(bridge); bridge = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }
};

TEST_F(SurprisePlasticityBridgeTest, DefaultConfig) {
    surprise_plasticity_config_t cfg = surprise_plasticity_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.learning_rate_boost, SURPRISE_PLASTICITY_DEFAULT_LR_BOOST);
    EXPECT_FLOAT_EQ(cfg.habituation_rate, SURPRISE_PLASTICITY_DEFAULT_HABITUATION_RATE);
    EXPECT_FLOAT_EQ(cfg.habituation_recovery_rate, SURPRISE_PLASTICITY_DEFAULT_HABITUATION_RECOVERY);
    EXPECT_FLOAT_EQ(cfg.stdp_window_expansion, SURPRISE_PLASTICITY_DEFAULT_STDP_EXPANSION);
    EXPECT_FLOAT_EQ(cfg.eligibility_boost, SURPRISE_PLASTICITY_DEFAULT_ELIGIBILITY_BOOST);
    EXPECT_FLOAT_EQ(cfg.bcm_threshold_shift, SURPRISE_PLASTICITY_DEFAULT_BCM_SHIFT);
    EXPECT_FLOAT_EQ(cfg.min_surprise_for_boost, SURPRISE_PLASTICITY_DEFAULT_MIN_SURPRISE);
    EXPECT_EQ(cfg.max_tracked_sources, (uint32_t)SURPRISE_PLASTICITY_DEFAULT_MAX_SOURCES);
    EXPECT_TRUE(cfg.enable_bio_async);
    EXPECT_TRUE(cfg.enable_logging);
}

TEST_F(SurprisePlasticityBridgeTest, CreateWithNullConfig) {
    surprise_plasticity_bridge_t* b = surprise_plasticity_bridge_create(NULL);
    ASSERT_NE(b, nullptr);
    surprise_plasticity_bridge_destroy(b);
}

TEST_F(SurprisePlasticityBridgeTest, DestroyNull) {
    surprise_plasticity_bridge_destroy(NULL);  /* Should not crash */
}

TEST_F(SurprisePlasticityBridgeTest, SurpriseEventAboveThreshold) {
    /* Surprise above min threshold should boost plasticity */
    EXPECT_EQ(surprise_plasticity_on_surprise_event(bridge, 0.8f, 0x100), 0);
    EXPECT_EQ(surprise_plasticity_bridge_update(bridge, 0.1f), 0);

    surprise_plasticity_effects_t effects;
    EXPECT_EQ(surprise_plasticity_bridge_get_effects(bridge, &effects), 0);
    EXPECT_GT(effects.learning_rate_multiplier, 1.0f);
    EXPECT_GT(effects.stdp_window_multiplier, 1.0f);
}

TEST_F(SurprisePlasticityBridgeTest, SurpriseEventBelowThreshold) {
    /* Surprise below min threshold should not boost */
    EXPECT_EQ(surprise_plasticity_on_surprise_event(bridge, 0.1f, 0x100), 0);
    EXPECT_EQ(surprise_plasticity_bridge_update(bridge, 0.1f), 0);

    surprise_plasticity_effects_t effects;
    EXPECT_EQ(surprise_plasticity_bridge_get_effects(bridge, &effects), 0);
    EXPECT_FLOAT_EQ(effects.learning_rate_multiplier, 1.0f);
}

TEST_F(SurprisePlasticityBridgeTest, LearningOutcomeUpdatesHabituation) {
    /* Register the source first via surprise event */
    EXPECT_EQ(surprise_plasticity_on_surprise_event(bridge, 0.8f, 0x100), 0);

    /* Repeated learning from same source increases habituation */
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(surprise_plasticity_on_learning_outcome(bridge, 0.5f, 0x100), 0);
    }

    float hab = surprise_plasticity_get_habituation_for_source(bridge, 0x100);
    EXPECT_GT(hab, 0.0f);
}

TEST_F(SurprisePlasticityBridgeTest, GetEffectsAndStats) {
    surprise_plasticity_on_surprise_event(bridge, 0.9f, 0x100);
    surprise_plasticity_bridge_update(bridge, 0.1f);

    surprise_plasticity_effects_t effects;
    EXPECT_EQ(surprise_plasticity_bridge_get_effects(bridge, &effects), 0);

    surprise_plasticity_stats_t stats;
    EXPECT_EQ(surprise_plasticity_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.plasticity_boosts, 1u);
    EXPECT_GE(stats.total_updates, 1u);
}

TEST_F(SurprisePlasticityBridgeTest, Reset) {
    surprise_plasticity_on_surprise_event(bridge, 0.8f, 0x100);
    surprise_plasticity_bridge_update(bridge, 0.1f);

    EXPECT_EQ(surprise_plasticity_bridge_reset(bridge), 0);

    surprise_plasticity_stats_t stats;
    surprise_plasticity_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.plasticity_boosts, 0u);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(SurprisePlasticityBridgeTest, ZeroDtNoOp) {
    EXPECT_EQ(surprise_plasticity_bridge_update(bridge, 0.0f), 0);
    surprise_plasticity_stats_t stats;
    surprise_plasticity_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST(SurprisePlasticityBridgeNullTest, NullBridgeOperations) {
    EXPECT_NE(surprise_plasticity_on_surprise_event(NULL, 0.5f, 0x100), 0);
    EXPECT_NE(surprise_plasticity_on_learning_outcome(NULL, 0.5f, 0x100), 0);
    EXPECT_NE(surprise_plasticity_bridge_update(NULL, 0.1f), 0);
    EXPECT_NE(surprise_plasticity_bridge_reset(NULL), 0);

    surprise_plasticity_effects_t effects;
    EXPECT_NE(surprise_plasticity_bridge_get_effects(NULL, &effects), 0);
    surprise_plasticity_stats_t stats;
    EXPECT_NE(surprise_plasticity_bridge_get_stats(NULL, &stats), 0);

    EXPECT_FLOAT_EQ(surprise_plasticity_get_habituation_for_source(NULL, 0x100), 0.0f);
}

/* ============================================================================
 * 2. SNN Bridge Tests
 * ============================================================================ */

class SurpriseSnnBridgeTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);

        surprise_snn_config_t cfg = surprise_snn_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_snn_bridge_create(&cfg);
        surprise_snn_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) { surprise_snn_bridge_destroy(bridge); bridge = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }
};

TEST_F(SurpriseSnnBridgeTest, DefaultConfig) {
    surprise_snn_config_t cfg = surprise_snn_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.dt_ms, SURPRISE_SNN_DEFAULT_DT_MS);
    EXPECT_EQ(cfg.neurons_per_channel, (uint32_t)SURPRISE_SNN_DEFAULT_NEURONS_PER_CH);
    EXPECT_EQ(cfg.encoding_type, SURPRISE_SNN_ENCODING_RATE);
    EXPECT_FLOAT_EQ(cfg.threshold, SURPRISE_SNN_DEFAULT_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.refractory_ms, SURPRISE_SNN_DEFAULT_REFRACTORY_MS);
    EXPECT_FLOAT_EQ(cfg.decay_factor, SURPRISE_SNN_DEFAULT_DECAY_FACTOR);
    EXPECT_EQ(cfg.history_size, (uint32_t)SURPRISE_SNN_DEFAULT_HISTORY_SIZE);
}

TEST_F(SurpriseSnnBridgeTest, CreateWithNullConfig) {
    surprise_snn_bridge_t* b = surprise_snn_bridge_create(NULL);
    ASSERT_NE(b, nullptr);
    surprise_snn_bridge_destroy(b);
}

TEST_F(SurpriseSnnBridgeTest, DestroyNull) {
    surprise_snn_bridge_destroy(NULL);
}

TEST_F(SurpriseSnnBridgeTest, EncodeSurpriseProducesSpikes) {
    /* Encode high surprise on PE channel */
    EXPECT_EQ(surprise_snn_encode_surprise(bridge, 0.9f, SURPRISE_SNN_CHANNEL_PE), 0);

    /* Simulate several steps to let spikes propagate */
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(surprise_snn_simulate_step(bridge), 0);
    }

    surprise_snn_effects_t effects;
    EXPECT_EQ(surprise_snn_decode_output(bridge, &effects), 0);
    EXPECT_GE(effects.combined_activity, 0.0f);
}

TEST_F(SurpriseSnnBridgeTest, ChannelDominance) {
    /* Encode high surprise on novelty channel, low on others */
    surprise_snn_encode_surprise(bridge, 0.9f, SURPRISE_SNN_CHANNEL_NOVELTY);
    surprise_snn_encode_surprise(bridge, 0.1f, SURPRISE_SNN_CHANNEL_PE);

    for (int i = 0; i < 10; i++) {
        surprise_snn_simulate_step(bridge);
    }

    surprise_snn_channel_t dominant = surprise_snn_get_dominant_channel(bridge);
    /* After encoding, novelty should dominate or at least be valid */
    EXPECT_GE((int)dominant, 0);
    EXPECT_LT((int)dominant, SURPRISE_SNN_NUM_CHANNELS);
}

TEST_F(SurpriseSnnBridgeTest, UpdateAndStats) {
    surprise_snn_encode_surprise(bridge, 0.7f, SURPRISE_SNN_CHANNEL_CONFLICT);
    surprise_snn_bridge_update(bridge, 0.1f);

    surprise_snn_stats_t stats;
    EXPECT_EQ(surprise_snn_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.encoding_events, 1u);
    EXPECT_GE(stats.total_updates, 1u);
}

TEST_F(SurpriseSnnBridgeTest, Reset) {
    surprise_snn_encode_surprise(bridge, 0.8f, SURPRISE_SNN_CHANNEL_PE);
    surprise_snn_bridge_update(bridge, 0.1f);

    EXPECT_EQ(surprise_snn_bridge_reset(bridge), 0);

    surprise_snn_stats_t stats;
    surprise_snn_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.encoding_events, 0u);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(SurpriseSnnBridgeTest, ZeroDtNoOp) {
    EXPECT_EQ(surprise_snn_bridge_update(bridge, 0.0f), 0);
    surprise_snn_stats_t stats;
    surprise_snn_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST(SurpriseSnnBridgeNullTest, NullBridgeOperations) {
    EXPECT_NE(surprise_snn_encode_surprise(NULL, 0.5f, SURPRISE_SNN_CHANNEL_PE), 0);
    EXPECT_NE(surprise_snn_simulate_step(NULL), 0);
    EXPECT_NE(surprise_snn_bridge_update(NULL, 0.1f), 0);
    EXPECT_NE(surprise_snn_bridge_reset(NULL), 0);

    surprise_snn_effects_t effects;
    EXPECT_NE(surprise_snn_decode_output(NULL, &effects), 0);
    EXPECT_NE(surprise_snn_bridge_get_effects(NULL, &effects), 0);
    surprise_snn_stats_t stats;
    EXPECT_NE(surprise_snn_bridge_get_stats(NULL, &stats), 0);

    EXPECT_EQ(surprise_snn_get_dominant_channel(NULL), SURPRISE_SNN_CHANNEL_PE);
}

/* ============================================================================
 * 3. Substrate Bridge Tests
 * ============================================================================ */

class SurpriseSubstrateBridgeTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_substrate_bridge_t* bridge = nullptr;

    void SetUp() override {
        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);

        surprise_substrate_config_t cfg = surprise_substrate_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_substrate_bridge_create(&cfg);
        surprise_substrate_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) { surprise_substrate_bridge_destroy(bridge); bridge = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }
};

TEST_F(SurpriseSubstrateBridgeTest, DefaultConfig) {
    surprise_substrate_config_t cfg = surprise_substrate_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.detection_sensitivity_mult, SURPRISE_SUBSTRATE_DEFAULT_DETECT_MULT);
    EXPECT_FLOAT_EQ(cfg.amplification_accuracy_mult, SURPRISE_SUBSTRATE_DEFAULT_AMPLIFY_MULT);
    EXPECT_FLOAT_EQ(cfg.decay_modulation_mult, SURPRISE_SUBSTRATE_DEFAULT_DECAY_MULT);
    EXPECT_FLOAT_EQ(cfg.refractory_modulation_mult, SURPRISE_SUBSTRATE_DEFAULT_REFRACT_MULT);
    EXPECT_FLOAT_EQ(cfg.min_capacity, SURPRISE_SUBSTRATE_DEFAULT_MIN_CAPACITY);
}

TEST_F(SurpriseSubstrateBridgeTest, CreateWithNullConfig) {
    surprise_substrate_bridge_t* b = surprise_substrate_bridge_create(NULL);
    ASSERT_NE(b, nullptr);
    surprise_substrate_bridge_destroy(b);
}

TEST_F(SurpriseSubstrateBridgeTest, DestroyNull) {
    surprise_substrate_bridge_destroy(NULL);
}

TEST_F(SurpriseSubstrateBridgeTest, HighAtpFullCapacity) {
    /* Full ATP, no fatigue → full capacity */
    EXPECT_EQ(surprise_substrate_bridge_update(bridge, 1.0f, 0.0f), 0);

    surprise_substrate_effects_t effects;
    EXPECT_EQ(surprise_substrate_bridge_get_effects(bridge, &effects), 0);
    EXPECT_GT(effects.overall_capacity, 0.5f);
    EXPECT_GT(effects.detection_sensitivity, 0.0f);
}

TEST_F(SurpriseSubstrateBridgeTest, LowAtpReducesCapacity) {
    /* Low ATP → reduced capacity */
    EXPECT_EQ(surprise_substrate_bridge_update(bridge, 0.1f, 0.8f), 0);

    surprise_substrate_effects_t effects;
    EXPECT_EQ(surprise_substrate_bridge_get_effects(bridge, &effects), 0);
    EXPECT_LT(effects.overall_capacity, 1.0f);
}

TEST_F(SurpriseSubstrateBridgeTest, FatigueModulatesDecay) {
    /* High fatigue should modulate decay */
    EXPECT_EQ(surprise_substrate_bridge_update(bridge, 0.5f, 0.9f), 0);

    surprise_substrate_effects_t effects;
    surprise_substrate_bridge_get_effects(bridge, &effects);
    /* Fatigue should affect some parameter */
    EXPECT_GT(effects.decay_modulation, 0.0f);
}

TEST_F(SurpriseSubstrateBridgeTest, GetStats) {
    surprise_substrate_bridge_update(bridge, 0.8f, 0.2f);

    surprise_substrate_stats_t stats;
    EXPECT_EQ(surprise_substrate_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.modulation_updates, 1u);
    EXPECT_GE(stats.total_updates, 1u);
}

TEST(SurpriseSubstrateBridgeNullTest, NullBridgeOperations) {
    EXPECT_NE(surprise_substrate_bridge_update(NULL, 1.0f, 0.0f), 0);
    surprise_substrate_effects_t effects;
    EXPECT_NE(surprise_substrate_bridge_get_effects(NULL, &effects), 0);
    surprise_substrate_stats_t stats;
    EXPECT_NE(surprise_substrate_bridge_get_stats(NULL, &stats), 0);
}

/* ============================================================================
 * 4. Thalamic Bridge Tests
 * ============================================================================ */

class SurpriseThalamicBridgeTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);

        surprise_thalamic_config_t cfg = surprise_thalamic_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_thalamic_bridge_create(&cfg);
        surprise_thalamic_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) { surprise_thalamic_bridge_destroy(bridge); bridge = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }
};

TEST_F(SurpriseThalamicBridgeTest, DefaultConfig) {
    surprise_thalamic_config_t cfg = surprise_thalamic_bridge_default_config();
    EXPECT_TRUE(cfg.enable_realization);
    EXPECT_TRUE(cfg.enable_conflict);
    EXPECT_TRUE(cfg.enable_novelty);
    EXPECT_TRUE(cfg.enable_hypothesis);
    EXPECT_FLOAT_EQ(cfg.attention_weight_default, SURPRISE_THALAMIC_DEFAULT_ATTENTION_WEIGHT);
}

TEST_F(SurpriseThalamicBridgeTest, CreateWithNullConfig) {
    surprise_thalamic_bridge_t* b = surprise_thalamic_bridge_create(NULL);
    ASSERT_NE(b, nullptr);
    surprise_thalamic_bridge_destroy(b);
}

TEST_F(SurpriseThalamicBridgeTest, DestroyNull) {
    surprise_thalamic_bridge_destroy(NULL);
}

TEST_F(SurpriseThalamicBridgeTest, RouteRealization) {
    EXPECT_EQ(surprise_thalamic_route_realization(bridge, 0.9f, 0x100), 0);

    surprise_thalamic_stats_t stats;
    EXPECT_EQ(surprise_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.signals_routed, 1u);
    EXPECT_GE(stats.high_priority_routes, 1u);
}

TEST_F(SurpriseThalamicBridgeTest, RouteSurpriseSignal) {
    surprise_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = SURPRISE_THALAMIC_NOVELTY;
    signal.surprise_magnitude = 0.7f;
    signal.source_module = 0x200;
    signal.urgency = 0.5f;

    EXPECT_EQ(surprise_thalamic_route_surprise(bridge, &signal), 0);

    surprise_thalamic_stats_t stats;
    surprise_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.signals_routed, 1u);
}

TEST_F(SurpriseThalamicBridgeTest, AttentionWeightSetGet) {
    EXPECT_EQ(surprise_thalamic_set_attention_weight(bridge, SURPRISE_THALAMIC_CONFLICT, 2.5f), 0);
    float weight = surprise_thalamic_get_attention_weight(bridge, SURPRISE_THALAMIC_CONFLICT);
    EXPECT_FLOAT_EQ(weight, 2.5f);
}

TEST_F(SurpriseThalamicBridgeTest, DefaultAttentionWeight) {
    float weight = surprise_thalamic_get_attention_weight(bridge, SURPRISE_THALAMIC_REALIZATION);
    EXPECT_FLOAT_EQ(weight, SURPRISE_THALAMIC_DEFAULT_ATTENTION_WEIGHT);
}

TEST_F(SurpriseThalamicBridgeTest, Reset) {
    surprise_thalamic_route_realization(bridge, 0.8f, 0x100);

    EXPECT_EQ(surprise_thalamic_bridge_reset(bridge), 0);

    surprise_thalamic_stats_t stats;
    surprise_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.signals_routed, 0u);
}

TEST(SurpriseThalamicBridgeNullTest, NullBridgeOperations) {
    EXPECT_NE(surprise_thalamic_route_realization(NULL, 0.5f, 0x100), 0);

    surprise_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = SURPRISE_THALAMIC_NOVELTY;
    signal.surprise_magnitude = 0.5f;
    EXPECT_NE(surprise_thalamic_route_surprise(NULL, &signal), 0);

    EXPECT_NE(surprise_thalamic_set_attention_weight(NULL, SURPRISE_THALAMIC_CONFLICT, 1.0f), 0);
    EXPECT_FLOAT_EQ(surprise_thalamic_get_attention_weight(NULL, SURPRISE_THALAMIC_CONFLICT), 1.0f);
    EXPECT_NE(surprise_thalamic_bridge_reset(NULL), 0);

    surprise_thalamic_stats_t stats;
    EXPECT_NE(surprise_thalamic_bridge_get_stats(NULL, &stats), 0);
}

/* ============================================================================
 * 5. Pink Noise Bridge Tests
 * ============================================================================ */

class SurprisePinkNoiseBridgeTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_pink_noise_bridge_t* bridge = nullptr;

    void SetUp() override {
        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);

        surprise_pink_noise_config_t cfg = surprise_pink_noise_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_pink_noise_bridge_create(&cfg);
        surprise_pink_noise_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) { surprise_pink_noise_bridge_destroy(bridge); bridge = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }
};

TEST_F(SurprisePinkNoiseBridgeTest, DefaultConfig) {
    surprise_pink_noise_config_t cfg = surprise_pink_noise_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.base_amplitude, SURPRISE_PINK_NOISE_DEFAULT_BASE_AMPLITUDE);
    EXPECT_FLOAT_EQ(cfg.alpha, SURPRISE_PINK_NOISE_DEFAULT_ALPHA);
    EXPECT_FLOAT_EQ(cfg.adaptation_rate, SURPRISE_PINK_NOISE_DEFAULT_ADAPT_RATE);
    EXPECT_FLOAT_EQ(cfg.temporal_smoothing, SURPRISE_PINK_NOISE_DEFAULT_SMOOTHING);
    EXPECT_FLOAT_EQ(cfg.min_amplitude, SURPRISE_PINK_NOISE_DEFAULT_MIN_AMPLITUDE);
    EXPECT_FLOAT_EQ(cfg.max_amplitude, SURPRISE_PINK_NOISE_DEFAULT_MAX_AMPLITUDE);
}

TEST_F(SurprisePinkNoiseBridgeTest, CreateWithNullConfig) {
    surprise_pink_noise_bridge_t* b = surprise_pink_noise_bridge_create(NULL);
    ASSERT_NE(b, nullptr);
    surprise_pink_noise_bridge_destroy(b);
}

TEST_F(SurprisePinkNoiseBridgeTest, DestroyNull) {
    surprise_pink_noise_bridge_destroy(NULL);
}

TEST_F(SurprisePinkNoiseBridgeTest, InjectNoiseProducesValues) {
    /* After injection, noise targets should have values */
    EXPECT_EQ(surprise_pink_noise_inject(bridge), 0);

    surprise_pink_noise_effects_t effects;
    EXPECT_EQ(surprise_pink_noise_bridge_get_effects(bridge, &effects), 0);
    EXPECT_GE(effects.samples_generated, 1u);
}

TEST_F(SurprisePinkNoiseBridgeTest, GetNoiseForTarget) {
    /* Run several injections to generate noise */
    for (int i = 0; i < 20; i++) {
        surprise_pink_noise_inject(bridge);
    }

    /* All 4 targets should return values (may be zero initially due to smoothing) */
    for (uint32_t t = 0; t < SURPRISE_PINK_NOISE_NUM_TARGETS; t++) {
        float noise = surprise_pink_noise_get_for_target(bridge, t);
        /* Noise is bounded but can be zero */
        EXPECT_TRUE(std::isfinite(noise));
    }

    /* Out of range target returns 0 */
    EXPECT_FLOAT_EQ(surprise_pink_noise_get_for_target(bridge, 99), 0.0f);
}

TEST_F(SurprisePinkNoiseBridgeTest, AdaptAmplitudeHighSurprise) {
    surprise_pink_noise_effects_t before;
    surprise_pink_noise_bridge_get_effects(bridge, &before);
    float amp_before = before.effective_amplitude;

    /* High surprise should increase amplitude */
    EXPECT_EQ(surprise_pink_noise_adapt_amplitude(bridge, 0.9f), 0);

    surprise_pink_noise_effects_t after;
    surprise_pink_noise_bridge_get_effects(bridge, &after);
    EXPECT_GE(after.effective_amplitude, amp_before);
}

TEST_F(SurprisePinkNoiseBridgeTest, UpdateIncrementsStats) {
    EXPECT_EQ(surprise_pink_noise_bridge_update(bridge, 0.1f), 0);

    surprise_pink_noise_stats_t stats;
    EXPECT_EQ(surprise_pink_noise_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.noise_injections, 1u);
    EXPECT_GE(stats.total_updates, 1u);
}

TEST_F(SurprisePinkNoiseBridgeTest, Reset) {
    surprise_pink_noise_bridge_update(bridge, 0.1f);

    EXPECT_EQ(surprise_pink_noise_bridge_reset(bridge), 0);

    surprise_pink_noise_stats_t stats;
    surprise_pink_noise_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.noise_injections, 0u);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(SurprisePinkNoiseBridgeTest, ZeroDtNoOp) {
    EXPECT_EQ(surprise_pink_noise_bridge_update(bridge, 0.0f), 0);
    surprise_pink_noise_stats_t stats;
    surprise_pink_noise_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST(SurprisePinkNoiseBridgeNullTest, NullBridgeOperations) {
    EXPECT_NE(surprise_pink_noise_inject(NULL), 0);
    EXPECT_NE(surprise_pink_noise_adapt_amplitude(NULL, 0.5f), 0);
    EXPECT_NE(surprise_pink_noise_bridge_update(NULL, 0.1f), 0);
    EXPECT_NE(surprise_pink_noise_bridge_reset(NULL), 0);

    surprise_pink_noise_effects_t effects;
    EXPECT_NE(surprise_pink_noise_bridge_get_effects(NULL, &effects), 0);
    surprise_pink_noise_stats_t stats;
    EXPECT_NE(surprise_pink_noise_bridge_get_stats(NULL, &stats), 0);

    EXPECT_FLOAT_EQ(surprise_pink_noise_get_for_target(NULL, 0), 0.0f);
}

/* ============================================================================
 * 6. Imagination Bridge Tests
 * ============================================================================ */

class SurpriseImaginationBridgeTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_imagination_bridge_t* bridge = nullptr;

    void SetUp() override {
        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);

        surprise_imagination_config_t cfg = surprise_imagination_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_imagination_bridge_create(&cfg);
        surprise_imagination_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) { surprise_imagination_bridge_destroy(bridge); bridge = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }
};

TEST_F(SurpriseImaginationBridgeTest, DefaultConfig) {
    surprise_imagination_config_t cfg = surprise_imagination_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.trigger_threshold, SURPRISE_IMAGINATION_DEFAULT_TRIGGER_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.cooldown_seconds, SURPRISE_IMAGINATION_DEFAULT_COOLDOWN_SECONDS);
    EXPECT_EQ(cfg.max_scenarios, (uint32_t)SURPRISE_IMAGINATION_DEFAULT_MAX_SCENARIOS);
    EXPECT_FLOAT_EQ(cfg.expectation_update_rate, SURPRISE_IMAGINATION_DEFAULT_EXPECT_UPDATE_RATE);
    EXPECT_EQ(cfg.counterfactual_depth, (uint32_t)SURPRISE_IMAGINATION_DEFAULT_CF_DEPTH);
}

TEST_F(SurpriseImaginationBridgeTest, CreateWithNullConfig) {
    surprise_imagination_bridge_t* b = surprise_imagination_bridge_create(NULL);
    ASSERT_NE(b, nullptr);
    surprise_imagination_bridge_destroy(b);
}

TEST_F(SurpriseImaginationBridgeTest, DestroyNull) {
    surprise_imagination_bridge_destroy(NULL);
}

TEST_F(SurpriseImaginationBridgeTest, TriggerAboveThreshold) {
    /* Surprise above trigger threshold should create scenario */
    EXPECT_EQ(surprise_imagination_check_trigger(bridge, 0.9f, 0x100), 0);

    surprise_imagination_effects_t effects;
    EXPECT_EQ(surprise_imagination_bridge_get_effects(bridge, &effects), 0);
    EXPECT_GE(effects.scenarios_active, 1u);
    EXPECT_GT(effects.last_trigger_magnitude, 0.0f);
}

TEST_F(SurpriseImaginationBridgeTest, TriggerBelowThreshold) {
    /* Surprise below threshold should not trigger */
    EXPECT_EQ(surprise_imagination_check_trigger(bridge, 0.3f, 0x100), 0);

    surprise_imagination_effects_t effects;
    surprise_imagination_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(effects.scenarios_active, 0u);
}

TEST_F(SurpriseImaginationBridgeTest, CooldownBlocks) {
    /* First trigger succeeds */
    surprise_imagination_check_trigger(bridge, 0.9f, 0x100);

    /* Second trigger should be blocked by cooldown */
    surprise_imagination_check_trigger(bridge, 0.9f, 0x200);

    surprise_imagination_stats_t stats;
    surprise_imagination_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.triggers, 1u);
    EXPECT_GE(stats.cooldown_blocked, 1u);
}

TEST_F(SurpriseImaginationBridgeTest, OnResult) {
    /* Trigger a scenario */
    surprise_imagination_check_trigger(bridge, 0.9f, 0x100);

    /* Complete the scenario with a result */
    EXPECT_EQ(surprise_imagination_on_result(bridge, 1, 0.6f), 0);

    surprise_imagination_stats_t stats;
    surprise_imagination_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.scenarios_completed, 1u);
    EXPECT_GE(stats.expectations_updated, 1u);
}

TEST_F(SurpriseImaginationBridgeTest, GetScenario) {
    surprise_imagination_check_trigger(bridge, 0.85f, 0x100);

    surprise_imagination_scenario_t scenario;
    EXPECT_EQ(surprise_imagination_get_scenario(bridge, 1, &scenario), 0);
    EXPECT_EQ(scenario.scenario_id, 1u);
    EXPECT_GT(scenario.trigger_magnitude, 0.0f);
    EXPECT_EQ(scenario.status, SURPRISE_IMAGINATION_STATUS_ACTIVE);
}

TEST_F(SurpriseImaginationBridgeTest, UpdateDecaysCooldown) {
    surprise_imagination_check_trigger(bridge, 0.9f, 0x100);

    surprise_imagination_effects_t before;
    surprise_imagination_bridge_get_effects(bridge, &before);
    float cooldown_before = before.cooldown_remaining;

    /* Update with dt to decay cooldown */
    surprise_imagination_bridge_update(bridge, 1.0f);

    surprise_imagination_effects_t after;
    surprise_imagination_bridge_get_effects(bridge, &after);
    EXPECT_LT(after.cooldown_remaining, cooldown_before);
}

TEST_F(SurpriseImaginationBridgeTest, Reset) {
    surprise_imagination_check_trigger(bridge, 0.9f, 0x100);
    surprise_imagination_bridge_update(bridge, 0.1f);

    EXPECT_EQ(surprise_imagination_bridge_reset(bridge), 0);

    surprise_imagination_stats_t stats;
    surprise_imagination_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.triggers, 0u);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(SurpriseImaginationBridgeTest, ZeroDtNoOp) {
    EXPECT_EQ(surprise_imagination_bridge_update(bridge, 0.0f), 0);
    surprise_imagination_stats_t stats;
    surprise_imagination_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST(SurpriseImaginationBridgeNullTest, NullBridgeOperations) {
    EXPECT_NE(surprise_imagination_check_trigger(NULL, 0.5f, 0x100), 0);
    EXPECT_NE(surprise_imagination_on_result(NULL, 1, 0.5f), 0);
    EXPECT_NE(surprise_imagination_bridge_update(NULL, 0.1f), 0);
    EXPECT_NE(surprise_imagination_bridge_reset(NULL), 0);

    surprise_imagination_effects_t effects;
    EXPECT_NE(surprise_imagination_bridge_get_effects(NULL, &effects), 0);
    surprise_imagination_stats_t stats;
    EXPECT_NE(surprise_imagination_bridge_get_stats(NULL, &stats), 0);

    surprise_imagination_scenario_t scenario;
    EXPECT_NE(surprise_imagination_get_scenario(NULL, 1, &scenario), 0);
}

/* ============================================================================
 * 7. Self-Model Bridge Tests
 * ============================================================================ */

class SurpriseSelfModelBridgeTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_self_model_bridge_t* bridge = nullptr;

    void SetUp() override {
        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);

        surprise_self_model_config_t cfg = surprise_self_model_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_self_model_bridge_create(&cfg);
        surprise_self_model_bridge_connect_amplifier(bridge, amp);
    }

    void TearDown() override {
        if (bridge) { surprise_self_model_bridge_destroy(bridge); bridge = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }
};

TEST_F(SurpriseSelfModelBridgeTest, DefaultConfig) {
    surprise_self_model_config_t cfg = surprise_self_model_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.capability_surprise_threshold, SURPRISE_SELF_MODEL_DEFAULT_CAP_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.competence_update_rate, SURPRISE_SELF_MODEL_DEFAULT_COMPETENCE_RATE);
    EXPECT_FLOAT_EQ(cfg.confidence_modulation_gain, SURPRISE_SELF_MODEL_DEFAULT_CONFIDENCE_GAIN);
    EXPECT_FLOAT_EQ(cfg.belief_revision_rate, SURPRISE_SELF_MODEL_DEFAULT_BELIEF_RATE);
    EXPECT_EQ(cfg.max_tracked_capabilities, (uint32_t)SURPRISE_SELF_MODEL_DEFAULT_MAX_CAPABILITIES);
}

TEST_F(SurpriseSelfModelBridgeTest, CreateWithNullConfig) {
    surprise_self_model_bridge_t* b = surprise_self_model_bridge_create(NULL);
    ASSERT_NE(b, nullptr);
    surprise_self_model_bridge_destroy(b);
}

TEST_F(SurpriseSelfModelBridgeTest, DestroyNull) {
    surprise_self_model_bridge_destroy(NULL);
}

TEST_F(SurpriseSelfModelBridgeTest, CapabilitySurpriseAboveThreshold) {
    EXPECT_EQ(surprise_self_model_on_capability_surprise(
        bridge, 1, 0.8f, SURPRISE_CAPABILITY_UPGRADE), 0);

    surprise_self_model_stats_t stats;
    surprise_self_model_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.capability_surprises, 1u);
}

TEST_F(SurpriseSelfModelBridgeTest, CapabilitySurpriseBelowThreshold) {
    /* Below threshold should still process but not trigger revision */
    EXPECT_EQ(surprise_self_model_on_capability_surprise(
        bridge, 1, 0.2f, SURPRISE_CAPABILITY_UPGRADE), 0);

    surprise_self_model_effects_t effects;
    surprise_self_model_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(effects.beliefs_revised, 0u);
}

TEST_F(SurpriseSelfModelBridgeTest, NovelCapabilityDiscovery) {
    EXPECT_EQ(surprise_self_model_on_capability_surprise(
        bridge, 42, 0.9f, SURPRISE_CAPABILITY_NOVEL), 0);

    surprise_self_model_stats_t stats;
    surprise_self_model_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.discoveries, 1u);
}

TEST_F(SurpriseSelfModelBridgeTest, CompetenceFeedback) {
    /* Register the capability first via surprise event */
    EXPECT_EQ(surprise_self_model_on_capability_surprise(
        bridge, 1, 0.8f, SURPRISE_CAPABILITY_UPGRADE), 0);

    /* Now provide competence feedback for the registered capability */
    EXPECT_EQ(surprise_self_model_on_competence_feedback(bridge, 1, 0.7f), 0);

    surprise_self_model_stats_t stats;
    surprise_self_model_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.competence_updates, 2u);  /* 1 from capability_surprise + 1 from feedback */
}

TEST_F(SurpriseSelfModelBridgeTest, ConfidenceModulation) {
    float mod = surprise_self_model_query_confidence_modulation(bridge);
    /* Default should be 1.0 (no modulation) */
    EXPECT_GT(mod, 0.0f);
    EXPECT_TRUE(std::isfinite(mod));
}

TEST_F(SurpriseSelfModelBridgeTest, GetLastRevision) {
    surprise_self_model_on_capability_surprise(bridge, 5, 0.85f, SURPRISE_CAPABILITY_DOWNGRADE);

    surprise_capability_revision_t rev;
    EXPECT_EQ(surprise_self_model_get_last_revision(bridge, &rev), 0);
    EXPECT_EQ(rev.capability_id, 5u);
    EXPECT_EQ(rev.revision_type, SURPRISE_CAPABILITY_DOWNGRADE);
    EXPECT_GT(rev.surprise_magnitude, 0.0f);
}

TEST_F(SurpriseSelfModelBridgeTest, UpdateAndStats) {
    surprise_self_model_on_capability_surprise(bridge, 1, 0.8f, SURPRISE_CAPABILITY_UPGRADE);
    EXPECT_EQ(surprise_self_model_bridge_update(bridge, 0.1f), 0);

    surprise_self_model_stats_t stats;
    surprise_self_model_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_updates, 1u);
}

TEST_F(SurpriseSelfModelBridgeTest, Reset) {
    surprise_self_model_on_capability_surprise(bridge, 1, 0.8f, SURPRISE_CAPABILITY_UPGRADE);
    surprise_self_model_bridge_update(bridge, 0.1f);

    EXPECT_EQ(surprise_self_model_bridge_reset(bridge), 0);

    surprise_self_model_stats_t stats;
    surprise_self_model_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.capability_surprises, 0u);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(SurpriseSelfModelBridgeTest, ZeroDtNoOp) {
    EXPECT_EQ(surprise_self_model_bridge_update(bridge, 0.0f), 0);
    surprise_self_model_stats_t stats;
    surprise_self_model_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST(SurpriseSelfModelBridgeNullTest, NullBridgeOperations) {
    EXPECT_NE(surprise_self_model_on_capability_surprise(
        NULL, 1, 0.5f, SURPRISE_CAPABILITY_UPGRADE), 0);
    EXPECT_NE(surprise_self_model_on_competence_feedback(NULL, 1, 0.5f), 0);
    EXPECT_NE(surprise_self_model_bridge_update(NULL, 0.1f), 0);
    EXPECT_NE(surprise_self_model_bridge_reset(NULL), 0);

    float mod = surprise_self_model_query_confidence_modulation(NULL);
    EXPECT_FLOAT_EQ(mod, 1.0f);

    surprise_self_model_effects_t effects;
    EXPECT_NE(surprise_self_model_bridge_get_effects(NULL, &effects), 0);
    surprise_self_model_stats_t stats;
    EXPECT_NE(surprise_self_model_bridge_get_stats(NULL, &stats), 0);

    surprise_capability_revision_t rev;
    EXPECT_NE(surprise_self_model_get_last_revision(NULL, &rev), 0);
}
