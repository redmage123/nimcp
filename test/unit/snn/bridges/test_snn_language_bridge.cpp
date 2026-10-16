/**
 * @file test_snn_language_bridge.cpp
 * @brief Unit tests for SNN-Language bridge (spike-driven word-concept binding)
 *
 * Tests Phases 1-6 of the SNN-Language-Creative integration.
 */

#include <gtest/gtest.h>
#include "snn/bridges/nimcp_snn_language_bridge.h"
#include "utils/memory/nimcp_memory.h"

#include <cstring>
#include <cmath>

class SNNLanguageBridgeTest : public ::testing::Test {
protected:
    snn_language_bridge_t* bridge;
    snn_lang_config_t config;

    void SetUp() override {
        config = snn_lang_config_default();
        bridge = snn_language_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) snn_language_bridge_destroy(bridge);
    }
};

//=============================================================================
// Phase 0: Configuration & Lifecycle
//=============================================================================

TEST_F(SNNLanguageBridgeTest, ConfigDefaults) {
    snn_lang_config_t cfg = snn_lang_config_default();

    /* Assert against the macros, not hardcoded numbers — the pop caps were
     * bumped (512→4096 concept, 4096→32768 word) after this test was written
     * and the literals went stale. Pinning to the macro keeps the check
     * meaningful ("the default wires the macro through") without re-drifting. */
    EXPECT_EQ(cfg.max_concept_pops, (uint32_t)SNN_LANG_MAX_CONCEPT_POPS);
    EXPECT_EQ(cfg.max_word_pops, (uint32_t)SNN_LANG_MAX_WORD_POPS);
    EXPECT_EQ(cfg.neurons_per_pop, SNN_LANG_NEURONS_PER_POP);
    EXPECT_FLOAT_EQ(cfg.stdp_tau_plus, SNN_LANG_DEFAULT_STDP_TAU);
    EXPECT_FLOAT_EQ(cfg.stdp_a_plus, SNN_LANG_DEFAULT_STDP_A_PLUS);
    EXPECT_FLOAT_EQ(cfg.stdp_a_minus, SNN_LANG_DEFAULT_STDP_A_MINUS);
    EXPECT_FLOAT_EQ(cfg.spike_blend, SNN_LANG_SPIKE_BLEND_DEFAULT);
    EXPECT_FLOAT_EQ(cfg.binding_w_max, SNN_LANG_BINDING_W_MAX);
    EXPECT_FLOAT_EQ(cfg.decay_rate, SNN_LANG_DECAY_RATE);
    EXPECT_TRUE(cfg.enable_da_modulation);
}

TEST_F(SNNLanguageBridgeTest, CreateDestroy) {
    // bridge already created in SetUp
    EXPECT_NE(bridge, nullptr);
    // destroy happens in TearDown — just verify no crash
}

TEST_F(SNNLanguageBridgeTest, CreateWithNullConfig) {
    snn_language_bridge_t* b = snn_language_bridge_create(NULL);
    // NULL config → NULL bridge (config required)
    EXPECT_EQ(b, nullptr);
}

TEST_F(SNNLanguageBridgeTest, Reset) {
    // Register and bind something first
    EXPECT_EQ(0, snn_language_bridge_register_concept(bridge, 0, 100));
    EXPECT_EQ(0, snn_language_bridge_register_word(bridge, 0, "test"));
    EXPECT_EQ(0, snn_language_bridge_bind(bridge, 0, 0, 0.5f));

    // Reset clears activations but keeps bindings
    EXPECT_EQ(0, snn_language_bridge_reset(bridge));

    // Stats should be cleared
    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decode_calls, 0u);
}

TEST_F(SNNLanguageBridgeTest, NullBridgeOperations) {
    EXPECT_NE(0, snn_language_bridge_reset(NULL));
    EXPECT_NE(0, snn_language_bridge_register_concept(NULL, 0, 100));
    EXPECT_NE(0, snn_language_bridge_register_word(NULL, 0, "test"));
}

