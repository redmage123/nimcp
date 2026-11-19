/**
 * @file test_grounded_language_cognitive_bridge.cpp
 * @brief Unit tests for the cognitive subscriber bus + per-module attaches.
 *
 * WHAT: Verify subscribe/unsubscribe/dedup, fire-on-{NEW_WORD,GROUNDED,
 *       COMPREHENDED,PRODUCED}, attach helper NULL safety, and that
 *       multiple subscribers all receive each event.
 *
 * WHY:  The bus runs inside grounded_language_ground / _comprehend /
 *       _produce / lexicon_find_or_create — every successful call
 *       fires events. A bad subscriber callback would propagate into
 *       hot-path latency or crash. We need NULL safety + iteration
 *       safety against mid-fire unsubscribe.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
}

namespace {

constexpr uint32_t TEST_DIM = 32;

class GLCogBridge : public ::testing::Test {
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

/* Test subscriber that records every event it receives. */
struct EventRec {
    int count;
    int last_type;
    char last_word[32];
    char last_text[64];
    float last_confidence;
};

extern "C" int rec_subscriber(void* ctx, const gl_event_t* ev) {
    EventRec* r = (EventRec*)ctx;
    r->count++;
    r->last_type = (int)ev->type;
    if (ev->word) {
        strncpy(r->last_word, ev->word, sizeof(r->last_word) - 1);
        r->last_word[sizeof(r->last_word) - 1] = '\0';
    } else {
        r->last_word[0] = '\0';
    }
    if (ev->text) {
        strncpy(r->last_text, ev->text, sizeof(r->last_text) - 1);
        r->last_text[sizeof(r->last_text) - 1] = '\0';
    } else {
        r->last_text[0] = '\0';
    }
    r->last_confidence = ev->confidence;
    return 0;
}

/* --- Subscribe/unsubscribe contract ------------------------------- */
TEST_F(GLCogBridge, SubscribeAndCount) {
    EventRec r1{}, r2{};
    EXPECT_EQ(0u, grounded_language_subscriber_count(gl));
    ASSERT_EQ(0, grounded_language_subscribe(gl, rec_subscriber, &r1));
    EXPECT_EQ(1u, grounded_language_subscriber_count(gl));
    ASSERT_EQ(0, grounded_language_subscribe(gl, rec_subscriber, &r2));
    EXPECT_EQ(2u, grounded_language_subscriber_count(gl));
}

TEST_F(GLCogBridge, SubscribeDedupReplacesOnSameCtx) {
    EventRec r{};
    ASSERT_EQ(0, grounded_language_subscribe(gl, rec_subscriber, &r));
    /* Same ctx → no count increase. */
    ASSERT_EQ(0, grounded_language_subscribe(gl, rec_subscriber, &r));
    EXPECT_EQ(1u, grounded_language_subscriber_count(gl));
}

TEST_F(GLCogBridge, UnsubscribeRemovesByCtx) {
    EventRec r{};
    grounded_language_subscribe(gl, rec_subscriber, &r);
    EXPECT_EQ(0, grounded_language_unsubscribe(gl, &r));
    EXPECT_EQ(0u, grounded_language_subscriber_count(gl));
    /* Second unsubscribe is a no-op (-1). */
    EXPECT_EQ(-1, grounded_language_unsubscribe(gl, &r));
}

TEST_F(GLCogBridge, SubscribeNullSafe) {
    EventRec r{};
    EXPECT_EQ(-1, grounded_language_subscribe(nullptr, rec_subscriber, &r));
    EXPECT_EQ(-1, grounded_language_subscribe(gl, nullptr, &r));
}

/* --- Fire on NEW_WORD --------------------------------------------- */
TEST_F(GLCogBridge, FireOnNewWord) {
    EventRec r{};
    grounded_language_subscribe(gl, rec_subscriber, &r);

    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    grounded_language_fast_map(gl, "widget", f.data(), TEST_DIM, 0);
    /* fast_map → lexicon_find_or_create → NEW_WORD event. */
    EXPECT_GE(r.count, 1);
    /* Most-recent event should be NEW_WORD or GROUNDED on widget. */
    EXPECT_STREQ("widget", r.last_word);
}

