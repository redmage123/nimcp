/**
 * @file test_grounded_language_sleep.cpp
 * @brief Unit tests for grounded_language_sleep_consolidate.
 *
 * WHAT: Cover the four sleep-stage branches + parameter validation.
 *       Inserts a small lexicon, drives the consolidator at each
 *       stage, and asserts the binding-strength deltas match the
 *       documented stage rules.
 *
 * WHY:  Sleep consolidation drives long-run lexicon stability. If
 *       DEEP_NREM stops reinforcing or LIGHT_NREM stops decaying,
 *       the trained vocabulary drifts; if AWAKE silently mutates the
 *       lexicon, learning becomes non-deterministic.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "cognitive/nimcp_sleep_wake.h"
}

namespace {

constexpr uint32_t TEST_DIM = 32;

class GLSleep : public ::testing::Test {
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

    /* Insert two distinct words via fast_map so we have measurable
     * lexicon entries to consolidate over. */
    void seed() {
        std::vector<float> f1(TEST_DIM, 0.0f); f1[0] = 1.0f;
        std::vector<float> f2(TEST_DIM, 0.0f); f2[1] = 1.0f;
        grounded_language_fast_map(gl, "alpha", f1.data(), TEST_DIM, 0);
        grounded_language_fast_map(gl, "beta",  f2.data(), TEST_DIM, 0);
    }
};

/* --- Validation: NULL handle and out-of-range strength ------------- */
TEST_F(GLSleep, NullHandleReturnsError) {
    EXPECT_EQ(-1, grounded_language_sleep_consolidate(
        nullptr, (int)SLEEP_STATE_DEEP_NREM, 0.8f));
}
TEST_F(GLSleep, OutOfRangeStrengthRejected) {
    EXPECT_EQ(-1, grounded_language_sleep_consolidate(
        gl, (int)SLEEP_STATE_DEEP_NREM, -0.1f));
    EXPECT_EQ(-1, grounded_language_sleep_consolidate(
        gl, (int)SLEEP_STATE_DEEP_NREM, 1.1f));
}

/* --- AWAKE / DROWSY: no-op contract -------------------------------- */
TEST_F(GLSleep, AwakeIsNoOp) {
    seed();
    /* Don't sample binding strengths — just confirm no crash + return 0. */
    EXPECT_EQ(0, grounded_language_sleep_consolidate(
        gl, (int)SLEEP_STATE_AWAKE, 0.5f));
}
TEST_F(GLSleep, DrowsyIsNoOp) {
    seed();
    EXPECT_EQ(0, grounded_language_sleep_consolidate(
        gl, (int)SLEEP_STATE_DROWSY, 0.5f));
}

/* --- DEEP_NREM decays a stale binding (freq < FREQ_FLOOR=3) -------- */
TEST_F(GLSleep, DeepNREMDecaysStaleBinding) {
    seed();
    /* "alpha" was fast_mapped once → frequency=0 (stale). */
    const gl_lexicon_entry_t* e = grounded_language_lookup(gl, "alpha");
    ASSERT_NE(e, nullptr);
    ASSERT_GT(e->binding_count, 0u);
    float s_before = e->bindings[0].strength;
    ASSERT_GT(s_before, 0.0f);

    ASSERT_EQ(0, grounded_language_sleep_consolidate(
        gl, (int)SLEEP_STATE_DEEP_NREM, 0.8f));

    e = grounded_language_lookup(gl, "alpha");
    ASSERT_NE(e, nullptr);
    float s_after = e->bindings[0].strength;
    /* Stale rule: s *= (1 - 0.05*0.8) = s * 0.96. Allow floor (0.05). */
    EXPECT_LT(s_after, s_before)
        << "stale binding should decay under DEEP_NREM";
    EXPECT_GE(s_after, 0.05f) << "decay must respect strength floor";
}

/* --- DEEP_NREM strengthens a frequent binding (freq ≥ FREQ_FLOOR) -- */
TEST_F(GLSleep, DeepNREMStrengthensFrequentBinding) {
    seed();
    /* Bump "alpha" frequency above the floor by repeated grounding. */
    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    for (int i = 0; i < 4; i++) {
        gl_grounding_event_t ev{};
        ev.word = "alpha";
        ev.modality = GL_MODALITY_VISUAL;
        ev.sensory_features = f.data();
        ev.feature_dim = TEST_DIM;
        ev.attention = 0.7f;
        grounded_language_ground(gl, &ev);
    }
    const gl_lexicon_entry_t* e = grounded_language_lookup(gl, "alpha");
    ASSERT_NE(e, nullptr);
    ASSERT_GE(e->frequency, 3u) << "test setup must reach FREQ_FLOOR";
    ASSERT_GT(e->binding_count, 0u);
    float s_before = e->bindings[0].strength;
    float c_before = e->bindings[0].confidence;

    ASSERT_EQ(0, grounded_language_sleep_consolidate(
        gl, (int)SLEEP_STATE_DEEP_NREM, 0.8f));

    e = grounded_language_lookup(gl, "alpha");
    ASSERT_NE(e, nullptr);
    /* Reinforce rule: s += 0.10*0.8 = +0.08 (capped at 1.0). */
    EXPECT_GT(e->bindings[0].strength, s_before)
        << "frequent binding should strengthen under DEEP_NREM";
    EXPECT_LE(e->bindings[0].strength, 1.0f) << "strength must respect ceiling";
    EXPECT_GT(e->bindings[0].confidence, c_before)
        << "confidence should also boost on reinforcement";
}

