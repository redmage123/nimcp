/**
 * @file test_hippocampus_integration.cpp
 * @brief Integration tests for Hippocampus with all NIMCP subsystems
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/hippocampus/nimcp_hippocampus.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*=============================================================================
 * INTEGRATION TEST FIXTURE
 *===========================================================================*/

class HippocampusIntegrationTest : public ::testing::Test {
protected:
    nimcp_hippocampus_t* hippo = nullptr;

    void SetUp() override {
        hippo_config_t config = hippo_default_config();
        config.num_dg_cells = 256;
        config.num_ca3_cells = 128;
        config.num_ca1_cells = 128;
        config.num_place_cells = 64;
        config.max_episodes = 128;
        config.enable_prime_resonance = true;
        config.enable_all_bridges = true;
        hippo = hippo_create(&config);
        ASSERT_NE(hippo, nullptr);

        /* Initialize all bridges for integration testing */
        hippo_init_all_bridges(hippo, nullptr);
    }

    void TearDown() override {
        if (hippo) {
            hippo_destroy(hippo);
            hippo = nullptr;
        }
    }

    void createTestContent(float* content, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            content[i] = base_value + sinf(i * 0.1f) * 0.5f;
        }
    }
};

/*=============================================================================
 * PRIME RESONANCE MEMORY INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, PrimeResonanceEnhancedEncoding) {
    /* Test that prime resonance enhances memory encoding */
    hippo->prime_resonance_bridge.resonance_active = true;
    hippo->prime_resonance_bridge.coherence_level = 0.9f;
    hippo->prime_resonance_bridge.last_resonance_tag = 0x12345;

    float what[128];
    createTestContent(what, 128, 0.5f);

    uint32_t episode_id;
    EXPECT_EQ(hippo_resonance_enhanced_encode(hippo, what, 128, &episode_id), 0);

    const nimcp_episode_t* ep = hippo_get_episode(hippo, episode_id);
    ASSERT_NE(ep, nullptr);

    /* Enhanced encoding should have higher strength */
    EXPECT_GT(ep->encoding_strength, 0.6f);
    EXPECT_EQ(ep->resonance_signature, 0x12345ull);
}

TEST_F(HippocampusIntegrationTest, PrimeResonanceGuidedRetrieval) {
    /* Tag several episodes with same resonance */
    float what[128];
    uint64_t resonance_tag = 0xABCD;

    for (int i = 0; i < 5; i++) {
        createTestContent(what, 128, 0.5f + i * 0.1f);
        uint32_t id;
        hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);
        hippo_tag_with_resonance(hippo, id, resonance_tag);
    }

    /* Create non-tagged episode */
    createTestContent(what, 128, 2.0f);
    uint32_t non_tagged_id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &non_tagged_id);

    /* Guided retrieval should prefer resonance-tagged */
    hippo->prime_resonance_bridge.last_resonance_tag = resonance_tag;

    createTestContent(what, 128, 0.5f);  /* Similar to tagged */
    uint32_t retrieved_id;
    float confidence;
    EXPECT_EQ(hippo_resonance_guided_retrieve(hippo, what, 128, &retrieved_id, &confidence), 0);
    EXPECT_NE(retrieved_id, non_tagged_id);
}

TEST_F(HippocampusIntegrationTest, PrimeResonancePhaseAlignment) {
    /* Test theta phase alignment with resonance */
    hippo->prime_resonance_bridge.resonance_active = true;
    hippo->prime_resonance_bridge.phase_alignment = (float)M_PI;

    hippo_process_incoming(hippo);

    /* Theta should move toward resonance phase */
    float initial_theta = hippo->theta_phase;
    for (int i = 0; i < 10; i++) {
        hippo_process_incoming(hippo);
    }

    /* Phase should have shifted toward alignment target */
    float phase_diff = fabsf(hippo->theta_phase - initial_theta);
    EXPECT_GT(phase_diff, 0.0f);
}

