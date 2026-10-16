/* Validate the 2026-05-16 source-side function-word guard.
 *
 * Seeds "the" (a GL_CLASS_FUNCTION word, set by seed_function_words in
 * grounded_language_create) with many concept bindings via fast_map.
 * Then calls learn_from_text on a sentence containing "the". The cross-bind
 * loop should NOT iterate "the"'s bindings — meaning crossbind_iters should
 * scale only with the content words present, not with B of "the".
 *
 * Before the fix: crossbind_iters ~= B(the) × (N-1).
 * After the fix:  crossbind_iters ~= sum over content words only.
 */
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

int main(void) {
    srand(42);

    semantic_memory_system_t* sm = semantic_memory_create();
    grounded_language_t* gl = grounded_language_create(32, sm);

    /* Seed "the" with 1000 distinct concept bindings. */
    float feat[32];
    for (int b = 0; b < 1000; b++) {
        for (int d = 0; d < 32; d++) {
            /* Larger perturbations so cosine-0.85 dedup doesn't collapse them. */
            feat[d] = (float)b * 0.31f + (float)d * 0.71f + (float)(rand() % 100) / 50.0f;
        }
        grounded_language_fast_map(gl, "the", feat, 32, 0);
    }

    /* Also seed "dog" and "cat" with some content bindings. */
    for (int b = 0; b < 50; b++) {
        for (int d = 0; d < 32; d++) {
            feat[d] = (float)b * 0.41f + (float)d * 0.13f + (float)(rand() % 100) / 100.0f;
        }
        grounded_language_fast_map(gl, "dog", feat, 32, 0);
        grounded_language_fast_map(gl, "cat", feat, 32, 0);
    }

    printf("=== Function-word guard validation ===\n");
    printf("Setup: 'the' seeded with up to 1000 bindings, 'dog'/'cat' with up to 50 each.\n\n");

    const char* test_inputs[] = {
        "the dog ran",
        "the cat sat on the dog",
        "the dog and the cat played with the ball near the house",
        NULL,
    };

    for (int i = 0; test_inputs[i]; i++) {
        printf("--- Input: \"%s\" ---\n", test_inputs[i]);
        double t0 = now_ms();
        grounded_language_learn_from_text(gl, test_inputs[i]);
        double t1 = now_ms();
        printf("(wallclock %.3f ms — check the [gl_profile] line above for crossbind_iters)\n\n", t1 - t0);
    }

    grounded_language_destroy(gl);
    semantic_memory_destroy(sm);
    return 0;
}
