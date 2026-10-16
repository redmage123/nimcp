#include <stddef.h>  /* for NULL */

#define LOG_MODULE "nimcp_population_coding"
#define LOG_MODULE_ID 0x0517
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(population_coding)

/**
 * @file nimcp_population_coding.c
 * @brief Population coding implementation
 */

#include "middleware/encoding/nimcp_population_coding.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/encoding/nimcp_positional_encoding.h"
#include "utils/rng/nimcp_rand.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

/* Version 1.2.0 - Added positional encoding integration for position-aware population coding */


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * WHAT: Population coding encoder internal state
 * WHY:  Maintain configuration and working memory
 * HOW:  Store config, working buffers, mutex for thread safety, PE encoder
 */
struct population_coding_encoder_struct {
    population_coding_config_t config;  /**< Encoder configuration */
    float* work_buffer;                 /**< Working memory buffer */
    uint32_t work_buffer_size;          /**< Size of work buffer */
    nimcp_mutex_t mutex;              /**< Thread safety mutex */
    uint32_t encode_count;              /**< Number of encode operations */

    /* Positional Encoding */
    nimcp_pos_encoder_t* pos_encoder;   /**< Positional encoding encoder */
    bool pe_initialized;                /**< PE encoder initialized flag */
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * WHAT: Calculate magnitude of 3D vector
 * WHY:  Needed for normalization and vector operations
 * HOW:  sqrt(x^2 + y^2 + z^2)
 */
static float calculate_magnitude(const vector3d_t* v) {
    if (!v) {
        return 0.0F;
    }
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

/**
 * WHAT: Calculate covariance between two variables
 * WHY:  Needed for correlation and PCA
 * HOW:  Cov(X,Y) = E[(X - E[X])(Y - E[Y])]
 */
static float calculate_covariance(
    const float* x,
    const float* y,
    uint32_t n,
    float mean_x,
    float mean_y
) {
    if (!x || !y || n == 0) {
        return 0.0F;
    }

    float sum = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        sum += (x[i] - mean_x) * (y[i] - mean_y);
    }
    return sum / (float)n;
}

/**
 * WHAT: Calculate mean of array
 * WHY:  Basic statistics for PCA and normalization
 * HOW:  Use tensor library for vectorized sum, fallback to scalar loop
 */
static float calculate_mean(const float* x, uint32_t n) {
    if (!x || n == 0) {
        return 0.0F;
    }

    /* Use tensor library for vectorized sum */
    uint32_t dims[] = {n};
    nimcp_tensor_t* t = nimcp_tensor_from_data(x, dims, 1, NIMCP_DTYPE_F32, false);
    if (t) {
        nimcp_tensor_t* sum_t = nimcp_tensor_sum(t);
        nimcp_tensor_destroy(t);
        if (sum_t) {
            double sum = nimcp_tensor_get_flat(sum_t, 0);
            nimcp_tensor_destroy(sum_t);
            return (float)(sum / (double)n);
        }
    }

    /* Fallback to scalar computation */
    float sum = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        sum += x[i];
    }
    return sum / (float)n;
}

/**
 * WHAT: Compute cross-correlation at zero lag
 * WHY:  Measure synchrony between spike trains
 * HOW:  Count coincident spikes within window
 */
static float compute_zero_lag_correlation(
    const spike_train_t* train1,
    const spike_train_t* train2,
    float window_ms
) {
    if (!train1 || !train2) {
        return 0.0F;
    }
    if (train1->num_spikes == 0 || train2->num_spikes == 0) {
        return 0.0F;
    }
    // BUGFIX: Validate spike_times arrays are not NULL before accessing
    if (!train1->spike_times || !train2->spike_times) {
        return 0.0F;
    }

    uint32_t coincidences = 0;
    uint32_t total_spikes = train1->num_spikes;

    // For each spike in train1, check if train2 has spike within window
    for (uint32_t i = 0; i < train1->num_spikes; i++) {
        uint64_t t1 = train1->spike_times[i];

        for (uint32_t j = 0; j < train2->num_spikes; j++) {
            int64_t diff = (int64_t)train2->spike_times[j] - (int64_t)t1;
            if (fabsf((float)diff) <= window_ms) {
                coincidences++;
                break;  // Count each spike in train1 only once
            }
        }
    }

    return (float)coincidences / (float)total_spikes;
}

/**
 * WHAT: Comparison function for qsort
 * WHY:  Sort rates for sparse coding
 * HOW:  Descending order by value
 */
typedef struct {
    float rate;
    uint32_t index;
} rate_index_pair_t;

static int compare_rate_index_desc(const void* a, const void* b) {
    const rate_index_pair_t* pair_a = (const rate_index_pair_t*)a;
    const rate_index_pair_t* pair_b = (const rate_index_pair_t*)b;

    if (pair_a->rate > pair_b->rate) return -1;
    if (pair_a->rate < pair_b->rate) return 1;
    return 0;
}

//=============================================================================
// Power Iteration for PCA
//=============================================================================

