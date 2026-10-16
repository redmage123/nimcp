/**
 * @file nimcp_cortical_surround.c
 * @brief Implementation of surround suppression and contextual modulation
 *
 * WHAT: Complete implementation of center-surround interactions including
 *       iso-orientation suppression, cross-orientation facilitation, collinear
 *       facilitation, and figure-ground segregation.
 * WHY:  Provides biologically-plausible model of V1 contextual modulation for
 *       contour integration, texture segregation, and perceptual grouping.
 * HOW:  Implements mathematical models from Cavanaugh et al. (2002), Series et al.
 *       (2003) with efficient spatial filtering and thread-safe operations.
 *
 * @version 1.0.0
 * @date 2025-12-15
 * @author NIMCP Development Team
 */

#include "core/cortical_columns/nimcp_cortical_surround.h"
#include "core/cortical_columns/nimcp_orientation_columns.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <float.h>

#define LOG_MODULE "cortical_surround"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_surround)

/* ============================================================================
 * Internal Constants
 * ========================================================================== */

#include "constants/nimcp_constants.h"
/** Small epsilon for numerical stability */
#define EPSILON NIMCP_EPSILON_NUMERICAL

/** Maximum modulation factor (prevents runaway facilitation) */
#define MAX_MODULATION_FACTOR 3.0f

/** Minimum modulation factor (prevents complete suppression) */
#define MIN_MODULATION_FACTOR 0.01f

/** Collinear distance falloff (spatial extent in pixels) */
#define COLLINEAR_DISTANCE_SIGMA 20.0f

/** Figure-ground window size (for local statistics) */
#define FG_WINDOW_SIZE 16

/* ============================================================================
 * Internal Helper Functions
 * ========================================================================== */

/**
 * @brief Convert degrees to radians
 *
 * WHAT: Converts angle from degrees to radians.
 * WHY:  Math functions require radians.
 * HOW:  Multiply by π/180.
 */
static inline float deg2rad(float degrees) {
    return degrees * (float)M_PI / 180.0f;
}

/**
 * @brief Compute angular difference with 180-degree periodicity
 *
 * WHAT: Calculates shortest angular distance between orientations.
 * WHY:  Orientations have 180-degree periodicity.
 * HOW:  Wraps difference to [-90, 90] range.
 */
static float orientation_difference(float angle1, float angle2) {
    float diff = fmodf(angle1 - angle2 + 90.0f, 180.0f);
    if (diff < 0.0f) {
        diff += 180.0f;
    }
    return diff - 90.0f;
}

/**
 * @brief Compute 2D Gaussian weight
 *
 * WHAT: Evaluates 2D Gaussian at given distance from center.
 * WHY:  Models spatial falloff of surround effects.
 * HOW:  G(r) = exp(-r² / (2σ²))
 */
static inline float gaussian_2d(float distance, float sigma) {
    if (sigma < EPSILON) {
        return 0.0f;
    }
    return expf(-distance * distance / (2.0f * sigma * sigma));
}

/**
 * @brief Compute orientation tuning Gaussian
 *
 * WHAT: Evaluates orientation selectivity kernel.
 * WHY:  Models orientation bandwidth for suppression/facilitation.
 * HOW:  G(θ) = exp(-θ² / (2σ²))
 */
static inline float orientation_tuning(float angle_diff, float bandwidth) {
    if (bandwidth < EPSILON) {
        return 0.0f;
    }
    float sigma = bandwidth / 2.355f; // Convert FWHM to sigma
    return expf(-angle_diff * angle_diff / (2.0f * sigma * sigma));
}

/**
 * @brief Initialize spatial weight fields
 *
 * WHAT: Pre-computes center and surround spatial weight profiles.
 * WHY:  Efficient application of surround effects without recomputation.
 * HOW:  Computes difference of Gaussians (DoG) for center-surround.
 */