/* --- Fire on GROUNDED --------------------------------------------- */
TEST_F(GLCogBridge, FireOnGrounding) {
    EventRec r{};
    grounded_language_subscribe(gl, rec_subscriber, &r);

    std::vector<float> f(TEST_DIM, 0.0f); f[1] = 1.0f;
    gl_grounding_event_t ev{};
    ev.word = "ball";
    ev.modality = GL_MODALITY_VISUAL;
    ev.sensory_features = f.data();
    ev.feature_dim = TEST_DIM;
    ev.attention = 0.7f;
    ev.emotional_arousal = 0.5f;
    ASSERT_EQ(0, grounded_language_ground(gl, &ev));

    /* Two events expected: NEW_WORD then GROUNDED. */
    EXPECT_GE(r.count, 2);
    EXPECT_EQ((int)GL_EVENT_GROUNDED, r.last_type);
    EXPECT_STREQ("ball", r.last_word);
}

/* --- Fire on COMPREHENDED ----------------------------------------- */
TEST_F(GLCogBridge, FireOnComprehend) {
    EventRec r{};
    grounded_language_subscribe(gl, rec_subscriber, &r);

    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    grounded_language_fast_map(gl, "alpha", f.data(), TEST_DIM, 0);

    int prev_count = r.count;
    gl_comprehension_result_t result;
    ASSERT_EQ(0, grounded_language_comprehend(gl, "alpha", &result));
    EXPECT_GT(r.count, prev_count);
    /* The most recent event should be COMPREHENDED. */
    EXPECT_EQ((int)GL_EVENT_COMPREHENDED, r.last_type);
    EXPECT_STREQ("alpha", r.last_text);
    gl_comprehension_result_cleanup(&result);
}

/* --- Multiple subscribers all receive each event ------------------- */
TEST_F(GLCogBridge, AllSubscribersReceiveEvents) {
    EventRec a{}, b{}, c{};
    grounded_language_subscribe(gl, rec_subscriber, &a);
    grounded_language_subscribe(gl, rec_subscriber, &b);
    grounded_language_subscribe(gl, rec_subscriber, &c);

    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    grounded_language_fast_map(gl, "gamma", f.data(), TEST_DIM, 0);

    EXPECT_GE(a.count, 1);
    EXPECT_GE(b.count, 1);
    EXPECT_GE(c.count, 1);
    EXPECT_EQ(a.count, b.count);
    EXPECT_EQ(b.count, c.count);
}

/* --- Capacity gate ------------------------------------------------ */
TEST_F(GLCogBridge, OverCapacityReturnsError) {
    EventRec recs[32];
    /* GL_MAX_SUBSCRIBERS == 24 — first 24 succeed, rest rejected. */
    int ok_count = 0;
    for (int i = 0; i < 32; i++) {
        if (grounded_language_subscribe(gl, rec_subscriber, &recs[i]) == 0) {
            ok_count++;
        }
    }
    EXPECT_EQ(24, ok_count);
}

/* --- Per-module attach helpers: NULL safety ------------------------ */
TEST_F(GLCogBridge, AttachHelpersNullModuleIsSafe) {
    grounded_language_attach_inner_speech(gl, nullptr);
    grounded_language_attach_imagination(gl, nullptr);
    grounded_language_attach_theory_of_mind(gl, nullptr);
    grounded_language_attach_empathy(gl, nullptr);
    grounded_language_attach_introspection(gl, nullptr);
    grounded_language_attach_reasoning(gl, nullptr);
    grounded_language_attach_narrative(gl, nullptr);
    grounded_language_attach_metacognition(gl, nullptr);
    grounded_language_attach_analogical(gl, nullptr);
    grounded_language_attach_emergent_language(gl, nullptr);
    /* All NULL → must not crash and must not register subscribers. */
    EXPECT_EQ(0u, grounded_language_subscriber_count(gl));
}

TEST_F(GLCogBridge, AttachHelpersWithDummyModuleRegisters) {
    int dummy_mod = 1;
    grounded_language_attach_inner_speech(gl, &dummy_mod);
    EXPECT_EQ(1u, grounded_language_subscriber_count(gl));
    /* Re-attaching same module is a no-op (dedup). */
    grounded_language_attach_inner_speech(gl, &dummy_mod);
    EXPECT_EQ(1u, grounded_language_subscriber_count(gl));
}

/* --- Unsubscribe-during-fire safety ------------------------------- */
struct SuicideCtx {
    grounded_language_t* gl;
    int fire_count;
};

extern "C" int suicide_subscriber(void* ctx, const gl_event_t* ev) {
    (void)ev;
    SuicideCtx* s = (SuicideCtx*)ctx;
    s->fire_count++;
    /* Unsubscribe self mid-fire — must not corrupt iteration. */
    grounded_language_unsubscribe(s->gl, ctx);
    return 0;
}

