/**
 * @file nimcp_orientation_columns.c
 * @brief Implementation of orientation-selective columns for V1 visual processing
 *
 * WHAT: Complete implementation of orientation columns and hypercolumns with
 *       Gabor filtering, energy models, tuning curves, and normalization.
 * WHY:  Provides biologically-plausible model of V1 orientation processing for
 *       edge detection and visual feature extraction.
 * HOW:  Implements mathematical models from neuroscience literature with
 *       thread-safe operations and efficient batch processing.
 *
 * @version 1.0.0
 * @date 2025-01-25
 * @author NIMCP Development Team
 */

#include "core/cortical_columns/nimcp_orientation_columns.h"
#include "utils/gabor/nimcp_gabor.h"  /* Shared Gabor filter library */
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <pthread.h>

#define LOG_MODULE "orientation_columns"

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(orientation_columns)

//=============================================================================
// Bio-Async Module Context (Thread-Safe Initialization)
//=============================================================================

static bio_module_context_t bio_ctx = NULL;
static bool bio_async_enabled = false;
static pthread_once_t bio_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t bio_cleanup_mutex = NIMCP_MUTEX_INITIALIZER;

static void orientation_columns_bio_init_impl(void) {
    if (!bio_router_is_initialized()) {
        return;
    }

    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_CORTICAL_ORIENTATION,
        .module_name = "orientation_columns",
        .inbox_capacity = 128,
        .user_data = NULL
    };

    bio_ctx = bio_router_register_module(&bio_info);
    if (bio_ctx) {
        bio_async_enabled = true;
        LOG_INFO(LOG_MODULE, "Bio-async registered for orientation_columns module");
    }
}

__attribute__((constructor))
static void orientation_columns_bio_init(void) {
    pthread_once(&bio_init_once, orientation_columns_bio_init_impl);
}

__attribute__((destructor))
static void orientation_columns_bio_cleanup(void) {
    nimcp_mutex_lock(&bio_cleanup_mutex);
    if (bio_async_enabled && bio_ctx) {
        bio_router_unregister_module(bio_ctx);
        bio_ctx = NULL;
        bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for orientation_columns module");
    }
    nimcp_mutex_unlock(&bio_cleanup_mutex);
}

/* ============================================================================
 * Internal Constants
 * ========================================================================== */

/** Default von Mises concentration parameter */
#define DEFAULT_KAPPA 4.0f

/** Default baseline response */
#define DEFAULT_BASELINE 0.1f

/** Default maximum response */
#define DEFAULT_MAX_RESPONSE 1.0f

/** Default normalization constant for divisive normalization */
#define DEFAULT_NORMALIZATION_CONSTANT 0.5f

/** Default inhibition strength */
#define DEFAULT_INHIBITION_STRENGTH 0.2f

/** Default Gabor gamma (aspect ratio) */
#define DEFAULT_GABOR_GAMMA 0.5f

/** Gabor kernel size multiplier (kernel = sigma × multiplier) */
#define GABOR_KERNEL_MULTIPLIER 6

#include "constants/nimcp_constants.h"
/** Small epsilon for numerical stability */
#define EPSILON NIMCP_EPSILON_NUMERICAL

/** Default patch size for batch processing */
#define DEFAULT_PATCH_SIZE 32

/* ============================================================================
 * Internal Helper Functions
 * ========================================================================== */

/**
 * @brief Convert degrees to radians (local helper for von Mises)
 */
static inline float deg2rad(float degrees) {
    return gabor_deg_to_rad(degrees);
}

/**
 * @brief Convert radians to degrees (local helper)
 */
static inline float rad2deg(float radians) {
    return gabor_rad_to_deg(radians);
}

/**
 * @brief Normalize angle to [0, 180) range - uses shared library
 */
static float normalize_orientation(float angle) {
    return gabor_normalize_orientation(angle);
}

/**
 * @brief Compute angular difference - uses shared library
 */
static float angular_difference(float angle1, float angle2) {
    return gabor_angular_difference(angle1, angle2);
}

/**
 * @brief Convert cc_gabor_params_t to gabor_filter_params_t
 *
 * WHAT: Bridges between orientation_columns' cc_gabor_params_t and shared library.
 * WHY:  Maintains backward compatibility while using shared Gabor implementation.
 * HOW:  Maps fields between the two structures.
 */
static void cc_to_filter_params(
    const cc_gabor_params_t* cc_params,
    gabor_filter_params_t* filter_params
) {
    if (!cc_params || !filter_params) {
        return;
    }

    gabor_default_params(filter_params);
    filter_params->orientation_deg = gabor_rad_to_deg(cc_params->theta);
    filter_params->wavelength = cc_params->lambda;
    filter_params->phase_deg = gabor_rad_to_deg(cc_params->psi);
    filter_params->aspect_ratio = cc_params->gamma;
    filter_params->sigma_x_override = cc_params->sigma_x;
    filter_params->sigma_y_override = cc_params->sigma_y;
}

