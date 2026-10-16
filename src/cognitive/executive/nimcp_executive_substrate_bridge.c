/**
 * @file nimcp_executive_substrate_bridge.c
 * @brief Executive-Substrate Integration Bridge Implementation
 * @version 1.0.0
 * @date 2024-12
 *
 * BIOLOGICAL BASIS:
 * The prefrontal cortex (PFC) is the most metabolically demanding brain region,
 * consuming disproportionate ATP for executive functions. ATP depletion
 * progressively impairs decision quality, inhibitory control, planning depth,
 * and cognitive flexibility.
 *
 * Uses shared metabolic modulation utilities from nimcp_metabolic_modulation.h
 */

#include "cognitive/executive/nimcp_executive_substrate_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <math.h>
#include <string.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(executive_substrate_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

BRIDGE_DEFINE_SECURITY_SETTERS(executive_substrate_bridge)

/* ============================================================================
 * Helper Functions (using shared nimcp_clamp_f from nimcp_metabolic_modulation.h)
 * ============================================================================ */

/**
 * @brief Update running average with exponential smoothing
 *
 * WHAT: Compute exponentially weighted moving average
 * WHY:  Track statistics over time without storing all values
 * HOW:  avg = (1-alpha)*old_avg + alpha*new_value
 */
static float update_running_avg(float current_avg, float new_value, float alpha)
{
    return (1.0f - alpha) * current_avg + alpha * new_value;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

void executive_substrate_default_config(executive_substrate_config_t* config)
{
    if (!config) {
        return;
    }

    /* Enable all modulations by default */
    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_executive_substrate_", 0.0f);


    config->enable_decision_modulation = true;
    config->enable_inhibition_modulation = true;
    config->enable_planning_modulation = true;
    config->enable_flexibility_modulation = true;
    config->enable_fatigue_tracking = true;

    /* Moderate sensitivity (1.0 = biological measurements) */
    config->decision_sensitivity = 1.0f;
    config->inhibition_sensitivity = 1.0f;
    config->planning_sensitivity = 1.0f;
    config->flexibility_sensitivity = 1.0f;
    config->fatigue_accumulation_rate = 0.01f;  /* 1% per update when ATP low */
    config->fatigue_recovery_rate = 0.005f;      /* 0.5% per update when ATP high */

    /* Thresholds from biological literature */
    config->impairment_threshold = EXECUTIVE_ATP_IMPAIRED_THRESHOLD;
    config->critical_threshold = EXECUTIVE_ATP_CRITICAL_THRESHOLD;

    /* Bio-async disabled by default */
    config->enable_bio_async = false;
    config->inbox_capacity = 32;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

executive_substrate_bridge_t* executive_substrate_bridge_create(
    const executive_substrate_config_t* config,
    nimcp_executive_t* executive,
    neural_substrate_t* substrate)
{
    /* Guard: Validate required components */
    if (!executive) {
        NIMCP_LOGGING_ERROR("Cannot create executive substrate bridge: NULL executive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_substrate_bridge_create: executive is NULL");
        return NULL;
    }

    if (!substrate) {
        NIMCP_LOGGING_ERROR("Cannot create executive substrate bridge: NULL substrate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_substrate_bridge_create: substrate is NULL");
        return NULL;
    }

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_create", 0.0f);


    executive_substrate_bridge_t* bridge =
        (executive_substrate_bridge_t*)nimcp_malloc(sizeof(executive_substrate_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate executive substrate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_substrate_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(executive_substrate_bridge_t));

    /* Store component references */
    bridge->substrate = substrate;
    bridge->executive = executive;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        executive_substrate_default_config(&bridge->config);
    }

    /* Initialize mutex */
    bridge->base.mutex = (nimcp_platform_mutex_t*)nimcp_malloc(sizeof(nimcp_platform_mutex_t));
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_substrate_bridge_create: bridge->base is NULL");
        return NULL;
    }

    if (nimcp_platform_mutex_init(bridge->base.mutex, false) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize mutex");
        nimcp_free(bridge->base.mutex);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "executive_substrate_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize effects to neutral (no modulation) */
    bridge->effects.decision_quality = 1.0f;
    bridge->effects.inhibition_strength = 1.0f;
    bridge->effects.planning_depth = 1.0f;
    bridge->effects.cognitive_flexibility = 1.0f;
    bridge->effects.fatigue_level = 0.0f;
    bridge->effects.is_impaired = false;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(executive_substrate_stats_t));
    bridge->stats.min_atp_level = 1.0f;
    bridge->stats.avg_decision_quality = 1.0f;
    bridge->stats.avg_inhibition_strength = 1.0f;
    bridge->stats.avg_planning_depth = 1.0f;
    bridge->stats.avg_flexibility = 1.0f;

    /* Initialize internal state */
    bridge->last_atp_level = 1.0f;
    bridge->last_update_time = 0;
    bridge->initialized = true;

    /* Connect to bio-async if enabled */
    if (bridge->config.enable_bio_async) {
        executive_substrate_connect_bio_async(bridge);
    }

    NIMCP_LOGGING_INFO("Created executive substrate bridge");

    return bridge;
}

void executive_substrate_bridge_destroy(executive_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        executive_substrate_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_platform_mutex_destroy(bridge->base.mutex);
        nimcp_free(bridge->base.mutex);
        bridge->base.mutex = NULL;
    }

    /* Free bridge */
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("Destroyed executive substrate bridge");
}

