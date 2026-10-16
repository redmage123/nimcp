/**
 * @file nimcp_streaming_statistics.c
 * @brief Streaming/Online Statistics Algorithms Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Implementation of memory-efficient streaming statistics algorithms
 * WHY:  Enable real-time statistical computation on unbounded data streams
 * HOW:  Welford, P-squared, t-digest, HyperLogLog, Count-Min Sketch, etc.
 *
 * @author NIMCP Development Team
 */

#include "utils/statistics/nimcp_streaming_statistics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>

#define LOG_MODULE "STREAM_STATS"

//=============================================================================
// Module State
//=============================================================================

static struct {
    bool initialized;
    nimcp_stream_stats_config_t config;
    nimcp_atomic_uint64_t rng_state;
} g_stream_stats = {0};

//=============================================================================
// Internal Structure Definitions
//=============================================================================

/**
 * @brief Online descriptive statistics accumulator (Welford's algorithm)
 */
struct nimcp_stream_stats_s {
    uint32_t magic;
    nimcp_atomic_uint64_t n;              /**< Count */
    double mean;                           /**< Running mean */
    double m2;                             /**< Sum of squared deviations */
    double m3;                             /**< Third moment sum */
    double m4;                             /**< Fourth moment sum */
    double min;                            /**< Minimum */
    double max;                            /**< Maximum */
    double sum;                            /**< Running sum */
    nimcp_mutex_t* mutex;                  /**< Lock for complex updates */
};

/**
 * @brief P-squared quantile markers
 */
typedef struct {
    double q[5];      /**< Marker heights */
    double n[5];      /**< Marker positions */
    double np[5];     /**< Desired marker positions */
    double dn[5];     /**< Increments */
} psquared_markers_t;

/**
 * @brief t-digest centroid
 */
typedef struct {
    double mean;
    double weight;
} tdigest_centroid_t;

/**
 * @brief Online quantile estimator
 */
struct nimcp_stream_quantile_s {
    uint32_t magic;
    int algorithm;                         /**< P-squared or t-digest */
    uint64_t count;
    union {
        struct {
            psquared_markers_t markers;
            double p;                      /**< Target quantile */
        } psquared;
        struct {
            tdigest_centroid_t* centroids;
            uint32_t n_centroids;
            uint32_t max_centroids;
            double compression;
            double min;
            double max;
        } tdigest;
    } data;
    nimcp_mutex_t* mutex;
};

/**
 * @brief Bivariate covariance accumulator
 */
struct nimcp_stream_cov_s {
    uint32_t magic;
    uint64_t n;
    double mean_x;
    double mean_y;
    double m2_x;
    double m2_y;
    double c;         /**< Co-moment sum */
    nimcp_mutex_t* mutex;
};

/**
 * @brief Multivariate covariance matrix accumulator
 */
struct nimcp_stream_cov_matrix_s {
    uint32_t magic;
    uint32_t n_dims;
    uint64_t n;
    double* means;     /**< Running means [n_dims] */
    double* cov;       /**< Upper triangular co-moments [n_dims * n_dims] */
    nimcp_mutex_t* mutex;
};

/**
 * @brief Incremental PCA
 */
struct nimcp_stream_pca_s {
    uint32_t magic;
    uint32_t n_components;
    uint32_t n_features;
    uint64_t n_samples;
    float forgetting_factor;
    bool whiten;
    float* components;      /**< [n_components x n_features] */
    float* mean;            /**< [n_features] */
    float* variance;        /**< [n_components] */
    float* singular_values; /**< [n_components] */
    nimcp_mutex_t* mutex;
};

/**
 * @brief Online linear regression (RLS)
 */
struct nimcp_stream_linreg_s {
    uint32_t magic;
    uint32_t n_features;
    uint64_t n_samples;
    float forgetting_factor;
    float* coefficients;    /**< [n_features + 1] (including intercept) */
    float* P;               /**< Inverse covariance matrix [(n_features+1)^2] */
    double ss_res;          /**< Residual sum of squares */
    double ss_tot;          /**< Total sum of squares */
    double y_mean;          /**< Running mean of y */
    nimcp_mutex_t* mutex;
};

/**
 * @brief Reservoir sampler
 */
struct nimcp_reservoir_s {
    uint32_t magic;
    uint32_t capacity;
    uint32_t size;
    uint64_t stream_count;
    bool weighted;
    float* samples;
    float* weights;         /**< Only for weighted sampling */
    nimcp_mutex_t* mutex;
};

/**
 * @brief Count-Min Sketch
 */
struct nimcp_cms_s {
    uint32_t magic;
    uint32_t width;
    uint32_t depth;
    uint64_t total_count;
    uint32_t* counters;     /**< [depth x width] */
    uint64_t* hash_seeds;   /**< [depth] */
    nimcp_mutex_t* mutex;
};

/**
 * @brief HyperLogLog
 */
struct nimcp_hll_s {
    uint32_t magic;
    uint32_t precision;     /**< Number of bits for bucket index */
    uint32_t n_buckets;     /**< 2^precision */
    uint8_t* buckets;       /**< Register values */
    nimcp_mutex_t* mutex;
};

/**
 * @brief EWMA/EWMV accumulator
 */
struct nimcp_ewma_s {
    uint32_t magic;
    float alpha;
    double ewma;
    double ewmv;            /**< Only used for EWMV variant */
    uint64_t count;
    bool has_variance;
    nimcp_mutex_t* mutex;
};

//=============================================================================
// Internal Helpers
//=============================================================================

static inline bool validate_magic(uint32_t magic, uint32_t expected)
{
    return magic == expected;
}

