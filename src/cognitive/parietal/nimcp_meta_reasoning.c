/**
 * @file nimcp_meta_reasoning.c
 * @brief Meta-reasoning engine implementation
 */

#include "cognitive/parietal/nimcp_meta_reasoning.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE(meta_reasoning, MESH_ADAPTER_CATEGORY_COGNITIVE)


struct meta_engine {
    meta_config_t config;
    meta_stats_t stats;
    float inflammation;
    float fatigue;
    float strategy_success[8];  /* Success rate per strategy type */
    uint32_t strategy_uses[8];
    float calibration_bias;
    uint32_t next_strategy_id;
};

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
}

static float apply_mod(const meta_engine_t* e, float v) {
    float f = 1.0f - e->inflammation * e->config.inflammation_sensitivity * 0.2f
                   - e->fatigue * e->config.fatigue_sensitivity * 0.3f;
    return v * fmaxf(0.4f, f);
}

meta_config_t meta_engine_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_engine_default_", 0.0f);


    return (meta_config_t){
        .confidence_calibration_strength = 0.5f,
        .strategy_adaptation_rate = 0.1f,
        .enable_anomaly_detection = true,
        .enable_strategy_switching = true,
        .monitoring_frequency = 5,
        .inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = 1.0f
    };
}

meta_engine_t* meta_engine_create(void) {
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_engine_create", 0.0f);


    meta_config_t c = meta_engine_default_config();
    return meta_engine_create_custom(&c);
}

meta_engine_t* meta_engine_create_custom(const meta_config_t* config) {
    if (!config) { set_error("NULL config"); return NULL; }
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_engine_create_c", 0.0f);


    meta_engine_t* e = nimcp_calloc(1, sizeof(meta_engine_t));
    if (!e) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate e");

        return NULL;

    }
    e->config = *config;
    e->next_strategy_id = 1;

    /* Initialize strategy success rates */
    for (int i = 0; i < 8; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 8 > 256) {
            meta_reasoning_heartbeat("meta_reasoni_loop",
                             (float)(i + 1) / (float)8);
        }

        e->strategy_success[i] = 0.5f;
        e->strategy_uses[i] = 0;
    }

    return e;
}

void meta_engine_destroy(meta_engine_t* engine) {
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_engine_destroy", 0.0f);


    if (engine) nimcp_free(engine);
}

meta_strategy_t* meta_select_strategy(meta_engine_t* engine, const meta_problem_t* problem) {
    if (!engine || !problem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_select_strategy: required parameter is NULL (engine, problem)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_select_strategy", 0.0f);


    meta_strategy_t* s = nimcp_calloc(1, sizeof(meta_strategy_t));
    if (!s) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate s");

        return NULL;

    }

    s->id = engine->next_strategy_id++;

    /* Select best strategy based on past performance and problem features */
    float best_score = 0;
    meta_strategy_type_t best_type = META_STRATEGY_ANALYTICAL;

    for (int i = 0; i < 8; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 8 > 256) {
            meta_reasoning_heartbeat("meta_reasoni_loop",
                             (float)(i + 1) / (float)8);
        }

        float score = engine->strategy_success[i];

        /* Adjust based on problem difficulty */
        if (problem->estimated_difficulty > 0.7f && i == META_STRATEGY_INTUITIVE) {
            score *= 0.8f;  /* Less intuitive for hard problems */
        }
        if (problem->estimated_difficulty < 0.3f && i == META_STRATEGY_DIVIDE_CONQUER) {
            score *= 0.9f;  /* Less divide-and-conquer for easy problems */
        }

        score = apply_mod(engine, score);

        if (score > best_score) {
            best_score = score;
            best_type = (meta_strategy_type_t)i;
        }
    }

    s->type = best_type;
    strncpy(s->name, meta_strategy_name(best_type), sizeof(s->name) - 1);
    s->suitability = best_score;
    s->past_success_rate = engine->strategy_success[best_type];
    s->computational_cost = 0.5f;
    s->time_estimate = 1.0f + problem->estimated_difficulty * 2.0f;

    engine->stats.strategies_selected++;
    engine->strategy_uses[best_type]++;

    return s;
}

