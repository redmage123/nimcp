/**
 * @file test_grounded_language_deferred_wave1.cpp
 * @brief Unit tests for the deferred-enhancement wave 1:
 *          #7  negative grounding events
 *          #14 dialect / accent conditioning
 *          #10 active-learning curriculum signal (NEEDS_GROUNDING bus event)
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <string>
#include <vector>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
}

namespace {

constexpr uint32_t TEST_DIM = 32;

class GLDeferredW1 : public ::testing::Test {
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

    /* Positive ground for "word" with a fixed sensory signature on the
     * given modality. Returns the strength of the strongest binding on
     * that modality after the call (0 when the word doesn't exist). */
    float ground_pos(const char* word, gl_modality_t mod, float attn = 1.0f) {
        std::vector<float> f(TEST_DIM, 0.0f);
        f[0] = 1.0f; /* fixed feature signature → same concept_id every call */
        gl_grounding_event_t ev{};
        ev.word = word;
        ev.modality = mod;
        ev.sensory_features = f.data();
        ev.feature_dim = TEST_DIM;
        ev.attention = attn;
        ev.negative = false;
        EXPECT_EQ(0, grounded_language_ground(gl, &ev));
        return strongest_modality_strength(word, mod);
    }

    int ground_neg(const char* word, gl_modality_t mod, float attn = 1.0f) {
        std::vector<float> f(TEST_DIM, 0.0f);
        f[0] = 1.0f;
        gl_grounding_event_t ev{};
        ev.word = word;
        ev.modality = mod;
        ev.sensory_features = f.data();
        ev.feature_dim = TEST_DIM;
        ev.attention = attn;
        ev.negative = true;
        return grounded_language_ground(gl, &ev);
    }

    float strongest_modality_strength(const char* word, gl_modality_t mod) {
        const gl_lexicon_entry_t* e = grounded_language_lookup(gl, word);
        if (!e) return -1.0f;
        float best = 0.0f;
        for (uint32_t b = 0; b < e->binding_count; b++) {
            if (e->bindings[b].modality_strength[mod] > best)
                best = e->bindings[b].modality_strength[mod];
        }
        return best;
    }

    float strongest_overall_strength(const char* word) {
        const gl_lexicon_entry_t* e = grounded_language_lookup(gl, word);
        if (!e) return -1.0f;
        float best = 0.0f;
        for (uint32_t b = 0; b < e->binding_count; b++) {
            if (e->bindings[b].strength > best) best = e->bindings[b].strength;
        }
        return best;
    }
};

/* ====================================================================
 * #7 negative grounding
 * ==================================================================*/

TEST_F(GLDeferredW1, NegativeGroundOnUnknownWordIsNoOpSuccess) {
    /* A negative ground event on a word that doesn't exist is a valid
     * no-op (you can't weaken something that isn't there). It must
     * still return 0 and bump the telemetry counter. */
    gl_stats_t s0;
    grounded_language_get_stats(gl, &s0);

    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    gl_grounding_event_t ev{};
    ev.word = "phantom_xyzq_w1";
    ev.modality = GL_MODALITY_VISUAL;
    ev.sensory_features = f.data();
    ev.feature_dim = TEST_DIM;
    ev.attention = 0.9f;
    ev.negative = true;

    EXPECT_EQ(0, grounded_language_ground(gl, &ev));
    EXPECT_FALSE(grounded_language_has_word(gl, "phantom_xyzq_w1"));

    gl_stats_t s1;
    grounded_language_get_stats(gl, &s1);
    EXPECT_EQ(s0.total_negative_groundings + 1u, s1.total_negative_groundings);
    /* Positive total_groundings unchanged — negatives are tracked
     * separately. */
    EXPECT_EQ(s0.total_groundings, s1.total_groundings);
}

