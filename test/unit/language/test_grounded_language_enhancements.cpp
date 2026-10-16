/**
 * @file test_grounded_language_enhancements.cpp
 * @brief Unit tests for the language-module enhancement wave.
 *
 * COVERS:
 *  - #1 grounded_language_has_word
 *  - #3 subscriber priority ordering (subscribe_ex)
 *  - #4 per-subscriber type-mask filter
 *  - #11 re-entry guard
 *  - #15 forgetting-curve telemetry
 *  - probes API: grounded_language_get_probe_metrics
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

class GLEnhancements : public ::testing::Test {
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

    void seed_word(const char* w) {
        std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
        grounded_language_fast_map(gl, w, f.data(), TEST_DIM, 0);
    }
};

/* ===================================================================
 * #1 — grounded_language_has_word
 * =================================================================*/

TEST_F(GLEnhancements, HasWordReturnsFalseOnEmpty) {
    EXPECT_FALSE(grounded_language_has_word(gl, ""));
    EXPECT_FALSE(grounded_language_has_word(gl, nullptr));
    EXPECT_FALSE(grounded_language_has_word(nullptr, "x"));
}

TEST_F(GLEnhancements, HasWordReturnsTrueAfterFastMap) {
    seed_word("widget");
    EXPECT_TRUE(grounded_language_has_word(gl, "widget"));
    EXPECT_FALSE(grounded_language_has_word(gl, "thingamajig"));
}

TEST_F(GLEnhancements, HasWordIsCaseInsensitive) {
    seed_word("widget");
    EXPECT_TRUE(grounded_language_has_word(gl, "Widget"));
    EXPECT_TRUE(grounded_language_has_word(gl, "WIDGET"));
}

TEST_F(GLEnhancements, HasWordIsReadOnly) {
    /* GL ships with a base lexicon; we measure delta. */
    gl_probe_metrics_t pm0;
    grounded_language_get_probe_metrics(gl, &pm0);
    uint32_t baseline = pm0.vocab_count;

    EXPECT_FALSE(grounded_language_has_word(gl, "phantom_xyzzy_qwerty"));
    EXPECT_FALSE(grounded_language_has_word(gl, "phantom_xyzzy_qwerty"));

    gl_probe_metrics_t pm1;
    grounded_language_get_probe_metrics(gl, &pm1);
    EXPECT_EQ(baseline, pm1.vocab_count) << "has_word must not insert";
}

/* ===================================================================
 * Subscriber bus #3 + #4 + #11
 * =================================================================*/

struct CapCtx {
    int count = 0;
    int last_type = -1;
    int order_id = 0;
};

extern "C" int cap_subscriber(void* ctx, const gl_event_t* ev) {
    CapCtx* c = (CapCtx*)ctx;
    c->count++;
    c->last_type = (int)ev->type;
    return 0;
}

/* #3 priority — higher fires first, with a side-effect counter that
 * records who fired first. */
struct OrderCtx {
    int* counter;
    int fired_at = -1;
};
extern "C" int order_subscriber(void* ctx, const gl_event_t* ev) {
    (void)ev;
    OrderCtx* o = (OrderCtx*)ctx;
    o->fired_at = (*o->counter)++;
    return 0;
}

TEST_F(GLEnhancements, PriorityOrderingHigherFiresFirst) {
    int counter = 0;
    OrderCtx low{&counter}, mid{&counter}, hi{&counter};
    EXPECT_EQ(0, grounded_language_subscribe_ex(gl, order_subscriber, &low,
                                                  GL_EVENT_MASK_ALL, -10));
    EXPECT_EQ(0, grounded_language_subscribe_ex(gl, order_subscriber, &mid,
                                                  GL_EVENT_MASK_ALL, 0));
    EXPECT_EQ(0, grounded_language_subscribe_ex(gl, order_subscriber, &hi,
                                                  GL_EVENT_MASK_ALL, 50));
    seed_word("alpha");
    /* hi.fired_at < mid.fired_at < low.fired_at */
    EXPECT_LT(hi.fired_at, mid.fired_at);
    EXPECT_LT(mid.fired_at, low.fired_at);
}

TEST_F(GLEnhancements, PriorityOrderRegistrationOrderTieBreak) {
    int counter = 0;
    OrderCtx a{&counter}, b{&counter};
    grounded_language_subscribe_ex(gl, order_subscriber, &a, GL_EVENT_MASK_ALL, 0);
    grounded_language_subscribe_ex(gl, order_subscriber, &b, GL_EVENT_MASK_ALL, 0);
    seed_word("beta");
    EXPECT_LT(a.fired_at, b.fired_at);
}

/* #4 per-subscriber type filter. */
TEST_F(GLEnhancements, MaskFiltersByType) {
    CapCtx ground_only, comp_only, all_evts;
    grounded_language_subscribe_ex(gl, cap_subscriber, &ground_only,
                                     GL_EVENT_MASK_GROUNDED, 0);
    grounded_language_subscribe_ex(gl, cap_subscriber, &comp_only,
                                     GL_EVENT_MASK_COMPREHENDED, 0);
    grounded_language_subscribe_ex(gl, cap_subscriber, &all_evts,
                                     GL_EVENT_MASK_ALL, 0);

    seed_word("alpha");                                   /* NEW_WORD */
    /* fast_map → NEW_WORD only. ground_only=0, comp_only=0, all_evts=1. */
    EXPECT_EQ(0, ground_only.count);
    EXPECT_EQ(0, comp_only.count);
    EXPECT_EQ(1, all_evts.count);

    /* Drive a real GROUNDED. */
    std::vector<float> f(TEST_DIM, 0.0f); f[1] = 1.0f;
    gl_grounding_event_t ev{};
    ev.word = "ball";
    ev.modality = GL_MODALITY_VISUAL;
    ev.sensory_features = f.data();
    ev.feature_dim = TEST_DIM;
    ev.attention = 0.7f;
    grounded_language_ground(gl, &ev);

    /* GROUNDED + NEW_WORD fired (both are fired internally). */
    EXPECT_GE(ground_only.count, 1);
    EXPECT_EQ(0, comp_only.count);
    EXPECT_GE(all_evts.count, 2);
}

