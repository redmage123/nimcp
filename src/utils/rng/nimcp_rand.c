/**
 * @file nimcp_rand.c
 * @brief Unified Random Number Generation Module for NIMCP - Implementation
 *
 * WHAT: Thread-safe RNG with multiple backends and quantum integration
 * WHY:  Centralized, high-quality random number generation
 * HOW:  Thread-local storage, LCG/Xorshift128+, Box-Muller, Voss-McCartney
 *
 * @author NIMCP Development Team
 * @date 2026-01-15
 */

#include "utils/rng/nimcp_rand.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/quantum/nimcp_quantum_monte_carlo.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

/*=============================================================================
 * MODULE IDENTIFICATION
 *===========================================================================*/

#define LOG_MODULE_NAME "rand"

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(rand)

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** LCG multiplier (MMIX by Knuth) */
#define LCG_MULTIPLIER 6364136223846793005ULL

/** LCG increment (MMIX by Knuth) */
#define LCG_INCREMENT  1442695040888963407ULL

/** Float conversion factor (2^-32) */
#define FLOAT_SCALE (1.0f / 4294967296.0f)

/** Double conversion factor (2^-53) */
#define DOUBLE_SCALE (1.0 / 9007199254740992.0)

/** Pi constant for Box-Muller */

/** Pink noise normalization factor for 16 octaves */
#define PINK_NORM_FACTOR (1.0f / 16.0f)

/*=============================================================================
 * THREAD-LOCAL STATE STRUCTURE
 *===========================================================================*/

/**
 * @brief Thread-local RNG state
 */
typedef struct {
    /* LCG state */
    uint64_t lcg_state;

    /* Xorshift128+ state */
    uint64_t xorshift_s0;
    uint64_t xorshift_s1;

    /* Box-Muller cache */
    float bm_cache;
    bool bm_has_cached;

    /* Pink noise state (Voss-McCartney) */
    float pink_octaves[NIMCP_RAND_PINK_OCTAVES];
    uint32_t pink_counter;
    float pink_running_sum;

    /* Current seed (for reproducibility) */
    uint64_t seed;

    /* Backend selection */
    nimcp_rand_backend_t backend;

    /* Initialization flag */
    bool initialized;
} tls_rand_state_t;

/*=============================================================================
 * THREAD-LOCAL STORAGE
 *===========================================================================*/

static _Thread_local tls_rand_state_t g_tls_state = {0};

/*=============================================================================
 * GLOBAL STATE
 *===========================================================================*/

/** Global configuration */
static nimcp_rand_config_t g_config;

/** Initialization flag */
static nimcp_atomic_bool_t g_initialized = {0};

/** Global statistics (atomic counters) */
static struct {
    nimcp_atomic_uint64_t uniform_calls;
    nimcp_atomic_uint64_t normal_calls;
    nimcp_atomic_uint64_t int_calls;
    nimcp_atomic_uint64_t pink_calls;
    nimcp_atomic_uint64_t quantum_calls;
    nimcp_atomic_uint64_t context_creates;
    nimcp_atomic_uint64_t context_destroys;
    nimcp_atomic_uint64_t seed_reseeds;
} g_stats;

/** UMM manager for context allocations */
static unified_mem_manager_t g_umm_manager = NULL;

/*=============================================================================
 * CONTEXT STRUCTURE (OPAQUE)
 *===========================================================================*/

struct nimcp_rand_ctx {
    /* LCG state */
    uint64_t lcg_state;

    /* Xorshift128+ state */
    uint64_t xorshift_s0;
    uint64_t xorshift_s1;

    /* Box-Muller cache */
    float bm_cache;
    bool bm_has_cached;

    /* Pink noise state */
    float pink_octaves[NIMCP_RAND_PINK_OCTAVES];
    uint32_t pink_counter;
    float pink_running_sum;

    /* Seed */
    uint64_t seed;

    /* Backend */
    nimcp_rand_backend_t backend;

    /* Magic for validation */
    uint32_t magic;
};

#define CTX_MAGIC 0x524E4743  /* 'RNGC' */

/*=============================================================================
 * INTERNAL HELPER: SEED MIXING
 *===========================================================================*/

/**
 * @brief Mix seed with SplitMix64 for better entropy
 */
