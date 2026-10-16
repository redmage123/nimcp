/**
 * profile_gl_learn_from_text.c
 *
 * Standalone profile harness for grounded_language_learn_from_text.
 *
 * Built in response to the 2026-05-15 brain wedge — all RPC workers
 * blocking inside brain.learn_language() at stage 2. Static analysis
 * pointed at the O(N² × B²) cross-bind loop (lines 3500-3518 of
 * nimcp_grounded_language.c) as the asymptotic culprit. This harness
 * gives real numbers without disrupting the production brain.
 *
 * The two scaling axes it isolates:
 *
 *   1. N (input word count): builds a fixed lexicon, then calls
 *      learn_from_text on inputs of N=5,10,20,50,100,200 words drawn
 *      from that lexicon. Confirms (or refutes) O(N²) growth in
 *      wallclock.
 *
 *   2. B (bindings per word): warms up by repeatedly calling
 *      learn_from_text on the SAME small vocabulary so bindings
 *      accumulate, sampling wallclock at intervals. Confirms (or
 *      refutes) the O(B²) factor as B grows.
 *
 * Reports a CSV-ish table on stdout. No assertions — pure measurement.
 */

#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "snn/bridges/nimcp_snn_language_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* Build a space-separated text of `n` random words drawn from "word0..wordN-1".
 * Caller frees. */
static char* make_text(int n, int vocab_size) {
    /* Generous buffer: words up to "word99999 " ≈ 10 chars */
    size_t cap = (size_t)n * 12 + 1;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    size_t pos = 0;
    for (int i = 0; i < n; i++) {
        int written = snprintf(buf + pos, cap - pos, "word%d ", rand() % vocab_size);
        if (written < 0 || (size_t)written >= cap - pos) break;
        pos += (size_t)written;
    }
    return buf;
}

