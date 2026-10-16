/**
 * @file nimcp_neuromod_wm_bridge.h
 * @brief Neuromodulatory-Working Memory Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridges neuromodulatory nuclei (LC, VTA, Raphe) to working memory systems
 * WHY:  Working memory capacity and flexibility are fundamentally modulated by DA/NE
 * HOW:  DA from VTA controls WM gain/stability, NE from LC controls flexibility
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * VTA (DOPAMINE) - WORKING MEMORY AXIS:
 * The PFC dopamine system is critical for working memory:
 * - D1 receptors: Enhance WM maintenance, increase stability
 * - D2 receptors: Facilitate WM updating, enable flexibility
 * - Inverted-U: Both too little and too much DA impairs WM
 * - Gating: DA controls what enters/exits WM buffers
 *
 * LOCUS COERULEUS (NE) - COGNITIVE FLEXIBILITY:
 * NE modulates the explore/exploit balance in WM:
 * - Tonic NE: High = exploration, task-switching; Low = exploitation, focus
 * - Phasic NE: Triggers WM reset/update on salient events
 * - Gain modulation: NE enhances signal-to-noise in PFC
 *
 * RAPHE (5-HT) - DELAY TOLERANCE:
 * Serotonin enables delayed responses requiring WM:
 * - Patience: 5-HT prevents premature WM release
 * - Impulse control: Maintains WM despite distractors
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Neuromodulatory -> Working Memory):
 * - DA levels -> WM gain control (inverted-U)
 * - D1/D2 balance -> stability/flexibility tradeoff
 * - NE levels -> cognitive flexibility
 * - 5-HT levels -> delay tolerance
 *
 * Top-Down (Working Memory -> Neuromodulatory):
 * - WM load -> DA demand signal to VTA
 * - Task-switch requirement -> LC phasic trigger
 * - WM overflow -> stress signal (increases NE)
 * - Successful maintenance -> reward prediction (DA)
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |             NEUROMODULATORY-WORKING MEMORY INTER-LAYER BRIDGE            |
 * +===========================================================================+
 * |                                                                           |
 * |   NEUROMODULATORY LAYER              WORKING MEMORY LAYER                 |
 * |   +-------------------+              +-------------------+                |
 * |   | VTA (DA)          |------------->| WM Gain Control   |                |
 * |   | - D1 pathway      |              | - Maintenance     |                |
 * |   | - D2 pathway      |              | - Updating        |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | LC (NE)           |------------->| Flexibility Ctrl  |                |
 * |   | - Tonic level     |              | - Task-switching  |                |
 * |   | - Phasic bursts   |              | - Reset signals   |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | Raphe (5-HT)      |------------->| Delay Buffer      |                |
 * |   | - Patience        |              | - Impulse control |                |
 * |   | - Delay tolerance |              | - Hold duration   |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |                        TOP-DOWN FEEDBACK                                  |
 * |   +-------------------+              +-------------------+                |
 * |   | VTA Demand        |<-------------| WM Load Signal    |                |
 * |   | LC Trigger        |<-------------| Task-Switch Req   |                |
 * |   | Stress Signal     |<-------------| WM Overflow       |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_WM_BRIDGE_H
#define NIMCP_NEUROMOD_WM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NEUROMOD_WM_BRIDGE_MAGIC        0x4E574D42  /* "NWMB" */

/* Bridge message types */
#define NEURO_WM_MSG_GAIN_CONTROL       0x0060
#define NEURO_WM_MSG_D1_STABILITY       0x0061
#define NEURO_WM_MSG_D2_FLEXIBILITY     0x0062
#define NEURO_WM_MSG_NE_FLEXIBILITY     0x0063
#define NEURO_WM_MSG_HT_DELAY           0x0064
#define NEURO_WM_MSG_LOAD_SIGNAL        0x0065
#define NEURO_WM_MSG_SWITCH_TRIGGER     0x0066
#define NEURO_WM_MSG_OVERFLOW           0x0067

/* Biological constants */
#define DA_WM_GAIN_BASELINE             1.0f    /* Baseline WM gain */
#define DA_WM_GAIN_MAX                  2.5f    /* Maximum gain boost */
#define DA_OPTIMAL_LEVEL                0.5f    /* Optimal DA for WM (inverted-U peak) */
#define NE_FLEXIBILITY_COUPLING         0.6f    /* NE-to-flexibility coupling */
#define HT_DELAY_COUPLING               0.5f    /* 5-HT-to-delay coupling */
#define D1_D2_BALANCE_BASELINE          0.5f    /* Baseline D1/D2 balance */

/* ============================================================================
 * Types
 * ============================================================================ */

typedef struct neuromod_wm_bridge_struct neuromod_wm_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* DA-WM coupling */
    float da_wm_gain_coupling;           /**< DA-to-WM-gain coupling strength [0-1] */
    float da_optimal_level;              /**< Optimal DA level for inverted-U [0-1] */
    float d1_stability_weight;           /**< D1 pathway stability weight [0-1] */
    float d2_flexibility_weight;         /**< D2 pathway flexibility weight [0-1] */

    /* NE-Flexibility coupling */
    float ne_flexibility_coupling;       /**< NE-to-flexibility coupling [0-1] */
    float ne_reset_threshold;            /**< NE level triggering WM reset [0-1] */
    float ne_tonic_baseline;             /**< Baseline tonic NE level [0-1] */

    /* 5-HT-Delay coupling */
    float ht_delay_coupling;             /**< 5-HT-to-delay-tolerance coupling [0-1] */
    float ht_impulse_suppression;        /**< 5-HT impulse control strength [0-1] */

    /* Top-down feedback */
    float load_da_demand_gain;           /**< WM load-to-DA demand coupling [0-1] */
    float switch_lc_trigger_gain;        /**< Task-switch-to-LC coupling [0-1] */
    float overflow_stress_gain;          /**< WM overflow-to-stress coupling [0-1] */

    /* Timing */
    uint32_t update_interval_ms;         /**< Update interval (default: 10ms) */

    /* Features */
    bool enable_inverted_u;              /**< Enable DA inverted-U relationship */
    bool enable_d1_d2_balance;           /**< Enable D1/D2 receptor balance */
    bool enable_flexibility_modulation;  /**< Enable NE flexibility effects */
    bool enable_delay_tolerance;         /**< Enable 5-HT delay effects */
    bool enable_logging;                 /**< Enable event logging */
} neuromod_wm_config_t;

