/**
 * @file nimcp_cortical_immune.h
 * @brief Cortical Immune Integration - Microglial Surveillance of Cortical Columns
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Integration layer between brain immune system and cortical columns,
 *       modeling microglial surveillance, inflammation-induced dysfunction,
 *       and immune response to abnormal columnar activity.
 * WHY:  Microglia monitor cortical health; neuroinflammation disrupts columnar
 *       processing and layer communication; abnormal patterns trigger immune response.
 * HOW:  Bidirectional coupling:
 *       - Immune → Cortex: Inflammation modulates layer gains, disrupts connectivity
 *       - Cortex → Immune: Abnormal activity patterns presented as antigens
 *
 * BIOLOGICAL BASIS:
 * ┌────────────────────────────────────────────────────────────────────┐
 * │                    CORTICAL IMMUNE INTERACTION                      │
 * ├────────────────────────────────────────────────────────────────────┤
 * │                                                                     │
 * │  MICROGLIA (Brain Immune Cells):                                   │
 * │  • Resting: Monitor columnar activity, process debris              │
 * │  • Activated: Release cytokines, alter synaptic transmission       │
 * │  • Reactive: Phagocytose damaged synapses, remodel circuits        │
 * │                                                                     │
 * │  INFLAMMATION EFFECTS ON CORTEX:                                   │
 * │  • Layer IV input disruption (cytokines reduce thalamic relay)     │
 * │  • Layer II/III lateral inhibition imbalance (E/I ratio shifts)    │
 * │  • Layer V output impairment (reduced bursting, motor deficits)    │
 * │  • Columnar competition disruption (winner-take-all breaks down)   │
 * │  • Feature selectivity reduction (tuning curves broaden)           │
 * │                                                                     │
 * │  CORTICAL TRIGGERS FOR IMMUNE RESPONSE:                            │
 * │  • Hyperexcitability (seizure-like activity)                       │
 * │  • Hypoactivity (stroke-like silence)                              │
 * │  • Abnormal synchronization (pathological oscillations)            │
 * │  • Feature map degradation (loss of orientation selectivity)       │
 * │  • Layer communication failure (feedforward/feedback breakdown)    │
 * │                                                                     │
 * └────────────────────────────────────────────────────────────────────┘
 *
 * INTEGRATION ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │               CORTICAL IMMUNE SURVEILLANCE                       │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                  │
 * │  Minicolumn Activity → Microglial Monitoring → Immune System    │
 * │         ↓                       ↓                      ↓         │
 * │   [Abnormal?] ────────→ [Antigen Presentation]                  │
 * │                                                                  │
 * │  Immune Response → Cytokine Release → Columnar Modulation       │
 * │         ↓                   ↓                    ↓               │
 * │  [Inflammation] ──→ [Layer Dysfunction] ──→ [Performance Loss]  │
 * │                                                                  │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * MATHEMATICAL MODELS:
 * - Inflammation impact: gain' = gain × (1 - inflammation_factor)
 * - Cytokine diffusion: C(x,t+dt) = C(x,t) + D·∇²C·dt
 * - Abnormality score: A = w₁·|act - μ|/σ + w₂·sync + w₃·silence
 * - Microglial activation: M(t+1) = M(t) + α·(A - θ)·[A > θ]
 *
 * REFERENCES:
 * - Kettenmann et al. (2011) "Physiology of microglia"
 * - Prinz et al. (2019) "Microglia in the CNS"
 * - Salter & Stevens (2017) "Microglia emerge as central players"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_IMMUNE_H
#define NIMCP_CORTICAL_IMMUNE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Cortical column modules */
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "core/cortical_columns/nimcp_cortical_layers.h"
#include "core/cortical_columns/nimcp_columnar_connectivity.h"
#include "core/cortical_columns/nimcp_feature_hypercolumns.h"
#include "core/cortical_columns/nimcp_orientation_columns.h"
#include "core/cortical_columns/nimcp_topographic_maps.h"

/* Brain immune system */
#include "cognitive/immune/nimcp_brain_immune.h"

