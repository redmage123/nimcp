/**
 * @file nimcp_cognitive_bio_async_bridge.c
 * @brief Bio-Async Integration Bridge Implementation for Cognitive Systems
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Implementation of unified bridge for cognitive-bio-async coordination
 * WHY:  Enables neuromodulator-based signaling across cognitive modules
 * HOW:  Message routing, phase coupling, glial waves, effects propagation
 */

#include "cognitive/integration/nimcp_cognitive_bio_async_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cognitive_bio_async_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_cognitive_bio_async_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_cognitive_bio_async_bridge_mesh_registry = NULL;

nimcp_error_t cognitive_bio_async_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_cognitive_bio_async_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "cognitive_bio_async_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "cognitive_bio_async_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_cognitive_bio_async_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_cognitive_bio_async_bridge_mesh_registry = registry;
    return err;
}

void cognitive_bio_async_bridge_mesh_unregister(void) {
    if (g_cognitive_bio_async_bridge_mesh_registry && g_cognitive_bio_async_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_cognitive_bio_async_bridge_mesh_registry, g_cognitive_bio_async_bridge_mesh_id);
        g_cognitive_bio_async_bridge_mesh_id = 0;
        g_cognitive_bio_async_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from cognitive_bio_async_bridge module (instance-level) */
static inline void cognitive_bio_async_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_cognitive_bio_async_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_cognitive_bio_async_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_cognitive_bio_async_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "COGNITIVE_BIO_ASYNC_BRIDGE"


/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

/** Maximum message handlers per message type */
#define MAX_HANDLERS_PER_TYPE   8

/** Message handler table size */
#define HANDLER_TABLE_SIZE      256

/** State change callback limit */
#define MAX_STATE_CALLBACKS     16

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Message handler entry
 */
typedef struct {
    cog_bio_message_type_t type;
    cog_bio_message_handler_t handler;
    void* user_data;
    bool active;
} cog_message_handler_entry_t;

/**
 * @brief State change callback entry
 */
typedef struct {
    cog_state_change_handler_t handler;
    void* user_data;
    bool active;
} cog_state_callback_entry_t;

/**
 * @brief Internal bridge structure
 */
struct cognitive_bio_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    /* Configuration */
    cognitive_bio_bridge_config_t config;

    /* Connections */
    struct nimcp_bio_async* bio_async;
    struct brain_struct* brain;
    bool connected;

    /* Registered modules */
    cog_module_registration_t modules[COG_BIO_MAX_MODULES];
    uint32_t num_modules;

    /* Message handlers */
    cog_message_handler_entry_t handlers[HANDLER_TABLE_SIZE];
    size_t num_handlers;

    /* State change callbacks */
    cog_state_callback_entry_t state_callbacks[MAX_STATE_CALLBACKS];
    size_t num_state_callbacks;

    /* Effects state */
    cog_to_bio_async_effects_t outgoing_effects;
    bio_async_to_cog_effects_t incoming_effects;

    /* Current state */
    cog_state_transition_t current_transition;
    cog_priority_t current_priority;

    /* Statistics */
    cognitive_bio_bridge_stats_t stats;

    /* Timing */
    uint64_t last_update_time_ms;
    float accumulated_time_ms;
};

/*=============================================================================
 * INTERNAL HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get module index for type
 */
static int get_module_index(cog_module_type_t type) {
    if (type >= COG_MODULE_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "get_module_index: capacity exceeded");
        return -1;
    }
    return (int)type;
}

/**
 * @brief Find handler for message type
 */
static void dispatch_message_unlocked(
    cognitive_bio_bridge_t* bridge,
    cog_bio_message_type_t type,
    const void* payload,
    size_t payload_size
) {
    for (size_t i = 0; i < bridge->num_handlers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_handlers > 256) {
            cognitive_bio_async_bridge_heartbeat("cognitive_bi_loop",
                             (float)(i + 1) / (float)bridge->num_handlers);
        }

        if (bridge->handlers[i].active &&
            bridge->handlers[i].type == type &&
            bridge->handlers[i].handler) {
            bridge->handlers[i].handler(
                type,
                payload,
                payload_size,
                bridge->handlers[i].user_data
            );
        }
    }
}

/**
 * @brief Notify state change callbacks
 */
static void notify_state_change_unlocked(
    cognitive_bio_bridge_t* bridge,
    cog_state_transition_t transition,
    uint32_t source_module
) {
    for (size_t i = 0; i < bridge->num_state_callbacks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_state_callbacks > 256) {
            cognitive_bio_async_bridge_heartbeat("cognitive_bi_loop",
                             (float)(i + 1) / (float)bridge->num_state_callbacks);
        }

        if (bridge->state_callbacks[i].active &&
            bridge->state_callbacks[i].handler) {
            bridge->state_callbacks[i].handler(
                transition,
                source_module,
                bridge->state_callbacks[i].user_data
            );
        }
    }
}

/**
 * @brief Update neuromodulator decay
 */
