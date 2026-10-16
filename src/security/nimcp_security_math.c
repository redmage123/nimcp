/**
 * @file nimcp_security_math.c
 * @brief Mathematical Security Enhancements Implementation
 *
 * Phase SC-3: Mathematical Security Framework
 *
 * Implements:
 * 1. Shannon Entropy Anomaly Detection
 * 2. Bayesian Trust Propagation
 * 3. Differential Privacy for Audit Logs
 *
 * @version 1.0.0
 * @author NIMCP Security Team
 */

#include "security/nimcp_security_math.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "security_math"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_math, MESH_ADAPTER_CATEGORY_SECURITY)


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Baseline entropy record
 */
typedef struct {
    uint32_t region_id;
    double baseline_entropy;
    double baseline_stddev;
    uint64_t sample_count;
    bool active;
} entropy_baseline_t;

#define MAX_ENTROPY_BASELINES 256

/**
 * @brief Entropy analyzer internal structure
 */
struct nimcp_entropy_analyzer {
    nimcp_entropy_config_t config;
    entropy_baseline_t baselines[MAX_ENTROPY_BASELINES];
    uint32_t baseline_count;
    nimcp_mutex_t lock;
    bool initialized;
};

/**
 * @brief Trust network internal structure
 */
struct nimcp_trust_network {
    nimcp_trust_config_t config;
    nimcp_trust_entity_t entities[NIMCP_TRUST_MAX_ENTITIES];
    uint32_t entity_count;
    nimcp_mutex_t lock;
    bool initialized;
};

/**
 * @brief Differential privacy internal structure
 */
struct nimcp_dp_context {
    nimcp_dp_config_t config;
    double budget_spent;
    uint64_t queries_answered;
    nimcp_mutex_t lock;
    bool initialized;
};

//=============================================================================
// Random Number Generation (Thread-local state)
//=============================================================================

static __thread uint64_t rng_state = 0;

static void init_rng(void)
{
    if (rng_state == 0) {
        rng_state = (uint64_t)time(NULL) ^ (uint64_t)pthread_self();
    }
}

/* xorshift64* PRNG */
static uint64_t next_random(void)
{
    init_rng();
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 0x2545F4914F6CDD1DULL;
}

static double random_uniform(void)
{
    return (double)next_random() / (double)UINT64_MAX;
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

double nimcp_safe_log2(double x)
{
    if (x <= 0.0) return 0.0;
    return log2(x);
}

double nimcp_random_laplace(double scale)
{
    double u = random_uniform() - 0.5;
    double sign = (u >= 0) ? 1.0 : -1.0;
    return -sign * scale * log(1.0 - 2.0 * fabs(u));
}

double nimcp_random_gaussian(double mean, double stddev)
{
    /* Box-Muller transform */
    double u1 = random_uniform();
    double u2 = random_uniform();

    /* Avoid log(0) */
    if (u1 < 1e-10) u1 = 1e-10;

    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    return mean + stddev * z;
}

/**
 * @brief Continued fraction expansion for incomplete beta
 */
static double beta_cf(double a, double b, double x)
{
    const int max_iter = 200;
    const double eps = 1e-10;

    double qab = a + b;
    double qap = a + 1.0;
    double qam = a - 1.0;
    double c = 1.0;
    double d = 1.0 - qab * x / qap;

    if (fabs(d) < eps) d = eps;
    d = 1.0 / d;
    double h = d;

    for (int m = 1; m <= max_iter; m++) {
        int m2 = 2 * m;

        /* Even step */
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if (fabs(d) < eps) d = eps;
        c = 1.0 + aa / c;
        if (fabs(c) < eps) c = eps;
        d = 1.0 / d;
        h *= d * c;

        /* Odd step */
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if (fabs(d) < eps) d = eps;
        c = 1.0 + aa / c;
        if (fabs(c) < eps) c = eps;
        d = 1.0 / d;
        double del = d * c;
        h *= del;

        if (fabs(del - 1.0) < eps) break;
    }

    return h;
}

double nimcp_beta_incomplete(double x, double a, double b)
{
    if (x < 0.0 || x > 1.0) return 0.0;
    if (x == 0.0) return 0.0;
    if (x == 1.0) return 1.0;

    /* Use log-gamma for stability */
    double bt = exp(lgamma(a + b) - lgamma(a) - lgamma(b) +
                    a * log(x) + b * log(1.0 - x));

    /* Use symmetry for numerical stability */
    if (x < (a + 1.0) / (a + b + 2.0)) {
        return bt * beta_cf(a, b, x) / a;
    } else {
        return 1.0 - bt * beta_cf(b, a, 1.0 - x) / b;
    }
}

//=============================================================================
// Part 1: Shannon Entropy Implementation
//=============================================================================

nimcp_entropy_config_t nimcp_entropy_default_config(void)
{
    return (nimcp_entropy_config_t){
        .deviation_threshold = 3.0,      /* 3 sigma */
        .min_entropy_threshold = 7.5,    /* Near-random threshold */
        .window_size = NIMCP_ENTROPY_WINDOW_SIZE,
        .track_baseline = true,
        .baseline_samples = 10
    };
}

nimcp_entropy_analyzer_t* nimcp_entropy_create(void)
{
    nimcp_entropy_analyzer_t* analyzer = nimcp_calloc(1, sizeof(*analyzer));
    if (!analyzer) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "analyzer is NULL");

        return NULL;

    }

    if (nimcp_mutex_init(&analyzer->lock, NULL) != NIMCP_SUCCESS) {
        nimcp_free(analyzer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_entropy_create: validation failed");
        return NULL;
    }
    return analyzer;
}

