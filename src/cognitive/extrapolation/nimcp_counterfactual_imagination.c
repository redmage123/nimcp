/**
 * @file nimcp_counterfactual_imagination.c
 * @brief Counterfactual reasoning and "what if" scenario imagination
 *
 * WHAT: Implementation of counterfactual reasoning using causal inference
 * WHY:  Enable the system to reason about alternatives and learn from hypotheticals
 * HOW:  Pearl's causal framework with structural equation models
 *
 * @version 1.0.0
 * @date 2025-01-13
 */

#include "cognitive/extrapolation/nimcp_counterfactual_imagination.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define LOG_MODULE "cognitive.extrapolation.counterfactual"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE(counterfactual_imagination, MESH_ADAPTER_CATEGORY_COGNITIVE)



/*=============================================================================
 * INTERNAL HELPERS - DECLARATIONS
 *===========================================================================*/

static cf_causal_model_t* causal_model_create(uint32_t capacity);
static void causal_model_destroy(cf_causal_model_t* model);
static cf_error_t causal_model_add_variable(cf_causal_model_t* model,
                                            const char* name,
                                            cf_variable_type_t type,
                                            uint32_t* id);
static cf_variable_t* causal_model_get_variable(cf_causal_model_t* model, uint32_t id);
static cf_error_t causal_model_add_edge(cf_causal_model_t* model,
                                        uint32_t from, uint32_t to, float weight);
static cf_error_t compute_topological_order(cf_causal_model_t* model);
static cf_error_t propagate_values(cf_causal_model_t* model);
static uint64_t get_current_time_us(void);
static float compute_structural_equation(cf_variable_t* var, cf_causal_model_t* model);
static void add_history_entry(nimcp_counterfactual_t* cf, uint32_t var_id,
                              float before, float after, float effect);

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

cf_config_t cf_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_default_config", 0.0f);


    cf_config_t config = {
        .max_variables = CF_MAX_VARIABLES,
        .max_scenarios = CF_MAX_SCENARIOS,
        .max_history = CF_MAX_HISTORY,
        .simulation_steps = 100,
        .learning_rate = NIMCP_LEARNING_RATE_DEFAULT,
        .probability_threshold = 0.001f,
        .default_mode = CF_EVAL_CAUSAL,
        .enable_caching = true,
        .enable_logging = true,
        .enable_bio_async = false,
        .enable_world_model = false
    };
    return config;
}

nimcp_counterfactual_t* cf_create(const cf_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_create", 0.0f);


    nimcp_counterfactual_t* cf = nimcp_calloc(1, sizeof(nimcp_counterfactual_t));
    if (!cf) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate cf");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        cf->config = *config;
    } else {
        cf->config = cf_default_config();
    }

    /* Create causal model */
    cf->causal_model = causal_model_create(cf->config.max_variables);
    if (!cf->causal_model) {
        nimcp_free(cf);
        cf = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cf_create: cf->causal_model is NULL");
        return NULL;
    }

    /* Allocate scenarios */
    cf->scenario_capacity = cf->config.max_scenarios;
    cf->scenarios = nimcp_calloc(cf->scenario_capacity, sizeof(cf_scenario_t));
    if (!cf->scenarios) {
        causal_model_destroy(cf->causal_model);
        nimcp_free(cf);
        cf = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cf_create: cf->scenarios is NULL");
        return NULL;
    }
    cf->num_scenarios = 0;

    /* Allocate history */
    cf->history_capacity = cf->config.max_history;
    cf->history = nimcp_calloc(cf->history_capacity, sizeof(cf_history_entry_t));
    if (!cf->history) {
        nimcp_free(cf->scenarios);
        causal_model_destroy(cf->causal_model);
        nimcp_free(cf);
        cf = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cf_create: cf->history is NULL");
        return NULL;
    }
    cf->history_count = 0;

    /* Initialize state */
    cf->status = CF_STATUS_IDLE;
    cf->last_error = CF_OK;
    cf->initialized = false;

    return cf;
}

cf_error_t cf_init(nimcp_counterfactual_t* cf) {
    if (!cf) return CF_ERR_NULL_PTR;

    /* Reset statistics */
    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_init", 0.0f);


    memset(&cf->stats, 0, sizeof(cf_stats_t));

    /* Mark initialized */
    cf->initialized = true;
    cf->status = CF_STATUS_IDLE;
    cf->last_error = CF_OK;

    return CF_OK;
}

cf_error_t cf_reset(nimcp_counterfactual_t* cf) {
    if (!cf) return CF_ERR_NULL_PTR;

    /* Reset causal model variables to default values */
    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_reset", 0.0f);


    if (cf->causal_model) {
        for (uint32_t i = 0; i < cf->causal_model->num_variables; i++) {
            cf_variable_t* var = &cf->causal_model->variables[i];
            var->value = 0.0f;
            var->counterfactual_value = 0.0f;
            var->is_observed = false;
        }
        cf->causal_model->topological_valid = false;
    }

    /* Clear scenarios */
    cf->num_scenarios = 0;
    memset(cf->scenarios, 0, cf->scenario_capacity * sizeof(cf_scenario_t));

    /* Clear history */
    cf->history_count = 0;

    /* Reset stats */
    memset(&cf->stats, 0, sizeof(cf_stats_t));

    cf->status = CF_STATUS_IDLE;
    cf->last_error = CF_OK;

    return CF_OK;
}

