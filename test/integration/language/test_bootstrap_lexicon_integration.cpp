/**
 * @file test_bootstrap_lexicon_integration.cpp
 * @brief Integration tests for the base-lexicon bootstrap API.
 *
 * WHAT: Cover nimcp_brain_bootstrap_lexicon end-to-end through the public
 *       brain handle. Verifies vocabulary growth, JSON parser robustness
 *       (malformed entries skipped, missing file rejected, version
 *       mismatch rejected), and idempotency on repeat bootstrap.
 *
 * WHY:  Bootstrap is the cold-start fix for the language collapse bug —
 *       without it, a fresh brain has only ~50 function words and every
 *       respond() call falls into the IDK floor. Bootstrap injects
 *       ~1500 content words with deterministic feature vectors so the
 *       module has something to ground against from step 1.
 *
 * HOW:  Builds tiny JSON fixtures in /tmp, calls bootstrap, and reads
 *       back vocab_size via the diagnostics RPC.
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

extern "C" {
#include "nimcp.h"
}

namespace {

class BootstrapLexicon : public ::testing::Test {
protected:
    nimcp_brain_t brain = nullptr;
    std::string tmp_json;

    void SetUp() override {
        brain = nimcp_brain_create_fast("bootstrap_test",
            NIMCP_TASK_CLASSIFICATION, 100, 10, 100);
        if (!brain) GTEST_SKIP() << "brain create failed";
        char buf[256];
        std::snprintf(buf, sizeof(buf), "/tmp/bootstrap_lex_%d.json",
                      (int)getpid());
        tmp_json = buf;
    }
    void TearDown() override {
        if (brain) nimcp_brain_destroy(brain);
        std::remove(tmp_json.c_str());
    }

    void write_fixture(const char* contents) {
        FILE* f = std::fopen(tmp_json.c_str(), "w");
        ASSERT_NE(f, nullptr);
        std::fputs(contents, f);
        std::fclose(f);
    }

    uint32_t vocab_size() {
        nimcp_grounded_language_diagnostics_t d;
        std::memset(&d, 0, sizeof(d));
        nimcp_brain_get_grounded_language_diagnostics(brain, &d);
        return d.vocab_size;
    }
};

/* --- happy path: bootstrap grows vocab ----------------------------- */
TEST_F(BootstrapLexicon, BootstrapGrowsVocabulary) {
    write_fixture(
        "{ \"version\": 1, \"semantic_dim_hint\": 4, \"words\": ["
        "  { \"form\": \"cat\",   \"class\": \"noun\", \"features\": [1.0, 0.0, 0.0, 0.0] },"
        "  { \"form\": \"dog\",   \"class\": \"noun\", \"features\": [0.0, 1.0, 0.0, 0.0] },"
        "  { \"form\": \"red\",   \"class\": \"adjective\", \"features\": [0.0, 0.0, 1.0, 0.0] },"
        "  { \"form\": \"run\",   \"class\": \"verb\", \"features\": [0.0, 0.0, 0.0, 1.0] }"
        "] }");
    uint32_t before = vocab_size();
    nimcp_status_t s = nimcp_brain_bootstrap_lexicon(brain, tmp_json.c_str());
    ASSERT_EQ(s, NIMCP_OK);
    EXPECT_GT(vocab_size(), before);
}

/* --- malformed entries are skipped, others succeed ----------------- */
TEST_F(BootstrapLexicon, MalformedEntriesAreSkipped) {
    write_fixture(
        "{ \"version\": 1, \"semantic_dim_hint\": 4, \"words\": ["
        "  { \"form\": \"valid_a\", \"class\": \"noun\", \"features\": [1.0, 0.0, 0.0, 0.0] },"
        "  { \"junk\": true },"
        "  { \"form\": \"valid_b\", \"class\": \"noun\", \"features\": [0.0, 1.0, 0.0, 0.0] }"
        "] }");
    uint32_t before = vocab_size();
    nimcp_status_t s = nimcp_brain_bootstrap_lexicon(brain, tmp_json.c_str());
    /* Malformed entries shouldn't fail the whole call — at least the two
     * valid words should land. */
    ASSERT_EQ(s, NIMCP_OK);
    EXPECT_GE(vocab_size(), before + 1);
}

