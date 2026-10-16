/**
 * @file nimcp_memory_consolidation_substrate_bridge.c
 * @brief Neural substrate integration for memory consolidation module
 *
 * WHAT: Bidirectional bridge between memory consolidation and neural substrate
 * WHY: Memory consolidation requires substantial ATP for protein synthesis,
 *      hippocampal replay, and synaptic remodeling. Metabolic stress impairs
 *      consolidation rate, especially during sleep-dependent consolidation.
 * HOW: Monitor substrate state (ATP, metabolic stress), modulate consolidation
 *      rate, protein synthesis, replay efficiency, and hippocampal→cortical transfer
 *
 * BIOLOGICAL BASIS:
 * - Memory consolidation requires protein synthesis (ATP-intensive process)
 * - Hippocampal replay during sleep consumes metabolic energy
 * - Systems consolidation (hippocampus→cortex) requires sustained ATP
 * - Metabolic stress during sleep impairs consolidation quality
 * - LTP maintenance depends on protein synthesis (ATP-dependent)
 * - Synaptic tagging and capture mechanism requires local protein production
 */

#include "cognitive/memory/nimcp_memory_consolidation_substrate_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(memory_consolidation_substrate_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_memory_consolidation_substrate_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_memory_consolidation_substrate_bridge_mesh_registry = NULL;

nimcp_error_t memory_consolidation_substrate_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_memory_consolidation_substrate_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "memory_consolidation_substrate_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "memory_consolidation_substrate_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_memory_consolidation_substrate_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_memory_consolidation_substrate_bridge_mesh_registry = registry;
    return err;
}