nimcp_result_t nimcp_entropy_init(
    nimcp_entropy_analyzer_t* analyzer,
    const nimcp_entropy_config_t* config)
{
    if (!analyzer) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&analyzer->lock);

    if (config) {
        analyzer->config = *config;
    } else {
        analyzer->config = nimcp_entropy_default_config();
    }

    analyzer->baseline_count = 0;
    analyzer->initialized = true;

    nimcp_mutex_unlock(&analyzer->lock);
    return NIMCP_SUCCESS;
}

void nimcp_entropy_destroy(nimcp_entropy_analyzer_t* analyzer)
{
    if (!analyzer) return;
    nimcp_mutex_destroy(&analyzer->lock);
    nimcp_free(analyzer);
}

double nimcp_entropy_calculate(const void* data, size_t size)
{
    if (!data || size == 0) return 0.0;

    const uint8_t* bytes = (const uint8_t*)data;
    uint64_t histogram[256] = {0};

    /* Build histogram */
    for (size_t i = 0; i < size; i++) {
        histogram[bytes[i]]++;
    }

    /* Calculate Shannon entropy */
    double entropy = 0.0;
    double n = (double)size;

    for (int i = 0; i < 256; i++) {
        if (histogram[i] > 0) {
            double p = (double)histogram[i] / n;
            entropy -= p * log2(p);
        }
    }

    return entropy;
}

double nimcp_entropy_joint(const void* data1, const void* data2, size_t size)
{
    if (!data1 || !data2 || size == 0) return 0.0;

    const uint8_t* b1 = (const uint8_t*)data1;
    const uint8_t* b2 = (const uint8_t*)data2;

    /* 256x256 joint histogram */
    uint64_t* joint = nimcp_calloc(256 * 256, sizeof(uint64_t));
    if (!joint) return 0.0;

    for (size_t i = 0; i < size; i++) {
        joint[b1[i] * 256 + b2[i]]++;
    }

    double entropy = 0.0;
    double n = (double)size;

    for (int i = 0; i < 256 * 256; i++) {
        if (joint[i] > 0) {
            double p = (double)joint[i] / n;
            entropy -= p * log2(p);
        }
    }

    nimcp_free(joint);
    return entropy;
}

double nimcp_entropy_mutual_information(const void* data1, const void* data2, size_t size)
{
    double h1 = nimcp_entropy_calculate(data1, size);
    double h2 = nimcp_entropy_calculate(data2, size);
    double h_joint = nimcp_entropy_joint(data1, data2, size);

    /* I(X;Y) = H(X) + H(Y) - H(X,Y) */
    return h1 + h2 - h_joint;
}

