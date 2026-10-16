/**
 * @file nimcp_hypo_curiosity_fep_bridge.h
 * @brief Hypothalamus-Curiosity FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration between hypothalamus CURIOSITY drive and curiosity system
 * WHY:  CURIOSITY drive motivates exploration; information gain reduces free energy
 * HOW:  Map CURIOSITY drive to exploration weight, information gain to FE reduction
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * CURIOSITY AS DRIVE:
 * - Hypothalamic curiosity drive is an information-seeking motivation
 * - Similar to other drives: increases over time, decreases with satisfaction
 * - Dopaminergic reward from novel information (VTA projections)
 * - Reference: Gottlieb et al. (2013) "Information-seeking, curiosity, and attention"
 *
 * FEP INTEGRATION:
 * ```
 * Hypothalamus State        →  FEP Mapping           →  Curiosity Effect
 * ─────────────────────────────────────────────────────────────────────────
 * CURIOSITY drive level    →  Exploration weight     →  Information-seeking
 * Information gain         →  Free energy reduction  →  Drive satisfaction
 * Novel stimuli            →  Prediction error       →  Curiosity trigger
 * Drive satisfaction       →  Reward signal          →  Learning update
 * ```
 *
 * EPISTEMIC VALUE IN FEP:
 * - Curiosity = epistemic value (expected information gain)
 * - Exploration policies minimize expected free energy
 * - G(pi) = pragmatic value + epistemic value
 * - Reference: Friston et al. (2017) "Active inference and epistemic value"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           HYPOTHALAMUS-CURIOSITY FEP BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║   ┌──────────────────┐         ┌──────────────────┐                      ║
 * ║   │  Hypothalamus    │────────▶│  Curiosity       │                      ║
 * ║   │  • CURIOSITY     │         │  • Exploration   │                      ║
 * ║   │  • Arousal       │         │  • Info gain     │                      ║
 * ║   │  • Reward        │         │  • Learning      │                      ║
 * ║   └──────────────────┘         └──────────────────┘                      ║
 * ║           ↓                              ↓                                ║
 * ║   Hypo → Curiosity:             Curiosity → Hypo:                        ║
 * ║   • Drive → Exploration         • Info gain → Satisfaction              ║
 * ║   • Arousal → Engagement        • Learning → Drive reduction            ║
 * ║   • Reward → Reinforcement      • Novelty → Drive increase              ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_CURIOSITY_FEP_BRIDGE_H
#define NIMCP_HYPO_CURIOSITY_FEP_BRIDGE_H

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

#define HYPO_CURIOSITY_FEP_DRIVE_THRESHOLD     0.4f
#define HYPO_CURIOSITY_FEP_INFO_GAIN_SCALE     2.0f
#define HYPO_CURIOSITY_FEP_NOVELTY_PE_SCALE    1.5f
#define HYPO_CURIOSITY_FEP_EXPLORATION_WEIGHT  0.7f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for hypothalamus-curiosity FEP bridge
 */
typedef struct {
    float drive_fe_weight;           /**< Weight for drive contribution to FE */
    float prediction_error_gain;     /**< Gain for prediction error signal */
    float precision_modulation;      /**< Base precision modulation factor */
    bool enable_active_inference;    /**< Enable active inference responses */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    float curiosity_exploration_scale;/**< How curiosity drive scales exploration */
    float info_gain_fe_reduction;    /**< How info gain reduces free energy */
    float novelty_pe_scale;          /**< How novelty scales prediction error */
    float epistemic_value_weight;    /**< Weight for epistemic value in policy */
} hypo_curiosity_fep_config_t;

/**
 * @brief Effects from hypothalamus on curiosity via FEP
 */
typedef struct {
    float free_energy;               /**< Current free energy level */
    float prediction_error;          /**< Current prediction error */
    float precision;                 /**< Current precision level */
    float active_inference_strength; /**< Strength of active inference response */
    float exploration_weight;        /**< Weight for exploration in policy */
    float epistemic_value;           /**< Current epistemic value estimate */
    float info_gain_fe_reduction;    /**< FE reduction from information gain */
    float novelty_signal;            /**< Novelty detection signal */
} hypo_curiosity_fep_effects_t;

/**
 * @brief Reverse effects from curiosity to hypothalamus
 */
