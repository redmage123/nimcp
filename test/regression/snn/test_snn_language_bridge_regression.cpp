/**
 * @file test_snn_language_bridge_regression.cpp
 * @brief Regression tests for SNN-Language bridge
 *
 * Guards against known bug patterns: hash collisions, weight drift,
 * serialization corruption, edge case crashes.
 */

#include <gtest/gtest.h>
#include "snn/bridges/nimcp_snn_language_bridge.h"
#include "utils/memory/nimcp_memory.h"

#include <cstring>
#include <cmath>

class SNNLanguageBridgeRegression : public ::testing::Test {
protected:
    snn_language_bridge_t* bridge;

    void SetUp() override {
        snn_lang_config_t config = snn_lang_config_default();
        bridge = snn_language_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) snn_language_bridge_destroy(bridge);
    }
};

/* Regression 1: Hash collision handling — many bindings same bucket */
TEST_F(SNNLanguageBridgeRegression, HashCollisionStress) {
    // Register many concepts and words with potentially colliding hashes
    for (uint32_t i = 0; i < 200; i++) {
        snn_language_bridge_register_concept(bridge, i, i * 8192);  // Multiples may collide
        char word[32];
        snprintf(word, sizeof(word), "w%u", i);
        snn_language_bridge_register_word(bridge, i, word);
        // Bind concept i → word i
        EXPECT_EQ(0, snn_language_bridge_bind(bridge, i, i, 0.5f));
    }

    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.active_bindings, 200u);

    // Verify each binding is independently accessible
    for (uint32_t i = 0; i < 200; i++) {
        float rates[200] = {0};
        rates[i] = 1.0f;
        snn_lang_word_result_t results[1];
        uint32_t num = 0;
        snn_language_bridge_decode_spikes(bridge, rates, 200, results, 1, &num);
        EXPECT_GT(num, 0u) << "Failed for concept " << i;
        EXPECT_EQ(results[0].word_pop, i) << "Wrong word for concept " << i;
    }
}

/* Regression 2: Weight stays in [0, 1] after many STDP updates */
TEST_F(SNNLanguageBridgeRegression, WeightBoundsSoftClamp) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "bounded");
    snn_language_bridge_bind(bridge, 0, 0, 0.5f);

    // Hammer LTP — weight should approach but not exceed 1.0
    for (int i = 0; i < 1000; i++) {
        float t = i * 50.0f;
        snn_language_bridge_concept_spike(bridge, 0, t + 5.0f);
        snn_language_bridge_word_spike(bridge, 0, t + 15.0f);
        snn_language_bridge_apply_stdp(bridge, t + 20.0f);
    }

    // Decode to check weight via activation
    float rates[] = {1.0f};
    snn_lang_word_result_t results[1];
    uint32_t num = 0;
    snn_language_bridge_decode_spikes(bridge, rates, 1, results, 1, &num);
    EXPECT_GT(num, 0u);
    EXPECT_LE(results[0].activation, 1.0f);
    EXPECT_GE(results[0].activation, 0.0f);
}

/* Regression 3: LTD doesn't go below 0 */
TEST_F(SNNLanguageBridgeRegression, WeightFloorZero) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "floor");
    snn_language_bridge_bind(bridge, 0, 0, 0.1f);  // Start low

    // Hammer LTD — word before concept = depression
    for (int i = 0; i < 1000; i++) {
        float t = i * 50.0f;
        snn_language_bridge_word_spike(bridge, 0, t + 5.0f);
        snn_language_bridge_concept_spike(bridge, 0, t + 15.0f);
        snn_language_bridge_apply_stdp(bridge, t + 20.0f);
    }

    // Weight should be at or near 0, but not negative
    float rates[] = {1.0f};
    snn_lang_word_result_t results[1];
    uint32_t num = 0;
    snn_language_bridge_decode_spikes(bridge, rates, 1, results, 1, &num);
    // Activation may be 0 (pruned) or very small
    if (num > 0) {
        EXPECT_GE(results[0].activation, 0.0f);
    }
}

/* Regression 4: Prune doesn't corrupt hash chain */
TEST_F(SNNLanguageBridgeRegression, PrunePreservesRemainingBindings) {
    // Create 50 bindings, half weak half strong
    for (uint32_t i = 0; i < 50; i++) {
        snn_language_bridge_register_concept(bridge, i, 100 + i);
        char word[16];
        snprintf(word, sizeof(word), "w%u", i);
        snn_language_bridge_register_word(bridge, i, word);
        float w = (i % 2 == 0) ? 0.01f : 0.8f;
        snn_language_bridge_bind(bridge, i, i, w);
    }

    // Prune weak
    snn_language_bridge_prune(bridge, 0.05f);

    // Verify strong bindings still work
    for (uint32_t i = 1; i < 50; i += 2) {  // Odd = strong
        float rates[50] = {0};
        rates[i] = 1.0f;
        snn_lang_word_result_t results[1];
        uint32_t num = 0;
        snn_language_bridge_decode_spikes(bridge, rates, 50, results, 1, &num);
        EXPECT_GT(num, 0u) << "Strong binding " << i << " lost after prune";
    }
}

