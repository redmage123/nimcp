/**
 * @file nimcp_occipital_substrate_bridge.c
 * @brief Bridge between Occipital Cortex and neural substrate
 *
 * WHAT: Links occipital visual cortex (V1-V5) to metabolic state
 * WHY: Visual cortex has extremely high metabolic demands (20% of brain glucose)
 * HOW: Monitors ATP/fatigue; modulates visual processing at each hierarchical level
 *
 * @author NIMCP Team
 * @date 2025-01-01
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/occipital/nimcp_occipital_substrate_bridge.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(occipital_substrate_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "OCCIPITAL_SUBSTRATE_BRIDGE"


/*=============================================================================
 * Internal Structure
 *===========================================================================*/

struct occipital_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* occipital;                          /**< Occipital adapter handle */
    neural_substrate_t* substrate;            /**< Neural substrate handle */
    occipital_substrate_config_t config;      /**< Bridge configuration */
    occipital_substrate_effects_t effects;    /**< Current metabolic effects */
    occipital_substrate_stats_t stats;        /**< Runtime statistics */
    bio_router_t* router;                     /**< Bio-async router */
    bool bio_async_connected;                 /**< Bio-async registration flag */
    float last_atp;                           /**< Last ATP reading */
    float last_fatigue;                       /**< Last fatigue reading */
};

/*=============================================================================
 * Helper Functions
 *===========================================================================*/

static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

/**
 * @brief Update running average statistic
 */
static float update_avg(float old_avg, float new_val, float alpha) {
    float result = (1.0f - alpha) * old_avg + alpha * new_val;
    return isfinite(result) ? result : old_avg;
}

/**
 * @brief Compute V1 effects based on metabolic state
 * V1 is most ATP-dependent as it's the primary processing stage
 */
static void compute_v1_effects(occipital_substrate_bridge_t* bridge,
                               float atp, float fatigue) {
    float min_cap = bridge->config.min_capacity;
    float atp_sens = bridge->config.atp_sensitivity;
    float fatigue_sens = bridge->config.fatigue_sensitivity;
    float v1_atp_w = bridge->config.v1_atp_weight;
    float v1_fat_w = bridge->config.v1_fatigue_weight;

    /* V1 contrast sensitivity: Highest ATP dependency */
    float atp_factor = atp * atp_sens * v1_atp_w;
    float fatigue_factor = (1.0f - fatigue * fatigue_sens * v1_fat_w);
    bridge->effects.v1_contrast_sensitivity = nimcp_clampf(atp_factor * fatigue_factor, min_cap, 1.0f);

    /* V1 orientation selectivity: Slightly less ATP dependent */
    atp_factor = atp * atp_sens * (v1_atp_w * 0.9f);
    bridge->effects.v1_orientation_selectivity = nimcp_clampf(atp_factor * fatigue_factor, min_cap, 1.0f);

    /* V1 spatial frequency: Affected by both, moderate sensitivity */
    atp_factor = atp * atp_sens * (v1_atp_w * 0.85f);
    fatigue_factor = (1.0f - fatigue * fatigue_sens * (v1_fat_w * 0.9f));
    bridge->effects.v1_spatial_frequency = nimcp_clampf(atp_factor * fatigue_factor, min_cap, 1.0f);
}

/**
 * @brief Compute V2 effects based on metabolic state
 * V2 handles contour integration and texture - moderate ATP dependency
 */
static void compute_v2_effects(occipital_substrate_bridge_t* bridge,
                               float atp, float fatigue) {
    float min_cap = bridge->config.min_capacity;
    float atp_sens = bridge->config.atp_sensitivity;
    float fatigue_sens = bridge->config.fatigue_sensitivity;

    /* V2 is less ATP-dependent than V1, uses ~80% of V1 weights */
    float v2_atp_w = bridge->config.v1_atp_weight * 0.8f;
    float v2_fat_w = bridge->config.v1_fatigue_weight * 1.1f;  /* Slightly more fatigue-sensitive */

    float atp_factor = atp * atp_sens * v2_atp_w;
    float fatigue_factor = (1.0f - fatigue * fatigue_sens * v2_fat_w);

    bridge->effects.v2_contour_integration = nimcp_clampf(atp_factor * fatigue_factor, min_cap, 1.0f);
    bridge->effects.v2_texture_processing = nimcp_clampf(atp_factor * fatigue_factor * 0.95f, min_cap, 1.0f);
    bridge->effects.v2_figure_ground = nimcp_clampf(atp_factor * fatigue_factor * 0.9f, min_cap, 1.0f);
}

