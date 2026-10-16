/**
 * @file nimcp_mathematical_intuition.c
 * @brief Pattern recognition and mathematical intuition implementation
 *
 * Implements sequence pattern detection, symmetry analysis, and
 * mathematical analogy solving for the parietal lobe module.
 */

#include "cognitive/parietal/nimcp_mathematical_intuition.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(mathematical_intuition, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define PI NIMCP_PI_F
#include "constants/nimcp_constants.h"
#define EPSILON NIMCP_EPSILON_NUMERICAL

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Internal mathematical intuition state
 */
struct math_intuition {
    /* Configuration */
    math_intuition_config_t config;

    /* Modulation state */
    float inflammation_level;
    float fatigue_level;

    /* Statistics */
    uint64_t patterns_detected;
    uint64_t symmetries_detected;
    uint64_t analogies_solved;
    uint64_t geometric_analyses;
    double total_pattern_confidence;
    double total_symmetry_confidence;

    /* Thread safety */
    nimcp_mutex_t* lock;
};

/* Thread-local error message */
static _Thread_local char g_math_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static void set_math_error(const char* msg) {
    strncpy(g_math_error, msg, sizeof(g_math_error) - 1);
    g_math_error[sizeof(g_math_error) - 1] = '\0';
}

static float fabsf_safe(float v) {
    return fabsf(v) < EPSILON ? EPSILON : fabsf(v);
}

/**
 * @brief Compute finite differences of sequence
 */
static void compute_differences(const float* seq, uint32_t len, float* diffs) {
    for (uint32_t i = 0; i < len - 1; i++) {
        diffs[i] = seq[i + 1] - seq[i];
    }
}

/**
 * @brief Compute ratios of consecutive elements
 */
static bool compute_ratios(const float* seq, uint32_t len, float* ratios) {
    for (uint32_t i = 0; i < len - 1; i++) {
        if (fabsf(seq[i]) < EPSILON) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compute_ratios: validation failed");
            return false;  /* Can't compute ratio with zero */
        }
        ratios[i] = seq[i + 1] / seq[i];
    }
    return true;
}

/**
 * @brief Check if array is approximately constant
 */
static bool is_constant(const float* arr, uint32_t len, float tolerance, float* value) {
    if (len == 0) {
        return false;
    }

    float mean = 0.0f;
    for (uint32_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)len);
        }

        mean += arr[i];
    }
    mean /= (float)len;

    for (uint32_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)len);
        }

        if (fabsf(arr[i] - mean) > tolerance) {
            return false;
        }
    }

    if (value) *value = mean;
    return true;
}

/**
 * @brief Compute mean squared error
 */
static float compute_mse(const float* actual, const float* predicted, uint32_t len) {
    float mse = 0.0f;
    for (uint32_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)len);
        }

        float diff = actual[i] - predicted[i];
        mse += diff * diff;
    }
    return mse / (float)len;
}

/**
 * @brief Fit polynomial using least squares (normal equations)
 */
static float fit_polynomial(const float* x, const float* y, uint32_t n,
                            uint8_t degree, float* coeffs) {
    if (degree > MATH_MAX_POLYNOMIAL_DEGREE || n <= degree) {
        return INFINITY;
    }

    uint8_t m = degree + 1;

    /* Build normal equations: (X^T X) * coeffs = X^T * y */
    /* Using simple approach for low-degree polynomials */

    float XtX[36] = {0};  /* Max 6x6 for degree 5 */
    float Xty[6] = {0};

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)n);
        }

        float xi_powers[6];
        xi_powers[0] = 1.0f;
        for (uint8_t j = 1; j < m; j++) {
            xi_powers[j] = xi_powers[j - 1] * x[i];
        }

        for (uint8_t row = 0; row < m; row++) {
            for (uint8_t col = 0; col < m; col++) {
                XtX[row * m + col] += xi_powers[row] * xi_powers[col];
            }
            Xty[row] += xi_powers[row] * y[i];
        }
    }

    /* Solve using Gaussian elimination (simple version) */
    float aug[42];  /* 6x7 augmented matrix */
    for (uint8_t i = 0; i < m; i++) {
        for (uint8_t j = 0; j < m; j++) {
            aug[i * (m + 1) + j] = XtX[i * m + j];
        }
        aug[i * (m + 1) + m] = Xty[i];
    }

    /* Forward elimination */
    for (uint8_t col = 0; col < m; col++) {
        /* Find pivot */
        uint8_t max_row = col;
        for (uint8_t row = col + 1; row < m; row++) {
            if (fabsf(aug[row * (m + 1) + col]) > fabsf(aug[max_row * (m + 1) + col])) {
                max_row = row;
            }
        }

        /* Swap rows */
        for (uint8_t j = 0; j <= m; j++) {
            float tmp = aug[col * (m + 1) + j];
            aug[col * (m + 1) + j] = aug[max_row * (m + 1) + j];
            aug[max_row * (m + 1) + j] = tmp;
        }

        if (fabsf(aug[col * (m + 1) + col]) < EPSILON) {
            return INFINITY;  /* Singular matrix */
        }

        /* Eliminate */
        for (uint8_t row = col + 1; row < m; row++) {
            float factor = aug[row * (m + 1) + col] / aug[col * (m + 1) + col];
            for (uint8_t j = col; j <= m; j++) {
                aug[row * (m + 1) + j] -= factor * aug[col * (m + 1) + j];
            }
        }
    }

    /* Back substitution */
    for (int8_t i = m - 1; i >= 0; i--) {
        coeffs[i] = aug[i * (m + 1) + m];
        for (uint8_t j = i + 1; j < m; j++) {
            coeffs[i] -= aug[i * (m + 1) + j] * coeffs[j];
        }
        coeffs[i] /= aug[i * (m + 1) + i];
    }

    /* Compute MSE */
    float mse = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)n);
        }

        float pred = 0.0f;
        float xi_pow = 1.0f;
        for (uint8_t j = 0; j < m; j++) {
            pred += coeffs[j] * xi_pow;
            xi_pow *= x[i];
        }
        float err = y[i] - pred;
        mse += err * err;
    }

    return mse / (float)n;
}

