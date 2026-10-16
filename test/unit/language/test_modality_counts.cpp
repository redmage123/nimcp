/**
 * @file test_modality_counts.cpp
 * @brief Unit tests for grounded_language_get_modality_counts() — the
 *        per-modality binding-coverage probe used by curriculum tests.
 *
 * WHAT: Direct exercise of the GL aggregator. The public C wrapper
 *       (nimcp_brain_get_modality_counts) layers brain-handle validation
 *       on top of this — those paths are covered through the Python RPC
 *       integration test rather than constructing a 2M-neuron brain here.
 *
 * WHY:  The modality-coverage telemetry feeds curriculum stage gates
 *       ("did we actually ground anything visually before progressing?").
 *       If the aggregator silently misses bindings, the curriculum
 *       advances on a zero signal and the failure surfaces several
 *       layers up. Catch it here.
 *
 * HOW:  GTest fixture mirrors test_top_phrases_public_api.cpp.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "nimcp.h"
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
}

namespace {

constexpr uint32_t TEST_DIM = 32;

class ModalityCountsUnit : public ::testing::Test {
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
        grounded_language_destroy(gl);
        semantic_memory_destroy(sm);
    }

    /* Helper: build and submit a grounding event. Mirrors the seed pattern
     * in test_grounded_language.cpp — non-zero attention so the binding is
     * actually committed by the Hebbian rule, no context sentence so we
     * don't accidentally co-create unrelated lexicon entries. */
    int ground(const char* word, gl_modality_t modality,
               const std::vector<float>& features) {
        gl_grounding_event_t event = {};
        event.word = word;
        event.modality = modality;
        event.sensory_features = features.data();
        event.feature_dim = static_cast<uint32_t>(features.size());
        event.attention = 0.9f;
        event.emotional_valence = 0.0f;
        event.emotional_arousal = 0.0f;
        return grounded_language_ground(gl, &event);
    }

    static std::vector<float> seeded_vector(uint32_t dim, uint32_t seed) {
        std::vector<float> v(dim, 0.0f);
        for (uint32_t i = 0; i < dim; i++) {
            /* Stable values in [0,1) so each test gets a deterministic
             * binding without triggering the ground-rejects-zero filter. */
            v[i] = 0.1f + 0.7f * static_cast<float>((seed + i * 7) % 13) / 13.0f;
        }
        return v;
    }
};

/* --- NULL handling. The aggregator must zero out_counts on bad input
 *     and otherwise be a no-op. The public C wrapper layers handle/GL
 *     validation on top — those paths return -1; this internal one just
 *     leaves the array zeroed. ----------------------------------------- */

TEST_F(ModalityCountsUnit, NullGlYieldsZeros) {
    uint32_t counts[GL_MODALITY_COUNT];
    /* Pre-fill with sentinel so we can prove the function zeroed it. */
    for (uint32_t i = 0; i < GL_MODALITY_COUNT; i++) counts[i] = 0xDEADBEEFu;
    grounded_language_get_modality_counts(nullptr, counts);
    for (uint32_t i = 0; i < GL_MODALITY_COUNT; i++) {
        EXPECT_EQ(counts[i], 0u) << "Index " << i << " not zeroed by NULL gl";
    }
}

TEST_F(ModalityCountsUnit, NullOutCountsIsNoOp) {
    /* Just must not crash. The contract is the function rejects NULL out. */
    grounded_language_get_modality_counts(gl, nullptr);
    SUCCEED();
}

/* --- Public C wrapper: a NULL handle must return -1 cleanly. The
 *     full handle-validation path requires a live brain, so we just
 *     hit the obvious NULL gate here. -------------------------------- */
TEST_F(ModalityCountsUnit, PublicWrapperRejectsNullHandle) {
    uint32_t counts[6] = {1, 2, 3, 4, 5, 6};
    int rc = nimcp_brain_get_modality_counts(nullptr, counts);
    EXPECT_EQ(rc, -1);
    /* Wrapper must zero on failure (same contract as get_top_phrases). */
    for (uint32_t i = 0; i < 6; i++) EXPECT_EQ(counts[i], 0u);
}

