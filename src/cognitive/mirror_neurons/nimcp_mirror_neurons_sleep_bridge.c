/**
 * @file nimcp_mirror_neurons_sleep_bridge.c
 * @brief Sleep-Mirror Neurons Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-21
 */

#include "cognitive/mirror_neurons/nimcp_mirror_neurons_sleep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mirror_neurons_sleep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mirror_neurons_sleep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mirror_neurons_sleep_bridge_mesh_registry = NULL;

nimcp_error_t mirror_neurons_sleep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mirror_neurons_sleep_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mirror_neurons_sleep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mirror_neurons_sleep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mirror_neurons_sleep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mirror_neurons_sleep_bridge_mesh_registry = registry;
    return err;
}

void mirror_neurons_sleep_bridge_mesh_unregister(void) {
    if (g_mirror_neurons_sleep_bridge_mesh_registry && g_mirror_neurons_sleep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mirror_neurons_sleep_bridge_mesh_registry, g_mirror_neurons_sleep_bridge_mesh_id);
        g_mirror_neurons_sleep_bridge_mesh_id = 0;
        g_mirror_neurons_sleep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from mirror_neurons_sleep_bridge module (instance-level) */
static inline void mirror_neurons_sleep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_neurons_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_neurons_sleep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_neurons_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/**
 * WHAT: Internal structure for mirror neurons sleep bridge
 * WHY:  Encapsulate bridge state and configuration
 * HOW:  Holds config, effects, sleep system reference, and synchronization
 */
struct mirror_neurons_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent; /**< Instance-level health agent */

    mirror_neurons_sleep_config_t config;      /**< Bridge configuration */
    sleep_system_t sleep_system;               /**< Reference to sleep system */
    mirror_neurons_sleep_effects_t effects;    /**< Current sleep effects */

    bool callback_registered;                  /**< Whether callback is active */
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(mirror_neurons_sleep_bridge, struct mirror_neurons_sleep_bridge_struct)

/* Forward declarations */
static void mirror_neurons_on_sleep_state_change(sleep_state_t new_state, void* user_data);
static void update_effects_for_state(mirror_neurons_sleep_bridge_t bridge, sleep_state_t state);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update mirror neuron parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state transitions affect mirror neuron activity immediately
 * - Premotor cortex shifts from active mirroring (awake) to replay (REM)
 * - Motor suppression prevents acting out observed actions during sleep
 */
static void mirror_neurons_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    mirror_neurons_sleep_bridge_t bridge = (mirror_neurons_sleep_bridge_t)user_data;

    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Mirror neurons bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);
    update_effects_for_state(bridge, new_state);
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Mirror modulated: activity=%.2f, empathy=%.2f, replay=%.2f, replay_active=%d",
                        bridge->effects.mirroring_activity_factor,
                        bridge->effects.empathy_modulation_factor,
                        bridge->effects.action_replay_factor,
                        bridge->effects.replay_active);
}

/**
 * WHAT: Update all effect factors for given sleep state
 * WHY:  Centralize effect computation logic
 * HOW:  Computes each factor based on state and configuration
 *
 * BIOLOGICAL BASIS:
 * - Mirroring activity suppressed during sleep, peaks during awake
 * - Empathy modulation follows arousal state
 * - Action observation requires wakefulness
 * - Replay peaks during REM sleep for motor consolidation
 * - Motor suppression prevents overt movement during sleep
 */
static void update_effects_for_state(mirror_neurons_sleep_bridge_t bridge, sleep_state_t state)
{
    bridge->effects.current_state = state;

    /* Update mirroring activity */
    if (bridge->config.enable_activity_modulation) {
        float activity_base = mirror_neurons_sleep_activity_for_state(state);
        bridge->effects.mirroring_activity_factor =
            activity_base * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength);
    } else {
        bridge->effects.mirroring_activity_factor = 1.0f;
    }

    /* Update empathy modulation */
    if (bridge->config.enable_empathy_modulation) {
        float empathy_base = mirror_neurons_sleep_empathy_for_state(state);
        bridge->effects.empathy_modulation_factor =
            empathy_base * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength);
    } else {
        bridge->effects.empathy_modulation_factor = 1.0f;
    }

    /* Update action observation sensitivity */
    if (bridge->config.enable_observation_modulation) {
        float observation_base = mirror_neurons_sleep_observation_for_state(state);
        bridge->effects.action_observation_factor =
            observation_base * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength);
    } else {
        bridge->effects.action_observation_factor = 1.0f;
    }

    /* Update action replay (peaks during REM) */
    if (bridge->config.enable_replay_modulation) {
        bridge->effects.action_replay_factor = mirror_neurons_sleep_replay_for_state(state);
    } else {
        bridge->effects.action_replay_factor = 0.1f;
    }

    /* Motor suppression during sleep states */
    bridge->effects.motor_suppression_factor = (state == SLEEP_STATE_AWAKE) ? 0.0f :
                                                (state == SLEEP_STATE_DROWSY) ? 0.3f :
                                                (state == SLEEP_STATE_LIGHT_NREM) ? 0.7f :
                                                (state == SLEEP_STATE_DEEP_NREM) ? 0.9f :
                                                (state == SLEEP_STATE_REM) ? 0.95f : 0.0f;

    /* Replay active during sleep states with high replay factor */
    bridge->effects.replay_active = (state == SLEEP_STATE_REM ||
                                     state == SLEEP_STATE_LIGHT_NREM ||
                                     state == SLEEP_STATE_DEEP_NREM);
}