static inline uint64_t splitmix64(uint64_t* state)
{
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/*=============================================================================
 * INTERNAL HELPER: LCG NEXT
 *===========================================================================*/

static inline uint64_t lcg_next(uint64_t* state)
{
    *state = (*state) * LCG_MULTIPLIER + LCG_INCREMENT;
    return *state;
}

/*=============================================================================
 * INTERNAL HELPER: XORSHIFT128+ NEXT
 *===========================================================================*/

static inline uint64_t xorshift128plus_next(uint64_t* s0, uint64_t* s1)
{
    uint64_t x = *s0;
    uint64_t y = *s1;
    *s0 = y;
    x ^= x << 23;
    *s1 = x ^ y ^ (x >> 17) ^ (y >> 26);
    return *s1 + y;
}

/*=============================================================================
 * INTERNAL HELPER: THREAD-LOCAL INITIALIZATION
 *===========================================================================*/

static void tls_ensure_initialized(void)
{
    if (g_tls_state.initialized) {
        return;
    }

    /* Generate entropy-based seed for this thread */
    uint64_t seed = nimcp_rand_entropy_seed();

    /* Initialize LCG */
    g_tls_state.lcg_state = seed;

    /* Initialize Xorshift128+ with mixed seed */
    uint64_t mix = seed;
    g_tls_state.xorshift_s0 = splitmix64(&mix);
    g_tls_state.xorshift_s1 = splitmix64(&mix);

    /* Clear Box-Muller cache */
    g_tls_state.bm_cache = 0.0f;
    g_tls_state.bm_has_cached = false;

    /* Initialize pink noise state */
    g_tls_state.pink_counter = 0;
    g_tls_state.pink_running_sum = 0.0f;
    for (int i = 0; i < NIMCP_RAND_PINK_OCTAVES; i++) {
        /* Pre-fill octaves with random values */
        /* BUG FIX: Must shift right by 32 bits to get a 32-bit value before scaling */
        float val = ((float)(lcg_next(&g_tls_state.lcg_state) >> 32) * FLOAT_SCALE * 2.0f) - 1.0f;
        g_tls_state.pink_octaves[i] = val;
        g_tls_state.pink_running_sum += val;
    }

    g_tls_state.seed = seed;
    g_tls_state.backend = NIMCP_RAND_BACKEND_XORSHIFT;
    g_tls_state.initialized = true;
}

/*=============================================================================
 * INTERNAL HELPER: RAW 64-BIT GENERATION
 *===========================================================================*/

static inline uint64_t tls_next_u64(void)
{
    tls_ensure_initialized();

    switch (g_tls_state.backend) {
        case NIMCP_RAND_BACKEND_LCG:
            return lcg_next(&g_tls_state.lcg_state);

        case NIMCP_RAND_BACKEND_XORSHIFT:
        default:
            return xorshift128plus_next(&g_tls_state.xorshift_s0,
                                        &g_tls_state.xorshift_s1);
    }
}

/*=============================================================================
 * INTERNAL HELPER: CONTEXT RAW 64-BIT GENERATION
 *===========================================================================*/

static inline uint64_t ctx_next_u64(nimcp_rand_ctx_t* ctx)
{
    switch (ctx->backend) {
        case NIMCP_RAND_BACKEND_LCG:
            return lcg_next(&ctx->lcg_state);

        case NIMCP_RAND_BACKEND_XORSHIFT:
        default:
            return xorshift128plus_next(&ctx->xorshift_s0, &ctx->xorshift_s1);
    }
}

/*=============================================================================
 * INTERNAL HELPER: PINK NOISE UPDATE (VOSS-MCCARTNEY)
 *===========================================================================*/

/**
 * @brief Update pink noise state using Voss-McCartney algorithm
 *
 * The algorithm works by:
 * 1. For each sample, check which octaves need updating based on counter
 * 2. Use trailing zeros of counter to determine which octave to update
 * 3. Update that octave with a new white noise sample
 * 4. Sum all octaves to get pink noise
 */
static float pink_next_tls(void)
{
    tls_ensure_initialized();

    uint32_t counter = g_tls_state.pink_counter++;

    /* Find the lowest set bit (which octave to update) */
    /* Using CTZ (count trailing zeros) */
    int octave = 0;
    if (counter != 0) {
        uint32_t temp = counter;
        while ((temp & 1) == 0 && octave < NIMCP_RAND_PINK_OCTAVES) {
            temp >>= 1;
            octave++;
        }
    }

    if (octave < NIMCP_RAND_PINK_OCTAVES) {
        /* Subtract old value from running sum */
        g_tls_state.pink_running_sum -= g_tls_state.pink_octaves[octave];

        /* Generate new white noise value in [-1, 1] */
        float new_val = ((float)(tls_next_u64() >> 32) * FLOAT_SCALE * 2.0f) - 1.0f;

        /* Store and add to running sum */
        g_tls_state.pink_octaves[octave] = new_val;
        g_tls_state.pink_running_sum += new_val;
    }

    /* Add white noise component for high frequency content */
    float white = ((float)(tls_next_u64() >> 32) * FLOAT_SCALE * 2.0f) - 1.0f;

    /* Combine and normalize */
    return (g_tls_state.pink_running_sum + white) * PINK_NORM_FACTOR;
}

static float pink_next_ctx(nimcp_rand_ctx_t* ctx)
{
    uint32_t counter = ctx->pink_counter++;

    int octave = 0;
    if (counter != 0) {
        uint32_t temp = counter;
        while ((temp & 1) == 0 && octave < NIMCP_RAND_PINK_OCTAVES) {
            temp >>= 1;
            octave++;
        }
    }

    if (octave < NIMCP_RAND_PINK_OCTAVES) {
        ctx->pink_running_sum -= ctx->pink_octaves[octave];
        float new_val = ((float)(ctx_next_u64(ctx) >> 32) * FLOAT_SCALE * 2.0f) - 1.0f;
        ctx->pink_octaves[octave] = new_val;
        ctx->pink_running_sum += new_val;
    }

    float white = ((float)(ctx_next_u64(ctx) >> 32) * FLOAT_SCALE * 2.0f) - 1.0f;
    return (ctx->pink_running_sum + white) * PINK_NORM_FACTOR;
}

/*=============================================================================
 * GLOBAL INITIALIZATION
 *===========================================================================*/

nimcp_rand_config_t nimcp_rand_default_config(void)
{
    nimcp_rand_config_t config = {
        .default_backend = NIMCP_RAND_BACKEND_XORSHIFT,
        .global_seed = NIMCP_RAND_DEFAULT_SEED,
        .enable_quantum = false,
        .enable_csprng = false,
        .thread_local_seeding = true,
        .quantum_walk_nodes = 256,
        .quantum_decoherence = 0.01f
    };
    return config;
}

nimcp_rand_result_t nimcp_rand_init(const nimcp_rand_config_t* config)
{
    /* Check if already initialized */
    if (nimcp_atomic_load_bool(&g_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        LOG_MODULE_WARN(LOG_MODULE_NAME, "Already initialized");
        return NIMCP_RAND_OK;
    }

    /* Use default config if none provided */
    if (config) {
        g_config = *config;
    } else {
        g_config = nimcp_rand_default_config();
    }

    /* Initialize atomic statistics */
    nimcp_atomic_init_u64(&g_stats.uniform_calls, 0);
    nimcp_atomic_init_u64(&g_stats.normal_calls, 0);
    nimcp_atomic_init_u64(&g_stats.int_calls, 0);
    nimcp_atomic_init_u64(&g_stats.pink_calls, 0);
    nimcp_atomic_init_u64(&g_stats.quantum_calls, 0);
    nimcp_atomic_init_u64(&g_stats.context_creates, 0);
    nimcp_atomic_init_u64(&g_stats.context_destroys, 0);
    nimcp_atomic_init_u64(&g_stats.seed_reseeds, 0);

    /* Create UMM manager for context allocations */
    unified_mem_config_t umm_config = unified_mem_default_config();
    umm_config.enable_cow = false;  /* No CoW needed for RNG contexts */
    umm_config.enable_tracking = true;
    g_umm_manager = unified_mem_create(&umm_config);

    if (!g_umm_manager) {
        LOG_MODULE_ERROR(LOG_MODULE_NAME, "Failed to create UMM manager");
        /* Continue without UMM - will use malloc fallback */
    }

    /* Mark as initialized */
    nimcp_atomic_store_bool(&g_initialized, true, NIMCP_MEMORY_ORDER_RELEASE);

    LOG_MODULE_INFO(LOG_MODULE_NAME, "Initialized with backend=%s, quantum=%s",
                    nimcp_rand_backend_name(g_config.default_backend),
                    g_config.enable_quantum ? "enabled" : "disabled");

    return NIMCP_RAND_OK;
}

void nimcp_rand_shutdown(void)
{
    if (!nimcp_atomic_load_bool(&g_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        return;
    }

    /* Destroy UMM manager */
    if (g_umm_manager) {
        unified_mem_destroy(g_umm_manager);
        g_umm_manager = NULL;
    }

    /* Mark as uninitialized */
    nimcp_atomic_store_bool(&g_initialized, false, NIMCP_MEMORY_ORDER_RELEASE);

    /* Reset thread-local state */
    g_tls_state.initialized = false;

    LOG_MODULE_INFO(LOG_MODULE_NAME, "Shutdown complete");
}

bool nimcp_rand_is_initialized(void)
{
    return nimcp_atomic_load_bool(&g_initialized, NIMCP_MEMORY_ORDER_ACQUIRE);
}

/*=============================================================================
 * THREAD-LOCAL RNG (FAST PATH)
 *===========================================================================*/

float nimcp_rand_uniform(void)
{
    nimcp_atomic_fetch_add_u64(&g_stats.uniform_calls, 1, NIMCP_MEMORY_ORDER_RELAXED);

    uint64_t bits = tls_next_u64();
    return (float)(bits >> 32) * FLOAT_SCALE;
}

double nimcp_rand_uniform_double(void)
{
    nimcp_atomic_fetch_add_u64(&g_stats.uniform_calls, 1, NIMCP_MEMORY_ORDER_RELAXED);

    uint64_t bits = tls_next_u64();
    /* Use upper 53 bits for double precision */
    return (double)(bits >> 11) * DOUBLE_SCALE;
}

int32_t nimcp_rand_int(int32_t max)
{
    if (max <= 0) {
        return 0;
    }

    nimcp_atomic_fetch_add_u64(&g_stats.int_calls, 1, NIMCP_MEMORY_ORDER_RELAXED);

    /* Rejection sampling for unbiased results */
    uint32_t threshold = (uint32_t)(-(int32_t)max) % (uint32_t)max;
    uint32_t r;

    do {
        r = (uint32_t)(tls_next_u64() >> 32);
    } while (r < threshold);

    return (int32_t)(r % (uint32_t)max);
}

int32_t nimcp_rand_range(int32_t min, int32_t max)
{
    if (min >= max) {
        return min;
    }

    int32_t range = max - min + 1;
    return min + nimcp_rand_int(range);
}

uint32_t nimcp_rand_uint(uint32_t max)
{
    if (max == 0) {
        return 0;
    }

    nimcp_atomic_fetch_add_u64(&g_stats.int_calls, 1, NIMCP_MEMORY_ORDER_RELAXED);

    uint32_t threshold = (-max) % max;
    uint32_t r;

    do {
        r = (uint32_t)(tls_next_u64() >> 32);
    } while (r < threshold);

    return r % max;
}

float nimcp_rand_normal(float mean, float stddev)
{
    nimcp_atomic_fetch_add_u64(&g_stats.normal_calls, 1, NIMCP_MEMORY_ORDER_RELAXED);

    tls_ensure_initialized();

    /* Box-Muller transform with caching */
    if (g_tls_state.bm_has_cached) {
        g_tls_state.bm_has_cached = false;
        return mean + stddev * g_tls_state.bm_cache;
    }

    float u1, u2, s;

    /* Marsaglia polar method for better numerical stability */
    do {
        u1 = 2.0f * nimcp_rand_uniform() - 1.0f;
        u2 = 2.0f * nimcp_rand_uniform() - 1.0f;
        s = u1 * u1 + u2 * u2;
    } while (s >= 1.0f || s == 0.0f);

    float mul = sqrtf(-2.0f * logf(s) / s);

    /* Cache second value */
    g_tls_state.bm_cache = u2 * mul;
    g_tls_state.bm_has_cached = true;

    return mean + stddev * (u1 * mul);
}

float nimcp_rand_exponential(float rate)
{
    if (rate <= 0.0f) {
        return 0.0f;
    }

    /* Inverse transform sampling: -ln(1-U)/rate */
    float u = nimcp_rand_uniform();
    /* Avoid log(0) by using 1-U where U in [0,1) gives 1-U in (0,1] */
    return -logf(1.0f - u) / rate;
}

float nimcp_rand_pink(void)
{
    nimcp_atomic_fetch_add_u64(&g_stats.pink_calls, 1, NIMCP_MEMORY_ORDER_RELAXED);
    return pink_next_tls();
}

nimcp_rand_result_t nimcp_rand_bytes(uint8_t* buffer, size_t len)
{
    if (!buffer) {
        return NIMCP_RAND_ERROR_NULL;
    }

    if (len == 0) {
        return NIMCP_RAND_OK;
    }

    tls_ensure_initialized();

    /* Generate bytes 8 at a time */
    size_t full_words = len / 8;
    for (size_t i = 0; i < full_words; i++) {
        uint64_t val = tls_next_u64();
        memcpy(buffer + i * 8, &val, 8);
    }

    /* Handle remaining bytes */
    size_t remaining = len % 8;
    if (remaining > 0) {
        uint64_t val = tls_next_u64();
        memcpy(buffer + full_words * 8, &val, remaining);
    }

    return NIMCP_RAND_OK;
}

/*=============================================================================
 * SEEDING
 *===========================================================================*/

void nimcp_rand_seed(uint64_t seed)
{
    nimcp_atomic_fetch_add_u64(&g_stats.seed_reseeds, 1, NIMCP_MEMORY_ORDER_RELAXED);

    /* Use entropy if seed is 0 */
    if (seed == 0) {
        seed = nimcp_rand_entropy_seed();
    }

    /* Reset LCG */
    g_tls_state.lcg_state = seed;

    /* Reset Xorshift128+ */
    uint64_t mix = seed;
    g_tls_state.xorshift_s0 = splitmix64(&mix);
    g_tls_state.xorshift_s1 = splitmix64(&mix);

    /* Clear Box-Muller cache */
    g_tls_state.bm_has_cached = false;

    /* Set backend and mark initialized BEFORE generating pink noise values */
    /* BUG FIX: Must set initialized=true and backend before calling any RNG */
    /* Otherwise tls_next_u64() will call tls_ensure_initialized() which */
    /* regenerates an entropy-based seed, destroying reproducibility */
    g_tls_state.seed = seed;
    g_tls_state.backend = NIMCP_RAND_BACKEND_XORSHIFT;
    g_tls_state.initialized = true;

    /* Reset pink noise state using the seeded generator */
    g_tls_state.pink_counter = 0;
    g_tls_state.pink_running_sum = 0.0f;
    for (int i = 0; i < NIMCP_RAND_PINK_OCTAVES; i++) {
        /* Use xorshift128plus_next directly to use the newly seeded state */
        uint64_t raw = xorshift128plus_next(&g_tls_state.xorshift_s0, &g_tls_state.xorshift_s1);
        float val = ((float)(raw >> 32) * FLOAT_SCALE * 2.0f) - 1.0f;
        g_tls_state.pink_octaves[i] = val;
        g_tls_state.pink_running_sum += val;
    }

    LOG_MODULE_DEBUG(LOG_MODULE_NAME, "Thread reseeded with seed=%lu", (unsigned long)seed);
}

uint64_t nimcp_rand_get_seed(void)
{
    tls_ensure_initialized();
    return g_tls_state.seed;
}

uint64_t nimcp_rand_entropy_seed(void)
{
    uint64_t seed = 0;

    /* Try /dev/urandom first */
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t bytes_read = read(fd, &seed, sizeof(seed));
        close(fd);
        if (bytes_read == sizeof(seed)) {
            return seed;
        }
    }

    /* Fallback: combine multiple entropy sources */
    struct timeval tv;
    gettimeofday(&tv, NULL);

    seed = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;

    /* Mix in address of local variable (ASLR entropy) */
    seed ^= (uintptr_t)&seed;

    /* Mix in clock_gettime if available */
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        seed ^= ((uint64_t)ts.tv_sec << 32) | (uint64_t)ts.tv_nsec;
    }
#endif

    /* Mix with SplitMix64 for better distribution */
    return splitmix64(&seed);
}

/*=============================================================================
 * CONTEXT-BASED RNG
 *===========================================================================*/

nimcp_rand_ctx_t* nimcp_rand_ctx_create(uint64_t seed)
{
    return nimcp_rand_ctx_create_with_backend(seed, g_config.default_backend);
}

nimcp_rand_ctx_t* nimcp_rand_ctx_create_with_backend(
    uint64_t seed,
    nimcp_rand_backend_t backend)
{
    nimcp_atomic_fetch_add_u64(&g_stats.context_creates, 1, NIMCP_MEMORY_ORDER_RELAXED);

    /*
     * BUG FIX: Use malloc consistently for context allocation.
     * Previous code tried UMM allocation but didn't track the handle,
     * causing double-free when nimcp_rand_ctx_destroy() called nimcp_free()
     * on UMM-allocated memory.
     *
     * Note: If UMM context allocation is needed in the future, we need to
     * add a unified_mem_handle_t field to nimcp_rand_ctx and use
     * unified_mem_free(handle) in destroy.
     */
    nimcp_rand_ctx_t* ctx = (nimcp_rand_ctx_t*)nimcp_malloc(sizeof(nimcp_rand_ctx_t));
    if (!ctx) {
        LOG_MODULE_ERROR(LOG_MODULE_NAME, "Failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_rand_ctx_create_with_backend: ctx is NULL");
        return NULL;
    }

    /* Use entropy if seed is 0 */
    if (seed == 0) {
        seed = nimcp_rand_entropy_seed();
    }

    /* Initialize backend FIRST - ctx_next_u64() needs this before pink noise init */
    /* BUG FIX: Previously backend was set AFTER pink noise init, causing */
    /* undefined behavior when malloc returned uninitialized memory */
    ctx->backend = backend;
    ctx->seed = seed;
    ctx->magic = CTX_MAGIC;

    /* Initialize state */
    ctx->lcg_state = seed;

    uint64_t mix = seed;
    ctx->xorshift_s0 = splitmix64(&mix);
    ctx->xorshift_s1 = splitmix64(&mix);

    ctx->bm_cache = 0.0f;
    ctx->bm_has_cached = false;

    ctx->pink_counter = 0;
    ctx->pink_running_sum = 0.0f;

    /* Initialize pink noise octaves (ctx->backend must be set first!) */
    for (int i = 0; i < NIMCP_RAND_PINK_OCTAVES; i++) {
        float val = ((float)(ctx_next_u64(ctx) >> 32) * FLOAT_SCALE * 2.0f) - 1.0f;
        ctx->pink_octaves[i] = val;
        ctx->pink_running_sum += val;
    }

    LOG_MODULE_DEBUG(LOG_MODULE_NAME, "Created context with seed=%lu, backend=%s",
                     (unsigned long)seed, nimcp_rand_backend_name(backend));

    return ctx;
}

void nimcp_rand_ctx_destroy(nimcp_rand_ctx_t* ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->magic != CTX_MAGIC) {
        LOG_MODULE_ERROR(LOG_MODULE_NAME, "Invalid context magic");
        return;
    }

    nimcp_atomic_fetch_add_u64(&g_stats.context_destroys, 1, NIMCP_MEMORY_ORDER_RELAXED);

    /* Invalidate magic before freeing */
    ctx->magic = 0;

    /* Context is always allocated via malloc (see nimcp_rand_ctx_create_with_backend) */
    nimcp_free(ctx);
}

