/**
 * @file test_grounded_language_deferred_wave2.cpp
 * @brief Unit tests for the deferred-enhancement wave 2:
 *          #13 phonological rhyme + alliteration retrieval
 *          #5  lexicon LRU eviction
 */

#include <gtest/gtest.h>
#include <cstring>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
}

namespace {

constexpr uint32_t TEST_DIM = 32;

class GLDeferredW2 : public ::testing::Test {
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

    /* Bump frequency for `word` n times by repeated grounding. */
    void heat(const char* w, int n) {
        std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
        for (int i = 0; i < n; i++) {
            gl_grounding_event_t ev{};
            ev.word = w;
            ev.modality = GL_MODALITY_VISUAL;
            ev.sensory_features = f.data();
            ev.feature_dim = TEST_DIM;
            ev.attention = 0.5f;
            grounded_language_ground(gl, &ev);
        }
    }
};

/* ====================================================================
 * #13 phonological rhyme retrieval
 * ==================================================================*/

TEST_F(GLDeferredW2, RhymeFindsExpectedSuffixMatches) {
    seed_word("light");
    seed_word("flight");
    seed_word("bright");
    seed_word("might");
    seed_word("apple");      /* not a rhyme */
    seed_word("orange");     /* not a rhyme */

    const char* hits[16] = {0};
    uint32_t n = grounded_language_lookup_rhymes(gl, "right", hits, 16);

    /* Expect at least the 4 -ight words; "right" itself is not seeded. */
    std::set<std::string> got;
    for (uint32_t i = 0; i < n; i++) got.insert(hits[i]);
    EXPECT_NE(got.find("light"),  got.end());
    EXPECT_NE(got.find("flight"), got.end());
    EXPECT_NE(got.find("bright"), got.end());
    EXPECT_NE(got.find("might"),  got.end());
    EXPECT_EQ(got.find("apple"),  got.end());
    EXPECT_EQ(got.find("orange"), got.end());
}

TEST_F(GLDeferredW2, RhymeExcludesSelfMatch) {
    seed_word("cat");
    seed_word("hat");
    seed_word("bat");
    const char* hits[8] = {0};
    uint32_t n = grounded_language_lookup_rhymes(gl, "cat", hits, 8);
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_STRNE("cat", hits[i]);
    }
}

TEST_F(GLDeferredW2, RhymeReturnsZeroForShortInput) {
    seed_word("at");
    seed_word("hat");
    /* "ab" has 2 letters → below GL_RHYME_SUFFIX_LEN. */
    const char* hits[8] = {0};
    EXPECT_EQ(0u, grounded_language_lookup_rhymes(gl, "ab", hits, 8));
}

TEST_F(GLDeferredW2, RhymeRespectsMaxOut) {
    seed_word("light");
    seed_word("flight");
    seed_word("bright");
    seed_word("might");
    const char* hits[2] = {0};
    uint32_t n = grounded_language_lookup_rhymes(gl, "right", hits, 2);
    EXPECT_LE(n, 2u);
}

TEST_F(GLDeferredW2, RhymeNullSafe) {
    const char* hits[4] = {0};
    EXPECT_EQ(0u, grounded_language_lookup_rhymes(nullptr, "cat", hits, 4));
    EXPECT_EQ(0u, grounded_language_lookup_rhymes(gl, nullptr, hits, 4));
    EXPECT_EQ(0u, grounded_language_lookup_rhymes(gl, "cat", nullptr, 4));
    EXPECT_EQ(0u, grounded_language_lookup_rhymes(gl, "cat", hits, 0));
}

/* ====================================================================
 * #13 alliteration retrieval
 * ==================================================================*/