/* Utilities */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CORTICAL_IMMUNE_MAX_MICROGLIAL_SITES    256   /**< Max surveillance sites */
#define CORTICAL_IMMUNE_MAX_COLUMNS             1024  /**< Max monitored columns */
#define CORTICAL_IMMUNE_MAX_LAYERS              6     /**< Max cortical layers */
#define CORTICAL_IMMUNE_SURVEILLANCE_RADIUS     5.0f  /**< Microglial radius (mm) */
#define CORTICAL_IMMUNE_ABNORMALITY_THRESHOLD   0.5f  /**< Trigger threshold */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Microglial activation states
 *
 * BIOLOGICAL BASIS: Microglia transition from resting surveillance to
 * reactive states during inflammation.
 */
typedef enum {
    MICROGLIA_RESTING = 0,      /**< Surveillance mode, ramified morphology */
    MICROGLIA_ALERT,            /**< Detected abnormality, increased motility */
    MICROGLIA_ACTIVATED,        /**< Pro-inflammatory, amoeboid morphology */
    MICROGLIA_REACTIVE,         /**< Phagocytic, synaptic pruning active */
    MICROGLIA_RESOLVING         /**< Anti-inflammatory, returning to rest */
} microglial_state_t;

/**
 * @brief Types of cortical abnormalities detected
 */
typedef enum {
    ABNORMALITY_HYPEREXCITABILITY = 0,  /**< Excessive firing */
    ABNORMALITY_HYPOACTIVITY,           /**< Insufficient activity */
    ABNORMALITY_SYNCHRONIZATION,        /**< Pathological sync */
    ABNORMALITY_LAYER_DYSFUNCTION,      /**< Inter-layer failure */
    ABNORMALITY_FEATURE_LOSS,           /**< Selectivity degradation */
    ABNORMALITY_COMPETITION_FAILURE,    /**< WTA breakdown */
    ABNORMALITY_COUNT
} cortical_abnormality_type_t;

/**
 * @brief Inflammation effects on cortical processing
 */
typedef enum {
    INFLAMMATION_EFFECT_GAIN_REDUCTION = 0,  /**< Reduce layer gain */
    INFLAMMATION_EFFECT_INHIBITION_LOSS,     /**< Reduce lateral inhibition */
    INFLAMMATION_EFFECT_CONNECTIVITY_LOSS,   /**< Reduce connection weights */
    INFLAMMATION_EFFECT_SELECTIVITY_LOSS,    /**< Broaden tuning curves */
    INFLAMMATION_EFFECT_NOISE_INCREASE,      /**< Add synaptic noise */
    INFLAMMATION_EFFECT_COUNT
} inflammation_effect_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Microglial surveillance site
 *
 * WHAT: Models a microglial cell monitoring cortical columns.
 * WHY:  Microglia patrol cortical territory, detecting abnormalities.
 * HOW:  Tracks activation state, cytokine levels, monitored columns.
 */
typedef struct {
    uint32_t site_id;                       /**< Unique site ID */
    microglial_state_t state;               /**< Current activation state */

    /* Spatial location */
    float cortical_x;                       /**< X position (mm) */
    float cortical_y;                       /**< Y position (mm) */
    float surveillance_radius;              /**< Surveillance radius (mm) */

    /* Activation */
    float activation_level;                 /**< Activation (0-1) */
    float cytokine_concentration;           /**< Local cytokine level (0-1) */
    uint64_t last_activation_time;          /**< Last activation timestamp */

    /* Monitored columns */
    uint32_t* monitored_column_ids;         /**< Column IDs in radius */
    uint32_t num_monitored_columns;         /**< Count of monitored columns */

    /* Abnormality detection */
    float abnormality_score;                /**< Current abnormality (0-1) */
    cortical_abnormality_type_t detected_type; /**< Type of abnormality */
    uint32_t detection_count;               /**< Times triggered */
} microglial_site_t;

/**
 * @brief Cortical column immune status
 *
 * WHAT: Tracks immune-related state for a cortical column.
 * WHY:  Inflammation affects individual columns differently.
 * HOW:  Stores inflammation level, immune modulation effects.
 */
