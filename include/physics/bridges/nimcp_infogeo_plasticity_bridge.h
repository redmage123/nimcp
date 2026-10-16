/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_infogeo_plasticity_bridge.h - Information Geometry to Plasticity Bridge
//=============================================================================
/**
 * @file nimcp_infogeo_plasticity_bridge.h
 * @brief Bridge connecting Information Geometry with Synaptic Plasticity
 *
 * WHAT: Provides bidirectional integration between Information Geometry module
 *       and synaptic plasticity mechanisms (STDP, eligibility traces).
 *
 * WHY:  Information geometry enhances plasticity in principled ways:
 *       - Natural gradient STDP converges faster with better generalization
 *       - Fisher information weights plasticity by parameter importance
 *       - Geodesic interpolation enables smooth weight transitions
 *       - KL divergence provides principled plasticity regularization
 *
 * HOW:  Two-way integration:
 *       1. InfoGeo -> Plasticity: Natural gradient modulates STDP magnitude
 *       2. Plasticity -> InfoGeo: Weight changes update Fisher estimates
 *       3. Eligibility traces weighted by Fisher information
 *       4. Manifold structure constrains plasticity directions
 *
 * BIOLOGICAL BASIS:
 * ```
 * INFORMATION GEOMETRY                    PLASTICITY APPLICATION
 * -----------------------------------------------------------------------
 * Fisher Information                  ->  Parameter importance for STDP
 * Natural Gradient                    ->  Optimal plasticity direction
 * Riemannian Metric                   ->  Curvature of learning landscape
 * KL Divergence                       ->  Regularization of weight changes
 * Geodesic Path                       ->  Smooth weight interpolation
 * Manifold Projection                 ->  Constrained plasticity
 * ```
 *
 * STDP MODULATION:
 * - High Fisher information -> larger STDP magnitude (important synapse)
 * - Low Fisher information -> smaller STDP magnitude (redundant synapse)
 * - Natural gradient direction -> optimal weight change direction
 * - Eligibility × Fisher -> importance-weighted traces
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_INFOGEO_PLASTICITY_BRIDGE_H
#define NIMCP_INFOGEO_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define INFOGEO_PLASTICITY_MODULE_NAME   "infogeo_plasticity_bridge"

/** Maximum synapses for Fisher-weighted STDP */
#define INFOGEO_PLASTICITY_MAX_SYNAPSES  8192

/** Maximum eligibility traces */
#define INFOGEO_PLASTICITY_MAX_TRACES    4096

/** Default STDP window (ms) */
#define INFOGEO_PLASTICITY_STDP_WINDOW   50.0f

/** Default Fisher importance threshold */
#define INFOGEO_PLASTICITY_FISHER_THRESH 0.01f

/** Default natural gradient strength */
#define INFOGEO_PLASTICITY_NATGRAD_STR   0.5f

/** Minimum Fisher value for modulation */
#define INFOGEO_PLASTICITY_FISHER_MIN    1e-6f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief STDP modulation method by Fisher information
 */
typedef enum {
    INFOGEO_STDP_MOD_LINEAR = 0,        /**< Linear scaling by Fisher */
    INFOGEO_STDP_MOD_SQRT,              /**< Square root scaling */
    INFOGEO_STDP_MOD_LOG,               /**< Logarithmic scaling */
    INFOGEO_STDP_MOD_THRESHOLD          /**< Threshold gating by Fisher */
} infogeo_stdp_modulation_t;

/**
 * @brief Eligibility trace weighting method
 */
typedef enum {
    INFOGEO_ELIG_WEIGHT_UNIFORM = 0,    /**< Uniform (no weighting) */
    INFOGEO_ELIG_WEIGHT_FISHER,         /**< Weight by Fisher diagonal */
    INFOGEO_ELIG_WEIGHT_CURVATURE,      /**< Weight by local curvature */
    INFOGEO_ELIG_WEIGHT_GRADIENT        /**< Weight by gradient magnitude */
} infogeo_elig_weighting_t;

/**
 * @brief Plasticity constraint method
 */
typedef enum {
    INFOGEO_PLAST_CONSTRAINT_NONE = 0,  /**< No constraints */
    INFOGEO_PLAST_CONSTRAINT_MANIFOLD,  /**< Project to manifold */
    INFOGEO_PLAST_CONSTRAINT_KL,        /**< KL divergence bound */
    INFOGEO_PLAST_CONSTRAINT_TRUST      /**< Trust region on geodesic */
} infogeo_plasticity_constraint_t;

