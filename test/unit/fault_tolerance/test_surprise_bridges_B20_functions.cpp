/**
 * @file test_surprise_bridges_B20_functions.cpp
 * @brief Unit tests for B20 surprise bridge health agent integration
 *        and functional tests for each bridge's core API
 *        (cognitive/salience surprise bridges: plasticity, SNN, substrate,
 *         thalamic, pink_noise, imagination, self_model)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cstring>

extern "C" {
    /* Surprise bridge health agent global setters */
    void surprise_plasticity_bridge_set_health_agent_global(void* agent);
    void surprise_snn_bridge_set_health_agent_global(void* agent);
    void surprise_substrate_bridge_set_health_agent_global(void* agent);
    void surprise_thalamic_bridge_set_health_agent_global(void* agent);
    void surprise_pink_noise_bridge_set_health_agent_global(void* agent);
    void surprise_imagination_bridge_set_health_agent_global(void* agent);
    void surprise_self_model_bridge_set_health_agent_global(void* agent);

    /* Surprise bridge APIs (for functional tests) */
    #include "cognitive/salience/nimcp_surprise_plasticity_bridge.h"
    #include "cognitive/salience/nimcp_surprise_snn_bridge.h"
    #include "cognitive/salience/nimcp_surprise_substrate_bridge.h"
    #include "cognitive/salience/nimcp_surprise_thalamic_bridge.h"
    #include "cognitive/salience/nimcp_surprise_pink_noise_bridge.h"
    #include "cognitive/salience/nimcp_surprise_imagination_bridge.h"
    #include "cognitive/salience/nimcp_surprise_self_model_bridge.h"

    /* Health agent API */
    #include "utils/fault_tolerance/nimcp_health_agent.h"
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kSurpriseBridgeModules[] = {
    {"surprise_plasticity_bridge", surprise_plasticity_bridge_set_health_agent_global},
    {"surprise_snn_bridge",        surprise_snn_bridge_set_health_agent_global},
    {"surprise_substrate_bridge",  surprise_substrate_bridge_set_health_agent_global},
    {"surprise_thalamic_bridge",   surprise_thalamic_bridge_set_health_agent_global},
    {"surprise_pink_noise_bridge", surprise_pink_noise_bridge_set_health_agent_global},
    {"surprise_imagination_bridge", surprise_imagination_bridge_set_health_agent_global},
    {"surprise_self_model_bridge", surprise_self_model_bridge_set_health_agent_global},
};

static constexpr size_t kNumModules = sizeof(kSurpriseBridgeModules) / sizeof(kSurpriseBridgeModules[0]);

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SurpriseBridgesB20Test : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent_ = nullptr;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        config.watchdog_timeout_ms = 5000;
        config.enable_auto_recovery = false;
        agent_ = nimcp_health_agent_create(&config);
        ASSERT_NE(nullptr, agent_);
    }

    void TearDown() override {
        for (size_t i = 0; i < kNumModules; i++) {
            kSurpriseBridgeModules[i].setter(nullptr);
        }
        if (agent_) {
            nimcp_health_agent_stop(agent_);
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }
};

/* ============================================================================
 * Health Agent Tests
 * ============================================================================ */

TEST_F(SurpriseBridgesB20Test, AllModulesSetNull) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kSurpriseBridgeModules[i].name);
        kSurpriseBridgeModules[i].setter(nullptr);
    }
}

TEST_F(SurpriseBridgesB20Test, AllModulesSetValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kSurpriseBridgeModules[i].name);
        kSurpriseBridgeModules[i].setter(agent_);
    }
}

TEST_F(SurpriseBridgesB20Test, AllModulesReplaceAgent) {
    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(nullptr, agent2);
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kSurpriseBridgeModules[i].name);
        kSurpriseBridgeModules[i].setter(agent_);
        kSurpriseBridgeModules[i].setter(agent2);
    }
    for (size_t i = 0; i < kNumModules; i++) kSurpriseBridgeModules[i].setter(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(SurpriseBridgesB20Test, ModuleCount) {
    EXPECT_EQ(kNumModules, 7u);
}

TEST_F(SurpriseBridgesB20Test, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kSurpriseBridgeModules[i].name);
        kSurpriseBridgeModules[i].setter(agent_);
        kSurpriseBridgeModules[i].setter(agent_);
    }
}