/**
 * @brief Evaluate polynomial at x
 */
static float eval_polynomial(const float* coeffs, uint8_t degree, float x) {
    float result = coeffs[degree];
    for (int8_t i = degree - 1; i >= 0; i--) {
        result = result * x + coeffs[i];
    }
    return result;
}

/**
 * @brief Compute centroid of point set
 */
static vec3_t compute_centroid(const vec3_t* points, uint32_t n) {
    vec3_t c = {0, 0, 0};
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)n);
        }

        c.x += points[i].x;
        c.y += points[i].y;
        c.z += points[i].z;
    }
    c.x /= (float)n;
    c.y /= (float)n;
    c.z /= (float)n;
    return c;
}

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

math_intuition_config_t math_intuition_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_intuition_defau", 0.0f);


    math_intuition_config_t config = {
        .pattern_confidence_threshold = 0.7f,
        .symmetry_tolerance = 0.01f,
        .max_polynomial_degree = MATH_MAX_POLYNOMIAL_DEGREE,
        .enable_oscillation_detection = true,
        .enable_bio_async = false,
        .inflammation_sensitivity = 0.5f,
        .fatigue_sensitivity = 0.5f
    };
    return config;
}

bool math_intuition_validate_config(const math_intuition_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "math_intuition_validate_config: config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_intuition_valid", 0.0f);


    if (config->pattern_confidence_threshold < 0.0f ||
        config->pattern_confidence_threshold > 1.0f) {
        set_math_error("Pattern confidence threshold must be in [0, 1]");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "math_intuition_validate_config: config is NULL");
        return false;
    }

    if (config->symmetry_tolerance <= 0.0f || config->symmetry_tolerance > 1.0f) {
        set_math_error("Symmetry tolerance must be in (0, 1]");
        return false;
    }

    if (config->max_polynomial_degree == 0 ||
        config->max_polynomial_degree > MATH_MAX_POLYNOMIAL_DEGREE) {
        set_math_error("Invalid polynomial degree");
        return false;
    }

    return true;
}

math_intuition_t* math_intuition_create(void) {
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_intuition_creat", 0.0f);


    return math_intuition_create_custom(NULL);
}

