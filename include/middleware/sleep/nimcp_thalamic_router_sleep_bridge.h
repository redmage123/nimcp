/**
 * @file nimcp_thalamic_router_sleep_bridge.h
 * @brief Sleep-Thalamic Router Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and thalamic router
 * WHY:  Sleep states modulate thalamic gating, routing efficiency, and attention filtering
 * HOW:  Sleep state modulates routing thresholds, queue capacity, and attention gating
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → THALAMIC ROUTING PATHWAYS:
 * ------------------------------------
 * 1. Thalamic Gating Hypothesis (Sherman & Guillery, 2001):
 *    - AWAKE: Active relay mode (tonic firing) - efficient cortical communication
 *    - DROWSY: Transition to burst mode - reduced relay fidelity
 *    - NREM: Burst mode dominates - reduced thalamic transmission
 *    - Deep NREM: Thalamic silence - cortical isolation (spindles)
 *    - REM: Restored relay but altered selectivity
 *    - Reference: McCormick & Bal (1997) "Sleep and arousal"
 *
 * 2. Thalamic Reticular Nucleus (TRN) Sleep Modulation:
 *    - AWAKE: TRN provides attention-based gating (high selectivity)
 *    - NREM: TRN generates spindles (10-15Hz rhythmic inhibition)
 *    - Deep NREM: Maximal TRN inhibition of relay neurons
 *    - Sleep reduces signal routing efficiency and attention filtering
 *    - Reference: Halassa & Acsády (2016) "Thalamic inhibition"
 *
 * 3. Routing Efficiency by Sleep State:
 *    - AWAKE: Full routing capacity (100%)
 *    - DROWSY: Reduced capacity (70%) - transition
 *    - LIGHT_NREM: Low capacity (40%) - spindle interference
 *    - DEEP_NREM: Minimal capacity (10%) - near-complete gating
 *    - REM: Moderate capacity (60%) - selective filtering
 *
 * 4. Attention Gating Modulation:
 *    - AWAKE: Strong attention filtering (high min_threshold)
 *    - DROWSY: Weakened filtering (reduced threshold)
 *    - NREM: Minimal filtering (low threshold)
 *    - Deep NREM: Near-zero filtering (sleep spindles dominate)
 *    - REM: Restored but altered filtering (emotional bias)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                SLEEP-THALAMIC ROUTER INTEGRATION BRIDGE                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Routing    Queue       Attention    Effect            ║
 * ║                    Capacity   Capacity     Threshold                      ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE            1.0        1.0          0.3          Full relay        ║
 * ║   DROWSY           0.7        0.8          0.2          Reduced relay     ║
 * ║   LIGHT_NREM       0.4        0.5          0.1          Spindle mode      ║
 * ║   DEEP_NREM        0.1        0.2          0.05         Near silence      ║
 * ║   REM              0.6        0.7          0.15         Selective relay   ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_THALAMIC_ROUTER_SLEEP_BRIDGE_H
#define NIMCP_THALAMIC_ROUTER_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "middleware/routing/nimcp_thalamic_router.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Thalamic Router Modulation
 * ============================================================================ */

/* Routing efficiency factor by sleep state */
#define THALAMIC_SLEEP_ROUTING_AWAKE         1.0f   /**< Full relay capacity */
#define THALAMIC_SLEEP_ROUTING_DROWSY        0.7f   /**< Reduced relay */
#define THALAMIC_SLEEP_ROUTING_LIGHT_NREM    0.4f   /**< Spindle interference */
#define THALAMIC_SLEEP_ROUTING_DEEP_NREM     0.1f   /**< Near-complete gating */
#define THALAMIC_SLEEP_ROUTING_REM           0.6f   /**< Selective relay */

/* Queue capacity modulation by sleep state */
#define THALAMIC_SLEEP_QUEUE_AWAKE           1.0f   /**< Full queue capacity */
#define THALAMIC_SLEEP_QUEUE_DROWSY          0.8f   /**< Slightly reduced */
#define THALAMIC_SLEEP_QUEUE_LIGHT_NREM      0.5f   /**< Halved capacity */
#define THALAMIC_SLEEP_QUEUE_DEEP_NREM       0.2f   /**< Minimal buffering */
#define THALAMIC_SLEEP_QUEUE_REM             0.7f   /**< Moderate capacity */