typedef struct {
    uint32_t column_id;                     /**< Column ID */

    /* Inflammation status */
    float inflammation_level;               /**< Local inflammation (0-1) */
    brain_inflammation_level_t inflammation_severity; /**< Categorized severity */

    /* Performance impact */
    float gain_modulation;                  /**< Gain factor (0-1) */
    float inhibition_modulation;            /**< Inhibition factor (0-1) */
    float connectivity_modulation;          /**< Connection factor (0-1) */
    float selectivity_modulation;           /**< Tuning sharpness (0-1) */

    /* Activity monitoring */
    float baseline_activation;              /**< Healthy baseline */
    float current_activation;               /**< Current activity */
    float activation_variance;              /**< Activity variance */

    /* Immune events */
    uint32_t linked_microglial_site;        /**< Nearest microglial site */
    uint32_t immune_activations;            /**< Times immune activated */
    uint64_t last_immune_event;             /**< Last immune event time */
} cortical_column_immune_t;

/**
 * @brief Layer-specific immune effects
 *
 * WHAT: Models inflammation impact on specific cortical layers.
 * WHY:  Different layers have different vulnerabilities.
 * HOW:  Tracks per-layer cytokine levels and dysfunction.
 */
typedef struct {
    cc_cortical_layer_t layer;              /**< Layer identifier */

    /* Cytokine levels */
    float il1_concentration;                /**< IL-1 (pro-inflammatory) */
    float il6_concentration;                /**< IL-6 (acute phase) */
    float tnf_concentration;                /**< TNF-α (severe) */
    float il10_concentration;               /**< IL-10 (anti-inflammatory) */

    /* Functional impact */
    float feedforward_gain;                 /**< FF pathway gain (0-1) */
    float feedback_gain;                    /**< FB pathway gain (0-1) */
    float lateral_gain;                     /**< Lateral gain (0-1) */
    float excitability;                     /**< Layer excitability (0-1) */

    /* Health metrics */
    float mean_activation;                  /**< Average activity */
    float activation_stability;             /**< Temporal stability */
    bool is_dysfunctional;                  /**< Dysfunction flag */
} layer_immune_state_t;

/**
 * @brief Cortical immune system configuration
 */
typedef struct {
    /* Monitoring parameters */
    uint32_t max_microglial_sites;          /**< Max surveillance sites */
    float microglial_density;               /**< Sites per mm² */
    float surveillance_radius;              /**< Monitoring radius (mm) */
    uint64_t surveillance_interval_ms;      /**< Check interval */

    /* Abnormality detection */
    float hyperexcitability_threshold;      /**< Detect high activity */
    float hypoactivity_threshold;           /**< Detect low activity */
    float synchronization_threshold;        /**< Detect sync */
    float feature_loss_threshold;           /**< Detect selectivity loss */

    /* Inflammation effects */
    float inflammation_gain_impact;         /**< Gain reduction factor */
    float cytokine_diffusion_rate;          /**< Diffusion coefficient */
    float inflammation_decay_rate;          /**< Resolution rate */

    /* Immune integration */
    bool enable_immune_integration;         /**< Connect to brain immune */
    bool enable_antigen_presentation;       /**< Present abnormalities */
    bool enable_cytokine_modulation;        /**< Apply cytokine effects */
    bool enable_recovery_tracking;          /**< Track resolution */
} cortical_immune_config_t;

/**
 * @brief Cortical immune statistics
 */
typedef struct {
    /* Microglial activity */
    uint32_t total_microglial_sites;
    uint32_t resting_microglia;
    uint32_t activated_microglia;
    uint32_t reactive_microglia;

    /* Abnormality detection */
    uint64_t total_abnormalities_detected;
    uint64_t hyperexcitability_events;
    uint64_t hypoactivity_events;
    uint64_t synchronization_events;
    uint64_t layer_dysfunction_events;

    /* Inflammation */
    float mean_inflammation_level;
    float max_inflammation_level;
    uint32_t inflamed_columns;

    /* Performance impact */
    float mean_gain_modulation;
    float mean_selectivity_loss;
    uint32_t dysfunctional_layers;

    /* Immune integration */
    uint64_t antigens_presented;
    uint64_t immune_responses_triggered;
    uint64_t resolutions_completed;
} cortical_immune_stats_t;

/* ============================================================================
 * Forward Declaration
 * ============================================================================ */

typedef struct cortical_immune_system cortical_immune_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration.
 * WHY:  Easy initialization with biologically realistic defaults.
 * HOW:  Return struct with balanced parameters.
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int cortical_immune_default_config(cortical_immune_config_t* config);

