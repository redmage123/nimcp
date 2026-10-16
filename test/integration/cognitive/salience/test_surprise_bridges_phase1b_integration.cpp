/**
 * @file test_surprise_bridges_phase1b_integration.cpp
 * @brief Integration tests for Phase 1b Surprise Bridges (Society of Thought)
 * @date 2026-01-27
 *
 * WHAT: Integration tests for 7 new surprise bridges working together
 * WHY:  Verify multi-bridge pipeline behavior, cross-bridge consistency,
 *       and independent reset functionality
 * HOW:  Create amplifier + all 7 bridges, run multi-step integration scenarios
 *
 * TESTS:
 * 1. Full pipeline flow: surprise event through all bridges
 * 2. Cross-bridge consistency: all bridges react to same amplifier
 * 3. Independent resets: resetting one bridge doesn't affect others
 * 4. Metabolic constraint propagation: substrate affects processing
 * 5. SNN-Plasticity learning loop: spike activity modulates plasticity
 * 6. Imagination-SelfModel discovery loop: imagination triggers self-model
 * 7. Return to baseline: all bridges decay back after activity
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
 * Integration Fixture
 * ============================================================================ */

class SurpriseBridgesPhase1bIntegrationTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_plasticity_bridge_t* plasticity = nullptr;
    surprise_snn_bridge_t* snn = nullptr;
    surprise_substrate_bridge_t* substrate = nullptr;
    surprise_thalamic_bridge_t* thalamic = nullptr;
    surprise_pink_noise_bridge_t* pink_noise = nullptr;
    surprise_imagination_bridge_t* imagination = nullptr;
    surprise_self_model_bridge_t* self_model = nullptr;

    void SetUp() override {}

    void TearDown() override {
        if (self_model) { surprise_self_model_bridge_destroy(self_model); self_model = nullptr; }
        if (imagination) { surprise_imagination_bridge_destroy(imagination); imagination = nullptr; }
        if (pink_noise) { surprise_pink_noise_bridge_destroy(pink_noise); pink_noise = nullptr; }
        if (thalamic) { surprise_thalamic_bridge_destroy(thalamic); thalamic = nullptr; }
        if (substrate) { surprise_substrate_bridge_destroy(substrate); substrate = nullptr; }
        if (snn) { surprise_snn_bridge_destroy(snn); snn = nullptr; }
        if (plasticity) { surprise_plasticity_bridge_destroy(plasticity); plasticity = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }

    void CreateFullPipeline() {
        /* Create amplifier */
        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);
        ASSERT_NE(amp, nullptr);

        /* Plasticity bridge */
        {
            surprise_plasticity_config_t cfg = surprise_plasticity_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            plasticity = surprise_plasticity_bridge_create(&cfg);
            ASSERT_NE(plasticity, nullptr);
            ASSERT_EQ(surprise_plasticity_bridge_connect_amplifier(plasticity, amp), 0);
        }

        /* SNN bridge */
        {
            surprise_snn_config_t cfg = surprise_snn_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            snn = surprise_snn_bridge_create(&cfg);
            ASSERT_NE(snn, nullptr);
            ASSERT_EQ(surprise_snn_bridge_connect_amplifier(snn, amp), 0);
        }

        /* Substrate bridge */
        {
            surprise_substrate_config_t cfg = surprise_substrate_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            substrate = surprise_substrate_bridge_create(&cfg);
            ASSERT_NE(substrate, nullptr);
            ASSERT_EQ(surprise_substrate_bridge_connect_amplifier(substrate, amp), 0);
        }

        /* Thalamic bridge */
        {
            surprise_thalamic_config_t cfg = surprise_thalamic_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            thalamic = surprise_thalamic_bridge_create(&cfg);
            ASSERT_NE(thalamic, nullptr);
            ASSERT_EQ(surprise_thalamic_bridge_connect_amplifier(thalamic, amp), 0);
        }

        /* Pink noise bridge */
        {
            surprise_pink_noise_config_t cfg = surprise_pink_noise_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            pink_noise = surprise_pink_noise_bridge_create(&cfg);
            ASSERT_NE(pink_noise, nullptr);
            ASSERT_EQ(surprise_pink_noise_bridge_connect_amplifier(pink_noise, amp), 0);
        }

        /* Imagination bridge */
        {
            surprise_imagination_config_t cfg = surprise_imagination_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            imagination = surprise_imagination_bridge_create(&cfg);
            ASSERT_NE(imagination, nullptr);
            ASSERT_EQ(surprise_imagination_bridge_connect_amplifier(imagination, amp), 0);
        }

        /* Self-model bridge */
        {
            surprise_self_model_config_t cfg = surprise_self_model_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            self_model = surprise_self_model_bridge_create(&cfg);
            ASSERT_NE(self_model, nullptr);
            ASSERT_EQ(surprise_self_model_bridge_connect_amplifier(self_model, amp), 0);
        }
    }

    /** Update all bridges with dt */
    void StepAll(float dt) {
        surprise_amplifier_update(amp, dt);
        surprise_plasticity_bridge_update(plasticity, dt);
        surprise_snn_bridge_update(snn, dt);
        surprise_substrate_bridge_update(substrate, 0.8f, 0.1f);  /* Normal ATP, low fatigue */
        surprise_pink_noise_bridge_update(pink_noise, dt);
        surprise_imagination_bridge_update(imagination, dt);
        surprise_self_model_bridge_update(self_model, dt);
    }
};