/**
 * WHAT: Extract single principal component using power iteration
 * WHY:  More efficient than full eigendecomposition for few components
 * HOW:  Iterative matrix-vector multiplication with normalization
 */
static bool compute_principal_component(
    const float* data_centered,
    uint32_t num_samples,
    uint32_t num_features,
    float* component_out,
    float* eigenvalue_out
) {
    if (!data_centered || !component_out || !eigenvalue_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compute_principal_component: required parameter is NULL (data_centered, component_out, eigenvalue_out)");
        return false;
    }

    const uint32_t max_iterations = 100;
    const float convergence_threshold = 1e-6F;

    // Initialize component with random values
    for (uint32_t i = 0; i < num_features; i++) {
        component_out[i] = nimcp_rand_uniform() * 2.0F - 1.0F;
    }

    // Normalize using tensor library
    float norm = 0.0F;
    {
        uint32_t dims[] = {num_features};
        nimcp_tensor_t* t = nimcp_tensor_from_data(component_out, dims, 1, NIMCP_DTYPE_F32, false);
        if (t) {
            norm = (float)nimcp_tensor_norm_p(t, 2.0);
            if (norm < FLT_EPSILON) {
                nimcp_tensor_destroy(t);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compute_principal_component: validation failed");
                return false;
            }
            nimcp_tensor_mul_scalar_(t, 1.0 / (double)norm);
            nimcp_tensor_destroy(t);
        } else {
            /* Fallback to scalar */
            for (uint32_t i = 0; i < num_features; i++) {
                norm += component_out[i] * component_out[i];
            }
            norm = sqrtf(norm);
            if (norm < FLT_EPSILON) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compute_principal_component: validation failed");
                return false;
            }
            for (uint32_t i = 0; i < num_features; i++) {
                component_out[i] /= norm;
            }
        }
    }

    // Power iteration
    float* temp = (float*)nimcp_malloc(num_features * sizeof(float));
    if (!temp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "compute_principal_component: temp is NULL");
        return false;
    }

    for (uint32_t iter = 0; iter < max_iterations; iter++) {
        // temp = data^T * data * component
        memset(temp, 0, num_features * sizeof(float));

        for (uint32_t s = 0; s < num_samples; s++) {
            float dot = 0.0F;
            for (uint32_t f = 0; f < num_features; f++) {
                dot += data_centered[s * num_features + f] * component_out[f];
            }
            for (uint32_t f = 0; f < num_features; f++) {
                temp[f] += data_centered[s * num_features + f] * dot;
            }
        }

        // Normalize temp using tensor library
        {
            uint32_t dims[] = {num_features};
            nimcp_tensor_t* t = nimcp_tensor_from_data(temp, dims, 1, NIMCP_DTYPE_F32, false);
            if (t) {
                norm = (float)nimcp_tensor_norm_p(t, 2.0);
                nimcp_tensor_destroy(t);
            } else {
                /* Fallback to scalar */
                norm = 0.0F;
                for (uint32_t i = 0; i < num_features; i++) {
                    norm += temp[i] * temp[i];
                }
                norm = sqrtf(norm);
            }
        }

        // WHY: For low-rank data, residual may have zero variance
        // HOW: Set component to zero and eigenvalue to zero, then succeed
        if (norm < FLT_EPSILON) {
            for (uint32_t i = 0; i < num_features; i++) {
                component_out[i] = 0.0F;
            }
            *eigenvalue_out = 0.0F;
            nimcp_free(temp);
            return true;
        }

        // Check convergence
        float change = 0.0F;
        for (uint32_t i = 0; i < num_features; i++) {
            float new_val = temp[i] / norm;
            change += fabsf(new_val - component_out[i]);
            component_out[i] = new_val;
        }

        if (change < convergence_threshold) {
            break;
        }
    }

    // Calculate eigenvalue: lambda = component^T * Cov * component
    *eigenvalue_out = norm / (float)num_samples;

    nimcp_free(temp);
    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

