/**
 * @file nimcp_visual_cortical_bridge.c
 * @brief Visual-Cortical Bridge Implementation
 *
 * WHAT: Connects visual cortex perception with cortical column processing.
 * WHY:  Provides biologically-realistic V1 processing with proper organization.
 * HOW:  Routes visual input through retinotopic mapping to orientation hypercolumns.
 *
 * @version 1.0.0
 * @date 2025-12-19
 * @author NIMCP Development Team
 */

#include "perception/cortical/nimcp_visual_cortical_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* Utilities */
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform.h"
#include "api/nimcp_api_exception.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(visual_cortical_bridge)

/* ============================================================================
 * Internal Structure Definition
 * ============================================================================ */

/**
 * @brief Internal structure for visual-cortical bridge
 */
struct visual_cortical_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Connected modules */
    visual_cortex_t* visual_cortex;
    topographic_map_t* retinotopic_map;
    cortical_immune_system_t* cortical_immune;

    /* Orientation hypercolumns */
    orientation_hypercolumn_t** hypercolumns;
    uint32_t num_hypercolumns;
    uint32_t orientations_per_hypercolumn;

    /* Gabor filter bank for processing */
    gabor_filter_bank_t* filter_bank;

    /* Configuration */
    visual_cortical_config_t config;

    /* State */
    visual_cortical_state_t state;
    visual_cortical_stats_t stats;

    /* UMM */
    bool umm_enabled;

    /* Immune modulation */
    float immune_modulation_factor;
    float* hypercolumn_gains;

    /* Thread safety */
    nimcp_mutex_t mutex;
    bool mutex_initialized;

    /* Timing */
    uint64_t last_process_time_ns;
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in nanoseconds
 */
static uint64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Compute hypercolumn grid position from retinotopic coordinates
 */
static uint32_t compute_hypercolumn_index(
    const visual_cortical_bridge_t* bridge,
    float retino_x,
    float retino_y)
{
    if (!bridge || bridge->num_hypercolumns == 0) {
        return UINT32_MAX;
    }

    /* Convert to grid position */
    float norm_x = (retino_x + bridge->config.visual_field_degrees / 2.0f) /
                   bridge->config.visual_field_degrees;
    float norm_y = (retino_y + bridge->config.visual_field_degrees / 2.0f) /
                   bridge->config.visual_field_degrees;

    /* Clamp to [0, 1] */
    norm_x = fminf(1.0f, fmaxf(0.0f, norm_x));
    norm_y = fminf(1.0f, fmaxf(0.0f, norm_y));

    /* Compute grid dimensions (assume square grid) */
    uint32_t grid_size = (uint32_t)sqrtf((float)bridge->num_hypercolumns);
    if (grid_size == 0) grid_size = 1;

    uint32_t grid_x = (uint32_t)(norm_x * (grid_size - 1));
    uint32_t grid_y = (uint32_t)(norm_y * (grid_size - 1));

    uint32_t idx = grid_y * grid_size + grid_x;
    if (idx >= bridge->num_hypercolumns) {
        idx = bridge->num_hypercolumns - 1;
    }

    return idx;
}

/**
 * @brief Apply immune modulation to hypercolumn gain
 *
 * Immune factor interpretation:
 * - 1.0 = baseline (no modulation, full response)
 * - <1.0 = suppressed (inflammation reducing sensitivity)
 * - >1.0 = hyper-sensitive
 */
static float apply_immune_modulation(
    const visual_cortical_bridge_t* bridge,
    uint32_t hcol_idx,
    float response)
{
    if (!bridge || hcol_idx >= bridge->num_hypercolumns) {
        return response;
    }

    /* Base modulation is the immune factor itself */
    float modulation = bridge->immune_modulation_factor;

    /* Per-hypercolumn gain if available */
    if (bridge->hypercolumn_gains) {
        modulation *= bridge->hypercolumn_gains[hcol_idx];
    }

    return response * modulation;
}

