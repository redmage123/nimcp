/**
 * @file nimcp_brain_regions_immune_bridge.h
 * @brief Brain Regions-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and brain regions
 * WHY:  Biological evidence shows regional-specific immune responses (neuroinflammation
 *       affects different brain regions differently). Essential for realistic modeling
 *       of region-specific sickness behavior and neurodegenerative conditions.
 * HOW:  Region-specific cytokine sensitivity, inflammation propagation across regions,
 *       abnormal regional activity triggers immune surveillance.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → BRAIN REGIONS PATHWAYS:
 * --------------------------------
 * 1. Region-Specific Cytokine Sensitivity:
 *    - Hippocampus: High IL-6 sensitivity → memory impairment
 *    - Prefrontal cortex: High IL-1β sensitivity → executive dysfunction
 *    - Motor cortex: Moderate TNF-α sensitivity → motor slowing
 *    - Sensory cortex: Low baseline sensitivity → sensory processing preserved
 *    - Reference: Barrientos et al. (2015) "Neuroinflammation in the hippocampus"
 *    - Reference: Yirmiya & Goshen (2011) "Immune modulation of memory"
 *
 * 2. Regional Inflammation Cascades:
 *    - Local inflammation can spread to connected regions
 *    - Thalamus acts as relay for inflammatory signals
 *    - Basal ganglia inflammation affects motor loops
 *    - Reference: Perry & Holmes (2014) "Microglial priming in CNS disease"
 *
 * 3. Layer-Specific Effects Within Regions:
 *    - Layer IV input disruption (thalamic relay impairment)
 *    - Layer II/III recurrent inhibition changes (E/I imbalance)
 *    - Layer V output reduction (motor/cognitive slowing)
 *    - Reference: Kettenmann et al. (2011) "Physiology of microglia"
 *
 * 4. Functional Network Disruption:
 *    - Inter-region connectivity reduced by inflammation
 *    - Default mode network affected in sickness behavior
 *    - Salience network hyperactive during immune challenge
 *    - Reference: Harrison et al. (2009) "Neural basis of sickness behavior"
 *
 * BRAIN REGIONS → IMMUNE PATHWAYS:
 * --------------------------------
 * 1. Abnormal Regional Activity as Danger Signals:
 *    - Seizure-like hyperactivity → immediate immune alert
 *    - Stroke-like hypoactivity → delayed immune response
 *    - Abnormal cross-region communication → network surveillance
 *    - Reference: Vezzani et al. (2011) "Epilepsy and neuroinflammation"
 *
 * 2. Region-Specific Pathology Detection:
 *    - Hippocampal dysfunction → memory circuit damage marker
 *    - Prefrontal abnormality → executive control threat
 *    - Motor cortex irregularity → movement disorder signal
 *    - Reference: Heneka et al. (2015) "Neuroinflammation in Alzheimer's"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  BRAIN REGIONS-IMMUNE BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               IMMUNE → BRAIN REGIONS PATHWAYS                      │  ║
 * ║   │                                                                    │  ║
 * ║   │   ┌──────────────────────────────────────────────────────────────┐│  ║
 * ║   │   │                REGION-SPECIFIC SENSITIVITY                   ││  ║
 * ║   │   │  Hippocampus:    IL-6 ↑↑, IL-1β ↑, TNF-α ↑ → Memory Loss    ││  ║
 * ║   │   │  Prefrontal:     IL-1β ↑↑, IL-6 ↑ → Executive Dysfunction   ││  ║
 * ║   │   │  Motor:          TNF-α ↑, IL-6 ↑ → Motor Slowing            ││  ║
 * ║   │   │  Sensory:        Low sensitivity → Preserved Processing      ││  ║
 * ║   │   │  Thalamus:       Relay disruption → Sensory gating loss     ││  ║
 * ║   │   │  Basal Ganglia:  IL-1β ↑ → Movement disorders               ││  ║
 * ║   │   └──────────────────────────────────────────────────────────────┘│  ║
 * ║   │                              ↓                                    │  ║
 * ║   │   ┌──────────────────────────────────────────────────────────────┐│  ║
 * ║   │   │              INFLAMMATION PROPAGATION                        ││  ║
 * ║   │   │  Source Region → Connected Regions → Systemic Spread        ││  ║
 * ║   │   └──────────────────────────────────────────────────────────────┘│  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               BRAIN REGIONS → IMMUNE PATHWAYS                      │  ║
 * ║   │                                                                    │  ║
 * ║   │   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐           │  ║
 * ║   │   │ HYPERACTIVE  │  │ HYPOACTIVE   │  │ DESYNCHR.    │           │  ║
 * ║   │   │ Seizure-like │  │ Stroke-like  │  │ Disconnection│           │  ║
 * ║   │   └──────┬───────┘  └──────┬───────┘  └──────┬───────┘           │  ║
 * ║   │          │                 │                 │                   │  ║
 * ║   │          └─────────────────┴─────────────────┘                   │  ║
 * ║   │                            ↓                                      │  ║
 * ║   │   ┌──────────────────────────────────────────────────────────────┐│  ║
 * ║   │   │           IMMUNE SURVEILLANCE & RESPONSE                     ││  ║
 * ║   │   │  Antigen Presentation → B Cell Activation → Antibody Prod   ││  ║
 * ║   │   └──────────────────────────────────────────────────────────────┘│  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 * - Bio-async integration for inter-module messaging
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_REGIONS_IMMUNE_BRIDGE_H
#define NIMCP_BRAIN_REGIONS_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "core/brain_regions/nimcp_brain_regions.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Utilities */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Region-specific cytokine sensitivity factors */
/* Hippocampus - highly sensitive to IL-6 (memory impact) */
#define REGION_HIPPOCAMPUS_IL1_SENSITIVITY     0.8f
#define REGION_HIPPOCAMPUS_IL6_SENSITIVITY     1.5f  /**< High IL-6 sensitivity */
#define REGION_HIPPOCAMPUS_TNF_SENSITIVITY     0.9f
#define REGION_HIPPOCAMPUS_IFN_SENSITIVITY     0.7f