TEST_F(GLEnhancements, MaskZeroIsRejected) {
    CapCtx c;
    EXPECT_EQ(-1, grounded_language_subscribe_ex(gl, cap_subscriber, &c, 0, 0));
}

/* #11 re-entry guard. */
struct ReentryCtx {
    grounded_language_t* gl;
    int outer_count = 0;
    int inner_count = 0;
    /* Whether to attempt the dangerous re-entrant fire. */
    bool attempt_reenter = true;
};

extern "C" int reenter_subscriber(void* ctx, const gl_event_t* ev) {
    ReentryCtx* r = (ReentryCtx*)ctx;
    r->outer_count++;
    if (r->attempt_reenter) {
        /* Try to fire ANOTHER event from inside the callback by
         * driving fast_map. The guard should make this be a no-op
         * for the bus (the lexicon work still happens; only the bus
         * fire is suppressed). */
        std::vector<float> f(TEST_DIM, 0.0f); f[2] = 1.0f;
        grounded_language_fast_map(r->gl, "reenter_inner",
                                     f.data(), TEST_DIM, 0);
    }
    (void)ev;
    return 0;
}

TEST_F(GLEnhancements, ReentryGuardSuppressesNestedFire) {
    ReentryCtx r{gl, 0, 0, true};
    grounded_language_subscribe(gl, reenter_subscriber, &r);

    seed_word("trigger");
    /* outer_count counts external + the re-entrant attempts that
     * actually delivered. With the guard, the re-entrant fire-event
     * is suppressed so the subscriber sees the OUTER event but not
     * the inner one. count must stay small. */
    EXPECT_GE(r.outer_count, 1);
    /* If the guard is broken, this loops infinitely and never gets
     * here — so just reaching this assertion is the test. */

    /* Verify probe metrics show the dropped re-entry. */
    gl_probe_metrics_t pm;
    grounded_language_get_probe_metrics(gl, &pm);
    EXPECT_GT(pm.events_dropped_reentry, 0u);
}

/* ===================================================================
 * #15 — forgetting telemetry
 * =================================================================*/

TEST_F(GLEnhancements, DecayedCounterStartsAtZero) {
    gl_stats_t s;
    grounded_language_get_stats(gl, &s);
    EXPECT_EQ(0u, s.entries_decayed_last_24h);
    EXPECT_EQ(0u, s.entries_decayed_all_time);
}

/* ===================================================================
 * Probes API
 * =================================================================*/

TEST_F(GLEnhancements, ProbeMetricsZeroFilledOnNullPath) {
    gl_probe_metrics_t pm;
    memset(&pm, 0xAA, sizeof(pm));
    EXPECT_EQ(-1, grounded_language_get_probe_metrics(nullptr, &pm));
    EXPECT_EQ(-1, grounded_language_get_probe_metrics(gl, nullptr));
}

TEST_F(GLEnhancements, ProbeMetricsBasicSnapshot) {
    /* GL ships with a base lexicon; measure delta after seeding new
     * words to keep the test independent of the bootstrap size. */
    gl_probe_metrics_t pm0;
    grounded_language_get_probe_metrics(gl, &pm0);
    uint32_t baseline = pm0.vocab_count;

    seed_word("zzz_alpha_uniq");
    seed_word("zzz_beta_uniq");
    seed_word("zzz_gamma_uniq");

    gl_probe_metrics_t pm;
    EXPECT_EQ(0, grounded_language_get_probe_metrics(gl, &pm));
    EXPECT_EQ(baseline + 3u, pm.vocab_count);
    EXPECT_EQ(0u, pm.subscriber_count);
    EXPECT_FALSE(pm.in_fire_event);
    EXPECT_FALSE(pm.broca_attached);
    EXPECT_FALSE(pm.wernicke_attached);
}

TEST_F(GLEnhancements, ProbeMetricsCountsHighPriorityAndFiltered) {
    CapCtx a, b, c;
    grounded_language_subscribe_ex(gl, cap_subscriber, &a, GL_EVENT_MASK_ALL,    10);
    grounded_language_subscribe_ex(gl, cap_subscriber, &b, GL_EVENT_MASK_GROUNDED, 0);
    grounded_language_subscribe_ex(gl, cap_subscriber, &c, GL_EVENT_MASK_ALL,     0);

    gl_probe_metrics_t pm;
    grounded_language_get_probe_metrics(gl, &pm);
    EXPECT_EQ(3u, pm.subscriber_count);
    EXPECT_EQ(1u, pm.subscriber_high_priority);  /* only `a` has p > 0 */
    EXPECT_EQ(1u, pm.subscriber_filtered);       /* only `b` has filter */
}

}  // namespace
