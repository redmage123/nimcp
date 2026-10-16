/**
 * @file nimcp_feature_hypercolumns.c
 * @brief Implementation of feature hypercolumn module
 *
 * WHAT: Complete implementation of feature-selective hypercolumns with
 * tuning curves, population coding, competitive dynamics, and learning.
 *
 * WHY: Provides systematic feature representation through organized
 * minicolumns, enabling sparse coding and population vector decoding.
 *
 * HOW: Implements Gaussian/von Mises tuning, competitive normalization,
 * Hebbian learning, and population statistics for multi-dimensional
 * feature spaces.
 *
 * @version 1.0.0
 * @date 2025-01-25
 */

#include "core/cortical_columns/nimcp_feature_hypercolumns.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/rng/nimcp_rand.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <float.h>
#include <stdlib.h>
#include <pthread.h>

#define LOG_MODULE "feature_hypercolumns"

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(feature_hypercolumns)

//=============================================================================
// Bio-Async Module Context (Thread-Safe Initialization)
//=============================================================================

static bio_module_context_t bio_ctx = NULL;
static bool bio_async_enabled = false;
static pthread_once_t bio_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t bio_cleanup_mutex = NIMCP_MUTEX_INITIALIZER;

static void feature_hypercolumns_bio_init_impl(void) {
    if (!bio_router_is_initialized()) {
        return;
    }

    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_CORTICAL_HYPERCOLUMNS,
        .module_name = "feature_hypercolumns",
        .inbox_capacity = 128,
        .user_data = NULL
    };

    bio_ctx = bio_router_register_module(&bio_info);
    if (bio_ctx) {
        bio_async_enabled = true;
        LOG_INFO(LOG_MODULE, "Bio-async registered for feature_hypercolumns module");
    }
}

__attribute__((constructor))
static void feature_hypercolumns_bio_init(void) {
    pthread_once(&bio_init_once, feature_hypercolumns_bio_init_impl);
}

__attribute__((destructor))
static void feature_hypercolumns_bio_cleanup(void) {
    nimcp_mutex_lock(&bio_cleanup_mutex);
    if (bio_async_enabled && bio_ctx) {
        bio_router_unregister_module(bio_ctx);
        bio_ctx = NULL;
        bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for feature_hypercolumns module");
    }
    nimcp_mutex_unlock(&bio_cleanup_mutex);
}

/* ============================================================================
 * Constants
 * ========================================================================== */

#define PI NIMCP_PI_F
#define TWOPI (2.0f * PI)
#define EPSILON 1e-10f
#define DEFAULT_TUNING_WIDTH 0.2f
#define SPARSITY_THRESHOLD 0.01f

/* ============================================================================
 * Internal Helper Functions
 * ========================================================================== */

/**
 * WHAT: Computes circular (angular) distance
 * WHY: Needed for periodic features (orientation, hue, direction)
 * HOW: Finds minimum distance considering wrap-around
 */
static float compute_circular_distance(float a, float b, float period) {
    float diff = fmodf(fabsf(a - b), period);
    if (diff > period / 2.0F) {
        diff = period - diff;
    }
    return diff;
}

/**
 * WHAT: Evaluates Gaussian tuning curve
 * WHY: Standard bell-shaped selectivity for linear features
 * HOW: R = exp(-(x - x_pref)^2 / (2 * sigma^2))
 */
static float evaluate_gaussian_tuning(
    float value,
    float preferred,
    float sigma
) {
    float diff = value - preferred;
    float response = expf(-(diff * diff) / (2.0F * sigma * sigma));
    return response;
}

/**
 * WHAT: Evaluates von Mises tuning curve (circular Gaussian)
 * WHY: Appropriate for circular/periodic features
 * HOW: R = exp(kappa * cos(theta - theta_pref))
 */
static float evaluate_von_mises_tuning(
    float value,
    float preferred,
    float kappa,
    float period
) {
    float diff = compute_circular_distance(value, preferred, period);
    float response = expf(kappa * cosf(TWOPI * diff / period));
    return response;
}

/**
 * WHAT: Converts tuning width to von Mises kappa parameter
 * WHY: Need concentration parameter for circular tuning
 * HOW: Approximate relationship: kappa ≈ 1 / sigma^2
 */
static float sigma_to_kappa(float sigma) {
    if (sigma < EPSILON) {
        return 100.0F;
    }
    return 1.0F / (sigma * sigma);
}

/**
 * WHAT: Computes multi-dimensional column index
 * WHY: Flatten multi-dimensional feature space to 1D array
 * HOW: Row-major indexing across all dimensions
 */
static uint32_t compute_column_index(
    const uint32_t* indices,
    const feature_dimension_t* dimensions,
    uint32_t num_dimensions
) {
    uint32_t index = 0;
    uint32_t multiplier = 1;

    for (int32_t i = num_dimensions - 1; i >= 0; i--) {
        index += indices[i] * multiplier;
        multiplier *= dimensions[i].num_columns;
    }

    return index;
}

/**
 * WHAT: Decomposes flat index to multi-dimensional indices
 * WHY: Reverse operation of compute_column_index
 * HOW: Extract indices from row-major flattened index
 */
