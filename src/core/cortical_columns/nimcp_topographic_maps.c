/**
 * @file nimcp_topographic_maps.c
 * @brief Implementation of topographic mapping for cortical organization
 *
 * WHAT: Complete implementation of retinotopic, tonotopic, and somatotopic
 *       mappings with mathematical transforms and receptive field calculations.
 *
 * WHY: Provides biologically-realistic spatial organization of cortical columns
 *      with non-uniform magnification matching sensory cortex properties.
 *
 * HOW: Implements forward/inverse transforms, activity projection, and
 *      neighborhood queries using type-specific mathematical algorithms.
 *
 * @version 1.0.0
 * @date 2025-01-25
 * @author NIMCP Development Team
 */

#include "core/cortical_columns/nimcp_topographic_maps.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
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
#include <pthread.h>

#define LOG_MODULE "topographic_maps"

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(topographic_maps)

/* Forward declarations for helper functions used before definition */
static uint32_t coords_to_column_id(uint32_t x, uint32_t y,
                                     uint32_t width, uint32_t height);
static void column_id_to_coords(uint32_t column_id, uint32_t width,
                                 uint32_t* out_x, uint32_t* out_y);

//=============================================================================
// Bio-Async Module Context (Thread-Safe Initialization)
//=============================================================================

static bio_module_context_t bio_ctx = NULL;
static bool bio_async_enabled = false;
static pthread_once_t bio_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t bio_cleanup_mutex = NIMCP_MUTEX_INITIALIZER;

static void topographic_maps_bio_init_impl(void) {
    if (!bio_router_is_initialized()) {
        return;
    }

    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_CORTICAL_TOPOGRAPHIC,
        .module_name = "topographic_maps",
        .inbox_capacity = 128,
        .user_data = NULL
    };

    bio_ctx = bio_router_register_module(&bio_info);
    if (bio_ctx) {
        bio_async_enabled = true;
        LOG_INFO(LOG_MODULE, "Bio-async registered for topographic_maps module");
    }
}

__attribute__((constructor))
static void topographic_maps_bio_init(void) {
    pthread_once(&bio_init_once, topographic_maps_bio_init_impl);
}

__attribute__((destructor))
static void topographic_maps_bio_cleanup(void) {
    nimcp_mutex_lock(&bio_cleanup_mutex);
    if (bio_async_enabled && bio_ctx) {
        bio_router_unregister_module(bio_ctx);
        bio_ctx = NULL;
        bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for topographic_maps module");
    }
    nimcp_mutex_unlock(&bio_cleanup_mutex);
}

/* ============================================================================
 * Logging Macros
 * ========================================================================== */

#define TOPO_LOG_ERROR(...) LOG_ERROR(LOG_MODULE, __VA_ARGS__)
#define TOPO_LOG_WARN(...) LOG_WARN(LOG_MODULE, __VA_ARGS__)
#define TOPO_LOG_INFO(...) LOG_INFO(LOG_MODULE, __VA_ARGS__)
#define TOPO_LOG_DEBUG(...) LOG_DEBUG(LOG_MODULE, __VA_ARGS__)

/* ============================================================================
 * Constants
 * ========================================================================== */

#define TOPOGRAPHIC_MIN_MAGNIFICATION 0.01f
#define TOPOGRAPHIC_MAX_MAGNIFICATION 100.0f
#define TOPOGRAPHIC_DEFAULT_RF_SIZE 1.0f
#include "constants/nimcp_constants.h"
#define TOPOGRAPHIC_EPSILON NIMCP_EPSILON_NUMERICAL

/* Mathematical constants */

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

/* ============================================================================
 * Internal Data Structures
 * ========================================================================== */

/**
 * WHAT: Internal representation of topographic map
 * WHY: Encapsulates all state and allows thread-safe access
 * HOW: Contains configuration, cached values, and mutex
 */
struct topographic_map {
    topographic_map_config_t config;    /**< Configuration parameters */
    nimcp_platform_mutex_t* mutex;      /**< Thread safety */

    /* Cortical grid */
    uint32_t cortical_width;            /**< Number of columns (x-axis) */
    uint32_t cortical_height;           /**< Number of columns (y-axis) */
    uint32_t total_columns;             /**< Total number of columns */

    /* Cached transform parameters */
    float* column_rf_centers;           /**< Receptive field centers per column */
    float* column_rf_sizes;             /**< Receptive field sizes per column */
    float* column_magnifications;       /**< Magnification factors per column */

    /* Type-specific cached data */
    bool cache_valid;                   /**< Whether cache is initialized */
    float transform_scale_x;            /**< X-axis scaling factor */
    float transform_scale_y;            /**< Y-axis scaling factor */
    float transform_offset_x;           /**< X-axis offset */
    float transform_offset_y;           /**< Y-axis offset */
};

/* ============================================================================
 * Forward Declarations (Internal Functions)
 * ========================================================================== */

static bool topographic_initialize_cache(topographic_map_t* map);
static void topographic_free_cache(topographic_map_t* map);

/* Retinotopic helpers */
static void retinotopic_input_to_cortex(topographic_map_t* map,
    const float* input, float* cortical);
static void retinotopic_cortex_to_input(topographic_map_t* map,
    const float* cortical, float* input);
static float retinotopic_get_magnification(topographic_map_t* map,
    const float* input);

/* Tonotopic helpers */
static void tonotopic_input_to_cortex(topographic_map_t* map,
    const float* input, float* cortical);
static void tonotopic_cortex_to_input(topographic_map_t* map,
    const float* cortical, float* input);
static float tonotopic_get_magnification(topographic_map_t* map,
    const float* input);

/* Somatotopic helpers */
static void somatotopic_input_to_cortex(topographic_map_t* map,
    const float* input, float* cortical);
static void somatotopic_cortex_to_input(topographic_map_t* map,
    const float* cortical, float* input);
static float somatotopic_get_magnification(topographic_map_t* map,
    const float* input);

/* Utility helpers */
/**
 * WHAT: Creates a topographic map from configuration
 * WHY: Initialize mapping structure with specified parameters
 * HOW: Allocates memory, initializes type-specific data structures
 */