//=============================================================================
// Phase 1: Spike-to-Word Decoding
//=============================================================================

TEST_F(SNNLanguageBridgeTest, RegisterConcept) {
    EXPECT_EQ(0, snn_language_bridge_register_concept(bridge, 0, 100));
    EXPECT_EQ(0, snn_language_bridge_register_concept(bridge, 1, 200));
    EXPECT_EQ(0, snn_language_bridge_register_concept(bridge, 2, 300));
}

TEST_F(SNNLanguageBridgeTest, RegisterWord) {
    EXPECT_EQ(0, snn_language_bridge_register_word(bridge, 0, "dog"));
    EXPECT_EQ(0, snn_language_bridge_register_word(bridge, 1, "cat"));
    EXPECT_EQ(0, snn_language_bridge_register_word(bridge, 2, "bird"));
}

TEST_F(SNNLanguageBridgeTest, RegisterConceptOutOfRange) {
    EXPECT_NE(0, snn_language_bridge_register_concept(bridge, SNN_LANG_MAX_CONCEPT_POPS + 1, 100));
}

TEST_F(SNNLanguageBridgeTest, RegisterWordOutOfRange) {
    EXPECT_NE(0, snn_language_bridge_register_word(bridge, SNN_LANG_MAX_WORD_POPS + 1, "overflow"));
}

TEST_F(SNNLanguageBridgeTest, RegisterWordNullForm) {
    EXPECT_NE(0, snn_language_bridge_register_word(bridge, 0, NULL));
}

TEST_F(SNNLanguageBridgeTest, DecodeSpikes) {
    // Set up concept→word binding
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_concept(bridge, 1, 200);
    snn_language_bridge_register_word(bridge, 0, "dog");
    snn_language_bridge_register_word(bridge, 1, "cat");
    snn_language_bridge_bind(bridge, 0, 0, 0.9f);  // concept 0 → "dog"
    snn_language_bridge_bind(bridge, 1, 1, 0.8f);  // concept 1 → "cat"

    // Fire concept 0 strongly
    float rates[] = {1.0f, 0.1f};
    snn_lang_word_result_t results[2];
    uint32_t num_results = 0;

    int ret = snn_language_bridge_decode_spikes(bridge, rates, 2, results, 2, &num_results);
    EXPECT_EQ(0, ret);
    EXPECT_GT(num_results, 0u);

    // "dog" should have highest activation
    EXPECT_EQ(results[0].word_pop, 0u);
    EXPECT_GT(results[0].activation, 0.0f);
}

TEST_F(SNNLanguageBridgeTest, DecodeSpikesNullInputs) {
    uint32_t num_results = 0;
    snn_lang_word_result_t results[1];
    EXPECT_NE(0, snn_language_bridge_decode_spikes(bridge, NULL, 0, results, 1, &num_results));
}

TEST_F(SNNLanguageBridgeTest, EncodeWord) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "hello");
    snn_language_bridge_bind(bridge, 0, 0, 0.7f);

    float activations[1] = {0.0f};
    EXPECT_EQ(0, snn_language_bridge_encode_word(bridge, 0, activations, 1));
    EXPECT_GT(activations[0], 0.0f);
}

//=============================================================================
// Phase 2: STDP-Driven Word-Concept Binding
//=============================================================================

TEST_F(SNNLanguageBridgeTest, BindConceptWord) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "fire");

    EXPECT_EQ(0, snn_language_bridge_bind(bridge, 0, 0, 0.5f));

    // Verify binding exists via decode
    float rates[] = {1.0f};
    snn_lang_word_result_t results[1];
    uint32_t num = 0;
    snn_language_bridge_decode_spikes(bridge, rates, 1, results, 1, &num);
    EXPECT_GT(num, 0u);
}

TEST_F(SNNLanguageBridgeTest, ConceptSpikeRecord) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    EXPECT_EQ(0, snn_language_bridge_concept_spike(bridge, 0, 10.0f));
}

