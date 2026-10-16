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
// nimcp_ph_hub_bridge.h - pH Dynamics to Cognitive Hub Integration Bridge
//=============================================================================
/**
 * @file nimcp_ph_hub_bridge.h
 * @brief Bridge connecting pH dynamics with cognitive hub and global workspace
 *
 * WHAT: Central integration bridge between pH dynamics and the cognitive hub,
 *       coordinating pH state with global workspace and cross-modal processing.
 *
 * WHY:  pH affects global cognitive function and workspace dynamics:
 *       - pH deviations impair cognitive performance across domains
 *       - Acidosis reduces global workspace access/broadcast
 *       - pH homeostasis maintenance competes for cognitive resources
 *       - Interoceptive pH signals contribute to conscious awareness
 *       - pH state modulates cross-modal binding efficiency
 *
 * HOW:  Central hub integration:
 *       1. pH -> Hub: pH state as interoceptive signal for workspace
 *       2. Hub -> pH: Cognitive demands affect metabolic pH
 *       3. Broadcast modulation: pH affects global ignition threshold
 *       4. Resource allocation: pH homeostasis priority signaling
 *       5. Cross-module coordination: pH effects on all connected bridges
 *
 * BIOLOGICAL BASIS:
 * ```
 * pH DYNAMICS                              COGNITIVE HUB EFFECTS
 * ---------------------------------------------------------------
 * Normal pH (7.35-7.45)                 -> Optimal workspace function
 * Mild acidosis (7.2-7.35)              -> Reduced cognitive performance
 * Severe acidosis (< 7.2)               -> Cognitive impairment
 * pH interoception                      -> Contributes to body awareness
 * Metabolic stress                      -> Priority homeostatic signaling
 * Hypercapnia (high CO2)                -> Drowsiness, confusion
 * ```
 *
 * GLOBAL WORKSPACE INTEGRATION:
 * - pH state contributes to global workspace contents
 * - Severe pH deviation triggers priority interrupts
 * - pH affects broadcast threshold and duration
 * - Metabolic state modulates workspace access competition
 *
 * COGNITIVE EFFECTS OF pH:
 * - Attention: Reduced with acidosis
 * - Working memory: Impaired at pH < 7.25
 * - Decision making: Altered risk assessment with pH deviation
 * - Processing speed: Slowed by metabolic acidosis
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PH_HUB_BRIDGE_H
#define NIMCP_PH_HUB_BRIDGE_H

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
#define PH_HUB_MODULE_NAME              "ph_hub_bridge"

/** Maximum connected bridge modules */
#define PH_HUB_MAX_MODULES              16

/** Maximum workspace coalitions */
#define PH_HUB_MAX_COALITIONS           32

/** Normal pH for optimal cognition */
#define PH_HUB_OPTIMAL_PH               7.4f

/** pH threshold for cognitive impairment */
#define PH_HUB_IMPAIRMENT_THRESHOLD     7.25f

/** pH threshold for priority interrupt */
#define PH_HUB_PRIORITY_THRESHOLD       7.15f

/** Cognitive efficiency at pH 7.0 */
#define PH_HUB_EFFICIENCY_PH_7_0        0.6f

/** Interoceptive signal gain */
#define PH_HUB_INTEROCEPTIVE_GAIN       2.0f

/** Global broadcast threshold modifier per 0.1 pH */
#define PH_HUB_BROADCAST_MOD            0.08f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Connected bridge module types
 */
typedef enum {
    PH_HUB_MODULE_SNN = 0,               /**< pH-SNN bridge */
    PH_HUB_MODULE_PLASTICITY,            /**< pH-plasticity bridge */
    PH_HUB_MODULE_FEP,                   /**< pH-FEP bridge */
    PH_HUB_MODULE_SUBSTRATE,             /**< pH-substrate bridge */
    PH_HUB_MODULE_THALAMIC,              /**< pH-thalamic bridge */
    PH_HUB_MODULE_IMMUNE,                /**< pH-immune bridge */
    PH_HUB_MODULE_COUNT
} ph_hub_module_t;

/**
 * @brief Cognitive domain affected by pH
 */
typedef enum {
    PH_HUB_DOMAIN_ATTENTION = 0,         /**< Attention systems */
    PH_HUB_DOMAIN_WORKING_MEMORY,        /**< Working memory */
    PH_HUB_DOMAIN_EXECUTIVE,             /**< Executive function */
    PH_HUB_DOMAIN_PERCEPTION,            /**< Perceptual processing */
    PH_HUB_DOMAIN_MOTOR,                 /**< Motor control */
    PH_HUB_DOMAIN_LANGUAGE,              /**< Language processing */
    PH_HUB_DOMAIN_EMOTION,               /**< Emotional processing */
    PH_HUB_DOMAIN_INTEROCEPTION,         /**< Body state awareness */
    PH_HUB_DOMAIN_COUNT
} ph_hub_cognitive_domain_t;

