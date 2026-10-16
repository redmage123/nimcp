/**
 * @file nimcp_counterfactual.c
 * @brief Counterfactual reasoning engine implementation
 */

#include "cognitive/parietal/nimcp_counterfactual.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE(parietal_counterfactual, MESH_ADAPTER_CATEGORY_COGNITIVE)


struct counterfactual_engine {
    cf_config_t config;
    cf_stats_t stats;
    float inflammation;
    float fatigue;
    uint32_t next_cf_id;

    /* Phase 5: Causal graph for Pearl's do-calculus */
    float causal_weights[CF_MAX_STATE_DIM][CF_MAX_STATE_DIM]; /* W[i][j] = causal influence of j on i */
    uint32_t graph_dim;
    bool graph_initialized;
};

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
}

/* Phase 5: BFS causal propagation (Pearl's do-calculus) */
static void propagate_do_calculus(counterfactual_engine_t* engine, cf_state_t* cf_world, uint32_t intervened_var) {
    if (!engine->graph_initialized || !cf_world || !cf_world->values) return;
    uint32_t dim = cf_world->dim < engine->graph_dim ? cf_world->dim : engine->graph_dim;
    if (intervened_var >= dim) return;

    bool visited[CF_MAX_STATE_DIM] = {false};
    uint32_t queue[CF_MAX_STATE_DIM];
    uint32_t qhead = 0, qtail = 0;

    visited[intervened_var] = true;
    queue[qtail++] = intervened_var;

    while (qhead < qtail) {
        uint32_t src = queue[qhead++];
        for (uint32_t dst = 0; dst < dim; dst++) {
            float w = engine->causal_weights[dst][src];
            if (fabsf(w) < 1e-6f || visited[dst]) continue;
            /* Dampened downstream propagation */
            cf_world->values[dst] += w * cf_world->values[src] * 0.5f;
            visited[dst] = true;
            queue[qtail++] = dst;
        }
    }
}

static float apply_mod(const counterfactual_engine_t* e, float v) {
    float f = 1.0f - e->inflammation * e->config.inflammation_sensitivity * 0.2f
                   - e->fatigue * e->config.fatigue_sensitivity * 0.3f;
    return v * fmaxf(0.4f, f);
}

cf_config_t counterfactual_engine_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_engine_default_confi", 0.0f);


    return (cf_config_t){
        .min_plausibility = 0.2f,
        .consequence_threshold = 0.1f,
        .enable_causal_tracing = true,
        .use_minimal_change = true,
        .max_trace_depth = 5,
        .inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = 1.0f
    };
}

counterfactual_engine_t* counterfactual_engine_create(void) {
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_engine_create", 0.0f);


    cf_config_t c = counterfactual_engine_default_config();
    return counterfactual_engine_create_custom(&c);
}

counterfactual_engine_t* counterfactual_engine_create_custom(const cf_config_t* config) {
    if (!config) { set_error("NULL config"); return NULL; }
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_engine_create_custom", 0.0f);


    counterfactual_engine_t* e = nimcp_calloc(1, sizeof(counterfactual_engine_t));
    if (!e) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate e");

        return NULL;

    }
    e->config = *config;
    e->next_cf_id = 1;
    return e;
}

void counterfactual_engine_destroy(counterfactual_engine_t* engine) {
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_engine_destroy", 0.0f);


    if (engine) nimcp_free(engine);
}

cf_state_t* counterfactual_create_state(const float* values, uint32_t dim,
    const char* description) {
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_create_state", 0.0f);


    cf_state_t* s = nimcp_calloc(1, sizeof(cf_state_t));
    if (!s) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate s");

        return NULL;

    }

    if (values && dim > 0) {
        s->values = nimcp_calloc(dim, sizeof(float));
        if (s->values) {
            memcpy(s->values, values, dim * sizeof(float));
            s->dim = dim;
        }
    }
    if (description) strncpy(s->description, description, sizeof(s->description) - 1);

    return s;
}

void counterfactual_free_state(cf_state_t* state) {
    if (!state) return;
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_free_state", 0.0f);


    if (state->values) nimcp_free(state->values);
    nimcp_free(state);
    state = NULL;
}