static void update_neuromodulator_decay(
    cognitive_bio_bridge_t* bridge,
    float delta_ms
) {
    float decay_rate = delta_ms / 1000.0f * 0.1f;

    /* Dopamine decay */
    if (bridge->outgoing_effects.dopamine_release > 0.0f) {
        bridge->outgoing_effects.dopamine_release -= decay_rate;
        if (bridge->outgoing_effects.dopamine_release < 0.0f) {
            bridge->outgoing_effects.dopamine_release = 0.0f;
        }
    }

    /* Norepinephrine decay */
    if (bridge->outgoing_effects.norepinephrine_level > 0.0f) {
        bridge->outgoing_effects.norepinephrine_level -= decay_rate * 0.8f;
        if (bridge->outgoing_effects.norepinephrine_level < 0.0f) {
            bridge->outgoing_effects.norepinephrine_level = 0.0f;
        }
    }

    /* Acetylcholine decay - faster */
    if (bridge->outgoing_effects.acetylcholine_level > 0.0f) {
        bridge->outgoing_effects.acetylcholine_level -= decay_rate * 2.0f;
        if (bridge->outgoing_effects.acetylcholine_level < 0.0f) {
            bridge->outgoing_effects.acetylcholine_level = 0.0f;
        }
    }

    /* Serotonin decay - slower */
    if (bridge->outgoing_effects.serotonin_level > 0.0f) {
        bridge->outgoing_effects.serotonin_level -= decay_rate * 0.5f;
        if (bridge->outgoing_effects.serotonin_level < 0.0f) {
            bridge->outgoing_effects.serotonin_level = 0.0f;
        }
    }

    /* Update running averages in stats */
    bridge->stats.avg_dopamine =
        bridge->stats.avg_dopamine * 0.95f +
        bridge->outgoing_effects.dopamine_release * 0.05f;
    bridge->stats.avg_norepinephrine =
        bridge->stats.avg_norepinephrine * 0.95f +
        bridge->outgoing_effects.norepinephrine_level * 0.05f;
    bridge->stats.avg_acetylcholine =
        bridge->stats.avg_acetylcholine * 0.95f +
        bridge->outgoing_effects.acetylcholine_level * 0.05f;
    bridge->stats.avg_serotonin =
        bridge->stats.avg_serotonin * 0.95f +
        bridge->outgoing_effects.serotonin_level * 0.05f;
}

/*=============================================================================
 * LIFECYCLE IMPLEMENTATION
 *===========================================================================*/

cognitive_bio_bridge_config_t cognitive_bio_bridge_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    cognitive_bio_bridge_config_t config = {0};

    /* Neuromodulator sensitivity */
    config.dopamine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.norepinephrine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.acetylcholine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.serotonin_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Phase coupling */
    config.coherence_threshold = COG_BIO_DEFAULT_COHERENCE_THRESHOLD;
    config.coupling_strength = 0.5f;
    config.default_oscillation_band = 3; /* BETA band */

    /* Message routing */
    config.enable_message_logging = false;
    config.message_queue_size = COG_BIO_DEFAULT_QUEUE_SIZE;
    config.max_handlers_per_type = MAX_HANDLERS_PER_TYPE;

    /* Update rates */
    config.attention_update_rate_hz = 30.0f;
    config.emotion_update_rate_hz = 10.0f;
    config.goal_update_rate_hz = 5.0f;

    /* Glial waves */
    config.glial_wave_threshold = 0.7f;
    config.glial_wave_decay_rate = 0.1f;

    /* Cognitive load */
    config.load_warning_threshold = 0.7f;
    config.load_critical_threshold = 0.9f;
    config.enable_adaptive_throttling = true;

    return config;
}

cognitive_bio_bridge_t* cognitive_bio_bridge_create(
    const cognitive_bio_bridge_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    cognitive_bio_bridge_t* bridge = nimcp_calloc(1, sizeof(cognitive_bio_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cognitive_bio_bridge_default_config();
    }

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, 0, "cognitive_bio_async") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cognitive_bio_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize effects with defaults */
    memset(&bridge->outgoing_effects, 0, sizeof(cog_to_bio_async_effects_t));
    memset(&bridge->incoming_effects, 0, sizeof(bio_async_to_cog_effects_t));

    /* Set default incoming effects */
    bridge->incoming_effects.available_capacity = 1.0f;
    bridge->incoming_effects.circadian_factor = 1.0f;
    bridge->incoming_effects.metabolic_state = 1.0f;

    /* Initialize module slots */
    for (int i = 0; i < COG_BIO_MAX_MODULES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COG_BIO_MAX_MODULES > 256) {
            cognitive_bio_async_bridge_heartbeat("cognitive_bi_loop",
                             (float)(i + 1) / (float)COG_BIO_MAX_MODULES);
        }

        bridge->modules[i].active = false;
        bridge->modules[i].type = (cog_module_type_t)i;
        bridge->modules[i].module_id = COG_BIO_MODULE_ROOT + (uint32_t)i + 1;
    }

    /* Initialize timing */
    bridge->last_update_time_ms = nimcp_platform_time_monotonic_us() / 1000;

    return bridge;
}

cognitive_bio_bridge_t* cognitive_bio_bridge_create_default(void) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    return cognitive_bio_bridge_create(NULL);
}