/**
 * @brief Compute Gabor filter value at a point - uses shared library
 *
 * WHAT: Evaluates Gabor function at (x, y) with given parameters.
 * WHY:  Core of simple cell receptive field model.
 * HOW:  Converts to shared library format and calls gabor_evaluate.
 */
static float gabor_function(
    float x, float y,
    const cc_gabor_params_t* params
) {
    if (!params) {
        return 0.0F;
    }

    gabor_filter_params_t filter_params;
    cc_to_filter_params(params, &filter_params);

    return gabor_evaluate(x, y, &filter_params);
}

/**
 * @brief Compute von Mises function
 *
 * WHAT: Evaluates von Mises (circular normal) distribution.
 * WHY:  Models orientation tuning curves.
 * HOW:  exp(κ × cos(2(θ - μ)))
 */
static float von_mises(float theta, float mu, float kappa) {
    float angle_diff = deg2rad(theta - mu);
    return expf(kappa * cosf(2.0F * angle_diff));
}

/**
 * @brief Initialize default Gabor parameters - uses shared library
 *
 * WHAT: Sets up Gabor parameters based on spatial frequency and orientation.
 * WHY:  Provide sensible defaults for Gabor filtering.
 * HOW:  Uses shared library then converts back to cc_gabor_params_t.
 */
static void init_default_gabor_params(
    cc_gabor_params_t* params,
    float spatial_frequency,
    float orientation_deg,
    float phase
) {
    if (!params) {
        return;
    }

    /* Use shared library to compute defaults */
    gabor_filter_params_t filter_params;
    gabor_params_from_frequency(&filter_params, spatial_frequency, orientation_deg);
    filter_params.phase_deg = gabor_rad_to_deg(phase);  /* phase is in radians here */

    /* Convert back to cc_gabor_params_t */
    float sigma_x, sigma_y;
    gabor_compute_sigmas(&filter_params, &sigma_x, &sigma_y);

    params->lambda = filter_params.wavelength;
    params->sigma_x = sigma_x;
    params->sigma_y = sigma_y;
    params->gamma = filter_params.aspect_ratio;
    params->psi = phase;  /* Keep as radians per cc_gabor_params_t convention */
    params->theta = gabor_deg_to_rad(filter_params.orientation_deg);
}

/* ============================================================================
 * Orientation Column Implementation
 * ========================================================================== */

orientation_column_t* orientation_column_create(
    float preferred_orientation,
    float tuning_width,
    float spatial_frequency
) {
    /* Guard clauses */
    if (tuning_width <= 0.0F || spatial_frequency <= 0.0F) {
        LOG_ERROR("Invalid parameters: tuning_width and spatial_frequency must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "orientation_column_create: tuning_width and spatial_frequency must be > 0");
        return NULL;
    }

    /* Allocate column structure */
    orientation_column_t* col = (orientation_column_t*)nimcp_malloc(
        sizeof(orientation_column_t)
    );
    if (!col) {
        LOG_ERROR("Failed to allocate orientation column");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "orientation_column_create: allocation failed");
        return NULL;
    }

    memset(col, 0, sizeof(orientation_column_t));

    /* Initialize basic parameters */
    col->preferred_orientation = normalize_orientation(preferred_orientation);
    col->tuning_width = tuning_width;
    col->spatial_frequency = spatial_frequency;
    col->phase = 0.0F;
    col->activation = 0.0F;
    col->column_id = 0;

    /* Initialize Gabor parameters */
    init_default_gabor_params(
        &col->gabor_params,
        spatial_frequency,
        col->preferred_orientation,
        0.0F
    );

    /* Initialize tuning curve parameters */
    col->kappa = DEFAULT_KAPPA;
    col->baseline_response = DEFAULT_BASELINE;
    col->max_response = DEFAULT_MAX_RESPONSE;

    /* Initialize mutex */
    col->mutex = nimcp_malloc(sizeof(nimcp_platform_mutex_t));
    if (!col->mutex) {
        LOG_ERROR("Failed to allocate mutex for orientation column");
        nimcp_free(col);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "orientation_column_create: col->mutex is NULL");
        return NULL;
    }

    if (nimcp_platform_mutex_init(col->mutex, false) != 0) {
        LOG_ERROR("Failed to initialize mutex for orientation column");
        nimcp_free(col->mutex);
        nimcp_free(col);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "orientation_column_create: validation failed");
        return NULL;
    }

    LOG_DEBUG("Created orientation column: pref=%.1f°, width=%.1f°, freq=%.2f",
             col->preferred_orientation, tuning_width, spatial_frequency);

    return col;
}