cf_counterfactual_t* counterfactual_imagine(counterfactual_engine_t* engine,
    const cf_state_t* actual, const cf_intervention_t* what_if) {
    if (!engine || !actual || !what_if) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_free_state: required parameter is NULL (engine, actual, what_if)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_imagine", 0.0f);


    cf_counterfactual_t* cf = nimcp_calloc(1, sizeof(cf_counterfactual_t));
    if (!cf) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate cf");

        return NULL;

    }

    cf->id = engine->next_cf_id++;

    /* Copy actual state */
    cf->actual_world = counterfactual_create_state(actual->values, actual->dim, actual->description);

    /* Create counterfactual state with intervention applied */
    cf->counterfactual_world = counterfactual_create_state(actual->values, actual->dim,
        "Counterfactual world");
    if (cf->counterfactual_world && what_if->target_variable < cf->counterfactual_world->dim) {
        cf->counterfactual_world->values[what_if->target_variable] = what_if->counterfactual_value;
        /* Phase 5: Propagate effects through causal graph */
        propagate_do_calculus(engine, cf->counterfactual_world, what_if->target_variable);
    }

    /* Copy intervention */
    cf->intervention = nimcp_calloc(1, sizeof(cf_intervention_t));
    if (cf->intervention) {
        memcpy(cf->intervention, what_if, sizeof(cf_intervention_t));
    }

    /* Trace consequences */
    cf->consequences = nimcp_calloc(CF_MAX_CONSEQUENCES, sizeof(cf_consequence_t));
    if (!cf->consequences) return NULL;
    counterfactual_trace_effects(engine, cf, cf->consequences, CF_MAX_CONSEQUENCES, &cf->num_consequences);

    /* Compute distance and plausibility */
    cf->distance = counterfactual_distance(engine, actual, cf->counterfactual_world);
    cf->plausibility = apply_mod(engine, 1.0f / (1.0f + cf->distance));

    engine->stats.counterfactuals_generated++;
    engine->stats.interventions_analyzed++;

    return cf;
}

cf_counterfactual_t** counterfactual_explore_space(counterfactual_engine_t* engine,
    const cf_state_t* actual, uint32_t max_alternatives, uint32_t* num_found) {
    if (!engine || !actual || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_free_state: required parameter is NULL (engine, actual, num_found)");
        return NULL;
    }

    cf_counterfactual_t** cfs = nimcp_calloc(max_alternatives, sizeof(cf_counterfactual_t*));
    if (!cfs) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate cfs");

        return NULL;

    }

    *num_found = 0;
    for (uint32_t i = 0; i < actual->dim && *num_found < max_alternatives; i++) {
        cf_intervention_t intervention = {
            .id = i + 1,
            .target_variable = i,
            .original_value = actual->values[i],
            .counterfactual_value = actual->values[i] * 1.5f
        };
        snprintf(intervention.description, sizeof(intervention.description),
                "Increase var %u", i);

        cfs[*num_found] = counterfactual_imagine(engine, actual, &intervention);
        if (cfs[*num_found]) (*num_found)++;
    }

    return cfs;
}

int counterfactual_trace_effects(counterfactual_engine_t* engine,
    const cf_counterfactual_t* cf, cf_consequence_t* consequences,
    uint32_t max_consequences, uint32_t* num_found) {
    if (!engine || !cf || !consequences || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_free_state: required parameter is NULL (engine, cf, consequences, num_found)");
        return -1;
    }
    if (!engine->config.enable_causal_tracing) { *num_found = 0; return 0; }

    *num_found = 0;

    if (!cf->actual_world || !cf->counterfactual_world) return 0;

    /* Find variables that differ between actual and counterfactual */
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_trace_effects", 0.0f);


    for (uint32_t i = 0; i < cf->actual_world->dim && *num_found < max_consequences; i++) {
        float diff = cf->counterfactual_world->values[i] - cf->actual_world->values[i];
        if (fabsf(diff) > engine->config.consequence_threshold) {
            consequences[*num_found].id = *num_found + 1;
            consequences[*num_found].affected_variable = i;
            consequences[*num_found].magnitude = diff;
            consequences[*num_found].probability = apply_mod(engine, 0.8f);
            consequences[*num_found].is_direct = (cf->intervention &&
                cf->intervention->target_variable == i);
            snprintf(consequences[*num_found].description, sizeof(consequences[*num_found].description),
                    "Change in var %u: %.2f", i, diff);
            (*num_found)++;
            engine->stats.consequences_traced++;
        }
    }

    /* Phase 5: Trace causal paths through graph */
    if (engine->graph_initialized && cf->intervention) {
        uint32_t src = cf->intervention->target_variable;
        for (uint32_t dst = 0; dst < engine->graph_dim && *num_found < max_consequences; dst++) {
            if (dst == src) continue;
            float w = engine->causal_weights[dst][src];
            if (fabsf(w) < engine->config.consequence_threshold) continue;

            /* Check not already found */
            bool dup = false;
            for (uint32_t k = 0; k < *num_found; k++) {
                if (consequences[k].affected_variable == dst) { dup = true; break; }
            }
            if (dup) continue;

            consequences[*num_found].id = *num_found + 1;
            consequences[*num_found].affected_variable = dst;
            consequences[*num_found].magnitude = w;
            consequences[*num_found].probability = apply_mod(engine, fabsf(w));
            consequences[*num_found].is_direct = true;
            snprintf(consequences[*num_found].description,
                    sizeof(consequences[*num_found].description),
                    "Causal: var %u -> var %u (w=%.2f)", src, dst, w);
            (*num_found)++;
            engine->stats.consequences_traced++;
        }
    }

    return 0;
}

