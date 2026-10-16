/**
 * @file nimcp_circular_buffer_fep_bridge.h
 * @brief Free Energy Principle - Circular Buffer Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and temporal buffering
 * WHY:  Buffering represents prediction horizon management in FEP; buffer state
 *       indicates temporal precision and capacity for future planning
 * HOW:  FEP precision → buffer size/attention; buffer utilization → FEP uncertainty;
 *       prediction horizon determines buffer depth
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FEP → BUFFERING PATHWAYS:
 * --------------------------
 * 1. Prediction Horizon as Buffer Depth:
 *    - FEP planning horizon determines how far ahead to buffer
 *    - Longer horizon → larger buffer capacity needed
 *    - Active inference requires temporal context
 *    - Reference: Friston (2017) "Active inference and temporal depth"
 *
 * 2. Precision-Weighted Buffer Attention:
 *    - High FEP precision → attend to recent buffer contents
 *    - Low precision → rely on older buffered states
 *    - Precision determines buffer "focus window"
 *    - Reference: Feldman & Friston (2010) "Attention and precision"
 *
 * 3. Expected Sequences Prime Buffer:
 *    - FEP predictions set expected buffer fill patterns
 *    - Prediction errors trigger buffer capacity adjustment
 *    - Adaptive buffering based on temporal demands
 *
 * BUFFERING → FEP PATHWAYS:
 * --------------------------
 * 1. Buffer Utilization as Capacity Constraint:
 *    - High utilization → limited temporal horizon
 *    - Overflow events → prediction horizon too ambitious
 *    - Feeds back as FEP uncertainty increase
 *    - Reference: Working memory capacity limits in active inference
 *
 * 2. Buffer Overflows as Surprise:
 *    - Overflow = failed to maintain temporal context
 *    - Indicates prediction horizon exceeded capacity
 *    - Triggers free energy increase and belief updates
 *
 * 3. Buffer Patterns as Temporal Observations:
 *    - Buffer fill rate → observation of temporal dynamics
 *    - Buffer synchrony → coordinated temporal structure
 *    - Replay from buffer → offline consolidation
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  FEP-CIRCULAR BUFFER BRIDGE                                ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              FEP → BUFFERING PATHWAYS                               │  ║
 * ║   │                                                                     │  ║
 * ║   │   Prediction Horizon → Buffer Capacity                             │  ║
 * ║   │   Precision → Buffer Attention Window                              │  ║
 * ║   │   Expected Sequences → Buffer Priming                              │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │            BUFFERING → FEP PATHWAYS                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   Buffer Utilization → FEP Capacity Constraint                     │  ║
 * ║   │   Overflows → Surprise / Prediction Error                          │  ║
 * ║   │   Buffer Patterns → Temporal Observations                          │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CIRCULAR_BUFFER_FEP_BRIDGE_H
#define NIMCP_CIRCULAR_BUFFER_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "middleware/buffering/nimcp_circular_buffer.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FEP_BUFFER_MIN_HORIZON             4      /**< Minimum buffer horizon */
#define FEP_BUFFER_MAX_HORIZON             256    /**< Maximum buffer horizon */
#define FEP_BUFFER_HIGH_UTILIZATION        0.8f   /**< High utilization threshold */
#define FEP_BUFFER_OVERFLOW_SURPRISE       5.0f   /**< Surprise from overflow */
#define FEP_BUFFER_PRECISION_WINDOW_BASE   10     /**< Base window size */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct circular_buffer_fep_bridge circular_buffer_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for circular buffer-FEP bridge
 */
typedef struct {
    /* Feature enables */
    bool enable_horizon_adjustment;      /**< Horizon → buffer capacity */
    bool enable_precision_windowing;     /**< Precision → attention window */
    bool enable_overflow_surprise;       /**< Overflows → FEP surprise */
    bool enable_utilization_feedback;    /**< Utilization → uncertainty */

    /* Sensitivity factors */
    float horizon_sensitivity;           /**< Horizon adjustment sensitivity */
    float precision_sensitivity;         /**< Precision windowing sensitivity */
    float overflow_sensitivity;          /**< Overflow impact scaling */
    float utilization_sensitivity;       /**< Utilization feedback scaling */
} circular_buffer_fep_config_t;