int meta_evaluate_strategies(meta_engine_t* engine, const meta_problem_t* problem,
    meta_strategy_t* strategies, uint32_t max_strategies, uint32_t* num_found) {
    if (!engine || !problem || !strategies || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_select_strategy: required parameter is NULL (engine, problem, strategies, num_found)");
        return -1;
    }

    *num_found = 0;
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_evaluate_strate", 0.0f);


    for (int i = 0; i < 8 && *num_found < max_strategies; i++) {
        strategies[*num_found].id = *num_found + 1;
        strategies[*num_found].type = (meta_strategy_type_t)i;
        strncpy(strategies[*num_found].name, meta_strategy_name((meta_strategy_type_t)i),
                sizeof(strategies[*num_found].name) - 1);
        strategies[*num_found].suitability = apply_mod(engine, engine->strategy_success[i]);
        strategies[*num_found].past_success_rate = engine->strategy_success[i];
        strategies[*num_found].computational_cost = 0.3f + 0.1f * i;
        strategies[*num_found].time_estimate = 1.0f + 0.5f * i;
        (*num_found)++;
    }

    return 0;
}

int meta_switch_strategy(meta_engine_t* engine, meta_reasoning_chain_t* chain,
    const meta_strategy_t* new_strategy) {
    if (!engine || !chain || !new_strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_select_strategy: required parameter is NULL (engine, chain, new_strategy)");
        return -1;
    }
    if (!engine->config.enable_strategy_switching) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_select_strategy: engine->config is NULL");
        return -1;
    }

    /* Update current step with new strategy */
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_switch_strategy", 0.0f);


    if (chain->num_steps > 0) {
        chain->steps[chain->num_steps - 1].strategy_used = new_strategy->type;
    }

    engine->stats.strategy_switches++;
    return 0;
}

float meta_calibrate_confidence(meta_engine_t* engine, const meta_reasoning_chain_t* chain) {
    if (!engine || !chain) return 0.5f;

    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_calibrate_confi", 0.0f);


    float calibrated = chain->overall_confidence;

    /* Apply calibration bias learned from experience */
    calibrated -= engine->calibration_bias;
    calibrated *= engine->config.confidence_calibration_strength;

    return apply_mod(engine, fmaxf(0.0f, fminf(1.0f, calibrated)));
}

float meta_estimate_accuracy(meta_engine_t* engine, float stated_confidence) {
    if (!engine) return stated_confidence;

    /* Adjust for known calibration bias */
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_estimate_accura", 0.0f);


    float adjusted = stated_confidence - engine->calibration_bias;
    return apply_mod(engine, fmaxf(0.0f, fminf(1.0f, adjusted)));
}

int meta_update_calibration(meta_engine_t* engine, float prediction, float actual) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_update_calibration: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_update_calibrat", 0.0f);


    float error = prediction - actual;
    engine->calibration_bias += engine->config.strategy_adaptation_rate * error;
    engine->calibration_bias = fmaxf(-0.3f, fminf(0.3f, engine->calibration_bias));

    engine->stats.avg_calibration_error =
        (engine->stats.avg_calibration_error * 0.9f) + (fabsf(error) * 0.1f);

    return 0;
}

int meta_monitor_reasoning(meta_engine_t* engine, const meta_reasoning_chain_t* chain,
    meta_anomaly_t* anomalies, uint32_t max_anomalies, uint32_t* num_found) {
    if (!engine || !chain || !anomalies || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_update_calibration: required parameter is NULL (engine, chain, anomalies, num_found)");
        return -1;
    }
    if (!engine->config.enable_anomaly_detection) { *num_found = 0; return 0; }

    *num_found = 0;

    /* Check for confidence drops */
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_monitor_reasoni", 0.0f);


    for (uint32_t i = 1; i < chain->num_steps && *num_found < max_anomalies; i++) {
        float drop = chain->steps[i-1].confidence - chain->steps[i].confidence;
        if (drop > 0.3f) {
            anomalies[*num_found].id = *num_found + 1;
            anomalies[*num_found].step_number = i;
            anomalies[*num_found].severity = drop;
            anomalies[*num_found].is_recoverable = true;
            snprintf(anomalies[*num_found].description, sizeof(anomalies[*num_found].description),
                    "Confidence drop at step %u", i);
            (*num_found)++;
            engine->stats.anomalies_detected++;
        }
    }

    /* Check for stalled progress */
    if (chain->num_steps > 3 && chain->progress_rate < 0.1f) {
        if (*num_found < max_anomalies) {
            anomalies[*num_found].id = *num_found + 1;
            anomalies[*num_found].step_number = chain->num_steps - 1;
            anomalies[*num_found].severity = 0.6f;
            anomalies[*num_found].is_recoverable = true;
            snprintf(anomalies[*num_found].description, sizeof(anomalies[*num_found].description),
                    "Progress stalled");
            (*num_found)++;
            engine->stats.anomalies_detected++;
        }
    }

    return 0;
}

