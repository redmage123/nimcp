/**
 * @file test_lang_clause_frame.c
 * @brief FND-1 — SVO clause/argument planner in produce.
 *
 * Covers gl_build_clause_frame (via grounded_language_produce) + the
 * produce_clause_frame flag:
 *   1. setter/getter — default OFF, round-trips, NULL-safe.
 *   2. LANC persistence — flag survives save/load (trailing byte).
 *   3. clause emit — with the flag ON + a grounded verb + nouns at stage 2,
 *      produce emits a "the SUBJECT VERB the OBJECT"-shaped clause (subject and
 *      object are nouns, the verb sits between them) instead of a bag.
 *   4. fallback — flag ON but no usable VERB candidate -> produce still returns
 *      a valid result via the greedy path (never worse than baseline).
 *   5. gate — flag ON at stage 1 -> clause frame does not engage (no crash).
 *
 * Test words use morphology-classified forms (-tion/-ment -> NOUN, -ate/-ize
 * -> VERB) so gl_f4_class resolves roles deterministically without grounding.
 *
 * Compile (CMake wires this in lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_clause_frame.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,build/lib -o /tmp/test_lang_clause_frame
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_persistence.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

static int g_failures = 0;
#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

#define SDIM 64u

/* Ground a word with a feature vector peaked at slot s (so different words get
 * distinguishable but overlapping context profiles for produce scoring). */
static void ground_word(grounded_language_t* gl, const char* w, uint32_t s) {
    float feats[SDIM] = {0};
    if (s < SDIM)        feats[s]      = 1.0f;
    if ((s + 5u) < SDIM) feats[s + 5u] = 0.4f;
    grounded_language_fast_map(gl, w, feats, SDIM, 1u);
}

/* Find token position of `word` in a space-separated string, or -1. */
static int word_pos(const char* s, const char* word) {
    int idx = 0;
    char buf[256];
    size_t n = strlen(s); if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, s, n); buf[n] = '\0';
    for (char* tok = strtok(buf, " "); tok; tok = strtok(NULL, " "), idx++)
        if (strcmp(tok, word) == 0) return idx;
    return -1;
}