/**
 * @brief FEP effects on circular buffer
 */
typedef struct {
    /* Buffer configuration */
    uint32_t target_capacity;            /**< Target buffer capacity */
    uint32_t attention_window;           /**< Precision-based window size */
    size_t horizon_depth;                /**< Prediction horizon depth */

    /* Buffer modulation */
    float overflow_tolerance;            /**< Tolerance for overflows */
    bool primed_for_sequence;            /**< Buffer primed for expected seq */
} circular_buffer_fep_effects_t;

/**
 * @brief Current state of buffer-FEP interaction
 */
typedef struct {
    /* FEP state */
    float current_precision;             /**< Current FEP precision */
    uint32_t prediction_horizon;         /**< FEP prediction horizon */

    /* Buffer state */
    float buffer_utilization;            /**< Current utilization [0-1] */
    uint32_t overflow_count;             /**< Recent overflow events */
    uint32_t buffer_size;                /**< Current buffer size */

    /* Derived state */
    float capacity_constraint;           /**< Constraint from buffer limits */
    float temporal_surprise;             /**< Surprise from buffer events */
} circular_buffer_fep_state_t;

/**
 * @brief Statistics for buffer-FEP bridge
 */
typedef struct {
    /* Buffer adaptation stats */
    uint64_t horizon_adjustments;        /**< Horizon adjustment count */
    uint64_t capacity_changes;           /**< Capacity change count */
    float avg_utilization;               /**< Average utilization */

    /* Overflow stats */
    uint64_t total_overflows;            /**< Total overflow events */
    uint64_t surprise_events;            /**< Surprise generation count */
    float avg_overflow_surprise;         /**< Average overflow surprise */

    /* Windowing stats */
    uint64_t window_adjustments;         /**< Window size changes */
    float avg_attention_window;          /**< Average attention window */
} circular_buffer_fep_stats_t;

/**
 * @brief Circular buffer-FEP bridge state
 */
struct circular_buffer_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    circular_buffer_fep_config_t config;

    /* Connected systems */
    circular_buffer_t* buffer;           /**< Circular buffer */
    fep_system_t* fep_system;            /**< FEP system */

    /* Current effects and state */
    circular_buffer_fep_effects_t effects;
    circular_buffer_fep_state_t state;

    /* Statistics */
    circular_buffer_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default circular buffer-FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int circular_buffer_fep_bridge_default_config(
    circular_buffer_fep_config_t* config
);

/**
 * @brief Create circular buffer-FEP bridge
 *
 * WHAT: Initialize buffer-FEP integration bridge
 * WHY:  Enable bidirectional buffer-FEP interaction
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
circular_buffer_fep_bridge_t* circular_buffer_fep_bridge_create(
    const circular_buffer_fep_config_t* config
);

/**
 * @brief Destroy circular buffer-FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void circular_buffer_fep_bridge_destroy(
    circular_buffer_fep_bridge_t* bridge
);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect circular buffer
 *
 * WHAT: Link bridge to circular buffer
 * WHY:  Enable buffer monitoring and control
 * HOW:  Store buffer pointer
 *
 * @param bridge Buffer-FEP bridge
 * @param buffer Circular buffer
 * @return 0 on success
 */
int circular_buffer_fep_bridge_connect_buffer(
    circular_buffer_fep_bridge_t* bridge,
    circular_buffer_t* buffer
);

/**
 * @brief Connect FEP system
 *
 * WHAT: Link bridge to FEP system
 * WHY:  Enable FEP state monitoring and feedback
 * HOW:  Store FEP system pointer
 *
 * @param bridge Buffer-FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int circular_buffer_fep_bridge_connect_fep(
    circular_buffer_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Disconnect all systems
 *
 * WHAT: Unlink buffer and FEP systems
 * WHY:  Safe shutdown
 * HOW:  Clear system pointers
 *
 * @param bridge Buffer-FEP bridge
 * @return 0 on success
 */
int circular_buffer_fep_bridge_disconnect(
    circular_buffer_fep_bridge_t* bridge
);

/* ============================================================================
 * FEP → Buffer Direction
 * ============================================================================ */