/**
 * @brief Compute V3 effects based on metabolic state
 * V3 processes dynamic form - moderate sensitivity
 */
static void compute_v3_effects(occipital_substrate_bridge_t* bridge,
                               float atp, float fatigue) {
    float min_cap = bridge->config.min_capacity;
    float atp_sens = bridge->config.atp_sensitivity;
    float fatigue_sens = bridge->config.fatigue_sensitivity;

    /* V3 is at dorsal/ventral split - balanced sensitivity */
    float v3_atp_w = bridge->config.dorsal_atp_weight * 0.5f + bridge->config.v4_atp_weight * 0.5f;
    float v3_fat_w = bridge->config.v5_fatigue_weight * 0.5f + bridge->config.ventral_fatigue_weight * 0.5f;

    float atp_factor = atp * atp_sens * v3_atp_w;
    float fatigue_factor = (1.0f - fatigue * fatigue_sens * v3_fat_w);

    bridge->effects.v3_dynamic_form = nimcp_clampf(atp_factor * fatigue_factor, min_cap, 1.0f);
}

/**
 * @brief Compute V4 effects based on metabolic state
 * V4 handles color constancy and complex form - ventral stream
 */
static void compute_v4_effects(occipital_substrate_bridge_t* bridge,
                               float atp, float fatigue) {
    float min_cap = bridge->config.min_capacity;
    float atp_sens = bridge->config.atp_sensitivity;
    float fatigue_sens = bridge->config.fatigue_sensitivity;
    float v4_atp_w = bridge->config.v4_atp_weight;
    float v4_fat_w = bridge->config.v4_fatigue_weight;
    float ventral_fat_w = bridge->config.ventral_fatigue_weight;

    float atp_factor = atp * atp_sens * v4_atp_w;
    float fatigue_factor = (1.0f - fatigue * fatigue_sens * v4_fat_w);

    /* Color constancy is particularly fatigue-sensitive */
    float color_fatigue = (1.0f - fatigue * fatigue_sens * v4_fat_w * 1.2f);
    bridge->effects.v4_color_constancy = nimcp_clampf(atp_factor * color_fatigue, min_cap, 1.0f);

    bridge->effects.v4_complex_form = nimcp_clampf(atp_factor * fatigue_factor, min_cap, 1.0f);

    /* Object recognition uses ventral stream fatigue weight */
    float obj_fatigue = (1.0f - fatigue * fatigue_sens * ventral_fat_w);
    bridge->effects.v4_object_recognition = nimcp_clampf(atp_factor * obj_fatigue, min_cap, 1.0f);
}

/**
 * @brief Compute V5/MT effects based on metabolic state
 * V5/MT handles motion detection - dorsal stream
 */
static void compute_v5_effects(occipital_substrate_bridge_t* bridge,
                               float atp, float fatigue) {
    float min_cap = bridge->config.min_capacity;
    float atp_sens = bridge->config.atp_sensitivity;
    float fatigue_sens = bridge->config.fatigue_sensitivity;
    float v5_atp_w = bridge->config.v5_atp_weight;
    float v5_fat_w = bridge->config.v5_fatigue_weight;
    float dorsal_atp_w = bridge->config.dorsal_atp_weight;

    float atp_factor = atp * atp_sens * v5_atp_w;
    float fatigue_factor = (1.0f - fatigue * fatigue_sens * v5_fat_w);

    bridge->effects.v5_motion_direction = nimcp_clampf(atp_factor * fatigue_factor, min_cap, 1.0f);

    /* Optic flow integration uses dorsal stream weight */
    float optic_atp = atp * atp_sens * dorsal_atp_w;
    bridge->effects.v5_optic_flow = nimcp_clampf(optic_atp * fatigue_factor, min_cap, 1.0f);

    /* Speed tuning is slightly less sensitive */
    bridge->effects.v5_speed_tuning = nimcp_clampf(atp_factor * fatigue_factor * 0.95f, min_cap, 1.0f);

    /* Motion coherence is most fatigue-sensitive in V5 */
    float coherence_fatigue = (1.0f - fatigue * fatigue_sens * v5_fat_w * 1.3f);
    bridge->effects.v5_motion_coherence = nimcp_clampf(atp_factor * coherence_fatigue, min_cap, 1.0f);
}