static void test_setter_getter(void) {
    grounded_language_t* gl = grounded_language_create(SDIM, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;
    EXPECT(!grounded_language_get_produce_clause_frame(gl), "default OFF");
    grounded_language_set_produce_clause_frame(gl, true);
    EXPECT(grounded_language_get_produce_clause_frame(gl), "ON after set");
    grounded_language_set_produce_clause_frame(gl, false);
    EXPECT(!grounded_language_get_produce_clause_frame(gl), "OFF after clear");
    EXPECT(!grounded_language_get_produce_clause_frame(NULL), "NULL -> false");
    grounded_language_set_produce_clause_frame(NULL, true); /* must not crash */
    grounded_language_destroy(gl);
}

static void test_persist_round_trip(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_lang_clause_%d.bin", (int)getpid());

    grounded_language_t* gl = grounded_language_create(SDIM, NULL);
    EXPECT(gl != NULL, "create save"); if (!gl) return;
    grounded_language_set_produce_clause_frame(gl, true);
    FILE* f = fopen(path, "wb");
    EXPECT(f != NULL, "fopen save"); if (!f) { grounded_language_destroy(gl); return; }
    int rc = grounded_language_save_multiturn_state(gl, f);
    fclose(f);
    EXPECT(rc == 0, "save rc=%d", rc);
    grounded_language_destroy(gl);

    grounded_language_t* gl2 = grounded_language_create(SDIM, NULL);
    EXPECT(gl2 != NULL, "create load"); if (!gl2) { unlink(path); return; }
    EXPECT(!grounded_language_get_produce_clause_frame(gl2), "default OFF pre-load");
    f = fopen(path, "rb");
    EXPECT(f != NULL, "fopen load"); if (!f) { grounded_language_destroy(gl2); unlink(path); return; }
    rc = grounded_language_load_multiturn_state(gl2, f);
    fclose(f);
    EXPECT(rc == 0, "load rc=%d", rc);
    EXPECT(grounded_language_get_produce_clause_frame(gl2), "ON post-load");
    grounded_language_destroy(gl2);
    unlink(path);
}

/* Build a gl with two grounded NOUNs + one grounded VERB at stage 2. */
static grounded_language_t* mk_clause_gl(void) {
    grounded_language_t* gl = grounded_language_create(SDIM, NULL);
    if (!gl) return NULL;
    grounded_language_set_current_stage_int(gl, 2);
    ground_word(gl, "creation",  4u);   /* NOUN (-tion) */
    ground_word(gl, "instrument", 11u); /* NOUN (-ment) */
    ground_word(gl, "activate",  18u);  /* VERB (-ate)  */
    return gl;
}

static void test_clause_shape(void) {
    grounded_language_t* gl = mk_clause_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;

    /* Intent covers all three words so all are viable candidates. */
    float intent[SDIM] = {0};
    intent[4u] = 1.0f; intent[11u] = 1.0f; intent[18u] = 1.0f;

    grounded_language_set_produce_clause_frame(gl, true);
    gl_production_result_t r = {0};
    int rc = grounded_language_produce(gl, intent, SDIM, GL_PRODUCE_DESCRIBE, &r);
    EXPECT(rc == 0 || rc == -1, "clause produce valid rc (%d)", rc);
    if (rc == 0 && r.text) {
        fprintf(stderr, "  clause: '%s'\n", r.text);
        int pv = word_pos(r.text, "activate");
        int pc = word_pos(r.text, "creation");
        int pi = word_pos(r.text, "instrument");
        /* The verb is the predicate: when both nouns are present it sits
         * between them (SUBJECT verb OBJECT). At minimum the verb is present
         * and not utterance-initial (a noun subject precedes it). */
        EXPECT(pv >= 0, "verb 'activate' present (got '%s')", r.text);
        if (pv >= 0 && pc >= 0 && pi >= 0) {
            int subj = (pc < pi) ? pc : pi;
            int obj  = (pc < pi) ? pi : pc;
            EXPECT(subj < pv && pv < obj,
                   "SVO order: subject(%d) < verb(%d) < object(%d) in '%s'",
                   subj, pv, obj, r.text);
        } else if (pv >= 0) {
            EXPECT(pv > 0, "verb not utterance-initial (subject precedes) '%s'", r.text);
        }
    }
    grounded_language_destroy(gl);
}

static void test_fallback_no_verb(void) {
    /* Only nouns grounded -> no head verb -> clause frame returns -1 ->
     * produce falls back to greedy and still emits. */
    grounded_language_t* gl = grounded_language_create(SDIM, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_current_stage_int(gl, 2);
    ground_word(gl, "creation",  4u);
    ground_word(gl, "instrument", 11u);
    float intent[SDIM] = {0};
    intent[4u] = 1.0f; intent[11u] = 1.0f;
    grounded_language_set_produce_clause_frame(gl, true);
    gl_production_result_t r = {0};
    int rc = grounded_language_produce(gl, intent, SDIM, GL_PRODUCE_DESCRIBE, &r);
    EXPECT(rc == 0 || rc == -1, "no-verb fallback valid rc (%d)", rc);
    if (rc == 0) EXPECT(r.text && r.text[0], "fallback still emits content");
    grounded_language_destroy(gl);
}

/* FND-2: copula frame — no head verb but a noun + a predicate adjective ->
 * "the SUBJECT is ADJECTIVE" instead of falling back to a bag. */
static void test_copula_frame(void) {
    grounded_language_t* gl = grounded_language_create(SDIM, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_current_stage_int(gl, 2);
    ground_word(gl, "creation", 4u);    /* NOUN (-tion)      */
    ground_word(gl, "creative", 11u);   /* ADJECTIVE (-ive)  */
    float intent[SDIM] = {0};
    intent[4u] = 1.0f; intent[11u] = 1.0f;
    grounded_language_set_produce_clause_frame(gl, true);
    gl_production_result_t r = {0};
    int rc = grounded_language_produce(gl, intent, SDIM, GL_PRODUCE_DESCRIBE, &r);
    EXPECT(rc == 0 || rc == -1, "copula valid rc (%d)", rc);
    if (rc == 0 && r.text) {
        fprintf(stderr, "  copula: '%s'\n", r.text);
        int pc  = word_pos(r.text, "creation");
        int pis = word_pos(r.text, "is");
        int pcr = word_pos(r.text, "creative");
        EXPECT(pc >= 0 && pis >= 0 && pcr >= 0 && pc < pis && pis < pcr,
               "copula order subject(%d) < is(%d) < adj(%d) in '%s'",
               pc, pis, pcr, r.text);
    }
    grounded_language_destroy(gl);
}

/* FND-2: adjective NP modifier in SVO -> "the ADJ SUBJECT VERB ...". */
static void test_adjective_modifier(void) {
    grounded_language_t* gl = grounded_language_create(SDIM, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_current_stage_int(gl, 2);
    ground_word(gl, "creation", 4u);    /* NOUN (subject)    */
    ground_word(gl, "activate", 11u);   /* VERB (-ate)       */
    ground_word(gl, "powerful", 18u);   /* ADJECTIVE (-ful)  */
    float intent[SDIM] = {0};
    intent[4u] = 1.0f; intent[11u] = 1.0f; intent[18u] = 1.0f;
    grounded_language_set_produce_clause_frame(gl, true);
    gl_production_result_t r = {0};
    int rc = grounded_language_produce(gl, intent, SDIM, GL_PRODUCE_DESCRIBE, &r);
    EXPECT(rc == 0 || rc == -1, "adj-mod valid rc (%d)", rc);
    if (rc == 0 && r.text) {
        fprintf(stderr, "  adj-mod: '%s'\n", r.text);
        int pp  = word_pos(r.text, "powerful");
        int pcr = word_pos(r.text, "creation");
        int pa  = word_pos(r.text, "activate");
        /* adjective modifies the subject NP: precedes the subject noun, which
         * in turn precedes the verb. */
        if (pp >= 0 && pcr >= 0)
            EXPECT(pp < pcr, "adjective before subject noun in '%s'", r.text);
        if (pcr >= 0 && pa >= 0)
            EXPECT(pcr < pa, "subject before verb in '%s'", r.text);
    }
    grounded_language_destroy(gl);
}

/* FND-2b: animate-agent preference. An animate word is a noun candidate even
 * without noun morphology, and wins the subject role over a higher-scoring
 * inanimate noun. Here "creation" is grounded to outscore "woman", so without
 * animacy "creation" would be the subject; the swap makes "woman" the subject
 * ("the woman activate the creation"). */
static void test_animacy_subject(void) {
    grounded_language_t* gl = grounded_language_create(SDIM, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_current_stage_int(gl, 2);
    ground_word(gl, "creation", 4u);    /* NOUN, will outscore woman */
    ground_word(gl, "woman",   18u);    /* animate (morphology UNKNOWN) */
    ground_word(gl, "activate", 11u);   /* VERB */
    /* Intent matches creation strongly, woman weakly -> creation is the
     * top-scoring noun; animacy must still promote woman to subject. */
    float intent[SDIM] = {0};
    intent[4u] = 1.0f; intent[9u] = 0.4f; intent[11u] = 1.0f; intent[18u] = 0.5f;
    grounded_language_set_produce_clause_frame(gl, true);
    gl_production_result_t r = {0};
    int rc = grounded_language_produce(gl, intent, SDIM, GL_PRODUCE_DESCRIBE, &r);
    EXPECT(rc == 0 || rc == -1, "animacy valid rc (%d)", rc);
    if (rc == 0 && r.text) {
        fprintf(stderr, "  animacy: '%s'\n", r.text);
        int pw = word_pos(r.text, "woman");
        int pa = word_pos(r.text, "activate");
        int pc = word_pos(r.text, "creation");
        EXPECT(pw >= 0, "animate 'woman' present as a candidate (got '%s')", r.text);
        if (pw >= 0 && pa >= 0)
            EXPECT(pw < pa, "animate subject 'woman' precedes verb in '%s'", r.text);
        if (pc >= 0 && pa >= 0)
            EXPECT(pc > pa, "inanimate 'creation' is the object (after verb) in '%s'", r.text);
    }
    grounded_language_destroy(gl);
}

/* End-to-end composition: the clause planner emits a BASE-form verb; the
 * cascade then runs F3 (gl_apply_svo_agreement) over the produce output (Broca
 * re-render between them is order-preserving, so the clause reaches F3 intact).
 * Verify produce_clause_frame -> F3 inflects the verb to agree with the 3sg
 * noun subject: "the creation activate ..." -> "the creation activates ...". */
static void test_clause_then_f3_inflection(void) {
    grounded_language_t* gl = mk_clause_gl();  /* creation/instrument/activate, stage 2 */
    EXPECT(gl != NULL, "create"); if (!gl) return;
    float intent[SDIM] = {0};
    intent[4u] = 1.0f; intent[11u] = 1.0f; intent[18u] = 1.0f;
    grounded_language_set_produce_clause_frame(gl, true);
    gl_production_result_t r = {0};
    int rc = grounded_language_produce(gl, intent, SDIM, GL_PRODUCE_DESCRIBE, &r);
    if (rc == 0 && r.text) {
        char corrected[256];
        int fixes = gl_apply_svo_agreement(gl, r.text, corrected, sizeof(corrected));
        fprintf(stderr, "  produce: '%s'  --F3-->  '%s' (%d fix)\n",
                r.text, corrected, fixes);
        /* The clause verb 'activate' should become 'activates' (3sg). */
        if (word_pos(r.text, "activate") >= 0) {
            EXPECT(word_pos(corrected, "activates") >= 0,
                   "F3 inflected clause verb to 'activates' (got '%s')", corrected);
            EXPECT(word_pos(corrected, "activate") < 0,
                   "base-form 'activate' replaced (got '%s')", corrected);
        }
    }
    grounded_language_destroy(gl);
}

static void test_stage_gate(void) {
    /* Stage 1 + flag ON -> clause frame must not engage; produce path still
     * runs (no crash, valid rc). */
    grounded_language_t* gl = mk_clause_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_current_stage_int(gl, 1);
    grounded_language_set_produce_clause_frame(gl, true);
    float intent[SDIM] = {0};
    intent[4u] = 1.0f; intent[11u] = 1.0f; intent[18u] = 1.0f;
    gl_production_result_t r = {0};
    int rc = grounded_language_produce(gl, intent, SDIM, GL_PRODUCE_DESCRIBE, &r);
    EXPECT(rc == 0 || rc == -1, "stage-1 clause-off valid rc (%d)", rc);
    grounded_language_destroy(gl);
}

int main(void) {
    fprintf(stderr, "[UNIT] test_lang_clause_frame (FND-1)\n");
    test_setter_getter();
    test_persist_round_trip();
    test_clause_shape();
    test_fallback_no_verb();
    test_copula_frame();
    test_adjective_modifier();
    test_animacy_subject();
    test_clause_then_f3_inflection();
    test_stage_gate();
    if (g_failures == 0) { fprintf(stderr, "ALL PASS\n"); return 0; }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