topographic_map_t* topographic_map_create(const topographic_map_config_t* config) {
    if (!config) {
        TOPO_LOG_ERROR("[TopographicMaps] NULL config in topographic_map_create");
        return NULL;
    }

    /* Allocate map structure */
    topographic_map_t* map = (topographic_map_t*)nimcp_calloc(1, sizeof(topographic_map_t));
    if (!map) {
        TOPO_LOG_ERROR("[TopographicMaps] Failed to allocate topographic map");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "topographic_map_create: allocation failed");
        return NULL;
    }

    /* Copy configuration */
    map->config = *config;

    /* Deep copy somatotopic regions if present */
    if (config->type == TOPOGRAPHIC_SOMATOTOPIC && config->somatotopic.regions &&
        config->somatotopic.num_regions > 0) {
        size_t regions_size = sizeof(somatotopic_region_t) * config->somatotopic.num_regions;
        map->config.somatotopic.regions = (somatotopic_region_t*)nimcp_malloc(regions_size);
        if (!map->config.somatotopic.regions) {
            nimcp_free(map);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "topographic_map_create: regions allocation failed");
            return NULL;
        }
        memcpy(map->config.somatotopic.regions, config->somatotopic.regions, regions_size);
    }

    /* Compute cortical grid dimensions */
    float cortical_width_f = config->cortical_range[1] - config->cortical_range[0];
    float cortical_height_f = config->cortical_range[3] - config->cortical_range[2];
    map->cortical_width = (cortical_width_f > 0.0f) ? (uint32_t)cortical_width_f : 1;
    map->cortical_height = (cortical_height_f > 0.0f) ? (uint32_t)cortical_height_f : 1;
    map->total_columns = map->cortical_width * map->cortical_height;

    /* Compute transform parameters */
    float input_range_x = config->input_range[1] - config->input_range[0];
    float input_range_y = (config->input_dims >= 2) ?
                          config->input_range[3] - config->input_range[2] : 1.0f;
    map->transform_scale_x = (input_range_x > TOPOGRAPHIC_EPSILON) ?
                              cortical_width_f / input_range_x : 1.0f;
    map->transform_scale_y = (input_range_y > TOPOGRAPHIC_EPSILON) ?
                              cortical_height_f / input_range_y : 1.0f;
    map->transform_offset_x = config->cortical_range[0];
    map->transform_offset_y = config->cortical_range[2];

    /* Create mutex */
    map->mutex = nimcp_malloc(sizeof(nimcp_platform_mutex_t));
    if (!map->mutex) {
        /* Free somatotopic regions if we deep-copied them */
        if (config->type == TOPOGRAPHIC_SOMATOTOPIC && map->config.somatotopic.regions) {
            nimcp_free(map->config.somatotopic.regions);
        }
        nimcp_free(map);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "topographic_map_create: mutex allocation failed");
        return NULL;
    }
    nimcp_platform_mutex_init(map->mutex, false);

    /* Initialize cache */
    map->cache_valid = false;
    topographic_initialize_cache(map);

    TOPO_LOG_INFO("[TopographicMaps] Created %s map: %ux%u (%u columns)",
                  config->type == TOPOGRAPHIC_RETINOTOPIC ? "retinotopic" :
                  config->type == TOPOGRAPHIC_TONOTOPIC ? "tonotopic" :
                  config->type == TOPOGRAPHIC_SOMATOTOPIC ? "somatotopic" : "custom",
                  map->cortical_width, map->cortical_height, map->total_columns);

    return map;
}

/**
 * WHAT: Destroys a topographic map and frees resources
 * WHY: Prevent memory leaks
 * HOW: Frees all allocated memory in reverse order
 */
void topographic_map_destroy(topographic_map_t* map)
{
    /* Guard: NULL is safe */
    if (!map) {
        return;
    }

    /* Free cache */
    topographic_free_cache(map);

    /* Free somatotopic regions if present */
    if (map->config.type == TOPOGRAPHIC_SOMATOTOPIC &&
        map->config.somatotopic.regions) {
        nimcp_free(map->config.somatotopic.regions);
    }

    /* Destroy mutex */
    if (map->mutex) {
        nimcp_platform_mutex_destroy(map->mutex);
        nimcp_free(map->mutex);
        map->mutex = NULL;
    }

    /* Free map structure */
    nimcp_free(map);

    TOPO_LOG_DEBUG("[TopographicMaps] Destroyed topographic map");
}

/* ============================================================================
 * Specialized Constructors
 * ========================================================================== */

/**
 * WHAT: Creates a retinotopic map for visual cortex (V1)
 * WHY: Convenience function for common vision case
 * HOW: Fills in config structure with retinotopic parameters
 */
topographic_map_t* topographic_map_create_retinotopic(
    const retinotopic_params_t* params,
    uint32_t cortical_width,
    uint32_t cortical_height)
{
    /* Guard: Validate inputs */
    if (!params || cortical_width == 0 || cortical_height == 0) {
        TOPO_LOG_ERROR("[TopographicMaps] Invalid retinotopic parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topographic_map_create_retinotopic: params is NULL");
        return NULL;
    }

    /* Build config */
    topographic_map_config_t config;
    memset(&config, 0, sizeof(config));

    config.type = TOPOGRAPHIC_RETINOTOPIC;
    config.input_dims = 2;  /* (eccentricity, angle) */
    config.cortical_dims = 2;

    /* Input range: eccentricity [0, max_ecc], angle [0, 2π] */
    config.input_range[0] = 0.0F;
    config.input_range[1] = params->foveal_radius * 10.0F; /* 10x foveal radius */
    config.input_range[2] = 0.0F;
    config.input_range[3] = params->angle_coverage;

    /* Cortical range */
    config.cortical_range[0] = 0.0F;
    config.cortical_range[1] = (float)cortical_width;
    config.cortical_range[2] = 0.0F;
    config.cortical_range[3] = (float)cortical_height;

    config.magnification_factor = params->cortical_magnification;
    memcpy(&config.retinotopic, params, sizeof(retinotopic_params_t));

    return topographic_map_create(&config);
}

/**
 * WHAT: Creates a tonotopic map for auditory cortex (A1)
 * WHY: Convenience function for frequency mapping
 * HOW: Fills in config structure with tonotopic parameters
 */
topographic_map_t* topographic_map_create_tonotopic(
    const tonotopic_params_t* params,
    uint32_t num_frequency_bands)
{
    /* Guard: Validate inputs */
    if (!params || num_frequency_bands == 0) {
        TOPO_LOG_ERROR("[TopographicMaps] Invalid tonotopic parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topographic_map_create_tonotopic: params is NULL");
        return NULL;
    }

    /* Build config */
    topographic_map_config_t config;
    memset(&config, 0, sizeof(config));

    config.type = TOPOGRAPHIC_TONOTOPIC;
    config.input_dims = 1;  /* Frequency */
    config.cortical_dims = 2; /* But we'll use 1D layout in 2D grid */

    /* Input range: frequency [min, max] */
    config.input_range[0] = params->min_frequency;
    config.input_range[1] = params->max_frequency;

    /* Cortical range: linear strip */
    config.cortical_range[0] = 0.0F;
    config.cortical_range[1] = (float)num_frequency_bands;
    config.cortical_range[2] = 0.0F;
    config.cortical_range[3] = 1.0F;

    config.magnification_factor = 1.0F;
    memcpy(&config.tonotopic, params, sizeof(tonotopic_params_t));

    return topographic_map_create(&config);
}