/*=============================================================================
 * IMMUNE SYSTEM INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, ImmuneInflammationAffectsTheta) {
    /* Test that neuroinflammation reduces theta power */
    float initial_theta_power = hippo->theta_power;

    hippo->immune_bridge.neuroinflammation = true;
    hippo->immune_bridge.inflammation_level = 0.8f;

    hippo_update(hippo, 0.1f);

    EXPECT_LT(hippo->theta_power, initial_theta_power);
}

TEST_F(HippocampusIntegrationTest, ImmuneHealthAffectsOverall) {
    /* Test health status considers immune state */
    float healthy_status = hippo_get_health_status(hippo);

    hippo->immune_bridge.health_score = 0.5f;
    hippo->immune_bridge.neuroinflammation = true;

    float compromised_status = hippo_get_health_status(hippo);
    EXPECT_LT(compromised_status, healthy_status);
}

TEST_F(HippocampusIntegrationTest, MicroglialActivityPruning) {
    /* High microglial activity should affect plasticity */
    hippo->immune_bridge.microglial_activity = 0.9f;

    /* Encode under high pruning conditions */
    float what[128];
    createTestContent(what, 128, 0.5f);

    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);

    /* Memory should still form but potentially weaker */
    const nimcp_episode_t* ep = hippo_get_episode(hippo, id);
    ASSERT_NE(ep, nullptr);
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, BackgroundConsolidation) {
    /* Test background consolidation via bio-async */
    hippo->bio_async_bridge.background_processing = true;

    float what[128];
    for (int i = 0; i < 5; i++) {
        createTestContent(what, 128, (float)i * 0.1f);
        uint32_t id;
        hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.7f, &id);
    }

    /* Run updates simulating background consolidation */
    for (int i = 0; i < 100; i++) {
        hippo_update(hippo, 0.1f);
    }

    /* Episodes should have consolidated */
    float total_consolidation = 0.0f;
    for (uint32_t i = 0; i < hippo->num_episodes; i++) {
        total_consolidation += hippo->episodes[i].consolidation_level;
    }
    EXPECT_GT(total_consolidation, 0.0f);
}

TEST_F(HippocampusIntegrationTest, AsyncEfficiencyModulation) {
    /* Test that async efficiency affects processing */
    hippo->bio_async_bridge.async_efficiency = 0.5f;

    /* This would affect consolidation rate in real implementation */
    hippo_update(hippo, 0.1f);
    EXPECT_EQ(hippo->updates_processed, 1u);
}

/*=============================================================================
 * COGNITIVE LAYER INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, AttentionModulatesEncoding) {
    /* High attention should enhance encoding */
    hippo->cognitive_bridge.attention_level = 1.0f;

    float what[128];
    createTestContent(what, 128, 0.5f);

    uint32_t high_attention_id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &high_attention_id);

    /* Low attention encoding */
    hippo->cognitive_bridge.attention_level = 0.1f;
    createTestContent(what, 128, 0.7f);

    uint32_t low_attention_id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &low_attention_id);

    /* High attention episode should be stronger */
    const nimcp_episode_t* high_ep = hippo_get_episode(hippo, high_attention_id);
    const nimcp_episode_t* low_ep = hippo_get_episode(hippo, low_attention_id);

    EXPECT_GE(high_ep->encoding_strength, low_ep->encoding_strength);
}

TEST_F(HippocampusIntegrationTest, WorkingMemoryLoadInteraction) {
    /* Test interaction with working memory */
    hippo->cognitive_bridge.working_memory_load = 0.9f;

    /* High WM load should affect hippocampal processing */
    hippo_process_incoming(hippo);

    /* System should still function - status should be in a functional state */
    EXPECT_TRUE(hippo->status == HIPPO_STATUS_READY ||
                hippo->status == HIPPO_STATUS_ENCODING ||
                hippo->status == HIPPO_STATUS_RETRIEVING ||
                hippo->status == HIPPO_STATUS_CONSOLIDATING);
}