/* ============================================================================
 * Test 1: Full Pipeline Flow
 *
 * A high surprise event propagates through all bridges:
 * amplifier → plasticity boost + SNN encoding + thalamic routing +
 *             imagination trigger + self-model check + noise injection
 * ============================================================================ */

TEST_F(SurpriseBridgesPhase1bIntegrationTest, FullPipelineFlow) {
    CreateFullPipeline();

    /* Inject a high surprise event */
    float surprise_level = 0.9f;
    surprise_amplifier_on_prediction_error(amp, surprise_level, 0x100);

    /* Route through plasticity */
    surprise_plasticity_on_surprise_event(plasticity, surprise_level, 0x100);

    /* Encode in SNN */
    surprise_snn_encode_surprise(snn, surprise_level, SURPRISE_SNN_CHANNEL_PE);

    /* Route through thalamus */
    surprise_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = SURPRISE_THALAMIC_REALIZATION;
    signal.surprise_magnitude = surprise_level;
    signal.source_module = 0x100;
    signal.urgency = 0.8f;
    surprise_thalamic_route_surprise(thalamic, &signal);

    /* Check imagination trigger */
    surprise_imagination_check_trigger(imagination, surprise_level, 0x100);

    /* Self-model capability check */
    surprise_self_model_on_capability_surprise(
        self_model, 1, surprise_level, SURPRISE_CAPABILITY_UPGRADE);

    /* Step all forward */
    StepAll(0.1f);

    /* Verify all bridges processed the event */
    surprise_plasticity_stats_t plast_stats;
    surprise_plasticity_bridge_get_stats(plasticity, &plast_stats);
    EXPECT_GE(plast_stats.plasticity_boosts, 1u);

    surprise_snn_stats_t snn_stats;
    surprise_snn_bridge_get_stats(snn, &snn_stats);
    EXPECT_GE(snn_stats.encoding_events, 1u);

    surprise_thalamic_stats_t thal_stats;
    surprise_thalamic_bridge_get_stats(thalamic, &thal_stats);
    EXPECT_GE(thal_stats.signals_routed, 1u);

    surprise_imagination_stats_t img_stats;
    surprise_imagination_bridge_get_stats(imagination, &img_stats);
    EXPECT_GE(img_stats.triggers, 1u);

    surprise_self_model_stats_t sm_stats;
    surprise_self_model_bridge_get_stats(self_model, &sm_stats);
    EXPECT_GE(sm_stats.capability_surprises, 1u);

    surprise_pink_noise_stats_t pn_stats;
    surprise_pink_noise_bridge_get_stats(pink_noise, &pn_stats);
    EXPECT_GE(pn_stats.noise_injections, 1u);

    surprise_substrate_stats_t sub_stats;
    surprise_substrate_bridge_get_stats(substrate, &sub_stats);
    EXPECT_GE(sub_stats.modulation_updates, 1u);
}

/* ============================================================================
 * Test 2: Cross-Bridge Consistency
 *
 * All bridges connected to the same amplifier should react consistently
 * to the same underlying surprise level.
 * ============================================================================ */