/**
 * WHAT: Creates a somatotopic map for somatosensory cortex (S1)
 * WHY: Convenience function for body surface mapping
 * HOW: Allocates regions array for later population
 */
topographic_map_t* topographic_map_create_somatotopic(uint32_t num_body_regions)
{
    /* Guard: Validate input */
    if (num_body_regions == 0) {
        TOPO_LOG_ERROR("[TopographicMaps] Invalid number of body regions");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topographic_map_create_somatotopic: num_body_regions is zero");
        return NULL;
    }

    /* Build config */
    topographic_map_config_t config;
    memset(&config, 0, sizeof(config));

    config.type = TOPOGRAPHIC_SOMATOTOPIC;
    config.input_dims = 1;  /* Body position along axis */
    config.cortical_dims = 2;

    /* Allocate regions array */
    config.somatotopic.regions = (somatotopic_region_t*)
        nimcp_malloc(sizeof(somatotopic_region_t) * num_body_regions);

    if (!config.somatotopic.regions) {
        TOPO_LOG_ERROR("[TopographicMaps] Failed to allocate regions");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "topographic_map_create_somatotopic: config is NULL");
        return NULL;
    }

    config.somatotopic.num_regions = num_body_regions;
    config.somatotopic.total_cortical_extent = 100.0F; /* Default 100mm */
    config.somatotopic.is_bilateral = false;

    /* Default input/cortical ranges (will be updated as regions are added) */
    config.input_range[0] = 0.0F;
    config.input_range[1] = 1.0F;
    config.cortical_range[0] = 0.0F;
    config.cortical_range[1] = 100.0F;
    config.cortical_range[2] = 0.0F;
    config.cortical_range[3] = 10.0F;

    config.magnification_factor = 1.0F;

    topographic_map_t* map = topographic_map_create(&config);

    /* topographic_map_create deep-copies regions, so free the local allocation */
    nimcp_free(config.somatotopic.regions);

    return map;
}

/* ============================================================================
 * Coordinate Mapping Functions
 * ========================================================================== */

/**
 * WHAT: Maps input coordinates to cortical coordinates (forward mapping)
 * WHY: Determine cortical location for sensory input
 * HOW: Dispatches to type-specific transform function
 */
void topographic_map_input_to_cortex(
    topographic_map_t* map,
    const float* input_coords,
    float* cortical_coords,
    uint32_t num_points)
{
    /* Guard: Validate inputs */
    if (!map || !input_coords || !cortical_coords || num_points == 0) {
        TOPO_LOG_ERROR("[TopographicMaps] Invalid input_to_cortex parameters");
        return;
    }

    /* Process pending bio-async messages */
    if (bio_async_enabled && bio_ctx) {
        bio_router_process_inbox(bio_ctx, 5);
    }

    nimcp_platform_mutex_lock(map->mutex);

    /* Process each point */
    for (uint32_t i = 0; i < num_points; i++) {
        const float* in = &input_coords[i * map->config.input_dims];
        float* out = &cortical_coords[i * map->config.cortical_dims];

        /* Dispatch to type-specific function */
        switch (map->config.type) {
            case TOPOGRAPHIC_RETINOTOPIC:
                retinotopic_input_to_cortex(map, in, out);
                break;
            case TOPOGRAPHIC_TONOTOPIC:
                tonotopic_input_to_cortex(map, in, out);
                break;
            case TOPOGRAPHIC_SOMATOTOPIC:
                somatotopic_input_to_cortex(map, in, out);
                break;
            default:
                /* Linear mapping as fallback */
                out[0] = map->transform_offset_x +
                    (in[0] - map->config.input_range[0]) * map->transform_scale_x;
                if (map->config.cortical_dims > 1 && map->config.input_dims > 1) {
                    out[1] = map->transform_offset_y +
                        (in[1] - map->config.input_range[2]) * map->transform_scale_y;
                } else if (map->config.cortical_dims > 1) {
                    out[1] = 0.0F;
                }
                break;
        }
    }

    nimcp_platform_mutex_unlock(map->mutex);
}

/**
 * WHAT: Maps cortical coordinates back to input coordinates (inverse mapping)
 * WHY: Determine receptive field centers from cortical position
 * HOW: Dispatches to type-specific inverse transform
 */
void topographic_map_cortex_to_input(
    topographic_map_t* map,
    const float* cortical_coords,
    float* input_coords,
    uint32_t num_points)
{
    /* Guard: Validate inputs */
    if (!map || !cortical_coords || !input_coords || num_points == 0) {
        TOPO_LOG_ERROR("[TopographicMaps] Invalid cortex_to_input parameters");
        return;
    }

    nimcp_platform_mutex_lock(map->mutex);

    /* Process each point */
    for (uint32_t i = 0; i < num_points; i++) {
        const float* in = &cortical_coords[i * map->config.cortical_dims];
        float* out = &input_coords[i * map->config.input_dims];

        /* Dispatch to type-specific function */
        switch (map->config.type) {
            case TOPOGRAPHIC_RETINOTOPIC:
                retinotopic_cortex_to_input(map, in, out);
                break;
            case TOPOGRAPHIC_TONOTOPIC:
                tonotopic_cortex_to_input(map, in, out);
                break;
            case TOPOGRAPHIC_SOMATOTOPIC:
                somatotopic_cortex_to_input(map, in, out);
                break;
            default:
                /* Linear inverse mapping */
                out[0] = map->config.input_range[0] +
                    (in[0] - map->transform_offset_x) / map->transform_scale_x;
                if (map->config.input_dims > 1 && map->config.cortical_dims > 1) {
                    out[1] = map->config.input_range[2] +
                        (in[1] - map->transform_offset_y) / map->transform_scale_y;
                }
                break;
        }
    }

    nimcp_platform_mutex_unlock(map->mutex);
}

/**
 * WHAT: Gets the cortical column ID for a given input coordinate
 * WHY: Map sensory input to specific column for processing
 * HOW: Transforms coordinate and quantizes to column grid
 */