/* Prefrontal cortex - highly sensitive to IL-1β (executive dysfunction) */
#define REGION_PREFRONTAL_IL1_SENSITIVITY      1.4f  /**< High IL-1β sensitivity */
#define REGION_PREFRONTAL_IL6_SENSITIVITY      0.9f
#define REGION_PREFRONTAL_TNF_SENSITIVITY      0.7f
#define REGION_PREFRONTAL_IFN_SENSITIVITY      0.6f

/* Motor cortex - moderate sensitivity */
#define REGION_MOTOR_IL1_SENSITIVITY           0.6f
#define REGION_MOTOR_IL6_SENSITIVITY           0.7f
#define REGION_MOTOR_TNF_SENSITIVITY           1.0f  /**< Moderate TNF sensitivity */
#define REGION_MOTOR_IFN_SENSITIVITY           0.5f

/* Sensory cortex - low baseline sensitivity */
#define REGION_SENSORY_IL1_SENSITIVITY         0.4f
#define REGION_SENSORY_IL6_SENSITIVITY         0.5f
#define REGION_SENSORY_TNF_SENSITIVITY         0.4f
#define REGION_SENSORY_IFN_SENSITIVITY         0.3f

/* Thalamus - relay disruption sensitivity */
#define REGION_THALAMUS_IL1_SENSITIVITY        0.9f
#define REGION_THALAMUS_IL6_SENSITIVITY        0.8f
#define REGION_THALAMUS_TNF_SENSITIVITY        0.8f
#define REGION_THALAMUS_IFN_SENSITIVITY        0.6f

/* Basal ganglia - movement disorder sensitivity */
#define REGION_BASAL_GANGLIA_IL1_SENSITIVITY   1.2f  /**< High IL-1β sensitivity */
#define REGION_BASAL_GANGLIA_IL6_SENSITIVITY   0.8f
#define REGION_BASAL_GANGLIA_TNF_SENSITIVITY   0.9f
#define REGION_BASAL_GANGLIA_IFN_SENSITIVITY   0.7f

/* Cerebellum - coordination sensitivity */
#define REGION_CEREBELLUM_IL1_SENSITIVITY      0.5f
#define REGION_CEREBELLUM_IL6_SENSITIVITY      0.6f
#define REGION_CEREBELLUM_TNF_SENSITIVITY      0.7f
#define REGION_CEREBELLUM_IFN_SENSITIVITY      0.4f

/* Inflammation propagation */
#define INFLAMMATION_PROPAGATION_RATE          0.3f   /**< 30% propagation per connection */
#define INFLAMMATION_PROPAGATION_THRESHOLD     0.5f   /**< Min inflammation to propagate */
#define INFLAMMATION_THALAMIC_AMPLIFICATION    1.5f   /**< Thalamus amplifies propagation */

