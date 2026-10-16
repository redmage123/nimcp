/**
 * @file nimcp_jepa_latent.c
 * @brief JEPA Latent Space Representation Implementation
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Implementation of JEPA latent embedding operations
 * WHY:  Core building block for JEPA prediction and comparison
 * HOW:  Memory-efficient operations with precision tracking
 */

#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <float.h>

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define LOG_MODULE "[JEPA_LATENT]"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(jepa_latent, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Global Statistics (accessed via atomics for thread safety)
 * ============================================================================ */

static jepa_latent_stats_t g_latent_stats = {0};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int jepa_latent_default_config(jepa_latent_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, LOG_MODULE " NULL config pointer");

    config->latent_dim = NIMCP_JEPA_LATENT_DIM;
    config->enable_variance = true;
    config->norm_type = JEPA_NORM_L2;
    config->initial_precision = JEPA_LATENT_DEFAULT_PRECISION;
    config->modality = JEPA_MODALITY_UNKNOWN;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

jepa_latent_t* jepa_latent_create(const jepa_latent_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_create", 0.0f);


    jepa_latent_config_t default_config;

    /* Use defaults if no config provided */
    if (!config) {
        jepa_latent_default_config(&default_config);
        config = &default_config;
    }

    /* Validate dimension */
    if (config->latent_dim < JEPA_LATENT_MIN_DIM ||
        config->latent_dim > JEPA_LATENT_MAX_DIM) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Invalid latent_dim: %u (must be %u-%u)",
                           config->latent_dim, JEPA_LATENT_MIN_DIM, JEPA_LATENT_MAX_DIM);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_latent_create: operation failed");
        return NULL;
    }

    /* Allocate latent structure */
    jepa_latent_t* latent = nimcp_malloc(sizeof(jepa_latent_t));
    if (!latent) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to allocate latent structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_latent_create: latent is NULL");
        return NULL;
    }
    memset(latent, 0, sizeof(jepa_latent_t));

    /* Allocate embedding array */
    latent->embedding = nimcp_calloc(config->latent_dim, sizeof(float));
    if (!latent->embedding) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to allocate embedding array");
        nimcp_free(latent);
        latent = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_latent_create: latent->embedding is NULL");
        return NULL;
    }
    /* calloc zero-initializes */

    /* Optionally allocate variance array */
    if (config->enable_variance) {
        latent->variance = nimcp_calloc(config->latent_dim, sizeof(float));
        if (!latent->variance) {
            NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to allocate variance array");
            nimcp_free(latent->embedding);
            nimcp_free(latent);
            latent = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_latent_create: latent->variance is NULL");
            return NULL;
        }
        /* Initialize variance to 1.0 (unit variance) */
        for (uint32_t i = 0; i < config->latent_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && config->latent_dim > 256) {
                jepa_latent_heartbeat("jepa_latent_loop",
                                 (float)(i + 1) / (float)config->latent_dim);
            }

            latent->variance[i] = 1.0f;
        }
    }

    /* Initialize metadata */
    latent->latent_dim = config->latent_dim;
    latent->precision = config->initial_precision;
    latent->modality = config->modality;
    latent->timestamp_ms = 0;
    latent->sequence_position = 0;
    latent->is_normalized = false;
    latent->norm_type = config->norm_type;
    latent->ref_count = 1;

    /* Update statistics - thread-safe atomic increment */
    __atomic_fetch_add(&g_latent_stats.latents_created, 1, __ATOMIC_RELAXED);

    NIMCP_LOGGING_DEBUG(LOG_MODULE " Created latent: dim=%u, variance=%s, modality=%s",
                       latent->latent_dim,
                       latent->variance ? "enabled" : "disabled",
                       jepa_modality_to_string(latent->modality));

    return latent;
}

jepa_latent_t* jepa_latent_create_dim(uint32_t latent_dim) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_create_dim", 0.0f);


    jepa_latent_config_t config;
    jepa_latent_default_config(&config);
    config.latent_dim = latent_dim;
    return jepa_latent_create(&config);
}