uint32_t topographic_map_get_column_for_input(
    topographic_map_t* map,
    const float* input_coords)
{
    /* Guard: Validate inputs */
    if (!map || !input_coords) {
        TOPO_LOG_ERROR("[TopographicMaps] Invalid get_column_for_input parameters");
        return UINT32_MAX;
    }

    /* Transform to cortical coordinates */
    float cortical[2];
    topographic_map_input_to_cortex(map, input_coords, cortical, 1);

    /* Convert to grid coordinates */
    nimcp_platform_mutex_lock(map->mutex);

    float norm_x = (cortical[0] - map->config.cortical_range[0]) /
        (map->config.cortical_range[1] - map->config.cortical_range[0]);
    float norm_y = (cortical[1] - map->config.cortical_range[2]) /
        (map->config.cortical_range[3] - map->config.cortical_range[2]);

    uint32_t x = (uint32_t)(norm_x * map->cortical_width);
    uint32_t y = (uint32_t)(norm_y * map->cortical_height);

    /* Clamp to valid range */
    if (x >= map->cortical_width) x = map->cortical_width - 1;
    if (y >= map->cortical_height) y = map->cortical_height - 1;

    uint32_t column_id = coords_to_column_id(x, y,
        map->cortical_width, map->cortical_height);

    nimcp_platform_mutex_unlock(map->mutex);

    return column_id;
}

/* ============================================================================
 * Receptive Field Functions
 * ========================================================================== */

/**
 * WHAT: Computes receptive field center and size for a cortical column
 * WHY: Determine what input region each column responds to
 * HOW: Uses cached values if available, otherwise computes on-the-fly
 */
void topographic_map_get_receptive_field(
    topographic_map_t* map,
    uint32_t column_id,
    float* rf_center,
    float* rf_size)
{
    /* Guard: Validate inputs */
    if (!map || column_id >= map->total_columns) {
        TOPO_LOG_ERROR("[TopographicMaps] Invalid receptive field query");
        return;
    }

    nimcp_platform_mutex_lock(map->mutex);

    /* Use cached values if available */
    if (map->cache_valid && map->column_rf_centers && rf_center) {
        uint32_t offset = column_id * map->config.input_dims;
        memcpy(rf_center, &map->column_rf_centers[offset],
            map->config.input_dims * sizeof(float));
    } else if (rf_center) {
        /* Compute on-the-fly */
        uint32_t x, y;
        column_id_to_coords(column_id, map->cortical_width, &x, &y);

        /* Convert to cortical coordinates */
        float cortical[2];
        cortical[0] = map->config.cortical_range[0] +
            ((float)x + 0.5F) / map->cortical_width *
            (map->config.cortical_range[1] - map->config.cortical_range[0]);
        cortical[1] = map->config.cortical_range[2] +
            ((float)y + 0.5F) / map->cortical_height *
            (map->config.cortical_range[3] - map->config.cortical_range[2]);

        /* Inverse transform (unlock/relock handled by function) */
        nimcp_platform_mutex_unlock(map->mutex);
        topographic_map_cortex_to_input(map, cortical, rf_center, 1);
        nimcp_platform_mutex_lock(map->mutex);
    }

    /* RF size */
    if (rf_size) {
        if (map->cache_valid && map->column_rf_sizes) {
            *rf_size = map->column_rf_sizes[column_id];
        } else {
            /* Estimate from magnification */
            if (rf_center && map->column_rf_centers) {
                float mag = topographic_map_get_magnification(map, rf_center);
                *rf_size = (mag > TOPOGRAPHIC_EPSILON) ?
                    (1.0F / mag) : TOPOGRAPHIC_DEFAULT_RF_SIZE;
            } else {
                *rf_size = TOPOGRAPHIC_DEFAULT_RF_SIZE;
            }
        }
    }

    nimcp_platform_mutex_unlock(map->mutex);
}

/**
 * WHAT: Computes cortical magnification factor at input location
 * WHY: Determine cortical area per input area (spatial resolution)
 * HOW: Dispatches to type-specific magnification function
 */
float topographic_map_get_magnification(
    topographic_map_t* map,
    const float* input_coords)
{
    /* Guard: Validate inputs */
    if (!map || !input_coords) {
        TOPO_LOG_ERROR("[TopographicMaps] Invalid magnification query");
        return 0.0F;
    }

    nimcp_platform_mutex_lock(map->mutex);

    float mag = 0.0F;

    /* Dispatch to type-specific function */
    switch (map->config.type) {
        case TOPOGRAPHIC_RETINOTOPIC:
            mag = retinotopic_get_magnification(map, input_coords);
            break;
        case TOPOGRAPHIC_TONOTOPIC:
            mag = tonotopic_get_magnification(map, input_coords);
            break;
        case TOPOGRAPHIC_SOMATOTOPIC:
            mag = somatotopic_get_magnification(map, input_coords);
            break;
        default:
            mag = map->config.magnification_factor;
            break;
    }

    nimcp_platform_mutex_unlock(map->mutex);

    return mag;
}

/* ============================================================================
 * Activity Projection Functions
 * ========================================================================== */

/**
 * WHAT: Projects input activity pattern onto cortical surface
 * WHY: Transform sensory input into cortical activation pattern
 * HOW: Resamples using bilinear interpolation and magnification weighting
 */