nimcp_rand_ctx_t* nimcp_rand_ctx_clone(const nimcp_rand_ctx_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    if (ctx->magic != CTX_MAGIC) {
        LOG_MODULE_ERROR(LOG_MODULE_NAME, "Invalid context magic");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rand_ctx_clone: validation failed");
        return NULL;
    }

    nimcp_rand_ctx_t* clone = (nimcp_rand_ctx_t*)nimcp_malloc(sizeof(nimcp_rand_ctx_t));
    if (!clone) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "clone is NULL");

        return NULL;
    }

    memcpy(clone, ctx, sizeof(nimcp_rand_ctx_t));

    nimcp_atomic_fetch_add_u64(&g_stats.context_creates, 1, NIMCP_MEMORY_ORDER_RELAXED);

    return clone;
}

void nimcp_rand_ctx_seed(nimcp_rand_ctx_t* ctx, uint64_t seed)
{
    if (!ctx || ctx->magic != CTX_MAGIC) {
        return;
    }

    if (seed == 0) {
        seed = nimcp_rand_entropy_seed();
    }

    ctx->lcg_state = seed;

    uint64_t mix = seed;
    ctx->xorshift_s0 = splitmix64(&mix);
    ctx->xorshift_s1 = splitmix64(&mix);

    ctx->bm_has_cached = false;

    ctx->pink_counter = 0;
    ctx->pink_running_sum = 0.0f;
    for (int i = 0; i < NIMCP_RAND_PINK_OCTAVES; i++) {
        float val = ((float)(ctx_next_u64(ctx) >> 32) * FLOAT_SCALE * 2.0f) - 1.0f;
        ctx->pink_octaves[i] = val;
        ctx->pink_running_sum += val;
    }

    ctx->seed = seed;

    nimcp_atomic_fetch_add_u64(&g_stats.seed_reseeds, 1, NIMCP_MEMORY_ORDER_RELAXED);
}