math_intuition_t* math_intuition_create_custom(const math_intuition_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_intuition_creat", 0.0f);


    math_intuition_config_t cfg;

    if (config) {
        if (!math_intuition_validate_config(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "math_intuition_create_custom: math_intuition_validate_config is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = math_intuition_default_config();
    }

    math_intuition_t* mi = nimcp_calloc(1, sizeof(math_intuition_t));
    if (!mi) {
        set_math_error("Failed to allocate math intuition");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate mi");

        return NULL;
    }

    mi->config = cfg;

    /* Create mutex */
    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    mi->lock = nimcp_mutex_create(&attr);
    if (!mi->lock) {
        set_math_error("Failed to create mutex");
        nimcp_free(mi);
        mi = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "math_intuition_create_custom: mi->lock is NULL");
        return NULL;
    }

    return mi;
}

void math_intuition_destroy(math_intuition_t* mi) {
    if (!mi) return;

    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_intuition_destr", 0.0f);


    if (mi->lock) {
        nimcp_mutex_destroy(mi->lock);
    }

    nimcp_free(mi);
    mi = NULL;
}

/* ============================================================================
 * PATTERN DETECTION API
 * ============================================================================ */

detected_pattern_t math_detect_pattern(
    math_intuition_t* mi,
    const float* sequence,
    uint32_t length
) {
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_detect_pattern", 0.0f);


    detected_pattern_t result = {0};
    result.type = PATTERN_UNKNOWN;
    result.sequence_length = length;

    if (!mi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "math_detect_pattern: mi is NULL");
        result.confidence = -1.0f;
        return result;
    }
    if (!sequence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "math_detect_pattern: sequence is NULL");
        result.confidence = -1.0f;
        return result;
    }
    if (length < 2) {
        return result;
    }

    nimcp_mutex_lock(mi->lock);

    /* Allocate working arrays */
    float* diffs = nimcp_calloc((length - 1), sizeof(float));
    float* ratios = nimcp_calloc((length - 1), sizeof(float));
    float* x_vals = nimcp_calloc(length, sizeof(float));

    if (!diffs || !ratios || !x_vals) {
        nimcp_free(diffs);
        diffs = NULL;
        nimcp_free(ratios);
        ratios = NULL;
        nimcp_free(x_vals);
        x_vals = NULL;
        nimcp_mutex_unlock(mi->lock);
        return result;
    }

    for (uint32_t i = 0; i < length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && length > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)length);
        }

        x_vals[i] = (float)i;
    }

    float tolerance = 0.01f * (1.0f + mi->inflammation_level * mi->config.inflammation_sensitivity);
    float best_mse = INFINITY;

    /* Check for constant sequence */
    float const_val = 0.0f;
    if (is_constant(sequence, length, tolerance, &const_val)) {
        result.type = PATTERN_CONSTANT;
        result.params.constant.value = const_val;
        result.fit_error = 0.0f;
        result.confidence = 1.0f;
        goto done;
    }

    /* Compute differences and ratios */
    compute_differences(sequence, length, diffs);
    bool has_ratios = compute_ratios(sequence, length, ratios);

    /* Check for arithmetic progression */
    float common_diff = 0.0f;
    if (is_constant(diffs, length - 1, tolerance, &common_diff)) {
        result.type = PATTERN_ARITHMETIC;
        result.params.arithmetic.first_term = sequence[0];
        result.params.arithmetic.difference = common_diff;

        /* Compute fit error */
        float mse = 0.0f;
        for (uint32_t i = 0; i < length; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && length > 256) {
                mathematical_intuition_heartbeat("mathematical_loop",
                                 (float)(i + 1) / (float)length);
            }

            float pred = sequence[0] + i * common_diff;
            float err = sequence[i] - pred;
            mse += err * err;
        }
        result.fit_error = mse / (float)length;
        result.confidence = 0.95f - result.fit_error * 10.0f;
        if (result.confidence < 0.7f) result.confidence = 0.7f;
        goto done;
    }

    /* Check for geometric progression */
    if (has_ratios) {
        float common_ratio = 0.0f;
        if (is_constant(ratios, length - 1, tolerance, &common_ratio) &&
            fabsf(common_ratio) > EPSILON) {
            result.type = PATTERN_GEOMETRIC;
            result.params.geometric.first_term = sequence[0];
            result.params.geometric.ratio = common_ratio;

            /* Compute fit error */
            float mse = 0.0f;
            float pred = sequence[0];
            for (uint32_t i = 0; i < length; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && length > 256) {
                    mathematical_intuition_heartbeat("mathematical_loop",
                                     (float)(i + 1) / (float)length);
                }

                float err = sequence[i] - pred;
                mse += err * err;
                pred *= common_ratio;
            }
            result.fit_error = mse / (float)length;
            result.confidence = 0.95f - result.fit_error * 10.0f;
            if (result.confidence < 0.7f) result.confidence = 0.7f;
            goto done;
        }
    }

    /* Check for Fibonacci-like (each term = sum of previous two) */
    if (length >= 3) {
        bool is_fib = true;
        for (uint32_t i = 2; i < length; i++) {
            float expected = sequence[i - 1] + sequence[i - 2];
            if (fabsf(sequence[i] - expected) > tolerance * fabsf_safe(expected)) {
                is_fib = false;
                break;
            }
        }
        if (is_fib) {
            result.type = PATTERN_FIBONACCI;
            result.params.fibonacci.a0 = sequence[0];
            result.params.fibonacci.a1 = sequence[1];
            result.params.fibonacci.alpha = 1.0f;
            result.params.fibonacci.beta = 1.0f;
            result.fit_error = 0.0f;
            result.confidence = 0.95f;
            goto done;
        }
    }

    /* Check for squares */
    if (length >= 3) {
        bool is_squares = true;
        for (uint32_t i = 0; i < length; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && length > 256) {
                mathematical_intuition_heartbeat("mathematical_loop",
                                 (float)(i + 1) / (float)length);
            }

            float expected = (float)((i + 1) * (i + 1));
            if (fabsf(sequence[i] - expected) > tolerance * expected) {
                is_squares = false;
                break;
            }
        }
        if (is_squares) {
            result.type = PATTERN_SQUARE;
            result.fit_error = 0.0f;
            result.confidence = 0.95f;
            goto done;
        }
    }

    /* Check for triangular numbers */
    if (length >= 3) {
        bool is_triangular = true;
        for (uint32_t i = 0; i < length; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && length > 256) {
                mathematical_intuition_heartbeat("mathematical_loop",
                                 (float)(i + 1) / (float)length);
            }

            float expected = (float)((i + 1) * (i + 2)) / 2.0f;
            if (fabsf(sequence[i] - expected) > tolerance * fabsf_safe(expected)) {
                is_triangular = false;
                break;
            }
        }
        if (is_triangular) {
            result.type = PATTERN_TRIANGULAR;
            result.fit_error = 0.0f;
            result.confidence = 0.95f;
            goto done;
        }
    }

    /* Try polynomial fits of increasing degree */
    for (uint8_t deg = 2; deg <= mi->config.max_polynomial_degree && deg < length; deg++) {
        float coeffs[MATH_MAX_POLYNOMIAL_DEGREE + 1] = {0};
        float mse = fit_polynomial(x_vals, sequence, length, deg, coeffs);

        if (mse < best_mse && mse < tolerance * tolerance * 100.0f) {
            best_mse = mse;
            result.type = PATTERN_POLYNOMIAL;
            result.params.polynomial.degree = deg;
            memcpy(result.params.polynomial.coefficients, coeffs, sizeof(coeffs));
            result.fit_error = mse;
            result.confidence = 0.9f - mse * 100.0f;
            if (result.confidence < 0.5f) result.confidence = 0.5f;
        }
    }