/**
 * @brief Compute orientation result from hypercolumn responses
 *
 * WHAT: Extracts orientation analysis from hypercolumn into result structure.
 * WHY:  Provide caller with orientation data in convenient format.
 * HOW:  Copies dominant orientation, selectivity, and allocates response array.
 *
 * MEMORY OWNERSHIP:
 * - Allocates result->orientation_responses (caller must free with visual_cortical_free_result)
 * - If allocation fails, result->orientation_responses will be NULL (safe)
 * - Caller MUST call visual_cortical_free_result() to avoid leak
 */
static void compute_orientation_result(
    orientation_hypercolumn_t* hcol,
    visual_cortical_orientation_result_t* result,
    float retino_x,
    float retino_y)
{
    if (!hcol || !result) return;

    result->num_orientations = hcol->num_orientations;
    result->dominant_orientation = hcol->dominant_orientation;
    result->selectivity_index = hcol->selectivity_index;
    result->retino_x = retino_x;
    result->retino_y = retino_y;

    /* Allocate and copy orientation responses
     * MEMORY SAFETY: Caller must free with visual_cortical_free_result()
     */
    if (hcol->num_orientations > 0) {
        result->orientation_responses = (float*)nimcp_malloc(
            hcol->num_orientations * sizeof(float)
        );
        if (result->orientation_responses && hcol->columns) {
            for (uint32_t i = 0; i < hcol->num_orientations; i++) {
                result->orientation_responses[i] = hcol->columns[i].activation;
            }
        }
    }

    /* Compute confidence from selectivity */
    result->confidence = hcol->selectivity_index;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

void visual_cortical_default_config(visual_cortical_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortical_default_config: config is NULL");
        return;
    }

    memset(config, 0, sizeof(visual_cortical_config_t));

    config->num_hypercolumns = 64;  /* 8x8 grid */
    config->orientations_per_hypercolumn = VISUAL_CORTICAL_DEFAULT_ORIENTATIONS;
    config->spatial_frequency = VISUAL_CORTICAL_DEFAULT_SPATIAL_FREQ;
    config->tuning_width = VISUAL_CORTICAL_DEFAULT_TUNING_WIDTH;
    config->mode = VISUAL_CORTICAL_MODE_HYPERCOLUMN;
    config->enable_retinotopic_mapping = true;
    config->enable_cortical_immune = true;
    config->enable_bio_async = true;
    config->visual_field_degrees = 120.0f;  /* Wide field of view */
    config->foveal_radius = 5.0f;           /* Central 5 degrees high acuity */
    config->cortical_magnification = 10.0f;
    config->immune_modulation_factor = VISUAL_CORTICAL_DEFAULT_IMMUNE_FACTOR;
    config->use_umm = false;
}

