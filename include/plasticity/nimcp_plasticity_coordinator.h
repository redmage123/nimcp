/**
 * @file nimcp_plasticity_coordinator.h
 * @brief Plasticity Coordinator - Unified Manager for All Plasticity Mechanisms
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Central coordinator managing 8 plasticity mechanisms with conflict resolution,
 *       scheduling, energy budgeting, and state-based coordination
 * WHY:  Multiple plasticity rules can conflict (STDP→LTP, BCM→LTD). A coordinator
 *       ensures coherent learning by resolving conflicts, scheduling updates, and
 *       integrating with brain immune and metabolic systems.
 * HOW:  Registry pattern for mechanisms, state machine for plasticity modes,
 *       strategy pattern for conflict resolution, mediator for cross-mechanism coordination
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * MULTIPLE PLASTICITY TIMESCALES:
 * --------------------------------
 * The brain employs plasticity at multiple timescales simultaneously:
 * - Fast (ms): Short-term plasticity (STP) - vesicle depletion/facilitation
 * - Medium (100ms-1s): STDP, BCM - spike-timing and rate-based rules
 * - Slow (minutes-hours): Homeostatic, synaptic scaling - network stabilization
 * - Very slow (hours-days): Consolidation, structural plasticity
 *
 * PLASTICITY STATES (LEARNING PHASES):
 * ------------------------------------
 * Different brain states prioritize different plasticity modes:
 * 1. ACQUISITION: New learning (STDP+BCM dominant, homeostatic suppressed)
 * 2. CONSOLIDATION: Memory stabilization (eligibility→weight, homeostatic active)
 * 3. MAINTENANCE: Stable state (minimal plasticity, energy conservation)
 * 4. STABILIZING: Preventing runaway (homeostatic+scaling dominant)
 *
 * CONFLICT RESOLUTION:
 * --------------------
 * When multiple rules disagree (e.g., STDP→LTP but BCM→LTD):
 * - STDP_DOMINANT: STDP wins (early learning, precise timing)
 * - BCM_DOMINANT: BCM wins (rate-based stabilization)
 * - AVERAGE: Average the two signals (balanced learning)
 * - IMMUNE_MODULATED: Brain immune modulates resolution (inflammation→conservative)
 *
 * ENERGY BUDGET:
 * --------------
 * Plasticity is metabolically expensive (ATP for receptor trafficking, protein synthesis):
 * - Each mechanism has energy cost per update
 * - Coordinator tracks total energy expenditure
 * - Low ATP → reduce plasticity (survival priority)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    PLASTICITY COORDINATOR                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   MECHANISM REGISTRY                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────┐  ┌──────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │  ║
 * ║   │   │ STDP │  │ BCM  │  │Homeosta  │  │Eligibili │  │Dendritic │   │  ║
 * ║   │   │      │  │      │  │  tic     │  │   ty     │  │          │   │  ║
 * ║   │   └──────┘  └──────┘  └──────────┘  └──────────┘  └──────────┘   │  ║
 * ║   │   ┌──────┐  ┌──────────┐  ┌──────────────┐                        │  ║
 * ║   │   │ STP  │  │Adaptive  │  │Predictive    │                        │  ║
 * ║   │   │      │  │          │  │Coding        │                        │  ║
 * ║   │   └──────┘  └──────────┘  └──────────────┘                        │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   STATE MACHINE                                     │  ║
 * ║   │                                                                     │  ║
 * ║   │   ACQUISITION → CONSOLIDATION → MAINTENANCE → STABILIZING          │  ║
 * ║   │        ↑              ↓              ↓              ↓               │  ║
 * ║   │        └──────────────┴──────────────┴──────────────┘               │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              CONFLICT RESOLUTION & SCHEDULING                       │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐ │  ║
 * ║   │   │  STDP vs BCM    │   │ Time-Multiplex  │   │ Energy Budget   │ │  ║
 * ║   │   │  Arbitration    │   │  STP/STDP/BCM   │   │   Tracking      │ │  ║
 * ║   │   └─────────────────┘   └─────────────────┘   └─────────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   INTEGRATION LAYER                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   Brain Immune System        Bio-Async Router                      │  ║
 * ║   │   ──────────────────         ──────────────────                    │  ║
 * ║   │   Inflammation → LR reduce   Inter-mechanism messaging             │  ║
 * ║   │   Cytokines → Conflict bias  Plasticity event propagation          │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Registry: Dynamic mechanism registration/unregistration
 * - State: Different plasticity modes with distinct behaviors
 * - Strategy: Pluggable conflict resolution strategies
 * - Mediator: Coordinates cross-mechanism interactions
 * - Observer: Event callbacks for plasticity changes
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * REFERENCES:
 * - Turrigiano & Nelson (2004) "Homeostatic plasticity in the developing nervous system"
 * - Clopath et al. (2010) "Connectivity reflects coding: a model of voltage-based STDP"
 * - Zenke & Ganguli (2018) "SuperSpike: Supervised learning in multilayer spiking neural networks"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PLASTICITY_COORDINATOR_H
#define NIMCP_PLASTICITY_COORDINATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Plasticity mechanisms */
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "plasticity/dendritic/nimcp_dendritic.h"
#include "plasticity/stp/nimcp_stp.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "plasticity/predictive/nimcp_predictive_coding.h"

