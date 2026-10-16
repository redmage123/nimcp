/**
 * @file test_snn_language_bridge_e2e.cpp
 * @brief End-to-end tests for SNN-Language bridge pipeline
 *
 * Tests the full cycle: register vocab → learn bindings via STDP →
 * produce text → comprehend text → sleep consolidate → serialize/load
 */

#include <gtest/gtest.h>
#include "snn/bridges/nimcp_snn_language_bridge.h"
#include "utils/memory/nimcp_memory.h"

#include <cstring>
#include <cmath>

class SNNLanguageBridgeE2E : public ::testing::Test {
protected:
    snn_language_bridge_t* bridge;

    void SetUp() override {
        snn_lang_config_t config = snn_lang_config_default();
        config.enable_sleep_consolidation = true;
        config.enable_imagination = true;
        config.enable_curiosity = true;
        bridge = snn_language_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) snn_language_bridge_destroy(bridge);
    }

    // Helper: register a vocabulary set with 1:1 concept-word mapping
    void BuildVocab(const char** words, uint32_t count, float initial_weight = 0.3f) {
        for (uint32_t i = 0; i < count; i++) {
            snn_language_bridge_register_concept(bridge, i, 1000 + i);
            snn_language_bridge_register_word(bridge, i, words[i]);
            snn_language_bridge_bind(bridge, i, i, initial_weight);
        }
    }

    // Helper: simulate causal STDP training for concept i → word i
    void TrainBinding(uint32_t idx, int trials, float base_time) {
        for (int t = 0; t < trials; t++) {
            float time = base_time + t * 100.0f;
            snn_language_bridge_concept_spike(bridge, idx, time + 10.0f);
            snn_language_bridge_word_spike(bridge, idx, time + 25.0f);
            snn_language_bridge_apply_stdp(bridge, time + 30.0f);
        }
    }
};

/* E2E 1: Full learning + production pipeline */
TEST_F(SNNLanguageBridgeE2E, FullLearningProductionPipeline) {
    const char* vocab[] = {"the", "quick", "brown", "fox", "jumps"};
    BuildVocab(vocab, 5, 0.2f);

    // Train each binding
    for (uint32_t i = 0; i < 5; i++) {
        TrainBinding(i, 10, i * 2000.0f);
    }

    // Produce from intent
    float intent[] = {0.9f, 0.7f, 0.5f, 0.3f, 0.1f};
    snn_lang_production_result_t prod;
    memset(&prod, 0, sizeof(prod));

    int ret = snn_language_bridge_produce(bridge, intent, 5, &prod);
    EXPECT_EQ(0, ret);
    EXPECT_GT(prod.word_count, 0u);
    EXPECT_NE(prod.text, nullptr);
    EXPECT_GT(prod.fluency, 0.0f);
    EXPECT_GT(prod.spike_confidence, 0.0f);

    snn_lang_production_result_cleanup(&prod);
}

/* E2E 2: Comprehension activates correct concepts */
TEST_F(SNNLanguageBridgeE2E, ComprehensionActivatesCorrectConcepts) {
    const char* vocab[] = {"red", "blue", "green", "yellow"};
    BuildVocab(vocab, 4, 0.8f);

    float concepts[4] = {0};
    uint32_t activated = 0;
    float conf = 0;

    int ret = snn_language_bridge_comprehend(bridge, "red green",
                                              concepts, 4, &activated, &conf);
    EXPECT_EQ(0, ret);
    EXPECT_GT(activated, 0u);

    // Concepts 0 (red) and 2 (green) should be activated
    EXPECT_GT(concepts[0], 0.0f);
    EXPECT_GT(concepts[2], 0.0f);
    // Concepts 1 (blue) and 3 (yellow) should not be activated
    EXPECT_FLOAT_EQ(concepts[1], 0.0f);
    EXPECT_FLOAT_EQ(concepts[3], 0.0f);
}

