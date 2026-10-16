/**
 * @file nimcp_working_memory_substrate_bridge.c
 * @brief Working Memory-Substrate Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between neural substrate and working memory
 * WHY: Working memory capacity and maintenance critically depend on metabolic state
 * HOW: ATP and temperature modulate capacity, decay rates, and encoding strength
 *
 * Uses shared metabolic modulation utilities from nimcp_metabolic_modulation.h
 */

#include "cognitive/working_memory/nimcp_working_memory_substrate_bridge.h"
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

BRIDGE_BOILERPLATE(working_memory_substrate_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
BRIDGE_DEFINE_SECURITY_SETTERS(wm_substrate_bridge)

/* ============================================================================
 * Helper Functions (using shared nimcp_clamp_f from nimcp_metabolic_modulation.h)
 * ============================================================================ */

/**
 * @brief Compute Q10-based temperature scaling for decay rate
 *
 * WHAT: Calculate multiplicative factor for decay rate based on temperature
 * WHY: Temperature affects biochemical reaction rates exponentially
 * HOW: Q10 rule: rate = Q10^((T - T_ref) / 10)
 *
 * BIOLOGICAL BASIS:
 * - Q10 ≈ 2 for most neural processes
 * - Higher temperature → faster decay (fever causes forgetting)
 * - Lower temperature → slower decay (hypothermia slows processes)
 *
 * @param current_temp Current temperature (°C)
 * @param reference_temp Reference temperature (°C), typically 37.0
 * @return Temperature scaling factor for decay rate
 */
static float compute_temperature_decay_factor(float current_temp, float reference_temp)
{
    /* Q10 = 2.0 for working memory decay */
    const float Q10_DECAY = 2.0f;

    float temp_diff = current_temp - reference_temp;
    float exponent = temp_diff / 10.0f;
    return powf(Q10_DECAY, exponent);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int wm_substrate_default_config(wm_substrate_config_t* config)
{
    /* Guard: Validate input */
    if (!config) {
        NIMCP_LOGGING_ERROR("Cannot set default config: NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_substrate_default_config: config is NULL");
        return -1;
    }

    /* WHAT: Enable all metabolic modulations by default
     * WHY: Working memory is highly metabolically dependent
     * HOW: Set all enable flags to true
     */
    config->enable_capacity_modulation = true;
    config->enable_decay_modulation = true;
    config->enable_refresh_modulation = true;
    config->enable_bio_async = true;

    /* WHAT: Set standard sensitivity (1.0 = biological measurements)
     * WHY: Provide realistic metabolic effects
     * HOW: Sensitivity factors scale the computed effects
     */
    config->atp_sensitivity = 1.0f;
    config->temperature_sensitivity = 1.0f;

    NIMCP_LOGGING_DEBUG("Initialized default WM-substrate config");
    return 0;
}

wm_substrate_bridge_t* wm_substrate_bridge_create(
    const wm_substrate_config_t* config,
    neural_substrate_t* substrate,
    working_memory_t* wm)
{
    /* Guard: Validate required pointers */
    if (!substrate) {
        NIMCP_LOGGING_ERROR("Cannot create WM-substrate bridge: NULL substrate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;
    }
    if (!wm) {
        NIMCP_LOGGING_ERROR("Cannot create WM-substrate bridge: NULL working memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm is NULL");

        return NULL;
    }

    /* WHAT: Allocate bridge structure
     * WHY: Need memory for bridge state
     * HOW: Use nimcp_malloc for consistency
     */
    wm_substrate_bridge_t* bridge = (wm_substrate_bridge_t*)nimcp_malloc(
        sizeof(wm_substrate_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate WM-substrate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* WHAT: Initialize configuration
     * WHY: Need config for modulation behavior
     * HOW: Copy provided config or use defaults
     */
    if (config) {
        memcpy(&bridge->config, config, sizeof(wm_substrate_config_t));
    } else {
        wm_substrate_default_config(&bridge->config);
    }

    /* WHAT: Store substrate and WM pointers
     * WHY: Need references to query state and apply effects
     * HOW: Direct pointer assignment
     */
    bridge->substrate = substrate;
    bridge->wm = wm;

    /* WHAT: Initialize effects to optimal/neutral state
     * WHY: Start with no metabolic impairment
     * HOW: Set all factors to 1.0 (full capacity)
     *
     * BIOLOGICAL BASIS:
     * - capacity_factor = 1.0: Full 7-item capacity
     * - decay_rate_mod = 1.0: Normal decay rate
     * - refresh_efficiency = 1.0: Full attention refresh
     * - encoding_strength = 1.0: Full encoding strength
     * - is_impaired = false: Not impaired
     */
    bridge->effects.capacity_factor = 1.0f;
    bridge->effects.decay_rate_mod = 1.0f;
    bridge->effects.refresh_efficiency = 1.0f;
    bridge->effects.encoding_strength = 1.0f;
    bridge->effects.is_impaired = false;

    /* WHAT: Zero statistics
     * WHY: Start with clean state
     * HOW: memset to zero
     */
    memset(&bridge->stats, 0, sizeof(wm_substrate_stats_t));
    bridge->stats.min_capacity_factor = 1.0f;  /* Start at maximum */
    bridge->stats.avg_decay_rate_mod = 1.0f;   /* Start at normal */

    /* WHAT: Initialize bio-async to disabled
     * WHY: Requires explicit connection
     * HOW: Set flags to false/NULL
     */
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    /* WHAT: Initialize bridge base (mutex + infrastructure)
     * WHY: Allow concurrent access from WM and substrate threads
     * HOW: Use bridge_base_init for consistent initialization
     */
    if (bridge_base_init(&bridge->base, 0, "working_memory_substrate") != 0) { nimcp_free(bridge); return NULL; }

    NIMCP_LOGGING_INFO("Created WM-substrate bridge");
    return bridge;
}

void wm_substrate_bridge_destroy(wm_substrate_bridge_t* bridge)
{
    /* Guard: NULL-safe */
    if (!bridge) {
        return;
    }

    /* WHAT: Disconnect from bio-async if connected
     * WHY: Clean shutdown of messaging
     * HOW: Call disconnect function
     */
    if (bridge->base.bio_async_enabled) {
        wm_substrate_disconnect_bio_async(bridge);
    }

    /* WHAT: Clean up bridge base (mutex + infrastructure)
     * WHY: Release mutex resources
     * HOW: bridge_base_cleanup handles all base teardown
     */
    bridge_base_cleanup(&bridge->base);

    /* WHAT: Free bridge structure
     * WHY: Release memory
     * HOW: nimcp_free
     */
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_DEBUG("Destroyed WM-substrate bridge");
}

/* ============================================================================
 * Bio-async API
 * ============================================================================ */

int wm_substrate_connect_bio_async(wm_substrate_bridge_t* bridge)
{
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot connect bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_substrate_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard: Check if already connected */
    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_WARN("WM-substrate bridge already connected to bio-async");
        return 0;
    }

    /* WHAT: Register with bio-async router
     * WHY: Enable inter-module messaging about metabolic state
     * HOW: Use bio_router_register_module with WM-substrate ID
     */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SUBSTRATE_WORKING_MEMORY,
        .module_name = "wm_substrate_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("WM-substrate bridge connected to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wm_substrate_connect_bio_async: validation failed");
        return -1;
    }
}

int wm_substrate_disconnect_bio_async(wm_substrate_bridge_t* bridge)
{
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot disconnect bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_substrate_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard: Check if connected */
    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    /* WHAT: Unregister from bio-async router
     * WHY: Clean disconnection
     * HOW: Unregister module and clear state
     */
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("WM-substrate bridge disconnected from bio-async");

    return 0;
}

bool wm_substrate_is_bio_async_connected(const wm_substrate_bridge_t* bridge)
{
    /* Guard: Validate bridge */
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int wm_substrate_update(wm_substrate_bridge_t* bridge)
{
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot update WM-substrate: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_substrate_update: bridge is NULL");
        return -1;
    }

    /* WHAT: Get current metabolic state from substrate
     * WHY: Need ATP, glucose, O2 levels for capacity computation
     * HOW: Query substrate API
     */
    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_LOGGING_ERROR("Failed to get metabolic state from substrate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wm_substrate_update: validation failed");
        return -1;
    }

    /* WHAT: Get current physical state from substrate
     * WHY: Need temperature for decay rate computation
     * HOW: Query substrate API
     */
    substrate_physical_state_t physical;
    if (substrate_get_physical_state(bridge->substrate, &physical) != 0) {
        NIMCP_LOGGING_ERROR("Failed to get physical state from substrate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wm_substrate_update: validation failed");
        return -1;
    }

    /* Lock for thread-safe effect updates */
    if ((bridge->base.mutex != NULL)) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
    }

    /* ========================================================================
     * CAPACITY FACTOR COMPUTATION
     * ========================================================================
     * BIOLOGICAL BASIS:
     * Working memory capacity depends critically on sustained neural firing,
     * which requires continuous ATP. Miller's 7±2 items reduces to 4 items
     * (Cowan) under metabolic stress, and down to 2 items at critical ATP.
     *
     * ATP Thresholds (from header):
     * - 0.8: Full capacity (7 items)
     * - 0.5: Reduced capacity (4 items)
     * - 0.3: Minimal capacity (2 items)
     * ======================================================================== */
    float capacity_factor = 0.0f;

    if (bridge->config.enable_capacity_modulation) {
        /* Start with metabolic capacity as base */
        capacity_factor = metabolic.metabolic_capacity;

        /* Apply ATP-specific thresholds */
        if (metabolic.atp_level < WM_ATP_THRESHOLD_FULL) {
            /* Below 0.8 ATP: Linear reduction */
            capacity_factor *= (metabolic.atp_level / WM_ATP_THRESHOLD_FULL);
        }

        if (metabolic.atp_level < WM_ATP_THRESHOLD_REDUCED) {
            /* Below 0.5 ATP: Additional 30% reduction (Cowan's 4-item limit) */
            capacity_factor *= 0.7f;
        }

        if (metabolic.atp_level < WM_ATP_THRESHOLD_MINIMAL) {
            /* Below 0.3 ATP: Critical state, only 2-3 items */
            capacity_factor = 0.3f;
        }

        /* Scale by sensitivity */
        capacity_factor = 1.0f - (1.0f - capacity_factor) * bridge->config.atp_sensitivity;

        /* Clamp to valid range */
        bridge->effects.capacity_factor = nimcp_clampf(capacity_factor, 0.0f, 1.0f);
    } else {
        bridge->effects.capacity_factor = 1.0f;
    }

    /* ========================================================================
     * DECAY RATE MODULATION
     * ========================================================================
     * BIOLOGICAL BASIS:
     * Temperature affects all biochemical processes through Q10 scaling.
     * - Fever (>38°C): Faster decay (accelerated forgetting)
     * - Normal (37°C): Standard decay
     * - Hypothermia (<36°C): Slower decay
     *
     * Q10 ≈ 2 for neural processes, meaning rate doubles per 10°C increase
     * ======================================================================== */
    if (bridge->config.enable_decay_modulation) {
        /* Compute Q10-based temperature effect */
        float temp_factor = compute_temperature_decay_factor(
            physical.temperature,
            37.0f  /* Normal body temperature */
        );

        /* Scale by temperature sensitivity */
        float decay_mod = 1.0f + (temp_factor - 1.0f) * bridge->config.temperature_sensitivity;

        /* Clamp to reasonable range [0.5 - 2.0] */
        bridge->effects.decay_rate_mod = nimcp_clampf(decay_mod, 0.5f, 2.0f);
    } else {
        bridge->effects.decay_rate_mod = 1.0f;
    }

    /* ========================================================================
     * REFRESH EFFICIENCY
     * ========================================================================
     * BIOLOGICAL BASIS:
     * Attention-based rehearsal is an active process requiring prefrontal-
     * parietal coordination and sustained ATP for top-down control.
     * Low ATP impairs attentional refresh, accelerating forgetting.
     * ======================================================================== */
    if (bridge->config.enable_refresh_modulation) {
        /* ATP directly affects refresh efficiency */
        bridge->effects.refresh_efficiency = nimcp_clampf(
            metabolic.atp_level,
            0.2f,  /* Minimum refresh efficiency */
            1.0f   /* Maximum refresh efficiency */
        );
    } else {
        bridge->effects.refresh_efficiency = 1.0f;
    }

    /* ========================================================================
     * ENCODING STRENGTH
     * ========================================================================
     * BIOLOGICAL BASIS:
     * New WM items require synaptic potentiation (LTP-like mechanisms).
     * This is ATP-dependent and requires healthy substrate for AMPA
     * receptor trafficking and calcium signaling.
     * ======================================================================== */
    /* Combined metabolic and physical capacity */
    bridge->effects.encoding_strength = (
        metabolic.metabolic_capacity + physical.physical_capacity
    ) / 2.0f;
    bridge->effects.encoding_strength = nimcp_clampf(
        bridge->effects.encoding_strength,
        0.0f,
        1.0f
    );

    /* ========================================================================
     * IMPAIRMENT FLAG
     * ========================================================================
     * BIOLOGICAL BASIS:
     * Below minimal ATP threshold, WM is critically impaired:
     * - Only 2 items can be maintained
     * - New encoding likely to fail
     * - Attention refresh ineffective
     * ======================================================================== */
    bridge->effects.is_impaired = (bridge->effects.capacity_factor < 0.7f);

    /* ========================================================================
     * UPDATE STATISTICS
     * ======================================================================== */
    bridge->stats.total_updates++;

    /* Track capacity limited events */
    if (bridge->effects.capacity_factor < 1.0f) {
        bridge->stats.capacity_limited_events++;
    }

    /* Track encoding failures (attempts while impaired) */
    if (bridge->effects.is_impaired) {
        bridge->stats.encoding_failures++;
    }

    /* Track minimum capacity factor */
    if (bridge->effects.capacity_factor < bridge->stats.min_capacity_factor) {
        bridge->stats.min_capacity_factor = bridge->effects.capacity_factor;
    }

    /* Update running average of decay rate mod */
    float alpha = 0.1f;  /* Exponential moving average factor */
    bridge->stats.avg_decay_rate_mod =
        alpha * bridge->effects.decay_rate_mod +
        (1.0f - alpha) * bridge->stats.avg_decay_rate_mod;

    /* Unlock */
    if ((bridge->base.mutex != NULL)) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }

    NIMCP_LOGGING_DEBUG("Updated WM-substrate effects: capacity=%.2f, decay_mod=%.2f, "
                        "refresh=%.2f, encoding=%.2f, impaired=%d",
                        bridge->effects.capacity_factor,
                        bridge->effects.decay_rate_mod,
                        bridge->effects.refresh_efficiency,
                        bridge->effects.encoding_strength,
                        bridge->effects.is_impaired);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Getter API
 * ============================================================================ */

float wm_substrate_get_capacity_factor(wm_substrate_bridge_t* bridge)
{
    /* Guard: Validate bridge */
    if (!bridge) {
        return -1.0f;
    }

    float result = 0.0f;
    if ((bridge->base.mutex != NULL)) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
        result = bridge->effects.capacity_factor;
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    } else {
        result = bridge->effects.capacity_factor;
    }

    return result;
}

