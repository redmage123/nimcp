//=============================================================================
// nimcp_bg_outcome_devaluation.c - Outcome Devaluation for Goal/Habit Testing
//=============================================================================

#include "core/brain/subcortical/nimcp_bg_outcome_devaluation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bg_outcome_devaluation, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

struct bg_outcome_deval {
    /* Configuration */
    bgod_config_t config;

    /* Outcomes */
    bgod_outcome_t* outcomes;
    uint32_t num_outcomes;

    /* Action-outcome associations */
    bgod_action_outcome_t* associations;
    uint32_t num_associations;

    /* Response tracking */
    uint32_t* action_response_counts;
    float* action_response_rates;

    /* Devaluation test history */
    bgod_test_result_t* test_history;
    uint32_t num_tests;
    uint32_t max_tests;

    /* Overall behavior classification */
    bgod_behavior_type_t dominant_behavior;
    float goal_weight;
    float habit_weight;

    /* Statistics */
    bgod_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * LIFECYCLE IMPLEMENTATION
 * ============================================================================ */

void bgod_default_config(bgod_config_t* config) {
    if (!config) return;

    config->max_outcomes = BGOD_MAX_OUTCOMES;
    config->max_actions = BGOD_MAX_ACTIONS;

    config->satiety_rate = 0.1f;
    config->satiety_decay = BGOD_SATIETY_DECAY_RATE;
    config->aversion_learning_rate = 0.5f;
    config->aversion_extinction_rate = 0.01f;

    config->goal_threshold = 0.3f;      /* > 30% reduction = goal-directed */
    config->habit_threshold = 0.1f;     /* < 10% reduction = habitual */

    config->enable_revaluation = true;
    config->track_sensitivity = true;
}

bg_outcome_deval_t* bgod_create(const bgod_config_t* config) {
    bg_outcome_deval_t* deval = nimcp_calloc(1, sizeof(bg_outcome_deval_t));
    if (!deval) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "deval is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        deval->config = *config;
    } else {
        bgod_default_config(&deval->config);
    }

    /* Allocate outcomes */
    deval->outcomes = nimcp_calloc(deval->config.max_outcomes, sizeof(bgod_outcome_t));
    if (!deval->outcomes) goto cleanup;

    /* Allocate associations */
    uint32_t max_assoc = deval->config.max_outcomes * deval->config.max_actions;
    deval->associations = nimcp_calloc(max_assoc, sizeof(bgod_action_outcome_t));
    if (!deval->associations) goto cleanup;

    /* Allocate response tracking */
    deval->action_response_counts = nimcp_calloc(deval->config.max_actions, sizeof(uint32_t));
    deval->action_response_rates = nimcp_calloc(deval->config.max_actions, sizeof(float));
    if (!deval->action_response_counts || !deval->action_response_rates) goto cleanup;

    /* Allocate test history */
    deval->max_tests = 100;
    deval->test_history = nimcp_calloc(deval->max_tests, sizeof(bgod_test_result_t));
    if (!deval->test_history) goto cleanup;

    /* Initialize state */
    deval->num_outcomes = 0;
    deval->num_associations = 0;
    deval->num_tests = 0;
    deval->dominant_behavior = BGOD_BEHAVIOR_UNKNOWN;
    deval->goal_weight = 0.5f;
    deval->habit_weight = 0.5f;

    /* Create mutex */
    deval->mutex = nimcp_mutex_create(NULL);
    if (!deval->mutex) goto cleanup;

    return deval;

cleanup:
    bgod_destroy(deval);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bgod_create: allocation failed");
    return NULL;
}

void bgod_destroy(bg_outcome_deval_t* deval) {
    if (!deval) return;

    if (deval->mutex) {
        nimcp_mutex_free(deval->mutex);
    }

    nimcp_free(deval->outcomes);
    nimcp_free(deval->associations);
    nimcp_free(deval->action_response_counts);
    nimcp_free(deval->action_response_rates);
    nimcp_free(deval->test_history);
    nimcp_free(deval);
}