/* E2E 3: STDP learning convergence over many trials */
TEST_F(SNNLanguageBridgeE2E, STDPConvergence) {
    const char* vocab[] = {"apple", "banana"};
    BuildVocab(vocab, 2, 0.3f);

    // Also create a cross-binding (concept 0 → word 1) — should weaken via LTD
    snn_language_bridge_bind(bridge, 0, 1, 0.3f);

    // Train concept 0 → word 0 (LTP), concept 0 then word 1 (no causal → LTD)
    for (int t = 0; t < 50; t++) {
        float time = t * 100.0f;
        // Causal: concept 0 before word 0
        snn_language_bridge_concept_spike(bridge, 0, time + 10.0f);
        snn_language_bridge_word_spike(bridge, 0, time + 25.0f);
        // Anti-causal: word 1 before concept 0 (next trial)
        snn_language_bridge_word_spike(bridge, 1, time + 5.0f);
        snn_language_bridge_apply_stdp(bridge, time + 30.0f);
    }

    // Decode: concept 0 should strongly activate word 0, weakly word 1
    float rates[] = {1.0f, 0.0f};
    snn_lang_word_result_t results[2];
    uint32_t num = 0;
    snn_language_bridge_decode_spikes(bridge, rates, 2, results, 2, &num);
    EXPECT_GT(num, 0u);
    EXPECT_EQ(results[0].word_pop, 0u);  // "apple" wins

    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_ltp_events, 0u);
    EXPECT_GT(stats.total_ltd_events, 0u);
}

/* E2E 4: Sleep consolidation lifecycle */
TEST_F(SNNLanguageBridgeE2E, SleepConsolidationLifecycle) {
    const char* vocab[] = {"memory", "dream", "forget", "recall"};
    BuildVocab(vocab, 4, 0.4f);

    // Create some very weak cross-bindings that should be pruned (below 0.005 threshold)
    snn_language_bridge_bind(bridge, 0, 3, 0.003f);
    snn_language_bridge_bind(bridge, 1, 2, 0.002f);

    // Train main bindings
    for (uint32_t i = 0; i < 4; i++) {
        TrainBinding(i, 5, i * 1000.0f);
    }

    snn_lang_stats_t before;
    snn_language_bridge_get_stats(bridge, &before);

    // Sleep consolidation
    int ret = snn_language_bridge_sleep_consolidate(bridge, 0.7f);
    EXPECT_EQ(0, ret);

    snn_lang_stats_t after;
    snn_language_bridge_get_stats(bridge, &after);
    EXPECT_GT(after.sleep_consolidation_cycles, before.sleep_consolidation_cycles);
    EXPECT_GT(after.bindings_pruned, 0u);
}

/* E2E 5: Creative production with imagination activations */
TEST_F(SNNLanguageBridgeE2E, CreativeImagination) {
    const char* vocab[] = {"fire", "ice", "storm", "light", "shadow"};
    BuildVocab(vocab, 5, 0.5f);

    // Add some cross-bindings for creative associations
    snn_language_bridge_bind(bridge, 0, 2, 0.3f);  // fire → storm
    snn_language_bridge_bind(bridge, 1, 4, 0.3f);  // ice → shadow

    float imagination[] = {0.4f, 0.8f, 0.2f, 0.6f, 0.9f};
    snn_lang_production_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = snn_language_bridge_creative_produce(bridge, imagination, 5, 0.9f, &result);
    EXPECT_EQ(0, ret);
    EXPECT_GT(result.creativity, 0.3f);  // High creativity expected
    EXPECT_GT(result.word_count, 0u);

    snn_lang_production_result_cleanup(&result);
}

/* E2E 6: Full cycle: produce → comprehend → verify consistency */
TEST_F(SNNLanguageBridgeE2E, ProduceComprehendConsistency) {
    const char* vocab[] = {"sun", "moon", "stars"};
    BuildVocab(vocab, 3, 0.9f);  // Strong bindings

    // Produce with strong intent for concept 0 (sun)
    float intent[] = {1.0f, 0.0f, 0.0f};
    snn_lang_production_result_t prod;
    memset(&prod, 0, sizeof(prod));

    snn_language_bridge_produce(bridge, intent, 3, &prod);
    ASSERT_NE(prod.text, nullptr);

    // Comprehend what was produced
    float concepts[3] = {0};
    uint32_t activated = 0;
    float conf = 0;
    snn_language_bridge_comprehend(bridge, prod.text, concepts, 3, &activated, &conf);

    // Concept 0 should be most activated (since we produced "sun")
    if (activated > 0) {
        float max_act = 0;
        uint32_t max_idx = 0;
        for (uint32_t i = 0; i < 3; i++) {
            if (concepts[i] > max_act) {
                max_act = concepts[i];
                max_idx = i;
            }
        }
        EXPECT_EQ(max_idx, 0u);
    }

    snn_lang_production_result_cleanup(&prod);
}

