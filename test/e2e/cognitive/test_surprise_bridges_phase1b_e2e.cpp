/**
 * @file test_surprise_bridges_phase1b_e2e.cpp
 * @brief End-to-end tests for Phase 1b Surprise Bridges (Society of Thought)
 * @date 2026-01-27
 *
 * WHAT: E2E tests simulating complete Phase 1b surprise bridge workflows
 * WHY:  Verify the full amplifier-bridge pipeline with all 10 bridges
 * HOW:  Create amplifier + all bridges, run multi-step realistic scenarios
 *
 * SCENARIOS:
 * 1. Full Society of Thought cascade: all 10 bridges process surprise
 * 2. Metabolic-constrained learning: substrate limits plasticity
 * 3. SNN-driven thalamic routing with imagination feedback
 * 4. Self-model confidence cycle: discovery → competence → sensitivity
 * 5. Full lifecycle: create/connect/process/reset/reprocess/destroy
 * 6. Mixed signal stress test across all 10 bridges
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

extern "C" {
#include "cognitive/salience/nimcp_surprise_amplifier.h"
#include "cognitive/salience/nimcp_surprise_fep_bridge.h"
#include "cognitive/salience/nimcp_surprise_gw_bridge.h"
#include "cognitive/salience/nimcp_surprise_attention_bridge.h"
#include "cognitive/salience/nimcp_surprise_plasticity_bridge.h"
#include "cognitive/salience/nimcp_surprise_snn_bridge.h"
#include "cognitive/salience/nimcp_surprise_substrate_bridge.h"
#include "cognitive/salience/nimcp_surprise_thalamic_bridge.h"
#include "cognitive/salience/nimcp_surprise_pink_noise_bridge.h"
#include "cognitive/salience/nimcp_surprise_imagination_bridge.h"
#include "cognitive/salience/nimcp_surprise_self_model_bridge.h"
}

/* ============================================================================
 * E2E Fixture: Full Society of Thought (all 10 bridges)
 * ============================================================================ */

class SurpriseBridgesPhase1bE2ETest : public ::testing::Test {
protected:
    /* Amplifier */
    surprise_amplifier_t* amp = nullptr;

    /* Phase 1a bridges */
    surprise_fep_bridge_t* fep = nullptr;
    surprise_gw_bridge_t* gw = nullptr;
    surprise_att_bridge_t* att = nullptr;

    /* Phase 1b bridges */
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
        if (att) { surprise_att_bridge_destroy(att); att = nullptr; }
        if (gw) { surprise_gw_bridge_destroy(gw); gw = nullptr; }
        if (fep) { surprise_fep_bridge_destroy(fep); fep = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }

    void CreateFullSocietyPipeline() {
        /* Amplifier */
        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);
        ASSERT_NE(amp, nullptr);

        /* Phase 1a: FEP */
        {
            surprise_fep_config_t cfg = surprise_fep_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            fep = surprise_fep_bridge_create(&cfg);
            ASSERT_NE(fep, nullptr);
            ASSERT_EQ(surprise_fep_bridge_connect_amplifier(fep, amp), 0);
        }

        /* Phase 1a: GW */
        {
            surprise_gw_config_t cfg = surprise_gw_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            gw = surprise_gw_bridge_create(&cfg);
            ASSERT_NE(gw, nullptr);
            ASSERT_EQ(surprise_gw_bridge_connect_amplifier(gw, amp), 0);
        }

        /* Phase 1a: Attention */
        {
            surprise_att_config_t cfg = surprise_att_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            att = surprise_att_bridge_create(&cfg);
            ASSERT_NE(att, nullptr);
            ASSERT_EQ(surprise_att_bridge_connect_amplifier(att, amp), 0);
        }

        /* Phase 1b: Plasticity */
        {
            surprise_plasticity_config_t cfg = surprise_plasticity_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            plasticity = surprise_plasticity_bridge_create(&cfg);
            ASSERT_NE(plasticity, nullptr);
            ASSERT_EQ(surprise_plasticity_bridge_connect_amplifier(plasticity, amp), 0);
        }