/**
 * @brief Compute layer-specific gains
 * Cortical layers have different metabolic profiles
 */
static void compute_layer_effects(occipital_substrate_bridge_t* bridge,
                                  float atp, float fatigue) {
    if (!bridge->config.enable_layer_modulation) {
        bridge->effects.layer_4_gain = 1.0f;
        bridge->effects.layer_23_gain = 1.0f;
        bridge->effects.layer_56_gain = 1.0f;
        return;
    }

    float min_cap = bridge->config.min_capacity;
    float atp_sens = bridge->config.atp_sensitivity;
    float fatigue_sens = bridge->config.fatigue_sensitivity;

    /* Layer 4: Highest ATP dependency (receives thalamic input) */
    float layer4_atp = atp * atp_sens * 1.3f;
    float layer4_fatigue = (1.0f - fatigue * fatigue_sens * 0.7f);
    bridge->effects.layer_4_gain = nimcp_clampf(layer4_atp * layer4_fatigue, min_cap, 1.0f);

    /* Layers 2/3: Lateral processing, moderate ATP, higher fatigue sensitivity */
    float layer23_atp = atp * atp_sens * 1.0f;
    float layer23_fatigue = (1.0f - fatigue * fatigue_sens * 1.2f);
    bridge->effects.layer_23_gain = nimcp_clampf(layer23_atp * layer23_fatigue, min_cap, 1.0f);

    /* Layers 5/6: Output and feedback, lower ATP dependency */
    float layer56_atp = atp * atp_sens * 0.9f;
    float layer56_fatigue = (1.0f - fatigue * fatigue_sens * 1.0f);
    bridge->effects.layer_56_gain = nimcp_clampf(layer56_atp * layer56_fatigue, min_cap, 1.0f);
}

/**
 * @brief Compute global effects
 */
static void compute_global_effects(occipital_substrate_bridge_t* bridge,
                                   float atp, float fatigue) {
    float min_cap = bridge->config.min_capacity;
    float atp_sens = bridge->config.atp_sensitivity;
    float fatigue_sens = bridge->config.fatigue_sensitivity;

    /* Attention capacity: Affected by both ATP and fatigue */
    float attention_atp = atp * atp_sens * 1.0f;
    float attention_fatigue = (1.0f - fatigue * fatigue_sens * 1.1f);
    bridge->effects.attention_capacity = nimcp_clampf(attention_atp * attention_fatigue, min_cap, 1.0f);

    /* Processing speed: More affected by fatigue than ATP */
    float speed_atp = atp * atp_sens * 0.8f;
    float speed_fatigue = (1.0f - fatigue * fatigue_sens * 1.4f);
    bridge->effects.processing_speed = nimcp_clampf(speed_atp * speed_fatigue * 0.9f + 0.1f, min_cap, 1.0f);

    /* Overall capacity: Weighted average of all areas */
    float v1_avg = (bridge->effects.v1_contrast_sensitivity +
                    bridge->effects.v1_orientation_selectivity +
                    bridge->effects.v1_spatial_frequency) / 3.0f;
    float v2_avg = (bridge->effects.v2_contour_integration +
                    bridge->effects.v2_texture_processing +
                    bridge->effects.v2_figure_ground) / 3.0f;
    float v4_avg = (bridge->effects.v4_color_constancy +
                    bridge->effects.v4_complex_form +
                    bridge->effects.v4_object_recognition) / 3.0f;
    float v5_avg = (bridge->effects.v5_motion_direction +
                    bridge->effects.v5_optic_flow +
                    bridge->effects.v5_speed_tuning +
                    bridge->effects.v5_motion_coherence) / 4.0f;

    /* V1 weighted more heavily as it's foundational */
    bridge->effects.overall_capacity = (v1_avg * 0.35f +
                                        v2_avg * 0.20f +
                                        bridge->effects.v3_dynamic_form * 0.10f +
                                        v4_avg * 0.15f +
                                        v5_avg * 0.20f);
}