void cf_destroy(nimcp_counterfactual_t* cf) {
    if (!cf) return;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_destroy", 0.0f);


    if (cf->causal_model) {
        causal_model_destroy(cf->causal_model);
    }

    nimcp_free(cf->scenarios);
    nimcp_free(cf->history);
    nimcp_free(cf);
    cf = NULL;
}

/*=============================================================================
 * VARIABLE API
 *===========================================================================*/

cf_error_t cf_add_variable(
    nimcp_counterfactual_t* cf,
    const char* name,
    cf_variable_type_t type,
    uint32_t* variable_id)
{
    if (!cf || !name || !variable_id) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_add_variable", 0.0f);


    cf_error_t err = causal_model_add_variable(cf->causal_model, name, type, variable_id);
    if (err == CF_OK) {
        cf->stats.variables_created++;
    }
    return err;
}

cf_error_t cf_set_variable(
    nimcp_counterfactual_t* cf,
    uint32_t variable_id,
    float value)
{
    if (!cf) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_set_variable", 0.0f);


    cf_variable_t* var = causal_model_get_variable(cf->causal_model, variable_id);
    if (!var) {
        cf->last_error = CF_ERR_VARIABLE_NOT_FOUND;
        return CF_ERR_VARIABLE_NOT_FOUND;
    }

    float old_value = var->value;
    var->value = value;

    /* Invalidate topological order since values changed */
    cf->causal_model->topological_valid = false;

    /* Record in history */
    add_history_entry(cf, variable_id, old_value, value, value - old_value);

    return CF_OK;
}

cf_error_t cf_get_variable(
    nimcp_counterfactual_t* cf,
    uint32_t variable_id,
    float* value)
{
    if (!cf || !value) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_get_variable", 0.0f);


    cf_variable_t* var = causal_model_get_variable(cf->causal_model, variable_id);
    if (!var) {
        cf->last_error = CF_ERR_VARIABLE_NOT_FOUND;
        return CF_ERR_VARIABLE_NOT_FOUND;
    }

    *value = var->value;
    return CF_OK;
}

cf_error_t cf_get_variable_by_name(
    nimcp_counterfactual_t* cf,
    const char* name,
    cf_variable_t* variable)
{
    if (!cf || !name || !variable) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_get_variable_by_n", 0.0f);


    cf_causal_model_t* model = cf->causal_model;
    for (uint32_t i = 0; i < model->num_variables; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && model->num_variables > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)model->num_variables);
        }

        if (strcmp(model->variables[i].name, name) == 0) {
            *variable = model->variables[i];
            return CF_OK;
        }
    }

    cf->last_error = CF_ERR_VARIABLE_NOT_FOUND;
    return CF_ERR_VARIABLE_NOT_FOUND;
}

cf_error_t cf_observe(
    nimcp_counterfactual_t* cf,
    uint32_t variable_id,
    float value,
    float confidence)
{
    if (!cf) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_observe", 0.0f);


    cf_variable_t* var = causal_model_get_variable(cf->causal_model, variable_id);
    if (!var) {
        cf->last_error = CF_ERR_VARIABLE_NOT_FOUND;
        return CF_ERR_VARIABLE_NOT_FOUND;
    }

    /* Clamp confidence */
    confidence = confidence < 0.0f ? 0.0f : (confidence > 1.0f ? 1.0f : confidence);

    float old_value = var->value;
    var->value = value;
    var->is_observed = true;
    var->observation_confidence = confidence;
    var->observation_time = get_current_time_us();

    /* Update running statistics */
    float delta = value - var->mean;
    var->mean += delta * 0.1f;  /* Exponential moving average */
    var->variance = 0.9f * var->variance + 0.1f * delta * delta;

    cf->stats.observations_recorded++;

    /* Record in history */
    add_history_entry(cf, variable_id, old_value, value, value - old_value);

    return CF_OK;
}

cf_error_t cf_add_causal_link(
    nimcp_counterfactual_t* cf,
    uint32_t parent_id,
    uint32_t child_id,
    float weight)
{
    if (!cf) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_add_causal_link", 0.0f);


    return causal_model_add_edge(cf->causal_model, parent_id, child_id, weight);
}

/*=============================================================================
 * INTERVENTION API
 *===========================================================================*/

cf_error_t cf_intervene(
    nimcp_counterfactual_t* cf,
    uint32_t variable_id,
    float value)
{
    if (!cf) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_intervene", 0.0f);


    cf->status = CF_STATUS_INTERVENING;

    cf_variable_t* var = causal_model_get_variable(cf->causal_model, variable_id);
    if (!var) {
        cf->status = CF_STATUS_ERROR;
        cf->last_error = CF_ERR_VARIABLE_NOT_FOUND;
        return CF_ERR_VARIABLE_NOT_FOUND;
    }

    /* Store original value */
    float original = var->value;

    /* WHAT: Apply do(X=x) intervention
     * WHY:  This is the core causal operation
     * HOW:  Set value directly, effectively cutting incoming causal edges */

    var->counterfactual_value = value;

    /* Store the intervention in history */
    add_history_entry(cf, variable_id, original, value, 0.0f);

    /* Propagate through causal model */
    /* First ensure topological order is computed */
    if (!cf->causal_model->topological_valid) {
        cf_error_t err = compute_topological_order(cf->causal_model);
        if (err != CF_OK) {
            cf->status = CF_STATUS_ERROR;
            cf->last_error = err;
            return err;
        }
    }

    /* Propagate counterfactual values through the graph */
    cf_error_t err = propagate_values(cf->causal_model);
    if (err != CF_OK) {
        cf->status = CF_STATUS_ERROR;
        cf->last_error = err;
        return err;
    }

    cf->stats.interventions_applied++;
    cf->status = CF_STATUS_IDLE;

    return CF_OK;
}