typedef struct {
    float info_gain_satisfaction;    /**< Drive satisfaction from info gain */
    float learning_progress;         /**< Progress in learning new info */
    float novelty_drive_boost;       /**< Drive boost from novelty */
} curiosity_hypo_effects_t;

/**
 * @brief Bridge state
 */
typedef struct {
    bool active;
    uint64_t update_count;
    float current_curiosity_drive;
    float current_info_gain;
    float current_novelty;
    float cumulative_fe_reduction;
    uint64_t exploration_triggers;
    uint64_t info_gain_events;
} hypo_curiosity_fep_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t exploration_policies;
    uint64_t info_gain_fe_reductions;
    uint64_t novelty_pe_events;
    float avg_free_energy;
    float avg_exploration_weight;
    float avg_info_gain;
    float total_fe_reduction;
} hypo_curiosity_fep_stats_t;

/**
 * @brief Hypothalamus-Curiosity FEP bridge handle
 */
typedef struct hypo_curiosity_fep_bridge {
    bridge_base_t base;                        /**< MUST be first: base bridge infrastructure */
    hypo_curiosity_fep_config_t config;
    fep_system_t* fep_system;
    hypo_drive_system_handle_t* drive_system;
    hypo_curiosity_fep_effects_t fep_effects;
    curiosity_hypo_effects_t curiosity_effects;
    hypo_curiosity_fep_state_t state;
    hypo_curiosity_fep_stats_t stats;
} hypo_curiosity_fep_bridge_t;

/* ============================================================================
 * API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int hypo_curiosity_fep_default_config(hypo_curiosity_fep_config_t* config);

/**
 * @brief Create bridge
 */
hypo_curiosity_fep_bridge_t* hypo_curiosity_fep_create(
    const hypo_curiosity_fep_config_t* config,
    fep_system_t* fep_system);

/**
 * @brief Destroy bridge
 */
void hypo_curiosity_fep_destroy(hypo_curiosity_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 */
int hypo_curiosity_fep_reset(hypo_curiosity_fep_bridge_t* bridge);

/**
 * @brief Update bridge (main processing loop)
 */
int hypo_curiosity_fep_update(hypo_curiosity_fep_bridge_t* bridge, uint64_t delta_ms);

/**
 * @brief Compute exploration weight from curiosity drive
 */
int hypo_curiosity_fep_compute_exploration(hypo_curiosity_fep_bridge_t* bridge,
    float curiosity_drive, float* exploration_weight);

/**
 * @brief Compute FE reduction from information gain
 */
int hypo_curiosity_fep_compute_fe_reduction(hypo_curiosity_fep_bridge_t* bridge,
    float info_gain, float* fe_reduction);

/**
 * @brief Modulate precision based on novelty
 */
int hypo_curiosity_fep_modulate_precision(hypo_curiosity_fep_bridge_t* bridge,
    float novelty_level, float* precision);

/**
 * @brief Get current effects
 */
int hypo_curiosity_fep_get_effects(const hypo_curiosity_fep_bridge_t* bridge,
    hypo_curiosity_fep_effects_t* effects);

/**
 * @brief Get statistics
 */
int hypo_curiosity_fep_get_stats(const hypo_curiosity_fep_bridge_t* bridge,
    hypo_curiosity_fep_stats_t* stats);

/**
 * @brief Report information gain (reduces FE, satisfies drive)
 */
int hypo_curiosity_fep_report_info_gain(hypo_curiosity_fep_bridge_t* bridge,
    float info_gain);

/**
 * @brief Report novelty detection (triggers PE, boosts drive)
 */
int hypo_curiosity_fep_report_novelty(hypo_curiosity_fep_bridge_t* bridge,
    float novelty_magnitude);

/**
 * @brief Connect to bio-async messaging
 */
int hypo_curiosity_fep_connect_bio_async(hypo_curiosity_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async messaging
 */
int hypo_curiosity_fep_disconnect_bio_async(hypo_curiosity_fep_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool hypo_curiosity_fep_is_bio_async_connected(const hypo_curiosity_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 */
int hypo_curiosity_fep_process_messages(hypo_curiosity_fep_bridge_t* bridge);

/**
 * @brief Connect to drive system
 */
int hypo_curiosity_fep_connect_drives(hypo_curiosity_fep_bridge_t* bridge,
    hypo_drive_system_handle_t* drives);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_CURIOSITY_FEP_BRIDGE_H */