population_coding_encoder_t population_coding_create(
    const population_coding_config_t* config
) {
    // Allocate encoder
    population_coding_encoder_t encoder = (population_coding_encoder_t)nimcp_calloc(
        1, sizeof(struct population_coding_encoder_struct)
    );
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "encoder is NULL");

        return NULL;
    }

    // Set configuration
    if (config) {
        memcpy(&encoder->config, config, sizeof(population_coding_config_t));
    } else {
        encoder->config = population_coding_default_config();
    }

    // Validate and clamp configuration
    if (encoder->config.n_pca_components == 0) {
        encoder->config.n_pca_components = POPULATION_DEFAULT_PCA_COMPONENTS;
    }
    if (encoder->config.n_pca_components > POPULATION_MAX_PCA_COMPONENTS) {
        encoder->config.n_pca_components = POPULATION_MAX_PCA_COMPONENTS;
    }
    if (encoder->config.correlation_window_ms < POPULATION_MIN_CORRELATION_WINDOW) {
        encoder->config.correlation_window_ms = POPULATION_MIN_CORRELATION_WINDOW;
    }
    if (encoder->config.correlation_window_ms > POPULATION_MAX_CORRELATION_WINDOW) {
        encoder->config.correlation_window_ms = POPULATION_MAX_CORRELATION_WINDOW;
    }

    // Allocate work buffer (for correlation matrix, etc.)
    encoder->work_buffer_size = POPULATION_MAX_NEURONS;
    encoder->work_buffer = (float*)nimcp_malloc(
        encoder->work_buffer_size * sizeof(float)
    );
    if (!encoder->work_buffer) {
        nimcp_free(encoder);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "population_coding_create: encoder->work_buffer is NULL");
        return NULL;
    }

    // Initialize mutex
    if (nimcp_mutex_init(&encoder->mutex, NULL) != NIMCP_SUCCESS) {
        nimcp_free(encoder->work_buffer);
        nimcp_free(encoder);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "population_coding_create: validation failed");
        return NULL;
    }

    encoder->encode_count = 0;

    // Initialize positional encoding fields
    encoder->pos_encoder = NULL;
    encoder->pe_initialized = false;

    return encoder;
}

void population_coding_destroy(population_coding_encoder_t encoder) {
    if (!encoder) {
        return;
    }

    nimcp_mutex_destroy(&encoder->mutex);

    if (encoder->work_buffer) {
        nimcp_free(encoder->work_buffer);
    }

    // Destroy positional encoder if initialized
    if (encoder->pos_encoder) {
        nimcp_pos_encoder_destroy(encoder->pos_encoder);
        encoder->pos_encoder = NULL;
    }

    nimcp_free(encoder);
}

population_coding_config_t population_coding_default_config(void) {
    population_coding_config_t config = {
        .n_pca_components = POPULATION_DEFAULT_PCA_COMPONENTS,
        .correlation_window_ms = 2.0F,  // 2ms window for spike synchrony detection
        .synchrony_threshold = 0.5F,
        .sparsity_target = POPULATION_SPARSITY_THRESHOLD,
        .enable_pca = true,
        .enable_synchrony = true,

        /* Positional Encoding defaults */
        .enable_positional_encoding = false,  // Disabled by default
        .pe_embedding_dim = NIMCP_SMALL_EMBEDDING_DIM,               // Standard embedding dimension
        .pe_frequency_base = 10000.0F,        // Standard transformer base
        .position_weight = 0.3F               // 30% position weighting
    };
    return config;
}

//=============================================================================
// Vector Sum Coding
//=============================================================================

bool population_coding_encode_vector_sum(
    population_coding_encoder_t encoder,
    const float* rates,
    const tuning_curve_t* tuning_curves,
    uint32_t num_neurons,
    vector3d_t* vector_out
) {
    // Guard clauses
    if (!encoder || !rates || !tuning_curves || !vector_out || num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_encode_vector_sum: required parameter is NULL (encoder, rates, tuning_curves, vector_out)");
        return false;
    }
    if (num_neurons > POPULATION_MAX_NEURONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_encode_vector_sum: validation failed");
        return false;
    }

    nimcp_mutex_lock(&encoder->mutex);

    // Initialize output vector
    vector_out->x = 0.0F;
    vector_out->y = 0.0F;
    vector_out->z = 0.0F;

    // Weighted sum of preferred directions
    float total_weight = 0.0F;
    for (uint32_t i = 0; i < num_neurons; i++) {
        float weight = rates[i];
        if (weight > 0.0F) {
            vector_out->x += weight * tuning_curves[i].preferred_direction.x;
            vector_out->y += weight * tuning_curves[i].preferred_direction.y;
            vector_out->z += weight * tuning_curves[i].preferred_direction.z;
            total_weight += weight;
        }
    }

    // Calculate magnitude before normalization
    vector_out->magnitude = calculate_magnitude(vector_out);

    // Normalize by total weight for average direction
    if (total_weight > 0.0F) {
        vector_out->x /= total_weight;
        vector_out->y /= total_weight;
        vector_out->z /= total_weight;
    }

    encoder->encode_count++;
    nimcp_mutex_unlock(&encoder->mutex);
    return true;
}

bool population_coding_decode_vector_sum(
    population_coding_encoder_t encoder,
    const vector3d_t* vector,
    const tuning_curve_t* tuning_curves,
    uint32_t num_neurons,
    float* rates_out
) {
    // Guard clauses
    if (!encoder || !vector || !tuning_curves || !rates_out || num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_decode_vector_sum: required parameter is NULL (encoder, vector, tuning_curves, rates_out)");
        return false;
    }
    if (num_neurons > POPULATION_MAX_NEURONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_decode_vector_sum: validation failed");
        return false;
    }

    nimcp_mutex_lock(&encoder->mutex);

    // Calculate rate for each neuron based on cosine tuning
    for (uint32_t i = 0; i < num_neurons; i++) {
        // Cosine of angle between vector and preferred direction
        float dot = population_coding_vector3d_dot(
            vector,
            &tuning_curves[i].preferred_direction
        );

        float v_mag = calculate_magnitude(vector);
        float p_mag = calculate_magnitude(&tuning_curves[i].preferred_direction);

        float cos_angle = 0.0F;
        if (v_mag > 0.0F && p_mag > 0.0F) {
            cos_angle = dot / (v_mag * p_mag);
            // Clamp to [-1, 1]
            if (cos_angle > 1.0F) cos_angle = 1.0F;
            if (cos_angle < -1.0F) cos_angle = -1.0F;
        }

        // Rate = max_rate * max(0, cos(angle))
        rates_out[i] = tuning_curves[i].max_rate * fmaxf(0.0F, cos_angle);
    }

    nimcp_mutex_unlock(&encoder->mutex);
    return true;
}

