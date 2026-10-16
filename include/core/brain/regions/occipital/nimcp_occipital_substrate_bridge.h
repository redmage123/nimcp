/**
 * @file nimcp_occipital_substrate_bridge.h
 * @brief Bridge between Occipital Cortex and neural substrate
 *
 * WHAT: Links occipital visual cortex (V1-V5) to metabolic state
 * WHY: Visual cortex has extremely high metabolic demands (20% of brain glucose)
 * HOW: Monitors ATP/fatigue; modulates visual processing at each hierarchical level
 *
 * BIOLOGICAL BASIS:
 * - V1 (Primary Visual): Highest metabolic demand, edge/orientation processing
 * - V2 (Secondary Visual): Contour integration, texture processing
 * - V3 (Dorsal-Ventral Split): Dynamic form processing
 * - V4 (Ventral Stream): Color constancy, complex form recognition
 * - V5/MT (Dorsal Stream): Motion detection, optic flow computation
 *
 * METABOLIC EFFECTS:
 * - Low ATP: Reduced contrast sensitivity, orientation selectivity impaired
 * - High Fatigue: Motion detection slowed, color constancy disrupted
 * - Severe Depletion: Visual field constriction, processing delays
 *
 * LAYER-SPECIFIC EFFECTS:
 * - Layer 4 (Granular): Highest ATP dependency (thalamic input)
 * - Layers 2/3: Lateral processing affected by fatigue
 * - Layers 5/6: Feedback to thalamus modulated
 *
 * @author NIMCP Team
 * @date 2025-01-01
 */

#ifndef NIMCP_OCCIPITAL_SUBSTRATE_BRIDGE_H
#define NIMCP_OCCIPITAL_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Bio-async module ID for occipital substrate bridge */
#define BIO_MODULE_SUBSTRATE_OCCIPITAL 0x2D10

/** ATP thresholds for visual processing */
#define OCCIPITAL_ATP_FULL         0.8f   /**< Full visual capacity */
#define OCCIPITAL_ATP_REDUCED      0.5f   /**< Reduced but functional */
#define OCCIPITAL_ATP_CRITICAL     0.3f   /**< Severely impaired */

/** Fatigue thresholds */
#define OCCIPITAL_FATIGUE_LOW      0.3f   /**< Minimal impact */
#define OCCIPITAL_FATIGUE_MODERATE 0.5f   /**< Noticeable degradation */
#define OCCIPITAL_FATIGUE_HIGH     0.7f   /**< Significant impairment */

/*=============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Metabolic effects on visual processing hierarchy
 *
 * Each field represents the modulation factor [0-1] for that visual area.
 * 1.0 = full capacity, lower values indicate metabolic impairment.
 */
typedef struct {
    /* V1 Primary Visual Cortex Effects */
    float v1_contrast_sensitivity;    /**< Edge detection sensitivity [0-1] */
    float v1_orientation_selectivity; /**< Orientation column precision [0-1] */
    float v1_spatial_frequency;       /**< Spatial frequency response [0-1] */

    /* V2 Secondary Visual Cortex Effects */
    float v2_contour_integration;     /**< Border ownership processing [0-1] */
    float v2_texture_processing;      /**< Texture segregation [0-1] */
    float v2_figure_ground;           /**< Figure-ground separation [0-1] */

    /* V3 Dynamic Form Processing */
    float v3_dynamic_form;            /**< Dynamic shape processing [0-1] */

    /* V4 Ventral Stream Effects */
    float v4_color_constancy;         /**< Illumination-invariant color [0-1] */
    float v4_complex_form;            /**< Complex shape processing [0-1] */
    float v4_object_recognition;      /**< Object feature extraction [0-1] */

    /* V5/MT Dorsal Stream Effects */
    float v5_motion_direction;        /**< Motion direction selectivity [0-1] */
    float v5_optic_flow;              /**< Global motion integration [0-1] */
    float v5_speed_tuning;            /**< Speed discrimination [0-1] */
    float v5_motion_coherence;        /**< Motion coherence detection [0-1] */

    /* Attention and Global Effects */
    float attention_capacity;         /**< Available attentional resources [0-1] */
    float processing_speed;           /**< Overall processing latency factor [0-1] */
    float overall_capacity;           /**< Combined visual capacity [0-1] */

    /* Layer-specific modulation (for cortical column integration) */
    float layer_4_gain;               /**< Layer 4 (input layer) gain [0-1] */
    float layer_23_gain;              /**< Layers 2/3 (association) gain [0-1] */
    float layer_56_gain;              /**< Layers 5/6 (output) gain [0-1] */
} occipital_substrate_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Enable/disable toggles */
    bool enable_atp_modulation;       /**< ATP affects visual acuity */
    bool enable_fatigue_modulation;   /**< Fatigue affects processing speed */
    bool enable_bio_async;            /**< Enable bio-async messaging */
    bool enable_layer_modulation;     /**< Enable layer-specific effects */

    /* Sensitivity parameters */
    float atp_sensitivity;            /**< ATP impact multiplier [0-2] */
    float fatigue_sensitivity;        /**< Fatigue impact multiplier [0-2] */
    float min_capacity;               /**< Minimum capacity floor [0-1] */

    /* Area-specific weights */
    float v1_atp_weight;              /**< V1 ATP sensitivity (default: 1.2) */
    float v1_fatigue_weight;          /**< V1 fatigue sensitivity (default: 0.8) */
    float v4_atp_weight;              /**< V4 (color) ATP sensitivity */
    float v4_fatigue_weight;          /**< V4 fatigue sensitivity */
    float v5_atp_weight;              /**< V5 (motion) ATP sensitivity */
    float v5_fatigue_weight;          /**< V5 fatigue sensitivity */

    /* Dorsal/Ventral stream weights */
    float dorsal_atp_weight;          /**< Dorsal stream ATP dependency */
    float ventral_fatigue_weight;     /**< Ventral stream fatigue dependency */
} occipital_substrate_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates_processed;       /**< Total substrate updates */
    uint64_t low_atp_events;          /**< Times ATP dropped below threshold */
    uint64_t high_fatigue_events;     /**< Times fatigue exceeded threshold */
    uint64_t critical_atp_events;     /**< Times ATP was critically low */
    float avg_v1_sensitivity;         /**< Average V1 contrast sensitivity */
    float avg_v4_color;               /**< Average V4 color processing */
    float avg_v5_motion;              /**< Average V5 motion detection */
    float avg_overall_capacity;       /**< Average overall capacity */
    float min_observed_capacity;      /**< Minimum capacity ever observed */
    float max_observed_fatigue;       /**< Maximum fatigue ever observed */
    uint64_t bio_messages_sent;       /**< Bio-async messages broadcast */
    uint64_t bio_messages_received;   /**< Bio-async messages received */
} occipital_substrate_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct occipital_substrate_bridge occipital_substrate_bridge_t;