TEST_F(SNNLanguageBridgeTest, WordSpikeRecord) {
    snn_language_bridge_register_word(bridge, 0, "spike");
    EXPECT_EQ(0, snn_language_bridge_word_spike(bridge, 0, 15.0f));
}

TEST_F(SNNLanguageBridgeTest, STDPLTPPositiveTiming) {
    // Concept spike (pre) before word spike (post) = LTP
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "learn");
    snn_language_bridge_bind(bridge, 0, 0, 0.3f);

    // Concept fires at t=10, word fires at t=30 (dt=+20ms → LTP)
    snn_language_bridge_concept_spike(bridge, 0, 10.0f);
    snn_language_bridge_word_spike(bridge, 0, 30.0f);
    snn_language_bridge_apply_stdp(bridge, 35.0f);

    // Verify weight increased
    float rates[] = {1.0f};
    snn_lang_word_result_t results[1];
    uint32_t num = 0;
    snn_language_bridge_decode_spikes(bridge, rates, 1, results, 1, &num);
    EXPECT_GT(num, 0u);
    // The activation should reflect higher weight than initial 0.3
    EXPECT_GT(results[0].activation, 0.3f);
}

TEST_F(SNNLanguageBridgeTest, STDPLTDNegativeTiming) {
    // Word spike (post) before concept spike (pre) = LTD
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "forget");
    snn_language_bridge_bind(bridge, 0, 0, 0.5f);

    // Word fires at t=10, concept fires at t=30 (dt=-20ms → LTD)
    snn_language_bridge_word_spike(bridge, 0, 10.0f);
    snn_language_bridge_concept_spike(bridge, 0, 30.0f);
    snn_language_bridge_apply_stdp(bridge, 35.0f);

    // Weight should have decreased
    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_stdp_updates, 0u);
}

TEST_F(SNNLanguageBridgeTest, STDPMultipleBindings) {
    // Multiple concept-word pairs with different timings
    for (uint32_t i = 0; i < 5; i++) {
        snn_language_bridge_register_concept(bridge, i, 100 + i);
        char word[16];
        snprintf(word, sizeof(word), "word%u", i);
        snn_language_bridge_register_word(bridge, i, word);
        snn_language_bridge_bind(bridge, i, i, 0.4f);
    }

    // Spike all concepts, then all words
    for (uint32_t i = 0; i < 5; i++) {
        snn_language_bridge_concept_spike(bridge, i, 10.0f + i);
        snn_language_bridge_word_spike(bridge, i, 25.0f + i);
    }
    snn_language_bridge_apply_stdp(bridge, 40.0f);

    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_stdp_updates, 0u);
    EXPECT_EQ(stats.active_bindings, 5u);
}

TEST_F(SNNLanguageBridgeTest, PruneWeakBindings) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "weak");
    snn_language_bridge_register_word(bridge, 1, "strong");
    snn_language_bridge_bind(bridge, 0, 0, 0.01f);  // Very weak
    snn_language_bridge_bind(bridge, 0, 1, 0.9f);   // Strong

    int pruned = snn_language_bridge_prune(bridge, 0.05f);
    EXPECT_GE(pruned, 0);

    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_LE(stats.active_bindings, 1u);  // Only strong binding remains
}

//=============================================================================
// Phase 3: Production (Broca pathway)
//=============================================================================

TEST_F(SNNLanguageBridgeTest, ProduceWord) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_concept(bridge, 1, 200);
    snn_language_bridge_register_word(bridge, 0, "hello");
    snn_language_bridge_register_word(bridge, 1, "world");
    snn_language_bridge_bind(bridge, 0, 0, 0.9f);
    snn_language_bridge_bind(bridge, 1, 1, 0.8f);

    float concept_acts[] = {0.9f, 0.1f};
    snn_lang_word_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = snn_language_bridge_produce_word(bridge, concept_acts, 2, &result);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(result.word_pop, 0u);  // "hello" should win
    EXPECT_GT(result.activation, 0.0f);
    EXPECT_GT(result.confidence, 0.0f);
}

