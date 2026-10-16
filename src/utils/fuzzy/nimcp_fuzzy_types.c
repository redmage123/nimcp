//=============================================================================
// nimcp_fuzzy_types.c - Core Fuzzy Logic Type Implementations
//=============================================================================
/**
 * @file nimcp_fuzzy_types.c
 * @brief Implementation of fuzzy membership functions, sets, variables, hedges
 *
 * WHAT: 14 membership function evaluations, 8 hedges, fuzzy set/variable
 *       operations, discrete set operations, entropy/cardinality utilities
 * WHY:  Foundation for all fuzzy inference and cross-module integration
 * HOW:  Each MF type is evaluated via closed-form mathematical expressions;
 *       hedges transform membership degrees via power/complement operations
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include "constants/nimcp_buffer_constants.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdarg.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fuzzy_types)

//=============================================================================
// Exception Handling
//=============================================================================
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

#ifdef _MSC_VER
static __declspec(thread) char tls_fuzzy_types_error[NIMCP_ERROR_BUFFER_SIZE] = {0};
#else
static __thread char tls_fuzzy_types_error[NIMCP_ERROR_BUFFER_SIZE] = {0};
#endif

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(tls_fuzzy_types_error, sizeof(tls_fuzzy_types_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Internal Engine Structure
//=============================================================================

struct fuzzy_types_engine {
    fuzzy_types_config_t config;
    fuzzy_types_stats_t stats;
    float inflammation_level;
    float fatigue_level;
};

//=============================================================================
// Lifecycle
//=============================================================================

fuzzy_types_config_t fuzzy_types_default_config(void) {
    fuzzy_types_config_t config;
    memset(&config, 0, sizeof(config));
    config.default_resolution = FUZZY_RESOLUTION;
    config.alpha_cut_default = 0.0f;
    config.enable_caching = false;
    config.cache_size = 1024;
    config.inflammation_sensitivity = 0.3f;
    config.fatigue_sensitivity = 0.2f;
    return config;
}

fuzzy_types_engine_t* fuzzy_types_create(void) {
    return fuzzy_types_create_custom(NULL);
}

fuzzy_types_engine_t* fuzzy_types_create_custom(const fuzzy_types_config_t* config) {
    fuzzy_types_engine_t* engine = (fuzzy_types_engine_t*)nimcp_calloc(1, sizeof(fuzzy_types_engine_t));
    if (!engine) {
        set_error("Failed to allocate fuzzy types engine");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "fuzzy_types_create_custom: Failed to allocate fuzzy types engine");
        return NULL;
    }

    if (config) {
        engine->config = *config;
    } else {
        engine->config = fuzzy_types_default_config();
    }

    memset(&engine->stats, 0, sizeof(engine->stats));
    engine->inflammation_level = 0.0f;
    engine->fatigue_level = 0.0f;

    fuzzy_types_heartbeat("fuzzy_types_create", 1.0f);
    return engine;
}

void fuzzy_types_destroy(fuzzy_types_engine_t* engine) {
    if (!engine) return;
    fuzzy_types_heartbeat("fuzzy_types_destroy", 0.0f);
    nimcp_free(engine);
}

//=============================================================================
// Membership Function Evaluation
//=============================================================================

static float mf_triangular(float x, const float* p) {
    float a = p[0], b = p[1], c = p[2];
    if (x <= a || x >= c) return 0.0f;
    if (x <= b) return (b - a > FUZZY_PRECISION) ? (x - a) / (b - a) : 1.0f;
    return (c - b > FUZZY_PRECISION) ? (c - x) / (c - b) : 1.0f;
}

static float mf_trapezoidal(float x, const float* p) {
    float a = p[0], b = p[1], c = p[2], d = p[3];
    if (x <= a || x >= d) return 0.0f;
    if (x >= b && x <= c) return 1.0f;
    if (x < b) return (b - a > FUZZY_PRECISION) ? (x - a) / (b - a) : 1.0f;
    return (d - c > FUZZY_PRECISION) ? (d - x) / (d - c) : 1.0f;
}

static float mf_gaussian(float x, const float* p) {
    float mean = p[0], sigma = p[1];
    if (fabsf(sigma) < FUZZY_PRECISION) return (fabsf(x - mean) < FUZZY_PRECISION) ? 1.0f : 0.0f;
    float d = (x - mean) / sigma;
    return expf(-0.5f * d * d);
}

static float mf_gaussian_double(float x, const float* p) {
    float mean1 = p[0], sigma1 = p[1], mean2 = p[2], sigma2 = p[3];
    if (x <= mean1) {
        float d = (fabsf(sigma1) < FUZZY_PRECISION) ? 0.0f : (x - mean1) / sigma1;
        return expf(-0.5f * d * d);
    }
    if (x >= mean2) {
        float d = (fabsf(sigma2) < FUZZY_PRECISION) ? 0.0f : (x - mean2) / sigma2;
        return expf(-0.5f * d * d);
    }
    return 1.0f;
}

static float mf_bell(float x, const float* p) {
    float a = p[0], b = p[1], c = p[2];
    if (fabsf(a) < FUZZY_PRECISION) return (fabsf(x - c) < FUZZY_PRECISION) ? 1.0f : 0.0f;
    float base = fabsf((x - c) / a);
    float power = 2.0f * b;
    return 1.0f / (1.0f + powf(base, power));
}

static float mf_sigmoid(float x, const float* p) {
    float a = p[0], c = p[1];
    return 1.0f / (1.0f + expf(-a * (x - c)));
}

static float mf_sigmoid_diff(float x, const float* p) {
    float s1 = 1.0f / (1.0f + expf(-p[0] * (x - p[1])));
    float s2 = 1.0f / (1.0f + expf(-p[2] * (x - p[3])));
    float result = s1 - s2;
    return nimcp_clampf(result, 0.0f, 1.0f);
}

static float mf_sigmoid_prod(float x, const float* p) {
    float s1 = 1.0f / (1.0f + expf(-p[0] * (x - p[1])));
    float s2 = 1.0f / (1.0f + expf(-p[2] * (x - p[3])));
    return s1 * s2;
}

static float mf_s_shaped(float x, const float* p) {
    float a = p[0], b = p[1];
    if (x <= a) return 0.0f;
    if (x >= b) return 1.0f;
    float mid = (a + b) / 2.0f;
    if (x <= mid) {
        float t = (x - a) / (b - a);
        return 2.0f * t * t;
    }
    float t = (b - x) / (b - a);
    return 1.0f - 2.0f * t * t;
}

static float mf_z_shaped(float x, const float* p) {
    float a = p[0], b = p[1];
    if (x <= a) return 1.0f;
    if (x >= b) return 0.0f;
    float mid = (a + b) / 2.0f;
    if (x <= mid) {
        float t = (x - a) / (b - a);
        return 1.0f - 2.0f * t * t;
    }
    float t = (b - x) / (b - a);
    return 2.0f * t * t;
}

static float mf_pi_shaped(float x, const float* p) {
    float a = p[0], b = p[1], c = p[2], d = p[3];
    /* S-shaped from a to b, then Z-shaped from c to d */
    float s_params[2] = {a, b};
    float z_params[2] = {c, d};
    if (x <= b && x >= a) return mf_s_shaped(x, s_params);
    if (x > b && x < c) return 1.0f;
    if (x >= c && x <= d) return mf_z_shaped(x, z_params);
    if (x < a) return 0.0f;
    return 0.0f;
}