int meta_get_state(meta_engine_t* engine, const meta_reasoning_chain_t* chain,
    meta_state_t* state) {
    if (!engine || !chain || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_update_calibration: required parameter is NULL (engine, chain, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_get_state", 0.0f);


    memset(state, 0, sizeof(meta_state_t));
    state->confidence = meta_calibrate_confidence(engine, chain);
    state->progress = (chain->num_steps > 0) ?
                      chain->steps[chain->num_steps - 1].progress : 0;

    return 0;
}

float meta_estimate_progress(meta_engine_t* engine, const meta_reasoning_chain_t* chain) {
    if (!engine || !chain || chain->num_steps == 0) return 0;
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_estimate_progre", 0.0f);


    return apply_mod(engine, chain->steps[chain->num_steps - 1].progress);
}

int meta_learn_from_outcome(meta_engine_t* engine, const meta_problem_t* problem,
    const meta_strategy_t* strategy, bool success, float performance) {
    if (!engine || !problem || !strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_estimate_progress: required parameter is NULL (engine, problem, strategy)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_learn_from_outc", 0.0f);


    int type = (int)strategy->type;
    if (type < 0 || type >= 8) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_estimate_progress: capacity exceeded");
        return -1;
    }

    /* Update success rate for this strategy */
    float update = success ? 1.0f : 0.0f;
    engine->strategy_success[type] =
        engine->strategy_success[type] * (1.0f - engine->config.strategy_adaptation_rate) +
        update * engine->config.strategy_adaptation_rate;

    engine->stats.avg_strategy_success =
        (engine->stats.avg_strategy_success * 0.9f) + (performance * 0.1f);

    if (success) {
        engine->stats.anomalies_recovered++;
    }

    return 0;
}

int meta_learn_from_chain(meta_engine_t* engine, const meta_reasoning_chain_t* chain,
    bool success) {
    if (!engine || !chain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_estimate_progress: required parameter is NULL (engine, chain)");
        return -1;
    }

    /* Update calibration based on overall outcome */
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_learn_from_chai", 0.0f);


    meta_update_calibration(engine, chain->overall_confidence, success ? 1.0f : 0.0f);

    return 0;
}

void meta_free_strategy(meta_strategy_t* strategy) {
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_free_strategy", 0.0f);


    if (strategy) nimcp_free(strategy);
}

void meta_free_chain(meta_reasoning_chain_t* chain) {
    if (!chain) return;
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_free_chain", 0.0f);


    if (chain->steps) nimcp_free(chain->steps);
    nimcp_free(chain);
    chain = NULL;
}

int meta_set_inflammation(meta_engine_t* engine, float level) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_set_inflammation: engine is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_set_inflammatio", 0.0f);


    engine->inflammation = fmaxf(0, fminf(1, level));
    return 0;
}

int meta_set_fatigue(meta_engine_t* engine, float level) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_set_fatigue: engine is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_set_fatigue", 0.0f);


    engine->fatigue = fmaxf(0, fminf(1, level));
    return 0;
}

int meta_get_stats(const meta_engine_t* engine, meta_stats_t* stats) {
    if (!engine || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_get_stats: required parameter is NULL (engine, stats)");
        return -1;
    }
    *stats = engine->stats;
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_get_stats", 0.0f);


    return 0;
}

