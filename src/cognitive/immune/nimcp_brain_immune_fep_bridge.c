/**
 * @file nimcp_brain_immune_fep_bridge.c
 * @brief FEP Bridge for Brain Immune System Implementation
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between brain immune system and FEP
 * WHY:  Model immune system as allostatic regulator minimizing free energy
 * HOW:  Cytokines map to precision weights, inflammation maps to prediction error
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines (IL-1, IL-6, TNF) increase FEP precision
 * - Anti-inflammatory cytokines (IL-10) decrease FEP precision
 * - Inflammation levels map to prediction error magnitude
 * - FEP free energy guides threat assessment confidence
 */

#include "cognitive/immune/nimcp_brain_immune_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "brain_immune_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(brain_immune_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_brain_immune_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_brain_immune_fep_bridge_mesh_registry = NULL;

nimcp_error_t brain_immune_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_brain_immune_fep_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "brain_immune_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "brain_immune_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_brain_immune_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_brain_immune_fep_bridge_mesh_registry = registry;
    return err;
}

void brain_immune_fep_bridge_mesh_unregister(void) {
    if (g_brain_immune_fep_bridge_mesh_registry && g_brain_immune_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_brain_immune_fep_bridge_mesh_registry, g_brain_immune_fep_bridge_mesh_id);
        g_brain_immune_fep_bridge_mesh_id = 0;
        g_brain_immune_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from brain_immune_fep_bridge module (instance-level) */
static inline void brain_immune_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_brain_immune_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_brain_immune_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_brain_immune_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Compute precision from cytokine concentrations
 *
 * WHAT: Calculate overall precision modulation from active cytokines
 * WHY:  Pro-inflammatory cytokines increase precision, anti-inflammatory decrease
 * HOW:  Weighted sum of cytokine concentrations with type-specific factors
 */
static float compute_cytokine_precision(const brain_immune_system_t* immune) {
    if (!immune) return 1.0f;

    float precision = 1.0f;
    float il1_total = 0.0f;
    float il6_total = 0.0f;
    float tnf_total = 0.0f;
    float ifn_total = 0.0f;
    float il10_total = 0.0f;

    /* Aggregate cytokine concentrations by type */
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->cytokine_count > 256) {
            brain_immune_fep_bridge_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)immune->cytokine_count);
        }

        const brain_cytokine_t* cyt = &immune->cytokines[i];
        switch (cyt->type) {
            case BRAIN_CYTOKINE_IL1:
                il1_total += cyt->concentration;
                break;
            case BRAIN_CYTOKINE_IL6:
                il6_total += cyt->concentration;
                break;
            case BRAIN_CYTOKINE_TNF:
                tnf_total += cyt->concentration;
                break;
            case BRAIN_CYTOKINE_IFN_GAMMA:
                ifn_total += cyt->concentration;
                break;
            case BRAIN_CYTOKINE_IL10:
                il10_total += cyt->concentration;
                break;
            default:
                break;
        }
    }

    /* Apply precision weights */
    precision += il1_total * (BRAIN_IMMUNE_FEP_IL1_PRECISION_WEIGHT - 1.0f);
    precision += il6_total * (BRAIN_IMMUNE_FEP_IL6_PRECISION_WEIGHT - 1.0f);
    precision += tnf_total * (BRAIN_IMMUNE_FEP_TNF_PRECISION_WEIGHT - 1.0f);
    precision += ifn_total * (BRAIN_IMMUNE_FEP_IFN_PRECISION_WEIGHT - 1.0f);

    /* IL-10 reduces precision (anti-inflammatory) */
    precision *= fmaxf(0.1f, 1.0f - il10_total * (1.0f - BRAIN_IMMUNE_FEP_IL10_PRECISION_WEIGHT));

    /* Clamp to valid range */
    return fmaxf(0.1f, fminf(3.0f, precision));
}

/**
 * @brief Compute prediction error from inflammation level
 *
 * WHAT: Map inflammation severity to prediction error magnitude
 * WHY:  Higher inflammation indicates larger prediction failures
 * HOW:  Scale inflammation level to prediction error range
 */