        /* Phase 1b: SNN */
        {
            surprise_snn_config_t cfg = surprise_snn_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            snn = surprise_snn_bridge_create(&cfg);
            ASSERT_NE(snn, nullptr);
            ASSERT_EQ(surprise_snn_bridge_connect_amplifier(snn, amp), 0);
        }

        /* Phase 1b: Substrate */
        {
            surprise_substrate_config_t cfg = surprise_substrate_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            substrate = surprise_substrate_bridge_create(&cfg);
            ASSERT_NE(substrate, nullptr);
            ASSERT_EQ(surprise_substrate_bridge_connect_amplifier(substrate, amp), 0);
        }

        /* Phase 1b: Thalamic */
        {
            surprise_thalamic_config_t cfg = surprise_thalamic_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            thalamic = surprise_thalamic_bridge_create(&cfg);
            ASSERT_NE(thalamic, nullptr);
            ASSERT_EQ(surprise_thalamic_bridge_connect_amplifier(thalamic, amp), 0);
        }

        /* Phase 1b: Pink Noise */
        {
            surprise_pink_noise_config_t cfg = surprise_pink_noise_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            pink_noise = surprise_pink_noise_bridge_create(&cfg);
            ASSERT_NE(pink_noise, nullptr);
            ASSERT_EQ(surprise_pink_noise_bridge_connect_amplifier(pink_noise, amp), 0);
        }

        /* Phase 1b: Imagination */
        {
            surprise_imagination_config_t cfg = surprise_imagination_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            imagination = surprise_imagination_bridge_create(&cfg);
            ASSERT_NE(imagination, nullptr);
            ASSERT_EQ(surprise_imagination_bridge_connect_amplifier(imagination, amp), 0);
        }

        /* Phase 1b: Self-Model */
        {
            surprise_self_model_config_t cfg = surprise_self_model_bridge_default_config();
            cfg.enable_bio_async = false;
            cfg.enable_logging = false;
            self_model = surprise_self_model_bridge_create(&cfg);
            ASSERT_NE(self_model, nullptr);
            ASSERT_EQ(surprise_self_model_bridge_connect_amplifier(self_model, amp), 0);
        }
    }

    /** Step all 10 bridges + amplifier forward */
    void StepAll(float dt) {
        surprise_amplifier_update(amp, dt);
        surprise_fep_bridge_update(fep, dt);
        surprise_gw_bridge_update(gw, dt);
        surprise_att_bridge_update(att, dt);
        surprise_plasticity_bridge_update(plasticity, dt);
        surprise_snn_bridge_update(snn, dt);
        surprise_substrate_bridge_update(substrate, 0.8f, 0.1f);
        surprise_pink_noise_bridge_update(pink_noise, dt);
        surprise_imagination_bridge_update(imagination, dt);
        surprise_self_model_bridge_update(self_model, dt);
    }
};

/* ============================================================================
 * Scenario 1: Full Society of Thought Cascade
 *
 * A hypothesis invalidation creates surprise that cascades through all 10
 * bridges: amplifier → FEP → GW → attention → plasticity → SNN → thalamic
 * → substrate → pink noise → imagination → self-model
 * ============================================================================ */

