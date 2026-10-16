/**
 * @file nimcp_neuromod_reasoning_bridge.h
 * @brief Neuromodulatory-Superhuman Reasoning Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridges neuromodulatory nuclei to metacognitive and superhuman reasoning systems
 * WHY:  Metacognition is fundamentally modulated by neuromodulators (DA=confidence, NE=control, 5-HT=patience)
 * HOW:  DA modulates confidence, NE modulates cognitive control, 5-HT enables deliberation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * VTA (DOPAMINE) - CONFIDENCE AND REWARD:
 * Dopamine modulates metacognitive confidence:
 * - Confidence calibration: DA affects how confident we feel about decisions
 * - Reward sensitivity: DA affects how much we value correct answers
 * - Exploration vs exploitation: DA controls curiosity in reasoning
 * - Error sensitivity: Low DA = hypersensitive to errors; High DA = overconfident
 *
 * LOCUS COERULEUS (NE) - COGNITIVE CONTROL:
 * NE is the master controller of cognitive mode:
 * - Arousal and alertness: NE determines cognitive resource availability
 * - Gain modulation: NE controls signal-to-noise in reasoning circuits
 * - Mode switching: NE enables switching between intuitive and analytical thinking
 * - Uncertainty response: NE increases when facing novel problems
 *
 * RAPHE (5-HT) - DELIBERATION AND PATIENCE:
 * Serotonin enables careful, deliberate reasoning:
 * - Patience: 5-HT allows waiting for better solutions
 * - Impulse control: 5-HT prevents premature conclusions
 * - Error tolerance: 5-HT affects persistence after mistakes
 * - Mood-cognition link: 5-HT connects emotional state to reasoning quality
 *
 * HABENULA - ERROR SIGNALING:
 * The habenula signals reasoning failures:
 * - Error detection: Hab activates when reasoning fails
 * - Strategy revision: Hab triggers reconsideration of approach
 * - Learned helplessness: Chronic Hab activation impairs reasoning confidence
 *
 * METACOGNITIVE FUNCTIONS:
 * ========================
 * - Confidence calibration: How well does confidence match actual accuracy?
 * - Cognitive control: Ability to direct and redirect reasoning
 * - Error monitoring: Detection and response to reasoning errors
 * - Strategy selection: Choosing between reasoning approaches
 * - Resource allocation: Distributing cognitive effort
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Neuromodulatory -> Reasoning):
 * - DA levels -> confidence, curiosity, reward sensitivity
 * - NE levels -> cognitive control, mode switching, alertness
 * - 5-HT levels -> deliberation, patience, impulse control
 * - Habenula -> error signals, strategy revision
 *
 * Top-Down (Reasoning -> Neuromodulatory):
 * - Reasoning success -> VTA reward
 * - Novel problem -> LC activation
 * - Deep thinking needed -> 5-HT demand
 * - Reasoning failure -> Habenula activation
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |           NEUROMODULATORY-SUPERHUMAN REASONING INTER-LAYER BRIDGE        |
 * +===========================================================================+
 * |                                                                           |
 * |   NEUROMODULATORY LAYER              REASONING LAYER                      |
 * |   +-------------------+              +-------------------+                |
 * |   | VTA (DA)          |------------->| Confidence System |                |
 * |   | - Confidence      |              | - Calibration     |                |
 * |   | - Curiosity       |              | - Risk assessment |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | LC (NE)           |------------->| Control System    |                |
 * |   | - Cognitive ctrl  |              | - Mode switching  |                |
 * |   | - Alertness       |              | - Resource alloc  |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | Raphe (5-HT)      |------------->| Deliberation Sys  |                |
 * |   | - Patience        |              | - Deep thinking   |                |
 * |   | - Impulse control |              | - Error tolerance |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | Habenula          |------------->| Error Monitor     |                |
 * |   | - Error signals   |              | - Strategy revise |                |
 * |   | - Disappointment  |              | - Reconsider      |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |                        TOP-DOWN FEEDBACK                                  |
 * |   +-------------------+              +-------------------+                |
 * |   | VTA Activation    |<-------------| Correct Answer    |                |
 * |   | LC Trigger        |<-------------| Novel Problem     |                |
 * |   | 5-HT Demand       |<-------------| Deep Think Need   |                |
 * |   | Hab Activation    |<-------------| Reasoning Error   |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_REASONING_BRIDGE_H
#define NIMCP_NEUROMOD_REASONING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NEUROMOD_REASONING_BRIDGE_MAGIC     0x4E525342  /* "NRSB" */