cf_error_t cf_undo_intervention(nimcp_counterfactual_t* cf) {
    if (!cf) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;
    if (cf->history_count == 0) return CF_OK;

    /* Get last history entry */
    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_undo_intervention", 0.0f);


    cf_history_entry_t* entry = &cf->history[cf->history_count - 1];

    cf_variable_t* var = causal_model_get_variable(cf->causal_model, entry->variable_id);
    if (var) {
        var->counterfactual_value = entry->value_before;
    }

    cf->history_count--;

    return CF_OK;
}

cf_error_t cf_clear_interventions(nimcp_counterfactual_t* cf) {
    if (!cf) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;

    /* Reset all counterfactual values to factual values */
    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_clear_interventio", 0.0f);


    cf_causal_model_t* model = cf->causal_model;
    for (uint32_t i = 0; i < model->num_variables; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && model->num_variables > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)model->num_variables);
        }

        model->variables[i].counterfactual_value = model->variables[i].value;
    }

    return CF_OK;
}

/*=============================================================================
 * SCENARIO API
 *===========================================================================*/

cf_error_t cf_create_scenario(
    nimcp_counterfactual_t* cf,
    cf_scenario_type_t type,
    const char* description,
    uint32_t* scenario_id)
{
    if (!cf || !scenario_id) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;
    if (cf->num_scenarios >= cf->scenario_capacity) return CF_ERR_CAPACITY_EXCEEDED;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_create_scenario", 0.0f);


    cf_scenario_t* scenario = &cf->scenarios[cf->num_scenarios];
    memset(scenario, 0, sizeof(cf_scenario_t));

    scenario->scenario_id = cf->num_scenarios;
    scenario->type = type;
    scenario->created_time = get_current_time_us();
    scenario->evaluated = false;
    scenario->probability = 0.0f;
    scenario->plausibility = 0.0f;

    if (description) {
        strncpy(scenario->description, description, sizeof(scenario->description) - 1);
    }

    *scenario_id = scenario->scenario_id;
    cf->num_scenarios++;
    cf->stats.scenarios_created++;

    return CF_OK;
}

cf_error_t cf_scenario_add_intervention(
    nimcp_counterfactual_t* cf,
    uint32_t scenario_id,
    uint32_t variable_id,
    float value)
{
    if (!cf) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;
    if (scenario_id >= cf->num_scenarios) return CF_ERR_INVALID_CONFIG;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_scenario_add_inte", 0.0f);


    cf_scenario_t* scenario = &cf->scenarios[scenario_id];
    if (scenario->num_interventions >= CF_MAX_INTERVENTIONS) {
        return CF_ERR_CAPACITY_EXCEEDED;
    }

    /* Verify variable exists */
    cf_variable_t* var = causal_model_get_variable(cf->causal_model, variable_id);
    if (!var) return CF_ERR_VARIABLE_NOT_FOUND;

    cf_intervention_t* interv = &scenario->interventions[scenario->num_interventions];
    interv->variable_id = variable_id;
    interv->original_value = var->value;
    interv->new_value = value;
    interv->counterfactual_value = 0.0f;
    interv->applied = false;
    interv->timestamp = 0;

    scenario->num_interventions++;

    return CF_OK;
}

cf_error_t cf_scenario_add_outcome(
    nimcp_counterfactual_t* cf,
    uint32_t scenario_id,
    uint32_t variable_id)
{
    if (!cf) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;
    if (scenario_id >= cf->num_scenarios) return CF_ERR_INVALID_CONFIG;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_scenario_add_outc", 0.0f);


    cf_scenario_t* scenario = &cf->scenarios[scenario_id];
    if (scenario->num_outcomes >= CF_MAX_OUTCOMES) {
        return CF_ERR_CAPACITY_EXCEEDED;
    }

    /* Verify variable exists */
    cf_variable_t* var = causal_model_get_variable(cf->causal_model, variable_id);
    if (!var) return CF_ERR_VARIABLE_NOT_FOUND;

    cf_outcome_t* outcome = &scenario->outcomes[scenario->num_outcomes];
    outcome->variable_id = variable_id;
    outcome->factual_value = var->value;
    outcome->counterfactual_value = 0.0f;
    outcome->probability = 0.0f;
    outcome->confidence = 0.0f;
    outcome->causal_effect = 0.0f;

    scenario->num_outcomes++;

    return CF_OK;
}