static bool initialize_spatial_fields(cortical_surround_t* surround) {
    if (!surround) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "initialize_spatial_fields: surround is NULL");
        return false;
    }

    uint32_t field_size = surround->config.field_size;
    uint32_t array_size = field_size * field_size;
    int32_t center = (int32_t)(field_size / 2);

    // Allocate weight arrays
    surround->field.suppression_weights = nimcp_malloc(array_size * sizeof(float));
    surround->field.facilitation_weights = nimcp_malloc(array_size * sizeof(float));

    if (!surround->field.suppression_weights || !surround->field.facilitation_weights) {
        NIMCP_LOGGING_ERROR("Failed to allocate spatial field weights");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "initialize_spatial_fields: required parameter is NULL (surround->field, surround->field)");
        return false;
    }

    surround->field.field_size = field_size;

    float center_sigma = surround->config.center_radius / 2.355f;
    float surround_sigma = surround->config.surround_radius / 2.355f;

    // Compute center-surround weights (difference of Gaussians)
    for (uint32_t y = 0; y < field_size; y++) {
        for (uint32_t x = 0; x < field_size; x++) {
            int32_t dx = (int32_t)x - center;
            int32_t dy = (int32_t)y - center;
            float distance = sqrtf((float)(dx * dx + dy * dy));

            uint32_t idx = y * field_size + x;

            // Center Gaussian (classical RF)
            float center_weight = gaussian_2d(distance, center_sigma);

            // Surround Gaussian (extra-classical RF)
            float surround_weight = gaussian_2d(distance, surround_sigma);

            // Suppression: surround - center (inhibitory)
            surround->field.suppression_weights[idx] =
                fmaxf(0.0f, surround_weight - center_weight);

            // Facilitation: weaker surround influence
            surround->field.facilitation_weights[idx] =
                surround_weight * 0.5f;
        }
    }

    return true;
}

/* ============================================================================
 * Lifecycle Functions
 * ========================================================================== */

bool cortical_surround_default_config(surround_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_surround_default_config: config is NULL");
        return false;
    }

    config->center_radius = SURROUND_DEFAULT_CENTER_RADIUS;
    config->surround_radius = SURROUND_DEFAULT_SURROUND_RADIUS;
    config->iso_suppression_strength = SURROUND_DEFAULT_ISO_SUPPRESSION;
    config->cross_facilitation_strength = SURROUND_DEFAULT_CROSS_FACILITATION;
    config->collinear_facilitation = SURROUND_DEFAULT_COLLINEAR_FACILITATION;
    config->orientation_bandwidth = SURROUND_DEFAULT_ORIENTATION_BANDWIDTH;
    config->enable_figure_ground = true;
    config->field_size = 65; // Odd number for symmetric kernel

    return true;
}