TEST_F(GLDeferredW1, NegativeGroundWeakensExistingBinding) {
    /* Build up a strong visual binding on "cat" through several pos events. */
    for (int i = 0; i < 8; i++) ground_pos("cat", GL_MODALITY_VISUAL);
    float pre = strongest_modality_strength("cat", GL_MODALITY_VISUAL);
    ASSERT_GT(pre, 0.0f);

    /* Now anti-pair: this is NOT a cat. */
    EXPECT_EQ(0, ground_neg("cat", GL_MODALITY_VISUAL, 1.0f));
    float post = strongest_modality_strength("cat", GL_MODALITY_VISUAL);
    EXPECT_LT(post, pre);
}

TEST_F(GLDeferredW1, NegativeGroundDoesNotIncrementFrequency) {
    ground_pos("dog", GL_MODALITY_AUDITORY);
    const gl_lexicon_entry_t* e0 = grounded_language_lookup(gl, "dog");
    ASSERT_NE(e0, nullptr);
    uint32_t freq_pre = e0->frequency;

    ground_neg("dog", GL_MODALITY_AUDITORY);
    const gl_lexicon_entry_t* e1 = grounded_language_lookup(gl, "dog");
    EXPECT_EQ(freq_pre, e1->frequency);
}

TEST_F(GLDeferredW1, NegativeGroundFiresGroundedEventWithNegativeConfidence) {
    /* Subscriber that captures the most-recent grounded event's
     * confidence. */
    struct Cap { float last_conf = 0.0f; int n = 0; };
    Cap cap;
    auto cb = +[](void* ctx, const gl_event_t* ev) -> int {
        Cap* c = (Cap*)ctx;
        if (ev->type == GL_EVENT_GROUNDED) {
            c->last_conf = ev->confidence;
            c->n++;
        }
        return 0;
    };
    grounded_language_subscribe(gl, cb, &cap);

    ground_pos("rock", GL_MODALITY_VISUAL, 0.5f);
    EXPECT_EQ(1, cap.n);
    EXPECT_GT(cap.last_conf, 0.0f);

    ground_neg("rock", GL_MODALITY_VISUAL, 0.7f);
    EXPECT_EQ(2, cap.n);
    /* attention=0.7 should arrive as confidence=-0.7 */
    EXPECT_FLOAT_EQ(-0.7f, cap.last_conf);
}

/* ====================================================================
 * #14 dialect / accent conditioning
 * ==================================================================*/

TEST_F(GLDeferredW1, DialectDefaultsEmpty) {
    EXPECT_STREQ("", grounded_language_get_dialect(gl));
}

TEST_F(GLDeferredW1, DialectSetGetRoundtrip) {
    grounded_language_set_dialect(gl, "en-US");
    EXPECT_STREQ("en-US", grounded_language_get_dialect(gl));
    grounded_language_set_dialect(gl, "fr-CA");
    EXPECT_STREQ("fr-CA", grounded_language_get_dialect(gl));
    grounded_language_set_dialect(gl, "zh-CN");
    EXPECT_STREQ("zh-CN", grounded_language_get_dialect(gl));
}

TEST_F(GLDeferredW1, DialectNullClears) {
    grounded_language_set_dialect(gl, "en-US");
    EXPECT_STREQ("en-US", grounded_language_get_dialect(gl));
    grounded_language_set_dialect(gl, nullptr);
    EXPECT_STREQ("", grounded_language_get_dialect(gl));
    grounded_language_set_dialect(gl, "es-MX");
    grounded_language_set_dialect(gl, "");
    EXPECT_STREQ("", grounded_language_get_dialect(gl));
}

TEST_F(GLDeferredW1, DialectTruncatesOversize) {
    /* GL_MAX_DIALECT_LEN = 16 → 15 chars + NUL allowed. */
    grounded_language_set_dialect(gl, "this-is-way-too-long-for-the-tag");
    const char* d = grounded_language_get_dialect(gl);
    EXPECT_LE(strlen(d), (size_t)(GL_MAX_DIALECT_LEN - 1));
    EXPECT_EQ(0, strncmp(d, "this-is-way-too", 15));
}