/* Bridge message types */
#define NEURO_RSN_MSG_CONFIDENCE            0x00A0
#define NEURO_RSN_MSG_CONTROL               0x00A1
#define NEURO_RSN_MSG_DELIBERATION          0x00A2
#define NEURO_RSN_MSG_ERROR_SIGNAL          0x00A3
#define NEURO_RSN_MSG_CORRECT_ANSWER        0x00A4
#define NEURO_RSN_MSG_NOVEL_PROBLEM         0x00A5
#define NEURO_RSN_MSG_DEEP_THINK            0x00A6
#define NEURO_RSN_MSG_REASONING_ERROR       0x00A7

/* Biological constants */
#define DA_CONFIDENCE_COUPLING              0.6f    /* DA-to-confidence coupling */
#define DA_CURIOSITY_COUPLING               0.5f    /* DA-to-curiosity coupling */
#define NE_CONTROL_COUPLING                 0.7f    /* NE-to-control coupling */
#define NE_ALERTNESS_COUPLING               0.6f    /* NE-to-alertness coupling */
#define HT_DELIBERATION_COUPLING            0.5f    /* 5-HT-to-deliberation coupling */
#define HT_PATIENCE_COUPLING                0.6f    /* 5-HT-to-patience coupling */
#define HAB_ERROR_COUPLING                  0.6f    /* Habenula-to-error coupling */

/* Cognitive mode constants */
#define MODE_SWITCH_THRESHOLD               0.6f    /* NE threshold for mode switch */
#define CONFIDENCE_BASELINE                 0.5f    /* Baseline confidence */
#define CONTROL_BASELINE                    0.5f    /* Baseline cognitive control */

/* ============================================================================
 * Types
 * ============================================================================ */

typedef struct neuromod_reasoning_bridge_struct neuromod_reasoning_bridge_t;

/**
 * @brief Reasoning mode
 */
typedef enum {
    REASONING_MODE_BALANCED = 0,   /* Mixed intuitive/analytical */
    REASONING_MODE_INTUITIVE,      /* Fast, heuristic-based (low NE) */
    REASONING_MODE_ANALYTICAL,     /* Slow, deliberate (high NE) */
    REASONING_MODE_CREATIVE,       /* Exploratory, divergent (high DA) */
    REASONING_MODE_CAUTIOUS,       /* Careful, error-avoiding (high 5-HT) */
    REASONING_MODE_IMPAIRED        /* Reasoning impaired (high Hab) */
} reasoning_mode_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* DA-Confidence coupling */
    float da_confidence_coupling;        /**< DA-to-confidence coupling [0-1] */
    float da_curiosity_coupling;         /**< DA-to-curiosity coupling [0-1] */
    float da_reward_sensitivity;         /**< DA effect on reward sensitivity [0-1] */

    /* NE-Control coupling */
    float ne_control_coupling;           /**< NE-to-cognitive-control coupling [0-1] */
    float ne_alertness_coupling;         /**< NE-to-alertness coupling [0-1] */
    float ne_mode_switch_threshold;      /**< NE threshold for mode switching [0-1] */

    /* 5-HT-Deliberation coupling */
    float ht_deliberation_coupling;      /**< 5-HT-to-deliberation coupling [0-1] */
    float ht_patience_coupling;          /**< 5-HT-to-patience coupling [0-1] */
    float ht_impulse_suppression;        /**< 5-HT impulse control strength [0-1] */

    /* Habenula-Error coupling */
    float hab_error_coupling;            /**< Habenula-to-error sensitivity [0-1] */
    float hab_strategy_revision_gain;    /**< Habenula effect on strategy revision [0-1] */

    /* Metacognition parameters */
    float confidence_baseline;           /**< Baseline confidence level [0-1] */
    float control_baseline;              /**< Baseline control level [0-1] */

    /* Top-down feedback */
    float success_vta_trigger_gain;      /**< Correct answer-to-VTA coupling [0-1] */
    float novelty_lc_trigger_gain;       /**< Novel problem-to-LC coupling [0-1] */
    float depth_ht_demand_gain;          /**< Deep thinking-to-5-HT coupling [0-1] */
    float error_hab_trigger_gain;        /**< Reasoning error-to-Habenula [0-1] */

    /* Timing */
    uint32_t update_interval_ms;         /**< Update interval (default: 10ms) */

    /* Features */
    bool enable_confidence_modulation;   /**< Enable DA confidence effects */
    bool enable_control_modulation;      /**< Enable NE control effects */
    bool enable_deliberation_modulation; /**< Enable 5-HT deliberation effects */
    bool enable_error_monitoring;        /**< Enable habenula error effects */
    bool enable_mode_classification;     /**< Enable reasoning mode classification */
    bool enable_logging;                 /**< Enable event logging */
} neuromod_reasoning_config_t;