static int gl_total_bindings_for(grounded_language_t* gl) {
    gl_stats_t s;
    memset(&s, 0, sizeof(s));
    grounded_language_get_stats(gl, &s);
    return (int)s.total_bindings;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    srand(42);

    semantic_memory_system_t* sm = semantic_memory_create();
    grounded_language_t* gl = grounded_language_create(32, sm);
    if (!gl) {
        fprintf(stderr, "grounded_language_create failed\n");
        return 1;
    }

    printf("=== Profile: grounded_language_learn_from_text ===\n");
    printf("(measuring wallclock — pure CPU, single-threaded)\n\n");

    /* -------------------------------------------------------------------
     * Phase 1: N-scaling at fresh state (B≈0)
     * Each call uses a fresh random sample from a vocab of 100 words.
     * At fresh state the cross-bind loop bodies do little work (no
     * bindings to iterate).
     * ------------------------------------------------------------- */
    printf("Phase 1: N-scaling at fresh state (B≈0, vocab=100)\n");
    printf("  N        ms/call  bindings_after\n");
    const int phase1_sizes[] = {5, 10, 20, 50, 100, 200};
    const int phase1_count = (int)(sizeof(phase1_sizes)/sizeof(phase1_sizes[0]));
    for (int i = 0; i < phase1_count; i++) {
        int n = phase1_sizes[i];
        /* Single sample call — warm-up cache effects matter less at this scale. */
        char* text = make_text(n, 100);
        double t0 = now_ms();
        grounded_language_learn_from_text(gl, text);
        double t1 = now_ms();
        printf("  %4d   %8.3f  %d\n",
               n, t1 - t0, gl_total_bindings_for(gl));
        free(text);
    }

    /* -------------------------------------------------------------------
     * Phase 2: B-scaling at fixed N=20
     * Repeatedly call learn_from_text on the SAME 20-word vocab.
     * Each call adds bindings to those 20 words (cross-bind loop).
     * Sample the wallclock every K iterations.
     * ------------------------------------------------------------- */
    /* Reset by recreating */
    grounded_language_destroy(gl);
    semantic_memory_destroy(sm);
    sm = semantic_memory_create();
    gl = grounded_language_create(32, sm);

    printf("\nPhase 2: B-scaling at fixed N=20 (warming up bindings)\n");
    printf("  iter    ms/call  total_bindings\n");
    const int phase2_iters = 500;
    const int phase2_n = 20;
    const int phase2_vocab = 30;  /* small vocab → bindings accumulate fast */
    double cumulative_ms = 0.0;
    for (int iter = 1; iter <= phase2_iters; iter++) {
        char* text = make_text(phase2_n, phase2_vocab);
        double t0 = now_ms();
        grounded_language_learn_from_text(gl, text);
        double t1 = now_ms();
        cumulative_ms += (t1 - t0);
        free(text);

        if (iter == 1 || iter == 5 || iter == 10 || iter == 25 ||
            iter == 50 || iter == 100 || iter == 200 || iter == 300 ||
            iter == 400 || iter == 500) {
            int total = gl_total_bindings_for(gl);
            printf("  %4d   %8.3f  %d\n",
                   iter, t1 - t0, total);
        }
    }
    printf("  total wallclock for %d iterations: %.1f ms (%.2f ms/call avg)\n",
           phase2_iters, cumulative_ms, cumulative_ms / phase2_iters);

    /* -------------------------------------------------------------------
     * Phase 3: N-scaling at warm state (B>>0)
     * Phase 2 found that learn_from_text alone never creates bindings —
     * the cross-bind loop body skips when entry_i->binding_count==0.
     * To exercise the suspected wedge path we must seed bindings via
     * fast_map (which calls lexicon_bind directly). Vary B (bindings
     * per word) across a sweep so we can read off the O(B²) factor.
     * ------------------------------------------------------------- */
    grounded_language_destroy(gl);
    semantic_memory_destroy(sm);
    sm = semantic_memory_create();
    gl = grounded_language_create(32, sm);

    printf("\nPhase 3: N=50, varying B (bindings per word)\n");
    printf("  B/word   ms/call (after seeding)\n");
    const int phase3_vocab = 50;          /* matches phase3 input size */
    const int phase3_input_n = 50;
    const int b_targets[] = {1, 10, 50, 100, 250, 500, 1000, 2500, 5000};
    const int b_count = (int)(sizeof(b_targets)/sizeof(b_targets[0]));
    int b_seeded = 0;
    char wbuf[32];
    float feat[32];
    for (int t = 0; t < b_count; t++) {
        int b_target = b_targets[t];
        /* Seed bindings up to b_target per word using fast_map.
         * fast_map → lexicon_bind with unique concept_id per call
         * (find_or_create_concept hashes the feature vector). */
        while (b_seeded < b_target) {
            for (int w = 0; w < phase3_vocab; w++) {
                snprintf(wbuf, sizeof(wbuf), "word%d", w);
                /* Distinct feature vec → distinct concept_id. */
                for (int d = 0; d < 32; d++) {
                    feat[d] = ((float)w * 31.0f + (float)b_seeded * 17.0f + (float)d) / 1000.0f;
                }
                grounded_language_fast_map(gl, wbuf, feat, 32, 0);
            }
            b_seeded++;
        }
        /* Time a single learn_from_text call at N=50 in this populated state. */
        char* text = make_text(phase3_input_n, phase3_vocab);
        double t0 = now_ms();
        grounded_language_learn_from_text(gl, text);
        double t1 = now_ms();
        int total = gl_total_bindings_for(gl);
        printf("  %4d     %8.3f  (total_bindings=%d, avg %.1f/word)\n",
               b_target, t1 - t0, total, (double)total / phase3_vocab);
        free(text);
    }

    /* -------------------------------------------------------------------
     * Phase 4: N-scaling at fixed high B (B=5000 after phase 3)
     * Now hold the bindings constant from phase 3 and sweep N to see
     * the (suspected) O(N²) growth at production-like binding density.
     * ------------------------------------------------------------- */
    printf("\nPhase 4: N-scaling at high B (per-word avg≈5000)\n");
    printf("  N        ms/call\n");
    const int phase4_sizes[] = {10, 50, 100, 200, 500, 1000};
    const int phase4_count = (int)(sizeof(phase4_sizes)/sizeof(phase4_sizes[0]));
    for (int i = 0; i < phase4_count; i++) {
        int n = phase4_sizes[i];
        char* text = make_text(n, phase3_vocab);
        double t0 = now_ms();
        grounded_language_learn_from_text(gl, text);
        double t1 = now_ms();
        printf("  %5d  %8.3f\n", n, t1 - t0);
        free(text);
    }

    /* -------------------------------------------------------------------
     * Phase 4b: WITH SNN BRIDGE CONNECTED — same scenario as Phase 4 but
     * mirror_binding_to_bridge now fires (gl->snn_bridge != NULL).
     * If the mirror cascade is the production wedge, Phase 4b should
     * be dramatically slower than Phase 4.
     * ------------------------------------------------------------- */
    snn_lang_config_t cfg = snn_lang_config_default();
    snn_language_bridge_t* bridge = snn_language_bridge_create(&cfg);
    if (!bridge) {
        fprintf(stderr, "snn_language_bridge_create failed\n");
        grounded_language_destroy(gl);
        semantic_memory_destroy(sm);
        return 1;
    }
    grounded_language_connect_snn_bridge(gl, bridge);

    printf("\nPhase 4b: N-scaling at high B WITH SNN bridge connected\n");
    printf("  (mirror_binding_to_bridge now fires per lexicon_bind)\n");
    printf("  N        ms/call\n");
    for (int i = 0; i < phase4_count; i++) {
        int n = phase4_sizes[i];
        char* text = make_text(n, phase3_vocab);
        double t0 = now_ms();
        grounded_language_learn_from_text(gl, text);
        double t1 = now_ms();
        printf("  %5d  %8.3f\n", n, t1 - t0);
        free(text);
        if (t1 - t0 > 30000.0) {
            /* Guard: if a single call takes >30s, stop the sweep so
             * we don't sit here for an hour proving the bottleneck. */
            printf("  (aborting sweep — single call exceeded 30s)\n");
            break;
        }
    }

    /* -------------------------------------------------------------------
     * Phase 5: N=200 with bindings pushed to ~10K/word
     * Cap-test: push toward production's stage-2 binding density.
     * (Bridge still connected from Phase 4b.)
     * ------------------------------------------------------------- */
    printf("\nPhase 5: pushing B to 10K/word, time learn_from_text at N=200\n");
    printf("  cumulative_B/word  ms/call\n");
    const int phase5_n = 200;
    while (b_seeded < 10000) {
        for (int w = 0; w < phase3_vocab; w++) {
            snprintf(wbuf, sizeof(wbuf), "word%d", w);
            for (int d = 0; d < 32; d++) {
                feat[d] = ((float)w * 31.0f + (float)b_seeded * 17.0f + (float)d) / 1000.0f;
            }
            grounded_language_fast_map(gl, wbuf, feat, 32, 0);
        }
        b_seeded++;
        if (b_seeded == 2000 || b_seeded == 5000 || b_seeded == 10000) {
            char* text = make_text(phase5_n, phase3_vocab);
            double t0 = now_ms();
            grounded_language_learn_from_text(gl, text);
            double t1 = now_ms();
            int total = gl_total_bindings_for(gl);
            printf("  %5d            %8.3f  (total_bindings=%d, avg %.0f/word)\n",
                   b_seeded, t1 - t0, total, (double)total / phase3_vocab);
            free(text);
        }
    }

    grounded_language_destroy(gl);
    semantic_memory_destroy(sm);
    snn_language_bridge_destroy(bridge);
    return 0;
}
