/**
 * @file nimcp_heterosynaptic_sleep_bridge.h
 * @brief Sleep-Heterosynaptic Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between sleep/wake system and heterosynaptic plasticity
 * WHY:  Sleep states modulate synaptic competition and heterosynaptic depression
 * HOW:  Sleep state adjusts competition radius, depression strength, and winner-take-all dynamics
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → HETEROSYNAPTIC PATHWAYS:
 * -------------------------------
 * 1. Synaptic Homeostasis During Sleep (Tononi & Cirelli, 2014):
 *    - AWAKE: Full competition, strong heterosynaptic depression
 *    - NREM: Reduced competition for consolidation
 *    - REM: Moderate competition for selective strengthening
 *    - Sleep downscales competition to preserve important synapses
 *
 * 2. Competition Modulation by Sleep State:
 *    - AWAKE: 100% competition strength, normal radius
 *    - DROWSY: 80% competition strength
 *    - LIGHT_NREM: 50% competition (wider tolerance for co-activation)
 *    - DEEP_NREM: 30% competition (minimal suppression)
 *    - REM: 70% competition (selective consolidation)
 *
 * 3. Winner-Take-All Suppression:
 *    - Awake: Strong WTA (sparse coding)
 *    - Sleep: Relaxed WTA (allows clustered activation)
 *    - Supports memory consolidation without excessive pruning
 *
 * 4. Spatial Radius Modulation:
 *    - Awake: Standard competition radius
 *    - NREM: Reduced radius (local competition only)
 *    - Prevents global suppression during consolidation
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              SLEEP-HETEROSYNAPTIC INTEGRATION BRIDGE                       ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE    Competition  Depression   Radius    Effect               ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE          100%         100%         15μm      Full competition     ║
 * ║   DROWSY         80%          80%          15μm      Reduced suppression  ║
 * ║   LIGHT_NREM     50%          50%          10μm      Consolidation mode   ║
 * ║   DEEP_NREM      30%          30%          8μm       Minimal competition  ║
 * ║   REM            70%          70%          12μm      Selective pruning    ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HETEROSYNAPTIC_SLEEP_BRIDGE_H
#define NIMCP_HETEROSYNAPTIC_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/heterosynaptic/nimcp_heterosynaptic.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Heterosynaptic Modulation
 * ============================================================================ */

/* Competition strength modulation by sleep state */
#define HETERO_SLEEP_COMPETITION_AWAKE         1.0f   /**< Full competition */
#define HETERO_SLEEP_COMPETITION_DROWSY        0.8f   /**< Slight reduction */
#define HETERO_SLEEP_COMPETITION_LIGHT_NREM    0.5f   /**< Consolidation mode */
#define HETERO_SLEEP_COMPETITION_DEEP_NREM     0.3f   /**< Minimal competition */
#define HETERO_SLEEP_COMPETITION_REM           0.7f   /**< Selective pruning */

/* Depression factor modulation */
#define HETERO_SLEEP_DEPRESSION_AWAKE          1.0f
#define HETERO_SLEEP_DEPRESSION_DROWSY         0.8f
#define HETERO_SLEEP_DEPRESSION_LIGHT_NREM     0.5f
#define HETERO_SLEEP_DEPRESSION_DEEP_NREM      0.3f
#define HETERO_SLEEP_DEPRESSION_REM            0.7f

/* Radius modulation (fraction of base radius) */
#define HETERO_SLEEP_RADIUS_AWAKE              1.0f   /**< 15μm standard */
#define HETERO_SLEEP_RADIUS_DROWSY             1.0f
#define HETERO_SLEEP_RADIUS_LIGHT_NREM         0.67f  /**< 10μm reduced */
#define HETERO_SLEEP_RADIUS_DEEP_NREM          0.53f  /**< 8μm minimal */
#define HETERO_SLEEP_RADIUS_REM                0.8f   /**< 12μm moderate */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-heterosynaptic bridge configuration
 */
typedef struct {
    bool enable_competition_modulation;  /**< Enable competition strength changes */
    bool enable_depression_modulation;   /**< Enable depression factor changes */
    bool enable_radius_modulation;       /**< Enable radius changes */
    float modulation_strength;           /**< Overall modulation strength (0-1) */
} hetero_sleep_config_t;

/**
 * @brief Computed sleep effects on heterosynaptic plasticity
 */
typedef struct {
    float competition_factor;            /**< Multiply competition strength by this */
    float depression_factor;             /**< Multiply depression factor by this */
    float radius_factor;                 /**< Multiply competition radius by this */
    sleep_state_t current_state;         /**< Current sleep state */
    float sleep_pressure;                /**< Current sleep pressure */
    bool competition_enabled;            /**< False during deep sleep */
} hetero_sleep_effects_t;

/**
 * @brief Sleep-heterosynaptic integration bridge
 */
typedef struct hetero_sleep_bridge_struct* hetero_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-heterosynaptic bridge configuration
 * WHY:  Provide sensible defaults based on synaptic homeostasis
 */
int hetero_sleep_default_config(hetero_sleep_config_t* config);

/**
 * WHAT: Create sleep-heterosynaptic bridge
 * WHY:  Initialize integration between sleep and heterosynaptic systems
 */
hetero_sleep_bridge_t hetero_sleep_bridge_create(
    const hetero_sleep_config_t* config,
    sleep_system_t sleep_system,
    hetero_system_t* hetero_system
);

/**
 * WHAT: Destroy sleep-heterosynaptic bridge
 * WHY:  Clean up resources
 */
void hetero_sleep_bridge_destroy(hetero_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update heterosynaptic effects from sleep system state
 * WHY:  Compute how current sleep state affects heterosynaptic parameters
 */
int hetero_sleep_update(hetero_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated heterosynaptic parameters for current sleep state
 * WHY:  Apply sleep modulation to competition and depression
 */
int hetero_sleep_get_effects(const hetero_sleep_bridge_t bridge, hetero_sleep_effects_t* effects);

/**
 * WHAT: Get sleep-modulated competition strength
 * WHY:  Convenience function for heterosynaptic update calls
 */
float hetero_sleep_get_competition_factor(const hetero_sleep_bridge_t bridge, float base_competition);

/**
 * WHAT: Get sleep-modulated depression factor
 * WHY:  Apply sleep modulation to heterosynaptic depression
 */
float hetero_sleep_get_depression_factor(const hetero_sleep_bridge_t bridge, float base_depression);

/**
 * WHAT: Get sleep-modulated competition radius
 * WHY:  Adjust spatial extent of competition based on sleep state
 */
float hetero_sleep_get_radius(const hetero_sleep_bridge_t bridge, float base_radius);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float hetero_sleep_get_competition_factor_for_state(sleep_state_t state);
float hetero_sleep_get_depression_factor_for_state(sleep_state_t state);
float hetero_sleep_get_radius_factor_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HETEROSYNAPTIC_SLEEP_BRIDGE_H */
