/**
 * @file nimcp_intuition_substrate_bridge.c
 * @brief Intuition-Neural Substrate Bridge Implementation
 *
 * Links intuitive reasoning to metabolic state for biologically-realistic
 * modulation of insight depth, pattern recognition, and creative leaps.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/parietal/nimcp_intuition_substrate_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(intuition_substrate_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from intuition_substrate_bridge module (instance-level) */
static inline void intuition_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_intuition_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_intuition_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_intuition_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "INTUITION_SUBSTRATE_BRIDGE"


struct intuition_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    intuition_system_t* intuition;
    neural_substrate_t* substrate;
    intuition_substrate_config_t config;
    intuition_substrate_effects_t effects;
    intuition_substrate_stats_t stats;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(intuition_substrate_bridge)

intuition_substrate_config_t intuition_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    intuition_substrate_bridge_heartbeat("intuition_su_intuition_substrate_", 0.0f);


    intuition_substrate_config_t cfg = {
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_stress_modulation = true,
        .enable_bio_async = false,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .stress_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.2f
    };
    return cfg;
}

intuition_substrate_bridge_t* intuition_substrate_bridge_create(
    intuition_system_t* intuition,
    neural_substrate_t* substrate,
    const intuition_substrate_config_t* config
) {
    if (!substrate) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    intuition_substrate_bridge_heartbeat("intuition_su_create", 0.0f);


    intuition_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(intuition_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->intuition = intuition;
    bridge->substrate = substrate;
    bridge->config = config ? *config : intuition_substrate_default_config();
    bridge->effects.insight_depth = 1.0f;
    bridge->effects.intuition_accuracy = 1.0f;
    bridge->effects.processing_speed = 1.0f;
    bridge->effects.abstraction_capacity = 1.0f;
    bridge->effects.creative_leap_potential = 1.0f;
    bridge->effects.counterfactual_capacity = 1.0f;
    bridge->effects.meta_reasoning_depth = 1.0f;
    bridge->effects.overall_capacity = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    if (bridge_base_init(&bridge->base, 0, "intuition_substrate") != 0) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "intuition_substrate");
    return bridge;
}

void intuition_substrate_bridge_destroy(intuition_substrate_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    intuition_substrate_bridge_heartbeat("intuition_su_destroy", 0.0f);


    if (bridge) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
    }
}

int intuition_substrate_bridge_reset(intuition_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_substrate_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_substrate_bridge_heartbeat("intuition_su_reset", 0.0f);


    bridge->effects.insight_depth = 1.0f;
    bridge->effects.intuition_accuracy = 1.0f;
    bridge->effects.processing_speed = 1.0f;
    bridge->effects.abstraction_capacity = 1.0f;
    bridge->effects.creative_leap_potential = 1.0f;
    bridge->effects.counterfactual_capacity = 1.0f;
    bridge->effects.meta_reasoning_depth = 1.0f;
    bridge->effects.overall_capacity = 1.0f;
    bridge->update_count = 0;
    return 0;
}

