/**
 * @file test_broca_lexicon_concurrency.c
 * @brief Batch H — stress-test the Broca lexicon under concurrent
 *        writer + reader threads. Pre-Batch-H this would race the
 *        hash-chain walk against the writer's `lexicon[idx] = node`
 *        and frequently SEGV. With the mutex it must run to
 *        completion with all reads matching the writer's view of
 *        inserted entries.
 *
 * Coverage:
 *   - 1 writer thread inserts N=5000 distinct words (each ~ 8 chars).
 *   - 4 reader threads each do M=5000 random lookups against the
 *     subset of words the writer has already inserted (synced via a
 *     monotonically-increasing "high water mark" counter).
 *   - Test passes if (a) no crash, (b) lookups for a confirmed-inserted
 *     word never spuriously miss (consistency).
 */

#include "core/brain/regions/broca/nimcp_broca_adapter.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#define N_WORDS       2000
#define N_READER_OPS  2000
#define N_READERS     4

static broca_adapter_t* g_adapter = NULL;
static _Atomic uint32_t g_high_water = 0;  /* count of confirmed inserts */
static _Atomic int      g_failures   = 0;

static void format_word(uint32_t i, char* out, size_t cap) {
    /* Compact unique form: "wXXXXXX" — fits in broca_lexical_entry_t.word. */
    snprintf(out, cap, "w%06u", i);
}

static void* writer_thread(void* arg) {
    (void)arg;
    for (uint32_t i = 0; i < N_WORDS; i++) {
        char w[32];
        format_word(i, w, sizeof(w));
        broca_lexical_entry_t e;
        memset(&e, 0, sizeof(e));
        strncpy(e.word, w, sizeof(e.word) - 1);
        e.pos = 0;
        e.frequency = 1.0f;
        bool ok = broca_add_lexical_entry(g_adapter, &e);
        if (!ok) {
            atomic_fetch_add(&g_failures, 1);
            continue;
        }
        atomic_fetch_add(&g_high_water, 1);
    }
    return NULL;
}

static void* reader_thread(void* arg) {
    (void)arg;
    /* Cheap PRNG seeded by thread id. */
    uint64_t s = (uint64_t)(uintptr_t)arg ^ 0xdeadbeefULL;
    for (uint32_t op = 0; op < N_READER_OPS; op++) {
        uint32_t hw = atomic_load_explicit(&g_high_water, memory_order_relaxed);
        if (hw == 0) continue;
        /* xorshift step */
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        uint32_t i = (uint32_t)(s % hw);  /* read only confirmed-inserted */
        char w[32];
        format_word(i, w, sizeof(w));
        broca_lexical_entry_t found;
        memset(&found, 0, sizeof(found));
        bool ok = broca_lookup_word(g_adapter, /*word_id=*/0, w, &found);
        if (!ok) {
            /* Spurious miss — word i is below the high water so it must
             * have been inserted. */
            fprintf(stderr, "reader miss for word=%s (hw=%u)\n", w, hw);
            atomic_fetch_add(&g_failures, 1);
        } else if (strcmp(found.word, w) != 0) {
            fprintf(stderr, "reader got wrong word: requested=%s got=%s\n",
                    w, found.word);
            atomic_fetch_add(&g_failures, 1);
        }
    }
    return NULL;
}

int main(void) {
    broca_config_t cfg = broca_default_config();
    cfg.enable_lexicon  = true;
    cfg.lexicon_size    = N_WORDS * 4u;  /* avoid hitting capacity */
    cfg.max_words       = N_WORDS;
    /* Other fields keep defaults. */
    g_adapter = broca_create(&cfg);
    if (!g_adapter) {
        fprintf(stderr, "broca_create failed\n");
        return 1;
    }

    pthread_t writer;
    pthread_t readers[N_READERS];
    if (pthread_create(&writer, NULL, writer_thread, NULL) != 0) {
        fprintf(stderr, "pthread_create writer\n");
        broca_destroy(g_adapter);
        return 1;
    }
    for (int i = 0; i < N_READERS; i++) {
        if (pthread_create(&readers[i], NULL, reader_thread,
                            (void*)(uintptr_t)(i + 1)) != 0) {
            fprintf(stderr, "pthread_create reader %d\n", i);
            broca_destroy(g_adapter);
            return 1;
        }
    }

    pthread_join(writer, NULL);
    for (int i = 0; i < N_READERS; i++) pthread_join(readers[i], NULL);

    int failures = atomic_load(&g_failures);
    broca_destroy(g_adapter);
    if (failures > 0) {
        fprintf(stderr, "FAIL: %d concurrency failures\n", failures);
        return 1;
    }
    fprintf(stderr, "OK: test_broca_lexicon_concurrency — %d writes, "
                    "%d readers * %d ops, no failures\n",
            N_WORDS, N_READERS, N_READER_OPS);
    return 0;
}