/**
 * @brief Update statistics
 */
static void update_stats(occipital_substrate_bridge_t* bridge, float atp, float fatigue) {
    const float alpha = 0.1f;

    bridge->stats.updates_processed++;

    /* Track low ATP events */
    if (atp < OCCIPITAL_ATP_REDUCED) {
        bridge->stats.low_atp_events++;
    }
    if (atp < OCCIPITAL_ATP_CRITICAL) {
        bridge->stats.critical_atp_events++;
    }

    /* Track high fatigue events */
    if (fatigue > OCCIPITAL_FATIGUE_MODERATE) {
        bridge->stats.high_fatigue_events++;
    }

    /* Update running averages */
    bridge->stats.avg_v1_sensitivity = update_avg(
        bridge->stats.avg_v1_sensitivity,
        bridge->effects.v1_contrast_sensitivity, alpha);

    bridge->stats.avg_v4_color = update_avg(
        bridge->stats.avg_v4_color,
        bridge->effects.v4_color_constancy, alpha);

    bridge->stats.avg_v5_motion = update_avg(
        bridge->stats.avg_v5_motion,
        bridge->effects.v5_motion_direction, alpha);

    bridge->stats.avg_overall_capacity = update_avg(
        bridge->stats.avg_overall_capacity,
        bridge->effects.overall_capacity, alpha);

    /* Track extremes */
    if (bridge->effects.overall_capacity < bridge->stats.min_observed_capacity ||
        bridge->stats.updates_processed == 1) {
        bridge->stats.min_observed_capacity = bridge->effects.overall_capacity;
    }
    if (fatigue > bridge->stats.max_observed_fatigue) {
        bridge->stats.max_observed_fatigue = fatigue;
    }
}

/*=============================================================================
 * Configuration API
 *===========================================================================*/