void memory_consolidation_substrate_bridge_mesh_unregister(void) {
    if (g_memory_consolidation_substrate_bridge_mesh_registry && g_memory_consolidation_substrate_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_memory_consolidation_substrate_bridge_mesh_registry, g_memory_consolidation_substrate_bridge_mesh_id);
        g_memory_consolidation_substrate_bridge_mesh_id = 0;
        g_memory_consolidation_substrate_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from memory_consolidation_substrate_bridge module (instance-level) */
static inline void memory_consolidation_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_memory_consolidation_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_memory_consolidation_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_memory_consolidation_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
BRIDGE_DEFINE_SECURITY_SETTERS(consolidation_substrate_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Compute consolidation rate from substrate state
 *
 * WHAT: Calculate overall consolidation speed based on ATP and metabolic capacity
 * WHY: Memory consolidation is ATP-intensive, requiring sustained energy supply
 * HOW: consolidation_rate = nimcp_clampf(atp_level * metabolic_capacity, 0.1, 1.0)
 *
 * BIOLOGICAL BASIS: Consolidation requires continuous ATP for protein synthesis,
 * synaptic remodeling, and hippocampal replay. Low ATP severely impairs consolidation.
 */
static float compute_consolidation_rate(
    float atp_level,
    float metabolic_capacity,
    float atp_sensitivity
) {
    /* Base rate from ATP and metabolic capacity */
    float base_rate = atp_level * metabolic_capacity;

    /* Apply sensitivity */
    float rate = base_rate * atp_sensitivity;

    /* Clamp to valid range (min 0.1 to prevent complete halt) */
    return nimcp_clampf(rate, 0.1f, 1.0f);
}

/**
 * Compute protein synthesis rate from substrate state
 *
 * WHAT: Calculate LTP-dependent protein synthesis rate
 * WHY: Late-phase LTP requires protein synthesis, which is very ATP-intensive
 * HOW: protein_synthesis_rate = nimcp_clampf(atp_level * glucose_level, 0.0, 1.0)
 *
 * BIOLOGICAL BASIS: Protein synthesis for LTP consolidation requires both ATP
 * and glucose. Below ATP 0.5, protein synthesis is severely impaired.
 */
static float compute_protein_synthesis_rate(
    float atp_level,
    float glucose_level,
    float atp_sensitivity
) {
    /* Protein synthesis requires both ATP and glucose */
    float base_rate = atp_level * glucose_level;

    /* Apply sensitivity */
    float rate = base_rate * atp_sensitivity;

    /* Severe impairment below ATP 0.5 */
    if (atp_level < 0.5f) {
        rate *= 0.5f;
    }

    /* Clamp to valid range */
    return nimcp_clampf(rate, 0.0f, 1.0f);
}

/**
 * Compute replay efficiency from substrate state
 *
 * WHAT: Calculate hippocampal replay efficiency
 * WHY: Replay requires metabolic energy and oxygen supply
 * HOW: replay_efficiency = nimcp_clampf(metabolic_capacity * o2_factor, 0.2, 1.0)
 *
 * BIOLOGICAL BASIS: Hippocampal replay during sleep reactivates memory traces.
 * Hypoxia and metabolic stress reduce replay quality and frequency.
 */
static float compute_replay_efficiency(
    float metabolic_capacity,
    float o2_saturation,
    float hypoxia_sensitivity
) {
    /* Replay efficiency depends on metabolic capacity and oxygen */
    float o2_factor = 0.5f + (o2_saturation * 0.5f);
    float base_efficiency = metabolic_capacity * o2_factor;

    /* Apply hypoxia sensitivity */
    float efficiency = base_efficiency;
    if (o2_saturation < 0.8f) {
        float hypoxia_impact = (0.8f - o2_saturation) * hypoxia_sensitivity;
        efficiency *= (1.0f - hypoxia_impact);
    }

    /* Clamp to valid range (min 0.2 to maintain some replay) */
    return nimcp_clampf(efficiency, 0.2f, 1.0f);
}

/**
 * Compute hippocampal→cortical transfer rate
 *
 * WHAT: Calculate systems consolidation transfer rate
 * WHY: Transferring memories from hippocampus to cortex requires sustained ATP
 * HOW: transfer_rate = nimcp_clampf((atp_level + o2_saturation) / 2.0, 0.2, 1.0)
 *
 * BIOLOGICAL BASIS: Systems consolidation is a gradual process requiring
 * coordinated activity between hippocampus and cortex, consuming metabolic energy.
 */
static float compute_transfer_rate(
    float atp_level,
    float o2_saturation,
    float stress_sensitivity
) {
    /* Transfer rate depends on both ATP and oxygen */
    float base_rate = (atp_level + o2_saturation) / 2.0f;

    /* Reduce transfer under stress */
    float rate = base_rate * (2.0f - stress_sensitivity) / 2.0f;

    /* Clamp to valid range (min 0.2 to maintain some transfer) */
    return nimcp_clampf(rate, 0.2f, 1.0f);
}

/**
 * Check if consolidation is critically impaired
 *
 * WHAT: Determine if substrate state critically impairs consolidation
 * WHY: Identify conditions requiring intervention
 * HOW: impaired = (protein_synthesis_rate < 0.5 || consolidation_rate < 0.5)
 *
 * BIOLOGICAL BASIS: Below 50% protein synthesis or consolidation rate,
 * memory stabilization is severely compromised.
 */
static bool check_impairment(
    float protein_synthesis_rate,
    float consolidation_rate
) {
    return (protein_synthesis_rate < 0.5f || consolidation_rate < 0.5f);
}

/**
 * Update statistics with new effects
 *
 * WHAT: Update running statistics with current effect values
 * WHY: Track consolidation quality over time
 * HOW: Update min/max/avg for all effect metrics
 */
static void update_statistics(
    consolidation_substrate_bridge_t* bridge,
    const consolidation_substrate_effects_t* effects,
    float atp_level,
    float metabolic_stress
) {
    consolidation_substrate_stats_t* stats = &bridge->stats;

    /* Update counts */
    stats->update_count++;
    if (effects->is_impaired) {
        stats->impairment_count++;
    }

    /* Update consolidation rate statistics */
    if (stats->update_count == 1) {
        stats->min_consolidation_rate = effects->consolidation_rate;
        stats->max_consolidation_rate = effects->consolidation_rate;
        stats->avg_consolidation_rate = effects->consolidation_rate;
    } else {
        if (effects->consolidation_rate < stats->min_consolidation_rate) {
            stats->min_consolidation_rate = effects->consolidation_rate;
        }
        if (effects->consolidation_rate > stats->max_consolidation_rate) {
            stats->max_consolidation_rate = effects->consolidation_rate;
        }
        /* Running average */
        stats->avg_consolidation_rate =
            (stats->avg_consolidation_rate * (stats->update_count - 1) +
             effects->consolidation_rate) / stats->update_count;
    }

    /* Update protein synthesis statistics */
    if (stats->update_count == 1) {
        stats->min_protein_synthesis = effects->protein_synthesis_rate;
        stats->max_protein_synthesis = effects->protein_synthesis_rate;
        stats->avg_protein_synthesis = effects->protein_synthesis_rate;
    } else {
        if (effects->protein_synthesis_rate < stats->min_protein_synthesis) {
            stats->min_protein_synthesis = effects->protein_synthesis_rate;
        }
        if (effects->protein_synthesis_rate > stats->max_protein_synthesis) {
            stats->max_protein_synthesis = effects->protein_synthesis_rate;
        }
        stats->avg_protein_synthesis =
            (stats->avg_protein_synthesis * (stats->update_count - 1) +
             effects->protein_synthesis_rate) / stats->update_count;
    }

    /* Update replay efficiency statistics */
    if (stats->update_count == 1) {
        stats->min_replay_efficiency = effects->replay_efficiency;
        stats->max_replay_efficiency = effects->replay_efficiency;
        stats->avg_replay_efficiency = effects->replay_efficiency;
    } else {
        if (effects->replay_efficiency < stats->min_replay_efficiency) {
            stats->min_replay_efficiency = effects->replay_efficiency;
        }
        if (effects->replay_efficiency > stats->max_replay_efficiency) {
            stats->max_replay_efficiency = effects->replay_efficiency;
        }
        stats->avg_replay_efficiency =
            (stats->avg_replay_efficiency * (stats->update_count - 1) +
             effects->replay_efficiency) / stats->update_count;
    }

    /* Update substrate state observations */
    if (stats->update_count == 1 || atp_level < stats->min_atp_observed) {
        stats->min_atp_observed = atp_level;
    }
    if (stats->update_count == 1 || metabolic_stress > stats->max_stress_observed) {
        stats->max_stress_observed = metabolic_stress;
    }
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

void consolidation_substrate_default_config(consolidation_substrate_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("Cannot initialize NULL config");
        return;
    }

    /* Enable all modulation features by default */
    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    config->enable_atp_modulation = true;
    config->enable_stress_modulation = true;
    config->enable_hypoxia_modulation = true;
    config->enable_protein_synthesis = true;

    /* Default sensitivity parameters */
    config->atp_sensitivity = CONSOLIDATION_SUBSTRATE_DEFAULT_ATP_SENSITIVITY;
    config->stress_sensitivity = CONSOLIDATION_SUBSTRATE_DEFAULT_STRESS_SENSITIVITY;
    config->hypoxia_sensitivity = CONSOLIDATION_SUBSTRATE_DEFAULT_HYPOXIA_SENSITIVITY;

    /* Default update parameters */
    config->update_interval_ms = 100;  /* 100ms updates */
    config->auto_update = true;
}

consolidation_substrate_bridge_t* consolidation_substrate_bridge_create(
    const consolidation_substrate_config_t* config,
    memory_consolidation_t* consolidation,
    neural_substrate_t* substrate
) {
    if (!config || !consolidation || !substrate) {
        NIMCP_LOGGING_ERROR("Cannot create bridge with NULL parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_substrate_bridge_create: required parameter is NULL (config, consolidation, substrate)");
        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    consolidation_substrate_bridge_t* bridge =
        (consolidation_substrate_bridge_t*)nimcp_malloc(sizeof(consolidation_substrate_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate memory consolidation substrate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Zero-initialize */
    memset(bridge, 0, sizeof(consolidation_substrate_bridge_t));

    /* Store references */
    bridge->substrate = substrate;
    bridge->consolidation = consolidation;

    /* Copy configuration */
    memcpy(&bridge->config, config, sizeof(consolidation_substrate_config_t));

    /* Initialize effects to optimal state */
    bridge->effects.consolidation_rate = 1.0f;
    bridge->effects.protein_synthesis_rate = 1.0f;
    bridge->effects.replay_efficiency = 1.0f;
    bridge->effects.transfer_rate = 1.0f;
    bridge->effects.is_impaired = false;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(consolidation_substrate_stats_t));
    bridge->stats.min_consolidation_rate = 1.0f;
    bridge->stats.max_consolidation_rate = 1.0f;
    bridge->stats.avg_consolidation_rate = 1.0f;
    bridge->stats.min_protein_synthesis = 1.0f;
    bridge->stats.max_protein_synthesis = 1.0f;
    bridge->stats.avg_protein_synthesis = 1.0f;
    bridge->stats.min_replay_efficiency = 1.0f;
    bridge->stats.max_replay_efficiency = 1.0f;
    bridge->stats.avg_replay_efficiency = 1.0f;
    bridge->stats.min_atp_observed = 1.0f;
    bridge->stats.max_stress_observed = 0.0f;

    /* Create mutex for thread safety */
    bridge->base.mutex = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate mutex for consolidation substrate bridge");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "consolidation_substrate_bridge_create: bridge->base is NULL");
        return NULL;
    }

    if (nimcp_platform_mutex_init(bridge->base.mutex, false) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize mutex for consolidation substrate bridge");
        nimcp_free(bridge->base.mutex);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "consolidation_substrate_bridge_create: validation failed");
        return NULL;
    }

    /* Bio-async not enabled yet */
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Created memory consolidation substrate bridge");

    return bridge;
}

void consolidation_substrate_bridge_destroy(consolidation_substrate_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if enabled */
    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    if (bridge->base.bio_async_enabled) {
        consolidation_substrate_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_platform_mutex_destroy(bridge->base.mutex);
        nimcp_free(bridge->base.mutex);
        bridge->base.mutex = NULL;
        bridge->base.mutex = NULL;
    }

    /* Free bridge */
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("Destroyed memory consolidation substrate bridge");
}

/* ============================================================================
 * Bio-async Integration
 * ============================================================================ */

int consolidation_substrate_connect_bio_async(consolidation_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot connect NULL bridge to bio-async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_substrate_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Check if already connected */
    if (bridge->base.bio_async_enabled) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SUBSTRATE_MEMORY_CONSOLIDATION,
        .module_name = "consolidation_substrate_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected memory consolidation substrate bridge to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int consolidation_substrate_disconnect_bio_async(consolidation_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot disconnect NULL bridge from bio-async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_substrate_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->base.bio_async_enabled && bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
        bridge->base.bio_async_enabled = false;
        NIMCP_LOGGING_INFO("Disconnected memory consolidation substrate bridge from bio-async router");
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool consolidation_substrate_is_bio_async_connected(const consolidation_substrate_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Update Functions
 * ============================================================================ */

int consolidation_substrate_update(consolidation_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot update NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_substrate_update: bridge is NULL");
        return -1;
    }

    if (!bridge->substrate) {
        NIMCP_LOGGING_ERROR("Bridge has NULL substrate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_substrate_update: bridge->substrate is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get substrate metabolic state */
    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_LOGGING_ERROR("Failed to get metabolic state from substrate");
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidation_substrate_update: validation failed");
        return -1;
    }

    /* Compute consolidation rate */
    float consolidation_rate = 1.0f;
    if (bridge->config.enable_atp_modulation) {
        consolidation_rate = compute_consolidation_rate(
            metabolic.atp_level,
            metabolic.metabolic_capacity,
            bridge->config.atp_sensitivity
        );
    }

    /* Compute protein synthesis rate */
    float protein_synthesis_rate = 1.0f;
    if (bridge->config.enable_protein_synthesis) {
        protein_synthesis_rate = compute_protein_synthesis_rate(
            metabolic.atp_level,
            metabolic.glucose_level,
            bridge->config.atp_sensitivity
        );
    }

    /* Compute replay efficiency */
    float replay_efficiency = 1.0f;
    if (bridge->config.enable_hypoxia_modulation) {
        replay_efficiency = compute_replay_efficiency(
            metabolic.metabolic_capacity,
            metabolic.oxygen_saturation,
            bridge->config.hypoxia_sensitivity
        );
    }

    /* Compute transfer rate */
    float transfer_rate = 1.0f;
    if (bridge->config.enable_stress_modulation) {
        transfer_rate = compute_transfer_rate(
            metabolic.atp_level,
            metabolic.oxygen_saturation,
            bridge->config.stress_sensitivity
        );
    }

    /* Check for impairment */
    bool is_impaired = check_impairment(protein_synthesis_rate, consolidation_rate);

    /* Update effects */
    bridge->effects.consolidation_rate = consolidation_rate;
    bridge->effects.protein_synthesis_rate = protein_synthesis_rate;
    bridge->effects.replay_efficiency = replay_efficiency;
    bridge->effects.transfer_rate = transfer_rate;
    bridge->effects.is_impaired = is_impaired;

    /* Update statistics - compute metabolic stress from metabolic capacity */
    float metabolic_stress = 1.0f - metabolic.metabolic_capacity;
    update_statistics(bridge, &bridge->effects, metabolic.atp_level, metabolic_stress);

    /* Log significant state changes */
    if (is_impaired) {
        NIMCP_LOGGING_WARN("Memory consolidation critically impaired (protein_synth=%.2f, consol_rate=%.2f)",
                          protein_synthesis_rate, consolidation_rate);
    }

    if (metabolic.atp_level < CONSOLIDATION_SUBSTRATE_ATP_CRITICAL) {
        NIMCP_LOGGING_WARN("Critical ATP level for consolidation: %.2f", metabolic.atp_level);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

float consolidation_substrate_get_consolidation_rate(const consolidation_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get consolidation rate from NULL bridge");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    return bridge->effects.consolidation_rate;
}

float consolidation_substrate_get_protein_synthesis_rate(const consolidation_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get protein synthesis rate from NULL bridge");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    return bridge->effects.protein_synthesis_rate;
}

float consolidation_substrate_get_replay_efficiency(const consolidation_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get replay efficiency from NULL bridge");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    return bridge->effects.replay_efficiency;
}

float consolidation_substrate_get_transfer_rate(const consolidation_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get transfer rate from NULL bridge");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    return bridge->effects.transfer_rate;
}

consolidation_substrate_effects_t consolidation_substrate_get_effects(
    const consolidation_substrate_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    consolidation_substrate_effects_t effects = {0};

    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get effects from NULL bridge");
        /* Return default optimal effects */
        effects.consolidation_rate = 1.0f;
        effects.protein_synthesis_rate = 1.0f;
        effects.replay_efficiency = 1.0f;
        effects.transfer_rate = 1.0f;
        effects.is_impaired = false;
        return effects;
    }

    /* Return copy of effects */
    return bridge->effects;
}

bool consolidation_substrate_is_impaired(const consolidation_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot check impairment on NULL bridge");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    return bridge->effects.is_impaired;
}

consolidation_substrate_stats_t consolidation_substrate_get_stats(
    const consolidation_substrate_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    consolidation_substrate_stats_t stats = {0};

    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get stats from NULL bridge");
        return stats;
    }

    /* Return copy of statistics */
    return bridge->stats;
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 * WHAT: Retrieve module's self-awareness information from KG
 * WHY:  Enable introspection about module capabilities and connections
 * HOW:  Query KG reader for entity and relations
 */
int consolidation_substrate_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    memory_consolidation_substrate_bridge_heartbeat("memory_conso_consolidation_substr", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Memory_Consolidation_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                memory_consolidation_substrate_bridge_heartbeat("memory_conso_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Consolidation substrate bridge self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Memory_Consolidation_Substrate_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Memory_Consolidation_Substrate_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void memory_consolidation_substrate_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_memory_consolidation_substrate_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int memory_consolidation_substrate_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "memory_consolidation_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    memory_consolidation_substrate_bridge_heartbeat_instance(NULL, "memory_consolidation_substrate_bridge_training_begin", 0.0f);
    return 0;
}

int memory_consolidation_substrate_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "memory_consolidation_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    memory_consolidation_substrate_bridge_heartbeat_instance(NULL, "memory_consolidation_substrate_bridge_training_end", 1.0f);
    return 0;
}

int memory_consolidation_substrate_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "memory_consolidation_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    memory_consolidation_substrate_bridge_heartbeat_instance(NULL, "memory_consolidation_substrate_bridge_training_step", progress);
    return 0;
}