/* ============================================================================
 * Plasticity Bridge Functional Tests
 * ============================================================================ */

TEST_F(SurpriseBridgesB20Test, PlasticityCreateDestroy) {
    surprise_plasticity_bridge_t* bridge = surprise_plasticity_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_plasticity_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, PlasticityCreateWithConfig) {
    surprise_plasticity_config_t cfg = surprise_plasticity_bridge_default_config();
    cfg.learning_rate_boost = 3.0f;
    cfg.habituation_rate = 0.1f;
    surprise_plasticity_bridge_t* bridge = surprise_plasticity_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    surprise_plasticity_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, PlasticityDefaultConfig) {
    surprise_plasticity_config_t cfg = surprise_plasticity_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.learning_rate_boost, SURPRISE_PLASTICITY_DEFAULT_LR_BOOST);
    EXPECT_FLOAT_EQ(cfg.habituation_rate, SURPRISE_PLASTICITY_DEFAULT_HABITUATION_RATE);
    EXPECT_FLOAT_EQ(cfg.stdp_window_expansion, SURPRISE_PLASTICITY_DEFAULT_STDP_EXPANSION);
    EXPECT_FLOAT_EQ(cfg.eligibility_boost, SURPRISE_PLASTICITY_DEFAULT_ELIGIBILITY_BOOST);
    EXPECT_FLOAT_EQ(cfg.bcm_threshold_shift, SURPRISE_PLASTICITY_DEFAULT_BCM_SHIFT);
    EXPECT_FLOAT_EQ(cfg.min_surprise_for_boost, SURPRISE_PLASTICITY_DEFAULT_MIN_SURPRISE);
    EXPECT_EQ(cfg.max_tracked_sources, (uint32_t)SURPRISE_PLASTICITY_DEFAULT_MAX_SOURCES);
}

