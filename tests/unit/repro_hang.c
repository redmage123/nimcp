/* Local reproduction of the pod resume hang.
 * Hypothesis: with semantic_memory NON-NULL, loading the full ~50K-word
 * WordNet bulk lexicon goes O(N^2) somewhere (find_or_create_concept scan,
 * KG event flood, or similar). With semantic_memory NULL it is O(1) per word.
 *
 * Build:
 *   gcc -O0 -g -I include tests/unit/repro_hang.c -L build/lib -lnimcp \
 *       -Wl,-rpath,build/lib -o /tmp/repro_hang
 * Run:
 *   /tmp/repro_hang 1   # semantic_memory NON-NULL (suspected hang)
 *   /tmp/repro_hang 0   # semantic_memory NULL (baseline, should be fast)
 */
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

extern int nimcp_internal_load_bulk_lexicon(grounded_language_t* gl,
                                            const char* bin_path);

static double now_s(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

int main(int argc, char** argv) {
    int with_sm = (argc > 1) ? atoi(argv[1]) : 1;
    const char* path = "data/lexicon/wordnet_glove_v1.bin";

    semantic_memory_system_t* sm = NULL;
    if (with_sm) {
        sm = semantic_memory_create();
        fprintf(stderr, "[repro] semantic_memory=%p\n", (void*)sm);
    } else {
        fprintf(stderr, "[repro] semantic_memory=NULL (baseline)\n");
    }

    grounded_language_t* gl = grounded_language_create(128, sm);
    if (!gl) { fprintf(stderr, "[repro] gl create FAILED\n"); return 2; }

    fprintf(stderr, "[repro] loading bulk lexicon %s ...\n", path);
    double t0 = now_s();
    int loaded = nimcp_internal_load_bulk_lexicon(gl, path);
    double dt = now_s() - t0;
    fprintf(stderr, "[repro] loaded=%d in %.2f s\n", loaded, dt);

    return 0;
}