static float mf_singleton(float x, const float* p) {
    return (fabsf(x - p[0]) < FUZZY_PRECISION) ? 1.0f : 0.0f;
}

static float mf_piecewise_linear(float x, const float* p, uint32_t num_params) {
    if (num_params < 4) return 0.0f;
    uint32_t num_points = num_params / 2;

    /* Below first point */
    if (x <= p[0]) return nimcp_clampf(p[1], 0.0f, 1.0f);

    /* Above last point */
    if (x >= p[(num_points - 1) * 2]) return nimcp_clampf(p[(num_points - 1) * 2 + 1], 0.0f, 1.0f);

    /* Linear interpolation between points */
    for (uint32_t i = 0; i < num_points - 1; i++) {
        float x0 = p[i * 2], y0 = p[i * 2 + 1];
        float x1 = p[(i + 1) * 2], y1 = p[(i + 1) * 2 + 1];
        if (x >= x0 && x <= x1) {
            float dx = x1 - x0;
            if (fabsf(dx) < FUZZY_PRECISION) return nimcp_clampf(y0, 0.0f, 1.0f);
            float t = (x - x0) / dx;
            return nimcp_clampf(y0 + t * (y1 - y0), 0.0f, 1.0f);
        }
    }
    return 0.0f;
}

float fuzzy_mf_evaluate(const fuzzy_mf_t* mf, float x) {
    if (!mf) return 0.0f;

    float result;
    switch (mf->type) {
        case FUZZY_MF_TRIANGULAR:      result = mf_triangular(x, mf->params); break;
        case FUZZY_MF_TRAPEZOIDAL:     result = mf_trapezoidal(x, mf->params); break;
        case FUZZY_MF_GAUSSIAN:        result = mf_gaussian(x, mf->params); break;
        case FUZZY_MF_GAUSSIAN_DOUBLE: result = mf_gaussian_double(x, mf->params); break;
        case FUZZY_MF_BELL:            result = mf_bell(x, mf->params); break;
        case FUZZY_MF_SIGMOID:         result = mf_sigmoid(x, mf->params); break;
        case FUZZY_MF_SIGMOID_DIFF:    result = mf_sigmoid_diff(x, mf->params); break;
        case FUZZY_MF_SIGMOID_PROD:    result = mf_sigmoid_prod(x, mf->params); break;
        case FUZZY_MF_PI_SHAPED:       result = mf_pi_shaped(x, mf->params); break;
        case FUZZY_MF_S_SHAPED:        result = mf_s_shaped(x, mf->params); break;
        case FUZZY_MF_Z_SHAPED:        result = mf_z_shaped(x, mf->params); break;
        case FUZZY_MF_SINGLETON:       result = mf_singleton(x, mf->params); break;
        case FUZZY_MF_PIECEWISE_LINEAR:result = mf_piecewise_linear(x, mf->params, mf->num_params); break;
        case FUZZY_MF_CUSTOM:
            if (mf->custom_fn) {
                result = mf->custom_fn(x, mf->params, mf->num_params, mf->custom_data);
            } else {
                result = 0.0f;
            }
            break;
        default:
            result = 0.0f;
            break;
    }

    return nimcp_clampf(result, 0.0f, 1.0f);
}