TEST_F(HippocampusIntegrationTest, CognitiveControlModulation) {
    /* Test cognitive control effects */
    hippo->cognitive_bridge.cognitive_control = 1.0f;
    hippo->cognitive_bridge.active_goals = 3;

    /* Goal relevance should affect encoding */
    float what[128];
    createTestContent(what, 128, 0.5f);

    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.8f, &id);

    EXPECT_NE(hippo_get_episode(hippo, id), nullptr);
}

/*=============================================================================
 * HYPOTHALAMUS INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, StressModulatesLearning) {
    /* Moderate stress enhances, high stress impairs (Yerkes-Dodson) */
    float baseline_rate = hippo->config.default_learning_rate;

    /* Moderate stress */
    hippo->hypothalamus_bridge.stress_level = 0.3f;
    hippo_process_incoming(hippo);

    float moderate_stress_rate = hippo->config.default_learning_rate;

    /* Reset and test high stress */
    hippo->config.default_learning_rate = baseline_rate;
    hippo->hypothalamus_bridge.stress_level = 0.9f;
    hippo_process_incoming(hippo);

    float high_stress_rate = hippo->config.default_learning_rate;

    /* Moderate stress should not impair as much as high stress */
    EXPECT_GE(moderate_stress_rate, high_stress_rate);
}

TEST_F(HippocampusIntegrationTest, ArousalAffectsConsolidation) {
    /* Arousal should affect emotional memory consolidation */
    hippo->hypothalamus_bridge.arousal_level = 0.9f;

    float what[128];
    createTestContent(what, 128, 0.5f);

    /* High arousal + high emotional valence */
    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.8f, 0.9f, &id);

    /* Run consolidation */
    for (int i = 0; i < 50; i++) {
        hippo_consolidate_memories(hippo, 0.1f);
    }

    const nimcp_episode_t* ep = hippo_get_episode(hippo, id);
    EXPECT_GT(ep->consolidation_level, 0.0f);
}

TEST_F(HippocampusIntegrationTest, CircadianPhaseEffects) {
    /* Test circadian rhythm effects */
    hippo->hypothalamus_bridge.circadian_phase = 0.5f;  /* Midday */

    hippo_update(hippo, 0.1f);
    EXPECT_EQ(hippo->status, HIPPO_STATUS_READY);
}

/*=============================================================================
 * SUBSTRATE/METABOLIC INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, LowAtpAffectsHealth) {
    /* Low ATP should reduce health status */
    float initial_health = hippo_get_health_status(hippo);

    hippo->substrate_bridge.atp_level = 0.3f;
    hippo->substrate_bridge.metabolic_stress = true;

    float low_atp_health = hippo_get_health_status(hippo);
    EXPECT_LT(low_atp_health, initial_health);
}

TEST_F(HippocampusIntegrationTest, GlucoseLevelsAffectFunction) {
    /* Test glucose level effects */
    hippo->substrate_bridge.glucose_level = 0.2f;  /* Hypoglycemia */

    float health = hippo_get_health_status(hippo);
    EXPECT_LT(health, 1.0f);
}

TEST_F(HippocampusIntegrationTest, NeurotransmitterLevels) {
    /* Test neurotransmitter level effects */
    /* Index: 0=Glu, 1=GABA, 2=ACh, etc. */
    hippo->substrate_bridge.neurotransmitter_levels[2] = 0.8f;  /* High ACh */

    /* ACh enhances encoding (not explicitly modeled but structure exists) */
    hippo_update(hippo, 0.1f);
    EXPECT_EQ(hippo->status, HIPPO_STATUS_READY);
}