static void decompose_column_index(
    uint32_t flat_index,
    uint32_t* indices,
    const feature_dimension_t* dimensions,
    uint32_t num_dimensions
) {
    for (int32_t i = num_dimensions - 1; i >= 0; i--) {
        indices[i] = flat_index % dimensions[i].num_columns;
        flat_index /= dimensions[i].num_columns;
    }
}

/* ============================================================================
 * Feature Dimension Configuration
 * ========================================================================== */

feature_dimension_t feature_dimension_create(
    feature_type_t type,
    float min_value,
    float max_value,
    uint32_t num_columns
) {
    feature_dimension_t dim;
    dim.type = type;
    dim.min_value = min_value;
    dim.max_value = max_value;
    dim.num_columns = num_columns;
    dim.tuning_width = DEFAULT_TUNING_WIDTH;

    // Set circular based on type
    switch (type) {
        case FEATURE_ORIENTATION:
        case FEATURE_DIRECTION:
        case FEATURE_COLOR_HUE:
            dim.is_circular = true;
            break;
        default:
            dim.is_circular = false;
            break;
    }

    return dim;
}

void feature_dimension_set_circular(
    feature_dimension_t* dim,
    bool is_circular
) {
    if (!dim) {
        LOG_ERROR(LOG_MODULE, "NULL dimension pointer");
        return;
    }

    dim->is_circular = is_circular;
}

void feature_dimension_set_tuning_width(
    feature_dimension_t* dim,
    float width
) {
    if (!dim) {
        LOG_ERROR(LOG_MODULE, "NULL dimension pointer");
        return;
    }

    if (width <= 0.0F) {
        LOG_ERROR(LOG_MODULE, "Invalid tuning width: %f", width);
        return;
    }

    dim->tuning_width = width;
}

/* ============================================================================
 * Hypercolumn Creation and Destruction
 * ========================================================================== */

/**
 * WHAT: Initializes single feature column
 * WHY: Sets up tuning properties for one column
 * HOW: Assigns preferred value and allocates weight array
 */
static bool init_feature_column(
    feature_column_t* col,
    float preferred_value,
    float tuning_width,
    uint32_t num_weights
) {
    if (!col) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_feature_column: col is NULL");
        return false;
    }

    col->preferred_value = preferred_value;
    col->tuning_width = tuning_width;
    col->activation = 0.0F;
    col->num_weights = num_weights;
    col->weights = NULL;

    if (num_weights > 0) {
        col->weights = (float*)nimcp_malloc(num_weights * sizeof(float));
        if (!col->weights) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_feature_column: col->weights is NULL");
            return false;
        }

        // Initialize with small random values
        for (uint32_t i = 0; i < num_weights; i++) {
            col->weights[i] = (nimcp_rand_uniform() - 0.5F) * 0.1F;
        }
    }

    return true;
}

feature_hypercolumn_t* feature_hypercolumn_create(
    const feature_dimension_t* dimensions,
    uint32_t num_dimensions
) {
    if (!dimensions || num_dimensions == 0) {
        LOG_ERROR(LOG_MODULE, "Invalid dimensions");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "feature_hypercolumn_create: dimensions is NULL");
        return NULL;
    }

    feature_hypercolumn_t* hcol = (feature_hypercolumn_t*)nimcp_malloc(
        sizeof(feature_hypercolumn_t)
    );
    if (!hcol) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate hypercolumn");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "feature_hypercolumn_create: hcol is NULL");
        return NULL;
    }
    memset(hcol, 0, sizeof(feature_hypercolumn_t));

    // Copy dimensions
    hcol->num_dimensions = num_dimensions;
    hcol->dimensions = (feature_dimension_t*)nimcp_malloc(
        num_dimensions * sizeof(feature_dimension_t)
    );
    if (!hcol->dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "feature_hypercolumn_create: hcol->dimensions is NULL");
        goto cleanup;
    }
    memcpy(hcol->dimensions, dimensions,
           num_dimensions * sizeof(feature_dimension_t));

    // Compute total columns (product of all dimensions)
    hcol->total_columns = 1;
    for (uint32_t i = 0; i < num_dimensions; i++) {
        hcol->total_columns *= dimensions[i].num_columns;
    }

    // Allocate columns
    hcol->columns = (feature_column_t*)nimcp_malloc(
        hcol->total_columns * sizeof(feature_column_t)
    );
    if (!hcol->columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "feature_hypercolumn_create: hcol->columns is NULL");
        goto cleanup;
    }
    memset(hcol->columns, 0, hcol->total_columns * sizeof(feature_column_t));

    // Initialize each column
    uint32_t* indices = (uint32_t*)nimcp_malloc(
        num_dimensions * sizeof(uint32_t)
    );
    if (!indices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "feature_hypercolumn_create: indices is NULL");
        goto cleanup;
    }

    for (uint32_t col_idx = 0; col_idx < hcol->total_columns; col_idx++) {
        decompose_column_index(col_idx, indices, dimensions, num_dimensions);

        // Compute preferred value (multi-dimensional)
        // For now, use first dimension's value as representative
        uint32_t dim_idx = indices[0];
        float preferred = dimensions[0].min_value +
            (dim_idx * (dimensions[0].max_value - dimensions[0].min_value)) /
            fmaxf(1.0F, dimensions[0].num_columns - 1.0F);

        if (!init_feature_column(&hcol->columns[col_idx],
                                 preferred,
                                 dimensions[0].tuning_width,
                                 0)) {
            // Cleanup on failure — free weights for already-initialized columns
            for (uint32_t j = 0; j < col_idx; j++) {
                if (hcol->columns[j].weights) {
                    nimcp_free(hcol->columns[j].weights);
                }
            }
            nimcp_free(indices);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "feature_hypercolumn_create: column init failed");
            goto cleanup;
        }
    }

    nimcp_free(indices);

    // Initialize spatial properties
    hcol->position[0] = 0.0F;
    hcol->position[1] = 0.0F;
    hcol->position[2] = 0.0F;
    hcol->receptive_field_size = 1.0F;

    // Create mutex
    hcol->mutex = nimcp_platform_mutex_create();
    if (!hcol->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "feature_hypercolumn_create: mutex allocation failed");
        goto cleanup_with_columns;
    }

    LOG_INFO(LOG_MODULE, "Created feature hypercolumn: %u dims, %u columns",
             num_dimensions, hcol->total_columns);

    return hcol;