done:
    /* Update statistics */
    if (result.type != PATTERN_UNKNOWN) {
        mi->patterns_detected++;
        mi->total_pattern_confidence += result.confidence;
    }

    nimcp_free(diffs);
    diffs = NULL;
    nimcp_free(ratios);
    ratios = NULL;
    nimcp_free(x_vals);
    x_vals = NULL;

    nimcp_mutex_unlock(mi->lock);

    return result;
}

float math_extrapolate(
    math_intuition_t* mi,
    const detected_pattern_t* pattern,
    uint32_t index
) {
    if (!mi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "math_extrapolate: mi is NULL");
        return 0.0f;
    }
    if (!pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "math_extrapolate: pattern is NULL");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_extrapolate", 0.0f);


    switch (pattern->type) {
        case PATTERN_CONSTANT:
            return pattern->params.constant.value;

        case PATTERN_ARITHMETIC:
            return pattern->params.arithmetic.first_term +
                   (float)index * pattern->params.arithmetic.difference;

        case PATTERN_GEOMETRIC:
            return pattern->params.geometric.first_term *
                   powf(pattern->params.geometric.ratio, (float)index);

        case PATTERN_FIBONACCI: {
            if (index == 0) return pattern->params.fibonacci.a0;
            if (index == 1) return pattern->params.fibonacci.a1;
            float a = pattern->params.fibonacci.a0;
            float b = pattern->params.fibonacci.a1;
            for (uint32_t i = 2; i <= index; i++) {
                float c = a + b;
                a = b;
                b = c;
            }
            return b;
        }

        case PATTERN_POLYNOMIAL:
            return eval_polynomial(pattern->params.polynomial.coefficients,
                                   pattern->params.polynomial.degree,
                                   (float)index);

        case PATTERN_SQUARE:
            return (float)((index + 1) * (index + 1));

        case PATTERN_TRIANGULAR:
            return (float)((index + 1) * (index + 2)) / 2.0f;

        default:
            return 0.0f;
    }
}

uint32_t math_predict_sequence(
    math_intuition_t* mi,
    const detected_pattern_t* pattern,
    float* predictions,
    uint32_t num_predictions
) {
    if (!mi || !pattern || !predictions || num_predictions == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_predict_sequenc", 0.0f);


    for (uint32_t i = 0; i < num_predictions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_predictions > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)num_predictions);
        }

        predictions[i] = math_extrapolate(mi, pattern, pattern->sequence_length + i);
    }

    return num_predictions;
}

float math_check_pattern_fit(
    math_intuition_t* mi,
    const detected_pattern_t* pattern,
    float value,
    uint32_t index
) {
    if (!mi || !pattern) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_check_pattern_f", 0.0f);


    float expected = math_extrapolate(mi, pattern, index);
    float error = fabsf(value - expected) / fabsf_safe(expected);

    return 1.0f / (1.0f + error);
}

const char* math_pattern_type_name(pattern_type_t type) {
    switch (type) {
        case PATTERN_UNKNOWN: return "Unknown";
        case PATTERN_CONSTANT: return "Constant";
        case PATTERN_ARITHMETIC: return "Arithmetic";
        case PATTERN_GEOMETRIC: return "Geometric";
        case PATTERN_FIBONACCI: return "Fibonacci";
        case PATTERN_POLYNOMIAL: return "Polynomial";
        case PATTERN_EXPONENTIAL: return "Exponential";
        case PATTERN_LOGARITHMIC: return "Logarithmic";
        case PATTERN_OSCILLATORY: return "Oscillatory";
        case PATTERN_PRIME: return "Prime";
        case PATTERN_SQUARE: return "Square";
        case PATTERN_TRIANGULAR: return "Triangular";
        case PATTERN_CUSTOM: return "Custom";
        default: return "Invalid";
    }
}

/* ============================================================================
 * GEOMETRIC REASONING API
 * ============================================================================ */

geometric_result_t math_analyze_lines(
    math_intuition_t* mi,
    vec3_t line1_start, vec3_t line1_end,
    vec3_t line2_start, vec3_t line2_end
) {
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_analyze_lines", 0.0f);


    geometric_result_t result = {0};

    if (!mi) return result;

    nimcp_mutex_lock(mi->lock);

    /* Compute direction vectors */
    vec3_t d1 = vec3_sub(line1_end, line1_start);
    vec3_t d2 = vec3_sub(line2_end, line2_start);

    /* Normalize directions */
    d1 = vec3_normalize(d1);
    d2 = vec3_normalize(d2);

    /* Compute dot product (for parallel/perpendicular check) */
    float dot = vec3_dot(d1, d2);

    /* Check for parallel */
    if (fabsf(fabsf(dot) - 1.0f) < mi->config.symmetry_tolerance) {
        result.relation = GEOM_RELATION_PARALLEL;
        result.confidence = 1.0f - fabsf(fabsf(dot) - 1.0f) * 10.0f;
        result.angle = 0.0f;
    }
    /* Check for perpendicular */
    else if (fabsf(dot) < mi->config.symmetry_tolerance) {
        result.relation = GEOM_RELATION_PERPENDICULAR;
        result.confidence = 1.0f - fabsf(dot) * 10.0f;
        result.angle = 90.0f;
    }
    /* Otherwise intersecting */
    else {
        result.relation = GEOM_RELATION_INTERSECTING;
        result.angle = acosf(nimcp_clamp01(fabsf(dot))) * 180.0f / PI;
        result.confidence = 0.9f;

        /* Compute intersection point (2D approximation) */
        /* Using parametric form: P = line1_start + t * d1 */
        vec3_t w = vec3_sub(line1_start, line2_start);
        float denom = d1.x * d2.y - d1.y * d2.x;
        if (fabsf(denom) > EPSILON) {
            float t = (d2.x * w.y - d2.y * w.x) / denom;
            result.intersection_point.x = line1_start.x + t * d1.x;
            result.intersection_point.y = line1_start.y + t * d1.y;
            result.intersection_point.z = 0.0f;
        }
    }

    mi->geometric_analyses++;

    nimcp_mutex_unlock(mi->lock);

    return result;
}

