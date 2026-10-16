/**
 * @file nimcp_neuromod_attention_bridge.h
 * @brief Neuromodulatory-Attention Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridges neuromodulatory nuclei (LC, VTA, Raphe, Habenula) to attention systems
 * WHY:  Attention is fundamentally controlled by norepinephrine from Locus Coeruleus
 * HOW:  LC/NE modulates attention gain, VTA/DA modulates salience, 5-HT modulates patience
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * LOCUS COERULEUS - ATTENTION AXIS:
 * The LC-NE system is the primary controller of attention state:
 * - Phasic LC bursts: Trigger attention shifts to salient stimuli
 * - Tonic LC activity: Controls overall vigilance/arousal level
 * - Gain modulation: NE increases signal-to-noise ratio in cortex
 * - Adaptive gain theory: LC optimizes explore/exploit tradeoff
 *
 * VTA - SALIENCE MODULATION:
 * Dopamine from VTA modulates attention to rewarding stimuli:
 * - Incentive salience: DA makes reward-predictive stimuli attention-grabbing
 * - Motivational attention: DA biases attention toward goal-relevant features
 *
 * RAPHE - TEMPORAL ATTENTION:
 * Serotonin from Raphe modulates attention timing:
 * - Patience: 5-HT enables sustained attention during delays
 * - Impulse control: 5-HT prevents premature attention shifts
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Neuromodulatory -> Attention):
 * - NE levels -> attention gain modulation
 * - Phasic LC bursts -> attention shift signals
 * - DA levels -> salience boost for rewarding stimuli
 * - 5-HT levels -> sustained attention capacity
 * - Habenula activity -> attention withdrawal from aversive
 *
 * Top-Down (Attention -> Neuromodulatory):
 * - Novel stimuli detection -> LC phasic burst trigger
 * - Goal-relevant features -> VTA activation
 * - Attention fatigue -> reduced NE, increased adenosine
 * - Conflict detection -> increased NE/DA demands
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |               NEUROMODULATORY-ATTENTION INTER-LAYER BRIDGE                |
 * +===========================================================================+
 * |                                                                           |
 * |   NEUROMODULATORY LAYER              ATTENTION LAYER                      |
 * |   +-------------------+              +-------------------+                |
 * |   | LC (NE)           |------------->| Attention Gain    |                |
 * |   | - Tonic level     |              | - Signal boost    |                |
 * |   | - Phasic bursts   |              | - Noise reduction |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | VTA (DA)          |------------->| Salience Filter   |                |
 * |   | - Incentive       |              | - Reward bias     |                |
 * |   | - Motivation      |              | - Goal attention  |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | Raphe (5-HT)      |------------->| Sustained Attn    |                |
 * |   | - Patience        |              | - Delay tolerance |                |
 * |   | - Impulse control |              | - Focus duration  |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | Habenula          |------------->| Aversion Filter   |                |
 * |   | - Disappointment  |              | - Threat avoidance|                |
 * |   | - Avoidance       |              | - Withdraw attn   |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |                        TOP-DOWN FEEDBACK                                  |
 * |   +-------------------+              +-------------------+                |
 * |   | LC Trigger        |<-------------| Novelty Detection |                |
 * |   | VTA Activation    |<-------------| Reward Features   |                |
 * |   | NE/DA Demand      |<-------------| Conflict Detection|                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_ATTENTION_BRIDGE_H
#define NIMCP_NEUROMOD_ATTENTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NEUROMOD_ATTENTION_BRIDGE_MAGIC     0x4E415442  /* "NATB" */

/* Bridge message types */
#define NEURO_ATT_MSG_GAIN_MODULATE         0x0050
#define NEURO_ATT_MSG_PHASIC_SHIFT          0x0051
#define NEURO_ATT_MSG_SALIENCE_BOOST        0x0052
#define NEURO_ATT_MSG_SUSTAINED_CAPACITY    0x0053
#define NEURO_ATT_MSG_AVERSION_WITHDRAW     0x0054
#define NEURO_ATT_MSG_NOVELTY_DETECTED      0x0055
#define NEURO_ATT_MSG_REWARD_FEATURE        0x0056
#define NEURO_ATT_MSG_CONFLICT_SIGNAL       0x0057

/* Biological constants */
#define NE_ATTENTION_GAIN_BASELINE          1.0f    /* Baseline gain multiplier */
#define NE_ATTENTION_GAIN_MAX               3.0f    /* Maximum gain boost */
#define NE_PHASIC_SHIFT_THRESHOLD           0.7f    /* NE level for attention shift */
#define DA_SALIENCE_COUPLING                0.5f    /* DA-to-salience coupling strength */
#define HT_PATIENCE_COUPLING                0.4f    /* 5-HT-to-patience coupling */
#define HAB_AVERSION_COUPLING               0.6f    /* Habenula-to-avoidance coupling */

/* ============================================================================
 * Types
 * ============================================================================ */

