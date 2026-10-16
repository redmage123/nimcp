/* Does produce DISCRIMINATE between distinct intents with semantic_memory
 * NULL + the context_vector concept-feature fallback? If every intent yields
 * the same word, the collapse is NOT fixed. Build:
 *   gcc -O0 -g -I include tests/unit/repro_discriminate.c -L build/lib -lnimcp \
 *       -Wl,-rpath,build/lib -o /tmp/repro_discriminate
 */
#include "language/nimcp_grounded_language.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int nimcp_internal_load_bulk_lexicon(grounded_language_t* gl, const char* p);

int main(void) {
    grounded_language_t* gl = grounded_language_create(128, NULL); /* sm NULL */
    if (!gl) { printf("create FAIL\n"); return 2; }
    int n = nimcp_internal_load_bulk_lexicon(gl, "data/lexicon/wordnet_glove_v1.bin");
    fprintf(stderr, "loaded %d words (sm=NULL)\n", n);

    const char* probes[] = {"cat","dog","water","fire","king","love","run","red",
                            "house","music","money","death","tree","ocean"};
    int np = (int)(sizeof(probes)/sizeof(probes[0]));

    char outs[32][256];
    int got = 0;
    for (int i = 0; i < np; i++) {
        const gl_lexicon_entry_t* e = grounded_language_lookup(gl, probes[i]);
        if (!e || !e->context_initialized || !e->context_vector) {
            fprintf(stderr, "  %-8s : (not in lexicon / no ctx)\n", probes[i]);
            continue;
        }
        gl_production_result_t r; memset(&r, 0, sizeof(r));
        int rc = grounded_language_produce(gl, e->context_vector, 128,
                                           GL_PRODUCE_DESCRIBE, &r);
        const char* t = (rc == 0 && r.text) ? r.text : "(none)";
        snprintf(outs[got], sizeof(outs[got]), "%s", t);
        fprintf(stderr, "  intent=%-8s -> produce=\"%s\" (relev=%.2f)\n",
                probes[i], t, (rc==0)? r.relevance : -1.0f);
        got++;
    }

    /* Count distinct outputs */
    int distinct = 0;
    for (int i = 0; i < got; i++) {
        int seen = 0;
        for (int j = 0; j < i; j++) if (strcmp(outs[i], outs[j]) == 0) { seen = 1; break; }
        if (!seen) distinct++;
    }
    fprintf(stderr, "\nRESULT: %d/%d intents produced; %d DISTINCT outputs\n",
            got, np, distinct);
    fprintf(stderr, "%s\n", (distinct >= 3) ? "DISCRIMINATES (collapse not present)"
                                            : "COLLAPSED (outputs not distinct)");
    return 0;
}