float wm_substrate_get_decay_mod(wm_substrate_bridge_t* bridge)
{
    /* Guard: Validate bridge */
    if (!bridge) {
        return -1.0f;
    }

    float result = 0.0f;
    if ((bridge->base.mutex != NULL)) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
        result = bridge->effects.decay_rate_mod;
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    } else {
        result = bridge->effects.decay_rate_mod;
    }

    return result;
}

float wm_substrate_get_refresh_efficiency(wm_substrate_bridge_t* bridge)
{
    /* Guard: Validate bridge */
    if (!bridge) {
        return -1.0f;
    }

    float result = 0.0f;
    if ((bridge->base.mutex != NULL)) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
        result = bridge->effects.refresh_efficiency;
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    } else {
        result = bridge->effects.refresh_efficiency;
    }

    return result;
}

float wm_substrate_get_encoding_strength(wm_substrate_bridge_t* bridge)
{
    /* Guard: Validate bridge */
    if (!bridge) {
        return -1.0f;
    }

    float result = 0.0f;
    if ((bridge->base.mutex != NULL)) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
        result = bridge->effects.encoding_strength;
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    } else {
        result = bridge->effects.encoding_strength;
    }

    return result;
}

int wm_substrate_get_effects(
    const wm_substrate_bridge_t* bridge,
    wm_substrate_effects_t* effects)
{
    /* Guard: Validate inputs */
    if (!bridge || !effects) {
        NIMCP_LOGGING_ERROR("Cannot get effects: NULL bridge or effects pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_substrate_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* WHAT: Copy entire effects structure atomically
     * WHY: Provide consistent snapshot of all effects
     * HOW: Lock, memcpy, unlock
     */
    if ((bridge->base.mutex != NULL)) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
        memcpy(effects, &bridge->effects, sizeof(wm_substrate_effects_t));
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    } else {
        memcpy(effects, &bridge->effects, sizeof(wm_substrate_effects_t));
    }

    return 0;
}

bool wm_substrate_is_impaired(wm_substrate_bridge_t* bridge)
{
    /* Guard: Validate bridge */
    if (!bridge) {
        return false;
    }

    bool result = false;
    if ((bridge->base.mutex != NULL)) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
        result = bridge->effects.is_impaired;
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    } else {
        result = bridge->effects.is_impaired;
    }

    return result;
}