/* Regression 5: Serialization roundtrip preserves exact weights */
TEST_F(SNNLanguageBridgeRegression, SerializationExactWeights) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_concept(bridge, 1, 200);
    snn_language_bridge_register_word(bridge, 0, "exact");
    snn_language_bridge_register_word(bridge, 1, "precise");
    snn_language_bridge_bind(bridge, 0, 0, 0.123456f);
    snn_language_bridge_bind(bridge, 1, 1, 0.789012f);

    const char* path = "/tmp/test_snn_lang_regression.bin";
    ASSERT_EQ(0, snn_language_bridge_save(bridge, path));

    snn_language_bridge_t* loaded = snn_language_bridge_load(path);
    ASSERT_NE(loaded, nullptr);

    // Compare activations for each concept
    for (uint32_t c = 0; c < 2; c++) {
        float rates_orig[2] = {0}, rates_loaded[2] = {0};
        rates_orig[c] = 1.0f;
        rates_loaded[c] = 1.0f;

        snn_lang_word_result_t r1[1], r2[1];
        uint32_t n1 = 0, n2 = 0;
        snn_language_bridge_decode_spikes(bridge, rates_orig, 2, r1, 1, &n1);
        snn_language_bridge_decode_spikes(loaded, rates_loaded, 2, r2, 1, &n2);

        EXPECT_EQ(n1, n2);
        if (n1 > 0 && n2 > 0) {
            EXPECT_FLOAT_EQ(r1[0].activation, r2[0].activation);
        }
    }

    snn_language_bridge_destroy(loaded);
    remove(path);
}

/* Regression 6: Double destroy doesn't crash */
TEST_F(SNNLanguageBridgeRegression, DoubleDestroyNoCrash) {
    snn_language_bridge_t* b = snn_language_bridge_create(NULL);
    snn_language_bridge_destroy(b);
    // Second destroy with dangling pointer — can't safely test, but NULL is safe
    snn_language_bridge_destroy(NULL);
}

/* Regression 7: Reset followed by operations */
TEST_F(SNNLanguageBridgeRegression, ResetThenOperate) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "reset");
    snn_language_bridge_bind(bridge, 0, 0, 0.5f);

    snn_language_bridge_reset(bridge);

    // Should still be able to decode (bindings preserved)
    float rates[] = {1.0f};
    snn_lang_word_result_t results[1];
    uint32_t num = 0;
    EXPECT_EQ(0, snn_language_bridge_decode_spikes(bridge, rates, 1, results, 1, &num));
}

/* Regression 8: Comprehend with repeated words */
TEST_F(SNNLanguageBridgeRegression, ComprehendRepeatedWords) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "echo");
    snn_language_bridge_bind(bridge, 0, 0, 0.8f);

    float acts[1] = {0};
    uint32_t activated = 0;
    float conf = 0;
    int ret = snn_language_bridge_comprehend(bridge, "echo echo echo",
                                              acts, 1, &activated, &conf);
    EXPECT_EQ(0, ret);
    // Repeated words should accumulate activation
    EXPECT_GT(acts[0], 0.0f);
}

/* Regression 9: Production with zero intent */
TEST_F(SNNLanguageBridgeRegression, ProduceZeroIntent) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "nothing");
    snn_language_bridge_bind(bridge, 0, 0, 0.5f);

    float intent[] = {0.0f};
    snn_lang_production_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = snn_language_bridge_produce(bridge, intent, 1, &result);
    // Zero intent may return -1 (nothing to produce) or 0 with empty output — both are valid
    (void)ret;
    // Should not crash regardless
    snn_lang_production_result_cleanup(&result);
}

/* Regression 10: Concurrent bind + prune stability */
TEST_F(SNNLanguageBridgeRegression, BindPruneInterleaved) {
    for (uint32_t round = 0; round < 10; round++) {
        for (uint32_t i = 0; i < 20; i++) {
            snn_language_bridge_register_concept(bridge, i, i);
            char w[16];
            snprintf(w, sizeof(w), "w%u_%u", round, i);
            snn_language_bridge_register_word(bridge, i, w);
            snn_language_bridge_bind(bridge, i, i, (i % 3 == 0) ? 0.01f : 0.7f);
        }
        snn_language_bridge_prune(bridge, 0.05f);
    }

    snn_lang_stats_t stats;
    snn_language_bridge_get_stats(bridge, &stats);
    // Should have some bindings remaining
    EXPECT_GT(stats.active_bindings, 0u);
}

/* Regression 11: STDP with identical pre/post timing */
TEST_F(SNNLanguageBridgeRegression, STDPSimultaneousSpikes) {
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "simultaneous");
    snn_language_bridge_bind(bridge, 0, 0, 0.5f);

    // Simultaneous spike — dt=0
    snn_language_bridge_concept_spike(bridge, 0, 10.0f);
    snn_language_bridge_word_spike(bridge, 0, 10.0f);
    EXPECT_EQ(0, snn_language_bridge_apply_stdp(bridge, 15.0f));
    // Should not crash or produce NaN
}

/* Regression 12: Blend at extremes */
TEST_F(SNNLanguageBridgeRegression, BlendExtremes) {
    snn_language_bridge_set_blend(bridge, 0.0f);
    EXPECT_FLOAT_EQ(snn_language_bridge_get_blend(bridge), 0.0f);

    snn_language_bridge_set_blend(bridge, 1.0f);
    EXPECT_FLOAT_EQ(snn_language_bridge_get_blend(bridge), 1.0f);

    // Produce with extreme blend should not crash
    snn_language_bridge_register_concept(bridge, 0, 100);
    snn_language_bridge_register_word(bridge, 0, "extreme");
    snn_language_bridge_bind(bridge, 0, 0, 0.5f);

    float intent[] = {1.0f};
    snn_lang_production_result_t result;
    memset(&result, 0, sizeof(result));
    snn_language_bridge_produce(bridge, intent, 1, &result);
    snn_lang_production_result_cleanup(&result);
}