//=============================================================================
// Hedge Application
//=============================================================================

float fuzzy_apply_hedge(float membership, fuzzy_hedge_t hedge) {
    float mu = nimcp_clampf(membership, 0.0f, 1.0f);
    switch (hedge) {
        case FUZZY_HEDGE_NONE:         return mu;
        case FUZZY_HEDGE_VERY:         return mu * mu;
        case FUZZY_HEDGE_SOMEWHAT:     return sqrtf(mu);
        case FUZZY_HEDGE_EXTREMELY:    return mu * mu * mu;
        case FUZZY_HEDGE_SLIGHTLY: {
            /* Intensification around 0.5: increase if < 0.5, decrease if > 0.5 */
            if (mu <= 0.5f) return sqrtf(mu) - mu * mu;
            return mu * mu - sqrtf(mu) + 1.0f;
        }
        case FUZZY_HEDGE_NOT:          return 1.0f - mu;
        case FUZZY_HEDGE_MORE_OR_LESS: return powf(mu, 0.75f);
        case FUZZY_HEDGE_INDEED: {
            /* Intensification: shifts toward 0 or 1 */
            if (mu <= 0.5f) return 2.0f * mu * mu;
            return 1.0f - 2.0f * (1.0f - mu) * (1.0f - mu);
        }
        default: return mu;
    }
}