void orientation_column_destroy(orientation_column_t* col) {
    if (!col) {
        return;
    }

    if (col->mutex) {
        nimcp_platform_mutex_destroy(col->mutex);
        nimcp_free(col->mutex);
        col->mutex = NULL;
    }

    nimcp_free(col);
}

bool orientation_column_set_gabor(
    orientation_column_t* col,
    const cc_gabor_params_t* params
) {
    if (!col || !params) {
        LOG_ERROR("NULL parameter in orientation_column_set_gabor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orientation_column_set_gabor: required parameter is NULL (col, params)");
        return false;
    }

    nimcp_platform_mutex_lock(col->mutex);
    memcpy(&col->gabor_params, params, sizeof(cc_gabor_params_t));
    nimcp_platform_mutex_unlock(col->mutex);

    return true;
}

float orientation_column_apply_gabor(
    orientation_column_t* col,
    const float* image_patch,
    uint32_t patch_width,
    uint32_t patch_height
) {
    /* Guard clauses */
    if (!col || !image_patch || patch_width == 0 || patch_height == 0) {
        LOG_ERROR("Invalid parameters in orientation_column_apply_gabor");
        return 0.0F;
    }

    nimcp_platform_mutex_lock(col->mutex);

    float response = 0.0F;
    int32_t center_x = patch_width / 2;
    int32_t center_y = patch_height / 2;

    /* Determine kernel size based on sigma */
    int32_t kernel_size = (int32_t)(
        col->gabor_params.sigma_x * GABOR_KERNEL_MULTIPLIER
    );
    if (kernel_size < 3) {
        kernel_size = 3;
    }

    /* Convolve Gabor kernel with image patch */
    for (int32_t y = 0; y < (int32_t)patch_height; y++) {
        for (int32_t x = 0; x < (int32_t)patch_width; x++) {
            int32_t dx = x - center_x;
            int32_t dy = y - center_y;

            /* Skip pixels outside kernel support */
            if (abs(dx) > kernel_size || abs(dy) > kernel_size) {
                continue;
            }

            float gabor_val = gabor_function(
                (float)dx, (float)dy,
                &col->gabor_params
            );
            float pixel_val = image_patch[y * patch_width + x];

            response += gabor_val * pixel_val;
        }
    }

    col->activation = response;
    nimcp_platform_mutex_unlock(col->mutex);

    return response;
}

float orientation_column_compute_energy(
    orientation_column_t* col,
    const float* image_patch,
    uint32_t patch_width,
    uint32_t patch_height
) {
    /* Guard clauses */
    if (!col || !image_patch || patch_width == 0 || patch_height == 0) {
        LOG_ERROR("Invalid parameters in orientation_column_compute_energy");
        return 0.0F;
    }

    nimcp_platform_mutex_lock(col->mutex);

    /* Create even and odd phase Gabor filters (quadrature pair) */
    cc_gabor_params_t even_params = col->gabor_params;
    even_params.psi = 0.0F;

    cc_gabor_params_t odd_params = col->gabor_params;
    odd_params.psi = (float)M_PI / 2.0F;

    float even_response = 0.0F;
    float odd_response = 0.0F;

    int32_t center_x = patch_width / 2;
    int32_t center_y = patch_height / 2;

    int32_t kernel_size = (int32_t)(
        col->gabor_params.sigma_x * GABOR_KERNEL_MULTIPLIER
    );
    if (kernel_size < 3) {
        kernel_size = 3;
    }

    /* Compute responses for both even and odd filters */
    for (int32_t y = 0; y < (int32_t)patch_height; y++) {
        for (int32_t x = 0; x < (int32_t)patch_width; x++) {
            int32_t dx = x - center_x;
            int32_t dy = y - center_y;

            if (abs(dx) > kernel_size || abs(dy) > kernel_size) {
                continue;
            }

            float even_gabor = gabor_function(
                (float)dx, (float)dy, &even_params
            );
            float odd_gabor = gabor_function(
                (float)dx, (float)dy, &odd_params
            );
            float pixel_val = image_patch[y * patch_width + x];

            even_response += even_gabor * pixel_val;
            odd_response += odd_gabor * pixel_val;
        }
    }

    /* Energy model: E = sqrt(even² + odd²) */
    float energy = sqrtf(
        even_response * even_response +
        odd_response * odd_response
    );

    col->activation = energy;
    nimcp_platform_mutex_unlock(col->mutex);

    return energy;
}

