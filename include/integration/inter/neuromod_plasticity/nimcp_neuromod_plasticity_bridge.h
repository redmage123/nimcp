/**
 * @file nimcp_neuromod_plasticity_bridge.h
 * @brief Neuromodulatory-Plasticity Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridges neuromodulatory nuclei to synaptic plasticity mechanisms
 * WHY:  Learning is fundamentally gated by neuromodulators (DA gates LTP, NE consolidation)
 * HOW:  DA signals enable/disable learning, NE enhances emotional memory, 5-HT stabilizes
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * VTA (DOPAMINE) - REWARD-GATED PLASTICITY:
 * Dopamine is THE signal for reward-based learning:
 * - Reward prediction error: DA encodes difference between expected and received reward
 * - LTP gating: DA release enables LTP at recently active synapses
 * - Credit assignment: Phasic DA marks which actions led to reward
 * - Temporal eligibility: DA converts eligibility traces to permanent changes
 *
 * LOCUS COERULEUS (NE) - EMOTIONAL MEMORY:
 * NE enhances memory consolidation for emotionally salient events:
 * - Amygdala-hippocampus: NE strengthens emotional memory formation
 * - Stress memories: High NE during stress creates strong memories
 * - Attention-gated learning: NE signals "this is important, remember it"
 * - Flexibility: High NE enables rapid relearning/updating
 *
 * RAPHE (5-HT) - MEMORY STABILIZATION:
 * Serotonin affects memory consolidation and stability:
 * - Sleep consolidation: 5-HT levels during sleep affect memory transfer
 * - Interference protection: 5-HT reduces interference between memories
 * - Mood-congruent learning: 5-HT levels affect what gets consolidated
 *
 * HABENULA - ANTI-REWARD LEARNING:
 * The habenula signals negative prediction errors:
 * - Punishment learning: Hab activates when expected reward fails
 * - Avoidance learning: Hab signals enable learning to avoid bad outcomes
 * - Depression link: Chronic Hab activation impairs positive learning
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Neuromodulatory -> Plasticity):
 * - DA level -> LTP eligibility gate
 * - DA phasic -> reward prediction error
 * - NE level -> emotional memory boost
 * - 5-HT level -> consolidation rate
 * - Habenula -> punishment/avoidance learning
 *
 * Top-Down (Plasticity -> Neuromodulatory):
 * - Learning success -> VTA reward signal
 * - Prediction error -> DA demand
 * - Novel learning -> LC activation
 * - Memory conflict -> 5-HT modulation request
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |             NEUROMODULATORY-PLASTICITY INTER-LAYER BRIDGE                 |
 * +===========================================================================+
 * |                                                                           |
 * |   NEUROMODULATORY LAYER              PLASTICITY LAYER                     |
 * |   +-------------------+              +-------------------+                |
 * |   | VTA (DA)          |------------->| LTP/LTD Gates     |                |
 * |   | - Reward signal   |              | - Eligibility     |                |
 * |   | - Prediction err  |              | - Credit assign   |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | LC (NE)           |------------->| Memory Strength   |                |
 * |   | - Arousal         |              | - Emotional boost |                |
 * |   | - Salience        |              | - Rapid learning  |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | Raphe (5-HT)      |------------->| Consolidation     |                |
 * |   | - Sleep state     |              | - Stabilization   |                |
 * |   | - Mood            |              | - Interference    |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | Habenula          |------------->| Avoidance Learn   |                |
 * |   | - Disappointment  |              | - Punishment      |                |
 * |   | - Neg pred error  |              | - Extinction      |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |                        TOP-DOWN FEEDBACK                                  |
 * |   +-------------------+              +-------------------+                |
 * |   | VTA Activation    |<-------------| Learning Success  |                |
 * |   | LC Trigger        |<-------------| Novel Pattern     |                |
 * |   | 5-HT Modulation   |<-------------| Memory Conflict   |                |
 * |   | Hab Activation    |<-------------| Prediction Miss   |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_PLASTICITY_BRIDGE_H
#define NIMCP_NEUROMOD_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NEUROMOD_PLASTICITY_BRIDGE_MAGIC    0x4E504C42  /* "NPLB" */

/* Bridge message types */
#define NEURO_PLAS_MSG_LTP_GATE             0x0080
#define NEURO_PLAS_MSG_LTD_GATE             0x0081
#define NEURO_PLAS_MSG_REWARD_PE            0x0082
#define NEURO_PLAS_MSG_EMOTIONAL_BOOST      0x0083
#define NEURO_PLAS_MSG_CONSOLIDATION        0x0084
#define NEURO_PLAS_MSG_AVOIDANCE            0x0085
#define NEURO_PLAS_MSG_LEARNING_SUCCESS     0x0086
#define NEURO_PLAS_MSG_PREDICTION_ERROR     0x0087