void topographic_map_project_activity(
    topographic_map_t* map,
    const float* input_activity,
    uint32_t input_width,
    uint32_t input_height,
    float* cortical_activity,
    uint32_t cortical_width,
    uint32_t cortical_height)
{
    /* Guard: Validate inputs */
    if (!map || !input_activity || !cortical_activity) {
        TOPO_LOG_ERROR("[TopographicMaps] Invalid activity projection parameters");
        return;
    }

    /* Clear output */
    memset(cortical_activity, 0, cortical_width * cortical_height * sizeof(float));

    nimcp_platform_mutex_lock(map->mutex);

    /* For each cortical position */
    for (uint32_t cy = 0; cy < cortical_height; cy++) {
        for (uint32_t cx = 0; cx < cortical_width; cx++) {
            /* Convert to cortical coordinates */
            float cortical[2];
            cortical[0] = map->config.cortical_range[0] +
                ((float)cx + 0.5F) / cortical_width *
                (map->config.cortical_range[1] - map->config.cortical_range[0]);
            cortical[1] = map->config.cortical_range[2] +
                ((float)cy + 0.5F) / cortical_height *
                (map->config.cortical_range[3] - map->config.cortical_range[2]);

            /* Get corresponding input position */
            float input_pos[3];
            nimcp_platform_mutex_unlock(map->mutex);
            topographic_map_cortex_to_input(map, cortical, input_pos, 1);
            nimcp_platform_mutex_lock(map->mutex);

            /* Convert to input grid coordinates (normalized) */
            float input_x_norm = (input_pos[0] - map->config.input_range[0]) /
                (map->config.input_range[1] - map->config.input_range[0]);
            float input_y_norm = (map->config.input_dims > 1) ?
                (input_pos[1] - map->config.input_range[2]) /
                (map->config.input_range[3] - map->config.input_range[2]) : 0.0F;

            float input_x = input_x_norm * input_width;
            float input_y = input_y_norm * input_height;

            /* Bilinear interpolation */
            if (input_x >= 0 && input_x < input_width - 1 &&
                input_y >= 0 && input_y < input_height - 1) {

                uint32_t ix0 = (uint32_t)input_x;
                uint32_t iy0 = (uint32_t)input_y;
                uint32_t ix1 = ix0 + 1;
                uint32_t iy1 = iy0 + 1;

                float fx = input_x - ix0;
                float fy = input_y - iy0;

                float v00 = input_activity[iy0 * input_width + ix0];
                float v10 = input_activity[iy0 * input_width + ix1];
                float v01 = input_activity[iy1 * input_width + ix0];
                float v11 = input_activity[iy1 * input_width + ix1];

                float value = (1 - fx) * (1 - fy) * v00 +
                             fx * (1 - fy) * v10 +
                             (1 - fx) * fy * v01 +
                             fx * fy * v11;

                cortical_activity[cy * cortical_width + cx] = value;
            }
        }
    }

    nimcp_platform_mutex_unlock(map->mutex);
}

/* ============================================================================
 * Column Assignment Functions
 * ========================================================================== */

/**
 * WHAT: Assigns input coordinates to all cortical columns
 * WHY: Pre-compute receptive field centers for all columns
 * HOW: Iterates cortical grid and applies inverse mapping
 */
void topographic_map_assign_columns(
    topographic_map_t* map,
    uint32_t* column_ids,
    float* column_positions,
    uint32_t num_columns)
{
    /* Guard: Validate inputs */
    if (!map || !column_positions || num_columns != map->total_columns) {
        TOPO_LOG_ERROR("[TopographicMaps] Invalid column assignment parameters");
        return;
    }

    /* Iterate all columns */
    for (uint32_t i = 0; i < num_columns; i++) {
        if (column_ids) {
            column_ids[i] = i;
        }

        /* Get RF center from cache or compute */
        float* rf_center = &column_positions[i * map->config.input_dims];
        topographic_map_get_receptive_field(map, i, rf_center, NULL);
    }
}

/* ============================================================================
 * Neighborhood Functions
 * ========================================================================== */

/**
 * WHAT: Finds neighboring columns within cortical distance
 * WHY: Determine lateral connectivity patterns
 * HOW: Searches cortical grid using Euclidean distance
 */
uint32_t topographic_map_get_neighbors(
    topographic_map_t* map,
    uint32_t column_id,
    float radius,
    uint32_t* neighbor_ids,
    uint32_t max_neighbors)
{
    /* Guard: Validate inputs */
    if (!map || column_id >= map->total_columns || !neighbor_ids || max_neighbors == 0) {
        TOPO_LOG_ERROR("[TopographicMaps] Invalid neighbor query");
        return 0;
    }

    nimcp_platform_mutex_lock(map->mutex);

    /* Get center column coordinates */
    uint32_t cx, cy;
    column_id_to_coords(column_id, map->cortical_width, &cx, &cy);

    /* Convert radius to grid units */
    float cortical_width_f = map->config.cortical_range[1] - map->config.cortical_range[0];
    float cortical_height_f = map->config.cortical_range[3] - map->config.cortical_range[2];

    float radius_x = radius / cortical_width_f * map->cortical_width;
    float radius_y = radius / cortical_height_f * map->cortical_height;
    float radius_sq = radius_x * radius_x + radius_y * radius_y;

    /* Search window */
    uint32_t x_min = (cx > (uint32_t)radius_x) ? (cx - (uint32_t)radius_x) : 0;
    uint32_t x_max = (cx + (uint32_t)radius_x < map->cortical_width) ?
        (cx + (uint32_t)radius_x) : (map->cortical_width - 1);
    uint32_t y_min = (cy > (uint32_t)radius_y) ? (cy - (uint32_t)radius_y) : 0;
    uint32_t y_max = (cy + (uint32_t)radius_y < map->cortical_height) ?
        (cy + (uint32_t)radius_y) : (map->cortical_height - 1);

    /* Find neighbors */
    uint32_t count = 0;
    for (uint32_t y = y_min; y <= y_max && count < max_neighbors; y++) {
        for (uint32_t x = x_min; x <= x_max && count < max_neighbors; x++) {
            uint32_t nid = coords_to_column_id(x, y, map->cortical_width, map->cortical_height);

            /* Skip self */
            if (nid == column_id) {
                continue;
            }

            /* Check distance */
            float dx = (float)x - (float)cx;
            float dy = (float)y - (float)cy;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq <= radius_sq) {
                neighbor_ids[count++] = nid;
            }
        }
    }

    nimcp_platform_mutex_unlock(map->mutex);

    return count;
}

/* ============================================================================
 * Statistics and Diagnostics
 * ========================================================================== */

/**
 * WHAT: Computes statistics about the topographic mapping
 * WHY: Validate mapping and analyze cortical organization
 * HOW: Samples mapping across grid and aggregates metrics
 */
void topographic_map_get_stats(
    topographic_map_t* map,
    topographic_stats_t* stats)
{
    /* Guard: Validate inputs */
    if (!map || !stats) {
        TOPO_LOG_ERROR("[TopographicMaps] Invalid stats query");
        return;
    }

    memset(stats, 0, sizeof(topographic_stats_t));

    nimcp_platform_mutex_lock(map->mutex);

    stats->num_columns = map->total_columns;
    stats->min_magnification = FLT_MAX;
    stats->max_magnification = 0.0F;
    stats->mean_magnification = 0.0F;

    /* Sample magnification across grid */
    uint32_t sample_count = 0;
    for (uint32_t i = 0; i < map->total_columns; i += 10) { /* Sample every 10th */
        float rf_center[3];
        nimcp_platform_mutex_unlock(map->mutex);
        topographic_map_get_receptive_field(map, i, rf_center, NULL);

        float mag = topographic_map_get_magnification(map, rf_center);
        nimcp_platform_mutex_lock(map->mutex);

        if (mag > stats->max_magnification) stats->max_magnification = mag;
        if (mag < stats->min_magnification) stats->min_magnification = mag;
        stats->mean_magnification += mag;
        sample_count++;
    }

    if (sample_count > 0) {
        stats->mean_magnification /= sample_count;
    }

    /* Cortical area */
    float width = map->config.cortical_range[1] - map->config.cortical_range[0];
    float height = map->config.cortical_range[3] - map->config.cortical_range[2];
    stats->total_cortical_area = width * height;
    stats->coverage_ratio = 1.0F; /* Assume full coverage */

    nimcp_platform_mutex_unlock(map->mutex);
}

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