/* Activity abnormality thresholds */
#define REGION_HYPERACTIVITY_THRESHOLD         3.0f   /**< 3x normal activity */
#define REGION_HYPOACTIVITY_THRESHOLD          0.2f   /**< 20% of normal activity */
#define REGION_DESYNC_THRESHOLD                0.3f   /**< 30% connectivity loss */
#define REGION_ABNORMALITY_PERSISTENCE         3      /**< 3 consecutive abnormal readings */

/* Layer-specific inflammation impact */
#define LAYER_4_INPUT_DISRUPTION_FACTOR        0.6f   /**< 40% input disruption at systemic */
#define LAYER_23_INHIBITION_FACTOR             0.7f   /**< 30% inhibition change */
#define LAYER_5_OUTPUT_REDUCTION_FACTOR        0.5f   /**< 50% output reduction */
#define LAYER_6_FEEDBACK_DISRUPTION_FACTOR     0.6f   /**< 40% feedback disruption */

/* Bio-async module ID */
#define BIO_MODULE_IMMUNE_BRAIN_REGIONS        0x0D20 /**< Bio-async module ID */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Regional inflammation severity levels
 */
typedef enum {
    REGION_INFLAMMATION_NONE = 0,     /**< No regional inflammation */
    REGION_INFLAMMATION_MILD,         /**< Mild: subtle processing changes */
    REGION_INFLAMMATION_MODERATE,     /**< Moderate: noticeable impairment */
    REGION_INFLAMMATION_SEVERE,       /**< Severe: significant dysfunction */
    REGION_INFLAMMATION_CRITICAL      /**< Critical: major functional loss */
} region_inflammation_level_t;

/**
 * @brief Regional abnormality types for immune triggering
 */
typedef enum {
    REGION_ABNORMALITY_NONE = 0,
    REGION_ABNORMALITY_HYPERACTIVE,   /**< Seizure-like hyperactivity */
    REGION_ABNORMALITY_HYPOACTIVE,    /**< Stroke-like hypoactivity */
    REGION_ABNORMALITY_DESYNC,        /**< Cross-region desynchronization */
    REGION_ABNORMALITY_LAYER_FAILURE, /**< Layer communication failure */
    REGION_ABNORMALITY_MIXED          /**< Multiple abnormality types */
} region_abnormality_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Region-specific cytokine sensitivity
 *
 * Different brain regions have different sensitivity to cytokines
 */
typedef struct {
    brain_region_type_t region_type;  /**< Which region */
    float il1_sensitivity;            /**< IL-1β sensitivity [0.0-2.0] */
    float il6_sensitivity;            /**< IL-6 sensitivity [0.0-2.0] */
    float tnf_sensitivity;            /**< TNF-α sensitivity [0.0-2.0] */
    float ifn_sensitivity;            /**< IFN-γ sensitivity [0.0-2.0] */
    float il10_responsiveness;        /**< IL-10 recovery responsiveness [0.0-1.0] */
} region_cytokine_sensitivity_t;

/**
 * @brief Per-region inflammation state
 */
typedef struct {
    uint32_t region_id;               /**< Region ID */
    brain_region_type_t region_type;  /**< Region type */
    region_inflammation_level_t level;/**< Current inflammation level */
    float intensity;                  /**< Normalized intensity [0-1] */
    float duration_sec;               /**< How long inflamed */

    /* Cytokine impact factors */
    float il1_impact;                 /**< Current IL-1β impact */
    float il6_impact;                 /**< Current IL-6 impact */
    float tnf_impact;                 /**< Current TNF-α impact */
    float ifn_impact;                 /**< Current IFN-γ impact */
    float composite_impact;           /**< Combined cytokine impact */

    /* Functional effects */
    float activity_modulation;        /**< Activity scaling factor [0-1] */
    float connectivity_modulation;    /**< Inter-region connectivity [0-1] */
    float layer_disruption[LAYER_COUNT]; /**< Per-layer disruption [0-1] */
} region_inflammation_state_t;

/**
 * @brief Regional abnormality detection state
 */