/* Simple XorShift64 RNG for thread-local random numbers */
static inline uint64_t xorshift64(uint64_t* state)
{
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static inline double random_double(void)
{
    uint64_t state = nimcp_atomic_load_u64(&g_stream_stats.rng_state,
                                           NIMCP_MEMORY_ORDER_RELAXED);
    uint64_t next = xorshift64(&state);
    nimcp_atomic_store_u64(&g_stream_stats.rng_state, state,
                          NIMCP_MEMORY_ORDER_RELAXED);
    return (next >> 11) * (1.0 / 9007199254740992.0);
}

/* FNV-1a hash */
static inline uint64_t fnv1a_hash(const void* data, size_t len)
{
    const uint8_t* p = (const uint8_t*)data;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= p[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

/* Count leading zeros */
static inline uint32_t clz64(uint64_t x)
{
    if (x == 0) return 64;
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_clzll(x);
#else
    uint32_t n = 0;
    if ((x & 0xFFFFFFFF00000000ULL) == 0) { n += 32; x <<= 32; }
    if ((x & 0xFFFF000000000000ULL) == 0) { n += 16; x <<= 16; }
    if ((x & 0xFF00000000000000ULL) == 0) { n += 8; x <<= 8; }
    if ((x & 0xF000000000000000ULL) == 0) { n += 4; x <<= 4; }
    if ((x & 0xC000000000000000ULL) == 0) { n += 2; x <<= 2; }
    if ((x & 0x8000000000000000ULL) == 0) { n += 1; }
    return n;
#endif
}

//=============================================================================
// Module Initialization
//=============================================================================

nimcp_stream_stats_config_t nimcp_stream_stats_default_config(void)
{
    nimcp_stream_stats_config_t config = {
        .enable_gpu = true,
        .gpu_batch_threshold = 1000,
        .enable_thread_safety = true,
        .tdigest_compression = NIMCP_TDIGEST_DEFAULT_COMPRESSION,
        .hll_precision = NIMCP_HLL_DEFAULT_PRECISION,
        .cms_width = NIMCP_CMS_DEFAULT_WIDTH,
        .cms_depth = NIMCP_CMS_DEFAULT_DEPTH,
        .reservoir_size = NIMCP_RESERVOIR_DEFAULT_SIZE,
        .random_seed = 0
    };
    return config;
}

nimcp_stream_stats_result_t nimcp_stream_stats_init(
    const nimcp_stream_stats_config_t* config)
{
    if (g_stream_stats.initialized) {
        return NIMCP_STREAM_OK;
    }

    if (config) {
        g_stream_stats.config = *config;
    } else {
        g_stream_stats.config = nimcp_stream_stats_default_config();
    }

    /* Initialize RNG state */
    uint64_t seed = g_stream_stats.config.random_seed;
    if (seed == 0) {
        seed = (uint64_t)time(NULL) ^ ((uint64_t)clock() << 32);
    }
    nimcp_atomic_init_u64(&g_stream_stats.rng_state, seed);

    g_stream_stats.initialized = true;
    NIMCP_LOGGING_INFO("Streaming statistics module initialized");
    return NIMCP_STREAM_OK;
}

void nimcp_stream_stats_shutdown(void)
{
    if (!g_stream_stats.initialized) {
        return;
    }

    g_stream_stats.initialized = false;
    NIMCP_LOGGING_INFO("Streaming statistics module shutdown");
}

bool nimcp_stream_stats_is_initialized(void)
{
    return g_stream_stats.initialized;
}

//=============================================================================
// Online Descriptive Statistics Implementation
//=============================================================================

nimcp_stream_stats_t nimcp_stream_stats_create(void)
{
    struct nimcp_stream_stats_s* stats = nimcp_calloc(1, sizeof(*stats));
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate streaming stats accumulator");
        return NULL;
    }

    stats->magic = NIMCP_STREAM_STATS_MAGIC;
    nimcp_atomic_init_u64(&stats->n, 0);
    stats->mean = 0.0;
    stats->m2 = 0.0;
    stats->m3 = 0.0;
    stats->m4 = 0.0;
    stats->min = INFINITY;
    stats->max = -INFINITY;
    stats->sum = 0.0;

    if (g_stream_stats.config.enable_thread_safety) {
        stats->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
        if (stats->mutex) {
            nimcp_mutex_init(stats->mutex, NULL);
        }
    }

    return stats;
}

void nimcp_stream_stats_destroy(nimcp_stream_stats_t stats)
{
    if (!stats) return;
    if (!validate_magic(stats->magic, NIMCP_STREAM_STATS_MAGIC)) return;

    if (stats->mutex) {
        nimcp_mutex_destroy(stats->mutex);
        nimcp_free(stats->mutex);
    }

    stats->magic = 0;
    nimcp_free(stats);
}

nimcp_stream_stats_result_t nimcp_stream_stats_update(
    nimcp_stream_stats_t stats,
    double value)
{
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL stats in update");
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(stats->magic, NIMCP_STREAM_STATS_MAGIC)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "Invalid stats magic");
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (stats->mutex) {
        nimcp_mutex_lock(stats->mutex);
    }

    /* Welford's online algorithm with higher moments */
    uint64_t n1 = nimcp_atomic_load_u64(&stats->n, NIMCP_MEMORY_ORDER_RELAXED);
    uint64_t n = n1 + 1;

    double delta = value - stats->mean;
    double delta_n = delta / (double)n;
    double delta_n2 = delta_n * delta_n;
    double term1 = delta * delta_n * (double)n1;

    /* Update mean */
    stats->mean += delta_n;

    /* Update fourth moment (must come before m2/m3 updates) */
    stats->m4 += term1 * delta_n2 * ((double)n * (double)n - 3.0 * (double)n + 3.0)
               + 6.0 * delta_n2 * stats->m2
               - 4.0 * delta_n * stats->m3;

    /* Update third moment (must come before m2 update) */
    stats->m3 += term1 * delta_n * ((double)n - 2.0)
               - 3.0 * delta_n * stats->m2;

    /* Update second moment */
    stats->m2 += term1;

    /* Update min/max/sum */
    if (value < stats->min) stats->min = value;
    if (value > stats->max) stats->max = value;
    stats->sum += value;

    nimcp_atomic_store_u64(&stats->n, n, NIMCP_MEMORY_ORDER_RELEASE);

    if (stats->mutex) {
        nimcp_mutex_unlock(stats->mutex);
    }

    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_stream_stats_update_batch(
    nimcp_stream_stats_t stats,
    const float* values,
    uint32_t count)
{
    if (!stats || !values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in batch update");
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (count == 0) {
        return NIMCP_STREAM_OK;
    }

    /* TODO: GPU acceleration for large batches */
    for (uint32_t i = 0; i < count; i++) {
        nimcp_stream_stats_result_t result = nimcp_stream_stats_update(stats,
                                                                       (double)values[i]);
        if (result != NIMCP_STREAM_OK) {
            return result;
        }
    }

    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_stream_stats_merge(
    nimcp_stream_stats_t dest,
    const nimcp_stream_stats_t src)
{
    if (!dest || !src) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in merge");
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(dest->magic, NIMCP_STREAM_STATS_MAGIC) ||
        !validate_magic(src->magic, NIMCP_STREAM_STATS_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (dest->mutex) nimcp_mutex_lock(dest->mutex);

    uint64_t na = nimcp_atomic_load_u64(&dest->n, NIMCP_MEMORY_ORDER_RELAXED);
    uint64_t nb = nimcp_atomic_load_u64(&src->n, NIMCP_MEMORY_ORDER_RELAXED);

    if (nb == 0) {
        if (dest->mutex) nimcp_mutex_unlock(dest->mutex);
        return NIMCP_STREAM_OK;
    }

    if (na == 0) {
        /* Copy src to dest */
        dest->mean = src->mean;
        dest->m2 = src->m2;
        dest->m3 = src->m3;
        dest->m4 = src->m4;
        dest->min = src->min;
        dest->max = src->max;
        dest->sum = src->sum;
        nimcp_atomic_store_u64(&dest->n, nb, NIMCP_MEMORY_ORDER_RELEASE);
        if (dest->mutex) nimcp_mutex_unlock(dest->mutex);
        return NIMCP_STREAM_OK;
    }

    /* Chan et al. parallel combination formula */
    uint64_t n = na + nb;
    double delta = src->mean - dest->mean;
    double delta2 = delta * delta;
    double delta3 = delta2 * delta;
    double delta4 = delta2 * delta2;

    double na_d = (double)na;
    double nb_d = (double)nb;
    double n_d = (double)n;

    /* Combined mean */
    double new_mean = (na_d * dest->mean + nb_d * src->mean) / n_d;

    /* Combined M2 */
    double new_m2 = dest->m2 + src->m2 + delta2 * na_d * nb_d / n_d;

    /* Combined M3 */
    double new_m3 = dest->m3 + src->m3
                  + delta3 * na_d * nb_d * (na_d - nb_d) / (n_d * n_d)
                  + 3.0 * delta * (na_d * src->m2 - nb_d * dest->m2) / n_d;

    /* Combined M4 */
    double new_m4 = dest->m4 + src->m4
                  + delta4 * na_d * nb_d * (na_d * na_d - na_d * nb_d + nb_d * nb_d) / (n_d * n_d * n_d)
                  + 6.0 * delta2 * (na_d * na_d * src->m2 + nb_d * nb_d * dest->m2) / (n_d * n_d)
                  + 4.0 * delta * (na_d * src->m3 - nb_d * dest->m3) / n_d;

    dest->mean = new_mean;
    dest->m2 = new_m2;
    dest->m3 = new_m3;
    dest->m4 = new_m4;

    if (src->min < dest->min) dest->min = src->min;
    if (src->max > dest->max) dest->max = src->max;
    dest->sum += src->sum;

    nimcp_atomic_store_u64(&dest->n, n, NIMCP_MEMORY_ORDER_RELEASE);

    if (dest->mutex) nimcp_mutex_unlock(dest->mutex);
    return NIMCP_STREAM_OK;
}

double nimcp_stream_stats_get_mean(const nimcp_stream_stats_t stats)
{
    if (!stats || !validate_magic(stats->magic, NIMCP_STREAM_STATS_MAGIC)) {
        return NAN;
    }
    uint64_t n = nimcp_atomic_load_u64(&stats->n, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (n == 0) return NAN;
    return stats->mean;
}

double nimcp_stream_stats_get_variance(const nimcp_stream_stats_t stats)
{
    if (!stats || !validate_magic(stats->magic, NIMCP_STREAM_STATS_MAGIC)) {
        return NAN;
    }
    uint64_t n = nimcp_atomic_load_u64(&stats->n, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (n < 2) return NAN;
    return stats->m2 / (double)(n - 1);
}

double nimcp_stream_stats_get_variance_population(const nimcp_stream_stats_t stats)
{
    if (!stats || !validate_magic(stats->magic, NIMCP_STREAM_STATS_MAGIC)) {
        return NAN;
    }
    uint64_t n = nimcp_atomic_load_u64(&stats->n, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (n == 0) return NAN;
    return stats->m2 / (double)n;
}

double nimcp_stream_stats_get_std(const nimcp_stream_stats_t stats)
{
    double var = nimcp_stream_stats_get_variance(stats);
    if (isnan(var)) return NAN;
    return sqrt(var);
}

double nimcp_stream_stats_get_min(const nimcp_stream_stats_t stats)
{
    if (!stats || !validate_magic(stats->magic, NIMCP_STREAM_STATS_MAGIC)) {
        return NAN;
    }
    return stats->min;
}

double nimcp_stream_stats_get_max(const nimcp_stream_stats_t stats)
{
    if (!stats || !validate_magic(stats->magic, NIMCP_STREAM_STATS_MAGIC)) {
        return NAN;
    }
    return stats->max;
}

uint64_t nimcp_stream_stats_get_count(const nimcp_stream_stats_t stats)
{
    if (!stats || !validate_magic(stats->magic, NIMCP_STREAM_STATS_MAGIC)) {
        return 0;
    }
    return nimcp_atomic_load_u64(&stats->n, NIMCP_MEMORY_ORDER_ACQUIRE);
}

double nimcp_stream_stats_get_sum(const nimcp_stream_stats_t stats)
{
    if (!stats || !validate_magic(stats->magic, NIMCP_STREAM_STATS_MAGIC)) {
        return NAN;
    }
    return stats->sum;
}

double nimcp_stream_stats_get_skewness(const nimcp_stream_stats_t stats)
{
    if (!stats || !validate_magic(stats->magic, NIMCP_STREAM_STATS_MAGIC)) {
        return NAN;
    }
    uint64_t n = nimcp_atomic_load_u64(&stats->n, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (n < 3 || stats->m2 == 0.0) return NAN;

    double n_d = (double)n;
    return (sqrt(n_d * (n_d - 1.0)) / (n_d - 2.0)) *
           (stats->m3 / pow(stats->m2, 1.5)) * sqrt(n_d);
}

double nimcp_stream_stats_get_kurtosis(const nimcp_stream_stats_t stats)
{
    if (!stats || !validate_magic(stats->magic, NIMCP_STREAM_STATS_MAGIC)) {
        return NAN;
    }
    uint64_t n = nimcp_atomic_load_u64(&stats->n, NIMCP_MEMORY_ORDER_ACQUIRE);
    if (n < 4 || stats->m2 == 0.0) return NAN;

    double n_d = (double)n;
    double excess_kurt = (n_d * stats->m4) / (stats->m2 * stats->m2) - 3.0;

    /* Bias correction */
    return ((n_d - 1.0) / ((n_d - 2.0) * (n_d - 3.0))) *
           ((n_d + 1.0) * excess_kurt + 6.0);
}

nimcp_stream_stats_result_t nimcp_stream_stats_reset(nimcp_stream_stats_t stats)
{
    if (!stats) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(stats->magic, NIMCP_STREAM_STATS_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (stats->mutex) nimcp_mutex_lock(stats->mutex);

    nimcp_atomic_store_u64(&stats->n, 0, NIMCP_MEMORY_ORDER_RELEASE);
    stats->mean = 0.0;
    stats->m2 = 0.0;
    stats->m3 = 0.0;
    stats->m4 = 0.0;
    stats->min = INFINITY;
    stats->max = -INFINITY;
    stats->sum = 0.0;

    if (stats->mutex) nimcp_mutex_unlock(stats->mutex);
    return NIMCP_STREAM_OK;
}

//=============================================================================
// Online Quantile Estimation Implementation
//=============================================================================

/* P-squared algorithm helper functions */
static void psquared_init(psquared_markers_t* m, double p)
{
    m->dn[0] = 0.0;
    m->dn[1] = p / 2.0;
    m->dn[2] = p;
    m->dn[3] = (1.0 + p) / 2.0;
    m->dn[4] = 1.0;

    for (int i = 0; i < 5; i++) {
        m->n[i] = (double)(i + 1);
        m->np[i] = 1.0 + 2.0 * p * (double)i + (1.0 - p) * (double)(i * (i - 1)) / 2.0;
    }
}

static double psquared_parabolic(double qim1, double qi, double qip1,
                                  double nim1, double ni, double nip1, double d)
{
    return qi + d / (nip1 - nim1) * (
        (ni - nim1 + d) * (qip1 - qi) / (nip1 - ni) +
        (nip1 - ni - d) * (qi - qim1) / (ni - nim1)
    );
}

static double psquared_linear(double qi, double qj, double ni, double nj, double d)
{
    return qi + d * (qj - qi) / (nj - ni);
}

nimcp_stream_quantile_t nimcp_stream_quantile_create(
    const nimcp_stream_quantile_config_t* config)
{
    struct nimcp_stream_quantile_s* q = nimcp_calloc(1, sizeof(*q));
    if (!q) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate quantile estimator");
        return NULL;
    }

    q->magic = NIMCP_STREAM_QUANTILE_MAGIC;
    q->count = 0;

    if (config) {
        q->algorithm = config->algorithm;
        if (config->algorithm == NIMCP_QUANTILE_TDIGEST) {
            q->data.tdigest.compression = config->compression > 0 ?
                config->compression : NIMCP_TDIGEST_DEFAULT_COMPRESSION;
            q->data.tdigest.max_centroids = (uint32_t)(q->data.tdigest.compression * 2);
            q->data.tdigest.centroids = nimcp_calloc(q->data.tdigest.max_centroids,
                                                     sizeof(tdigest_centroid_t));
            if (!q->data.tdigest.centroids) {
                nimcp_free(q);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stream_quantile_create: q->data is NULL");
                return NULL;
            }
            q->data.tdigest.n_centroids = 0;
            q->data.tdigest.min = INFINITY;
            q->data.tdigest.max = -INFINITY;
        } else {
            q->data.psquared.p = config->target_quantile > 0 ?
                config->target_quantile : 0.5;
            psquared_init(&q->data.psquared.markers, q->data.psquared.p);
        }
    } else {
        /* Default: P-squared median */
        q->algorithm = NIMCP_QUANTILE_PSQUARED;
        q->data.psquared.p = 0.5;
        psquared_init(&q->data.psquared.markers, 0.5);
    }

    if (g_stream_stats.config.enable_thread_safety) {
        q->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
        if (q->mutex) {
            nimcp_mutex_init(q->mutex, NULL);
        }
    }

    return q;
}

void nimcp_stream_quantile_destroy(nimcp_stream_quantile_t quantile)
{
    if (!quantile) return;
    if (!validate_magic(quantile->magic, NIMCP_STREAM_QUANTILE_MAGIC)) return;

    if (quantile->algorithm == NIMCP_QUANTILE_TDIGEST &&
        quantile->data.tdigest.centroids) {
        nimcp_free(quantile->data.tdigest.centroids);
    }

    if (quantile->mutex) {
        nimcp_mutex_destroy(quantile->mutex);
        nimcp_free(quantile->mutex);
    }

    quantile->magic = 0;
    nimcp_free(quantile);
}

nimcp_stream_stats_result_t nimcp_stream_quantile_update(
    nimcp_stream_quantile_t quantile,
    double value)
{
    if (!quantile) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(quantile->magic, NIMCP_STREAM_QUANTILE_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (quantile->mutex) nimcp_mutex_lock(quantile->mutex);

    quantile->count++;

    if (quantile->algorithm == NIMCP_QUANTILE_PSQUARED) {
        psquared_markers_t* m = &quantile->data.psquared.markers;

        /* First 5 observations: collect and sort */
        if (quantile->count <= 5) {
            m->q[quantile->count - 1] = value;
            if (quantile->count == 5) {
                /* Sort markers */
                for (int i = 0; i < 5; i++) {
                    for (int j = i + 1; j < 5; j++) {
                        if (m->q[i] > m->q[j]) {
                            double tmp = m->q[i];
                            m->q[i] = m->q[j];
                            m->q[j] = tmp;
                        }
                    }
                }
            }
        } else {
            /* Find cell k where x falls */
            int k;
            if (value < m->q[0]) {
                m->q[0] = value;
                k = 0;
            } else if (value >= m->q[4]) {
                m->q[4] = value;
                k = 3;
            } else {
                for (k = 0; k < 4; k++) {
                    if (value < m->q[k + 1]) break;
                }
            }

            /* Increment positions of markers > k */
            for (int i = k + 1; i < 5; i++) {
                m->n[i]++;
            }

            /* Update desired positions */
            for (int i = 0; i < 5; i++) {
                m->np[i] += m->dn[i];
            }

            /* Adjust heights of markers 1-3 if necessary */
            for (int i = 1; i < 4; i++) {
                double d = m->np[i] - m->n[i];
                if ((d >= 1.0 && m->n[i + 1] - m->n[i] > 1) ||
                    (d <= -1.0 && m->n[i - 1] - m->n[i] < -1)) {
                    int sign = (d > 0) ? 1 : -1;
                    double q_new = psquared_parabolic(m->q[i - 1], m->q[i], m->q[i + 1],
                                                      m->n[i - 1], m->n[i], m->n[i + 1],
                                                      (double)sign);
                    if (m->q[i - 1] < q_new && q_new < m->q[i + 1]) {
                        m->q[i] = q_new;
                    } else {
                        m->q[i] = psquared_linear(m->q[i], m->q[i + sign],
                                                  m->n[i], m->n[i + sign],
                                                  (double)sign);
                    }
                    m->n[i] += sign;
                }
            }
        }
    } else {
        /* t-digest update - simplified implementation */
        tdigest_centroid_t* c = quantile->data.tdigest.centroids;
        uint32_t* nc = &quantile->data.tdigest.n_centroids;

        if (value < quantile->data.tdigest.min) quantile->data.tdigest.min = value;
        if (value > quantile->data.tdigest.max) quantile->data.tdigest.max = value;

        if (*nc < quantile->data.tdigest.max_centroids) {
            /* Add new centroid */
            c[*nc].mean = value;
            c[*nc].weight = 1.0;
            (*nc)++;
        } else {
            /* Merge with closest centroid */
            uint32_t closest = 0;
            double min_dist = fabs(value - c[0].mean);
            for (uint32_t i = 1; i < *nc; i++) {
                double dist = fabs(value - c[i].mean);
                if (dist < min_dist) {
                    min_dist = dist;
                    closest = i;
                }
            }
            double total_weight = c[closest].weight + 1.0;
            c[closest].mean = (c[closest].mean * c[closest].weight + value) / total_weight;
            c[closest].weight = total_weight;
        }
    }

    if (quantile->mutex) nimcp_mutex_unlock(quantile->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_stream_quantile_update_batch(
    nimcp_stream_quantile_t quantile,
    const float* values,
    uint32_t count)
{
    if (!quantile || !values) {
        return NIMCP_STREAM_ERROR_NULL;
    }

    for (uint32_t i = 0; i < count; i++) {
        nimcp_stream_stats_result_t result =
            nimcp_stream_quantile_update(quantile, (double)values[i]);
        if (result != NIMCP_STREAM_OK) {
            return result;
        }
    }
    return NIMCP_STREAM_OK;
}

double nimcp_stream_quantile_get(const nimcp_stream_quantile_t quantile, double p)
{
    if (!quantile || !validate_magic(quantile->magic, NIMCP_STREAM_QUANTILE_MAGIC)) {
        return NAN;
    }
    if (quantile->count == 0) {
        return NAN;
    }

    if (quantile->algorithm == NIMCP_QUANTILE_PSQUARED) {
        if (quantile->count < 5) {
            /* Return from sorted initial observations */
            return quantile->data.psquared.markers.q[(int)(p * (quantile->count - 1))];
        }
        return quantile->data.psquared.markers.q[2]; /* Always returns target quantile */
    } else {
        /* t-digest quantile lookup - simplified */
        if (quantile->data.tdigest.n_centroids == 0) {
            return NAN;
        }

        /* Sort centroids by mean for lookup */
        /* For a full implementation, keep centroids sorted */
        tdigest_centroid_t* c = quantile->data.tdigest.centroids;
        uint32_t nc = quantile->data.tdigest.n_centroids;

        /* Simple linear search for now */
        double total_weight = 0;
        for (uint32_t i = 0; i < nc; i++) {
            total_weight += c[i].weight;
        }

        double target = p * total_weight;
        double cumsum = 0;
        for (uint32_t i = 0; i < nc; i++) {
            if (cumsum + c[i].weight >= target) {
                return c[i].mean;
            }
            cumsum += c[i].weight;
        }
        return c[nc - 1].mean;
    }
}

nimcp_stream_stats_result_t nimcp_stream_quantile_merge(
    nimcp_stream_quantile_t dest,
    const nimcp_stream_quantile_t src)
{
    if (!dest || !src) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(dest->magic, NIMCP_STREAM_QUANTILE_MAGIC) ||
        !validate_magic(src->magic, NIMCP_STREAM_QUANTILE_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }
    if (dest->algorithm != src->algorithm) {
        return NIMCP_STREAM_ERROR_MISMATCH;
    }

    /* P-squared doesn't support true merging; just return OK */
    /* t-digest merge would combine centroids */
    if (dest->algorithm == NIMCP_QUANTILE_TDIGEST) {
        /* Simplified: just combine centroid lists */
        if (dest->mutex) nimcp_mutex_lock(dest->mutex);

        for (uint32_t i = 0; i < src->data.tdigest.n_centroids; i++) {
            if (dest->data.tdigest.n_centroids < dest->data.tdigest.max_centroids) {
                dest->data.tdigest.centroids[dest->data.tdigest.n_centroids++] =
                    src->data.tdigest.centroids[i];
            }
        }
        if (src->data.tdigest.min < dest->data.tdigest.min) {
            dest->data.tdigest.min = src->data.tdigest.min;
        }
        if (src->data.tdigest.max > dest->data.tdigest.max) {
            dest->data.tdigest.max = src->data.tdigest.max;
        }
        dest->count += src->count;

        if (dest->mutex) nimcp_mutex_unlock(dest->mutex);
    }

    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_stream_quantile_reset(
    nimcp_stream_quantile_t quantile)
{
    if (!quantile) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(quantile->magic, NIMCP_STREAM_QUANTILE_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (quantile->mutex) nimcp_mutex_lock(quantile->mutex);

    quantile->count = 0;
    if (quantile->algorithm == NIMCP_QUANTILE_PSQUARED) {
        psquared_init(&quantile->data.psquared.markers, quantile->data.psquared.p);
    } else {
        quantile->data.tdigest.n_centroids = 0;
        quantile->data.tdigest.min = INFINITY;
        quantile->data.tdigest.max = -INFINITY;
    }

    if (quantile->mutex) nimcp_mutex_unlock(quantile->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_stream_quantile_t nimcp_stream_median_create(void)
{
    nimcp_stream_quantile_config_t config = {
        .algorithm = NIMCP_QUANTILE_PSQUARED,
        .target_quantile = 0.5f
    };
    return nimcp_stream_quantile_create(&config);
}

double nimcp_stream_median(const nimcp_stream_quantile_t quantile)
{
    return nimcp_stream_quantile_get(quantile, 0.5);
}

//=============================================================================
// Online Covariance/Correlation Implementation
//=============================================================================

nimcp_stream_cov_t nimcp_stream_cov_create(void)
{
    struct nimcp_stream_cov_s* cov = nimcp_calloc(1, sizeof(*cov));
    if (!cov) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate covariance accumulator");
        return NULL;
    }

    cov->magic = NIMCP_STREAM_COV_MAGIC;
    cov->n = 0;
    cov->mean_x = 0.0;
    cov->mean_y = 0.0;
    cov->m2_x = 0.0;
    cov->m2_y = 0.0;
    cov->c = 0.0;

    if (g_stream_stats.config.enable_thread_safety) {
        cov->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
        if (cov->mutex) {
            nimcp_mutex_init(cov->mutex, NULL);
        }
    }

    return cov;
}

void nimcp_stream_cov_destroy(nimcp_stream_cov_t cov)
{
    if (!cov) return;
    if (!validate_magic(cov->magic, NIMCP_STREAM_COV_MAGIC)) return;

    if (cov->mutex) {
        nimcp_mutex_destroy(cov->mutex);
        nimcp_free(cov->mutex);
    }

    cov->magic = 0;
    nimcp_free(cov);
}

nimcp_stream_stats_result_t nimcp_stream_cov_update(
    nimcp_stream_cov_t cov,
    double x,
    double y)
{
    if (!cov) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(cov->magic, NIMCP_STREAM_COV_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (cov->mutex) nimcp_mutex_lock(cov->mutex);

    cov->n++;
    double n = (double)cov->n;

    double dx = x - cov->mean_x;
    double dy = y - cov->mean_y;

    cov->mean_x += dx / n;
    cov->mean_y += dy / n;

    double dx2 = x - cov->mean_x;
    double dy2 = y - cov->mean_y;

    cov->m2_x += dx * dx2;
    cov->m2_y += dy * dy2;
    cov->c += dx * dy2;

    if (cov->mutex) nimcp_mutex_unlock(cov->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_stream_cov_update_batch(
    nimcp_stream_cov_t cov,
    const float* x,
    const float* y,
    uint32_t count)
{
    if (!cov || !x || !y) {
        return NIMCP_STREAM_ERROR_NULL;
    }

    for (uint32_t i = 0; i < count; i++) {
        nimcp_stream_stats_result_t result =
            nimcp_stream_cov_update(cov, (double)x[i], (double)y[i]);
        if (result != NIMCP_STREAM_OK) {
            return result;
        }
    }
    return NIMCP_STREAM_OK;
}

double nimcp_stream_cov_get(const nimcp_stream_cov_t cov)
{
    if (!cov || !validate_magic(cov->magic, NIMCP_STREAM_COV_MAGIC)) {
        return NAN;
    }
    if (cov->n < 2) return NAN;
    return cov->c / (double)(cov->n - 1);
}

double nimcp_stream_corr_get(const nimcp_stream_cov_t cov)
{
    if (!cov || !validate_magic(cov->magic, NIMCP_STREAM_COV_MAGIC)) {
        return NAN;
    }
    if (cov->n < 2) return NAN;
    if (cov->m2_x == 0.0 || cov->m2_y == 0.0) return NAN;
    return cov->c / sqrt(cov->m2_x * cov->m2_y);
}

nimcp_stream_stats_result_t nimcp_stream_cov_merge(
    nimcp_stream_cov_t dest,
    const nimcp_stream_cov_t src)
{
    if (!dest || !src) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(dest->magic, NIMCP_STREAM_COV_MAGIC) ||
        !validate_magic(src->magic, NIMCP_STREAM_COV_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (src->n == 0) return NIMCP_STREAM_OK;

    if (dest->mutex) nimcp_mutex_lock(dest->mutex);

    if (dest->n == 0) {
        dest->n = src->n;
        dest->mean_x = src->mean_x;
        dest->mean_y = src->mean_y;
        dest->m2_x = src->m2_x;
        dest->m2_y = src->m2_y;
        dest->c = src->c;
    } else {
        double na = (double)dest->n;
        double nb = (double)src->n;
        double n = na + nb;

        double dx = src->mean_x - dest->mean_x;
        double dy = src->mean_y - dest->mean_y;

        dest->mean_x = (na * dest->mean_x + nb * src->mean_x) / n;
        dest->mean_y = (na * dest->mean_y + nb * src->mean_y) / n;

        dest->m2_x += src->m2_x + dx * dx * na * nb / n;
        dest->m2_y += src->m2_y + dy * dy * na * nb / n;
        dest->c += src->c + dx * dy * na * nb / n;

        dest->n += src->n;
    }

    if (dest->mutex) nimcp_mutex_unlock(dest->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_stream_cov_reset(nimcp_stream_cov_t cov)
{
    if (!cov) return NIMCP_STREAM_ERROR_NULL;
    if (!validate_magic(cov->magic, NIMCP_STREAM_COV_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (cov->mutex) nimcp_mutex_lock(cov->mutex);

    cov->n = 0;
    cov->mean_x = 0.0;
    cov->mean_y = 0.0;
    cov->m2_x = 0.0;
    cov->m2_y = 0.0;
    cov->c = 0.0;

    if (cov->mutex) nimcp_mutex_unlock(cov->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_stream_cov_matrix_t nimcp_stream_cov_matrix_create(uint32_t n_dims)
{
    if (n_dims == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stream_cov_matrix_create: n_dims is zero");
        return NULL;
    }

    struct nimcp_stream_cov_matrix_s* cov = nimcp_calloc(1, sizeof(*cov));
    if (!cov) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate covariance matrix");
        return NULL;
    }

    cov->magic = NIMCP_STREAM_COV_MAGIC;
    cov->n_dims = n_dims;
    cov->n = 0;

    cov->means = nimcp_calloc(n_dims, sizeof(double));
    cov->cov = nimcp_calloc(n_dims * n_dims, sizeof(double));

    if (!cov->means || !cov->cov) {
        nimcp_free(cov->means);
        nimcp_free(cov->cov);
        nimcp_free(cov);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stream_cov_matrix_create: required parameter is NULL (cov->means, cov->cov)");
        return NULL;
    }

    if (g_stream_stats.config.enable_thread_safety) {
        cov->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
        if (cov->mutex) {
            nimcp_mutex_init(cov->mutex, NULL);
        }
    }

    return cov;
}

void nimcp_stream_cov_matrix_destroy(nimcp_stream_cov_matrix_t cov)
{
    if (!cov) return;
    if (!validate_magic(cov->magic, NIMCP_STREAM_COV_MAGIC)) return;

    nimcp_free(cov->means);
    nimcp_free(cov->cov);

    if (cov->mutex) {
        nimcp_mutex_destroy(cov->mutex);
        nimcp_free(cov->mutex);
    }

    cov->magic = 0;
    nimcp_free(cov);
}

nimcp_stream_stats_result_t nimcp_stream_cov_matrix_update(
    nimcp_stream_cov_matrix_t cov,
    const float* values)
{
    if (!cov || !values) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(cov->magic, NIMCP_STREAM_COV_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (cov->mutex) nimcp_mutex_lock(cov->mutex);

    cov->n++;
    double n = (double)cov->n;
    uint32_t d = cov->n_dims;

    /* Heap-allocate delta — dimension count can be large, alloca() risks
     * stack overflow. */
    double* delta = (double*)nimcp_malloc(d * sizeof(double));
    if (!delta) {
        if (cov->mutex) nimcp_mutex_unlock(cov->mutex);
        return NIMCP_STREAM_ERROR_MEMORY;
    }
    for (uint32_t i = 0; i < d; i++) {
        delta[i] = (double)values[i] - cov->means[i];
    }

    /* Update means */
    for (uint32_t i = 0; i < d; i++) {
        cov->means[i] += delta[i] / n;
    }

    /* Update covariance matrix (upper triangular) */
    for (uint32_t i = 0; i < d; i++) {
        double delta2_i = (double)values[i] - cov->means[i];
        for (uint32_t j = i; j < d; j++) {
            double delta2_j = (double)values[j] - cov->means[j];
            cov->cov[i * d + j] += delta[i] * delta2_j;
        }
    }

    nimcp_free(delta);
    if (cov->mutex) nimcp_mutex_unlock(cov->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_stream_cov_matrix_get(
    const nimcp_stream_cov_matrix_t cov,
    float* out_matrix)
{
    if (!cov || !out_matrix) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(cov->magic, NIMCP_STREAM_COV_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }
    if (cov->n < 2) {
        return NIMCP_STREAM_ERROR_EMPTY;
    }

    uint32_t d = cov->n_dims;
    double denom = (double)(cov->n - 1);

    /* Fill symmetric matrix */
    for (uint32_t i = 0; i < d; i++) {
        for (uint32_t j = i; j < d; j++) {
            float val = (float)(cov->cov[i * d + j] / denom);
            out_matrix[i * d + j] = val;
            out_matrix[j * d + i] = val;
        }
    }

    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_stream_corr_matrix_get(
    const nimcp_stream_cov_matrix_t cov,
    float* out_matrix)
{
    if (!cov || !out_matrix) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(cov->magic, NIMCP_STREAM_COV_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }
    if (cov->n < 2) {
        return NIMCP_STREAM_ERROR_EMPTY;
    }

    uint32_t d = cov->n_dims;
    double denom = (double)(cov->n - 1);

    /* Heap-allocate to avoid stack overflow with large dimension counts */
    double* stds = (double*)nimcp_malloc(d * sizeof(double));
    if (!stds) return NIMCP_STREAM_ERROR_MEMORY;

    for (uint32_t i = 0; i < d; i++) {
        stds[i] = sqrt(cov->cov[i * d + i] / denom);
    }

    /* Fill correlation matrix */
    for (uint32_t i = 0; i < d; i++) {
        for (uint32_t j = i; j < d; j++) {
            float val;
            if (stds[i] == 0.0 || stds[j] == 0.0) {
                val = (i == j) ? 1.0f : 0.0f;
            } else {
                val = (float)((cov->cov[i * d + j] / denom) / (stds[i] * stds[j]));
            }
            out_matrix[i * d + j] = val;
            out_matrix[j * d + i] = val;
        }
    }

    nimcp_free(stds);
    return NIMCP_STREAM_OK;
}

//=============================================================================
// Online PCA Implementation (Simplified)
//=============================================================================

nimcp_stream_pca_t nimcp_stream_pca_create(const nimcp_stream_pca_config_t* config)
{
    if (!config || config->n_components == 0 || config->n_features == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stream_pca_create: config is NULL");
        return NULL;
    }
    if (config->n_components > NIMCP_STREAM_PCA_MAX_COMPONENTS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_stream_pca_create: validation failed");
        return NULL;
    }

    struct nimcp_stream_pca_s* pca = nimcp_calloc(1, sizeof(*pca));
    if (!pca) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate PCA");
        return NULL;
    }

    pca->magic = NIMCP_STREAM_PCA_MAGIC;
    pca->n_components = config->n_components;
    pca->n_features = config->n_features;
    pca->n_samples = 0;
    pca->forgetting_factor = config->forgetting_factor > 0 ? config->forgetting_factor : 1.0f;
    pca->whiten = config->whiten;

    pca->components = nimcp_calloc(config->n_components * config->n_features, sizeof(float));
    pca->mean = nimcp_calloc(config->n_features, sizeof(float));
    pca->variance = nimcp_calloc(config->n_components, sizeof(float));
    pca->singular_values = nimcp_calloc(config->n_components, sizeof(float));

    if (!pca->components || !pca->mean || !pca->variance || !pca->singular_values) {
        nimcp_free(pca->components);
        nimcp_free(pca->mean);
        nimcp_free(pca->variance);
        nimcp_free(pca->singular_values);
        nimcp_free(pca);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stream_pca_create: required parameter is NULL (pca->components, pca->mean, pca->variance, pca->singular_values)");
        return NULL;
    }

    if (g_stream_stats.config.enable_thread_safety) {
        pca->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
        if (pca->mutex) {
            nimcp_mutex_init(pca->mutex, NULL);
        }
    }

    return pca;
}

void nimcp_stream_pca_destroy(nimcp_stream_pca_t pca)
{
    if (!pca) return;
    if (!validate_magic(pca->magic, NIMCP_STREAM_PCA_MAGIC)) return;

    nimcp_free(pca->components);
    nimcp_free(pca->mean);
    nimcp_free(pca->variance);
    nimcp_free(pca->singular_values);

    if (pca->mutex) {
        nimcp_mutex_destroy(pca->mutex);
        nimcp_free(pca->mutex);
    }

    pca->magic = 0;
    nimcp_free(pca);
}

nimcp_stream_stats_result_t nimcp_stream_pca_partial_fit(
    nimcp_stream_pca_t pca,
    const float* data,
    uint32_t n_samples)
{
    if (!pca || !data) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(pca->magic, NIMCP_STREAM_PCA_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }
    if (n_samples == 0) {
        return NIMCP_STREAM_OK;
    }

    if (pca->mutex) nimcp_mutex_lock(pca->mutex);

    uint32_t nf = pca->n_features;
    uint32_t nc = pca->n_components;

    /* Update running mean */
    for (uint32_t s = 0; s < n_samples; s++) {
        pca->n_samples++;
        double n = (double)pca->n_samples;
        const float* x = data + s * nf;

        for (uint32_t i = 0; i < nf; i++) {
            pca->mean[i] += ((float)x[i] - pca->mean[i]) / (float)n;
        }
    }

    /* For a full implementation, we would use incremental SVD here */
    /* This is a simplified placeholder that initializes components randomly */
    if (pca->n_samples == n_samples) {
        /* First batch - initialize components with simple heuristic */
        for (uint32_t c = 0; c < nc; c++) {
            float norm = 0.0f;
            for (uint32_t i = 0; i < nf; i++) {
                float val = (float)((random_double() - 0.5) * 2.0);
                pca->components[c * nf + i] = val;
                norm += val * val;
            }
            norm = sqrtf(norm);
            for (uint32_t i = 0; i < nf; i++) {
                pca->components[c * nf + i] /= norm;
            }
            pca->variance[c] = 1.0f;
            pca->singular_values[c] = 1.0f;
        }
    }

    if (pca->mutex) nimcp_mutex_unlock(pca->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_stream_pca_transform(
    const nimcp_stream_pca_t pca,
    const float* data,
    uint32_t n_samples,
    float* transformed)
{
    if (!pca || !data || !transformed) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(pca->magic, NIMCP_STREAM_PCA_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }
    if (pca->n_samples == 0) {
        return NIMCP_STREAM_ERROR_EMPTY;
    }

    uint32_t nf = pca->n_features;
    uint32_t nc = pca->n_components;

    /* Project each sample onto components */
    for (uint32_t s = 0; s < n_samples; s++) {
        const float* x = data + s * nf;
        float* out = transformed + s * nc;

        for (uint32_t c = 0; c < nc; c++) {
            float sum = 0.0f;
            const float* comp = pca->components + c * nf;
            for (uint32_t i = 0; i < nf; i++) {
                sum += (x[i] - pca->mean[i]) * comp[i];
            }
            out[c] = sum;
            if (pca->whiten && pca->variance[c] > 0) {
                out[c] /= sqrtf(pca->variance[c]);
            }
        }
    }

    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_stream_pca_get_components(
    const nimcp_stream_pca_t pca,
    float* components)
{
    if (!pca || !components) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(pca->magic, NIMCP_STREAM_PCA_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    memcpy(components, pca->components,
           pca->n_components * pca->n_features * sizeof(float));
    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_stream_pca_get_explained_variance(
    const nimcp_stream_pca_t pca,
    float* variances)
{
    if (!pca || !variances) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(pca->magic, NIMCP_STREAM_PCA_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    memcpy(variances, pca->variance, pca->n_components * sizeof(float));
    return NIMCP_STREAM_OK;
}

uint64_t nimcp_stream_pca_get_n_samples(const nimcp_stream_pca_t pca)
{
    if (!pca || !validate_magic(pca->magic, NIMCP_STREAM_PCA_MAGIC)) {
        return 0;
    }
    return pca->n_samples;
}

nimcp_stream_stats_result_t nimcp_stream_pca_reset(nimcp_stream_pca_t pca)
{
    if (!pca) return NIMCP_STREAM_ERROR_NULL;
    if (!validate_magic(pca->magic, NIMCP_STREAM_PCA_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (pca->mutex) nimcp_mutex_lock(pca->mutex);

    pca->n_samples = 0;
    memset(pca->components, 0, pca->n_components * pca->n_features * sizeof(float));
    memset(pca->mean, 0, pca->n_features * sizeof(float));
    memset(pca->variance, 0, pca->n_components * sizeof(float));
    memset(pca->singular_values, 0, pca->n_components * sizeof(float));

    if (pca->mutex) nimcp_mutex_unlock(pca->mutex);
    return NIMCP_STREAM_OK;
}

//=============================================================================
// Online Linear Regression Implementation
//=============================================================================

nimcp_stream_linreg_t nimcp_stream_linreg_create(
    uint32_t n_features,
    float forgetting_factor)
{
    if (n_features == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stream_linreg_create: n_features is zero");
        return NULL;
    }

    struct nimcp_stream_linreg_s* reg = nimcp_calloc(1, sizeof(*reg));
    if (!reg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate linear regression");
        return NULL;
    }

    reg->magic = NIMCP_STREAM_LINREG_MAGIC;
    reg->n_features = n_features;
    reg->n_samples = 0;
    reg->forgetting_factor = (forgetting_factor > 0 && forgetting_factor <= 1.0f) ?
        forgetting_factor : 1.0f;

    uint32_t np1 = n_features + 1;
    reg->coefficients = nimcp_calloc(np1, sizeof(float));
    reg->P = nimcp_calloc(np1 * np1, sizeof(float));

    if (!reg->coefficients || !reg->P) {
        nimcp_free(reg->coefficients);
        nimcp_free(reg->P);
        nimcp_free(reg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stream_linreg_create: required parameter is NULL (reg->coefficients, reg->P)");
        return NULL;
    }

    /* Initialize P as identity matrix times large value */
    for (uint32_t i = 0; i < np1; i++) {
        reg->P[i * np1 + i] = 1000.0f;
    }

    if (g_stream_stats.config.enable_thread_safety) {
        reg->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
        if (reg->mutex) {
            nimcp_mutex_init(reg->mutex, NULL);
        }
    }

    return reg;
}

void nimcp_stream_linreg_destroy(nimcp_stream_linreg_t reg)
{
    if (!reg) return;
    if (!validate_magic(reg->magic, NIMCP_STREAM_LINREG_MAGIC)) return;

    nimcp_free(reg->coefficients);
    nimcp_free(reg->P);

    if (reg->mutex) {
        nimcp_mutex_destroy(reg->mutex);
        nimcp_free(reg->mutex);
    }

    reg->magic = 0;
    nimcp_free(reg);
}

nimcp_stream_stats_result_t nimcp_stream_linreg_update(
    nimcp_stream_linreg_t reg,
    const float* x,
    float y)
{
    if (!reg || !x) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(reg->magic, NIMCP_STREAM_LINREG_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (reg->mutex) nimcp_mutex_lock(reg->mutex);

    uint32_t np1 = reg->n_features + 1;
    float lambda = reg->forgetting_factor;

    /* Heap-allocate scratch buffers — n_features can be large */
    float* x_ext = (float*)nimcp_malloc(np1 * sizeof(float));
    float* Px = (float*)nimcp_malloc(np1 * sizeof(float));
    float* K = (float*)nimcp_malloc(np1 * sizeof(float));
    if (!x_ext || !Px || !K) {
        nimcp_free(x_ext); nimcp_free(Px); nimcp_free(K);
        if (reg->mutex) nimcp_mutex_unlock(reg->mutex);
        return NIMCP_STREAM_ERROR_MEMORY;
    }

    /* Extended feature vector with intercept [x, 1] */
    memcpy(x_ext, x, reg->n_features * sizeof(float));
    x_ext[reg->n_features] = 1.0f;

    /* Prediction error */
    float y_pred = 0.0f;
    for (uint32_t i = 0; i < np1; i++) {
        y_pred += reg->coefficients[i] * x_ext[i];
    }
    float error = y - y_pred;

    /* P * x */
    for (uint32_t i = 0; i < np1; i++) {
        Px[i] = 0.0f;
        for (uint32_t j = 0; j < np1; j++) {
            Px[i] += reg->P[i * np1 + j] * x_ext[j];
        }
    }

    /* x' * P * x */
    float xPx = 0.0f;
    for (uint32_t i = 0; i < np1; i++) {
        xPx += x_ext[i] * Px[i];
    }

    /* Kalman gain: K = P * x / (lambda + x' * P * x) */
    float denom = lambda + xPx;
    for (uint32_t i = 0; i < np1; i++) {
        K[i] = Px[i] / denom;
    }

    /* Update coefficients: w = w + K * error */
    for (uint32_t i = 0; i < np1; i++) {
        reg->coefficients[i] += K[i] * error;
    }

    /* Update P: P = (P - K * x' * P) / lambda */
    for (uint32_t i = 0; i < np1; i++) {
        for (uint32_t j = 0; j < np1; j++) {
            reg->P[i * np1 + j] = (reg->P[i * np1 + j] - K[i] * Px[j]) / lambda;
        }
    }

    /* Update R-squared statistics */
    reg->n_samples++;
    double n = (double)reg->n_samples;
    double delta = y - reg->y_mean;
    reg->y_mean += delta / n;
    reg->ss_tot += delta * (y - reg->y_mean);
    reg->ss_res += error * error;

    nimcp_free(x_ext); nimcp_free(Px); nimcp_free(K);
    if (reg->mutex) nimcp_mutex_unlock(reg->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_stream_linreg_update_batch(
    nimcp_stream_linreg_t reg,
    const float* X,
    const float* y,
    uint32_t n_samples)
{
    if (!reg || !X || !y) {
        return NIMCP_STREAM_ERROR_NULL;
    }

    for (uint32_t s = 0; s < n_samples; s++) {
        nimcp_stream_stats_result_t result =
            nimcp_stream_linreg_update(reg, X + s * reg->n_features, y[s]);
        if (result != NIMCP_STREAM_OK) {
            return result;
        }
    }
    return NIMCP_STREAM_OK;
}

float nimcp_stream_linreg_predict(
    const nimcp_stream_linreg_t reg,
    const float* x)
{
    if (!reg || !x) return NAN;
    if (!validate_magic(reg->magic, NIMCP_STREAM_LINREG_MAGIC)) return NAN;

    float y = reg->coefficients[reg->n_features]; /* intercept */
    for (uint32_t i = 0; i < reg->n_features; i++) {
        y += reg->coefficients[i] * x[i];
    }
    return y;
}

nimcp_stream_stats_result_t nimcp_stream_linreg_predict_batch(
    const nimcp_stream_linreg_t reg,
    const float* X,
    uint32_t n_samples,
    float* predictions)
{
    if (!reg || !X || !predictions) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(reg->magic, NIMCP_STREAM_LINREG_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    for (uint32_t s = 0; s < n_samples; s++) {
        predictions[s] = nimcp_stream_linreg_predict(reg, X + s * reg->n_features);
    }
    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_stream_linreg_get_coefficients(
    const nimcp_stream_linreg_t reg,
    float* coefficients)
{
    if (!reg || !coefficients) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(reg->magic, NIMCP_STREAM_LINREG_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    memcpy(coefficients, reg->coefficients, (reg->n_features + 1) * sizeof(float));
    return NIMCP_STREAM_OK;
}

float nimcp_stream_linreg_get_r_squared(const nimcp_stream_linreg_t reg)
{
    if (!reg || !validate_magic(reg->magic, NIMCP_STREAM_LINREG_MAGIC)) {
        return NAN;
    }
    if (reg->n_samples < 2 || reg->ss_tot == 0.0) {
        return NAN;
    }
    return (float)(1.0 - reg->ss_res / reg->ss_tot);
}

nimcp_stream_stats_result_t nimcp_stream_linreg_reset(nimcp_stream_linreg_t reg)
{
    if (!reg) return NIMCP_STREAM_ERROR_NULL;
    if (!validate_magic(reg->magic, NIMCP_STREAM_LINREG_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (reg->mutex) nimcp_mutex_lock(reg->mutex);

    uint32_t np1 = reg->n_features + 1;

    memset(reg->coefficients, 0, np1 * sizeof(float));
    memset(reg->P, 0, np1 * np1 * sizeof(float));
    for (uint32_t i = 0; i < np1; i++) {
        reg->P[i * np1 + i] = 1000.0f;
    }

    reg->n_samples = 0;
    reg->ss_res = 0.0;
    reg->ss_tot = 0.0;
    reg->y_mean = 0.0;

    if (reg->mutex) nimcp_mutex_unlock(reg->mutex);
    return NIMCP_STREAM_OK;
}

//=============================================================================
// Reservoir Sampling Implementation
//=============================================================================

nimcp_reservoir_t nimcp_reservoir_create(uint32_t reservoir_size)
{
    if (reservoir_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_reservoir_create: reservoir_size is zero");
        return NULL;
    }

    struct nimcp_reservoir_s* res = nimcp_calloc(1, sizeof(*res));
    if (!res) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate reservoir");
        return NULL;
    }

    res->magic = NIMCP_RESERVOIR_MAGIC;
    res->capacity = reservoir_size;
    res->size = 0;
    res->stream_count = 0;
    res->weighted = false;

    res->samples = nimcp_calloc(reservoir_size, sizeof(float));
    if (!res->samples) {
        nimcp_free(res);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_reservoir_create: res->samples is NULL");
        return NULL;
    }

    if (g_stream_stats.config.enable_thread_safety) {
        res->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
        if (res->mutex) {
            nimcp_mutex_init(res->mutex, NULL);
        }
    }

    return res;
}

void nimcp_reservoir_destroy(nimcp_reservoir_t reservoir)
{
    if (!reservoir) return;
    if (!validate_magic(reservoir->magic, NIMCP_RESERVOIR_MAGIC)) return;

    nimcp_free(reservoir->samples);
    nimcp_free(reservoir->weights);

    if (reservoir->mutex) {
        nimcp_mutex_destroy(reservoir->mutex);
        nimcp_free(reservoir->mutex);
    }

    reservoir->magic = 0;
    nimcp_free(reservoir);
}

nimcp_stream_stats_result_t nimcp_reservoir_update(
    nimcp_reservoir_t reservoir,
    float value)
{
    if (!reservoir) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(reservoir->magic, NIMCP_RESERVOIR_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (reservoir->mutex) nimcp_mutex_lock(reservoir->mutex);

    reservoir->stream_count++;

    if (reservoir->size < reservoir->capacity) {
        /* Fill phase */
        reservoir->samples[reservoir->size++] = value;
    } else {
        /* Replace phase (Algorithm R) */
        uint64_t j = (uint64_t)(random_double() * (double)reservoir->stream_count);
        if (j < reservoir->capacity) {
            reservoir->samples[j] = value;
        }
    }

    if (reservoir->mutex) nimcp_mutex_unlock(reservoir->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_reservoir_update_batch(
    nimcp_reservoir_t reservoir,
    const float* values,
    uint32_t count)
{
    if (!reservoir || !values) {
        return NIMCP_STREAM_ERROR_NULL;
    }

    for (uint32_t i = 0; i < count; i++) {
        nimcp_stream_stats_result_t result = nimcp_reservoir_update(reservoir, values[i]);
        if (result != NIMCP_STREAM_OK) {
            return result;
        }
    }
    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_reservoir_get_sample(
    const nimcp_reservoir_t reservoir,
    float* sample,
    uint32_t* actual_size)
{
    if (!reservoir || !sample || !actual_size) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(reservoir->magic, NIMCP_RESERVOIR_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    *actual_size = reservoir->size;
    memcpy(sample, reservoir->samples, reservoir->size * sizeof(float));
    return NIMCP_STREAM_OK;
}

uint64_t nimcp_reservoir_get_stream_size(const nimcp_reservoir_t reservoir)
{
    if (!reservoir || !validate_magic(reservoir->magic, NIMCP_RESERVOIR_MAGIC)) {
        return 0;
    }
    return reservoir->stream_count;
}

nimcp_stream_stats_result_t nimcp_reservoir_reset(nimcp_reservoir_t reservoir)
{
    if (!reservoir) return NIMCP_STREAM_ERROR_NULL;
    if (!validate_magic(reservoir->magic, NIMCP_RESERVOIR_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (reservoir->mutex) nimcp_mutex_lock(reservoir->mutex);

    reservoir->size = 0;
    reservoir->stream_count = 0;

    if (reservoir->mutex) nimcp_mutex_unlock(reservoir->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_reservoir_t nimcp_reservoir_weighted_create(uint32_t reservoir_size)
{
    nimcp_reservoir_t res = nimcp_reservoir_create(reservoir_size);
    if (!res) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_reservoir_weighted_create: res is NULL");
        return NULL;
    }

    res->weighted = true;
    res->weights = nimcp_calloc(reservoir_size, sizeof(float));
    if (!res->weights) {
        nimcp_reservoir_destroy(res);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_reservoir_weighted_create: res->weights is NULL");
        return NULL;
    }

    return res;
}

nimcp_stream_stats_result_t nimcp_reservoir_weighted_update(
    nimcp_reservoir_t reservoir,
    float value,
    float weight)
{
    if (!reservoir) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(reservoir->magic, NIMCP_RESERVOIR_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }
    if (!reservoir->weighted || !reservoir->weights) {
        return NIMCP_STREAM_ERROR_INVALID;
    }
    if (weight <= 0) {
        return NIMCP_STREAM_ERROR_PARAMS;
    }

    if (reservoir->mutex) nimcp_mutex_lock(reservoir->mutex);

    reservoir->stream_count++;

    /* Algorithm A-Res: key = u^(1/w) where u is uniform random */
    double u = random_double();
    double key = pow(u, 1.0 / weight);

    if (reservoir->size < reservoir->capacity) {
        reservoir->samples[reservoir->size] = value;
        reservoir->weights[reservoir->size] = (float)key;
        reservoir->size++;
    } else {
        /* Find minimum key */
        uint32_t min_idx = 0;
        float min_key = reservoir->weights[0];
        for (uint32_t i = 1; i < reservoir->capacity; i++) {
            if (reservoir->weights[i] < min_key) {
                min_key = reservoir->weights[i];
                min_idx = i;
            }
        }

        if ((float)key > min_key) {
            reservoir->samples[min_idx] = value;
            reservoir->weights[min_idx] = (float)key;
        }
    }

    if (reservoir->mutex) nimcp_mutex_unlock(reservoir->mutex);
    return NIMCP_STREAM_OK;
}

//=============================================================================
// Count-Min Sketch Implementation
//=============================================================================

nimcp_cms_t nimcp_cms_create(uint32_t width, uint32_t depth)
{
    if (width == 0 || depth == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cms_create: width is zero");
        return NULL;
    }

    struct nimcp_cms_s* cms = nimcp_calloc(1, sizeof(*cms));
    if (!cms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate Count-Min Sketch");
        return NULL;
    }

    cms->magic = NIMCP_CMS_MAGIC;
    cms->width = width;
    cms->depth = depth;
    cms->total_count = 0;

    cms->counters = nimcp_calloc(width * depth, sizeof(uint32_t));
    cms->hash_seeds = nimcp_calloc(depth, sizeof(uint64_t));

    if (!cms->counters || !cms->hash_seeds) {
        nimcp_free(cms->counters);
        nimcp_free(cms->hash_seeds);
        nimcp_free(cms);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cms_create: required parameter is NULL (cms->counters, cms->hash_seeds)");
        return NULL;
    }

    /* Initialize hash seeds */
    for (uint32_t i = 0; i < depth; i++) {
        cms->hash_seeds[i] = (uint64_t)(random_double() * (double)UINT64_MAX);
    }

    if (g_stream_stats.config.enable_thread_safety) {
        cms->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
        if (cms->mutex) {
            nimcp_mutex_init(cms->mutex, NULL);
        }
    }

    return cms;
}

void nimcp_cms_destroy(nimcp_cms_t cms)
{
    if (!cms) return;
    if (!validate_magic(cms->magic, NIMCP_CMS_MAGIC)) return;

    nimcp_free(cms->counters);
    nimcp_free(cms->hash_seeds);

    if (cms->mutex) {
        nimcp_mutex_destroy(cms->mutex);
        nimcp_free(cms->mutex);
    }

    cms->magic = 0;
    nimcp_free(cms);
}

static inline uint32_t cms_hash(uint64_t item, uint64_t seed, uint32_t width)
{
    uint64_t hash = item * seed;
    hash ^= hash >> 33;
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= hash >> 33;
    return (uint32_t)(hash % width);
}

nimcp_stream_stats_result_t nimcp_cms_update(
    nimcp_cms_t cms,
    uint64_t item,
    int32_t count)
{
    if (!cms) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(cms->magic, NIMCP_CMS_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (cms->mutex) nimcp_mutex_lock(cms->mutex);

    for (uint32_t i = 0; i < cms->depth; i++) {
        uint32_t idx = cms_hash(item, cms->hash_seeds[i], cms->width);
        cms->counters[i * cms->width + idx] += count;
    }
    cms->total_count += count;

    if (cms->mutex) nimcp_mutex_unlock(cms->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_cms_update_string(
    nimcp_cms_t cms,
    const char* key,
    int32_t count)
{
    if (!cms || !key) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    uint64_t hash = fnv1a_hash(key, strlen(key));
    return nimcp_cms_update(cms, hash, count);
}

uint64_t nimcp_cms_query(const nimcp_cms_t cms, uint64_t item)
{
    if (!cms || !validate_magic(cms->magic, NIMCP_CMS_MAGIC)) {
        return 0;
    }

    uint64_t min_count = UINT64_MAX;
    for (uint32_t i = 0; i < cms->depth; i++) {
        uint32_t idx = cms_hash(item, cms->hash_seeds[i], cms->width);
        uint64_t count = cms->counters[i * cms->width + idx];
        if (count < min_count) {
            min_count = count;
        }
    }
    return min_count;
}

uint64_t nimcp_cms_query_string(const nimcp_cms_t cms, const char* key)
{
    if (!cms || !key) return 0;
    uint64_t hash = fnv1a_hash(key, strlen(key));
    return nimcp_cms_query(cms, hash);
}

nimcp_stream_stats_result_t nimcp_cms_merge(nimcp_cms_t dest, const nimcp_cms_t src)
{
    if (!dest || !src) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(dest->magic, NIMCP_CMS_MAGIC) ||
        !validate_magic(src->magic, NIMCP_CMS_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }
    if (dest->width != src->width || dest->depth != src->depth) {
        return NIMCP_STREAM_ERROR_MISMATCH;
    }

    if (dest->mutex) nimcp_mutex_lock(dest->mutex);

    uint32_t total = dest->width * dest->depth;
    for (uint32_t i = 0; i < total; i++) {
        dest->counters[i] += src->counters[i];
    }
    dest->total_count += src->total_count;

    if (dest->mutex) nimcp_mutex_unlock(dest->mutex);
    return NIMCP_STREAM_OK;
}

uint64_t nimcp_cms_get_total_count(const nimcp_cms_t cms)
{
    if (!cms || !validate_magic(cms->magic, NIMCP_CMS_MAGIC)) {
        return 0;
    }
    return cms->total_count;
}

nimcp_stream_stats_result_t nimcp_cms_reset(nimcp_cms_t cms)
{
    if (!cms) return NIMCP_STREAM_ERROR_NULL;
    if (!validate_magic(cms->magic, NIMCP_CMS_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (cms->mutex) nimcp_mutex_lock(cms->mutex);

    memset(cms->counters, 0, cms->width * cms->depth * sizeof(uint32_t));
    cms->total_count = 0;

    if (cms->mutex) nimcp_mutex_unlock(cms->mutex);
    return NIMCP_STREAM_OK;
}

//=============================================================================
// HyperLogLog Implementation
//=============================================================================

nimcp_hll_t nimcp_hll_create(uint32_t precision)
{
    if (precision < 4 || precision > 18) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hll_create: validation failed");
        return NULL;
    }

    struct nimcp_hll_s* hll = nimcp_calloc(1, sizeof(*hll));
    if (!hll) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate HyperLogLog");
        return NULL;
    }

    hll->magic = NIMCP_HLL_MAGIC;
    hll->precision = precision;
    hll->n_buckets = 1U << precision;

    hll->buckets = nimcp_calloc(hll->n_buckets, sizeof(uint8_t));
    if (!hll->buckets) {
        nimcp_free(hll);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hll_create: hll->buckets is NULL");
        return NULL;
    }

    if (g_stream_stats.config.enable_thread_safety) {
        hll->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
        if (hll->mutex) {
            nimcp_mutex_init(hll->mutex, NULL);
        }
    }

    return hll;
}

void nimcp_hll_destroy(nimcp_hll_t hll)
{
    if (!hll) return;
    if (!validate_magic(hll->magic, NIMCP_HLL_MAGIC)) return;

    nimcp_free(hll->buckets);

    if (hll->mutex) {
        nimcp_mutex_destroy(hll->mutex);
        nimcp_free(hll->mutex);
    }

    hll->magic = 0;
    nimcp_free(hll);
}

nimcp_stream_stats_result_t nimcp_hll_add(nimcp_hll_t hll, uint64_t item)
{
    if (!hll) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(hll->magic, NIMCP_HLL_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    /* Hash the item */
    uint64_t hash = fnv1a_hash(&item, sizeof(item));

    /* Extract bucket index and remaining bits */
    uint32_t idx = hash >> (64 - hll->precision);
    uint64_t remaining = hash << hll->precision | (1ULL << (hll->precision - 1));

    /* Count leading zeros + 1 */
    uint8_t rho = (uint8_t)(clz64(remaining) + 1);

    if (hll->mutex) nimcp_mutex_lock(hll->mutex);

    if (rho > hll->buckets[idx]) {
        hll->buckets[idx] = rho;
    }

    if (hll->mutex) nimcp_mutex_unlock(hll->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_hll_add_string(nimcp_hll_t hll, const char* key)
{
    if (!hll || !key) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    uint64_t hash = fnv1a_hash(key, strlen(key));
    return nimcp_hll_add(hll, hash);
}

nimcp_stream_stats_result_t nimcp_hll_add_batch(
    nimcp_hll_t hll,
    const uint64_t* items,
    uint32_t count)
{
    if (!hll || !items) {
        return NIMCP_STREAM_ERROR_NULL;
    }

    for (uint32_t i = 0; i < count; i++) {
        nimcp_stream_stats_result_t result = nimcp_hll_add(hll, items[i]);
        if (result != NIMCP_STREAM_OK) {
            return result;
        }
    }
    return NIMCP_STREAM_OK;
}

uint64_t nimcp_hll_count(const nimcp_hll_t hll)
{
    if (!hll || !validate_magic(hll->magic, NIMCP_HLL_MAGIC)) {
        return 0;
    }

    double alpha;
    uint32_t m = hll->n_buckets;

    /* Alpha values for bias correction */
    switch (hll->precision) {
        case 4: alpha = 0.673; break;
        case 5: alpha = 0.697; break;
        case 6: alpha = 0.709; break;
        default: alpha = 0.7213 / (1.0 + 1.079 / m); break;
    }

    /* Harmonic mean of 2^(-M[j]) */
    double sum = 0.0;
    uint32_t zeros = 0;

    for (uint32_t i = 0; i < m; i++) {
        sum += 1.0 / (double)(1ULL << hll->buckets[i]);
        if (hll->buckets[i] == 0) {
            zeros++;
        }
    }

    double estimate = alpha * m * m / sum;

    /* Linear counting correction for small cardinalities */
    if (estimate <= 2.5 * m && zeros > 0) {
        estimate = m * log((double)m / zeros);
    }

    /* Large range correction */
    if (estimate > (1.0 / 30.0) * (double)(1ULL << 32)) {
        estimate = -(double)(1ULL << 32) * log(1.0 - estimate / (double)(1ULL << 32));
    }

    return (uint64_t)(estimate + 0.5);
}

nimcp_stream_stats_result_t nimcp_hll_merge(nimcp_hll_t dest, const nimcp_hll_t src)
{
    if (!dest || !src) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(dest->magic, NIMCP_HLL_MAGIC) ||
        !validate_magic(src->magic, NIMCP_HLL_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }
    if (dest->precision != src->precision) {
        return NIMCP_STREAM_ERROR_MISMATCH;
    }

    if (dest->mutex) nimcp_mutex_lock(dest->mutex);

    for (uint32_t i = 0; i < dest->n_buckets; i++) {
        if (src->buckets[i] > dest->buckets[i]) {
            dest->buckets[i] = src->buckets[i];
        }
    }

    if (dest->mutex) nimcp_mutex_unlock(dest->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_stream_stats_result_t nimcp_hll_reset(nimcp_hll_t hll)
{
    if (!hll) return NIMCP_STREAM_ERROR_NULL;
    if (!validate_magic(hll->magic, NIMCP_HLL_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (hll->mutex) nimcp_mutex_lock(hll->mutex);

    memset(hll->buckets, 0, hll->n_buckets);

    if (hll->mutex) nimcp_mutex_unlock(hll->mutex);
    return NIMCP_STREAM_OK;
}

//=============================================================================
// EWMA/EWMV Implementation
//=============================================================================

nimcp_ewma_t nimcp_ewma_create(float alpha)
{
    if (alpha <= 0.0f || alpha > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_ewma_create: validation failed");
        return NULL;
    }

    struct nimcp_ewma_s* ewma = nimcp_calloc(1, sizeof(*ewma));
    if (!ewma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate EWMA");
        return NULL;
    }

    ewma->magic = NIMCP_EWMA_MAGIC;
    ewma->alpha = alpha;
    ewma->ewma = 0.0;
    ewma->ewmv = 0.0;
    ewma->count = 0;
    ewma->has_variance = false;

    if (g_stream_stats.config.enable_thread_safety) {
        ewma->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
        if (ewma->mutex) {
            nimcp_mutex_init(ewma->mutex, NULL);
        }
    }

    return ewma;
}

void nimcp_ewma_destroy(nimcp_ewma_t ewma)
{
    if (!ewma) return;
    if (!validate_magic(ewma->magic, NIMCP_EWMA_MAGIC)) return;

    if (ewma->mutex) {
        nimcp_mutex_destroy(ewma->mutex);
        nimcp_free(ewma->mutex);
    }

    ewma->magic = 0;
    nimcp_free(ewma);
}

nimcp_stream_stats_result_t nimcp_ewma_update(nimcp_ewma_t ewma, double value)
{
    if (!ewma) {
        return NIMCP_STREAM_ERROR_NULL;
    }
    if (!validate_magic(ewma->magic, NIMCP_EWMA_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (ewma->mutex) nimcp_mutex_lock(ewma->mutex);

    ewma->count++;

    if (ewma->count == 1) {
        ewma->ewma = value;
        ewma->ewmv = 0.0;
    } else {
        double delta = value - ewma->ewma;
        ewma->ewma = ewma->alpha * value + (1.0 - ewma->alpha) * ewma->ewma;

        if (ewma->has_variance) {
            ewma->ewmv = (1.0 - ewma->alpha) * (ewma->ewmv + ewma->alpha * delta * delta);
        }
    }

    if (ewma->mutex) nimcp_mutex_unlock(ewma->mutex);
    return NIMCP_STREAM_OK;
}

double nimcp_ewma_get(const nimcp_ewma_t ewma)
{
    if (!ewma || !validate_magic(ewma->magic, NIMCP_EWMA_MAGIC)) {
        return NAN;
    }
    if (ewma->count == 0) return NAN;
    return ewma->ewma;
}

nimcp_stream_stats_result_t nimcp_ewma_reset(nimcp_ewma_t ewma)
{
    if (!ewma) return NIMCP_STREAM_ERROR_NULL;
    if (!validate_magic(ewma->magic, NIMCP_EWMA_MAGIC)) {
        return NIMCP_STREAM_ERROR_INVALID;
    }

    if (ewma->mutex) nimcp_mutex_lock(ewma->mutex);

    ewma->ewma = 0.0;
    ewma->ewmv = 0.0;
    ewma->count = 0;

    if (ewma->mutex) nimcp_mutex_unlock(ewma->mutex);
    return NIMCP_STREAM_OK;
}

nimcp_ewma_t nimcp_ewmv_create(float alpha)
{
    nimcp_ewma_t ewma = nimcp_ewma_create(alpha);
    if (ewma) {
        ewma->has_variance = true;
    }
    return ewma;
}

double nimcp_ewmv_get(const nimcp_ewma_t ewma)
{
    if (!ewma || !validate_magic(ewma->magic, NIMCP_EWMA_MAGIC)) {
        return NAN;
    }
    if (!ewma->has_variance || ewma->count < 2) return NAN;
    return ewma->ewmv;
}

double nimcp_ewms_get(const nimcp_ewma_t ewma)
{
    double var = nimcp_ewmv_get(ewma);
    if (isnan(var)) return NAN;
    return sqrt(var);
}

//=============================================================================
// GPU Acceleration Stubs
//=============================================================================

bool nimcp_stream_stats_gpu_available(void)
{
#ifdef NIMCP_ENABLE_CUDA
    return true;
#else
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stream_stats_gpu_available: operation failed");
    return false;
#endif
}

nimcp_stream_stats_result_t nimcp_stream_stats_gpu_batch(
    const float* values,
    uint32_t count,
    float* d_mean,
    float* d_variance,
    float* d_min,
    float* d_max)
{
    (void)values;
    (void)count;
    (void)d_mean;
    (void)d_variance;
    (void)d_min;
    (void)d_max;
    /* Implementation in GPU kernel file */
    return NIMCP_STREAM_ERROR_GPU;
}

nimcp_stream_stats_result_t nimcp_stream_cov_gpu_batch(
    const float* x,
    const float* y,
    uint32_t count,
    float* d_covariance)
{
    (void)x;
    (void)y;
    (void)count;
    (void)d_covariance;
    /* Implementation in GPU kernel file */
    return NIMCP_STREAM_ERROR_GPU;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_stream_stats_error_string(nimcp_stream_stats_result_t result)
{
    switch (result) {
        case NIMCP_STREAM_OK:           return "Success";
        case NIMCP_STREAM_ERROR_NULL:   return "NULL pointer argument";
        case NIMCP_STREAM_ERROR_INVALID: return "Invalid structure (bad magic)";
        case NIMCP_STREAM_ERROR_MEMORY: return "Memory allocation failed";
        case NIMCP_STREAM_ERROR_PARAMS: return "Invalid parameters";
        case NIMCP_STREAM_ERROR_EMPTY:  return "No data in accumulator";
        case NIMCP_STREAM_ERROR_MISMATCH: return "Dimension mismatch";
        case NIMCP_STREAM_ERROR_OVERFLOW: return "Numeric overflow";
        case NIMCP_STREAM_ERROR_GPU:    return "GPU operation failed";
        case NIMCP_STREAM_ERROR_NOT_INIT: return "Module not initialized";
        default: return "Unknown error";
    }
}

uint64_t nimcp_stream_hash_string(const char* key)
{
    if (!key) return 0;
    return fnv1a_hash(key, strlen(key));
}
