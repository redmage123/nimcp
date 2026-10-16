/**
 * @file nimcp_axon_plasticity_bridge.h
 * @brief Axon-Plasticity Bridge - Connects axons to plasticity and myelination
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bridge connecting axons to structural plasticity, intrinsic excitability, and myelination
 * WHY:  Axons are isolated from plasticity - conduction properties cannot adapt
 * HOW:  Routes activity signals and metabolic state between axons and plasticity
 *
 * BIOLOGICAL BASIS:
 * - Axonal plasticity modulates conduction velocity and reliability
 * - Activity-dependent myelination adapts axon properties
 * - Axonal excitability changes with sustained activity (intrinsic plasticity)
 * - Metabolic state affects action potential generation and propagation
 * - Axon initial segment plasticity regulates neuronal output
 *
 * INTEGRATION POINTS:
 * - nimcp_axon.h: Axon properties and conduction
 * - nimcp_plasticity_orchestrator.h: Central plasticity coordination
 * - nimcp_oligodendrocytes.h: Myelination state
 * - nimcp_myelin_sheath.h: Myelin properties
 *
 * DESIGN PATTERNS:
 * - Bridge: Decouples axon from plasticity implementation
 * - Observer: Notifies myelination system of activity changes
 * - Adapter: Translates axon events to plasticity signals
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_AXON_PLASTICITY_BRIDGE_H
#define NIMCP_AXON_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "core/axon/nimcp_axon.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define AXON_PLASTICITY_MAX_SEGMENTS      64    /**< Max axon segments */
#define AXON_PLASTICITY_MODULE_NAME       "axon_plasticity_bridge"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct axon_plasticity_bridge axon_plasticity_bridge_t;

/* Forward declarations for connected systems */
typedef struct structural_plasticity_state structural_state_t;
typedef struct intrinsic_excitability_state intrinsic_excitability_state_t;
typedef struct metabolic_state metabolic_state_t;
typedef struct nimcp_myelin_sheath nimcp_myelin_sheath_t;
typedef struct oligodendrocyte oligodendrocyte_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Axon plasticity event types
 */
typedef enum {
    AXON_EVENT_SPIKE_GENERATED = 0,  /**< Action potential initiated */
    AXON_EVENT_SPIKE_PROPAGATED,     /**< Action potential conducted */
    AXON_EVENT_CONDUCTION_FAILURE,   /**< Failed propagation */
    AXON_EVENT_MYELINATION_CHANGE,   /**< Myelin state changed */
    AXON_EVENT_BRANCH_GROWTH,        /**< Axon collateral growth */
    AXON_EVENT_BRANCH_RETRACTION,    /**< Axon collateral pruning */
    AXON_EVENT_AIS_SHIFT             /**< Axon initial segment plasticity */
} axon_plasticity_event_t;

/**
 * @brief Conduction state
 */
typedef enum {
    CONDUCTION_NORMAL = 0,           /**< Normal propagation */
    CONDUCTION_SLOWED,               /**< Demyelination/fatigue */
    CONDUCTION_BLOCKED,              /**< Failed propagation */
    CONDUCTION_ENHANCED              /**< Increased myelination */
} axon_conduction_state_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Axon-plasticity bridge configuration
 */