/**
 * @brief Current bridge state
 */
typedef struct {
    /* Metacognitive parameters */
    float confidence_level;              /**< Current confidence [0-1] */
    float curiosity_level;               /**< Current curiosity/exploration [0-1] */
    float cognitive_control;             /**< Current cognitive control [0-1] */
    float alertness_level;               /**< Current alertness [0-1] */
    float deliberation_level;            /**< Current deliberation depth [0-1] */
    float patience_level;                /**< Current patience/impulse control [0-1] */
    float error_sensitivity;             /**< Current error sensitivity [0-1] */
    reasoning_mode_t current_mode;       /**< Current reasoning mode */

    /* Neuromodulator levels (cached) */
    float da_level;                      /**< Current DA level [0-1] */
    float ne_level;                      /**< Current NE level [0-1] */
    float ht_level;                      /**< Current 5-HT level [0-1] */
    float hab_level;                     /**< Current habenula activity [0-1] */

    /* Feedback signals */
    float success_signal;                /**< Recent reasoning success [0-1] */
    float novelty_signal;                /**< Recent novel problem [0-1] */
    float depth_demand;                  /**< Deep thinking demand [0-1] */
    float error_signal;                  /**< Recent reasoning error [0-1] */

    /* Derived metrics */
    float reasoning_quality;             /**< Overall reasoning quality [0-1] */
    float metacognitive_awareness;       /**< Self-awareness of reasoning [0-1] */
    float bridge_coherence;              /**< Bottom-up/top-down coherence [0-1] */
    uint64_t last_update_us;             /**< Last update timestamp */
} neuromod_reasoning_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Bottom-up events */
    uint32_t confidence_modulations;     /**< Confidence modulation events */
    uint32_t control_modulations;        /**< Control modulation events */
    uint32_t deliberation_modulations;   /**< Deliberation modulation events */
    uint32_t error_signals;              /**< Error signal events */

    /* Mode changes */
    uint32_t mode_switches;              /**< Reasoning mode switches */
    uint32_t intuitive_episodes;         /**< Times in intuitive mode */
    uint32_t analytical_episodes;        /**< Times in analytical mode */
    uint32_t creative_episodes;          /**< Times in creative mode */

    /* Reasoning outcomes */
    uint32_t successful_reasoning;       /**< Successful reasoning events */
    uint32_t novel_problems;             /**< Novel problems encountered */
    uint32_t deep_thinking_episodes;     /**< Deep thinking episodes */
    uint32_t reasoning_errors;           /**< Reasoning errors */

    /* Metacognition metrics */
    float avg_confidence;                /**< Average confidence */
    float avg_control;                   /**< Average cognitive control */
    float avg_reasoning_quality;         /**< Average reasoning quality */
    float confidence_accuracy_corr;      /**< Confidence-accuracy correlation */

    uint64_t total_updates;              /**< Total update cycles */
    uint64_t bottom_up_messages;         /**< Total bottom-up messages */
    uint64_t top_down_messages;          /**< Total top-down messages */
} neuromod_reasoning_stats_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* Lifecycle */
neuromod_reasoning_config_t neuromod_reasoning_default_config(void);
neuromod_reasoning_bridge_t* neuromod_reasoning_create(const neuromod_reasoning_config_t* config);
void neuromod_reasoning_destroy(neuromod_reasoning_bridge_t* bridge);