cleanup_with_columns:
    /* Free weights from already-initialized columns */
    if (hcol->columns) {
        for (uint32_t i = 0; i < hcol->total_columns; i++) {
            if (hcol->columns[i].weights) {
                nimcp_free(hcol->columns[i].weights);
            }
        }
    }
cleanup:
    if (hcol) {
        nimcp_free(hcol->columns);
        nimcp_free(hcol->dimensions);
        nimcp_free(hcol);
    }
    return NULL;
}

void feature_hypercolumn_destroy(feature_hypercolumn_t* hcol) {
    if (!hcol) {
        return;
    }

    if (hcol->mutex) {
        nimcp_platform_mutex_destroy(hcol->mutex);
        nimcp_free(hcol->mutex);
    }

    if (hcol->columns) {
        for (uint32_t i = 0; i < hcol->total_columns; i++) {
            if (hcol->columns[i].weights) {
                nimcp_free(hcol->columns[i].weights);
            }
        }
        nimcp_free(hcol->columns);
    }

    if (hcol->dimensions) {
        nimcp_free(hcol->dimensions);
    }

    nimcp_free(hcol);
}

/* ============================================================================
 * Convenience Constructors
 * ========================================================================== */

feature_hypercolumn_t* feature_hypercolumn_create_orientation(
    uint32_t num_orientations
) {
    if (num_orientations == 0) {
        LOG_ERROR(LOG_MODULE, "Invalid orientation count");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "feature_hypercolumn_create_orientation: num_orientations is zero");
        return NULL;
    }

    feature_dimension_t dim = feature_dimension_create(
        FEATURE_ORIENTATION, 0.0F, 180.0F, num_orientations
    );
    dim.is_circular = true;
    dim.tuning_width = 30.0F / 180.0F;  // ~30 degrees bandwidth

    return feature_hypercolumn_create(&dim, 1);
}

feature_hypercolumn_t* feature_hypercolumn_create_direction(
    uint32_t num_directions
) {
    if (num_directions == 0) {
        LOG_ERROR(LOG_MODULE, "Invalid direction count");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "feature_hypercolumn_create_direction: num_directions is zero");
        return NULL;
    }

    feature_dimension_t dim = feature_dimension_create(
        FEATURE_DIRECTION, 0.0F, 360.0F, num_directions
    );
    dim.is_circular = true;
    dim.tuning_width = 45.0F / 360.0F;  // ~45 degrees bandwidth

    return feature_hypercolumn_create(&dim, 1);
}