float fuzzy_mf_evaluate_hedged(const fuzzy_mf_t* mf, float x, fuzzy_hedge_t hedge) {
    float mu = fuzzy_mf_evaluate(mf, x);
    return fuzzy_apply_hedge(mu, hedge);
}

//=============================================================================
// Membership Function Construction Helpers
//=============================================================================

fuzzy_mf_t fuzzy_mf_triangular(float a, float b, float c) {
    fuzzy_mf_t mf;
    memset(&mf, 0, sizeof(mf));
    mf.type = FUZZY_MF_TRIANGULAR;
    mf.params[0] = a; mf.params[1] = b; mf.params[2] = c;
    mf.num_params = 3;
    return mf;
}

fuzzy_mf_t fuzzy_mf_trapezoidal(float a, float b, float c, float d) {
    fuzzy_mf_t mf;
    memset(&mf, 0, sizeof(mf));
    mf.type = FUZZY_MF_TRAPEZOIDAL;
    mf.params[0] = a; mf.params[1] = b; mf.params[2] = c; mf.params[3] = d;
    mf.num_params = 4;
    return mf;
}

fuzzy_mf_t fuzzy_mf_gaussian(float mean, float sigma) {
    fuzzy_mf_t mf;
    memset(&mf, 0, sizeof(mf));
    mf.type = FUZZY_MF_GAUSSIAN;
    mf.params[0] = mean; mf.params[1] = sigma;
    mf.num_params = 2;
    return mf;
}

fuzzy_mf_t fuzzy_mf_bell(float width, float slope, float center) {
    fuzzy_mf_t mf;
    memset(&mf, 0, sizeof(mf));
    mf.type = FUZZY_MF_BELL;
    mf.params[0] = width; mf.params[1] = slope; mf.params[2] = center;
    mf.num_params = 3;
    return mf;
}

fuzzy_mf_t fuzzy_mf_sigmoid(float slope, float center) {
    fuzzy_mf_t mf;
    memset(&mf, 0, sizeof(mf));
    mf.type = FUZZY_MF_SIGMOID;
    mf.params[0] = slope; mf.params[1] = center;
    mf.num_params = 2;
    return mf;
}

fuzzy_mf_t fuzzy_mf_s_shaped(float foot, float shoulder) {
    fuzzy_mf_t mf;
    memset(&mf, 0, sizeof(mf));
    mf.type = FUZZY_MF_S_SHAPED;
    mf.params[0] = foot; mf.params[1] = shoulder;
    mf.num_params = 2;
    return mf;
}

fuzzy_mf_t fuzzy_mf_z_shaped(float shoulder, float foot) {
    fuzzy_mf_t mf;
    memset(&mf, 0, sizeof(mf));
    mf.type = FUZZY_MF_Z_SHAPED;
    mf.params[0] = shoulder; mf.params[1] = foot;
    mf.num_params = 2;
    return mf;
}

fuzzy_mf_t fuzzy_mf_singleton(float x0) {
    fuzzy_mf_t mf;
    memset(&mf, 0, sizeof(mf));
    mf.type = FUZZY_MF_SINGLETON;
    mf.params[0] = x0;
    mf.num_params = 1;
    return mf;
}

fuzzy_mf_t fuzzy_mf_custom(fuzzy_mf_callback_t fn, const float* params,
                             uint32_t num_params, void* user_data) {
    fuzzy_mf_t mf;
    memset(&mf, 0, sizeof(mf));
    mf.type = FUZZY_MF_CUSTOM;
    mf.custom_fn = fn;
    mf.custom_data = user_data;
    if (params && num_params > 0) {
        uint32_t count = (num_params > FUZZY_MAX_PARAMS) ? FUZZY_MAX_PARAMS : num_params;
        memcpy(mf.params, params, count * sizeof(float));
        mf.num_params = count;
    }
    return mf;
}