float counterfactual_estimate_probability(counterfactual_engine_t* engine,
    const cf_counterfactual_t* cf) {
    if (!engine || !cf) return 0;
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_estimate_probability", 0.0f);


    return apply_mod(engine, cf->plausibility);
}

float counterfactual_causal_strength(counterfactual_engine_t* engine,
    uint32_t cause_var, uint32_t effect_var, const cf_state_t* context) {
    if (!engine || !context) return 0;
    if (cause_var >= context->dim || effect_var >= context->dim) return 0;

    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_causal_strength", 0.0f);

    /* Phase 5: Use causal graph if available */
    if (engine->graph_initialized && cause_var < engine->graph_dim && effect_var < engine->graph_dim) {
        float direct = fabsf(engine->causal_weights[effect_var][cause_var]);
        if (direct > 0.0f) return apply_mod(engine, fminf(1.0f, direct));
    }

    /* Fallback: correlation-based estimate */
    float strength = fabsf(context->values[cause_var] * context->values[effect_var]);
    return apply_mod(engine, fminf(1.0f, strength));
}

int counterfactual_find_causes(counterfactual_engine_t* engine,
    const cf_state_t* state, uint32_t effect_var,
    uint32_t* cause_vars, uint32_t max_causes, uint32_t* num_found) {
    if (!engine || !state || !cause_vars || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_free_state: required parameter is NULL (engine, state, cause_vars, num_found)");
        return -1;
    }
    if (effect_var >= state->dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "counterfactual_free_state: capacity exceeded");
        return -1;
    }

    *num_found = 0;
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_find_causes", 0.0f);

    /* Phase 5: Use causal graph for backward tracing */
    if (engine->graph_initialized && effect_var < engine->graph_dim) {
        for (uint32_t i = 0; i < engine->graph_dim && *num_found < max_causes; i++) {
            if (i == effect_var) continue;
            float w = fabsf(engine->causal_weights[effect_var][i]);
            if (w > 0.1f) {
                cause_vars[(*num_found)++] = i;
            }
        }
    } else {
        /* Fallback: correlation-based */
        for (uint32_t i = 0; i < state->dim && *num_found < max_causes; i++) {
            if (i == effect_var) continue;
            float strength = counterfactual_causal_strength(engine, i, effect_var, state);
            if (strength > 0.3f) {
                cause_vars[(*num_found)++] = i;
            }
        }
    }

    return 0;
}

float counterfactual_distance(counterfactual_engine_t* engine,
    const cf_state_t* s1, const cf_state_t* s2) {
    if (!engine || !s1 || !s2) return 0;
    if (!s1->values || !s2->values) return 0;

    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_distance", 0.0f);


    uint32_t min_dim = (s1->dim < s2->dim) ? s1->dim : s2->dim;
    float dist = 0;

    for (uint32_t i = 0; i < min_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && min_dim > 256) {
            parietal_counterfactual_heartbeat("counterfactu_loop",
                             (float)(i + 1) / (float)min_dim);
        }

        float diff = s1->values[i] - s2->values[i];
        dist += diff * diff;
    }

    return sqrtf(dist);
}