TEST_F(GLDeferredW2, AlliterationFindsSamePrefix) {
    /* Use a rare prefix ('z') so the base lexicon doesn't crowd out our
     * seeded words from the bounded result set. */
    seed_word("zalt_uniq");
    seed_word("zea_uniq");
    seed_word("zand_uniq");
    seed_word("zun_uniq");
    seed_word("apple");        /* not alliterating */
    seed_word("orange");       /* not alliterating */

    const char* hits[64] = {0};
    uint32_t n = grounded_language_lookup_alliterations(gl, "zwim_uniq", hits, 64);

    std::set<std::string> got;
    for (uint32_t i = 0; i < n; i++) got.insert(hits[i]);
    EXPECT_NE(got.find("zalt_uniq"), got.end());
    EXPECT_NE(got.find("zea_uniq"),  got.end());
    EXPECT_NE(got.find("zand_uniq"), got.end());
    EXPECT_NE(got.find("zun_uniq"),  got.end());
    EXPECT_EQ(got.find("apple"),    got.end());
    EXPECT_EQ(got.find("orange"),   got.end());
}

TEST_F(GLDeferredW2, AlliterationCaseInsensitive) {
    seed_word("Apple");
    seed_word("aardvark");
    seed_word("avocado");
    const char* hits[8] = {0};
    /* Input has uppercase first letter — must match lowercase 'a' lexicon. */
    uint32_t n = grounded_language_lookup_alliterations(gl, "Ant", hits, 8);
    EXPECT_GE(n, 3u);
}

TEST_F(GLDeferredW2, AlliterationExcludesSelf) {
    seed_word("apple");
    seed_word("avocado");
    const char* hits[4] = {0};
    uint32_t n = grounded_language_lookup_alliterations(gl, "apple", hits, 4);
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_STRNE("apple", hits[i]);
    }
}

TEST_F(GLDeferredW2, AlliterationNonAlphaInputReturnsZero) {
    const char* hits[4] = {0};
    EXPECT_EQ(0u, grounded_language_lookup_alliterations(gl, "1234", hits, 4));
    EXPECT_EQ(0u, grounded_language_lookup_alliterations(gl, "", hits, 4));
}

/* ====================================================================
 * #5 LRU eviction
 * ==================================================================*/

TEST_F(GLDeferredW2, EvictLruOnEmptyOrZeroIsNoOp) {
    /* GL ships with a base lexicon — but every entry has freq=0 so all
     * are eviction candidates. With n=0 nothing is evicted. */
    EXPECT_EQ(0u, grounded_language_evict_lru(gl, 0));
    EXPECT_EQ(0u, grounded_language_evict_lru(nullptr, 5));
}

TEST_F(GLDeferredW2, EvictLruRemovesLowestFrequencyFirst) {
    /* Seed cold and hot words. Cold = freq=1 (just fast-mapped), hot =
     * freq high via repeated grounding. fast_map sets freq=1; ground()
     * bumps it. Pin floor is 10 — keep heated word above that. */
    seed_word("cold_a");
    seed_word("cold_b");
    seed_word("hot_word");
    heat("hot_word", 30);  /* push frequency well past pin floor */

    EXPECT_TRUE(grounded_language_has_word(gl, "cold_a"));
    EXPECT_TRUE(grounded_language_has_word(gl, "cold_b"));
    EXPECT_TRUE(grounded_language_has_word(gl, "hot_word"));

    /* Evict 1 — should be one of the cold words, not hot. */
    uint32_t evicted = grounded_language_evict_lru(gl, 1);
    EXPECT_GE(evicted, 1u);
    EXPECT_TRUE(grounded_language_has_word(gl, "hot_word"));

    /* At least one cold word should be gone. */
    bool a_gone = !grounded_language_has_word(gl, "cold_a");
    bool b_gone = !grounded_language_has_word(gl, "cold_b");
    EXPECT_TRUE(a_gone || b_gone);
}

TEST_F(GLDeferredW2, EvictLruRespectsPinThreshold) {
    /* Heat several entries past pin floor. Eviction must skip them. */
    seed_word("anchor1");
    seed_word("anchor2");
    seed_word("anchor3");
    heat("anchor1", 30);
    heat("anchor2", 30);
    heat("anchor3", 30);

    /* Request 100 evictions — should evict at most the unpinned base
     * lexicon (already-low-freq) but never touch the heated anchors. */
    grounded_language_evict_lru(gl, 100);

    EXPECT_TRUE(grounded_language_has_word(gl, "anchor1"));
    EXPECT_TRUE(grounded_language_has_word(gl, "anchor2"));
    EXPECT_TRUE(grounded_language_has_word(gl, "anchor3"));
}