/* Integration modules */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* Utilities */
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PLASTICITY_COORDINATOR_MAX_MECHANISMS  32    /**< Max registered mechanisms */
#define PLASTICITY_COORDINATOR_MODULE_NAME     "plasticity_coordinator"
#define PLASTICITY_COORDINATOR_MAX_CONFLICTS   64    /**< Max tracked conflicts per update */

/* Energy costs (arbitrary ATP units per update) */
#define PLASTICITY_ENERGY_COST_STDP           1.0f   /**< STDP spike processing */
#define PLASTICITY_ENERGY_COST_BCM            2.0f   /**< BCM threshold sliding */
#define PLASTICITY_ENERGY_COST_HOMEOSTATIC    3.0f   /**< Homeostatic scaling */
#define PLASTICITY_ENERGY_COST_ELIGIBILITY    0.5f   /**< Eligibility trace decay */
#define PLASTICITY_ENERGY_COST_DENDRITIC      1.5f   /**< Dendritic computation */
#define PLASTICITY_ENERGY_COST_STP            0.3f   /**< STP state update */
#define PLASTICITY_ENERGY_COST_ADAPTIVE       1.0f   /**< Adaptive threshold */
#define PLASTICITY_ENERGY_COST_PREDICTIVE     2.5f   /**< Predictive coding */

/* Default update intervals (milliseconds) */
#define PLASTICITY_UPDATE_INTERVAL_STP        1      /**< Fast: STP every 1ms */
#define PLASTICITY_UPDATE_INTERVAL_STDP       10     /**< Medium: STDP every 10ms */
#define PLASTICITY_UPDATE_INTERVAL_BCM        50     /**< Medium: BCM every 50ms */
#define PLASTICITY_UPDATE_INTERVAL_ADAPTIVE   100    /**< Slow: Adaptive every 100ms */
#define PLASTICITY_UPDATE_INTERVAL_HOMEOSTATIC 1000  /**< Very slow: Homeostatic every 1s */
#define PLASTICITY_UPDATE_INTERVAL_PREDICTIVE 20     /**< Medium: Predictive every 20ms */
#define PLASTICITY_UPDATE_INTERVAL_ELIGIBILITY 10    /**< Medium: Eligibility every 10ms */
#define PLASTICITY_UPDATE_INTERVAL_DENDRITIC  10     /**< Medium: Dendritic every 10ms */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct plasticity_coordinator plasticity_coordinator_t;

/* Forward-declare tpb_context_t to avoid a circular include with the
 * training-plasticity-bridge header (which depends on many cognitive types). */
typedef struct tpb_context tpb_context_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Plasticity mechanism types
 *
 * WHAT: Types of plasticity mechanisms the coordinator can manage
 * WHY:  Need unique identifiers for each mechanism type
 */
typedef enum {
    PLASTICITY_TYPE_STDP = 0,          /**< Spike-timing-dependent plasticity */
    PLASTICITY_TYPE_BCM,               /**< Bienenstock-Cooper-Munro */
    PLASTICITY_TYPE_HOMEOSTATIC,       /**< Homeostatic plasticity */
    PLASTICITY_TYPE_ELIGIBILITY,       /**< Eligibility traces */
    PLASTICITY_TYPE_DENDRITIC,         /**< Dendritic plasticity */
    PLASTICITY_TYPE_STP,               /**< Short-term plasticity */
    PLASTICITY_TYPE_ADAPTIVE,          /**< Adaptive threshold */
    PLASTICITY_TYPE_PREDICTIVE,        /**< Predictive coding */
    PLASTICITY_TYPE_STRUCTURAL,        /**< Structural plasticity (synaptogenesis) */
    PLASTICITY_TYPE_COUNT              /**< Total types */
} plasticity_mechanism_type_t;