/**
 * @brief Current bridge state
 */
typedef struct {
    /* WM modulation state */
    float wm_gain;                       /**< Current WM gain multiplier */
    float stability_level;               /**< D1-mediated stability [0-1] */
    float flexibility_level;             /**< D2/NE-mediated flexibility [0-1] */
    float delay_tolerance;               /**< 5-HT-mediated delay tolerance [0-1] */
    float d1_d2_balance;                 /**< Current D1/D2 balance [0=D2, 1=D1] */

    /* Neuromodulator levels (cached) */
    float da_level;                      /**< Current DA level [0-1] */
    float ne_level;                      /**< Current NE level [0-1] */
    float ht_level;                      /**< Current 5-HT level [0-1] */

    /* Top-down signals */
    float wm_load;                       /**< Current WM load [0-1] */
    float switch_demand;                 /**< Task-switch demand [0-1] */
    float overflow_signal;               /**< WM overflow signal [0-1] */

    /* Metrics */
    float bridge_coherence;              /**< Bottom-up/top-down coherence [0-1] */
    uint64_t last_update_us;             /**< Last update timestamp */
} neuromod_wm_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Bottom-up events */
    uint32_t gain_modulations;           /**< Gain modulation events */
    uint32_t stability_adjustments;      /**< Stability adjustment events */
    uint32_t flexibility_adjustments;    /**< Flexibility adjustment events */
    uint32_t delay_modulations;          /**< Delay modulation events */
    uint32_t wm_resets;                  /**< WM reset events triggered by NE */

    /* Top-down events */
    uint32_t load_signals;               /**< WM load signal events */
    uint32_t switch_triggers;            /**< Task-switch trigger events */
    uint32_t overflow_events;            /**< WM overflow events */

    /* Aggregates */
    float avg_wm_gain;                   /**< Average WM gain */
    float avg_flexibility;               /**< Average flexibility level */
    float avg_coherence;                 /**< Average bridge coherence */

    uint64_t total_updates;              /**< Total update cycles */
    uint64_t bottom_up_messages;         /**< Total bottom-up messages */
    uint64_t top_down_messages;          /**< Total top-down messages */
} neuromod_wm_stats_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* Lifecycle */
neuromod_wm_config_t neuromod_wm_default_config(void);
neuromod_wm_bridge_t* neuromod_wm_create(const neuromod_wm_config_t* config);
void neuromod_wm_destroy(neuromod_wm_bridge_t* bridge);

/* Bottom-up modulation (neuromod -> WM) */
int neuromod_wm_apply_da_gain(neuromod_wm_bridge_t* bridge, float da_level, float* gain_out);
int neuromod_wm_apply_d1_stability(neuromod_wm_bridge_t* bridge, float d1_activation, float* stability_out);
int neuromod_wm_apply_d2_flexibility(neuromod_wm_bridge_t* bridge, float d2_activation, float* flexibility_out);
int neuromod_wm_apply_ne_flexibility(neuromod_wm_bridge_t* bridge, float ne_level, float* flexibility_out);
int neuromod_wm_apply_ne_reset(neuromod_wm_bridge_t* bridge, float ne_burst, bool* reset_triggered);
int neuromod_wm_apply_ht_delay(neuromod_wm_bridge_t* bridge, float ht_level, float* delay_out);

/* Top-down feedback (WM -> neuromod) */
int neuromod_wm_report_load(neuromod_wm_bridge_t* bridge, float wm_load, float* da_demand_out);
int neuromod_wm_report_switch_need(neuromod_wm_bridge_t* bridge, float switch_urgency, float* lc_trigger_out);
int neuromod_wm_report_overflow(neuromod_wm_bridge_t* bridge, float overflow_level, float* stress_out);

/* Unified modulation */
int neuromod_wm_compute_modulation(neuromod_wm_bridge_t* bridge,
                                   float da_level, float ne_level, float ht_level,
                                   neuromod_wm_state_t* state_out);

/* Update and state */
int neuromod_wm_update(neuromod_wm_bridge_t* bridge, float delta_ms);
int neuromod_wm_get_state(const neuromod_wm_bridge_t* bridge, neuromod_wm_state_t* state_out);
int neuromod_wm_get_stats(const neuromod_wm_bridge_t* bridge, neuromod_wm_stats_t* stats_out);
int neuromod_wm_reset_stats(neuromod_wm_bridge_t* bridge);

/* Diagnostics */
bool neuromod_wm_is_connected(const neuromod_wm_bridge_t* bridge);
float neuromod_wm_get_coherence(const neuromod_wm_bridge_t* bridge);
void neuromod_wm_print_summary(const neuromod_wm_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMOD_WM_BRIDGE_H */