/**
 * @brief Adjust buffer capacity based on prediction horizon
 *
 * WHAT: Set buffer capacity from FEP planning horizon
 * WHY:  Ensure sufficient temporal depth for active inference
 * HOW:  Map horizon to buffer capacity
 *
 * @param bridge Buffer-FEP bridge
 * @param horizon Prediction horizon (timesteps)
 * @return 0 on success
 */
int circular_buffer_fep_adjust_horizon(
    circular_buffer_fep_bridge_t* bridge,
    uint32_t horizon
);

/**
 * @brief Set attention window from precision
 *
 * WHAT: Adjust buffer attention window based on FEP precision
 * WHY:  High precision focuses on recent history
 * HOW:  Map precision to window size
 *
 * @param bridge Buffer-FEP bridge
 * @param precision Current FEP precision [0-1]
 * @return 0 on success
 */
int circular_buffer_fep_set_precision_window(
    circular_buffer_fep_bridge_t* bridge,
    float precision
);

/**
 * @brief Prime buffer for expected sequence
 *
 * WHAT: Configure buffer for predicted temporal pattern
 * WHY:  Prepare buffer for expected fill pattern
 * HOW:  Set overflow strategy and priming flag
 *
 * @param bridge Buffer-FEP bridge
 * @param expected_fill_rate Expected buffer fill rate
 * @return 0 on success
 */
int circular_buffer_fep_prime_sequence(
    circular_buffer_fep_bridge_t* bridge,
    float expected_fill_rate
);

/* ============================================================================
 * Buffer → FEP Direction
 * ============================================================================ */

/**
 * @brief Report buffer utilization to FEP
 *
 * WHAT: Convert buffer utilization to FEP capacity constraint
 * WHY:  High utilization indicates limited temporal capacity
 * HOW:  Map utilization to uncertainty increase
 *
 * @param bridge Buffer-FEP bridge
 * @param utilization Current buffer utilization [0-1]
 * @return 0 on success
 */
int circular_buffer_fep_report_utilization(
    circular_buffer_fep_bridge_t* bridge,
    float utilization
);

/**
 * @brief Report buffer overflow as surprise
 *
 * WHAT: Convert buffer overflow to FEP surprise signal
 * WHY:  Overflow indicates failed temporal prediction
 * HOW:  Generate surprise proportional to overflow severity
 *
 * @param bridge Buffer-FEP bridge
 * @param overflow_count Number of recent overflows
 * @return 0 on success
 */
int circular_buffer_fep_report_overflow(
    circular_buffer_fep_bridge_t* bridge,
    uint32_t overflow_count
);

/**
 * @brief Report buffer patterns as temporal observations
 *
 * WHAT: Convert buffer state to FEP temporal observations
 * WHY:  Buffer patterns indicate temporal structure
 * HOW:  Extract fill patterns and report to FEP
 *
 * @param bridge Buffer-FEP bridge
 * @return 0 on success
 */
int circular_buffer_fep_report_patterns(
    circular_buffer_fep_bridge_t* bridge
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update buffer-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep buffer and FEP synchronized
 * HOW:  Update horizon, precision windowing, and observations
 *
 * @param bridge Buffer-FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int circular_buffer_fep_bridge_update(
    circular_buffer_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Buffer-FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int circular_buffer_fep_bridge_get_state(
    const circular_buffer_fep_bridge_t* bridge,
    circular_buffer_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Buffer-FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int circular_buffer_fep_bridge_get_stats(
    const circular_buffer_fep_bridge_t* bridge,
    circular_buffer_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for buffer-FEP coordination
 * WHY:  Distributed buffer state signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Buffer-FEP bridge
 * @return 0 on success
 */
int circular_buffer_fep_bridge_connect_bio_async(
    circular_buffer_fep_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Buffer-FEP bridge
 * @return 0 on success
 */
int circular_buffer_fep_bridge_disconnect_bio_async(
    circular_buffer_fep_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Buffer-FEP bridge
 * @return true if bio-async enabled
 */
bool circular_buffer_fep_bridge_is_bio_async_connected(
    const circular_buffer_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CIRCULAR_BUFFER_FEP_BRIDGE_H */
