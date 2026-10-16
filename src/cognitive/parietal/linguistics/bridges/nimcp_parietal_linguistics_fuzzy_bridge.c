/**
 * @file nimcp_parietal_linguistics_fuzzy_bridge.c
 * @brief Fuzzy Logic Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-31
 */

#include "cognitive/parietal/linguistics/bridges/nimcp_parietal_linguistics_fuzzy_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_mesh.h"
#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include "utils/memory/nimcp_memory.h"
#include "constants/nimcp_learning_constants.h"

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define LOG_MODULE_LING_FUZZY "LING_FUZZY"
#define FUZZY_BRIDGE_MAGIC 0x46555A5A  /* "FUZZ" */

/* Default MF parameters */
#define DEFAULT_NEAR_SIGMA      1.0f
#define DEFAULT_FAR_THRESHOLD   5.0f
#define DEFAULT_ANGLE_SIGMA     (M_PI / 6.0f)

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Internal fuzzy bridge state
 */
struct ling_fuzzy_bridge {
    uint32_t magic;                     /**< Validation magic */

    /* Configuration */
    ling_fuzzy_bridge_config_t config;

    /* Preposition definitions */
    ling_fuzzy_preposition_t prepositions[LING_FUZZY_MAX_PREPOSITIONS];
    uint32_t num_prepositions;

    /* Context state */
    float current_context_scale;

    /* Mesh integration */
    linguistics_mesh_t* mesh;
    bool mesh_registered;
    linguistics_belief_t current_belief;
    float current_precision;

    /* Statistics */
    ling_fuzzy_bridge_stats_t stats;

    /* Thread safety */
    void* mutex;  /* nimcp_mutex_t* */

    /* Timing */
    uint64_t creation_time_ms;
    uint64_t last_eval_time_us;
};

/* ============================================================================
 * THREAD-LOCAL ERROR
 * ============================================================================ */