jepa_latent_t* jepa_latent_clone(const jepa_latent_t* src) {
    if (!src) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "src is NULL");

        return NULL;
    }

    /* Create config from source */
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_clone", 0.0f);


    jepa_latent_config_t config = {
        .latent_dim = src->latent_dim,
        .enable_variance = (src->variance != NULL),
        .norm_type = src->norm_type,
        .initial_precision = src->precision,
        .modality = src->modality
    };

    /* Create new latent */
    jepa_latent_t* clone = jepa_latent_create(&config);
    if (!clone) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "clone is NULL");

        return NULL;
    }

    /* Copy embedding data */
    memcpy(clone->embedding, src->embedding, src->latent_dim * sizeof(float));

    /* Copy variance if present */
    if (src->variance && clone->variance) {
        memcpy(clone->variance, src->variance, src->latent_dim * sizeof(float));
    }

    /* Copy metadata */
    clone->timestamp_ms = src->timestamp_ms;
    clone->sequence_position = src->sequence_position;
    clone->is_normalized = src->is_normalized;

    return clone;
}

void jepa_latent_destroy(jepa_latent_t* latent) {
    if (!latent) {
        return;
    }

    /* Thread-safe decrement and check - atomic fetch-and-subtract returns old value */
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_destroy", 0.0f);


    uint32_t old_count = __atomic_fetch_sub(&latent->ref_count, 1, __ATOMIC_ACQ_REL);
    if (old_count > 1) {
        return;  /* Other references still exist */
    }

    /* Free arrays */
    if (latent->embedding) {
        nimcp_free(latent->embedding);
        latent->embedding = NULL;
    }

    if (latent->variance) {
        nimcp_free(latent->variance);
        latent->variance = NULL;
    }

    /* Free structure */
    nimcp_free(latent);
    latent = NULL;

    /* Update statistics - thread-safe atomic increment */
    __atomic_fetch_add(&g_latent_stats.latents_destroyed, 1, __ATOMIC_RELAXED);
}

int jepa_latent_reset(jepa_latent_t* latent) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_reset", 0.0f);


    NIMCP_CHECK_THROW(latent, NIMCP_ERROR_NULL_POINTER, "latent is NULL");

    /* Zero embedding */
    memset(latent->embedding, 0, latent->latent_dim * sizeof(float));

    /* Reset variance to unit if present */
    if (latent->variance) {
        for (uint32_t i = 0; i < latent->latent_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && latent->latent_dim > 256) {
                jepa_latent_heartbeat("jepa_latent_loop",
                                 (float)(i + 1) / (float)latent->latent_dim);
            }

            latent->variance[i] = 1.0f;
        }
    }

    /* Reset metadata */
    latent->precision = JEPA_LATENT_DEFAULT_PRECISION;
    latent->timestamp_ms = 0;
    latent->sequence_position = 0;
    latent->is_normalized = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Data Access API
 * ============================================================================ */

int jepa_latent_set_embedding(jepa_latent_t* latent, const float* values, uint32_t dim) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_set_embedding", 0.0f);


    NIMCP_CHECK_THROW(latent && values, NIMCP_ERROR_NULL_POINTER, "latent or values is NULL");

    if (dim != latent->latent_dim) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Dimension mismatch in set_embedding: %u vs %u",
                           dim, latent->latent_dim);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memcpy(latent->embedding, values, dim * sizeof(float));

    /* Embedding changed, so it may no longer be normalized */
    latent->is_normalized = false;

    return NIMCP_SUCCESS;
}

int jepa_latent_get_embedding(const jepa_latent_t* latent, float* values, uint32_t max_dim) {
    if (!latent || !values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_latent_get_embedding: required parameter is NULL (latent, values)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_get_embedding", 0.0f);


    uint32_t copy_dim = (max_dim < latent->latent_dim) ? max_dim : latent->latent_dim;
    memcpy(values, latent->embedding, copy_dim * sizeof(float));

    return (int)copy_dim;
}

int jepa_latent_set_variance(jepa_latent_t* latent, const float* variance, uint32_t dim) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_set_variance", 0.0f);


    NIMCP_CHECK_THROW(latent && variance, NIMCP_ERROR_NULL_POINTER, "latent or variance is NULL");

    if (!latent->variance) {
        /* Allocate variance array if not present */
        latent->variance = nimcp_calloc(latent->latent_dim, sizeof(float));
        if (!latent->variance) {
            return NIMCP_ERROR_NO_MEMORY;
        }
    }

    if (dim > latent->latent_dim) {
        dim = latent->latent_dim;
    }

    memcpy(latent->variance, variance, dim * sizeof(float));

    /* Update precision */
    return jepa_latent_update_precision(latent);
}