/**
 * WHAT: Adds a body region to a somatotopic map
 * WHY: Define homunculus structure region by region
 * HOW: Stores region in internal array
 */
bool topographic_map_add_body_region(
    topographic_map_t* map,
    const somatotopic_region_t* region)
{
    /* Guard: Validate inputs */
    if (!map || !region || map->config.type != TOPOGRAPHIC_SOMATOTOPIC) {
        TOPO_LOG_ERROR("[TopographicMaps] Invalid body region");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "topographic_map_add_body_region: required parameter is NULL (map, region)");
        return false;
    }

    nimcp_platform_mutex_lock(map->mutex);

    /* Find first empty slot */
    for (uint32_t i = 0; i < map->config.somatotopic.num_regions; i++) {
        if (map->config.somatotopic.regions[i].input_start == 0.0F &&
            map->config.somatotopic.regions[i].input_end == 0.0F) {
            memcpy(&map->config.somatotopic.regions[i], region,
                sizeof(somatotopic_region_t));
            nimcp_platform_mutex_unlock(map->mutex);
            return true;
        }
    }

    nimcp_platform_mutex_unlock(map->mutex);

    TOPO_LOG_ERROR("[TopographicMaps] No free region slots");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topographic_map_add_body_region: operation failed");
    return false;
}

/**
 * WHAT: Validates a topographic map configuration
 * WHY: Catch configuration errors before creating map
 * HOW: Checks ranges, parameters, and type-specific constraints
 */
bool topographic_map_validate_config(const topographic_map_config_t* config)
{
    /* Guard: NULL check */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "topographic_map_validate_config: config is NULL");
        return false;
    }

    /* Check dimensions */
    if (config->input_dims == 0 || config->input_dims > 3 ||
        config->cortical_dims == 0 || config->cortical_dims > 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topographic_map_validate_config: invalid dimensions");
        return false;
    }

    /* Check ranges */
    if (config->input_range[1] <= config->input_range[0]) {
        return false;
    }

    if (config->cortical_range[1] <= config->cortical_range[0] ||
        config->cortical_range[3] <= config->cortical_range[2]) {
        return false;
    }

    /* Type-specific validation */
    switch (config->type) {
        case TOPOGRAPHIC_RETINOTOPIC:
            if (config->retinotopic.foveal_radius <= 0.0F ||
                config->retinotopic.cortical_magnification <= 0.0F ||
                config->retinotopic.log_polar_a < 0.0F) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topographic_map_validate_config: operation failed");
                return false;
            }
            break;

        case TOPOGRAPHIC_TONOTOPIC:
            if (config->tonotopic.min_frequency <= 0.0F ||
                config->tonotopic.max_frequency <= config->tonotopic.min_frequency) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topographic_map_validate_config: operation failed");
                return false;
            }
            break;

        case TOPOGRAPHIC_SOMATOTOPIC:
            if (config->somatotopic.num_regions == 0) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topographic_map_validate_config: config->somatotopic.num_regions is zero");
                return false;
            }
            break;

        default:
            break;
    }

    return true;
}

/**
 * WHAT: Gets the cortical dimensions of the map
 * WHY: Query map structure for array allocation
 * HOW: Returns stored width/height values
 */
void topographic_map_get_dimensions(
    topographic_map_t* map,
    uint32_t* width,
    uint32_t* height)
{
    /* Guard: Validate input */
    if (!map) {
        return;
    }

    nimcp_platform_mutex_lock(map->mutex);

    if (width) *width = map->cortical_width;
    if (height) *height = map->cortical_height;

    nimcp_platform_mutex_unlock(map->mutex);
}

/* ============================================================================
 * Internal Helper Functions
 * ========================================================================== */

/**
 * WHAT: Initializes cached data structures
 * WHY: Pre-compute receptive fields for fast lookup
 * HOW: Allocates arrays and populates with inverse mapping
 */
static bool topographic_initialize_cache(topographic_map_t* map)
{
    /* Guard: Already initialized */
    if (map->cache_valid) {
        return true;
    }

    /* Allocate cache arrays */
    size_t rf_center_size = map->total_columns * map->config.input_dims * sizeof(float);
    map->column_rf_centers = (float*)nimcp_malloc(rf_center_size);

    map->column_rf_sizes = (float*)nimcp_malloc(map->total_columns * sizeof(float));

    map->column_magnifications = (float*)nimcp_malloc(map->total_columns * sizeof(float));

    if (!map->column_rf_centers || !map->column_rf_sizes || !map->column_magnifications) {
        topographic_free_cache(map);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "topographic_initialize_cache: required parameter is NULL (map->column_rf_centers, map->column_rf_sizes, map->column_magnifications)");
        return false;
    }

    /* Populate cache */
    for (uint32_t i = 0; i < map->total_columns; i++) {
        uint32_t x, y;
        column_id_to_coords(i, map->cortical_width, &x, &y);

        /* Convert to cortical coordinates */
        float cortical[2];
        cortical[0] = map->config.cortical_range[0] +
            ((float)x + 0.5F) / map->cortical_width *
            (map->config.cortical_range[1] - map->config.cortical_range[0]);
        cortical[1] = map->config.cortical_range[2] +
            ((float)y + 0.5F) / map->cortical_height *
            (map->config.cortical_range[3] - map->config.cortical_range[2]);

        /* Get RF center via inverse mapping */
        float* rf_center = &map->column_rf_centers[i * map->config.input_dims];

        switch (map->config.type) {
            case TOPOGRAPHIC_RETINOTOPIC:
                retinotopic_cortex_to_input(map, cortical, rf_center);
                break;
            case TOPOGRAPHIC_TONOTOPIC:
                tonotopic_cortex_to_input(map, cortical, rf_center);
                break;
            case TOPOGRAPHIC_SOMATOTOPIC:
                somatotopic_cortex_to_input(map, cortical, rf_center);
                break;
            default:
                rf_center[0] = map->config.input_range[0] +
                    (cortical[0] - map->transform_offset_x) / map->transform_scale_x;
                if (map->config.input_dims > 1) {
                    rf_center[1] = map->config.input_range[2] +
                        (cortical[1] - map->transform_offset_y) / map->transform_scale_y;
                }
                break;
        }

        /* Compute magnification and RF size */
        float mag;
        switch (map->config.type) {
            case TOPOGRAPHIC_RETINOTOPIC:
                mag = retinotopic_get_magnification(map, rf_center);
                break;
            case TOPOGRAPHIC_TONOTOPIC:
                mag = tonotopic_get_magnification(map, rf_center);
                break;
            case TOPOGRAPHIC_SOMATOTOPIC:
                mag = somatotopic_get_magnification(map, rf_center);
                break;
            default:
                mag = map->config.magnification_factor;
                break;
        }

        map->column_magnifications[i] = mag;
        map->column_rf_sizes[i] = (mag > TOPOGRAPHIC_EPSILON) ?
            (1.0F / mag) : TOPOGRAPHIC_DEFAULT_RF_SIZE;
    }

    map->cache_valid = true;
    return true;
}

