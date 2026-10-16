/**
 * @file nimcp_hypo_introspection_fep_bridge.h
 * @brief Hypothalamus-Introspection FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration between hypothalamus interoception and introspection
 * WHY:  Interoception accuracy affects prediction error; body awareness generates FE
 * HOW:  Map interoceptive accuracy to PE, body awareness to free energy
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * INTEROCEPTION-INTROSPECTION INTERACTION:
 * - Hypothalamus receives interoceptive signals from body (visceral afferents)
 * - Insula integrates these with higher-order body representation
 * - Interoceptive accuracy correlates with metacognitive ability
 * - Reference: Critchley & Garfinkel (2017) "Interoception and emotion"
 *
 * FEP INTEGRATION:
 * ```
 * Hypothalamus State        →  FEP Mapping           →  Introspection Effect
 * ─────────────────────────────────────────────────────────────────────────
 * Interoception accuracy   →  Prediction error       →  Self-model accuracy
 * Body awareness level     →  Free energy            →  Metacognitive load
 * Physiological deviation  →  Surprise               →  Introspective focus
 * Homeostatic error        →  PE magnitude           →  Self-monitoring intensity
 * ```
 *
 * PREDICTIVE INTEROCEPTION:
 * - Body states predicted by generative model
 * - Interoceptive surprise = deviation from expected body state
 * - Allostatic inference: predict AND control body states
 * - Reference: Seth & Friston (2016) "Active interoceptive inference and the
 *   emotional brain"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           HYPOTHALAMUS-INTROSPECTION FEP BRIDGE                           ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║   ┌──────────────────┐         ┌──────────────────┐                      ║
 * ║   │  Hypothalamus    │────────▶│  Introspection   │                      ║
 * ║   │  • Interoception │         │  • Self-model    │                      ║
 * ║   │  • Body state    │         │  • Metacognition │                      ║
 * ║   │  • Homeostasis   │         │  • Awareness     │                      ║
 * ║   └──────────────────┘         └──────────────────┘                      ║
 * ║           ↓                              ↓                                ║
 * ║   Hypo → Introspection:         Introspection → Hypo:                    ║
 * ║   • Accuracy → PE               • Self-model → Predictions              ║
 * ║   • Awareness → FE              • Meta-insight → Regulation             ║
 * ║   • Deviation → Surprise        • Monitoring → Control                   ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_INTROSPECTION_FEP_BRIDGE_H
#define NIMCP_HYPO_INTROSPECTION_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_homeostasis.h"
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

#define HYPO_INTRO_FEP_ACCURACY_THRESHOLD    0.7f
#define HYPO_INTRO_FEP_AWARENESS_SCALE       1.2f
#define HYPO_INTRO_FEP_DEVIATION_PE_SCALE    2.5f
#define HYPO_INTRO_FEP_HOMEOSTATIC_WEIGHT    0.6f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for hypothalamus-introspection FEP bridge
 */
typedef struct {
    float drive_fe_weight;           /**< Weight for drive contribution to FE */
    float prediction_error_gain;     /**< Gain for prediction error signal */
    float precision_modulation;      /**< Base precision modulation factor */
    bool enable_active_inference;    /**< Enable active inference responses */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    float interoception_pe_scale;    /**< How interoception accuracy scales PE */
    float body_awareness_fe_scale;   /**< How body awareness scales FE */
    float deviation_surprise_scale;  /**< How deviation scales surprise */
    float homeostatic_error_scale;   /**< How homeostatic error scales PE */
} hypo_intro_fep_config_t;

/**
 * @brief Effects from hypothalamus on introspection via FEP
 */
typedef struct {
    float free_energy;               /**< Current free energy level */
    float prediction_error;          /**< Current prediction error */
    float precision;                 /**< Current precision level */
    float active_inference_strength; /**< Strength of active inference response */
    float interoceptive_pe;          /**< PE from interoception accuracy */
    float body_awareness_fe;         /**< FE from body awareness level */
    float surprise;                  /**< Surprise from physiological deviation */
    float metacognitive_load;        /**< Metacognitive processing load */
} hypo_intro_fep_effects_t;