TEST_F(SNNLanguageBridgeTest, ProduceSequence) {
    // Register vocab
    const char* words[] = {"the", "quick", "brown", "fox"};
    for (uint32_t i = 0; i < 4; i++) {
        snn_language_bridge_register_concept(bridge, i, 100 + i);
        snn_language_bridge_register_word(bridge, i, words[i]);
        snn_language_bridge_bind(bridge, i, i, 0.8f);
    }

    float intent[] = {0.5f, 0.5f, 0.5f, 0.5f};
    snn_lang_production_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = snn_language_bridge_produce(bridge, intent, 4, &result);
    EXPECT_EQ(0, ret);
    EXPECT_GT(result.word_count, 0u);
    EXPECT_NE(result.text, nullptr);
    EXPECT_GT(result.fluency, 0.0f);

    snn_lang_production_result_cleanup(&result);
}

TEST_F(SNNLanguageBridgeTest, ProduceNullInputs) {
    snn_lang_production_result_t result;
    EXPECT_NE(0, snn_language_bridge_produce(bridge, NULL, 0, &result));
    EXPECT_NE(0, snn_language_bridge_produce(NULL, NULL, 0, &result));
}

//=============================================================================
// Phase 4: Comprehension (Wernicke pathway)
//=============================================================================

TEST_F(SNNLanguageBridgeTest, ComprehendText) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "dog");
    snn_language_bridge_bind(bridge, 0, 0, 0.9f);

    float activations[1] = {0.0f};
    uint32_t num_activated = 0;
    float confidence = 0.0f;

    int ret = snn_language_bridge_comprehend(bridge, "dog",
                                              activations, 1, &num_activated, &confidence);
    EXPECT_EQ(0, ret);
    EXPECT_GT(num_activated, 0u);
    EXPECT_GT(activations[0], 0.0f);
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(SNNLanguageBridgeTest, ComprehendUnknownWord) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "known");
    snn_language_bridge_bind(bridge, 0, 0, 0.9f);

    float activations[1] = {0.0f};
    uint32_t num_activated = 0;
    float confidence = 0.0f;

    int ret = snn_language_bridge_comprehend(bridge, "unknown_xyz",
                                              activations, 1, &num_activated, &confidence);
    EXPECT_EQ(0, ret);
    // Unknown word → no concept activation
    EXPECT_FLOAT_EQ(activations[0], 0.0f);
}

TEST_F(SNNLanguageBridgeTest, ComprehendMultiWord) {
    const char* words[] = {"big", "red", "ball"};
    for (uint32_t i = 0; i < 3; i++) {
        snn_language_bridge_register_concept(bridge, i, 100 + i);
        snn_language_bridge_register_word(bridge, i, words[i]);
        snn_language_bridge_bind(bridge, i, i, 0.8f);
    }

    float activations[3] = {0.0f};
    uint32_t num_activated = 0;
    float confidence = 0.0f;

    int ret = snn_language_bridge_comprehend(bridge, "big red ball",
                                              activations, 3, &num_activated, &confidence);
    EXPECT_EQ(0, ret);
    EXPECT_GE(num_activated, 1u);
}

TEST_F(SNNLanguageBridgeTest, ComprehendNullText) {
    float activations[1];
    uint32_t num = 0;
    float conf = 0.0f;
    EXPECT_NE(0, snn_language_bridge_comprehend(bridge, NULL, activations, 1, &num, &conf));
}

//=============================================================================
// Phase 5: Creative/Imagination Integration
//=============================================================================

TEST_F(SNNLanguageBridgeTest, CreativeProduce) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "dream");
    snn_language_bridge_register_word(bridge, 1, "star");
    snn_language_bridge_bind(bridge, 0, 0, 0.7f);
    snn_language_bridge_bind(bridge, 0, 1, 0.6f);

    float imagination[] = {0.8f};
    snn_lang_production_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = snn_language_bridge_creative_produce(bridge, imagination, 1, 0.5f, &result);
    EXPECT_EQ(0, ret);
    EXPECT_GT(result.creativity, 0.0f);

    snn_lang_production_result_cleanup(&result);
}