/* Attention threshold modulation by sleep state */
#define THALAMIC_SLEEP_ATTENTION_AWAKE       0.3f   /**< High selectivity */
#define THALAMIC_SLEEP_ATTENTION_DROWSY      0.2f   /**< Reduced filtering */
#define THALAMIC_SLEEP_ATTENTION_LIGHT_NREM  0.1f   /**< Weak filtering */
#define THALAMIC_SLEEP_ATTENTION_DEEP_NREM   0.05f  /**< Minimal filtering */
#define THALAMIC_SLEEP_ATTENTION_REM         0.15f  /**< Moderate filtering */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-Thalamic Router bridge configuration
 */
typedef struct {
    bool enable_routing_modulation;      /**< Enable routing capacity changes */
    bool enable_queue_modulation;        /**< Enable queue capacity changes */
    bool enable_attention_modulation;    /**< Enable attention threshold changes */
    float modulation_strength;           /**< Overall modulation strength (0-1) */
} thalamic_router_sleep_config_t;

/**
 * @brief Computed sleep effects on thalamic router
 */
typedef struct {
    float routing_efficiency_factor;     /**< Multiply routing capacity by this */
    float queue_capacity_factor;         /**< Multiply queue size by this */
    float attention_threshold;           /**< Minimum attention for routing */
    sleep_state_t current_state;         /**< Current sleep state */
    float sleep_pressure;                /**< Current sleep pressure */
    bool routing_enabled;                /**< False during deep sleep */
} thalamic_router_sleep_effects_t;

/**
 * @brief Sleep-Thalamic Router integration bridge
 */
typedef struct thalamic_router_sleep_bridge_struct* thalamic_router_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-thalamic router bridge configuration
 * WHY:  Provide sensible defaults based on thalamic gating hypothesis
 */
int thalamic_router_sleep_default_config(thalamic_router_sleep_config_t* config);

/**
 * WHAT: Create sleep-thalamic router bridge
 * WHY:  Initialize integration between sleep and thalamic routing systems
 */
thalamic_router_sleep_bridge_t thalamic_router_sleep_bridge_create(
    const thalamic_router_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-thalamic router bridge
 * WHY:  Clean up resources and unregister callback
 */
void thalamic_router_sleep_bridge_destroy(thalamic_router_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update thalamic router effects from sleep system state
 * WHY:  Compute how current sleep state affects routing parameters
 */
int thalamic_router_sleep_update(thalamic_router_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated thalamic router parameters for current sleep state
 * WHY:  Apply sleep modulation to routing operations
 */
int thalamic_router_sleep_get_effects(
    const thalamic_router_sleep_bridge_t bridge,
    thalamic_router_sleep_effects_t* effects
);

/**
 * WHAT: Get sleep-modulated routing efficiency
 * WHY:  Determine effective routing capacity for current sleep state
 */
float thalamic_router_sleep_get_routing_efficiency(
    const thalamic_router_sleep_bridge_t bridge
);

/**
 * WHAT: Get sleep-modulated queue capacity
 * WHY:  Determine effective queue size for current sleep state
 */
float thalamic_router_sleep_get_queue_capacity(
    const thalamic_router_sleep_bridge_t bridge
);

/**
 * WHAT: Get sleep-modulated attention threshold
 * WHY:  Determine minimum attention for routing in current sleep state
 */
float thalamic_router_sleep_get_attention_threshold(
    const thalamic_router_sleep_bridge_t bridge
);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float thalamic_router_sleep_get_routing_factor(sleep_state_t state);
float thalamic_router_sleep_get_queue_factor(sleep_state_t state);
float thalamic_router_sleep_get_attention_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THALAMIC_ROUTER_SLEEP_BRIDGE_H */
