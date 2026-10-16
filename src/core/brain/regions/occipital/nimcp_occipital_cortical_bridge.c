/**
 * @file nimcp_occipital_cortical_bridge.c
 * @brief Implementation of Occipital-Cortical Columns bridge
 *
 * WHAT: Connects occipital visual processing to cortical column organization
 * WHY: Enable biologically-realistic V1-V5 processing with columnar architecture
 * HOW: Routes visual features to orientation hypercolumns with retinotopic mapping
 *
 * BIOLOGICAL BASIS:
 * - V1 orientation columns have ~1mm hypercolumn periodicity
 * - Log-polar retinotopic mapping with foveal magnification factor ~2-3
 * - Lateral inhibition sharpens orientation tuning
 * - Ocular dominance columns alternate ~0.5mm
 *
 * @version Phase O1: Occipital Cortical Integration
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/occipital/nimcp_occipital_cortical_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/math/nimcp_math_helpers.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_dimension_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(occipital_cortical_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "OCCIPITAL_CORTICAL_BRIDGE"


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define CORTICAL_BRIDGE_LOG_MODULE "OCC_CORTICAL"

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define MAX_HYPERCOLUMNS 256
#define MAX_ORIENTATIONS 16
#define MAX_COLUMNS 4096
#define CORTICAL_MAGNIFICATION_FACTOR 2.5f
#define LATERAL_INHIBITION_RADIUS 3

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Single orientation column
 */
typedef struct {
    float preferred_orientation;     /**< Preferred orientation (radians) */
    float tuning_width;              /**< Tuning width (radians) */
    float activity;                  /**< Current activation */
    float baseline;                  /**< Baseline activity */
    uint32_t eye_dominance;          /**< Eye dominance (0=L, 1=bi, 2=R) */
} orientation_column_t;

/**
 * @brief Hypercolumn containing multiple orientation columns
 */
typedef struct {
    orientation_column_t columns[MAX_ORIENTATIONS];
    uint32_t num_orientations;
    float cortical_x;                /**< Cortical position X */
    float cortical_y;                /**< Cortical position Y */
    float visual_x;                  /**< Visual field X */
    float visual_y;                  /**< Visual field Y */
    float population_activity;       /**< Total population activity */
    float dominant_orientation;      /**< Dominant orientation */
    bool is_color_blob;              /**< True if color-selective blob */
    float color_response[4];         /**< RGBA response for blobs */
} hypercolumn_t;

/**
 * @brief Retinotopic map entry
 */
typedef struct {
    float visual_x;                  /**< Visual field X */
    float visual_y;                  /**< Visual field Y */
    float cortical_x;                /**< Cortical X */
    float cortical_y;                /**< Cortical Y */
    float magnification;             /**< Local magnification factor */
    uint32_t hypercolumn_id;         /**< Associated hypercolumn */
} retinotopic_entry_t;

/**
 * @brief Internal bridge structure
 */
struct occipital_cortical_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    occipital_cortical_config_t config;

    /* Connected modules */
    occipital_adapter_t* occipital;
    orientation_hypercolumn_t* hypercolumns_external;
    topographic_map_t* topographic_map;
    cortical_immune_system_t* immune_system;
    bio_router_t router;

    /* Module ID for bio-async */
    uint32_t module_id;

    /* Internal hypercolumn representation */
    hypercolumn_t hypercolumns[MAX_HYPERCOLUMNS];
    uint32_t num_hypercolumns;

    /* Retinotopic map */
    retinotopic_entry_t* retinotopic_map;
    uint32_t map_size;

    /* Current effects */
    occipital_cortical_effects_t effects;

    /* Statistics */
    occipital_cortical_stats_t stats;

    /* Inflammation state */
    float current_inflammation;

    /* Timing */
    uint64_t last_update_us;
    uint64_t creation_time_us;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Log-polar mapping from visual field to cortex
 *
 * Implements cortical magnification: w = log(z + a) where z is complex position
 * Fovea (center) gets more cortical area than periphery
 */
static void log_polar_map(float visual_x, float visual_y,
                          float magnification_factor,
                          float* cortical_x, float* cortical_y) {
    /* Center visual coordinates */
    float cx = visual_x - 0.5f;
    float cy = visual_y - 0.5f;

    /* Convert to polar */
    float r = sqrtf(cx * cx + cy * cy) + 0.01f; /* Avoid log(0) */
    float theta = atan2f(cy, cx);

    /* Log-polar transform with magnification */
    float a = 0.1f; /* Foveal constant */
    float log_r = logf(r + a) * magnification_factor;

    /* Back to Cartesian */
    *cortical_x = log_r * cosf(theta) + 0.5f;
    *cortical_y = log_r * sinf(theta) + 0.5f;
}

