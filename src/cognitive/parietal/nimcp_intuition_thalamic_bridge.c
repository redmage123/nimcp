/**
 * @file nimcp_intuition_thalamic_bridge.c
 * @brief Intuition-Thalamic Bridge Implementation
 */

#include "cognitive/parietal/nimcp_intuition_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(intuition_thalamic_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from intuition_thalamic_bridge module (instance-level) */
static inline void intuition_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_intuition_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_intuition_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_intuition_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "INTUITION_THALAMIC_BRIDGE"


struct intuition_thalamic_bridge {
    bridge_base_t base;
    intuition_system_t* intuition;
    thalamic_router_t* router;
    intuition_thalamic_config_t config;
    intuition_thalamic_stats_t stats;
    float attention_weight;
    float signal_type_boosts[8]; /* Boost per signal type */

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(intuition_thalamic_bridge)

intuition_thalamic_config_t intuition_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_intuition_thalamic_d", 0.0f);


    intuition_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_priority_routing = true,
        .enable_broadcast = true,
        .min_confidence_threshold = NIMCP_CONFIDENCE_LOW,
        .min_attention_threshold = 0.2f,
        .emotional_attention_boost = 0.2f,
        .novelty_attention_boost = 0.15f,
        .num_hunch_targets = 0,
        .num_insight_targets = 0,
        .num_meta_targets = 0
    };
    memset(cfg.default_hunch_targets, 0, sizeof(cfg.default_hunch_targets));
    memset(cfg.default_insight_targets, 0, sizeof(cfg.default_insight_targets));
    memset(cfg.default_meta_targets, 0, sizeof(cfg.default_meta_targets));
    return cfg;
}