TEST_F(ModalityCountsUnit, PublicWrapperRejectsNullCounts) {
    int rc = nimcp_brain_get_modality_counts(nullptr, nullptr);
    EXPECT_EQ(rc, -1);
}

/* --- A fresh GL has zero learned bindings and therefore zero per-
 *     modality counts. Function-word seeding at create() time doesn't
 *     install bindings (those carry a concept_id), only lexicon entries —
 *     binding_count stays 0, so all per-modality counts stay 0. -------- */
TEST_F(ModalityCountsUnit, FreshSystemAllZero) {
    uint32_t counts[GL_MODALITY_COUNT] = {0};
    grounded_language_get_modality_counts(gl, counts);
    for (uint32_t m = 0; m < GL_MODALITY_COUNT; m++) {
        EXPECT_EQ(counts[m], 0u)
            << "Fresh GL has non-zero count at modality " << m;
    }
}

/* --- After grounding three different words across three modalities,
 *     the modality count for each engaged channel must be at least 1
 *     and the un-touched channels must remain 0. ---------------------- */
TEST_F(ModalityCountsUnit, GroundingPopulatesCorrectChannels) {
    auto v_visual   = seeded_vector(TEST_DIM, 11);
    auto v_auditory = seeded_vector(TEST_DIM, 23);
    auto v_motor    = seeded_vector(TEST_DIM, 37);

    EXPECT_EQ(ground("sunrise", GL_MODALITY_VISUAL,   v_visual),   0);
    EXPECT_EQ(ground("thunder", GL_MODALITY_AUDITORY, v_auditory), 0);
    EXPECT_EQ(ground("grasp",   GL_MODALITY_MOTOR,    v_motor),    0);

    uint32_t counts[GL_MODALITY_COUNT] = {0};
    grounded_language_get_modality_counts(gl, counts);

    /* Each ground() call creates a binding with the engaged modality's
     * strength set to (attention * 0.8 + 0.2) > 0; the other modalities
     * stay at zero on the fresh binding. So we expect exactly the three
     * touched channels to be positive. */
    EXPECT_GE(counts[GL_MODALITY_VISUAL],   1u) << "Visual channel empty after grounding 'sunrise'";
    EXPECT_GE(counts[GL_MODALITY_AUDITORY], 1u) << "Auditory channel empty after grounding 'thunder'";
    EXPECT_GE(counts[GL_MODALITY_MOTOR],    1u) << "Motor channel empty after grounding 'grasp'";
    EXPECT_EQ(counts[GL_MODALITY_EMOTIONAL],  0u);
    EXPECT_EQ(counts[GL_MODALITY_SPATIAL],    0u);
    EXPECT_EQ(counts[GL_MODALITY_LINGUISTIC], 0u);
}

/* --- Counts must increase monotonically as we ground more words.
 *     Each new word with its own concept_id adds a binding; that
 *     binding contributes one increment to its modality's count. ----- */
TEST_F(ModalityCountsUnit, CountsAreMonotonicAcrossGroundings) {
    auto v = seeded_vector(TEST_DIM, 5);

    uint32_t before[GL_MODALITY_COUNT] = {0};
    grounded_language_get_modality_counts(gl, before);

    EXPECT_EQ(ground("dawn",  GL_MODALITY_VISUAL, v), 0);
    EXPECT_EQ(ground("dusk",  GL_MODALITY_VISUAL, v), 0);
    EXPECT_EQ(ground("noon",  GL_MODALITY_VISUAL, v), 0);

    uint32_t after[GL_MODALITY_COUNT] = {0};
    grounded_language_get_modality_counts(gl, after);

    EXPECT_GT(after[GL_MODALITY_VISUAL], before[GL_MODALITY_VISUAL])
        << "Visual count did not grow after three new visual groundings";
    /* Untouched channels must not go up. */
    for (uint32_t m = 1; m < GL_MODALITY_COUNT; m++) {
        EXPECT_EQ(after[m], before[m])
            << "Channel " << m << " moved despite no grounding";
    }
}

}  // namespace