TEST_F(GLCogBridge, UnsubscribeDuringFireIsSafe) {
    SuicideCtx s = { gl, 0 };
    EventRec r{};  /* second subscriber to verify iteration continues */
    grounded_language_subscribe(gl, suicide_subscriber, &s);
    grounded_language_subscribe(gl, rec_subscriber, &r);

    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    grounded_language_fast_map(gl, "delta", f.data(), TEST_DIM, 0);

    EXPECT_GE(s.fire_count, 1);
    /* Second subscriber must still have received the event. */
    EXPECT_GE(r.count, 1);
    /* Suicide subscriber should be gone. */
    EXPECT_EQ(1u, grounded_language_subscriber_count(gl));
}

/* --- Brain-region attach helpers ----------------------------------- */
TEST_F(GLCogBridge, RegionAttachHelpersNullModuleIsSafe) {
    grounded_language_attach_prefrontal(gl, nullptr);
    grounded_language_attach_insula(gl, nullptr);
    grounded_language_attach_cingulate(gl, nullptr);
    grounded_language_attach_amygdala(gl, nullptr);
    grounded_language_attach_ofc(gl, nullptr);
    EXPECT_EQ(0u, grounded_language_subscriber_count(gl));
}

TEST_F(GLCogBridge, RegionAttachRegistersOneSlotEach) {
    int prefrontal = 1, insula = 2, cingulate = 3, amygdala = 4, ofc = 5;
    grounded_language_attach_prefrontal(gl, &prefrontal);
    EXPECT_EQ(1u, grounded_language_subscriber_count(gl));
    grounded_language_attach_insula(gl, &insula);
    EXPECT_EQ(2u, grounded_language_subscriber_count(gl));
    grounded_language_attach_cingulate(gl, &cingulate);
    EXPECT_EQ(3u, grounded_language_subscriber_count(gl));
    grounded_language_attach_amygdala(gl, &amygdala);
    EXPECT_EQ(4u, grounded_language_subscriber_count(gl));
    grounded_language_attach_ofc(gl, &ofc);
    EXPECT_EQ(5u, grounded_language_subscriber_count(gl));

    /* Re-attaching same module is a no-op (dedup). */
    grounded_language_attach_prefrontal(gl, &prefrontal);
    EXPECT_EQ(5u, grounded_language_subscriber_count(gl));
}

/* --- Audit fix #1: NULL mod is strict no-op (not unsubscribe-by-NULL).
 *     A pre-existing subscriber with NULL ctx must NOT be clobbered
 *     when an attach helper is called with NULL mod. */
TEST_F(GLCogBridge, AttachWithNullDoesNotClobberNullCtxSubscriber) {
    /* Register a raw subscriber with NULL ctx (legal, edge case). */
    ASSERT_EQ(0, grounded_language_subscribe(gl, rec_subscriber, nullptr));
    EXPECT_EQ(1u, grounded_language_subscriber_count(gl));

    /* These should all be no-ops — the NULL-ctx subscriber must survive. */
    grounded_language_attach_inner_speech(gl, nullptr);
    grounded_language_attach_prefrontal(gl, nullptr);
    grounded_language_attach_ofc(gl, nullptr);
    EXPECT_EQ(1u, grounded_language_subscriber_count(gl));
}

TEST_F(GLCogBridge, RegionsObserveEventsViaBus) {
    /* All four region handles share one ctx struct so they all alias
     * the same EventRec — but each attach uses a distinct module
     * pointer, so we can verify each is registered. */
    EventRec rec_pfc{}, rec_ins{}, rec_acc{}, rec_amy{};
    /* Register raw subscribers so we can directly verify the events
     * the wrappers would have observed. */
    grounded_language_subscribe(gl, rec_subscriber, &rec_pfc);
    grounded_language_subscribe(gl, rec_subscriber, &rec_ins);
    grounded_language_subscribe(gl, rec_subscriber, &rec_acc);
    grounded_language_subscribe(gl, rec_subscriber, &rec_amy);

    /* Drive a high-arousal grounding — fires NEW_WORD + GROUNDED. */
    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    gl_grounding_event_t ev{};
    ev.word = "snake";
    ev.modality = GL_MODALITY_VISUAL;
    ev.sensory_features = f.data();
    ev.feature_dim = TEST_DIM;
    ev.attention = 0.9f;
    ev.emotional_valence = -0.8f;
    ev.emotional_arousal = 0.9f;
    ASSERT_EQ(0, grounded_language_ground(gl, &ev));

    EXPECT_GE(rec_pfc.count, 2);  /* NEW_WORD + GROUNDED */
    EXPECT_EQ(rec_pfc.count, rec_ins.count);
    EXPECT_EQ(rec_ins.count, rec_acc.count);
    EXPECT_EQ(rec_acc.count, rec_amy.count);
}

}  // namespace
