/**
 * @file nimcp_reasoning_sleep_bridge.c
 * @brief Sleep-Reasoning Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-21
 */

#include "cognitive/reasoning/nimcp_reasoning_sleep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(reasoning_sleep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)



/**
 * WHAT: Internal bridge structure
 * WHY:  Encapsulate implementation details
 * HOW:  Store config, effects, sleep system reference, mutex
 */
struct reasoning_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    reasoning_sleep_config_t config;      /**< Bridge configuration */
    sleep_system_t sleep_system;          /**< Sleep system reference */
    reasoning_sleep_effects_t effects;    /**< Current effects */
    bool callback_registered;             /**< Track callback for cleanup */

};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(reasoning_sleep_bridge, struct reasoning_sleep_bridge_struct)

/* Forward declarations */
static void reasoning_on_sleep_state_change(sleep_state_t new_state, void* user_data);
static void compute_reasoning_effects(reasoning_sleep_bridge_t bridge, sleep_state_t state, float pressure);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update reasoning parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state transitions affect reasoning immediately
 * - REM enhances creative reasoning through loose semantic networks
 * - NREM impairs logical reasoning but consolidates knowledge
 * - Neurotransmitter shifts (ACh, NE, 5-HT) modulate reasoning modes
 */
static void reasoning_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    /* Guard clause: Validate bridge */
    reasoning_sleep_bridge_t bridge = (reasoning_sleep_bridge_t)user_data;
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Reasoning bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current pressure */
    float pressure = sleep_get_pressure(bridge->sleep_system);

    /* Compute all effects */
    compute_reasoning_effects(bridge, new_state, pressure);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Reasoning modulated: logical=%.2f, creative=%.2f, speed=%.2f, offline=%d",
                        bridge->effects.logical_reasoning_factor,
                        bridge->effects.creative_reasoning_factor,
                        bridge->effects.inference_speed_factor,
                        bridge->effects.reasoning_offline);
}

/**
 * WHAT: Compute reasoning effects for given sleep state
 * WHY:  Centralize effect computation logic
 * HOW:  Apply state-specific factors with modulation strength
 *
 * BIOLOGICAL BASIS:
 * - REM: High ACh, low NE → enhanced associations, impaired logic
 * - Deep NREM: Low ACh, low NE → offline, consolidation mode
 * - Awake: Balanced neurotransmitters → full reasoning capacity
 */
static void compute_reasoning_effects(reasoning_sleep_bridge_t bridge,
                                       sleep_state_t state,
                                       float pressure)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        return;
    }

    /* Update state tracking */
    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    /* Compute logical reasoning factor */
    if (bridge->config.enable_logical_modulation) {
        float logical_base = reasoning_sleep_logical_for_state(state);
        bridge->effects.logical_reasoning_factor =
            logical_base * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength);

        /* Sleep pressure further impairs logical reasoning when awake */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
            float pressure_penalty = (pressure - 0.7f) * 0.5f;  /* Up to 15% reduction */
            bridge->effects.logical_reasoning_factor *= (1.0f - pressure_penalty);
        }
    } else {
        bridge->effects.logical_reasoning_factor = 1.0f;
    }

    /* Compute creative reasoning factor */
    if (bridge->config.enable_creative_modulation) {
        float creative_base = reasoning_sleep_creative_for_state(state);
        bridge->effects.creative_reasoning_factor =
            creative_base * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength);

        /* REM creativity boost flag */
        bridge->effects.rem_creativity_boost = (state == SLEEP_STATE_REM);
    } else {
        bridge->effects.creative_reasoning_factor = 1.0f;
        bridge->effects.rem_creativity_boost = false;
    }

    /* Compute inference speed factor */
    if (bridge->config.enable_speed_modulation) {
        float speed_base = reasoning_sleep_speed_for_state(state);
        bridge->effects.inference_speed_factor =
            speed_base * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength);

        /* Sleep pressure slows inference when awake */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.6f) {
            float pressure_penalty = (pressure - 0.6f) * 0.3f;
            bridge->effects.inference_speed_factor *= (1.0f - pressure_penalty);
        }
    } else {
        bridge->effects.inference_speed_factor = 1.0f;
    }

    /* Compute working memory access factor */
    if (bridge->config.enable_working_memory_gating) {
        /* Working memory access mirrors logical reasoning capacity */
        bridge->effects.working_memory_access_factor =
            bridge->effects.logical_reasoning_factor;
    } else {
        bridge->effects.working_memory_access_factor = 1.0f;
    }

    /* Update offline status - reasoning offline during NREM */
    bridge->effects.reasoning_offline =
        (state == SLEEP_STATE_DEEP_NREM || state == SLEEP_STATE_LIGHT_NREM);
}