/*=============================================================================
 * Configuration API
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * Returns biologically-motivated defaults:
 * - V1 has higher ATP dependency (earliest processing)
 * - V5/MT has higher fatigue sensitivity (motion requires sustained attention)
 * - Ventral stream (V4) more affected by fatigue (object recognition)
 *
 * @return Default configuration
 */
occipital_substrate_config_t occipital_substrate_default_config(void);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create occipital substrate bridge
 *
 * @param occipital Occipital adapter handle (void* for flexibility)
 * @param substrate Neural substrate handle (required)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
occipital_substrate_bridge_t* occipital_substrate_bridge_create(
    void* occipital,
    neural_substrate_t* substrate,
    const occipital_substrate_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
void occipital_substrate_bridge_destroy(occipital_substrate_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int occipital_substrate_bridge_reset(occipital_substrate_bridge_t* bridge);

/*=============================================================================
 * Update and Effects API
 *===========================================================================*/

/**
 * @brief Update metabolic effects from current substrate state
 *
 * Reads ATP/fatigue from substrate, computes modulation effects for V1-V5.
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int occipital_substrate_bridge_update(occipital_substrate_bridge_t* bridge);

/**
 * @brief Get current metabolic effects
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int occipital_substrate_bridge_get_effects(
    const occipital_substrate_bridge_t* bridge,
    occipital_substrate_effects_t* effects
);

/**
 * @brief Apply metabolic effects to occipital adapter
 *
 * Modulates occipital processing parameters based on current metabolic state.
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int occipital_substrate_bridge_apply_effects(occipital_substrate_bridge_t* bridge);

/**
 * @brief Get effect for specific visual area
 *
 * @param bridge Bridge handle
 * @param area Visual area (0=V1, 1=V2, 2=V3, 3=V4, 4=V5)
 * @return Capacity factor [0-1], or -1.0f on error
 */
float occipital_substrate_bridge_get_area_capacity(
    const occipital_substrate_bridge_t* bridge,
    uint32_t area
);

/*=============================================================================
 * Bio-Async Communication
 *===========================================================================*/

/**
 * @brief Register with bio-async router
 *
 * Registers handlers for:
 * - BIO_MSG_METABOLIC_STATE: Substrate metabolic updates
 * - BIO_MSG_FATIGUE_UPDATE: Fatigue level changes
 * - BIO_MSG_ATP_CRITICAL: Critical ATP warning
 *
 * @param bridge Bridge handle
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int occipital_substrate_bridge_register_bio_async(
    occipital_substrate_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Broadcast visual capacity message
 *
 * Notifies downstream modules of current visual processing capacity.
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int occipital_substrate_bridge_broadcast_capacity(
    occipital_substrate_bridge_t* bridge
);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge handle
 * @param max_messages Max messages to process (0 = all)
 * @return Number of messages processed, -1 on error
 */
int occipital_substrate_bridge_process_messages(
    occipital_substrate_bridge_t* bridge,
    uint32_t max_messages
);

/*=============================================================================
 * Statistics API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int occipital_substrate_bridge_get_stats(
    const occipital_substrate_bridge_t* bridge,
    occipital_substrate_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void occipital_substrate_bridge_reset_stats(occipital_substrate_bridge_t* bridge);

/*=============================================================================
 * Query API
 *===========================================================================*/

/**
 * @brief Check if visual system is impaired
 *
 * @param bridge Bridge handle
 * @return true if overall capacity < 0.5, false otherwise
 */
bool occipital_substrate_bridge_is_impaired(
    const occipital_substrate_bridge_t* bridge
);

/**
 * @brief Check if ATP is critically low
 *
 * @param bridge Bridge handle
 * @return true if ATP < OCCIPITAL_ATP_CRITICAL
 */
bool occipital_substrate_bridge_is_atp_critical(
    const occipital_substrate_bridge_t* bridge
);

/**
 * @brief Get configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int occipital_substrate_bridge_get_config(
    const occipital_substrate_bridge_t* bridge,
    occipital_substrate_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCCIPITAL_SUBSTRATE_BRIDGE_H */