cortical_surround_t* cortical_surround_create(const surround_config_t* config) {
    // Guard: Validate configuration
    surround_config_t default_config;
    if (!config) {
        cortical_surround_default_config(&default_config);
        config = &default_config;
    }

    if (config->field_size > SURROUND_MAX_FIELD_SIZE || config->field_size < 3) {
        NIMCP_LOGGING_ERROR("Invalid field_size: %u (must be 3-%u)",
                           config->field_size, SURROUND_MAX_FIELD_SIZE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_surround_create: validation failed");
        return NULL;
    }

    if (config->field_size % 2 == 0) {
        NIMCP_LOGGING_ERROR("field_size must be odd for symmetric kernel");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_surround_create: 2 is zero");
        return NULL;
    }

    // Allocate main structure
    cortical_surround_t* surround = nimcp_malloc(sizeof(cortical_surround_t));
    if (!surround) {
        NIMCP_LOGGING_ERROR("Failed to allocate surround module");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surround is NULL");

        return NULL;
    }

    memset(surround, 0, sizeof(cortical_surround_t));
    memcpy(&surround->config, config, sizeof(surround_config_t));

    // Create mutex
    surround->mutex = nimcp_platform_mutex_create();
    if (!surround->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(surround);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_surround_create: surround->mutex is NULL");
        return NULL;
    }

    // Initialize spatial fields
    if (!initialize_spatial_fields(surround)) {
        NIMCP_LOGGING_ERROR("Failed to initialize spatial fields");
        nimcp_platform_mutex_destroy(surround->mutex);
        nimcp_free(surround->mutex);
        surround->mutex = NULL;
        nimcp_free(surround);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cortical_surround_create: initialize_spatial_fields is NULL");
        return NULL;
    }

    // Initialize statistics
    surround->total_updates = 0;
    surround->mean_suppression = 0.0f;
    surround->mean_facilitation = 0.0f;

    NIMCP_LOGGING_INFO("Created surround module: center=%.1f, surround=%.1f, field_size=%u",
                      config->center_radius, config->surround_radius, config->field_size);

    return surround;
}

void cortical_surround_destroy(cortical_surround_t* surround) {
    if (!surround) {
        return;
    }

    // Disconnect bio-async if connected
    if (surround->bio_async_enabled) {
        cortical_surround_disconnect_bio_async(surround);
    }

    // Free spatial fields
    if (surround->field.suppression_weights) {
        nimcp_free(surround->field.suppression_weights);
    }
    if (surround->field.facilitation_weights) {
        nimcp_free(surround->field.facilitation_weights);
    }

    // Free response arrays
    if (surround->center_responses) {
        nimcp_free(surround->center_responses);
    }
    if (surround->surround_responses) {
        nimcp_free(surround->surround_responses);
    }
    if (surround->modulated_responses) {
        nimcp_free(surround->modulated_responses);
    }

    // Free figure-ground arrays
    if (surround->border_ownership) {
        nimcp_free(surround->border_ownership);
    }
    if (surround->texture_contrast) {
        nimcp_free(surround->texture_contrast);
    }

    // Destroy mutex
    if (surround->mutex) {
        nimcp_platform_mutex_destroy(surround->mutex);
        nimcp_free(surround->mutex);
        surround->mutex = NULL;
    }

    nimcp_free(surround);
    NIMCP_LOGGING_DEBUG("Destroyed surround module");
}

/* ============================================================================
 * Integration Functions
 * ========================================================================== */

bool cortical_surround_connect_orientation_columns(
    cortical_surround_t* surround,
    struct orientation_hypercolumn_t** hypercolumns,
    uint32_t num_hypercolumns)
{
    if (!surround || !hypercolumns) {
        NIMCP_LOGGING_ERROR("NULL pointer in connect_orientation_columns");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_surround_connect_orientation_columns: required parameter is NULL (surround, hypercolumns)");
        return false;
    }

    if (num_hypercolumns == 0) {
        NIMCP_LOGGING_ERROR("num_hypercolumns is zero");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_surround_connect_orientation_columns: num_hypercolumns is zero");
        return false;
    }

    nimcp_platform_mutex_lock(surround->mutex);

    surround->orientation_columns = hypercolumns;
    surround->num_hypercolumns = num_hypercolumns;

    nimcp_platform_mutex_unlock(surround->mutex);

    NIMCP_LOGGING_INFO("Connected %u orientation hypercolumns", num_hypercolumns);
    return true;
}

/* ============================================================================
 * Surround Computation Functions
 * ========================================================================== */

float cortical_surround_compute_iso_suppression(
    const cortical_surround_t* surround,
    float center_orientation,
    float surround_orientation,
    float center_response,
    float surround_response)
{
    // Guard: Validate inputs
    if (!surround) {
        return 0.0f;
    }

    if (center_response < EPSILON || surround_response < EPSILON) {
        return 0.0f;
    }

    // Compute orientation difference
    float angle_diff = orientation_difference(center_orientation, surround_orientation);
    float abs_diff = fabsf(angle_diff);

    // Orientation tuning (strongest suppression for similar orientations)
    float orientation_weight = orientation_tuning(abs_diff,
                                                  surround->config.orientation_bandwidth);

    // Iso-orientation suppression: S(θ) = S_max × exp(-θ² / 2σ²) × R_surround
    float suppression = surround->config.iso_suppression_strength *
                       orientation_weight *
                       surround_response;

    return fminf(suppression, 1.0f);
}

float cortical_surround_compute_cross_facilitation(
    const cortical_surround_t* surround,
    float center_orientation,
    float surround_orientation,
    float center_response,
    float surround_response)
{
    // Guard: Validate inputs
    if (!surround) {
        return 0.0f;
    }

    if (center_response < EPSILON || surround_response < EPSILON) {
        return 0.0f;
    }

    // Compute orientation difference
    float angle_diff = orientation_difference(center_orientation, surround_orientation);
    float abs_diff = fabsf(angle_diff);

    // Cross-orientation facilitation peaks at ~90 degrees
    float orthogonality = fabsf(abs_diff - 90.0f);
    float cross_weight = 1.0f - orientation_tuning(orthogonality,
                                                   surround->config.orientation_bandwidth);

    // Cross-orientation facilitation
    float facilitation = surround->config.cross_facilitation_strength *
                        cross_weight *
                        surround_response;

    return fminf(facilitation, 1.0f);
}

float cortical_surround_apply_collinear_facilitation(
    const cortical_surround_t* surround,
    float center_x,
    float center_y,
    float center_orientation,
    float neighbor_x,
    float neighbor_y,
    float neighbor_orientation,
    float neighbor_response)
{
    // Guard: Validate inputs
    if (!surround) {
        return 0.0f;
    }

    if (neighbor_response < EPSILON) {
        return 0.0f;
    }

    // Compute connecting line angle
    float dx = neighbor_x - center_x;
    float dy = neighbor_y - center_y;
    float distance = sqrtf(dx * dx + dy * dy);

    if (distance < EPSILON) {
        return 0.0f;
    }

    float connection_angle = atan2f(dy, dx) * 180.0f / (float)M_PI;
    if (connection_angle < 0.0f) {
        connection_angle += 180.0f;
    }

    // Check alignment between center orientation and connecting line
    float center_alignment = fabsf(orientation_difference(center_orientation,
                                                         connection_angle));

    // Check alignment between neighbor orientation and connecting line
    float neighbor_alignment = fabsf(orientation_difference(neighbor_orientation,
                                                           connection_angle));

    // Both edges must be aligned with connecting line
    float alignment_score = cosf(deg2rad(center_alignment)) *
                           cosf(deg2rad(neighbor_alignment));

    // Collinear facilitation: F = F_max × cos²(θ_align) × distance_falloff
    float distance_weight = gaussian_2d(distance, COLLINEAR_DISTANCE_SIGMA);
    float facilitation = surround->config.collinear_facilitation *
                        alignment_score * alignment_score *
                        distance_weight *
                        neighbor_response;

    return fmaxf(0.0f, fminf(facilitation, 1.0f));
}

bool cortical_surround_detect_figure_ground(
    cortical_surround_t* surround,
    uint32_t center_x,
    uint32_t center_y,
    const float* orientation_map,
    const float* response_map,
    uint32_t width,
    uint32_t height,
    float* border_ownership,
    float* texture_contrast)
{
    // Guard: Validate inputs
    if (!surround || !orientation_map || !response_map) {
        NIMCP_LOGGING_ERROR("NULL pointer in detect_figure_ground");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_surround_detect_figure_ground: required parameter is NULL (surround, orientation_map, response_map)");
        return false;
    }

    if (!border_ownership || !texture_contrast) {
        NIMCP_LOGGING_ERROR("NULL output pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_surround_detect_figure_ground: required parameter is NULL (border_ownership, texture_contrast)");
        return false;
    }

    if (!surround->config.enable_figure_ground) {
        *border_ownership = 0.0f;
        *texture_contrast = 0.0f;
        return true;
    }

    if (center_x >= width || center_y >= height) {
        NIMCP_LOGGING_ERROR("Center position out of bounds");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_surround_detect_figure_ground: capacity exceeded");
        return false;
    }

    uint32_t center_idx = center_y * width + center_x;
    float center_orientation = orientation_map[center_idx];

    // Compute orientation discontinuity in surround
    int32_t half_window = FG_WINDOW_SIZE / 2;
    float orientation_variance = 0.0f;
    float response_variance = 0.0f;
    uint32_t sample_count = 0;

    for (int32_t dy = -half_window; dy <= half_window; dy++) {
        for (int32_t dx = -half_window; dx <= half_window; dx++) {
            if (dx == 0 && dy == 0) continue;

            int32_t x = (int32_t)center_x + dx;
            int32_t y = (int32_t)center_y + dy;

            if (x < 0 || x >= (int32_t)width || y < 0 || y >= (int32_t)height) {
                continue;
            }

            uint32_t idx = (uint32_t)y * width + (uint32_t)x;
            float surround_orientation = orientation_map[idx];
            float surround_response = response_map[idx];

            float orientation_diff = orientation_difference(center_orientation,
                                                           surround_orientation);
            orientation_variance += orientation_diff * orientation_diff;
            response_variance += fabsf(response_map[center_idx] - surround_response);
            sample_count++;
        }
    }

    if (sample_count > 0) {
        orientation_variance /= (float)sample_count;
        response_variance /= (float)sample_count;
    }

    // Border ownership: High orientation variance suggests border
    *border_ownership = tanhf(orientation_variance / 900.0f); // Normalize by 30² degrees

    // Texture contrast: High response variance suggests texture boundary
    *texture_contrast = tanhf(response_variance);

    return true;
}

float cortical_surround_modulate_response(
    cortical_surround_t* surround,
    uint32_t center_x,
    uint32_t center_y,
    const float* orientation_map,
    const float* response_map,
    uint32_t width,
    uint32_t height)
{
    // Guard: Validate inputs
    if (!surround || !orientation_map || !response_map) {
        NIMCP_LOGGING_ERROR("NULL pointer in modulate_response");
        return 0.0f;
    }

    if (center_x >= width || center_y >= height) {
        NIMCP_LOGGING_ERROR("Center position out of bounds");
        return 0.0f;
    }

    uint32_t center_idx = center_y * width + center_x;
    float center_response = response_map[center_idx];
    float center_orientation = orientation_map[center_idx];

    if (center_response < EPSILON) {
        return 0.0f;
    }

    float total_suppression = 0.0f;
    float total_facilitation = 0.0f;
    uint32_t half_field = surround->field.field_size / 2;

    // Scan surround using spatial weight fields
    for (uint32_t fy = 0; fy < surround->field.field_size; fy++) {
        for (uint32_t fx = 0; fx < surround->field.field_size; fx++) {
            int32_t global_x = (int32_t)center_x + (int32_t)fx - (int32_t)half_field;
            int32_t global_y = (int32_t)center_y + (int32_t)fy - (int32_t)half_field;

            // Skip center
            if (global_x == (int32_t)center_x && global_y == (int32_t)center_y) {
                continue;
            }

            // Bounds check
            if (global_x < 0 || global_x >= (int32_t)width ||
                global_y < 0 || global_y >= (int32_t)height) {
                continue;
            }

            uint32_t surround_idx = (uint32_t)global_y * width + (uint32_t)global_x;
            float surround_response = response_map[surround_idx];
            float surround_orientation = orientation_map[surround_idx];

            uint32_t field_idx = fy * surround->field.field_size + fx;
            float spatial_weight = surround->field.suppression_weights[field_idx];

            if (spatial_weight < EPSILON || surround_response < EPSILON) {
                continue;
            }

            // Iso-orientation suppression
            float iso_suppression = cortical_surround_compute_iso_suppression(
                surround, center_orientation, surround_orientation,
                center_response, surround_response);
            total_suppression += iso_suppression * spatial_weight;

            // Cross-orientation facilitation
            float cross_facilitation = cortical_surround_compute_cross_facilitation(
                surround, center_orientation, surround_orientation,
                center_response, surround_response);
            total_facilitation += cross_facilitation * spatial_weight * 0.5f;

            // Collinear facilitation
            float collinear_facilitation = cortical_surround_apply_collinear_facilitation(
                surround, (float)center_x, (float)center_y, center_orientation,
                (float)global_x, (float)global_y, surround_orientation, surround_response);
            total_facilitation += collinear_facilitation;
        }
    }

    // Apply modulation: R_out = R_center × (1 - suppression + facilitation)
    float modulation_factor = 1.0f - total_suppression + total_facilitation;
    modulation_factor = fmaxf(MIN_MODULATION_FACTOR,
                             fminf(MAX_MODULATION_FACTOR, modulation_factor));

    float modulated_response = center_response * modulation_factor;

    // Update statistics (thread-safe)
    nimcp_platform_mutex_lock(surround->mutex);
    surround->total_updates++;
    float alpha = 1.0f / (float)surround->total_updates;
    surround->mean_suppression += alpha * (total_suppression - surround->mean_suppression);
    surround->mean_facilitation += alpha * (total_facilitation - surround->mean_facilitation);
    nimcp_platform_mutex_unlock(surround->mutex);

    return modulated_response;
}

bool cortical_surround_batch_modulate(
    cortical_surround_t* surround,
    const float* orientation_map,
    const float* response_map,
    float* output_map,
    uint32_t width,
    uint32_t height)
{
    // Guard: Validate inputs
    if (!surround || !orientation_map || !response_map || !output_map) {
        NIMCP_LOGGING_ERROR("NULL pointer in batch_modulate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_surround_batch_modulate: required parameter is NULL (surround, orientation_map, response_map, output_map)");
        return false;
    }

    if (width == 0 || height == 0) {
        NIMCP_LOGGING_ERROR("Invalid dimensions: %ux%u", width, height);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_surround_batch_modulate: width is zero");
        return false;
    }

    // Process entire map
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t idx = y * width + x;
            output_map[idx] = cortical_surround_modulate_response(
                surround, x, y, orientation_map, response_map, width, height);
        }
    }

    return true;
}

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

