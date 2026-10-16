/**
 * @file nimcp_executive_substrate_bridge.h
 * @brief Executive-Substrate Integration Bridge
 *
 * BIOLOGICAL BASIS:
 * The prefrontal cortex (PFC), responsible for executive functions, is one of the most
 * metabolically demanding brain regions. Executive control processes including decision-making,
 * inhibition, planning, and cognitive flexibility require substantial ATP to maintain
 * neuronal firing, neurotransmitter synthesis, and synaptic transmission.
 *
 * METABOLIC DEMANDS:
 * - Decision quality depends on sufficient ATP for sustained PFC neural activity
 * - Inhibitory control (impulse suppression) requires GABAergic signaling energy
 * - Planning depth correlates with working memory maintenance costs
 * - Cognitive flexibility involves metabolic cost of task-switching overhead
 * - Fatigue accumulates when energy expenditure exceeds ATP availability
 *
 * KEY PRINCIPLES:
 * 1. ATP depletion progressively impairs executive function quality
 * 2. Inhibition fails first (frontal lobe vulnerability to hypometabolism)
 * 3. Planning horizon shortens under metabolic stress (working memory collapse)
 * 4. Fatigue reduces cognitive flexibility (perseveration under energy scarcity)
 * 5. Severe depletion causes executive impairment (dysexecutive syndrome)
 *
 * INTEGRATION:
 * - Neural substrate provides ATP levels and metabolic state
 * - Executive system consumes ATP for control operations
 * - Bridge modulates executive performance based on energy availability
 * - Bio-async messaging coordinates with other substrate bridges
 *
 * @author NIMCP Development Team
 * @date 2024-12
 */

#ifndef NIMCP_EXECUTIVE_SUBSTRATE_BRIDGE_H
#define NIMCP_EXECUTIVE_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/nimcp_executive.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Use executive_controller_t from nimcp_executive.h */
typedef executive_controller_t nimcp_executive_t;

/* ========================================================================
 * CONSTANTS
 * ======================================================================== */

/**
 * @brief Bio-async module identifier for executive substrate bridge
 *
 * WHY: Enables inter-module communication with other substrate bridges
 * HOW: Registered with bio_router for message passing
 */
#define BIO_MODULE_SUBSTRATE_EXECUTIVE 0x1202

/**
 * @brief ATP thresholds for executive function (fraction of maximum)
 *
 * BIOLOGICAL BASIS:
 * - Normal function: >70% ATP (full executive control)
 * - Mild fatigue: 50-70% ATP (reduced planning, flexibility)
 * - Moderate impairment: 30-50% ATP (poor decisions, weak inhibition)
 * - Severe impairment: <30% ATP (executive dysfunction)
 */
#define EXECUTIVE_ATP_OPTIMAL_THRESHOLD 0.70f    /* Full executive function */
#define EXECUTIVE_ATP_FATIGUE_THRESHOLD 0.50f    /* Noticeable fatigue onset */
#define EXECUTIVE_ATP_IMPAIRED_THRESHOLD 0.30f   /* Executive impairment */
#define EXECUTIVE_ATP_CRITICAL_THRESHOLD 0.15f   /* Severe dysfunction */

/**
 * @brief ATP consumption rates for executive operations
 *
 * WHY: Executive functions have varying metabolic costs
 * HOW: Planning > Decision > Inhibition in energy requirements
 */
#define EXECUTIVE_ATP_COST_DECISION 0.05f        /* Per decision operation */
#define EXECUTIVE_ATP_COST_INHIBITION 0.03f      /* Per inhibition event */
#define EXECUTIVE_ATP_COST_PLANNING 0.08f        /* Per planning step */
#define EXECUTIVE_ATP_COST_TASK_SWITCH 0.06f     /* Per task switch */

/* ========================================================================
 * STRUCTURES
 * ======================================================================== */

/**
 * @brief Executive substrate effects
 *
 * WHAT: Computed effects of metabolic state on executive functions
 * WHY: Quantifies how ATP availability impacts cognitive control
 * HOW: Values scaled 0-1, reduced when ATP depleted
 */
typedef struct {
    float decision_quality;         /**< Decision-making accuracy [0-1] */
    float inhibition_strength;      /**< Impulse control strength [0-1] */
    float planning_depth;           /**< Planning horizon capacity [0-1] */
    float cognitive_flexibility;    /**< Task switching ability [0-1] */
    float fatigue_level;            /**< Accumulated fatigue [0-1] */
    bool is_impaired;               /**< Executive impairment flag */
} executive_substrate_effects_t;

/**
 * @brief Executive substrate bridge configuration
 *
 * WHAT: Configuration for executive-substrate integration
 * WHY: Allows tuning of metabolic sensitivity and feature enables
 * HOW: Default config provides biologically plausible parameters
 */