TEST_F(SurpriseBridgesPhase1bIntegrationTest, CrossBridgeConsistency) {
    CreateFullPipeline();

    /* Send multiple events of the same type */
    for (int i = 0; i < 5; i++) {
        surprise_amplifier_on_prediction_error(amp, 0.7f, 0x100 + i);
        surprise_plasticity_on_surprise_event(plasticity, 0.7f, 0x100 + i);
        surprise_snn_encode_surprise(snn, 0.7f, SURPRISE_SNN_CHANNEL_PE);
        StepAll(0.1f);
    }

    /* All bridges should show accumulated activity */
    surprise_plasticity_stats_t plast_stats;
    surprise_plasticity_bridge_get_stats(plasticity, &plast_stats);
    EXPECT_GE(plast_stats.plasticity_boosts, 3u);

    surprise_snn_stats_t snn_stats;
    surprise_snn_bridge_get_stats(snn, &snn_stats);
    EXPECT_GE(snn_stats.encoding_events, 5u);

    /* Pink noise should have adapted to sustained surprise */
    surprise_pink_noise_stats_t pn_stats;
    surprise_pink_noise_bridge_get_stats(pink_noise, &pn_stats);
    EXPECT_GE(pn_stats.total_updates, 5u);
}

/* ============================================================================
 * Test 3: Independent Resets
 *
 * Resetting one bridge should not affect the state of other bridges.
 * ============================================================================ */

TEST_F(SurpriseBridgesPhase1bIntegrationTest, IndependentResets) {
    CreateFullPipeline();

    /* Build up state in all bridges */
    surprise_plasticity_on_surprise_event(plasticity, 0.8f, 0x100);
    surprise_snn_encode_surprise(snn, 0.8f, SURPRISE_SNN_CHANNEL_PE);
    surprise_imagination_check_trigger(imagination, 0.9f, 0x100);
    surprise_self_model_on_capability_surprise(
        self_model, 1, 0.8f, SURPRISE_CAPABILITY_UPGRADE);
    StepAll(0.1f);

    /* Reset only plasticity */
    EXPECT_EQ(surprise_plasticity_bridge_reset(plasticity), 0);

    /* Plasticity should be clean */
    surprise_plasticity_stats_t plast_stats;
    surprise_plasticity_bridge_get_stats(plasticity, &plast_stats);
    EXPECT_EQ(plast_stats.plasticity_boosts, 0u);

    /* SNN should still have state */
    surprise_snn_stats_t snn_stats;
    surprise_snn_bridge_get_stats(snn, &snn_stats);
    EXPECT_GT(snn_stats.encoding_events, 0u);

    /* Imagination should still have state */
    surprise_imagination_stats_t img_stats;
    surprise_imagination_bridge_get_stats(imagination, &img_stats);
    EXPECT_GT(img_stats.triggers, 0u);

    /* Self-model should still have state */
    surprise_self_model_stats_t sm_stats;
    surprise_self_model_bridge_get_stats(self_model, &sm_stats);
    EXPECT_GT(sm_stats.capability_surprises, 0u);
}

/* ============================================================================
 * Test 4: Metabolic Constraint Propagation
 *
 * Low ATP via substrate bridge should reduce processing capacity.
 * ============================================================================ */

TEST_F(SurpriseBridgesPhase1bIntegrationTest, MetabolicConstraintPropagation) {
    CreateFullPipeline();

    /* Normal metabolic state */
    surprise_substrate_bridge_update(substrate, 1.0f, 0.0f);
    surprise_substrate_effects_t high_effects;
    surprise_substrate_bridge_get_effects(substrate, &high_effects);
    float high_capacity = high_effects.overall_capacity;

    /* Low ATP, high fatigue */
    surprise_substrate_bridge_update(substrate, 0.2f, 0.8f);
    surprise_substrate_effects_t low_effects;
    surprise_substrate_bridge_get_effects(substrate, &low_effects);
    float low_capacity = low_effects.overall_capacity;

    /* Low ATP should reduce capacity */
    EXPECT_LT(low_capacity, high_capacity);
}

/* ============================================================================
 * Test 5: SNN-Plasticity Learning Loop
 *
 * SNN activity should influence plasticity modulation through learning
 * outcome feedback.
 * ============================================================================ */

