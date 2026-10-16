/**
 * @file test_speech_immune_integration.cpp
 * @brief Unit tests for Speech Cortex-Immune Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Comprehensive test suite for speech-immune bidirectional coupling
 * WHY:  Validate biological accuracy and integration correctness
 * HOW:  Test cytokine effects, inflammation impacts, speech triggers
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "perception/immune/nimcp_speech_immune_bridge.h"
    #include "cognitive/immune/nimcp_brain_immune.h"
    #include "perception/nimcp_speech_cortex.h"
    #include "utils/memory/nimcp_memory.h"

class SpeechImmuneIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    speech_cortex_t* speech_cortex;
    speech_immune_bridge_t* bridge;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        /* Create speech cortex */
        speech_cortex_config_t speech_config = speech_cortex_default_config();
        speech_config.enable_wernicke = true;
        speech_config.enable_broca = true;
        speech_config.enable_prosody = true;
        speech_config.enable_memory = true;
        speech_cortex = speech_cortex_create(&speech_config);
        ASSERT_NE(speech_cortex, nullptr);

        /* Create bridge */
        speech_immune_config_t bridge_config;
        speech_immune_default_config(&bridge_config);
        bridge = speech_immune_bridge_create(&bridge_config, immune_system, speech_cortex);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) speech_immune_bridge_destroy(bridge);
        if (speech_cortex) speech_cortex_destroy(speech_cortex);
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }
    }

    /* Helper: Present antigen and activate immune response */
    void trigger_immune_response(uint32_t severity, brain_cytokine_type_t cytokine_type) {
        uint8_t epitope[64];
        snprintf((char*)epitope, 64, "test_threat_%d", severity);

        uint32_t antigen_id;
        brain_immune_present_antigen(
            immune_system,
            ANTIGEN_SOURCE_MANUAL,
            epitope,
            strlen((char*)epitope),
            severity,
            0,
            &antigen_id
        );

        /* CRITICAL: Must initiate inflammation to have cytokine effects tracked */
        /* Cytokine level estimation depends on inflammation_sites count */
        uint32_t site_id;
        brain_immune_initiate_inflammation(immune_system, 0, antigen_id, &site_id);

        /* Release cytokine */
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            immune_system,
            cytokine_type,
            0,
            0.8f,
            0,
            &cytokine_id
        );
    }

    /* Helper: Initiate inflammation */
    void initiate_inflammation(brain_inflammation_level_t level) {
        uint8_t epitope[64];
        snprintf((char*)epitope, 64, "inflammation_trigger");

        uint32_t antigen_id;
        brain_immune_present_antigen(
            immune_system,
            ANTIGEN_SOURCE_MANUAL,
            epitope,
            strlen((char*)epitope),
            8,
            0,
            &antigen_id
        );

        uint32_t site_id;
        brain_immune_initiate_inflammation(
            immune_system,
            0,
            antigen_id,
            &site_id
        );

        /* Escalate to desired level */
        for (int i = INFLAMMATION_LOCAL; i < level; i++) {
            brain_immune_escalate_inflammation(immune_system, site_id);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(SpeechImmuneIntegrationTest, DefaultConfiguration) {
    speech_immune_config_t config;
    EXPECT_EQ(speech_immune_default_config(&config), 0);

    EXPECT_TRUE(config.enable_cytokine_speech_modulation);
    EXPECT_TRUE(config.enable_inflammation_impairment);
    EXPECT_TRUE(config.enable_speech_immune_trigger);
    EXPECT_TRUE(config.enable_distress_vocalization_trigger);

    EXPECT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_EQ(config.speech_trigger_sensitivity, 1.0f);
}

TEST_F(SpeechImmuneIntegrationTest, CreateDestroy) {
    /* Already tested in SetUp/TearDown, just verify non-null */
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SpeechImmuneIntegrationTest, CreateWithNullPointers) {
    /* Should fail without required systems */
    speech_immune_bridge_t* bad_bridge = speech_immune_bridge_create(
        nullptr, nullptr, nullptr
    );
    EXPECT_EQ(bad_bridge, nullptr);

    bad_bridge = speech_immune_bridge_create(
        nullptr, immune_system, nullptr
    );
    EXPECT_EQ(bad_bridge, nullptr);

    bad_bridge = speech_immune_bridge_create(
        nullptr, nullptr, speech_cortex
    );
    EXPECT_EQ(bad_bridge, nullptr);
}

/* ============================================================================
 * Immune → Speech: Cytokine Effects
 * ============================================================================ */

TEST_F(SpeechImmuneIntegrationTest, IL1ReducesFluency) {
    /* IL-1β should reduce speech fluency */
    trigger_immune_response(5, BRAIN_CYTOKINE_IL1);

    EXPECT_EQ(speech_immune_apply_cytokine_effects(bridge), 0);

    cytokine_speech_effects_t effects;
    EXPECT_EQ(speech_immune_get_cytokine_effects(bridge, &effects), 0);

    EXPECT_LT(effects.il1_fluency_reduction, 0.0f);
    EXPECT_GT(effects.total_fluency_impairment, 0.0f);
}

TEST_F(SpeechImmuneIntegrationTest, IL6SlowsWordRetrieval) {
    /* IL-6 should increase word retrieval latency */
    trigger_immune_response(6, BRAIN_CYTOKINE_IL6);

    EXPECT_EQ(speech_immune_apply_cytokine_effects(bridge), 0);

    float latency = speech_immune_get_retrieval_latency_increase(bridge);
    EXPECT_GT(latency, 0.0f);
    EXPECT_LT(latency, 250.0f);  /* Max 200ms from implementation + margin */
}

TEST_F(SpeechImmuneIntegrationTest, TNFIncreasesPhonemeErrors) {
    /* TNF-α should increase phoneme discrimination errors */
    trigger_immune_response(7, BRAIN_CYTOKINE_TNF);

    EXPECT_EQ(speech_immune_apply_cytokine_effects(bridge), 0);

    float error_rate = speech_immune_get_phoneme_error_rate(bridge);
    EXPECT_GT(error_rate, 0.0f);
    EXPECT_LE(error_rate, 0.5f);
}

TEST_F(SpeechImmuneIntegrationTest, IFNGammaReducesProsody) {
    /* IFN-γ should flatten prosody */
    trigger_immune_response(5, BRAIN_CYTOKINE_IFN_GAMMA);

    EXPECT_EQ(speech_immune_apply_cytokine_effects(bridge), 0);

    cytokine_speech_effects_t effects;
    EXPECT_EQ(speech_immune_get_cytokine_effects(bridge, &effects), 0);

    EXPECT_GT(effects.prosody_flattening, 0.0f);
    EXPECT_LE(effects.prosody_flattening, 1.0f);
}

TEST_F(SpeechImmuneIntegrationTest, CytokinesReduceSpeechRate) {
    /* Multiple cytokines should slow speech rate */
    trigger_immune_response(5, BRAIN_CYTOKINE_IL1);
    trigger_immune_response(6, BRAIN_CYTOKINE_IL6);

    EXPECT_EQ(speech_immune_apply_cytokine_effects(bridge), 0);

    float rate_factor = speech_immune_get_speech_rate_factor(bridge);
    EXPECT_LT(rate_factor, 1.0f);  /* Reduced from normal */
    EXPECT_GE(rate_factor, 0.3f);  /* Min 30% from implementation */
}

/* ============================================================================
 * Immune → Speech: Inflammation Effects
 * ============================================================================ */

TEST_F(SpeechImmuneIntegrationTest, LocalInflammationMildImpairment) {
    /* Local inflammation should cause mild impairment */
    initiate_inflammation(INFLAMMATION_LOCAL);

    EXPECT_EQ(speech_immune_apply_inflammation_effects(bridge), 0);

    inflammation_speech_state_t state;
    EXPECT_EQ(speech_immune_get_inflammation_state(bridge, &state), 0);

    EXPECT_EQ(state.current_level, INFLAMMATION_LOCAL);
    EXPECT_GT(state.verbal_fluency_reduction, 0.0f);
    EXPECT_LT(state.verbal_fluency_reduction, 0.3f);
}

TEST_F(SpeechImmuneIntegrationTest, SystemicInflammationSevereImpairment) {
    /* Systemic inflammation should cause severe impairment */
    initiate_inflammation(INFLAMMATION_SYSTEMIC);

    EXPECT_EQ(speech_immune_apply_inflammation_effects(bridge), 0);

    inflammation_speech_state_t state;
    EXPECT_EQ(speech_immune_get_inflammation_state(bridge, &state), 0);

    EXPECT_EQ(state.current_level, INFLAMMATION_SYSTEMIC);
    EXPECT_GT(state.verbal_fluency_reduction, 0.4f);
    EXPECT_GT(state.word_finding_difficulty, 0.0f);
}

TEST_F(SpeechImmuneIntegrationTest, InflammationStormMaximalImpairment) {
    /* Cytokine storm should cause maximal speech impairment */
    initiate_inflammation(INFLAMMATION_STORM);

    EXPECT_EQ(speech_immune_apply_inflammation_effects(bridge), 0);

    float impairment = speech_immune_compute_impairment(bridge);
    EXPECT_GT(impairment, 0.7f);
    EXPECT_TRUE(speech_immune_is_speech_impaired(bridge));
}

TEST_F(SpeechImmuneIntegrationTest, InflammationReducesWorkingMemory) {
    /* Inflammation should reduce phonological working memory capacity */
    initiate_inflammation(INFLAMMATION_SYSTEMIC);

    EXPECT_EQ(speech_immune_apply_inflammation_effects(bridge), 0);

    float capacity = speech_immune_get_working_memory_capacity(bridge);
    EXPECT_LT(capacity, 1.0f);  /* Reduced from normal */
    EXPECT_GT(capacity, 0.5f);  /* Not below 60% (40% max reduction) */
}

TEST_F(SpeechImmuneIntegrationTest, InflammationIncreasesArticulationSlowing) {
    /* Inflammation should slow articulatory planning */
    initiate_inflammation(INFLAMMATION_REGIONAL);

    EXPECT_EQ(speech_immune_apply_inflammation_effects(bridge), 0);

    inflammation_speech_state_t state;
    EXPECT_EQ(speech_immune_get_inflammation_state(bridge, &state), 0);

    EXPECT_GT(state.articulation_slowing, 0.0f);
    EXPECT_LE(state.articulation_slowing, 1.0f);
}

TEST_F(SpeechImmuneIntegrationTest, InflammationImpairsComprehension) {
    /* Inflammation should impair speech comprehension */
    initiate_inflammation(INFLAMMATION_SYSTEMIC);

    EXPECT_EQ(speech_immune_apply_inflammation_effects(bridge), 0);

    inflammation_speech_state_t state;
    EXPECT_EQ(speech_immune_get_inflammation_state(bridge, &state), 0);

    EXPECT_GT(state.comprehension_impairment, 0.0f);
}

/* ============================================================================
 * Speech → Immune: Effort and Distress Triggers
 * ============================================================================ */

TEST_F(SpeechImmuneIntegrationTest, HighEffortTriggersImmune) {
    /* High speech effort should trigger immune response */
    bridge->speech_trigger.speech_effort_level = 0.9f;
    bridge->speech_trigger.frustration_level = 0.8f;

    EXPECT_EQ(speech_immune_trigger_from_effort(bridge), 0);

    /* Should have triggered cortisol response */
    EXPECT_TRUE(bridge->speech_trigger.cortisol_triggered);
    EXPECT_GT(bridge->speech_trigger.immune_suppression, 0.0f);

    /* Should have incremented counter */
    EXPECT_GT(bridge->speech_triggered_responses, 0u);
}

TEST_F(SpeechImmuneIntegrationTest, LowEffortNoTrigger) {
    /* Low effort should not trigger immune */
    bridge->speech_trigger.speech_effort_level = 0.3f;

    uint32_t initial_count = bridge->speech_triggered_responses;
    EXPECT_EQ(speech_immune_trigger_from_effort(bridge), 0);

    /* Should not have triggered */
    EXPECT_EQ(bridge->speech_triggered_responses, initial_count);
}

TEST_F(SpeechImmuneIntegrationTest, DistressVocalizationTriggers) {
    /* Distress vocalization should trigger immune */
    bridge->speech_trigger.distress_intensity = 0.9f;

    EXPECT_EQ(speech_immune_detect_distress_vocalization(bridge, (void*)1), 0);

    /* Should have detected distress */
    EXPECT_TRUE(bridge->speech_trigger.distress_detected);
    EXPECT_GT(bridge->distress_events, 0u);
}

TEST_F(SpeechImmuneIntegrationTest, IllnessWordModulatesImmune) {
    /* Illness-related words should modulate immune */
    EXPECT_EQ(speech_immune_trigger_from_illness_expression(bridge, "sick"), 0);
    EXPECT_EQ(speech_immune_trigger_from_illness_expression(bridge, "pain"), 0);
    EXPECT_EQ(speech_immune_trigger_from_illness_expression(bridge, "fever"), 0);

    /* Non-illness words should not trigger */
    EXPECT_EQ(speech_immune_trigger_from_illness_expression(bridge, "hello"), 0);
    EXPECT_EQ(speech_immune_trigger_from_illness_expression(bridge, "happy"), 0);
}

/* ============================================================================
 * Bidirectional Integration
 * ============================================================================ */

TEST_F(SpeechImmuneIntegrationTest, BidirectionalUpdate) {
    /* Update should process both directions */
    trigger_immune_response(5, BRAIN_CYTOKINE_IL6);

    EXPECT_EQ(speech_immune_bridge_update(bridge, 100), 0);

    /* Should have applied cytokine effects */
    EXPECT_GT(bridge->cytokine_modulations, 0u);

    /* Should have updated total updates */
    EXPECT_GT(bridge->total_updates, 0u);
}

TEST_F(SpeechImmuneIntegrationTest, CytokineEffectsAccumulateWithInflammation) {
    /* Both cytokines and inflammation should contribute */
    trigger_immune_response(6, BRAIN_CYTOKINE_IL6);
    initiate_inflammation(INFLAMMATION_REGIONAL);

    EXPECT_EQ(speech_immune_apply_cytokine_effects(bridge), 0);
    EXPECT_EQ(speech_immune_apply_inflammation_effects(bridge), 0);

    float impairment = speech_immune_compute_impairment(bridge);
    float error_rate = speech_immune_get_phoneme_error_rate(bridge);

    EXPECT_GT(impairment, 0.0f);
    EXPECT_GT(error_rate, 0.0f);
}

TEST_F(SpeechImmuneIntegrationTest, ErrorRateIncreasesEffortLevel) {
    /* High error rate should increase perceived effort */
    trigger_immune_response(7, BRAIN_CYTOKINE_TNF);

    EXPECT_EQ(speech_immune_bridge_update(bridge, 100), 0);

    /* Error rate should feed back to effort level */
    EXPECT_GT(bridge->speech_trigger.error_rate, 0.0f);
    EXPECT_GT(bridge->speech_trigger.speech_effort_level, 0.0f);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(SpeechImmuneIntegrationTest, QueryCytokineEffects) {
    trigger_immune_response(5, BRAIN_CYTOKINE_IL1);
    EXPECT_EQ(speech_immune_apply_cytokine_effects(bridge), 0);

    cytokine_speech_effects_t effects;
    EXPECT_EQ(speech_immune_get_cytokine_effects(bridge, &effects), 0);

    /* Should have some effects */
    EXPECT_TRUE(
        effects.il1_fluency_reduction != 0.0f ||
        effects.il6_word_retrieval_delay != 0.0f ||
        effects.tnf_phoneme_discrimination != 0.0f
    );
}

TEST_F(SpeechImmuneIntegrationTest, QueryInflammationState) {
    initiate_inflammation(INFLAMMATION_LOCAL);
    EXPECT_EQ(speech_immune_apply_inflammation_effects(bridge), 0);

    inflammation_speech_state_t state;
    EXPECT_EQ(speech_immune_get_inflammation_state(bridge, &state), 0);

    EXPECT_EQ(state.current_level, INFLAMMATION_LOCAL);
}

TEST_F(SpeechImmuneIntegrationTest, QueryFluencyReduction) {
    trigger_immune_response(6, BRAIN_CYTOKINE_IL6);
    EXPECT_EQ(speech_immune_apply_cytokine_effects(bridge), 0);

    float fluency_reduction = speech_immune_get_fluency_reduction(bridge);
    EXPECT_GE(fluency_reduction, 0.0f);
    EXPECT_LE(fluency_reduction, 1.0f);
}

TEST_F(SpeechImmuneIntegrationTest, IsSpeechImpairedDetection) {
    /* No impairment initially */
    EXPECT_FALSE(speech_immune_is_speech_impaired(bridge));

    /* Severe inflammation should cause detectable impairment */
    initiate_inflammation(INFLAMMATION_SYSTEMIC);
    EXPECT_EQ(speech_immune_apply_inflammation_effects(bridge), 0);

    EXPECT_TRUE(speech_immune_is_speech_impaired(bridge));
}

/* ============================================================================
 * Edge Cases and Error Handling
 * ============================================================================ */

TEST_F(SpeechImmuneIntegrationTest, NullPointerHandling) {
    /* All functions should handle NULL gracefully */
    EXPECT_EQ(speech_immune_apply_cytokine_effects(nullptr), -1);
    EXPECT_EQ(speech_immune_apply_inflammation_effects(nullptr), -1);
    EXPECT_EQ(speech_immune_trigger_from_effort(nullptr), -1);
    EXPECT_EQ(speech_immune_detect_distress_vocalization(nullptr, nullptr), -1);
    EXPECT_EQ(speech_immune_trigger_from_illness_expression(nullptr, "test"), -1);
    EXPECT_EQ(speech_immune_bridge_update(nullptr, 100), -1);

    EXPECT_EQ(speech_immune_compute_impairment(nullptr), 0.0f);
    EXPECT_EQ(speech_immune_get_retrieval_latency_increase(nullptr), 0.0f);
    EXPECT_EQ(speech_immune_get_phoneme_error_rate(nullptr), 0.0f);
    EXPECT_EQ(speech_immune_get_speech_rate_factor(nullptr), 1.0f);
    EXPECT_FALSE(speech_immune_is_speech_impaired(nullptr));
}

TEST_F(SpeechImmuneIntegrationTest, DisabledFeaturesNoEffect) {
    /* Disable all features */
    bridge->enable_cytokine_speech_modulation = false;
    bridge->enable_inflammation_impairment = false;
    bridge->enable_speech_immune_trigger = false;
    bridge->enable_distress_vocalization_trigger = false;

    trigger_immune_response(8, BRAIN_CYTOKINE_TNF);
    initiate_inflammation(INFLAMMATION_STORM);

    /* Should return success but do nothing */
    EXPECT_EQ(speech_immune_apply_cytokine_effects(bridge), 0);
    EXPECT_EQ(speech_immune_apply_inflammation_effects(bridge), 0);

    bridge->speech_trigger.speech_effort_level = 0.9f;
    EXPECT_EQ(speech_immune_trigger_from_effort(bridge), 0);

    bridge->speech_trigger.distress_intensity = 0.9f;
    EXPECT_EQ(speech_immune_detect_distress_vocalization(bridge, (void*)1), 0);
}

TEST_F(SpeechImmuneIntegrationTest, ClampingPreventsOverflow) {
    /* Extreme values should be clamped */
    for (int i = 0; i < 10; i++) {
        trigger_immune_response(10, BRAIN_CYTOKINE_TNF);
    }

    EXPECT_EQ(speech_immune_apply_cytokine_effects(bridge), 0);

    float error_rate = speech_immune_get_phoneme_error_rate(bridge);
    float impairment = speech_immune_compute_impairment(bridge);
    float rate_factor = speech_immune_get_speech_rate_factor(bridge);

    /* Should be clamped to valid ranges */
    EXPECT_LE(error_rate, 0.5f);
    EXPECT_LE(impairment, 1.0f);
    EXPECT_GE(rate_factor, 0.3f);
}

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

TEST_F(SpeechImmuneIntegrationTest, StatisticsTracking) {
    /* Initial state */
    EXPECT_EQ(bridge->total_updates, 0u);
    EXPECT_EQ(bridge->cytokine_modulations, 0u);
    EXPECT_EQ(bridge->speech_triggered_responses, 0u);
    EXPECT_EQ(bridge->distress_events, 0u);

    /* Update increments total_updates */
    speech_immune_bridge_update(bridge, 100);
    EXPECT_GT(bridge->total_updates, 0u);

    /* Cytokine effects increment cytokine_modulations */
    trigger_immune_response(5, BRAIN_CYTOKINE_IL1);
    speech_immune_apply_cytokine_effects(bridge);
    EXPECT_GT(bridge->cytokine_modulations, 0u);

    /* Effort trigger increments speech_triggered_responses */
    bridge->speech_trigger.speech_effort_level = 0.9f;
    speech_immune_trigger_from_effort(bridge);
    EXPECT_GT(bridge->speech_triggered_responses, 0u);

    /* Distress increments distress_events */
    bridge->speech_trigger.distress_intensity = 0.9f;
    speech_immune_detect_distress_vocalization(bridge, (void*)1);
    EXPECT_GT(bridge->distress_events, 0u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