int intuition_substrate_bridge_update(intuition_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_substrate_bridge_update: required parameter is NULL (bridge, bridge->substrate)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    intuition_substrate_bridge_heartbeat("intuition_su_update", 0.0f);


    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "intuition_substrate_bridge_update");
    BRIDGE_LGSS_GATE(bridge, "intuition_substrate_bridge_update");

    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "intuition_substrate_bridge_update: validation failed");
        return -1;
    }

    float atp = metabolic.atp_level;
    float fatigue = 1.0f - metabolic.metabolic_capacity;
    float min_cap = bridge->config.min_capacity;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* Track statistics */
    if (isfinite(atp) && bridge->update_count < UINT32_MAX) {
        bridge->stats.avg_atp_level = (bridge->stats.avg_atp_level * bridge->update_count + atp) / (bridge->update_count + 1);
    }
    if (isfinite(fatigue) && bridge->update_count < UINT32_MAX) {
        bridge->stats.avg_fatigue_level = (bridge->stats.avg_fatigue_level * bridge->update_count + fatigue) / (bridge->update_count + 1);
    }

    if (atp < INTUITION_ATP_IMPAIRED_THRESHOLD) {
        bridge->stats.low_atp_events++;
    }
    if (fatigue > INTUITION_FATIGUE_SEVERE_THRESHOLD) {
        bridge->stats.high_fatigue_events++;
    }

    /* ATP modulation - affects insight depth, accuracy, abstraction, creativity */
    if (bridge->config.enable_atp_modulation) {
        float atp_factor = atp * bridge->config.atp_sensitivity;

        /* Insight depth scales with ATP - low ATP means shallow pattern matching */
        if (atp >= INTUITION_ATP_OPTIMAL_THRESHOLD) {
            bridge->effects.insight_depth = 1.0f;
        } else if (atp >= INTUITION_ATP_REDUCED_THRESHOLD) {
            bridge->effects.insight_depth = nimcp_clampf(0.6f + (atp - 0.5f) * 2.0f, min_cap, 1.0f);
        } else {
            bridge->effects.insight_depth = nimcp_clampf(atp_factor, min_cap, 0.6f);
        }

        bridge->effects.intuition_accuracy = nimcp_clampf(atp_factor * 1.05f, min_cap, 1.0f);
        bridge->effects.abstraction_capacity = nimcp_clampf(atp_factor * 1.1f, min_cap, 1.0f);
        bridge->effects.creative_leap_potential = nimcp_clampf(atp_factor * 0.95f, min_cap, 1.0f);
        bridge->effects.counterfactual_capacity = nimcp_clampf(atp_factor, min_cap, 1.0f);
        bridge->effects.meta_reasoning_depth = nimcp_clampf(atp_factor * 1.05f, min_cap, 1.0f);
    }

    /* Fatigue modulation - primarily affects processing speed */
    if (bridge->config.enable_fatigue_modulation) {
        float fatigue_inverse = 1.0f - fatigue;
        float fatigue_factor = fatigue_inverse * bridge->config.fatigue_sensitivity;

        /* Processing speed decreases with fatigue */
        if (fatigue < INTUITION_FATIGUE_MILD_THRESHOLD) {
            bridge->effects.processing_speed = 1.0f;
        } else if (fatigue < INTUITION_FATIGUE_MODERATE_THRESHOLD) {
            bridge->effects.processing_speed = nimcp_clampf(0.8f - (fatigue - 0.4f) * 0.5f, min_cap, 1.0f);
        } else if (fatigue < INTUITION_FATIGUE_SEVERE_THRESHOLD) {
            bridge->effects.processing_speed = nimcp_clampf(0.6f - (fatigue - 0.6f) * 0.5f, min_cap, 0.8f);
        } else {
            bridge->effects.processing_speed = nimcp_clampf(fatigue_factor * 0.5f, min_cap, 0.5f);
        }
    }

    /* Compute overall capacity as weighted average */
    bridge->effects.overall_capacity = (
        bridge->effects.insight_depth * 0.2f +
        bridge->effects.intuition_accuracy * 0.15f +
        bridge->effects.processing_speed * 0.15f +
        bridge->effects.abstraction_capacity * 0.15f +
        bridge->effects.creative_leap_potential * 0.15f +
        bridge->effects.counterfactual_capacity * 0.1f +
        bridge->effects.meta_reasoning_depth * 0.1f
    );

    if (isfinite(bridge->effects.overall_capacity) && bridge->update_count < UINT32_MAX) {
        bridge->stats.avg_capacity = (bridge->stats.avg_capacity * bridge->update_count + bridge->effects.overall_capacity) / (bridge->update_count + 1);
    }
    bridge->update_count++;
    bridge->stats.updates++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int intuition_substrate_bridge_get_effects(
    const intuition_substrate_bridge_t* bridge,
    intuition_substrate_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_substrate_bridge_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    intuition_substrate_bridge_heartbeat("intuition_su_get_effects", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, effects, sizeof(*effects));

    return 0;
}

int intuition_substrate_bridge_apply_effects(intuition_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_substrate_bridge_heartbeat("intuition_su_apply_effects", 0.0f);


    bridge->stats.effects_applied++;
    return 0;
}

int intuition_substrate_bridge_register_bio_async(
    intuition_substrate_bridge_t* bridge,
    bio_router_t* router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_substrate_bridge_heartbeat("intuition_su_register_bio_async", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, router, sizeof(*router));

    bridge->router = router;
    bridge->bio_async_connected = (router != NULL);
    return 0;
}

int intuition_substrate_bridge_handle_message(
    intuition_substrate_bridge_t* bridge,
    const bio_message_header_t* msg
) {
    if (!bridge || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_substrate_bridge_handle_message: required parameter is NULL (bridge, msg)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, msg, sizeof(*msg));
    bridge->stats.bio_messages_received++;
    return 0;
}

int intuition_substrate_bridge_get_stats(
    const intuition_substrate_bridge_t* bridge,
    intuition_substrate_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_substrate_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    intuition_substrate_bridge_heartbeat("intuition_su_get_stats", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, stats, sizeof(*stats));

    return 0;
}

void intuition_substrate_bridge_reset_stats(intuition_substrate_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    intuition_substrate_bridge_heartbeat("intuition_su_reset_stats", 0.0f);


    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int intuition_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    intuition_substrate_bridge_heartbeat("intuition_su_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Intuition_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                intuition_substrate_bridge_heartbeat("intuition_su_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Intuition_Substrate_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Intuition_Substrate_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

//=============================================================================
// Instance Health Agent Setter (B23 Upgrade)
//=============================================================================

void intuition_substrate_bridge_set_instance_health_agent(
    intuition_substrate_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B23 Upgrade)
//=============================================================================

int intuition_substrate_bridge_training_begin(intuition_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "intuition_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    intuition_substrate_bridge_heartbeat_instance(bridge->health_agent, "intuition_substrate_bridge_training_begin", 0.0f);
    return 0;
}

int intuition_substrate_bridge_training_end(intuition_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "intuition_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    intuition_substrate_bridge_heartbeat_instance(bridge->health_agent, "intuition_substrate_bridge_training_end", 1.0f);
    return 0;
}

int intuition_substrate_bridge_training_step(intuition_substrate_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "intuition_substrate_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "intuition_substrate_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "intuition_substrate_bridge_training_step");
    intuition_substrate_bridge_heartbeat_instance(bridge->health_agent, "intuition_substrate_bridge_training_step", progress);
    return 0;
}