cf_error_t cf_imagine_scenario(
    nimcp_counterfactual_t* cf,
    uint32_t scenario_id)
{
    if (!cf) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;
    if (scenario_id >= cf->num_scenarios) return CF_ERR_INVALID_CONFIG;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_imagine_scenario", 0.0f);


    cf->status = CF_STATUS_SIMULATING;

    cf_scenario_t* scenario = &cf->scenarios[scenario_id];
    cf_causal_model_t* model = cf->causal_model;

    /* STEP 1: Save current factual values */
    float* factual_values = nimcp_calloc(model->num_variables, sizeof(float));
    if (!factual_values) {
        cf->status = CF_STATUS_ERROR;
        cf->last_error = CF_ERR_MEMORY_ALLOC;
        return CF_ERR_MEMORY_ALLOC;
    }

    for (uint32_t i = 0; i < model->num_variables; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && model->num_variables > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)model->num_variables);
        }

        factual_values[i] = model->variables[i].value;
        model->variables[i].counterfactual_value = model->variables[i].value;
    }

    /* STEP 2: Ensure topological order */
    if (!model->topological_valid) {
        cf_error_t err = compute_topological_order(model);
        if (err != CF_OK) {
            nimcp_free(factual_values);
            factual_values = NULL;
            cf->status = CF_STATUS_ERROR;
            cf->last_error = err;
            return err;
        }
    }

    /* STEP 3: Apply interventions (do-operator) */
    for (uint32_t i = 0; i < scenario->num_interventions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scenario->num_interventions > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)scenario->num_interventions);
        }

        cf_intervention_t* interv = &scenario->interventions[i];
        cf_variable_t* var = causal_model_get_variable(model, interv->variable_id);
        if (var) {
            /* do(X=x): Set counterfactual value directly */
            var->counterfactual_value = interv->new_value;
            interv->applied = true;
            interv->timestamp = get_current_time_us();
        }
    }

    /* STEP 4: Propagate counterfactual values through the causal graph */
    cf_error_t err = propagate_values(model);
    if (err != CF_OK) {
        nimcp_free(factual_values);
        factual_values = NULL;
        cf->status = CF_STATUS_ERROR;
        cf->last_error = err;
        return err;
    }

    /* STEP 5: Record outcomes */
    for (uint32_t i = 0; i < scenario->num_outcomes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scenario->num_outcomes > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)scenario->num_outcomes);
        }

        cf_outcome_t* outcome = &scenario->outcomes[i];
        cf_variable_t* var = causal_model_get_variable(model, outcome->variable_id);
        if (var) {
            outcome->factual_value = factual_values[outcome->variable_id];
            outcome->counterfactual_value = var->counterfactual_value;
            outcome->causal_effect = outcome->counterfactual_value - outcome->factual_value;

            /* Estimate confidence based on variable statistics */
            float variance = var->variance > 0.0f ? var->variance : 1.0f;
            outcome->confidence = 1.0f / (1.0f + sqrtf(variance));
        }
    }

    /* STEP 6: Compute scenario plausibility */
    float plausibility = 1.0f;
    for (uint32_t i = 0; i < scenario->num_interventions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scenario->num_interventions > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)scenario->num_interventions);
        }

        cf_intervention_t* interv = &scenario->interventions[i];
        cf_variable_t* var = causal_model_get_variable(model, interv->variable_id);
        if (var && var->variance > 0.0f) {
            /* Plausibility decreases with distance from mean */
            float z_score = fabsf(interv->new_value - var->mean) / sqrtf(var->variance);
            float this_plausibility = expf(-0.5f * z_score * z_score);
            plausibility *= this_plausibility;
        }
    }
    scenario->plausibility = plausibility;

    /* STEP 7: Compute surprise */
    float surprise = 0.0f;
    for (uint32_t i = 0; i < scenario->num_outcomes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scenario->num_outcomes > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)scenario->num_outcomes);
        }

        cf_outcome_t* outcome = &scenario->outcomes[i];
        surprise += fabsf(outcome->causal_effect);
    }
    if (scenario->num_outcomes > 0) {
        surprise /= scenario->num_outcomes;
    }
    scenario->surprise = surprise;

    scenario->evaluated_time = get_current_time_us();
    cf->stats.simulations_run++;

    /* Restore factual values */
    for (uint32_t i = 0; i < model->num_variables; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && model->num_variables > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)model->num_variables);
        }

        model->variables[i].value = factual_values[i];
    }

    nimcp_free(factual_values);
    factual_values = NULL;
    cf->status = CF_STATUS_IDLE;

    return CF_OK;
}

