/**
 * @file test_lang_config_persist.c
 * @brief Audit fix — verify campaign feature flags + tunables round-trip
 *        via grounded_language_save_multiturn_state / _load_multiturn_state.
 *
 * Prior to this fix, the trainer had to re-flip every default-OFF flag
 * on every --resume. Now each flag + its tunable is in the multiturn
 * sidecar's CONFIG block (magic 'LANC'), and the load path applies
 * them via the public setters.
 *
 * Coverage:
 *   1. test_round_trip_all_flags_on:
 *      Enable every flag, set non-default tunables, save, destroy,
 *      recreate, load, verify all flags + tunables match.
 *   2. test_round_trip_all_flags_off:
 *      Default state round-trips identically.
 *   3. test_partial_state_intermixed:
 *      A subset of flags ON with the rest at defaults; verify only
 *      the changed ones flip on load.
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_persistence.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
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

static void tmp_path(char* buf, size_t len, const char* tag) {
    snprintf(buf, len, "/tmp/test_lang_cfg_persist_%d_%s.bin",
             (int)getpid(), tag);
}

static void test_round_trip_all_flags_on(void)
{
    char path[256];
    tmp_path(path, sizeof(path), "all_on");

    /* Save side. */
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create save");
    if (!gl) return;

    /* Flip everything ON with non-default tunables so the test
     * catches both flags AND values. */
    grounded_language_set_negation_enabled(gl, true);
    grounded_language_set_sense_disambiguation_enabled(gl, true);
    grounded_language_set_speech_act_classification_enabled(gl, true);
    grounded_language_set_sentence_segmentation_enabled(gl, true);
    grounded_language_set_topic_shift_enabled(gl, true);
    grounded_language_set_topic_shift_threshold(gl, 0.42f);
    grounded_language_set_topic_shift_min_turns(gl, 7);
    grounded_language_set_reconsolidation_enabled(gl, true);
    grounded_language_set_reconsolidation_decay(gl, 0.17f);

    FILE* f = fopen(path, "wb");
    EXPECT(f != NULL, "fopen save");
    if (!f) { grounded_language_destroy(gl); return; }
    int rc = grounded_language_save_multiturn_state(gl, f);
    fclose(f);
    EXPECT(rc == 0, "save rc=%d", rc);
    grounded_language_destroy(gl);

    /* Load side — fresh gl with defaults. */
    grounded_language_t* gl2 = grounded_language_create(0, NULL);
    EXPECT(gl2 != NULL, "create load");
    if (!gl2) { unlink(path); return; }

    /* Verify defaults before load. */
    EXPECT(!grounded_language_get_speech_act_classification_enabled(gl2),
           "speech_act default OFF");
    EXPECT(!grounded_language_get_sentence_segmentation_enabled(gl2),
           "sentence_seg default OFF");
    EXPECT(!grounded_language_get_topic_shift_enabled(gl2),
           "topic_shift default OFF");
    EXPECT(!grounded_language_get_reconsolidation_enabled(gl2),
           "reconsolidation default OFF");

    f = fopen(path, "rb");
    EXPECT(f != NULL, "fopen load");
    if (!f) { grounded_language_destroy(gl2); unlink(path); return; }
    rc = grounded_language_load_multiturn_state(gl2, f);
    fclose(f);
    EXPECT(rc == 0, "load rc=%d", rc);

    /* Verify all flags applied. */
    EXPECT(grounded_language_get_negation_enabled(gl2), "negation ON");
    EXPECT(grounded_language_get_sense_disambiguation_enabled(gl2),
           "sense ON");
    EXPECT(grounded_language_get_speech_act_classification_enabled(gl2),
           "speech_act ON");
    EXPECT(grounded_language_get_sentence_segmentation_enabled(gl2),
           "sentence_seg ON");
    EXPECT(grounded_language_get_topic_shift_enabled(gl2),
           "topic_shift ON");
    EXPECT(grounded_language_get_reconsolidation_enabled(gl2),
           "reconsolidation ON");
    /* Tunables. */
    EXPECT(fabsf(grounded_language_get_topic_shift_threshold(gl2) - 0.42f) < 1e-5f,
           "topic_shift threshold round-trip: got %.4f",
           grounded_language_get_topic_shift_threshold(gl2));
    EXPECT(grounded_language_get_topic_shift_min_turns(gl2) == 7u,
           "topic_shift min_turns round-trip: got %u",
           grounded_language_get_topic_shift_min_turns(gl2));
    EXPECT(fabsf(grounded_language_get_reconsolidation_decay(gl2) - 0.17f) < 1e-5f,
           "reconsolidation decay round-trip: got %.4f",
           grounded_language_get_reconsolidation_decay(gl2));

    grounded_language_destroy(gl2);
    unlink(path);
}

