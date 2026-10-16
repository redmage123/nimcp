/**
 * @file nimcp_stp_sleep_bridge.h
 * @brief Sleep-STP (Short-Term Plasticity) Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and short-term plasticity
 * WHY:  Sleep states fundamentally alter synaptic vesicle dynamics and facilitation
 * HOW:  Sleep state modulates depression/facilitation time constants and release probability
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → STP PATHWAYS:
 * ----------------------
 * 1. Vesicle Recovery During Sleep:
 *    - AWAKE: Standard vesicle recovery (τ_D ~ 200ms)
 *    - DROWSY: Slower recovery (reduced metabolic activity)
 *    - LIGHT_NREM: Enhanced recovery (synaptic resource restoration)
 *    - DEEP_NREM: Maximum recovery rate (homeostatic restoration)
 *    - REM: Standard recovery with increased spontaneous release
 *    - Reference: Vyazovskiy et al. (2008) "Molecular and electrophysiological sleep homeostasis"
 *
 * 2. Facilitation Dynamics:
 *    - AWAKE: Standard facilitation decay (τ_F ~ 50ms)
 *    - NREM: Slower facilitation decay (prolonged integration)
 *    - Deep NREM: Minimal facilitation (stable release probability)
 *    - REM: Enhanced facilitation (increased spontaneous activity)
 *
 * 3. Release Probability (U):
 *    - AWAKE: Standard release probability
 *    - DROWSY: Slightly reduced (energy conservation)
 *    - LIGHT_NREM: Reduced (selective release)
 *    - DEEP_NREM: Minimal release (conservation mode)
 *    - REM: Enhanced release (replay and consolidation)
 *    - Reference: Huber et al. (2013) "Sleep homeostasis and cortical synchronization"
 *
 * 4. Depression Recovery:
 *    - AWAKE: Standard recovery from depression
 *    - NREM: Accelerated recovery (synaptic homeostasis)
 *    - Deep NREM: Maximum recovery (prepare for waking)
 *    - Reference: Tononi & Cirelli (2014) "Sleep and synaptic homeostasis"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  SLEEP-STP INTEGRATION BRIDGE                              ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      U       τ_D Recov  τ_F Decay  Effect                  ║
 * ║   ────────────────────────────────────────────────────────────────────    ║
 * ║   AWAKE            1.0     1.0        1.0         Standard STP            ║
 * ║   DROWSY           0.9     0.8        1.2         Reduced release         ║
 * ║   LIGHT_NREM       0.7     1.3        1.5         Enhanced recovery       ║
 * ║   DEEP_NREM        0.5     1.6        2.0         Maximum restoration     ║
 * ║   REM              1.2     1.0        0.8         Enhanced release        ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_STP_SLEEP_BRIDGE_H
#define NIMCP_STP_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/stp/nimcp_stp.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State STP Modulation
 * ============================================================================ */

/* Release probability (U) modulation by sleep state */
#define STP_SLEEP_U_AWAKE         1.0f   /**< Standard release */
#define STP_SLEEP_U_DROWSY        0.9f   /**< Reduced release */
#define STP_SLEEP_U_LIGHT_NREM    0.7f   /**< Low release (conservation) */
#define STP_SLEEP_U_DEEP_NREM     0.5f   /**< Minimal release */
#define STP_SLEEP_U_REM           1.2f   /**< Enhanced release (replay) */

/* Depression recovery time constant modulation */
#define STP_SLEEP_TAU_D_AWAKE      1.0f   /**< Standard recovery */
#define STP_SLEEP_TAU_D_DROWSY     0.8f   /**< Slower recovery */
#define STP_SLEEP_TAU_D_LIGHT_NREM 1.3f   /**< Enhanced recovery */
#define STP_SLEEP_TAU_D_DEEP_NREM  1.6f   /**< Maximum recovery */
#define STP_SLEEP_TAU_D_REM        1.0f   /**< Standard recovery */

/* Facilitation decay time constant modulation */
#define STP_SLEEP_TAU_F_AWAKE      1.0f   /**< Standard decay */
#define STP_SLEEP_TAU_F_DROWSY     1.2f   /**< Slower decay */
#define STP_SLEEP_TAU_F_LIGHT_NREM 1.5f   /**< Prolonged facilitation */
#define STP_SLEEP_TAU_F_DEEP_NREM  2.0f   /**< Maximum prolongation */
#define STP_SLEEP_TAU_F_REM        0.8f   /**< Faster decay */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-STP bridge configuration
 */
typedef struct {
    bool enable_u_modulation;       /**< Enable release probability changes */
    bool enable_tau_d_modulation;   /**< Enable depression recovery changes */
    bool enable_tau_f_modulation;   /**< Enable facilitation decay changes */
    float modulation_strength;      /**< Overall modulation strength (0-1) */
} stp_sleep_config_t;

/**
 * @brief Computed sleep effects on STP
 */
typedef struct {
    float release_probability_factor;   /**< Multiply U by this */
    float depression_recovery_factor;   /**< Multiply τ_D by this (higher = faster recovery) */
    float facilitation_decay_factor;    /**< Multiply τ_F by this (higher = slower decay) */
    sleep_state_t current_state;        /**< Current sleep state */
    float sleep_pressure;               /**< Current sleep pressure */
    bool vesicle_restoration_active;    /**< True during deep NREM */
} stp_sleep_effects_t;

/**
 * @brief Sleep-STP integration bridge
 */
typedef struct stp_sleep_bridge_struct* stp_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-STP bridge configuration
 * WHY:  Provide sensible defaults based on vesicle dynamics
 */
int stp_sleep_default_config(stp_sleep_config_t* config);

/**
 * WHAT: Create sleep-STP bridge
 * WHY:  Initialize integration between sleep and STP systems
 */
stp_sleep_bridge_t stp_sleep_bridge_create(
    const stp_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-STP bridge
 * WHY:  Clean up resources and unregister callbacks
 */
void stp_sleep_bridge_destroy(stp_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update STP effects from sleep system state
 * WHY:  Compute how current sleep state affects STP parameters
 */
int stp_sleep_update(stp_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated STP parameters for current sleep state
 * WHY:  Apply sleep modulation to vesicle dynamics
 */
int stp_sleep_get_effects(const stp_sleep_bridge_t bridge,
                           stp_sleep_effects_t* effects);

/**
 * WHAT: Get sleep-modulated release probability
 * WHY:  Apply U modulation to synaptic transmission
 */
float stp_sleep_get_release_probability(const stp_sleep_bridge_t bridge,
                                         float base_u);

/**
 * WHAT: Get sleep-modulated depression recovery time constant
 * WHY:  Apply τ_D modulation to vesicle recovery
 */
float stp_sleep_get_tau_depression(const stp_sleep_bridge_t bridge,
                                    float base_tau_d);

/**
 * WHAT: Get sleep-modulated facilitation decay time constant
 * WHY:  Apply τ_F modulation to calcium dynamics
 */
float stp_sleep_get_tau_facilitation(const stp_sleep_bridge_t bridge,
                                      float base_tau_f);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float stp_sleep_get_u_factor(sleep_state_t state);
float stp_sleep_get_tau_d_factor(sleep_state_t state);
float stp_sleep_get_tau_f_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STP_SLEEP_BRIDGE_H */