int bgod_reset(bg_outcome_deval_t* deval) {
    if (!deval) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgod_reset: deval is NULL");
        return -1;
    }

    nimcp_mutex_lock(deval->mutex);

    /* Reset outcomes */
    for (uint32_t i = 0; i < deval->num_outcomes; i++) {
        deval->outcomes[i].current_value = deval->outcomes[i].base_value;
        deval->outcomes[i].satiety_level = 0.0f;
        deval->outcomes[i].aversion_level = 0.0f;
        deval->outcomes[i].state = BGOD_OUTCOME_VALUED;
        deval->outcomes[i].consumption_count = 0;
    }

    /* Reset associations */
    for (uint32_t i = 0; i < deval->num_associations; i++) {
        deval->associations[i].current_rate = deval->associations[i].baseline_rate;
    }

    /* Reset response tracking */
    memset(deval->action_response_counts, 0, deval->config.max_actions * sizeof(uint32_t));
    memset(deval->action_response_rates, 0, deval->config.max_actions * sizeof(float));

    /* Reset test history */
    deval->num_tests = 0;

    /* Reset behavior classification */
    deval->dominant_behavior = BGOD_BEHAVIOR_UNKNOWN;
    deval->goal_weight = 0.5f;
    deval->habit_weight = 0.5f;

    /* Reset statistics */
    memset(&deval->stats, 0, sizeof(bgod_stats_t));

    nimcp_mutex_unlock(deval->mutex);
    return 0;
}

/* ============================================================================
 * OUTCOME MANAGEMENT IMPLEMENTATION
 * ============================================================================ */

int bgod_register_outcome(bg_outcome_deval_t* deval,
                           const bgod_outcome_t* outcome,
                           uint32_t* out_id) {
    if (!deval || !outcome) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgod_register_outcome: required parameter is NULL (deval, outcome)");
        return -1;
    }

    nimcp_mutex_lock(deval->mutex);

    if (deval->num_outcomes >= deval->config.max_outcomes) {
        nimcp_mutex_unlock(deval->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bgod_register_outcome: capacity exceeded");
        return -1;
    }

    uint32_t id = deval->num_outcomes;
    deval->outcomes[id] = *outcome;
    deval->outcomes[id].id = id;
    deval->outcomes[id].current_value = outcome->base_value;
    deval->outcomes[id].state = BGOD_OUTCOME_VALUED;
    deval->num_outcomes++;

    deval->stats.total_outcomes = deval->num_outcomes;

    if (out_id) *out_id = id;

    nimcp_mutex_unlock(deval->mutex);
    return 0;
}

int bgod_set_outcome_value(bg_outcome_deval_t* deval,
                            uint32_t outcome_id,
                            float value) {
    if (!deval || outcome_id >= deval->num_outcomes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgod_set_outcome_value: invalid parameters");
        return -1;
    }

    nimcp_mutex_lock(deval->mutex);
    deval->outcomes[outcome_id].base_value = value;
    deval->outcomes[outcome_id].current_value = value;
    nimcp_mutex_unlock(deval->mutex);
    return 0;
}

float bgod_get_outcome_value(const bg_outcome_deval_t* deval,
                              uint32_t outcome_id) {
    if (!deval || outcome_id >= deval->num_outcomes) return 0.0f;
    return deval->outcomes[outcome_id].current_value;
}

int bgod_register_association(bg_outcome_deval_t* deval,
                               uint32_t action_id,
                               uint32_t outcome_id,
                               float strength) {
    if (!deval) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgod_register_association: deval is NULL");
        return -1;
    }
    if (outcome_id >= deval->num_outcomes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgod_register_association: outcome_id out of range");
        return -1;
    }
    if (action_id >= deval->config.max_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bgod_register_association: action_id out of range");
        return -1;
    }

    nimcp_mutex_lock(deval->mutex);

    uint32_t max_assoc = deval->config.max_outcomes * deval->config.max_actions;
    if (deval->num_associations >= max_assoc) {
        nimcp_mutex_unlock(deval->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bgod_register_association: associations capacity exceeded");
        return -1;
    }

    bgod_action_outcome_t* assoc = &deval->associations[deval->num_associations];
    assoc->action_id = action_id;
    assoc->outcome_id = outcome_id;
    assoc->association_strength = nimcp_clampf(strength, 0.0f, 1.0f);
    assoc->baseline_rate = 1.0f;
    assoc->current_rate = 1.0f;
    assoc->training_trials = 0;

    deval->num_associations++;

    nimcp_mutex_unlock(deval->mutex);
    return 0;
}