TEST_F(SNNLanguageBridgeTest, CuriosityModulate) {
    EXPECT_EQ(0, snn_language_bridge_curiosity_modulate(bridge, 0.8f, 0.6f));
}

TEST_F(SNNLanguageBridgeTest, CuriosityModulateClamps) {
    // Values should be accepted even if extreme
    EXPECT_EQ(0, snn_language_bridge_curiosity_modulate(bridge, 0.0f, 0.0f));
    EXPECT_EQ(0, snn_language_bridge_curiosity_modulate(bridge, 1.0f, 1.0f));
}

//=============================================================================
// Phase 6: Sleep Consolidation
//=============================================================================

TEST_F(SNNLanguageBridgeTest, SleepConsolidate) {
    // Set up some bindings with varied eligibility
    for (uint32_t i = 0; i < 10; i++) {
        snn_language_bridge_register_concept(bridge, i, 100 + i);
        char word[16];
        snprintf(word, sizeof(word), "word%u", i);
        snn_language_bridge_register_word(bridge, i, word);
        snn_language_bridge_bind(bridge, i, i, 0.1f * (i + 1));
    }

    // Fire some spikes to build eligibility
    for (uint32_t i = 0; i < 5; i++) {
        snn_language_bridge_concept_spike(bridge, i, 10.0f + i);
        snn_language_bridge_word_spike(bridge, i, 25.0f + i);
    }
    snn_language_bridge_apply_stdp(bridge, 30.0f);

    // Consolidate
    int ret = snn_language_bridge_sleep_consolidate(bridge, 0.5f);
    EXPECT_EQ(0, ret);

    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.sleep_consolidation_cycles, 0u);
}

//=============================================================================
// Statistics & Introspection
//=============================================================================

TEST_F(SNNLanguageBridgeTest, GetStats) {
    snn_lang_stats_t stats;
    EXPECT_EQ(0, snn_language_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(stats.active_bindings, 0u);
    EXPECT_FLOAT_EQ(stats.spike_blend_current, SNN_LANG_SPIKE_BLEND_DEFAULT);
}

TEST_F(SNNLanguageBridgeTest, GetStatsNull) {
    snn_lang_stats_t stats;
    EXPECT_NE(0, snn_language_bridge_get_stats(NULL, &stats));
    EXPECT_NE(0, snn_language_bridge_get_stats(bridge, NULL));
}

TEST_F(SNNLanguageBridgeTest, ResetStats) {
    // Generate some stats
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "test");
    snn_language_bridge_bind(bridge, 0, 0, 0.5f);
    float rates[] = {1.0f};
    snn_lang_word_result_t results[1];
    uint32_t num = 0;
    snn_language_bridge_decode_spikes(bridge, rates, 1, results, 1, &num);

    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_decode_calls, 0u);

    // Reset
    EXPECT_EQ(0, snn_language_bridge_reset_stats(bridge));
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decode_calls, 0u);
}

TEST_F(SNNLanguageBridgeTest, BlendGetSet) {
    EXPECT_FLOAT_EQ(snn_language_bridge_get_blend(bridge), SNN_LANG_SPIKE_BLEND_DEFAULT);

    snn_language_bridge_set_blend(bridge, 0.5f);
    EXPECT_FLOAT_EQ(snn_language_bridge_get_blend(bridge), 0.5f);

    // Clamp to [0, 1]
    snn_language_bridge_set_blend(bridge, -0.1f);
    EXPECT_GE(snn_language_bridge_get_blend(bridge), 0.0f);

    snn_language_bridge_set_blend(bridge, 1.5f);
    EXPECT_LE(snn_language_bridge_get_blend(bridge), 1.0f);
}

TEST_F(SNNLanguageBridgeTest, BlendNullBridge) {
    EXPECT_FLOAT_EQ(snn_language_bridge_get_blend(NULL), 0.0f);
    // Should not crash
    snn_language_bridge_set_blend(NULL, 0.5f);
}