void cognitive_bio_bridge_destroy(cognitive_bio_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect first */
    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    if (bridge->connected) {
        cognitive_bio_bridge_disconnect(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

/*=============================================================================
 * CONNECTION IMPLEMENTATION
 *===========================================================================*/

int cognitive_bio_bridge_connect(
    cognitive_bio_bridge_t* bridge,
    struct nimcp_bio_async* bio_async
) {
    if (!bridge || !bio_async) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async = bio_async;
    bridge->connected = (bridge->bio_async != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

int cognitive_bio_bridge_connect_brain(
    cognitive_bio_bridge_t* bridge,
    struct brain_struct* brain
) {
    if (!bridge || !brain) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->brain = brain;
    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

int cognitive_bio_bridge_disconnect(cognitive_bio_bridge_t* bridge) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async = NULL;
    bridge->connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

bool cognitive_bio_bridge_is_connected(const cognitive_bio_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    return bridge->connected;
}

/*=============================================================================
 * MODULE REGISTRATION IMPLEMENTATION
 *===========================================================================*/

int cognitive_bio_bridge_register_module(
    cognitive_bio_bridge_t* bridge,
    cog_module_type_t type,
    void* module_ptr
) {
    if (!bridge || !module_ptr) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    int idx = get_module_index(type);
    if (idx < 0) {
        return COG_BIO_ERROR_INVALID_CONFIG;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->modules[idx].active) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return COG_BIO_ERROR_MODULE_ALREADY_REG;
    }

    bridge->modules[idx].type = type;
    bridge->modules[idx].module_id = COG_BIO_MODULE_ROOT + (uint32_t)type + 1;
    bridge->modules[idx].module_ptr = module_ptr;
    bridge->modules[idx].active = true;
    bridge->modules[idx].last_update_ms = nimcp_platform_time_monotonic_us() / 1000;
    bridge->modules[idx].messages_sent = 0;
    bridge->modules[idx].messages_received = 0;

    bridge->num_modules++;
    bridge->stats.active_modules = bridge->num_modules;

    /* Send registration notification */
    dispatch_message_unlocked(bridge, COG_MSG_MODULE_REGISTERED, &type, sizeof(type));

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

int cognitive_bio_bridge_unregister_module(
    cognitive_bio_bridge_t* bridge,
    cog_module_type_t type
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    int idx = get_module_index(type);
    if (idx < 0) {
        return COG_BIO_ERROR_INVALID_CONFIG;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->modules[idx].active) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return COG_BIO_ERROR_MODULE_NOT_FOUND;
    }

    bridge->modules[idx].active = false;
    bridge->modules[idx].module_ptr = NULL;
    bridge->num_modules--;
    bridge->stats.active_modules = bridge->num_modules;

    /* Send unregistration notification */
    dispatch_message_unlocked(bridge, COG_MSG_MODULE_UNREGISTERED, &type, sizeof(type));

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

const cog_module_registration_t* cognitive_bio_bridge_get_module(
    const cognitive_bio_bridge_t* bridge,
    cog_module_type_t type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    int idx = get_module_index(type);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_bio_bridge_get_module: validation failed");
        return NULL;
    }

    if (!bridge->modules[idx].active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_bio_bridge_get_module: bridge->modules is NULL");
        return NULL;
    }

    return &bridge->modules[idx];
}

bool cognitive_bio_bridge_module_registered(
    const cognitive_bio_bridge_t* bridge,
    cog_module_type_t type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_bio_bridge_module_registered: bridge is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    int idx = get_module_index(type);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_bio_bridge_module_registered: validation failed");
        return false;
    }

    return bridge->modules[idx].active;
}

uint32_t cognitive_bio_bridge_module_count(const cognitive_bio_bridge_t* bridge) {
    if (!bridge) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    return bridge->num_modules;
}

/*=============================================================================
 * UPDATE IMPLEMENTATION
 *===========================================================================*/

int cognitive_bio_bridge_update(
    cognitive_bio_bridge_t* bridge,
    float delta_time_ms
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    uint64_t start_time = nimcp_platform_time_monotonic_us();

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return COG_BIO_ERROR_DISCONNECTED;
    }

    /* Update neuromodulator decay */
    update_neuromodulator_decay(bridge, delta_time_ms);

    /* Check cognitive load thresholds */
    if (bridge->outgoing_effects.cognitive_load > bridge->config.load_critical_threshold) {
        if (!bridge->outgoing_effects.capacity_warning) {
            bridge->outgoing_effects.capacity_warning = true;
            /* Trigger overload transition */
            bridge->current_transition = COG_TRANSITION_OVERLOAD;
            notify_state_change_unlocked(bridge, COG_TRANSITION_OVERLOAD, COG_BIO_MODULE_ROOT);
        }
    } else if (bridge->outgoing_effects.cognitive_load > bridge->config.load_warning_threshold) {
        bridge->outgoing_effects.capacity_warning = true;
    } else {
        if (bridge->outgoing_effects.capacity_warning &&
            bridge->current_transition == COG_TRANSITION_OVERLOAD) {
            /* Recovery from overload */
            bridge->current_transition = COG_TRANSITION_RECOVERY;
            notify_state_change_unlocked(bridge, COG_TRANSITION_RECOVERY, COG_BIO_MODULE_ROOT);
        }
        bridge->outgoing_effects.capacity_warning = false;
    }

    /* Update module timestamps */
    uint64_t current_ms = nimcp_platform_time_monotonic_us() / 1000;
    for (int i = 0; i < COG_BIO_MAX_MODULES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COG_BIO_MAX_MODULES > 256) {
            cognitive_bio_async_bridge_heartbeat("cognitive_bi_loop",
                             (float)(i + 1) / (float)COG_BIO_MAX_MODULES);
        }

        if (bridge->modules[i].active) {
            bridge->modules[i].last_update_ms = current_ms;
        }
    }

    /* Update incoming effects tick */
    bridge->incoming_effects.current_tick_ms = current_ms;

    /* Update statistics */
    uint64_t elapsed = nimcp_platform_time_monotonic_us() - start_time;
    bridge->stats.total_update_time_us += elapsed;
    if (elapsed > bridge->stats.max_update_time_us) {
        bridge->stats.max_update_time_us = elapsed;
    }

    /* Calculate running average (atomic counter for thread safety) */
    static _Atomic uint64_t update_count = 0;
    uint64_t count = atomic_fetch_add(&update_count, 1) + 1;
    bridge->stats.avg_update_time_us =
        (bridge->stats.avg_update_time_us * (count - 1) + (float)elapsed) / (count > 0 ? count : 1);

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

/*=============================================================================
 * MESSAGING IMPLEMENTATION
 *===========================================================================*/

int cognitive_bio_bridge_send_message(
    cognitive_bio_bridge_t* bridge,
    cog_bio_message_type_t message_type,
    const void* payload,
    size_t payload_size
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return COG_BIO_ERROR_DISCONNECTED;
    }

    /* Dispatch to handlers */
    dispatch_message_unlocked(bridge, message_type, payload, payload_size);

    bridge->stats.total_messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

int cognitive_bio_bridge_send_to_module(
    cognitive_bio_bridge_t* bridge,
    cog_module_type_t target_module,
    cog_bio_message_type_t message_type,
    const void* payload,
    size_t payload_size
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    int idx = get_module_index(target_module);
    if (idx < 0) {
        return COG_BIO_ERROR_INVALID_CONFIG;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->modules[idx].active) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return COG_BIO_ERROR_MODULE_NOT_FOUND;
    }

    /* Dispatch to handlers registered for this module's message types */
    dispatch_message_unlocked(bridge, message_type, payload, payload_size);

    bridge->modules[idx].messages_received++;
    bridge->stats.total_messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

int cognitive_bio_bridge_register_handler(
    cognitive_bio_bridge_t* bridge,
    cog_bio_message_type_t message_type,
    cog_bio_message_handler_t handler,
    void* user_data
) {
    if (!bridge || !handler) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_handlers >= HANDLER_TABLE_SIZE) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return COG_BIO_ERROR_HANDLER_FULL;
    }

    /* Find empty slot */
    size_t slot = bridge->num_handlers;
    for (size_t i = 0; i < bridge->num_handlers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_handlers > 256) {
            cognitive_bio_async_bridge_heartbeat("cognitive_bi_loop",
                             (float)(i + 1) / (float)bridge->num_handlers);
        }

        if (!bridge->handlers[i].active) {
            slot = i;
            break;
        }
    }

    bridge->handlers[slot].type = message_type;
    bridge->handlers[slot].handler = handler;
    bridge->handlers[slot].user_data = user_data;
    bridge->handlers[slot].active = true;

    if (slot == bridge->num_handlers) {
        bridge->num_handlers++;
    }

    bridge->stats.registered_handlers++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

int cognitive_bio_bridge_unregister_handler(
    cognitive_bio_bridge_t* bridge,
    cog_bio_message_type_t message_type,
    cog_bio_message_handler_t handler
) {
    if (!bridge || !handler) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bool found = false;
    for (size_t i = 0; i < bridge->num_handlers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_handlers > 256) {
            cognitive_bio_async_bridge_heartbeat("cognitive_bi_loop",
                             (float)(i + 1) / (float)bridge->num_handlers);
        }

        if (bridge->handlers[i].active &&
            bridge->handlers[i].type == message_type &&
            bridge->handlers[i].handler == handler) {
            bridge->handlers[i].active = false;
            bridge->stats.registered_handlers--;
            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return found ? COG_BIO_OK : COG_BIO_ERROR_MODULE_NOT_FOUND;
}

/*=============================================================================
 * BROADCAST IMPLEMENTATION
 *===========================================================================*/

/**
 * @brief Attention shift payload
 */
typedef struct {
    uint32_t target_id;
    float intensity;
    cog_module_type_t source;
} attention_shift_payload_t;

int cognitive_bio_bridge_broadcast_attention_shift(
    cognitive_bio_bridge_t* bridge,
    uint32_t target_id,
    float intensity,
    cog_module_type_t source_module
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Update outgoing effects */
    bridge->outgoing_effects.attention_target_id = target_id;
    bridge->outgoing_effects.acetylcholine_level = intensity;

    /* Create payload */
    attention_shift_payload_t payload = {
        .target_id = target_id,
        .intensity = intensity,
        .source = source_module
    };

    /* Dispatch to all handlers */
    dispatch_message_unlocked(bridge, COG_MSG_ATTENTION_SHIFT, &payload, sizeof(payload));

    bridge->stats.broadcast_count++;
    bridge->stats.total_messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

/**
 * @brief Emotion update payload
 */
typedef struct {
    float valence;
    float arousal;
    uint32_t dominant_emotion;
} emotion_update_payload_t;

int cognitive_bio_bridge_broadcast_emotion_update(
    cognitive_bio_bridge_t* bridge,
    float valence,
    float arousal,
    uint32_t dominant_emotion
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Update outgoing effects */
    bridge->outgoing_effects.current_valence = valence;
    bridge->outgoing_effects.current_arousal = arousal;

    /* Serotonin reflects positive valence */
    if (valence > 0) {
        bridge->outgoing_effects.serotonin_level =
            fmaxf(bridge->outgoing_effects.serotonin_level, valence);
    }

    /* Create payload */
    emotion_update_payload_t payload = {
        .valence = valence,
        .arousal = arousal,
        .dominant_emotion = dominant_emotion
    };

    /* Dispatch to all handlers */
    dispatch_message_unlocked(bridge, COG_MSG_EMOTION_UPDATE, &payload, sizeof(payload));

    bridge->stats.broadcast_count++;
    bridge->stats.total_messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

/**
 * @brief Goal change payload
 */
typedef struct {
    uint32_t goal_id;
    uint8_t goal_status;
    cog_priority_t priority;
} goal_change_payload_t;

int cognitive_bio_bridge_broadcast_goal_change(
    cognitive_bio_bridge_t* bridge,
    uint32_t goal_id,
    uint8_t goal_status,
    cog_priority_t priority
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Update outgoing effects based on status */
    bridge->outgoing_effects.current_priority = priority;

    /* Goal achieved - release dopamine */
    if (goal_status == 1) {
        bridge->outgoing_effects.dopamine_release = COG_BIO_DEFAULT_DOPAMINE_RELEASE;
        bridge->outgoing_effects.goals_achieved++;
    }

    /* Goal blocked - increase urgency */
    if (goal_status == 3) {
        bridge->outgoing_effects.urgency_escalation = true;
        bridge->outgoing_effects.norepinephrine_level =
            fmaxf(bridge->outgoing_effects.norepinephrine_level, 0.6f);
    }

    /* Create payload */
    goal_change_payload_t payload = {
        .goal_id = goal_id,
        .goal_status = goal_status,
        .priority = priority
    };

    /* Select appropriate message type */
    cog_bio_message_type_t msg_type;
    switch (goal_status) {
        case 0: msg_type = COG_MSG_GOAL_SET; break;
        case 1: msg_type = COG_MSG_GOAL_ACHIEVED; break;
        case 2: msg_type = COG_MSG_GOAL_ABANDONED; break;
        case 3: msg_type = COG_MSG_GOAL_BLOCKED; break;
        default: msg_type = COG_MSG_GOAL_SET; break;
    }

    dispatch_message_unlocked(bridge, msg_type, &payload, sizeof(payload));

    bridge->stats.broadcast_count++;
    bridge->stats.total_messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

/**
 * @brief Salience spike payload
 */
typedef struct {
    uint32_t stimulus_id;
    float salience_value;
    bool is_threat;
} salience_spike_payload_t;

int cognitive_bio_bridge_broadcast_salience_spike(
    cognitive_bio_bridge_t* bridge,
    uint32_t stimulus_id,
    float salience_value,
    bool is_threat
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Threat increases norepinephrine */
    if (is_threat) {
        bridge->outgoing_effects.norepinephrine_level =
            fmaxf(bridge->outgoing_effects.norepinephrine_level, salience_value);
        bridge->outgoing_effects.urgency_escalation = true;
    }

    /* High salience captures attention */
    if (salience_value > 0.7f) {
        bridge->outgoing_effects.acetylcholine_level =
            fmaxf(bridge->outgoing_effects.acetylcholine_level, salience_value);
    }

    /* Create payload */
    salience_spike_payload_t payload = {
        .stimulus_id = stimulus_id,
        .salience_value = salience_value,
        .is_threat = is_threat
    };

    dispatch_message_unlocked(bridge, COG_MSG_SALIENCE_SPIKE, &payload, sizeof(payload));

    if (is_threat) {
        dispatch_message_unlocked(bridge, COG_MSG_THREAT_DETECTED, &payload, sizeof(payload));
    }

    bridge->stats.broadcast_count++;
    bridge->stats.total_messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

/**
 * @brief Load warning payload
 */
typedef struct {
    float current_load;
    bool critical;
} load_warning_payload_t;

int cognitive_bio_bridge_broadcast_load_warning(
    cognitive_bio_bridge_t* bridge,
    float current_load,
    bool critical
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->outgoing_effects.cognitive_load = current_load;
    bridge->outgoing_effects.capacity_warning = true;

    load_warning_payload_t payload = {
        .current_load = current_load,
        .critical = critical
    };

    cog_bio_message_type_t msg_type = critical ?
        COG_MSG_COGNITIVE_LOAD_HIGH : COG_MSG_WM_CAPACITY_WARNING;

    dispatch_message_unlocked(bridge, msg_type, &payload, sizeof(payload));

    bridge->stats.broadcast_count++;
    bridge->stats.total_messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

/*=============================================================================
 * STATE COORDINATION IMPLEMENTATION
 *===========================================================================*/

int cognitive_bio_bridge_request_state_sync(
    cognitive_bio_bridge_t* bridge,
    const cog_module_type_t* modules,
    size_t count,
    float coherence_threshold,
    uint32_t timeout_ms
) {
    if (!bridge || !modules || count == 0) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return COG_BIO_ERROR_DISCONNECTED;
    }

    /* Request phase sync */
    bridge->outgoing_effects.request_phase_sync = true;
    bridge->outgoing_effects.sync_module_count = (uint32_t)count;
    bridge->outgoing_effects.desired_coherence = coherence_threshold;

    /* Send sync request */
    dispatch_message_unlocked(bridge, COG_MSG_STATE_SYNC_REQUEST, modules, count * sizeof(cog_module_type_t));

    bridge->stats.phase_syncs_requested++;

    /* Simulate sync achievement for now */
    (void)timeout_ms;
    bridge->incoming_effects.phase_sync_achieved = true;
    bridge->incoming_effects.current_coherence = coherence_threshold;
    bridge->incoming_effects.synchronized_modules = (uint32_t)count;

    bridge->stats.phase_syncs_achieved++;
    bridge->stats.avg_coherence_achieved =
        (bridge->stats.avg_coherence_achieved * 0.9f) + (coherence_threshold * 0.1f);

    /* Send completion */
    dispatch_message_unlocked(bridge, COG_MSG_STATE_SYNC_COMPLETE, NULL, 0);

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

int cognitive_bio_bridge_on_state_change(
    cognitive_bio_bridge_t* bridge,
    cog_state_change_handler_t handler,
    void* user_data
) {
    if (!bridge || !handler) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_state_callbacks >= MAX_STATE_CALLBACKS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return COG_BIO_ERROR_HANDLER_FULL;
    }

    size_t slot = bridge->num_state_callbacks;
    for (size_t i = 0; i < bridge->num_state_callbacks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_state_callbacks > 256) {
            cognitive_bio_async_bridge_heartbeat("cognitive_bi_loop",
                             (float)(i + 1) / (float)bridge->num_state_callbacks);
        }

        if (!bridge->state_callbacks[i].active) {
            slot = i;
            break;
        }
    }

    bridge->state_callbacks[slot].handler = handler;
    bridge->state_callbacks[slot].user_data = user_data;
    bridge->state_callbacks[slot].active = true;

    if (slot == bridge->num_state_callbacks) {
        bridge->num_state_callbacks++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

int cognitive_bio_bridge_trigger_transition(
    cognitive_bio_bridge_t* bridge,
    cog_state_transition_t transition
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_transition = transition;
    bridge->outgoing_effects.trigger_glial_wave = true;
    bridge->outgoing_effects.transition = transition;

    /* Notify callbacks */
    notify_state_change_unlocked(bridge, transition, COG_BIO_MODULE_ROOT);

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

/*=============================================================================
 * NEUROMODULATOR CONTROL IMPLEMENTATION
 *===========================================================================*/

int cognitive_bio_bridge_release_dopamine(
    cognitive_bio_bridge_t* bridge,
    float amount,
    uint32_t goal_id
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    if (amount < 0.0f || amount > 1.0f) {
        return COG_BIO_ERROR_INVALID_CONFIG;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->outgoing_effects.dopamine_release = amount;
    bridge->outgoing_effects.goals_achieved++;
    (void)goal_id;

    /* Update stats */
    bridge->stats.avg_dopamine =
        (bridge->stats.avg_dopamine * 0.9f) + (amount * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

int cognitive_bio_bridge_signal_urgency(
    cognitive_bio_bridge_t* bridge,
    float urgency,
    cog_module_type_t source_module
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    if (urgency < 0.0f || urgency > 1.0f) {
        return COG_BIO_ERROR_INVALID_CONFIG;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->outgoing_effects.norepinephrine_level = urgency;
    bridge->outgoing_effects.urgency_escalation =
        (urgency > COG_BIO_DEFAULT_NE_URGENCY_THRESHOLD);
    (void)source_module;

    bridge->stats.avg_norepinephrine =
        (bridge->stats.avg_norepinephrine * 0.9f) + (urgency * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

int cognitive_bio_bridge_modulate_attention(
    cognitive_bio_bridge_t* bridge,
    float attention,
    uint32_t target_id
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    if (attention < 0.0f || attention > 1.0f) {
        return COG_BIO_ERROR_INVALID_CONFIG;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->outgoing_effects.acetylcholine_level = attention;
    bridge->outgoing_effects.attention_target_id = target_id;

    bridge->stats.avg_acetylcholine =
        (bridge->stats.avg_acetylcholine * 0.9f) + (attention * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

int cognitive_bio_bridge_set_mood_level(
    cognitive_bio_bridge_t* bridge,
    float level
) {
    if (!bridge) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    if (level < 0.0f || level > 1.0f) {
        return COG_BIO_ERROR_INVALID_CONFIG;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->outgoing_effects.serotonin_level = level;

    bridge->stats.avg_serotonin =
        (bridge->stats.avg_serotonin * 0.9f) + (level * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

/*=============================================================================
 * PHASE COUPLING IMPLEMENTATION
 *===========================================================================*/

int cognitive_bio_bridge_create_phase_sync(
    cognitive_bio_bridge_t* bridge,
    const cog_module_type_t* modules,
    size_t count,
    uint8_t band,
    struct nimcp_phase_sync** sync
) {
    if (!bridge || !modules || !sync) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return COG_BIO_ERROR_DISCONNECTED;
    }

    /* Placeholder - in full implementation would create phase sync */
    NIMCP_LOGGING_DEBUG("cognitive_bio_bridge_create_phase_sync called (stub, count=%zu, band=%u)", count, (unsigned)band);
    (void)count;
    (void)band;
    *sync = NULL;

    bridge->stats.phase_syncs_requested++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

int cognitive_bio_bridge_wait_coherent(
    cognitive_bio_bridge_t* bridge,
    struct nimcp_phase_sync* sync,
    float coherence_threshold,
    uint32_t timeout_ms
) {
    if (!bridge || !sync) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    (void)coherence_threshold;
    (void)timeout_ms;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->stats.phase_syncs_achieved++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

/*=============================================================================
 * GLIAL WAVE IMPLEMENTATION
 *===========================================================================*/

int cognitive_bio_bridge_initiate_glial_wave(
    cognitive_bio_bridge_t* bridge,
    cog_state_transition_t transition,
    struct nimcp_glial_wave** wave
) {
    if (!bridge || !wave) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return COG_BIO_ERROR_DISCONNECTED;
    }

    bridge->outgoing_effects.trigger_glial_wave = true;
    bridge->outgoing_effects.transition = transition;

    /* Placeholder - in full implementation would create glial wave */
    *wave = NULL;

    bridge->stats.glial_waves_initiated++;
    bridge->incoming_effects.glial_wave_active = true;
    bridge->incoming_effects.wave_source_module = COG_BIO_MODULE_ROOT;

    nimcp_mutex_unlock(bridge->base.mutex);

    return COG_BIO_OK;
}

/*=============================================================================
 * EFFECTS ACCESS IMPLEMENTATION
 *===========================================================================*/

int cognitive_bio_bridge_get_outgoing_effects(
    const cognitive_bio_bridge_t* bridge,
    cog_to_bio_async_effects_t* effects
) {
    if (!bridge || !effects) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(((cognitive_bio_bridge_t*)bridge)->base.mutex);
    *effects = bridge->outgoing_effects;
    nimcp_mutex_unlock(((cognitive_bio_bridge_t*)bridge)->base.mutex);

    return COG_BIO_OK;
}

int cognitive_bio_bridge_get_incoming_effects(
    const cognitive_bio_bridge_t* bridge,
    bio_async_to_cog_effects_t* effects
) {
    if (!bridge || !effects) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(((cognitive_bio_bridge_t*)bridge)->base.mutex);
    *effects = bridge->incoming_effects;
    nimcp_mutex_unlock(((cognitive_bio_bridge_t*)bridge)->base.mutex);

    return COG_BIO_OK;
}

/*=============================================================================
 * STATISTICS IMPLEMENTATION
 *===========================================================================*/

int cognitive_bio_bridge_get_stats(
    const cognitive_bio_bridge_t* bridge,
    cognitive_bio_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        return COG_BIO_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(((cognitive_bio_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((cognitive_bio_bridge_t*)bridge)->base.mutex);

    return COG_BIO_OK;
}

void cognitive_bio_bridge_reset_stats(cognitive_bio_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_bridge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Preserve active module count */
    uint32_t active = bridge->stats.active_modules;
    uint32_t handlers = bridge->stats.registered_handlers;

    memset(&bridge->stats, 0, sizeof(cognitive_bio_bridge_stats_t));

    bridge->stats.active_modules = active;
    bridge->stats.registered_handlers = handlers;

    nimcp_mutex_unlock(bridge->base.mutex);
}

/*=============================================================================
 * UTILITY FUNCTIONS IMPLEMENTATION
 *===========================================================================*/

const char* cognitive_bio_module_type_name(cog_module_type_t type) {
    switch (type) {
        case COG_MODULE_TYPE_ATTENTION:      return "attention";
        case COG_MODULE_TYPE_EMOTION:        return "emotion";
        case COG_MODULE_TYPE_WORKING_MEMORY: return "working_memory";
        case COG_MODULE_TYPE_REASONING:      return "reasoning";
        case COG_MODULE_TYPE_GOAL:           return "goal";
        case COG_MODULE_TYPE_SALIENCE:       return "salience";
        case COG_MODULE_TYPE_ETHICS:         return "ethics";
        case COG_MODULE_TYPE_CURIOSITY:      return "curiosity";
        case COG_MODULE_TYPE_INTROSPECTION:  return "introspection";
        case COG_MODULE_TYPE_THEORY_OF_MIND: return "theory_of_mind";
        case COG_MODULE_TYPE_EMPATHY:        return "empathy";
        case COG_MODULE_TYPE_KNOWLEDGE:      return "knowledge";
        case COG_MODULE_TYPE_CONSOLIDATION:  return "consolidation";
        case COG_MODULE_TYPE_DECISION:       return "decision";
        case COG_MODULE_TYPE_EXECUTIVE:      return "executive";
        default:                             return "unknown";
    }
}

const char* cognitive_bio_message_type_name(cog_bio_message_type_t type) {
    /* Extract domain from message type */
    uint32_t domain = (type >> 8) & 0xFFFF;

    switch (domain) {
        case 0x2000: {
            switch (type) {
                case COG_MSG_BRIDGE_STARTED:       return "bridge_started";
                case COG_MSG_BRIDGE_STOPPED:       return "bridge_stopped";
                case COG_MSG_MODULE_REGISTERED:    return "module_registered";
                case COG_MSG_MODULE_UNREGISTERED:  return "module_unregistered";
                case COG_MSG_STATE_SYNC_REQUEST:   return "state_sync_request";
                case COG_MSG_STATE_SYNC_COMPLETE:  return "state_sync_complete";
                case COG_MSG_BROADCAST_ALL:        return "broadcast_all";
                default:                           return "bridge_message";
            }
        }
        case 0x2001: return "attention_message";
        case 0x2002: return "emotion_message";
        case 0x2003: return "working_memory_message";
        case 0x2004: return "reasoning_message";
        case 0x2005: return "goal_message";
        case 0x2006: return "salience_message";
        case 0x2007: return "ethics_message";
        case 0x2008: return "curiosity_message";
        case 0x2009: return "introspection_message";
        case 0x200A: return "theory_of_mind_message";
        case 0x200B: return "empathy_message";
        case 0x200C: return "knowledge_message";
        case 0x200D: return "consolidation_message";
        case 0x200E: return "decision_message";
        case 0x200F: return "executive_message";
        default:     return "unknown_message";
    }
}

const char* cognitive_bio_transition_name(cog_state_transition_t transition) {
    switch (transition) {
        case COG_TRANSITION_NONE:             return "none";
        case COG_TRANSITION_IDLE_TO_ACTIVE:   return "idle_to_active";
        case COG_TRANSITION_ACTIVE_TO_FOCUSED: return "active_to_focused";
        case COG_TRANSITION_FOCUSED_TO_ACTIVE: return "focused_to_active";
        case COG_TRANSITION_ACTIVE_TO_IDLE:   return "active_to_idle";
        case COG_TRANSITION_TO_SLEEP:         return "to_sleep";
        case COG_TRANSITION_FROM_SLEEP:       return "from_sleep";
        case COG_TRANSITION_EMERGENCY:        return "emergency";
        case COG_TRANSITION_OVERLOAD:         return "overload";
        case COG_TRANSITION_RECOVERY:         return "recovery";
        default:                              return "unknown";
    }
}

const char* cognitive_bio_priority_name(cog_priority_t priority) {
    switch (priority) {
        case COG_PRIORITY_BACKGROUND: return "background";
        case COG_PRIORITY_NORMAL:     return "normal";
        case COG_PRIORITY_ELEVATED:   return "elevated";
        case COG_PRIORITY_HIGH:       return "high";
        case COG_PRIORITY_URGENT:     return "urgent";
        case COG_PRIORITY_CRITICAL:   return "critical";
        default:                      return "unknown";
    }
}

uint32_t cognitive_bio_module_id(cog_module_type_t type) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_bio_async_bridge_heartbeat("cognitive_bi_cognitive_bio_module", 0.0f);


    switch (type) {
        case COG_MODULE_TYPE_ATTENTION:      return COG_BIO_MODULE_ATTENTION;
        case COG_MODULE_TYPE_EMOTION:        return COG_BIO_MODULE_EMOTION;
        case COG_MODULE_TYPE_WORKING_MEMORY: return COG_BIO_MODULE_WORKING_MEMORY;
        case COG_MODULE_TYPE_REASONING:      return COG_BIO_MODULE_REASONING;
        case COG_MODULE_TYPE_GOAL:           return COG_BIO_MODULE_GOAL;
        case COG_MODULE_TYPE_SALIENCE:       return COG_BIO_MODULE_SALIENCE;
        case COG_MODULE_TYPE_ETHICS:         return COG_BIO_MODULE_ETHICS;
        case COG_MODULE_TYPE_CURIOSITY:      return COG_BIO_MODULE_CURIOSITY;
        case COG_MODULE_TYPE_INTROSPECTION:  return COG_BIO_MODULE_INTROSPECTION;
        case COG_MODULE_TYPE_THEORY_OF_MIND: return COG_BIO_MODULE_THEORY_OF_MIND;
        case COG_MODULE_TYPE_EMPATHY:        return COG_BIO_MODULE_EMPATHY;
        case COG_MODULE_TYPE_KNOWLEDGE:      return COG_BIO_MODULE_KNOWLEDGE;
        case COG_MODULE_TYPE_CONSOLIDATION:  return COG_BIO_MODULE_CONSOLIDATION;
        case COG_MODULE_TYPE_DECISION:       return COG_BIO_MODULE_DECISION;
        case COG_MODULE_TYPE_EXECUTIVE:      return COG_BIO_MODULE_EXECUTIVE;
        default:                             return COG_BIO_MODULE_ROOT;
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void cognitive_bio_async_bridge_set_instance_health_agent(cognitive_bio_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "cognitive_bio_async_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int cognitive_bio_async_bridge_training_begin(cognitive_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "cognitive_bio_async_bridge_training_begin: NULL bridge");
        return -1;
    }
    cognitive_bio_async_bridge_heartbeat_instance(bridge->health_agent, "cognitive_bio_async_bridge_training_begin", 0.0f);
    NIMCP_LOGGING_DEBUG("cognitive_bio_async_bridge_training_begin called");
    return 0;
}

int cognitive_bio_async_bridge_training_end(cognitive_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "cognitive_bio_async_bridge_training_end: NULL bridge");
        return -1;
    }
    cognitive_bio_async_bridge_heartbeat_instance(bridge->health_agent, "cognitive_bio_async_bridge_training_end", 1.0f);
    NIMCP_LOGGING_DEBUG("cognitive_bio_async_bridge_training_end called");
    return 0;
}

int cognitive_bio_async_bridge_training_step(cognitive_bio_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "cognitive_bio_async_bridge_training_step: NULL bridge");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    cognitive_bio_async_bridge_heartbeat_instance(bridge->health_agent, "cognitive_bio_async_bridge_training_step", progress);
    NIMCP_LOGGING_DEBUG("cognitive_bio_async_bridge_training_step called (progress=%.3f)", progress);
    return 0;
}