int jepa_latent_update_precision(jepa_latent_t* latent) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_update_precision", 0.0f);


    NIMCP_CHECK_THROW(latent, NIMCP_ERROR_NULL_POINTER, "latent is NULL");

    if (!latent->variance) {
        /* No variance data, use default precision */
        latent->precision = JEPA_LATENT_DEFAULT_PRECISION;
        return NIMCP_SUCCESS;
    }

    /* Compute mean variance */
    double sum_var = 0.0;
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && latent->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)latent->latent_dim);
        }

        sum_var += latent->variance[i];
    }
    float mean_var = (float)(sum_var / latent->latent_dim);

    /* Precision = 1 / mean_variance, with bounds */
    if (mean_var < JEPA_LATENT_EPSILON) {
        latent->precision = JEPA_LATENT_MAX_PRECISION;
    } else {
        latent->precision = 1.0f / mean_var;
    }

    /* Clamp to valid range */
    if (latent->precision < JEPA_LATENT_MIN_PRECISION) {
        latent->precision = JEPA_LATENT_MIN_PRECISION;
    } else if (latent->precision > JEPA_LATENT_MAX_PRECISION) {
        latent->precision = JEPA_LATENT_MAX_PRECISION;
    }

    /* Update avg_precision with exponential moving average.
     * Use CAS loop for thread-safe float update. */
    {
        union { float f; uint32_t u; } old_val, new_val;
        do {
            old_val.u = __atomic_load_n((uint32_t*)&g_latent_stats.avg_precision, __ATOMIC_RELAXED);
            new_val.f = 0.9f * old_val.f + 0.1f * latent->precision;
        } while (!__atomic_compare_exchange_n((uint32_t*)&g_latent_stats.avg_precision,
                                              &old_val.u, new_val.u, false,
                                              __ATOMIC_RELAXED, __ATOMIC_RELAXED));
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Normalization API
 * ============================================================================ */

float jepa_latent_norm(const jepa_latent_t* latent) {
    if (!latent || !latent->embedding) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_norm", 0.0f);


    double sum_sq = 0.0;
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && latent->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)latent->latent_dim);
        }

        sum_sq += (double)latent->embedding[i] * latent->embedding[i];
    }

    return (float)sqrt(sum_sq);
}

int jepa_latent_normalize(jepa_latent_t* latent) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_normalize", 0.0f);


    NIMCP_CHECK_THROW(latent && latent->embedding, NIMCP_ERROR_NULL_POINTER, "latent or embedding is NULL");

    float norm = jepa_latent_norm(latent);
    if (norm < JEPA_LATENT_EPSILON) {
        /* Zero vector, can't normalize */
        NIMCP_LOGGING_WARN(LOG_MODULE " Cannot normalize zero-norm embedding");
        return NIMCP_ERROR_INVALID_STATE;
    }

    float inv_norm = 1.0f / (fabsf(norm) > 1e-7f ? norm : 1e-7f);
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && latent->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)latent->latent_dim);
        }

        latent->embedding[i] *= inv_norm;
    }

    latent->is_normalized = true;
    latent->norm_type = JEPA_NORM_L2;
    __atomic_fetch_add(&g_latent_stats.normalizations, 1, __ATOMIC_RELAXED);

    return NIMCP_SUCCESS;
}

