/**
 * @file nimcp_counterfactual.h
 * @brief Counterfactual reasoning engine for "what if" analysis
 *
 * WHAT: Engine for counterfactual reasoning and causal analysis
 * WHY:  Enable hypothetical scenario exploration and causal understanding
 * HOW:  State modification, consequence tracing, possibility space
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_COUNTERFACTUAL_H
#define NIMCP_COUNTERFACTUAL_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CF_MAX_STATE_DIM        256
#define CF_MAX_INTERVENTIONS    32
#define CF_MAX_CONSEQUENCES     64
#define CF_MAX_ALTERNATIVES     16
#define BIO_MODULE_COUNTERFACTUAL 0x03A5

typedef struct counterfactual_engine counterfactual_engine_t;

typedef struct {
    float* values;
    uint32_t dim;
    char description[256];
    uint64_t timestamp;
} cf_state_t;

typedef struct {
    uint32_t id;
    char description[256];
    uint32_t target_variable;
    float original_value;
    float counterfactual_value;
} cf_intervention_t;

typedef struct {
    uint32_t id;
    char description[256];
    uint32_t affected_variable;
    float magnitude;
    float probability;
    bool is_direct;
} cf_consequence_t;

typedef struct {
    uint32_t id;
    cf_state_t* actual_world;
    cf_state_t* counterfactual_world;
    cf_intervention_t* intervention;
    cf_consequence_t* consequences;
    uint32_t num_consequences;
    float distance;
    float plausibility;
} cf_counterfactual_t;

typedef struct {
    float min_plausibility;
    float consequence_threshold;
    bool enable_causal_tracing;
    bool use_minimal_change;
    uint32_t max_trace_depth;
    float inflammation_sensitivity;
    float fatigue_sensitivity;
} cf_config_t;

typedef struct {
    uint64_t counterfactuals_generated;
    uint64_t interventions_analyzed;
    uint64_t consequences_traced;
    float avg_distance;
    float avg_consequence_count;
} cf_stats_t;

/* Lifecycle */
counterfactual_engine_t* counterfactual_engine_create(void);
counterfactual_engine_t* counterfactual_engine_create_custom(const cf_config_t* config);
void counterfactual_engine_destroy(counterfactual_engine_t* engine);
cf_config_t counterfactual_engine_default_config(void);

/* State Management */
cf_state_t* counterfactual_create_state(const float* values, uint32_t dim,
    const char* description);
void counterfactual_free_state(cf_state_t* state);

/* Counterfactual Generation */
cf_counterfactual_t* counterfactual_imagine(counterfactual_engine_t* engine,
    const cf_state_t* actual, const cf_intervention_t* what_if);
cf_counterfactual_t** counterfactual_explore_space(counterfactual_engine_t* engine,
    const cf_state_t* actual, uint32_t max_alternatives, uint32_t* num_found);

/* Consequence Tracing */
int counterfactual_trace_effects(counterfactual_engine_t* engine,
    const cf_counterfactual_t* cf, cf_consequence_t* consequences,
    uint32_t max_consequences, uint32_t* num_found);
float counterfactual_estimate_probability(counterfactual_engine_t* engine,
    const cf_counterfactual_t* cf);

/* Causal Analysis */
float counterfactual_causal_strength(counterfactual_engine_t* engine,
    uint32_t cause_var, uint32_t effect_var, const cf_state_t* context);
int counterfactual_find_causes(counterfactual_engine_t* engine,
    const cf_state_t* state, uint32_t effect_var,
    uint32_t* cause_vars, uint32_t max_causes, uint32_t* num_found);

/* Distance Metrics */
float counterfactual_distance(counterfactual_engine_t* engine,
    const cf_state_t* s1, const cf_state_t* s2);
cf_counterfactual_t* counterfactual_find_closest(counterfactual_engine_t* engine,
    const cf_state_t* actual, const cf_state_t* target);

/* Cleanup */
void counterfactual_free(cf_counterfactual_t* cf);

/* Modulation */
int counterfactual_set_inflammation(counterfactual_engine_t* engine, float level);
int counterfactual_set_fatigue(counterfactual_engine_t* engine, float level);

/* Statistics */
int counterfactual_get_stats(const counterfactual_engine_t* engine, cf_stats_t* stats);
void counterfactual_reset_stats(counterfactual_engine_t* engine);
const char* counterfactual_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COUNTERFACTUAL_H */