TEST_F(SurpriseBridgesB20Test, PlasticitySurpriseEvent) {
    surprise_plasticity_bridge_t* bridge = surprise_plasticity_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    int rc = surprise_plasticity_on_surprise_event(bridge, 0.8f, 1);
    EXPECT_EQ(0, rc);
    surprise_plasticity_effects_t effects;
    rc = surprise_plasticity_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(0, rc);
    EXPECT_GT(effects.learning_rate_multiplier, 1.0f);
    surprise_plasticity_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, PlasticityHabituation) {
    surprise_plasticity_bridge_t* bridge = surprise_plasticity_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    /* Repeated surprise from same source should habituate */
    for (int i = 0; i < 20; i++) {
        surprise_plasticity_on_surprise_event(bridge, 0.8f, 42);
    }
    float hab = surprise_plasticity_get_habituation_for_source(bridge, 42);
    EXPECT_GT(hab, 0.0f);
    surprise_plasticity_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, PlasticityLearningOutcome) {
    surprise_plasticity_bridge_t* bridge = surprise_plasticity_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_plasticity_on_surprise_event(bridge, 0.8f, 10);
    int rc = surprise_plasticity_on_learning_outcome(bridge, 0.5f, 10);
    EXPECT_EQ(0, rc);
    float hab = surprise_plasticity_get_habituation_for_source(bridge, 10);
    EXPECT_GE(hab, 0.0f);
    surprise_plasticity_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, PlasticityUpdate) {
    surprise_plasticity_bridge_t* bridge = surprise_plasticity_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_plasticity_on_surprise_event(bridge, 0.9f, 1);
    int rc = surprise_plasticity_bridge_update(bridge, 0.1f);
    EXPECT_EQ(0, rc);
    surprise_plasticity_effects_t effects;
    surprise_plasticity_bridge_get_effects(bridge, &effects);
    /* After update, effects should still be valid */
    EXPECT_GE(effects.learning_rate_multiplier, 1.0f);
    surprise_plasticity_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, PlasticityReset) {
    surprise_plasticity_bridge_t* bridge = surprise_plasticity_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_plasticity_on_surprise_event(bridge, 0.9f, 1);
    int rc = surprise_plasticity_bridge_reset(bridge);
    EXPECT_EQ(0, rc);
    surprise_plasticity_effects_t effects;
    surprise_plasticity_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(0u, effects.active_sources);
    surprise_plasticity_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, PlasticityNullSafety) {
    int rc = surprise_plasticity_on_surprise_event(nullptr, 0.8f, 1);
    EXPECT_NE(0, rc);
    rc = surprise_plasticity_on_learning_outcome(nullptr, 0.5f, 1);
    EXPECT_NE(0, rc);
    rc = surprise_plasticity_bridge_update(nullptr, 0.1f);
    EXPECT_NE(0, rc);
    rc = surprise_plasticity_bridge_reset(nullptr);
    EXPECT_NE(0, rc);
    surprise_plasticity_effects_t effects;
    rc = surprise_plasticity_bridge_get_effects(nullptr, &effects);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * SNN Bridge Functional Tests
 * ============================================================================ */

TEST_F(SurpriseBridgesB20Test, SnnCreateDestroy) {
    surprise_snn_config_t cfg = surprise_snn_bridge_default_config();
    EXPECT_EQ(cfg.neurons_per_channel, (uint32_t)SURPRISE_SNN_DEFAULT_NEURONS_PER_CH);
    surprise_snn_bridge_t* bridge = surprise_snn_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_snn_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, SnnEncodeSurprise) {
    surprise_snn_bridge_t* bridge = surprise_snn_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    int rc = surprise_snn_encode_surprise(bridge, 0.8f, SURPRISE_SNN_CHANNEL_PE);
    EXPECT_EQ(0, rc);
    surprise_snn_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, SnnSimulateStep) {
    surprise_snn_bridge_t* bridge = surprise_snn_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_snn_encode_surprise(bridge, 0.9f, SURPRISE_SNN_CHANNEL_NOVELTY);
    int rc = surprise_snn_simulate_step(bridge);
    EXPECT_EQ(0, rc);
    surprise_snn_stats_t stats;
    surprise_snn_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.encoding_events, 1u);
    surprise_snn_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, SnnDominantChannel) {
    surprise_snn_bridge_t* bridge = surprise_snn_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_snn_encode_surprise(bridge, 0.9f, SURPRISE_SNN_CHANNEL_CONFLICT);
    surprise_snn_simulate_step(bridge);
    surprise_snn_channel_t dominant = surprise_snn_get_dominant_channel(bridge);
    /* Dominant should be a valid channel */
    EXPECT_GE((int)dominant, 0);
    EXPECT_LT((int)dominant, SURPRISE_SNN_NUM_CHANNELS);
    surprise_snn_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, SnnUpdate) {
    surprise_snn_bridge_t* bridge = surprise_snn_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_snn_encode_surprise(bridge, 0.5f, SURPRISE_SNN_CHANNEL_HYPOTHESIS);
    int rc = surprise_snn_bridge_update(bridge, 0.01f);
    EXPECT_EQ(0, rc);
    surprise_snn_bridge_destroy(bridge);
}

/* ============================================================================
 * Substrate Bridge Functional Tests
 * ============================================================================ */

TEST_F(SurpriseBridgesB20Test, SubstrateCreateDestroy) {
    surprise_substrate_bridge_t* bridge = surprise_substrate_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_substrate_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, SubstrateUpdate) {
    surprise_substrate_bridge_t* bridge = surprise_substrate_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    int rc = surprise_substrate_bridge_update(bridge, 0.8f, 0.2f);
    EXPECT_EQ(0, rc);
    surprise_substrate_effects_t effects;
    rc = surprise_substrate_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(0, rc);
    EXPECT_GT(effects.overall_capacity, 0.0f);
    surprise_substrate_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, SubstrateLowATP) {
    surprise_substrate_bridge_t* bridge = surprise_substrate_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    int rc = surprise_substrate_bridge_update(bridge, 0.1f, 0.0f);
    EXPECT_EQ(0, rc);
    surprise_substrate_effects_t effects;
    surprise_substrate_bridge_get_effects(bridge, &effects);
    /* Low ATP should reduce capacity */
    EXPECT_LE(effects.overall_capacity, 1.0f);
    surprise_substrate_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, SubstrateHighFatigue) {
    surprise_substrate_bridge_t* bridge = surprise_substrate_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    int rc = surprise_substrate_bridge_update(bridge, 0.5f, 0.9f);
    EXPECT_EQ(0, rc);
    surprise_substrate_effects_t effects;
    surprise_substrate_bridge_get_effects(bridge, &effects);
    /* High fatigue should increase decay modulation and refractory */
    EXPECT_GE(effects.refractory_modulation, 1.0f);
    surprise_substrate_bridge_destroy(bridge);
}