typedef struct {
    uint32_t region_id;               /**< Region being monitored */
    brain_region_type_t region_type;  /**< Type of region */

    /* Abnormality indicators */
    region_abnormality_type_t abnormality_type;
    float abnormality_score;          /**< Overall score [0-1] */
    uint32_t consecutive_abnormal;    /**< Consecutive abnormal readings */

    /* Activity metrics */
    float current_activity;           /**< Current activity level */
    float baseline_activity;          /**< Normal baseline activity */
    float activity_ratio;             /**< Current/baseline ratio */

    /* Connectivity metrics */
    float input_connectivity;         /**< Input region connectivity */
    float output_connectivity;        /**< Output region connectivity */
    float connectivity_baseline;      /**< Normal connectivity */

    /* Immune trigger state */
    bool immune_triggered;            /**< Has immune been triggered */
    uint32_t antigen_id;              /**< ID of presented antigen */
    uint32_t immune_severity;         /**< Severity for immune system [1-10] */
} region_abnormality_state_t;

/**
 * @brief Inter-region inflammation propagation state
 */
typedef struct {
    uint32_t source_region_id;        /**< Origin of inflammation */
    uint32_t target_region_id;        /**< Affected region */
    float propagation_strength;       /**< Propagation factor [0-1] */
    float propagation_delay_ms;       /**< Delay in propagation */
    bool is_thalamic_relay;           /**< Via thalamus (amplified) */
} inflammation_propagation_t;

/**
 * @brief Statistics for brain regions immune bridge
 */
typedef struct {
    uint64_t total_updates;           /**< Total update calls */
    uint32_t regions_monitored;       /**< Number of regions being monitored */
    uint32_t inflammations_applied;   /**< Inflammation effects applied */
    uint32_t propagations_triggered;  /**< Inter-region propagations */
    uint32_t abnormalities_detected;  /**< Abnormalities detected */
    uint32_t antigens_presented;      /**< Antigens presented to immune */
    uint32_t recoveries;              /**< IL-10 mediated recoveries */
    float avg_region_activity;        /**< Average regional activity */
    float max_inflammation_intensity; /**< Maximum inflammation seen */
} brain_regions_immune_stats_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_region_specific_sensitivity;
    bool enable_inflammation_propagation;
    bool enable_layer_effects;
    bool enable_abnormality_detection;
    bool enable_il10_recovery;
    bool enable_bio_async;

    /* Sensitivity multipliers */
    float cytokine_sensitivity_multiplier;  /**< Global cytokine sensitivity [0.5-2.0] */
    float propagation_rate_multiplier;      /**< Propagation rate adjust [0.5-2.0] */
    float abnormality_sensitivity;          /**< Abnormality detection sensitivity [0.5-2.0] */

    /* Thresholds */
    float hyperactivity_threshold;    /**< Threshold for hyperactivity [2.0-5.0] */
    float hypoactivity_threshold;     /**< Threshold for hypoactivity [0.1-0.4] */
    uint32_t persistence_threshold;   /**< Consecutive abnormal readings [2-5] */

    /* Capacity limits */
    uint32_t max_regions;             /**< Maximum regions to monitor */
    uint32_t max_propagations;        /**< Maximum concurrent propagations */
} brain_regions_immune_config_t;

/**
 * @brief Complete brain regions-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_module_t* brain_module;
    brain_immune_system_t* immune_system;

    /* Region sensitivity profiles */
    region_cytokine_sensitivity_t* sensitivities;
    uint32_t sensitivity_count;

    /* Per-region inflammation states */
    region_inflammation_state_t* inflammation_states;
    uint32_t inflammation_state_count;
    uint32_t max_inflammation_states;

    /* Per-region abnormality tracking */
    region_abnormality_state_t* abnormality_states;
    uint32_t abnormality_state_count;
    uint32_t max_abnormality_states;

    /* Inflammation propagation tracking */
    inflammation_propagation_t* propagations;
    uint32_t propagation_count;
    uint32_t max_propagations;

    /* Configuration */
    brain_regions_immune_config_t config;

    /* Statistics */
    brain_regions_immune_stats_t stats;

    } brain_regions_immune_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int brain_regions_immune_default_config(brain_regions_immune_config_t* config);

/**
 * @brief Create brain regions-immune bridge
 *
 * WHAT: Initialize bidirectional brain regions-immune integration
 * WHY:  Enable realistic region-specific immune effects
 * HOW:  Allocate structure, link subsystems, initialize sensitivity profiles
 *
 * @param config Configuration (NULL for defaults)
 * @param brain_module Brain module system
 * @param immune_system Brain immune system
 * @return New bridge or NULL on failure
 */