occipital_substrate_config_t occipital_substrate_default_config(void) {
    occipital_substrate_config_t config = {
        /* Enable/disable toggles */
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = false,
        .enable_layer_modulation = true,

        /* Sensitivity parameters */
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.2f,

        /* V1 weights (highest ATP, lower fatigue sensitivity) */
        .v1_atp_weight = 1.2f,
        .v1_fatigue_weight = 0.8f,

        /* V4 weights (color - moderate ATP, higher fatigue) */
        .v4_atp_weight = 1.0f,
        .v4_fatigue_weight = 1.1f,

        /* V5 weights (motion - lower ATP, highest fatigue) */
        .v5_atp_weight = 0.9f,
        .v5_fatigue_weight = 1.3f,

        /* Stream weights */
        .dorsal_atp_weight = 0.95f,
        .ventral_fatigue_weight = 1.15f
    };
    return config;
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

occipital_substrate_bridge_t* occipital_substrate_bridge_create(
    void* occipital,
    neural_substrate_t* substrate,
    const occipital_substrate_config_t* config)
{
    /* NULL substrate is allowed - uses default metabolic values for testing */

    occipital_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(occipital_substrate_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    if (bridge_base_init(&bridge->base, 0, "occipital_substrate") != 0) { nimcp_free(bridge); return NULL; }

    bridge->occipital = occipital;
    bridge->substrate = substrate;
    bridge->config = config ? *config : occipital_substrate_default_config();
    bridge->router = NULL;
    bridge->bio_async_connected = false;
    bridge->last_atp = 1.0f;
    bridge->last_fatigue = 0.0f;

    /* Initialize effects to full capacity */
    bridge->effects.v1_contrast_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->effects.v1_orientation_selectivity = 1.0f;
    bridge->effects.v1_spatial_frequency = 1.0f;
    bridge->effects.v2_contour_integration = 1.0f;
    bridge->effects.v2_texture_processing = 1.0f;
    bridge->effects.v2_figure_ground = 1.0f;
    bridge->effects.v3_dynamic_form = 1.0f;
    bridge->effects.v4_color_constancy = 1.0f;
    bridge->effects.v4_complex_form = 1.0f;
    bridge->effects.v4_object_recognition = 1.0f;
    bridge->effects.v5_motion_direction = 1.0f;
    bridge->effects.v5_optic_flow = 1.0f;
    bridge->effects.v5_speed_tuning = 1.0f;
    bridge->effects.v5_motion_coherence = 1.0f;
    bridge->effects.attention_capacity = 1.0f;
    bridge->effects.processing_speed = 1.0f;
    bridge->effects.overall_capacity = 1.0f;
    bridge->effects.layer_4_gain = 1.0f;
    bridge->effects.layer_23_gain = 1.0f;
    bridge->effects.layer_56_gain = 1.0f;

    /* Initialize stats */
    memset(&bridge->stats, 0, sizeof(occipital_substrate_stats_t));
    bridge->stats.min_observed_capacity = 1.0f;

    return bridge;
}

void occipital_substrate_bridge_destroy(occipital_substrate_bridge_t* bridge) {
    if (bridge) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
    }
}

int occipital_substrate_bridge_reset(occipital_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Reset effects to full capacity */
    bridge->effects.v1_contrast_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->effects.v1_orientation_selectivity = 1.0f;
    bridge->effects.v1_spatial_frequency = 1.0f;
    bridge->effects.v2_contour_integration = 1.0f;
    bridge->effects.v2_texture_processing = 1.0f;
    bridge->effects.v2_figure_ground = 1.0f;
    bridge->effects.v3_dynamic_form = 1.0f;
    bridge->effects.v4_color_constancy = 1.0f;
    bridge->effects.v4_complex_form = 1.0f;
    bridge->effects.v4_object_recognition = 1.0f;
    bridge->effects.v5_motion_direction = 1.0f;
    bridge->effects.v5_optic_flow = 1.0f;
    bridge->effects.v5_speed_tuning = 1.0f;
    bridge->effects.v5_motion_coherence = 1.0f;
    bridge->effects.attention_capacity = 1.0f;
    bridge->effects.processing_speed = 1.0f;
    bridge->effects.overall_capacity = 1.0f;
    bridge->effects.layer_4_gain = 1.0f;
    bridge->effects.layer_23_gain = 1.0f;
    bridge->effects.layer_56_gain = 1.0f;

    bridge->last_atp = 1.0f;
    bridge->last_fatigue = 0.0f;

    return 0;
}

/*=============================================================================
 * Update and Effects API
 *===========================================================================*/

int occipital_substrate_bridge_update(occipital_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    float atp = 1.0f;      /* Default full ATP */
    float fatigue = 0.0f;  /* Default no fatigue */

    /* Get metabolic state from substrate if available */
    if (bridge->substrate) {
        substrate_metabolic_state_t metabolic;
        if (substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
            atp = metabolic.atp_level;
            fatigue = 1.0f - metabolic.metabolic_capacity;  /* Convert capacity to fatigue */
        }
    }

    /* Store for reference */
    bridge->last_atp = atp;
    bridge->last_fatigue = fatigue;

    /* Compute effects for each visual area */
    if (bridge->config.enable_atp_modulation || bridge->config.enable_fatigue_modulation) {
        compute_v1_effects(bridge, atp, fatigue);
        compute_v2_effects(bridge, atp, fatigue);
        compute_v3_effects(bridge, atp, fatigue);
        compute_v4_effects(bridge, atp, fatigue);
        compute_v5_effects(bridge, atp, fatigue);
        compute_layer_effects(bridge, atp, fatigue);
        compute_global_effects(bridge, atp, fatigue);
    }

    /* Update statistics */
    update_stats(bridge, atp, fatigue);

    return 0;
}

int occipital_substrate_bridge_get_effects(
    const occipital_substrate_bridge_t* bridge,
    occipital_substrate_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_substrate_bridge_get_effects: required parameter is NULL");
        return -1;
    }
    *effects = bridge->effects;
    return 0;
}