/*=============================================================================
 * CONTEXT-BASED SAMPLING FUNCTIONS
 *===========================================================================*/

float nimcp_rand_ctx_uniform(nimcp_rand_ctx_t* ctx)
{
    if (!ctx || ctx->magic != CTX_MAGIC) {
        return 0.0f;
    }

    uint64_t bits = ctx_next_u64(ctx);
    return (float)(bits >> 32) * FLOAT_SCALE;
}

double nimcp_rand_ctx_uniform_double(nimcp_rand_ctx_t* ctx)
{
    if (!ctx || ctx->magic != CTX_MAGIC) {
        return 0.0;
    }

    uint64_t bits = ctx_next_u64(ctx);
    return (double)(bits >> 11) * DOUBLE_SCALE;
}

int32_t nimcp_rand_ctx_int(nimcp_rand_ctx_t* ctx, int32_t max)
{
    if (!ctx || ctx->magic != CTX_MAGIC || max <= 0) {
        return 0;
    }

    uint32_t threshold = (uint32_t)(-(int32_t)max) % (uint32_t)max;
    uint32_t r;

    do {
        r = (uint32_t)(ctx_next_u64(ctx) >> 32);
    } while (r < threshold);

    return (int32_t)(r % (uint32_t)max);
}

int32_t nimcp_rand_ctx_range(nimcp_rand_ctx_t* ctx, int32_t min, int32_t max)
{
    if (!ctx || ctx->magic != CTX_MAGIC || min >= max) {
        return min;
    }

    int32_t range = max - min + 1;
    return min + nimcp_rand_ctx_int(ctx, range);
}

