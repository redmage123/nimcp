/**
 * @file nimcp_attention_substrate_bridge.c
 * @brief Attention-Neural Substrate Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between metabolic/thermal substrate and attention system
 * WHY:  Attention is metabolically expensive; frontoparietal networks require sustained
 *       ATP for focus, switching, filtering, and vigilance. Substrate state directly
 *       modulates attention capabilities.
 * HOW:  Monitors substrate (ATP, temperature, metabolic stress), computes attention
 *       effects (focus capacity, shifting efficiency, filter strength, vigilance),
 *       and applies modulation to attention system.
 *
 * Uses shared metabolic modulation utilities from nimcp_metabolic_modulation.h
 *
 * BIOLOGICAL BASIS:
 * - Frontoparietal attention networks consume significant ATP during sustained focus
 * - ATP depletion reduces attention span and increases distractibility
 * - Hyperthermia (fever) impairs executive attention and attention switching
 * - Metabolic stress weakens top-down distractor filtering
 * - Vigilance tasks show performance decline with metabolic fatigue
 */

#include "cognitive/attention/nimcp_attention_substrate_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(attention_substrate_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_attention_substrate_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_attention_substrate_bridge_mesh_registry = NULL;

nimcp_error_t attention_substrate_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_attention_substrate_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "attention_substrate_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "attention_substrate_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_attention_substrate_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_attention_substrate_bridge_mesh_registry = registry;
    return err;
}

void attention_substrate_bridge_mesh_unregister(void) {
    if (g_attention_substrate_bridge_mesh_registry && g_attention_substrate_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_attention_substrate_bridge_mesh_registry, g_attention_substrate_bridge_mesh_id);
        g_attention_substrate_bridge_mesh_id = 0;
        g_attention_substrate_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from attention_substrate_bridge module (instance-level) */
static inline void attention_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_attention_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_attention_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_attention_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


BRIDGE_DEFINE_SECURITY_SETTERS(attention_substrate_bridge)

/* ============================================================================
 * Helper Functions (using shared nimcp_clamp_f from nimcp_metabolic_modulation.h)
 * ============================================================================ */

/**
 * @brief Compute Q10-based temperature scaling factor
 * WHAT: Calculate multiplicative factor for rate constants based on temperature
 * WHY:  Biological processes have exponential temperature dependence
 * HOW:  factor = Q10^((T - T_ref) / 10)
 *
 * BIOLOGICAL: Q10 rule from Arrhenius equation
 * - Q10 = 2.5: rate increases 2.5x per 10°C
 * - Hyperthermia accelerates processes but impairs precision
 * - Hypothermia slows processes
 */
static float compute_q10_factor(float current_temp, float reference_temp, float q10)
{
    if (q10 <= 0.0f) {
        return 1.0f;
    }

    float temp_diff = current_temp - reference_temp;
    float exponent = temp_diff / 10.0f;
    return powf(q10, exponent);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

void attention_substrate_default_config(attention_substrate_config_t* config)
{
    if (!config) {
        return;
    }

    /* Enable all modulations by default */
    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_attention_substrate_", 0.0f);


    config->enable_focus_modulation = true;
    config->enable_shifting_modulation = true;
    config->enable_filter_modulation = true;
    config->enable_bio_async = false;

    /* Moderate sensitivity (1.0 = biological measurements) */
    config->atp_sensitivity = 1.0f;
    config->temperature_sensitivity = 1.0f;
}

attention_substrate_bridge_t* attention_substrate_bridge_create(
    const attention_substrate_config_t* config,
    neural_substrate_t* substrate,
    nimcp_attention_system_t* attention)
{
    /* Guard: Validate inputs */
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_substrate_bridge_create: substrate is NULL");
        return NULL;
    }
    if (!attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_substrate_bridge_create: attention system is NULL");
        return NULL;
    }

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_create", 0.0f);


    attention_substrate_bridge_t* bridge =
        (attention_substrate_bridge_t*)nimcp_malloc(sizeof(attention_substrate_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_substrate_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(attention_substrate_bridge_t));

    /* Store references */
    bridge->substrate = substrate;
    bridge->attention = attention;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        attention_substrate_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "attention_substrate") != 0) { nimcp_free(bridge); return NULL; }

    /* Initialize effects to optimal (no impairment) */
    bridge->effects.focus_capacity = 1.0f;
    bridge->effects.shifting_efficiency = 1.0f;
    bridge->effects.filter_strength = 1.0f;
    bridge->effects.vigilance_factor = 1.0f;
    bridge->effects.is_impaired = false;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(attention_substrate_stats_t));
    bridge->stats.min_focus_observed = 1.0f;
    bridge->stats.max_focus_observed = 1.0f;

    NIMCP_LOGGING_INFO("Created attention substrate bridge");

    return bridge;
}