/**
 * @brief Plasticity coordinator state
 *
 * WHAT: Overall learning phase/mode
 * WHY:  Different states prioritize different mechanisms
 * HOW:  State machine controls which mechanisms are dominant
 *
 * BIOLOGICAL: Maps to sleep/wake cycles, learning phases
 */
typedef enum {
    PLASTICITY_STATE_ACQUISITION = 0,  /**< New learning (STDP+BCM active) */
    PLASTICITY_STATE_CONSOLIDATION,    /**< Memory consolidation (eligibility→weight) */
    PLASTICITY_STATE_MAINTENANCE,      /**< Stable state (minimal plasticity) */
    PLASTICITY_STATE_STABILIZING,      /**< Preventing runaway (homeostatic dominant) */
    PLASTICITY_STATE_COUNT
} plasticity_coordinator_state_t;

/**
 * @brief Conflict resolution strategy
 *
 * WHAT: How to resolve conflicts when mechanisms disagree
 * WHY:  STDP may say LTP, BCM may say LTD - need resolution
 * HOW:  Different strategies for different learning contexts
 */
typedef enum {
    CONFLICT_RESOLUTION_STDP_DOMINANT = 0,  /**< STDP wins (precise timing) */
    CONFLICT_RESOLUTION_BCM_DOMINANT,       /**< BCM wins (rate-based) */
    CONFLICT_RESOLUTION_AVERAGE,            /**< Average the signals */
    CONFLICT_RESOLUTION_WEIGHTED_AVERAGE,   /**< Weighted by mechanism priority */
    CONFLICT_RESOLUTION_IMMUNE_MODULATED,   /**< Brain immune modulates */
    CONFLICT_RESOLUTION_ENERGY_LIMITED,     /**< Lowest energy cost wins */
    CONFLICT_RESOLUTION_COUNT
} conflict_resolution_strategy_t;

/* ============================================================================
 * Mechanism Registration Structures
 * ============================================================================ */

/**
 * @brief Generic mechanism handle (opaque pointer)
 */
typedef void* plasticity_mechanism_handle_t;

/**
 * @brief Mechanism update callback
 *
 * WHAT: Function pointer for updating a mechanism
 * WHY:  Each mechanism has different update signature; callback abstracts this
 *
 * @param mechanism Mechanism handle
 * @param dt Time delta (seconds)
 * @return 0 on success, -1 on error
 */
typedef int (*plasticity_mechanism_update_fn_t)(plasticity_mechanism_handle_t mechanism, float dt);

/**
 * @brief Mechanism weight change query callback
 *
 * WHAT: Get proposed weight change from a mechanism
 * WHY:  For conflict resolution, need to know what each mechanism wants
 *
 * @param mechanism Mechanism handle
 * @param synapse_id Synapse identifier
 * @param weight_change_out Output: proposed weight change
 * @return 0 on success, -1 on error
 */
typedef int (*plasticity_mechanism_get_weight_change_fn_t)(
    plasticity_mechanism_handle_t mechanism,
    uint32_t synapse_id,
    float* weight_change_out
);

/**
 * @brief Registered mechanism entry
 */
typedef struct {
    uint32_t mechanism_id;                         /**< Unique mechanism ID */
    const char* mechanism_name;                    /**< Human-readable name */
    plasticity_mechanism_type_t type;              /**< Mechanism type */
    plasticity_mechanism_handle_t handle;          /**< Opaque mechanism pointer */
    plasticity_mechanism_update_fn_t update_fn;    /**< Update callback */
    plasticity_mechanism_get_weight_change_fn_t get_weight_change_fn; /**< Query callback */

    /* Scheduling */
    uint64_t update_interval_ms;                   /**< Update interval (milliseconds) */
    uint64_t last_update_time;                     /**< Last update timestamp */

    /* Priority & energy */
    float priority;                                /**< Mechanism priority [0-1] */
    float energy_cost;                             /**< Energy cost per update */

    /* State */
    bool enabled;                                  /**< Whether updates are enabled */
    uint64_t update_count;                         /**< Total updates */
    float total_energy_consumed;                   /**< Cumulative energy */
} plasticity_mechanism_entry_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Per-state configuration
 *
 * WHAT: Configuration for each plasticity state
 * WHY:  Different states have different mechanism priorities
 */