/* Biological constants */
#define DA_LTP_GATE_THRESHOLD               0.3f    /* DA level to enable LTP */
#define DA_LTP_GATE_STRENGTH                0.7f    /* DA gating strength */
#define NE_MEMORY_BOOST_COUPLING            0.5f    /* NE memory enhancement */
#define HT_CONSOLIDATION_COUPLING           0.4f    /* 5-HT consolidation rate */
#define HAB_AVOIDANCE_COUPLING              0.6f    /* Habenula avoidance learning */

/* Eligibility trace parameters */
#define ELIGIBILITY_TRACE_DECAY             0.95f   /* Per-timestep decay */
#define ELIGIBILITY_WINDOW_MS               1000    /* Window for credit assignment */

/* ============================================================================
 * Types
 * ============================================================================ */

typedef struct neuromod_plasticity_bridge_struct neuromod_plasticity_bridge_t;

/**
 * @brief Plasticity mode
 */
typedef enum {
    PLASTICITY_MODE_NORMAL = 0,     /* Standard learning */
    PLASTICITY_MODE_BOOSTED,        /* Enhanced by NE/arousal */
    PLASTICITY_MODE_GATED,          /* Waiting for DA signal */
    PLASTICITY_MODE_CONSOLIDATING,  /* Sleep/rest consolidation */
    PLASTICITY_MODE_SUPPRESSED      /* Learning suppressed (high Hab) */
} plasticity_mode_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* DA-LTP coupling */
    float da_ltp_gate_threshold;         /**< DA level to enable LTP [0-1] */
    float da_ltp_gate_strength;          /**< Strength of DA gating [0-1] */
    float da_ltd_gate_strength;          /**< Strength of DA on LTD [0-1] */
    float da_reward_pe_coupling;         /**< Reward prediction error coupling [0-1] */

    /* NE-Memory coupling */
    float ne_memory_boost_coupling;      /**< NE memory enhancement [0-1] */
    float ne_emotional_tag_threshold;    /**< NE level for emotional tagging [0-1] */
    float ne_rapid_learning_boost;       /**< NE boost for rapid learning [0-1] */

    /* 5-HT-Consolidation coupling */
    float ht_consolidation_coupling;     /**< 5-HT consolidation rate [0-1] */
    float ht_interference_reduction;     /**< 5-HT interference protection [0-1] */

    /* Habenula-Avoidance coupling */
    float hab_avoidance_coupling;        /**< Habenula avoidance learning [0-1] */
    float hab_extinction_coupling;       /**< Habenula contribution to extinction [0-1] */

    /* Eligibility traces */
    float eligibility_decay;             /**< Eligibility trace decay rate [0-1] */
    uint32_t eligibility_window_ms;      /**< Eligibility window duration */

    /* Top-down feedback */
    float success_vta_trigger_gain;      /**< Learning success-to-VTA [0-1] */
    float novelty_lc_trigger_gain;       /**< Novel pattern-to-LC [0-1] */
    float conflict_ht_modulation_gain;   /**< Memory conflict-to-5-HT [0-1] */
    float miss_hab_trigger_gain;         /**< Prediction miss-to-Hab [0-1] */

    /* Timing */
    uint32_t update_interval_ms;         /**< Update interval (default: 10ms) */

    /* Features */
    bool enable_da_gating;               /**< Enable DA LTP gating */
    bool enable_ne_boost;                /**< Enable NE memory boost */
    bool enable_consolidation;           /**< Enable 5-HT consolidation */
    bool enable_avoidance_learning;      /**< Enable habenula avoidance */
    bool enable_eligibility_traces;      /**< Enable eligibility traces */
    bool enable_logging;                 /**< Enable event logging */
} neuromod_plasticity_config_t;

/**
 * @brief Current bridge state
 */