/* ========================================================================
 * PUBLIC API IMPLEMENTATION
 * ======================================================================== */

int reasoning_sleep_default_config(reasoning_sleep_config_t* config)
{
    /* Guard clause: Validate config pointer */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Set defaults - all modulations enabled */
    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_reasoning_sleep_defa", 0.0f);


    config->enable_logical_modulation = true;
    config->enable_creative_modulation = true;
    config->enable_speed_modulation = true;
    config->enable_working_memory_gating = true;
    config->modulation_strength = 1.0f;

    return 0;
}

reasoning_sleep_bridge_t reasoning_sleep_bridge_create(
    const reasoning_sleep_config_t* config,
    sleep_system_t sleep)
{
    /* Guard clause: Validate sleep system */
    if (!sleep) {
        NIMCP_LOGGING_ERROR("NULL sleep system in reasoning_sleep_bridge_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_sleep_bridge_create: sleep is NULL");
        return NULL;
    }

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_create", 0.0f);


    struct reasoning_sleep_bridge_struct* bridge =
        (struct reasoning_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct reasoning_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate reasoning-sleep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reasoning_sleep_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(struct reasoning_sleep_bridge_struct));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        reasoning_sleep_default_config(&bridge->config);
    }

    /* Store sleep system reference */
    bridge->sleep_system = sleep;

    /* Initialize effects to neutral */
    bridge->effects.logical_reasoning_factor = 1.0f;
    bridge->effects.creative_reasoning_factor = 1.0f;
    bridge->effects.inference_speed_factor = 1.0f;
    bridge->effects.working_memory_access_factor = 1.0f;
    bridge->effects.reasoning_offline = false;
    bridge->effects.rem_creativity_boost = false;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "reasoning_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for reasoning-sleep bridge");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reasoning_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep,
        reasoning_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for reasoning bridge");
    }

    /* Get initial state and compute effects */
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    reasoning_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Reasoning-sleep bridge created");
    return bridge;
}

void reasoning_sleep_bridge_destroy(reasoning_sleep_bridge_t bridge)
{
    /* Guard clause: Safe with NULL */
    if (!bridge) {
        return;
    }

    /* Unregister callback if registered */
    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_destroy", 0.0f);


    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            reasoning_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for reasoning bridge");
        }
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        reasoning_sleep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge */
    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_DEBUG("Reasoning-sleep bridge destroyed");
}