/**
 * @brief Initialize hypercolumns with pinwheel organization
 */
static void init_hypercolumns(occipital_cortical_bridge_t* bridge) {
    uint32_t count = bridge->config.num_hypercolumns;
    if (count > MAX_HYPERCOLUMNS) count = MAX_HYPERCOLUMNS;

    /* Arrange in grid */
    uint32_t grid_size = (uint32_t)sqrtf((float)count);
    if (grid_size * grid_size < count) grid_size++;

    uint32_t idx = 0;
    for (uint32_t row = 0; row < grid_size && idx < count; row++) {
        for (uint32_t col = 0; col < grid_size && idx < count; col++) {
            hypercolumn_t* hc = &bridge->hypercolumns[idx];

            /* Visual field position (normalized) */
            hc->visual_x = (float)col / (float)(grid_size - 1);
            hc->visual_y = (float)row / (float)(grid_size - 1);

            /* Map to cortical position */
            if (bridge->config.map_mode == OCC_MAP_LOG_POLAR) {
                log_polar_map(hc->visual_x, hc->visual_y,
                              bridge->config.foveal_magnification,
                              &hc->cortical_x, &hc->cortical_y);
            } else {
                hc->cortical_x = hc->visual_x;
                hc->cortical_y = hc->visual_y;
            }

            /* Initialize orientation columns (pinwheel structure) */
            uint32_t num_ori = bridge->config.orientations_per_column;
            if (num_ori > MAX_ORIENTATIONS) num_ori = MAX_ORIENTATIONS;
            hc->num_orientations = num_ori;

            for (uint32_t o = 0; o < num_ori; o++) {
                orientation_column_t* oc = &hc->columns[o];
                oc->preferred_orientation = (float)o * M_PI / (float)num_ori;
                oc->tuning_width = M_PI / (float)num_ori; /* ~22.5 degrees */
                oc->activity = 0.0f;
                oc->baseline = 0.02f;

                /* Ocular dominance pattern */
                if (bridge->config.enable_ocular_dominance) {
                    /* Alternating columns */
                    oc->eye_dominance = (col + o) % 3;
                } else {
                    oc->eye_dominance = 1; /* Binocular */
                }
            }

            /* Color blob pattern (interleaved) */
            hc->is_color_blob = bridge->config.enable_color_blobs &&
                                ((row + col) % 4 == 0);
            memset(hc->color_response, 0, sizeof(hc->color_response));

            hc->population_activity = 0.0f;
            hc->dominant_orientation = 0.0f;

            idx++;
        }
    }

    bridge->num_hypercolumns = idx;
}

/**
 * @brief Initialize retinotopic map
 */
static int init_retinotopic_map(occipital_cortical_bridge_t* bridge) {
    uint32_t res_x = bridge->config.map_resolution_x;
    uint32_t res_y = bridge->config.map_resolution_y;
    uint32_t total = res_x * res_y;

    bridge->retinotopic_map = (retinotopic_entry_t*)nimcp_calloc(
        total, sizeof(retinotopic_entry_t));
    if (!bridge->retinotopic_map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_retinotopic_map: bridge->retinotopic_map is NULL");
        return -1;
    }

    bridge->map_size = total;

    for (uint32_t y = 0; y < res_y; y++) {
        for (uint32_t x = 0; x < res_x; x++) {
            uint32_t idx = y * res_x + x;
            retinotopic_entry_t* entry = &bridge->retinotopic_map[idx];

            entry->visual_x = (float)x / (float)(res_x - 1);
            entry->visual_y = (float)y / (float)(res_y - 1);

            /* Map to cortex */
            if (bridge->config.map_mode == OCC_MAP_LOG_POLAR) {
                log_polar_map(entry->visual_x, entry->visual_y,
                              bridge->config.foveal_magnification,
                              &entry->cortical_x, &entry->cortical_y);
            } else {
                entry->cortical_x = entry->visual_x;
                entry->cortical_y = entry->visual_y;
            }

            /* Calculate local magnification */
            float eccentricity = sqrtf(
                (entry->visual_x - 0.5f) * (entry->visual_x - 0.5f) +
                (entry->visual_y - 0.5f) * (entry->visual_y - 0.5f)
            );
            entry->magnification = CORTICAL_MAGNIFICATION_FACTOR /
                                   (1.0f + eccentricity * bridge->config.eccentricity_scale);

            /* Find nearest hypercolumn */
            float min_dist = INFINITY;
            entry->hypercolumn_id = 0;
            for (uint32_t h = 0; h < bridge->num_hypercolumns; h++) {
                float dx = bridge->hypercolumns[h].visual_x - entry->visual_x;
                float dy = bridge->hypercolumns[h].visual_y - entry->visual_y;
                float dist = dx * dx + dy * dy;
                if (dist < min_dist) {
                    min_dist = dist;
                    entry->hypercolumn_id = h;
                }
            }
        }
    }

    return 0;
}