intuition_thalamic_bridge_t* intuition_thalamic_bridge_create(
    intuition_system_t* intuition,
    thalamic_router_t* router,
    const intuition_thalamic_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_create", 0.0f);


    intuition_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(intuition_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    if (bridge_base_init(&bridge->base, 0, "intuition_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }
    bridge->intuition = intuition;
    bridge->router = router;
    bridge->config = config ? *config : intuition_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Set default attention boosts for each signal type */
    bridge->signal_type_boosts[0] = INTUITION_ATTENTION_HUNCH_DEFAULT;
    bridge->signal_type_boosts[1] = INTUITION_ATTENTION_INSIGHT_DEFAULT;
    bridge->signal_type_boosts[2] = INTUITION_ATTENTION_ANALOGY_DEFAULT;
    bridge->signal_type_boosts[3] = INTUITION_ATTENTION_HYPOTHESIS_DEFAULT;
    bridge->signal_type_boosts[4] = INTUITION_ATTENTION_BLEND_DEFAULT;
    bridge->signal_type_boosts[5] = INTUITION_ATTENTION_COUNTERFACTUAL_DEFAULT;
    bridge->signal_type_boosts[6] = INTUITION_ATTENTION_META_DEFAULT;
    bridge->signal_type_boosts[7] = 0.5f; /* extrapolation default */

    return bridge;
}

void intuition_thalamic_bridge_destroy(intuition_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "intuition_thalamic");
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int intuition_thalamic_bridge_reset(intuition_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int intuition_thalamic_route_hunch(
    intuition_thalamic_bridge_t* bridge,
    const hunch_t* hunch
) {
    if (!bridge || !hunch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_thalamic_route_hunch: required parameter is NULL (bridge, hunch)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_intuition_thalamic_r", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, hunch, sizeof(*hunch));

    nimcp_mutex_lock(bridge->base.mutex);

    float confidence = hunch->score.confidence;
    if (bridge->config.enable_attention_gating && confidence < bridge->config.min_confidence_threshold) {
        bridge->stats.signals_dropped++;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    bridge->stats.hunches_routed++;
    bridge->stats.signals_routed++;
    bridge->stats.avg_confidence = (bridge->stats.avg_confidence * (bridge->stats.signals_routed - 1) +
                                    confidence) / bridge->stats.signals_routed;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int intuition_thalamic_route_insight(
    intuition_thalamic_bridge_t* bridge,
    const void* insight,
    float novelty
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_thalamic_route_insight: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_intuition_thalamic_r", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, insight, sizeof(*insight));

    nimcp_mutex_lock(bridge->base.mutex);

    float effective_attention = bridge->attention_weight;
    if (novelty > 0.5f) {
        effective_attention += bridge->config.novelty_attention_boost;
    }

    bridge->stats.insights_routed++;
    bridge->stats.signals_routed++;
    bridge->stats.avg_attention_weight = (bridge->stats.avg_attention_weight * (bridge->stats.signals_routed - 1) +
                                          effective_attention) / bridge->stats.signals_routed;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int intuition_thalamic_route_analogy(
    intuition_thalamic_bridge_t* bridge,
    const void* analogy,
    float strength
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_thalamic_route_analogy: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_intuition_thalamic_r", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, analogy, sizeof(*analogy));

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->config.enable_attention_gating && strength < bridge->config.min_confidence_threshold) {
        bridge->stats.signals_dropped++;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    bridge->stats.analogies_routed++;
    bridge->stats.signals_routed++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int intuition_thalamic_route_hypothesis(
    intuition_thalamic_bridge_t* bridge,
    const hypogen_theory_t* theory
) {
    if (!bridge || !theory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_thalamic_route_hypothesis: required parameter is NULL (bridge, theory)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_intuition_thalamic_r", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, theory, sizeof(*theory));

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->stats.hypotheses_routed++;
    bridge->stats.signals_routed++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int intuition_thalamic_route_blend(
    intuition_thalamic_bridge_t* bridge,
    const void* blend,
    float creativity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_thalamic_route_blend: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_intuition_thalamic_r", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, blend, sizeof(*blend));

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->stats.blends_routed++;
    bridge->stats.signals_routed++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int intuition_thalamic_route_meta(
    intuition_thalamic_bridge_t* bridge,
    const void* meta_signal
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_thalamic_route_meta: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_intuition_thalamic_r", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, meta_signal, sizeof(*meta_signal));

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->stats.meta_signals_routed++;
    bridge->stats.signals_routed++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int intuition_thalamic_route_signal(
    intuition_thalamic_bridge_t* bridge,
    const intuition_signal_t* signal,
    const intuition_routing_target_t* targets,
    uint32_t num_targets
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_thalamic_route_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_intuition_thalamic_r", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, signal, sizeof(*signal));

    nimcp_mutex_lock(bridge->base.mutex);

    float effective_attention = bridge->attention_weight;

    /* Apply emotional boost */
    if (signal->emotional_valence > 0.3f || signal->emotional_valence < -0.3f) {
        effective_attention += bridge->config.emotional_attention_boost;
    }

    /* Apply novelty boost */
    if (signal->novelty > 0.5f) {
        effective_attention += bridge->config.novelty_attention_boost;
    }

    /* Check gating */
    if (bridge->config.enable_attention_gating) {
        if (signal->confidence < bridge->config.min_confidence_threshold) {
            bridge->stats.signals_dropped++;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
        if (effective_attention < bridge->config.min_attention_threshold) {
            bridge->stats.signals_dropped++;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    /* Check for priority bypass */
    if (bridge->config.enable_priority_routing && signal->confidence > 0.9f) {
        bridge->stats.signals_bypassed++;
    }

    bridge->stats.signals_routed++;
    bridge->stats.avg_attention_weight = (bridge->stats.avg_attention_weight * (bridge->stats.signals_routed - 1) +
                                          effective_attention) / bridge->stats.signals_routed;
    bridge->stats.avg_confidence = (bridge->stats.avg_confidence * (bridge->stats.signals_routed - 1) +
                                    signal->confidence) / bridge->stats.signals_routed;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int intuition_thalamic_set_attention(
    intuition_thalamic_bridge_t* bridge,
    float attention
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_intuition_thalamic_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int intuition_thalamic_get_attention(
    const intuition_thalamic_bridge_t* bridge,
    float* attention
) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_intuition_thalamic_g", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, attention, sizeof(*attention));

    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int intuition_thalamic_boost_attention(
    intuition_thalamic_bridge_t* bridge,
    uint32_t signal_type,
    float boost
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_thalamic_boost_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_intuition_thalamic_b", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Map signal type to array index */
    uint32_t index = 0;
    switch (signal_type) {
        case INTUITION_SIGNAL_HUNCH:        index = 0; break;
        case INTUITION_SIGNAL_INSIGHT:      index = 1; break;
        case INTUITION_SIGNAL_ANALOGY:      index = 2; break;
        case INTUITION_SIGNAL_HYPOTHESIS:   index = 3; break;
        case INTUITION_SIGNAL_BLEND:        index = 4; break;
        case INTUITION_SIGNAL_COUNTERFACTUAL: index = 5; break;
        case INTUITION_SIGNAL_META:         index = 6; break;
        case INTUITION_SIGNAL_EXTRAPOLATION: index = 7; break;
        default:
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "intuition_thalamic_boost_attention: operation failed");
            return -1;
    }

    bridge->signal_type_boosts[index] += boost;
    if (bridge->signal_type_boosts[index] > 1.0f) {
        bridge->signal_type_boosts[index] = 1.0f;
    }
    if (bridge->signal_type_boosts[index] < 0.0f) {
        bridge->signal_type_boosts[index] = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int intuition_thalamic_bridge_get_stats(
    const intuition_thalamic_bridge_t* bridge,
    intuition_thalamic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_get_stats", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, stats, sizeof(*stats));

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

void intuition_thalamic_bridge_reset_stats(intuition_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_reset_stats", 0.0f);


    if (bridge) {
        nimcp_mutex_lock(bridge->base.mutex);
        memset(&bridge->stats, 0, sizeof(bridge->stats));
        nimcp_mutex_unlock(bridge->base.mutex);
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int intuition_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    intuition_thalamic_bridge_heartbeat("intuition_th_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Intuition_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                intuition_thalamic_bridge_heartbeat("intuition_th_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Intuition_Thalamic_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Intuition_Thalamic_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

//=============================================================================
// Instance Health Agent Setter (B23 Upgrade)
//=============================================================================

void intuition_thalamic_bridge_set_instance_health_agent(
    intuition_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B23 Upgrade)
//=============================================================================

int intuition_thalamic_bridge_training_begin(intuition_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "intuition_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    intuition_thalamic_bridge_heartbeat_instance(bridge->health_agent, "intuition_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int intuition_thalamic_bridge_training_end(intuition_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "intuition_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    intuition_thalamic_bridge_heartbeat_instance(bridge->health_agent, "intuition_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int intuition_thalamic_bridge_training_step(intuition_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "intuition_thalamic_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "intuition_thalamic_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "intuition_thalamic_bridge_training_step");
    intuition_thalamic_bridge_heartbeat_instance(bridge->health_agent, "intuition_thalamic_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