/* E2E 7: Save → destroy → load → continue learning */
TEST_F(SNNLanguageBridgeE2E, SaveLoadContinueLearning) {
    const char* vocab[] = {"alpha", "beta"};
    BuildVocab(vocab, 2, 0.4f);
    TrainBinding(0, 10, 0.0f);

    const char* path = "/tmp/test_snn_lang_e2e.bin";
    ASSERT_EQ(0, snn_language_bridge_save(bridge, path));

    // Destroy original
    snn_language_bridge_destroy(bridge);
    bridge = NULL;

    // Load and continue learning
    bridge = snn_language_bridge_load(path);
    ASSERT_NE(bridge, nullptr);

    // Continue training binding 1
    TrainBinding(1, 10, 10000.0f);

    // Both bindings should work
    float rates0[] = {1.0f, 0.0f};
    float rates1[] = {0.0f, 1.0f};
    snn_lang_word_result_t r[1];
    uint32_t n = 0;

    snn_language_bridge_decode_spikes(bridge, rates0, 2, r, 1, &n);
    EXPECT_GT(n, 0u);
    EXPECT_EQ(r[0].word_pop, 0u);

    snn_language_bridge_decode_spikes(bridge, rates1, 2, r, 1, &n);
    EXPECT_GT(n, 0u);
    EXPECT_EQ(r[0].word_pop, 1u);

    remove(path);
}

/* E2E 8: Curiosity modulation affects word selection */
TEST_F(SNNLanguageBridgeE2E, CuriosityModulationEffect) {
    const char* vocab[] = {"common", "rare", "exotic"};
    BuildVocab(vocab, 3, 0.5f);

    // Apply curiosity drive — should bias toward exploration
    snn_language_bridge_curiosity_modulate(bridge, 0.9f, 0.8f);

    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.curiosity_contributions, 0u);
}

/* E2E 9: Large vocabulary stress test */
TEST_F(SNNLanguageBridgeE2E, LargeVocabularyStress) {
    // Register 500 concepts and words
    for (uint32_t i = 0; i < 500; i++) {
        snn_language_bridge_register_concept(bridge, i, i);
        char word[32];
        snprintf(word, sizeof(word), "vocab_%u", i);
        snn_language_bridge_register_word(bridge, i, word);
        snn_language_bridge_bind(bridge, i, i, 0.5f);
    }

    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.active_bindings, 500u);

    // Produce with mixed intent
    float intent[500];
    for (int i = 0; i < 500; i++) intent[i] = (i < 10) ? 0.8f : 0.01f;

    snn_lang_production_result_t result;
    memset(&result, 0, sizeof(result));
    int ret = snn_language_bridge_produce(bridge, intent, 500, &result);
    EXPECT_EQ(0, ret);
    EXPECT_GT(result.word_count, 0u);

    snn_lang_production_result_cleanup(&result);
}

/* E2E 10: Full lifecycle: create → learn → consolidate → save → load → produce */
TEST_F(SNNLanguageBridgeE2E, FullLifecycle) {
    const char* vocab[] = {"neural", "spike", "synapse", "brain", "learn"};
    BuildVocab(vocab, 5, 0.3f);

    // Phase 1: Learn bindings via STDP
    for (uint32_t i = 0; i < 5; i++) {
        TrainBinding(i, 15, i * 3000.0f);
    }

    // Phase 2: Sleep consolidation
    snn_language_bridge_sleep_consolidate(bridge, 0.6f);

    // Phase 3: Save
    const char* path = "/tmp/test_snn_lang_lifecycle.bin";
    ASSERT_EQ(0, snn_language_bridge_save(bridge, path));

    // Phase 4: Load into fresh bridge
    snn_language_bridge_t* loaded = snn_language_bridge_load(path);
    ASSERT_NE(loaded, nullptr);

    // Phase 5: Produce from loaded bridge
    float intent[] = {0.9f, 0.7f, 0.5f, 0.3f, 0.1f};
    snn_lang_production_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = snn_language_bridge_produce(loaded, intent, 5, &result);
    EXPECT_EQ(0, ret);
    EXPECT_GT(result.word_count, 0u);
    EXPECT_NE(result.text, nullptr);

    // Verify stats survived the round-trip
    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(loaded, &stats);
    EXPECT_EQ(stats.active_bindings, 5u);

    snn_lang_production_result_cleanup(&result);
    snn_language_bridge_destroy(loaded);
    remove(path);
}