/* ============================================================================
 * Bio-async Integration API
 * ============================================================================ */

int executive_substrate_connect_bio_async(executive_substrate_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_executive_substrate_", 0.0f);


    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SUBSTRATE_EXECUTIVE,
        .module_name = "executive_substrate_bridge",
        .inbox_capacity = bridge->config.inbox_capacity,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected executive substrate bridge to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_substrate_connect_bio_async: validation failed");
    return -1;
}

int executive_substrate_disconnect_bio_async(executive_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->base.bio_async_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_substrate_disconnect_bio_async: required parameter is NULL (bridge, bridge->base)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_executive_substrate_", 0.0f);


    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected executive substrate bridge from bio-async");

    return 0;
}

bool executive_substrate_is_bio_async_connected(const executive_substrate_bridge_t* bridge)
{
    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_executive_substrate_", 0.0f);


    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int executive_substrate_update(executive_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_substrate_update: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_executive_substrate_", 0.0f);


    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "executive_substrate_update");
    BRIDGE_LGSS_GATE(bridge, "executive_substrate_update");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* ========================================================================
     * Get current metabolic state
     * ======================================================================== */
    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_substrate_update: validation failed");
        return -1;
    }

    float atp_level = metabolic.atp_level;
    float glucose_level = metabolic.glucose_level;
    float metabolic_capacity = metabolic.metabolic_capacity;

    /* Track statistics */
    if (atp_level < bridge->stats.min_atp_level) {
        bridge->stats.min_atp_level = atp_level;
    }
    bridge->stats.avg_atp_level = update_running_avg(
        bridge->stats.avg_atp_level,
        atp_level,
        0.01f
    );

    /* ========================================================================
     * BIOLOGICAL: Decision Quality
     * - Prefrontal cortex requires high ATP for optimal decision-making
     * - decision_quality = clamp(atp_level * metabolic_capacity, 0.3, 1.0)
     * ======================================================================== */
    if (bridge->config.enable_decision_modulation) {
        float base_quality = atp_level * metabolic_capacity;
        float modulated = 1.0f - (1.0f - base_quality) * bridge->config.decision_sensitivity;
        bridge->effects.decision_quality = nimcp_clampf(modulated, 0.3f, 1.0f);
    } else {
        bridge->effects.decision_quality = 1.0f;
    }

    /* ========================================================================
     * BIOLOGICAL: Inhibition Strength
     * - Impulse control is most sensitive to ATP depletion (frontal vulnerability)
     * - Low ATP reduces inhibition → increased impulsivity
     * - inhibition_strength = clamp(atp_level * 1.3, 0.2, 1.0)
     * - Multiplier of 1.3 reflects enhanced sensitivity
     * ======================================================================== */
    if (bridge->config.enable_inhibition_modulation) {
        float base_inhibition = atp_level * 1.3f;
        float modulated = 1.0f - (1.0f - base_inhibition) * bridge->config.inhibition_sensitivity;
        bridge->effects.inhibition_strength = nimcp_clampf(modulated, 0.2f, 1.0f);
    } else {
        bridge->effects.inhibition_strength = 1.0f;
    }

    /* ========================================================================
     * BIOLOGICAL: Planning Depth
     * - Deep planning requires sustained cognitive effort and working memory
     * - ATP depletion limits planning horizon
     * - planning_depth = clamp(metabolic_capacity, 0.2, 1.0)
     * ======================================================================== */
    if (bridge->config.enable_planning_modulation) {
        float modulated = 1.0f - (1.0f - metabolic_capacity) * bridge->config.planning_sensitivity;
        bridge->effects.planning_depth = nimcp_clampf(modulated, 0.2f, 1.0f);
    } else {
        bridge->effects.planning_depth = 1.0f;
    }

    /* ========================================================================
     * BIOLOGICAL: Cognitive Flexibility
     * - Task switching requires energy for context updating
     * - Combines ATP and glucose availability
     * - flexibility = clamp((atp_level + glucose_level) / 2.0, 0.3, 1.0)
     * ======================================================================== */
    if (bridge->config.enable_flexibility_modulation) {
        float base_flexibility = (atp_level + glucose_level) / 2.0f;
        float modulated = 1.0f - (1.0f - base_flexibility) * bridge->config.flexibility_sensitivity;
        bridge->effects.cognitive_flexibility = nimcp_clampf(modulated, 0.3f, 1.0f);
    } else {
        bridge->effects.cognitive_flexibility = 1.0f;
    }

    /* ========================================================================
     * BIOLOGICAL: Fatigue Level
     * - Accumulates when ATP is low, recovers when ATP is high
     * - fatigue_level = 1.0 - metabolic_capacity
     * - Also use rate-based accumulation/recovery
     * ======================================================================== */
    if (bridge->config.enable_fatigue_tracking) {
        /* Base fatigue from metabolic state */
        float base_fatigue = 1.0f - metabolic_capacity;

        /* Accumulation when ATP is low */
        if (atp_level < EXECUTIVE_ATP_FATIGUE_THRESHOLD) {
            float deficit = (EXECUTIVE_ATP_FATIGUE_THRESHOLD - atp_level) /
                           EXECUTIVE_ATP_FATIGUE_THRESHOLD;
            bridge->effects.fatigue_level += deficit * bridge->config.fatigue_accumulation_rate;
        }

        /* Recovery when ATP is high */
        if (atp_level > EXECUTIVE_ATP_OPTIMAL_THRESHOLD) {
            float surplus = (atp_level - EXECUTIVE_ATP_OPTIMAL_THRESHOLD) /
                           (1.0f - EXECUTIVE_ATP_OPTIMAL_THRESHOLD);
            bridge->effects.fatigue_level -= surplus * bridge->config.fatigue_recovery_rate;
        }

        /* Blend with base fatigue */
        bridge->effects.fatigue_level = (bridge->effects.fatigue_level + base_fatigue) / 2.0f;

        /* Clamp to valid range */
        bridge->effects.fatigue_level = nimcp_clampf(bridge->effects.fatigue_level, 0.0f, 1.0f);

        /* Track maximum fatigue */
        if (bridge->effects.fatigue_level > bridge->stats.max_fatigue_level) {
            bridge->stats.max_fatigue_level = bridge->effects.fatigue_level;
        }
    } else {
        bridge->effects.fatigue_level = 0.0f;
    }

    /* ========================================================================
     * BIOLOGICAL: Impairment Detection
     * - is_impaired = (decision_quality < 0.7 || fatigue_level > 0.5)
     * - Also check if ATP below impairment threshold
     * ======================================================================== */
    bool was_impaired = bridge->effects.is_impaired;

    bridge->effects.is_impaired = (
        atp_level < bridge->config.impairment_threshold ||
        bridge->effects.decision_quality < 0.7f ||
        bridge->effects.fatigue_level > 0.5f
    );

    /* Track impairment events (transitions to impaired state) */
    if (bridge->effects.is_impaired && !was_impaired) {
        bridge->stats.impairment_events++;
        NIMCP_LOGGING_WARN("Executive function impaired (ATP: %.2f, Decision: %.2f, Fatigue: %.2f)",
            atp_level, bridge->effects.decision_quality, bridge->effects.fatigue_level);
    }

    /* ========================================================================
     * Update Statistics
     * ======================================================================== */
    bridge->stats.update_count++;

    /* Running averages */
    bridge->stats.avg_decision_quality = update_running_avg(
        bridge->stats.avg_decision_quality,
        bridge->effects.decision_quality,
        0.01f
    );
    bridge->stats.avg_inhibition_strength = update_running_avg(
        bridge->stats.avg_inhibition_strength,
        bridge->effects.inhibition_strength,
        0.01f
    );
    bridge->stats.avg_planning_depth = update_running_avg(
        bridge->stats.avg_planning_depth,
        bridge->effects.planning_depth,
        0.01f
    );
    bridge->stats.avg_flexibility = update_running_avg(
        bridge->stats.avg_flexibility,
        bridge->effects.cognitive_flexibility,
        0.01f
    );

    /* Store last ATP level for next update */
    bridge->last_atp_level = atp_level;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query API - Individual Effects
 * ============================================================================ */

