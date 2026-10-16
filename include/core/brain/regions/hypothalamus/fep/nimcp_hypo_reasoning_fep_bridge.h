/**
 * @file nimcp_hypo_reasoning_fep_bridge.h
 * @brief Hypothalamus-Reasoning FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration between hypothalamus drives and reasoning system
 * WHY:  Fatigue drives affect reasoning precision; cognitive load generates free energy
 * HOW:  Map fatigue to precision reduction, cognitive load to free energy, errors to PE
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FATIGUE-REASONING INTERACTION:
 * - Hypothalamic fatigue signals (orexin/hypocretin) modulate prefrontal activity
 * - Fatigue reduces synaptic precision, increasing inference errors
 * - Sleep pressure (adenosine accumulation) degrades reasoning performance
 * - Reference: Lim & Dinges (2010) "A meta-analysis of the impact of sleep
 *   deprivation on cognitive variables"
 *
 * FEP INTEGRATION:
 * ```
 * Hypothalamus State        →  FEP Mapping           →  Reasoning Effect
 * ─────────────────────────────────────────────────────────────────────────
 * High FATIGUE drive       →  Precision reduction   →  Reduced inference confidence
 * High cognitive load      →  Increased free energy →  More errors, less coherence
 * Reasoning errors         →  Prediction error      →  Surprise signals
 * Drive satisfaction       →  Free energy reduction →  Improved performance
 * ```
 *
 * ALLOSTATIC BALANCE:
 * - Optimal reasoning requires homeostatic balance
 * - Fatigue drive pushes for rest to restore precision
 * - Active inference: take actions to reduce cognitive load
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           HYPOTHALAMUS-REASONING FEP BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║   ┌──────────────────┐         ┌──────────────────┐                      ║
 * ║   │  Hypothalamus    │────────▶│  Reasoning       │                      ║
 * ║   │  • Fatigue drive │         │  • Inference     │                      ║
 * ║   │  • Arousal       │         │  • Abduction     │                      ║
 * ║   │  • Energy state  │         │  • Deduction     │                      ║
 * ║   └──────────────────┘         └──────────────────┘                      ║
 * ║           ↓                              ↓                                ║
 * ║   Hypo → Reasoning:             Reasoning → Hypo:                        ║
 * ║   • Fatigue → Low precision     • Load → Fatigue increase               ║
 * ║   • Arousal → Inference speed   • Errors → Drive urgency                ║
 * ║   • Rest → Restored precision   • Success → Drive satisfaction          ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_REASONING_FEP_BRIDGE_H
#define NIMCP_HYPO_REASONING_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define HYPO_REASONING_FEP_FATIGUE_THRESHOLD     0.6f
#define HYPO_REASONING_FEP_LOAD_THRESHOLD        0.7f
#define HYPO_REASONING_FEP_PRECISION_MIN         0.2f
#define HYPO_REASONING_FEP_ERROR_PE_SCALE        2.0f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for hypothalamus-reasoning FEP bridge
 */
typedef struct {
    float drive_fe_weight;           /**< Weight for drive contribution to FE */
    float prediction_error_gain;     /**< Gain for prediction error signal */
    float precision_modulation;      /**< Base precision modulation factor */
    bool enable_active_inference;    /**< Enable active inference responses */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    float fatigue_precision_scale;   /**< How fatigue scales precision [0,1] */
    float load_fe_scale;             /**< How load scales free energy */
    float error_pe_scale;            /**< How errors scale prediction error */
    float arousal_inference_scale;   /**< How arousal affects inference speed */
} hypo_reasoning_fep_config_t;

/**
 * @brief Effects from hypothalamus on reasoning via FEP
 */
typedef struct {
    float free_energy;               /**< Current free energy level */
    float prediction_error;          /**< Current prediction error */
    float precision;                 /**< Current precision level */
    float active_inference_strength; /**< Strength of active inference response */
    float fatigue_effect;            /**< Effect of fatigue on reasoning */
    float cognitive_load_fe;         /**< Free energy from cognitive load */
    float error_pe;                  /**< Prediction error from reasoning errors */
} hypo_reasoning_fep_effects_t;