/**
 * @brief Create cortical immune system
 *
 * WHAT: Initialize cortical immune surveillance layer.
 * WHY:  Set up microglial monitoring and immune integration.
 * HOW:  Allocate microglial sites, initialize monitoring.
 *
 * @param config Configuration (NULL for defaults)
 * @return New cortical immune system or NULL on failure
 */
cortical_immune_system_t* cortical_immune_create(
    const cortical_immune_config_t* config
);

/**
 * @brief Destroy cortical immune system
 *
 * WHAT: Clean up cortical immune resources.
 * WHY:  Proper resource deallocation.
 * HOW:  Free sites, unregister from brain immune.
 *
 * @param system System to destroy
 */
void cortical_immune_destroy(cortical_immune_system_t* system);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Link cortical immune to brain immune system.
 * WHY:  Enable antigen presentation and cytokine responses.
 * HOW:  Register callbacks, store reference.
 *
 * @param system Cortical immune system
 * @param brain_immune Brain immune system
 * @return 0 on success
 */
int cortical_immune_connect_brain_immune(
    cortical_immune_system_t* system,
    brain_immune_system_t* brain_immune
);

/**
 * @brief Register minicolumn for monitoring
 *
 * WHAT: Add minicolumn to immune surveillance.
 * WHY:  Track columnar activity for abnormalities.
 * HOW:  Assign to nearest microglial site, track state.
 *
 * @param system Cortical immune system
 * @param column Minicolumn to monitor
 * @param column_id Column identifier
 * @return 0 on success
 */
int cortical_immune_register_minicolumn(
    cortical_immune_system_t* system,
    minicolumn_t* column,
    uint32_t column_id
);

/**
 * @brief Register hypercolumn for monitoring
 *
 * WHAT: Add hypercolumn to immune surveillance.
 * WHY:  Monitor hypercolumn-level abnormalities.
 * HOW:  Register all constituent minicolumns.
 *
 * @param system Cortical immune system
 * @param hcol Hypercolumn to monitor
 * @param hcol_id Hypercolumn identifier
 * @return 0 on success
 */
int cortical_immune_register_hypercolumn(
    cortical_immune_system_t* system,
    hypercolumn_t* hcol,
    uint32_t hcol_id
);

/**
 * @brief Register laminar structure for monitoring
 *
 * WHAT: Add laminar structure to layer-specific monitoring.
 * WHY:  Detect layer dysfunction and inter-layer failures.
 * HOW:  Monitor feedforward/feedback pathways.
 *
 * @param system Cortical immune system
 * @param layers Laminar structure to monitor
 * @param region_id Region identifier
 * @return 0 on success
 */
int cortical_immune_register_laminar_structure(
    cortical_immune_system_t* system,
    laminar_structure_t* layers,
    uint32_t region_id
);

/**
 * @brief Register orientation hypercolumn
 *
 * WHAT: Monitor orientation selectivity for degradation.
 * WHY:  Feature selectivity loss indicates cortical dysfunction.
 * HOW:  Track tuning curve width, OSI, circular variance.
 *
 * @param system Cortical immune system
 * @param orient_hcol Orientation hypercolumn
 * @param hcol_id Hypercolumn ID
 * @return 0 on success
 */
int cortical_immune_register_orientation_hypercolumn(
    cortical_immune_system_t* system,
    orientation_hypercolumn_t* orient_hcol,
    uint32_t hcol_id
);

/* ============================================================================
 * Microglial Surveillance API
 * ============================================================================ */

/**
 * @brief Create microglial surveillance site
 *
 * WHAT: Establish microglial monitoring at cortical location.
 * WHY:  Model spatial distribution of microglial cells.
 * HOW:  Create site, assign columnar surveillance radius.
 *
 * @param system Cortical immune system
 * @param cortical_x X position (mm)
 * @param cortical_y Y position (mm)
 * @param radius Surveillance radius (mm)
 * @param site_id Output: assigned site ID
 * @return 0 on success
 */
int cortical_immune_create_microglial_site(
    cortical_immune_system_t* system,
    float cortical_x,
    float cortical_y,
    float radius,
    uint32_t* site_id
);

/**
 * @brief Update microglial surveillance
 *
 * WHAT: Check monitored columns for abnormalities.
 * WHY:  Detect cortical dysfunction requiring immune response.
 * HOW:  Compute abnormality scores, activate microglia if needed.
 *
 * @param system Cortical immune system
 * @param delta_ms Time since last update
 * @return Number of new abnormalities detected
 */
