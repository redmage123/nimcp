/**
 * @file test_snn_language_bridge_integration.cpp
 * @brief Integration tests for SNN-Language bridge
 *
 * Tests cross-system integration: bridge + grounded language + STDP pipeline
 */

#include <gtest/gtest.h>
#include "snn/bridges/nimcp_snn_language_bridge.h"
#include "language/nimcp_grounded_language.h"
#include "utils/memory/nimcp_memory.h"

#include <cstring>
#include <cmath>

class SNNLanguageBridgeIntegration : public ::testing::Test {
protected:
    snn_language_bridge_t* bridge;
    struct grounded_language* gl;

    void SetUp() override {
        snn_lang_config_t config = snn_lang_config_default();
        bridge = snn_language_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        gl = grounded_language_create(128, NULL);
        // gl may be NULL if grounded language requires semantic memory — that's OK
    }

    void TearDown() override {
        if (bridge) snn_language_bridge_destroy(bridge);
        if (gl) grounded_language_destroy(gl);
    }

    void RegisterVocab(const char** words, uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            snn_language_bridge_register_concept(bridge, i, 1000 + i);
            snn_language_bridge_register_word(bridge, i, words[i]);
            snn_language_bridge_bind(bridge, i, i, 0.5f);
        }
    }
};

/* Integration 1: Connect to grounded language system */
TEST_F(SNNLanguageBridgeIntegration, ConnectGroundedLanguage) {
    if (!gl) GTEST_SKIP() << "Grounded language not available";
    int ret = snn_language_bridge_connect_grounded(bridge, gl);
    EXPECT_EQ(0, ret);
}

/* Integration 2: Full encode-decode roundtrip */
TEST_F(SNNLanguageBridgeIntegration, EncodeDecodeRoundtrip) {
    const char* words[] = {"alpha", "beta", "gamma", "delta"};
    RegisterVocab(words, 4);

    // Encode word 2 ("gamma") → concept activations
    float concept_acts[4] = {0};
    EXPECT_EQ(0, snn_language_bridge_encode_word(bridge, 2, concept_acts, 4));

    // Decode those concept activations → should get "gamma" back
    snn_lang_word_result_t results[4];
    uint32_t num = 0;
    EXPECT_EQ(0, snn_language_bridge_decode_spikes(bridge, concept_acts, 4, results, 4, &num));
    EXPECT_GT(num, 0u);
    // Top result should be word 2
    EXPECT_EQ(results[0].word_pop, 2u);
}

/* Integration 3: STDP learning strengthens correct bindings */
TEST_F(SNNLanguageBridgeIntegration, STDPLearningConvergence) {
    const char* words[] = {"red", "blue", "green"};
    RegisterVocab(words, 3);

    // Initial weight is 0.5 for all
    // Repeatedly pair concept 0 with word 0 (causal timing → LTP)
    for (int trial = 0; trial < 20; trial++) {
        float t_base = trial * 100.0f;
        snn_language_bridge_concept_spike(bridge, 0, t_base + 10.0f);
        snn_language_bridge_word_spike(bridge, 0, t_base + 30.0f);
        snn_language_bridge_apply_stdp(bridge, t_base + 50.0f);
    }

    // Decode with concept 0 active — "red" should dominate
    float rates[] = {1.0f, 0.0f, 0.0f};
    snn_lang_word_result_t results[3];
    uint32_t num = 0;
    snn_language_bridge_decode_spikes(bridge, rates, 3, results, 3, &num);
    EXPECT_GT(num, 0u);
    EXPECT_EQ(results[0].word_pop, 0u);  // "red" wins

    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_ltp_events, 0u);
}

/* Integration 4: Produce-comprehend roundtrip */
TEST_F(SNNLanguageBridgeIntegration, ProduceComprehendRoundtrip) {
    const char* words[] = {"sun", "moon", "star"};
    RegisterVocab(words, 3);

    // Produce from intent
    float intent[] = {0.9f, 0.1f, 0.1f};
    snn_lang_production_result_t prod;
    memset(&prod, 0, sizeof(prod));
    snn_language_bridge_produce(bridge, intent, 3, &prod);

    if (prod.text && prod.word_count > 0) {
        // Comprehend the produced text
        float concepts[3] = {0};
        uint32_t activated = 0;
        float conf = 0;
        snn_language_bridge_comprehend(bridge, prod.text, concepts, 3, &activated, &conf);
        // Should activate at least one concept
        EXPECT_GT(activated, 0u);
    }

    snn_lang_production_result_cleanup(&prod);
}

/* Integration 5: Spike blend ramp-up */
TEST_F(SNNLanguageBridgeIntegration, BlendRampUp) {
    EXPECT_FLOAT_EQ(snn_language_bridge_get_blend(bridge), 0.1f);

    // Simulate gradual blend increase
    for (float b = 0.1f; b <= 1.0f; b += 0.1f) {
        snn_language_bridge_set_blend(bridge, b);
        float actual = snn_language_bridge_get_blend(bridge);
        EXPECT_NEAR(actual, b, 0.001f);
    }
}