geometric_result_t math_check_congruent(
    math_intuition_t* mi,
    const vec3_t* shape1, uint32_t num_vertices1,
    const vec3_t* shape2, uint32_t num_vertices2
) {
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_check_congruent", 0.0f);


    geometric_result_t result = {0};

    if (!mi || !shape1 || !shape2) return result;

    nimcp_mutex_lock(mi->lock);

    /* Shapes must have same number of vertices for congruence */
    if (num_vertices1 != num_vertices2) {
        result.relation = GEOM_RELATION_NONE;
        result.confidence = 1.0f;
        mi->geometric_analyses++;
        nimcp_mutex_unlock(mi->lock);
        return result;
    }

    /* Compute edge lengths for both shapes */
    float* edges1 = nimcp_calloc(num_vertices1, sizeof(float));
    float* edges2 = nimcp_calloc(num_vertices2, sizeof(float));

    if (!edges1 || !edges2) {
        nimcp_free(edges1);
        edges1 = NULL;
        nimcp_free(edges2);
        edges2 = NULL;
        nimcp_mutex_unlock(mi->lock);
        return result;
    }

    for (uint32_t i = 0; i < num_vertices1; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_vertices1 > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)num_vertices1);
        }

        uint32_t next = (i + 1) % num_vertices1;
        edges1[i] = vec3_distance(shape1[i], shape1[next]);
        edges2[i] = vec3_distance(shape2[i], shape2[next]);
    }

    /* Sort edges and compare */
    /* Simple bubble sort for small vertex counts */
    for (uint32_t i = 0; i < num_vertices1 - 1; i++) {
        for (uint32_t j = 0; j < num_vertices1 - i - 1; j++) {
            if (edges1[j] > edges1[j + 1]) {
                float tmp = edges1[j];
                edges1[j] = edges1[j + 1];
                edges1[j + 1] = tmp;
            }
            if (edges2[j] > edges2[j + 1]) {
                float tmp = edges2[j];
                edges2[j] = edges2[j + 1];
                edges2[j + 1] = tmp;
            }
        }
    }

    /* Compare sorted edges */
    float max_diff = 0.0f;
    for (uint32_t i = 0; i < num_vertices1; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_vertices1 > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)num_vertices1);
        }

        float diff = fabsf(edges1[i] - edges2[i]);
        if (diff > max_diff) max_diff = diff;
    }

    float avg_edge = 0.0f;
    for (uint32_t i = 0; i < num_vertices1; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_vertices1 > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)num_vertices1);
        }

        avg_edge += edges1[i];
    }
    avg_edge /= (float)num_vertices1;

    float relative_diff = max_diff / fabsf_safe(avg_edge);

    if (relative_diff < mi->config.symmetry_tolerance) {
        result.relation = GEOM_RELATION_CONGRUENT;
        result.confidence = 1.0f - relative_diff / mi->config.symmetry_tolerance;
        result.scale_factor = 1.0f;
    }

    nimcp_free(edges1);
    edges1 = NULL;
    nimcp_free(edges2);
    edges2 = NULL;

    mi->geometric_analyses++;

    nimcp_mutex_unlock(mi->lock);

    return result;
}

geometric_result_t math_check_similar(
    math_intuition_t* mi,
    const vec3_t* shape1, uint32_t num_vertices1,
    const vec3_t* shape2, uint32_t num_vertices2
) {
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_check_similar", 0.0f);


    geometric_result_t result = {0};

    if (!mi || !shape1 || !shape2 || num_vertices1 != num_vertices2) {
        return result;
    }

    nimcp_mutex_lock(mi->lock);

    /* Compute perimeters */
    float perim1 = 0.0f, perim2 = 0.0f;
    for (uint32_t i = 0; i < num_vertices1; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_vertices1 > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)num_vertices1);
        }

        uint32_t next = (i + 1) % num_vertices1;
        perim1 += vec3_distance(shape1[i], shape1[next]);
        perim2 += vec3_distance(shape2[i], shape2[next]);
    }

    float scale = perim1 / fabsf_safe(perim2);

    /* Check if shapes are similar by comparing scaled edge ratios */
    float max_ratio_diff = 0.0f;
    for (uint32_t i = 0; i < num_vertices1; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_vertices1 > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)num_vertices1);
        }

        uint32_t next = (i + 1) % num_vertices1;
        float e1 = vec3_distance(shape1[i], shape1[next]);
        float e2 = vec3_distance(shape2[i], shape2[next]) * scale;
        float diff = fabsf(e1 - e2) / fabsf_safe(e1);
        if (diff > max_ratio_diff) max_ratio_diff = diff;
    }

    if (max_ratio_diff < mi->config.symmetry_tolerance * 5.0f) {
        result.relation = GEOM_RELATION_SIMILAR;
        result.scale_factor = scale;
        result.confidence = 1.0f - max_ratio_diff / (mi->config.symmetry_tolerance * 5.0f);
    }

    mi->geometric_analyses++;

    nimcp_mutex_unlock(mi->lock);

    return result;
}