uint32_t cortical_immune_update_surveillance(
    cortical_immune_system_t* system,
    uint64_t delta_ms
);

/**
 * @brief Activate microglial site
 *
 * WHAT: Transition microglia from resting to activated state.
 * WHY:  Respond to detected abnormality.
 * HOW:  Change state, begin cytokine release.
 *
 * @param system Cortical immune system
 * @param site_id Microglial site to activate
 * @param abnormality_type Type of detected abnormality
 * @return 0 on success
 */
int cortical_immune_activate_microglia(
    cortical_immune_system_t* system,
    uint32_t site_id,
    cortical_abnormality_type_t abnormality_type
);

/* ============================================================================
 * Abnormality Detection API
 * ============================================================================ */

/**
 * @brief Detect hyperexcitability
 *
 * WHAT: Check if column activity exceeds normal range.
 * WHY:  Hyperexcitability can indicate seizure-like activity.
 * HOW:  Compare activation to baseline + threshold.
 *
 * @param system Cortical immune system
 * @param column_id Column to check
 * @param activation Current activation level
 * @return Abnormality score (0-1), 0 if normal
 */
float cortical_immune_detect_hyperexcitability(
    cortical_immune_system_t* system,
    uint32_t column_id,
    float activation
);

/**
 * @brief Detect hypoactivity
 *
 * WHAT: Check if column activity below normal range.
 * WHY:  Hypoactivity can indicate stroke-like dysfunction.
 * HOW:  Compare activation to baseline - threshold.
 *
 * @param system Cortical immune system
 * @param column_id Column to check
 * @param activation Current activation level
 * @return Abnormality score (0-1), 0 if normal
 */
float cortical_immune_detect_hypoactivity(
    cortical_immune_system_t* system,
    uint32_t column_id,
    float activation
);

/**
 * @brief Detect abnormal synchronization
 *
 * WHAT: Check for pathological coherence across columns.
 * WHY:  Excessive synchrony indicates network dysfunction.
 * HOW:  Compute cross-correlation, detect high coherence.
 *
 * @param system Cortical immune system
 * @param column_ids Array of column IDs to check
 * @param activations Array of current activations
 * @param num_columns Number of columns
 * @return Synchronization score (0-1), 0 if normal
 */
float cortical_immune_detect_synchronization(
    cortical_immune_system_t* system,
    const uint32_t* column_ids,
    const float* activations,
    uint32_t num_columns
);

/**
 * @brief Detect layer dysfunction
 *
 * WHAT: Check for feedforward/feedback pathway failure.
 * WHY:  Layer communication failure indicates cortical damage.
 * HOW:  Monitor inter-layer signal flow, detect breakdowns.
 *
 * @param system Cortical immune system
 * @param region_id Laminar structure region ID
 * @param layer Layer to check
 * @return Dysfunction score (0-1), 0 if normal
 */
float cortical_immune_detect_layer_dysfunction(
    cortical_immune_system_t* system,
    uint32_t region_id,
    cc_cortical_layer_t layer
);

/**
 * @brief Detect feature selectivity loss
 *
 * WHAT: Check if orientation tuning has degraded.
 * WHY:  Loss of selectivity indicates functional impairment.
 * HOW:  Track OSI, tuning width changes over time.
 *
 * @param system Cortical immune system
 * @param hcol_id Orientation hypercolumn ID
 * @param current_osi Current orientation selectivity index
 * @return Selectivity loss score (0-1), 0 if normal
 */
float cortical_immune_detect_feature_loss(
    cortical_immune_system_t* system,
    uint32_t hcol_id,
    float current_osi
);

/* ============================================================================
 * Inflammation API (Immune → Cortex)
 * ============================================================================ */

/**
 * @brief Apply inflammation to cortical column
 *
 * WHAT: Modulate column processing based on inflammation.
 * WHY:  Model cytokine effects on neural activity.
 * HOW:  Reduce gains, increase noise, broaden tuning.
 *
 * @param system Cortical immune system
 * @param column_id Column to affect
 * @param inflammation_level Inflammation (0-1)
 * @return 0 on success
 */
int cortical_immune_apply_inflammation(
    cortical_immune_system_t* system,
    uint32_t column_id,
    float inflammation_level
);