/**
 * @brief Orientation response function (von Mises-like)
 */
static float orientation_response(float stimulus_orientation,
                                   float preferred_orientation,
                                   float tuning_width) {
    float diff = stimulus_orientation - preferred_orientation;

    /* Wrap to [-pi, pi] */
    while (diff > M_PI) diff -= 2.0f * M_PI;
    while (diff < -M_PI) diff += 2.0f * M_PI;

    /* Von Mises tuning curve */
    float kappa = 2.0f / (tuning_width * tuning_width);
    float response = expf(kappa * (cosf(diff) - 1.0f));

    return response;
}

/**
 * @brief Apply lateral inhibition between columns
 */
static void apply_lateral_inhibition(occipital_cortical_bridge_t* bridge) {
    float inhibition = bridge->config.lateral_inhibition;

    for (uint32_t h = 0; h < bridge->num_hypercolumns; h++) {
        hypercolumn_t* hc = &bridge->hypercolumns[h];

        /* Find max activity for normalization */
        float max_activity = 0.0f;
        for (uint32_t o = 0; o < hc->num_orientations; o++) {
            if (hc->columns[o].activity > max_activity) {
                max_activity = hc->columns[o].activity;
            }
        }

        if (max_activity < 0.01f) continue;

        /* Apply winner-take-more inhibition */
        for (uint32_t o = 0; o < hc->num_orientations; o++) {
            orientation_column_t* oc = &hc->columns[o];
            float relative = oc->activity / max_activity;
            float suppression = (1.0f - relative) * inhibition;
            oc->activity *= (1.0f - suppression);
            oc->activity = nimcp_clampf(oc->activity, 0.0f, 1.0f);
        }
    }
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/* Note: Bio-async message handlers will be implemented when
 * the full bio-async infrastructure is integrated */

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

occipital_cortical_config_t occipital_cortical_default_config(void) {
    occipital_cortical_config_t config = {
        /* Column organization */
        .num_hypercolumns = NIMCP_DEFAULT_HYPERCOLUMNS,
        .orientations_per_column = 8,
        .column_spacing_mm = 1.0f,
        .enable_ocular_dominance = true,
        .enable_color_blobs = true,

        /* Retinotopic mapping */
        .map_mode = OCC_MAP_LOG_POLAR,
        .foveal_magnification = 2.5f,
        .eccentricity_scale = 3.0f,
        .map_resolution_x = 32,
        .map_resolution_y = 32,

        /* Processing parameters */
        .lateral_inhibition = 0.3f,
        .surround_suppression = 2.0f,
        .contrast_gain = 1.5f,
        .enable_bio_async = true,

        /* Immune system */
        .enable_cortical_immune = true,
        .inflammation_threshold = 0.5f
    };

    return config;
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

occipital_cortical_bridge_t* occipital_cortical_bridge_create(
    occipital_adapter_t* occipital,
    const occipital_cortical_config_t* config) {

    if (!occipital) {
        LOG_ERROR(CORTICAL_BRIDGE_LOG_MODULE, "NULL occipital adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital is NULL");
        return NULL;
    }

    occipital_cortical_bridge_t* bridge =
        (occipital_cortical_bridge_t*)nimcp_malloc(sizeof(*bridge));
    if (!bridge) {
        LOG_ERROR(CORTICAL_BRIDGE_LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_cortical_bridge_create: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(*bridge));

    if (bridge_base_init(&bridge->base, 0, "occipital_cortical") != 0) { nimcp_free(bridge); return NULL; }

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = occipital_cortical_default_config();
    }

    /* Store occipital reference */
    bridge->occipital = occipital;

    /* Initialize hypercolumns */
    init_hypercolumns(bridge);

    /* Initialize retinotopic map */
    if (init_retinotopic_map(bridge) != 0) {
        LOG_ERROR(CORTICAL_BRIDGE_LOG_MODULE, "Failed to init retinotopic map");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_cortical_bridge_create: retinotopic map init failed");
        return NULL;
    }

    /* Set timestamps */
    bridge->creation_time_us = get_time_us();
    bridge->last_update_us = bridge->creation_time_us;

    /* Generate module ID */
    bridge->module_id = BIO_MODULE_OCCIPITAL + 0x20; /* Offset for cortical bridge */

    LOG_INFO(CORTICAL_BRIDGE_LOG_MODULE,
             "Cortical bridge created with %u hypercolumns",
             bridge->num_hypercolumns);

    return bridge;
}

void occipital_cortical_bridge_destroy(occipital_cortical_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "occipital_cortical");

    LOG_INFO(CORTICAL_BRIDGE_LOG_MODULE, "Destroying cortical bridge");

    if (bridge->retinotopic_map) {
        nimcp_free(bridge->retinotopic_map);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int occipital_cortical_bridge_reset(occipital_cortical_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Reset all column activities */
    for (uint32_t h = 0; h < bridge->num_hypercolumns; h++) {
        hypercolumn_t* hc = &bridge->hypercolumns[h];
        for (uint32_t o = 0; o < hc->num_orientations; o++) {
            hc->columns[o].activity = hc->columns[o].baseline;
        }
        hc->population_activity = 0.0f;
        hc->dominant_orientation = 0.0f;
        memset(hc->color_response, 0, sizeof(hc->color_response));
    }

    /* Reset effects */
    memset(&bridge->effects, 0, sizeof(bridge->effects));

    /* Reset inflammation */
    bridge->current_inflammation = 0.0f;

    bridge->last_update_us = get_time_us();

    LOG_DEBUG(CORTICAL_BRIDGE_LOG_MODULE, "Bridge reset");

    return 0;
}

/*=============================================================================
 * CONNECTION API
 *===========================================================================*/

int occipital_cortical_connect_hypercolumns(
    occipital_cortical_bridge_t* bridge,
    orientation_hypercolumn_t* hypercolumns) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->hypercolumns_external = hypercolumns;

    LOG_INFO(CORTICAL_BRIDGE_LOG_MODULE, "Connected to external hypercolumns");

    return 0;
}

int occipital_cortical_connect_topographic_map(
    occipital_cortical_bridge_t* bridge,
    topographic_map_t* map) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->topographic_map = map;

    LOG_INFO(CORTICAL_BRIDGE_LOG_MODULE, "Connected to topographic map");

    return 0;
}

int occipital_cortical_connect_immune(
    occipital_cortical_bridge_t* bridge,
    cortical_immune_system_t* immune) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->immune_system = immune;

    LOG_INFO(CORTICAL_BRIDGE_LOG_MODULE, "Connected to cortical immune system");

    return 0;
}