//=============================================================================
// Fuzzy Set Operations
//=============================================================================

int fuzzy_set_create(fuzzy_set_t* set, const char* name,
                      const fuzzy_mf_t* mf, fuzzy_hedge_t hedge) {
    if (!set) {
        set_error("fuzzy_set_create: set is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_set_create: set is NULL");
        return FUZZY_ERR_NULL;
    }
    if (!name) {
        set_error("fuzzy_set_create: name is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_set_create: name is NULL");
        return FUZZY_ERR_NULL;
    }
    if (!mf) {
        set_error("fuzzy_set_create: mf is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_set_create: mf is NULL");
        return FUZZY_ERR_NULL;
    }

    memset(set, 0, sizeof(fuzzy_set_t));
    strncpy(set->name, name, FUZZY_MAX_NAME_LEN - 1);
    set->name[FUZZY_MAX_NAME_LEN - 1] = '\0';
    set->mf = *mf;
    set->hedge = hedge;
    set->alpha_cut = 0.0f;
    return FUZZY_ERR_OK;
}

float fuzzy_set_evaluate(const fuzzy_set_t* set, float x) {
    if (!set) return 0.0f;
    float mu = fuzzy_mf_evaluate(&set->mf, x);
    mu = fuzzy_apply_hedge(mu, set->hedge);
    if (mu < set->alpha_cut) return 0.0f;
    return mu;
}

//=============================================================================
// Linguistic Variable Operations
//=============================================================================

int fuzzy_variable_create(fuzzy_variable_t* var, const char* name,
                           float universe_min, float universe_max) {
    if (!var) {
        set_error("fuzzy_variable_create: var is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_variable_create: var is NULL");
        return FUZZY_ERR_NULL;
    }
    if (!name) {
        set_error("fuzzy_variable_create: name is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_variable_create: name is NULL");
        return FUZZY_ERR_NULL;
    }
    if (universe_min >= universe_max) {
        set_error("fuzzy_variable_create: invalid universe range [%f, %f]",
                  (double)universe_min, (double)universe_max);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "fuzzy_variable_create: invalid universe range [%f, %f]",
                  (double)universe_min, (double)universe_max);
        return FUZZY_ERR_UNIVERSE_RANGE;
    }

    memset(var, 0, sizeof(fuzzy_variable_t));
    strncpy(var->name, name, FUZZY_MAX_NAME_LEN - 1);
    var->name[FUZZY_MAX_NAME_LEN - 1] = '\0';
    var->universe_min = universe_min;
    var->universe_max = universe_max;
    var->num_terms = 0;
    return FUZZY_ERR_OK;
}

int fuzzy_variable_add_term(fuzzy_variable_t* var, const fuzzy_set_t* term) {
    if (!var || !term) {
        set_error("fuzzy_variable_add_term: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_variable_add_term: NULL argument");
        return FUZZY_ERR_NULL;
    }
    if (var->num_terms >= FUZZY_MAX_TERMS) {
        set_error("fuzzy_variable_add_term: max terms (%u) reached", FUZZY_MAX_TERMS);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_OUT_OF_RANGE, "fuzzy_variable_add_term: max terms (%u) reached", FUZZY_MAX_TERMS);
        return FUZZY_ERR_MAX_TERMS;
    }

    var->terms[var->num_terms] = *term;
    var->num_terms++;
    return FUZZY_ERR_OK;
}