/**
 * WHAT: Frees cached data structures
 * WHY: Clean up memory on destroy
 * HOW: Frees arrays and marks cache invalid
 */
static void topographic_free_cache(topographic_map_t* map)
{
    if (map->column_rf_centers) {
        nimcp_free(map->column_rf_centers);
        map->column_rf_centers = NULL;
    }

    if (map->column_rf_sizes) {
        nimcp_free(map->column_rf_sizes);
        map->column_rf_sizes = NULL;
    }

    if (map->column_magnifications) {
        nimcp_free(map->column_magnifications);
        map->column_magnifications = NULL;
    }

    map->cache_valid = false;
}

/* ============================================================================
 * Retinotopic Transform Functions
 * ========================================================================== */

/**
 * WHAT: Forward retinotopic transform (visual input → cortex)
 * WHY: Implements log-polar mapping with foveal magnification
 * HOW: Applies cortical_x = k*log(r+a), cortical_y = k*θ
 */
static void retinotopic_input_to_cortex(
    topographic_map_t* map,
    const float* input,
    float* cortical)
{
    const retinotopic_params_t* params = &map->config.retinotopic;

    /* Input: (eccentricity, angle) */
    float eccentricity = input[0];
    float angle = (map->config.input_dims > 1) ? input[1] : 0.0F;

    /* Log-polar transform */
    float log_ecc = logf(eccentricity + params->log_polar_a);

    /* Map to cortical coordinates */
    float cortical_width = map->config.cortical_range[1] - map->config.cortical_range[0];
    float cortical_height = map->config.cortical_range[3] - map->config.cortical_range[2];

    float max_log_ecc = logf(map->config.input_range[1] + params->log_polar_a);
    float min_log_ecc = logf(params->log_polar_a);

    cortical[0] = map->config.cortical_range[0] +
        (log_ecc - min_log_ecc) / (max_log_ecc - min_log_ecc) * cortical_width;

    cortical[1] = map->config.cortical_range[2] +
        (angle / params->angle_coverage) * cortical_height;

    /* Apply aspect ratio */
    cortical[0] *= params->aspect_ratio;
}

/**
 * WHAT: Inverse retinotopic transform (cortex → visual input)
 * WHY: Determine receptive field centers in visual space
 * HOW: Inverts log-polar: r = exp(x/k) - a, θ = y/k
 */
static void retinotopic_cortex_to_input(
    topographic_map_t* map,
    const float* cortical,
    float* input)
{
    const retinotopic_params_t* params = &map->config.retinotopic;

    /* Normalize cortical position */
    float cortical_width = map->config.cortical_range[1] - map->config.cortical_range[0];
    float cortical_height = map->config.cortical_range[3] - map->config.cortical_range[2];

    float norm_x = (cortical[0] / params->aspect_ratio - map->config.cortical_range[0]) /
        cortical_width;
    float norm_y = (cortical[1] - map->config.cortical_range[2]) / cortical_height;

    /* Inverse log-polar */
    float max_log_ecc = logf(map->config.input_range[1] + params->log_polar_a);
    float min_log_ecc = logf(params->log_polar_a);

    float log_ecc = min_log_ecc + norm_x * (max_log_ecc - min_log_ecc);
    float eccentricity = expf(log_ecc) - params->log_polar_a;
    float angle = norm_y * params->angle_coverage;

    input[0] = nimcp_clampf(eccentricity, map->config.input_range[0], map->config.input_range[1]);
    if (map->config.input_dims > 1) {
        input[1] = nimcp_clampf(angle, map->config.input_range[2], map->config.input_range[3]);
    }
}

/**
 * WHAT: Computes retinotopic magnification factor
 * WHY: Fovea has much higher cortical representation than periphery
 * HOW: M = M₀ / (1 + E/E₂) where E is eccentricity
 */
static float retinotopic_get_magnification(
    topographic_map_t* map,
    const float* input)
{
    const retinotopic_params_t* params = &map->config.retinotopic;

    float eccentricity = input[0];

    /* Cortical magnification formula: M = M₀ / (1 + E/E₂) */
    float mag = params->cortical_magnification /
        (1.0F + eccentricity / params->eccentricity_half);

    return nimcp_clampf(mag, TOPOGRAPHIC_MIN_MAGNIFICATION, TOPOGRAPHIC_MAX_MAGNIFICATION);
}

/* ============================================================================
 * Tonotopic Transform Functions
 * ========================================================================== */

/**
 * WHAT: Forward tonotopic transform (frequency → cortex)
 * WHY: Implements logarithmic frequency mapping
 * HOW: x = log₂(f/f_min) / log₂(f_max/f_min)
 */
static void tonotopic_input_to_cortex(
    topographic_map_t* map,
    const float* input,
    float* cortical)
{
    const tonotopic_params_t* params = &map->config.tonotopic;

    float frequency = input[0];

    float cortical_width = map->config.cortical_range[1] - map->config.cortical_range[0];

    if (params->is_logarithmic) {
        /* Logarithmic mapping */
        float log_f = log2f(frequency / params->min_frequency);
        float log_range = log2f(params->max_frequency / params->min_frequency);

        cortical[0] = map->config.cortical_range[0] + (log_f / log_range) * cortical_width;
    } else {
        /* Linear mapping */
        float norm_f = (frequency - params->min_frequency) /
            (params->max_frequency - params->min_frequency);

        cortical[0] = map->config.cortical_range[0] + norm_f * cortical_width;
    }

    cortical[1] = map->config.cortical_range[2]; /* 1D strip */
}

