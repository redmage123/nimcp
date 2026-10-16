/**
 * @file nimcp_working_memory_fep_bridge.h
 * @brief Free Energy Principle - Working Memory Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and working memory system
 * WHY:  Working memory maintains items via precision-weighted active inference. FEP provides
 *       theoretical grounding: attention = precision, maintenance = belief updating.
 * HOW:  FEP precision values gate working memory maintenance; working memory content influences
 *       FEP belief updates and prediction error weighting.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * WORKING MEMORY AS ACTIVE INFERENCE:
 * -----------------------------------
 * - Friston & Buzsaki (2016): Working memory = active inference over hidden states
 * - Maintenance requires precision-weighted attention
 * - Item salience = precision of belief in item relevance
 * - Capacity limits emerge from precision constraints
 * - Miller's 7±2 reflects precision allocation limits
 *
 * FEP → WORKING MEMORY PATHWAYS:
 * ------------------------------
 * 1. Precision Modulates Capacity:
 *    - High precision → More items maintainable
 *    - Low precision → Reduced effective capacity
 *    - Precision = confidence in item relevance
 *
 * 2. Prediction Error Triggers Refresh:
 *    - High PE → Item needs updating (refresh)
 *    - Low PE → Item stable (no refresh needed)
 *    - Automatic attention allocation
 *
 * 3. Expected Free Energy Guides Item Selection:
 *    - Items minimizing EFE are prioritized
 *    - Information gain drives item encoding
 *    - Goal relevance modulates salience
 *
 * WORKING MEMORY → FEP PATHWAYS:
 * -------------------------------
 * 1. Item Content Provides Context for Predictions:
 *    - Working memory = short-term context
 *    - Context modulates generative model
 *    - Items influence prediction generation
 *
 * 2. Capacity Pressure Signals Resource Constraints:
 *    - High utilization → Increase precision costs
 *    - Low utilization → Reduce precision allocation
 *    - Resource management feedback
 *
 * 3. Item Evictions Signal Belief Updates:
 *    - Eviction = belief no longer relevant
 *    - Triggers generative model adjustment
 *    - Context switching signal
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WORKING_MEMORY_FEP_BRIDGE_H
#define NIMCP_WORKING_MEMORY_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_working_memory.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Precision-capacity mapping */
#define WM_FEP_HIGH_PRECISION_CAPACITY_BOOST    2      /**< Extra items with high precision */
#define WM_FEP_LOW_PRECISION_CAPACITY_PENALTY   2      /**< Fewer items with low precision */
#define WM_FEP_PRECISION_THRESHOLD              1.0f   /**< Precision threshold for boost */

/* Prediction error refresh thresholds */
#define WM_FEP_PE_REFRESH_THRESHOLD             2.0f   /**< PE threshold to trigger refresh */
#define WM_FEP_PE_EVICTION_THRESHOLD            5.0f   /**< PE threshold to trigger eviction */

/* Capacity pressure thresholds */
#define WM_FEP_CAPACITY_WARNING_THRESHOLD       0.75f  /**< 75% utilization warning */
#define WM_FEP_CAPACITY_CRITICAL_THRESHOLD      0.90f  /**< 90% utilization critical */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct working_memory_fep_bridge working_memory_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Working Memory-FEP bridge
 */
typedef struct {
    /* FEP → Working Memory */
    float precision_capacity_scaling;        /**< How much precision affects capacity */
    float pe_refresh_threshold;              /**< PE threshold for automatic refresh */
    float efe_salience_weight;               /**< EFE contribution to salience */
    bool enable_precision_capacity_modulation; /**< Enable precision → capacity */
    bool enable_pe_auto_refresh;             /**< Enable PE → auto refresh */
    bool enable_efe_item_selection;          /**< Enable EFE-based item selection */

    /* Working Memory → FEP */
    float item_context_weight;               /**< How much items influence predictions */
    float capacity_pressure_sensitivity;     /**< Sensitivity to capacity pressure */
    bool enable_context_modulation;          /**< Enable WM → prediction context */
    bool enable_capacity_feedback;           /**< Enable capacity → precision cost */
    bool enable_eviction_signals;            /**< Enable eviction → belief update */

    /* Sensitivity factors */
    float precision_sensitivity;             /**< Precision effect scaling */
    float wm_sensitivity;                    /**< WM effect scaling */
} working_memory_fep_config_t;

/**
 * @brief FEP effects on working memory
 */