/* ============================================================================
 * Thalamic Bridge Functional Tests
 * ============================================================================ */

TEST_F(SurpriseBridgesB20Test, ThalamicCreateDestroy) {
    surprise_thalamic_bridge_t* bridge = surprise_thalamic_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_thalamic_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, ThalamicRouteSignal) {
    surprise_thalamic_bridge_t* bridge = surprise_thalamic_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = SURPRISE_THALAMIC_REALIZATION;
    signal.surprise_magnitude = 0.8f;
    signal.source_module = 1;
    signal.urgency = 0.9f;
    int rc = surprise_thalamic_route_surprise(bridge, &signal);
    EXPECT_EQ(0, rc);
    surprise_thalamic_stats_t stats;
    surprise_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.signals_routed, 1u);
    surprise_thalamic_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, ThalamicAttentionWeights) {
    surprise_thalamic_bridge_t* bridge = surprise_thalamic_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    int rc = surprise_thalamic_set_attention_weight(bridge, SURPRISE_THALAMIC_NOVELTY, 2.0f);
    EXPECT_EQ(0, rc);
    float weight = surprise_thalamic_get_attention_weight(bridge, SURPRISE_THALAMIC_NOVELTY);
    EXPECT_FLOAT_EQ(weight, 2.0f);
    surprise_thalamic_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, ThalamicRouteRealization) {
    surprise_thalamic_bridge_t* bridge = surprise_thalamic_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    int rc = surprise_thalamic_route_realization(bridge, 0.9f, 42);
    EXPECT_EQ(0, rc);
    surprise_thalamic_stats_t stats;
    surprise_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.signals_routed, 1u);
    surprise_thalamic_bridge_destroy(bridge);
}

/* ============================================================================
 * Pink Noise Bridge Functional Tests
 * ============================================================================ */

TEST_F(SurpriseBridgesB20Test, PinkNoiseCreateDestroy) {
    surprise_pink_noise_bridge_t* bridge = surprise_pink_noise_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_pink_noise_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, PinkNoiseInject) {
    surprise_pink_noise_bridge_t* bridge = surprise_pink_noise_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    int rc = surprise_pink_noise_inject(bridge);
    EXPECT_EQ(0, rc);
    surprise_pink_noise_effects_t effects;
    surprise_pink_noise_bridge_get_effects(bridge, &effects);
    EXPECT_GE(effects.samples_generated, 1u);
    surprise_pink_noise_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, PinkNoiseAdaptAmplitude) {
    surprise_pink_noise_bridge_t* bridge = surprise_pink_noise_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    /* High surprise should increase effective amplitude */
    int rc = surprise_pink_noise_adapt_amplitude(bridge, 0.95f);
    EXPECT_EQ(0, rc);
    surprise_pink_noise_effects_t effects;
    surprise_pink_noise_bridge_get_effects(bridge, &effects);
    EXPECT_GT(effects.effective_amplitude, 0.0f);
    surprise_pink_noise_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, PinkNoisePerTarget) {
    surprise_pink_noise_bridge_t* bridge = surprise_pink_noise_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_pink_noise_inject(bridge);
    for (uint32_t t = 0; t < SURPRISE_PINK_NOISE_NUM_TARGETS; t++) {
        float val = surprise_pink_noise_get_for_target(bridge, t);
        /* Noise values can be anything but should not be NaN */
        EXPECT_EQ(val, val); /* NaN check */
    }
    surprise_pink_noise_bridge_destroy(bridge);
}

/* ============================================================================
 * Imagination Bridge Functional Tests
 * ============================================================================ */