/* ============================================================================
 * DEVALUATION IMPLEMENTATION
 * ============================================================================ */

int bgod_devalue_by_satiety(bg_outcome_deval_t* deval,
                             uint32_t outcome_id,
                             float consumption_amount) {
    if (!deval || outcome_id >= deval->num_outcomes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgod_devalue_by_satiety: invalid parameters");
        return -1;
    }

    nimcp_mutex_lock(deval->mutex);

    bgod_outcome_t* outcome = &deval->outcomes[outcome_id];

    /* Increase satiety */
    outcome->satiety_level += consumption_amount * deval->config.satiety_rate;
    outcome->satiety_level = nimcp_clampf(outcome->satiety_level, 0.0f, 1.0f);

    /* Reduce current value based on satiety */
    outcome->current_value = outcome->base_value * (1.0f - outcome->satiety_level);
    outcome->state = BGOD_OUTCOME_DEVALUED;

    outcome->consumption_count++;
    deval->stats.devalued_outcomes++;

    nimcp_mutex_unlock(deval->mutex);
    return 0;
}

int bgod_devalue_by_aversion(bg_outcome_deval_t* deval,
                              uint32_t outcome_id,
                              float aversion_strength) {
    if (!deval || outcome_id >= deval->num_outcomes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgod_devalue_by_aversion: invalid parameters");
        return -1;
    }

    nimcp_mutex_lock(deval->mutex);

    bgod_outcome_t* outcome = &deval->outcomes[outcome_id];

    /* Set aversion level */
    outcome->aversion_level = nimcp_clampf(aversion_strength, 0.0f, BGOD_AVERSION_STRENGTH);

    /* Reduce current value based on aversion */
    outcome->current_value = outcome->base_value * (1.0f - outcome->aversion_level);
    outcome->state = BGOD_OUTCOME_DEVALUED;

    deval->stats.devalued_outcomes++;

    nimcp_mutex_unlock(deval->mutex);
    return 0;
}

int bgod_revalue_outcome(bg_outcome_deval_t* deval, uint32_t outcome_id) {
    if (!deval || outcome_id >= deval->num_outcomes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgod_revalue_outcome: deval is NULL");
        return -1;
    }
    if (!deval->config.enable_revaluation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgod_revalue_outcome: deval->config is NULL");
        return -1;
    }

    nimcp_mutex_lock(deval->mutex);

    bgod_outcome_t* outcome = &deval->outcomes[outcome_id];

    /* Restore value */
    outcome->satiety_level = 0.0f;
    outcome->aversion_level = 0.0f;
    outcome->current_value = outcome->base_value;
    outcome->state = BGOD_OUTCOME_REVALUED;

    nimcp_mutex_unlock(deval->mutex);
    return 0;
}

float bgod_get_action_value(const bg_outcome_deval_t* deval,
                             uint32_t action_id) {
    if (!deval || action_id >= deval->config.max_actions) return 0.0f;

    float value = 0.0f;

    /* Sum value of all outcomes associated with this action */
    for (uint32_t i = 0; i < deval->num_associations; i++) {
        if (deval->associations[i].action_id == action_id) {
            uint32_t outcome_id = deval->associations[i].outcome_id;
            float outcome_value = deval->outcomes[outcome_id].current_value;
            float assoc_strength = deval->associations[i].association_strength;
            value += outcome_value * assoc_strength;
        }
    }

    return value;
}

/* ============================================================================
 * TESTING IMPLEMENTATION
 * ============================================================================ */