int fuzzy_variable_fuzzify(const fuzzy_variable_t* var, float crisp_input,
                            fuzzy_value_t* out_value) {
    if (!var || !out_value) {
        set_error("fuzzy_variable_fuzzify: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_variable_fuzzify: NULL argument");
        return FUZZY_ERR_NULL;
    }

    memset(out_value, 0, sizeof(fuzzy_value_t));
    out_value->num_terms = var->num_terms;
    out_value->dominant_term = 0;
    out_value->dominant_degree = 0.0f;

    for (uint32_t i = 0; i < var->num_terms; i++) {
        float mu = fuzzy_set_evaluate(&var->terms[i], crisp_input);
        out_value->memberships[i] = mu;
        if (mu > out_value->dominant_degree) {
            out_value->dominant_degree = mu;
            out_value->dominant_term = i;
        }
    }

    out_value->entropy = fuzzy_entropy(out_value->memberships, out_value->num_terms);
    return FUZZY_ERR_OK;
}

float fuzzy_variable_centroid(const fuzzy_variable_t* var, const fuzzy_value_t* value) {
    if (!var || !value || value->num_terms == 0) return 0.0f;

    uint32_t resolution = FUZZY_RESOLUTION;
    float x_min = var->universe_min;
    float x_max = var->universe_max;
    float dx = (x_max - x_min) / (float)(resolution - 1);

    float num = 0.0f, den = 0.0f;
    for (uint32_t i = 0; i < resolution; i++) {
        float x = x_min + (float)i * dx;

        /* Aggregate: max of (term_membership * fuzzified_value) for each term */
        float agg = 0.0f;
        for (uint32_t t = 0; t < value->num_terms && t < var->num_terms; t++) {
            float term_mu = fuzzy_set_evaluate(&var->terms[t], x);
            float clipped = (term_mu < value->memberships[t]) ? term_mu : value->memberships[t];
            if (clipped > agg) agg = clipped;
        }

        num += x * agg;
        den += agg;
    }

    if (den < FUZZY_PRECISION) return (x_min + x_max) / 2.0f;
    return num / den;
}

//=============================================================================
// Discrete Set Operations
//=============================================================================

int fuzzy_discrete_set_create(fuzzy_discrete_set_t* set, uint32_t resolution,
                               float x_min, float x_max) {
    if (!set) {
        set_error("fuzzy_discrete_set_create: NULL set pointer");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_discrete_set_create: NULL set pointer");
        return FUZZY_ERR_NULL;
    }
    if (resolution == 0) {
        set_error("fuzzy_discrete_set_create: resolution is 0");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "fuzzy_discrete_set_create: resolution is 0");
        return FUZZY_ERR_RESOLUTION;
    }

    set->values = (float*)nimcp_calloc(resolution, sizeof(float));
    if (!set->values) {
        set_error("fuzzy_discrete_set_create: allocation failed");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "fuzzy_discrete_set_create: allocation failed");
        return FUZZY_ERR_ALLOC;
    }
    set->resolution = resolution;
    set->x_min = x_min;
    set->x_max = x_max;
    return FUZZY_ERR_OK;
}

void fuzzy_discrete_set_free(fuzzy_discrete_set_t* set) {
    if (!set) return;
    if (set->values) {
        nimcp_free(set->values);
        set->values = NULL;
    }
    set->resolution = 0;
}

int fuzzy_discrete_set_union(const fuzzy_discrete_set_t* a, const fuzzy_discrete_set_t* b,
                              fuzzy_discrete_set_t* out) {
    if (!a || !b || !out) return FUZZY_ERR_NULL;
    if (a->resolution != b->resolution) return FUZZY_ERR_DIMENSION_MISMATCH;
    if (!a->values || !b->values) return FUZZY_ERR_NULL;

    /* Initialize and allocate output */
    memset(out, 0, sizeof(fuzzy_discrete_set_t));
    int rc = fuzzy_discrete_set_create(out, a->resolution, a->x_min, a->x_max);
    if (rc != FUZZY_ERR_OK) return rc;

    for (uint32_t i = 0; i < a->resolution; i++) {
        out->values[i] = (a->values[i] > b->values[i]) ? a->values[i] : b->values[i];
    }
    return FUZZY_ERR_OK;
}

