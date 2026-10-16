/**
 * @file nimcp_dendritic_sleep_bridge.h
 * @brief Sleep-Dendritic Plasticity Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and dendritic computation
 * WHY:  Sleep states fundamentally alter dendritic integration and NMDA dynamics
 * HOW:  Sleep state modulates NMDA receptor sensitivity, dendritic excitability, and Ca2+ dynamics
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → DENDRITIC PATHWAYS:
 * ----------------------------
 * 1. NMDA Receptor Modulation During Sleep:
 *    - AWAKE: Full NMDA activity (coincidence detection)
 *    - NREM: Enhanced NMDA conductance (facilitates replay)
 *    - Deep NREM: Increased Mg2+ block removal efficiency
 *    - REM: Normal NMDA but with increased dendritic excitability
 *    - Reference: Chauvette et al. (2012) "Sleep oscillations modulate NMDA"
 *
 * 2. Dendritic Spike Generation:
 *    - AWAKE: Standard threshold for dendritic spikes
 *    - LIGHT_NREM: Reduced threshold (enhanced integration)
 *    - DEEP_NREM: Further reduced threshold (consolidation-friendly)
 *    - REM: Normal threshold but increased spontaneous activity
 *    - Reference: Antic et al. (2010) "Dendritic calcium spikes"
 *
 * 3. Calcium Dynamics:
 *    - AWAKE: Standard Ca2+ influx and decay
 *    - NREM: Slower Ca2+ decay (prolonged integration window)
 *    - Deep NREM: Enhanced Ca2+ influx via NMDA (consolidation boost)
 *    - REM: Normal dynamics with increased spontaneous events
 *
 * 4. Dendritic Integration Properties:
 *    - AWAKE: Linear-to-supralinear summation
 *    - LIGHT_NREM: Enhanced supralinear integration
 *    - DEEP_NREM: Maximum supralinearity (memory consolidation)
 *    - REM: Variable integration (exploration mode)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║             SLEEP-DENDRITIC PLASTICITY INTEGRATION BRIDGE                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      NMDA    Spike Θ   Ca2+ Decay   Effect                 ║
 * ║   ────────────────────────────────────────────────────────────────────    ║
 * ║   AWAKE            1.0     1.0       1.0           Standard dendrites     ║
 * ║   DROWSY           1.1     0.95      0.9           Increased sensitivity  ║
 * ║   LIGHT_NREM       1.2     0.90      0.8           Enhanced integration   ║
 * ║   DEEP_NREM        1.4     0.85      0.7           Consolidation boost    ║
 * ║   REM              1.1     1.0       1.0           Exploration mode       ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_DENDRITIC_SLEEP_BRIDGE_H
#define NIMCP_DENDRITIC_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/dendritic/nimcp_dendritic.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Dendritic Modulation
 * ============================================================================ */

/* NMDA conductance modulation by sleep state */
#define DENDRITIC_SLEEP_NMDA_AWAKE         1.0f   /**< Standard NMDA */
#define DENDRITIC_SLEEP_NMDA_DROWSY        1.1f   /**< Slightly enhanced */
#define DENDRITIC_SLEEP_NMDA_LIGHT_NREM    1.2f   /**< Enhanced for replay */
#define DENDRITIC_SLEEP_NMDA_DEEP_NREM     1.4f   /**< Maximum enhancement */
#define DENDRITIC_SLEEP_NMDA_REM           1.1f   /**< Moderate enhancement */

/* Dendritic spike threshold modulation by sleep state */
#define DENDRITIC_SLEEP_THRESHOLD_AWAKE      1.0f   /**< Standard threshold */
#define DENDRITIC_SLEEP_THRESHOLD_DROWSY     0.95f  /**< Slightly reduced */
#define DENDRITIC_SLEEP_THRESHOLD_LIGHT_NREM 0.90f  /**< Reduced (easier spikes) */
#define DENDRITIC_SLEEP_THRESHOLD_DEEP_NREM  0.85f  /**< Minimum threshold */
#define DENDRITIC_SLEEP_THRESHOLD_REM        1.0f   /**< Standard */