uint32_t nimcp_rand_ctx_uint(nimcp_rand_ctx_t* ctx, uint32_t max)
{
    if (!ctx || ctx->magic != CTX_MAGIC || max == 0) {
        return 0;
    }

    uint32_t threshold = (-max) % max;
    uint32_t r;

    do {
        r = (uint32_t)(ctx_next_u64(ctx) >> 32);
    } while (r < threshold);

    return r % max;
}

float nimcp_rand_ctx_normal(nimcp_rand_ctx_t* ctx, float mean, float stddev)
{
    if (!ctx || ctx->magic != CTX_MAGIC) {
        return mean;
    }

    /* Box-Muller with caching */
    if (ctx->bm_has_cached) {
        ctx->bm_has_cached = false;
        return mean + stddev * ctx->bm_cache;
    }

    float u1, u2, s;

    do {
        u1 = 2.0f * nimcp_rand_ctx_uniform(ctx) - 1.0f;
        u2 = 2.0f * nimcp_rand_ctx_uniform(ctx) - 1.0f;
        s = u1 * u1 + u2 * u2;
    } while (s >= 1.0f || s == 0.0f);

    float mul = sqrtf(-2.0f * logf(s) / s);

    ctx->bm_cache = u2 * mul;
    ctx->bm_has_cached = true;

    return mean + stddev * (u1 * mul);
}

float nimcp_rand_ctx_exponential(nimcp_rand_ctx_t* ctx, float rate)
{
    if (!ctx || ctx->magic != CTX_MAGIC || rate <= 0.0f) {
        return 0.0f;
    }

    float u = nimcp_rand_ctx_uniform(ctx);
    return -logf(1.0f - u) / rate;
}

float nimcp_rand_ctx_pink(nimcp_rand_ctx_t* ctx)
{
    if (!ctx || ctx->magic != CTX_MAGIC) {
        return 0.0f;
    }

    return pink_next_ctx(ctx);
}

nimcp_rand_result_t nimcp_rand_ctx_bytes(nimcp_rand_ctx_t* ctx, uint8_t* buffer, size_t len)
{
    if (!ctx || ctx->magic != CTX_MAGIC) {
        return NIMCP_RAND_ERROR_INVALID;
    }

    if (!buffer) {
        return NIMCP_RAND_ERROR_NULL;
    }

    if (len == 0) {
        return NIMCP_RAND_OK;
    }

    size_t full_words = len / 8;
    for (size_t i = 0; i < full_words; i++) {
        uint64_t val = ctx_next_u64(ctx);
        memcpy(buffer + i * 8, &val, 8);
    }

    size_t remaining = len % 8;
    if (remaining > 0) {
        uint64_t val = ctx_next_u64(ctx);
        memcpy(buffer + full_words * 8, &val, remaining);
    }

    return NIMCP_RAND_OK;
}

/*=============================================================================
 * ARRAY/BATCH OPERATIONS
 *===========================================================================*/

void nimcp_rand_uniform_array(float* out, size_t n)
{
    if (!out || n == 0) {
        return;
    }

    for (size_t i = 0; i < n; i++) {
        out[i] = nimcp_rand_uniform();
    }
}

void nimcp_rand_normal_array(float* out, size_t n, float mean, float stddev)
{
    if (!out || n == 0) {
        return;
    }

    for (size_t i = 0; i < n; i++) {
        out[i] = nimcp_rand_normal(mean, stddev);
    }
}

void nimcp_rand_shuffle_u32(uint32_t* array, size_t n)
{
    if (!array || n <= 1) {
        return;
    }

    /* Fisher-Yates shuffle */
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = nimcp_rand_uint((uint32_t)(i + 1));
        uint32_t temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

void nimcp_rand_shuffle(void* array, size_t n, size_t element_size)
{
    if (!array || n <= 1 || element_size == 0) {
        return;
    }

    uint8_t* arr = (uint8_t*)array;
    uint8_t* temp = (uint8_t*)nimcp_malloc(element_size);
    if (!temp) {
        return;
    }

    /* Fisher-Yates shuffle */
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = nimcp_rand_uint((uint32_t)(i + 1));

        /* Swap elements */
        memcpy(temp, arr + i * element_size, element_size);
        memcpy(arr + i * element_size, arr + j * element_size, element_size);
        memcpy(arr + j * element_size, temp, element_size);
    }

    nimcp_free(temp);
}