int fuzzy_discrete_set_intersection(const fuzzy_discrete_set_t* a, const fuzzy_discrete_set_t* b,
                                     fuzzy_discrete_set_t* out) {
    if (!a || !b || !out) return FUZZY_ERR_NULL;
    if (a->resolution != b->resolution) return FUZZY_ERR_DIMENSION_MISMATCH;
    if (!a->values || !b->values) return FUZZY_ERR_NULL;

    /* Initialize and allocate output */
    memset(out, 0, sizeof(fuzzy_discrete_set_t));
    int rc = fuzzy_discrete_set_create(out, a->resolution, a->x_min, a->x_max);
    if (rc != FUZZY_ERR_OK) return rc;

    for (uint32_t i = 0; i < a->resolution; i++) {
        out->values[i] = (a->values[i] < b->values[i]) ? a->values[i] : b->values[i];
    }
    return FUZZY_ERR_OK;
}

int fuzzy_mf_discretize(const fuzzy_mf_t* mf, float x_min, float x_max,
                         uint32_t resolution, fuzzy_discrete_set_t* out) {
    if (!mf || !out) {
        set_error("fuzzy_mf_discretize: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_mf_discretize: NULL argument");
        return FUZZY_ERR_NULL;
    }
    if (resolution == 0) {
        set_error("fuzzy_mf_discretize: resolution is 0");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "fuzzy_mf_discretize: resolution is 0");
        return FUZZY_ERR_RESOLUTION;
    }

    /* Initialize and allocate output */
    memset(out, 0, sizeof(fuzzy_discrete_set_t));
    {
        int rc = fuzzy_discrete_set_create(out, resolution, x_min, x_max);
        if (rc != FUZZY_ERR_OK) return rc;
    }

    float dx = (x_max - x_min) / (float)(resolution - 1);
    for (uint32_t i = 0; i < resolution; i++) {
        float x = x_min + (float)i * dx;
        out->values[i] = fuzzy_mf_evaluate(mf, x);
    }
    out->x_min = x_min;
    out->x_max = x_max;
    out->resolution = resolution;
    return FUZZY_ERR_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

float fuzzy_entropy(const float* memberships, uint32_t count) {
    if (!memberships || count == 0) return 0.0f;

    /* Normalize to form a probability distribution */
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        sum += memberships[i];
    }
    if (sum < FUZZY_PRECISION) return 0.0f;

    float entropy = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float p = memberships[i] / sum;
        if (p > FUZZY_PRECISION) {
            entropy -= p * log2f(p);
        }
    }
    return entropy;
}

float fuzzy_cardinality(const float* memberships, uint32_t count) {
    if (!memberships || count == 0) return 0.0f;
    float card = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        card += memberships[i];
    }
    return card;
}

//=============================================================================
// Modulation
//=============================================================================

int fuzzy_types_set_inflammation(fuzzy_types_engine_t* engine, float level) {
    if (!engine) return FUZZY_ERR_NULL;
    engine->inflammation_level = nimcp_clampf(level, 0.0f, 1.0f);
    return FUZZY_ERR_OK;
}

int fuzzy_types_set_fatigue(fuzzy_types_engine_t* engine, float level) {
    if (!engine) return FUZZY_ERR_NULL;
    engine->fatigue_level = nimcp_clampf(level, 0.0f, 1.0f);
    return FUZZY_ERR_OK;
}

//=============================================================================
// Statistics
//=============================================================================

int fuzzy_types_get_stats(const fuzzy_types_engine_t* engine, fuzzy_types_stats_t* stats) {
    if (!engine || !stats) return FUZZY_ERR_NULL;
    *stats = engine->stats;
    return FUZZY_ERR_OK;
}

void fuzzy_types_reset_stats(fuzzy_types_engine_t* engine) {
    if (!engine) return;
    memset(&engine->stats, 0, sizeof(engine->stats));
}

const char* fuzzy_types_get_last_error(void) {
    return tls_fuzzy_types_error;
}