float executive_substrate_get_decision_quality(const executive_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->config.enable_decision_modulation) {
        return 1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_executive_substrate_", 0.0f);


    return bridge->effects.decision_quality;
}

float executive_substrate_get_inhibition_strength(const executive_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->config.enable_inhibition_modulation) {
        return 1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_executive_substrate_", 0.0f);


    return bridge->effects.inhibition_strength;
}

float executive_substrate_get_planning_depth(const executive_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->config.enable_planning_modulation) {
        return 1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_executive_substrate_", 0.0f);


    return bridge->effects.planning_depth;
}

float executive_substrate_get_cognitive_flexibility(const executive_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->config.enable_flexibility_modulation) {
        return 1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_executive_substrate_", 0.0f);


    return bridge->effects.cognitive_flexibility;
}

float executive_substrate_get_fatigue(const executive_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->config.enable_fatigue_tracking) {
        return 0.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_executive_substrate_", 0.0f);


    return bridge->effects.fatigue_level;
}

executive_substrate_effects_t executive_substrate_get_effects(
    const executive_substrate_bridge_t* bridge)
{
    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_executive_substrate_", 0.0f);


    executive_substrate_effects_t effects = {0};

    if (!bridge) {
        /* Return neutral effects */
        effects.decision_quality = 1.0f;
        effects.inhibition_strength = 1.0f;
        effects.planning_depth = 1.0f;
        effects.cognitive_flexibility = 1.0f;
        effects.fatigue_level = 0.0f;
        effects.is_impaired = false;
        return effects;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);
    effects = bridge->effects;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    return effects;
}