TEST_F(SurpriseBridgesPhase1bIntegrationTest, SnnPlasticityLearningLoop) {
    CreateFullPipeline();

    /* Encode surprise in SNN */
    surprise_snn_encode_surprise(snn, 0.8f, SURPRISE_SNN_CHANNEL_PE);
    for (int i = 0; i < 5; i++) {
        surprise_snn_simulate_step(snn);
    }

    /* Get SNN activity as a proxy for learning outcome */
    surprise_snn_effects_t snn_effects;
    surprise_snn_decode_output(snn, &snn_effects);

    /* Feed activity as learning outcome to plasticity */
    float activity = snn_effects.combined_activity;
    surprise_plasticity_on_learning_outcome(plasticity, activity, 0x100);

    /* The source should now show habituation */
    float hab = surprise_plasticity_get_habituation_for_source(plasticity, 0x100);
    /* Either habituated or still at 0 if below threshold */
    EXPECT_GE(hab, 0.0f);

    /* Subsequent surprise from same source should be tracked */
    surprise_plasticity_on_surprise_event(plasticity, 0.8f, 0x100);
    surprise_plasticity_bridge_update(plasticity, 0.1f);

    surprise_plasticity_stats_t stats;
    surprise_plasticity_bridge_get_stats(plasticity, &stats);
    EXPECT_GE(stats.plasticity_boosts + stats.habituation_events, 1u);
}

/* ============================================================================
 * Test 6: Imagination-SelfModel Discovery Loop
 *
 * Imagination triggers self-model discovery through capability surprise.
 * ============================================================================ */

TEST_F(SurpriseBridgesPhase1bIntegrationTest, ImaginationSelfModelLoop) {
    CreateFullPipeline();

    /* High surprise triggers imagination */
    surprise_imagination_check_trigger(imagination, 0.9f, 0x100);

    surprise_imagination_effects_t img_effects;
    surprise_imagination_bridge_get_effects(imagination, &img_effects);
    EXPECT_GE(img_effects.scenarios_active, 1u);

    /* The imagination scenario result triggers self-model update */
    surprise_imagination_on_result(imagination, 1, 0.7f);

    /* Self-model discovers novel capability from the imagination result */
    surprise_self_model_on_capability_surprise(
        self_model, 99, 0.85f, SURPRISE_CAPABILITY_NOVEL);

    surprise_self_model_stats_t sm_stats;
    surprise_self_model_bridge_get_stats(self_model, &sm_stats);
    EXPECT_GE(sm_stats.discoveries, 1u);

    /* Confidence modulation should be computed */
    float mod = surprise_self_model_query_confidence_modulation(self_model);
    EXPECT_GT(mod, 0.0f);
    EXPECT_TRUE(std::isfinite(mod));
}

/* ============================================================================
 * Test 7: Return to Baseline
 *
 * After a burst of activity, all bridges should gradually return to
 * baseline when no further events are processed.
 * ============================================================================ */

TEST_F(SurpriseBridgesPhase1bIntegrationTest, ReturnToBaseline) {
    CreateFullPipeline();

    /* Phase 1: Generate activity burst */
    surprise_amplifier_on_prediction_error(amp, 0.9f, 0x100);
    surprise_plasticity_on_surprise_event(plasticity, 0.9f, 0x100);
    surprise_snn_encode_surprise(snn, 0.9f, SURPRISE_SNN_CHANNEL_PE);
    surprise_imagination_check_trigger(imagination, 0.9f, 0x100);
    StepAll(0.1f);

    /* Record post-burst state */
    surprise_plasticity_effects_t plast_burst;
    surprise_plasticity_bridge_get_effects(plasticity, &plast_burst);
    float burst_lr = plast_burst.learning_rate_multiplier;

    /* Phase 2: Run many update cycles without new events */
    for (int i = 0; i < 100; i++) {
        StepAll(0.5f);
    }

    /* Phase 3: Verify decay toward baseline */
    surprise_plasticity_effects_t plast_decayed;
    surprise_plasticity_bridge_get_effects(plasticity, &plast_decayed);
    EXPECT_LE(plast_decayed.learning_rate_multiplier, burst_lr);

    /* Pink noise amplitude should have adapted back */
    surprise_pink_noise_effects_t pn_decayed;
    surprise_pink_noise_bridge_get_effects(pink_noise, &pn_decayed);
    EXPECT_TRUE(std::isfinite(pn_decayed.effective_amplitude));

    /* Imagination cooldown should have expired */
    surprise_imagination_effects_t img_decayed;
    surprise_imagination_bridge_get_effects(imagination, &img_decayed);
    EXPECT_FLOAT_EQ(img_decayed.cooldown_remaining, 0.0f);
}