//=============================================================================
// Center of Mass Coding
//=============================================================================

bool population_coding_encode_center_of_mass(
    population_coding_encoder_t encoder,
    const float* rates,
    const vector3d_t* positions,
    uint32_t num_neurons,
    vector3d_t* center_out
) {
    // Guard clauses
    if (!encoder || !rates || !positions || !center_out || num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_encode_center_of_mass: required parameter is NULL (encoder, rates, positions, center_out)");
        return false;
    }
    if (num_neurons > POPULATION_MAX_NEURONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_encode_center_of_mass: validation failed");
        return false;
    }

    nimcp_mutex_lock(&encoder->mutex);

    // Initialize center
    center_out->x = 0.0F;
    center_out->y = 0.0F;
    center_out->z = 0.0F;

    // Weighted sum of positions
    float total_rate = 0.0F;
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (rates[i] > 0.0F) {
            center_out->x += rates[i] * positions[i].x;
            center_out->y += rates[i] * positions[i].y;
            center_out->z += rates[i] * positions[i].z;
            total_rate += rates[i];
        }
    }

    // WHY: Fail if no active neurons (all rates zero)
    // HOW: Check total_rate and return false for degenerate case
    if (total_rate <= 0.0F) {
        nimcp_mutex_unlock(&encoder->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_encode_center_of_mass: validation failed");
        return false;
    }

    // Normalize by total rate
    center_out->x /= total_rate;
    center_out->y /= total_rate;
    center_out->z /= total_rate;

    center_out->magnitude = calculate_magnitude(center_out);

    encoder->encode_count++;
    nimcp_mutex_unlock(&encoder->mutex);
    return true;
}

//=============================================================================
// PCA Encoding
//=============================================================================

bool population_coding_encode_pca(
    population_coding_encoder_t encoder,
    const float* activity_matrix,
    uint32_t num_samples,
    uint32_t num_neurons,
    pca_result_t* result_out
) {
    // Guard clauses
    if (!encoder || !activity_matrix || !result_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_encode_pca: required parameter is NULL (encoder, activity_matrix, result_out)");
        return false;
    }
    if (num_samples == 0 || num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_encode_pca: num_samples is zero");
        return false;
    }
    if (!encoder->config.enable_pca) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_encode_pca: encoder->config is NULL");
        return false;
    }
    if (result_out->n_components == 0 || result_out->n_components > num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_encode_pca: result_out->n_components is zero");
        return false;
    }

    nimcp_mutex_lock(&encoder->mutex);

    // Calculate mean for each neuron
    for (uint32_t n = 0; n < num_neurons; n++) {
        float sum = 0.0F;
        for (uint32_t s = 0; s < num_samples; s++) {
            sum += activity_matrix[s * num_neurons + n];
        }
        result_out->mean[n] = sum / (float)num_samples;
    }

    // Center the data
    float* centered = (float*)nimcp_malloc(num_samples * num_neurons * sizeof(float));
    if (!centered) {
        nimcp_mutex_unlock(&encoder->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "population_coding_encode_pca: centered is NULL");
        return false;
    }

    for (uint32_t s = 0; s < num_samples; s++) {
        for (uint32_t n = 0; n < num_neurons; n++) {
            centered[s * num_neurons + n] =
                activity_matrix[s * num_neurons + n] - result_out->mean[n];
        }
    }

    // Extract principal components using power iteration
    float* residual = (float*)nimcp_malloc(num_samples * num_neurons * sizeof(float));
    if (!residual) {
        nimcp_free(centered);
        nimcp_mutex_unlock(&encoder->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "population_coding_encode_pca: residual is NULL");
        return false;
    }
    memcpy(residual, centered, num_samples * num_neurons * sizeof(float));

    for (uint32_t k = 0; k < result_out->n_components; k++) {
        float* component = &result_out->components[k * num_neurons];
        float eigenvalue;

        if (!compute_principal_component(
            residual, num_samples, num_neurons,
            component, &eigenvalue
        )) {
            nimcp_free(centered);
            nimcp_free(residual);
            nimcp_mutex_unlock(&encoder->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_encode_pca: operation failed");
            return false;
        }

        result_out->eigenvalues[k] = eigenvalue;

        // Deflate: remove this component from residual
        for (uint32_t s = 0; s < num_samples; s++) {
            float proj = 0.0F;
            for (uint32_t n = 0; n < num_neurons; n++) {
                proj += residual[s * num_neurons + n] * component[n];
            }
            for (uint32_t n = 0; n < num_neurons; n++) {
                residual[s * num_neurons + n] -= proj * component[n];
            }
        }
    }

    nimcp_free(centered);
    nimcp_free(residual);

    encoder->encode_count++;
    nimcp_mutex_unlock(&encoder->mutex);
    return true;
}