/* ============================================================================
 * SYMMETRY DETECTION API
 * ============================================================================ */

symmetry_result_t math_detect_symmetry(
    math_intuition_t* mi,
    const vec3_t* points,
    uint32_t num_points
) {
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_detect_symmetry", 0.0f);


    symmetry_result_t result = {0};

    if (!mi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "math_detect_symmetry: mi is NULL");
        result.confidence = -1.0f;
        return result;
    }
    if (!points) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "math_detect_symmetry: points is NULL");
        result.confidence = -1.0f;
        return result;
    }
    if (num_points < 2) {
        return result;
    }

    nimcp_mutex_lock(mi->lock);

    vec3_t centroid = compute_centroid(points, num_points);

    /* Check for point symmetry */
    float point_sym_error = 0.0f;
    uint32_t point_sym_matches = 0;

    for (uint32_t i = 0; i < num_points; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_points > 256) {
            mathematical_intuition_heartbeat("mathematical_loop",
                             (float)(i + 1) / (float)num_points);
        }

        /* Find antipodal point */
        vec3_t antipode = vec3_sub(vec3_scale(centroid, 2.0f), points[i]);

        /* Check if antipode exists in point set */
        float min_dist = INFINITY;
        for (uint32_t j = 0; j < num_points; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_points > 256) {
                mathematical_intuition_heartbeat("mathematical_loop",
                                 (float)(j + 1) / (float)num_points);
            }

            float dist = vec3_distance(antipode, points[j]);
            if (dist < min_dist) min_dist = dist;
        }

        if (min_dist < mi->config.symmetry_tolerance * 10.0f) {
            point_sym_matches++;
            point_sym_error += min_dist;
        }
    }

    if (point_sym_matches > num_points / 2) {
        result.has_point_symmetry = true;
        result.symmetry_center = centroid;
        result.confidence = (float)point_sym_matches / (float)num_points;
    }

    /* Check for reflection symmetry (2D - using XY plane) */
    float best_reflection_conf = 0.0f;
    vec3_t best_axis_dir = {1, 0, 0};

    /* Try several axis orientations */
    for (int angle_deg = 0; angle_deg < 180; angle_deg += 15) {
        float angle = (float)angle_deg * PI / 180.0f;
        vec3_t axis_dir = {cosf(angle), sinf(angle), 0.0f};

        /* For each point, check if reflection exists */
        uint32_t matches = 0;
        for (uint32_t i = 0; i < num_points; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_points > 256) {
                mathematical_intuition_heartbeat("mathematical_loop",
                                 (float)(i + 1) / (float)num_points);
            }

            vec3_t rel = vec3_sub(points[i], centroid);

            /* Project onto axis and compute reflection */
            float proj = vec3_dot(rel, axis_dir);
            vec3_t reflected = vec3_sub(rel, vec3_scale(axis_dir, 2.0f * proj));
            reflected = vec3_add(reflected, centroid);

            /* Look for reflected point */
            for (uint32_t j = 0; j < num_points; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && num_points > 256) {
                    mathematical_intuition_heartbeat("mathematical_loop",
                                     (float)(j + 1) / (float)num_points);
                }

                if (vec3_distance(reflected, points[j]) < mi->config.symmetry_tolerance * 10.0f) {
                    matches++;
                    break;
                }
            }
        }

        float conf = (float)matches / (float)num_points;
        if (conf > best_reflection_conf) {
            best_reflection_conf = conf;
            best_axis_dir = axis_dir;
        }
    }

    if (best_reflection_conf > 0.8f) {
        result.has_reflection = true;
        result.reflection_axis = best_axis_dir;
        result.reflection_point = centroid;
        result.type = SYMMETRY_REFLECTION;
        if (best_reflection_conf > result.confidence) {
            result.confidence = best_reflection_conf;
        }
    }

    /* Check for rotational symmetry */
    uint32_t best_order = 1;
    float best_rot_conf = 0.0f;

    for (uint32_t order = 2; order <= 8; order++) {
        float rot_angle = 2.0f * PI / (float)order;
        uint32_t matches = 0;

        for (uint32_t i = 0; i < num_points; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_points > 256) {
                mathematical_intuition_heartbeat("mathematical_loop",
                                 (float)(i + 1) / (float)num_points);
            }

            vec3_t rel = vec3_sub(points[i], centroid);

            /* Rotate by angle */
            float new_x = rel.x * cosf(rot_angle) - rel.y * sinf(rot_angle);
            float new_y = rel.x * sinf(rot_angle) + rel.y * cosf(rot_angle);
            vec3_t rotated = {new_x + centroid.x, new_y + centroid.y, rel.z + centroid.z};

            /* Check if rotated point exists */
            for (uint32_t j = 0; j < num_points; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && num_points > 256) {
                    mathematical_intuition_heartbeat("mathematical_loop",
                                     (float)(j + 1) / (float)num_points);
                }

                if (vec3_distance(rotated, points[j]) < mi->config.symmetry_tolerance * 10.0f) {
                    matches++;
                    break;
                }
            }
        }

        float conf = (float)matches / (float)num_points;
        if (conf > best_rot_conf && conf > 0.8f) {
            best_rot_conf = conf;
            best_order = order;
        }
    }

    if (best_order > 1 && best_rot_conf > 0.8f) {
        result.has_rotation = true;
        result.rotation_order = best_order;
        result.rotation_center = centroid;
        result.rotation_axis = vec3_create(0, 0, 1);
        if (result.type == SYMMETRY_NONE) result.type = SYMMETRY_ROTATIONAL;
        if (best_rot_conf > result.confidence) {
            result.confidence = best_rot_conf;
        }
    }

    mi->symmetries_detected++;
    mi->total_symmetry_confidence += result.confidence;

    nimcp_mutex_unlock(mi->lock);

    return result;
}