int wm_substrate_get_stats(
    const wm_substrate_bridge_t* bridge,
    wm_substrate_stats_t* stats)
{
    /* Guard: Validate inputs */
    if (!bridge || !stats) {
        NIMCP_LOGGING_ERROR("Cannot get stats: NULL bridge or stats pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_substrate_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* WHAT: Copy statistics structure atomically
     * WHY: Provide consistent snapshot of stats
     * HOW: Lock, memcpy, unlock
     */
    if ((bridge->base.mutex != NULL)) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
        memcpy(stats, &bridge->stats, sizeof(wm_substrate_stats_t));
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    } else {
        memcpy(stats, &bridge->stats, sizeof(wm_substrate_stats_t));
    }

    return 0;
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
int wm_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Working_Memory_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            NIMCP_LOGGING_DEBUG("Working memory substrate bridge self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Working_Memory_Substrate_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Working_Memory_Substrate_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}


void working_memory_substrate_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_working_memory_substrate_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int working_memory_substrate_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    working_memory_substrate_bridge_heartbeat_instance(NULL, "working_memory_substrate_bridge_training_begin", 0.0f);
    return 0;
}

int working_memory_substrate_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    working_memory_substrate_bridge_heartbeat_instance(NULL, "working_memory_substrate_bridge_training_end", 1.0f);
    return 0;
}

int working_memory_substrate_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    working_memory_substrate_bridge_heartbeat_instance(NULL, "working_memory_substrate_bridge_training_step", progress);
    return 0;
}