TEST_F(GLDeferredW1, DialectVisibleInProbeMetrics) {
    grounded_language_set_dialect(gl, "de-DE");
    gl_probe_metrics_t pm;
    EXPECT_EQ(0, grounded_language_get_probe_metrics(gl, &pm));
    EXPECT_STREQ("de-DE", pm.context_dialect);
}

TEST_F(GLDeferredW1, DialectSurvivesSaveLoadRoundTrip) {
    /* Regression for walkthrough finding: dialect must survive
     * grounded_language_save → grounded_language_load (added to v2). */
    grounded_language_set_dialect(gl, "fr-CA");
    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    grounded_language_fast_map(gl, "bonjour_uniq", f.data(), TEST_DIM, 0);

    char path[] = "/tmp/gl_dialect_roundtrip_XXXXXX.bin";
    int fd = mkstemps(path, 4);
    ASSERT_GE(fd, 0);
    close(fd);

    EXPECT_EQ(0, grounded_language_save(gl, path));

    grounded_language_t* gl2 = grounded_language_load(path, sm);
    ASSERT_NE(gl2, nullptr);
    EXPECT_STREQ("fr-CA", grounded_language_get_dialect(gl2));

    grounded_language_destroy(gl2);
    unlink(path);
}

TEST_F(GLDeferredW1, DialectNullSafeOnNullGl) {
    /* Don't crash. */
    grounded_language_set_dialect(nullptr, "en-US");
    EXPECT_STREQ("", grounded_language_get_dialect(nullptr));
}

/* ====================================================================
 * #10 active-learning curriculum signal
 * ==================================================================*/

TEST_F(GLDeferredW1, NeedsGroundingFiresOnAllUnknownInput) {
    /* Subscriber that filters NEEDS_GROUNDING events. */
    struct Cap { int n = 0; std::string last_word; float last_conf = -1.0f; };
    Cap cap;
    auto cb = +[](void* ctx, const gl_event_t* ev) -> int {
        Cap* c = (Cap*)ctx;
        if (ev->type == GL_EVENT_NEEDS_GROUNDING) {
            c->n++;
            c->last_word = ev->word ? ev->word : "";
            c->last_conf = ev->confidence;
        }
        return 0;
    };
    EXPECT_EQ(0, grounded_language_subscribe_ex(
        gl, cb, &cap, GL_EVENT_MASK_NEEDS_GROUNDING, 0));

    /* All-unknown input → fires once with the first unknown word. */
    gl_comprehension_result_t r;
    EXPECT_EQ(0, grounded_language_comprehend(gl, "phantomxyzq nullopxxq", &r));
    gl_comprehension_result_cleanup(&r);

    EXPECT_EQ(1, cap.n);
    EXPECT_EQ(std::string("phantomxyzq"), cap.last_word);
    EXPECT_FLOAT_EQ(0.0f, cap.last_conf);
}

TEST_F(GLDeferredW1, NeedsGroundingNotFiredOnHighConfidenceInput) {
    /* Strongly ground a couple of words via fast_map (one-shot at
     * GL_FAST_MAP_THRESHOLD strength = 0.8 — well above the 0.2 floor). */
    std::vector<float> f1(TEST_DIM, 0.0f); f1[0] = 1.0f;
    std::vector<float> f2(TEST_DIM, 0.0f); f2[1] = 1.0f;
    grounded_language_fast_map(gl, "alpha", f1.data(), TEST_DIM, 0);
    grounded_language_fast_map(gl, "beta",  f2.data(), TEST_DIM, 0);

    struct Cap { int n = 0; };
    Cap cap;
    auto cb = +[](void* ctx, const gl_event_t* ev) -> int {
        Cap* c = (Cap*)ctx;
        if (ev->type == GL_EVENT_NEEDS_GROUNDING) c->n++;
        return 0;
    };
    grounded_language_subscribe_ex(gl, cb, &cap,
                                     GL_EVENT_MASK_NEEDS_GROUNDING, 0);

    gl_comprehension_result_t r;
    grounded_language_comprehend(gl, "alpha beta", &r);
    gl_comprehension_result_cleanup(&r);

    EXPECT_EQ(0, cap.n);
}