float orientation_column_get_response(
    orientation_column_t* col,
    float stimulus_orientation
) {
    if (!col) {
        LOG_ERROR("NULL column in orientation_column_get_response");
        return 0.0F;
    }

    nimcp_platform_mutex_lock(col->mutex);

    float normalized_stim = normalize_orientation(stimulus_orientation);

    /* Von Mises tuning curve */
    float response = col->baseline_response +
                    col->max_response * von_mises(
                        normalized_stim,
                        col->preferred_orientation,
                        col->kappa
                    );

    nimcp_platform_mutex_unlock(col->mutex);

    return response;
}

bool orientation_column_get_tuning_curve(
    orientation_column_t* col,
    float* orientations,
    float* responses,
    uint32_t num_points
) {
    /* Guard clauses */
    if (!col || !orientations || !responses || num_points == 0) {
        LOG_ERROR("Invalid parameters in orientation_column_get_tuning_curve");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orientation_column_get_tuning_curve: required parameter is NULL (col, orientations, responses)");
        return false;
    }

    float step = 180.0F / (float)num_points;

    for (uint32_t i = 0; i < num_points; i++) {
        orientations[i] = (float)i * step;
        responses[i] = orientation_column_get_response(col, orientations[i]);
    }

    return true;
}

bool orientation_column_get_stats(
    orientation_column_t* col,
    orientation_stats_t* stats
) {
    if (!col || !stats) {
        LOG_ERROR("NULL parameter in orientation_column_get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orientation_column_get_stats: required parameter is NULL (col, stats)");
        return false;
    }

    nimcp_platform_mutex_lock(col->mutex);

    memset(stats, 0, sizeof(orientation_stats_t));
    stats->mean_activation = col->activation;
    stats->max_activation = col->activation;
    stats->min_activation = col->activation;
    stats->tuning_sharpness = col->kappa;
    stats->total_activations = 1;

    nimcp_platform_mutex_unlock(col->mutex);

    return true;
}

/* ============================================================================
 * Hypercolumn Implementation
 * ========================================================================== */

orientation_hypercolumn_t* orientation_hypercolumn_create(
    uint32_t num_orientations,
    float spatial_frequency,
    float tuning_width
) {
    /* Guard clauses */
    if (num_orientations == 0 || num_orientations > ORIENTATION_MAX_ORIENTATIONS) {
        LOG_ERROR("Invalid num_orientations: %u (must be 1-%d)",
                 num_orientations, ORIENTATION_MAX_ORIENTATIONS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "orientation_hypercolumn_create: num_orientations is zero");
        return NULL;
    }

    if (spatial_frequency <= 0.0F || tuning_width <= 0.0F) {
        LOG_ERROR("Invalid parameters: spatial_frequency and tuning_width must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "orientation_hypercolumn_create: spatial_frequency and tuning_width must be > 0");
        return NULL;
    }

    /* Allocate hypercolumn structure */
    orientation_hypercolumn_t* hcol = (orientation_hypercolumn_t*)nimcp_malloc(
        sizeof(orientation_hypercolumn_t)
    );
    if (!hcol) {
        LOG_ERROR("Failed to allocate hypercolumn");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "orientation_hypercolumn_create: allocation failed");
        return NULL;
    }

    memset(hcol, 0, sizeof(orientation_hypercolumn_t));

    /* Allocate columns array */
    hcol->columns = (orientation_column_t*)nimcp_malloc(
        sizeof(orientation_column_t) * num_orientations
    );
    if (!hcol->columns) {
        LOG_ERROR("Failed to allocate columns array");
        nimcp_free(hcol);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "orientation_hypercolumn_create: hcol->columns is NULL");
        return NULL;
    }

    /* Initialize hypercolumn parameters */
    hcol->num_orientations = num_orientations;
    hcol->dominant_orientation = 0.0F;
    hcol->selectivity_index = 0.0F;
    hcol->circular_variance = 1.0F;
    hcol->inhibition_strength = DEFAULT_INHIBITION_STRENGTH;
    hcol->normalization_constant = DEFAULT_NORMALIZATION_CONSTANT;
    hcol->pinwheel_center_x = 0.0F;
    hcol->pinwheel_center_y = 0.0F;

    /* Create orientation columns evenly spaced from 0-180 degrees */
    float orientation_step = 180.0F / (float)num_orientations;

    for (uint32_t i = 0; i < num_orientations; i++) {
        float orientation = (float)i * orientation_step;

        orientation_column_t* col = orientation_column_create(
            orientation, tuning_width, spatial_frequency
        );

        if (!col) {
            LOG_ERROR("Failed to create column %u", i);
            /* Cleanup previously created columns */
            for (uint32_t j = 0; j < i; j++) {
                orientation_column_destroy(&hcol->columns[j]);
            }
            nimcp_free(hcol->columns);
            nimcp_free(hcol);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orientation_hypercolumn_create: col is NULL");
            return NULL;
        }

        /* Copy column data into array */
        memcpy(&hcol->columns[i], col, sizeof(orientation_column_t));
        hcol->columns[i].column_id = i;

        /* Free temporary column structure (mutex is now owned by array) */
        nimcp_free(col);
    }

    /* Initialize hypercolumn mutex */
    hcol->mutex = nimcp_malloc(sizeof(nimcp_platform_mutex_t));
    if (!hcol->mutex) {
        LOG_ERROR("Failed to allocate mutex for hypercolumn");
        for (uint32_t i = 0; i < num_orientations; i++) {
            if (hcol->columns[i].mutex) {
                nimcp_platform_mutex_destroy(hcol->columns[i].mutex);
                nimcp_free(hcol->columns[i].mutex);
                hcol->columns[i].mutex = NULL;
            }
        }
        nimcp_free(hcol->columns);
        nimcp_free(hcol);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "orientation_hypercolumn_create: validation failed");
        return NULL;
    }

    if (nimcp_platform_mutex_init(hcol->mutex, false) != 0) {
        LOG_ERROR("Failed to initialize mutex for hypercolumn");
        for (uint32_t i = 0; i < num_orientations; i++) {
            if (hcol->columns[i].mutex) {
                nimcp_platform_mutex_destroy(hcol->columns[i].mutex);
                nimcp_free(hcol->columns[i].mutex);
                hcol->columns[i].mutex = NULL;
            }
        }
        nimcp_free(hcol->mutex);
        nimcp_free(hcol->columns);
        nimcp_free(hcol);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "orientation_hypercolumn_create: validation failed");
        return NULL;
    }

    LOG_DEBUG("Created hypercolumn: %u orientations, freq=%.2f, width=%.1f°",
             num_orientations, spatial_frequency, tuning_width);

    return hcol;
}