static float compute_inflammation_error(const brain_immune_system_t* immune,
                                         float scale) {
    if (!immune) return 0.0f;

    float max_error = 0.0f;

    /* Find highest inflammation level across all sites */
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            brain_immune_fep_bridge_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        const brain_inflammation_site_t* site = &immune->inflammation_sites[i];
        float error = 0.0f;

        switch (site->level) {
            case INFLAMMATION_NONE:
                error = 0.0f;
                break;
            case INFLAMMATION_LOCAL:
                error = 1.0f * scale;
                break;
            case INFLAMMATION_REGIONAL:
                error = 3.0f * scale;
                break;
            case INFLAMMATION_SYSTEMIC:
                error = 6.0f * scale;
                break;
            case INFLAMMATION_STORM:
                error = BRAIN_IMMUNE_FEP_MAX_PREDICTION_ERROR;
                break;
            default:
                error = 0.0f;
                break;
        }

        if (error > max_error) {
            max_error = error;
        }
    }

    return fminf(max_error, BRAIN_IMMUNE_FEP_MAX_PREDICTION_ERROR);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int brain_immune_fep_default_config(brain_immune_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_def", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->cytokine_precision_scale = 1.0f;
    config->enable_precision_modulation = true;

    config->inflammation_error_scale = BRAIN_IMMUNE_FEP_INFLAMMATION_SCALE;
    config->enable_inflammation_errors = true;

    config->fep_threat_threshold = BRAIN_IMMUNE_FEP_THREAT_FE_THRESHOLD;
    config->fep_tolerance_threshold = BRAIN_IMMUNE_FEP_TOLERANCE_FE_THRESHOLD;
    config->enable_fep_guided_responses = true;

    config->enable_bio_async = true;
    config->enable_logging = true;

    return 0;
}