uint32_t nimcp_rand_choice(const float* weights, uint32_t n)
{
    if (!weights || n == 0) {
        return 0;
    }

    /* Compute total weight */
    float total = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        total += weights[i];
    }

    if (total <= 0.0f) {
        return 0;
    }

    /* Sample from cumulative distribution */
    float target = nimcp_rand_uniform() * total;
    float cumulative = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        cumulative += weights[i];
        if (target < cumulative) {
            return i;
        }
    }

    return n - 1;
}

nimcp_rand_result_t nimcp_rand_sample(uint32_t n, uint32_t k, uint32_t* out)
{
    if (!out) {
        return NIMCP_RAND_ERROR_NULL;
    }

    if (k > n) {
        return NIMCP_RAND_ERROR_INVALID;
    }

    if (k == 0) {
        return NIMCP_RAND_OK;
    }

    /* Floyd's algorithm for sampling k from n */
    if (k <= n / 2) {
        /* Selection sampling */
        uint32_t* indices = (uint32_t*)nimcp_malloc(n * sizeof(uint32_t));
        if (!indices) {
            return NIMCP_RAND_ERROR_MEMORY;
        }

        for (uint32_t i = 0; i < n; i++) {
            indices[i] = i;
        }

        /* Partial Fisher-Yates */
        for (uint32_t i = 0; i < k; i++) {
            uint32_t j = i + nimcp_rand_uint(n - i);
            uint32_t temp = indices[i];
            indices[i] = indices[j];
            indices[j] = temp;
            out[i] = indices[i];
        }

        nimcp_free(indices);
    } else {
        /* For k close to n, use rejection sampling */
        bool* selected = (bool*)nimcp_calloc(n, sizeof(bool));
        if (!selected) {
            return NIMCP_RAND_ERROR_MEMORY;
        }

        uint32_t count = 0;
        while (count < k) {
            uint32_t idx = nimcp_rand_uint(n);
            if (!selected[idx]) {
                selected[idx] = true;
                out[count++] = idx;
            }
        }

        nimcp_free(selected);
    }

    return NIMCP_RAND_OK;
}

/*=============================================================================
 * QUANTUM-ENHANCED SAMPLING
 *===========================================================================*/

