/**
 * @file nimcp_neuromod_gametheory_bridge.h
 * @brief Neuromodulatory-Game Theory Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridges neuromodulatory nuclei to game-theoretic decision making
 * WHY:  Social decisions are fundamentally modulated by 5-HT (fairness), DA (competition), NE (urgency)
 * HOW:  5-HT affects cooperation/trust, DA affects risk/dominance, NE affects time pressure
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * RAPHE (5-HT) - SOCIAL COOPERATION:
 * Serotonin is the "prosocial" neuromodulator:
 * - Fairness perception: High 5-HT increases rejection of unfair offers
 * - Cooperation: 5-HT promotes cooperative strategies in social dilemmas
 * - Trust: 5-HT enhances willingness to trust others
 * - Punishment: 5-HT modulates altruistic punishment
 *
 * VTA (DA) - COMPETITION AND RISK:
 * Dopamine drives competitive and risky behaviors:
 * - Risk-taking: High DA increases willingness to gamble
 * - Dominance: DA promotes competitive strategies
 * - Reward sensitivity: DA affects how much reward matters
 * - Loss aversion: Baseline DA affects loss sensitivity
 *
 * LOCUS COERULEUS (NE) - URGENCY AND PRESSURE:
 * NE modulates decision speed and pressure sensitivity:
 * - Time pressure: High NE accelerates decisions under time limits
 * - Arousal: NE affects urgency in negotiations
 * - Exploration: NE controls explore/exploit in repeated games
 *
 * HABENULA - LOSS AND DISAPPOINTMENT:
 * The habenula signals negative outcomes in games:
 * - Loss aversion: Hab activation enhances loss sensitivity
 * - Disappointment: Hab fires when game outcomes disappoint
 * - Withdrawal: High Hab activity promotes exit strategies
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Neuromodulatory -> Game Theory):
 * - 5-HT levels -> cooperation tendency
 * - DA levels -> risk tolerance, competitiveness
 * - NE levels -> decision urgency
 * - Habenula -> loss aversion, withdrawal
 *
 * Top-Down (Game Theory -> Neuromodulatory):
 * - Fair offers -> 5-HT satisfaction
 * - Winning -> VTA reward
 * - Time pressure -> LC activation
 * - Losing/betrayal -> Habenula activation
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |             NEUROMODULATORY-GAME THEORY INTER-LAYER BRIDGE               |
 * +===========================================================================+
 * |                                                                           |
 * |   NEUROMODULATORY LAYER              GAME THEORY LAYER                    |
 * |   +-------------------+              +-------------------+                |
 * |   | Raphe (5-HT)      |------------->| Cooperation Mode  |                |
 * |   | - Fairness        |              | - Trust level     |                |
 * |   | - Prosocial       |              | - Fair offers     |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | VTA (DA)          |------------->| Competition Mode  |                |
 * |   | - Risk appetite   |              | - Dominance       |                |
 * |   | - Reward seeking  |              | - Aggression      |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | LC (NE)           |------------->| Urgency Mode      |                |
 * |   | - Time pressure   |              | - Quick decisions |                |
 * |   | - Arousal         |              | - Exploration     |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | Habenula          |------------->| Loss Aversion     |                |
 * |   | - Disappointment  |              | - Exit strategies |                |
 * |   | - Punishment      |              | - Risk avoidance  |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |                        TOP-DOWN FEEDBACK                                  |
 * |   +-------------------+              +-------------------+                |
 * |   | 5-HT Satisfaction |<-------------| Fair Treatment    |                |
 * |   | VTA Reward        |<-------------| Game Won          |                |
 * |   | LC Activation     |<-------------| Time Pressure     |                |
 * |   | Hab Activation    |<-------------| Game Lost/Betrayal|                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_GAMETHEORY_BRIDGE_H
#define NIMCP_NEUROMOD_GAMETHEORY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NEUROMOD_GAMETHEORY_BRIDGE_MAGIC    0x4E475442  /* "NGTB" */

/* Bridge message types */
#define NEURO_GT_MSG_COOPERATION            0x0090
#define NEURO_GT_MSG_COMPETITION            0x0091
#define NEURO_GT_MSG_URGENCY                0x0092
#define NEURO_GT_MSG_LOSS_AVERSION          0x0093
#define NEURO_GT_MSG_FAIR_OFFER             0x0094
#define NEURO_GT_MSG_GAME_WON               0x0095
#define NEURO_GT_MSG_GAME_LOST              0x0096
#define NEURO_GT_MSG_BETRAYAL               0x0097

/* Biological constants */
#define HT_COOPERATION_COUPLING             0.6f    /* 5-HT-to-cooperation coupling */
#define DA_COMPETITION_COUPLING             0.5f    /* DA-to-competition coupling */
#define DA_RISK_COUPLING                    0.6f    /* DA-to-risk coupling */
#define NE_URGENCY_COUPLING                 0.5f    /* NE-to-urgency coupling */
#define HAB_LOSS_AVERSION_COUPLING          0.7f    /* Habenula-to-loss aversion */