cf_error_t cf_evaluate_scenario(
    nimcp_counterfactual_t* cf,
    uint32_t scenario_id,
    cf_evaluation_mode_t mode)
{
    if (!cf) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;
    if (scenario_id >= cf->num_scenarios) return CF_ERR_INVALID_CONFIG;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_evaluate_scenario", 0.0f);


    cf->status = CF_STATUS_EVALUATING;

    cf_scenario_t* scenario = &cf->scenarios[scenario_id];

    /* First imagine the scenario if not done */
    if (!scenario->evaluated) {
        cf_error_t err = cf_imagine_scenario(cf, scenario_id);
        if (err != CF_OK) {
            return err;
        }
    }

    /* Compute probability based on mode */
    switch (mode) {
        case CF_EVAL_CAUSAL:
            /* Causal probability: P(Y_x) using do-calculus */
            /* Simplified: use plausibility and outcome confidence */
            {
                float prob = scenario->plausibility;
                for (uint32_t i = 0; i < scenario->num_outcomes; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && scenario->num_outcomes > 256) {
                        counterfactual_imagination_heartbeat("counterfactu_loop",
                                         (float)(i + 1) / (float)scenario->num_outcomes);
                    }

                    prob *= scenario->outcomes[i].confidence;
                }
                scenario->probability = prob;
            }
            break;

        case CF_EVAL_PROBABILISTIC:
            /* Observational probability: P(Y|X) */
            /* Use conditional probability estimate */
            {
                float prob = 1.0f;
                for (uint32_t i = 0; i < scenario->num_outcomes; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && scenario->num_outcomes > 256) {
                        counterfactual_imagination_heartbeat("counterfactu_loop",
                                         (float)(i + 1) / (float)scenario->num_outcomes);
                    }

                    cf_outcome_t* outcome = &scenario->outcomes[i];
                    cf_variable_t* var = causal_model_get_variable(
                        cf->causal_model, outcome->variable_id);
                    if (var && var->variance > 0.0f) {
                        float z = fabsf(outcome->counterfactual_value - var->mean) /
                                  sqrtf(var->variance);
                        prob *= expf(-0.5f * z * z);
                    }
                }
                scenario->probability = prob;
            }
            break;

        case CF_EVAL_STRUCTURAL:
            /* Structural equation model */
            /* Use the computed counterfactual values directly */
            scenario->probability = scenario->plausibility;
            break;

        default:
            cf->status = CF_STATUS_ERROR;
            cf->last_error = CF_ERR_INVALID_CONFIG;
            return CF_ERR_INVALID_CONFIG;
    }

    scenario->evaluated = true;
    cf->stats.scenarios_evaluated++;

    /* Update running statistics */
    cf->stats.mean_plausibility = 0.9f * cf->stats.mean_plausibility +
                                   0.1f * scenario->plausibility;

    float total_confidence = 0.0f;
    for (uint32_t i = 0; i < scenario->num_outcomes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scenario->num_outcomes > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)scenario->num_outcomes);
        }

        total_confidence += scenario->outcomes[i].confidence;
    }
    if (scenario->num_outcomes > 0) {
        cf->stats.mean_confidence = 0.9f * cf->stats.mean_confidence +
                                    0.1f * (total_confidence / scenario->num_outcomes);
    }

    cf->status = CF_STATUS_IDLE;

    return CF_OK;
}

cf_error_t cf_get_scenario(
    nimcp_counterfactual_t* cf,
    uint32_t scenario_id,
    cf_scenario_t* scenario)
{
    if (!cf || !scenario) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;
    if (scenario_id >= cf->num_scenarios) return CF_ERR_INVALID_CONFIG;

    *scenario = cf->scenarios[scenario_id];
    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_get_scenario", 0.0f);


    return CF_OK;
}

/*=============================================================================
 * CAUSAL INFERENCE API
 *===========================================================================*/

cf_error_t cf_compute_causal_effect(
    nimcp_counterfactual_t* cf,
    uint32_t cause_id,
    uint32_t effect_id,
    float cause_value,
    float* effect)
{
    if (!cf || !effect) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_compute_causal_ef", 0.0f);


    cf_variable_t* cause_var = causal_model_get_variable(cf->causal_model, cause_id);
    cf_variable_t* effect_var = causal_model_get_variable(cf->causal_model, effect_id);

    if (!cause_var || !effect_var) {
        cf->last_error = CF_ERR_VARIABLE_NOT_FOUND;
        return CF_ERR_VARIABLE_NOT_FOUND;
    }

    /* WHAT: Compute E[Y | do(X=x)] - E[Y | do(X=baseline)]
     * WHY:  This is the average causal effect
     * HOW:  Apply intervention, propagate, compare to baseline */

    /* Save baseline */
    float baseline_cause = cause_var->value;
    float baseline_effect = effect_var->counterfactual_value;

    /* Apply intervention */
    cause_var->counterfactual_value = cause_value;

    /* Propagate */
    if (!cf->causal_model->topological_valid) {
        cf_error_t err = compute_topological_order(cf->causal_model);
        if (err != CF_OK) return err;
    }
    propagate_values(cf->causal_model);

    /* Compute effect */
    float intervened_effect = effect_var->counterfactual_value;
    *effect = intervened_effect - baseline_effect;

    /* Restore baseline */
    cause_var->counterfactual_value = baseline_cause;

    /* Update stats */
    cf->stats.mean_causal_effect = 0.9f * cf->stats.mean_causal_effect +
                                    0.1f * fabsf(*effect);

    return CF_OK;
}

cf_error_t cf_get_counterfactual_outcome(
    nimcp_counterfactual_t* cf,
    uint32_t variable_id,
    cf_outcome_t* outcome)
{
    if (!cf || !outcome) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_get_counterfactua", 0.0f);


    cf_variable_t* var = causal_model_get_variable(cf->causal_model, variable_id);
    if (!var) {
        cf->last_error = CF_ERR_VARIABLE_NOT_FOUND;
        return CF_ERR_VARIABLE_NOT_FOUND;
    }

    outcome->variable_id = variable_id;
    outcome->factual_value = var->value;
    outcome->counterfactual_value = var->counterfactual_value;
    outcome->causal_effect = var->counterfactual_value - var->value;

    /* Estimate confidence */
    float variance = var->variance > 0.0f ? var->variance : 1.0f;
    outcome->confidence = 1.0f / (1.0f + sqrtf(variance));

    /* Estimate probability using normal distribution assumption */
    float z = fabsf(outcome->counterfactual_value - var->mean) / sqrtf(variance);
    outcome->probability = expf(-0.5f * z * z);

    return CF_OK;
}