static _Thread_local char s_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_last_error, sizeof(s_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static bool is_valid_bridge(const ling_fuzzy_bridge_t* bridge) {
    return bridge && bridge->magic == FUZZY_BRIDGE_MAGIC;
}

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static uint64_t get_time_ms(void) {
    return get_time_us() / 1000ULL;
}

/**
 * @brief Evaluate a single membership function
 */
static float evaluate_mf(const fuzzy_mf_t* mf, float x) {
    if (!mf) return 0.0f;

    float result = 0.0f;

    switch (mf->type) {
        case FUZZY_MF_TRIANGULAR: {
            /* Params: [a, b, c] - left foot, peak, right foot */
            float a = mf->params[0];
            float b = mf->params[1];
            float c = mf->params[2];
            if (x <= a || x >= c) {
                result = 0.0f;
            } else if (x <= b) {
                result = (x - a) / (b - a);
            } else {
                result = (c - x) / (c - b);
            }
            break;
        }

        case FUZZY_MF_TRAPEZOIDAL: {
            /* Params: [a, b, c, d] */
            float a = mf->params[0];
            float b = mf->params[1];
            float c = mf->params[2];
            float d = mf->params[3];
            if (x <= a || x >= d) {
                result = 0.0f;
            } else if (x >= b && x <= c) {
                result = 1.0f;
            } else if (x < b) {
                result = (x - a) / (b - a);
            } else {
                result = (d - x) / (d - c);
            }
            break;
        }

        case FUZZY_MF_GAUSSIAN: {
            /* Params: [mean, sigma] */
            float mean = mf->params[0];
            float sigma = mf->params[1];
            if (sigma <= 0.0f) sigma = 1.0f;
            float z = (x - mean) / sigma;
            result = expf(-0.5f * z * z);
            break;
        }

        case FUZZY_MF_SIGMOID: {
            /* Params: [a, c] - slope, center */
            float a = mf->params[0];
            float c = mf->params[1];
            result = 1.0f / (1.0f + expf(-a * (x - c)));
            break;
        }

        case FUZZY_MF_S_SHAPED: {
            /* Params: [a, b] - foot, shoulder */
            float a = mf->params[0];
            float b = mf->params[1];
            if (x <= a) {
                result = 0.0f;
            } else if (x >= b) {
                result = 1.0f;
            } else {
                float mid = (a + b) / 2.0f;
                if (x <= mid) {
                    float t = (x - a) / (mid - a);
                    result = 2.0f * t * t;
                } else {
                    float t = (x - mid) / (b - mid);
                    result = 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
                }
            }
            break;
        }

        case FUZZY_MF_Z_SHAPED: {
            /* Params: [a, b] - shoulder, foot */
            float a = mf->params[0];
            float b = mf->params[1];
            if (x <= a) {
                result = 1.0f;
            } else if (x >= b) {
                result = 0.0f;
            } else {
                float mid = (a + b) / 2.0f;
                if (x <= mid) {
                    float t = (x - a) / (mid - a);
                    result = 1.0f - 2.0f * t * t;
                } else {
                    float t = (x - mid) / (b - mid);
                    result = 2.0f * (1.0f - t) * (1.0f - t);
                }
            }
            break;
        }

        case FUZZY_MF_BELL: {
            /* Params: [a, b, c] - width, slope, center */
            float a = mf->params[0];
            float b = mf->params[1];
            float c = mf->params[2];
            if (a <= 0.0f) a = 1.0f;
            float t = fabsf((x - c) / a);
            result = 1.0f / (1.0f + powf(t, 2.0f * b));
            break;
        }

        default:
            result = 0.0f;
            break;
    }

    /* Clamp to [0, 1] */
    if (result < 0.0f) result = 0.0f;
    if (result > 1.0f) result = 1.0f;

    return result;
}

/**
 * @brief Apply linguistic hedge to membership value
 */
static float apply_hedge_internal(float mu, fuzzy_hedge_t hedge) {
    switch (hedge) {
        case FUZZY_HEDGE_NONE:
            return mu;
        case FUZZY_HEDGE_VERY:
            /* Concentration: μ² */
            return mu * mu;
        case FUZZY_HEDGE_SOMEWHAT:
            /* Dilation: √μ */
            return sqrtf(mu);
        case FUZZY_HEDGE_EXTREMELY:
            /* Strong concentration: μ³ */
            return mu * mu * mu;
        case FUZZY_HEDGE_SLIGHTLY:
            /* Intensification around 0.5 */
            if (mu <= 0.5f) {
                return 2.0f * mu * mu;
            } else {
                return 1.0f - 2.0f * (1.0f - mu) * (1.0f - mu);
            }
        case FUZZY_HEDGE_NOT:
            /* Complement: 1 - μ */
            return 1.0f - mu;
        case FUZZY_HEDGE_MORE_OR_LESS:
            /* Mild dilation: μ^0.75 */
            return powf(mu, 0.75f);
        case FUZZY_HEDGE_INDEED:
            /* Intensification: shifts toward 0 or 1 */
            if (mu <= 0.5f) {
                return 2.0f * mu * mu;
            } else {
                return 1.0f - 2.0f * (1.0f - mu) * (1.0f - mu);
            }
        default:
            return mu;
    }
}

/**
 * @brief Initialize default preposition MFs
 */
static void init_default_prepositions(ling_fuzzy_bridge_t* bridge) {
    /* NEAR - Gaussian centered at 0 */
    ling_fuzzy_preposition_t* near_prep = &bridge->prepositions[SPATIAL_PREP_NEAR];
    near_prep->preposition = SPATIAL_PREP_NEAR;
    strncpy(near_prep->name, "near", sizeof(near_prep->name) - 1);
    near_prep->distance_mf.type = FUZZY_MF_GAUSSIAN;
    near_prep->distance_mf.params[0] = 0.0f;  /* mean */
    near_prep->distance_mf.params[1] = bridge->config.default_near_sigma;  /* sigma */
    near_prep->distance_mf.num_params = 2;
    near_prep->has_distance = true;
    near_prep->is_symmetric = true;
    near_prep->salience = 0.8f;

    /* FAR - Z-shaped (high at large distances) */
    ling_fuzzy_preposition_t* far_prep = &bridge->prepositions[SPATIAL_PREP_FAR];
    far_prep->preposition = SPATIAL_PREP_FAR;
    strncpy(far_prep->name, "far", sizeof(far_prep->name) - 1);
    far_prep->distance_mf.type = FUZZY_MF_S_SHAPED;
    far_prep->distance_mf.params[0] = bridge->config.default_far_threshold * 0.5f;  /* foot */
    far_prep->distance_mf.params[1] = bridge->config.default_far_threshold;  /* shoulder */
    far_prep->distance_mf.num_params = 2;
    far_prep->has_distance = true;
    far_prep->is_symmetric = true;
    far_prep->salience = 0.7f;

    /* LEFT - Angular Gaussian centered at -π/2 */
    ling_fuzzy_preposition_t* left_prep = &bridge->prepositions[SPATIAL_PREP_LEFT];
    left_prep->preposition = SPATIAL_PREP_LEFT;
    strncpy(left_prep->name, "left", sizeof(left_prep->name) - 1);
    left_prep->angle_mf.type = FUZZY_MF_GAUSSIAN;
    left_prep->angle_mf.params[0] = -M_PI / 2.0f;  /* mean = -90° */
    left_prep->angle_mf.params[1] = bridge->config.default_angle_sigma;  /* sigma */
    left_prep->angle_mf.num_params = 2;
    left_prep->has_angle = true;
    left_prep->is_symmetric = false;
    left_prep->salience = 0.9f;

    /* RIGHT - Angular Gaussian centered at +π/2 */
    ling_fuzzy_preposition_t* right_prep = &bridge->prepositions[SPATIAL_PREP_RIGHT];
    right_prep->preposition = SPATIAL_PREP_RIGHT;
    strncpy(right_prep->name, "right", sizeof(right_prep->name) - 1);
    right_prep->angle_mf.type = FUZZY_MF_GAUSSIAN;
    right_prep->angle_mf.params[0] = M_PI / 2.0f;  /* mean = +90° */
    right_prep->angle_mf.params[1] = bridge->config.default_angle_sigma;
    right_prep->angle_mf.num_params = 2;
    right_prep->has_angle = true;
    right_prep->is_symmetric = false;
    right_prep->salience = 0.9f;

    /* ABOVE - Height Gaussian centered at positive */
    ling_fuzzy_preposition_t* above_prep = &bridge->prepositions[SPATIAL_PREP_ABOVE];
    above_prep->preposition = SPATIAL_PREP_ABOVE;
    strncpy(above_prep->name, "above", sizeof(above_prep->name) - 1);
    above_prep->height_mf.type = FUZZY_MF_S_SHAPED;
    above_prep->height_mf.params[0] = 0.0f;   /* foot */
    above_prep->height_mf.params[1] = 1.0f;   /* shoulder */
    above_prep->height_mf.num_params = 2;
    above_prep->has_height = true;
    above_prep->is_symmetric = false;
    above_prep->salience = 0.85f;

    /* BELOW - Height Z-shaped (high at negative) */
    ling_fuzzy_preposition_t* below_prep = &bridge->prepositions[SPATIAL_PREP_BELOW];
    below_prep->preposition = SPATIAL_PREP_BELOW;
    strncpy(below_prep->name, "below", sizeof(below_prep->name) - 1);
    below_prep->height_mf.type = FUZZY_MF_Z_SHAPED;
    below_prep->height_mf.params[0] = -1.0f;  /* shoulder */
    below_prep->height_mf.params[1] = 0.0f;   /* foot */
    below_prep->height_mf.num_params = 2;
    below_prep->has_height = true;
    below_prep->is_symmetric = false;
    below_prep->salience = 0.85f;

    /* IN - Triangular distance centered at 0 (containment) */
    ling_fuzzy_preposition_t* in_prep = &bridge->prepositions[SPATIAL_PREP_IN];
    in_prep->preposition = SPATIAL_PREP_IN;
    strncpy(in_prep->name, "in", sizeof(in_prep->name) - 1);
    in_prep->distance_mf.type = FUZZY_MF_TRIANGULAR;
    in_prep->distance_mf.params[0] = -0.5f;  /* a */
    in_prep->distance_mf.params[1] = 0.0f;   /* b (peak) */
    in_prep->distance_mf.params[2] = 0.5f;   /* c */
    in_prep->distance_mf.num_params = 3;
    in_prep->has_distance = true;
    in_prep->is_symmetric = true;
    in_prep->salience = 0.95f;

    /* ON - Similar to IN but with support relation implied */
    ling_fuzzy_preposition_t* on_prep = &bridge->prepositions[SPATIAL_PREP_ON];
    on_prep->preposition = SPATIAL_PREP_ON;
    strncpy(on_prep->name, "on", sizeof(on_prep->name) - 1);
    on_prep->distance_mf.type = FUZZY_MF_TRIANGULAR;
    on_prep->distance_mf.params[0] = -0.3f;
    on_prep->distance_mf.params[1] = 0.0f;
    on_prep->distance_mf.params[2] = 0.3f;
    on_prep->distance_mf.num_params = 3;
    on_prep->height_mf.type = FUZZY_MF_GAUSSIAN;
    on_prep->height_mf.params[0] = 0.0f;  /* mean: at same height */
    on_prep->height_mf.params[1] = 0.2f;  /* small sigma */
    on_prep->height_mf.num_params = 2;
    on_prep->has_distance = true;
    on_prep->has_height = true;
    on_prep->is_symmetric = false;
    on_prep->salience = 0.9f;

    /* BETWEEN - requires two reference points (use distance as midpoint deviation) */
    ling_fuzzy_preposition_t* between_prep = &bridge->prepositions[SPATIAL_PREP_BETWEEN];
    between_prep->preposition = SPATIAL_PREP_BETWEEN;
    strncpy(between_prep->name, "between", sizeof(between_prep->name) - 1);
    between_prep->distance_mf.type = FUZZY_MF_GAUSSIAN;
    between_prep->distance_mf.params[0] = 0.0f;  /* midpoint */
    between_prep->distance_mf.params[1] = 0.5f;  /* sigma */
    between_prep->distance_mf.num_params = 2;
    between_prep->has_distance = true;
    between_prep->is_symmetric = true;
    between_prep->salience = 0.75f;

    bridge->num_prepositions = SPATIAL_PREPOSITION_COUNT;
}

/* ============================================================================
 * LIFECYCLE API IMPLEMENTATION
 * ============================================================================ */

ling_fuzzy_bridge_config_t ling_fuzzy_bridge_default_config(void) {
    ling_fuzzy_bridge_config_t config = {
        .default_near_sigma = DEFAULT_NEAR_SIGMA,
        .default_far_threshold = DEFAULT_FAR_THRESHOLD,
        .default_angle_sigma = DEFAULT_ANGLE_SIGMA,
        .base_precision = LING_FUZZY_DEFAULT_PRECISION,
        .precision_decay = NIMCP_EMA_DECAY_FAST,
        .enable_mesh = true,
        .mesh_learning_rate = NIMCP_LEARNING_RATE_COARSE,
        .enable_bbb = true,
        .enable_health = true,
        .enable_logging = true,
        .indoor_scale = 0.5f,
        .outdoor_scale = 2.0f
    };
    return config;
}

ling_fuzzy_bridge_t* ling_fuzzy_bridge_create(
    const ling_fuzzy_bridge_config_t* config
) {
    ling_fuzzy_bridge_t* bridge = (ling_fuzzy_bridge_t*)nimcp_calloc(1, sizeof(ling_fuzzy_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate fuzzy bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ling_fuzzy_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->magic = FUZZY_BRIDGE_MAGIC;
    bridge->config = config ? *config : ling_fuzzy_bridge_default_config();
    bridge->current_context_scale = 1.0f;
    bridge->current_precision = bridge->config.base_precision;
    bridge->creation_time_ms = get_time_ms();

    /* Initialize default preposition MFs */
    init_default_prepositions(bridge);

    /* Initialize current belief */
    bridge->current_belief.certainty = 0.5f;
    bridge->current_belief.precision = bridge->current_precision;

    if (bridge->config.enable_logging) {
        /* LOG_MODULE_INFO(LOG_MODULE_LING_FUZZY, "Fuzzy bridge created with %u prepositions",
                        bridge->num_prepositions); */
    }

    return bridge;
}

void ling_fuzzy_bridge_destroy(ling_fuzzy_bridge_t* bridge) {
    if (!is_valid_bridge(bridge)) {
        return;
    }

    bridge->magic = 0;  /* Invalidate */
    nimcp_free(bridge);
    bridge = NULL;
}

int ling_fuzzy_bridge_register_mesh(
    ling_fuzzy_bridge_t* bridge,
    linguistics_mesh_t* mesh
) {
    if (!is_valid_bridge(bridge)) {
        set_error("Invalid bridge");
        return LING_FUZZY_ERR_NULL;
    }
    if (!mesh) {
        set_error("NULL mesh coordinator");
        return LING_FUZZY_ERR_NULL;
    }

    /* Get mesh handler interface */
    linguistics_mesh_handler_t handler;
    int ret = ling_fuzzy_get_mesh_handler(bridge, &handler);
    if (ret != LING_FUZZY_ERR_OK) {
        return ret;
    }

    /* Register with mesh */
    ret = linguistics_mesh_register_participant(
        mesh,
        BIO_MODULE_LING_FUZZY_BRIDGE,
        "fuzzy_bridge",
        handler
    );
    if (ret != 0) {
        set_error("Failed to register with mesh: %d", ret);
        return LING_FUZZY_ERR_MESH_REGISTER;
    }

    bridge->mesh = mesh;
    bridge->mesh_registered = true;

    if (bridge->config.enable_logging) {
        /* LOG_MODULE_INFO(LOG_MODULE_LING_FUZZY, "Registered with linguistics mesh"); */
    }

    return LING_FUZZY_ERR_OK;
}

/* ============================================================================
 * FUZZY EVALUATION API IMPLEMENTATION
 * ============================================================================ */

int ling_fuzzy_evaluate_preposition(
    ling_fuzzy_bridge_t* bridge,
    spatial_preposition_t preposition,
    float distance,
    float angle,
    float height,
    ling_fuzzy_result_t* result
) {
    if (!is_valid_bridge(bridge)) {
        set_error("Invalid bridge");
        return LING_FUZZY_ERR_NULL;
    }
    if (!result) {
        set_error("NULL result pointer");
        return LING_FUZZY_ERR_NULL;
    }
    if (preposition < 0 || preposition >= SPATIAL_PREPOSITION_COUNT) {
        set_error("Invalid preposition: %d", preposition);
        return LING_FUZZY_ERR_INVALID_PREP;
    }

    uint64_t start_us = get_time_us();
    memset(result, 0, sizeof(*result));

    const ling_fuzzy_preposition_t* prep = &bridge->prepositions[preposition];

    /* Apply context scale */
    float scaled_distance = distance / bridge->current_context_scale;
    float scaled_height = height / bridge->current_context_scale;

    /* Evaluate each dimension */
    float membership = 1.0f;
    int dim_count = 0;

    if (prep->has_distance && !isnan(distance)) {
        result->distance_membership = evaluate_mf(&prep->distance_mf, scaled_distance);
        membership *= result->distance_membership;
        dim_count++;
    }

    if (prep->has_angle && !isnan(angle)) {
        result->angle_membership = evaluate_mf(&prep->angle_mf, angle);
        membership *= result->angle_membership;
        dim_count++;
    }

    if (prep->has_height && !isnan(height)) {
        result->height_membership = evaluate_mf(&prep->height_mf, scaled_height);
        membership *= result->height_membership;
        dim_count++;
    }

    /* If no dimensions evaluated, default to 0 */
    if (dim_count == 0) {
        membership = 0.0f;
    }

    /* Compute precision based on sharpness of membership */
    /* Higher membership = higher precision */
    float precision = bridge->config.base_precision;
    if (membership > 0.5f) {
        precision *= 1.0f + (membership - 0.5f);
    } else {
        precision *= bridge->config.precision_decay;
    }
    if (precision < LING_FUZZY_PRECISION_FLOOR) {
        precision = LING_FUZZY_PRECISION_FLOOR;
    }
    if (precision > LING_FUZZY_PRECISION_CEILING) {
        precision = LING_FUZZY_PRECISION_CEILING;
    }

    result->membership = membership;
    result->confidence = membership;  /* Use membership as confidence */
    result->precision = precision;
    result->preposition = preposition;
    result->hedge_applied = FUZZY_HEDGE_NONE;
    result->crisp_distance = distance;
    result->crisp_angle = angle;
    result->crisp_height = height;

    /* Update stats */
    bridge->stats.total_evaluations++;
    uint64_t elapsed_us = get_time_us() - start_us;
    bridge->stats.avg_latency_us = (bridge->stats.avg_latency_us * 0.99f) + ((float)elapsed_us * 0.01f);
    if (isfinite(membership)) {
        bridge->stats.avg_membership = (bridge->stats.avg_membership * 0.99f) + (membership * 0.01f);
    }
    if (isfinite(precision)) {
        bridge->stats.avg_precision = (bridge->stats.avg_precision * 0.99f) + (precision * 0.01f);
    }

    /* Update current precision for mesh */
    bridge->current_precision = precision;

    return LING_FUZZY_ERR_OK;
}

int ling_fuzzy_apply_hedge(
    ling_fuzzy_bridge_t* bridge,
    float membership,
    fuzzy_hedge_t hedge,
    float* result
) {
    if (!is_valid_bridge(bridge)) {
        return LING_FUZZY_ERR_NULL;
    }
    if (!result) {
        return LING_FUZZY_ERR_NULL;
    }
    if (hedge < 0 || hedge >= FUZZY_HEDGE_TYPE_COUNT) {
        return LING_FUZZY_ERR_INVALID_HEDGE;
    }

    *result = apply_hedge_internal(membership, hedge);
    bridge->stats.hedge_applications++;

    return LING_FUZZY_ERR_OK;
}

int ling_fuzzy_evaluate_hedged(
    ling_fuzzy_bridge_t* bridge,
    spatial_preposition_t preposition,
    fuzzy_hedge_t hedge,
    float distance,
    float angle,
    float height,
    ling_fuzzy_result_t* result
) {
    /* First evaluate base preposition */
    int ret = ling_fuzzy_evaluate_preposition(bridge, preposition,
                                               distance, angle, height, result);
    if (ret != LING_FUZZY_ERR_OK) {
        return ret;
    }

    /* Apply hedge */
    if (hedge != FUZZY_HEDGE_NONE) {
        result->membership = apply_hedge_internal(result->membership, hedge);
        result->hedge_applied = hedge;
        bridge->stats.hedge_applications++;
    }

    return LING_FUZZY_ERR_OK;
}

int ling_fuzzy_select_preposition(
    ling_fuzzy_bridge_t* bridge,
    float distance,
    float angle,
    float height,
    spatial_preposition_t* preposition,
    float* membership
) {
    if (!is_valid_bridge(bridge) || !preposition || !membership) {
        return LING_FUZZY_ERR_NULL;
    }

    float best_membership = 0.0f;
    spatial_preposition_t best_prep = SPATIAL_PREP_NEAR;

    ling_fuzzy_result_t result;
    for (uint32_t i = 0; i < bridge->num_prepositions; i++) {
        int ret = ling_fuzzy_evaluate_preposition(bridge, (spatial_preposition_t)i,
                                                   distance, angle, height, &result);
        if (ret == LING_FUZZY_ERR_OK && result.membership > best_membership) {
            best_membership = result.membership;
            best_prep = (spatial_preposition_t)i;
        }
    }

    *preposition = best_prep;
    *membership = best_membership;

    return LING_FUZZY_ERR_OK;
}

/* ============================================================================
 * MEMBERSHIP FUNCTION MANAGEMENT API
 * ============================================================================ */

int ling_fuzzy_get_preposition_mf(
    const ling_fuzzy_bridge_t* bridge,
    spatial_preposition_t preposition,
    ling_fuzzy_dimension_t dimension,
    fuzzy_mf_t* mf
) {
    if (!is_valid_bridge(bridge) || !mf) {
        return LING_FUZZY_ERR_NULL;
    }
    if (preposition < 0 || preposition >= SPATIAL_PREPOSITION_COUNT) {
        return LING_FUZZY_ERR_INVALID_PREP;
    }

    const ling_fuzzy_preposition_t* prep = &bridge->prepositions[preposition];

    switch (dimension) {
        case LING_FUZZY_DIM_DISTANCE:
            *mf = prep->distance_mf;
            break;
        case LING_FUZZY_DIM_ANGLE:
            *mf = prep->angle_mf;
            break;
        case LING_FUZZY_DIM_HEIGHT:
            *mf = prep->height_mf;
            break;
        default:
            return LING_FUZZY_ERR_INVALID_PREP;
    }

    return LING_FUZZY_ERR_OK;
}

int ling_fuzzy_set_preposition_mf(
    ling_fuzzy_bridge_t* bridge,
    spatial_preposition_t preposition,
    ling_fuzzy_dimension_t dimension,
    const fuzzy_mf_t* mf
) {
    if (!is_valid_bridge(bridge) || !mf) {
        return LING_FUZZY_ERR_NULL;
    }
    if (preposition < 0 || preposition >= SPATIAL_PREPOSITION_COUNT) {
        return LING_FUZZY_ERR_INVALID_PREP;
    }

    ling_fuzzy_preposition_t* prep = &bridge->prepositions[preposition];

    switch (dimension) {
        case LING_FUZZY_DIM_DISTANCE:
            prep->distance_mf = *mf;
            prep->has_distance = true;
            break;
        case LING_FUZZY_DIM_ANGLE:
            prep->angle_mf = *mf;
            prep->has_angle = true;
            break;
        case LING_FUZZY_DIM_HEIGHT:
            prep->height_mf = *mf;
            prep->has_height = true;
            break;
        default:
            return LING_FUZZY_ERR_INVALID_PREP;
    }

    return LING_FUZZY_ERR_OK;
}

int ling_fuzzy_set_context_scale(ling_fuzzy_bridge_t* bridge, float scale) {
    if (!is_valid_bridge(bridge)) {
        return LING_FUZZY_ERR_NULL;
    }
    if (scale <= 0.0f) {
        return LING_FUZZY_ERR_INVALID_PREP;
    }

    bridge->current_context_scale = scale;
    return LING_FUZZY_ERR_OK;
}

/* ============================================================================
 * MESH HANDLER INTERFACE IMPLEMENTATION
 * ============================================================================ */

int ling_fuzzy_mesh_process(
    void* ctx,
    const linguistics_request_t* request,
    linguistics_belief_t* belief
) {
    ling_fuzzy_bridge_t* bridge = (ling_fuzzy_bridge_t*)ctx;
    if (!is_valid_bridge(bridge) || !request || !belief) {
        return LING_FUZZY_ERR_NULL;
    }

    memset(belief, 0, sizeof(*belief));

    /* Process based on request type */
    if (request->type == LING_REQUEST_PARSE_SPATIAL ||
        request->type == LING_REQUEST_PARSE_SPATIAL) {
        /* Try to evaluate if we have spatial semantics */
        /* For now, produce a baseline belief */
        belief->certainty = 0.5f;
        belief->precision = bridge->current_precision;

        /* If input contains spatial info, evaluate */
        if (request->input_flags & 0x01) {  /* Has distance */
            ling_fuzzy_result_t result;
            int ret = ling_fuzzy_evaluate_preposition(
                bridge,
                SPATIAL_PREP_NEAR,  /* Default */
                request->input_magnitude,
                NAN,
                NAN,
                &result
            );
            if (ret == LING_FUZZY_ERR_OK) {
                belief->certainty = result.membership;
                belief->precision = result.precision;
            }
        }
    }

    /* Encode belief into vector (simple encoding) */
    belief->belief_vector[0] = belief->certainty;
    belief->vector_dim = 1;

    bridge->stats.mesh_contributions++;

    return LING_FUZZY_ERR_OK;
}

int ling_fuzzy_mesh_update(
    void* ctx,
    const linguistics_belief_t* neighbors,
    uint32_t count,
    linguistics_belief_t* updated
) {
    ling_fuzzy_bridge_t* bridge = (ling_fuzzy_bridge_t*)ctx;
    if (!is_valid_bridge(bridge) || !neighbors || !updated) {
        return LING_FUZZY_ERR_NULL;
    }

    /* Start with current belief */
    *updated = bridge->current_belief;

    /* FEP update: μ' = μ - lr × Σ(Π_i × ε_i) */
    float lr = bridge->config.mesh_learning_rate;

    for (uint32_t i = 0; i < count; i++) {
        const linguistics_belief_t* neighbor = &neighbors[i];

        /* Compute prediction error */
        float error = neighbor->certainty - updated->certainty;

        /* Precision-weighted update */
        float weight = neighbor->precision * lr;
        updated->certainty += weight * error;

        /* Update belief vector */
        for (uint32_t d = 0; d < updated->vector_dim && d < neighbor->vector_dim; d++) {
            float vec_error = neighbor->belief_vector[d] - updated->belief_vector[d];
            updated->belief_vector[d] += weight * vec_error;
        }
    }

    /* Clamp certainty */
    if (updated->certainty < 0.0f) updated->certainty = 0.0f;
    if (updated->certainty > 1.0f) updated->certainty = 1.0f;

    /* Update internal state */
    bridge->current_belief = *updated;
    bridge->stats.mesh_updates++;

    return LING_FUZZY_ERR_OK;
}

float ling_fuzzy_mesh_get_precision(void* ctx) {
    ling_fuzzy_bridge_t* bridge = (ling_fuzzy_bridge_t*)ctx;
    if (!is_valid_bridge(bridge)) {
        return LING_FUZZY_PRECISION_FLOOR;
    }
    return bridge->current_precision;
}

int ling_fuzzy_get_mesh_handler(
    ling_fuzzy_bridge_t* bridge,
    linguistics_mesh_handler_t* handler
) {
    if (!is_valid_bridge(bridge) || !handler) {
        return LING_FUZZY_ERR_NULL;
    }

    handler->process = ling_fuzzy_mesh_process;
    handler->update = ling_fuzzy_mesh_update;
    handler->get_precision = ling_fuzzy_mesh_get_precision;
    handler->ctx = bridge;

    return LING_FUZZY_ERR_OK;
}

/* ============================================================================
 * INFERENCE API IMPLEMENTATION
 * ============================================================================ */

int ling_fuzzy_infer_spatial(
    ling_fuzzy_bridge_t* bridge,
    const char* phrase,
    spatial_semantics_t* semantics
) {
    if (!is_valid_bridge(bridge) || !phrase || !semantics) {
        return LING_FUZZY_ERR_NULL;
    }

    memset(semantics, 0, sizeof(*semantics));

    /* Parse phrase for preposition and hedge */
    /* Simple implementation - look for known prepositions */
    spatial_preposition_t prep = SPATIAL_PREP_NEAR;
    fuzzy_hedge_t hedge = FUZZY_HEDGE_NONE;

    /* Check for hedges first */
    if (strstr(phrase, "very") != NULL) {
        hedge = FUZZY_HEDGE_VERY;
    } else if (strstr(phrase, "somewhat") != NULL) {
        hedge = FUZZY_HEDGE_SOMEWHAT;
    } else if (strstr(phrase, "extremely") != NULL) {
        hedge = FUZZY_HEDGE_EXTREMELY;
    }

    /* Check for prepositions */
    if (strstr(phrase, "near") != NULL) {
        prep = SPATIAL_PREP_NEAR;
    } else if (strstr(phrase, "far") != NULL) {
        prep = SPATIAL_PREP_FAR;
    } else if (strstr(phrase, "left") != NULL) {
        prep = SPATIAL_PREP_LEFT;
    } else if (strstr(phrase, "right") != NULL) {
        prep = SPATIAL_PREP_RIGHT;
    } else if (strstr(phrase, "above") != NULL) {
        prep = SPATIAL_PREP_ABOVE;
    } else if (strstr(phrase, "below") != NULL) {
        prep = SPATIAL_PREP_BELOW;
    } else if (strstr(phrase, "in") != NULL) {
        prep = SPATIAL_PREP_IN;
    } else if (strstr(phrase, "on") != NULL) {
        prep = SPATIAL_PREP_ON;
    } else if (strstr(phrase, "between") != NULL) {
        prep = SPATIAL_PREP_BETWEEN;
    }

    semantics->preposition = prep;
    semantics->hedge_type = hedge;
    semantics->hedge_applied = (hedge != FUZZY_HEDGE_NONE);

    /* Get MF parameters for the preposition */
    const ling_fuzzy_preposition_t* prep_def = &bridge->prepositions[prep];
    if (prep_def->has_distance) {
        semantics->distance_center = prep_def->distance_mf.params[0];
        semantics->distance_spread = prep_def->distance_mf.params[1];
        semantics->distance_membership = 1.0f;  /* Default full membership */
    }
    if (prep_def->has_angle) {
        semantics->angle_membership = 1.0f;  /* Default full membership */
    }

    semantics->overall_confidence = prep_def->salience;

    return LING_FUZZY_ERR_OK;
}

int ling_fuzzy_defuzzify_distance(
    ling_fuzzy_bridge_t* bridge,
    spatial_preposition_t preposition,
    float membership,
    float* distance
) {
    if (!is_valid_bridge(bridge) || !distance) {
        return LING_FUZZY_ERR_NULL;
    }
    if (preposition < 0 || preposition >= SPATIAL_PREPOSITION_COUNT) {
        return LING_FUZZY_ERR_INVALID_PREP;
    }

    const ling_fuzzy_preposition_t* prep = &bridge->prepositions[preposition];
    if (!prep->has_distance) {
        *distance = 0.0f;
        return LING_FUZZY_ERR_OK;
    }

    /* Simple defuzzification: for Gaussian, solve for x given μ */
    /* μ = exp(-0.5 * ((x - mean) / sigma)^2) */
    /* ln(μ) = -0.5 * ((x - mean) / sigma)^2 */
    /* x = mean ± sigma * sqrt(-2 * ln(μ)) */
    if (prep->distance_mf.type == FUZZY_MF_GAUSSIAN) {
        float mean = prep->distance_mf.params[0];
        float sigma = prep->distance_mf.params[1];
        if (membership > 0.0f && membership < 1.0f) {
            float term = sqrtf(-2.0f * logf(membership));
            *distance = mean + sigma * term;  /* Take positive side */
        } else if (membership >= 1.0f) {
            *distance = mean;
        } else {
            *distance = mean + 3.0f * sigma;  /* ~99.7% of distribution */
        }
    } else {
        /* Default: linear approximation */
        *distance = (1.0f - membership) * bridge->config.default_far_threshold;
    }

    /* Apply context scale */
    *distance *= bridge->current_context_scale;

    return LING_FUZZY_ERR_OK;
}

/* ============================================================================
 * STATISTICS API IMPLEMENTATION
 * ============================================================================ */

int ling_fuzzy_bridge_get_stats(
    const ling_fuzzy_bridge_t* bridge,
    ling_fuzzy_bridge_stats_t* stats
) {
    if (!is_valid_bridge(bridge) || !stats) {
        return LING_FUZZY_ERR_NULL;
    }

    *stats = bridge->stats;
    return LING_FUZZY_ERR_OK;
}

void ling_fuzzy_bridge_reset_stats(ling_fuzzy_bridge_t* bridge) {
    if (!is_valid_bridge(bridge)) {
        return;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

const char* ling_fuzzy_bridge_get_last_error(void) {
    return s_last_error;
}

/* ============================================================================
 * UTILITY API IMPLEMENTATION
 * ============================================================================ */

const char* ling_fuzzy_preposition_name(spatial_preposition_t preposition) {
    static const char* names[] = {
        "near", "far", "left", "right", "above", "below",
        "in", "on", "at", "between", "behind", "front",
        "beside", "around", "through", "across", "along",
        "toward", "away", "inside", "outside", "under", "over"
    };

    if (preposition >= 0 && preposition < (int)(sizeof(names) / sizeof(names[0]))) {
        return names[preposition];
    }
    return "unknown";
}

const char* ling_fuzzy_hedge_name(fuzzy_hedge_t hedge) {
    static const char* names[] = {
        "none", "very", "somewhat", "extremely",
        "slightly", "not", "more_or_less", "indeed"
    };

    if (hedge >= 0 && hedge < FUZZY_HEDGE_TYPE_COUNT) {
        return names[hedge];
    }
    return "unknown";
}

int ling_fuzzy_parse_hedge(const char* str, fuzzy_hedge_t* hedge) {
    if (!str || !hedge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ling_fuzzy_parse_hedge: required parameter is NULL (str, hedge)");
        return -1;
    }

    if (strcasecmp(str, "very") == 0) {
        *hedge = FUZZY_HEDGE_VERY;
    } else if (strcasecmp(str, "somewhat") == 0) {
        *hedge = FUZZY_HEDGE_SOMEWHAT;
    } else if (strcasecmp(str, "extremely") == 0) {
        *hedge = FUZZY_HEDGE_EXTREMELY;
    } else if (strcasecmp(str, "slightly") == 0) {
        *hedge = FUZZY_HEDGE_SLIGHTLY;
    } else if (strcasecmp(str, "not") == 0) {
        *hedge = FUZZY_HEDGE_NOT;
    } else if (strcasecmp(str, "more_or_less") == 0 ||
               strcasecmp(str, "more or less") == 0) {
        *hedge = FUZZY_HEDGE_MORE_OR_LESS;
    } else if (strcasecmp(str, "indeed") == 0) {
        *hedge = FUZZY_HEDGE_INDEED;
    } else {
        *hedge = FUZZY_HEDGE_NONE;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ling_fuzzy_parse_hedge: operation failed");
        return -1;  /* Unknown hedge */
    }

    return 0;
}