void meta_reset_stats(meta_engine_t* engine) {
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_meta_reset_stats", 0.0f);


    if (engine) memset(&engine->stats, 0, sizeof(engine->stats));
}

const char* meta_get_last_error(void) { return g_last_error; }

const char* meta_strategy_name(meta_strategy_type_t type) {
    switch (type) {
        case META_STRATEGY_ANALYTICAL:      return "analytical";
        case META_STRATEGY_INTUITIVE:       return "intuitive";
        case META_STRATEGY_ANALOGICAL:      return "analogical";
        case META_STRATEGY_TRIAL_ERROR:     return "trial_error";
        case META_STRATEGY_DIVIDE_CONQUER:  return "divide_conquer";
        case META_STRATEGY_MEANS_ENDS:      return "means_ends";
        case META_STRATEGY_WORKING_BACKWARD: return "working_backward";
        case META_STRATEGY_BRAINSTORMING:   return "brainstorming";
        default:                            return "unknown";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int meta_reasoning_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    meta_reasoning_heartbeat("meta_reasoni_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Meta_Reasoning");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                meta_reasoning_heartbeat("meta_reasoni_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Meta_Reasoning");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Meta_Reasoning");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void meta_reasoning_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_meta_reasoning_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Functions
 * ============================================================================ */

int meta_reasoning_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "meta_reasoning_training_begin: NULL argument");
        return -1;
    }
    meta_reasoning_heartbeat_instance(g_meta_reasoning_health_agent, "meta_reasoning_training_begin", 0.0f);
    meta_engine_t* mr = (meta_engine_t*)instance;
    memset(&mr->stats, 0, sizeof(mr->stats));
    mr->calibration_bias = 0.0f;
    for (int i = 0; i < 8; i++) {
        mr->strategy_success[i] = 0.5f;
        mr->strategy_uses[i] = 0;
    }
    NIMCP_LOGGING_INFO("Meta-reasoning training begin: stats and strategy history reset");
    return 0;
}

int meta_reasoning_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "meta_reasoning_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    meta_reasoning_heartbeat_instance(g_meta_reasoning_health_agent, "meta_reasoning_training_step", progress);
    meta_engine_t* mr = (meta_engine_t*)instance;
    mr->stats.strategies_selected++;
    /* Progressive learning rate decay for strategy adaptation */
    float decay = 1.0f - 0.3f * progress;
    if (decay < 0.5f) decay = 0.5f;
    mr->config.strategy_adaptation_rate *= decay;
    if (mr->config.strategy_adaptation_rate < 0.01f)
        mr->config.strategy_adaptation_rate = 0.01f;
    /* Sharpen confidence calibration with experience */
    mr->config.confidence_calibration_strength += 0.01f * progress;
    if (mr->config.confidence_calibration_strength > 1.0f)
        mr->config.confidence_calibration_strength = 1.0f;
    /* Reduce calibration bias toward zero */
    mr->calibration_bias *= (1.0f - 0.05f * progress);
    return 0;
}

int meta_reasoning_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "meta_reasoning_training_end: NULL argument");
        return -1;
    }
    meta_reasoning_heartbeat_instance(g_meta_reasoning_health_agent, "meta_reasoning_training_end", 1.0f);
    meta_engine_t* mr = (meta_engine_t*)instance;
    /* Compute average strategy success */
    float total_success = 0.0f;
    uint32_t total_uses = 0;
    for (int i = 0; i < 8; i++) {
        total_success += mr->strategy_success[i] * (float)mr->strategy_uses[i];
        total_uses += mr->strategy_uses[i];
    }
    mr->stats.avg_strategy_success = (total_uses > 0)
        ? total_success / (float)total_uses : 0.0f;
    NIMCP_LOGGING_INFO("Meta-reasoning training end: %lu strategies selected, %lu switches, "
                       "avg_success=%.4f, calibration_bias=%.4f",
                       (unsigned long)mr->stats.strategies_selected,
                       (unsigned long)mr->stats.strategy_switches,
                       mr->stats.avg_strategy_success,
                       mr->calibration_bias);
    return 0;
}