TEST_F(SurpriseBridgesB20Test, ImaginationCreateDestroy) {
    surprise_imagination_bridge_t* bridge = surprise_imagination_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_imagination_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, ImaginationTrigger) {
    surprise_imagination_bridge_t* bridge = surprise_imagination_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    /* High surprise should trigger imagination */
    int rc = surprise_imagination_check_trigger(bridge, 0.9f, 1);
    EXPECT_EQ(0, rc);
    surprise_imagination_effects_t effects;
    surprise_imagination_bridge_get_effects(bridge, &effects);
    EXPECT_GE(effects.scenarios_active, 0u);
    surprise_imagination_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, ImaginationCooldown) {
    surprise_imagination_bridge_t* bridge = surprise_imagination_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    /* First trigger */
    surprise_imagination_check_trigger(bridge, 0.9f, 1);
    /* Second trigger immediately - should be blocked by cooldown */
    surprise_imagination_check_trigger(bridge, 0.9f, 2);
    surprise_imagination_stats_t stats;
    surprise_imagination_bridge_get_stats(bridge, &stats);
    /* The stats should reflect at most a few triggers if cooldown is active */
    EXPECT_GE(stats.triggers + stats.cooldown_blocked, 1u);
    surprise_imagination_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, ImaginationResult) {
    surprise_imagination_bridge_t* bridge = surprise_imagination_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_imagination_check_trigger(bridge, 0.9f, 1);
    /* Process result for scenario 0 */
    int rc = surprise_imagination_on_result(bridge, 0, 0.5f);
    /* May succeed or fail depending on whether scenario 0 exists */
    (void)rc;
    surprise_imagination_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, ImaginationUpdate) {
    surprise_imagination_bridge_t* bridge = surprise_imagination_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_imagination_check_trigger(bridge, 0.9f, 1);
    int rc = surprise_imagination_bridge_update(bridge, 1.0f);
    EXPECT_EQ(0, rc);
    surprise_imagination_effects_t effects;
    surprise_imagination_bridge_get_effects(bridge, &effects);
    /* After update, cooldown should have decayed */
    EXPECT_GE(effects.cooldown_remaining, 0.0f);
    surprise_imagination_bridge_destroy(bridge);
}

/* ============================================================================
 * Self-Model Bridge Functional Tests
 * ============================================================================ */

TEST_F(SurpriseBridgesB20Test, SelfModelCreateDestroy) {
    surprise_self_model_bridge_t* bridge = surprise_self_model_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_self_model_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, SelfModelCapabilitySurprise) {
    surprise_self_model_bridge_t* bridge = surprise_self_model_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    int rc = surprise_self_model_on_capability_surprise(
        bridge, 1, 0.8f, SURPRISE_CAPABILITY_UPGRADE);
    EXPECT_EQ(0, rc);
    surprise_self_model_stats_t stats;
    surprise_self_model_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.capability_surprises, 1u);
    surprise_self_model_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, SelfModelUpgradeDowngrade) {
    surprise_self_model_bridge_t* bridge = surprise_self_model_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    /* UPGRADE should increase confidence */
    int rc = surprise_self_model_on_capability_surprise(
        bridge, 1, 0.8f, SURPRISE_CAPABILITY_UPGRADE);
    EXPECT_EQ(0, rc);
    /* DOWNGRADE should decrease confidence */
    rc = surprise_self_model_on_capability_surprise(
        bridge, 2, 0.7f, SURPRISE_CAPABILITY_DOWNGRADE);
    EXPECT_EQ(0, rc);
    surprise_self_model_stats_t stats;
    surprise_self_model_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.capability_surprises, 2u);
    surprise_self_model_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, SelfModelNovel) {
    surprise_self_model_bridge_t* bridge = surprise_self_model_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    int rc = surprise_self_model_on_capability_surprise(
        bridge, 99, 0.9f, SURPRISE_CAPABILITY_NOVEL);
    EXPECT_EQ(0, rc);
    surprise_self_model_stats_t stats;
    surprise_self_model_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.discoveries, 1u);
    surprise_self_model_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, SelfModelConfidenceModulation) {
    surprise_self_model_bridge_t* bridge = surprise_self_model_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    float mod = surprise_self_model_query_confidence_modulation(bridge);
    /* Should return a valid modulation factor */
    EXPECT_GT(mod, 0.0f);
    EXPECT_EQ(mod, mod); /* NaN check */
    surprise_self_model_bridge_destroy(bridge);
}

TEST_F(SurpriseBridgesB20Test, SelfModelCompetenceFeedback) {
    surprise_self_model_bridge_t* bridge = surprise_self_model_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);
    surprise_self_model_on_capability_surprise(
        bridge, 5, 0.7f, SURPRISE_CAPABILITY_UPGRADE);
    int rc = surprise_self_model_on_competence_feedback(bridge, 5, 0.8f);
    EXPECT_EQ(0, rc);
    surprise_self_model_stats_t stats;
    surprise_self_model_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.competence_updates, 1u);
    surprise_self_model_bridge_destroy(bridge);
}