typedef struct {
    /* Feature enables */
    bool enable_decision_modulation;     /**< Modulate decision quality */
    bool enable_inhibition_modulation;   /**< Modulate impulse control */
    bool enable_planning_modulation;     /**< Modulate planning depth */
    bool enable_flexibility_modulation;  /**< Modulate task switching */
    bool enable_fatigue_tracking;        /**< Track cumulative fatigue */

    /* Sensitivity parameters */
    float decision_sensitivity;          /**< Decision quality ATP sensitivity [0-1] */
    float inhibition_sensitivity;        /**< Inhibition ATP sensitivity [0-1] */
    float planning_sensitivity;          /**< Planning depth ATP sensitivity [0-1] */
    float flexibility_sensitivity;       /**< Flexibility ATP sensitivity [0-1] */
    float fatigue_accumulation_rate;     /**< Rate of fatigue buildup */
    float fatigue_recovery_rate;         /**< Rate of fatigue recovery */

    /* Thresholds */
    float impairment_threshold;          /**< ATP level for impairment flag */
    float critical_threshold;            /**< ATP level for severe dysfunction */

    /* Bio-async configuration */
    bool enable_bio_async;               /**< Enable bio-async messaging */
    uint32_t inbox_capacity;             /**< Message inbox size */
} executive_substrate_config_t;

/**
 * @brief Executive substrate bridge statistics
 *
 * WHAT: Performance and health metrics for the bridge
 * WHY: Tracks metabolic impact on executive function over time
 * HOW: Counters and aggregates updated during bridge operation
 */
typedef struct {
    uint64_t update_count;               /**< Number of updates performed */
    uint64_t impairment_events;          /**< Times executive became impaired */
    uint64_t decision_operations;        /**< Decision-making operations */
    uint64_t inhibition_operations;      /**< Inhibition events */
    uint64_t planning_operations;        /**< Planning operations */
    uint64_t task_switches;              /**< Task switching events */

    /* Metabolic tracking */
    float total_atp_consumed;            /**< Total ATP consumed */
    float avg_atp_level;                 /**< Average ATP level observed */
    float min_atp_level;                 /**< Minimum ATP level observed */

    /* Performance tracking */
    float avg_decision_quality;          /**< Average decision quality */
    float avg_inhibition_strength;       /**< Average inhibition strength */
    float avg_planning_depth;            /**< Average planning depth */
    float avg_flexibility;               /**< Average cognitive flexibility */
    float max_fatigue_level;             /**< Maximum fatigue reached */
} executive_substrate_stats_t;

/**
 * @brief Executive substrate bridge structure
 *
 * WHAT: Main bridge structure integrating executive and substrate systems
 * WHY: Coordinates metabolic modulation of executive functions
 * HOW: Updates effects based on ATP, applies to executive system
 */
typedef struct executive_substrate_bridge_t {
    bridge_base_t base;                  /**< MUST be first: base bridge infrastructure */

    /* Core components */
    neural_substrate_t* substrate;       /**< Neural substrate system */
    nimcp_executive_t* executive;        /**< Executive function system */

    /* Configuration and state */
    executive_substrate_config_t config; /**< Bridge configuration */
    executive_substrate_effects_t effects; /**< Current effects */
    executive_substrate_stats_t stats;   /**< Performance statistics */

    /* Internal state */
    float last_atp_level;                /**< Previous ATP level */
    uint64_t last_update_time;           /**< Last update timestamp */
    bool initialized;                    /**< Initialization flag */
} executive_substrate_bridge_t;

/* ========================================================================
 * API FUNCTIONS
 * ======================================================================== */

/**
 * @brief Initialize default executive substrate configuration
 *
 * WHAT: Populates config with biologically plausible defaults
 * WHY: Provides starting point for executive-substrate integration
 * HOW: Sets feature enables, sensitivities, and thresholds
 *
 * @param config Configuration structure to initialize
 */
void executive_substrate_default_config(executive_substrate_config_t* config);

/**
 * @brief Create executive substrate bridge
 *
 * WHAT: Allocates and initializes executive-substrate bridge
 * WHY: Establishes metabolic modulation of executive functions
 * HOW: Connects to substrate and executive systems, starts monitoring
 *
 * BIOLOGICAL BASIS: Prefrontal cortex metabolic dependence on ATP availability
 *
 * @param config Bridge configuration
 * @param executive Executive function system
 * @param substrate Neural substrate system
 * @return Initialized bridge, or NULL on failure
 */
executive_substrate_bridge_t* executive_substrate_bridge_create(
    const executive_substrate_config_t* config,
    nimcp_executive_t* executive,
    neural_substrate_t* substrate
);

/**
 * @brief Destroy executive substrate bridge
 *
 * WHAT: Cleans up and deallocates bridge
 * WHY: Releases resources, disconnects from systems
 * HOW: Disconnects bio-async, frees memory, destroys mutex
 *
 * @param bridge Bridge to destroy
 */