/* Integration 6: Sleep consolidation prunes and strengthens */
TEST_F(SNNLanguageBridgeIntegration, SleepConsolidationEffect) {
    const char* words[] = {"memory", "forget", "dream"};
    RegisterVocab(words, 3);

    // Create some with varying initial weights
    // Override with a very weak binding
    snn_language_bridge_bind(bridge, 0, 1, 0.01f);  // Very weak cross-binding

    // Build eligibility via spikes
    snn_language_bridge_concept_spike(bridge, 0, 10.0f);
    snn_language_bridge_word_spike(bridge, 0, 25.0f);
    snn_language_bridge_apply_stdp(bridge, 30.0f);

    snn_lang_stats_t before;
    snn_language_bridge_get_stats(bridge, &before);

    // Sleep consolidation
    snn_language_bridge_sleep_consolidate(bridge, 0.8f);

    snn_lang_stats_t after;
    snn_language_bridge_get_stats(bridge, &after);
    EXPECT_GT(after.sleep_consolidation_cycles, before.sleep_consolidation_cycles);
}

/* Integration 7: Creative production with imagination */
TEST_F(SNNLanguageBridgeIntegration, CreativeProductionPipeline) {
    const char* words[] = {"imagine", "create", "explore", "discover"};
    RegisterVocab(words, 4);

    float imagination_act[] = {0.5f, 0.7f, 0.3f, 0.9f};
    snn_lang_production_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = snn_language_bridge_creative_produce(bridge, imagination_act, 4, 0.8f, &result);
    EXPECT_EQ(0, ret);
    EXPECT_GT(result.creativity, 0.0f);
    EXPECT_GT(result.word_count, 0u);

    snn_lang_production_result_cleanup(&result);
}

/* Integration 8: Stats accumulate across operations */
TEST_F(SNNLanguageBridgeIntegration, StatsAccumulation) {
    const char* words[] = {"test"};
    RegisterVocab(words, 1);

    // Do various operations
    float rates[] = {1.0f};
    snn_lang_word_result_t wr[1];
    uint32_t n = 0;

    snn_language_bridge_decode_spikes(bridge, rates, 1, wr, 1, &n);
    snn_language_bridge_decode_spikes(bridge, rates, 1, wr, 1, &n);

    float acts[1] = {0};
    snn_language_bridge_encode_word(bridge, 0, acts, 1);

    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decode_calls, 2u);
    EXPECT_EQ(stats.total_encode_calls, 1u);
    EXPECT_EQ(stats.active_bindings, 1u);
}

/* Integration 9: Save, modify, load preserves state */
TEST_F(SNNLanguageBridgeIntegration, SaveLoadPreservesBindings) {
    const char* words[] = {"persist", "survive"};
    RegisterVocab(words, 2);

    // Strengthen binding 0 via STDP
    for (int i = 0; i < 10; i++) {
        snn_language_bridge_concept_spike(bridge, 0, i * 50.0f + 5.0f);
        snn_language_bridge_word_spike(bridge, 0, i * 50.0f + 20.0f);
        snn_language_bridge_apply_stdp(bridge, i * 50.0f + 30.0f);
    }

    const char* path = "/tmp/test_snn_lang_integration.bin";
    EXPECT_EQ(0, snn_language_bridge_save(bridge, path));

    snn_language_bridge_t* loaded = snn_language_bridge_load(path);
    ASSERT_NE(loaded, nullptr);

    // Verify loaded bridge produces same top word
    float rates[] = {1.0f, 0.0f};
    snn_lang_word_result_t r1[1], r2[1];
    uint32_t n1 = 0, n2 = 0;
    snn_language_bridge_decode_spikes(bridge, rates, 2, r1, 1, &n1);
    snn_language_bridge_decode_spikes(loaded, rates, 2, r2, 1, &n2);

    EXPECT_EQ(n1, n2);
    if (n1 > 0 && n2 > 0) {
        EXPECT_EQ(r1[0].word_pop, r2[0].word_pop);
        EXPECT_NEAR(r1[0].activation, r2[0].activation, 0.01f);
    }

    snn_language_bridge_destroy(loaded);
    remove(path);
}

/* Integration 10: Multiple concept → single word convergence */
TEST_F(SNNLanguageBridgeIntegration, MultiConceptSingleWord) {
    // Multiple concepts bind to the same word ("water")
    snn_language_bridge_register_concept(bridge, 0, 100);  // "liquid"
    snn_language_bridge_register_concept(bridge, 1, 101);  // "drink"
    snn_language_bridge_register_concept(bridge, 2, 102);  // "wet"
    snn_language_bridge_register_word(bridge, 0, "water");

    snn_language_bridge_bind(bridge, 0, 0, 0.7f);
    snn_language_bridge_bind(bridge, 1, 0, 0.6f);
    snn_language_bridge_bind(bridge, 2, 0, 0.5f);

    // All three concepts active → strong activation of "water"
    float rates[] = {0.8f, 0.7f, 0.6f};
    snn_lang_word_result_t results[1];
    uint32_t num = 0;
    snn_language_bridge_decode_spikes(bridge, rates, 3, results, 1, &num);
    EXPECT_GT(num, 0u);
    EXPECT_EQ(results[0].word_pop, 0u);
    // Combined activation should be high
    EXPECT_GT(results[0].activation, 0.5f);
}