typedef struct {
    /* Precision effects */
    float current_precision;                 /**< Current FEP precision */
    int32_t capacity_adjustment;             /**< Capacity adjustment from precision */
    uint32_t effective_capacity;             /**< Effective capacity with precision */

    /* Prediction error effects */
    float current_prediction_error;          /**< Current PE magnitude */
    bool auto_refresh_triggered;             /**< Auto-refresh active */
    uint32_t items_to_refresh;               /**< Number of items needing refresh */

    /* Expected free energy effects */
    float current_efe;                       /**< Current EFE value */
    float* item_efe_scores;                  /**< EFE scores per item */
    uint32_t num_items;                      /**< Number of items tracked */
} working_memory_fep_effects_t;

/**
 * @brief Working Memory effects on FEP
 */
typedef struct {
    /* Context effects */
    float* context_vector;                   /**< WM content as context */
    uint32_t context_dim;                    /**< Context dimensionality */
    float context_strength;                  /**< Context influence strength */

    /* Capacity pressure effects */
    float capacity_utilization;              /**< Current utilization [0-1] */
    float precision_cost_multiplier;         /**< Precision cost from pressure */

    /* Eviction signals */
    bool eviction_occurred;                  /**< Eviction happened */
    uint32_t items_evicted;                  /**< Number evicted */
    float belief_update_signal;              /**< Signal strength for update */
} fep_working_memory_effects_t;

/**
 * @brief Current state of Working Memory-FEP interaction
 */
typedef struct {
    /* Current values */
    float current_precision;                 /**< Current FEP precision */
    uint32_t current_wm_size;                /**< Current WM items */
    float current_prediction_error;          /**< Current PE magnitude */

    /* Applied modifiers */
    int32_t capacity_adjustment;             /**< Applied capacity change */
    uint32_t items_refreshed;                /**< Items refreshed this cycle */
    float context_modulation;                /**< Applied context strength */

    /* State flags */
    bool capacity_warning;                   /**< Capacity pressure warning */
    bool capacity_critical;                  /**< Capacity pressure critical */
    bool auto_refresh_active;                /**< Auto-refresh active */

    /* Timestamps */
    uint64_t last_refresh_time;              /**< Last refresh timestamp */
    uint64_t last_eviction_time;             /**< Last eviction timestamp */
} working_memory_fep_state_t;

/**
 * @brief Statistics for Working Memory-FEP bridge
 */
typedef struct {
    /* FEP → Working Memory */
    uint64_t precision_capacity_adjustments; /**< Times precision adjusted capacity */
    uint64_t pe_triggered_refreshes;         /**< PE-triggered refreshes */
    uint64_t efe_item_selections;            /**< EFE-guided selections */
    float avg_precision;                     /**< Average FEP precision */
    float avg_capacity_adjustment;           /**< Average capacity change */

    /* Working Memory → FEP */
    uint64_t context_modulations;            /**< Context modulation events */
    uint64_t capacity_warnings;              /**< Capacity warnings */
    uint64_t eviction_signals;               /**< Eviction signals sent */
    float avg_wm_utilization;                /**< Average WM utilization */
    float avg_context_strength;              /**< Average context influence */

    /* Performance */
    float avg_prediction_error;              /**< Average PE magnitude */
    float avg_free_energy;                   /**< Average free energy */
} working_memory_fep_stats_t;

/**
 * @brief Working Memory-FEP bridge state
 */
struct working_memory_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    working_memory_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;                /**< FEP system */
    working_memory_t* working_memory;        /**< Working memory */

    /* Current effects */
    working_memory_fep_effects_t fep_effects; /**< FEP → WM */
    fep_working_memory_effects_t wm_effects;  /**< WM → FEP */
    working_memory_fep_state_t state;

    /* Statistics */
    working_memory_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Working Memory-FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int working_memory_fep_bridge_default_config(working_memory_fep_config_t* config);

/**
 * @brief Create Working Memory-FEP bridge
 *
 * WHAT: Initialize Working Memory-FEP integration bridge
 * WHY:  Enable bidirectional WM-FEP interaction
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
working_memory_fep_bridge_t* working_memory_fep_bridge_create(
    const working_memory_fep_config_t* config
);

/**
 * @brief Destroy Working Memory-FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void working_memory_fep_bridge_destroy(working_memory_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect FEP system
 *
 * WHAT: Link bridge to FEP system
 * WHY:  Enable FEP state monitoring and modulation
 * HOW:  Store FEP system pointer
 *
 * @param bridge Working Memory-FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int working_memory_fep_bridge_connect_fep(
    working_memory_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect working memory system
 *
 * WHAT: Link bridge to working memory system
 * WHY:  Enable WM state monitoring and modulation
 * HOW:  Store working memory pointer
 *
 * @param bridge Working Memory-FEP bridge
 * @param wm Working memory system
 * @return 0 on success
 */