int occipital_substrate_bridge_apply_effects(occipital_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* In a full implementation, this would call occipital adapter functions
     * to apply the metabolic effects. For now, we just validate the bridge. */
    (void)bridge->occipital;

    return 0;
}

float occipital_substrate_bridge_get_area_capacity(
    const occipital_substrate_bridge_t* bridge,
    uint32_t area)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_substrate_bridge_get_area_capacity: bridge is NULL");
        return -1.0f;
    }

    switch (area) {
        case 0: /* V1 */
            return (bridge->effects.v1_contrast_sensitivity +
                    bridge->effects.v1_orientation_selectivity +
                    bridge->effects.v1_spatial_frequency) / 3.0f;
        case 1: /* V2 */
            return (bridge->effects.v2_contour_integration +
                    bridge->effects.v2_texture_processing +
                    bridge->effects.v2_figure_ground) / 3.0f;
        case 2: /* V3 */
            return bridge->effects.v3_dynamic_form;
        case 3: /* V4 */
            return (bridge->effects.v4_color_constancy +
                    bridge->effects.v4_complex_form +
                    bridge->effects.v4_object_recognition) / 3.0f;
        case 4: /* V5 */
            return (bridge->effects.v5_motion_direction +
                    bridge->effects.v5_optic_flow +
                    bridge->effects.v5_speed_tuning +
                    bridge->effects.v5_motion_coherence) / 4.0f;
        default:
            return -1.0f;
    }
}

/*=============================================================================
 * Bio-Async Communication
 *===========================================================================*/

int occipital_substrate_bridge_register_bio_async(
    occipital_substrate_bridge_t* bridge,
    bio_router_t* router)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->router = router;
    bridge->bio_async_connected = (router != NULL);

    if (router != NULL) {
        /* Would register message handlers here:
         * - BIO_MSG_METABOLIC_STATE
         * - BIO_MSG_FATIGUE_UPDATE
         * - BIO_MSG_ATP_CRITICAL
         */
    }

    return 0;
}

int occipital_substrate_bridge_broadcast_capacity(
    occipital_substrate_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->bio_async_connected || !bridge->router) {
        return 0;  /* Not connected, not an error */
    }

    /* Would broadcast visual capacity message here */
    bridge->stats.bio_messages_sent++;

    return 0;
}

int occipital_substrate_bridge_process_messages(
    occipital_substrate_bridge_t* bridge,
    uint32_t max_messages)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->bio_async_connected || !bridge->router) {
        return 0;
    }

    /* Would process pending bio-async messages here */
    (void)max_messages;

    return 0;
}

/*=============================================================================
 * Statistics API
 *===========================================================================*/

int occipital_substrate_bridge_get_stats(
    const occipital_substrate_bridge_t* bridge,
    occipital_substrate_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_substrate_bridge_get_stats: required parameter is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void occipital_substrate_bridge_reset_stats(occipital_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_substrate_bridge_reset_stats: bridge is NULL");
        return;
    }
    memset(&bridge->stats, 0, sizeof(occipital_substrate_stats_t));
    bridge->stats.min_observed_capacity = 1.0f;
}

/*=============================================================================
 * Query API
 *===========================================================================*/

bool occipital_substrate_bridge_is_impaired(
    const occipital_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->effects.overall_capacity < 0.5f;
}

bool occipital_substrate_bridge_is_atp_critical(
    const occipital_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->last_atp < OCCIPITAL_ATP_CRITICAL;
}

int occipital_substrate_bridge_get_config(
    const occipital_substrate_bridge_t* bridge,
    occipital_substrate_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_substrate_bridge_get_config: required parameter is NULL");
        return -1;
    }
    *config = bridge->config;
    return 0;
}