int jepa_latent_layer_normalize(jepa_latent_t* latent) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_layer_normalize", 0.0f);


    NIMCP_CHECK_THROW(latent && latent->embedding, NIMCP_ERROR_NULL_POINTER, "latent or embedding is NULL");

    /* Compute mean */
    double sum = 0.0;
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && latent->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)latent->latent_dim);
        }

        sum += latent->embedding[i];
    }
    float mean = (float)(sum / latent->latent_dim);

    /* Compute variance */
    double sum_sq_diff = 0.0;
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && latent->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)latent->latent_dim);
        }

        float diff = latent->embedding[i] - mean;
        sum_sq_diff += diff * diff;
    }
    float variance = (float)(sum_sq_diff / latent->latent_dim);
    float std = sqrtf(variance + JEPA_LATENT_EPSILON);

    /* Normalize: (x - mean) / std */
    float inv_std = 1.0f / (fabsf(std) > 1e-7f ? std : 1e-7f);
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && latent->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)latent->latent_dim);
        }

        latent->embedding[i] = (latent->embedding[i] - mean) * inv_std;
    }

    latent->is_normalized = true;
    latent->norm_type = JEPA_NORM_LAYERNORM;
    __atomic_fetch_add(&g_latent_stats.normalizations, 1, __ATOMIC_RELAXED);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Similarity API
 * ============================================================================ */

float jepa_latent_dot(const jepa_latent_t* a, const jepa_latent_t* b) {
    if (!a || !b || !a->embedding || !b->embedding) {
        return NAN;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_dot", 0.0f);


    if (a->latent_dim != b->latent_dim) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Dimension mismatch: %u vs %u",
                           a->latent_dim, b->latent_dim);
        return NAN;
    }

    double dot = 0.0;
    for (uint32_t i = 0; i < a->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && a->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)a->latent_dim);
        }

        dot += (double)a->embedding[i] * b->embedding[i];
    }

    return (float)dot;
}

float jepa_latent_cosine_similarity(const jepa_latent_t* a, const jepa_latent_t* b) {
    if (!a || !b) {
        return NAN;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_cosine_similarity", 0.0f);


    float dot = jepa_latent_dot(a, b);
    if (isnan(dot)) {
        return NAN;
    }

    float norm_a = jepa_latent_norm(a);
    float norm_b = jepa_latent_norm(b);

    if (norm_a < JEPA_LATENT_EPSILON || norm_b < JEPA_LATENT_EPSILON) {
        return 0.0f;  /* Zero vector has no direction */
    }

    __atomic_fetch_add(&g_latent_stats.similarity_ops, 1, __ATOMIC_RELAXED);
    return dot / (norm_a * norm_b);
}

float jepa_latent_similarity(const jepa_latent_t* a, const jepa_latent_t* b,
                              jepa_similarity_t metric) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_similarity", 0.0f);


    switch (metric) {
        case JEPA_SIM_COSINE:
            return jepa_latent_cosine_similarity(a, b);

        case JEPA_SIM_DOT_PRODUCT:
            return jepa_latent_dot(a, b);

        case JEPA_SIM_EUCLIDEAN: {
            float dist = jepa_latent_distance(a, b);
            return isnan(dist) ? NAN : -dist;  /* Negative for similarity */
        }

        case JEPA_SIM_PRECISION_WEIGHTED:
            return jepa_latent_precision_similarity(a, b);

        default:
            return NAN;
    }
}

float jepa_latent_precision_similarity(const jepa_latent_t* a, const jepa_latent_t* b) {
    if (!a || !b || !a->embedding || !b->embedding) {
        return NAN;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_precision_similarity", 0.0f);


    if (a->latent_dim != b->latent_dim) {
        return NAN;
    }

    /* Use variance if available, else uniform precision */
    double weighted_dot = 0.0;
    double total_precision = 0.0;

    for (uint32_t i = 0; i < a->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && a->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)a->latent_dim);
        }

        /* Compute per-dimension precision */
        float prec_a = (a->variance && a->variance[i] > JEPA_LATENT_EPSILON) ?
                       (1.0f / a->variance[i]) : 1.0f;
        float prec_b = (b->variance && b->variance[i] > JEPA_LATENT_EPSILON) ?
                       (1.0f / b->variance[i]) : 1.0f;

        /* Use harmonic mean of precisions */
        float prec = 2.0f * prec_a * prec_b / (prec_a + prec_b + JEPA_LATENT_EPSILON);

        weighted_dot += prec * a->embedding[i] * b->embedding[i];
        total_precision += prec;
    }

    __atomic_fetch_add(&g_latent_stats.similarity_ops, 1, __ATOMIC_RELAXED);

    if (total_precision < JEPA_LATENT_EPSILON) {
        return 0.0f;
    }

    return (float)(weighted_dot / total_precision);
}

