/**
 * @file test_grounded_language_persistence.cpp
 * @brief Unit tests for the grounded_language sidecar persistence API.
 *
 * WHAT: Cover gl_persistence_save / gl_persistence_load round-trip and
 *       the defensive load paths (missing file, bad magic, version
 *       mismatch, corrupt payload).
 *
 * WHY:  Without persistence, every daemon restart wipes the trained
 *       lexicon and the language module has to re-learn from zero.
 *       Round-trip integrity is the contract we depend on; defensive
 *       loads stop a bad sidecar from crashing brain init.
 *
 * HOW:  No brain handle — exercise grounded_language_create → seed →
 *       save → destroy → create again → load → verify counters.
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_persistence.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
}

namespace {

constexpr uint32_t TEST_DIM = 32;

class GLPersistence : public ::testing::Test {
protected:
    grounded_language_t* gl = nullptr;
    semantic_memory_system_t* sm = nullptr;
    std::string tmp_path;

    void SetUp() override {
        sm = semantic_memory_create();
        ASSERT_NE(sm, nullptr);
        gl = grounded_language_create(TEST_DIM, sm);
        ASSERT_NE(gl, nullptr);
        char buf[256];
        std::snprintf(buf, sizeof(buf), "/tmp/gl_persist_%d_%s.gl_lang",
                      (int)getpid(),
                      ::testing::UnitTest::GetInstance()
                          ->current_test_info()->name());
        tmp_path = buf;
        std::remove(tmp_path.c_str());
    }
    void TearDown() override {
        if (gl) grounded_language_destroy(gl);
        if (sm) semantic_memory_destroy(sm);
        std::remove(tmp_path.c_str());
    }

    void seed_words(int n) {
        for (int i = 0; i < n; i++) {
            std::vector<float> feats(TEST_DIM, 0.0f);
            feats[i % TEST_DIM] = 1.0f;
            char w[32];
            std::snprintf(w, sizeof(w), "word_%d", i);
            grounded_language_fast_map(gl, w, feats.data(), TEST_DIM, 0);
        }
    }
};

/* --- Round-trip: save → fresh handle → load → counters match -------- */
TEST_F(GLPersistence, RoundTripPreservesVocabSize) {
    seed_words(8);
    gl_stats_t before{}; grounded_language_get_stats(gl, &before);
    ASSERT_GT(before.vocab_size, 0u);

    ASSERT_EQ(0, gl_persistence_save(gl, tmp_path.c_str()));

    /* Replace the handle with a fresh one and load. */
    grounded_language_destroy(gl);
    gl = grounded_language_create(TEST_DIM, sm);
    ASSERT_NE(gl, nullptr);

    ASSERT_EQ(0, gl_persistence_load(gl, tmp_path.c_str()));
    gl_stats_t after{}; grounded_language_get_stats(gl, &after);
    EXPECT_EQ(after.vocab_size, before.vocab_size);
}

/* --- Defensive: missing file --------------------------------------- */
TEST_F(GLPersistence, LoadReturnsErrorOnMissingFile) {
    /* tmp_path was deleted in SetUp — load should fail cleanly. */
    EXPECT_EQ(-1, gl_persistence_load(gl, tmp_path.c_str()));
    /* Module should remain usable. */
    gl_stats_t s{}; grounded_language_get_stats(gl, &s);
    EXPECT_GT(s.vocab_size, 0u);  /* function-word seed survived */
}

/* --- Defensive: NULL args ------------------------------------------ */
TEST_F(GLPersistence, SaveRejectsNullHandle) {
    EXPECT_EQ(-1, gl_persistence_save(nullptr, tmp_path.c_str()));
}
TEST_F(GLPersistence, SaveRejectsNullPath) {
    EXPECT_EQ(-1, gl_persistence_save(gl, nullptr));
}
TEST_F(GLPersistence, LoadRejectsNullHandle) {
    EXPECT_EQ(-1, gl_persistence_load(nullptr, tmp_path.c_str()));
}
TEST_F(GLPersistence, LoadRejectsNullPath) {
    EXPECT_EQ(-1, gl_persistence_load(gl, nullptr));
}

/* --- Defensive: bad magic ------------------------------------------ */
TEST_F(GLPersistence, LoadRejectsBadMagic) {
    FILE* f = std::fopen(tmp_path.c_str(), "wb");
    ASSERT_NE(f, nullptr);
    const char garbage[] = "NOTNIMCP_GL_BUTSOMETHINGELSE";
    std::fwrite(garbage, 1, sizeof(garbage), f);
    std::fclose(f);
    EXPECT_EQ(-1, gl_persistence_load(gl, tmp_path.c_str()));
}

/* --- Defensive: truncated file ------------------------------------- */
TEST_F(GLPersistence, LoadRejectsTruncatedFile) {
    seed_words(4);
    ASSERT_EQ(0, gl_persistence_save(gl, tmp_path.c_str()));
    /* Truncate to 4 bytes — header magic alone, no body. */
    FILE* f = std::fopen(tmp_path.c_str(), "rb+");
    ASSERT_NE(f, nullptr);
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fclose(f);
    ASSERT_GT(sz, 4);
    /* Replace with a 4-byte file (truncated header). */
    f = std::fopen(tmp_path.c_str(), "wb");
    std::fwrite("NIMC", 1, 4, f);
    std::fclose(f);

    grounded_language_destroy(gl);
    gl = grounded_language_create(TEST_DIM, sm);
    EXPECT_EQ(-1, gl_persistence_load(gl, tmp_path.c_str()));
}

}  // namespace