TEST_F(GLDeferredW2, EvictLruRebuildsHashTable) {
    /* After eviction, lookups must still work for surviving entries
     * (no stale probe chains pointing to freed memory). */
    seed_word("survivor1");
    seed_word("survivor2");
    seed_word("survivor3");
    seed_word("victim_a");
    seed_word("victim_b");
    heat("survivor1", 30);
    heat("survivor2", 30);
    heat("survivor3", 30);

    grounded_language_evict_lru(gl, 100);

    EXPECT_TRUE(grounded_language_has_word(gl, "survivor1"));
    EXPECT_TRUE(grounded_language_has_word(gl, "survivor2"));
    EXPECT_TRUE(grounded_language_has_word(gl, "survivor3"));

    /* Insertion of a fresh entry post-eviction must succeed. */
    seed_word("post_evict");
    EXPECT_TRUE(grounded_language_has_word(gl, "post_evict"));
}

/* ====================================================================
 * #5 LRU eviction via sleep_consolidate (the safe trigger point)
 * ==================================================================*/

extern "C" {
#include "cognitive/nimcp_sleep_wake.h"
}

TEST_F(GLDeferredW2, SleepConsolidateAtDeepNremDoesNotPanic) {
    /* Sanity: even when vocab is below high-water, deep-NREM consolidation
     * should run cleanly without invoking eviction. */
    seed_word("alpha");
    seed_word("beta");
    int rc = grounded_language_sleep_consolidate(
        gl, (int)SLEEP_STATE_DEEP_NREM, 0.8f);
    EXPECT_EQ(0, rc);
    EXPECT_TRUE(grounded_language_has_word(gl, "alpha"));
    EXPECT_TRUE(grounded_language_has_word(gl, "beta"));
}

TEST_F(GLDeferredW2, FindOrCreateNoLongerAutoEvicts) {
    /* Regression for the auto-trigger UAF/word-loss bug found in
     * walkthrough. Repeatedly grounding the same low-frequency word
     * after seeding a few others should NEVER cause that word to be
     * silently re-allocated (which the old auto-trigger could do). */
    seed_word("anchor1");
    seed_word("anchor2");
    seed_word("survivor_low_freq");

    const gl_lexicon_entry_t* before =
        grounded_language_lookup(gl, "survivor_low_freq");
    ASSERT_NE(before, nullptr);
    uint32_t pre_freq = before->frequency;

    /* Add many more words — but eviction is no longer auto-triggered.
     * The held entry pointer must still point at the same form. */
    for (int i = 0; i < 50; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "filler_%d", i);
        seed_word(buf);
    }

    const gl_lexicon_entry_t* after =
        grounded_language_lookup(gl, "survivor_low_freq");
    ASSERT_NE(after, nullptr);
    /* Same heap object — auto-eviction is gone. */
    EXPECT_EQ(before, after);
    EXPECT_EQ(pre_freq, after->frequency);
}

TEST_F(GLDeferredW2, EvictLruDecrementsVocabCount) {
    gl_probe_metrics_t pm0;
    grounded_language_get_probe_metrics(gl, &pm0);
    seed_word("victim_x");
    seed_word("victim_y");
    seed_word("victim_z");

    gl_probe_metrics_t pm1;
    grounded_language_get_probe_metrics(gl, &pm1);
    EXPECT_EQ(pm0.vocab_count + 3u, pm1.vocab_count);

    uint32_t n = grounded_language_evict_lru(gl, 3);
    EXPECT_GE(n, 3u);

    gl_probe_metrics_t pm2;
    grounded_language_get_probe_metrics(gl, &pm2);
    EXPECT_LT(pm2.vocab_count, pm1.vocab_count);
}

}  // namespace