float jepa_latent_distance(const jepa_latent_t* a, const jepa_latent_t* b) {
    if (!a || !b || !a->embedding || !b->embedding) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_distance", 0.0f);


    if (a->latent_dim != b->latent_dim) {
        return -1.0f;
    }

    double sum_sq = 0.0;
    for (uint32_t i = 0; i < a->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && a->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)a->latent_dim);
        }

        float diff = a->embedding[i] - b->embedding[i];
        sum_sq += diff * diff;
    }

    return (float)sqrt(sum_sq);
}

/* ============================================================================
 * Interpolation API
 * ============================================================================ */

int jepa_latent_interpolate(const jepa_latent_t* a, const jepa_latent_t* b,
                             float alpha, jepa_latent_t* result) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_interpolate", 0.0f);


    NIMCP_CHECK_THROW(a && b && result, NIMCP_ERROR_NULL_POINTER, "a, b, or result is NULL");

    if (a->latent_dim != b->latent_dim || a->latent_dim != result->latent_dim) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Clamp alpha to [0, 1] */
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    float one_minus_alpha = 1.0f - alpha;

    /* Linear interpolation */
    for (uint32_t i = 0; i < a->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && a->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)a->latent_dim);
        }

        result->embedding[i] = one_minus_alpha * a->embedding[i] +
                               alpha * b->embedding[i];
    }

    /* Interpolate variance if both have it */
    if (a->variance && b->variance && result->variance) {
        for (uint32_t i = 0; i < a->latent_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && a->latent_dim > 256) {
                jepa_latent_heartbeat("jepa_latent_loop",
                                 (float)(i + 1) / (float)a->latent_dim);
            }

            result->variance[i] = one_minus_alpha * a->variance[i] +
                                  alpha * b->variance[i];
        }
        jepa_latent_update_precision(result);
    }

    result->is_normalized = false;
    __atomic_fetch_add(&g_latent_stats.interpolations, 1, __ATOMIC_RELAXED);

    return NIMCP_SUCCESS;
}

int jepa_latent_slerp(const jepa_latent_t* a, const jepa_latent_t* b,
                       float alpha, jepa_latent_t* result) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_slerp", 0.0f);


    NIMCP_CHECK_THROW(a && b && result, NIMCP_ERROR_NULL_POINTER, "a, b, or result is NULL");

    if (a->latent_dim != b->latent_dim || a->latent_dim != result->latent_dim) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Clamp alpha */
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    /* Compute cosine of angle */
    float cos_theta = jepa_latent_cosine_similarity(a, b);
    if (isnan(cos_theta)) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Clamp to prevent acos domain error */
    if (cos_theta > 1.0f) cos_theta = 1.0f;
    if (cos_theta < -1.0f) cos_theta = -1.0f;

    /* If vectors are nearly parallel, use linear interpolation */
    if (cos_theta > 0.9995f) {
        return jepa_latent_interpolate(a, b, alpha, result);
    }

    /* SLERP formula */
    float theta = acosf(cos_theta);
    float sin_theta = sinf(theta);
    float scale_a = sinf((1.0f - alpha) * theta) / sin_theta;
    float scale_b = sinf(alpha * theta) / sin_theta;

    for (uint32_t i = 0; i < a->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && a->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)a->latent_dim);
        }

        result->embedding[i] = scale_a * a->embedding[i] +
                               scale_b * b->embedding[i];
    }

    result->is_normalized = true;  /* SLERP preserves normalization */
    result->norm_type = JEPA_NORM_L2;
    __atomic_fetch_add(&g_latent_stats.interpolations, 1, __ATOMIC_RELAXED);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Arithmetic API
 * ============================================================================ */

int jepa_latent_add(const jepa_latent_t* a, const jepa_latent_t* b,
                     jepa_latent_t* result) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_add", 0.0f);


    NIMCP_CHECK_THROW(a && b && result, NIMCP_ERROR_NULL_POINTER, "a, b, or result is NULL");

    if (a->latent_dim != b->latent_dim || a->latent_dim != result->latent_dim) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    for (uint32_t i = 0; i < a->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && a->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)a->latent_dim);
        }

        result->embedding[i] = a->embedding[i] + b->embedding[i];
    }

    result->is_normalized = false;
    return NIMCP_SUCCESS;
}