typedef struct {
    /* Conduction parameters */
    float base_conduction_velocity;     /**< Unmyelinated velocity (m/s) */
    float max_conduction_velocity;      /**< Max myelinated velocity (m/s) */
    float conduction_fatigue_rate;      /**< Fatigue per spike (0-1) */
    float conduction_recovery_tau_ms;   /**< Recovery time constant */

    /* Myelination interaction */
    float activity_myelination_gain;    /**< Activity → myelination rate */
    float myelination_velocity_gain;    /**< Myelination → velocity gain */
    bool enable_adaptive_myelination;   /**< Enable activity-dependent myelin */

    /* Intrinsic plasticity */
    float excitability_adaptation_rate; /**< How fast excitability adapts */
    float excitability_min;             /**< Min excitability (0-1) */
    float excitability_max;             /**< Max excitability (0-1) */
    bool enable_intrinsic_plasticity;   /**< Enable excitability adaptation */

    /* Structural plasticity */
    float branch_growth_threshold;      /**< Activity for branch growth */
    float branch_prune_threshold;       /**< Inactivity for pruning */
    bool enable_structural_plasticity;  /**< Enable axon morphology changes */

    /* Bio-async */
    bool enable_bio_async;
    uint32_t inbox_capacity;
} axon_plasticity_config_t;

/**
 * @brief Per-segment state
 */
typedef struct {
    uint32_t segment_id;                /**< Segment identifier */
    float myelination_level;            /**< Current myelination (0-1) */
    float conduction_velocity;          /**< Current velocity (m/s) */
    float excitability;                 /**< Intrinsic excitability (0-1) */
    float fatigue_level;                /**< Conduction fatigue (0-1) */
    float activity_score;               /**< Recent activity level */
    uint64_t last_spike_time;           /**< Last spike through segment */
    axon_conduction_state_t conduction_state;
} axon_segment_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t spikes_generated;
    uint64_t spikes_propagated;
    uint64_t conduction_failures;
    uint64_t myelination_events;
    uint64_t structural_events;
    float avg_conduction_velocity;
    float avg_myelination;
    float avg_excitability;
    float total_activity;
} axon_plasticity_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Axon-plasticity bridge state
 */
struct axon_plasticity_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    axon_plasticity_config_t config;

    /* Core connections */
    axon_t* axon;                     /**< Connected axon */
    plasticity_orchestrator_t* plasticity_orch;
    nimcp_myelin_sheath_t* myelin;          /**< Myelin sheath state */

    /* Plasticity connections */
    structural_state_t* structural;
    intrinsic_excitability_state_t* intrinsic;
    metabolic_state_t* metabolic;

    /* Per-segment state */
    axon_segment_state_t* segments;
    size_t num_segments;
    size_t segment_capacity;

    /* Aggregate state */
    float avg_conduction_velocity;
    float avg_myelination_factor;
    float total_activity;

    /* Statistics */
    axon_plasticity_stats_t stats;

    /* State */
    bool initialized;
    uint64_t last_update_time;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide biologically realistic defaults
 * WHY:  Easy initialization with plausible parameters
 * HOW:  Return struct based on experimental data
 *
 * @param config Output configuration
 * @return 0 on success
 */
int axon_plasticity_default_config(axon_plasticity_config_t* config);

/**
 * @brief Create axon-plasticity bridge
 *
 * WHAT: Initialize bridge connecting axon to plasticity
 * WHY:  Enable axonal plasticity and myelination adaptation
 * HOW:  Allocate structures, set up segment tracking
 *
 * @param config Configuration (NULL for defaults)
 * @param axon Axon to connect
 * @param orch Plasticity orchestrator
 * @return New bridge or NULL on failure
 */
axon_plasticity_bridge_t* axon_plasticity_create(
    const axon_plasticity_config_t* config,
    axon_t* axon,
    plasticity_orchestrator_t* orch);

/**
 * @brief Destroy axon-plasticity bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free allocations, disconnect from bio-async
 *
 * @param bridge Bridge to destroy
 */