typedef struct {
    bool enable_stdp;
    bool enable_bcm;
    bool enable_homeostatic;
    bool enable_eligibility;
    bool enable_dendritic;
    bool enable_stp;
    bool enable_adaptive;
    bool enable_predictive;

    /* Mechanism priorities (0-1) for conflict resolution */
    float stdp_priority;
    float bcm_priority;
    float homeostatic_priority;
    float eligibility_priority;
    float dendritic_priority;
    float stp_priority;
    float adaptive_priority;
    float predictive_priority;
} plasticity_state_config_t;

/**
 * @brief Plasticity coordinator configuration
 */
typedef struct {
    /* State configurations */
    plasticity_state_config_t state_configs[PLASTICITY_STATE_COUNT];

    /* Global settings */
    plasticity_coordinator_state_t initial_state;  /**< Starting state */
    conflict_resolution_strategy_t conflict_strategy; /**< Conflict resolution */
    uint32_t max_mechanisms;                       /**< Maximum registered mechanisms */

    /* Energy budget */
    bool enable_energy_tracking;                   /**< Track energy expenditure */
    float energy_budget_per_second;                /**< Max energy per second (ATP units) */
    float low_energy_threshold;                    /**< Threshold for low energy mode */

    /* Integration enables */
    bool enable_bio_async;                         /**< Enable bio-async integration */
    bool enable_brain_immune;                      /**< Enable brain immune integration */
    bool enable_statistics;                        /**< Track detailed statistics */
    bool enable_logging;                           /**< Enable logging */

    /* Advanced */
    bool auto_state_transitions;                   /**< Auto-transition between states */
    uint64_t consolidation_trigger_interval_ms;    /**< Trigger consolidation every N ms */
    float conflict_threshold;                      /**< Min weight change diff to count as conflict */
} plasticity_coordinator_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Per-mechanism statistics
 */
typedef struct {
    uint64_t total_updates;
    float total_energy_consumed;
    float avg_update_time_us;
    uint64_t conflicts_participated;               /**< How many conflicts this mechanism was in */
    uint64_t conflicts_won;                        /**< How many times this mechanism "won" */
} plasticity_mechanism_stats_t;

/**
 * @brief Conflict event record
 */
typedef struct {
    uint32_t synapse_id;
    plasticity_mechanism_type_t mechanism_a;
    plasticity_mechanism_type_t mechanism_b;
    float weight_change_a;
    float weight_change_b;
    float resolved_weight_change;
    conflict_resolution_strategy_t strategy_used;
    uint64_t timestamp;
} plasticity_conflict_event_t;

/**
 * @brief Coordinator-wide statistics
 */
typedef struct {
    /* Mechanism counts */
    uint32_t total_mechanisms;
    uint32_t active_mechanisms;

    /* Update performance */
    uint64_t total_update_cycles;
    uint64_t total_mechanism_updates;
    float avg_cycle_time_us;

    /* Conflicts */
    uint64_t total_conflicts;
    uint64_t conflicts_resolved;
    plasticity_conflict_event_t recent_conflicts[PLASTICITY_COORDINATOR_MAX_CONFLICTS];
    uint32_t recent_conflict_count;

    /* Energy */
    float total_energy_consumed;
    float current_energy_rate;                     /**< Energy/second */
    uint32_t low_energy_events;                    /**< Times energy budget was exceeded */

    /* State transitions */
    uint64_t state_transition_count;
    plasticity_coordinator_state_t current_state;
    uint64_t time_in_current_state_ms;

    /* Per-mechanism stats */
    plasticity_mechanism_stats_t mechanism_stats[PLASTICITY_TYPE_COUNT];
} plasticity_coordinator_stats_t;

/* ============================================================================
 * Main Coordinator Structure
 * ============================================================================ */

/**
 * @brief Plasticity coordinator state
 */
struct plasticity_coordinator {
    plasticity_coordinator_config_t config;        /**< Configuration */
    plasticity_coordinator_state_t state;          /**< Current state */