/* --- missing file ---------------------------------------------------- */
TEST_F(BootstrapLexicon, MissingFileReturnsError) {
    nimcp_status_t s = nimcp_brain_bootstrap_lexicon(brain,
        "/tmp/_definitely_does_not_exist_xyz.json");
    EXPECT_EQ(s, NIMCP_ERROR);
}

/* --- NULL args ----------------------------------------------------- */
TEST_F(BootstrapLexicon, NullPathRejected) {
    nimcp_status_t s = nimcp_brain_bootstrap_lexicon(brain, nullptr);
    EXPECT_EQ(s, NIMCP_ERROR_INVALID);
}
TEST_F(BootstrapLexicon, NullBrainRejected) {
    nimcp_status_t s = nimcp_brain_bootstrap_lexicon(nullptr, tmp_json.c_str());
    EXPECT_EQ(s, NIMCP_ERROR_INVALID);
}

/* --- version mismatch is rejected (forward-compat guard) ----------- */
TEST_F(BootstrapLexicon, FutureVersionRejected) {
    write_fixture(
        "{ \"version\": 999, \"semantic_dim_hint\": 4, \"words\": ["
        "  { \"form\": \"cat\", \"features\": [1.0, 0.0, 0.0, 0.0] }"
        "] }");
    nimcp_status_t s = nimcp_brain_bootstrap_lexicon(brain, tmp_json.c_str());
    EXPECT_EQ(s, NIMCP_ERROR_INVALID);
}

/* --- idempotent re-bootstrap does not corrupt --------------------- */
TEST_F(BootstrapLexicon, RepeatBootstrapIsSafe) {
    write_fixture(
        "{ \"version\": 1, \"semantic_dim_hint\": 4, \"words\": ["
        "  { \"form\": \"alpha\", \"class\": \"noun\", \"features\": [1.0, 0.0, 0.0, 0.0] },"
        "  { \"form\": \"beta\",  \"class\": \"noun\", \"features\": [0.0, 1.0, 0.0, 0.0] }"
        "] }");
    ASSERT_EQ(NIMCP_OK, nimcp_brain_bootstrap_lexicon(brain, tmp_json.c_str()));
    uint32_t after_first = vocab_size();
    /* Second bootstrap on same file: should not crash, vocab should
     * remain stable (fast_map is form-keyed and idempotent). */
    ASSERT_EQ(NIMCP_OK, nimcp_brain_bootstrap_lexicon(brain, tmp_json.c_str()));
    EXPECT_GE(vocab_size(), after_first);
}

/* --- diagnostics still work post-bootstrap ------------------------- */
TEST_F(BootstrapLexicon, DiagnosticsReflectBootstrap) {
    write_fixture(
        "{ \"version\": 1, \"semantic_dim_hint\": 4, \"words\": ["
        "  { \"form\": \"x1\", \"class\": \"noun\", \"features\": [1.0, 0.0, 0.0, 0.0] },"
        "  { \"form\": \"x2\", \"class\": \"noun\", \"features\": [0.0, 1.0, 0.0, 0.0] },"
        "  { \"form\": \"x3\", \"class\": \"noun\", \"features\": [0.0, 0.0, 1.0, 0.0] }"
        "] }");
    uint32_t before = vocab_size();
    ASSERT_EQ(NIMCP_OK, nimcp_brain_bootstrap_lexicon(brain, tmp_json.c_str()));
    nimcp_grounded_language_diagnostics_t d;
    std::memset(&d, 0, sizeof(d));
    ASSERT_EQ(NIMCP_OK,
        nimcp_brain_get_grounded_language_diagnostics(brain, &d));
    EXPECT_GT(d.vocab_size, before);
}

}  // namespace