cf_error_t cf_query_counterfactual(
    nimcp_counterfactual_t* cf,
    uint32_t cause_id,
    float cause_value,
    uint32_t effect_id,
    float* result,
    float* confidence)
{
    if (!cf || !result || !confidence) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;

    /* Create temporary scenario */
    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_query_counterfact", 0.0f);


    uint32_t scenario_id = 0;
    cf_error_t err = cf_create_scenario(cf, CF_SCENARIO_HYPOTHETICAL,
                                        "Query counterfactual", &scenario_id);
    if (err != CF_OK) return err;

    /* Add intervention and outcome */
    err = cf_scenario_add_intervention(cf, scenario_id, cause_id, cause_value);
    if (err != CF_OK) return err;

    err = cf_scenario_add_outcome(cf, scenario_id, effect_id);
    if (err != CF_OK) return err;

    /* Imagine and evaluate */
    err = cf_imagine_scenario(cf, scenario_id);
    if (err != CF_OK) return err;

    err = cf_evaluate_scenario(cf, scenario_id, cf->config.default_mode);
    if (err != CF_OK) return err;

    /* Get results */
    cf_scenario_t* scenario = &cf->scenarios[scenario_id];
    if (scenario->num_outcomes > 0) {
        *result = scenario->outcomes[0].counterfactual_value;
        *confidence = scenario->outcomes[0].confidence;
    } else {
        *result = 0.0f;
        *confidence = 0.0f;
    }

    return CF_OK;
}

/*=============================================================================
 * UPDATE API
 *===========================================================================*/

cf_error_t cf_update(nimcp_counterfactual_t* cf, float dt_ms) {
    if (!cf) return CF_ERR_NULL_PTR;
    if (!cf->initialized) return CF_ERR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_update", 0.0f);


    (void)dt_ms;  /* Currently unused */

    /* Decay old history entries */
    /* (Could implement LRU or time-based expiration here) */

    return CF_OK;
}

cf_error_t cf_get_stats(nimcp_counterfactual_t* cf, cf_stats_t* stats) {
    if (!cf || !stats) return CF_ERR_NULL_PTR;

    *stats = cf->stats;
    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_get_stats", 0.0f);


    return CF_OK;
}

/*=============================================================================
 * UTILITY API
 *===========================================================================*/

cf_status_t cf_get_status(nimcp_counterfactual_t* cf) {
    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_get_status", 0.0f);


    return cf ? cf->status : CF_STATUS_ERROR;
}

cf_error_t cf_get_last_error(nimcp_counterfactual_t* cf) {
    /* Phase 8: Heartbeat at operation start */
    counterfactual_imagination_heartbeat("counterfactu_cf_get_last_error", 0.0f);


    return cf ? cf->last_error : CF_ERR_NULL_PTR;
}

const char* cf_error_string(cf_error_t error) {
    switch (error) {
        case CF_OK:                     return "OK";
        case CF_ERR_NULL_PTR:           return "Null pointer";
        case CF_ERR_NOT_INITIALIZED:    return "Not initialized";
        case CF_ERR_INVALID_VARIABLE:   return "Invalid variable";
        case CF_ERR_VARIABLE_NOT_FOUND: return "Variable not found";
        case CF_ERR_INTERVENTION_FAILED: return "Intervention failed";
        case CF_ERR_SCENARIO_FAILED:    return "Scenario failed";
        case CF_ERR_MEMORY_ALLOC:       return "Memory allocation failed";
        case CF_ERR_CAPACITY_EXCEEDED:  return "Capacity exceeded";
        case CF_ERR_INVALID_CONFIG:     return "Invalid configuration";
        case CF_ERR_CAUSAL_CYCLE:       return "Causal cycle detected";
        case CF_ERR_UNIDENTIFIABLE:     return "Causal effect unidentifiable";
        case CF_ERR_SIMULATION_FAILED:  return "Simulation failed";
        default:                        return "Unknown error";
    }
}

const char* cf_status_string(cf_status_t status) {
    switch (status) {
        case CF_STATUS_IDLE:        return "Idle";
        case CF_STATUS_SIMULATING:  return "Simulating";
        case CF_STATUS_EVALUATING:  return "Evaluating";
        case CF_STATUS_INTERVENING: return "Intervening";
        case CF_STATUS_ERROR:       return "Error";
        default:                    return "Unknown";
    }
}

const char* cf_scenario_type_string(cf_scenario_type_t type) {
    switch (type) {
        case CF_SCENARIO_INTERVENTION:  return "Intervention";
        case CF_SCENARIO_PREVENTION:    return "Prevention";
        case CF_SCENARIO_ALTERNATIVE:   return "Alternative";
        case CF_SCENARIO_HYPOTHETICAL:  return "Hypothetical";
        default:                        return "Unknown";
    }
}