int occipital_cortical_bridge_register_bio_async(
    occipital_cortical_bridge_t* bridge,
    struct bio_router_struct* router) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->router = router;

    if (router) {
        LOG_INFO(CORTICAL_BRIDGE_LOG_MODULE, "Registered with bio-async router");
    }

    return 0;
}

/*=============================================================================
 * PROCESSING API
 *===========================================================================*/

int occipital_cortical_map_visual_to_cortical(
    occipital_cortical_bridge_t* bridge,
    float visual_x, float visual_y,
    float* cortical_x, float* cortical_y) {

    if (!bridge || !cortical_x || !cortical_y) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cortical_map_visual_to_cortical: required parameter is NULL");
        return -1;
    }

    if (bridge->config.map_mode == OCC_MAP_LOG_POLAR) {
        log_polar_map(visual_x, visual_y,
                      bridge->config.foveal_magnification,
                      cortical_x, cortical_y);
    } else {
        *cortical_x = visual_x;
        *cortical_y = visual_y;
    }

    bridge->stats.retinotopic_mappings++;

    return 0;
}

int occipital_cortical_bridge_process(occipital_cortical_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Get visual features from occipital adapter */
    /* In a full implementation, this would query the adapter for edge/orientation data */

    /* For now, simulate with test pattern */
    float test_orientation = 0.5f; /* ~30 degrees */
    float test_contrast = 0.8f;

    /* Update each hypercolumn */
    float total_activity = 0.0f;
    uint32_t active_count = 0;

    for (uint32_t h = 0; h < bridge->num_hypercolumns; h++) {
        hypercolumn_t* hc = &bridge->hypercolumns[h];

        /* Simulate edge/orientation input at this location */
        float local_orientation = test_orientation +
            0.1f * sinf(hc->visual_x * 10.0f) * cosf(hc->visual_y * 10.0f);

        /* Update each orientation column */
        float max_response = 0.0f;
        float max_orientation = 0.0f;
        float pop_activity = 0.0f;

        for (uint32_t o = 0; o < hc->num_orientations; o++) {
            orientation_column_t* oc = &hc->columns[o];

            /* Calculate response */
            float response = orientation_response(
                local_orientation,
                oc->preferred_orientation,
                oc->tuning_width
            ) * test_contrast * bridge->config.contrast_gain;

            /* Apply immune modulation if enabled */
            if (bridge->config.enable_cortical_immune &&
                bridge->current_inflammation > 0.0f) {
                response *= (1.0f - 0.3f * bridge->current_inflammation);
            }

            oc->activity = nimcp_clampf(response + oc->baseline, 0.0f, 1.0f);
            pop_activity += oc->activity;

            if (oc->activity > max_response) {
                max_response = oc->activity;
                max_orientation = oc->preferred_orientation;
            }
        }

        hc->population_activity = pop_activity / (float)hc->num_orientations;
        hc->dominant_orientation = max_orientation;

        if (hc->population_activity > 0.1f) {
            active_count++;
            total_activity += hc->population_activity;
            bridge->stats.columns_activated++;
        }

        bridge->stats.hypercolumn_updates++;
    }

    /* Apply lateral inhibition */
    apply_lateral_inhibition(bridge);

    /* Update effects */
    if (active_count > 0) {
        bridge->effects.mean_column_activity = total_activity / (float)active_count;
    } else {
        bridge->effects.mean_column_activity = 0.0f;
    }

    return 0;
}