/* Game theory thresholds */
#define FAIR_OFFER_THRESHOLD                0.4f    /* Minimum fair offer (0.5 = 50%) */
#define COOPERATION_DEFAULT                 0.5f    /* Default cooperation level */
#define RISK_NEUTRAL                        0.5f    /* Risk-neutral threshold */

/* ============================================================================
 * Types
 * ============================================================================ */

typedef struct neuromod_gametheory_bridge_struct neuromod_gametheory_bridge_t;

/**
 * @brief Game theory strategy mode
 */
typedef enum {
    GT_STRATEGY_BALANCED = 0,     /* Balanced cooperation/competition */
    GT_STRATEGY_COOPERATIVE,      /* Preferring cooperation */
    GT_STRATEGY_COMPETITIVE,      /* Preferring competition */
    GT_STRATEGY_CAUTIOUS,         /* High loss aversion */
    GT_STRATEGY_AGGRESSIVE,       /* High risk, low loss aversion */
    GT_STRATEGY_URGENT            /* Time-pressured decisions */
} gt_strategy_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* 5-HT-Cooperation coupling */
    float ht_cooperation_coupling;       /**< 5-HT-to-cooperation coupling [0-1] */
    float ht_fairness_sensitivity;       /**< 5-HT effect on fairness perception [0-1] */
    float ht_trust_coupling;             /**< 5-HT effect on trust [0-1] */

    /* DA-Competition coupling */
    float da_competition_coupling;       /**< DA-to-competition coupling [0-1] */
    float da_risk_coupling;              /**< DA-to-risk tolerance coupling [0-1] */
    float da_dominance_coupling;         /**< DA effect on dominance behavior [0-1] */

    /* NE-Urgency coupling */
    float ne_urgency_coupling;           /**< NE-to-urgency coupling [0-1] */
    float ne_exploration_coupling;       /**< NE effect on exploration [0-1] */

    /* Habenula-Loss coupling */
    float hab_loss_aversion_coupling;    /**< Habenula-to-loss aversion [0-1] */
    float hab_withdrawal_coupling;       /**< Habenula effect on withdrawal [0-1] */

    /* Fair offer parameters */
    float fair_offer_threshold;          /**< Minimum acceptable fair offer [0-1] */
    float unfair_rejection_gain;         /**< Strength of unfair offer rejection [0-1] */

    /* Top-down feedback */
    float fairness_ht_trigger_gain;      /**< Fairness-to-5-HT coupling [0-1] */
    float winning_vta_trigger_gain;      /**< Winning-to-VTA coupling [0-1] */
    float pressure_lc_trigger_gain;      /**< Time pressure-to-LC coupling [0-1] */
    float losing_hab_trigger_gain;       /**< Losing-to-Habenula coupling [0-1] */

    /* Timing */
    uint32_t update_interval_ms;         /**< Update interval (default: 10ms) */

    /* Features */
    bool enable_cooperation_modulation;  /**< Enable 5-HT cooperation effects */
    bool enable_competition_modulation;  /**< Enable DA competition effects */
    bool enable_urgency_modulation;      /**< Enable NE urgency effects */
    bool enable_loss_aversion;           /**< Enable habenula loss aversion */
    bool enable_strategy_classification; /**< Enable strategy classification */
    bool enable_logging;                 /**< Enable event logging */
} neuromod_gametheory_config_t;

/**
 * @brief Current bridge state
 */