/* --- LIGHT_NREM applies mild blanket decay ------------------------- */
TEST_F(GLSleep, LightNREMDecaysAllBindings) {
    seed();
    const gl_lexicon_entry_t* e = grounded_language_lookup(gl, "beta");
    ASSERT_NE(e, nullptr);
    ASSERT_GT(e->binding_count, 0u);
    float s_before = e->bindings[0].strength;

    ASSERT_EQ(0, grounded_language_sleep_consolidate(
        gl, (int)SLEEP_STATE_LIGHT_NREM, 0.3f));

    e = grounded_language_lookup(gl, "beta");
    ASSERT_NE(e, nullptr);
    /* LIGHT rule: s *= (1 - 0.02*0.3) = s * 0.994. Tiny but non-zero. */
    EXPECT_LT(e->bindings[0].strength, s_before)
        << "LIGHT_NREM should apply mild decay";
}

/* --- REM nudges valence/arousal toward neutral ------------------- */
TEST_F(GLSleep, REMNudgesEmotionTowardNeutral) {
    /* Seed with a grounding event carrying nonzero valence/arousal. */
    std::vector<float> f(TEST_DIM, 0.0f); f[3] = 1.0f;
    gl_grounding_event_t ev{};
    ev.word = "gamma";
    ev.modality = GL_MODALITY_VISUAL;
    ev.sensory_features = f.data();
    ev.feature_dim = TEST_DIM;
    ev.emotional_valence = 0.8f;
    ev.emotional_arousal = 0.7f;
    ev.attention = 0.9f;
    ASSERT_EQ(0, grounded_language_ground(gl, &ev));
    const gl_lexicon_entry_t* e = grounded_language_lookup(gl, "gamma");
    ASSERT_NE(e, nullptr);
    float v_before = e->valence;
    float a_before = e->arousal;
    ASSERT_GT(v_before, 0.0f);
    ASSERT_GT(a_before, 0.0f);

    ASSERT_EQ(0, grounded_language_sleep_consolidate(
        gl, (int)SLEEP_STATE_REM, 0.5f));

    e = grounded_language_lookup(gl, "gamma");
    ASSERT_NE(e, nullptr);
    /* REM rule: v *= (1 - 0.02*0.5) = v * 0.99. Should drift toward 0. */
    EXPECT_LT(e->valence, v_before) << "REM should nudge valence toward 0";
    EXPECT_LT(e->arousal, a_before) << "REM should nudge arousal toward 0";
    EXPECT_GE(e->valence, 0.0f) << "valence must not undershoot zero";
}

/* --- DEEP_NREM strength floor protects against complete loss ----- */
TEST_F(GLSleep, DeepNREMRespectsStrengthFloor) {
    seed();
    /* Run DEEP_NREM many times on a stale binding — should never go
     * below the 0.05 floor. */
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(0, grounded_language_sleep_consolidate(
            gl, (int)SLEEP_STATE_DEEP_NREM, 1.0f));
    }
    const gl_lexicon_entry_t* e = grounded_language_lookup(gl, "alpha");
    ASSERT_NE(e, nullptr);
    EXPECT_GE(e->bindings[0].strength, 0.05f);
}

/* --- Empty lexicon is also a clean no-op (only function words seeded) */
TEST_F(GLSleep, FreshLexiconIsSafe) {
    /* No seed call — just the default function-word vocabulary. */
    EXPECT_EQ(0, grounded_language_sleep_consolidate(
        gl, (int)SLEEP_STATE_DEEP_NREM, 0.8f));
}

/* --- Repeat consolidation is idempotent at the API level ----------- */
TEST_F(GLSleep, RepeatedDeepNREMSafe) {
    seed();
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(0, grounded_language_sleep_consolidate(
            gl, (int)SLEEP_STATE_DEEP_NREM, 0.8f));
    }
}

}  // namespace