int bgod_run_test(bg_outcome_deval_t* deval,
                   uint32_t action_id,
                   uint32_t outcome_id,
                   bgod_method_t method,
                   bgod_test_result_t* result) {
    if (!deval || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgod_revalue_outcome: required parameter is NULL (deval, result)");
        return -1;
    }
    if (action_id >= deval->config.max_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bgod_revalue_outcome: capacity exceeded");
        return -1;
    }
    if (outcome_id >= deval->num_outcomes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgod_revalue_outcome: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(deval->mutex);

    /* Find association */
    bgod_action_outcome_t* assoc = NULL;
    for (uint32_t i = 0; i < deval->num_associations; i++) {
        if (deval->associations[i].action_id == action_id &&
            deval->associations[i].outcome_id == outcome_id) {
            assoc = &deval->associations[i];
            break;
        }
    }

    if (!assoc) {
        nimcp_mutex_unlock(deval->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgod_revalue_outcome: assoc is NULL");
        return -1;
    }

    /* Record pre-devaluation rate */
    result->pre_rate = assoc->baseline_rate;

    /* Apply devaluation based on method */
    switch (method) {
        case BGOD_METHOD_SATIETY:
            bgod_devalue_by_satiety(deval, outcome_id, 1.0f);
            break;
        case BGOD_METHOD_AVERSION:
            bgod_devalue_by_aversion(deval, outcome_id, BGOD_AVERSION_STRENGTH);
            break;
        default:
            break;
    }

    /* Compute post-devaluation rate */
    /* Goal-directed: rate decreases proportionally to value decrease */
    /* Habitual: rate stays same despite value decrease */
    float value_ratio = deval->outcomes[outcome_id].current_value /
                        (deval->outcomes[outcome_id].base_value + 0.001f);

    /* Mix of goal-directed and habitual */
    float goal_rate = result->pre_rate * value_ratio;
    float habit_rate = result->pre_rate;

    result->post_rate = deval->goal_weight * goal_rate +
                        deval->habit_weight * habit_rate;

    assoc->current_rate = result->post_rate;

    /* Compute sensitivity */
    result->ratio = result->post_rate / (result->pre_rate + 0.001f);
    result->sensitivity = 1.0f - result->ratio;

    /* Classify behavior */
    result->behavior = bgod_classify_behavior(result);
    result->outcome_id = outcome_id;
    result->method = method;

    /* Store in history */
    if (deval->num_tests < deval->max_tests) {
        deval->test_history[deval->num_tests] = *result;
        deval->num_tests++;
    }

    deval->stats.tests_performed = deval->num_tests;

    nimcp_mutex_unlock(deval->mutex);
    return 0;
}

bgod_behavior_type_t bgod_classify_behavior(const bgod_test_result_t* result) {
    if (!result) return BGOD_BEHAVIOR_UNKNOWN;

    float reduction = 1.0f - result->ratio;

    if (reduction > 0.3f) {
        return BGOD_BEHAVIOR_GOAL_DIRECTED;
    } else if (reduction < 0.1f) {
        return BGOD_BEHAVIOR_HABITUAL;
    } else {
        return BGOD_BEHAVIOR_MIXED;
    }
}

bgod_behavior_type_t bgod_get_overall_behavior(const bg_outcome_deval_t* deval) {
    if (!deval) return BGOD_BEHAVIOR_UNKNOWN;
    return deval->dominant_behavior;
}

float bgod_get_sensitivity(const bg_outcome_deval_t* deval,
                            uint32_t action_id) {
    if (!deval || action_id >= deval->config.max_actions) return 0.0f;

    /* Compute mean sensitivity across all tests for this action */
    float sum = 0.0f;
    uint32_t count = 0;

    for (uint32_t i = 0; i < deval->num_tests; i++) {
        /* Find tests related to associations with this action */
        for (uint32_t j = 0; j < deval->num_associations; j++) {
            if (deval->associations[j].action_id == action_id &&
                deval->associations[j].outcome_id == deval->test_history[i].outcome_id) {
                sum += deval->test_history[i].sensitivity;
                count++;
                break;
            }
        }
    }

    return (count > 0) ? sum / count : 0.0f;
}

/* ============================================================================
 * PROCESSING IMPLEMENTATION
 * ============================================================================ */

int bgod_step(bg_outcome_deval_t* deval, float dt_ms) {
    if (!deval || dt_ms <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgod_step: deval is NULL");
        return -1;
    }

    nimcp_mutex_lock(deval->mutex);

    float decay_factor = 1.0f - deval->config.satiety_decay * dt_ms / 1000.0f;

    /* Decay satiety and aversion over time */
    for (uint32_t i = 0; i < deval->num_outcomes; i++) {
        bgod_outcome_t* outcome = &deval->outcomes[i];

        /* Satiety decays */
        outcome->satiety_level *= decay_factor;

        /* Aversion decays slower */
        outcome->aversion_level *= (1.0f - deval->config.aversion_extinction_rate * dt_ms / 1000.0f);

        /* Update current value */
        float devaluation = outcome->satiety_level + outcome->aversion_level;
        devaluation = nimcp_clampf(devaluation, 0.0f, 1.0f);
        outcome->current_value = outcome->base_value * (1.0f - devaluation);

        /* Update state */
        if (devaluation < 0.05f) {
            outcome->state = BGOD_OUTCOME_VALUED;
        }
    }

    /* Update dominant behavior based on test history */
    if (deval->num_tests > 0) {
        float goal_count = 0.0f;
        float habit_count = 0.0f;

        for (uint32_t i = 0; i < deval->num_tests; i++) {
            if (deval->test_history[i].behavior == BGOD_BEHAVIOR_GOAL_DIRECTED) {
                goal_count++;
            } else if (deval->test_history[i].behavior == BGOD_BEHAVIOR_HABITUAL) {
                habit_count++;
            }
        }

        if (goal_count > habit_count * 2) {
            deval->dominant_behavior = BGOD_BEHAVIOR_GOAL_DIRECTED;
        } else if (habit_count > goal_count * 2) {
            deval->dominant_behavior = BGOD_BEHAVIOR_HABITUAL;
        } else {
            deval->dominant_behavior = BGOD_BEHAVIOR_MIXED;
        }

        deval->stats.dominant_behavior = deval->dominant_behavior;
    }

    nimcp_mutex_unlock(deval->mutex);
    return 0;
}

int bgod_record_consumption(bg_outcome_deval_t* deval,
                             uint32_t outcome_id,
                             float amount) {
    if (!deval || outcome_id >= deval->num_outcomes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgod_step: deval is NULL");
        return -1;
    }

    nimcp_mutex_lock(deval->mutex);

    deval->outcomes[outcome_id].consumption_count++;
    deval->outcomes[outcome_id].satiety_level +=
        amount * deval->config.satiety_rate;
    deval->outcomes[outcome_id].satiety_level =
        nimcp_clampf(deval->outcomes[outcome_id].satiety_level, 0.0f, 1.0f);

    nimcp_mutex_unlock(deval->mutex);
    return 0;
}

int bgod_record_response(bg_outcome_deval_t* deval, uint32_t action_id) {
    if (!deval || action_id >= deval->config.max_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bgod_record_response: deval is NULL");
        return -1;
    }

    nimcp_mutex_lock(deval->mutex);
    deval->action_response_counts[action_id]++;
    nimcp_mutex_unlock(deval->mutex);
    return 0;
}

int bgod_get_stats(const bg_outcome_deval_t* deval, bgod_stats_t* stats) {
    if (!deval || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgod_get_stats: required parameter is NULL (deval, stats)");
        return -1;
    }
    *stats = deval->stats;
    return 0;
}

/* ============================================================================
 * INTEGRATION IMPLEMENTATION
 * ============================================================================ */

int bgod_modulate_action_values(const bg_outcome_deval_t* deval,
                                 float* action_values,
                                 uint32_t num_actions) {
    if (!deval || !action_values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgod_get_stats: required parameter is NULL (deval, action_values)");
        return -1;
    }

    for (uint32_t a = 0; a < num_actions && a < deval->config.max_actions; a++) {
        float modulation = bgod_get_action_value(deval, a);
        action_values[a] *= (0.5f + 0.5f * modulation);  /* Scale by outcome value */
    }

    return 0;
}

float bgod_get_goal_weight(const bg_outcome_deval_t* deval) {
    if (!deval) return 0.5f;
    return deval->goal_weight;
}

float bgod_get_habit_weight(const bg_outcome_deval_t* deval) {
    if (!deval) return 0.5f;
    return deval->habit_weight;
}