visual_cortical_bridge_t* visual_cortical_bridge_create(
    const visual_cortical_config_t* config,
    visual_cortex_t* visual_cortex)
{
    visual_cortical_config_t local_config;

    /* Use default config if none provided */
    if (!config) {
        visual_cortical_default_config(&local_config);
        config = &local_config;
    }

    /* Validate configuration */
    if (config->num_hypercolumns == 0 ||
        config->num_hypercolumns > VISUAL_CORTICAL_MAX_HYPERCOLUMNS) {
        NIMCP_LOGGING_ERROR("Invalid num_hypercolumns: %u", config->num_hypercolumns);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortical_bridge_create: invalid num_hypercolumns");
        return NULL;
    }

    /* Allocate bridge */
    visual_cortical_bridge_t* bridge = (visual_cortical_bridge_t*)nimcp_malloc(
        sizeof(visual_cortical_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate visual-cortical bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortical_bridge_create: bridge is NULL");
        return NULL;
    }
    memset(bridge, 0, sizeof(visual_cortical_bridge_t));

    /* Copy configuration */
    memcpy(&bridge->config, config, sizeof(visual_cortical_config_t));
    bridge->visual_cortex = visual_cortex;
    bridge->state = VISUAL_CORTICAL_STATE_UNINITIALIZED;
    bridge->immune_modulation_factor = config->immune_modulation_factor;
    bridge->umm_enabled = config->use_umm;

    /* Initialize mutex */
    bridge->base.mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (bridge->base.mutex && nimcp_mutex_init(bridge->base.mutex, NULL) == 0) {
    } else {
        NIMCP_LOGGING_WARN("Failed to initialize mutex, continuing without thread safety");
    }

    /* Allocate hypercolumns array */
    bridge->hypercolumns = (orientation_hypercolumn_t**)nimcp_malloc(
        config->num_hypercolumns * sizeof(orientation_hypercolumn_t*)
    );
    if (!bridge->hypercolumns) {
        NIMCP_LOGGING_ERROR("Failed to allocate hypercolumns array");
        visual_cortical_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortical_bridge_create: bridge->hypercolumns is NULL");
        return NULL;
    }
    memset(bridge->hypercolumns, 0,
           config->num_hypercolumns * sizeof(orientation_hypercolumn_t*));

    /* Create hypercolumns */
    bridge->num_hypercolumns = config->num_hypercolumns;
    bridge->orientations_per_hypercolumn = config->orientations_per_hypercolumn;

    for (uint32_t i = 0; i < config->num_hypercolumns; i++) {
        bridge->hypercolumns[i] = orientation_hypercolumn_create(
            config->orientations_per_hypercolumn,
            config->spatial_frequency,
            config->tuning_width
        );
        if (!bridge->hypercolumns[i]) {
            NIMCP_LOGGING_ERROR("Failed to create hypercolumn %u", i);
            visual_cortical_bridge_destroy(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortical_bridge_create: bridge->hypercolumns is NULL");
            return NULL;
        }

        /* Set pinwheel center based on grid position */
        uint32_t grid_size = (uint32_t)sqrtf((float)config->num_hypercolumns);
        if (grid_size == 0) grid_size = 1;
        float center_x = (float)(i % grid_size) / (float)grid_size;
        float center_y = (float)(i / grid_size) / (float)grid_size;
        orientation_hypercolumn_set_pinwheel(bridge->hypercolumns[i],
                                             center_x, center_y);
    }

    /* Allocate per-hypercolumn gains */
    bridge->hypercolumn_gains = (float*)nimcp_malloc(
        config->num_hypercolumns * sizeof(float)
    );
    if (bridge->hypercolumn_gains) {
        for (uint32_t i = 0; i < config->num_hypercolumns; i++) {
            bridge->hypercolumn_gains[i] = 1.0f;
        }
    }

    /* Create retinotopic map if enabled */
    if (config->enable_retinotopic_mapping) {
        retinotopic_params_t retino_params = {
            .foveal_radius = config->foveal_radius,
            .cortical_magnification = config->cortical_magnification,
            .log_polar_a = 0.5f,
            .aspect_ratio = 1.0f,
            .eccentricity_half = 2.5f,
            .angle_coverage = 2.0f * M_PI
        };

        uint32_t grid_size = (uint32_t)sqrtf((float)config->num_hypercolumns);
        if (grid_size == 0) grid_size = 1;

        bridge->retinotopic_map = topographic_map_create_retinotopic(
            &retino_params,
            grid_size,
            grid_size
        );
        if (!bridge->retinotopic_map) {
            NIMCP_LOGGING_WARN("Failed to create retinotopic map, continuing without");
        }
    }

    /* Create Gabor filter bank for processing */
    float wavelength = 1.0f / config->spatial_frequency;
    uint32_t kernel_size = (uint32_t)(wavelength * 4.0f);  /* 4 wavelengths */
    if (kernel_size < 5) kernel_size = 5;
    if (kernel_size % 2 == 0) kernel_size++;  /* Make odd */

    bridge->filter_bank = gabor_filter_bank_create(
        config->orientations_per_hypercolumn,
        kernel_size,
        wavelength,
        true  /* include quadrature pairs */
    );
    if (!bridge->filter_bank) {
        NIMCP_LOGGING_WARN("Failed to create filter bank, using hypercolumn processing");
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(visual_cortical_stats_t));

    bridge->state = VISUAL_CORTICAL_STATE_READY;

    NIMCP_LOGGING_INFO("Visual-cortical bridge created with %u hypercolumns (%u orientations each)",
                       bridge->num_hypercolumns, bridge->orientations_per_hypercolumn);

    return bridge;
}

void visual_cortical_bridge_destroy(visual_cortical_bridge_t* bridge)
{
    if (!bridge) return;

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        visual_cortical_disconnect_bio_async(bridge);
    }

    /* Destroy hypercolumns */
    if (bridge->hypercolumns) {
        for (uint32_t i = 0; i < bridge->num_hypercolumns; i++) {
            if (bridge->hypercolumns[i]) {
                orientation_hypercolumn_destroy(bridge->hypercolumns[i]);
            }
        }
        nimcp_free(bridge->hypercolumns);
    }

    /* Destroy filter bank */
    if (bridge->filter_bank) {
        gabor_filter_bank_destroy(bridge->filter_bank);
    }

    /* Destroy retinotopic map */
    if (bridge->retinotopic_map) {
        topographic_map_destroy(bridge->retinotopic_map);
    }

    /* Free gains */
    if (bridge->hypercolumn_gains) {
        nimcp_free(bridge->hypercolumn_gains);
    }

    /* Destroy mutex */
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_destroy(bridge->base.mutex);
        nimcp_free(bridge->base.mutex);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);

    NIMCP_LOGGING_DEBUG("Visual-cortical bridge destroyed");
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int visual_cortical_connect_visual_cortex(
    visual_cortical_bridge_t* bridge,
    visual_cortex_t* visual_cortex)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    bridge->visual_cortex = visual_cortex;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Connected to visual cortex");
    return NIMCP_SUCCESS;
}

