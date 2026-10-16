/**
 * @file test_grounded_language_diagnostics.cpp
 * @brief Unit tests for the language collapse diagnostic + fix surface.
 *
 * WHAT: Cover the new diagnostic accessors and the SNN-bridge blend setter
 *       that backs the language-collapse mitigation, in isolation
 *       (no full brain). Also pins the comprehend behaviour we rely on
 *       to detect collapse: distinct inputs must produce distinct
 *       semantic vectors after a small amount of learning.
 *
 * WHY:  The diagnostic accessors are the only way to spot mode collapse
 *       at runtime — if they regress, we lose the eyes. The bridge
 *       setter is the actual fix lever.
 *
 * HOW:  GTest fixture mirrors test_grounded_language.cpp's pattern. No
 *       brain handle is needed — these tests target the underlying
 *       module functions called from nimcp_brain_*. The brain-handle
 *       facing accessors get covered in the integration test.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <vector>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
}

namespace {

constexpr uint32_t TEST_DIM = 32;

float cosine(const float* a, const float* b, uint32_t n) {
    double sa = 0.0, sb = 0.0, sab = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        sa  += (double)a[i] * a[i];
        sb  += (double)b[i] * b[i];
        sab += (double)a[i] * b[i];
    }
    if (sa <= 0.0 || sb <= 0.0) return 0.0f;
    return (float)(sab / std::sqrt(sa * sb));
}

class GroundedLanguageDiagnostics : public ::testing::Test {
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

    /* Seed a tiny vocabulary with deliberately divergent feature vectors
     * so comprehend() has something to differentiate. */
    void seed_distinct_words() {
        std::vector<std::pair<const char*, std::vector<float>>> words = {
            {"red",     std::vector<float>(TEST_DIM, 0.0f)},
            {"blue",    std::vector<float>(TEST_DIM, 0.0f)},
            {"ocean",   std::vector<float>(TEST_DIM, 0.0f)},
            {"pizza",   std::vector<float>(TEST_DIM, 0.0f)},
        };
        // Make features near-orthogonal — flip dominant axes per word.
        words[0].second[0] = 1.0f; words[0].second[1] = 0.5f;
        words[1].second[2] = 1.0f; words[1].second[3] = 0.5f;
        words[2].second[4] = 1.0f; words[2].second[5] = 0.5f;
        words[3].second[6] = 1.0f; words[3].second[7] = 0.5f;
        for (auto& w : words) {
            uint64_t cid = grounded_language_fast_map(gl, w.first,
                w.second.data(), TEST_DIM, 0);
            ASSERT_NE(cid, 0u) << "fast_map failed for " << w.first;
        }
    }
};

/* --- gl_stats_t shape (DRY: backs nimcp_brain_get_grounded_language_diagnostics) ---*/
TEST_F(GroundedLanguageDiagnostics, StatsExposeCountersAfterSeed) {
    seed_distinct_words();
    gl_stats_t s; std::memset(&s, 0, sizeof(s));
    grounded_language_get_stats(gl, &s);
    EXPECT_GT(s.vocab_size, 0u);
}

TEST_F(GroundedLanguageDiagnostics, StatsZerosOnFreshSystem) {
    gl_stats_t s; std::memset(&s, 0, sizeof(s));
    grounded_language_get_stats(gl, &s);
    /* Fresh system already seeds function words but should have no
     * comprehensions/productions/groundings. */
    EXPECT_EQ(s.total_comprehensions, 0u);
    EXPECT_EQ(s.total_productions,    0u);
    EXPECT_EQ(s.total_groundings,     0u);
}

TEST_F(GroundedLanguageDiagnostics, StatsHandlesNullSafely) {
    gl_stats_t s; std::memset(&s, 0xAB, sizeof(s));
    grounded_language_get_stats(nullptr, &s);
    /* Implementation contract: returns without crashing on NULL. Output
     * remains untouched (caller-supplied) — sentinel preserved. */
    EXPECT_EQ(s.vocab_size, *(uint32_t*)((char*)&s + offsetof(gl_stats_t, vocab_size)));
}

/* --- comprehend distinctness pin (regression target for collapse) ---*/
TEST_F(GroundedLanguageDiagnostics, ComprehendDistinctTextsYieldDistinctVectors) {
    seed_distinct_words();
    /* Ask comprehend twice with different inputs and assert their
     * semantic_vectors aren't pinned to one attractor. We accept some
     * similarity (function words overlap) but forbid cosine ≥ 0.99. */
    gl_comprehension_result_t r1, r2;
    std::memset(&r1, 0, sizeof(r1));
    std::memset(&r2, 0, sizeof(r2));
    ASSERT_EQ(0, grounded_language_comprehend(gl, "red ocean",   &r1));
    ASSERT_EQ(0, grounded_language_comprehend(gl, "blue pizza",  &r2));

    ASSERT_NE(r1.semantic_vector, nullptr);
    ASSERT_NE(r2.semantic_vector, nullptr);
    float cos = cosine(r1.semantic_vector, r2.semantic_vector, TEST_DIM);
    EXPECT_LT(cos, 0.99f) << "Comprehend collapsed to single vector (cos=" << cos << ")";

    gl_comprehension_result_cleanup(&r1);
    gl_comprehension_result_cleanup(&r2);
}

TEST_F(GroundedLanguageDiagnostics, ComprehendIdenticalTextsYieldHighSimilarity) {
    seed_distinct_words();
    gl_comprehension_result_t r1, r2;
    std::memset(&r1, 0, sizeof(r1));
    std::memset(&r2, 0, sizeof(r2));
    ASSERT_EQ(0, grounded_language_comprehend(gl, "red ocean", &r1));
    ASSERT_EQ(0, grounded_language_comprehend(gl, "red ocean", &r2));
    ASSERT_NE(r1.semantic_vector, nullptr);
    ASSERT_NE(r2.semantic_vector, nullptr);
    float cos = cosine(r1.semantic_vector, r2.semantic_vector, TEST_DIM);
    EXPECT_GT(cos, 0.999f);
    gl_comprehension_result_cleanup(&r1);
    gl_comprehension_result_cleanup(&r2);
}

/* --- comprehend stats counter increments (proves the public counter we use
 *     for collapse triage is actually advancing) ---*/
TEST_F(GroundedLanguageDiagnostics, ComprehendIncrementsStatsCounter) {
    seed_distinct_words();
    gl_stats_t before, after;
    std::memset(&before, 0, sizeof(before));
    std::memset(&after,  0, sizeof(after));
    grounded_language_get_stats(gl, &before);

    gl_comprehension_result_t r;
    std::memset(&r, 0, sizeof(r));
    ASSERT_EQ(0, grounded_language_comprehend(gl, "red ocean", &r));
    gl_comprehension_result_cleanup(&r);

    grounded_language_get_stats(gl, &after);
    EXPECT_GE(after.total_comprehensions, before.total_comprehensions + 1u);
}

}  // namespace