int working_memory_fep_bridge_connect_working_memory(
    working_memory_fep_bridge_t* bridge,
    working_memory_t* wm
);

/**
 * @brief Disconnect all systems
 *
 * WHAT: Unlink FEP and working memory systems
 * WHY:  Safe shutdown
 * HOW:  Clear system pointers
 *
 * @param bridge Working Memory-FEP bridge
 * @return 0 on success
 */
int working_memory_fep_bridge_disconnect(working_memory_fep_bridge_t* bridge);

/* ============================================================================
 * FEP → Working Memory Direction
 * ============================================================================ */

/**
 * @brief Apply precision modulation to working memory capacity
 *
 * WHAT: Modulate WM capacity based on FEP precision
 * WHY:  High precision enables more items to be maintained
 * HOW:  Adjust effective capacity by precision value
 *
 * @param bridge Working Memory-FEP bridge
 * @return 0 on success
 */
int working_memory_fep_apply_precision_capacity_modulation(
    working_memory_fep_bridge_t* bridge
);

/**
 * @brief Trigger automatic refresh based on prediction error
 *
 * WHAT: Refresh items when prediction errors exceed threshold
 * WHY:  Maintain items when context changes (high PE)
 * HOW:  Detect high PE, refresh relevant items
 *
 * @param bridge Working Memory-FEP bridge
 * @param pe_magnitude Prediction error magnitude
 * @return 0 on success
 */
int working_memory_fep_pe_auto_refresh(
    working_memory_fep_bridge_t* bridge,
    float pe_magnitude
);

/**
 * @brief Guide item selection via expected free energy
 *
 * WHAT: Select items to encode based on EFE
 * WHY:  Items minimizing EFE are most relevant
 * HOW:  Compute EFE for candidate items, select best
 *
 * @param bridge Working Memory-FEP bridge
 * @return 0 on success
 */
int working_memory_fep_efe_item_selection(working_memory_fep_bridge_t* bridge);

/* ============================================================================
 * Working Memory → FEP Direction
 * ============================================================================ */

/**
 * @brief Apply working memory context to predictions
 *
 * WHAT: Modulate FEP predictions based on WM content
 * WHY:  Current context influences predictions
 * HOW:  Extract WM content as context vector
 *
 * @param bridge Working Memory-FEP bridge
 * @return 0 on success
 */
int working_memory_fep_apply_context_modulation(working_memory_fep_bridge_t* bridge);

/**
 * @brief Signal capacity pressure to FEP precision
 *
 * WHAT: Increase precision costs when WM is full
 * WHY:  Resource constraints should influence precision allocation
 * HOW:  Scale precision cost by capacity utilization
 *
 * @param bridge Working Memory-FEP bridge
 * @return 0 on success
 */
int working_memory_fep_signal_capacity_pressure(working_memory_fep_bridge_t* bridge);

/**
 * @brief Signal eviction events for belief updates
 *
 * WHAT: Notify FEP when items are evicted
 * WHY:  Eviction implies belief no longer relevant
 * HOW:  Send eviction signal to trigger update
 *
 * @param bridge Working Memory-FEP bridge
 * @return 0 on success
 */
int working_memory_fep_signal_eviction(working_memory_fep_bridge_t* bridge);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update Working Memory-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep WM and FEP systems synchronized
 * HOW:  Update effects, apply modulations, check thresholds
 *
 * @param bridge Working Memory-FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int working_memory_fep_bridge_update(
    working_memory_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Working Memory-FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int working_memory_fep_bridge_get_state(
    const working_memory_fep_bridge_t* bridge,
    working_memory_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Working Memory-FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int working_memory_fep_bridge_get_stats(
    const working_memory_fep_bridge_t* bridge,
    working_memory_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for WM-FEP coordination
 * WHY:  Distributed WM-precision signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Working Memory-FEP bridge
 * @return 0 on success
 */
int working_memory_fep_bridge_connect_bio_async(working_memory_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Working Memory-FEP bridge
 * @return 0 on success
 */
int working_memory_fep_bridge_disconnect_bio_async(working_memory_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Working Memory-FEP bridge
 * @return true if bio-async enabled
 */
bool working_memory_fep_bridge_is_bio_async_connected(
    const working_memory_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WORKING_MEMORY_FEP_BRIDGE_H */
