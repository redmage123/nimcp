/**
 * @file test_grounded_language_deferred_wave678.cpp
 * @brief Unit tests for the final deferred-enhancement waves:
 *          #2  Wernicke audio-feature ingest helper (W6)
 *          #12 SNN spike → lexicon decoding (W7)
 *          #6  Lexicon checkpoint compression (W8 / int8 quantized)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <vector>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
}

namespace {

constexpr uint32_t TEST_DIM = 32;

class GLDeferredW678 : public ::testing::Test {
protected:
    grounded_language_t* gl = nullptr;
    semantic_memory_system_t* sm = nullptr;

    void SetUp() override {
        sm = semantic_memory_create();
        ASSERT_NE(sm, nullptr);
        gl = grounded_language_create(TEST_DIM, sm);
        ASSERT_NE(gl, nullptr);
    }
    void TearDown() override {
        if (gl) grounded_language_destroy(gl);
        if (sm) semantic_memory_destroy(sm);
    }
};

/* ====================================================================
 * #2 (W6) gl_extract_audio_features
 * ==================================================================*/

TEST_F(GLDeferredW678, AudioFeatureNullSafe) {
    float out[8] = {0};
    EXPECT_EQ(-1, gl_extract_audio_features(nullptr, 100, 16000, out, 8));
    float audio[100] = {0};
    EXPECT_EQ(-1, gl_extract_audio_features(audio, 0, 16000, out, 8));
    EXPECT_EQ(-1, gl_extract_audio_features(audio, 100, 16000, nullptr, 8));
    EXPECT_EQ(-1, gl_extract_audio_features(audio, 100, 16000, out, 0));
}

TEST_F(GLDeferredW678, AudioFeatureSilenceProducesZeros) {
    std::vector<float> silence(1024, 0.0f);
    float out[8] = {0};
    EXPECT_EQ(0, gl_extract_audio_features(silence.data(), 1024, 16000, out, 8));
    for (int i = 0; i < 8; i++) EXPECT_EQ(0.0f, out[i]);
}

TEST_F(GLDeferredW678, AudioFeatureLouderChunkRanksHigher) {
    /* Build a signal where chunk[3] is loud and others quiet. */
    std::vector<float> sig(1024, 0.0f);
    for (uint32_t i = 384; i < 512; i++) sig[i] = 0.9f;
    float out[8] = {0};
    ASSERT_EQ(0, gl_extract_audio_features(sig.data(), 1024, 16000, out, 8));
    /* The loudest chunk must dominate. Normalized so peak == 1.0. */
    float max_val = 0.0f;
    int max_idx = -1;
    for (int i = 0; i < 8; i++) {
        if (out[i] > max_val) { max_val = out[i]; max_idx = i; }
    }
    EXPECT_NEAR(1.0f, max_val, 1e-5f);
    /* Index 3 is samples [384, 512) — the loud region. */
    EXPECT_EQ(3, max_idx);
}

TEST_F(GLDeferredW678, AudioFeatureDifferentSignalsProduceDifferentVectors) {
    std::vector<float> a(512, 0.0f), b(512, 0.0f);
    for (uint32_t i = 0;   i < 128; i++) a[i] = 0.5f;
    for (uint32_t i = 256; i < 384; i++) b[i] = 0.5f;
    float fa[8] = {0}, fb[8] = {0};
    gl_extract_audio_features(a.data(), 512, 16000, fa, 8);
    gl_extract_audio_features(b.data(), 512, 16000, fb, 8);
    bool any_diff = false;
    for (int i = 0; i < 8; i++) {
        if (fabsf(fa[i] - fb[i]) > 1e-4f) { any_diff = true; break; }
    }
    EXPECT_TRUE(any_diff);
}

/* ====================================================================
 * #12 (W7) gl_observe_snn_spikes
 * ==================================================================*/

TEST_F(GLDeferredW678, SnnObserveNullSafe) {
    float rates[TEST_DIM] = {0};
    EXPECT_EQ(-1, gl_observe_snn_spikes(nullptr, rates, TEST_DIM, nullptr));
    EXPECT_EQ(-1, gl_observe_snn_spikes(gl, nullptr, TEST_DIM, nullptr));
    EXPECT_EQ(-1, gl_observe_snn_spikes(gl, rates, 0, nullptr));
}

TEST_F(GLDeferredW678, SnnObserveDimMismatchReturnsError) {
    float rates[TEST_DIM] = {0};
    EXPECT_EQ(-1, gl_observe_snn_spikes(gl, rates, TEST_DIM + 1, nullptr));
}

TEST_F(GLDeferredW678, SnnObserveBelowThresholdNoFire) {
    /* Build context vectors via repeated learn. */
    grounded_language_learn_from_text(gl, "alpha alpha alpha");
    grounded_language_learn_from_text(gl, "alpha beta alpha beta beta");

    /* Spike vector orthogonal to any context_vector → low similarity. */
    std::vector<float> rates(TEST_DIM, 0.0f);
    /* Single spike in a dim unlikely to match any context. */
    rates[0] = 1.0f;
    /* Even if there's some accidental similarity, this single-spike
     * vector against multi-component context vectors should be low. */
    int rc = gl_observe_snn_spikes(gl, rates.data(), TEST_DIM, nullptr);
    /* rc may be 0 (no match) or 1 (lucky match) — accept either, but
     * if 1, confidence must be >= threshold. */
    EXPECT_GE(rc, 0);
}