static void test_round_trip_all_flags_off(void)
{
    char path[256];
    tmp_path(path, sizeof(path), "defaults");

    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    /* Don't touch anything — save defaults. */
    FILE* f = fopen(path, "wb");
    if (!f) { grounded_language_destroy(gl); return; }
    int rc = grounded_language_save_multiturn_state(gl, f);
    fclose(f);
    EXPECT(rc == 0, "save defaults");
    grounded_language_destroy(gl);

    grounded_language_t* gl2 = grounded_language_create(0, NULL);
    if (!gl2) { unlink(path); return; }
    f = fopen(path, "rb");
    if (!f) { grounded_language_destroy(gl2); unlink(path); return; }
    rc = grounded_language_load_multiturn_state(gl2, f);
    fclose(f);
    EXPECT(rc == 0, "load defaults");

    /* All campaign flags should be at defaults (mostly OFF; negation
     * is ON-by-default). */
    EXPECT(grounded_language_get_negation_enabled(gl2), "negation default ON preserved");
    EXPECT(!grounded_language_get_speech_act_classification_enabled(gl2),
           "speech_act default OFF preserved");
    EXPECT(!grounded_language_get_topic_shift_enabled(gl2),
           "topic_shift default OFF preserved");
    EXPECT(!grounded_language_get_reconsolidation_enabled(gl2),
           "reconsolidation default OFF preserved");

    grounded_language_destroy(gl2);
    unlink(path);
}

static void test_partial_state_intermixed(void)
{
    char path[256];
    tmp_path(path, sizeof(path), "partial");

    grounded_language_t* gl = grounded_language_create(0, NULL);
    if (!gl) return;

    /* Mixed: TA-5 ON with non-default decay, TB-9 ON, others default. */
    grounded_language_set_reconsolidation_enabled(gl, true);
    grounded_language_set_reconsolidation_decay(gl, 0.30f);
    grounded_language_set_speech_act_classification_enabled(gl, true);

    FILE* f = fopen(path, "wb");
    if (!f) { grounded_language_destroy(gl); return; }
    grounded_language_save_multiturn_state(gl, f);
    fclose(f);
    grounded_language_destroy(gl);

    grounded_language_t* gl2 = grounded_language_create(0, NULL);
    if (!gl2) { unlink(path); return; }
    f = fopen(path, "rb");
    if (!f) { grounded_language_destroy(gl2); unlink(path); return; }
    grounded_language_load_multiturn_state(gl2, f);
    fclose(f);

    EXPECT(grounded_language_get_reconsolidation_enabled(gl2),
           "reconsolidation ON post-load");
    EXPECT(fabsf(grounded_language_get_reconsolidation_decay(gl2) - 0.30f) < 1e-5f,
           "decay 0.30 round-trip");
    EXPECT(grounded_language_get_speech_act_classification_enabled(gl2),
           "speech_act ON post-load");
    EXPECT(!grounded_language_get_sentence_segmentation_enabled(gl2),
           "sentence_seg stayed OFF");
    EXPECT(!grounded_language_get_topic_shift_enabled(gl2),
           "topic_shift stayed OFF");

    grounded_language_destroy(gl2);
    unlink(path);
}

/* full-walkthrough 2026-05-25: NLP-2 coref + NLP-1 subword OOV flags were
 * persisted as trailing LANC bytes (no version bump). Verify they (a) toggle
 * via the public setters/getters and (b) survive a save/load round-trip. */
static void test_round_trip_nlp_reachability_flags(void)
{
    char path[256];
    tmp_path(path, sizeof(path), "nlp_reach");

    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create save");
    if (!gl) return;

    /* Defaults must be OFF. */
    EXPECT(!grounded_language_get_coref_resolution_enabled(gl),
           "coref default OFF");
    EXPECT(!grounded_language_get_subword_oov_fallback_enabled(gl),
           "subword default OFF");

    /* Setter/getter round-trip in-memory. */
    grounded_language_set_coref_resolution_enabled(gl, true);
    grounded_language_set_subword_oov_fallback_enabled(gl, true);
    EXPECT(grounded_language_get_coref_resolution_enabled(gl),
           "coref ON after set");
    EXPECT(grounded_language_get_subword_oov_fallback_enabled(gl),
           "subword ON after set");

    FILE* f = fopen(path, "wb");
    EXPECT(f != NULL, "fopen save");
    if (!f) { grounded_language_destroy(gl); return; }
    int rc = grounded_language_save_multiturn_state(gl, f);
    fclose(f);
    EXPECT(rc == 0, "save rc=%d", rc);
    grounded_language_destroy(gl);

    /* Fresh gl — defaults OFF — then load and verify both flipped ON. */
    grounded_language_t* gl2 = grounded_language_create(0, NULL);
    EXPECT(gl2 != NULL, "create load");
    if (!gl2) { unlink(path); return; }
    EXPECT(!grounded_language_get_coref_resolution_enabled(gl2),
           "coref default OFF pre-load");
    EXPECT(!grounded_language_get_subword_oov_fallback_enabled(gl2),
           "subword default OFF pre-load");

    f = fopen(path, "rb");
    EXPECT(f != NULL, "fopen load");
    if (!f) { grounded_language_destroy(gl2); unlink(path); return; }
    rc = grounded_language_load_multiturn_state(gl2, f);
    fclose(f);
    EXPECT(rc == 0, "load rc=%d", rc);

    EXPECT(grounded_language_get_coref_resolution_enabled(gl2),
           "coref ON post-load");
    EXPECT(grounded_language_get_subword_oov_fallback_enabled(gl2),
           "subword ON post-load");

    grounded_language_destroy(gl2);
    unlink(path);
}

int main(void)
{
    fprintf(stderr, "=== test_lang_config_persist (audit fix) ===\n");
    test_round_trip_all_flags_on();
    test_round_trip_all_flags_off();
    test_partial_state_intermixed();
    test_round_trip_nlp_reachability_flags();

    if (g_failures == 0) {
        fprintf(stderr, "ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