brain_immune_fep_bridge_t* brain_immune_fep_create(
    const brain_immune_fep_config_t* config,
    brain_immune_system_t* immune_system,
    fep_system_t* fep_system
) {
    if (!immune_system || !fep_system) {
        NIMCP_LOGGING_ERROR("Cannot create bridge: null immune or FEP system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_fep_create: required parameter is NULL (immune_system, fep_system)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_cre", 0.0f);


    brain_immune_fep_bridge_t* bridge = nimcp_malloc(sizeof(brain_immune_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate brain immune FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }
    memset(bridge, 0, sizeof(brain_immune_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        brain_immune_fep_default_config(&bridge->config);
    }

    /* Connect systems */
    bridge->immune_system = immune_system;
    bridge->fep_system = fep_system;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "brain_immune_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_immune_fep_create: bridge->base is NULL");
        return NULL;
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Created brain immune FEP bridge");
    }

    return bridge;
}

void brain_immune_fep_destroy(brain_immune_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_des", 0.0f);


    if (bridge->base.bio_async_enabled) {
        brain_immune_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed brain immune FEP bridge");
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int brain_immune_fep_update(brain_immune_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_upd", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->immune_system || !bridge->fep_system) {
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute immune -> FEP effects */
    if (bridge->config.enable_precision_modulation) {
        float precision = compute_cytokine_precision(bridge->immune_system);
        precision *= bridge->config.cytokine_precision_scale;

        bridge->immune_effects.precision_modulation = precision;
        bridge->state.cytokine_modulations++;
    }

    if (bridge->config.enable_inflammation_errors) {
        float error = compute_inflammation_error(
            bridge->immune_system,
            bridge->config.inflammation_error_scale
        );

        bridge->immune_effects.prediction_error_magnitude = error;
        if (error > 0.0f) {
            bridge->state.inflammation_signals++;
        }
    }

    /* Compute FEP -> immune effects */
    if (bridge->config.enable_fep_guided_responses) {
        float fe = fep_get_free_energy(bridge->fep_system);

        /* High free energy indicates potential threat */
        if (fe > bridge->config.fep_threat_threshold) {
            float denom = bridge->config.fep_threat_threshold - bridge->config.fep_tolerance_threshold;
            if (fabsf(denom) < 1e-10f) {
                bridge->fep_effects.threat_assessment = 1.0f;
            } else {
                bridge->fep_effects.threat_assessment = fminf(1.0f,
                    (fe - bridge->config.fep_tolerance_threshold) / denom);
            }
            bridge->fep_effects.response_magnitude = bridge->fep_effects.threat_assessment;
            bridge->state.fep_guided_responses++;
        } else if (fe < bridge->config.fep_tolerance_threshold) {
            bridge->fep_effects.tolerance_confidence =
                (fabsf(bridge->config.fep_tolerance_threshold) > 1e-10f) ?
                (1.0f - (fe / bridge->config.fep_tolerance_threshold)) : 1.0f;
            bridge->fep_effects.threat_assessment = 0.0f;
        } else {
            /* Uncertain region */
            bridge->fep_effects.threat_assessment = 0.5f;
            bridge->fep_effects.tolerance_confidence = 0.5f;
        }

        /* Memory formation signal based on prediction error magnitude */
        bridge->fep_effects.memory_formation_signal =
            bridge->immune_effects.prediction_error_magnitude /
            BRAIN_IMMUNE_FEP_MAX_PREDICTION_ERROR;
    }

    /* Update running averages */
    float alpha = 0.01f;
    bridge->state.avg_precision_modulation =
        (1.0f - alpha) * bridge->state.avg_precision_modulation +
        alpha * bridge->immune_effects.precision_modulation;
    bridge->state.avg_prediction_error =
        (1.0f - alpha) * bridge->state.avg_prediction_error +
        alpha * bridge->immune_effects.prediction_error_magnitude;

    bridge->state.total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int brain_immune_fep_apply_to_immune(brain_immune_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_app", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->immune_system, NIMCP_ERROR_NULL_POINTER, "bridge or immune_system is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* FEP-guided response modulation would be applied here */
    /* Currently the effects are computed and stored in fep_effects */
    /* External code can query these to modulate immune responses */

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int brain_immune_fep_apply_to_fep(brain_immune_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_app", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Cytokine-driven precision modulation would be applied here */
    /* Currently the effects are computed and stored in immune_effects */
    /* External code can query these to modulate FEP precision */

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

float brain_immune_fep_get_precision_modulation(brain_immune_fep_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float precision = bridge->immune_effects.precision_modulation;
    nimcp_mutex_unlock(bridge->base.mutex);

    return precision;
}

float brain_immune_fep_get_prediction_error(brain_immune_fep_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float error = bridge->immune_effects.prediction_error_magnitude;
    nimcp_mutex_unlock(bridge->base.mutex);

    return error;
}

int brain_immune_fep_assess_threat(
    brain_immune_fep_bridge_t* bridge,
    uint32_t antigen_id,
    float* threat_prob
) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_ass", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && threat_prob, NIMCP_ERROR_NULL_POINTER, "bridge or threat_prob is NULL");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "fep_system is NULL");

    /* Get antigen outside lock to avoid external call under lock (copy to avoid dangling pointer) */
    brain_antigen_t antigen_copy;
    const brain_antigen_t* antigen = NULL;
    if (bridge->immune_system &&
        brain_immune_get_antigen_copy(bridge->immune_system, antigen_id, &antigen_copy) == 0) {
        antigen = &antigen_copy;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current free energy as basis for threat assessment */
    float fe = fep_get_free_energy(bridge->fep_system);

    /* Combine FEP free energy with current immune state */
    float base_threat = bridge->fep_effects.threat_assessment;
    float precision_factor = bridge->immune_effects.precision_modulation;

    /* Higher precision = more confident in threat assessment */
    *threat_prob = base_threat * precision_factor;
    *threat_prob = fmaxf(0.0f, fminf(1.0f, *threat_prob));

    /* Adjust based on antigen if we can access it */
    if (antigen) {
        /* Higher severity antigens get boosted threat probability */
        *threat_prob = fmaxf(*threat_prob, antigen->severity / 10.0f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int brain_immune_fep_get_stats(
    const brain_immune_fep_bridge_t* bridge,
    brain_immune_fep_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_get", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    stats->total_updates = bridge->state.total_updates;
    stats->precision_modulations = bridge->state.cytokine_modulations;
    stats->prediction_errors_generated = bridge->state.inflammation_signals;
    stats->fep_guided_actions = bridge->state.fep_guided_responses;
    stats->current_precision_modulation = bridge->immune_effects.precision_modulation;
    stats->current_prediction_error = bridge->immune_effects.prediction_error_magnitude;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int brain_immune_fep_connect_bio_async(brain_immune_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_con", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_IMMUNE_BRIDGE,
        .module_name = "brain_immune_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_INFO("Connected brain immune FEP bridge to bio-async router");
        }
    }

    return 0;
}

int brain_immune_fep_disconnect_bio_async(brain_immune_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_dis", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Disconnected brain immune FEP bridge from bio-async router");
    }

    return 0;
}

bool brain_immune_fep_is_bio_async_connected(const brain_immune_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_is_", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about brain immune FEP bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int brain_immune_fep_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    brain_immune_fep_bridge_heartbeat("brain_immune_brain_immune_fep_que", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Brain_Immune_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                brain_immune_fep_bridge_heartbeat("brain_immune_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Brain immune FEP bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Brain_Immune_FEP_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Brain_Immune_FEP_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void brain_immune_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_brain_immune_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int brain_immune_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    /* TODO: Pass bridge instance as health agent when instance tracking is implemented */
    brain_immune_fep_bridge_heartbeat_instance(NULL, "brain_immune_fep_bridge_training_begin", 0.0f);
    return 0;
}

int brain_immune_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_fep_bridge_training_end: NULL argument");
        return -1;
    }
    /* TODO: Pass bridge instance as health agent when instance tracking is implemented */
    brain_immune_fep_bridge_heartbeat_instance(NULL, "brain_immune_fep_bridge_training_end", 1.0f);
    return 0;
}

int brain_immune_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* TODO: Pass bridge instance as health agent when instance tracking is implemented */
    brain_immune_fep_bridge_heartbeat_instance(NULL, "brain_immune_fep_bridge_training_step", progress);
    return 0;
}