float math_check_symmetry_type(
    math_intuition_t* mi,
    const vec3_t* points,
    uint32_t num_points,
    symmetry_type_t type
) {
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_check_symmetry_", 0.0f);


    symmetry_result_t sym = math_detect_symmetry(mi, points, num_points);

    switch (type) {
        case SYMMETRY_REFLECTION:
            return sym.has_reflection ? sym.confidence : 0.0f;
        case SYMMETRY_ROTATIONAL:
            return sym.has_rotation ? sym.confidence : 0.0f;
        case SYMMETRY_POINT:
            return sym.has_point_symmetry ? sym.confidence : 0.0f;
        default:
            return 0.0f;
    }
}

float math_find_reflection_axis(
    math_intuition_t* mi,
    const vec3_t* points,
    uint32_t num_points,
    vec3_t* axis_point,
    vec3_t* axis_direction
) {
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_find_reflection", 0.0f);


    symmetry_result_t sym = math_detect_symmetry(mi, points, num_points);

    if (sym.has_reflection && axis_point && axis_direction) {
        *axis_point = sym.reflection_point;
        *axis_direction = sym.reflection_axis;
        return sym.confidence;
    }

    return 0.0f;
}

float math_find_rotation_symmetry(
    math_intuition_t* mi,
    const vec3_t* points,
    uint32_t num_points,
    vec3_t* center,
    uint32_t* order
) {
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_find_rotation_s", 0.0f);


    symmetry_result_t sym = math_detect_symmetry(mi, points, num_points);

    if (sym.has_rotation && center && order) {
        *center = sym.rotation_center;
        *order = sym.rotation_order;
        return sym.confidence;
    }

    return 0.0f;
}

/* ============================================================================
 * ANALOGY API
 * ============================================================================ */

analogy_result_t math_solve_analogy(
    math_intuition_t* mi,
    float a, float b, float c
) {
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_solve_analogy", 0.0f);


    analogy_result_t result = {0};

    if (!mi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "math_solve_analogy: mi is NULL");
        result.confidence = -1.0f;
        return result;
    }

    nimcp_mutex_lock(mi->lock);

    /* Try different relation types */
    float best_conf = 0.0f;

    /* Additive relation: b = a + d, so ? = c + d */
    float d_add = b - a;
    float ans_add = c + d_add;
    float conf_add = 0.8f;

    /* Multiplicative relation: b = a * r, so ? = c * r */
    float r_mul = (fabsf(a) > EPSILON) ? (b / a) : 0.0f;
    float ans_mul = c * r_mul;
    float conf_mul = (fabsf(a) > EPSILON) ? 0.85f : 0.0f;

    /* Power relation: b = a^p, so ? = c^p */
    float ans_pow = 0.0f;
    float conf_pow = 0.0f;
    if (a > 0.0f && b > 0.0f && c > 0.0f) {
        float p = logf(b) / logf(a);
        ans_pow = powf(c, p);
        conf_pow = 0.75f;
    }

    /* Choose best relation */
    if (conf_mul > best_conf) {
        best_conf = conf_mul;
        result.answer = ans_mul;
        result.relation_type = PATTERN_GEOMETRIC;
        result.relation_param = r_mul;
    }

    if (conf_add > best_conf) {
        best_conf = conf_add;
        result.answer = ans_add;
        result.relation_type = PATTERN_ARITHMETIC;
        result.relation_param = d_add;
    }

    if (conf_pow > best_conf) {
        best_conf = conf_pow;
        result.answer = ans_pow;
        result.relation_type = PATTERN_EXPONENTIAL;
        result.relation_param = logf(b) / logf(fabsf_safe(a));
    }

    result.confidence = best_conf * (1.0f - mi->fatigue_level * mi->config.fatigue_sensitivity * 0.2f);

    mi->analogies_solved++;

    nimcp_mutex_unlock(mi->lock);

    return result;
}

float math_check_analogy(
    math_intuition_t* mi,
    float a, float b, float c, float d
) {
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_check_analogy", 0.0f);


    analogy_result_t result = math_solve_analogy(mi, a, b, c);

    float error = fabsf(result.answer - d) / fabsf_safe(d);
    return result.confidence * (1.0f / (1.0f + error));
}

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int math_intuition_set_inflammation(math_intuition_t* mi, float level) {
    if (!mi) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mi is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_intuition_set_i", 0.0f);


    nimcp_mutex_lock(mi->lock);
    mi->inflammation_level = nimcp_clamp01(level);
    nimcp_mutex_unlock(mi->lock);

    return 0;
}