nimcp_result_t nimcp_entropy_analyze(
    nimcp_entropy_analyzer_t* analyzer,
    const void* data,
    size_t size,
    nimcp_entropy_result_t* result)
{
    if (!analyzer || !data || !result) return NIMCP_INVALID_PARAM;
    if (!analyzer->initialized) return NIMCP_NOT_INITIALIZED;

    memset(result, 0, sizeof(*result));

    const uint8_t* bytes = (const uint8_t*)data;

    /* Build histogram */
    for (size_t i = 0; i < size; i++) {
        result->byte_histogram[bytes[i]]++;
    }
    result->total_bytes = size;

    /* Calculate Shannon entropy */
    double entropy = 0.0;
    double max_p = 0.0;
    double n = (double)size;

    for (int i = 0; i < 256; i++) {
        if (result->byte_histogram[i] > 0) {
            double p = (double)result->byte_histogram[i] / n;
            entropy -= p * log2(p);
            if (p > max_p) max_p = p;
        }
    }

    result->entropy = entropy;
    result->min_entropy = -nimcp_safe_log2(max_p);  /* Renyi H_infinity */

    /* Check against baseline (if any) */
    result->baseline_entropy = 0.0;
    result->deviation = 0.0;
    result->is_anomaly = false;

    /* Classify data type by entropy */
    if (entropy > analyzer->config.min_entropy_threshold) {
        snprintf(result->analysis, sizeof(result->analysis),
                "High entropy (%.2f): likely encrypted/compressed data", entropy);
    } else if (entropy > 6.0) {
        snprintf(result->analysis, sizeof(result->analysis),
                "Medium-high entropy (%.2f): possibly executable code", entropy);
    } else if (entropy > 4.0) {
        snprintf(result->analysis, sizeof(result->analysis),
                "Medium entropy (%.2f): likely text/structured data", entropy);
    } else {
        snprintf(result->analysis, sizeof(result->analysis),
                "Low entropy (%.2f): sparse or repetitive data", entropy);
    }

    /* Anomaly score based on deviation from expected range */
    double expected_mid = 5.0;  /* Typical code/data entropy */
    result->anomaly_score = fabs(entropy - expected_mid) / 4.0;
    if (result->anomaly_score > 1.0) result->anomaly_score = 1.0;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_entropy_set_baseline(
    nimcp_entropy_analyzer_t* analyzer,
    uint32_t region_id,
    const void* data,
    size_t size)
{
    if (!analyzer || !data) return NIMCP_INVALID_PARAM;
    if (!analyzer->initialized) return NIMCP_NOT_INITIALIZED;

    double entropy = nimcp_entropy_calculate(data, size);

    nimcp_mutex_lock(&analyzer->lock);

    /* Find existing or create new baseline */
    entropy_baseline_t* baseline = NULL;
    for (uint32_t i = 0; i < analyzer->baseline_count; i++) {
        if (analyzer->baselines[i].region_id == region_id) {
            baseline = &analyzer->baselines[i];
            break;
        }
    }

    if (!baseline && analyzer->baseline_count < MAX_ENTROPY_BASELINES) {
        baseline = &analyzer->baselines[analyzer->baseline_count++];
        baseline->region_id = region_id;
        baseline->baseline_entropy = 0.0;
        baseline->baseline_stddev = 0.0;
        baseline->sample_count = 0;
        baseline->active = true;
    }

    if (baseline) {
        /* Running mean and variance (Welford's algorithm) */
        baseline->sample_count++;
        double delta = entropy - baseline->baseline_entropy;
        baseline->baseline_entropy += delta / (double)baseline->sample_count;
        double delta2 = entropy - baseline->baseline_entropy;
        double m2 = baseline->baseline_stddev * baseline->baseline_stddev *
                    (baseline->sample_count - 1);
        m2 += delta * delta2;
        baseline->baseline_stddev = sqrt(m2 / (double)baseline->sample_count);
    }

    nimcp_mutex_unlock(&analyzer->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_entropy_check_baseline(
    nimcp_entropy_analyzer_t* analyzer,
    uint32_t region_id,
    const void* data,
    size_t size,
    nimcp_entropy_result_t* result)
{
    if (!analyzer || !data || !result) return NIMCP_INVALID_PARAM;

    /* First do basic analysis */
    nimcp_result_t res = nimcp_entropy_analyze(analyzer, data, size, result);
    if (res != NIMCP_SUCCESS) return res;

    nimcp_mutex_lock(&analyzer->lock);

    /* Find baseline */
    entropy_baseline_t* baseline = NULL;
    for (uint32_t i = 0; i < analyzer->baseline_count; i++) {
        if (analyzer->baselines[i].region_id == region_id &&
            analyzer->baselines[i].active) {
            baseline = &analyzer->baselines[i];
            break;
        }
    }

    if (baseline && baseline->sample_count >= 2) {
        result->baseline_entropy = baseline->baseline_entropy;

        /* Calculate z-score */
        if (baseline->baseline_stddev > 0.001) {
            result->deviation = (result->entropy - baseline->baseline_entropy) /
                               baseline->baseline_stddev;
        }

        /* Check for anomaly */
        if (fabs(result->deviation) > analyzer->config.deviation_threshold) {
            result->is_anomaly = true;
            result->anomaly_score = fabs(result->deviation) /
                                   (analyzer->config.deviation_threshold * 2.0);
            if (result->anomaly_score > 1.0) result->anomaly_score = 1.0;

            snprintf(result->analysis, sizeof(result->analysis),
                    "ANOMALY: Entropy %.2f deviates %.1f sigma from baseline %.2f",
                    result->entropy, result->deviation, baseline->baseline_entropy);
        }
    }

    nimcp_mutex_unlock(&analyzer->lock);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Part 2: Bayesian Trust Implementation
//=============================================================================

nimcp_trust_config_t nimcp_trust_default_config(void)
{
    return (nimcp_trust_config_t){
        .prior_alpha = 1.0,           /* Uniform prior */
        .prior_beta = 1.0,
        .vouch_weight = 0.5,          /* 50% weight to vouchers */
        .decay_rate = 0.01,           /* 1% decay per day */
        .propagation_damping = 0.8,   /* 20% damping per hop */
        .max_propagation_depth = 3,
        .min_trust_threshold = 0.1
    };
}

nimcp_trust_network_t* nimcp_trust_create(void)
{
    nimcp_trust_network_t* network = nimcp_calloc(1, sizeof(*network));
    if (!network) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "network is NULL");

        return NULL;

    }

    if (nimcp_mutex_init(&network->lock, NULL) != NIMCP_SUCCESS) {
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_trust_create: validation failed");
        return NULL;
    }
    return network;
}

nimcp_result_t nimcp_trust_init(
    nimcp_trust_network_t* network,
    const nimcp_trust_config_t* config)
{
    if (!network) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&network->lock);

    if (config) {
        network->config = *config;
    } else {
        network->config = nimcp_trust_default_config();
    }

    network->entity_count = 0;
    network->initialized = true;

    nimcp_mutex_unlock(&network->lock);
    return NIMCP_SUCCESS;
}

void nimcp_trust_destroy(nimcp_trust_network_t* network)
{
    if (!network) return;
    nimcp_mutex_destroy(&network->lock);
    nimcp_free(network);
}

static nimcp_trust_entity_t* find_entity(nimcp_trust_network_t* network, uint32_t entity_id)
{
    for (uint32_t i = 0; i < network->entity_count; i++) {
        if (network->entities[i].entity_id == entity_id &&
            network->entities[i].active) {
            return &network->entities[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_entity: operation failed");
    return NULL;
}

static void update_trust_score(nimcp_trust_score_t* score)
{
    score->expected_trust = score->alpha / (score->alpha + score->beta);
    double ab = score->alpha + score->beta;
    score->variance = (score->alpha * score->beta) / (ab * ab * (ab + 1.0));
    score->confidence = 1.0 - sqrt(score->variance);
    if (score->confidence < 0.0) score->confidence = 0.0;
}

nimcp_result_t nimcp_trust_register_entity(
    nimcp_trust_network_t* network,
    uint32_t entity_id,
    const char* name)
{
    if (!network || !name) return NIMCP_INVALID_PARAM;
    if (!network->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&network->lock);

    if (network->entity_count >= NIMCP_TRUST_MAX_ENTITIES) {
        nimcp_mutex_unlock(&network->lock);
        return NIMCP_ERROR;
    }

    /* Check if already exists */
    if (find_entity(network, entity_id)) {
        nimcp_mutex_unlock(&network->lock);
        return NIMCP_SUCCESS;
    }

    nimcp_trust_entity_t* entity = &network->entities[network->entity_count++];
    entity->entity_id = entity_id;
    strncpy(entity->name, name, sizeof(entity->name) - 1);
    entity->name[sizeof(entity->name) - 1] = '\0';

    /* Initialize with prior */
    entity->direct_trust.alpha = network->config.prior_alpha;
    entity->direct_trust.beta = network->config.prior_beta;
    entity->direct_trust.observations = 0;
    update_trust_score(&entity->direct_trust);

    entity->derived_trust = entity->direct_trust;
    entity->combined_trust = entity->direct_trust;

    entity->voucher_count = 0;
    entity->last_interaction = (uint64_t)time(NULL);
    entity->active = true;

    nimcp_mutex_unlock(&network->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_trust_record_interaction(
    nimcp_trust_network_t* network,
    uint32_t entity_id,
    bool success,
    double weight)
{
    if (!network) return NIMCP_INVALID_PARAM;
    if (!network->initialized) return NIMCP_NOT_INITIALIZED;
    if (weight <= 0.0) weight = 1.0;

    nimcp_mutex_lock(&network->lock);

    nimcp_trust_entity_t* entity = find_entity(network, entity_id);
    if (!entity) {
        nimcp_mutex_unlock(&network->lock);
        return NIMCP_NOT_FOUND;
    }

    /* Bayesian update */
    if (success) {
        entity->direct_trust.alpha += weight;
    } else {
        entity->direct_trust.beta += weight;
    }
    entity->direct_trust.observations++;
    entity->last_interaction = (uint64_t)time(NULL);

    update_trust_score(&entity->direct_trust);
    entity->combined_trust = entity->direct_trust;  /* Will be recalculated on propagate */

    nimcp_mutex_unlock(&network->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_trust_add_voucher(
    nimcp_trust_network_t* network,
    uint32_t voucher_id,
    uint32_t entity_id)
{
    if (!network) return NIMCP_INVALID_PARAM;
    if (!network->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&network->lock);

    nimcp_trust_entity_t* entity = find_entity(network, entity_id);
    nimcp_trust_entity_t* voucher = find_entity(network, voucher_id);

    if (!entity || !voucher) {
        nimcp_mutex_unlock(&network->lock);
        return NIMCP_NOT_FOUND;
    }

    if (entity->voucher_count >= NIMCP_TRUST_MAX_VOUCHERS) {
        nimcp_mutex_unlock(&network->lock);
        return NIMCP_ERROR;
    }

    /* Check if already vouched */
    for (uint32_t i = 0; i < entity->voucher_count; i++) {
        if (entity->vouchers[i] == voucher_id) {
            nimcp_mutex_unlock(&network->lock);
            return NIMCP_SUCCESS;
        }
    }

    entity->vouchers[entity->voucher_count++] = voucher_id;

    nimcp_mutex_unlock(&network->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_trust_propagate(nimcp_trust_network_t* network)
{
    if (!network) return NIMCP_INVALID_PARAM;
    if (!network->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&network->lock);

    const int max_iterations = 10;
    const double convergence_threshold = 0.001;

    for (int iter = 0; iter < max_iterations; iter++) {
        double max_change = 0.0;

        for (uint32_t i = 0; i < network->entity_count; i++) {
            nimcp_trust_entity_t* entity = &network->entities[i];
            if (!entity->active || entity->voucher_count == 0) continue;

            /* Calculate derived trust from vouchers */
            double voucher_alpha = 0.0;
            double voucher_beta = 0.0;
            double total_weight = 0.0;

            for (uint32_t v = 0; v < entity->voucher_count; v++) {
                nimcp_trust_entity_t* voucher = find_entity(network, entity->vouchers[v]);
                if (!voucher) continue;

                double voucher_trust = voucher->combined_trust.expected_trust;
                if (voucher_trust < network->config.min_trust_threshold) continue;

                double weight = voucher_trust * network->config.propagation_damping;
                voucher_alpha += voucher->direct_trust.alpha * weight;
                voucher_beta += voucher->direct_trust.beta * weight;
                total_weight += weight;
            }

            if (total_weight > 0.0) {
                /* Combine direct and derived trust */
                double direct_weight = 1.0 - network->config.vouch_weight;
                double vouch_weight = network->config.vouch_weight;

                double new_alpha = entity->direct_trust.alpha * direct_weight +
                                  (voucher_alpha / total_weight) * vouch_weight;
                double new_beta = entity->direct_trust.beta * direct_weight +
                                 (voucher_beta / total_weight) * vouch_weight;

                /* Track convergence */
                double old_trust = entity->combined_trust.expected_trust;
                entity->combined_trust.alpha = new_alpha;
                entity->combined_trust.beta = new_beta;
                update_trust_score(&entity->combined_trust);

                double change = fabs(entity->combined_trust.expected_trust - old_trust);
                if (change > max_change) max_change = change;
            }
        }

        if (max_change < convergence_threshold) break;
    }

    nimcp_mutex_unlock(&network->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_trust_get_score(
    nimcp_trust_network_t* network,
    uint32_t entity_id,
    nimcp_trust_score_t* score)
{
    if (!network || !score) return NIMCP_INVALID_PARAM;
    if (!network->initialized) return NIMCP_NOT_INITIALIZED;

    nimcp_mutex_lock(&network->lock);

    nimcp_trust_entity_t* entity = find_entity(network, entity_id);
    if (!entity) {
        nimcp_mutex_unlock(&network->lock);
        return NIMCP_NOT_FOUND;
    }

    *score = entity->combined_trust;

    nimcp_mutex_unlock(&network->lock);
    return NIMCP_SUCCESS;
}

bool nimcp_trust_is_trusted(
    nimcp_trust_network_t* network,
    uint32_t entity_id,
    double threshold)
{
    nimcp_trust_score_t score;
    if (nimcp_trust_get_score(network, entity_id, &score) != NIMCP_SUCCESS) {
        return false;
    }
    return score.expected_trust >= threshold;
}

double nimcp_trust_probability_above(
    const nimcp_trust_score_t* score,
    double threshold)
{
    if (!score) return 0.0;
    if (threshold <= 0.0) return 1.0;
    if (threshold >= 1.0) return 0.0;

    /* P(Trust >= threshold) = 1 - I_threshold(alpha, beta) */
    return 1.0 - nimcp_beta_incomplete(threshold, score->alpha, score->beta);
}

//=============================================================================
// Part 3: Differential Privacy Implementation
//=============================================================================

nimcp_dp_config_t nimcp_dp_default_config(void)
{
    return (nimcp_dp_config_t){
        .epsilon = NIMCP_DP_DEFAULT_EPSILON,
        .delta = NIMCP_DP_DEFAULT_DELTA,
        .mechanism = NIMCP_DP_LAPLACE,
        .total_budget = 10.0,         /* Total epsilon budget */
        .enforce_budget = true
    };
}

nimcp_dp_context_t* nimcp_dp_create(void)
{
    nimcp_dp_context_t* ctx = nimcp_calloc(1, sizeof(*ctx));
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;

    }

    if (nimcp_mutex_init(&ctx->lock, NULL) != NIMCP_SUCCESS) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_dp_create: validation failed");
        return NULL;
    }
    return ctx;
}

nimcp_result_t nimcp_dp_init(
    nimcp_dp_context_t* ctx,
    const nimcp_dp_config_t* config)
{
    if (!ctx) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&ctx->lock);

    if (config) {
        ctx->config = *config;
    } else {
        ctx->config = nimcp_dp_default_config();
    }

    ctx->budget_spent = 0.0;
    ctx->queries_answered = 0;
    ctx->initialized = true;

    nimcp_mutex_unlock(&ctx->lock);
    return NIMCP_SUCCESS;
}

void nimcp_dp_destroy(nimcp_dp_context_t* ctx)
{
    if (!ctx) return;
    nimcp_mutex_destroy(&ctx->lock);
    nimcp_free(ctx);
}

nimcp_result_t nimcp_dp_add_laplace_noise(
    nimcp_dp_context_t* ctx,
    double value,
    double sensitivity,
    nimcp_dp_result_t* result)
{
    if (!ctx || !result) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;
    if (sensitivity <= 0.0) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&ctx->lock);

    /* Check budget */
    if (ctx->config.enforce_budget &&
        ctx->budget_spent + ctx->config.epsilon > ctx->config.total_budget) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_ERROR;  /* Budget exhausted */
    }

    /* Scale parameter for Laplace distribution */
    double scale = sensitivity / ctx->config.epsilon;
    double noise = nimcp_random_laplace(scale);

    result->true_value = value;
    result->noisy_value = value + noise;
    result->noise_added = noise;
    result->epsilon_spent = ctx->config.epsilon;

    ctx->budget_spent += ctx->config.epsilon;
    ctx->queries_answered++;

    result->remaining_budget = ctx->config.total_budget - ctx->budget_spent;

    /* Accuracy bound: |noise| <= scale * ln(1/beta) with prob 1-beta */
    /* For 95% confidence (beta = 0.05): bound = scale * ln(20) ~ 3*scale */
    result->accuracy_bound = scale * 3.0;

    nimcp_mutex_unlock(&ctx->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_dp_add_gaussian_noise(
    nimcp_dp_context_t* ctx,
    double value,
    double sensitivity,
    nimcp_dp_result_t* result)
{
    if (!ctx || !result) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;
    if (sensitivity <= 0.0) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&ctx->lock);

    /* Check budget */
    if (ctx->config.enforce_budget &&
        ctx->budget_spent + ctx->config.epsilon > ctx->config.total_budget) {
        nimcp_mutex_unlock(&ctx->lock);
        return NIMCP_ERROR;
    }

    /* Gaussian mechanism standard deviation */
    /* sigma = sensitivity * sqrt(2 * ln(1.25/delta)) / epsilon */
    double c = sqrt(2.0 * log(1.25 / ctx->config.delta));
    double sigma = sensitivity * c / ctx->config.epsilon;
    double noise = nimcp_random_gaussian(0.0, sigma);

    result->true_value = value;
    result->noisy_value = value + noise;
    result->noise_added = noise;
    result->epsilon_spent = ctx->config.epsilon;

    ctx->budget_spent += ctx->config.epsilon;
    ctx->queries_answered++;

    result->remaining_budget = ctx->config.total_budget - ctx->budget_spent;

    /* 95% accuracy bound for Gaussian: ~2*sigma */
    result->accuracy_bound = 2.0 * sigma;

    nimcp_mutex_unlock(&ctx->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_dp_count(
    nimcp_dp_context_t* ctx,
    uint64_t count,
    nimcp_dp_result_t* result)
{
    /* Count queries have sensitivity = 1 */
    return nimcp_dp_add_laplace_noise(ctx, (double)count, 1.0, result);
}

nimcp_result_t nimcp_dp_sum(
    nimcp_dp_context_t* ctx,
    double sum,
    double max_contribution,
    nimcp_dp_result_t* result)
{
    /* Sum queries have sensitivity = max_contribution */
    return nimcp_dp_add_laplace_noise(ctx, sum, max_contribution, result);
}

nimcp_result_t nimcp_dp_mean(
    nimcp_dp_context_t* ctx,
    double mean,
    uint64_t count,
    double max_value,
    nimcp_dp_result_t* result)
{
    if (count == 0) return NIMCP_INVALID_PARAM;

    /* Mean sensitivity = max_value / n */
    double sensitivity = max_value / (double)count;
    return nimcp_dp_add_laplace_noise(ctx, mean, sensitivity, result);
}

nimcp_result_t nimcp_dp_histogram(
    nimcp_dp_context_t* ctx,
    const uint64_t* histogram,
    uint32_t num_bins,
    double* noisy_histogram)
{
    if (!ctx || !histogram || !noisy_histogram) return NIMCP_INVALID_PARAM;
    if (!ctx->initialized) return NIMCP_NOT_INITIALIZED;

    /* Each bin has sensitivity 1 */
    /* Total privacy cost = epsilon (parallel composition) */
    for (uint32_t i = 0; i < num_bins; i++) {
        nimcp_dp_result_t result;
        /* Use per-bin epsilon = epsilon (parallel composition allows this) */
        nimcp_result_t res = nimcp_dp_add_laplace_noise(
            ctx, (double)histogram[i], 1.0, &result);
        if (res != NIMCP_SUCCESS) return res;

        noisy_histogram[i] = result.noisy_value;
        if (noisy_histogram[i] < 0.0) noisy_histogram[i] = 0.0;
    }

    return NIMCP_SUCCESS;
}

double nimcp_dp_remaining_budget(const nimcp_dp_context_t* ctx)
{
    if (!ctx) return 0.0;
    return ctx->config.total_budget - ctx->budget_spent;
}

nimcp_result_t nimcp_dp_reset_budget(nimcp_dp_context_t* ctx)
{
    if (!ctx) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&ctx->lock);
    ctx->budget_spent = 0.0;
    ctx->queries_answered = 0;
    nimcp_mutex_unlock(&ctx->lock);

    return NIMCP_SUCCESS;
}