/**
 * @brief Global workspace access state
 */
typedef enum {
    PH_HUB_WORKSPACE_NORMAL = 0,         /**< Normal access */
    PH_HUB_WORKSPACE_REDUCED,            /**< Reduced access/competition */
    PH_HUB_WORKSPACE_PRIORITY,           /**< pH has priority access */
    PH_HUB_WORKSPACE_IMPAIRED            /**< Workspace function impaired */
} ph_hub_workspace_state_t;

/**
 * @brief Interoceptive signal priority
 */
typedef enum {
    PH_HUB_SIGNAL_BACKGROUND = 0,        /**< Background interoception */
    PH_HUB_SIGNAL_AWARE,                 /**< Available to awareness */
    PH_HUB_SIGNAL_ATTENDED,              /**< Currently attended */
    PH_HUB_SIGNAL_URGENT                 /**< Urgent/priority signal */
} ph_hub_signal_priority_t;

/**
 * @brief Coordination command type
 */
typedef enum {
    PH_HUB_CMD_NONE = 0,                 /**< No command */
    PH_HUB_CMD_UPDATE_ALL,               /**< Update all bridges */
    PH_HUB_CMD_PRIORITY_RESPONSE,        /**< Initiate priority response */
    PH_HUB_CMD_REDUCE_LOAD,              /**< Reduce cognitive load */
    PH_HUB_CMD_INCREASE_MONITORING,      /**< Increase pH monitoring */
    PH_HUB_CMD_BROADCAST_STATE           /**< Broadcast pH state globally */
} ph_hub_coord_cmd_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** Hub integration parameters */
    bool enable_workspace_integration;   /**< Integrate with global workspace */
    bool enable_interoceptive_signal;    /**< Generate interoceptive signals */
    bool enable_cognitive_modulation;    /**< Modulate cognitive domains */
    bool enable_cross_bridge_coord;      /**< Coordinate between bridges */

    /** Workspace parameters */
    float workspace_access_threshold;    /**< pH for workspace access */
    float priority_interrupt_threshold;  /**< pH for priority interrupt */
    float broadcast_duration_ms;         /**< Default broadcast duration */

    /** Cognitive modulation parameters */
    float cognitive_impairment_threshold; /**< pH for impairment onset */
    float cognitive_efficiency_floor;    /**< Minimum efficiency (0-1) */
    float efficiency_recovery_rate;      /**< Recovery rate per pH unit */

    /** Interoceptive signal parameters */
    float interoceptive_gain;            /**< Signal amplification */
    float awareness_threshold;           /**< pH deviation for awareness */
    float urgency_threshold;             /**< pH deviation for urgency */

    /** Coordination parameters */
    float coordination_interval_ms;      /**< Inter-bridge coord interval */
    bool auto_priority_response;         /**< Auto-trigger priority response */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
} ph_hub_config_t;

/**
 * @brief Per-domain cognitive efficiency
 */
typedef struct {
    ph_hub_cognitive_domain_t domain;    /**< Cognitive domain */
    float efficiency;                    /**< Current efficiency (0-1) */
    float baseline_efficiency;           /**< Baseline at normal pH */
    float ph_sensitivity;                /**< Sensitivity to pH changes */
    float current_impairment;            /**< Current impairment level */
    bool is_critical;                    /**< Domain critically affected */
} ph_hub_domain_state_t;

/**
 * @brief Global workspace state
 */
typedef struct {
    ph_hub_workspace_state_t state;      /**< Current workspace state */
    float access_probability;            /**< P(pH access workspace) */
    float broadcast_threshold;           /**< Current broadcast threshold */
    float ignition_strength;             /**< Global ignition strength */
    float competition_strength;          /**< pH competition strength */
    bool ph_in_workspace;                /**< pH currently in workspace */
    float workspace_duration_ms;         /**< Time in workspace */
    uint32_t active_coalitions;          /**< Active coalition count */
} ph_hub_workspace_info_t;

/**
 * @brief Interoceptive pH signal
 */
typedef struct {
    ph_hub_signal_priority_t priority;   /**< Signal priority */
    float signal_strength;               /**< Signal strength (0-1) */
    float deviation_magnitude;           /**< pH deviation magnitude */
    float prediction_error;              /**< Interoceptive PE */
    float uncertainty;                   /**< Signal uncertainty */
    float valence;                       /**< Negative (acidosis) or neutral */
    float arousal;                       /**< Arousal component */
    bool contributes_to_awareness;       /**< Part of conscious awareness */
    float timestamp_ms;                  /**< Signal timestamp */
} ph_hub_interoceptive_signal_t;

/**
 * @brief Cross-bridge coordination state
 */