/**
 * WHAT: Inverse tonotopic transform (cortex → frequency)
 * WHY: Determine characteristic frequency of cortical location
 * HOW: f = f_min × 2^(x × log₂(f_max/f_min))
 */
static void tonotopic_cortex_to_input(
    topographic_map_t* map,
    const float* cortical,
    float* input)
{
    const tonotopic_params_t* params = &map->config.tonotopic;

    float cortical_width = map->config.cortical_range[1] - map->config.cortical_range[0];
    float norm_x = (cortical[0] - map->config.cortical_range[0]) / cortical_width;

    if (params->is_logarithmic) {
        /* Inverse logarithmic */
        float log_range = log2f(params->max_frequency / params->min_frequency);
        float frequency = params->min_frequency * powf(2.0F, norm_x * log_range);

        input[0] = nimcp_clampf(frequency, params->min_frequency, params->max_frequency);
    } else {
        /* Inverse linear */
        float frequency = params->min_frequency +
            norm_x * (params->max_frequency - params->min_frequency);

        input[0] = nimcp_clampf(frequency, params->min_frequency, params->max_frequency);
    }
}

/**
 * WHAT: Computes tonotopic magnification factor
 * WHY: Constant Q mapping means magnification proportional to frequency
 * HOW: M = k / f (for log mapping)
 */
static float tonotopic_get_magnification(
    topographic_map_t* map,
    const float* input)
{
    const tonotopic_params_t* params = &map->config.tonotopic;

    float frequency = input[0];

    if (params->is_logarithmic) {
        /* For log mapping, magnification decreases with frequency */
        float mag = map->config.magnification_factor *
            params->min_frequency / (frequency + TOPOGRAPHIC_EPSILON);

        return nimcp_clampf(mag, TOPOGRAPHIC_MIN_MAGNIFICATION, TOPOGRAPHIC_MAX_MAGNIFICATION);
    } else {
        /* Linear mapping has constant magnification */
        return map->config.magnification_factor;
    }
}

/* ============================================================================
 * Somatotopic Transform Functions
 * ========================================================================== */

/**
 * WHAT: Forward somatotopic transform (body position → cortex)
 * WHY: Implements piecewise homunculus mapping
 * HOW: Finds matching region and applies linear interpolation
 */
static void somatotopic_input_to_cortex(
    topographic_map_t* map,
    const float* input,
    float* cortical)
{
    const somatotopic_params_t* params = &map->config.somatotopic;

    float body_pos = input[0];

    /* Find matching region */
    for (uint32_t i = 0; i < params->num_regions; i++) {
        const somatotopic_region_t* region = &params->regions[i];

        if (body_pos >= region->input_start && body_pos <= region->input_end) {
            /* Linear interpolation within region */
            float t = (body_pos - region->input_start) /
                (region->input_end - region->input_start + TOPOGRAPHIC_EPSILON);

            cortical[0] = region->cortical_start +
                t * (region->cortical_end - region->cortical_start);
            cortical[1] = map->config.cortical_range[2];
            return;
        }
    }

    /* Fallback: linear mapping */
    cortical[0] = map->transform_offset_x +
        (body_pos - map->config.input_range[0]) * map->transform_scale_x;
    cortical[1] = map->config.cortical_range[2];
}

/**
 * WHAT: Inverse somatotopic transform (cortex → body position)
 * WHY: Determine body region represented at cortical location
 * HOW: Finds matching cortical range and inverts interpolation
 */
static void somatotopic_cortex_to_input(
    topographic_map_t* map,
    const float* cortical,
    float* input)
{
    const somatotopic_params_t* params = &map->config.somatotopic;

    float cortical_pos = cortical[0];

    /* Find matching region */
    for (uint32_t i = 0; i < params->num_regions; i++) {
        const somatotopic_region_t* region = &params->regions[i];

        if (cortical_pos >= region->cortical_start &&
            cortical_pos <= region->cortical_end) {
            /* Inverse linear interpolation */
            float t = (cortical_pos - region->cortical_start) /
                (region->cortical_end - region->cortical_start + TOPOGRAPHIC_EPSILON);

            input[0] = region->input_start +
                t * (region->input_end - region->input_start);
            return;
        }
    }

    /* Fallback: inverse linear mapping */
    input[0] = map->config.input_range[0] +
        (cortical_pos - map->transform_offset_x) / map->transform_scale_x;
}

/**
 * WHAT: Computes somatotopic magnification factor
 * WHY: Different body parts have different cortical representation
 * HOW: Retrieves magnification from matching body region
 */
static float somatotopic_get_magnification(
    topographic_map_t* map,
    const float* input)
{
    const somatotopic_params_t* params = &map->config.somatotopic;

    float body_pos = input[0];

    /* Find matching region */
    for (uint32_t i = 0; i < params->num_regions; i++) {
        const somatotopic_region_t* region = &params->regions[i];

        if (body_pos >= region->input_start && body_pos <= region->input_end) {
            float mag = region->magnification;
            return nimcp_clampf(mag, TOPOGRAPHIC_MIN_MAGNIFICATION, TOPOGRAPHIC_MAX_MAGNIFICATION);
        }
    }

    /* Fallback */
    return map->config.magnification_factor;
}

/* ============================================================================
 * Utility Helper Functions
 * ========================================================================== */

/**
 * WHAT: Converts 2D grid coordinates to linear column ID
 * WHY: Maps (x, y) to single index for array access
 * HOW: Row-major order: id = y * width + x
 */
static uint32_t coords_to_column_id(
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height)
{
    (void)height; /* Unused but kept for symmetry */
    return y * width + x;
}

/**
 * WHAT: Converts linear column ID to 2D grid coordinates
 * WHY: Maps single index back to (x, y) for spatial queries
 * HOW: Inverse row-major: x = id % width, y = id / width
 */
static void column_id_to_coords(
    uint32_t id,
    uint32_t width,
    uint32_t* x,
    uint32_t* y)
{
    if (width == 0) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    if (x) *x = id % width;
    if (y) *y = id / width;
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ========================================================================== */

/**
 * WHAT: Query knowledge graph for topographic maps module self-knowledge
 * WHY:  Enable self-awareness and introspection about this module's role
 * HOW:  Query KG for entity info, log observations, check relations
 */
int topographic_maps_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Topographic_Maps_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            TOPO_LOG_DEBUG("Topographic maps self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Topographic_Maps_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Topographic_Maps_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
