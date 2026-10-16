/**
 * @file nimcp_omni_logic_bridge.c
 * @brief Implementation of Omnidirectional Inference to Logic Gate Bridge
 */

#include "cognitive/logic/nimcp_omni_logic_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/jepa/nimcp_jepa_bidirectional.h"
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(omni_logic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_omni_logic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_omni_logic_bridge_mesh_registry = NULL;

nimcp_error_t omni_logic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_omni_logic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "omni_logic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "omni_logic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_omni_logic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_omni_logic_bridge_mesh_registry = registry;
    return err;
}

void omni_logic_bridge_mesh_unregister(void) {
    if (g_omni_logic_bridge_mesh_registry && g_omni_logic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_omni_logic_bridge_mesh_registry, g_omni_logic_bridge_mesh_id);
        g_omni_logic_bridge_mesh_id = 0;
        g_omni_logic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from omni_logic_bridge module (instance-level) */
static inline void omni_logic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_omni_logic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_omni_logic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_omni_logic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "OMNI_LOGIC_BRIDGE"


/* ============================================================================
 * Static Helpers
 * ============================================================================ */

static omni_infer_direction_t compute_recommended_direction(
    const omni_logic_conditions_t* conds,
    const omni_logic_config_t* config) {

    /* If goal is specified and causes unknown → backward chaining */
    if (conds->goal_specified && !conds->causes_known) {
        return OMNI_INFER_BACKWARD;
    }

    /* If causes known and goal unknown → forward chaining */
    if (conds->causes_known && !conds->goal_specified) {
        return OMNI_INFER_FORWARD;
    }

    /* If lateral modality is relevant → lateral inference */
    if (conds->lateral_relevant) {
        return OMNI_INFER_LATERAL;
    }

    /* If conflicting evidence → weighted combination */
    if (conds->conflicting_evidence) {
        return OMNI_INFER_WEIGHTED_COMBINE;
    }

    /* Default: use direction with highest confidence */
    if (conds->forward_confident && !conds->backward_confident) {
        return OMNI_INFER_FORWARD;
    }
    if (conds->backward_confident && !conds->forward_confident) {
        return OMNI_INFER_BACKWARD;
    }

    /* Both confident or neither → weighted combination */
    return OMNI_INFER_WEIGHTED_COMBINE;
}

static omni_logic_type_t compute_suggested_logic_type(
    const omni_logic_conditions_t* conds) {

    if (conds->goal_specified && !conds->causes_known) {
        return OMNI_LOGIC_ABDUCTIVE;
    }
    if (conds->causes_known && !conds->goal_specified) {
        return OMNI_LOGIC_DEDUCTIVE;
    }
    if (conds->lateral_relevant) {
        return OMNI_LOGIC_ANALOGICAL;
    }
    return OMNI_LOGIC_COMBINED;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int omni_logic_default_config(omni_logic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_default_c", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    memset(config, 0, sizeof(omni_logic_config_t));

    config->confidence_threshold = OMNI_LOGIC_DEFAULT_CONFIDENCE_THRESHOLD;
    config->forward_confidence_min = 0.5f;
    config->backward_confidence_min = 0.5f;

    config->default_forward_weight = 1.0f;
    config->default_backward_weight = 1.0f;
    config->default_lateral_weight = 0.8f;

    config->enable_chaining = true;
    config->max_chain_depth = 5;
    config->max_iterations = 10;

    config->create_default_gates = true;

    config->enable_bio_async = true;
    config->enable_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

omni_logic_bridge_t* omni_logic_bridge_create(const omni_logic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_create", 0.0f);


    omni_logic_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_logic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(omni_logic_config_t));
    } else {
        omni_logic_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "omni_logic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_logic_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Allocate rules array */
    bridge->max_rules = OMNI_LOGIC_MAX_RULES;
    bridge->rules = nimcp_calloc(bridge->max_rules, sizeof(omni_logic_rule_t));
    if (!bridge->rules) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_logic_bridge_create: bridge->rules is NULL");
        return NULL;
    }
    bridge->num_rules = 0;

    /* Initialize default conditions */
    memset(&bridge->conditions, 0, sizeof(omni_logic_conditions_t));
    bridge->conditions.available_time_ms = 1000.0f;

    /* Initialize default direction weights */
    bridge->logic_effects.direction_weight[0] = bridge->config.default_forward_weight;
    bridge->logic_effects.direction_weight[1] = bridge->config.default_backward_weight;
    bridge->logic_effects.direction_weight[2] = bridge->config.default_lateral_weight;
    bridge->logic_effects.direction_weight[3] = 0.5f;  /* Hierarchical up */
    bridge->logic_effects.direction_weight[4] = 0.5f;  /* Hierarchical down */

    bridge->logic_effects.max_iterations = bridge->config.max_iterations;
    bridge->logic_effects.confidence_threshold = bridge->config.confidence_threshold;
    bridge->logic_effects.enable_chaining = bridge->config.enable_chaining;

    memset(&bridge->stats, 0, sizeof(omni_logic_stats_t));

    NIMCP_LOGGING_INFO("Created %s bridge", "omni_logic");
    return bridge;
}

void omni_logic_bridge_destroy(omni_logic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "omni_logic");

    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_destroy", 0.0f);


    if (bridge->rules) {
        nimcp_free(bridge->rules);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int omni_logic_connect_jepa(omni_logic_bridge_t* bridge,
                             jepa_bidirectional_t* jepa) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_connect_j", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->jepa = jepa;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_logic_connect_pred_hier(omni_logic_bridge_t* bridge,
                                  predictive_hierarchy_t* pred_hier) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_connect_p", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->pred_hier = pred_hier;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_logic_connect_logic_network(omni_logic_bridge_t* bridge,
                                      neural_logic_network_t* logic_net) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_connect_l", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->logic_net = logic_net;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int omni_logic_update(omni_logic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_update", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update conditions from connected systems */
    if (bridge->jepa) {
        jepa_bidir_stats_t stats;
        if (jepa_bidirectional_get_stats(bridge->jepa, &stats) == NIMCP_SUCCESS) {
            /* Use free energy as a proxy for prediction error */
            float fe = stats.avg_free_energy;
            bridge->conditions.forward_pe = fe;
            bridge->conditions.backward_pe = fe;
            bridge->conditions.forward_confident =
                (fe < bridge->config.forward_confidence_min);
            bridge->conditions.backward_confident =
                (fe < bridge->config.backward_confidence_min);
        }
    }

    /* Compute omni → logic effects */
    bridge->omni_effects.forward_confidence =
        1.0f / (1.0f + bridge->conditions.forward_pe);
    bridge->omni_effects.backward_confidence =
        1.0f / (1.0f + bridge->conditions.backward_pe);
    bridge->omni_effects.lateral_confidence =
        1.0f / (1.0f + bridge->conditions.lateral_pe);
    bridge->omni_effects.hierarchical_confidence = 0.7f;

    bridge->omni_effects.suggested_type = compute_suggested_logic_type(&bridge->conditions);
    bridge->omni_effects.recommended_dir =
        compute_recommended_direction(&bridge->conditions, &bridge->config);

    /* Update logic → omni effects based on conditions */
    bridge->logic_effects.direction = bridge->omni_effects.recommended_dir;
    bridge->logic_effects.force_direction = false;

    /* Update statistics */
    bridge->stats.total_updates++;
    float n = (float)bridge->stats.total_updates;
    bridge->stats.avg_forward_confidence =
        (bridge->stats.avg_forward_confidence * (n - 1) + bridge->omni_effects.forward_confidence) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    bridge->stats.avg_backward_confidence =
        (bridge->stats.avg_backward_confidence * (n - 1) + bridge->omni_effects.backward_confidence) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_logic_apply_to_logic(omni_logic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_apply_to_", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.gate_evaluations++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_logic_apply_to_omni(omni_logic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_apply_to_", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Inference Direction API
 * ============================================================================ */

int omni_logic_get_direction(omni_logic_bridge_t* bridge,
                              omni_infer_direction_t* direction) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_get_direc", 0.0f);


    NIMCP_CHECK_THROW(bridge && direction, NIMCP_ERROR_INVALID_PARAM, "bridge or direction is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *direction = bridge->logic_effects.direction;
    bridge->stats.direction_decisions++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_logic_set_condition(omni_logic_bridge_t* bridge,
                              omni_logic_condition_t condition,
                              bool value) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_set_condi", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    switch (condition) {
        case OMNI_COND_GOAL_SPECIFIED:
            bridge->conditions.goal_specified = value;
            break;
        case OMNI_COND_CAUSES_KNOWN:
            bridge->conditions.causes_known = value;
            break;
        case OMNI_COND_FORWARD_CONFIDENT:
            bridge->conditions.forward_confident = value;
            break;
        case OMNI_COND_BACKWARD_CONFIDENT:
            bridge->conditions.backward_confident = value;
            break;
        case OMNI_COND_LATERAL_RELEVANT:
            bridge->conditions.lateral_relevant = value;
            break;
        case OMNI_COND_CONFLICTING_EVIDENCE:
            bridge->conditions.conflicting_evidence = value;
            break;
        case OMNI_COND_TIME_CONSTRAINED:
            bridge->conditions.time_constrained = value;
            break;
        default:
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "invalid condition type: %d", condition);
            return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_logic_get_conditions(omni_logic_bridge_t* bridge,
                               omni_logic_conditions_t* conditions) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_get_condi", 0.0f);


    NIMCP_CHECK_THROW(bridge && conditions, NIMCP_ERROR_INVALID_PARAM, "bridge or conditions is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(conditions, &bridge->conditions, sizeof(omni_logic_conditions_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

bool omni_logic_should_forward_chain(omni_logic_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_should_fo", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = (bridge->conditions.causes_known && !bridge->conditions.goal_specified);
    bridge->stats.forward_selected += result ? 1 : 0;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

bool omni_logic_should_backward_chain(omni_logic_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_should_ba", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = (bridge->conditions.goal_specified && !bridge->conditions.causes_known);
    bridge->stats.backward_selected += result ? 1 : 0;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

bool omni_logic_should_switch_direction(omni_logic_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_should_sw", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->conditions.conflicting_evidence;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

/* ============================================================================
 * Rule Management API
 * ============================================================================ */

int omni_logic_add_rule(omni_logic_bridge_t* bridge,
                         const char* name,
                         omni_logic_type_t type,
                         uint32_t gate_id,
                         uint32_t* rule_id) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_add_rule", 0.0f);


    NIMCP_CHECK_THROW(bridge && name, NIMCP_ERROR_INVALID_PARAM, "bridge or name is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_rules >= bridge->max_rules) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "max rules exceeded: %u", bridge->max_rules);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    omni_logic_rule_t* rule = &bridge->rules[bridge->num_rules];
    rule->rule_id = bridge->num_rules + 1;
    strncpy(rule->name, name, sizeof(rule->name) - 1);
    rule->name[sizeof(rule->name) - 1] = '\0';
    rule->type = type;
    rule->gate_id = gate_id;
    rule->confidence_threshold = bridge->config.confidence_threshold;
    rule->active = true;

    if (rule_id) *rule_id = rule->rule_id;
    bridge->num_rules++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_logic_remove_rule(omni_logic_bridge_t* bridge,
                            uint32_t rule_id) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_remove_ru", 0.0f);


    NIMCP_CHECK_THROW(bridge && rule_id != 0, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL or rule_id is 0");

    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->num_rules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_rules > 256) {
            omni_logic_bridge_heartbeat("omni_logic_b_loop",
                             (float)(i + 1) / (float)bridge->num_rules);
        }

        if (bridge->rules[i].rule_id == rule_id) {
            bridge->rules[i].active = false;
            nimcp_mutex_unlock(bridge->base.mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW(NIMCP_ERROR_NOT_FOUND, "rule_id %u not found", rule_id);
    return NIMCP_ERROR_NOT_FOUND;
}

int omni_logic_evaluate_rule(omni_logic_bridge_t* bridge,
                              uint32_t rule_id,
                              bool* result) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_evaluate_", 0.0f);


    NIMCP_CHECK_THROW(bridge && result && rule_id != 0, NIMCP_ERROR_INVALID_PARAM, "bridge/result is NULL or rule_id is 0");

    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->num_rules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_rules > 256) {
            omni_logic_bridge_heartbeat("omni_logic_b_loop",
                             (float)(i + 1) / (float)bridge->num_rules);
        }

        if (bridge->rules[i].rule_id == rule_id && bridge->rules[i].active) {
            /* Simple evaluation based on rule type and current state */
            float confidence = 0.0f;
            switch (bridge->rules[i].type) {
                case OMNI_LOGIC_DEDUCTIVE:
                    confidence = bridge->omni_effects.forward_confidence;
                    break;
                case OMNI_LOGIC_ABDUCTIVE:
                    confidence = bridge->omni_effects.backward_confidence;
                    break;
                case OMNI_LOGIC_ANALOGICAL:
                    confidence = bridge->omni_effects.lateral_confidence;
                    break;
                default:
                    confidence = (bridge->omni_effects.forward_confidence +
                                  bridge->omni_effects.backward_confidence) / 2.0f;
                    break;
            }
            *result = (confidence >= bridge->rules[i].confidence_threshold);
            nimcp_mutex_unlock(bridge->base.mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW(NIMCP_ERROR_NOT_FOUND, "rule_id %u not found or inactive", rule_id);
    return NIMCP_ERROR_NOT_FOUND;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int omni_logic_get_omni_effects(omni_logic_bridge_t* bridge,
                                 omni_to_logic_effects_t* effects) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_get_omni_", 0.0f);


    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_INVALID_PARAM, "bridge or effects is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->omni_effects, sizeof(omni_to_logic_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_logic_get_logic_effects(omni_logic_bridge_t* bridge,
                                  logic_to_omni_effects_t* effects) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_get_logic", 0.0f);


    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_INVALID_PARAM, "bridge or effects is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->logic_effects, sizeof(logic_to_omni_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_logic_get_stats(omni_logic_bridge_t* bridge,
                          omni_logic_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_INVALID_PARAM, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(omni_logic_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_logic_reset_stats(omni_logic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_reset_sta", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_logic_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

static nimcp_error_t handle_logic_direction_switch(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_logic_bridge_t* bridge = (omni_logic_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge && msg, NIMCP_ERROR_INVALID_PARAM, "bridge or msg is NULL");

    omni_logic_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_logic_gate_result(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_logic_bridge_t* bridge = (omni_logic_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge && msg, NIMCP_ERROR_INVALID_PARAM, "bridge or msg is NULL");

    /* Process logic gate result */
    omni_logic_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_logic_connect_bio_async(omni_logic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_connect_b", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    if (bridge->bio_async_connected) return NIMCP_SUCCESS;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_OMNI_LOGIC_BRIDGE,
        .module_name = "omni_logic_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "failed to register bio-async module");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    bridge->bio_context = ctx;

    bio_router_register_handler(ctx, BIO_MSG_OMNI_DIRECTION_SWITCH,
                                 handle_logic_direction_switch);
    bio_router_register_handler(ctx, BIO_MSG_LOGIC_GATE_RESULT,
                                 handle_logic_gate_result);

    bridge->bio_async_connected = true;
    return NIMCP_SUCCESS;
}

int omni_logic_disconnect_bio_async(omni_logic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_disconnec", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    if (!bridge->bio_async_connected) return NIMCP_SUCCESS;

    if (bridge->bio_context) {
        bio_router_unregister_module(bridge->bio_context);
        bridge->bio_context = NULL;
    }

    bridge->bio_async_connected = false;
    return NIMCP_SUCCESS;
}

bool omni_logic_is_bio_async_connected(const omni_logic_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_logic_bridge_heartbeat("omni_logic_b_omni_logic_is_bio_as", 0.0f);


    return bridge->bio_async_connected;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_logic_type_to_string(omni_logic_type_t type) {
    switch (type) {
        case OMNI_LOGIC_DEDUCTIVE: return "DEDUCTIVE";
        case OMNI_LOGIC_ABDUCTIVE: return "ABDUCTIVE";
        case OMNI_LOGIC_INDUCTIVE: return "INDUCTIVE";
        case OMNI_LOGIC_ANALOGICAL: return "ANALOGICAL";
        case OMNI_LOGIC_COMBINED: return "COMBINED";
        default: return "UNKNOWN";
    }
}

const char* omni_logic_direction_to_string(omni_infer_direction_t direction) {
    switch (direction) {
        case OMNI_INFER_FORWARD: return "FORWARD";
        case OMNI_INFER_BACKWARD: return "BACKWARD";
        case OMNI_INFER_LATERAL: return "LATERAL";
        case OMNI_INFER_HIERARCHICAL_UP: return "HIERARCHICAL_UP";
        case OMNI_INFER_HIERARCHICAL_DOWN: return "HIERARCHICAL_DOWN";
        case OMNI_INFER_WEIGHTED_COMBINE: return "WEIGHTED_COMBINE";
        case OMNI_INFER_SEQUENTIAL: return "SEQUENTIAL";
        default: return "UNKNOWN";
    }
}

const char* omni_logic_condition_to_string(omni_logic_condition_t condition) {
    switch (condition) {
        case OMNI_COND_GOAL_SPECIFIED: return "GOAL_SPECIFIED";
        case OMNI_COND_CAUSES_KNOWN: return "CAUSES_KNOWN";
        case OMNI_COND_FORWARD_CONFIDENT: return "FORWARD_CONFIDENT";
        case OMNI_COND_BACKWARD_CONFIDENT: return "BACKWARD_CONFIDENT";
        case OMNI_COND_LATERAL_RELEVANT: return "LATERAL_RELEVANT";
        case OMNI_COND_CONFLICTING_EVIDENCE: return "CONFLICTING_EVIDENCE";
        case OMNI_COND_TIME_CONSTRAINED: return "TIME_CONSTRAINED";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void omni_logic_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_omni_logic_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int omni_logic_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_logic_bridge_training_begin: NULL argument");
        return -1;
    }
    omni_logic_bridge_heartbeat_instance(NULL, "omni_logic_bridge_training_begin", 0.0f);
    return 0;
}

int omni_logic_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_logic_bridge_training_end: NULL argument");
        return -1;
    }
    omni_logic_bridge_heartbeat_instance(NULL, "omni_logic_bridge_training_end", 1.0f);
    return 0;
}

int omni_logic_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_logic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_logic_bridge_heartbeat_instance(NULL, "omni_logic_bridge_training_step", progress);
    return 0;
}