bool executive_substrate_is_impaired(const executive_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_executive_substrate_", 0.0f);


    return bridge->effects.is_impaired;
}

/* ============================================================================
 * Query API - Statistics
 * ============================================================================ */

executive_substrate_stats_t executive_substrate_get_stats(
    const executive_substrate_bridge_t* bridge)
{
    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_executive_substrate_", 0.0f);


    executive_substrate_stats_t stats = {0};

    if (!bridge) {
        return stats;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);
    stats = bridge->stats;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    return stats;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Executive Substrate Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int executive_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    executive_substrate_bridge_heartbeat("executive_su_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Executive_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                executive_substrate_bridge_heartbeat("executive_su_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Executive Substrate Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Executive_Substrate_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Executive_Substrate_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void executive_substrate_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_executive_substrate_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int executive_substrate_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "executive_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    executive_substrate_bridge_heartbeat_instance(NULL, "executive_substrate_bridge_training_begin", 0.0f);
    return 0;
}

int executive_substrate_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "executive_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    executive_substrate_bridge_heartbeat_instance(NULL, "executive_substrate_bridge_training_end", 1.0f);
    return 0;
}

int executive_substrate_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "executive_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    executive_substrate_bridge_heartbeat_instance(NULL, "executive_substrate_bridge_training_step", progress);
    return 0;
}