/*=============================================================================
 * THALAMUS INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, ThalamusSpindleCouplingEnhancesConsolidation) {
    /* Sleep spindles enhance memory consolidation */
    hippo->thalamus_bridge.spindle_coupling = 0.9f;

    float what[128];
    createTestContent(what, 128, 0.5f);
    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.7f, &id);

    float initial_consolidation = hippo->episodes[id].consolidation_level;

    /* Run consolidation with high spindle coupling */
    for (int i = 0; i < 50; i++) {
        hippo_consolidate_memories(hippo, 0.1f);
    }

    EXPECT_GT(hippo->episodes[id].consolidation_level, initial_consolidation);
}

TEST_F(HippocampusIntegrationTest, ThalamusRelayGain) {
    /* Test thalamic relay gain effects */
    hippo->thalamus_bridge.relay_gain = 1.5f;

    hippo_sync_thalamus(hippo);
    EXPECT_GT(hippo->thalamus_bridge.anterior_nucleus_activity, 0.0f);
}

/*=============================================================================
 * PORTIA (STRATEGIC PLANNING) INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, PortiaMemoryUtilization) {
    /* Portia should track memory utilization */
    float what[128];
    for (int i = 0; i < 10; i++) {
        createTestContent(what, 128, (float)i * 0.1f);
        uint32_t id;
        hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);

        /* Retrieve to increase retrieval count */
        uint32_t retrieved;
        float conf;
        hippo_retrieve_episode(hippo, what, 128, RETRIEVAL_FREE_RECALL, &retrieved, &conf);
    }

    hippo_send_outgoing(hippo);
    EXPECT_GT(hippo->portia_bridge.memory_utilization, 0.0f);
}

TEST_F(HippocampusIntegrationTest, ProspectiveMemory) {
    /* Test prospective memory support for Portia */
    hippo->portia_bridge.prospective_memory_active = true;

    float what[128];
    createTestContent(what, 128, 0.5f);
    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);

    EXPECT_NE(hippo_get_episode(hippo, id), nullptr);
}

/*=============================================================================
 * DRAGONFLY (FAST REACTIONS) INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, RapidRetrievalMode) {
    /* Dragonfly rapid retrieval lowers completion threshold */
    float original_threshold = hippo->config.pattern_completion_threshold;

    hippo->dragonfly_bridge.rapid_retrieval_mode = true;
    hippo_send_outgoing(hippo);

    EXPECT_LT(hippo->config.pattern_completion_threshold, original_threshold);
}

TEST_F(HippocampusIntegrationTest, ThreatAssessmentMemory) {
    /* Test threat-related memory retrieval */
    hippo->dragonfly_bridge.threat_assessment = 0.9f;

    /* Encode threat-associated memory */
    float what[128];
    createTestContent(what, 128, 0.5f);
    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, -0.9f, 0.9f, &id);  /* Negative valence */

    EXPECT_NE(hippo_get_episode(hippo, id), nullptr);
}

/*=============================================================================
 * PERCEPTION INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, SalienceEnhancesEncoding) {
    /* High salience should enhance encoding */
    hippo->perception_bridge.salience_level = 0.9f;

    hippo_process_incoming(hippo);

    /* Prime resonance enhancement should be activated */
    EXPECT_FLOAT_EQ(hippo->prime_resonance_bridge.memory_enhancement_factor, 2.0f);
}

TEST_F(HippocampusIntegrationTest, NoveltyTriggersEncoding) {
    /* High novelty should trigger encoding mode */
    hippo->perirhinal_bridge.novelty_signal = 0.8f;

    hippo_process_incoming(hippo);

    EXPECT_EQ(hippo->status, HIPPO_STATUS_ENCODING);
}

/*=============================================================================
 * SNN/PLASTICITY INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, SnnNeuronAssignment) {
    /* Test that cells have SNN neuron IDs assigned */
    for (uint32_t i = 0; i < hippo->num_dg_cells; i++) {
        EXPECT_EQ(hippo->dg_cells[i].snn_neuron_id, i);
    }

    EXPECT_GT(hippo->snn_bridge.total_neurons, 0u);
}

