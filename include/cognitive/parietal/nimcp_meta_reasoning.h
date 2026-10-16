/**
 * @file nimcp_meta_reasoning.h
 * @brief Meta-reasoning engine for reasoning about reasoning
 *
 * WHAT: Engine for meta-cognitive monitoring and control
 * WHY:  Enable self-awareness of reasoning processes and strategies
 * HOW:  Strategy selection, confidence calibration, monitoring
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_META_REASONING_H
#define NIMCP_META_REASONING_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define META_MAX_STRATEGIES     16
#define META_MAX_STEPS          64
#define META_MAX_ANOMALIES      32
#define BIO_MODULE_META_REASONING 0x03A6

typedef struct meta_engine meta_engine_t;

typedef enum {
    META_STRATEGY_ANALYTICAL,
    META_STRATEGY_INTUITIVE,
    META_STRATEGY_ANALOGICAL,
    META_STRATEGY_TRIAL_ERROR,
    META_STRATEGY_DIVIDE_CONQUER,
    META_STRATEGY_MEANS_ENDS,
    META_STRATEGY_WORKING_BACKWARD,
    META_STRATEGY_BRAINSTORMING
} meta_strategy_type_t;

typedef struct {
    uint32_t id;
    meta_strategy_type_t type;
    char name[128];
    float suitability;
    float past_success_rate;
    float computational_cost;
    float time_estimate;
} meta_strategy_t;

typedef struct {
    uint32_t id;
    char description[256];
    float* state;
    uint32_t state_dim;
    float estimated_difficulty;
    meta_strategy_type_t preferred_strategy;
} meta_problem_t;

typedef struct {
    uint32_t step_number;
    char description[256];
    float confidence;
    float progress;
    meta_strategy_type_t strategy_used;
} meta_reasoning_step_t;

typedef struct {
    meta_reasoning_step_t* steps;
    uint32_t num_steps;
    float overall_confidence;
    float progress_rate;
    bool completed;
} meta_reasoning_chain_t;

typedef struct {
    uint32_t id;
    char description[256];
    uint32_t step_number;
    float severity;
    bool is_recoverable;
} meta_anomaly_t;

typedef struct {
    meta_strategy_t* strategy;
    float confidence;
    float progress;
    meta_anomaly_t* detected_issues;
    uint32_t num_issues;
} meta_state_t;

typedef struct {
    float confidence_calibration_strength;
    float strategy_adaptation_rate;
    bool enable_anomaly_detection;
    bool enable_strategy_switching;
    uint32_t monitoring_frequency;
    float inflammation_sensitivity;
    float fatigue_sensitivity;
} meta_config_t;

typedef struct {
    uint64_t strategies_selected;
    uint64_t strategy_switches;
    uint64_t anomalies_detected;
    uint64_t anomalies_recovered;
    float avg_calibration_error;
    float avg_strategy_success;
} meta_stats_t;

/* Lifecycle */
meta_engine_t* meta_engine_create(void);
meta_engine_t* meta_engine_create_custom(const meta_config_t* config);
void meta_engine_destroy(meta_engine_t* engine);
meta_config_t meta_engine_default_config(void);

/* Strategy Selection */
meta_strategy_t* meta_select_strategy(meta_engine_t* engine, const meta_problem_t* problem);
int meta_evaluate_strategies(meta_engine_t* engine, const meta_problem_t* problem,
    meta_strategy_t* strategies, uint32_t max_strategies, uint32_t* num_found);
int meta_switch_strategy(meta_engine_t* engine, meta_reasoning_chain_t* chain,
    const meta_strategy_t* new_strategy);

/* Confidence Calibration */
float meta_calibrate_confidence(meta_engine_t* engine, const meta_reasoning_chain_t* chain);
float meta_estimate_accuracy(meta_engine_t* engine, float stated_confidence);
int meta_update_calibration(meta_engine_t* engine, float prediction, float actual);

/* Reasoning Monitoring */
int meta_monitor_reasoning(meta_engine_t* engine, const meta_reasoning_chain_t* chain,
    meta_anomaly_t* anomalies, uint32_t max_anomalies, uint32_t* num_found);
int meta_get_state(meta_engine_t* engine, const meta_reasoning_chain_t* chain,
    meta_state_t* state);
float meta_estimate_progress(meta_engine_t* engine, const meta_reasoning_chain_t* chain);

/* Learning */
int meta_learn_from_outcome(meta_engine_t* engine, const meta_problem_t* problem,
    const meta_strategy_t* strategy, bool success, float performance);
int meta_learn_from_chain(meta_engine_t* engine, const meta_reasoning_chain_t* chain,
    bool success);

/* Cleanup */
void meta_free_strategy(meta_strategy_t* strategy);
void meta_free_chain(meta_reasoning_chain_t* chain);

/* Modulation */
int meta_set_inflammation(meta_engine_t* engine, float level);
int meta_set_fatigue(meta_engine_t* engine, float level);

/* Statistics */
int meta_get_stats(const meta_engine_t* engine, meta_stats_t* stats);
void meta_reset_stats(meta_engine_t* engine);
const char* meta_get_last_error(void);
const char* meta_strategy_name(meta_strategy_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_META_REASONING_H */