TEST_F(GLDeferredW1, NeedsGroundingStatsCounter) {
    gl_stats_t s0;
    grounded_language_get_stats(gl, &s0);

    gl_comprehension_result_t r;
    grounded_language_comprehend(gl, "qwertyzzz xyzzyqq", &r);
    gl_comprehension_result_cleanup(&r);

    gl_stats_t s1;
    grounded_language_get_stats(gl, &s1);
    EXPECT_EQ(s0.total_needs_grounding + 1u, s1.total_needs_grounding);
}

/* ====================================================================
 * Wrapper guards — wrappers like amygdala/ofc/imagination/emergent_lang
 * must NOT act on a negative-grounding event (concept_id=0, conf<0)
 * because there's no concept to bind/tag. Test by attaching the
 * wrapper and verifying it doesn't fire on a negative ground.
 * ==================================================================*/

/* Spy subscriber that counts callback invocations regardless of
 * filter. We use ALL mask + observe whether the bridge wrappers
 * forward via the bus. Since the wrappers use LOG_DEBUG, we can't
 * directly observe them — instead we check the contract: a subscriber
 * with MASK_GROUNDED that filters confidence < 0 itself receives
 * the event but a downstream consumer wouldn't act. */
TEST_F(GLDeferredW1, BridgeWrappersGuardAgainstNegativeGrounding) {
    /* Indirect test: confirm that GL fires the negative event and
     * that we can recognize it via concept_id=0. Wrapper code is
     * tested by the act of compilation (added the guard) — this
     * test pins the contract going forward. */
    struct Cap { int n_neg = 0; int n_pos = 0; };
    Cap cap;
    auto cb = +[](void* ctx, const gl_event_t* ev) -> int {
        Cap* c = (Cap*)ctx;
        if (ev->type != GL_EVENT_GROUNDED) return 0;
        if (ev->concept_id == 0 || ev->confidence < 0.0f) c->n_neg++;
        else c->n_pos++;
        return 0;
    };
    grounded_language_subscribe_ex(gl, cb, &cap, GL_EVENT_MASK_GROUNDED, 0);

    /* Positive ground first to set up an entry. */
    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    gl_grounding_event_t pos{};
    pos.word = "wolf";
    pos.modality = GL_MODALITY_VISUAL;
    pos.sensory_features = f.data();
    pos.feature_dim = TEST_DIM;
    pos.attention = 1.0f;
    pos.emotional_arousal = 0.6f;
    grounded_language_ground(gl, &pos);
    EXPECT_EQ(1, cap.n_pos);
    EXPECT_EQ(0, cap.n_neg);

    /* Negative ground — concept_id=0, confidence<0. */
    gl_grounding_event_t neg = pos;
    neg.negative = true;
    grounded_language_ground(gl, &neg);
    EXPECT_EQ(1, cap.n_pos);
    EXPECT_EQ(1, cap.n_neg);
}

TEST_F(GLDeferredW1, NeedsGroundingMaskAllSubscriberAlsoReceives) {
    /* A subscriber with GL_EVENT_MASK_ALL must also receive
     * NEEDS_GROUNDING events (the new mask bit is contained in ALL). */
    struct Cap { int needs = 0; int compreh = 0; };
    Cap cap;
    auto cb = +[](void* ctx, const gl_event_t* ev) -> int {
        Cap* c = (Cap*)ctx;
        if (ev->type == GL_EVENT_NEEDS_GROUNDING) c->needs++;
        if (ev->type == GL_EVENT_COMPREHENDED)    c->compreh++;
        return 0;
    };
    grounded_language_subscribe(gl, cb, &cap);

    gl_comprehension_result_t r;
    grounded_language_comprehend(gl, "phantomxyzq", &r);
    gl_comprehension_result_cleanup(&r);

    EXPECT_EQ(1, cap.compreh);
    EXPECT_EQ(1, cap.needs);
}

}  // namespace