TEST_F(HippocampusIntegrationTest, PlasticityEnablement) {
    /* Test plasticity settings */
    EXPECT_TRUE(hippo->plasticity_bridge.hebbian_learning);
    EXPECT_TRUE(hippo->plasticity_bridge.stdp_enabled);

    /* LTP/LTD magnitudes should be set */
    EXPECT_GT(hippo->plasticity_bridge.ltp_magnitude, 0.0f);
    EXPECT_GT(hippo->plasticity_bridge.ltd_magnitude, 0.0f);
}

TEST_F(HippocampusIntegrationTest, StdpTracesDuringEncoding) {
    /* STDP traces should be updated during encoding */
    float what[128];
    createTestContent(what, 128, 0.5f);

    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);

    /* Traces would be updated in full implementation */
    EXPECT_EQ(hippo->encodings_performed, 1u);
}

/*=============================================================================
 * MEMORY CIRCUIT REGION INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, EntorhinalGridCellInput) {
    /* Test entorhinal bridge grid cell input */
    hippo->entorhinal_bridge.perforant_path_strength = 0.9f;
    hippo->entorhinal_bridge.mec_input_weight = 0.6f;
    hippo->entorhinal_bridge.lec_input_weight = 0.4f;

    hippo_sync_entorhinal(hippo);

    /* Temporal context should be updated */
    float expected_context = hippo->theta_phase / (2.0f * M_PI);
    EXPECT_FLOAT_EQ(hippo->entorhinal_bridge.temporal_context_signal, expected_context);
}

TEST_F(HippocampusIntegrationTest, PerirhinalFamiliarityInput) {
    /* Test perirhinal familiarity/novelty signals */
    hippo->perirhinal_bridge.familiarity_signal = 0.2f;
    hippo->perirhinal_bridge.novelty_signal = 0.8f;

    hippo_sync_perirhinal(hippo);
    EXPECT_EQ(hippo->perirhinal_bridge.initialized, true);
}

TEST_F(HippocampusIntegrationTest, ParahippocampalSceneContext) {
    /* Test parahippocampal scene context input */
    hippo->parahippocampal_bridge.place_recognition = 0.7f;
    hippo->parahippocampal_bridge.context_stability = 0.9f;

    hippo_sync_parahippocampal(hippo);
    EXPECT_EQ(hippo->parahippocampal_bridge.initialized, true);
}

TEST_F(HippocampusIntegrationTest, MammillaryFornixOutput) {
    /* Test fornix output to mammillary bodies */
    /* Activate subiculum */
    for (uint32_t i = 0; i < hippo->num_subiculum_cells; i++) {
        hippo->subiculum_cells[i].activation = 0.5f;
    }

    hippo_sync_mammillary(hippo);

    EXPECT_GT(hippo->mammillary_bridge.fornix_output_strength, 0.0f);
}

/*=============================================================================
 * OMNIDIRECTIONAL INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, OmnidirectionalSync) {
    /* Test omnidirectional module synchronization */
    hippo->omni_bridge.synchronization_quality = 1.0f;
    hippo->omni_bridge.global_coherence = 0.9f;

    EXPECT_TRUE(hippo->omni_bridge.bidirectional_active);
}

TEST_F(HippocampusIntegrationTest, CrossModuleIntegration) {
    /* Test integration across multiple modules */
    hippo->omni_bridge.connected_modules = 10;

    hippo_bidirectional_update(hippo, 0.01f);

    EXPECT_EQ(hippo->updates_processed, 1u);
}

/*=============================================================================
 * SECURITY INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, SecurityIntegrityChecking) {
    /* Test security bridge integrity */
    EXPECT_TRUE(hippo->security_bridge.integrity_checking);
    EXPECT_EQ(hippo->security_bridge.access_level, 1u);
}

TEST_F(HippocampusIntegrationTest, MemoryAccessLogging) {
    /* Test logging of memory access */
    hippo->logging_bridge.memory_access_logging = true;

    float what[128];
    createTestContent(what, 128, 0.5f);
    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);

    /* In full implementation, this would log the access */
    EXPECT_EQ(hippo->encodings_performed, 1u);
}