void orientation_hypercolumn_destroy(orientation_hypercolumn_t* hcol) {
    if (!hcol) {
        return;
    }

    if (hcol->columns) {
        for (uint32_t i = 0; i < hcol->num_orientations; i++) {
            if (hcol->columns[i].mutex) {
                nimcp_platform_mutex_destroy(hcol->columns[i].mutex);
                nimcp_free(hcol->columns[i].mutex);
                hcol->columns[i].mutex = NULL;
            }
        }
        nimcp_free(hcol->columns);
    }

    if (hcol->mutex) {
        nimcp_platform_mutex_destroy(hcol->mutex);
        nimcp_free(hcol->mutex);
        hcol->mutex = NULL;
    }

    nimcp_free(hcol);
}

bool orientation_hypercolumn_process(
    orientation_hypercolumn_t* hcol,
    const float* image_patch,
    uint32_t patch_width,
    uint32_t patch_height
) {
    // Process pending bio-async messages
    if (bio_ctx) {
        bio_router_process_inbox(bio_ctx, 5);
    }

    /* Guard clauses */
    if (!hcol || !image_patch || patch_width == 0 || patch_height == 0) {
        LOG_ERROR("Invalid parameters in orientation_hypercolumn_process");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orientation_hypercolumn_process: required parameter is NULL (hcol, image_patch)");
        return false;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    /* Compute energy response for each orientation */
    for (uint32_t i = 0; i < hcol->num_orientations; i++) {
        float energy = orientation_column_compute_energy(
            &hcol->columns[i],
            image_patch,
            patch_width,
            patch_height
        );

        hcol->columns[i].activation = energy;
    }

    /* Find dominant orientation (winner-take-all) */
    float max_activation = -FLT_MAX;
    uint32_t max_idx = 0;

    for (uint32_t i = 0; i < hcol->num_orientations; i++) {
        if (hcol->columns[i].activation > max_activation) {
            max_activation = hcol->columns[i].activation;
            max_idx = i;
        }
    }

    hcol->dominant_orientation = hcol->columns[max_idx].preferred_orientation;

    nimcp_platform_mutex_unlock(hcol->mutex);

    return true;
}

float orientation_hypercolumn_get_dominant(
    orientation_hypercolumn_t* hcol
) {
    if (!hcol) {
        LOG_ERROR("NULL hypercolumn in orientation_hypercolumn_get_dominant");
        return -1.0F;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    float max_activation = -FLT_MAX;
    uint32_t max_idx = 0;

    for (uint32_t i = 0; i < hcol->num_orientations; i++) {
        if (hcol->columns[i].activation > max_activation) {
            max_activation = hcol->columns[i].activation;
            max_idx = i;
        }
    }

    float dominant = hcol->columns[max_idx].preferred_orientation;

    nimcp_platform_mutex_unlock(hcol->mutex);

    return dominant;
}

bool orientation_hypercolumn_get_distribution(
    orientation_hypercolumn_t* hcol,
    float* orientations,
    float* responses,
    uint32_t* num_orientations
) {
    /* Guard clauses */
    if (!hcol || !orientations || !responses || !num_orientations) {
        LOG_ERROR("Invalid parameters in orientation_hypercolumn_get_distribution");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orientation_hypercolumn_get_distribution: required parameter is NULL (hcol, orientations, responses, num_orientations)");
        return false;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    for (uint32_t i = 0; i < hcol->num_orientations; i++) {
        orientations[i] = hcol->columns[i].preferred_orientation;
        responses[i] = hcol->columns[i].activation;
    }

    *num_orientations = hcol->num_orientations;

    nimcp_platform_mutex_unlock(hcol->mutex);

    return true;
}

bool orientation_hypercolumn_normalize(
    orientation_hypercolumn_t* hcol
) {
    if (!hcol) {
        LOG_ERROR("NULL hypercolumn in orientation_hypercolumn_normalize");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orientation_hypercolumn_normalize: hcol is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    /* Compute total activity: σ + Σ(R_j) */
    float total_activity = hcol->normalization_constant;

    for (uint32_t i = 0; i < hcol->num_orientations; i++) {
        total_activity += hcol->columns[i].activation;
    }

    /* Avoid division by zero */
    if (total_activity < EPSILON) {
        nimcp_platform_mutex_unlock(hcol->mutex);
        return true;
    }

    /* Apply divisive normalization: R_i' = R_i / total_activity */
    for (uint32_t i = 0; i < hcol->num_orientations; i++) {
        hcol->columns[i].activation /= total_activity;
    }

    nimcp_platform_mutex_unlock(hcol->mutex);

    return true;
}

bool orientation_hypercolumn_apply_inhibition(
    orientation_hypercolumn_t* hcol,
    float strength
) {
    /* Guard clauses */
    if (!hcol) {
        LOG_ERROR("NULL hypercolumn in orientation_hypercolumn_apply_inhibition");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orientation_hypercolumn_apply_inhibition: hcol is NULL");
        return false;
    }

    if (strength < 0.0F || strength > 1.0F) {
        LOG_ERROR("Invalid inhibition strength: %.2f (must be 0-1)", strength);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "orientation_hypercolumn_apply_inhibition: validation failed");
        return false;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    /* Allocate temporary array for new activations */
    float* new_activations = (float*)nimcp_malloc(
        sizeof(float) * hcol->num_orientations
    );
    if (!new_activations) {
        LOG_ERROR("Failed to allocate temporary activation array");
        nimcp_platform_mutex_unlock(hcol->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "orientation_hypercolumn_apply_inhibition: new_activations is NULL");
        return false;
    }

    /* Compute inhibition for each column */
    for (uint32_t i = 0; i < hcol->num_orientations; i++) {
        float inhibition = 0.0F;

        /* Sum inhibitory contributions from all other columns */
        for (uint32_t j = 0; j < hcol->num_orientations; j++) {
            if (i == j) {
                continue;
            }

            /* Compute inhibition kernel based on angular difference */
            float angle_diff = angular_difference(
                hcol->columns[i].preferred_orientation,
                hcol->columns[j].preferred_orientation
            );

            /* Gaussian inhibition kernel */
            float inhibition_kernel = expf(
                -angle_diff * angle_diff / (2.0F * 30.0F * 30.0F)
            );

            inhibition += hcol->columns[j].activation * inhibition_kernel;
        }

        /* Apply inhibition: R_i' = R_i - strength × inhibition */
        new_activations[i] = hcol->columns[i].activation - strength * inhibition;

        /* Ensure non-negative activations */
        if (new_activations[i] < 0.0F) {
            new_activations[i] = 0.0F;
        }
    }

    /* Update activations */
    for (uint32_t i = 0; i < hcol->num_orientations; i++) {
        hcol->columns[i].activation = new_activations[i];
    }

    nimcp_free(new_activations);
    nimcp_platform_mutex_unlock(hcol->mutex);

    return true;
}

/**
 * @brief Compute OSI without locking (caller must hold mutex)
 */
static float compute_osi_unlocked(orientation_hypercolumn_t* hcol) {
    /* Find preferred orientation (maximum response) */
    float max_response = -FLT_MAX;
    uint32_t max_idx = 0;

    for (uint32_t i = 0; i < hcol->num_orientations; i++) {
        if (hcol->columns[i].activation > max_response) {
            max_response = hcol->columns[i].activation;
            max_idx = i;
        }
    }

    float preferred_orientation = hcol->columns[max_idx].preferred_orientation;

    /* Find orthogonal orientation (90 degrees away) */
    float orthogonal_orientation = normalize_orientation(
        preferred_orientation + 90.0F
    );

    /* Find column closest to orthogonal orientation */
    float min_diff = 180.0F;
    uint32_t orthogonal_idx = 0;

    for (uint32_t i = 0; i < hcol->num_orientations; i++) {
        float diff = angular_difference(
            hcol->columns[i].preferred_orientation,
            orthogonal_orientation
        );
        if (diff < min_diff) {
            min_diff = diff;
            orthogonal_idx = i;
        }
    }

    float orthogonal_response = hcol->columns[orthogonal_idx].activation;

    /* Compute OSI = (R_pref - R_orth) / (R_pref + R_orth) */
    float osi;
    if (max_response + orthogonal_response < EPSILON) {
        osi = 0.0F;
    } else {
        osi = (max_response - orthogonal_response) /
              (max_response + orthogonal_response);
    }

    hcol->selectivity_index = osi;
    return osi;
}

/**
 * @brief Compute circular variance without locking (caller must hold mutex)
 */
static float compute_circular_variance_unlocked(orientation_hypercolumn_t* hcol) {
    /* Compute complex vector sum: Σ(R_i × e^(2iθ_i)) */
    float sum_real = 0.0F;
    float sum_imag = 0.0F;
    float sum_responses = 0.0F;

    for (uint32_t i = 0; i < hcol->num_orientations; i++) {
        float theta_rad = deg2rad(hcol->columns[i].preferred_orientation);
        float response = hcol->columns[i].activation;

        /* Double the angle for orientation (180-degree periodicity) */
        sum_real += response * cosf(2.0F * theta_rad);
        sum_imag += response * sinf(2.0F * theta_rad);
        sum_responses += response;
    }

    /* Compute circular variance: CV = 1 - |vector_sum| / sum_responses */
    float cv;
    if (sum_responses < EPSILON) {
        cv = 1.0F;  /* Maximum variance (no tuning) */
    } else {
        float vector_magnitude = sqrtf(
            sum_real * sum_real + sum_imag * sum_imag
        );
        cv = 1.0F - (vector_magnitude / sum_responses);
    }

    hcol->circular_variance = cv;
    return cv;
}

float orientation_hypercolumn_compute_osi(
    orientation_hypercolumn_t* hcol
) {
    if (!hcol) {
        LOG_ERROR("NULL hypercolumn in orientation_hypercolumn_compute_osi");
        return -1.0F;
    }

    nimcp_platform_mutex_lock(hcol->mutex);
    float osi = compute_osi_unlocked(hcol);
    nimcp_platform_mutex_unlock(hcol->mutex);

    return osi;
}

float orientation_hypercolumn_compute_circular_variance(
    orientation_hypercolumn_t* hcol
) {
    if (!hcol) {
        LOG_ERROR("NULL hypercolumn in orientation_hypercolumn_compute_circular_variance");
        return -1.0F;
    }

    nimcp_platform_mutex_lock(hcol->mutex);
    float cv = compute_circular_variance_unlocked(hcol);
    nimcp_platform_mutex_unlock(hcol->mutex);

    return cv;
}

bool orientation_hypercolumn_set_pinwheel(
    orientation_hypercolumn_t* hcol,
    float center_x,
    float center_y
) {
    if (!hcol) {
        LOG_ERROR("NULL hypercolumn in orientation_hypercolumn_set_pinwheel");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orientation_hypercolumn_set_pinwheel: hcol is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    hcol->pinwheel_center_x = center_x;
    hcol->pinwheel_center_y = center_y;

    nimcp_platform_mutex_unlock(hcol->mutex);

    LOG_DEBUG("Set pinwheel center to (%.2f, %.2f)", center_x, center_y);

    return true;
}

float orientation_hypercolumn_get_local_orientation(
    orientation_hypercolumn_t* hcol,
    float x,
    float y
) {
    if (!hcol) {
        LOG_ERROR("NULL hypercolumn in orientation_hypercolumn_get_local_orientation");
        return -1.0F;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    /* Compute angle from pinwheel center: θ(x,y) = atan2(y - y₀, x - x₀) */
    float dx = x - hcol->pinwheel_center_x;
    float dy = y - hcol->pinwheel_center_y;

    float angle_rad = atan2f(dy, dx);
    float angle_deg = rad2deg(angle_rad);

    /* Normalize to 0-180 range */
    angle_deg = normalize_orientation(angle_deg);

    nimcp_platform_mutex_unlock(hcol->mutex);

    return angle_deg;
}

bool orientation_hypercolumn_get_stats(
    orientation_hypercolumn_t* hcol,
    orientation_hypercolumn_stats_t* stats
) {
    if (!hcol || !stats) {
        LOG_ERROR("NULL parameter in orientation_hypercolumn_get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orientation_hypercolumn_get_stats: required parameter is NULL (hcol, stats)");
        return false;
    }

    nimcp_platform_mutex_lock(hcol->mutex);

    memset(stats, 0, sizeof(orientation_hypercolumn_stats_t));

    /* Compute OSI and circular variance (use unlocked helpers to avoid deadlock) */
    stats->mean_osi = compute_osi_unlocked(hcol);
    stats->mean_circular_variance = compute_circular_variance_unlocked(hcol);

    /* Count active columns and compute total activation */
    uint32_t num_active = 0;
    float total_activation = 0.0F;

    for (uint32_t i = 0; i < hcol->num_orientations; i++) {
        if (hcol->columns[i].activation > EPSILON) {
            num_active++;
        }
        total_activation += hcol->columns[i].activation;
    }

    stats->num_active_columns = num_active;
    stats->competition_strength = (total_activation > EPSILON) ?
        (float)num_active / (float)hcol->num_orientations : 0.0F;

    /* Compute coverage uniformity (inverse of variance) */
    float expected_activation = total_activation / (float)hcol->num_orientations;
    float variance = 0.0F;

    for (uint32_t i = 0; i < hcol->num_orientations; i++) {
        float diff = hcol->columns[i].activation - expected_activation;
        variance += diff * diff;
    }

    variance /= (float)hcol->num_orientations;
    stats->coverage_uniformity = (variance < EPSILON) ? 1.0F :
        1.0F / (1.0F + sqrtf(variance));

    nimcp_platform_mutex_unlock(hcol->mutex);

    return true;
}

/* ============================================================================
 * Batch Processing Implementation
 * ========================================================================== */

bool orientation_process_image(
    const float* image,
    uint32_t width,
    uint32_t height,
    orientation_hypercolumn_t** hypercolumns,
    uint32_t num_hypercolumns,
    float* orientation_map
) {
    /* Guard clauses */
    if (!image || !hypercolumns || !orientation_map) {
        LOG_ERROR("NULL parameter in orientation_process_image");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orientation_process_image: required parameter is NULL (image, hypercolumns, orientation_map)");
        return false;
    }

    if (width == 0 || height == 0 || num_hypercolumns == 0) {
        LOG_ERROR("Invalid dimensions in orientation_process_image");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "orientation_process_image: width is zero");
        return false;
    }

    uint32_t hypercolumn_idx = 0;
    uint32_t patch_size = DEFAULT_PATCH_SIZE;

    /* Process image in patches using hypercolumns */
    for (uint32_t y = 0; y < height; y += patch_size) {
        for (uint32_t x = 0; x < width; x += patch_size) {
            /* Compute actual patch dimensions (handle edge cases) */
            uint32_t patch_width = (x + patch_size > width) ?
                                   (width - x) : patch_size;
            uint32_t patch_height = (y + patch_size > height) ?
                                    (height - y) : patch_size;

            /* Extract image patch */
            float* patch = (float*)nimcp_malloc(
                sizeof(float) * patch_width * patch_height
            );
            if (!patch) {
                LOG_ERROR("Failed to allocate image patch");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "orientation_process_image: patch is NULL");
                return false;
            }

            for (uint32_t py = 0; py < patch_height; py++) {
                for (uint32_t px = 0; px < patch_width; px++) {
                    uint32_t img_x = x + px;
                    uint32_t img_y = y + py;
                    patch[py * patch_width + px] = image[img_y * width + img_x];
                }
            }

            /* Process patch if hypercolumn available */
            if (hypercolumn_idx < num_hypercolumns) {
                orientation_hypercolumn_process(
                    hypercolumns[hypercolumn_idx],
                    patch,
                    patch_width,
                    patch_height
                );

                /* Get dominant orientation */
                float dominant = orientation_hypercolumn_get_dominant(
                    hypercolumns[hypercolumn_idx]
                );

                /* Fill orientation map with dominant orientation */
                for (uint32_t py = 0; py < patch_height; py++) {
                    for (uint32_t px = 0; px < patch_width; px++) {
                        uint32_t img_x = x + px;
                        uint32_t img_y = y + py;
                        orientation_map[img_y * width + img_x] = dominant;
                    }
                }

                hypercolumn_idx++;
            }

            nimcp_free(patch);
        }
    }

    LOG_DEBUG("Processed image (%ux%u) with %u hypercolumns",
             width, height, hypercolumn_idx);

    return true;
}