/* Bottom-up modulation (neuromod -> reasoning) */
int neuromod_reasoning_apply_da_confidence(neuromod_reasoning_bridge_t* bridge, float da_level, float* conf_out);
int neuromod_reasoning_apply_da_curiosity(neuromod_reasoning_bridge_t* bridge, float da_level, float* curiosity_out);
int neuromod_reasoning_apply_ne_control(neuromod_reasoning_bridge_t* bridge, float ne_level, float* control_out);
int neuromod_reasoning_apply_ne_alertness(neuromod_reasoning_bridge_t* bridge, float ne_level, float* alert_out);
int neuromod_reasoning_apply_ht_deliberation(neuromod_reasoning_bridge_t* bridge, float ht_level, float* delib_out);
int neuromod_reasoning_apply_ht_patience(neuromod_reasoning_bridge_t* bridge, float ht_level, float* patience_out);
int neuromod_reasoning_apply_hab_error(neuromod_reasoning_bridge_t* bridge, float hab_level, float* error_sens_out);

/* Top-down feedback (reasoning -> neuromod) */
int neuromod_reasoning_report_success(neuromod_reasoning_bridge_t* bridge, float success, float* vta_trigger_out);
int neuromod_reasoning_report_novelty(neuromod_reasoning_bridge_t* bridge, float novelty, float* lc_trigger_out);
int neuromod_reasoning_report_depth_need(neuromod_reasoning_bridge_t* bridge, float depth, float* ht_demand_out);
int neuromod_reasoning_report_error(neuromod_reasoning_bridge_t* bridge, float error_severity, float* hab_trigger_out);

/* Metacognitive queries */
float neuromod_reasoning_get_confidence_calibration(neuromod_reasoning_bridge_t* bridge, float actual_accuracy);
bool neuromod_reasoning_should_switch_mode(neuromod_reasoning_bridge_t* bridge);
float neuromod_reasoning_estimate_effort_needed(neuromod_reasoning_bridge_t* bridge, float problem_complexity);

/* Unified modulation */
int neuromod_reasoning_compute_modulation(neuromod_reasoning_bridge_t* bridge,
                                          float da_level, float ne_level,
                                          float ht_level, float hab_level,
                                          neuromod_reasoning_state_t* state_out);

/* Mode classification */
reasoning_mode_t neuromod_reasoning_classify_mode(const neuromod_reasoning_bridge_t* bridge);
const char* neuromod_reasoning_mode_name(reasoning_mode_t mode);

/* Update and state */
int neuromod_reasoning_update(neuromod_reasoning_bridge_t* bridge, float delta_ms);
int neuromod_reasoning_get_state(const neuromod_reasoning_bridge_t* bridge, neuromod_reasoning_state_t* state_out);
int neuromod_reasoning_get_stats(const neuromod_reasoning_bridge_t* bridge, neuromod_reasoning_stats_t* stats_out);
int neuromod_reasoning_reset_stats(neuromod_reasoning_bridge_t* bridge);

/* Diagnostics */
bool neuromod_reasoning_is_connected(const neuromod_reasoning_bridge_t* bridge);
float neuromod_reasoning_get_quality(const neuromod_reasoning_bridge_t* bridge);
float neuromod_reasoning_get_coherence(const neuromod_reasoning_bridge_t* bridge);
void neuromod_reasoning_print_summary(const neuromod_reasoning_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMOD_REASONING_BRIDGE_H */