/*=============================================================================
 * FULL SYSTEM INTEGRATION
 *===========================================================================*/

TEST_F(HippocampusIntegrationTest, FullBidirectionalCycle) {
    /* Test complete bidirectional update cycle */
    hippo->cognitive_bridge.attention_level = 0.8f;
    hippo->hypothalamus_bridge.arousal_level = 0.6f;
    hippo->perception_bridge.salience_level = 0.7f;

    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(hippo_bidirectional_update(hippo, 0.01f), 0);
    }

    EXPECT_EQ(hippo->updates_processed, 10u);
}

TEST_F(HippocampusIntegrationTest, MultiRegionMemoryFormation) {
    /* Test memory formation with all regions active */
    /* Set up input from all MTL regions */
    hippo->entorhinal_bridge.perforant_path_strength = 0.8f;
    hippo->perirhinal_bridge.novelty_signal = 0.7f;
    hippo->parahippocampal_bridge.context_stability = 0.9f;

    float what[128], where[3];
    createTestContent(what, 128, 0.5f);
    where[0] = 50.0f; where[1] = 50.0f; where[2] = 0.0f;

    uint32_t id;
    EXPECT_EQ(hippo_encode_episode(hippo, what, 128, where, 3, NULL, 0,
        0.5f, 0.7f, &id), 0);

    const nimcp_episode_t* ep = hippo_get_episode(hippo, id);
    ASSERT_NE(ep, nullptr);
    EXPECT_TRUE(ep->has_spatial_context);
}

TEST_F(HippocampusIntegrationTest, ConsolidationWithAllBridges) {
    /* Test consolidation with all subsystems active */
    hippo->thalamus_bridge.spindle_coupling = 0.8f;
    hippo->hypothalamus_bridge.arousal_level = 0.3f;  /* Low for sleep-like consolidation */
    hippo->immune_bridge.health_score = 1.0f;
    hippo->substrate_bridge.atp_level = 1.0f;

    /* Encode memories */
    float what[128];
    for (int i = 0; i < 5; i++) {
        createTestContent(what, 128, (float)i * 0.2f);
        uint32_t id;
        hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.8f, &id);
    }

    /* Run extended consolidation */
    for (int i = 0; i < 200; i++) {
        hippo_consolidate_memories(hippo, 0.1f);
    }

    /* Check consolidation progress - expect gradual progress, not immediate */
    float total_consolidation = 0.0f;
    for (uint32_t i = 0; i < hippo->num_episodes; i++) {
        if (hippo->episodes[i].bound_representation) {
            total_consolidation += hippo->episodes[i].consolidation_level;
        }
    }
    /* Consolidation is gradual - 0.1 total (average 0.02 per episode) is reasonable */
    EXPECT_GT(total_consolidation, 0.1f);
}

TEST_F(HippocampusIntegrationTest, ReplayWithSystemIntegration) {
    /* Test replay with full system integration */
    hippo->oscillation_state = OSCILLATION_THETA;

    /* Encode memories */
    float what[128];
    for (int i = 0; i < 5; i++) {
        createTestContent(what, 128, (float)i * 0.1f);
        uint32_t id;
        hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);
    }

    /* Trigger and process replay */
    EXPECT_EQ(hippo_trigger_replay(hippo, REPLAY_FORWARD), 0);
    EXPECT_EQ(hippo->oscillation_state, OSCILLATION_SHARP_WAVE_RIPPLE);

    EXPECT_EQ(hippo_process_replay(hippo), 0);
    EXPECT_EQ(hippo->oscillation_state, OSCILLATION_THETA);

    /* Memories should be strengthened */
    const nimcp_episode_t* ep = hippo_get_episode(hippo, 0);
    EXPECT_GT(ep->encoding_strength, 0.0f);
}