/* Ca2+ decay time constant modulation */
#define DENDRITIC_SLEEP_CA_DECAY_AWAKE       1.0f   /**< Standard decay */
#define DENDRITIC_SLEEP_CA_DECAY_DROWSY      0.9f   /**< Slower decay */
#define DENDRITIC_SLEEP_CA_DECAY_LIGHT_NREM  0.8f   /**< Prolonged integration */
#define DENDRITIC_SLEEP_CA_DECAY_DEEP_NREM   0.7f   /**< Maximum prolongation */
#define DENDRITIC_SLEEP_CA_DECAY_REM         1.0f   /**< Standard */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-dendritic bridge configuration
 */
typedef struct {
    bool enable_nmda_modulation;      /**< Enable NMDA conductance changes */
    bool enable_threshold_modulation; /**< Enable spike threshold changes */
    bool enable_calcium_modulation;   /**< Enable Ca2+ dynamics changes */
    float modulation_strength;        /**< Overall modulation strength (0-1) */
} dendritic_sleep_config_t;

/**
 * @brief Computed sleep effects on dendritic computation
 */
typedef struct {
    float nmda_conductance_factor;    /**< Multiply NMDA g by this */
    float spike_threshold_factor;     /**< Multiply spike threshold by this */
    float calcium_decay_factor;       /**< Multiply Ca2+ decay tau by this */
    sleep_state_t current_state;      /**< Current sleep state */
    float sleep_pressure;             /**< Current sleep pressure */
    bool enhanced_integration;        /**< True during NREM (consolidation) */
} dendritic_sleep_effects_t;

/**
 * @brief Sleep-dendritic integration bridge
 */
typedef struct dendritic_sleep_bridge_struct* dendritic_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-dendritic bridge configuration
 * WHY:  Provide sensible defaults based on dendritic physiology
 */
int dendritic_sleep_default_config(dendritic_sleep_config_t* config);

/**
 * WHAT: Create sleep-dendritic bridge
 * WHY:  Initialize integration between sleep and dendritic systems
 */
dendritic_sleep_bridge_t dendritic_sleep_bridge_create(
    const dendritic_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-dendritic bridge
 * WHY:  Clean up resources and unregister callbacks
 */
void dendritic_sleep_bridge_destroy(dendritic_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update dendritic effects from sleep system state
 * WHY:  Compute how current sleep state affects dendritic parameters
 */
int dendritic_sleep_update(dendritic_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated dendritic parameters for current sleep state
 * WHY:  Apply sleep modulation to dendritic computation
 */
int dendritic_sleep_get_effects(const dendritic_sleep_bridge_t bridge,
                                 dendritic_sleep_effects_t* effects);

/**
 * WHAT: Get sleep-modulated NMDA conductance
 * WHY:  Apply NMDA modulation to synaptic integration
 */
float dendritic_sleep_get_nmda_conductance(const dendritic_sleep_bridge_t bridge,
                                            float base_conductance);

/**
 * WHAT: Get sleep-modulated dendritic spike threshold
 * WHY:  Apply threshold modulation to spike generation
 */
float dendritic_sleep_get_spike_threshold(const dendritic_sleep_bridge_t bridge,
                                           float base_threshold);

/**
 * WHAT: Get sleep-modulated calcium decay time constant
 * WHY:  Apply Ca2+ dynamics modulation to integration
 */
float dendritic_sleep_get_calcium_decay(const dendritic_sleep_bridge_t bridge,
                                         float base_tau);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float dendritic_sleep_get_nmda_factor(sleep_state_t state);
float dendritic_sleep_get_threshold_factor(sleep_state_t state);
float dendritic_sleep_get_calcium_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DENDRITIC_SLEEP_BRIDGE_H */