int mirror_neurons_sleep_default_config(mirror_neurons_sleep_config_t* config)
{
    /* Guard clause: Validate config */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_sleep_default_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat("mirror_neuro_mirror_neurons_sleep", 0.0f);


    config->enable_activity_modulation = true;
    config->enable_empathy_modulation = true;
    config->enable_observation_modulation = true;
    config->enable_replay_modulation = true;
    config->modulation_strength = 1.0f;

    return 0;
}

mirror_neurons_sleep_bridge_t mirror_neurons_sleep_bridge_create(
    const mirror_neurons_sleep_config_t* config,
    sleep_system_t sleep)
{
    /* Guard clause: Validate sleep system */
    if (!sleep) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep is NULL");

        return NULL;

    }

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat("mirror_neuro_create", 0.0f);

    NIMCP_LOGGING_DEBUG("Creating mirror_neurons_sleep_bridge");

    struct mirror_neurons_sleep_bridge_struct* bridge =
        (struct mirror_neurons_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct mirror_neurons_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_LOGGING_WARN("Failed to allocate mirror_neurons_sleep_bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    memset(bridge, 0, sizeof(struct mirror_neurons_sleep_bridge_struct));

    /* Initialize configuration */
    if (config) {
        bridge->config = *config;
    } else {
        mirror_neurons_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep;

    /* Initialize bio-async fields */
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    /* Initialize effects to awake defaults */
    bridge->effects.mirroring_activity_factor = 1.0f;
    bridge->effects.empathy_modulation_factor = 1.0f;
    bridge->effects.action_observation_factor = 1.0f;
    bridge->effects.action_replay_factor = 0.1f;
    bridge->effects.motor_suppression_factor = 0.0f;
    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.sleep_pressure = 0.0f;
    bridge->effects.replay_active = false;

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "mirror_neurons_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_neurons_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep,
        mirror_neurons_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for mirror neurons bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    mirror_neurons_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Mirror neurons-sleep bridge created");
    return bridge;
}

void mirror_neurons_sleep_bridge_destroy(mirror_neurons_sleep_bridge_t bridge)
{
    /* Guard clause: Handle NULL */
    if (!bridge) return;

    /* Disconnect from bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_destroy", 0.0f);

    NIMCP_LOGGING_DEBUG("Destroying mirror_neurons_sleep_bridge");


    if (bridge->base.bio_async_enabled) {
        mirror_neurons_sleep_disconnect_bio_async(bridge);
    }

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            mirror_neurons_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for mirror neurons bridge");
        }
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge structure */
    nimcp_free(bridge);
    bridge = NULL;
}

int mirror_neurons_sleep_update(mirror_neurons_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_sleep_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_mirror_neurons_sleep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Query current sleep state and pressure */
    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.sleep_pressure = pressure;

    /* Update all effects for current state */
    update_effects_for_state(bridge, state);

    /* Sleep pressure reduces mirroring even when awake */
    if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
        float pressure_penalty = (pressure - 0.7f) * 0.5f;
        bridge->effects.mirroring_activity_factor *= (1.0f - pressure_penalty);
        bridge->effects.empathy_modulation_factor *= (1.0f - pressure_penalty);
        bridge->effects.action_observation_factor *= (1.0f - pressure_penalty);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int mirror_neurons_sleep_get_effects(
    const mirror_neurons_sleep_bridge_t bridge,
    mirror_neurons_sleep_effects_t* effects)
{
    /* Guard clause: Validate inputs */
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_sleep_get_effects: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_mirror_neurons_sleep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float mirror_neurons_sleep_get_activity(const mirror_neurons_sleep_bridge_t bridge)
{
    /* Guard clause: Handle NULL */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_sleep_get_activity: bridge is NULL");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_mirror_neurons_sleep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.mirroring_activity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

bool mirror_neurons_sleep_is_replay_active(const mirror_neurons_sleep_bridge_t bridge)
{
    /* Guard clause: Handle NULL */
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_mirror_neurons_sleep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.replay_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

float mirror_neurons_sleep_activity_for_state(sleep_state_t state)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat("mirror_neuro_mirror_neurons_sleep", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return MIRROR_SLEEP_ACTIVITY_AWAKE;
        case SLEEP_STATE_DROWSY:     return MIRROR_SLEEP_ACTIVITY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return MIRROR_SLEEP_ACTIVITY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return MIRROR_SLEEP_ACTIVITY_DEEP_NREM;
        case SLEEP_STATE_REM:        return MIRROR_SLEEP_ACTIVITY_REM;
        default:                     return MIRROR_SLEEP_ACTIVITY_AWAKE;
    }
}

float mirror_neurons_sleep_empathy_for_state(sleep_state_t state)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat("mirror_neuro_mirror_neurons_sleep", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return MIRROR_SLEEP_EMPATHY_AWAKE;
        case SLEEP_STATE_DROWSY:     return MIRROR_SLEEP_EMPATHY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return MIRROR_SLEEP_EMPATHY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return MIRROR_SLEEP_EMPATHY_DEEP_NREM;
        case SLEEP_STATE_REM:        return MIRROR_SLEEP_EMPATHY_REM;
        default:                     return MIRROR_SLEEP_EMPATHY_AWAKE;
    }
}

float mirror_neurons_sleep_observation_for_state(sleep_state_t state)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat("mirror_neuro_mirror_neurons_sleep", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return MIRROR_SLEEP_OBSERVATION_AWAKE;
        case SLEEP_STATE_DROWSY:     return MIRROR_SLEEP_OBSERVATION_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return MIRROR_SLEEP_OBSERVATION_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return MIRROR_SLEEP_OBSERVATION_DEEP_NREM;
        case SLEEP_STATE_REM:        return MIRROR_SLEEP_OBSERVATION_REM;
        default:                     return MIRROR_SLEEP_OBSERVATION_AWAKE;
    }
}

float mirror_neurons_sleep_replay_for_state(sleep_state_t state)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat("mirror_neuro_mirror_neurons_sleep", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return MIRROR_SLEEP_REPLAY_AWAKE;
        case SLEEP_STATE_DROWSY:     return MIRROR_SLEEP_REPLAY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return MIRROR_SLEEP_REPLAY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return MIRROR_SLEEP_REPLAY_DEEP_NREM;
        case SLEEP_STATE_REM:        return MIRROR_SLEEP_REPLAY_REM;
        default:                     return MIRROR_SLEEP_REPLAY_AWAKE;
    }
}

/**
 * WHAT: Connect bridge to bio-async router
 * WHY:  Enable inter-module messaging for distributed mirror neuron signals
 * HOW:  Register with bio_router using BIO_MODULE_MIRROR_NEURONS_SLEEP
 *
 * BIOLOGICAL BASIS:
 * - Mirror neuron activity can be broadcast to other social cognition modules
 * - Action observation signals distributed to motor and sensory areas
 * - Empathy signals propagated to emotion and theory-of-mind systems
 */
int mirror_neurons_sleep_connect_bio_async(mirror_neurons_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in connect_bio_async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_sleep_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard clause: Already connected */
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_mirror_neurons_sleep", 0.0f);


    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_DEBUG("Bio-async already connected for mirror neurons sleep bridge");
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_MIRROR_NEURONS_SLEEP,
        .module_name = "mirror_neurons_sleep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected mirror neurons sleep bridge to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 */
int mirror_neurons_sleep_disconnect_bio_async(mirror_neurons_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in disconnect_bio_async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_sleep_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard clause: Not connected */
    if (!bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_DEBUG("Bio-async not connected for mirror neurons sleep bridge");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_mirror_neurons_sleep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Unregister from bio-async router */
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected mirror neurons sleep bridge from bio-async router");

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query bio-async connection status
 * HOW:  Return bio_async_enabled flag
 */
bool mirror_neurons_sleep_is_bio_async_connected(const mirror_neurons_sleep_bridge_t bridge)
{
    /* Guard clause: Handle NULL */
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_mirror_neurons_sleep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->base.bio_async_enabled;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

int mirror_neurons_sleep_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_sleep_bridge_heartbeat("mirror_neuro_mirror_neurons_sleep", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mirror_Neurons_Sleep_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mirror_neurons_sleep_bridge_heartbeat("mirror_neuro_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Mirror neurons sleep bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mirror_Neurons_Sleep_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mirror_Neurons_Sleep_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Instance Health Agent Setter
 * ============================================================================ */

void mirror_neurons_sleep_bridge_set_instance_health_agent(
    mirror_neurons_sleep_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Training Hook Stubs (Phase 8: Training-Immune Integration)
 * ============================================================================ */

int mirror_neurons_sleep_bridge_pre_training_hook(mirror_neurons_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_sleep_bridge_pre_training_hook: bridge is NULL");
        return -1;
    }
    mirror_neurons_sleep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_pre_train", 0.0f);
    return 0;
}

int mirror_neurons_sleep_bridge_post_training_hook(mirror_neurons_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_neurons_sleep_bridge_post_training_hook: bridge is NULL");
        return -1;
    }
    mirror_neurons_sleep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neuro_post_train", 1.0f);
    return 0;
}

int mirror_neurons_sleep_bridge_training_begin(mirror_neurons_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_neurons_sleep_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_neurons_sleep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neurons_sleep_bridge_training_begin", 0.0f);
    return 0;
}

int mirror_neurons_sleep_bridge_training_end(mirror_neurons_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_neurons_sleep_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_neurons_sleep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neurons_sleep_bridge_training_end", 1.0f);
    return 0;
}

int mirror_neurons_sleep_bridge_training_step(mirror_neurons_sleep_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_neurons_sleep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mirror_neurons_sleep_bridge_heartbeat_instance(bridge->health_agent, "mirror_neurons_sleep_bridge_training_step", progress);
    return 0;
}