/**
 * @brief Apply cytokines to cortical layer
 *
 * WHAT: Modulate layer processing via cytokine effects.
 * WHY:  Different cytokines have different layer effects.
 * HOW:  Adjust feedforward/feedback/lateral gains.
 *
 * @param system Cortical immune system
 * @param region_id Laminar structure region
 * @param layer Layer to modulate
 * @param cytokine_type Cytokine type
 * @param concentration Cytokine concentration (0-1)
 * @return 0 on success
 */
int cortical_immune_apply_cytokine(
    cortical_immune_system_t* system,
    uint32_t region_id,
    cc_cortical_layer_t layer,
    brain_cytokine_type_t cytokine_type,
    float concentration
);

/**
 * @brief Update cytokine diffusion
 *
 * WHAT: Simulate spatial spread of cytokines.
 * WHY:  Cytokines diffuse across cortical surface.
 * HOW:  Discretized diffusion equation.
 *
 * @param system Cortical immune system
 * @param delta_ms Time step
 * @return 0 on success
 */
int cortical_immune_update_cytokine_diffusion(
    cortical_immune_system_t* system,
    uint64_t delta_ms
);

/**
 * @brief Resolve inflammation
 *
 * WHAT: Begin inflammation resolution process.
 * WHY:  Restore normal cortical function after threat cleared.
 * HOW:  Gradually restore gains, reduce cytokines.
 *
 * @param system Cortical immune system
 * @param column_id Column to resolve
 * @return 0 on success
 */
int cortical_immune_resolve_inflammation(
    cortical_immune_system_t* system,
    uint32_t column_id
);

/* ============================================================================
 * Antigen Presentation API (Cortex → Immune)
 * ============================================================================ */

/**
 * @brief Present cortical abnormality as antigen
 *
 * WHAT: Convert detected abnormality to immune antigen.
 * WHY:  Trigger adaptive immune response to dysfunction.
 * HOW:  Create epitope from activity pattern, present to brain immune.
 *
 * @param system Cortical immune system
 * @param column_id Affected column
 * @param abnormality_type Type of abnormality
 * @param abnormality_score Severity (0-1)
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int cortical_immune_present_abnormality(
    cortical_immune_system_t* system,
    uint32_t column_id,
    cortical_abnormality_type_t abnormality_type,
    float abnormality_score,
    uint32_t* antigen_id
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get column immune status
 *
 * @param system Cortical immune system
 * @param column_id Column ID
 * @param status Output: immune status
 * @return 0 on success
 */
int cortical_immune_get_column_status(
    cortical_immune_system_t* system,
    uint32_t column_id,
    cortical_column_immune_t* status
);

/**
 * @brief Get layer immune state
 *
 * @param system Cortical immune system
 * @param region_id Laminar structure region
 * @param layer Layer to query
 * @param state Output: layer immune state
 * @return 0 on success
 */
int cortical_immune_get_layer_state(
    cortical_immune_system_t* system,
    uint32_t region_id,
    cc_cortical_layer_t layer,
    layer_immune_state_t* state
);

/**
 * @brief Get microglial site info
 *
 * @param system Cortical immune system
 * @param site_id Site ID
 * @param site Output: microglial site info
 * @return 0 on success
 */
int cortical_immune_get_microglial_site(
    cortical_immune_system_t* system,
    uint32_t site_id,
    microglial_site_t* site
);

/**
 * @brief Get cortical immune statistics
 *
 * @param system Cortical immune system
 * @param stats Output: statistics
 * @return 0 on success
 */
int cortical_immune_get_stats(
    cortical_immune_system_t* system,
    cortical_immune_stats_t* stats
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update cortical immune system
 *
 * WHAT: Process surveillance, diffusion, resolution.
 * WHY:  Advance immune state machine.
 * HOW:  Update surveillance, diffuse cytokines, resolve inflammation.
 *
 * @param system Cortical immune system
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int cortical_immune_update(
    cortical_immune_system_t* system,
    uint64_t delta_ms
);

/* ============================================================================
 * String Utilities
 * ============================================================================ */

const char* cortical_immune_microglial_state_to_string(microglial_state_t state);
const char* cortical_immune_abnormality_to_string(cortical_abnormality_type_t type);
const char* cortical_immune_effect_to_string(inflammation_effect_t effect);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_IMMUNE_H */