int jepa_latent_subtract(const jepa_latent_t* a, const jepa_latent_t* b,
                          jepa_latent_t* result) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_subtract", 0.0f);


    NIMCP_CHECK_THROW(a && b && result, NIMCP_ERROR_NULL_POINTER, "a, b, or result is NULL");

    if (a->latent_dim != b->latent_dim || a->latent_dim != result->latent_dim) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    for (uint32_t i = 0; i < a->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && a->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)a->latent_dim);
        }

        result->embedding[i] = a->embedding[i] - b->embedding[i];
    }

    result->is_normalized = false;
    return NIMCP_SUCCESS;
}

int jepa_latent_scale(jepa_latent_t* latent, float scale) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_scale", 0.0f);


    NIMCP_CHECK_THROW(latent, NIMCP_ERROR_NULL_POINTER, "latent is NULL");

    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && latent->latent_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)latent->latent_dim);
        }

        latent->embedding[i] *= scale;
    }

    /* Scaling by non-unit factor breaks L2 normalization */
    if (fabsf(scale - 1.0f) > JEPA_LATENT_EPSILON) {
        latent->is_normalized = false;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Projection API
 * ============================================================================ */

int jepa_latent_project(const jepa_latent_t* src,
                         const float* projection,
                         const float* bias,
                         uint32_t target_dim,
                         jepa_latent_t* result) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_project", 0.0f);


    NIMCP_CHECK_THROW(src && projection && result, NIMCP_ERROR_NULL_POINTER, "src, projection, or result is NULL");

    if (result->latent_dim < target_dim) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Matrix-vector multiplication: result = projection @ src + bias */
    /* projection is [target_dim x src_dim] in row-major order */
    for (uint32_t i = 0; i < target_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && target_dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)target_dim);
        }

        double sum = 0.0;
        for (uint32_t j = 0; j < src->latent_dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && src->latent_dim > 256) {
                jepa_latent_heartbeat("jepa_latent_loop",
                                 (float)(j + 1) / (float)src->latent_dim);
            }

            sum += projection[i * src->latent_dim + j] * src->embedding[j];
        }
        if (bias) {
            sum += bias[i];
        }
        result->embedding[i] = (float)sum;
    }

    result->is_normalized = false;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Pooling API
 * ============================================================================ */

int jepa_latent_mean_pool(const jepa_latent_t** latents, uint32_t num_latents,
                           jepa_latent_t* result) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_mean_pool", 0.0f);


    NIMCP_CHECK_THROW(latents && result && num_latents > 0, NIMCP_ERROR_NULL_POINTER, "latents or result is NULL, or num_latents is 0");

    /* Verify all latents have same dimension */
    uint32_t dim = result->latent_dim;
    for (uint32_t n = 0; n < num_latents; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && num_latents > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(n + 1) / (float)num_latents);
        }

        if (!latents[n] || latents[n]->latent_dim != dim) {
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Compute mean */
    memset(result->embedding, 0, dim * sizeof(float));

    for (uint32_t n = 0; n < num_latents; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && num_latents > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(n + 1) / (float)num_latents);
        }

        for (uint32_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                jepa_latent_heartbeat("jepa_latent_loop",
                                 (float)(i + 1) / (float)dim);
            }

            result->embedding[i] += latents[n]->embedding[i];
        }
    }

    float inv_n = 1.0f / (float)num_latents;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_latent_heartbeat("jepa_latent_loop",
                             (float)(i + 1) / (float)dim);
        }

        result->embedding[i] *= inv_n;
    }

    result->is_normalized = false;
    return NIMCP_SUCCESS;
}