typedef struct {
    /** Per-module status */
    bool module_active[PH_HUB_MODULE_COUNT];
    float module_ph_effect[PH_HUB_MODULE_COUNT];

    /** Coordination state */
    ph_hub_coord_cmd_t pending_command;  /**< Pending coordination command */
    float global_ph;                     /**< Current global pH */
    float global_ph_effect;              /**< Combined pH effect */
    float coordination_coherence;        /**< Inter-bridge coherence */

    /** Resource allocation */
    float homeostatic_priority;          /**< Priority for pH homeostasis */
    float cognitive_resource_budget;     /**< Available cognitive resources */
} ph_hub_coordination_t;

/**
 * @brief Hub output for cognitive systems
 */
typedef struct {
    /** Cognitive efficiency modulation */
    float global_efficiency_factor;      /**< Overall efficiency (0-1) */
    ph_hub_domain_state_t domains[PH_HUB_DOMAIN_COUNT];

    /** Workspace modulation */
    float broadcast_threshold_modifier;  /**< Broadcast threshold mod */
    float ignition_threshold_modifier;   /**< Ignition threshold mod */
    float access_competition_modifier;   /**< Access competition mod */

    /** Resource allocation */
    float attention_allocation;          /**< Attention to pH state */
    float homeostatic_urgency;           /**< Urgency of pH correction */

    /** Interoceptive output */
    ph_hub_interoceptive_signal_t signal; /**< Interoceptive signal */
} ph_hub_output_t;

/**
 * @brief Cognitive load feedback for pH
 */
typedef struct {
    float total_cognitive_load;          /**< Total cognitive demand (0-1) */
    float working_memory_load;           /**< WM demand */
    float attention_load;                /**< Attention demand */
    float processing_intensity;          /**< Processing intensity */
    float metabolic_demand;              /**< Estimated metabolic demand */
    float acid_production_estimate;      /**< Estimated acid production */
    float timestamp_ms;                  /**< Feedback timestamp */
} ph_hub_cognitive_feedback_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t ph_updates;                 /**< pH state updates */
    uint64_t workspace_entries;          /**< Times pH entered workspace */
    uint64_t priority_interrupts;        /**< Priority interrupt events */
    uint64_t coordination_events;        /**< Cross-bridge coordinations */
    float avg_cognitive_efficiency;      /**< Average cognitive efficiency */
    float time_impaired_ms;              /**< Time with impaired cognition */
    float time_in_workspace_ms;          /**< Time pH in workspace */
    float avg_interoceptive_strength;    /**< Average signal strength */
    float total_metabolic_feedback;      /**< Total metabolic feedback */
    float last_update_ms;                /**< Last update timestamp */
} ph_hub_stats_t;