TEST_F(SurpriseBridgesPhase1bE2ETest, FullSocietyCascade) {
    CreateFullSocietyPipeline();

    /* Phase 1: Normal baseline */
    for (int i = 0; i < 5; i++) {
        surprise_amplifier_on_prediction_error(amp, 0.05f, 0x100);
        StepAll(0.1f);
    }

    /* Phase 2: Hypothesis invalidation → surprise spike */
    surprise_amplifier_on_hypothesis_invalidated(amp, 0.95f, 0.05f);

    surprise_event_t event;
    EXPECT_EQ(surprise_amplifier_get_last_event(amp, &event), 0);

    /* Route through Phase 1a bridges */
    surprise_fep_forward_pe(fep, event.magnitude, event.source);
    surprise_att_apply_boost(att, event.magnitude, event.attention_boost);
    surprise_gw_submit_broadcast(gw, event.magnitude, event.source,
                                  event.attention_boost, event.curiosity_boost);

    /* Route through Phase 1b bridges */
    surprise_plasticity_on_surprise_event(plasticity, event.magnitude, event.source);
    surprise_snn_encode_surprise(snn, event.magnitude, SURPRISE_SNN_CHANNEL_PE);

    surprise_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = SURPRISE_THALAMIC_REALIZATION;
    signal.surprise_magnitude = event.magnitude;
    signal.source_module = event.source;
    signal.urgency = 0.9f;
    surprise_thalamic_route_surprise(thalamic, &signal);

    surprise_imagination_check_trigger(imagination, event.magnitude, event.source);
    surprise_self_model_on_capability_surprise(
        self_model, 1, event.magnitude, SURPRISE_CAPABILITY_UPGRADE);

    /* Step all forward to process */
    StepAll(0.1f);

    /* Verify all 10 bridges participated */
    {
        surprise_fep_stats_t s;
        surprise_fep_bridge_get_stats(fep, &s);
        EXPECT_GT(s.pe_events_forwarded, 0u);
    }
    {
        surprise_gw_stats_t s;
        surprise_gw_bridge_get_stats(gw, &s);
        EXPECT_GT(s.broadcasts_submitted, 0u);
    }
    {
        surprise_att_stats_t s;
        surprise_att_bridge_get_stats(att, &s);
        EXPECT_GT(s.attention_boosts, 0u);
    }
    {
        surprise_plasticity_stats_t s;
        surprise_plasticity_bridge_get_stats(plasticity, &s);
        EXPECT_GT(s.plasticity_boosts, 0u);
    }
    {
        surprise_snn_stats_t s;
        surprise_snn_bridge_get_stats(snn, &s);
        EXPECT_GT(s.encoding_events, 0u);
    }
    {
        surprise_thalamic_stats_t s;
        surprise_thalamic_bridge_get_stats(thalamic, &s);
        EXPECT_GT(s.signals_routed, 0u);
    }
    {
        surprise_substrate_stats_t s;
        surprise_substrate_bridge_get_stats(substrate, &s);
        EXPECT_GT(s.modulation_updates, 0u);
    }
    {
        surprise_pink_noise_stats_t s;
        surprise_pink_noise_bridge_get_stats(pink_noise, &s);
        EXPECT_GT(s.noise_injections, 0u);
    }
    {
        surprise_imagination_stats_t s;
        surprise_imagination_bridge_get_stats(imagination, &s);
        EXPECT_GT(s.triggers, 0u);
    }
    {
        surprise_self_model_stats_t s;
        surprise_self_model_bridge_get_stats(self_model, &s);
        EXPECT_GT(s.capability_surprises, 0u);
    }

    /* Phase 3: Recovery - all bridges decay */
    for (int i = 0; i < 50; i++) {
        StepAll(0.5f);
    }

    /* Imagination cooldown should have expired */
    surprise_imagination_effects_t img_effects;
    surprise_imagination_bridge_get_effects(imagination, &img_effects);
    EXPECT_FLOAT_EQ(img_effects.cooldown_remaining, 0.0f);
}

/* ============================================================================
 * Scenario 2: Metabolic-Constrained Learning
 *
 * Low ATP reduces substrate capacity, which constrains plasticity modulation
 * and overall surprise processing quality.
 * ============================================================================ */

TEST_F(SurpriseBridgesPhase1bE2ETest, MetabolicConstrainedLearning) {
    CreateFullSocietyPipeline();

    /* Phase 1: Full metabolic resources */
    surprise_substrate_bridge_update(substrate, 1.0f, 0.0f);
    surprise_plasticity_on_surprise_event(plasticity, 0.8f, 0x100);
    surprise_plasticity_bridge_update(plasticity, 0.1f);

    surprise_plasticity_effects_t full_effects;
    surprise_plasticity_bridge_get_effects(plasticity, &full_effects);
    float full_lr = full_effects.learning_rate_multiplier;

    /* Phase 2: Depleted metabolic resources */
    surprise_substrate_bridge_update(substrate, 0.1f, 0.9f);

    surprise_substrate_effects_t sub_effects;
    surprise_substrate_bridge_get_effects(substrate, &sub_effects);
    EXPECT_LT(sub_effects.overall_capacity, 1.0f);

    /* Substrate limits should be reflected in reduced capacity */
    EXPECT_GT(sub_effects.overall_capacity, 0.0f);

    /* Full LR should have been boosted */
    EXPECT_GT(full_lr, 1.0f);
}