/**
 * @brief Reverse effects from introspection to hypothalamus
 */
typedef struct {
    float self_model_predictions;    /**< Predictions from self-model */
    float regulation_signal;         /**< Meta-insight driven regulation */
    float monitoring_intensity;      /**< Self-monitoring intensity */
} intro_hypo_effects_t;

/**
 * @brief Bridge state
 */
typedef struct {
    bool active;
    uint64_t update_count;
    float current_interoception_accuracy;
    float current_body_awareness;
    float current_homeostatic_error;
    float current_deviation;
    uint64_t surprise_events;
} hypo_intro_fep_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t interoception_pe_events;
    uint64_t awareness_fe_spikes;
    uint64_t active_inference_triggers;
    float avg_free_energy;
    float avg_prediction_error;
    float avg_interoception_accuracy;
    float avg_body_awareness;
} hypo_intro_fep_stats_t;

/**
 * @brief Hypothalamus-Introspection FEP bridge handle
 */
typedef struct hypo_intro_fep_bridge {
    bridge_base_t base;                       /**< MUST be first: base bridge infrastructure */
    hypo_intro_fep_config_t config;
    fep_system_t* fep_system;
    hypo_drive_system_handle_t* drive_system;
    hypo_intro_fep_effects_t fep_effects;
    intro_hypo_effects_t intro_effects;
    hypo_intro_fep_state_t state;
    hypo_intro_fep_stats_t stats;
} hypo_intro_fep_bridge_t;

/* ============================================================================
 * API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int hypo_intro_fep_default_config(hypo_intro_fep_config_t* config);

/**
 * @brief Create bridge
 */
hypo_intro_fep_bridge_t* hypo_intro_fep_create(
    const hypo_intro_fep_config_t* config,
    fep_system_t* fep_system);

/**
 * @brief Destroy bridge
 */
void hypo_intro_fep_destroy(hypo_intro_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 */
int hypo_intro_fep_reset(hypo_intro_fep_bridge_t* bridge);

/**
 * @brief Update bridge (main processing loop)
 */
int hypo_intro_fep_update(hypo_intro_fep_bridge_t* bridge, uint64_t delta_ms);

/**
 * @brief Compute free energy from body awareness
 */
int hypo_intro_fep_compute_fe(hypo_intro_fep_bridge_t* bridge,
    float body_awareness, float* free_energy);

/**
 * @brief Compute prediction error from interoception accuracy
 */
int hypo_intro_fep_compute_pe(hypo_intro_fep_bridge_t* bridge,
    float interoception_accuracy, float* prediction_error);

/**
 * @brief Modulate precision based on homeostatic state
 */
int hypo_intro_fep_modulate_precision(hypo_intro_fep_bridge_t* bridge,
    float homeostatic_error, float* precision);

/**
 * @brief Get current effects
 */
int hypo_intro_fep_get_effects(const hypo_intro_fep_bridge_t* bridge,
    hypo_intro_fep_effects_t* effects);

/**
 * @brief Get statistics
 */
int hypo_intro_fep_get_stats(const hypo_intro_fep_bridge_t* bridge,
    hypo_intro_fep_stats_t* stats);

/**
 * @brief Report interoceptive deviation (generates surprise)
 */
int hypo_intro_fep_report_deviation(hypo_intro_fep_bridge_t* bridge,
    float deviation_magnitude);

/**
 * @brief Update interoception accuracy
 */
int hypo_intro_fep_update_interoception(hypo_intro_fep_bridge_t* bridge,
    float accuracy);

/**
 * @brief Connect to bio-async messaging
 */
int hypo_intro_fep_connect_bio_async(hypo_intro_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async messaging
 */
int hypo_intro_fep_disconnect_bio_async(hypo_intro_fep_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool hypo_intro_fep_is_bio_async_connected(const hypo_intro_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 */
int hypo_intro_fep_process_messages(hypo_intro_fep_bridge_t* bridge);

/**
 * @brief Connect to drive system
 */
int hypo_intro_fep_connect_drives(hypo_intro_fep_bridge_t* bridge,
    hypo_drive_system_handle_t* drives);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_INTROSPECTION_FEP_BRIDGE_H */
