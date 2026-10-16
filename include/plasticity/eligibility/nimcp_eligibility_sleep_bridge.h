/**
 * @file nimcp_eligibility_sleep_bridge.h
 * @brief Sleep-Eligibility Trace Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and eligibility traces
 * WHY:  Sleep states fundamentally alter temporal credit assignment dynamics
 * HOW:  Sleep state modulates trace decay rate, learning rate, and consolidation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → ELIGIBILITY PATHWAYS:
 * ------------------------------
 * 1. Synaptic Tagging and Capture (Frey & Morris 1997):
 *    - AWAKE: Traces accumulate as "synaptic tags" during activity
 *    - SLEEP: Tags are "captured" and consolidated during replay
 *    - Deep NREM: Tags decay slower during memory consolidation
 *    - Reference: Frey & Morris (1997) "Synaptic tagging and long-term potentiation"
 *
 * 2. Trace Decay Modulation:
 *    - AWAKE: Standard decay (λ = 0.95, ~20ms half-life)
 *    - DROWSY: Slower decay (pre-consolidation preparation)
 *    - LIGHT_NREM: Very slow decay (trace preservation for replay)
 *    - DEEP_NREM: Minimal decay (active consolidation window)
 *    - REM: Faster decay (cleanup after consolidation)
 *
 * 3. Dopamine-Sleep Interaction:
 *    - Dopamine tags traces during waking for consolidation
 *    - Sleep replay triggers dopamine bursts to capture tagged synapses
 *    - Reference: Gomperts et al. (2015) "VTA neurons coordinate with hippocampus"
 *
 * 4. Learning Rate by Sleep State:
 *    - AWAKE: Full learning rate (online credit assignment)
 *    - DROWSY: Reduced learning (transition)
 *    - LIGHT_NREM: Low rate (sorting phase)
 *    - DEEP_NREM: Enhanced rate (consolidation boost)
 *    - REM: Moderate rate (integration and creativity)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              SLEEP-ELIGIBILITY TRACE INTEGRATION BRIDGE                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      LR      Decay λ    Effect                              ║
 * ║   ────────────────────────────────────────────────────────────────────    ║
 * ║   AWAKE            1.0     0.95       Standard credit assignment          ║
 * ║   DROWSY           0.7     0.97       Slow decay for consolidation prep   ║
 * ║   LIGHT_NREM       0.4     0.98       Preserve traces for replay          ║
 * ║   DEEP_NREM        1.3     0.99       Enhanced consolidation window       ║
 * ║   REM              0.8     0.93       Cleanup after consolidation         ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ELIGIBILITY_SLEEP_BRIDGE_H
#define NIMCP_ELIGIBILITY_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Eligibility Modulation
 * ============================================================================ */

/* Learning rate modulation by sleep state */
#define ELIGIBILITY_SLEEP_LR_AWAKE         1.0f   /**< Full online learning */
#define ELIGIBILITY_SLEEP_LR_DROWSY        0.7f   /**< Reduced during transition */
#define ELIGIBILITY_SLEEP_LR_LIGHT_NREM    0.4f   /**< Low rate for sorting */
#define ELIGIBILITY_SLEEP_LR_DEEP_NREM     1.3f   /**< Enhanced for consolidation */
#define ELIGIBILITY_SLEEP_LR_REM           0.8f   /**< Moderate for integration */

/* Trace decay (lambda) modulation by sleep state */
#define ELIGIBILITY_SLEEP_DECAY_AWAKE      0.95f  /**< Standard decay */
#define ELIGIBILITY_SLEEP_DECAY_DROWSY     0.97f  /**< Slower decay */
#define ELIGIBILITY_SLEEP_DECAY_LIGHT_NREM 0.98f  /**< Preserve traces */
#define ELIGIBILITY_SLEEP_DECAY_DEEP_NREM  0.99f  /**< Minimal decay (consolidation) */
#define ELIGIBILITY_SLEEP_DECAY_REM        0.93f  /**< Faster decay (cleanup) */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-eligibility bridge configuration
 */
typedef struct {
    bool enable_lr_modulation;      /**< Enable learning rate changes */
    bool enable_decay_modulation;   /**< Enable trace decay changes */
    bool enable_consolidation;      /**< Enable trace consolidation during sleep */
    float modulation_strength;      /**< Overall modulation strength (0-1) */
} eligibility_sleep_config_t;

/**
 * @brief Computed sleep effects on eligibility traces
 */
typedef struct {
    float learning_rate_factor;     /**< Multiply base LR by this */
    float decay_factor;             /**< Lambda modifier for trace decay */
    sleep_state_t current_state;    /**< Current sleep state */
    float sleep_pressure;           /**< Current sleep pressure */
    bool consolidation_active;      /**< True during deep NREM consolidation */
} eligibility_sleep_effects_t;

/**
 * @brief Sleep-eligibility integration bridge
 */
typedef struct eligibility_sleep_bridge_struct* eligibility_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-eligibility bridge configuration
 * WHY:  Provide sensible defaults based on synaptic tagging hypothesis
 */
int eligibility_sleep_default_config(eligibility_sleep_config_t* config);

/**
 * WHAT: Create sleep-eligibility bridge
 * WHY:  Initialize integration between sleep and eligibility trace systems
 */
eligibility_sleep_bridge_t eligibility_sleep_bridge_create(
    const eligibility_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-eligibility bridge
 * WHY:  Clean up resources and unregister callbacks
 */
void eligibility_sleep_bridge_destroy(eligibility_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update eligibility effects from sleep system state
 * WHY:  Compute how current sleep state affects eligibility trace parameters
 */
int eligibility_sleep_update(eligibility_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated eligibility parameters for current sleep state
 * WHY:  Apply sleep modulation to trace updates
 */
int eligibility_sleep_get_effects(const eligibility_sleep_bridge_t bridge,
                                   eligibility_sleep_effects_t* effects);

/**
 * WHAT: Get sleep-modulated learning rate
 * WHY:  Convenience function for eligibility update calls
 */
float eligibility_sleep_get_learning_rate(const eligibility_sleep_bridge_t bridge,
                                           float base_lr);

/**
 * WHAT: Get sleep-modulated trace decay factor
 * WHY:  Apply decay modulation to trace persistence
 */
float eligibility_sleep_get_decay_lambda(const eligibility_sleep_bridge_t bridge,
                                          float base_lambda);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float eligibility_sleep_get_lr_factor(sleep_state_t state);
float eligibility_sleep_get_decay_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ELIGIBILITY_SLEEP_BRIDGE_H */