//=============================================================================
// Serialization
//=============================================================================

TEST_F(SNNLanguageBridgeTest, SaveLoad) {
    // Register and bind
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "persist");
    snn_language_bridge_bind(bridge, 0, 0, 0.75f);

    const char* path = "/tmp/test_snn_lang_bridge.bin";
    EXPECT_EQ(0, snn_language_bridge_save(bridge, path));

    snn_language_bridge_t* loaded = snn_language_bridge_load(path);
    ASSERT_NE(loaded, nullptr);

    // Verify loaded bridge has the binding
    float rates[] = {1.0f};
    snn_lang_word_result_t results[1];
    uint32_t num = 0;
    snn_language_bridge_decode_spikes(loaded, rates, 1, results, 1, &num);
    EXPECT_GT(num, 0u);

    snn_language_bridge_destroy(loaded);
    remove(path);
}

TEST_F(SNNLanguageBridgeTest, SaveNullPath) {
    EXPECT_NE(0, snn_language_bridge_save(bridge, NULL));
}

TEST_F(SNNLanguageBridgeTest, LoadNonexistent) {
    snn_language_bridge_t* loaded = snn_language_bridge_load("/tmp/nonexistent_bridge_xyz.bin");
    EXPECT_EQ(loaded, nullptr);
}

//=============================================================================
// Connection tests
//=============================================================================

TEST_F(SNNLanguageBridgeTest, ConnectGroundedNull) {
    // NULL subsystem pointer is accepted (disconnects)
    EXPECT_EQ(0, snn_language_bridge_connect_grounded(bridge, NULL));
}

TEST_F(SNNLanguageBridgeTest, ConnectNeuromodNull) {
    // NULL subsystem pointer is accepted (disconnects)
    EXPECT_EQ(0, snn_language_bridge_connect_neuromod(bridge, NULL));
}

TEST_F(SNNLanguageBridgeTest, ConnectImagination) {
    EXPECT_EQ(0, snn_language_bridge_connect_imagination(bridge, NULL));
}

TEST_F(SNNLanguageBridgeTest, ConnectCuriosity) {
    EXPECT_EQ(0, snn_language_bridge_connect_curiosity(bridge, NULL));
}

//=============================================================================
// Edge cases
//=============================================================================

TEST_F(SNNLanguageBridgeTest, DecodeWithNoBindings) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    float rates[] = {1.0f};
    snn_lang_word_result_t results[1];
    uint32_t num = 0;
    int ret = snn_language_bridge_decode_spikes(bridge, rates, 1, results, 1, &num);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(num, 0u);
}

TEST_F(SNNLanguageBridgeTest, STDPWithNoSpikes) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "silent");
    snn_language_bridge_bind(bridge, 0, 0, 0.5f);

    // Apply STDP with no prior spikes — should be no-op
    EXPECT_EQ(0, snn_language_bridge_apply_stdp(bridge, 100.0f));

    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_ltp_events, 0u);
    EXPECT_EQ(stats.total_ltd_events, 0u);
}

TEST_F(SNNLanguageBridgeTest, LargeVocabularyBinding) {
    // Bind 100 concept-word pairs
    for (uint32_t i = 0; i < 100; i++) {
        snn_language_bridge_register_concept(bridge, i, 1000 + i);
        char word[32];
        snprintf(word, sizeof(word), "vocabulary_%u", i);
        snn_language_bridge_register_word(bridge, i, word);
        snn_language_bridge_bind(bridge, i, i, 0.5f);
    }

    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.active_bindings, 100u);
}

TEST_F(SNNLanguageBridgeTest, ProductionCleanup) {
    snn_lang_production_result_t result;
    memset(&result, 0, sizeof(result));
    // Should not crash on zero result
    snn_lang_production_result_cleanup(&result);
    // Should not crash on NULL
    snn_lang_production_result_cleanup(NULL);
}