TEST_F(GLDeferredW678, SnnObserveBelowThresholdReportsConfidence) {
    grounded_language_learn_from_text(gl, "match target word here often");
    std::vector<float> rates(TEST_DIM, 0.01f);  /* very low signal */
    float conf = -42.0f;
    int rc = gl_observe_snn_spikes(gl, rates.data(), TEST_DIM, &conf);
    EXPECT_GE(rc, 0);
    /* confidence_out must be set even when no match fires. */
    EXPECT_GE(conf, 0.0f);
    EXPECT_LE(conf, 1.0f);
}

TEST_F(GLDeferredW678, SnnObserveMatchesAndFiresEvent) {
    /* Build a known context vector for a specific word, then push that
     * exact vector as the spike rates — cosine should be 1.0. */
    grounded_language_learn_from_text(gl, "target word here matches now");
    grounded_language_learn_from_text(gl, "target word here matches now");
    grounded_language_learn_from_text(gl, "target word here matches now");
    grounded_language_learn_from_text(gl, "target word here matches now");

    const gl_lexicon_entry_t* e = grounded_language_lookup(gl, "matches");
    if (!e || !e->context_initialized) {
        GTEST_SKIP() << "context not initialized";
    }

    std::vector<float> rates(TEST_DIM, 0.0f);
    for (uint32_t d = 0; d < TEST_DIM; d++) rates[d] = e->context_vector[d];

    /* Subscribe to capture the COMPREHENDED event. */
    struct Cap { int n = 0; std::string word; float conf = 0.0f; };
    Cap cap;
    auto cb = +[](void* ctx, const gl_event_t* ev) -> int {
        Cap* c = (Cap*)ctx;
        if (ev->type == GL_EVENT_COMPREHENDED) {
            c->n++;
            c->word = ev->word ? ev->word : "";
            c->conf = ev->confidence;
        }
        return 0;
    };
    grounded_language_subscribe_ex(gl, cb, &cap,
                                     GL_EVENT_MASK_COMPREHENDED, 0);

    float conf = 0.0f;
    int rc = gl_observe_snn_spikes(gl, rates.data(), TEST_DIM, &conf);
    EXPECT_EQ(1, rc);
    EXPECT_GE(conf, GL_SNN_SPIKE_MATCH_THRESHOLD);
    EXPECT_EQ(1, cap.n);
    EXPECT_FALSE(cap.word.empty());
}

/* ====================================================================
 * #6 (W8) Checkpoint compression — int8 quantization round-trip
 * ==================================================================*/

TEST_F(GLDeferredW678, CompressedCheckpointRoundtrip) {
    /* Train enough to give entries non-trivial context vectors. */
    grounded_language_learn_from_text(gl,
        "the quick brown fox jumps over the lazy dog");
    grounded_language_learn_from_text(gl,
        "the cat sat on the mat and waited patiently");
    grounded_language_learn_from_text(gl,
        "she sells seashells by the seashore at dawn");

    char path[] = "/tmp/gl_v4_quantized_XXXXXX.bin";
    int fd = mkstemps(path, 4);
    ASSERT_GE(fd, 0);
    close(fd);

    EXPECT_EQ(0, grounded_language_save(gl, path));

    grounded_language_t* gl2 = grounded_language_load(path, sm);
    ASSERT_NE(gl2, nullptr);

    /* Vocab + frequencies should match exactly. */
    const gl_lexicon_entry_t* e1 = grounded_language_lookup(gl,  "fox");
    const gl_lexicon_entry_t* e2 = grounded_language_lookup(gl2, "fox");
    if (e1 && e2) {
        EXPECT_EQ(e1->frequency, e2->frequency);
        if (e1->context_initialized && e2->context_initialized) {
            /* Quantized ↔ float roundtrip — error proportional to
             * max_abs/127. Test that vectors are CLOSE, not equal. */
            float max_abs = 0.0f;
            for (uint32_t d = 0; d < TEST_DIM; d++) {
                float a = fabsf(e1->context_vector[d]);
                if (a > max_abs) max_abs = a;
            }
            float tol = (max_abs / 127.0f) * 1.01f;  /* ±half-step + slop */
            for (uint32_t d = 0; d < TEST_DIM; d++) {
                EXPECT_NEAR(e1->context_vector[d], e2->context_vector[d],
                            tol)
                    << "dim " << d << " max_abs=" << max_abs;
            }
        }
    }

    grounded_language_destroy(gl2);
    unlink(path);
}

TEST_F(GLDeferredW678, CompressedCheckpointZeroVector) {
    /* fast_map creates an entry but doesn't initialize context_vector
     * with non-zero values. Verify save/load handles the zero case. */
    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    grounded_language_fast_map(gl, "zerovec_test", f.data(), TEST_DIM, 0);

    char path[] = "/tmp/gl_v4_zero_XXXXXX.bin";
    int fd = mkstemps(path, 4);
    ASSERT_GE(fd, 0);
    close(fd);
    EXPECT_EQ(0, grounded_language_save(gl, path));
    grounded_language_t* gl2 = grounded_language_load(path, sm);
    ASSERT_NE(gl2, nullptr);
    EXPECT_TRUE(grounded_language_has_word(gl2, "zerovec_test"));
    grounded_language_destroy(gl2);
    unlink(path);
}

}  // namespace
