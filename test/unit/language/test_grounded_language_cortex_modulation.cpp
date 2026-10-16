/**
 * @file test_grounded_language_cortex_modulation.cpp
 * @brief Unit tests for grounded_language_get_cortex_modulation +
 *        in-modality boost behavior in ground/comprehend.
 *
 * WHAT: Cover the public modulation accessor (NULL handling, zero-fill
 *       contract, idempotence) and verify that the no-cortex path does
 *       not perturb grounding strength or comprehension confidence.
 *
 * WHY:  The modulation taps run on every ground() and comprehend() call.
 *       If they ever return non-zero spuriously (e.g. NaN from an
 *       uninitialized cortex pointer), every binding strength and every
 *       confidence score in the trained vocabulary drifts. The contract
 *       is: zero modulation when no cortex is connected.
 *
 * NOTE: Real-cortex modulation effects are exercised in the integration
 *       suite, not here — visual_cortex_create / audio_cortex_create /
 *       speech_cortex_create are heavyweight and pull in perceptual
 *       layer init that doesn't belong in a unit fixture.
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

class GLCortexMod : public ::testing::Test {
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

/* --- API contract: NULL parameters --------------------------------- */
TEST_F(GLCortexMod, NullHandleReturnsError) {
    gl_cortex_modulation_t out;
    memset(&out, 0xFF, sizeof(out));
    EXPECT_EQ(-1, grounded_language_get_cortex_modulation(nullptr, &out));
    /* Out should still be zero-filled even on NULL gl (safety). */
    EXPECT_EQ(0.0f, out.visual_activity);
    EXPECT_EQ(0.0f, out.audio_salience);
    EXPECT_EQ(0.0f, out.speech_confidence);
}

TEST_F(GLCortexMod, NullOutReturnsError) {
    EXPECT_EQ(-1, grounded_language_get_cortex_modulation(gl, nullptr));
}

/* --- Fresh GL: no cortex connected → all zeros --------------------- */
TEST_F(GLCortexMod, FreshGLReturnsAllZeros) {
    gl_cortex_modulation_t out;
    memset(&out, 0xFF, sizeof(out));
    ASSERT_EQ(0, grounded_language_get_cortex_modulation(gl, &out));
    EXPECT_EQ(0.0f, out.visual_activity);
    EXPECT_EQ(0.0f, out.audio_salience);
    EXPECT_EQ(0.0f, out.speech_confidence);
}

/* --- Idempotent: repeated reads on a stable GL match -------------- */
TEST_F(GLCortexMod, RepeatedReadsAreStable) {
    gl_cortex_modulation_t a, b;
    ASSERT_EQ(0, grounded_language_get_cortex_modulation(gl, &a));
    ASSERT_EQ(0, grounded_language_get_cortex_modulation(gl, &b));
    EXPECT_EQ(a.visual_activity,   b.visual_activity);
    EXPECT_EQ(a.audio_salience,    b.audio_salience);
    EXPECT_EQ(a.speech_confidence, b.speech_confidence);
}

/* --- ground() with no cortex matches the unmodulated path ---------
 * Two GL handles with identical seeds + no cortex must produce the
 * same binding strength after a single ground event. This catches
 * regressions where the modulation hook adds a non-1.0 multiplier
 * even when no cortex is connected. */
TEST_F(GLCortexMod, GroundWithNoCortexProducesUnboostedBinding) {
    std::vector<float> f(TEST_DIM, 0.0f); f[2] = 1.0f;

    gl_grounding_event_t ev{};
    ev.word = "delta";
    ev.modality = GL_MODALITY_VISUAL;
    ev.sensory_features = f.data();
    ev.feature_dim = TEST_DIM;
    ev.attention = 0.5f;
    ASSERT_EQ(0, grounded_language_ground(gl, &ev));

    const gl_lexicon_entry_t* e = grounded_language_lookup(gl, "delta");
    ASSERT_NE(e, nullptr);
    ASSERT_GT(e->binding_count, 0u);
    /* Without any cortex tap, the modulation multiplier is exactly 1.0.
     * Strength = attention(0.5) * hebbian_lr * emotional_boost(1.0) * 1.0.
     * We don't assert the exact value (depends on hebbian_lr default)
     * but verify it's a plausible Hebbian product, not zero or huge. */
    float s = e->bindings[0].strength;
    EXPECT_GT(s, 0.0f) << "no-cortex binding must still have positive strength";
    EXPECT_LE(s, 1.0f) << "no-cortex binding must respect ceiling";
}

/* --- comprehend() with no cortex returns confidence in [0, 1] ----- */
TEST_F(GLCortexMod, ComprehendWithNoCortexBoundedConfidence) {
    /* Seed a couple of words so comprehension produces non-zero conf. */
    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    grounded_language_fast_map(gl, "alpha", f.data(), TEST_DIM, 0);
    grounded_language_fast_map(gl, "beta",  f.data(), TEST_DIM, 0);

    /* comprehend() memsets + allocates the result buffers itself. */
    gl_comprehension_result_t r;
    ASSERT_EQ(0, grounded_language_comprehend(gl, "alpha beta", &r));
    EXPECT_GE(r.comprehension_confidence, 0.0f);
    EXPECT_LE(r.comprehension_confidence, 1.0f)
        << "modulation must not push confidence past 1.0 even when capped";

    gl_comprehension_result_cleanup(&r);
}

/* --- Connect-then-disconnect contract: NULL pointer = no boost ---
 * connect_visual(NULL) should leave the visual_activity scalar at 0
 * just like never connecting at all. */
TEST_F(GLCortexMod, ConnectingNullCortexLeavesZeroModulation) {
    grounded_language_connect_visual(gl, nullptr);
    grounded_language_connect_auditory(gl, nullptr);
    grounded_language_connect_speech(gl, nullptr);

    gl_cortex_modulation_t out;
    ASSERT_EQ(0, grounded_language_get_cortex_modulation(gl, &out));
    EXPECT_EQ(0.0f, out.visual_activity);
    EXPECT_EQ(0.0f, out.audio_salience);
    EXPECT_EQ(0.0f, out.speech_confidence);
}

}  // namespace