void axon_plasticity_destroy(axon_plasticity_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int axon_plasticity_connect_structural(axon_plasticity_bridge_t* bridge, structural_state_t* s);
int axon_plasticity_connect_intrinsic(axon_plasticity_bridge_t* bridge, intrinsic_excitability_state_t* i);
int axon_plasticity_connect_metabolic(axon_plasticity_bridge_t* bridge, metabolic_state_t* m);
int axon_plasticity_connect_myelin(axon_plasticity_bridge_t* bridge, nimcp_myelin_sheath_t* myelin);
int axon_plasticity_connect_bio_async(axon_plasticity_bridge_t* bridge);
int axon_plasticity_disconnect_bio_async(axon_plasticity_bridge_t* bridge);
bool axon_plasticity_is_bio_async_connected(const axon_plasticity_bridge_t* bridge);

/* ============================================================================
 * Conduction API
 * ============================================================================ */

/**
 * @brief Update conduction velocity based on myelination
 *
 * WHAT: Compute current conduction velocity
 * WHY:  Myelination and fatigue affect propagation speed
 * HOW:  Apply myelination factor and fatigue to base velocity
 *
 * @param bridge Axon-plasticity bridge
 * @return 0 on success
 */
int axon_plasticity_update_conduction(axon_plasticity_bridge_t* bridge);

/**
 * @brief Get conduction velocity
 *
 * @param bridge Axon-plasticity bridge
 * @return Current conduction velocity (m/s)
 */
float axon_plasticity_get_conduction_velocity(axon_plasticity_bridge_t* bridge);

/**
 * @brief Process spike event
 *
 * WHAT: Handle action potential generation/propagation
 * WHY:  Update activity tracking and fatigue
 * HOW:  Record spike, update traces, check for failures
 *
 * @param bridge Axon-plasticity bridge
 * @param segment_id Segment where spike occurred
 * @param spike_time Spike timestamp
 * @return 0 if successful propagation, -1 if blocked
 */
int axon_plasticity_on_spike(
    axon_plasticity_bridge_t* bridge,
    uint32_t segment_id,
    uint64_t spike_time);

/* ============================================================================
 * Myelination API
 * ============================================================================ */

/**
 * @brief Update myelination based on activity
 *
 * WHAT: Adapt myelination to activity patterns
 * WHY:  Activity-dependent myelination is biological
 * HOW:  Increase myelin on active segments, decrease on inactive
 *
 * @param bridge Axon-plasticity bridge
 * @return 0 on success
 */
int axon_plasticity_update_myelination(axon_plasticity_bridge_t* bridge);

/**
 * @brief Get myelination level for segment
 *
 * @param bridge Axon-plasticity bridge
 * @param segment_id Segment to query
 * @return Myelination level (0-1)
 */
float axon_plasticity_get_myelination(
    const axon_plasticity_bridge_t* bridge,
    uint32_t segment_id);

/* ============================================================================
 * Structural Plasticity API
 * ============================================================================ */

/**
 * @brief Apply structural plasticity
 *
 * WHAT: Modify axon morphology based on activity
 * WHY:  Axons grow collaterals to active targets
 * HOW:  Check activity thresholds, trigger growth/pruning
 *
 * @param bridge Axon-plasticity bridge
 * @return 0 on success
 */
int axon_plasticity_apply_structural(axon_plasticity_bridge_t* bridge);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Full update cycle
 *
 * WHAT: Update all axon plasticity mechanisms
 * WHY:  Periodic maintenance of plasticity state
 * HOW:  Decay fatigue, update conduction, check myelination
 *
 * @param bridge Axon-plasticity bridge
 * @param dt_ms Time step in milliseconds
 * @return 0 on success
 */
int axon_plasticity_update(axon_plasticity_bridge_t* bridge, float dt_ms);

/**
 * @brief Get bridge statistics
 */
int axon_plasticity_get_stats(
    const axon_plasticity_bridge_t* bridge,
    axon_plasticity_stats_t* stats);

/**
 * @brief Reset statistics
 */
void axon_plasticity_reset_stats(axon_plasticity_bridge_t* bridge);

/* ============================================================================
 * String Conversion
 * ============================================================================ */

const char* axon_plasticity_event_to_string(axon_plasticity_event_t event);
const char* axon_conduction_state_to_string(axon_conduction_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AXON_PLASTICITY_BRIDGE_H */