typedef struct {
    /* Game theory parameters */
    float cooperation_tendency;          /**< Tendency to cooperate [0-1] */
    float competition_tendency;          /**< Tendency to compete [0-1] */
    float risk_tolerance;                /**< Risk tolerance level [0-1] */
    float loss_aversion;                 /**< Loss aversion level [0-1] */
    float decision_urgency;              /**< Decision urgency level [0-1] */
    float trust_level;                   /**< Trust in others [0-1] */
    float fairness_threshold;            /**< Current fairness threshold [0-1] */
    gt_strategy_t current_strategy;      /**< Current strategy mode */

    /* Neuromodulator levels (cached) */
    float ht_level;                      /**< Current 5-HT level [0-1] */
    float da_level;                      /**< Current DA level [0-1] */
    float ne_level;                      /**< Current NE level [0-1] */
    float hab_level;                     /**< Current habenula activity [0-1] */

    /* Game outcome signals */
    float fair_treatment_signal;         /**< Recent fair treatment [0-1] */
    float winning_signal;                /**< Recent game winning [0-1] */
    float losing_signal;                 /**< Recent game losing [0-1] */
    float betrayal_signal;               /**< Recent betrayal detection [0-1] */

    /* Metrics */
    float strategic_coherence;           /**< Strategy coherence [0-1] */
    float bridge_coherence;              /**< Bottom-up/top-down coherence [0-1] */
    uint64_t last_update_us;             /**< Last update timestamp */
} neuromod_gametheory_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Bottom-up events */
    uint32_t cooperation_modulations;    /**< Cooperation modulation events */
    uint32_t competition_modulations;    /**< Competition modulation events */
    uint32_t urgency_modulations;        /**< Urgency modulation events */
    uint32_t loss_aversion_events;       /**< Loss aversion events */

    /* Strategy changes */
    uint32_t strategy_shifts;            /**< Strategy mode changes */
    uint32_t cooperative_choices;        /**< Cooperative strategy choices */
    uint32_t competitive_choices;        /**< Competitive strategy choices */

    /* Game outcomes */
    uint32_t fair_offers_received;       /**< Fair offers received */
    uint32_t unfair_offers_rejected;     /**< Unfair offers rejected */
    uint32_t games_won;                  /**< Games won */
    uint32_t games_lost;                 /**< Games lost */
    uint32_t betrayals_detected;         /**< Betrayals detected */

    /* Aggregates */
    float avg_cooperation;               /**< Average cooperation level */
    float avg_risk_tolerance;            /**< Average risk tolerance */
    float avg_loss_aversion;             /**< Average loss aversion */

    uint64_t total_updates;              /**< Total update cycles */
    uint64_t bottom_up_messages;         /**< Total bottom-up messages */
    uint64_t top_down_messages;          /**< Total top-down messages */
} neuromod_gametheory_stats_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* Lifecycle */
neuromod_gametheory_config_t neuromod_gametheory_default_config(void);
neuromod_gametheory_bridge_t* neuromod_gametheory_create(const neuromod_gametheory_config_t* config);
void neuromod_gametheory_destroy(neuromod_gametheory_bridge_t* bridge);

/* Bottom-up modulation (neuromod -> game theory) */
int neuromod_gametheory_apply_ht_cooperation(neuromod_gametheory_bridge_t* bridge, float ht_level, float* coop_out);
int neuromod_gametheory_apply_da_competition(neuromod_gametheory_bridge_t* bridge, float da_level, float* comp_out);
int neuromod_gametheory_apply_da_risk(neuromod_gametheory_bridge_t* bridge, float da_level, float* risk_out);
int neuromod_gametheory_apply_ne_urgency(neuromod_gametheory_bridge_t* bridge, float ne_level, float* urgency_out);
int neuromod_gametheory_apply_hab_loss_aversion(neuromod_gametheory_bridge_t* bridge, float hab_level, float* aversion_out);

/* Top-down feedback (game theory -> neuromod) */
int neuromod_gametheory_report_fair_treatment(neuromod_gametheory_bridge_t* bridge, float fairness, float* ht_trigger_out);
int neuromod_gametheory_report_game_won(neuromod_gametheory_bridge_t* bridge, float win_magnitude, float* vta_trigger_out);
int neuromod_gametheory_report_game_lost(neuromod_gametheory_bridge_t* bridge, float loss_magnitude, float* hab_trigger_out);
int neuromod_gametheory_report_betrayal(neuromod_gametheory_bridge_t* bridge, float severity, float* hab_trigger_out);
int neuromod_gametheory_report_time_pressure(neuromod_gametheory_bridge_t* bridge, float pressure, float* lc_trigger_out);

/* Decision support */
float neuromod_gametheory_evaluate_offer(neuromod_gametheory_bridge_t* bridge, float offer_value);
bool neuromod_gametheory_should_cooperate(neuromod_gametheory_bridge_t* bridge, float opponent_history);
bool neuromod_gametheory_should_take_risk(neuromod_gametheory_bridge_t* bridge, float expected_value, float variance);

/* Unified modulation */
int neuromod_gametheory_compute_modulation(neuromod_gametheory_bridge_t* bridge,
                                           float ht_level, float da_level,
                                           float ne_level, float hab_level,
                                           neuromod_gametheory_state_t* state_out);

/* Strategy */
gt_strategy_t neuromod_gametheory_classify_strategy(const neuromod_gametheory_bridge_t* bridge);
const char* neuromod_gametheory_strategy_name(gt_strategy_t strategy);

/* Update and state */
int neuromod_gametheory_update(neuromod_gametheory_bridge_t* bridge, float delta_ms);
int neuromod_gametheory_get_state(const neuromod_gametheory_bridge_t* bridge, neuromod_gametheory_state_t* state_out);
int neuromod_gametheory_get_stats(const neuromod_gametheory_bridge_t* bridge, neuromod_gametheory_stats_t* stats_out);
int neuromod_gametheory_reset_stats(neuromod_gametheory_bridge_t* bridge);

/* Diagnostics */
bool neuromod_gametheory_is_connected(const neuromod_gametheory_bridge_t* bridge);
float neuromod_gametheory_get_coherence(const neuromod_gametheory_bridge_t* bridge);
void neuromod_gametheory_print_summary(const neuromod_gametheory_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMOD_GAMETHEORY_BRIDGE_H */