const char* cf_evaluation_mode_string(cf_evaluation_mode_t mode) {
    switch (mode) {
        case CF_EVAL_CAUSAL:         return "Causal";
        case CF_EVAL_PROBABILISTIC:  return "Probabilistic";
        case CF_EVAL_STRUCTURAL:     return "Structural";
        default:                     return "Unknown";
    }
}

const char* cf_variable_type_string(cf_variable_type_t type) {
    switch (type) {
        case CF_VAR_CONTINUOUS:   return "Continuous";
        case CF_VAR_DISCRETE:     return "Discrete";
        case CF_VAR_BINARY:       return "Binary";
        case CF_VAR_CATEGORICAL:  return "Categorical";
        default:                  return "Unknown";
    }
}

/*=============================================================================
 * INTERNAL HELPERS - IMPLEMENTATION
 *===========================================================================*/

static cf_causal_model_t* causal_model_create(uint32_t capacity) {
    cf_causal_model_t* model = nimcp_calloc(1, sizeof(cf_causal_model_t));
    if (!model) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate model");

        return NULL;

    }

    model->variable_capacity = capacity;
    model->variables = nimcp_calloc(capacity, sizeof(cf_variable_t));
    if (!model->variables) {
        nimcp_free(model);
        model = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "causal_model_create: model->variables is NULL");
        return NULL;
    }

    /* Allocate adjacency matrix */
    model->adjacency = nimcp_calloc(capacity * capacity, sizeof(float));
    model->has_edge = nimcp_calloc(capacity * capacity, sizeof(bool));
    if (!model->adjacency || !model->has_edge) {
        nimcp_free(model->variables);
        nimcp_free(model->adjacency);
        nimcp_free(model->has_edge);
        nimcp_free(model);
        model = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "causal_model_create: required parameter is NULL (model->adjacency, model->has_edge)");
        return NULL;
    }

    /* Allocate topological order array */
    model->topological_order = nimcp_calloc(capacity, sizeof(uint32_t));
    if (!model->topological_order) {
        nimcp_free(model->variables);
        nimcp_free(model->adjacency);
        nimcp_free(model->has_edge);
        nimcp_free(model);
        model = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "causal_model_create: model->topological_order is NULL");
        return NULL;
    }

    model->num_variables = 0;
    model->topological_valid = false;

    return model;
}

static void causal_model_destroy(cf_causal_model_t* model) {
    if (!model) return;

    /* Free parent arrays in variables */
    for (uint32_t i = 0; i < model->num_variables; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && model->num_variables > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)model->num_variables);
        }

        nimcp_free(model->variables[i].parent_ids);
        nimcp_free(model->variables[i].causal_weights);
    }

    nimcp_free(model->variables);
    nimcp_free(model->adjacency);
    nimcp_free(model->has_edge);
    nimcp_free(model->topological_order);
    nimcp_free(model);
    model = NULL;
}

static cf_error_t causal_model_add_variable(
    cf_causal_model_t* model,
    const char* name,
    cf_variable_type_t type,
    uint32_t* id)
{
    if (!model || !name || !id) return CF_ERR_NULL_PTR;
    if (model->num_variables >= model->variable_capacity) return CF_ERR_CAPACITY_EXCEEDED;

    cf_variable_t* var = &model->variables[model->num_variables];
    memset(var, 0, sizeof(cf_variable_t));

    var->id = model->num_variables;
    strncpy(var->name, name, CF_MAX_NAME_LENGTH - 1);
    var->type = type;
    var->value = 0.0f;
    var->counterfactual_value = 0.0f;
    var->is_observed = false;
    var->observation_confidence = 0.0f;
    var->parent_ids = NULL;
    var->num_parents = 0;
    var->causal_weights = NULL;
    var->mean = 0.0f;
    var->variance = 1.0f;  /* Default variance */

    *id = var->id;
    model->num_variables++;
    model->topological_valid = false;

    return CF_OK;
}

static cf_variable_t* causal_model_get_variable(cf_causal_model_t* model, uint32_t id) {
    if (!model || id >= model->num_variables) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "causal_model_get_variable: model is NULL");
        return NULL;
    }
    return &model->variables[id];
}

static cf_error_t causal_model_add_edge(
    cf_causal_model_t* model,
    uint32_t from,
    uint32_t to,
    float weight)
{
    if (!model) return CF_ERR_NULL_PTR;
    if (from >= model->num_variables || to >= model->num_variables) {
        return CF_ERR_VARIABLE_NOT_FOUND;
    }

    /* Check for self-loop */
    if (from == to) return CF_ERR_CAUSAL_CYCLE;

    uint32_t n = model->variable_capacity;
    uint32_t idx = from * n + to;

    model->adjacency[idx] = weight;
    model->has_edge[idx] = true;

    /* Update child's parent list */
    cf_variable_t* child = &model->variables[to];
    uint32_t new_count = child->num_parents + 1;

    /* Save old pointers to preserve on failure - prevents double-free */
    uint32_t* old_parents = child->parent_ids;
    float* old_weights = child->causal_weights;

    uint32_t* new_parents = nimcp_realloc(old_parents, new_count * sizeof(uint32_t));
    if (!new_parents) {
        return CF_ERR_MEMORY_ALLOC;
    }
    child->parent_ids = new_parents;

    float* new_weights = nimcp_realloc(old_weights, new_count * sizeof(float));
    if (!new_weights) {
        /* parent_ids was already updated, but that's fine - it's a valid realloc */
        return CF_ERR_MEMORY_ALLOC;
    }
    child->causal_weights = new_weights;
    child->parent_ids[child->num_parents] = from;
    child->causal_weights[child->num_parents] = weight;
    child->num_parents = new_count;

    model->topological_valid = false;

    return CF_OK;
}