void executive_substrate_bridge_destroy(executive_substrate_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Registers bridge as bio-async module
 * WHY: Enables inter-module messaging with other substrate bridges
 * HOW: Registers with BIO_MODULE_SUBSTRATE_EXECUTIVE ID
 *
 * @param bridge Executive substrate bridge
 * @return 0 on success, negative on error
 */
int executive_substrate_connect_bio_async(executive_substrate_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * WHAT: Unregisters bridge from bio-async system
 * WHY: Clean shutdown of messaging capabilities
 * HOW: Deregisters module, clears context
 *
 * @param bridge Executive substrate bridge
 * @return 0 on success, negative on error
 */
int executive_substrate_disconnect_bio_async(executive_substrate_bridge_t* bridge);

/**
 * @brief Check if bridge is connected to bio-async
 *
 * WHAT: Queries bio-async connection status
 * WHY: Allows conditional messaging behavior
 * HOW: Checks bio_async_enabled flag
 *
 * @param bridge Executive substrate bridge
 * @return true if connected, false otherwise
 */
bool executive_substrate_is_bio_async_connected(const executive_substrate_bridge_t* bridge);

/**
 * @brief Update executive substrate effects
 *
 * WHAT: Recomputes effects based on current ATP levels
 * WHY: Maintains accurate metabolic modulation of executive functions
 * HOW: Queries substrate ATP, updates effects, tracks statistics
 *
 * BIOLOGICAL BASIS:
 * - ATP depletion progressively impairs executive function
 * - Inhibition fails before other executive functions
 * - Planning depth correlates with working memory ATP availability
 * - Fatigue accumulates with sustained high cognitive load
 *
 * @param bridge Executive substrate bridge
 * @return 0 on success, negative on error
 */
int executive_substrate_update(executive_substrate_bridge_t* bridge);

/**
 * @brief Get current decision quality factor
 *
 * WHAT: Returns decision-making accuracy modulation [0-1]
 * WHY: Quantifies metabolic impact on decision quality
 * HOW: Scales with ATP availability above fatigue threshold
 *
 * @param bridge Executive substrate bridge
 * @return Decision quality [0-1], or 1.0 if disabled
 */
float executive_substrate_get_decision_quality(const executive_substrate_bridge_t* bridge);

/**
 * @brief Get current inhibition strength factor
 *
 * WHAT: Returns impulse control strength modulation [0-1]
 * WHY: Quantifies metabolic impact on inhibitory control
 * HOW: Most sensitive to ATP depletion (frontal vulnerability)
 *
 * @param bridge Executive substrate bridge
 * @return Inhibition strength [0-1], or 1.0 if disabled
 */
float executive_substrate_get_inhibition_strength(const executive_substrate_bridge_t* bridge);

/**
 * @brief Get current planning depth factor
 *
 * WHAT: Returns planning horizon capacity modulation [0-1]
 * WHY: Quantifies metabolic impact on planning ability
 * HOW: Scales with working memory ATP availability
 *
 * @param bridge Executive substrate bridge
 * @return Planning depth [0-1], or 1.0 if disabled
 */
float executive_substrate_get_planning_depth(const executive_substrate_bridge_t* bridge);

/**
 * @brief Get current cognitive flexibility factor
 *
 * WHAT: Returns task switching ability modulation [0-1]
 * WHY: Quantifies metabolic impact on flexibility
 * HOW: Reflects ATP cost of switching between task sets
 *
 * @param bridge Executive substrate bridge
 * @return Cognitive flexibility [0-1], or 1.0 if disabled
 */
float executive_substrate_get_cognitive_flexibility(const executive_substrate_bridge_t* bridge);

/**
 * @brief Get current fatigue level
 *
 * WHAT: Returns accumulated fatigue [0-1]
 * WHY: Tracks cumulative metabolic strain on executive system
 * HOW: Accumulates when ATP low, recovers when ATP high
 *
 * @param bridge Executive substrate bridge
 * @return Fatigue level [0-1], or 0.0 if disabled
 */
float executive_substrate_get_fatigue(const executive_substrate_bridge_t* bridge);

/**
 * @brief Get complete effects structure
 *
 * WHAT: Returns all current substrate effects
 * WHY: Provides comprehensive view of metabolic modulation
 * HOW: Returns copy of internal effects structure
 *
 * @param bridge Executive substrate bridge
 * @return Current effects structure
 */
executive_substrate_effects_t executive_substrate_get_effects(
    const executive_substrate_bridge_t* bridge
);

/**
 * @brief Check if executive function is impaired
 *
 * WHAT: Returns executive impairment status
 * WHY: Indicates when ATP depletion causes dysfunction
 * HOW: True when ATP below impairment threshold
 *
 * @param bridge Executive substrate bridge
 * @return true if impaired, false otherwise
 */
bool executive_substrate_is_impaired(const executive_substrate_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Returns performance and health metrics
 * WHY: Enables monitoring of metabolic impact over time
 * HOW: Returns copy of internal statistics structure
 *
 * @param bridge Executive substrate bridge
 * @return Current statistics structure
 */
executive_substrate_stats_t executive_substrate_get_stats(
    const executive_substrate_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXECUTIVE_SUBSTRATE_BRIDGE_H */