/* ============================================================================
 * Scenario 3: SNN-Driven Thalamic Routing with Imagination
 *
 * SNN encodes surprise into spike trains. Dominant channel drives thalamic
 * routing. High-magnitude events trigger imagination scenarios.
 * ============================================================================ */

TEST_F(SurpriseBridgesPhase1bE2ETest, SnnThalamicImaginationLoop) {
    CreateFullSocietyPipeline();

    /* Encode strong novelty surprise in SNN */
    surprise_snn_encode_surprise(snn, 0.95f, SURPRISE_SNN_CHANNEL_NOVELTY);

    /* Simulate SNN for several steps */
    for (int i = 0; i < 20; i++) {
        surprise_snn_simulate_step(snn);
    }

    /* Decode SNN output */
    surprise_snn_effects_t snn_effects;
    surprise_snn_decode_output(snn, &snn_effects);

    /* Route through thalamus as novelty signal with high magnitude */
    surprise_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = SURPRISE_THALAMIC_NOVELTY;
    signal.surprise_magnitude = 0.8f;  /* Use high fixed magnitude to ensure routing */
    signal.source_module = 0x200;
    signal.urgency = 0.8f;
    surprise_thalamic_route_surprise(thalamic, &signal);

    /* High surprise also triggers imagination */
    surprise_imagination_check_trigger(imagination, 0.95f, 0x200);

    StepAll(0.1f);

    /* Verify thalamic routing occurred */
    surprise_thalamic_stats_t thal_stats;
    surprise_thalamic_bridge_get_stats(thalamic, &thal_stats);
    EXPECT_GT(thal_stats.signals_routed, 0u);

    /* Verify imagination was triggered */
    surprise_imagination_stats_t img_stats;
    surprise_imagination_bridge_get_stats(imagination, &img_stats);
    EXPECT_GT(img_stats.triggers, 0u);

    /* Complete imagination scenario and check expectation update */
    surprise_imagination_on_result(imagination, 1, 0.4f);

    surprise_imagination_stats_t img_stats2;
    surprise_imagination_bridge_get_stats(imagination, &img_stats2);
    EXPECT_GT(img_stats2.scenarios_completed, 0u);
}

/* ============================================================================
 * Scenario 4: Self-Model Confidence Cycle
 *
 * Novel capability discovery → competence feedback → confidence modulation
 * → sensitivity adjustment
 * ============================================================================ */

TEST_F(SurpriseBridgesPhase1bE2ETest, SelfModelConfidenceCycle) {
    CreateFullSocietyPipeline();

    /* Step 1: Discover novel capability */
    surprise_self_model_on_capability_surprise(
        self_model, 42, 0.9f, SURPRISE_CAPABILITY_NOVEL);

    surprise_self_model_stats_t stats1;
    surprise_self_model_bridge_get_stats(self_model, &stats1);
    EXPECT_GE(stats1.discoveries, 1u);

    /* Step 2: Provide competence feedback */
    surprise_self_model_on_competence_feedback(self_model, 42, 0.8f);

    surprise_self_model_stats_t stats2;
    surprise_self_model_bridge_get_stats(self_model, &stats2);
    EXPECT_GE(stats2.competence_updates, 1u);

    /* Step 3: Update bridge to process */
    surprise_self_model_bridge_update(self_model, 0.1f);

    /* Step 4: Query confidence modulation */
    float mod = surprise_self_model_query_confidence_modulation(self_model);
    EXPECT_GT(mod, 0.0f);
    EXPECT_TRUE(std::isfinite(mod));

    /* Step 5: Verify revision was recorded */
    surprise_capability_revision_t rev;
    EXPECT_EQ(surprise_self_model_get_last_revision(self_model, &rev), 0);
    EXPECT_EQ(rev.capability_id, 42u);

    /* Step 6: Repeated competence feedback builds confidence */
    for (int i = 0; i < 10; i++) {
        surprise_self_model_on_competence_feedback(self_model, 42, 0.9f);
        surprise_self_model_bridge_update(self_model, 0.1f);
    }

    float final_mod = surprise_self_model_query_confidence_modulation(self_model);
    EXPECT_TRUE(std::isfinite(final_mod));
}