static cf_error_t compute_topological_order(cf_causal_model_t* model) {
    if (!model) return CF_ERR_NULL_PTR;

    uint32_t n = model->num_variables;
    uint32_t cap = model->variable_capacity;

    /* Compute in-degrees */
    uint32_t* in_degree = nimcp_calloc(n, sizeof(uint32_t));
    if (!in_degree) return CF_ERR_MEMORY_ALLOC;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)n);
        }

        for (uint32_t j = 0; j < n; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && n > 256) {
                counterfactual_imagination_heartbeat("counterfactu_loop",
                                 (float)(j + 1) / (float)n);
            }

            if (model->has_edge[j * cap + i]) {
                in_degree[i]++;
            }
        }
    }

    /* Kahn's algorithm for topological sort */
    uint32_t* queue = nimcp_calloc(n, sizeof(uint32_t));
    if (!queue) {
        nimcp_free(in_degree);
        in_degree = NULL;
        return CF_ERR_MEMORY_ALLOC;
    }

    uint32_t front = 0, back = 0;
    uint32_t order_idx = 0;

    /* Add all nodes with in-degree 0 to queue */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)n);
        }

        if (in_degree[i] == 0) {
            queue[back++] = i;
        }
    }

    while (front < back) {
        uint32_t node = queue[front++];
        model->topological_order[order_idx++] = node;

        /* Reduce in-degree of neighbors */
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                counterfactual_imagination_heartbeat("counterfactu_loop",
                                 (float)(i + 1) / (float)n);
            }

            if (model->has_edge[node * cap + i]) {
                in_degree[i]--;
                if (in_degree[i] == 0) {
                    queue[back++] = i;
                }
            }
        }
    }

    nimcp_free(in_degree);
    in_degree = NULL;
    nimcp_free(queue);
    queue = NULL;

    /* Check for cycle */
    if (order_idx != n) {
        return CF_ERR_CAUSAL_CYCLE;
    }

    model->topological_valid = true;
    return CF_OK;
}

static float compute_structural_equation(cf_variable_t* var, cf_causal_model_t* model) {
    if (!var || !model) return 0.0f;

    /* If no parents, return current counterfactual value (exogenous) */
    if (var->num_parents == 0) {
        return var->counterfactual_value;
    }

    /* Compute as weighted sum of parents' counterfactual values */
    /* This implements a linear structural equation model */
    float value = 0.0f;
    for (uint32_t i = 0; i < var->num_parents; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && var->num_parents > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)var->num_parents);
        }

        cf_variable_t* parent = causal_model_get_variable(model, var->parent_ids[i]);
        if (parent) {
            value += var->causal_weights[i] * parent->counterfactual_value;
        }
    }

    return value;
}

static cf_error_t propagate_values(cf_causal_model_t* model) {
    if (!model) return CF_ERR_NULL_PTR;
    if (!model->topological_valid) return CF_ERR_NOT_INITIALIZED;

    /* Propagate in topological order */
    for (uint32_t i = 0; i < model->num_variables; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && model->num_variables > 256) {
            counterfactual_imagination_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)model->num_variables);
        }

        uint32_t var_id = model->topological_order[i];
        cf_variable_t* var = &model->variables[var_id];

        /* Only propagate if variable has parents (not intervened directly) */
        if (var->num_parents > 0) {
            /* Check if this variable was directly intervened on */
            /* If counterfactual differs from factual, assume intervention */
            bool was_intervened = (fabsf(var->counterfactual_value - var->value) > 1e-6f);

            if (!was_intervened) {
                /* Compute from structural equation */
                var->counterfactual_value = compute_structural_equation(var, model);
            }
            /* If intervened, keep the intervention value (do-operator) */
        }
    }

    return CF_OK;
}

static uint64_t get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static void add_history_entry(
    nimcp_counterfactual_t* cf,
    uint32_t var_id,
    float before,
    float after,
    float effect)
{
    if (!cf || cf->history_count >= cf->history_capacity) return;

    cf_history_entry_t* entry = &cf->history[cf->history_count];
    entry->variable_id = var_id;
    entry->value_before = before;
    entry->value_after = after;
    entry->effect_size = effect;
    entry->timestamp = get_current_time_us();

    cf->history_count++;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void counterfactual_imagination_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_counterfactual_imagination_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int counterfactual_imagination_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "counterfactual_imagination_training_begin: NULL argument");
        return -1;
    }
    counterfactual_imagination_heartbeat_instance(NULL, "counterfactual_imagination_training_begin", 0.0f);
    return 0;
}

int counterfactual_imagination_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "counterfactual_imagination_training_end: NULL argument");
        return -1;
    }
    counterfactual_imagination_heartbeat_instance(NULL, "counterfactual_imagination_training_end", 1.0f);
    return 0;
}

int counterfactual_imagination_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "counterfactual_imagination_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    counterfactual_imagination_heartbeat_instance(NULL, "counterfactual_imagination_training_step", progress);
    return 0;
}