int occipital_cortical_bridge_update(occipital_cortical_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    uint64_t now = get_time_us();

    /* Calculate overall statistics */
    float max_activity = 0.0f;
    float total_selectivity = 0.0f;
    uint32_t active_count = 0;

    for (uint32_t h = 0; h < bridge->num_hypercolumns; h++) {
        hypercolumn_t* hc = &bridge->hypercolumns[h];

        if (hc->population_activity > max_activity) {
            max_activity = hc->population_activity;
        }

        if (hc->population_activity > 0.1f) {
            active_count++;

            /* Calculate orientation selectivity (variance of responses) */
            float mean = hc->population_activity;
            float variance = 0.0f;
            for (uint32_t o = 0; o < hc->num_orientations; o++) {
                float diff = hc->columns[o].activity - mean;
                variance += diff * diff;
            }
            variance /= (float)hc->num_orientations;
            total_selectivity += sqrtf(variance);
        }
    }

    /* Update effects */
    bridge->effects.max_column_activity = max_activity;
    if (active_count > 0) {
        bridge->effects.orientation_selectivity = total_selectivity / (float)active_count;
    }

    /* Foveal enhancement based on center activity */
    float foveal_activity = 0.0f;
    float peripheral_activity = 0.0f;
    uint32_t foveal_count = 0;
    uint32_t peripheral_count = 0;

    for (uint32_t h = 0; h < bridge->num_hypercolumns; h++) {
        hypercolumn_t* hc = &bridge->hypercolumns[h];
        float eccentricity = sqrtf(
            (hc->visual_x - 0.5f) * (hc->visual_x - 0.5f) +
            (hc->visual_y - 0.5f) * (hc->visual_y - 0.5f)
        );

        if (eccentricity < 0.2f) {
            foveal_activity += hc->population_activity;
            foveal_count++;
        } else {
            peripheral_activity += hc->population_activity;
            peripheral_count++;
        }
    }

    if (foveal_count > 0) {
        bridge->effects.foveal_enhancement =
            foveal_activity / (float)foveal_count;
    }
    if (peripheral_count > 0) {
        bridge->effects.peripheral_suppression =
            1.0f - (peripheral_activity / (float)peripheral_count);
    }

    /* Immune effects */
    bridge->effects.inflammation_level = bridge->current_inflammation;
    bridge->effects.immune_modulation =
        1.0f - 0.3f * bridge->current_inflammation;

    /* Processing efficiency */
    bridge->effects.processing_efficiency =
        bridge->effects.mean_column_activity * bridge->effects.immune_modulation;

    /* Signal quality based on selectivity */
    bridge->effects.signal_quality =
        nimcp_clampf(bridge->effects.orientation_selectivity * 2.0f, 0.0f, 1.0f);

    /* Update average stats */
    if (active_count > 0) {
        bridge->stats.avg_column_activity = bridge->effects.mean_column_activity;
        bridge->stats.avg_orientation_response = bridge->effects.orientation_selectivity;
    }

    bridge->last_update_us = now;

    return 0;
}