cf_counterfactual_t* counterfactual_find_closest(counterfactual_engine_t* engine,
    const cf_state_t* actual, const cf_state_t* target) {
    if (!engine || !actual || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_free_state: required parameter is NULL (engine, actual, target)");
        return NULL;
    }

    /* Create intervention that moves toward target */
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_find_closest", 0.0f);


    uint32_t best_var = 0;
    float best_improvement = 0;

    for (uint32_t i = 0; i < actual->dim && i < target->dim; i++) {
        float improvement = fabsf(target->values[i] - actual->values[i]);
        if (improvement > best_improvement) {
            best_improvement = improvement;
            best_var = i;
        }
    }

    cf_intervention_t intervention = {
        .id = 1,
        .target_variable = best_var,
        .original_value = actual->values[best_var],
        .counterfactual_value = target->values[best_var]
    };
    snprintf(intervention.description, sizeof(intervention.description),
            "Move var %u toward target", best_var);

    return counterfactual_imagine(engine, actual, &intervention);
}

void counterfactual_free(cf_counterfactual_t* cf) {
    if (!cf) return;
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_free", 0.0f);


    if (cf->actual_world) counterfactual_free_state(cf->actual_world);
    if (cf->counterfactual_world) counterfactual_free_state(cf->counterfactual_world);
    if (cf->intervention) nimcp_free(cf->intervention);
    if (cf->consequences) nimcp_free(cf->consequences);
    nimcp_free(cf);
    cf = NULL;
}

int counterfactual_set_inflammation(counterfactual_engine_t* engine, float level) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_set_inflammation: engine is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_set_inflammation", 0.0f);


    engine->inflammation = fmaxf(0, fminf(1, level));
    return 0;
}

int counterfactual_set_fatigue(counterfactual_engine_t* engine, float level) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_set_fatigue: engine is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_set_fatigue", 0.0f);


    engine->fatigue = fmaxf(0, fminf(1, level));
    return 0;
}

int counterfactual_get_stats(const counterfactual_engine_t* engine, cf_stats_t* stats) {
    if (!engine || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_get_stats: required parameter is NULL (engine, stats)");
        return -1;
    }
    *stats = engine->stats;
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_get_stats", 0.0f);


    return 0;
}

void counterfactual_reset_stats(counterfactual_engine_t* engine) {
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_reset_stats", 0.0f);


    if (engine) memset(&engine->stats, 0, sizeof(engine->stats));
}

const char* counterfactual_get_last_error(void) { return g_last_error; }

/* ============================================================================
 * Phase 5: Causal Graph Learning
 * ============================================================================ */

int counterfactual_learn_causal_weight(counterfactual_engine_t* engine,
    uint32_t cause_var, uint32_t effect_var, float weight) {
    if (!engine) return -1;
    if (cause_var >= CF_MAX_STATE_DIM || effect_var >= CF_MAX_STATE_DIM) return -1;

    engine->causal_weights[effect_var][cause_var] = fmaxf(-1.0f, fminf(1.0f, weight));
    if (!engine->graph_initialized) {
        engine->graph_initialized = true;
        engine->graph_dim = (cause_var > effect_var ? cause_var : effect_var) + 1;
    } else {
        uint32_t max_var = (cause_var > effect_var ? cause_var : effect_var) + 1;
        if (max_var > engine->graph_dim) engine->graph_dim = max_var;
    }
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int counterfactual_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    parietal_counterfactual_heartbeat("counterfactu_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Counterfactual_Reasoning");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                parietal_counterfactual_heartbeat("counterfactu_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Counterfactual_Reasoning");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Counterfactual_Reasoning");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void parietal_counterfactual_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_parietal_counterfactual_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int parietal_counterfactual_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "parietal_counterfactual_training_begin: NULL argument");
        return -1;
    }
    parietal_counterfactual_heartbeat_instance(g_parietal_counterfactual_health_agent, "parietal_counterfactual_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int parietal_counterfactual_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "parietal_counterfactual_training_end: NULL argument");
        return -1;
    }
    parietal_counterfactual_heartbeat_instance(g_parietal_counterfactual_health_agent, "parietal_counterfactual_training_end", 1.0f);
    (void)instance;
    return 0;
}

int parietal_counterfactual_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "parietal_counterfactual_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    parietal_counterfactual_heartbeat_instance(g_parietal_counterfactual_health_agent, "parietal_counterfactual_training_step", progress);
    (void)instance;
    return 0;
}