bool population_coding_project_pca(
    population_coding_encoder_t encoder,
    const float* activity,
    uint32_t num_neurons,
    const pca_result_t* pca_result,
    float* projection_out
) {
    // Guard clauses
    if (!encoder || !activity || !pca_result || !projection_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_project_pca: required parameter is NULL (encoder, activity, pca_result, projection_out)");
        return false;
    }
    if (num_neurons != pca_result->dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_project_pca: validation failed");
        return false;
    }

    nimcp_mutex_lock(&encoder->mutex);

    // Center activity
    float* centered = (float*)nimcp_malloc(num_neurons * sizeof(float));
    if (!centered) {
        nimcp_mutex_unlock(&encoder->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "population_coding_project_pca: centered is NULL");
        return false;
    }

    for (uint32_t n = 0; n < num_neurons; n++) {
        centered[n] = activity[n] - pca_result->mean[n];
    }

    // Project onto each component
    for (uint32_t k = 0; k < pca_result->n_components; k++) {
        float proj = 0.0F;
        const float* component = &pca_result->components[k * num_neurons];

        for (uint32_t n = 0; n < num_neurons; n++) {
            proj += centered[n] * component[n];
        }

        projection_out[k] = proj;
    }

    nimcp_free(centered);
    nimcp_mutex_unlock(&encoder->mutex);
    return true;
}

//=============================================================================
// Synchrony Analysis
//=============================================================================

bool population_coding_compute_synchrony(
    population_coding_encoder_t encoder,
    spike_train_t* const * spike_trains,
    uint32_t num_neurons,
    synchrony_result_t* result_out
) {
    // Guard clauses
    if (!encoder || !spike_trains || !result_out || num_neurons < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_compute_synchrony: required parameter is NULL (encoder, spike_trains, result_out)");
        return false;
    }
    if (!encoder->config.enable_synchrony) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_compute_synchrony: encoder->config is NULL");
        return false;
    }

    nimcp_mutex_lock(&encoder->mutex);

    // Initialize result
    result_out->synchrony_index = 0.0F;
    result_out->mean_correlation = 0.0F;
    result_out->peak_lag_ms = 0.0F;
    result_out->coherence = 0.0F;

    // WHY: Handle empty spike trains as valid edge case
    // HOW: Return success with zeroed synchrony (no spikes = no synchrony)
    bool has_spikes = false;
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (spike_trains[i] && spike_trains[i]->num_spikes > 0) {
            has_spikes = true;
            break;
        }
    }
    if (!has_spikes) {
        nimcp_mutex_unlock(&encoder->mutex);
        return true;  // Empty data is valid - return zeroed synchrony
    }

    // Sample pairs for large populations (avoid O(n^2) explosion)
    const uint32_t max_pairs = 1000;
    uint32_t num_pairs = (num_neurons * (num_neurons - 1)) / 2;
    bool sample_pairs = num_pairs > max_pairs;
    uint32_t pairs_to_compute = sample_pairs ? max_pairs : num_pairs;

    float sum_correlation = 0.0F;
    uint32_t valid_pairs = 0;

    if (sample_pairs) {
        // Random sampling of pairs
        for (uint32_t p = 0; p < pairs_to_compute; p++) {
            uint32_t i = nimcp_rand_uint(num_neurons);
            uint32_t j = nimcp_rand_uint(num_neurons);
            if (i == j) {
                j = (j + 1) % num_neurons;
            }

            float corr = compute_zero_lag_correlation(
                spike_trains[i],
                spike_trains[j],
                encoder->config.correlation_window_ms
            );

            sum_correlation += corr;
            valid_pairs++;
        }
    } else {
        // Compute all pairs
        for (uint32_t i = 0; i < num_neurons; i++) {
            for (uint32_t j = i + 1; j < num_neurons; j++) {
                float corr = compute_zero_lag_correlation(
                    spike_trains[i],
                    spike_trains[j],
                    encoder->config.correlation_window_ms
                );

                sum_correlation += corr;
                valid_pairs++;
            }
        }
    }

    // Calculate mean correlation
    if (valid_pairs > 0) {
        result_out->mean_correlation = sum_correlation / (float)valid_pairs;
        result_out->synchrony_index = result_out->mean_correlation;
    }

    // Coherence is same as synchrony for this simple implementation
    result_out->coherence = result_out->synchrony_index;

    encoder->encode_count++;
    nimcp_mutex_unlock(&encoder->mutex);
    return true;
}