typedef struct {
    /* Plasticity modulation state */
    float ltp_gate_level;                /**< Current LTP gate opening [0-1] */
    float ltd_gate_level;                /**< Current LTD gate opening [0-1] */
    float memory_boost;                  /**< Current memory boost factor [1-N] */
    float consolidation_rate;            /**< Current consolidation rate [0-1] */
    float avoidance_signal;              /**< Current avoidance learning signal [0-1] */
    plasticity_mode_t current_mode;      /**< Current plasticity mode */

    /* Eligibility trace */
    float eligibility_level;             /**< Current eligibility trace [0-1] */
    uint64_t eligibility_start_us;       /**< When eligibility started */

    /* Reward prediction error */
    float reward_prediction_error;       /**< Last RPE [-1 to +1] */
    float expected_reward;               /**< Current expected reward [0-1] */

    /* Neuromodulator levels (cached) */
    float da_level;                      /**< Current DA level [0-1] */
    float ne_level;                      /**< Current NE level [0-1] */
    float ht_level;                      /**< Current 5-HT level [0-1] */
    float hab_level;                     /**< Current habenula activity [0-1] */

    /* Top-down signals */
    float learning_success;              /**< Recent learning success [0-1] */
    float novelty_signal;                /**< Recent novelty detection [0-1] */
    float memory_conflict;               /**< Recent memory conflict [0-1] */
    float prediction_miss;               /**< Recent prediction miss [0-1] */

    /* Metrics */
    float learning_efficiency;           /**< Overall learning efficiency [0-1] */
    float bridge_coherence;              /**< Bottom-up/top-down coherence [0-1] */
    uint64_t last_update_us;             /**< Last update timestamp */
} neuromod_plasticity_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Bottom-up events */
    uint32_t ltp_gate_openings;          /**< LTP gate opening events */
    uint32_t ltd_gate_openings;          /**< LTD gate opening events */
    uint32_t memory_boosts;              /**< Memory boost events */
    uint32_t consolidation_events;       /**< Consolidation events */
    uint32_t avoidance_signals;          /**< Avoidance learning signals */

    /* Eligibility events */
    uint32_t eligibility_captures;       /**< Eligibility traces captured by DA */
    uint32_t eligibility_decays;         /**< Eligibility traces decayed */

    /* Top-down events */
    uint32_t success_signals;            /**< Learning success signals */
    uint32_t novelty_triggers;           /**< Novelty-to-LC triggers */
    uint32_t conflict_signals;           /**< Memory conflict signals */
    uint32_t prediction_misses;          /**< Prediction miss events */

    /* Aggregates */
    float avg_ltp_gate;                  /**< Average LTP gate level */
    float avg_memory_boost;              /**< Average memory boost */
    float avg_rpe;                       /**< Average reward prediction error */
    float total_rpe;                     /**< Cumulative RPE */

    uint64_t total_updates;              /**< Total update cycles */
    uint64_t bottom_up_messages;         /**< Total bottom-up messages */
    uint64_t top_down_messages;          /**< Total top-down messages */
} neuromod_plasticity_stats_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* Lifecycle */
neuromod_plasticity_config_t neuromod_plasticity_default_config(void);
neuromod_plasticity_bridge_t* neuromod_plasticity_create(const neuromod_plasticity_config_t* config);
void neuromod_plasticity_destroy(neuromod_plasticity_bridge_t* bridge);

/* Bottom-up modulation (neuromod -> plasticity) */
int neuromod_plasticity_apply_da_gating(neuromod_plasticity_bridge_t* bridge, float da_level, float* ltp_gate_out);
int neuromod_plasticity_apply_reward_pe(neuromod_plasticity_bridge_t* bridge, float reward, float expected, float* rpe_out);
int neuromod_plasticity_apply_ne_boost(neuromod_plasticity_bridge_t* bridge, float ne_level, float* boost_out);
int neuromod_plasticity_apply_ht_consolidation(neuromod_plasticity_bridge_t* bridge, float ht_level, float* rate_out);
int neuromod_plasticity_apply_hab_avoidance(neuromod_plasticity_bridge_t* bridge, float hab_level, float* avoidance_out);

/* Eligibility trace management */
int neuromod_plasticity_set_eligibility(neuromod_plasticity_bridge_t* bridge, float level);
int neuromod_plasticity_capture_eligibility(neuromod_plasticity_bridge_t* bridge, float da_signal);
int neuromod_plasticity_decay_eligibility(neuromod_plasticity_bridge_t* bridge, float delta_ms);

/* Top-down feedback (plasticity -> neuromod) */
int neuromod_plasticity_report_success(neuromod_plasticity_bridge_t* bridge, float success, float* vta_trigger_out);
int neuromod_plasticity_report_novelty(neuromod_plasticity_bridge_t* bridge, float novelty, float* lc_trigger_out);
int neuromod_plasticity_report_conflict(neuromod_plasticity_bridge_t* bridge, float conflict, float* ht_mod_out);
int neuromod_plasticity_report_prediction_miss(neuromod_plasticity_bridge_t* bridge, float miss, float* hab_trigger_out);

/* Unified modulation */
int neuromod_plasticity_compute_modulation(neuromod_plasticity_bridge_t* bridge,
                                           float da_level, float ne_level,
                                           float ht_level, float hab_level,
                                           neuromod_plasticity_state_t* state_out);

/* Update and state */
int neuromod_plasticity_update(neuromod_plasticity_bridge_t* bridge, float delta_ms);
int neuromod_plasticity_get_state(const neuromod_plasticity_bridge_t* bridge, neuromod_plasticity_state_t* state_out);
int neuromod_plasticity_get_stats(const neuromod_plasticity_bridge_t* bridge, neuromod_plasticity_stats_t* stats_out);
int neuromod_plasticity_reset_stats(neuromod_plasticity_bridge_t* bridge);

/* Diagnostics */
bool neuromod_plasticity_is_connected(const neuromod_plasticity_bridge_t* bridge);
float neuromod_plasticity_get_efficiency(const neuromod_plasticity_bridge_t* bridge);
float neuromod_plasticity_get_coherence(const neuromod_plasticity_bridge_t* bridge);
plasticity_mode_t neuromod_plasticity_get_mode(const neuromod_plasticity_bridge_t* bridge);
const char* neuromod_plasticity_mode_name(plasticity_mode_t mode);
void neuromod_plasticity_print_summary(const neuromod_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMOD_PLASTICITY_BRIDGE_H */