    /* Mechanism registry */
    plasticity_mechanism_entry_t* mechanisms;      /**< Registered mechanisms array */
    uint32_t mechanism_count;                      /**< Current mechanism count */
    uint32_t mechanism_capacity;                   /**< Array capacity */
    uint32_t next_mechanism_id;                    /**< Next mechanism ID */

    /* Integration handles */
    bio_module_context_t bio_context;              /**< Bio-async context */
    brain_immune_system_t* brain_immune;           /**< Brain immune system */

    /* Statistics */
    plasticity_coordinator_stats_t stats;

    /* Timing */
    uint64_t start_time;                           /**< Coordinator start time (ms) */
    uint64_t last_update_time;                     /**< Last global update (ms) */
    uint64_t last_consolidation_time;              /**< Last consolidation trigger */

    /* Energy tracking */
    float energy_consumed_this_second;             /**< Energy consumed in current second */
    uint64_t energy_tracking_start;                /**< Start of current 1-second window */

    /* Thread safety */
    nimcp_mutex_t* mutex;                          /**< Thread-safe operations */

    /* Runtime state */
    bool bio_async_connected;                      /**< Bio-async is connected */
    bool immune_connected;                         /**< Brain immune is connected */

    /* Backprop gating: when set, plasticity_coordinator_update() skips all
     * mechanism updates while nimcp_tpb_is_backprop_active() returns true.
     * This prevents STDP/BCM/homeostatic updates from racing concurrent
     * gradient-based weight updates during backprop. */
    tpb_context_t* plasticity_bridge;              /**< Backprop gate (not owned) */
    uint64_t backprop_gate_skip_count;             /**< Updates skipped due to gate */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default coordinator configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible settings
 * HOW:  Set state-specific mechanism priorities and defaults
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int plasticity_coordinator_default_config(plasticity_coordinator_config_t* config);

/**
 * @brief Create plasticity coordinator
 *
 * WHAT: Initialize coordinator infrastructure
 * WHY:  Central coordination point for all plasticity mechanisms
 * HOW:  Allocate mechanism registry, initialize statistics, register with bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return New coordinator or NULL on failure
 */
plasticity_coordinator_t* plasticity_coordinator_create(
    const plasticity_coordinator_config_t* config
);

/**
 * @brief Destroy plasticity coordinator
 *
 * WHAT: Clean up coordinator resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister all mechanisms, disconnect integrations, free memory
 *
 * NOTE: Does NOT destroy registered mechanisms (caller responsibility)
 *
 * @param coordinator Coordinator to destroy (NULL safe)
 */
void plasticity_coordinator_destroy(plasticity_coordinator_t* coordinator);

/* ============================================================================
 * Mechanism Registration API
 * ============================================================================ */

/**
 * @brief Register plasticity mechanism
 *
 * WHAT: Add mechanism to coordinator registry
 * WHY:  Enable coordinated updates and conflict resolution
 * HOW:  Store mechanism handle, update function, type
 *
 * @param coordinator Coordinator
 * @param name Mechanism name (e.g., "stdp_layer1")
 * @param type Mechanism type
 * @param handle Mechanism handle (opaque pointer)
 * @param update_fn Update callback
 * @param get_weight_change_fn Weight change query callback (NULL if not supported)
 * @param priority Mechanism priority [0-1]
 * @param energy_cost Energy cost per update
 * @param update_interval_ms Update interval (milliseconds)
 * @param mechanism_id_out Output: assigned mechanism ID
 * @return 0 on success, -1 on error
 */
int plasticity_coordinator_register_mechanism(
    plasticity_coordinator_t* coordinator,
    const char* name,
    plasticity_mechanism_type_t type,
    plasticity_mechanism_handle_t handle,
    plasticity_mechanism_update_fn_t update_fn,
    plasticity_mechanism_get_weight_change_fn_t get_weight_change_fn,
    float priority,
    float energy_cost,
    uint64_t update_interval_ms,
    uint32_t* mechanism_id_out
);

/**
 * @brief Unregister plasticity mechanism
 *
 * WHAT: Remove mechanism from coordinator
 * WHY:  Dynamic system reconfiguration
 * HOW:  Find and remove mechanism entry
 *
 * NOTE: Does NOT destroy mechanism (caller responsibility)
 *
 * @param coordinator Coordinator
 * @param mechanism_id Mechanism ID to unregister
 * @return 0 on success, -1 if not found
 */
int plasticity_coordinator_unregister_mechanism(
    plasticity_coordinator_t* coordinator,
    uint32_t mechanism_id
);

/**
 * @brief Enable/disable specific mechanism
 *
 * WHAT: Toggle mechanism update participation
 * WHY:  Selective activation without unregistering
 * HOW:  Set mechanism enabled flag
 *
 * @param coordinator Coordinator
 * @param mechanism_id Mechanism ID
 * @param enabled Enable (true) or disable (false)
 * @return 0 on success, -1 if not found
 */
int plasticity_coordinator_set_mechanism_enabled(
    plasticity_coordinator_t* coordinator,
    uint32_t mechanism_id,
    bool enabled
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main coordinator update
 *
 * WHAT: Update all mechanisms according to their intervals, resolve conflicts
 * WHY:  Core of coordinator - manages distributed plasticity
 * HOW:  Check each mechanism's interval, update due mechanisms, resolve conflicts
 *
 * NOTE: Call this regularly from main loop (e.g., every 1-10ms)
 *
 * @param coordinator Coordinator
 * @param current_time_ms Current time (milliseconds)
 * @param dt Time delta since last update (seconds)
 * @return Number of mechanisms updated, -1 on error
 */
int plasticity_coordinator_update(
    plasticity_coordinator_t* coordinator,
    uint64_t current_time_ms,
    float dt
);

/**
 * @brief Update specific mechanism
 *
 * WHAT: Update single mechanism by ID
 * WHY:  Manual override for specific mechanism
 * HOW:  Find mechanism, call update_fn
 *
 * @param coordinator Coordinator
 * @param mechanism_id Mechanism ID
 * @param dt Time delta (seconds)
 * @return 0 on success, -1 on error
 */
int plasticity_coordinator_update_mechanism(
    plasticity_coordinator_t* coordinator,
    uint32_t mechanism_id,
    float dt
);

/* ============================================================================
 * State Management API
 * ============================================================================ */

/**
 * @brief Set plasticity state
 *
 * WHAT: Transition to new plasticity state
 * WHY:  Different learning phases need different mechanisms
 * HOW:  Update state, enable/disable mechanisms per state config
 *
 * @param coordinator Coordinator
 * @param new_state New state
 * @return 0 on success, -1 on error
 */
int plasticity_coordinator_set_state(
    plasticity_coordinator_t* coordinator,
    plasticity_coordinator_state_t new_state
);

/**
 * @brief Get current plasticity state
 *
 * @param coordinator Coordinator
 * @return Current state
 */
plasticity_coordinator_state_t plasticity_coordinator_get_state(
    const plasticity_coordinator_t* coordinator
);

/**
 * @brief Trigger consolidation
 *
 * WHAT: Force transition to CONSOLIDATION state
 * WHY:  Manually trigger memory consolidation (e.g., during sleep)
 * HOW:  Set state to CONSOLIDATION, trigger eligibility→weight conversion
 *
 * @param coordinator Coordinator
 * @return 0 on success, -1 on error
 */
int plasticity_coordinator_trigger_consolidation(plasticity_coordinator_t* coordinator);

/* ============================================================================
 * Conflict Resolution API
 * ============================================================================ */

/**
 * @brief Resolve conflict between two mechanisms
 *
 * WHAT: Determine final weight change when mechanisms disagree
 * WHY:  STDP→LTP but BCM→LTD requires resolution
 * HOW:  Apply conflict resolution strategy
 *
 * @param coordinator Coordinator
 * @param synapse_id Synapse where conflict occurs
 * @param type_a First mechanism type
 * @param weight_change_a First mechanism's proposed change
 * @param type_b Second mechanism type
 * @param weight_change_b Second mechanism's proposed change
 * @param resolved_change_out Output: resolved weight change
 * @return 0 on success, -1 on error
 */
int plasticity_coordinator_resolve_conflict(
    plasticity_coordinator_t* coordinator,
    uint32_t synapse_id,
    plasticity_mechanism_type_t type_a,
    float weight_change_a,
    plasticity_mechanism_type_t type_b,
    float weight_change_b,
    float* resolved_change_out
);

/**
 * @brief Set conflict resolution strategy
 *
 * @param coordinator Coordinator
 * @param strategy New strategy
 * @return 0 on success, -1 on error
 */
int plasticity_coordinator_set_conflict_strategy(
    plasticity_coordinator_t* coordinator,
    conflict_resolution_strategy_t strategy
);

/**
 * @brief Wire the training-plasticity bridge for backprop-aware gating
 *
 * WHAT: Register a tpb_context_t* so plasticity_coordinator_update() can
 *       query nimcp_tpb_is_backprop_active() at the top of each tick and
 *       early-return when backprop is running.
 * WHY:  Biological plasticity (STDP/BCM/homeostatic) races the gradient-based
 *       weight updates performed by backprop, corrupting both. The UTM-side
 *       gate already protects UTM-owned networks; this closes the gap for
 *       mechanisms registered with the old coordinator.
 * HOW:  Store the (non-owned) context pointer on the coordinator. Passing
 *       NULL disables the gate.
 *
 * THREAD-SAFE: Yes (atomic pointer write under coordinator mutex).
 *
 * @param coordinator Coordinator
 * @param tpb         Plasticity bridge context (not owned; NULL to disable)
 * @return 0 on success, -1 on error
 */
int plasticity_coordinator_set_plasticity_bridge(
    plasticity_coordinator_t* coordinator,
    tpb_context_t* tpb
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Integrate coordinator with brain immune
 * WHY:  Enable immune modulation of plasticity
 * HOW:  Store handle, register for cytokine callbacks
 *
 * IMMUNE-PLASTICITY MODULATION:
 * - Inflammation → Reduce learning rates across all mechanisms
 * - IL-1β/IL-6 → Shift conflict resolution conservative (favor LTD)
 * - IL-10 → Restore normal plasticity
 * - IFN-γ → Emergency plasticity suppression
 *
 * @param coordinator Coordinator
 * @param immune Brain immune system
 * @return 0 on success, -1 on error
 */
int plasticity_coordinator_connect_brain_immune(
    plasticity_coordinator_t* coordinator,
    brain_immune_system_t* immune
);

/**
 * @brief Disconnect from brain immune
 *
 * @param coordinator Coordinator
 * @return 0 on success
 */
int plasticity_coordinator_disconnect_brain_immune(
    plasticity_coordinator_t* coordinator
);

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register coordinator with bio-async
 * WHY:  Enable inter-mechanism messaging and coordination
 * HOW:  Register module, set up handlers
 *
 * @param coordinator Coordinator
 * @return 0 on success, -1 on error
 */
int plasticity_coordinator_connect_bio_async(
    plasticity_coordinator_t* coordinator
);

/**
 * @brief Disconnect from bio-async
 *
 * @param coordinator Coordinator
 * @return 0 on success
 */
int plasticity_coordinator_disconnect_bio_async(
    plasticity_coordinator_t* coordinator
);

/* ============================================================================
 * Statistics and Monitoring API
 * ============================================================================ */

/**
 * @brief Get coordinator statistics
 *
 * @param coordinator Coordinator
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int plasticity_coordinator_get_stats(
    const plasticity_coordinator_t* coordinator,
    plasticity_coordinator_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param coordinator Coordinator
 */
void plasticity_coordinator_reset_stats(plasticity_coordinator_t* coordinator);

/**
 * @brief Get current energy consumption rate
 *
 * WHAT: Get energy consumed per second
 * WHY:  Monitor metabolic load
 * HOW:  Return current rate from tracking window
 *
 * @param coordinator Coordinator
 * @return Energy rate (ATP units/second)
 */
float plasticity_coordinator_get_energy_rate(
    const plasticity_coordinator_t* coordinator
);

/**
 * @brief Check if in low energy mode
 *
 * WHAT: Determine if energy budget is limiting plasticity
 * WHY:  Know when plasticity is being constrained
 * HOW:  Check if energy rate exceeds budget
 *
 * @param coordinator Coordinator
 * @return true if in low energy mode
 */
bool plasticity_coordinator_is_low_energy(
    const plasticity_coordinator_t* coordinator
);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert mechanism type to string
 *
 * @param type Mechanism type
 * @return Human-readable string
 */
const char* plasticity_mechanism_type_to_string(plasticity_mechanism_type_t type);

/**
 * @brief Convert state to string
 *
 * @param state State
 * @return Human-readable string
 */
const char* plasticity_coordinator_state_to_string(plasticity_coordinator_state_t state);

/**
 * @brief Convert conflict strategy to string
 *
 * @param strategy Strategy
 * @return Human-readable string
 */
const char* conflict_resolution_strategy_to_string(conflict_resolution_strategy_t strategy);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLASTICITY_COORDINATOR_H */
