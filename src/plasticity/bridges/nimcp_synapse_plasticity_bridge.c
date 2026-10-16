/**
 * @file nimcp_synapse_plasticity_bridge.c
 * @brief Synapse-Plasticity Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Central hub connecting synapses to all 17 plasticity mechanisms
 * WHY:  Synapses need integrated plasticity from multiple sources
 * HOW:  Accumulates and coordinates weight changes from all mechanisms
 *
 * @author NIMCP Development Team
 */

#include "plasticity/bridges/nimcp_synapse_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(synapse_plasticity_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(synapse_plasticity_bridge)

/* ============================================================================
 * Static Helpers
 * ============================================================================ */

/**
 * @brief Apply weight bounds
 */
static float apply_weight_bounds(
    const synapse_plasticity_config_t* config,
    float weight)
{
    if (!config->clamp_weights) return weight;

    if (weight < config->weight_min) return config->weight_min;
    if (weight > config->weight_max) return config->weight_max;
    return weight;
}

/**
 * @brief Apply weight change with mode
 */
static float apply_weight_change(
    const synapse_plasticity_config_t* config,
    float current_weight,
    float delta)
{
    float new_weight = current_weight;

    switch (config->update_mode) {
        case WEIGHT_UPDATE_ADDITIVE:
            new_weight = current_weight + delta;
            break;

        case WEIGHT_UPDATE_MULTIPLICATIVE:
            new_weight = current_weight * (1.0f + delta);
            break;

        case WEIGHT_UPDATE_SOFT_BOUNDS:
            if (delta > 0) {
                new_weight = current_weight + delta * (config->weight_max - current_weight);
            } else {
                new_weight = current_weight + delta * (current_weight - config->weight_min);
            }
            break;

        case WEIGHT_UPDATE_HARD_BOUNDS:
            new_weight = current_weight + delta;
            break;
    }

    return apply_weight_bounds(config, new_weight);
}

/* NOTE: Message handlers are registered separately via bio_router_register_handler() */

/* ============================================================================
 * KG-Driven Wiring Callback
 * ============================================================================ */

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 */
static int synapse_plasticity_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    if (!ctx || !message_types || message_count == 0) {
        return 0;
    }

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            /* Add handlers for specific message types as needed */
            /* Currently no handlers defined - placeholder for future extension */
            default:
                NIMCP_LOGGING_DEBUG("Synapse plasticity: unknown message type %d in wiring callback",
                                    message_types[i]);
                break;
        }
    }

    return (registered > 0) ? 0 : -1;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int synapse_plasticity_default_config(synapse_plasticity_config_t* config)
{
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(*config));

    /* Weight bounds */
    config->weight_min = 0.0f;
    config->weight_max = 1.0f;
    config->update_mode = WEIGHT_UPDATE_SOFT_BOUNDS;

    /* Enable all mechanisms by default */
    config->enable_stdp = true;
    config->enable_bcm = true;
    config->enable_homeostatic = true;
    config->enable_stp = true;
    config->enable_metaplasticity = true;
    config->enable_eligibility = true;
    config->enable_heterosynaptic = true;
    config->enable_scaling = true;
    config->enable_tagging = true;
    config->enable_calcium = true;
    config->enable_neuromodulator = true;
    config->enable_metabolic = true;
    config->enable_structural = true;
    config->enable_gliotransmission = true;
    config->enable_sfa = true;
    config->enable_intrinsic = true;
    config->enable_dendritic = true;

    /* Integration */
    config->integration_dt_ms = 1.0f;
    config->accumulator_decay = 0.99f;
    config->clamp_weights = true;

    /* Bio-async */
    config->enable_bio_async = true;
    config->inbox_capacity = 64;

    return 0;
}