bool population_coding_correlation_matrix(
    population_coding_encoder_t encoder,
    spike_train_t* const * spike_trains,
    uint32_t num_neurons,
    float* correlation_matrix_out
) {
    // Guard clauses
    if (!encoder || !spike_trains || !correlation_matrix_out || num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_correlation_matrix: required parameter is NULL (encoder, spike_trains, correlation_matrix_out)");
        return false;
    }

    nimcp_mutex_lock(&encoder->mutex);

    // Compute all pairwise correlations
    for (uint32_t i = 0; i < num_neurons; i++) {
        for (uint32_t j = 0; j < num_neurons; j++) {
            if (i == j) {
                correlation_matrix_out[i * num_neurons + j] = 1.0F;
            } else {
                float corr = compute_zero_lag_correlation(
                    spike_trains[i],
                    spike_trains[j],
                    encoder->config.correlation_window_ms
                );
                correlation_matrix_out[i * num_neurons + j] = corr;
            }
        }
    }

    nimcp_mutex_unlock(&encoder->mutex);
    return true;
}

//=============================================================================
// Distributed Representations
//=============================================================================

uint32_t population_coding_encode_sparse(
    population_coding_encoder_t encoder,
    const float* rates,
    uint32_t num_neurons,
    bool* sparse_code_out
) {
    // Guard clauses
    if (!encoder || !rates || !sparse_code_out || num_neurons == 0) {
        return 0;
    }

    nimcp_mutex_lock(&encoder->mutex);

    // Create array of (rate, index) pairs
    rate_index_pair_t* pairs = (rate_index_pair_t*)nimcp_malloc(
        num_neurons * sizeof(rate_index_pair_t)
    );
    if (!pairs) {
        nimcp_mutex_unlock(&encoder->mutex);
        return 0;
    }

    for (uint32_t i = 0; i < num_neurons; i++) {
        pairs[i].rate = rates[i];
        pairs[i].index = i;
    }

    // Sort by rate (descending)
    qsort(pairs, num_neurons, sizeof(rate_index_pair_t), compare_rate_index_desc);

    // Set top k% to active
    uint32_t k_active = (uint32_t)(encoder->config.sparsity_target * (float)num_neurons);
    if (k_active == 0) {
        k_active = 1;  // At least one active
    }

    // Initialize all to inactive
    memset(sparse_code_out, 0, num_neurons * sizeof(bool));

    // Set top k to active
    for (uint32_t i = 0; i < k_active; i++) {
        sparse_code_out[pairs[i].index] = true;
    }

    nimcp_free(pairs);
    nimcp_mutex_unlock(&encoder->mutex);
    return k_active;
}

float population_coding_sparse_overlap(
    const bool* code1,
    const bool* code2,
    uint32_t num_neurons
) {
    // Guard clauses
    if (!code1 || !code2 || num_neurons == 0) {
        return 0.0F;
    }

    uint32_t shared = 0;
    uint32_t active1 = 0;
    uint32_t active2 = 0;

    for (uint32_t i = 0; i < num_neurons; i++) {
        if (code1[i]) active1++;
        if (code2[i]) active2++;
        if (code1[i] && code2[i]) shared++;
    }

    // Jaccard similarity: |A ∩ B| / |A ∪ B|
    uint32_t union_size = active1 + active2 - shared;
    if (union_size == 0) {
        return 0.0F;
    }

    return (float)shared / (float)union_size;
}

//=============================================================================
// Utility Functions
//=============================================================================

pca_result_t* population_coding_pca_result_create(
    uint32_t n_components,
    uint32_t dim
) {
    // Guard clauses
    if (n_components == 0 || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_pca_result_create: n_components is zero");
        return NULL;
    }
    if (n_components > POPULATION_MAX_PCA_COMPONENTS || n_components > dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_pca_result_create: validation failed");
        return NULL;
    }

    pca_result_t* result = (pca_result_t*)nimcp_calloc(1, sizeof(pca_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;
    }

    result->components = (float*)nimcp_calloc(
        n_components * dim, sizeof(float)
    );
    result->eigenvalues = (float*)nimcp_calloc(n_components, sizeof(float));
    result->mean = (float*)nimcp_calloc(dim, sizeof(float));

    if (!result->components || !result->eigenvalues || !result->mean) {
        population_coding_pca_result_destroy(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "population_coding_pca_result_create: required parameter is NULL (result->components, result->eigenvalues, result->mean)");
        return NULL;
    }

    result->n_components = n_components;
    result->dim = dim;

    return result;
}

void population_coding_pca_result_destroy(pca_result_t* result) {
    if (!result) {
        return;
    }

    if (result->components) {
        nimcp_free(result->components);
    }
    if (result->eigenvalues) {
        nimcp_free(result->eigenvalues);
    }
    if (result->mean) {
        nimcp_free(result->mean);
    }

    nimcp_free(result);
}

pca_result_t* population_coding_pca_result_copy(const pca_result_t* src) {
    // Guard clause
    if (!src) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "src is NULL");

        return NULL;
    }

    pca_result_t* copy = population_coding_pca_result_create(
        src->n_components, src->dim
    );
    if (!copy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "copy is NULL");

        return NULL;
    }

    memcpy(copy->components, src->components,
           src->n_components * src->dim * sizeof(float));
    memcpy(copy->eigenvalues, src->eigenvalues,
           src->n_components * sizeof(float));
    memcpy(copy->mean, src->mean, src->dim * sizeof(float));

    return copy;
}