int jepa_latent_max_pool(const jepa_latent_t** latents, uint32_t num_latents,
                          jepa_latent_t* result) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_max_pool", 0.0f);


    NIMCP_CHECK_THROW(latents && result && num_latents > 0, NIMCP_ERROR_NULL_POINTER, "latents or result is NULL, or num_latents is 0");

    uint32_t dim = result->latent_dim;

    /* Initialize with first latent */
    if (!latents[0] || latents[0]->latent_dim != dim) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    memcpy(result->embedding, latents[0]->embedding, dim * sizeof(float));

    /* Take element-wise max */
    for (uint32_t n = 1; n < num_latents; n++) {
        if (!latents[n] || latents[n]->latent_dim != dim) {
            return NIMCP_ERROR_INVALID_PARAM;
        }
        for (uint32_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                jepa_latent_heartbeat("jepa_latent_loop",
                                 (float)(i + 1) / (float)dim);
            }

            if (latents[n]->embedding[i] > result->embedding[i]) {
                result->embedding[i] = latents[n]->embedding[i];
            }
        }
    }

    result->is_normalized = false;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int jepa_latent_get_stats(jepa_latent_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_get_stats", 0.0f);


    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    /* Thread-safe copy using atomic loads for integers, direct read for float */
    stats->latents_created = __atomic_load_n(&g_latent_stats.latents_created, __ATOMIC_RELAXED);
    stats->latents_destroyed = __atomic_load_n(&g_latent_stats.latents_destroyed, __ATOMIC_RELAXED);
    stats->normalizations = __atomic_load_n(&g_latent_stats.normalizations, __ATOMIC_RELAXED);
    stats->similarity_ops = __atomic_load_n(&g_latent_stats.similarity_ops, __ATOMIC_RELAXED);
    stats->interpolations = __atomic_load_n(&g_latent_stats.interpolations, __ATOMIC_RELAXED);
    stats->avg_precision = g_latent_stats.avg_precision;  /* Aligned float read is atomic on x86 */
    return NIMCP_SUCCESS;
}

int jepa_latent_reset_stats(void) {
    /* Thread-safe reset using atomic stores for integers, direct write for float */
    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_reset_stats", 0.0f);


    __atomic_store_n(&g_latent_stats.latents_created, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_latent_stats.latents_destroyed, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_latent_stats.normalizations, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_latent_stats.similarity_ops, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_latent_stats.interpolations, 0, __ATOMIC_RELAXED);
    g_latent_stats.avg_precision = 0.0f;  /* Aligned float write is atomic on x86 */
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* jepa_modality_to_string(jepa_modality_t modality) {
    switch (modality) {
        case JEPA_MODALITY_UNKNOWN:    return "unknown";
        case JEPA_MODALITY_VISUAL:     return "visual";
        case JEPA_MODALITY_SPEECH:     return "speech";
        case JEPA_MODALITY_TEXT:       return "text";
        case JEPA_MODALITY_MOTOR:      return "motor";
        case JEPA_MODALITY_MULTIMODAL: return "multimodal";
        default:                       return "invalid";
    }
}

const char* jepa_norm_type_to_string(jepa_norm_type_t norm_type) {
    switch (norm_type) {
        case JEPA_NORM_NONE:      return "none";
        case JEPA_NORM_L2:        return "L2";
        case JEPA_NORM_LAYERNORM: return "layernorm";
        case JEPA_NORM_BATCHNORM: return "batchnorm";
        default:                  return "invalid";
    }
}

const char* jepa_similarity_to_string(jepa_similarity_t metric) {
    switch (metric) {
        case JEPA_SIM_COSINE:             return "cosine";
        case JEPA_SIM_DOT_PRODUCT:        return "dot_product";
        case JEPA_SIM_EUCLIDEAN:          return "euclidean";
        case JEPA_SIM_PRECISION_WEIGHTED: return "precision_weighted";
        default:                          return "invalid";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int jepa_latent_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    jepa_latent_heartbeat("jepa_latent_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "JEPA_Latent_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                jepa_latent_heartbeat("jepa_latent_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "JEPA_Latent_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "JEPA_Latent_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void jepa_latent_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_jepa_latent_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int jepa_latent_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_latent_training_begin: NULL argument");
        return -1;
    }
    jepa_latent_heartbeat("jepa_latent_training_begin", 0.0f);
    return 0;
}

int jepa_latent_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_latent_training_end: NULL argument");
        return -1;
    }
    jepa_latent_heartbeat("jepa_latent_training_end", 1.0f);
    return 0;
}

int jepa_latent_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_latent_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    jepa_latent_heartbeat("jepa_latent_training_step", progress);
    return 0;
}