int occipital_cortical_get_column_activity(
    const occipital_cortical_bridge_t* bridge,
    float x, float y,
    column_activity_t* activity) {

    if (!bridge || !activity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cortical_get_column_activity: required parameter is NULL");
        return -1;
    }

    /* Find nearest hypercolumn */
    float min_dist = INFINITY;
    uint32_t nearest = 0;

    for (uint32_t h = 0; h < bridge->num_hypercolumns; h++) {
        float dx = bridge->hypercolumns[h].visual_x - x;
        float dy = bridge->hypercolumns[h].visual_y - y;
        float dist = dx * dx + dy * dy;
        if (dist < min_dist) {
            min_dist = dist;
            nearest = h;
        }
    }

    const hypercolumn_t* hc = &bridge->hypercolumns[nearest];

    activity->column_id = nearest;
    activity->type = hc->is_color_blob ? OCC_COLUMN_COLOR_BLOB : OCC_COLUMN_ORIENTATION;
    activity->position_x = hc->visual_x;
    activity->position_y = hc->visual_y;
    activity->preferred_orientation = hc->dominant_orientation;
    activity->activity = hc->population_activity;
    activity->tuning_width = M_PI / (float)hc->num_orientations;
    activity->eye_dominance = hc->columns[0].eye_dominance;

    return 0;
}

int occipital_cortical_get_hypercolumn_response(
    const occipital_cortical_bridge_t* bridge,
    uint32_t hypercolumn_id,
    hypercolumn_response_t* response) {

    if (!bridge || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cortical_get_hypercolumn_response: required parameter is NULL");
        return -1;
    }
    if (hypercolumn_id >= bridge->num_hypercolumns) return -1;

    const hypercolumn_t* hc = &bridge->hypercolumns[hypercolumn_id];

    response->hypercolumn_id = hypercolumn_id;
    response->position_x = hc->visual_x;
    response->position_y = hc->visual_y;
    response->num_orientations = hc->num_orientations;
    response->dominant_orientation = hc->dominant_orientation;
    response->population_activity = hc->population_activity;

    for (uint32_t o = 0; o < hc->num_orientations && o < 16; o++) {
        response->orientation_responses[o] = hc->columns[o].activity;
    }

    memcpy(response->color_response, hc->color_response,
           sizeof(hc->color_response));

    return 0;
}

int occipital_cortical_bridge_get_effects(
    const occipital_cortical_bridge_t* bridge,
    occipital_cortical_effects_t* effects) {

    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cortical_bridge_get_effects: required parameter is NULL");
        return -1;
    }

    *effects = bridge->effects;

    return 0;
}

/*=============================================================================
 * QUERY API
 *===========================================================================*/

int occipital_cortical_bridge_get_stats(
    const occipital_cortical_bridge_t* bridge,
    occipital_cortical_stats_t* stats) {

    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cortical_bridge_get_stats: required parameter is NULL");
        return -1;
    }

    *stats = bridge->stats;

    return 0;
}

void occipital_cortical_bridge_reset_stats(occipital_cortical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cortical_bridge_reset_stats: bridge is NULL");
        return;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

bool occipital_cortical_is_hypercolumns_connected(
    const occipital_cortical_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->hypercolumns_external != NULL;
}

bool occipital_cortical_is_map_connected(
    const occipital_cortical_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->topographic_map != NULL;
}

int occipital_cortical_bridge_get_config(
    const occipital_cortical_bridge_t* bridge,
    occipital_cortical_config_t* config) {

    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cortical_bridge_get_config: required parameter is NULL");
        return -1;
    }

    *config = bridge->config;

    return 0;
}

uint32_t occipital_cortical_get_active_column_count(
    const occipital_cortical_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cortical_get_active_column_count: bridge is NULL");
        return 0;
    }

    uint32_t count = 0;
    for (uint32_t h = 0; h < bridge->num_hypercolumns; h++) {
        if (bridge->hypercolumns[h].population_activity > 0.1f) {
            count++;
        }
    }

    return count;
}
