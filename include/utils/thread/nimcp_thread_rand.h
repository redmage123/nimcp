/**
 * @file nimcp_thread_rand.h
 * @brief Thread-safe random number generation.
 *
 * Provides nimcp_tl_rand() as a drop-in replacement for rand().
 * Uses thread-local storage with rand_r() for thread safety.
 */

#ifndef NIMCP_THREAD_RAND_H
#define NIMCP_THREAD_RAND_H

#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/**
 * Thread-safe replacement for rand().
 * Each thread gets its own seed, auto-initialized on first use.
 * @return Random unsigned int (same range as rand())
 */
static inline unsigned int nimcp_tl_rand(void) {
    static __thread unsigned int nimcp_tl_rand_seed = 0;
    if (nimcp_tl_rand_seed == 0) {
        nimcp_tl_rand_seed = (unsigned int)(uintptr_t)&nimcp_tl_rand_seed
                           ^ (unsigned int)time(NULL);
        if (nimcp_tl_rand_seed == 0) nimcp_tl_rand_seed = 1;
    }
    return (unsigned int)rand_r(&nimcp_tl_rand_seed);
}

/** Thread-safe RAND_MAX-normalized float in [0, 1] */
#define NIMCP_TL_RANDF() ((float)nimcp_tl_rand() / (float)RAND_MAX)

/** Thread-safe RAND_MAX-normalized double in [0, 1] */
#define NIMCP_TL_RANDD() ((double)nimcp_tl_rand() / (double)RAND_MAX)

#endif /* NIMCP_THREAD_RAND_H */