/**
 * @brief Interpolation method for weight transitions
 */
typedef enum {
    INFOGEO_INTERP_LINEAR = 0,          /**< Linear interpolation */
    INFOGEO_INTERP_GEODESIC,            /**< Geodesic interpolation */
    INFOGEO_INTERP_NATURAL              /**< Natural gradient path */
} infogeo_interpolation_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for Information Geometry-Plasticity bridge
 */
typedef struct {
    /** STDP modulation settings */
    infogeo_stdp_modulation_t stdp_modulation;   /**< How Fisher modulates STDP */
    float stdp_fisher_scale;                      /**< Scale factor for Fisher mod */
    float stdp_window_ms;                         /**< STDP time window */
    float ltp_magnitude;                          /**< Base LTP magnitude */
    float ltd_magnitude;                          /**< Base LTD magnitude */
    bool enable_fisher_ltp;                       /**< Fisher modulates LTP */
    bool enable_fisher_ltd;                       /**< Fisher modulates LTD */

    /** Eligibility trace settings */
    infogeo_elig_weighting_t elig_weighting;     /**< Trace weighting method */
    float elig_decay_ms;                          /**< Trace decay time constant */
    float elig_fisher_threshold;                  /**< Min Fisher for trace */
    bool enable_importance_traces;                /**< Fisher-weighted traces */

    /** Plasticity constraints */
    infogeo_plasticity_constraint_t constraint;  /**< Constraint method */
    float kl_bound;                               /**< Max KL divergence per update */
    float trust_radius;                           /**< Trust region radius */
    bool project_to_manifold;                     /**< Project weights to manifold */

    /** Interpolation settings */
    infogeo_interpolation_t interpolation;       /**< Weight interpolation method */
    uint32_t interpolation_steps;                 /**< Steps for geodesic interp */

    /** Natural gradient integration */
    float natural_gradient_strength;             /**< Natural gradient mixing */
    bool use_natural_direction;                   /**< Use natural grad direction */

    /** General settings */
    float update_interval_ms;                    /**< Bridge update interval */
    bool enable_logging;                          /**< Enable logging */
} infogeo_plasticity_config_t;

/**
 * @brief Synapse data for Fisher-weighted STDP
 */
typedef struct {
    uint32_t synapse_id;                /**< Synapse identifier */
    uint32_t pre_neuron;                /**< Presynaptic neuron ID */
    uint32_t post_neuron;               /**< Postsynaptic neuron ID */
    float current_weight;               /**< Current synaptic weight */
    float fisher_importance;            /**< Fisher information value */
    float eligibility_trace;            /**< Current eligibility trace */
    float last_pre_spike_ms;            /**< Last presynaptic spike time */
    float last_post_spike_ms;           /**< Last postsynaptic spike time */
} infogeo_synapse_t;

/**
 * @brief Fisher-modulated STDP event
 */
typedef struct {
    uint32_t synapse_id;                /**< Synapse identifier */
    float dt_ms;                        /**< Spike time difference (post - pre) */
    float base_stdp_change;             /**< Base STDP weight change */
    float fisher_modulation;            /**< Fisher modulation factor */
    float final_weight_change;          /**< Final modulated change */
    bool is_ltp;                        /**< True if LTP, false if LTD */
    float new_weight;                   /**< Weight after update */
} infogeo_stdp_event_t;

/**
 * @brief Fisher-weighted eligibility trace
 */
typedef struct {
    uint32_t synapse_id;                /**< Synapse identifier */
    float trace_value;                  /**< Current trace value */
    float fisher_weight;                /**< Fisher weighting applied */
    float importance_score;             /**< Combined importance score */
    float decay_rate;                   /**< Trace decay rate */
    float last_update_ms;               /**< Last update time */
    bool converted;                     /**< Whether trace was converted */
} infogeo_weighted_trace_t;

/**
 * @brief Geodesic weight interpolation state
 */