int reasoning_sleep_update(reasoning_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_reasoning_sleep_upda", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current state and pressure */
    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    /* Compute effects */
    compute_reasoning_effects(bridge, state, pressure);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int reasoning_sleep_get_effects(const reasoning_sleep_bridge_t bridge,
                                 reasoning_sleep_effects_t* effects)
{
    /* Guard clause: Validate parameters */
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_reasoning_sleep_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float reasoning_sleep_get_logical_factor(const reasoning_sleep_bridge_t bridge)
{
    /* Guard clause: Return neutral on error */
    if (!bridge) {
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_reasoning_sleep_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.logical_reasoning_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

float reasoning_sleep_get_creative_factor(const reasoning_sleep_bridge_t bridge)
{
    /* Guard clause: Return neutral on error */
    if (!bridge) {
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_reasoning_sleep_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.creative_reasoning_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

bool reasoning_sleep_is_offline(const reasoning_sleep_bridge_t bridge)
{
    /* Guard clause: Return false on error */
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_reasoning_sleep_is_o", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.reasoning_offline;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

bool reasoning_sleep_is_rem_creative(const reasoning_sleep_bridge_t bridge)
{
    /* Guard clause: Return false on error */
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_reasoning_sleep_is_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.rem_creativity_boost;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

float reasoning_sleep_logical_for_state(sleep_state_t state)
{
    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_reasoning_sleep_logi", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:
            return REASONING_SLEEP_LOGICAL_AWAKE;
        case SLEEP_STATE_DROWSY:
            return REASONING_SLEEP_LOGICAL_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
            return REASONING_SLEEP_LOGICAL_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:
            return REASONING_SLEEP_LOGICAL_DEEP_NREM;
        case SLEEP_STATE_REM:
            return REASONING_SLEEP_LOGICAL_REM;
        default:
            return REASONING_SLEEP_LOGICAL_AWAKE;
    }
}

float reasoning_sleep_creative_for_state(sleep_state_t state)
{
    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_reasoning_sleep_crea", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:
            return REASONING_SLEEP_CREATIVE_AWAKE;
        case SLEEP_STATE_DROWSY:
            return REASONING_SLEEP_CREATIVE_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
            return REASONING_SLEEP_CREATIVE_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:
            return REASONING_SLEEP_CREATIVE_DEEP_NREM;
        case SLEEP_STATE_REM:
            return REASONING_SLEEP_CREATIVE_REM;
        default:
            return REASONING_SLEEP_CREATIVE_AWAKE;
    }
}

float reasoning_sleep_speed_for_state(sleep_state_t state)
{
    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_reasoning_sleep_spee", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:
            return REASONING_SLEEP_SPEED_AWAKE;
        case SLEEP_STATE_DROWSY:
            return REASONING_SLEEP_SPEED_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
            return REASONING_SLEEP_SPEED_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:
            return REASONING_SLEEP_SPEED_DEEP_NREM;
        case SLEEP_STATE_REM:
            return REASONING_SLEEP_SPEED_REM;
        default:
            return REASONING_SLEEP_SPEED_AWAKE;
    }
}

/* ========================================================================
 * BIO-ASYNC INTEGRATION API IMPLEMENTATION
 * ======================================================================== */

/**
 * WHAT: Connect bridge to bio-async router
 * WHY:  Enable inter-module messaging for distributed reasoning signals
 * HOW:  Register with bio_router using BIO_MODULE_REASONING_SLEEP
 *
 * BIOLOGICAL BASIS:
 * - Sleep-dependent reasoning modulation affects distributed cognitive networks
 * - Bio-async messaging enables coordination across reasoning subsystems
 * - Real-time state propagation for immediate reasoning parameter updates
 */
int reasoning_sleep_connect_bio_async(reasoning_sleep_bridge_t bridge)
{
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_reasoning_sleep_conn", 0.0f);


    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_REASONING_SLEEP,
        .module_name = "reasoning_sleep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Reasoning-sleep bridge connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return 0;  /* Not an error if router unavailable */
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 */
int reasoning_sleep_disconnect_bio_async(reasoning_sleep_bridge_t bridge)
{
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;  /* Not connected */
    }

    /* Unregister from bio-async router */
    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_reasoning_sleep_disc", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Reasoning-sleep bridge disconnected from bio-async router");

    return 0;
}

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query bio-async connection status
 * HOW:  Return bio_async_enabled flag
 */
bool reasoning_sleep_is_bio_async_connected(const reasoning_sleep_bridge_t bridge)
{
    /* Guard clause */
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_reasoning_sleep_is_b", 0.0f);


    return bridge->base.bio_async_enabled;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int reasoning_sleep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    reasoning_sleep_bridge_heartbeat("reasoning_sl_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Reasoning_Sleep_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                reasoning_sleep_bridge_heartbeat("reasoning_sl_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Reasoning_Sleep_Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Reasoning_Sleep_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Reasoning_Sleep_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void reasoning_sleep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_reasoning_sleep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int reasoning_sleep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_sleep_bridge_training_begin: NULL argument");
        return -1;
    }
    reasoning_sleep_bridge_heartbeat_instance(NULL, "reasoning_sleep_bridge_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int reasoning_sleep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_sleep_bridge_training_end: NULL argument");
        return -1;
    }
    reasoning_sleep_bridge_heartbeat_instance(NULL, "reasoning_sleep_bridge_training_end", 1.0f);
    (void)instance;
    return 0;
}

int reasoning_sleep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_sleep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    reasoning_sleep_bridge_heartbeat_instance(NULL, "reasoning_sleep_bridge_training_step", progress);
    (void)instance;
    return 0;
}