int math_intuition_set_fatigue(math_intuition_t* mi, float level) {
    if (!mi) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mi is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_intuition_set_f", 0.0f);


    nimcp_mutex_lock(mi->lock);
    mi->fatigue_level = nimcp_clamp01(level);
    nimcp_mutex_unlock(mi->lock);

    return 0;
}

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int math_intuition_get_stats(math_intuition_t* mi, math_intuition_stats_t* stats) {
    if (!mi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "math_intuition_get_stats: mi is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "math_intuition_get_stats: stats is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_intuition_get_s", 0.0f);


    nimcp_mutex_lock(((math_intuition_t*)mi)->lock);

    stats->patterns_detected = mi->patterns_detected;
    stats->symmetries_detected = mi->symmetries_detected;
    stats->analogies_solved = mi->analogies_solved;
    stats->geometric_analyses = mi->geometric_analyses;

    if (mi->patterns_detected > 0) {
        stats->avg_pattern_confidence = (float)(mi->total_pattern_confidence /
                                                 (double)mi->patterns_detected);
    } else {
        stats->avg_pattern_confidence = 0.0f;
    }

    if (mi->symmetries_detected > 0) {
        stats->avg_symmetry_confidence = (float)(mi->total_symmetry_confidence /
                                                  (double)mi->symmetries_detected);
    } else {
        stats->avg_symmetry_confidence = 0.0f;
    }

    nimcp_mutex_unlock(((math_intuition_t*)mi)->lock);

    return 0;
}

void math_intuition_reset_stats(math_intuition_t* mi) {
    if (!mi) return;

    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_math_intuition_reset", 0.0f);


    nimcp_mutex_lock(mi->lock);

    mi->patterns_detected = 0;
    mi->symmetries_detected = 0;
    mi->analogies_solved = 0;
    mi->geometric_analyses = 0;
    mi->total_pattern_confidence = 0.0;
    mi->total_symmetry_confidence = 0.0;

    nimcp_mutex_unlock(mi->lock);
}

const char* math_intuition_get_last_error(void) {
    return g_math_error;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int mathematical_intuition_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    mathematical_intuition_heartbeat("mathematical_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mathematical_Intuition");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mathematical_intuition_heartbeat("mathematical_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mathematical_Intuition");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mathematical_Intuition");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void mathematical_intuition_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mathematical_intuition_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Functions
 * ============================================================================ */

int mathematical_intuition_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mathematical_intuition_training_begin: NULL argument");
        return -1;
    }
    mathematical_intuition_heartbeat_instance(g_mathematical_intuition_health_agent, "mathematical_intuition_training_begin", 0.0f);
    math_intuition_t* mi = (math_intuition_t*)instance;
    mi->patterns_detected = 0;
    mi->symmetries_detected = 0;
    mi->analogies_solved = 0;
    mi->geometric_analyses = 0;
    mi->total_pattern_confidence = 0.0;
    mi->total_symmetry_confidence = 0.0;
    NIMCP_LOGGING_INFO("Mathematical intuition training begin: counters reset");
    return 0;
}

int mathematical_intuition_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mathematical_intuition_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mathematical_intuition_heartbeat_instance(g_mathematical_intuition_health_agent, "mathematical_intuition_training_step", progress);
    math_intuition_t* mi = (math_intuition_t*)instance;
    mi->patterns_detected++;
    /* Sharpen pattern confidence threshold with training */
    float threshold_adjust = 0.01f * progress;
    mi->config.pattern_confidence_threshold += threshold_adjust;
    if (mi->config.pattern_confidence_threshold > 0.95f)
        mi->config.pattern_confidence_threshold = 0.95f;
    /* Tighten symmetry tolerance as pattern recognition improves */
    mi->config.symmetry_tolerance *= (1.0f - 0.05f * progress);
    if (mi->config.symmetry_tolerance < 0.001f)
        mi->config.symmetry_tolerance = 0.001f;
    /* Reduce inflammation impact through adaptation */
    mi->config.inflammation_sensitivity *= (1.0f - 0.1f * progress);
    if (mi->config.inflammation_sensitivity < 0.1f)
        mi->config.inflammation_sensitivity = 0.1f;
    return 0;
}

int mathematical_intuition_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mathematical_intuition_training_end: NULL argument");
        return -1;
    }
    mathematical_intuition_heartbeat_instance(g_mathematical_intuition_health_agent, "mathematical_intuition_training_end", 1.0f);
    math_intuition_t* mi = (math_intuition_t*)instance;
    float avg_pattern_conf = (mi->patterns_detected > 0)
        ? (float)(mi->total_pattern_confidence / (double)mi->patterns_detected)
        : 0.0f;
    float avg_sym_conf = (mi->symmetries_detected > 0)
        ? (float)(mi->total_symmetry_confidence / (double)mi->symmetries_detected)
        : 0.0f;
    NIMCP_LOGGING_INFO("Mathematical intuition training end: %lu patterns (avg_conf=%.4f), "
                       "%lu symmetries (avg_conf=%.4f), %lu analogies",
                       (unsigned long)mi->patterns_detected, avg_pattern_conf,
                       (unsigned long)mi->symmetries_detected, avg_sym_conf,
                       (unsigned long)mi->analogies_solved);
    return 0;
}
