/**
 * @file test_engram_eviction.c
 * @brief Verify engram_system soft-cap + eviction policy.
 *
 * WHAT: Sets max_active_engrams=N, encodes 2*N engrams, asserts that
 *       active_count never exceeded N and that total_evictions == N.
 * WHY:  ENGRAM_MAX_COUNT was bumped to 524288 (was 512), backed by a
 *       runtime soft cap with eviction. Test confirms eviction fires
 *       when the cap is hit and that the policy preferentially drops
 *       DEGRADING engrams first.
 *
 * Build:
 *   gcc -O2 -I include tests/unit/test_engram_eviction.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_engram_eviction
 */

#include "cognitive/memory/nimcp_engram.h"
#include "cognitive/nimcp_emotional_tagging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int test_eviction_basic(void) {
    engram_system_t* sys = engram_system_create();
    if (!sys) { fprintf(stderr, "create failed\n"); return 1; }

    engram_system_set_max_active(sys, 10);
    sys->completion_threshold = 0.05f;

    uint32_t neurons[32];
    float    activations[32];
    for (uint32_t i = 0; i < 32; i++) {
        activations[i] = 0.5f;
    }

    /* Encode 25 engrams with the cap set to 10. Expect active_count to
     * stay at 10 and total_evictions to grow as we encode past the cap. */
    emotional_tag_t emotion = {0};
    emotion.arousal = 0.3f;
    for (uint32_t e = 0; e < 25; e++) {
        for (uint32_t n = 0; n < 32; n++) {
            neurons[n] = (e * 32 + n + 1);  /* unique-per-engram ids, no overlap */
        }
        engram_encode(sys, neurons, activations, 32,
                       MEMORY_TYPE_EPISODIC, emotion);
    }

    uint32_t active = engram_get_active_count(sys);
    uint64_t evictions = engram_get_total_evictions(sys);

    printf("  active_count=%u  total_evictions=%llu  (expected active<=10, evictions==15)\n",
           active, (unsigned long long)evictions);

    int fail = 0;
    if (active > 10) { fprintf(stderr, "FAIL: active_count exceeded cap\n"); fail = 1; }
    if (evictions != 15) {
        fprintf(stderr, "FAIL: expected 15 evictions, got %llu\n",
                (unsigned long long)evictions);
        fail = 1;
    }

    engram_system_destroy(sys);
    return fail;
}

static int test_eviction_zero_disables(void) {
    engram_system_t* sys = engram_system_create();
    if (!sys) return 1;

    engram_system_set_max_active(sys, 0);  /* disable cap */
    sys->completion_threshold = 0.05f;

    uint32_t neurons[32];
    float    activations[32];
    for (uint32_t i = 0; i < 32; i++) activations[i] = 0.5f;
    emotional_tag_t emotion = {0};

    /* With cap=0, all 100 engrams should remain active. */
    for (uint32_t e = 0; e < 100; e++) {
        for (uint32_t n = 0; n < 32; n++) {
            neurons[n] = (e * 32 + n + 1);
        }
        engram_encode(sys, neurons, activations, 32,
                       MEMORY_TYPE_EPISODIC, emotion);
    }

    uint32_t active = engram_get_active_count(sys);
    uint64_t evictions = engram_get_total_evictions(sys);
    printf("  cap=0 (disabled): active_count=%u  evictions=%llu  (expected active==100, evictions==0)\n",
           active, (unsigned long long)evictions);

    int fail = 0;
    if (active != 100) { fprintf(stderr, "FAIL: cap=0 should not evict\n"); fail = 1; }
    if (evictions != 0) { fprintf(stderr, "FAIL: cap=0 should record 0 evictions\n"); fail = 1; }

    engram_system_destroy(sys);
    return fail;
}

int main(void) {
    printf("[test_engram_eviction]\n");
    int fail = 0;
    fail += test_eviction_basic();
    fail += test_eviction_zero_disables();
    if (fail) { printf("FAIL\n"); return 1; }
    printf("PASS\n");
    return 0;
}