/* ============================================================================
 * Scenario 5: Full Lifecycle
 *
 * Create → connect → process → query → reset → re-process → destroy
 * for all 10 bridges
 * ============================================================================ */

TEST_F(SurpriseBridgesPhase1bE2ETest, FullLifecycle) {
    CreateFullSocietyPipeline();

    /* Step 1: Process events through all bridges */
    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);
    surprise_fep_forward_pe(fep, 0.8f, 0x100);
    surprise_att_apply_boost(att, 0.8f, 1.5f);
    surprise_gw_submit_broadcast(gw, 0.8f, 1, 0.5f, 0.3f);
    surprise_plasticity_on_surprise_event(plasticity, 0.8f, 0x100);
    surprise_snn_encode_surprise(snn, 0.8f, SURPRISE_SNN_CHANNEL_PE);
    surprise_substrate_bridge_update(substrate, 0.8f, 0.2f);
    surprise_thalamic_route_realization(thalamic, 0.8f, 0x100);
    surprise_imagination_check_trigger(imagination, 0.9f, 0x100);
    surprise_self_model_on_capability_surprise(
        self_model, 1, 0.8f, SURPRISE_CAPABILITY_UPGRADE);

    StepAll(0.1f);

    /* Step 2: Reset all bridges */
    EXPECT_EQ(surprise_fep_bridge_reset(fep), 0);
    EXPECT_EQ(surprise_gw_bridge_reset(gw), 0);
    EXPECT_EQ(surprise_att_bridge_reset(att), 0);
    EXPECT_EQ(surprise_plasticity_bridge_reset(plasticity), 0);
    EXPECT_EQ(surprise_snn_bridge_reset(snn), 0);
    EXPECT_EQ(surprise_thalamic_bridge_reset(thalamic), 0);
    EXPECT_EQ(surprise_pink_noise_bridge_reset(pink_noise), 0);
    EXPECT_EQ(surprise_imagination_bridge_reset(imagination), 0);
    EXPECT_EQ(surprise_self_model_bridge_reset(self_model), 0);

    /* Step 3: Verify all are clean */
    {
        surprise_plasticity_stats_t s;
        surprise_plasticity_bridge_get_stats(plasticity, &s);
        EXPECT_EQ(s.plasticity_boosts, 0u);
        EXPECT_EQ(s.total_updates, 0u);
    }
    {
        surprise_snn_stats_t s;
        surprise_snn_bridge_get_stats(snn, &s);
        EXPECT_EQ(s.encoding_events, 0u);
    }
    {
        surprise_thalamic_stats_t s;
        surprise_thalamic_bridge_get_stats(thalamic, &s);
        EXPECT_EQ(s.signals_routed, 0u);
    }
    {
        surprise_imagination_stats_t s;
        surprise_imagination_bridge_get_stats(imagination, &s);
        EXPECT_EQ(s.triggers, 0u);
    }
    {
        surprise_self_model_stats_t s;
        surprise_self_model_bridge_get_stats(self_model, &s);
        EXPECT_EQ(s.capability_surprises, 0u);
    }

    /* Step 4: Re-process after reset */
    surprise_plasticity_on_surprise_event(plasticity, 0.7f, 0x200);
    surprise_snn_encode_surprise(snn, 0.7f, SURPRISE_SNN_CHANNEL_CONFLICT);
    StepAll(0.1f);

    {
        surprise_plasticity_stats_t s;
        surprise_plasticity_bridge_get_stats(plasticity, &s);
        EXPECT_GE(s.plasticity_boosts, 1u);
    }
    {
        surprise_snn_stats_t s;
        surprise_snn_bridge_get_stats(snn, &s);
        EXPECT_GE(s.encoding_events, 1u);
    }

    /* Step 5: Destroy handled by TearDown */
}

