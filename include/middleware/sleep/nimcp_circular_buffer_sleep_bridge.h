/**
 * @file nimcp_circular_buffer_sleep_bridge.h
 * @brief Sleep-Circular Buffer Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and circular buffer
 * WHY:  Sleep states affect buffer capacity, retention duration, and overflow strategy
 * HOW:  Sleep state modulates effective buffer size and data retention policies
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → CIRCULAR BUFFER PATHWAYS:
 * ----------------------------------
 * 1. Working Memory Capacity Reduction (Drummond et al., 2012):
 *    - AWAKE: Full buffer capacity (optimal working memory)
 *    - DROWSY: Reduced capacity (70%) - attention lapses
 *    - NREM: Minimal buffering (30%) - reduced online processing
 *    - Deep NREM: Offline mode (10%) - consolidation focus
 *    - REM: Moderate capacity (50%) - dream processing
 *    - Reference: "Sleep deprivation impairs working memory capacity"
 *
 * 2. Temporal Buffering and Sleep:
 *    - AWAKE: Short-term buffer for immediate processing
 *    - DROWSY: Increased retention time (slower decay)
 *    - NREM: Selective retention (consolidation candidates)
 *    - Deep NREM: Minimal new buffering, replay focus
 *    - REM: Creative recombination of buffered traces
 *
 * 3. Buffer Capacity by Sleep State:
 *    - AWAKE: 100% capacity
 *    - DROWSY: 70% capacity
 *    - LIGHT_NREM: 30% capacity
 *    - DEEP_NREM: 10% capacity (consolidation mode)
 *    - REM: 50% capacity (dream processing)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                SLEEP-CIRCULAR BUFFER INTEGRATION BRIDGE                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Buffer      Retention    Overflow    Effect            ║
 * ║                    Capacity    Duration     Strategy                      ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE            1.0         1.0          OVERWRITE   Full buffering    ║
 * ║   DROWSY           0.7         1.2          OVERWRITE   Reduced capacity  ║
 * ║   LIGHT_NREM       0.3         1.5          BLOCK       Limited buffer    ║
 * ║   DEEP_NREM        0.1         2.0          BLOCK       Consolidation     ║
 * ║   REM              0.5         1.3          OVERWRITE   Dream mode        ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CIRCULAR_BUFFER_SLEEP_BRIDGE_H
#define NIMCP_CIRCULAR_BUFFER_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "middleware/buffering/nimcp_circular_buffer.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Circular Buffer Modulation
 * ============================================================================ */

/* Buffer capacity factor by sleep state */
#define CIRCULAR_BUFFER_SLEEP_CAPACITY_AWAKE         1.0f   /**< Full capacity */
#define CIRCULAR_BUFFER_SLEEP_CAPACITY_DROWSY        0.7f   /**< Reduced capacity */
#define CIRCULAR_BUFFER_SLEEP_CAPACITY_LIGHT_NREM    0.3f   /**< Limited buffer */
#define CIRCULAR_BUFFER_SLEEP_CAPACITY_DEEP_NREM     0.1f   /**< Minimal buffer */
#define CIRCULAR_BUFFER_SLEEP_CAPACITY_REM           0.5f   /**< Dream mode */

/* Retention duration factor by sleep state */
#define CIRCULAR_BUFFER_SLEEP_RETENTION_AWAKE        1.0f   /**< Normal retention */
#define CIRCULAR_BUFFER_SLEEP_RETENTION_DROWSY       1.2f   /**< Slower decay */
#define CIRCULAR_BUFFER_SLEEP_RETENTION_LIGHT_NREM   1.5f   /**< Extended retention */
#define CIRCULAR_BUFFER_SLEEP_RETENTION_DEEP_NREM    2.0f   /**< Consolidation hold */
#define CIRCULAR_BUFFER_SLEEP_RETENTION_REM          1.3f   /**< Dream retention */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-Circular Buffer bridge configuration
 */
typedef struct {
    bool enable_capacity_modulation;     /**< Enable buffer capacity changes */
    bool enable_retention_modulation;    /**< Enable retention duration changes */
    bool enable_overflow_modulation;     /**< Enable overflow strategy changes */
    float modulation_strength;           /**< Overall modulation strength (0-1) */
} circular_buffer_sleep_config_t;

/**
 * @brief Computed sleep effects on circular buffer
 */
typedef struct {
    float capacity_factor;               /**< Multiply buffer capacity by this */
    float retention_duration_factor;     /**< Multiply retention time by this */
    overflow_strategy_t overflow_strategy; /**< Sleep-dependent overflow handling */
    sleep_state_t current_state;         /**< Current sleep state */
    float sleep_pressure;                /**< Current sleep pressure */
    bool buffering_enabled;              /**< False during deep sleep offline */
} circular_buffer_sleep_effects_t;

/**
 * @brief Sleep-Circular Buffer integration bridge
 */
typedef struct circular_buffer_sleep_bridge_struct* circular_buffer_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-circular buffer bridge configuration
 * WHY:  Provide sensible defaults based on working memory capacity research
 */
int circular_buffer_sleep_default_config(circular_buffer_sleep_config_t* config);

/**
 * WHAT: Create sleep-circular buffer bridge
 * WHY:  Initialize integration between sleep and buffering systems
 */
circular_buffer_sleep_bridge_t circular_buffer_sleep_bridge_create(
    const circular_buffer_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-circular buffer bridge
 * WHY:  Clean up resources and unregister callback
 */
void circular_buffer_sleep_bridge_destroy(circular_buffer_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update circular buffer effects from sleep system state
 * WHY:  Compute how current sleep state affects buffer parameters
 */
int circular_buffer_sleep_update(circular_buffer_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated circular buffer parameters for current sleep state
 * WHY:  Apply sleep modulation to buffer operations
 */
int circular_buffer_sleep_get_effects(
    const circular_buffer_sleep_bridge_t bridge,
    circular_buffer_sleep_effects_t* effects
);

/**
 * WHAT: Get sleep-modulated buffer capacity
 * WHY:  Determine effective buffer size for current sleep state
 */
float circular_buffer_sleep_get_capacity(
    const circular_buffer_sleep_bridge_t bridge
);

/**
 * WHAT: Get sleep-modulated retention duration
 * WHY:  Determine how long to retain buffered data
 */
float circular_buffer_sleep_get_retention_duration(
    const circular_buffer_sleep_bridge_t bridge
);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float circular_buffer_sleep_get_capacity_factor(sleep_state_t state);
float circular_buffer_sleep_get_retention_factor(sleep_state_t state);
overflow_strategy_t circular_buffer_sleep_get_overflow_strategy(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CIRCULAR_BUFFER_SLEEP_BRIDGE_H */