feature_hypercolumn_t* feature_hypercolumn_create_spatial_freq(
    uint32_t num_octaves,
    float min_freq,
    float max_freq
) {
    if (num_octaves == 0 || min_freq <= 0.0F || max_freq <= min_freq) {
        LOG_ERROR(LOG_MODULE, "Invalid spatial frequency parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "feature_hypercolumn_create_spatial_freq: num_octaves is zero");
        return NULL;
    }

    feature_dimension_t dim = feature_dimension_create(
        FEATURE_SPATIAL_FREQ, min_freq, max_freq, num_octaves
    );
    dim.is_circular = false;
    dim.tuning_width = 0.5F;  // Half-octave bandwidth

    return feature_hypercolumn_create(&dim, 1);
}

feature_hypercolumn_t* feature_hypercolumn_create_color(
    uint32_t num_hues,
    uint32_t num_saturations
) {
    if (num_hues == 0 || num_saturations == 0) {
        LOG_ERROR(LOG_MODULE, "Invalid color parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "feature_hypercolumn_create_color: num_hues is zero");
        return NULL;
    }

    feature_dimension_t dims[2];

    // Hue dimension (circular)
    dims[0] = feature_dimension_create(
        FEATURE_COLOR_HUE, 0.0F, 360.0F, num_hues
    );
    dims[0].is_circular = true;
    dims[0].tuning_width = 45.0F / 360.0F;

    // Saturation dimension (linear)
    dims[1] = feature_dimension_create(
        FEATURE_COLOR_SATURATION, 0.0F, 1.0F, num_saturations
    );
    dims[1].is_circular = false;
    dims[1].tuning_width = 0.2F;

    return feature_hypercolumn_create(dims, 2);
}

feature_hypercolumn_t* feature_hypercolumn_create_disparity(
    uint32_t num_disparities,
    float max_disparity
) {
    if (num_disparities == 0 || max_disparity <= 0.0F) {
        LOG_ERROR(LOG_MODULE, "Invalid disparity parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "feature_hypercolumn_create_disparity: num_disparities is zero");
        return NULL;
    }

    feature_dimension_t dim = feature_dimension_create(
        FEATURE_DISPARITY, -max_disparity, max_disparity, num_disparities
    );
    dim.is_circular = false;
    dim.tuning_width = max_disparity / num_disparities;

    return feature_hypercolumn_create(&dim, 1);
}

/* ============================================================================
 * Input Processing
 * ========================================================================== */

void feature_hypercolumn_process(
    feature_hypercolumn_t* hcol,
    const float* input_features,
    uint32_t num_features
) {
    // Process pending bio-async messages
    if (bio_ctx) {
        bio_router_process_inbox(bio_ctx, 5);
    }

    if (!hcol || !input_features) {
        LOG_ERROR(LOG_MODULE, "NULL pointer");
        return;
    }

    if (num_features != hcol->num_dimensions) {
        LOG_ERROR(LOG_MODULE, "Feature count mismatch: got %u, expected %u",
                  num_features, hcol->num_dimensions);
        return;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    // Process each column
    uint32_t* indices = (uint32_t*)nimcp_malloc(
        hcol->num_dimensions * sizeof(uint32_t)
    );
    if (!indices) {
        nimcp_platform_mutex_unlock(hcol->mutex);
        return;
    }

    for (uint32_t col_idx = 0; col_idx < hcol->total_columns; col_idx++) {
        decompose_column_index(col_idx, indices,
                              hcol->dimensions, hcol->num_dimensions);

        float response = 1.0F;

        // Compute joint response across all dimensions
        for (uint32_t dim = 0; dim < hcol->num_dimensions; dim++) {
            float feature_value = input_features[dim];
            uint32_t dim_idx = indices[dim];

            // Compute preferred value for this dimension
            float preferred = hcol->dimensions[dim].min_value +
                (dim_idx * (hcol->dimensions[dim].max_value -
                           hcol->dimensions[dim].min_value)) /
                fmaxf(1.0F, hcol->dimensions[dim].num_columns - 1.0F);

            float dim_response;
            if (hcol->dimensions[dim].is_circular) {
                float period = hcol->dimensions[dim].max_value -
                              hcol->dimensions[dim].min_value;
                float kappa = sigma_to_kappa(hcol->dimensions[dim].tuning_width);
                dim_response = evaluate_von_mises_tuning(
                    feature_value, preferred, kappa, period
                );
            } else {
                dim_response = evaluate_gaussian_tuning(
                    feature_value, preferred,
                    hcol->dimensions[dim].tuning_width *
                    (hcol->dimensions[dim].max_value -
                     hcol->dimensions[dim].min_value)
                );
            }

            response *= dim_response;
        }

        hcol->columns[col_idx].activation = response;
    }

    nimcp_free(indices);
    nimcp_platform_mutex_unlock(hcol->mutex);
}

void feature_hypercolumn_process_with_input(
    feature_hypercolumn_t* hcol,
    const float* raw_input,
    uint32_t input_size
) {
    if (!hcol || !raw_input) {
        LOG_ERROR(LOG_MODULE, "NULL pointer");
        return;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    // Ensure columns have weights
    bool weights_exist = (hcol->columns[0].weights != NULL);
    if (!weights_exist) {
        // Allocate weights if needed
        for (uint32_t i = 0; i < hcol->total_columns; i++) {
            hcol->columns[i].num_weights = input_size;
            hcol->columns[i].weights = (float*)nimcp_malloc(
                input_size * sizeof(float)
            );
            if (!hcol->columns[i].weights) {
                nimcp_platform_mutex_unlock(hcol->mutex);
                return;
            }

            // Random initialization
            for (uint32_t j = 0; j < input_size; j++) {
                hcol->columns[i].weights[j] =
                    (nimcp_rand_uniform() - 0.5F) * 0.1F;
            }
        }
    }

    // Compute weighted responses
    for (uint32_t col_idx = 0; col_idx < hcol->total_columns; col_idx++) {
        feature_column_t* col = &hcol->columns[col_idx];

        if (col->num_weights != input_size) {
            continue;
        }

        float weighted_sum = 0.0F;
        for (uint32_t i = 0; i < input_size; i++) {
            weighted_sum += col->weights[i] * raw_input[i];
        }

        // Apply nonlinearity (ReLU)
        col->activation = fmaxf(0.0F, weighted_sum);
    }

    nimcp_platform_mutex_unlock(hcol->mutex);
}

/* ============================================================================
 * Competition and Sparsity
 * ========================================================================== */

void feature_hypercolumn_normalize(feature_hypercolumn_t* hcol) {
    if (!hcol) {
        LOG_ERROR(LOG_MODULE, "NULL hypercolumn");
        return;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    float sum = 0.0F;
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        sum += hcol->columns[i].activation;
    }

    if (sum > EPSILON) {
        for (uint32_t i = 0; i < hcol->total_columns; i++) {
            hcol->columns[i].activation /= sum;
        }
    }

    nimcp_platform_mutex_unlock(hcol->mutex);
}

void feature_hypercolumn_softmax(
    feature_hypercolumn_t* hcol,
    float temperature
) {
    if (!hcol) {
        LOG_ERROR(LOG_MODULE, "NULL hypercolumn");
        return;
    }

    if (temperature <= 0.0F) {
        LOG_ERROR(LOG_MODULE, "Invalid temperature: %f", temperature);
        return;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    // Find max for numerical stability
    float max_activation = -FLT_MAX;
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        if (hcol->columns[i].activation > max_activation) {
            max_activation = hcol->columns[i].activation;
        }
    }

    // Compute exp and sum
    float sum = 0.0F;
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        float exp_val = expf((hcol->columns[i].activation - max_activation) /
                            temperature);
        hcol->columns[i].activation = exp_val;
        sum += exp_val;
    }

    // Normalize
    if (sum > EPSILON) {
        for (uint32_t i = 0; i < hcol->total_columns; i++) {
            hcol->columns[i].activation /= sum;
        }
    }

    nimcp_platform_mutex_unlock(hcol->mutex);
}

/**
 * WHAT: Comparison function for qsort
 * WHY: Needed to sort activations for k-winners
 * HOW: Descending order comparison
 */
static int compare_activations_desc(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    if (fa > fb) return -1;
    if (fa < fb) return 1;
    return 0;
}

void feature_hypercolumn_k_winners(
    feature_hypercolumn_t* hcol,
    uint32_t k
) {
    if (!hcol) {
        LOG_ERROR(LOG_MODULE, "NULL hypercolumn");
        return;
    }

    if (k == 0 || k >= hcol->total_columns) {
        return;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    // Copy activations for sorting
    float* sorted_acts = (float*)nimcp_malloc(
        hcol->total_columns * sizeof(float)
    );
    if (!sorted_acts) {
        nimcp_platform_mutex_unlock(hcol->mutex);
        return;
    }

    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        sorted_acts[i] = hcol->columns[i].activation;
    }

    // Sort to find k-th largest
    qsort(sorted_acts, hcol->total_columns, sizeof(float),
          compare_activations_desc);
    float threshold = sorted_acts[k - 1];

    nimcp_free(sorted_acts);

    // Zero out below threshold
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        if (hcol->columns[i].activation < threshold) {
            hcol->columns[i].activation = 0.0F;
        }
    }

    nimcp_platform_mutex_unlock(hcol->mutex);
}

void feature_hypercolumn_threshold(
    feature_hypercolumn_t* hcol,
    float threshold
) {
    if (!hcol) {
        LOG_ERROR(LOG_MODULE, "NULL hypercolumn");
        return;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        if (hcol->columns[i].activation < threshold) {
            hcol->columns[i].activation = 0.0F;
        }
    }

    nimcp_platform_mutex_unlock(hcol->mutex);
}

/* ============================================================================
 * Decoding (Population Readout)
 * ========================================================================== */

void feature_hypercolumn_decode(
    feature_hypercolumn_t* hcol,
    float* decoded_features
) {
    if (!hcol || !decoded_features) {
        LOG_ERROR(LOG_MODULE, "NULL pointer");
        return;
    }

    feature_hypercolumn_decode_population_vector(hcol, decoded_features);
}

float feature_hypercolumn_decode_single(
    feature_hypercolumn_t* hcol,
    uint32_t dimension
) {
    if (!hcol) {
        LOG_ERROR(LOG_MODULE, "NULL hypercolumn");
        return 0.0F;
    }

    if (dimension >= hcol->num_dimensions) {
        LOG_ERROR(LOG_MODULE, "Invalid dimension: %u", dimension);
        return 0.0F;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    float weighted_sum = 0.0F;
    float weight_total = 0.0F;

    uint32_t* indices = (uint32_t*)nimcp_malloc(
        hcol->num_dimensions * sizeof(uint32_t)
    );
    if (!indices) {
        nimcp_platform_mutex_unlock(hcol->mutex);
        return 0.0F;
    }

    for (uint32_t col_idx = 0; col_idx < hcol->total_columns; col_idx++) {
        float activation = hcol->columns[col_idx].activation;

        if (activation > EPSILON) {
            decompose_column_index(col_idx, indices,
                                  hcol->dimensions, hcol->num_dimensions);

            uint32_t dim_idx = indices[dimension];
            float preferred = hcol->dimensions[dimension].min_value +
                (dim_idx * (hcol->dimensions[dimension].max_value -
                           hcol->dimensions[dimension].min_value)) /
                fmaxf(1.0F, hcol->dimensions[dimension].num_columns - 1.0F);

            weighted_sum += activation * preferred;
            weight_total += activation;
        }
    }

    nimcp_free(indices);
    nimcp_platform_mutex_unlock(hcol->mutex);

    if (weight_total > EPSILON) {
        return weighted_sum / weight_total;
    }

    return 0.0F;
}

void feature_hypercolumn_decode_population_vector(
    feature_hypercolumn_t* hcol,
    float* decoded_features
) {
    if (!hcol || !decoded_features) {
        LOG_ERROR(LOG_MODULE, "NULL pointer");
        return;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    for (uint32_t dim = 0; dim < hcol->num_dimensions; dim++) {
        decoded_features[dim] = 0.0F;
    }

    float* weight_totals = (float*)nimcp_malloc(
        hcol->num_dimensions * sizeof(float)
    );
    if (!weight_totals) {
        nimcp_platform_mutex_unlock(hcol->mutex);
        return;
    }

    for (uint32_t dim = 0; dim < hcol->num_dimensions; dim++) {
        weight_totals[dim] = 0.0F;
    }

    uint32_t* indices = (uint32_t*)nimcp_malloc(
        hcol->num_dimensions * sizeof(uint32_t)
    );
    if (!indices) {
        nimcp_free(weight_totals);
        nimcp_platform_mutex_unlock(hcol->mutex);
        return;
    }

    for (uint32_t col_idx = 0; col_idx < hcol->total_columns; col_idx++) {
        float activation = hcol->columns[col_idx].activation;

        if (activation > EPSILON) {
            decompose_column_index(col_idx, indices,
                                  hcol->dimensions, hcol->num_dimensions);

            for (uint32_t dim = 0; dim < hcol->num_dimensions; dim++) {
                uint32_t dim_idx = indices[dim];
                float preferred = hcol->dimensions[dim].min_value +
                    (dim_idx * (hcol->dimensions[dim].max_value -
                               hcol->dimensions[dim].min_value)) /
                    fmaxf(1.0F, hcol->dimensions[dim].num_columns - 1.0F);

                decoded_features[dim] += activation * preferred;
                weight_totals[dim] += activation;
            }
        }
    }

    // Normalize
    for (uint32_t dim = 0; dim < hcol->num_dimensions; dim++) {
        if (weight_totals[dim] > EPSILON) {
            decoded_features[dim] /= weight_totals[dim];
        }
    }

    nimcp_free(indices);
    nimcp_free(weight_totals);
    nimcp_platform_mutex_unlock(hcol->mutex);
}

/* ============================================================================
 * Activation Access
 * ========================================================================== */

float feature_hypercolumn_get_activation(
    feature_hypercolumn_t* hcol,
    uint32_t column_idx
) {
    if (!hcol) {
        LOG_ERROR(LOG_MODULE, "NULL hypercolumn");
        return 0.0F;
    }

    if (column_idx >= hcol->total_columns) {
        LOG_ERROR(LOG_MODULE, "Invalid column index: %u", column_idx);
        return 0.0F;
    }

    nimcp_platform_mutex_lock(hcol->mutex);
    float activation = hcol->columns[column_idx].activation;
    nimcp_platform_mutex_unlock(hcol->mutex);

    return activation;
}

void feature_hypercolumn_get_all_activations(
    feature_hypercolumn_t* hcol,
    float* activations
) {
    if (!hcol || !activations) {
        LOG_ERROR(LOG_MODULE, "NULL pointer");
        return;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        activations[i] = hcol->columns[i].activation;
    }

    nimcp_platform_mutex_unlock(hcol->mutex);
}

uint32_t feature_hypercolumn_get_winner(feature_hypercolumn_t* hcol) {
    if (!hcol) {
        LOG_ERROR(LOG_MODULE, "NULL hypercolumn");
        return 0;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    uint32_t winner_idx = 0;
    float max_activation = hcol->columns[0].activation;

    for (uint32_t i = 1; i < hcol->total_columns; i++) {
        if (hcol->columns[i].activation > max_activation) {
            max_activation = hcol->columns[i].activation;
            winner_idx = i;
        }
    }

    nimcp_platform_mutex_unlock(hcol->mutex);

    return winner_idx;
}

/**
 * WHAT: Helper structure for tracking top-k columns
 * WHY: Need to track both index and activation value
 * HOW: Pair structure for sorting
 */
typedef struct {
    uint32_t index;
    float activation;
} index_activation_pair_t;

/**
 * WHAT: Comparison for top-k sorting
 * WHY: Sort by activation descending
 * HOW: Compare activation values
 */
static int compare_pairs_desc(const void* a, const void* b) {
    const index_activation_pair_t* pa = (const index_activation_pair_t*)a;
    const index_activation_pair_t* pb = (const index_activation_pair_t*)b;

    if (pa->activation > pb->activation) return -1;
    if (pa->activation < pb->activation) return 1;
    return 0;
}

void feature_hypercolumn_get_top_k(
    feature_hypercolumn_t* hcol,
    uint32_t k,
    uint32_t* indices,
    float* activations
) {
    if (!hcol || !indices || !activations) {
        LOG_ERROR(LOG_MODULE, "NULL pointer");
        return;
    }

    if (k == 0 || k > hcol->total_columns) {
        LOG_ERROR(LOG_MODULE, "Invalid k: %u", k);
        return;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    // Create pairs
    index_activation_pair_t* pairs = (index_activation_pair_t*)nimcp_malloc(
        hcol->total_columns * sizeof(index_activation_pair_t)
    );
    if (!pairs) {
        nimcp_platform_mutex_unlock(hcol->mutex);
        return;
    }

    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        pairs[i].index = i;
        pairs[i].activation = hcol->columns[i].activation;
    }

    // Sort
    qsort(pairs, hcol->total_columns, sizeof(index_activation_pair_t),
          compare_pairs_desc);

    // Extract top k
    for (uint32_t i = 0; i < k; i++) {
        indices[i] = pairs[i].index;
        activations[i] = pairs[i].activation;
    }

    nimcp_free(pairs);
    nimcp_platform_mutex_unlock(hcol->mutex);
}

/* ============================================================================
 * Learning
 * ========================================================================== */

void feature_hypercolumn_learn_hebbian(
    feature_hypercolumn_t* hcol,
    const float* input,
    float learning_rate
) {
    if (!hcol || !input) {
        LOG_ERROR(LOG_MODULE, "NULL pointer");
        return;
    }

    if (learning_rate <= 0.0F || learning_rate > 1.0F) {
        LOG_ERROR(LOG_MODULE, "Invalid learning rate: %f", learning_rate);
        return;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    for (uint32_t col_idx = 0; col_idx < hcol->total_columns; col_idx++) {
        feature_column_t* col = &hcol->columns[col_idx];

        if (!col->weights || col->num_weights == 0) {
            continue;
        }

        float activation = col->activation;

        // Hebbian update: Δw = η * activation * input
        for (uint32_t i = 0; i < col->num_weights; i++) {
            col->weights[i] += learning_rate * activation * input[i];
        }

        // Normalize weights to prevent runaway growth
        float norm = 0.0F;
        for (uint32_t i = 0; i < col->num_weights; i++) {
            norm += col->weights[i] * col->weights[i];
        }

        if (norm > EPSILON) {
            norm = sqrtf(norm);
            for (uint32_t i = 0; i < col->num_weights; i++) {
                col->weights[i] /= norm;
            }
        }
    }

    nimcp_platform_mutex_unlock(hcol->mutex);
}

void feature_hypercolumn_learn_competitive(
    feature_hypercolumn_t* hcol,
    const float* input,
    float learning_rate,
    float neighborhood_sigma
) {
    if (!hcol || !input) {
        LOG_ERROR(LOG_MODULE, "NULL pointer");
        return;
    }

    if (learning_rate <= 0.0F || learning_rate > 1.0F) {
        LOG_ERROR(LOG_MODULE, "Invalid learning rate: %f", learning_rate);
        return;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    // Find winner
    uint32_t winner_idx = 0;
    float max_activation = hcol->columns[0].activation;

    for (uint32_t i = 1; i < hcol->total_columns; i++) {
        if (hcol->columns[i].activation > max_activation) {
            max_activation = hcol->columns[i].activation;
            winner_idx = i;
        }
    }

    // Update winner and neighborhood
    for (uint32_t col_idx = 0; col_idx < hcol->total_columns; col_idx++) {
        feature_column_t* col = &hcol->columns[col_idx];

        if (!col->weights || col->num_weights == 0) {
            continue;
        }

        // Compute neighborhood strength
        float distance = fabsf((float)col_idx - (float)winner_idx);
        float neighborhood = expf(-(distance * distance) /
                                 (2.0F * neighborhood_sigma * neighborhood_sigma));

        // Update weights
        float effective_lr = learning_rate * neighborhood;
        for (uint32_t i = 0; i < col->num_weights; i++) {
            col->weights[i] += effective_lr * (input[i] - col->weights[i]);
        }

        // Normalize
        float norm = 0.0F;
        for (uint32_t i = 0; i < col->num_weights; i++) {
            norm += col->weights[i] * col->weights[i];
        }

        if (norm > EPSILON) {
            norm = sqrtf(norm);
            for (uint32_t i = 0; i < col->num_weights; i++) {
                col->weights[i] /= norm;
            }
        }
    }

    nimcp_platform_mutex_unlock(hcol->mutex);
}

/* ============================================================================
 * Tuning Curve Access
 * ========================================================================== */

void feature_hypercolumn_get_tuning_curve(
    feature_hypercolumn_t* hcol,
    uint32_t dimension,
    float* values,
    float* responses,
    uint32_t num_points
) {
    if (!hcol || !values || !responses) {
        LOG_ERROR(LOG_MODULE, "NULL pointer");
        return;
    }

    if (dimension >= hcol->num_dimensions) {
        LOG_ERROR(LOG_MODULE, "Invalid dimension: %u", dimension);
        return;
    }

    if (num_points == 0) {
        return;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    feature_dimension_t* dim = &hcol->dimensions[dimension];

    // Generate sample values
    for (uint32_t i = 0; i < num_points; i++) {
        values[i] = dim->min_value +
            (i * (dim->max_value - dim->min_value)) / (num_points - 1);
    }

    // Compute responses for each column
    for (uint32_t col_idx = 0; col_idx < dim->num_columns; col_idx++) {
        float preferred = dim->min_value +
            (col_idx * (dim->max_value - dim->min_value)) /
            fmaxf(1.0F, dim->num_columns - 1.0F);

        for (uint32_t pt = 0; pt < num_points; pt++) {
            float response;

            if (dim->is_circular) {
                float period = dim->max_value - dim->min_value;
                float kappa = sigma_to_kappa(dim->tuning_width);
                response = evaluate_von_mises_tuning(
                    values[pt], preferred, kappa, period
                );
            } else {
                response = evaluate_gaussian_tuning(
                    values[pt], preferred,
                    dim->tuning_width * (dim->max_value - dim->min_value)
                );
            }

            responses[pt * dim->num_columns + col_idx] = response;
        }
    }

    nimcp_platform_mutex_unlock(hcol->mutex);
}

/* ============================================================================
 * Multi-Column Operations
 * ========================================================================== */

void feature_hypercolumn_pool_responses(
    feature_hypercolumn_t** hcols,
    uint32_t num_hcols,
    float* pooled_output
) {
    if (!hcols || !pooled_output || num_hcols == 0) {
        LOG_ERROR(LOG_MODULE, "Invalid parameters");
        return;
    }

    if (!hcols[0]) {
        LOG_ERROR(LOG_MODULE, "NULL hypercolumn");
        return;
    }

    uint32_t total_columns = hcols[0]->total_columns;

    // Initialize output
    for (uint32_t i = 0; i < total_columns; i++) {
        pooled_output[i] = 0.0F;
    }

    // Average pooling
    for (uint32_t h = 0; h < num_hcols; h++) {
        if (!hcols[h] || hcols[h]->total_columns != total_columns) {
            continue;
        }

        nimcp_platform_mutex_lock(hcols[h]->mutex);

        for (uint32_t i = 0; i < total_columns; i++) {
            pooled_output[i] += hcols[h]->columns[i].activation;
        }

        nimcp_platform_mutex_unlock(hcols[h]->mutex);
    }

    // Normalize by number of hypercolumns
    for (uint32_t i = 0; i < total_columns; i++) {
        pooled_output[i] /= num_hcols;
    }
}

/* ============================================================================
 * Statistics
 * ========================================================================== */

void feature_hypercolumn_get_stats(
    feature_hypercolumn_t* hcol,
    feature_hypercolumn_stats_t* stats
) {
    if (!hcol || !stats) {
        LOG_ERROR(LOG_MODULE, "NULL pointer");
        return;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    float sum = 0.0F;
    float max_act = -FLT_MAX;
    uint32_t num_active = 0;
    uint32_t winner_idx = 0;

    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        float act = hcol->columns[i].activation;
        sum += act;

        if (act > max_act) {
            max_act = act;
            winner_idx = i;
        }

        if (act > SPARSITY_THRESHOLD) {
            num_active++;
        }
    }

    stats->mean_activation = sum / hcol->total_columns;
    stats->max_activation = max_act;
    stats->num_active = num_active;
    stats->winner_index = winner_idx;
    stats->sparsity = 1.0F - ((float)num_active / hcol->total_columns);

    if (stats->mean_activation > EPSILON) {
        stats->selectivity = max_act / stats->mean_activation;
    } else {
        stats->selectivity = 0.0F;
    }

    // Compute entropy
    float entropy = 0.0F;
    if (sum > EPSILON) {
        for (uint32_t i = 0; i < hcol->total_columns; i++) {
            float p = hcol->columns[i].activation / sum;
            if (p > EPSILON) {
                entropy -= p * log2f(p);
            }
        }
    }
    stats->entropy = entropy;

    nimcp_platform_mutex_unlock(hcol->mutex);
}

float feature_hypercolumn_compute_sparsity(feature_hypercolumn_t* hcol) {
    if (!hcol) {
        LOG_ERROR(LOG_MODULE, "NULL hypercolumn");
        return 0.0F;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    uint32_t num_active = 0;
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        if (hcol->columns[i].activation > SPARSITY_THRESHOLD) {
            num_active++;
        }
    }

    float sparsity = 1.0F - ((float)num_active / hcol->total_columns);

    nimcp_platform_mutex_unlock(hcol->mutex);

    return sparsity;
}

float feature_hypercolumn_compute_selectivity(feature_hypercolumn_t* hcol) {
    if (!hcol) {
        LOG_ERROR(LOG_MODULE, "NULL hypercolumn");
        return 0.0F;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    float sum = 0.0F;
    float max_act = -FLT_MAX;

    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        float act = hcol->columns[i].activation;
        sum += act;
        if (act > max_act) {
            max_act = act;
        }
    }

    float mean = sum / hcol->total_columns;
    float selectivity = 0.0F;

    if (mean > EPSILON) {
        selectivity = max_act / mean;
    }

    nimcp_platform_mutex_unlock(hcol->mutex);

    return selectivity;
}