vector3d_t population_coding_vector3d_make(float x, float y, float z) {
    vector3d_t v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.magnitude = sqrtf(x * x + y * y + z * z);
    return v;
}

float population_coding_vector3d_dot(const vector3d_t* v1, const vector3d_t* v2) {
    if (!v1 || !v2) {
        return 0.0F;
    }
    return v1->x * v2->x + v1->y * v2->y + v1->z * v2->z;
}

bool population_coding_vector3d_normalize(vector3d_t* v) {
    if (!v) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_vector3d_normalize: v is NULL");
        return false;
    }

    float mag = calculate_magnitude(v);
    if (mag < FLT_EPSILON) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_vector3d_normalize: validation failed");
        return false;  // Cannot normalize zero vector
    }

    v->x /= mag;
    v->y /= mag;
    v->z /= mag;
    v->magnitude = 1.0F;

    return true;
}

//=============================================================================
// Positional Encoding Integration
//=============================================================================

/**
 * WHAT: Configure positional encoding for population coding
 * WHY:  Enable position-aware population representations
 * HOW:  Set PE parameters and initialize internal encoder
 *
 * BIOLOGICAL BASIS:
 * - Place cells have position-dependent tuning curves
 * - Population codes represent continuous variables across neurons
 * - Spatial organization affects neural tuning and connectivity
 */
bool population_coding_set_pe_config(
    population_coding_encoder_t encoder,
    uint32_t embedding_dim,
    float frequency_base,
    float position_weight
) {
    // Guard clauses
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_set_pe_config: encoder is NULL");
        return false;
    }
    if (embedding_dim == 0 || embedding_dim > NIMCP_POS_MAX_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_set_pe_config: embedding_dim is zero");
        return false;
    }
    if (frequency_base <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_set_pe_config: validation failed");
        return false;
    }
    if (position_weight < 0.0F || position_weight > 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_set_pe_config: validation failed");
        return false;
    }

    nimcp_mutex_lock(&encoder->mutex);

    // Destroy existing encoder if present
    if (encoder->pos_encoder) {
        nimcp_pos_encoder_destroy(encoder->pos_encoder);
        encoder->pos_encoder = NULL;
        encoder->pe_initialized = false;
    }

    // Update configuration
    encoder->config.enable_positional_encoding = true;
    encoder->config.pe_embedding_dim = embedding_dim;
    encoder->config.pe_frequency_base = frequency_base;
    encoder->config.position_weight = position_weight;

    // Create positional encoder with sinusoidal encoding
    nimcp_pos_config_t pe_config = {
        .type = NIMCP_POS_SINUSOIDAL,
        .config.sinusoidal = {
            .base = {
                .max_seq_length = POPULATION_MAX_NEURONS,
                .embedding_dim = embedding_dim,
                .cache_enabled = true,     // Cache for efficiency
                .thread_safe = false       // Parent encoder handles thread safety
            },
            .frequency_base = frequency_base,
            .frequency_scale = 1.0F
        }
    };

    encoder->pos_encoder = nimcp_pos_encoder_create(&pe_config);
    if (!encoder->pos_encoder) {
        encoder->config.enable_positional_encoding = false;
        nimcp_mutex_unlock(&encoder->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_set_pe_config: encoder->pos_encoder is NULL");
        return false;
    }

    encoder->pe_initialized = true;
    nimcp_mutex_unlock(&encoder->mutex);
    return true;
}

/**
 * WHAT: Apply positional encoding to neuron positions in population
 * WHY:  Encode spatial layout of neurons for position-aware coding
 * HOW:  Apply sinusoidal PE to each neuron position in population
 *
 * BIOLOGICAL BASIS:
 * - Encodes the topographic organization of neural populations
 * - Similar to how grid cells encode spatial position
 * - Preserves relative position information in continuous space
 */
bool population_coding_encode_neuron_positions(
    population_coding_encoder_t encoder,
    uint32_t num_neurons,
    float* position_encodings_out
) {
    // Guard clauses
    if (!encoder || !position_encodings_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_encode_neuron_positions: required parameter is NULL (encoder, position_encodings_out)");
        return false;
    }
    if (num_neurons == 0 || num_neurons > POPULATION_MAX_NEURONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_encode_neuron_positions: num_neurons is zero");
        return false;
    }
    if (!encoder->config.enable_positional_encoding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_encode_neuron_positions: encoder->config is NULL");
        return false;
    }
    if (!encoder->pe_initialized || !encoder->pos_encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_encode_neuron_positions: required parameter is NULL (encoder->pe_initialized, encoder->pos_encoder)");
        return false;
    }

    nimcp_mutex_lock(&encoder->mutex);

    // WHAT: Encode each neuron position sequentially
    // WHY:  Each neuron has a unique index in the population
    // HOW:  Use PE sequence encoding for all positions [0, num_neurons)
    int result = nimcp_pos_encode_sequence(
        encoder->pos_encoder,
        0,                                      // Start at position 0
        num_neurons,                            // Encode all neuron positions
        position_encodings_out                  // Output buffer
    );

    nimcp_mutex_unlock(&encoder->mutex);

    return (result == NIMCP_POS_SUCCESS);
}