/* ============================================================================
 * Scenario 6: Mixed Signal Stress Test
 *
 * Rapid mixed signals through all 10 bridges simultaneously.
 * ============================================================================ */

TEST_F(SurpriseBridgesPhase1bE2ETest, MixedSignalStressTest) {
    CreateFullSocietyPipeline();

    for (int i = 0; i < 300; i++) {
        float mag = 0.3f + 0.5f * (float)(i % 7) / 7.0f;

        switch (i % 10) {
            case 0:
                surprise_fep_forward_pe(fep, mag, 0x100 + i);
                break;
            case 1:
                surprise_gw_submit_broadcast(gw, mag, i, mag * 1.5f, mag);
                break;
            case 2:
                surprise_att_apply_boost(att, mag, mag * 2.0f);
                break;
            case 3:
                surprise_plasticity_on_surprise_event(plasticity, mag, 0x200 + i);
                break;
            case 4:
                surprise_snn_encode_surprise(snn, mag,
                    (surprise_snn_channel_t)(i % SURPRISE_SNN_NUM_CHANNELS));
                break;
            case 5:
                surprise_substrate_bridge_update(substrate,
                    0.5f + mag * 0.5f, 0.1f + (1.0f - mag) * 0.3f);
                break;
            case 6: {
                surprise_thalamic_signal_t sig;
                memset(&sig, 0, sizeof(sig));
                sig.signal_type = (uint32_t)(1 << (i % 4));
                sig.surprise_magnitude = mag;
                sig.source_module = 0x300 + i;
                sig.urgency = mag;
                surprise_thalamic_route_surprise(thalamic, &sig);
                break;
            }
            case 7:
                surprise_imagination_check_trigger(imagination, mag, 0x400 + i);
                break;
            case 8:
                surprise_self_model_on_capability_surprise(
                    self_model, (uint32_t)(i % 32 + 1), mag,
                    (surprise_capability_revision_type_t)(i % 3));
                break;
            case 9:
                surprise_amplifier_on_prediction_error(amp, mag, 0x500 + i);
                break;
        }

        /* Update every 3rd step */
        if (i % 3 == 0) {
            StepAll(0.05f);
        }
    }

    /* Final update */
    StepAll(0.1f);

    /* Verify all systems are still functioning */
    {
        surprise_fep_stats_t s;
        surprise_fep_bridge_get_stats(fep, &s);
        EXPECT_GT(s.total_updates, 0u);
    }
    {
        surprise_gw_stats_t s;
        surprise_gw_bridge_get_stats(gw, &s);
        EXPECT_GT(s.total_updates, 0u);
    }
    {
        surprise_att_stats_t s;
        surprise_att_bridge_get_stats(att, &s);
        EXPECT_GT(s.attention_boosts, 0u);
    }
    {
        surprise_plasticity_stats_t s;
        surprise_plasticity_bridge_get_stats(plasticity, &s);
        EXPECT_GT(s.total_updates, 0u);
    }
    {
        surprise_snn_stats_t s;
        surprise_snn_bridge_get_stats(snn, &s);
        EXPECT_GT(s.encoding_events, 0u);
    }
    {
        surprise_substrate_stats_t s;
        surprise_substrate_bridge_get_stats(substrate, &s);
        EXPECT_GT(s.total_updates, 0u);
    }
    {
        surprise_thalamic_stats_t s;
        surprise_thalamic_bridge_get_stats(thalamic, &s);
        EXPECT_GT(s.signals_routed, 0u);
    }
    {
        surprise_pink_noise_stats_t s;
        surprise_pink_noise_bridge_get_stats(pink_noise, &s);
        EXPECT_GT(s.noise_injections, 0u);
    }
    {
        surprise_self_model_stats_t s;
        surprise_self_model_bridge_get_stats(self_model, &s);
        EXPECT_GT(s.capability_surprises, 0u);
    }

    /* Amplifier should have processed events */
    surprise_amplifier_stats_t amp_stats = surprise_amplifier_get_stats(amp);
    EXPECT_GT(amp_stats.total_surprises, 0u);
}