typedef struct neuromod_attention_bridge_struct neuromod_attention_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* NE-Attention coupling */
    float ne_gain_coupling;              /**< NE-to-gain modulation strength [0-1] */
    float ne_phasic_shift_threshold;     /**< NE level triggering attention shift [0-1] */
    float ne_tonic_vigilance_coupling;   /**< Tonic NE-to-vigilance coupling [0-1] */

    /* DA-Salience coupling */
    float da_salience_coupling;          /**< DA-to-salience boost strength [0-1] */
    float da_motivation_coupling;        /**< DA-to-goal-attention coupling [0-1] */

    /* 5-HT-Patience coupling */
    float ht_patience_coupling;          /**< 5-HT-to-sustained-attention coupling [0-1] */
    float ht_impulse_suppression;        /**< 5-HT impulse control strength [0-1] */

    /* Habenula-Aversion coupling */
    float hab_aversion_coupling;         /**< Habenula-to-avoidance coupling [0-1] */
    float hab_withdrawal_threshold;      /**< Threshold for attention withdrawal [0-1] */

    /* Top-down feedback */
    float novelty_lc_trigger_gain;       /**< Novelty-to-LC coupling [0-1] */
    float reward_vta_trigger_gain;       /**< Reward-to-VTA coupling [0-1] */
    float conflict_arousal_gain;         /**< Conflict-to-NE/DA demand [0-1] */

    /* Timing */
    uint32_t update_interval_ms;         /**< Update interval (default: 10ms) */

    /* Features */
    bool enable_adaptive_gain;           /**< Enable adaptive gain theory */
    bool enable_salience_filtering;      /**< Enable DA-based salience filter */
    bool enable_patience_modulation;     /**< Enable 5-HT patience effects */
    bool enable_aversion_withdrawal;     /**< Enable habenula attention withdrawal */
    bool enable_logging;                 /**< Enable event logging */
} neuromod_attention_config_t;

/**
 * @brief Current bridge state
 */
typedef struct {
    /* Attention modulation state */
    float attention_gain;                /**< Current attention gain multiplier */
    float vigilance_level;               /**< Current vigilance/arousal level [0-1] */
    float salience_boost;                /**< Current DA-driven salience boost [0-1] */
    float patience_capacity;             /**< Current sustained attention capacity [0-1] */
    float aversion_level;                /**< Current attention withdrawal level [0-1] */

    /* Neuromodulator levels (cached) */
    float ne_level;                      /**< Current NE level [0-1] */
    float da_level;                      /**< Current DA level [0-1] */
    float ht_level;                      /**< Current 5-HT level [0-1] */
    float hab_level;                     /**< Current habenula activity [0-1] */

    /* Top-down signals */
    float novelty_signal;                /**< Recent novelty detection [0-1] */
    float reward_signal;                 /**< Recent reward feature [0-1] */
    float conflict_signal;               /**< Recent conflict detection [0-1] */

    /* Metrics */
    float bridge_coherence;              /**< Bottom-up/top-down coherence [0-1] */
    uint64_t last_update_us;             /**< Last update timestamp */
} neuromod_attention_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Bottom-up events */
    uint32_t gain_modulations;           /**< Gain modulation events */
    uint32_t phasic_shifts;              /**< Phasic attention shifts */
    uint32_t salience_boosts;            /**< Salience boost events */
    uint32_t patience_modulations;       /**< Patience modulation events */
    uint32_t aversion_withdrawals;       /**< Aversion withdrawal events */

    /* Top-down events */
    uint32_t novelty_triggers;           /**< Novelty-to-LC triggers */
    uint32_t reward_triggers;            /**< Reward-to-VTA triggers */
    uint32_t conflict_signals;           /**< Conflict detection signals */

    /* Aggregates */
    float avg_attention_gain;            /**< Average attention gain */
    float avg_vigilance;                 /**< Average vigilance level */
    float avg_coherence;                 /**< Average bridge coherence */

    uint64_t total_updates;              /**< Total update cycles */
    uint64_t bottom_up_messages;         /**< Total bottom-up messages */
    uint64_t top_down_messages;          /**< Total top-down messages */
} neuromod_attention_stats_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* Lifecycle */
neuromod_attention_config_t neuromod_attention_default_config(void);
neuromod_attention_bridge_t* neuromod_attention_create(const neuromod_attention_config_t* config);
void neuromod_attention_destroy(neuromod_attention_bridge_t* bridge);

/* Bottom-up modulation (neuromod -> attention) */
int neuromod_attention_apply_ne_gain(neuromod_attention_bridge_t* bridge, float ne_level, float* gain_out);
int neuromod_attention_apply_phasic_shift(neuromod_attention_bridge_t* bridge, float ne_burst, bool* shift_triggered);
int neuromod_attention_apply_da_salience(neuromod_attention_bridge_t* bridge, float da_level, float* salience_out);
int neuromod_attention_apply_ht_patience(neuromod_attention_bridge_t* bridge, float ht_level, float* patience_out);
int neuromod_attention_apply_hab_aversion(neuromod_attention_bridge_t* bridge, float hab_level, float* withdrawal_out);

/* Top-down feedback (attention -> neuromod) */
int neuromod_attention_report_novelty(neuromod_attention_bridge_t* bridge, float novelty_score, float* lc_trigger_out);
int neuromod_attention_report_reward_feature(neuromod_attention_bridge_t* bridge, float reward_value, float* vta_trigger_out);
int neuromod_attention_report_conflict(neuromod_attention_bridge_t* bridge, float conflict_level, float* arousal_demand_out);

/* Unified modulation */
int neuromod_attention_compute_modulation(neuromod_attention_bridge_t* bridge,
                                          float ne_level, float da_level, float ht_level, float hab_level,
                                          neuromod_attention_state_t* state_out);

/* Update and state */
int neuromod_attention_update(neuromod_attention_bridge_t* bridge, float delta_ms);
int neuromod_attention_get_state(const neuromod_attention_bridge_t* bridge, neuromod_attention_state_t* state_out);
int neuromod_attention_get_stats(const neuromod_attention_bridge_t* bridge, neuromod_attention_stats_t* stats_out);
int neuromod_attention_reset_stats(neuromod_attention_bridge_t* bridge);

/* Diagnostics */
bool neuromod_attention_is_connected(const neuromod_attention_bridge_t* bridge);
float neuromod_attention_get_coherence(const neuromod_attention_bridge_t* bridge);
void neuromod_attention_print_summary(const neuromod_attention_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMOD_ATTENTION_BRIDGE_H */
