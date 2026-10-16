/**
 * @file nimcp_hypo_global_workspace_fep_bridge.h
 * @brief Hypothalamus-Global Workspace FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration between hypothalamus drives and global workspace
 * WHY:  Drive priority influences broadcast priority; attention demand generates FE
 * HOW:  Map drive urgency to broadcast weight, attention demand to free energy
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * DRIVE-WORKSPACE INTERACTION:
 * - Hypothalamic drives modulate cortical arousal via ascending pathways
 * - High-urgency drives capture global workspace access (hunger awareness)
 * - Orexin neurons promote wakefulness and workspace availability
 * - Reference: Dehaene & Changeux (2011) "Experimental and theoretical approaches
 *   to conscious processing"
 *
 * FEP INTEGRATION:
 * ```
 * Hypothalamus State        →  FEP Mapping           →  Workspace Effect
 * ─────────────────────────────────────────────────────────────────────────
 * High drive priority      →  Broadcast priority     →  Drive-related content wins
 * Attention demand         →  Free energy increase   →  Competition for access
 * Drive satisfaction       →  FE reduction           →  Workspace availability
 * Arousal level            →  Precision modulation   →  Broadcast clarity
 * ```
 *
 * GLOBAL NEURONAL WORKSPACE THEORY:
 * - Workspace as bottleneck for conscious access
 * - Drives compete with other content for broadcast
 * - Urgency = model evidence = probability of workspace access
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           HYPOTHALAMUS-GLOBAL WORKSPACE FEP BRIDGE                        ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║   ┌──────────────────┐         ┌──────────────────┐                      ║
 * ║   │  Hypothalamus    │────────▶│  Global Workspace│                      ║
 * ║   │  • Drive urgency │         │  • Broadcast     │                      ║
 * ║   │  • Arousal       │         │  • Competition   │                      ║
 * ║   │  • Priorities    │         │  • Access        │                      ║
 * ║   └──────────────────┘         └──────────────────┘                      ║
 * ║           ↓                              ↓                                ║
 * ║   Hypo → Workspace:             Workspace → Hypo:                        ║
 * ║   • Priority → Broadcast        • Access → Drive awareness              ║
 * ║   • Arousal → Availability      • Broadcast → Drive update              ║
 * ║   • Urgency → Competition       • Content → Action selection            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_GLOBAL_WORKSPACE_FEP_BRIDGE_H
#define NIMCP_HYPO_GLOBAL_WORKSPACE_FEP_BRIDGE_H

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

#define HYPO_GW_FEP_PRIORITY_THRESHOLD       0.5f
#define HYPO_GW_FEP_ATTENTION_FE_SCALE       1.5f
#define HYPO_GW_FEP_BROADCAST_PRECISION_MIN  0.3f
#define HYPO_GW_FEP_AROUSAL_WEIGHT           0.4f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for hypothalamus-workspace FEP bridge
 */
typedef struct {
    float drive_fe_weight;           /**< Weight for drive contribution to FE */
    float prediction_error_gain;     /**< Gain for prediction error signal */
    float precision_modulation;      /**< Base precision modulation factor */
    bool enable_active_inference;    /**< Enable active inference responses */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    float priority_broadcast_scale;  /**< How priority affects broadcast weight */
    float attention_fe_scale;        /**< How attention demand scales FE */
    float arousal_availability_scale;/**< How arousal affects workspace availability */
    float urgency_competition_scale; /**< How urgency affects competition */
} hypo_gw_fep_config_t;

/**
 * @brief Effects from hypothalamus on global workspace via FEP
 */
typedef struct {
    float free_energy;               /**< Current free energy level */
    float prediction_error;          /**< Current prediction error */
    float precision;                 /**< Current precision level */
    float active_inference_strength; /**< Strength of active inference response */
    float broadcast_priority;        /**< Priority for workspace broadcast */
    float workspace_availability;    /**< Current workspace availability */
    float competition_strength;      /**< Competition strength for access */
} hypo_gw_fep_effects_t;

/**
 * @brief Reverse effects from workspace to hypothalamus
 */