typedef struct {
    float* start_weights;               /**< Starting weights */
    float* end_weights;                 /**< Target weights */
    float* current_weights;             /**< Current interpolated weights */
    uint32_t num_weights;               /**< Number of weights */
    float progress;                     /**< Interpolation progress [0,1] */
    float geodesic_length;              /**< Total geodesic length */
    uint32_t current_step;              /**< Current interpolation step */
    uint32_t total_steps;               /**< Total interpolation steps */
} infogeo_interpolation_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t fisher_stdp_events;        /**< Fisher-modulated STDP events */
    uint64_t eligibility_conversions;   /**< Traces converted to weights */
    uint64_t manifold_projections;      /**< Weights projected to manifold */
    uint64_t kl_constraint_triggers;    /**< KL constraint activations */
    uint64_t geodesic_interpolations;   /**< Geodesic interpolations */
    float avg_fisher_modulation;        /**< Average Fisher modulation */
    float avg_ltp_magnitude;            /**< Average LTP magnitude */
    float avg_ltd_magnitude;            /**< Average LTD magnitude */
    float total_weight_change;          /**< Total weight change magnitude */
    float last_update_ms;               /**< Last update timestamp */
} infogeo_plasticity_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct infogeo_plasticity_bridge_struct infogeo_plasticity_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_plasticity_default_config(
    infogeo_plasticity_config_t* config
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Information Geometry-Plasticity bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT infogeo_plasticity_bridge_t* infogeo_plasticity_bridge_create(
    const infogeo_plasticity_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void infogeo_plasticity_bridge_destroy(
    infogeo_plasticity_bridge_t* bridge
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_plasticity_reset(infogeo_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Registration API
//=============================================================================

/**
 * @brief Register synapse for Fisher-weighted plasticity
 *
 * WHAT: Adds synapse to Fisher-weighted STDP processing
 * WHY:  Enables importance-weighted plasticity
 * HOW:  Stores synapse data and initializes Fisher value
 *
 * @param bridge Bridge handle
 * @param synapse Synapse data
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_plasticity_register_synapse(
    infogeo_plasticity_bridge_t* bridge,
    const infogeo_synapse_t* synapse
);

/**
 * @brief Update Fisher importance for synapse
 *
 * WHAT: Updates Fisher information value for synapse
 * WHY:  Fisher values change as network learns
 * HOW:  Sets new Fisher importance, affects STDP modulation
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param fisher_importance New Fisher importance value
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_plasticity_update_fisher(
    infogeo_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float fisher_importance
);

/**
 * @brief Batch update Fisher values from diagonal
 *
 * WHAT: Updates Fisher values for multiple synapses
 * WHY:  Efficient batch update from Fisher diagonal
 * HOW:  Applies diagonal Fisher values to registered synapses
 *
 * @param bridge Bridge handle
 * @param synapse_ids Array of synapse IDs
 * @param fisher_values Array of Fisher values
 * @param num_synapses Number of synapses
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_plasticity_batch_update_fisher(
    infogeo_plasticity_bridge_t* bridge,
    const uint32_t* synapse_ids,
    const float* fisher_values,
    uint32_t num_synapses
);

//=============================================================================
// Fisher-Weighted STDP API
//=============================================================================

/**
 * @brief Compute Fisher-modulated STDP for spike pair
 *
 * WHAT: Computes STDP weight change modulated by Fisher information
 * WHY:  Important synapses (high Fisher) get larger updates
 * HOW:  Base STDP × Fisher modulation factor
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param pre_spike_ms Presynaptic spike time
 * @param post_spike_ms Postsynaptic spike time
 * @param event Output STDP event (optional)
 * @return Computed weight change
 */
NIMCP_EXPORT float infogeo_plasticity_compute_stdp(
    infogeo_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_spike_ms,
    float post_spike_ms,
    infogeo_stdp_event_t* event
);

/**
 * @brief Process batch of STDP events
 *
 * WHAT: Processes multiple STDP events with Fisher modulation
 * WHY:  Efficient batch processing of plasticity
 * HOW:  Applies Fisher-weighted STDP to all pairs
 *
 * @param bridge Bridge handle
 * @param synapse_ids Array of synapse IDs
 * @param pre_spike_times Array of presynaptic spike times
 * @param post_spike_times Array of postsynaptic spike times
 * @param num_events Number of STDP events
 * @param events Output STDP events (optional)
 * @return Number of events processed
 */
NIMCP_EXPORT int infogeo_plasticity_batch_stdp(
    infogeo_plasticity_bridge_t* bridge,
    const uint32_t* synapse_ids,
    const float* pre_spike_times,
    const float* post_spike_times,
    uint32_t num_events,
    infogeo_stdp_event_t* events
);

//=============================================================================
// Eligibility Trace API
//=============================================================================

/**
 * @brief Update Fisher-weighted eligibility trace
 *
 * WHAT: Updates eligibility trace with Fisher weighting
 * WHY:  Important synapses maintain stronger traces
 * HOW:  trace = trace * decay + increment * fisher_weight
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param increment Trace increment
 * @param trace Output updated trace (optional)
 * @return Current trace value
 */
NIMCP_EXPORT float infogeo_plasticity_update_trace(
    infogeo_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float increment,
    infogeo_weighted_trace_t* trace
);

/**
 * @brief Convert eligibility traces to weight changes
 *
 * WHAT: Converts Fisher-weighted traces to weight updates
 * WHY:  Three-factor rule with importance weighting
 * HOW:  weight_change = trace * modulator * fisher_weight
 *
 * @param bridge Bridge handle
 * @param modulator Neuromodulatory signal (dopamine, etc.)
 * @param weight_changes Output weight changes (optional)
 * @param max_changes Maximum changes to return
 * @return Number of traces converted
 */
NIMCP_EXPORT int infogeo_plasticity_convert_traces(
    infogeo_plasticity_bridge_t* bridge,
    float modulator,
    float* weight_changes,
    uint32_t max_changes
);

/**
 * @brief Decay all eligibility traces
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_plasticity_decay_traces(
    infogeo_plasticity_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Constrained Plasticity API
//=============================================================================

/**
 * @brief Apply KL-constrained weight update
 *
 * WHAT: Updates weights with KL divergence constraint
 * WHY:  Prevents catastrophic forgetting
 * HOW:  Limits update magnitude by KL bound
 *
 * @param bridge Bridge handle
 * @param weights Current weights (modified in place)
 * @param proposed_update Proposed weight changes
 * @param num_weights Number of weights
 * @return Actual KL divergence of update
 */
NIMCP_EXPORT float infogeo_plasticity_kl_constrained_update(
    infogeo_plasticity_bridge_t* bridge,
    float* weights,
    const float* proposed_update,
    uint32_t num_weights
);

/**
 * @brief Project weights onto neural manifold
 *
 * WHAT: Projects weights to stay on learned manifold
 * WHY:  Constrains plasticity to valid solutions
 * HOW:  Uses manifold projection from info geometry
 *
 * @param bridge Bridge handle
 * @param weights Weights to project (modified in place)
 * @param num_weights Number of weights
 * @return Projection distance
 */
NIMCP_EXPORT float infogeo_plasticity_project_manifold(
    infogeo_plasticity_bridge_t* bridge,
    float* weights,
    uint32_t num_weights
);

//=============================================================================
// Geodesic Interpolation API
//=============================================================================

/**
 * @brief Initialize geodesic weight interpolation
 *
 * WHAT: Sets up geodesic interpolation between weight states
 * WHY:  Smooth transitions along shortest path
 * HOW:  Computes geodesic on weight manifold
 *
 * @param bridge Bridge handle
 * @param start_weights Starting weights
 * @param end_weights Target weights
 * @param num_weights Number of weights
 * @param num_steps Interpolation steps
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_plasticity_init_interpolation(
    infogeo_plasticity_bridge_t* bridge,
    const float* start_weights,
    const float* end_weights,
    uint32_t num_weights,
    uint32_t num_steps
);

/**
 * @brief Step geodesic interpolation
 *
 * WHAT: Advances geodesic interpolation by one step
 * WHY:  Gradual weight transition along geodesic
 * HOW:  Computes next point on geodesic path
 *
 * @param bridge Bridge handle
 * @param current_weights Output current interpolated weights
 * @param state Output interpolation state (optional)
 * @return 0 on success, 1 if complete, -1 on error
 */
NIMCP_EXPORT int infogeo_plasticity_step_interpolation(
    infogeo_plasticity_bridge_t* bridge,
    float* current_weights,
    infogeo_interpolation_state_t* state
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Decay traces, update statistics
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_plasticity_update(
    infogeo_plasticity_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_plasticity_get_stats(
    const infogeo_plasticity_bridge_t* bridge,
    infogeo_plasticity_stats_t* stats
);

/**
 * @brief Get synapse Fisher importance
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @return Fisher importance value, or -1.0 on error
 */
NIMCP_EXPORT float infogeo_plasticity_get_synapse_fisher(
    const infogeo_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get eligibility trace value
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @return Trace value, or -1.0 on error
 */
NIMCP_EXPORT float infogeo_plasticity_get_trace(
    const infogeo_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get interpolation progress
 *
 * @param bridge Bridge handle
 * @return Progress [0,1], or -1.0 if not interpolating
 */
NIMCP_EXPORT float infogeo_plasticity_get_interp_progress(
    const infogeo_plasticity_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INFOGEO_PLASTICITY_BRIDGE_H */