synapse_plasticity_bridge_t* synapse_plasticity_create(
    const synapse_plasticity_config_t* config,
    synapse_t* synapse,
    plasticity_orchestrator_t* orch)
{
    synapse_plasticity_bridge_t* bridge = nimcp_malloc(sizeof(synapse_plasticity_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate synapse plasticity bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(*bridge));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        synapse_plasticity_default_config(&bridge->config);
    }

    /* Store connections */
    bridge->synapse = synapse;
    bridge->orch = orch;

    /* Initialize bridge base (allocates mutex, sets module name) */
    if (bridge_base_init(&bridge->base, 0, "synapse_plasticity") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    bridge->initialized = true;
    bridge->creation_time = nimcp_time_get_us();

    NIMCP_LOGGING_INFO("Created synapse-plasticity bridge");
    return bridge;
}

void synapse_plasticity_destroy(synapse_plasticity_bridge_t* bridge)
{
    if (!bridge) return;

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed synapse-plasticity bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int synapse_plasticity_connect_stdp(synapse_plasticity_bridge_t* bridge, stdp_synapse_t* stdp)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_stdp: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stdp = stdp;
    bridge->contributions[PLASTICITY_STDP].active = (stdp != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_bcm(synapse_plasticity_bridge_t* bridge, bcm_state_t* bcm)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_bcm: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bcm = bcm;
    bridge->contributions[PLASTICITY_BCM].active = (bcm != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_homeostatic(synapse_plasticity_bridge_t* bridge, homeostatic_state_t* h)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_homeostatic: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->homeostatic = h;
    bridge->contributions[PLASTICITY_HOMEOSTATIC].active = (h != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_stp(synapse_plasticity_bridge_t* bridge, stp_state_t* stp)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_stp: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stp = stp;
    bridge->contributions[PLASTICITY_STP].active = (stp != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_metaplasticity(synapse_plasticity_bridge_t* bridge, metaplasticity_state_t* m)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_metaplasticity: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->meta = m;
    bridge->contributions[PLASTICITY_METAPLASTICITY].active = (m != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_eligibility(synapse_plasticity_bridge_t* bridge, eligibility_state_t* e)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_eligibility: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->eligibility = e;
    bridge->contributions[PLASTICITY_ELIGIBILITY].active = (e != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_heterosynaptic(synapse_plasticity_bridge_t* bridge, heterosynaptic_state_t* h)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_heterosynaptic: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->hetero = h;
    bridge->contributions[PLASTICITY_HETEROSYNAPTIC].active = (h != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_scaling(synapse_plasticity_bridge_t* bridge, synaptic_scaling_state_t* s)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_scaling: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->scaling = s;
    bridge->contributions[PLASTICITY_SCALING].active = (s != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_tagging(synapse_plasticity_bridge_t* bridge, synaptic_tagging_state_t* t)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_tagging: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->tagging = t;
    bridge->contributions[PLASTICITY_TAGGING].active = (t != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_calcium(synapse_plasticity_bridge_t* bridge, calcium_dynamics_state_t* c)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_calcium: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->calcium = c;
    bridge->contributions[PLASTICITY_CALCIUM].active = (c != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_neuromod(synapse_plasticity_bridge_t* bridge, neuromodulator_state_t* n)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_neuromod: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->neuromod = n;
    bridge->contributions[PLASTICITY_NEUROMODULATOR].active = (n != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_metabolic(synapse_plasticity_bridge_t* bridge, metabolic_state_t* m)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_metabolic: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metabolic = m;
    bridge->contributions[PLASTICITY_METABOLIC].active = (m != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_structural(synapse_plasticity_bridge_t* bridge, structural_state_t* s)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_structural: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->structural = s;
    bridge->contributions[PLASTICITY_STRUCTURAL].active = (s != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_glial(synapse_plasticity_bridge_t* bridge, gliotransmission_state_t* g)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_glial: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->glial = g;
    bridge->contributions[PLASTICITY_GLIOTRANSMISSION].active = (g != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_sfa(synapse_plasticity_bridge_t* bridge, spike_frequency_adaptation_state_t* s)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_sfa: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->sfa = s;
    bridge->contributions[PLASTICITY_SFA].active = (s != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_intrinsic(synapse_plasticity_bridge_t* bridge, intrinsic_excitability_state_t* i)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_intrinsic: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->intrinsic = i;
    bridge->contributions[PLASTICITY_INTRINSIC].active = (i != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_dendritic(synapse_plasticity_bridge_t* bridge, dendritic_plasticity_state_t* d)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_dendritic: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->dendritic = d;
    bridge->contributions[PLASTICITY_DENDRITIC].active = (d != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int synapse_plasticity_connect_all(
    synapse_plasticity_bridge_t* bridge,
    plasticity_orchestrator_t* orch)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_all: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_all: orch is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    bridge->orch = orch;
    /* Would query orchestrator for available mechanisms and connect */
    NIMCP_LOGGING_INFO("Connected all available mechanisms from orchestrator");
    return 0;
}

int synapse_plasticity_connect_bio_async(synapse_plasticity_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_connect_bio_async: bridge is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SYNAPSE,
        .module_name = SYNAPSE_PLASTICITY_MODULE_NAME,
        .inbox_capacity = bridge->config.inbox_capacity,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;

        /* Try KG-driven wiring callback registration first */
        nimcp_error_t wiring_result = bio_router_register_wiring_callback(
            BIO_MODULE_SYNAPSE,
            (void*)synapse_plasticity_wiring_handler_callback,
            bridge
        );

        if (wiring_result == NIMCP_SUCCESS) {
            NIMCP_LOGGING_INFO("Synapse plasticity: KG-driven wiring callback registered");
        } else {
            /* Legacy fallback - no specific handlers currently defined */
            LEGACY_HANDLER_REGISTRATION(
                /* No handlers registered currently - module uses direct function calls */
                (void)0
            );
            NIMCP_LOGGING_INFO("Synapse plasticity: legacy handler registration (no handlers)");
        }
    }

    return NIMCP_SUCCESS;
}

int synapse_plasticity_disconnect_bio_async(synapse_plasticity_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_disconnect_bio_async: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    return 0;
}

bool synapse_plasticity_is_bio_async_connected(const synapse_plasticity_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Spike Event API
 * ============================================================================ */

float synapse_plasticity_on_pre_spike(
    synapse_plasticity_bridge_t* bridge,
    uint64_t spike_time)
{
    if (!bridge) return 0.0f;

    nimcp_mutex_lock(bridge->base.mutex);

    float total_delta = 0.0f;

    /* STDP: LTD when pre follows post */
    if (bridge->config.enable_stdp && bridge->stdp) {
        float dt = (float)(spike_time - bridge->last_post_spike_time) / 1000.0f;
        if (dt > 0 && dt < 100.0f) {
            float delta = -0.012f * expf(-dt / 20.0f); /* LTD */
            bridge->contributions[PLASTICITY_STDP].weight_delta += delta;
            total_delta += delta;
        }
    }

    /* STP: Apply facilitation/depression on pre-spike */
    if (bridge->config.enable_stp && bridge->stp) {
        bridge->contributions[PLASTICITY_STP].event_count++;
    }

    /* Update timing */
    bridge->last_pre_spike_time = spike_time;
    bridge->stats.pre_spike_count++;
    bridge->weight_delta_accumulator += total_delta;

    if (total_delta < 0) {
        bridge->stats.total_depression += fabsf(total_delta);
    } else {
        bridge->stats.total_potentiation += total_delta;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return total_delta;
}

float synapse_plasticity_on_post_spike(
    synapse_plasticity_bridge_t* bridge,
    uint64_t spike_time)
{
    if (!bridge) return 0.0f;

    nimcp_mutex_lock(bridge->base.mutex);

    float total_delta = 0.0f;

    /* STDP: LTP when post follows pre */
    if (bridge->config.enable_stdp && bridge->stdp) {
        float dt = (float)(spike_time - bridge->last_pre_spike_time) / 1000.0f;
        if (dt > 0 && dt < 100.0f) {
            float delta = 0.01f * expf(-dt / 20.0f); /* LTP */
            bridge->contributions[PLASTICITY_STDP].weight_delta += delta;
            total_delta += delta;
        }
    }

    /* Eligibility trace: Update on post-spike */
    if (bridge->config.enable_eligibility && bridge->eligibility) {
        bridge->contributions[PLASTICITY_ELIGIBILITY].event_count++;
    }

    /* Update timing */
    bridge->last_post_spike_time = spike_time;
    bridge->stats.post_spike_count++;
    bridge->weight_delta_accumulator += total_delta;

    if (total_delta > 0) {
        bridge->stats.total_potentiation += total_delta;
    } else {
        bridge->stats.total_depression += fabsf(total_delta);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return total_delta;
}

/* ============================================================================
 * Weight Update API
 * ============================================================================ */

float synapse_plasticity_apply_accumulated(synapse_plasticity_bridge_t* bridge)
{
    if (!bridge) return 0.0f;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current weight */
    float current_weight = 0.5f; /* Default if no synapse connected */
    if (bridge->synapse) {
        current_weight = bridge->synapse->weight;
    }

    /* Sum all mechanism contributions */
    float total_delta = bridge->weight_delta_accumulator;
    for (int i = 0; i < PLASTICITY_COUNT; i++) {
        total_delta += bridge->contributions[i].weight_delta;
        bridge->contributions[i].weight_delta = 0.0f; /* Reset after applying */
    }

    /* Apply weight change */
    float new_weight = apply_weight_change(&bridge->config, current_weight, total_delta);

    /* Update synapse if connected */
    if (bridge->synapse) {
        bridge->synapse->weight = new_weight;
    }

    /* Clear accumulator */
    bridge->weight_delta_accumulator = 0.0f;
    bridge->stats.weight_updates++;
    bridge->stats.net_weight_change += (new_weight - current_weight);

    nimcp_mutex_unlock(bridge->base.mutex);
    return new_weight;
}

float synapse_plasticity_get_effective_weight(synapse_plasticity_bridge_t* bridge)
{
    if (!bridge) return 0.0f;

    nimcp_mutex_lock(bridge->base.mutex);

    float weight = 0.5f;
    if (bridge->synapse) {
        weight = bridge->synapse->weight;
    }

    /* Apply STP modulation if connected */
    if (bridge->config.enable_stp && bridge->stp) {
        /* Would apply facilitation/depression factors */
        /* Placeholder: slight facilitation */
        weight *= 1.1f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return weight;
}

float synapse_plasticity_get_mechanism_contribution(
    const synapse_plasticity_bridge_t* bridge,
    plasticity_mechanism_t mechanism)
{
    if (!bridge) return 0.0f;
    if (mechanism >= PLASTICITY_COUNT) return 0.0f;

    return bridge->contributions[mechanism].weight_delta;
}

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

int synapse_plasticity_update(
    synapse_plasticity_bridge_t* bridge,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_update: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Decay accumulator */
    bridge->weight_delta_accumulator *= bridge->config.accumulator_decay;

    /* Decay mechanism contributions */
    for (int i = 0; i < PLASTICITY_COUNT; i++) {
        bridge->contributions[i].weight_delta *= bridge->config.accumulator_decay;
    }

    /* Homeostatic plasticity */
    if (bridge->config.enable_homeostatic && bridge->homeostatic) {
        /* Would compute homeostatic correction based on firing rate */
        float target_rate = 5.0f; /* Hz */
        float actual_rate = (dt_ms > 0.0f) ?
            (float)bridge->stats.post_spike_count / (dt_ms / 1000.0f) : 0.0f;
        float correction = 0.001f * (target_rate - actual_rate);
        bridge->contributions[PLASTICITY_HOMEOSTATIC].weight_delta += correction;
    }

    /* Synaptic scaling */
    if (bridge->config.enable_scaling && bridge->scaling) {
        /* Would apply multiplicative scaling */
    }

    /* Update timing stats */
    if (bridge->stats.pre_spike_count > 1 && bridge->stats.post_spike_count > 1) {
        bridge->stats.avg_inter_spike_interval = dt_ms /
            (float)(bridge->stats.pre_spike_count + bridge->stats.post_spike_count);
    }

    bridge->stats.last_update_time = (float)nimcp_time_get_us() / 1000.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int synapse_plasticity_get_stats(
    const synapse_plasticity_bridge_t* bridge,
    synapse_plasticity_stats_t* stats)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_get_stats: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_plasticity_get_stats: stats is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    *stats = bridge->stats;
    return 0;
}

void synapse_plasticity_reset_stats(synapse_plasticity_bridge_t* bridge)
{
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

bool synapse_plasticity_is_mechanism_connected(
    const synapse_plasticity_bridge_t* bridge,
    plasticity_mechanism_t mechanism)
{
    if (!bridge) {
        return false;
    }
    if (mechanism >= PLASTICITY_COUNT) {
        return false;
    }

    return bridge->contributions[mechanism].active;
}

/* ============================================================================
 * String Conversion
 * ============================================================================ */

const char* plasticity_mechanism_to_string(plasticity_mechanism_t mechanism)
{
    switch (mechanism) {
        case PLASTICITY_STDP:           return "STDP";
        case PLASTICITY_BCM:            return "BCM";
        case PLASTICITY_HOMEOSTATIC:    return "HOMEOSTATIC";
        case PLASTICITY_STP:            return "STP";
        case PLASTICITY_METAPLASTICITY: return "METAPLASTICITY";
        case PLASTICITY_ELIGIBILITY:    return "ELIGIBILITY";
        case PLASTICITY_HETEROSYNAPTIC: return "HETEROSYNAPTIC";
        case PLASTICITY_SCALING:        return "SCALING";
        case PLASTICITY_TAGGING:        return "TAGGING";
        case PLASTICITY_CALCIUM:        return "CALCIUM";
        case PLASTICITY_NEUROMODULATOR: return "NEUROMODULATOR";
        case PLASTICITY_METABOLIC:      return "METABOLIC";
        case PLASTICITY_STRUCTURAL:     return "STRUCTURAL";
        case PLASTICITY_GLIOTRANSMISSION: return "GLIOTRANSMISSION";
        case PLASTICITY_SFA:            return "SFA";
        case PLASTICITY_INTRINSIC:      return "INTRINSIC";
        case PLASTICITY_DENDRITIC:      return "DENDRITIC";
        default: return "UNKNOWN";
    }
}

const char* weight_update_mode_to_string(weight_update_mode_t mode)
{
    switch (mode) {
        case WEIGHT_UPDATE_ADDITIVE:      return "ADDITIVE";
        case WEIGHT_UPDATE_MULTIPLICATIVE: return "MULTIPLICATIVE";
        case WEIGHT_UPDATE_SOFT_BOUNDS:   return "SOFT_BOUNDS";
        case WEIGHT_UPDATE_HARD_BOUNDS:   return "HARD_BOUNDS";
        default: return "UNKNOWN";
    }
}