typedef struct {
    float drive_awareness;           /**< Awareness of drive state */
    float action_selection_bias;     /**< Bias for drive-related actions */
    bool drive_in_broadcast;         /**< Drive content currently broadcast */
} gw_hypo_effects_t;

/**
 * @brief Bridge state
 */
typedef struct {
    bool active;
    uint64_t update_count;
    float current_priority;
    float current_arousal;
    float current_attention_demand;
    uint64_t broadcast_wins;
    uint64_t broadcast_losses;
    hypo_drive_type_t dominant_drive;
} hypo_gw_fep_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t priority_broadcasts;
    uint64_t attention_fe_spikes;
    uint64_t active_inference_triggers;
    float avg_free_energy;
    float avg_broadcast_priority;
    float avg_workspace_availability;
    float broadcast_win_rate;
} hypo_gw_fep_stats_t;

/**
 * @brief Hypothalamus-Global Workspace FEP bridge handle
 */
typedef struct hypo_gw_fep_bridge {
    bridge_base_t base;                     /**< MUST be first: base bridge infrastructure */
    hypo_gw_fep_config_t config;
    fep_system_t* fep_system;
    hypo_drive_system_handle_t* drive_system;
    hypo_gw_fep_effects_t fep_effects;
    gw_hypo_effects_t gw_effects;
    hypo_gw_fep_state_t state;
    hypo_gw_fep_stats_t stats;
} hypo_gw_fep_bridge_t;

/* ============================================================================
 * API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int hypo_gw_fep_default_config(hypo_gw_fep_config_t* config);

/**
 * @brief Create bridge
 */
hypo_gw_fep_bridge_t* hypo_gw_fep_create(
    const hypo_gw_fep_config_t* config,
    fep_system_t* fep_system);

/**
 * @brief Destroy bridge
 */
void hypo_gw_fep_destroy(hypo_gw_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 */
int hypo_gw_fep_reset(hypo_gw_fep_bridge_t* bridge);

/**
 * @brief Update bridge (main processing loop)
 */
int hypo_gw_fep_update(hypo_gw_fep_bridge_t* bridge, uint64_t delta_ms);

/**
 * @brief Compute free energy from attention demand
 */
int hypo_gw_fep_compute_fe(hypo_gw_fep_bridge_t* bridge,
    float attention_demand, float* free_energy);

/**
 * @brief Modulate precision based on arousal
 */
int hypo_gw_fep_modulate_precision(hypo_gw_fep_bridge_t* bridge,
    float arousal_level, float* precision);

/**
 * @brief Compute broadcast priority from drive urgency
 */
int hypo_gw_fep_compute_broadcast_priority(hypo_gw_fep_bridge_t* bridge,
    float drive_urgency, float* priority);

/**
 * @brief Get current effects
 */
int hypo_gw_fep_get_effects(const hypo_gw_fep_bridge_t* bridge,
    hypo_gw_fep_effects_t* effects);

/**
 * @brief Get statistics
 */
int hypo_gw_fep_get_stats(const hypo_gw_fep_bridge_t* bridge,
    hypo_gw_fep_stats_t* stats);

/**
 * @brief Report broadcast win (drive content got workspace access)
 */
int hypo_gw_fep_report_broadcast_win(hypo_gw_fep_bridge_t* bridge);

/**
 * @brief Report broadcast loss (drive content lost competition)
 */
int hypo_gw_fep_report_broadcast_loss(hypo_gw_fep_bridge_t* bridge);

/**
 * @brief Connect to bio-async messaging
 */
int hypo_gw_fep_connect_bio_async(hypo_gw_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async messaging
 */
int hypo_gw_fep_disconnect_bio_async(hypo_gw_fep_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool hypo_gw_fep_is_bio_async_connected(const hypo_gw_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 */
int hypo_gw_fep_process_messages(hypo_gw_fep_bridge_t* bridge);

/**
 * @brief Connect to drive system
 */
int hypo_gw_fep_connect_drives(hypo_gw_fep_bridge_t* bridge,
    hypo_drive_system_handle_t* drives);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_GLOBAL_WORKSPACE_FEP_BRIDGE_H */