/** Opaque bridge handle */
typedef struct ph_hub_bridge_struct ph_hub_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_default_config(ph_hub_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create pH-hub bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ph_hub_bridge_t* ph_hub_bridge_create(
    const ph_hub_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ph_hub_bridge_destroy(ph_hub_bridge_t* bridge);

/**
 * @brief Register connected pH bridge module
 *
 * @param bridge Hub bridge handle
 * @param module Module type
 * @param module_handle Opaque handle to module
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_register_module(
    ph_hub_bridge_t* bridge,
    ph_hub_module_t module,
    void* module_handle
);

/**
 * @brief Unregister connected module
 *
 * @param bridge Hub bridge handle
 * @param module Module type
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_unregister_module(
    ph_hub_bridge_t* bridge,
    ph_hub_module_t module
);

//=============================================================================
// pH Input API (pH Dynamics -> Hub)
//=============================================================================

/**
 * @brief Update pH state from pH dynamics module
 *
 * WHAT: Receives current pH values for hub integration
 * WHY:  pH state affects global cognitive function
 * HOW:  Updates workspace state, cognitive efficiency
 *
 * @param bridge Bridge handle
 * @param extracellular_ph Current extracellular pH
 * @param intracellular_ph Current intracellular pH
 * @param buffer_capacity Current buffer capacity
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_update_ph(
    ph_hub_bridge_t* bridge,
    float extracellular_ph,
    float intracellular_ph,
    float buffer_capacity
);

/**
 * @brief Report pH homeostatic status
 *
 * @param bridge Bridge handle
 * @param homeostasis_achieved Whether pH is in normal range
 * @param correction_effort Effort to maintain homeostasis (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_report_homeostasis(
    ph_hub_bridge_t* bridge,
    bool homeostasis_achieved,
    float correction_effort
);

//=============================================================================
// Hub Output API
//=============================================================================

/**
 * @brief Compute hub output for cognitive systems
 *
 * WHAT: Computes all pH-dependent cognitive modulations
 * WHY:  Single call for complete hub output
 * HOW:  Evaluates workspace, efficiency, interoception
 *
 * @param bridge Bridge handle
 * @param output Output hub modulation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_compute_output(
    ph_hub_bridge_t* bridge,
    ph_hub_output_t* output
);

/**
 * @brief Get global cognitive efficiency
 *
 * @param bridge Bridge handle
 * @param efficiency Output: efficiency factor (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_get_cognitive_efficiency(
    const ph_hub_bridge_t* bridge,
    float* efficiency
);

/**
 * @brief Get domain-specific efficiency
 *
 * @param bridge Bridge handle
 * @param domain Cognitive domain
 * @param efficiency Output: domain efficiency (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_get_domain_efficiency(
    const ph_hub_bridge_t* bridge,
    ph_hub_cognitive_domain_t domain,
    float* efficiency
);

/**
 * @brief Get interoceptive signal
 *
 * @param bridge Bridge handle
 * @param signal Output interoceptive signal
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_get_interoceptive_signal(
    const ph_hub_bridge_t* bridge,
    ph_hub_interoceptive_signal_t* signal
);

//=============================================================================
// Global Workspace API
//=============================================================================

/**
 * @brief Get workspace state
 *
 * @param bridge Bridge handle
 * @param info Output workspace info
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_get_workspace_state(
    const ph_hub_bridge_t* bridge,
    ph_hub_workspace_info_t* info
);

/**
 * @brief Request workspace broadcast
 *
 * WHAT: Requests pH state broadcast to global workspace
 * WHY:  Make pH state available to all cognitive processes
 * HOW:  Competes for workspace access
 *
 * @param bridge Bridge handle
 * @param urgency Urgency of request (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_request_broadcast(
    ph_hub_bridge_t* bridge,
    float urgency
);

/**
 * @brief Check if pH is in global workspace
 *
 * @param bridge Bridge handle
 * @return true if pH currently in workspace
 */
NIMCP_EXPORT bool ph_hub_in_workspace(
    const ph_hub_bridge_t* bridge
);

/**
 * @brief Get workspace access probability
 *
 * @param bridge Bridge handle
 * @param probability Output: access probability
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_get_access_probability(
    const ph_hub_bridge_t* bridge,
    float* probability
);

//=============================================================================
// Cross-Bridge Coordination API
//=============================================================================

/**
 * @brief Get coordination state
 *
 * @param bridge Bridge handle
 * @param coordination Output coordination state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_get_coordination(
    const ph_hub_bridge_t* bridge,
    ph_hub_coordination_t* coordination
);

/**
 * @brief Issue coordination command
 *
 * @param bridge Bridge handle
 * @param command Command to issue
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_issue_command(
    ph_hub_bridge_t* bridge,
    ph_hub_coord_cmd_t command
);

/**
 * @brief Trigger priority response across bridges
 *
 * WHAT: Initiates priority response for pH emergency
 * WHY:  Coordinate all bridges for critical pH correction
 * HOW:  Sends priority signals to all registered modules
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_trigger_priority_response(
    ph_hub_bridge_t* bridge
);

//=============================================================================
// Cognitive Feedback API (Hub -> pH)
//=============================================================================

/**
 * @brief Report cognitive load for metabolic feedback
 *
 * WHAT: Receives cognitive load from hub systems
 * WHY:  Cognitive demand affects metabolic acid production
 * HOW:  Accumulates demand, estimates acid production
 *
 * @param bridge Bridge handle
 * @param feedback Cognitive load feedback
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_report_cognitive_load(
    ph_hub_bridge_t* bridge,
    const ph_hub_cognitive_feedback_t* feedback
);

/**
 * @brief Get metabolic demand from cognition
 *
 * @param bridge Bridge handle
 * @param metabolic_demand Output: metabolic demand (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_get_metabolic_demand(
    const ph_hub_bridge_t* bridge,
    float* metabolic_demand
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Coordinate modules, update workspace state
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_update(
    ph_hub_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_hub_reset(ph_hub_bridge_t* bridge);

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
NIMCP_EXPORT int ph_hub_get_stats(
    const ph_hub_bridge_t* bridge,
    ph_hub_stats_t* stats
);

/**
 * @brief Check if cognition is impaired by pH
 *
 * @param bridge Bridge handle
 * @return true if cognitive impairment active
 */
NIMCP_EXPORT bool ph_hub_is_cognition_impaired(
    const ph_hub_bridge_t* bridge
);

/**
 * @brief Check if pH state is urgent
 *
 * @param bridge Bridge handle
 * @return true if pH deviation requires urgent attention
 */
NIMCP_EXPORT bool ph_hub_is_urgent(
    const ph_hub_bridge_t* bridge
);

/**
 * @brief Get number of registered modules
 *
 * @param bridge Bridge handle
 * @return Number of registered modules
 */
NIMCP_EXPORT uint32_t ph_hub_get_module_count(
    const ph_hub_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PH_HUB_BRIDGE_H */