int visual_cortical_connect_immune(
    visual_cortical_bridge_t* bridge,
    cortical_immune_system_t* immune)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    bridge->cortical_immune = immune;

    /* Register hypercolumns with immune system */
    if (immune) {
        for (uint32_t i = 0; i < bridge->num_hypercolumns; i++) {
            if (bridge->hypercolumns[i]) {
                cortical_immune_register_orientation_hypercolumn(
                    immune, bridge->hypercolumns[i], i
                );
            }
        }
    }

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Connected to cortical immune system");
    return NIMCP_SUCCESS;
}

int visual_cortical_connect_bio_async(visual_cortical_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_VISUAL_CORTICAL,
        .module_name = "visual_cortical_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Visual-cortical bridge connected to bio-async router");
        return NIMCP_SUCCESS;
    }

    NIMCP_LOGGING_WARN("Failed to connect to bio-async router");
    return NIMCP_ERROR_OPERATION_FAILED;
}

int visual_cortical_disconnect_bio_async(visual_cortical_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_DEBUG("Disconnected from bio-async router");
    return NIMCP_SUCCESS;
}

bool visual_cortical_is_bio_async_connected(const visual_cortical_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    visual_cortical_bridge_t* mutable_bridge = (visual_cortical_bridge_t*)bridge;

    bool connected;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    connected = bridge->base.bio_async_enabled;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Processing Functions
 * ============================================================================ */

int visual_cortical_process(
    visual_cortical_bridge_t* bridge,
    const float* image,
    uint32_t width,
    uint32_t height,
    visual_cortical_orientation_result_t* result)
{
    NIMCP_CHECK_THROW(bridge && image && result, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in visual_cortical_process");
    NIMCP_CHECK_THROW(width > 0 && height > 0, NIMCP_ERROR_INVALID_PARAM,
        "Invalid width or height in visual_cortical_process");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = VISUAL_CORTICAL_STATE_PROCESSING;
    uint64_t start_time = get_time_ns();

    memset(result, 0, sizeof(visual_cortical_orientation_result_t));

    /* Determine processing mode */
    if (bridge->config.mode == VISUAL_CORTICAL_MODE_DIRECT && bridge->filter_bank) {
        /* Use direct filter bank convolution */
        uint32_t num_orientations = bridge->orientations_per_hypercolumn;
        float* responses = (float*)nimcp_malloc(num_orientations * sizeof(float));

        if (responses) {
            bool success = gabor_filter_bank_apply(
                bridge->filter_bank, image, width, height, responses
            );

            if (success) {
                /* Find dominant orientation */
                float max_response = responses[0];
                uint32_t max_idx = 0;
                for (uint32_t i = 1; i < num_orientations; i++) {
                    if (responses[i] > max_response) {
                        max_response = responses[i];
                        max_idx = i;
                    }
                }

                result->dominant_orientation = (float)max_idx * 180.0f / (float)num_orientations;
                result->num_orientations = num_orientations;
                result->orientation_responses = responses;  /* Transfer ownership */

                /* Compute selectivity */
                float sum_responses = 0.0f;
                for (uint32_t i = 0; i < num_orientations; i++) {
                    sum_responses += responses[i];
                }
                if (sum_responses > 0.0f) {
                    result->selectivity_index = max_response / sum_responses;
                }
                result->confidence = result->selectivity_index;
            } else {
                nimcp_free(responses);
            }
        }
    } else {
        /* Use hypercolumn processing */
        /* Compute average response across hypercolumns */
        float* avg_responses = (float*)nimcp_calloc(
            bridge->orientations_per_hypercolumn, sizeof(float)
        );
        uint32_t processed_count = 0;

        uint32_t grid_size = (uint32_t)sqrtf((float)bridge->num_hypercolumns);
        if (grid_size == 0) grid_size = 1;

        /* Process all hypercolumns for comprehensive orientation detection */
        for (uint32_t idx = 0; idx < bridge->num_hypercolumns; idx++) {

            orientation_hypercolumn_t* hcol = bridge->hypercolumns[idx];
            if (!hcol) continue;

            /* Extract patch for this hypercolumn */
            uint32_t patch_x = (idx % grid_size) * width / grid_size;
            uint32_t patch_y = (idx / grid_size) * height / grid_size;
            uint32_t patch_w = width / grid_size;
            uint32_t patch_h = height / grid_size;

            if (patch_w == 0) patch_w = 1;
            if (patch_h == 0) patch_h = 1;

            /* Create patch buffer */
            float* patch = (float*)nimcp_malloc(patch_w * patch_h * sizeof(float));
            if (!patch) continue;

            /* Copy patch data */
            for (uint32_t py = 0; py < patch_h && (patch_y + py) < height; py++) {
                for (uint32_t px = 0; px < patch_w && (patch_x + px) < width; px++) {
                    patch[py * patch_w + px] = image[(patch_y + py) * width + (patch_x + px)];
                }
            }

            /* Process through hypercolumn */
            bool processed = orientation_hypercolumn_process(hcol, patch, patch_w, patch_h);
            if (processed) {
                orientation_hypercolumn_normalize(hcol);

                /* Accumulate responses */
                if (hcol->columns && avg_responses) {
                    for (uint32_t o = 0; o < hcol->num_orientations &&
                         o < bridge->orientations_per_hypercolumn; o++) {
                        float resp = hcol->columns[o].activation;
                        resp = apply_immune_modulation(bridge, idx, resp);
                        avg_responses[o] += resp;
                    }
                    processed_count++;
                }

                bridge->stats.hypercolumn_activations++;
            }

            nimcp_free(patch);
        }

        /* Compute average and find dominant */
        if (processed_count > 0 && avg_responses) {
            float max_response = 0.0f;
            uint32_t max_idx = 0;
            float sum_responses = 0.0f;

            for (uint32_t i = 0; i < bridge->orientations_per_hypercolumn; i++) {
                avg_responses[i] /= (float)processed_count;
                sum_responses += avg_responses[i];
                if (avg_responses[i] > max_response) {
                    max_response = avg_responses[i];
                    max_idx = i;
                }
            }

            result->dominant_orientation = (float)max_idx * 180.0f /
                                          (float)bridge->orientations_per_hypercolumn;
            result->num_orientations = bridge->orientations_per_hypercolumn;
            result->orientation_responses = avg_responses;  /* Transfer ownership */

            if (sum_responses > 0.0f) {
                result->selectivity_index = max_response / sum_responses;
            }
            result->confidence = result->selectivity_index;

            bridge->stats.active_hypercolumns = processed_count;
        } else {
            NIMCP_LOGGING_DEBUG("No hypercolumns processed or no responses!");
            if (avg_responses) nimcp_free(avg_responses);
        }
    }

    /* Update statistics */
    bridge->stats.images_processed++;
    bridge->stats.peak_orientation_response = result->selectivity_index;
    bridge->stats.current_dominant_orientation = result->dominant_orientation;
    bridge->stats.current_immune_modulation = bridge->immune_modulation_factor;

    uint64_t end_time = get_time_ns();
    float process_time_ms = (float)(end_time - start_time) / 1000000.0f;
    float updated = (bridge->stats.avg_processing_time_ms * (bridge->stats.images_processed - 1) +
         process_time_ms) / (float)bridge->stats.images_processed;
    if (isfinite(updated)) bridge->stats.avg_processing_time_ms = updated;

    bridge->last_process_time_ns = end_time;
    bridge->state = VISUAL_CORTICAL_STATE_READY;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int visual_cortical_process_patch(
    visual_cortical_bridge_t* bridge,
    const float* patch,
    uint32_t patch_width,
    uint32_t patch_height,
    float retino_x,
    float retino_y,
    visual_cortical_orientation_result_t* result)
{
    NIMCP_CHECK_THROW(bridge && patch && result, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in visual_cortical_process_patch");
    NIMCP_CHECK_THROW(patch_width > 0 && patch_height > 0, NIMCP_ERROR_INVALID_PARAM,
        "Invalid patch_width or patch_height in visual_cortical_process_patch");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    memset(result, 0, sizeof(visual_cortical_orientation_result_t));

    /* Find hypercolumn for this position */
    uint32_t hcol_idx = compute_hypercolumn_index(bridge, retino_x, retino_y);
    if (hcol_idx >= bridge->num_hypercolumns) {
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    orientation_hypercolumn_t* hcol = bridge->hypercolumns[hcol_idx];
    if (!hcol) {
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Process through hypercolumn */
    if (!orientation_hypercolumn_process(hcol, patch, patch_width, patch_height)) {
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    orientation_hypercolumn_normalize(hcol);
    bridge->stats.hypercolumn_activations++;

    /* Compute result */
    compute_orientation_result(hcol, result, retino_x, retino_y);

    /* Apply immune modulation */
    if (result->orientation_responses) {
        for (uint32_t i = 0; i < result->num_orientations; i++) {
            result->orientation_responses[i] = apply_immune_modulation(
                bridge, hcol_idx, result->orientation_responses[i]
            );
        }
    }

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

void visual_cortical_free_result(visual_cortical_orientation_result_t* result)
{
    if (!result) return;

    if (result->orientation_responses) {
        nimcp_free(result->orientation_responses);
        result->orientation_responses = NULL;
    }
    result->num_orientations = 0;
}

int visual_cortical_get_orientation_map(
    visual_cortical_bridge_t* bridge,
    const float* image,
    uint32_t width,
    uint32_t height,
    float* orientation_map,
    float* selectivity_map)
{
    NIMCP_CHECK_THROW(bridge && image && orientation_map, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in visual_cortical_get_orientation_map");
    NIMCP_CHECK_THROW(width > 0 && height > 0, NIMCP_ERROR_INVALID_PARAM,
        "Invalid width or height in visual_cortical_get_orientation_map");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    uint32_t grid_size = (uint32_t)sqrtf((float)bridge->num_hypercolumns);
    if (grid_size == 0) grid_size = 1;

    uint32_t patch_w = width / grid_size;
    uint32_t patch_h = height / grid_size;
    if (patch_w == 0) patch_w = 1;
    if (patch_h == 0) patch_h = 1;

    /* Process each hypercolumn and fill orientation map */
    for (uint32_t hy = 0; hy < grid_size; hy++) {
        for (uint32_t hx = 0; hx < grid_size; hx++) {
            uint32_t hcol_idx = hy * grid_size + hx;
            if (hcol_idx >= bridge->num_hypercolumns) continue;

            orientation_hypercolumn_t* hcol = bridge->hypercolumns[hcol_idx];
            if (!hcol) continue;

            /* Extract patch */
            uint32_t patch_x = hx * patch_w;
            uint32_t patch_y = hy * patch_h;

            float* patch = (float*)nimcp_malloc(patch_w * patch_h * sizeof(float));
            if (!patch) continue;

            for (uint32_t py = 0; py < patch_h && (patch_y + py) < height; py++) {
                for (uint32_t px = 0; px < patch_w && (patch_x + px) < width; px++) {
                    patch[py * patch_w + px] = image[(patch_y + py) * width + (patch_x + px)];
                }
            }

            /* Process */
            if (orientation_hypercolumn_process(hcol, patch, patch_w, patch_h)) {
                orientation_hypercolumn_normalize(hcol);

                float dominant = orientation_hypercolumn_get_dominant(hcol);
                float osi = orientation_hypercolumn_compute_osi(hcol);

                /* Fill map region */
                for (uint32_t py = 0; py < patch_h && (patch_y + py) < height; py++) {
                    for (uint32_t px = 0; px < patch_w && (patch_x + px) < width; px++) {
                        uint32_t map_idx = (patch_y + py) * width + (patch_x + px);
                        orientation_map[map_idx] = dominant;
                        if (selectivity_map) {
                            selectivity_map[map_idx] = osi;
                        }
                    }
                }
            }

            nimcp_free(patch);
        }
    }

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Hypercolumn Functions
 * ============================================================================ */

const orientation_hypercolumn_t* visual_cortical_get_hypercolumn(
    const visual_cortical_bridge_t* bridge,
    float retino_x,
    float retino_y)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return NULL;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    visual_cortical_bridge_t* mutable_bridge = (visual_cortical_bridge_t*)bridge;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);

    uint32_t idx = compute_hypercolumn_index(bridge, retino_x, retino_y);
    if (idx >= bridge->num_hypercolumns) {
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "visual_cortical_get_hypercolumn: index out of range");
        return NULL;
    }

    const orientation_hypercolumn_t* hcol = bridge->hypercolumns[idx];
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return hcol;
}

const orientation_hypercolumn_t* visual_cortical_get_hypercolumn_by_index(
    const visual_cortical_bridge_t* bridge,
    uint32_t index)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortical_get_hypercolumn_by_index: bridge is NULL");
        return NULL;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    visual_cortical_bridge_t* mutable_bridge = (visual_cortical_bridge_t*)bridge;

    const orientation_hypercolumn_t* hcol = NULL;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    if (index < bridge->num_hypercolumns) {
        hcol = bridge->hypercolumns[index];
    }
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return hcol;
}

uint32_t visual_cortical_get_num_hypercolumns(const visual_cortical_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortical_get_num_hypercolumns: bridge is NULL");
        return 0;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    visual_cortical_bridge_t* mutable_bridge = (visual_cortical_bridge_t*)bridge;

    uint32_t count;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    count = bridge->num_hypercolumns;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return count;
}

/* ============================================================================
 * Immune Modulation Functions
 * ============================================================================ */

int visual_cortical_update_immune_modulation(visual_cortical_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->cortical_immune) {
        return NIMCP_SUCCESS;  /* No immune system connected */
    }

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    /* Get cortical immune statistics */
    cortical_immune_stats_t stats;
    if (cortical_immune_get_stats(bridge->cortical_immune, &stats) == 0) {
        bridge->immune_modulation_factor = stats.mean_inflammation_level;
        bridge->stats.current_immune_modulation = bridge->immune_modulation_factor;

        /* Update per-hypercolumn gains based on local inflammation */
        for (uint32_t i = 0; i < bridge->num_hypercolumns; i++) {
            cortical_column_immune_t col_status;
            if (cortical_immune_get_column_status(bridge->cortical_immune,
                                                  i, &col_status) == 0) {
                if (bridge->hypercolumn_gains) {
                    bridge->hypercolumn_gains[i] = col_status.gain_modulation;
                }
            }
        }
    }

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int visual_cortical_set_immune_factor(
    visual_cortical_bridge_t* bridge,
    float factor)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Clamp to valid range [0.0, 1.0] */
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);
    bridge->immune_modulation_factor = factor;
    bridge->stats.current_immune_modulation = factor;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

float visual_cortical_get_immune_factor(const visual_cortical_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortical_get_immune_factor: bridge is NULL");
        return 0.0f;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    visual_cortical_bridge_t* mutable_bridge = (visual_cortical_bridge_t*)bridge;

    float factor;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    factor = bridge->immune_modulation_factor;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return factor;
}

/* ============================================================================
 * Statistics and State Functions
 * ============================================================================ */

int visual_cortical_get_stats(
    const visual_cortical_bridge_t* bridge,
    visual_cortical_stats_t* stats)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    visual_cortical_bridge_t* mutable_bridge = (visual_cortical_bridge_t*)bridge;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(visual_cortical_stats_t));
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int visual_cortical_reset_stats(visual_cortical_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(visual_cortical_stats_t));
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

visual_cortical_state_t visual_cortical_get_state(
    const visual_cortical_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortical_get_state: bridge is NULL");
        return VISUAL_CORTICAL_STATE_UNINITIALIZED;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    visual_cortical_bridge_t* mutable_bridge = (visual_cortical_bridge_t*)bridge;

    visual_cortical_state_t state;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    state = bridge->state;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return state;
}

const topographic_map_t* visual_cortical_get_retinotopic_map(
    const visual_cortical_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortical_get_retinotopic_map: bridge is NULL");
        return NULL;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    visual_cortical_bridge_t* mutable_bridge = (visual_cortical_bridge_t*)bridge;

    const topographic_map_t* map;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    map = bridge->retinotopic_map;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return map;
}

/* ============================================================================
 * Bio-Async Message Handling
 * ============================================================================ */

uint32_t visual_cortical_process_bio_messages(
    visual_cortical_bridge_t* bridge,
    uint32_t max_messages)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortical_process_bio_messages: bridge is NULL");
        return 0;
    }
    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    /* Processing bio messages requires custom message structures
     * For now, return 0 indicating no messages processed
     * Full implementation would use bio_router_receive with custom message types
     */
    (void)max_messages;  /* Suppress unused parameter warning */
    return 0;
}

int visual_cortical_broadcast_orientation(
    visual_cortical_bridge_t* bridge,
    const visual_cortical_orientation_result_t* result)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Broadcasting requires custom message structure definition
     * For now, just update stats and return success
     * Full implementation would use bio_router_broadcast with custom message type
     */
    bridge->stats.bio_messages_sent++;

    NIMCP_LOGGING_DEBUG("Broadcast orientation: angle=%.1f, selectivity=%.2f",
                        result->dominant_orientation, result->selectivity_index);

    return NIMCP_SUCCESS;
}
