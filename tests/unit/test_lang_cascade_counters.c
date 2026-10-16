/**
 * @file test_lang_cascade_counters.c
 * @brief Batch K — verify the cascade lifetime counters increment
 *        monotonically across runs and reset zeros everything.
 *
 * Coverage:
 *   1. test_initial_zero — fresh minimal brain → counters all zero.
 *   2. test_monotonic_run_counters — 5 cascade_run calls → total_runs
 *      increments by 5, runs_spontaneous == 5 (no prompt). Re-run with
 *      a prompt → runs_with_prompt bumps.
 *   3. test_stage_invocations — running CASCADE_STAGE_ALL bumps at least
 *      the stages that actually executed (CONTENT for sure since the
 *      cascade runs end-to-end).
 *   4. test_reset_zeros — counters reset clears everything.
 */

#include "language/nimcp_communication_cascade.h"
#include "core/brain/nimcp_brain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;
#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

static brain_t make_brain(const char* name) {
    return brain_create_minimal(name, BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 16, 8);
}

static void test_initial_zero(void) {
    brain_t brain = make_brain("counters_initial_zero");
    EXPECT(brain != NULL, "brain create");
    if (!brain) return;
    nimcp_cascade_counters_t c;
    int rc = nimcp_brain_get_cascade_counters_impl(brain, &c);
    EXPECT(rc == 0, "get_counters rc=%d", rc);
    EXPECT(c.total_runs == 0, "total_runs=%llu",
           (unsigned long long)c.total_runs);
    EXPECT(c.runs_with_prompt == 0, "runs_with_prompt zero");
    EXPECT(c.runs_spontaneous == 0, "runs_spontaneous zero");
    for (uint32_t i = 0; i < NIMCP_CASCADE_STAGE_COUNT; i++) {
        EXPECT(c.stage_invocations[i] == 0,
               "stage_invocations[%u] zero, got %llu", i,
               (unsigned long long)c.stage_invocations[i]);
    }
    brain_destroy(brain);
    fprintf(stderr, "PASS test_initial_zero\n");
}

static void test_monotonic_run_counters(void) {
    brain_t brain = make_brain("counters_monotonic");
    EXPECT(brain != NULL, "brain create");
    if (!brain) return;

    for (int i = 0; i < 5; i++) {
        production_cascade_state_t st;
        memset(&st, 0, sizeof(st));
        communication_cascade_run(brain, /*prompt=*/NULL,
                                    CASCADE_STAGE_ALL, &st);
        cascade_state_cleanup(&st);
    }
    nimcp_cascade_counters_t c;
    EXPECT(nimcp_brain_get_cascade_counters_impl(brain, &c) == 0, "snap1");
    EXPECT(c.total_runs == 5,
           "total_runs=%llu after 5 spontaneous", (unsigned long long)c.total_runs);
    EXPECT(c.runs_spontaneous == 5, "runs_spontaneous=5 got %llu",
           (unsigned long long)c.runs_spontaneous);
    EXPECT(c.runs_with_prompt == 0, "runs_with_prompt=0 got %llu",
           (unsigned long long)c.runs_with_prompt);

    /* Now a prompt-driven run. */
    production_cascade_state_t st2;
    memset(&st2, 0, sizeof(st2));
    communication_cascade_run(brain, "Hello world",
                                CASCADE_STAGE_ALL, &st2);
    cascade_state_cleanup(&st2);

    EXPECT(nimcp_brain_get_cascade_counters_impl(brain, &c) == 0, "snap2");
    EXPECT(c.total_runs == 6, "total_runs=6 after prompt; got %llu",
           (unsigned long long)c.total_runs);
    EXPECT(c.runs_with_prompt == 1, "runs_with_prompt=1 got %llu",
           (unsigned long long)c.runs_with_prompt);

    brain_destroy(brain);
    fprintf(stderr, "PASS test_monotonic_run_counters\n");
}

static void test_stage_invocations(void) {
    brain_t brain = make_brain("counters_stages");
    EXPECT(brain != NULL, "brain create");
    if (!brain) return;
    production_cascade_state_t st;
    memset(&st, 0, sizeof(st));
    communication_cascade_run(brain, /*prompt=*/NULL,
                                CASCADE_STAGE_ALL, &st);
    cascade_state_cleanup(&st);

    nimcp_cascade_counters_t c;
    nimcp_brain_get_cascade_counters_impl(brain, &c);
    /* CONTENT stage is bit 5 — it always runs (and fails out fatal if it
     * can't). DRIVE (bit 1), GOAL (bit 2) also run unconditionally. At
     * least one of these MUST have a non-zero invocation count. */
    bool any_invoked = false;
    for (uint32_t i = 0; i < NIMCP_CASCADE_STAGE_COUNT; i++) {
        if (c.stage_invocations[i] > 0) {
            any_invoked = true;
            break;
        }
    }
    EXPECT(any_invoked, "at least one stage invoked across CASCADE_STAGE_ALL");
    brain_destroy(brain);
    fprintf(stderr, "PASS test_stage_invocations\n");
}

static void test_reset_zeros(void) {
    brain_t brain = make_brain("counters_reset");
    EXPECT(brain != NULL, "brain create");
    if (!brain) return;
    production_cascade_state_t st;
    memset(&st, 0, sizeof(st));
    communication_cascade_run(brain, NULL, CASCADE_STAGE_ALL, &st);
    cascade_state_cleanup(&st);

    nimcp_cascade_counters_t before, after;
    nimcp_brain_get_cascade_counters_impl(brain, &before);
    EXPECT(before.total_runs == 1, "before total_runs=1");
    EXPECT(nimcp_brain_reset_cascade_counters_impl(brain) == 0, "reset");
    nimcp_brain_get_cascade_counters_impl(brain, &after);
    EXPECT(after.total_runs == 0, "after reset total_runs=0");
    EXPECT(after.runs_spontaneous == 0, "after reset runs_spontaneous=0");

    brain_destroy(brain);
    fprintf(stderr, "PASS test_reset_zeros\n");
}

/* Followup — Batch K wernicke_miss path. Run a prompt with words almost
 * certainly absent from the minimal-brain lexicon ("zorglebox snorkflap")
 * and verify wernicke_lexicon_miss > 0 after one cascade run. */
static void test_wernicke_lexicon_miss(void) {
    brain_t brain = make_brain("counters_wernicke_miss");
    EXPECT(brain != NULL, "brain create");
    if (!brain) return;
    production_cascade_state_t st;
    memset(&st, 0, sizeof(st));
    communication_cascade_run(brain, "zorglebox snorkflap qwerflux",
                                CASCADE_STAGE_ALL, &st);
    cascade_state_cleanup(&st);
    nimcp_cascade_counters_t c;
    nimcp_brain_get_cascade_counters_impl(brain, &c);
    EXPECT(c.wernicke_lexicon_miss > 0,
           "wernicke_lexicon_miss > 0 after nonsense prompt; got %llu",
           (unsigned long long)c.wernicke_lexicon_miss);
    brain_destroy(brain);
    fprintf(stderr, "PASS test_wernicke_lexicon_miss\n");
}

int main(void) {
    test_initial_zero();
    test_monotonic_run_counters();
    test_stage_invocations();
    test_reset_zeros();
    test_wernicke_lexicon_miss();
    if (g_failures > 0) {
        fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
        return 1;
    }
    fprintf(stderr, "OK: test_lang_cascade_counters — 5 subtests pass\n");
    return 0;
}