/**
 * WHAT: Decode population activity with position weighting
 * WHY:  Incorporate spatial position information in decoding
 * HOW:  Weight decoding by position similarity using PE
 *
 * BIOLOGICAL BASIS:
 * - Models how spatial context modulates population readout
 * - Similar to attention mechanisms in cortical processing
 * - Neurons closer to target position contribute more
 */
bool population_coding_position_aware_decode(
    population_coding_encoder_t encoder,
    const float* rates,
    const float* position_encodings,
    uint32_t num_neurons,
    const float* query_position,
    const tuning_curve_t* tuning_curves,
    vector3d_t* vector_out
) {
    // Guard clauses
    if (!encoder || !rates || !position_encodings || !query_position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_position_aware_decode: required parameter is NULL (encoder, rates, position_encodings, query_position)");
        return false;
    }
    if (!tuning_curves || !vector_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_position_aware_decode: required parameter is NULL (tuning_curves, vector_out)");
        return false;
    }
    if (num_neurons == 0 || num_neurons > POPULATION_MAX_NEURONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_position_aware_decode: num_neurons is zero");
        return false;
    }
    if (!encoder->config.enable_positional_encoding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_position_aware_decode: encoder->config is NULL");
        return false;
    }
    if (!encoder->pe_initialized || !encoder->pos_encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_position_aware_decode: required parameter is NULL (encoder->pe_initialized, encoder->pos_encoder)");
        return false;
    }

    nimcp_mutex_lock(&encoder->mutex);

    uint32_t pe_dim = encoder->config.pe_embedding_dim;
    float position_weight = encoder->config.position_weight;

    // Allocate temporary buffer for weighted rates
    float* weighted_rates = (float*)nimcp_malloc(num_neurons * sizeof(float));
    if (!weighted_rates) {
        nimcp_mutex_unlock(&encoder->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "population_coding_position_aware_decode: weighted_rates is NULL");
        return false;
    }

    // WHAT: Compute position-weighted firing rates
    // WHY:  Weight each neuron by its position similarity to query
    // HOW:  weighted_rate[i] = rate[i] * (1-w + w*dot(PE(i), query_PE))
    for (uint32_t i = 0; i < num_neurons; i++) {
        // Calculate dot product between neuron position and query position
        float dot_product = 0.0F;
        const float* neuron_pe = &position_encodings[i * pe_dim];

        for (uint32_t d = 0; d < pe_dim; d++) {
            dot_product += neuron_pe[d] * query_position[d];
        }

        // Normalize dot product to [0, 1] range
        // WHY: PE vectors are not normalized, so normalize similarity score
        // HOW: Use sigmoid-like scaling: similarity = (dot + pe_dim) / (2*pe_dim)
        float similarity = (dot_product + (float)pe_dim) / (2.0F * (float)pe_dim);

        // Clamp similarity to [0, 1]
        if (similarity < 0.0F) similarity = 0.0F;
        if (similarity > 1.0F) similarity = 1.0F;

        // Apply position weighting: blend base rate with position-weighted rate
        weighted_rates[i] = rates[i] * (1.0F - position_weight + position_weight * similarity);
    }

    // WHAT: Encode vector sum using weighted rates
    // WHY:  Standard population vector encoding with position modulation
    // HOW:  Weighted sum of preferred directions by position-weighted rates
    vector_out->x = 0.0F;
    vector_out->y = 0.0F;
    vector_out->z = 0.0F;
    float total_weight = 0.0F;

    for (uint32_t i = 0; i < num_neurons; i++) {
        float weight = weighted_rates[i];
        if (weight > 0.0F) {
            vector_out->x += weight * tuning_curves[i].preferred_direction.x;
            vector_out->y += weight * tuning_curves[i].preferred_direction.y;
            vector_out->z += weight * tuning_curves[i].preferred_direction.z;
            total_weight += weight;
        }
    }

    // Calculate magnitude before normalization
    vector_out->magnitude = calculate_magnitude(vector_out);

    // Normalize by total weight for average direction
    if (total_weight > 0.0F) {
        vector_out->x /= total_weight;
        vector_out->y /= total_weight;
        vector_out->z /= total_weight;
    } else {
        // No active neurons
        nimcp_free(weighted_rates);
        nimcp_mutex_unlock(&encoder->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_coding_position_aware_decode: validation failed");
        return false;
    }

    nimcp_free(weighted_rates);
    encoder->encode_count++;
    nimcp_mutex_unlock(&encoder->mutex);
    return true;
}