bool cortical_surround_get_stats(
    const cortical_surround_t* surround,
    surround_stats_t* stats)
{
    // Guard: Validate inputs
    if (!surround || !stats) {
        NIMCP_LOGGING_ERROR("NULL pointer in get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_surround_get_stats: required parameter is NULL (surround, stats)");
        return false;
    }

    nimcp_platform_mutex_lock(surround->mutex);

    stats->mean_iso_suppression = surround->mean_suppression;
    stats->mean_cross_facilitation = surround->mean_facilitation * 0.5f;
    stats->mean_collinear_facilitation = surround->mean_facilitation * 0.5f;
    stats->max_suppression_observed = surround->config.iso_suppression_strength;
    stats->max_facilitation_observed = surround->config.collinear_facilitation;
    stats->total_updates = surround->total_updates;
    stats->figure_ground_strength = surround->config.enable_figure_ground ? 1.0f : 0.0f;

    nimcp_platform_mutex_unlock(surround->mutex);

    return true;
}

bool cortical_surround_reset_stats(cortical_surround_t* surround) {
    // Guard: Validate input
    if (!surround) {
        NIMCP_LOGGING_ERROR("NULL surround pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_surround_reset_stats: surround is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(surround->mutex);

    surround->total_updates = 0;
    surround->mean_suppression = 0.0f;
    surround->mean_facilitation = 0.0f;

    nimcp_platform_mutex_unlock(surround->mutex);

    NIMCP_LOGGING_DEBUG("Reset surround statistics");
    return true;
}

/* ============================================================================
 * Bio-Async Integration Functions
 * ========================================================================== */

bool cortical_surround_connect_bio_async(cortical_surround_t* surround) {
    // Guard: Validate input
    if (!surround) {
        NIMCP_LOGGING_ERROR("NULL surround pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_surround_connect_bio_async: surround is NULL");
        return false;
    }

    if (surround->bio_async_enabled) {
        NIMCP_LOGGING_DEBUG("Bio-async already connected");
        return true;
    }

    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_WARN("Bio-async router not available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cortical_surround_connect_bio_async: bio_router_is_initialized is NULL");
        return false;
    }

    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_CORTICAL_SURROUND,
        .module_name = "cortical_surround",
        .inbox_capacity = 64,
        .user_data = surround
    };

    surround->bio_ctx = bio_router_register_module(&bio_info);
    if (!surround->bio_ctx) {
        NIMCP_LOGGING_ERROR("Failed to register with bio-async router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_surround_connect_bio_async: surround->bio_ctx is NULL");
        return false;
    }

    surround->bio_async_enabled = true;
    NIMCP_LOGGING_INFO("Connected to bio-async router");
    return true;
}

bool cortical_surround_disconnect_bio_async(cortical_surround_t* surround) {
    // Guard: Validate input
    if (!surround) {
        NIMCP_LOGGING_ERROR("NULL surround pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_surround_disconnect_bio_async: surround is NULL");
        return false;
    }

    if (!surround->bio_async_enabled) {
        return true;
    }

    if (surround->bio_ctx) {
        bio_router_unregister_module(surround->bio_ctx);
        surround->bio_ctx = NULL;
    }

    surround->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return true;
}

bool cortical_surround_is_bio_async_connected(const cortical_surround_t* surround) {
    if (!surround) {
        return false;
    }
    return surround->bio_async_enabled;
}