/**
 * @brief Reverse effects from reasoning to hypothalamus
 */
typedef struct {
    float load_induced_fatigue;      /**< Fatigue increase from load */
    float error_urgency;             /**< Drive urgency from errors */
    float success_satisfaction;      /**< Drive satisfaction from success */
} reasoning_hypo_effects_t;

/**
 * @brief Bridge state
 */
typedef struct {
    bool active;
    uint64_t update_count;
    float current_fatigue;
    float current_load;
    float current_precision;
    uint64_t reasoning_errors;
    uint64_t reasoning_successes;
} hypo_reasoning_fep_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t precision_reductions;
    uint64_t fe_spikes;
    uint64_t active_inference_triggers;
    float avg_free_energy;
    float avg_precision;
    float avg_prediction_error;
    float max_cognitive_load;
} hypo_reasoning_fep_stats_t;

/**
 * @brief Hypothalamus-Reasoning FEP bridge handle
 */
typedef struct hypo_reasoning_fep_bridge {
    bridge_base_t base;                       /**< MUST be first: base bridge infrastructure */
    hypo_reasoning_fep_config_t config;
    fep_system_t* fep_system;
    hypo_drive_system_handle_t* drive_system;
    hypo_reasoning_fep_effects_t fep_effects;
    reasoning_hypo_effects_t reasoning_effects;
    hypo_reasoning_fep_state_t state;
    hypo_reasoning_fep_stats_t stats;
} hypo_reasoning_fep_bridge_t;

/* ============================================================================
 * API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int hypo_reasoning_fep_default_config(hypo_reasoning_fep_config_t* config);

/**
 * @brief Create bridge
 */
hypo_reasoning_fep_bridge_t* hypo_reasoning_fep_create(
    const hypo_reasoning_fep_config_t* config,
    fep_system_t* fep_system);

/**
 * @brief Destroy bridge
 */
void hypo_reasoning_fep_destroy(hypo_reasoning_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 */
int hypo_reasoning_fep_reset(hypo_reasoning_fep_bridge_t* bridge);

/**
 * @brief Update bridge (main processing loop)
 */
int hypo_reasoning_fep_update(hypo_reasoning_fep_bridge_t* bridge, uint64_t delta_ms);

/**
 * @brief Compute free energy from cognitive load
 */
int hypo_reasoning_fep_compute_fe(hypo_reasoning_fep_bridge_t* bridge,
    float cognitive_load, float* free_energy);

/**
 * @brief Modulate precision based on fatigue
 */
int hypo_reasoning_fep_modulate_precision(hypo_reasoning_fep_bridge_t* bridge,
    float fatigue_level, float* precision);

/**
 * @brief Get current effects
 */
int hypo_reasoning_fep_get_effects(const hypo_reasoning_fep_bridge_t* bridge,
    hypo_reasoning_fep_effects_t* effects);

/**
 * @brief Get statistics
 */
int hypo_reasoning_fep_get_stats(const hypo_reasoning_fep_bridge_t* bridge,
    hypo_reasoning_fep_stats_t* stats);

/**
 * @brief Report reasoning error for PE computation
 */
int hypo_reasoning_fep_report_error(hypo_reasoning_fep_bridge_t* bridge,
    float error_magnitude);

/**
 * @brief Report reasoning success
 */
int hypo_reasoning_fep_report_success(hypo_reasoning_fep_bridge_t* bridge);

/**
 * @brief Connect to bio-async messaging
 */
int hypo_reasoning_fep_connect_bio_async(hypo_reasoning_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async messaging
 */
int hypo_reasoning_fep_disconnect_bio_async(hypo_reasoning_fep_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool hypo_reasoning_fep_is_bio_async_connected(const hypo_reasoning_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 */
int hypo_reasoning_fep_process_messages(hypo_reasoning_fep_bridge_t* bridge);

/**
 * @brief Connect to drive system
 */
int hypo_reasoning_fep_connect_drives(hypo_reasoning_fep_bridge_t* bridge,
    hypo_drive_system_handle_t* drives);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_REASONING_FEP_BRIDGE_H */