brain_regions_immune_bridge_t* brain_regions_immune_bridge_create(
    const brain_regions_immune_config_t* config,
    brain_module_t* brain_module,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy brain regions-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void brain_regions_immune_bridge_destroy(brain_regions_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with bio-async messaging system
 * WHY:  Enable inter-module communication
 * HOW:  Register module with router, store context
 *
 * @param bridge Brain regions immune bridge
 * @return 0 on success, -1 on error
 */
int brain_regions_immune_connect_bio_async(brain_regions_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async messaging system
 * WHY:  Clean shutdown
 * HOW:  Unregister module from router
 *
 * @param bridge Brain regions immune bridge
 * @return 0 on success, -1 on error
 */
int brain_regions_immune_disconnect_bio_async(brain_regions_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Brain regions immune bridge
 * @return true if connected to bio-async
 */
bool brain_regions_immune_is_bio_async_connected(const brain_regions_immune_bridge_t* bridge);

/* ============================================================================
 * Region Sensitivity API
 * ============================================================================ */

/**
 * @brief Set custom sensitivity profile for region type
 *
 * WHAT: Override default sensitivity for a region type
 * WHY:  Allow tuning for specific models/experiments
 * HOW:  Store custom sensitivity in bridge, apply to matching regions
 *
 * @param bridge Brain regions immune bridge
 * @param sensitivity Custom sensitivity profile
 * @return 0 on success, -1 on error
 */
int brain_regions_immune_set_sensitivity(
    brain_regions_immune_bridge_t* bridge,
    const region_cytokine_sensitivity_t* sensitivity
);

/**
 * @brief Get sensitivity profile for region type
 *
 * WHAT: Query current sensitivity for a region type
 * WHY:  Introspection and debugging
 * HOW:  Look up sensitivity in bridge, return copy
 *
 * @param bridge Brain regions immune bridge
 * @param region_type Type of region
 * @param sensitivity Output sensitivity profile
 * @return 0 on success, -1 if not found
 */
int brain_regions_immune_get_sensitivity(
    const brain_regions_immune_bridge_t* bridge,
    brain_region_type_t region_type,
    region_cytokine_sensitivity_t* sensitivity
);

/* ============================================================================
 * Immune → Brain Regions API
 * ============================================================================ */

/**
 * @brief Apply immune effects to all regions
 *
 * WHAT: Modulate all brain regions based on immune state
 * WHY:  Cytokines affect different regions differently
 * HOW:  Query cytokine levels, apply region-specific sensitivity
 *
 * @param bridge Brain regions immune bridge
 * @return 0 on success
 */
int brain_regions_immune_apply_effects(brain_regions_immune_bridge_t* bridge);

/**
 * @brief Apply immune effects to specific region
 *
 * WHAT: Modulate single region based on immune state
 * WHY:  Targeted effect application
 * HOW:  Query cytokines, apply sensitivity to single region
 *
 * @param bridge Brain regions immune bridge
 * @param region_id ID of region to modulate
 * @return 0 on success, -1 if region not found
 */
int brain_regions_immune_apply_to_region(
    brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
);

/**
 * @brief Apply layer-specific inflammation effects
 *
 * WHAT: Apply inflammation to cortical layers within region
 * WHY:  Different layers have different immune sensitivity
 * HOW:  Modulate layer gains based on inflammation level
 *
 * @param bridge Brain regions immune bridge
 * @param region_id ID of region
 * @return 0 on success
 */
int brain_regions_immune_apply_layer_effects(
    brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
);

/**
 * @brief Propagate inflammation between regions
 *
 * WHAT: Spread inflammation from source to connected regions
 * WHY:  Inflammation can cascade through neural networks
 * HOW:  Follow region connections, apply propagation factor
 *
 * @param bridge Brain regions immune bridge
 * @return Number of propagations triggered
 */
int brain_regions_immune_propagate_inflammation(brain_regions_immune_bridge_t* bridge);

/**
 * @brief Apply IL-10 recovery to region
 *
 * WHAT: Gradually restore region function with IL-10
 * WHY:  IL-10 (anti-inflammatory) promotes recovery
 * HOW:  Interpolate toward baseline function
 *
 * @param bridge Brain regions immune bridge
 * @param region_id ID of region
 * @param il10_concentration IL-10 level [0-1]
 * @return 0 on success
 */
int brain_regions_immune_restore_region(
    brain_regions_immune_bridge_t* bridge,
    uint32_t region_id,
    float il10_concentration
);

/* ============================================================================
 * Brain Regions → Immune API
 * ============================================================================ */

/**
 * @brief Detect abnormalities in all regions
 *
 * WHAT: Check all regions for pathological activity patterns
 * WHY:  Abnormal activity may indicate neural pathology
 * HOW:  Compare current activity to baselines
 *
 * @param bridge Brain regions immune bridge
 * @return Number of abnormalities detected
 */
int brain_regions_immune_detect_abnormalities(brain_regions_immune_bridge_t* bridge);

/**
 * @brief Detect abnormality in specific region
 *
 * WHAT: Check single region for pathological activity
 * WHY:  Targeted monitoring
 * HOW:  Compare activity/connectivity to baseline
 *
 * @param bridge Brain regions immune bridge
 * @param region_id ID of region to check
 * @return Abnormality type detected (NONE if normal)
 */
region_abnormality_type_t brain_regions_immune_detect_region_abnormality(
    brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
);

/**
 * @brief Trigger immune response from region abnormality
 *
 * WHAT: Present abnormal region activity as antigen
 * WHY:  Persistent abnormality may indicate pathology
 * HOW:  Create epitope from region signature, present to immune
 *
 * @param bridge Brain regions immune bridge
 * @param region_id ID of abnormal region
 * @return 0 on success, -1 on error
 */
int brain_regions_immune_trigger_response(
    brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
);

/**
 * @brief Compute abnormality severity for region
 *
 * WHAT: Calculate severity score for immune system
 * WHY:  Different abnormalities have different severity
 * HOW:  Weighted score based on abnormality type and magnitude
 *
 * @param bridge Brain regions immune bridge
 * @param region_id ID of region
 * @return Severity score [1-10]
 */
uint32_t brain_regions_immune_compute_severity(
    const brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update brain regions-immune bridge (both directions)
 *
 * WHAT: Process all region-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply immune effects, detect abnormalities, trigger responses
 *
 * @param bridge Brain regions immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int brain_regions_immune_bridge_update(
    brain_regions_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get inflammation state for region
 *
 * @param bridge Brain regions immune bridge
 * @param region_id ID of region
 * @param state Output state structure
 * @return 0 on success, -1 if not found
 */
int brain_regions_immune_get_inflammation_state(
    const brain_regions_immune_bridge_t* bridge,
    uint32_t region_id,
    region_inflammation_state_t* state
);

/**
 * @brief Get abnormality state for region
 *
 * @param bridge Brain regions immune bridge
 * @param region_id ID of region
 * @param state Output state structure
 * @return 0 on success, -1 if not found
 */
int brain_regions_immune_get_abnormality_state(
    const brain_regions_immune_bridge_t* bridge,
    uint32_t region_id,
    region_abnormality_state_t* state
);

/**
 * @brief Get overall activity modulation for region
 *
 * @param bridge Brain regions immune bridge
 * @param region_id ID of region
 * @return Activity modulation factor [0-1], 1.0 if not found
 */
float brain_regions_immune_get_activity_modulation(
    const brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
);

/**
 * @brief Get connectivity modulation for region
 *
 * @param bridge Brain regions immune bridge
 * @param region_id ID of region
 * @return Connectivity modulation factor [0-1], 1.0 if not found
 */
float brain_regions_immune_get_connectivity_modulation(
    const brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
);

/**
 * @brief Check if region is under immune modulation
 *
 * @param bridge Brain regions immune bridge
 * @param region_id ID of region
 * @return true if region is being modulated
 */
bool brain_regions_immune_is_region_modulated(
    const brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Brain regions immune bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int brain_regions_immune_get_stats(
    const brain_regions_immune_bridge_t* bridge,
    brain_regions_immune_stats_t* stats
);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert region inflammation level to string
 *
 * @param level Inflammation level
 * @return Human-readable string
 */
const char* region_inflammation_level_to_string(region_inflammation_level_t level);

/**
 * @brief Convert region abnormality type to string
 *
 * @param type Abnormality type
 * @return Human-readable string
 */
const char* region_abnormality_type_to_string(region_abnormality_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_REGIONS_IMMUNE_BRIDGE_H */