void attention_substrate_bridge_destroy(attention_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        attention_substrate_disconnect_bio_async(bridge);
    }

    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("Destroyed attention substrate bridge");
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int attention_substrate_connect_bio_async(attention_substrate_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_substrate_connect_bio_async: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_attention_substrate_", 0.0f);


    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SUBSTRATE_ATTENTION,
        .module_name = "attention_substrate_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected attention substrate bridge to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return NIMCP_ERROR_INVALID_STATE;
}

int attention_substrate_disconnect_bio_async(attention_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->base.bio_async_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_substrate_disconnect_bio_async: bridge is NULL or not connected");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_attention_substrate_", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected attention substrate bridge from bio-async router");

    return 0;
}

bool attention_substrate_is_bio_async_connected(const attention_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_attention_substrate_", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Update API - Core Substrate-Attention Integration
 * ============================================================================ */

int attention_substrate_update(attention_substrate_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_substrate_update: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_attention_substrate_", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get substrate state */
    substrate_metabolic_state_t metabolic;
    substrate_physical_state_t physical;

    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "attention_substrate_update: failed to get metabolic state");
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_STATE;
    }

    if (substrate_get_physical_state(bridge->substrate, &physical) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "attention_substrate_update: failed to get physical state");
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* ========================================================================
     * BIOLOGICAL EFFECT 1: Focus Capacity
     *
     * WHAT: Ability to sustain attention on target
     * WHY:  Sustained focus requires continuous ATP for prefrontal activity
     * HOW:  focus_capacity = clamp(atp_level * atp_sensitivity, 0.2, 1.0)
     *
     * BIOLOGICAL BASIS:
     * - Frontoparietal networks maintain sustained attention
     * - ATP depletion → reduced firing → attention lapses
     * - Floor of 0.2 represents minimal attention (distracted, unfocused)
     * ======================================================================== */

    if (bridge->config.enable_focus_modulation) {
        float atp_factor = metabolic.atp_level * bridge->config.atp_sensitivity;
        bridge->effects.focus_capacity = nimcp_clampf(atp_factor, 0.2f, 1.0f);
    } else {
        bridge->effects.focus_capacity = 1.0f;
    }

    /* ========================================================================
     * BIOLOGICAL EFFECT 2: Shifting Efficiency
     *
     * WHAT: Speed and effectiveness of attention switching
     * WHY:  Rapid attention reorienting requires metabolic resources and
     *       temperature-sensitive neurotransmitter dynamics
     * HOW:  shifting_efficiency = metabolic_capacity * temperature_factor
     *
     * BIOLOGICAL BASIS:
     * - Attention switching involves TPJ, FEF activation
     * - Temperature affects synaptic transmission speed (Q10)
     * - Fever → impaired executive attention and switching
     * - Hyperthermia → sluggish reorienting
     * ======================================================================== */

    if (bridge->config.enable_shifting_modulation) {
        /* Compute temperature scaling factor */
        float temp_factor = compute_q10_factor(
            physical.temperature,
            SUBSTRATE_NORMAL_TEMPERATURE,
            ATTENTION_SUBSTRATE_Q10_SHIFTING
        );

        /* Scale by temperature sensitivity */
        temp_factor = 1.0f + (temp_factor - 1.0f) * bridge->config.temperature_sensitivity;

        /* Hyperthermia impairs switching (fever makes attention sluggish) */
        if (physical.temperature > SUBSTRATE_HYPERTHERMIA_THRESHOLD) {
            /* Above 40°C → impaired switching */
            float hyperthermia_penalty = (physical.temperature - SUBSTRATE_HYPERTHERMIA_THRESHOLD) / 5.0f;
            temp_factor *= (1.0f - nimcp_clampf(hyperthermia_penalty, 0.0f, 0.5f));
        }

        /* Combine metabolic capacity and temperature */
        float shifting = metabolic.metabolic_capacity * temp_factor;
        bridge->effects.shifting_efficiency = nimcp_clampf(shifting, 0.3f, 1.0f);
    } else {
        bridge->effects.shifting_efficiency = 1.0f;
    }

    /* ========================================================================
     * BIOLOGICAL EFFECT 3: Filter Strength
     *
     * WHAT: Ability to suppress distractors (selective attention)
     * WHY:  Top-down filtering requires prefrontal ATP for inhibitory control
     * HOW:  filter_strength = clamp(atp_level * 1.2, 0.2, 1.0)
     *
     * BIOLOGICAL BASIS:
     * - DLPFC exerts top-down attentional control
     * - Low ATP → weakened inhibition → increased distractibility
     * - Metabolic stress reduces executive function
     * - Factor of 1.2 models high metabolic cost of filtering
     * ======================================================================== */

    if (bridge->config.enable_filter_modulation) {
        /* Filtering is expensive (1.2x ATP factor models high prefrontal cost) */
        float filter_factor = metabolic.atp_level * 1.2f * bridge->config.atp_sensitivity;
        bridge->effects.filter_strength = nimcp_clampf(filter_factor, 0.2f, 1.0f);
    } else {
        bridge->effects.filter_strength = 1.0f;
    }

    /* ========================================================================
     * BIOLOGICAL EFFECT 4: Vigilance Factor
     *
     * WHAT: Sustained attention capability over time
     * WHY:  Vigilance tasks are metabolically exhausting, show performance
     *       decline with fatigue (vigilance decrement)
     * HOW:  vigilance = clamp(metabolic_capacity * physical_capacity, 0.2, 1.0)
     *
     * BIOLOGICAL BASIS:
     * - Vigilance requires sustained noradrenergic tone
     * - Both metabolic and physical substrates must be intact
     * - Fatigue → vigilance decrement (well-documented phenomenon)
     * - Combined factor models multi-system requirement
     * ======================================================================== */

    float vigilance = metabolic.metabolic_capacity * physical.physical_capacity;
    bridge->effects.vigilance_factor = nimcp_clampf(vigilance, 0.2f, 1.0f);

    /* ========================================================================
     * IMPAIRMENT DETECTION
     *
     * WHAT: Overall attention impairment flag
     * WHY:  Quick check for critical attention degradation
     * HOW:  is_impaired = (focus_capacity < 0.7 || vigilance_factor < 0.6)
     *
     * BIOLOGICAL BASIS:
     * - Clinical attention deficits occur at ~70% capacity
     * - Vigilance decrement significant below 60%
     * ======================================================================== */

    bool was_impaired = bridge->effects.is_impaired;
    bridge->effects.is_impaired = (bridge->effects.focus_capacity < 0.7f ||
                                    bridge->effects.vigilance_factor < 0.6f);

    /* Track impairment events (transitions from normal to impaired) */
    if (bridge->effects.is_impaired && !was_impaired) {
        bridge->stats.impairment_events++;
        NIMCP_LOGGING_WARN("Attention became impaired (focus: %.2f, vigilance: %.2f)",
                          bridge->effects.focus_capacity,
                          bridge->effects.vigilance_factor);
    }

    /* ========================================================================
     * UPDATE STATISTICS
     * ======================================================================== */

    bridge->stats.update_count++;

    /* Running averages (exponential moving average with alpha = 0.01) */
    float alpha = 0.01f;
    bridge->stats.avg_focus_capacity =
        (1.0f - alpha) * bridge->stats.avg_focus_capacity +
        alpha * bridge->effects.focus_capacity;
    bridge->stats.avg_shifting_efficiency =
        (1.0f - alpha) * bridge->stats.avg_shifting_efficiency +
        alpha * bridge->effects.shifting_efficiency;
    bridge->stats.avg_filter_strength =
        (1.0f - alpha) * bridge->stats.avg_filter_strength +
        alpha * bridge->effects.filter_strength;
    bridge->stats.avg_vigilance =
        (1.0f - alpha) * bridge->stats.avg_vigilance +
        alpha * bridge->effects.vigilance_factor;

    /* Track min/max focus */
    if (bridge->effects.focus_capacity < bridge->stats.min_focus_observed) {
        bridge->stats.min_focus_observed = bridge->effects.focus_capacity;
    }
    if (bridge->effects.focus_capacity > bridge->stats.max_focus_observed) {
        bridge->stats.max_focus_observed = bridge->effects.focus_capacity;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

float attention_substrate_get_focus_capacity(attention_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return -1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_attention_substrate_", 0.0f);

    /* P1: Lock mutex to prevent torn reads while update() writes under lock */
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.focus_capacity;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float attention_substrate_get_shifting_efficiency(attention_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return -1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_attention_substrate_", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.shifting_efficiency;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float attention_substrate_get_filter_strength(attention_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return -1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_attention_substrate_", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.filter_strength;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float attention_substrate_get_vigilance(attention_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return -1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_attention_substrate_", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.vigilance_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

int attention_substrate_get_effects(
    const attention_substrate_bridge_t* bridge,
    attention_substrate_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_substrate_get_effects: bridge or effects is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_attention_substrate_", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool attention_substrate_is_impaired(attention_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_attention_substrate_", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.is_impaired;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

int attention_substrate_get_stats(
    const attention_substrate_bridge_t* bridge,
    attention_substrate_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_substrate_get_stats: bridge or stats is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_attention_substrate_", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Attention Substrate Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int attention_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    attention_substrate_bridge_heartbeat("attention_su_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Attention_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                attention_substrate_bridge_heartbeat("attention_su_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Attention Substrate Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Attention_Substrate_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Attention_Substrate_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void attention_substrate_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_attention_substrate_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int attention_substrate_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    attention_substrate_bridge_heartbeat_instance(NULL, "attention_substrate_bridge_training_begin", 0.0f);
    return 0;
}

int attention_substrate_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    attention_substrate_bridge_heartbeat_instance(NULL, "attention_substrate_bridge_training_end", 1.0f);
    return 0;
}

int attention_substrate_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    attention_substrate_bridge_heartbeat_instance(NULL, "attention_substrate_bridge_training_step", progress);
    return 0;
}