nimcp_rand_result_t nimcp_rand_quantum_sample(
    const float* probabilities,
    uint32_t n,
    uint32_t num_samples,
    uint32_t* samples)
{
    if (!probabilities || !samples) {
        return NIMCP_RAND_ERROR_NULL;
    }

    if (n == 0 || num_samples == 0) {
        return NIMCP_RAND_ERROR_INVALID;
    }

    nimcp_atomic_fetch_add_u64(&g_stats.quantum_calls, 1, NIMCP_MEMORY_ORDER_RELAXED);

    /* Check if quantum is enabled and available */
    if (g_config.enable_quantum) {
        /* Use QMC amplitude estimation */
        qmc_amplitude_config_t qmc_config = {
            .num_samples = num_samples,
            .use_importance = true,
            .proposal_dist = NULL,  /* Use probabilities directly */
            .seed = (uint32_t)nimcp_rand_entropy_seed()
        };

        /* For each sample, draw from probability distribution */
        /* Build cumulative distribution */
        float* cumulative = (float*)nimcp_malloc(n * sizeof(float));
        if (!cumulative) {
            return NIMCP_RAND_ERROR_MEMORY;
        }

        qmc_build_cumulative(probabilities, cumulative, n);

        for (uint32_t i = 0; i < num_samples; i++) {
            float target = nimcp_rand_uniform();
            samples[i] = qmc_binary_sample(cumulative, n, target);
        }

        nimcp_free(cumulative);
    } else {
        /* Fallback to classical sampling */
        float* cumulative = (float*)nimcp_malloc(n * sizeof(float));
        if (!cumulative) {
            return NIMCP_RAND_ERROR_MEMORY;
        }

        cumulative[0] = probabilities[0];
        for (uint32_t i = 1; i < n; i++) {
            cumulative[i] = cumulative[i-1] + probabilities[i];
        }

        /* Normalize */
        float total = cumulative[n-1];
        if (total > 0.0f) {
            for (uint32_t i = 0; i < n; i++) {
                cumulative[i] /= total;
            }
        }

        /* Sample */
        for (uint32_t i = 0; i < num_samples; i++) {
            float target = nimcp_rand_uniform();

            /* Binary search */
            uint32_t lo = 0, hi = n;
            while (lo < hi) {
                uint32_t mid = lo + (hi - lo) / 2;
                if (cumulative[mid] < target) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            samples[i] = lo < n ? lo : n - 1;
        }

        nimcp_free(cumulative);
    }

    return NIMCP_RAND_OK;
}

nimcp_rand_result_t nimcp_rand_amcs(
    float (*energy_fn)(const float* state, uint32_t dim, void* user_data),
    const float* initial_state,
    uint32_t dim,
    uint32_t num_samples,
    float* samples,
    void* user_data)
{
    if (!energy_fn || !initial_state || !samples) {
        return NIMCP_RAND_ERROR_NULL;
    }

    if (dim == 0 || num_samples == 0) {
        return NIMCP_RAND_ERROR_INVALID;
    }

    nimcp_atomic_fetch_add_u64(&g_stats.quantum_calls, 1, NIMCP_MEMORY_ORDER_RELAXED);

    if (g_config.enable_quantum) {
        /* Use QMC adaptive annealing */
        qmc_anneal_config_t config = qmc_anneal_default_config();
        config.num_iterations = num_samples;
        config.target_acceptance = QMC_TARGET_ACCEPTANCE_RATE;
        config.strategy = QMC_PROPOSAL_ADAPTIVE;
        config.seed = (uint32_t)nimcp_rand_entropy_seed();

        qmc_anneal_result_t result = {0};

        qmc_result_t qmc_res = qmc_adaptive_anneal(
            energy_fn, initial_state, dim, &config, user_data, &result
        );

        if (qmc_res != QMC_OK) {
            LOG_MODULE_WARN(LOG_MODULE_NAME, "QMC annealing failed, using fallback");
            /* Fall through to classical MCMC */
        } else {
            /* Copy results */
            memcpy(samples, result.best_state, dim * sizeof(float));
            qmc_anneal_result_free(&result);
            return NIMCP_RAND_OK;
        }
    }

    /* Classical Metropolis-Hastings fallback */
    float* current = (float*)nimcp_malloc(dim * sizeof(float));
    float* proposal = (float*)nimcp_malloc(dim * sizeof(float));

    if (!current || !proposal) {
        nimcp_free(current);
        nimcp_free(proposal);
        return NIMCP_RAND_ERROR_MEMORY;
    }

    memcpy(current, initial_state, dim * sizeof(float));
    float current_energy = energy_fn(current, dim, user_data);

    /* Adaptive step size */
    float step_size = 1.0f;
    uint32_t accepts = 0;
    uint32_t adapt_interval = 100;

    for (uint32_t i = 0; i < num_samples; i++) {
        /* Generate proposal */
        for (uint32_t d = 0; d < dim; d++) {
            proposal[d] = current[d] + nimcp_rand_normal(0.0f, step_size);
        }

        float proposal_energy = energy_fn(proposal, dim, user_data);

        /* Acceptance probability */
        float alpha = expf(-(proposal_energy - current_energy));

        if (nimcp_rand_uniform() < alpha) {
            memcpy(current, proposal, dim * sizeof(float));
            current_energy = proposal_energy;
            accepts++;
        }

        /* Store sample */
        memcpy(samples + i * dim, current, dim * sizeof(float));

        /* Adapt step size to target acceptance rate */
        if ((i + 1) % adapt_interval == 0) {
            float acceptance_rate = (float)accepts / (float)adapt_interval;
            if (acceptance_rate < 0.2f) {
                step_size *= 0.9f;
            } else if (acceptance_rate > 0.3f) {
                step_size *= 1.1f;
            }
            accepts = 0;
        }
    }

    nimcp_free(current);
    nimcp_free(proposal);

    return NIMCP_RAND_OK;
}

nimcp_rand_result_t nimcp_rand_qmcts(
    float (*evaluate_fn)(const void* state, void* user_data),
    const void* initial_state,
    size_t state_size,
    uint32_t max_iterations,
    void* best_state,
    void* user_data)
{
    if (!evaluate_fn || !initial_state || !best_state) {
        return NIMCP_RAND_ERROR_NULL;
    }

    if (state_size == 0 || max_iterations == 0) {
        return NIMCP_RAND_ERROR_INVALID;
    }

    nimcp_atomic_fetch_add_u64(&g_stats.quantum_calls, 1, NIMCP_MEMORY_ORDER_RELAXED);

    /* Simple MCTS implementation */
    /* For now, just use random sampling with best-so-far tracking */

    void* current = nimcp_malloc(state_size);
    if (!current) {
        return NIMCP_RAND_ERROR_MEMORY;
    }

    memcpy(current, initial_state, state_size);
    memcpy(best_state, initial_state, state_size);
    float best_score = evaluate_fn(initial_state, user_data);

    for (uint32_t iter = 0; iter < max_iterations; iter++) {
        /* Perturb state randomly */
        uint8_t* bytes = (uint8_t*)current;
        uint32_t mutation_point = nimcp_rand_uint((uint32_t)state_size);
        bytes[mutation_point] ^= (1 << nimcp_rand_int(8));

        float score = evaluate_fn(current, user_data);

        if (score > best_score) {
            best_score = score;
            memcpy(best_state, current, state_size);
        } else {
            /* Revert with some probability (simulated annealing) */
            float temp = 1.0f - (float)iter / (float)max_iterations;
            if (nimcp_rand_uniform() > temp) {
                memcpy(current, best_state, state_size);
            }
        }
    }

    nimcp_free(current);

    LOG_MODULE_DEBUG(LOG_MODULE_NAME, "QMCTS completed with best_score=%f", best_score);

    return NIMCP_RAND_OK;
}

/*=============================================================================
 * STATISTICS AND DEBUGGING
 *===========================================================================*/

void nimcp_rand_get_stats(nimcp_rand_stats_t* stats)
{
    if (!stats) {
        return;
    }

    stats->uniform_calls = nimcp_atomic_load_u64(&g_stats.uniform_calls, NIMCP_MEMORY_ORDER_RELAXED);
    stats->normal_calls = nimcp_atomic_load_u64(&g_stats.normal_calls, NIMCP_MEMORY_ORDER_RELAXED);
    stats->int_calls = nimcp_atomic_load_u64(&g_stats.int_calls, NIMCP_MEMORY_ORDER_RELAXED);
    stats->pink_calls = nimcp_atomic_load_u64(&g_stats.pink_calls, NIMCP_MEMORY_ORDER_RELAXED);
    stats->quantum_calls = nimcp_atomic_load_u64(&g_stats.quantum_calls, NIMCP_MEMORY_ORDER_RELAXED);
    stats->context_creates = nimcp_atomic_load_u64(&g_stats.context_creates, NIMCP_MEMORY_ORDER_RELAXED);
    stats->context_destroys = nimcp_atomic_load_u64(&g_stats.context_destroys, NIMCP_MEMORY_ORDER_RELAXED);
    stats->seed_reseeds = nimcp_atomic_load_u64(&g_stats.seed_reseeds, NIMCP_MEMORY_ORDER_RELAXED);
}

void nimcp_rand_reset_stats(void)
{
    nimcp_atomic_store_u64(&g_stats.uniform_calls, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats.normal_calls, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats.int_calls, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats.pink_calls, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats.quantum_calls, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats.context_creates, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats.context_destroys, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&g_stats.seed_reseeds, 0, NIMCP_MEMORY_ORDER_RELAXED);
}

const char* nimcp_rand_backend_name(nimcp_rand_backend_t backend)
{
    switch (backend) {
        case NIMCP_RAND_BACKEND_LCG:     return "LCG";
        case NIMCP_RAND_BACKEND_XORSHIFT: return "Xorshift128+";
        case NIMCP_RAND_BACKEND_PCG:     return "PCG";
        case NIMCP_RAND_BACKEND_QUANTUM: return "Quantum";
        case NIMCP_RAND_BACKEND_CSPRNG:  return "CSPRNG";
        case NIMCP_RAND_BACKEND_AUTO:    return "Auto";
        default:                         return "Unknown";
    }
}

nimcp_rand_result_t nimcp_rand_self_test(void)
{
    LOG_MODULE_INFO(LOG_MODULE_NAME, "Starting self-test...");

    /* Reset RNG state to ensure reproducible self-test results */
    /* Using fixed seed for deterministic testing */
    nimcp_rand_seed(0x5E1F7E57);  /* "SELFTEST" in hex-like form */

    const uint32_t num_samples = 10000;

    /* Test 1: Uniform mean should be ~0.5 */
    {
        double sum = 0.0;
        for (uint32_t i = 0; i < num_samples; i++) {
            sum += nimcp_rand_uniform();
        }
        double mean = sum / (double)num_samples;

        if (mean < 0.45 || mean > 0.55) {
            LOG_MODULE_ERROR(LOG_MODULE_NAME, "Uniform mean test FAILED: mean=%f", mean);
            return NIMCP_RAND_ERROR_INVALID;
        }
        LOG_MODULE_DEBUG(LOG_MODULE_NAME, "Uniform mean test PASSED: mean=%f", mean);
    }

    /* Test 2: Uniform variance should be ~1/12 = 0.0833 */
    {
        double sum = 0.0, sum_sq = 0.0;
        for (uint32_t i = 0; i < num_samples; i++) {
            double x = nimcp_rand_uniform();
            sum += x;
            sum_sq += x * x;
        }
        double mean = sum / (double)num_samples;
        double variance = (sum_sq / (double)num_samples) - (mean * mean);

        if (variance < 0.07 || variance > 0.10) {
            LOG_MODULE_ERROR(LOG_MODULE_NAME, "Uniform variance test FAILED: var=%f", variance);
            return NIMCP_RAND_ERROR_INVALID;
        }
        LOG_MODULE_DEBUG(LOG_MODULE_NAME, "Uniform variance test PASSED: var=%f", variance);
    }

    /* Test 3: Normal distribution mean and stddev */
    {
        double sum = 0.0, sum_sq = 0.0;
        float target_mean = 5.0f;
        float target_stddev = 2.0f;

        for (uint32_t i = 0; i < num_samples; i++) {
            double x = nimcp_rand_normal(target_mean, target_stddev);
            sum += x;
            sum_sq += x * x;
        }
        double mean = sum / (double)num_samples;
        double variance = (sum_sq / (double)num_samples) - (mean * mean);
        double stddev = sqrt(variance);

        if (fabs(mean - target_mean) > 0.2) {
            LOG_MODULE_ERROR(LOG_MODULE_NAME, "Normal mean test FAILED: mean=%f, expected=%f", mean, target_mean);
            return NIMCP_RAND_ERROR_INVALID;
        }
        if (fabs(stddev - target_stddev) > 0.2) {
            LOG_MODULE_ERROR(LOG_MODULE_NAME, "Normal stddev test FAILED: stddev=%f, expected=%f", stddev, target_stddev);
            return NIMCP_RAND_ERROR_INVALID;
        }
        LOG_MODULE_DEBUG(LOG_MODULE_NAME, "Normal test PASSED: mean=%f, stddev=%f", mean, stddev);
    }

    /* Test 4: Integer distribution uniformity (chi-squared) */
    {
        const int32_t max_val = 10;
        uint32_t bins[10] = {0};

        for (uint32_t i = 0; i < num_samples; i++) {
            int32_t x = nimcp_rand_int(max_val);
            if (x >= 0 && x < max_val) {
                bins[x]++;
            }
        }

        double expected = (double)num_samples / (double)max_val;
        double chi_sq = 0.0;
        for (int i = 0; i < max_val; i++) {
            double diff = (double)bins[i] - expected;
            chi_sq += (diff * diff) / expected;
        }

        /* Chi-squared critical value for 9 df at p=0.05 is 16.92 */
        if (chi_sq > 20.0) {
            LOG_MODULE_ERROR(LOG_MODULE_NAME, "Integer uniformity test FAILED: chi_sq=%f", chi_sq);
            return NIMCP_RAND_ERROR_INVALID;
        }
        LOG_MODULE_DEBUG(LOG_MODULE_NAME, "Integer uniformity test PASSED: chi_sq=%f", chi_sq);
    }

    /* Test 5: Pink noise should have decreasing power with frequency */
    {
        float samples[1024];
        for (int i = 0; i < 1024; i++) {
            samples[i] = nimcp_rand_pink();
        }

        /* Simple variance check - pink noise should be bounded */
        double sum = 0.0, sum_sq = 0.0;
        for (int i = 0; i < 1024; i++) {
            sum += samples[i];
            sum_sq += samples[i] * samples[i];
        }
        double variance = (sum_sq / 1024.0) - ((sum * sum) / (1024.0 * 1024.0));

        /* Pink noise should have reasonable variance */
        if (variance < 0.01 || variance > 1.0) {
            LOG_MODULE_ERROR(LOG_MODULE_NAME, "Pink noise variance test FAILED: var=%f", variance);
            return NIMCP_RAND_ERROR_INVALID;
        }
        LOG_MODULE_DEBUG(LOG_MODULE_NAME, "Pink noise test PASSED: var=%f", variance);
    }

    /* Test 6: Context reproducibility */
    {
        nimcp_rand_ctx_t* ctx1 = nimcp_rand_ctx_create(12345);
        nimcp_rand_ctx_t* ctx2 = nimcp_rand_ctx_create(12345);

        if (!ctx1 || !ctx2) {
            LOG_MODULE_ERROR(LOG_MODULE_NAME, "Context creation test FAILED");
            if (ctx1) nimcp_rand_ctx_destroy(ctx1);
            if (ctx2) nimcp_rand_ctx_destroy(ctx2);
            return NIMCP_RAND_ERROR_MEMORY;
        }

        bool match = true;
        for (int i = 0; i < 100; i++) {
            float v1 = nimcp_rand_ctx_uniform(ctx1);
            float v2 = nimcp_rand_ctx_uniform(ctx2);
            if (v1 != v2) {
                match = false;
                break;
            }
        }

        nimcp_rand_ctx_destroy(ctx1);
        nimcp_rand_ctx_destroy(ctx2);

        if (!match) {
            LOG_MODULE_ERROR(LOG_MODULE_NAME, "Context reproducibility test FAILED");
            return NIMCP_RAND_ERROR_INVALID;
        }
        LOG_MODULE_DEBUG(LOG_MODULE_NAME, "Context reproducibility test PASSED");
    }

    LOG_MODULE_INFO(LOG_MODULE_NAME, "All self-tests PASSED");
    return NIMCP_RAND_OK;
}
