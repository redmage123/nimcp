/**
 * @file nimcp_mirror_snn_bridge.c
 * @brief Mirror Neuron - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "cognitive/mirror_neurons/nimcp_mirror_snn_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/neuron_types/nimcp_neuron_types.h"
#include "core/synapse_types/nimcp_synapse_types.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

/* G8: guarded lightweight-pop assignment. See attention bridge for rationale. */
static int g8_add_lw_pop(snn_network_t* snn, uint32_t n, neuron_type_t t,
                         const char* name, uint32_t* out)
{
    int id = snn_network_add_population_lightweight(snn, n, t, name);
    if (id < 0) { return -1; }
    *out = (uint32_t)id;
    return 0;
}

#define LOG_MODULE "mirror_snn_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(mirror_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from mirror_snn_bridge module (instance-level) */
static inline void mirror_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


//=============================================================================
// Internal Structures
//=============================================================================

struct mirror_snn_bridge {
    bridge_base_t base;                  /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    mirror_snn_config_t config;

    /* SNN network */
    snn_network_t* snn;
    bool owns_snn;                       /**< true if bridge created the SNN */

    /* State tracking */
    mirror_snn_state_t state;
    mirror_snn_action_state_t actions[MIRROR_SNN_MAX_ACTIONS];
    uint32_t num_actions;

    /* Population mapping */
    uint32_t observation_pop;            /**< Observation input population */
    uint32_t execution_pop;              /**< Execution input population */
    uint32_t hidden_pop;                 /**< Hidden layer population */
    uint32_t output_pop;                 /**< Output/recognition population */

    /* Callbacks */
    mirror_snn_spike_callback_t spike_callback;
    void* spike_callback_data;
    mirror_snn_recognition_callback_t recognition_callback;
    void* recognition_callback_data;
    mirror_snn_training_callback_t training_callback;
    void* training_callback_data;
    mirror_snn_health_callback_t health_callback;
    void* health_callback_data;

    /* Bio-async integration */
    bool bio_async_connected;
    bio_module_context_t bio_ctx;

    /* Immune integration */
    void* immune_system;
    bool immune_connected;

    /* Statistics */
    mirror_snn_stats_t stats;

    /* Timing */
    uint64_t last_update_us;
    snn_state_health_t last_health;

    /* Health agent (instance-level) */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(mirror_snn_bridge)

static uint64_t get_time_us(void) {
    return nimcp_time_get_us();
}

static void init_action_state(mirror_snn_action_state_t* action, uint32_t id) {
    memset(action, 0, sizeof(*action));
    action->action_id = id;
    action->status = MIRROR_SNN_ACTION_INACTIVE;
}

//=============================================================================
// Bio-Async Message Handlers
//=============================================================================

static int handle_spike_event(
    snn_network_t* network,
    snn_bio_msg_type_t type,
    const void* msg,
    size_t msg_size,
    void* user_data
) {
    mirror_snn_bridge_t* bridge = (mirror_snn_bridge_t*)user_data;
    if (!bridge || !msg || type != SNN_BIO_MSG_SPIKE_EVENT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_spike_event: required parameter is NULL (bridge, msg)");
        return -1;
    }

    const snn_bio_spike_msg_t* spike = (const snn_bio_spike_msg_t*)msg;

    /* Update statistics */
    bridge->stats.bio_messages_received++;

    /* Invoke user callback if registered */
    if (bridge->spike_callback) {
        bridge->spike_callback(
            spike->population_id,
            spike->neuron_id,
            spike->spike_time,
            bridge->spike_callback_data
        );
    }

    return 0;
}

static int handle_training_event(
    snn_network_t* network,
    snn_bio_msg_type_t type,
    const void* msg,
    size_t msg_size,
    void* user_data
) {
    mirror_snn_bridge_t* bridge = (mirror_snn_bridge_t*)user_data;
    if (!bridge || !msg || type != SNN_BIO_MSG_STDP_EVENT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_training_event: required parameter is NULL (bridge, msg)");
        return -1;
    }

    const snn_bio_stdp_msg_t* stdp = (const snn_bio_stdp_msg_t*)msg;

    /* Update statistics */
    bridge->stats.stdp_events++;
    bridge->stats.avg_weight_change =
        0.99f * bridge->stats.avg_weight_change + 0.01f * fabsf(stdp->delta_w);

    /* Invoke user callback if registered */
    if (bridge->training_callback) {
        bridge->training_callback(
            stdp->pre_id,  /* Use as synapse identifier */
            stdp->delta_w,
            stdp->new_weight,
            bridge->training_callback_data
        );
    }

    return 0;
}

static int handle_population_activity(
    snn_network_t* network,
    snn_bio_msg_type_t type,
    const void* msg,
    size_t msg_size,
    void* user_data
) {
    mirror_snn_bridge_t* bridge = (mirror_snn_bridge_t*)user_data;
    if (!bridge || !msg || type != SNN_BIO_MSG_POPULATION_ACTIVITY) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_population_activity: required parameter is NULL (bridge, msg)");
        return -1;
    }

    const snn_bio_population_msg_t* pop = (const snn_bio_population_msg_t*)msg;

    /* Update action state if this is the output population */
    if (pop->population_id == bridge->output_pop) {
        /* Find or create action entry */
        for (uint32_t i = 0; i < bridge->num_actions; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_actions > 256) {
                mirror_snn_bridge_heartbeat("mirror_snn_b_loop",
                                 (float)(i + 1) / (float)bridge->num_actions);
            }

            if (bridge->actions[i].action_id == pop->population_id) {
                bridge->actions[i].spike_count = pop->n_active;
                bridge->actions[i].last_spike_us = get_time_us();
                break;
            }
        }
    }

    return 0;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

mirror_snn_config_t mirror_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_config_de", 0.0f);


    mirror_snn_config_t config = {
        .input_dim = MIRROR_SNN_INPUT_DIM,
        .hidden_dim = NIMCP_DEFAULT_HIDDEN_SIZE,
        .output_dim = MIRROR_SNN_OUTPUT_DIM,
        .neurons_per_action = MIRROR_SNN_NEURONS_PER_ACTION,

        .encoding_method = MIRROR_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .encoding_threshold = 0.1f,
        .encoding_window_ms = MIRROR_SNN_ENCODING_WINDOW,

        .dt_ms = MIRROR_SNN_DEFAULT_DT,
        .simulation_duration_ms = 100.0f,
        .enable_recurrence = true,

        .enable_training = true,
        .learning_rate = NIMCP_LEARNING_RATE_FINE,
        .enable_reward_modulation = true,

        .decoding_threshold = 0.2f,
        .confidence_gain = 1.0f,

        .enable_bio_async = true,
        .bio_async_priority = 1,

        .enable_immune_integration = true,
        .update_interval_ms = 10.0f
    };
    return config;
}

mirror_snn_bridge_t* mirror_snn_create(const mirror_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_create", 0.0f);


    mirror_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(mirror_snn_bridge_t));
    if (!bridge) {
        NIMCP_LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Copy configuration */
    bridge->config = config ? *config : mirror_snn_config_default();

    /* Create SNN network */
    snn_config_t snn_config;
    snn_config_feedforward(&snn_config,
        bridge->config.input_dim,
        bridge->config.hidden_dim,
        bridge->config.output_dim);

    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;
    /* Allow additional populations (default from feedforward is only 3) */
    snn_config.n_populations = 0;  /* Use SNN_MAX_POPULATIONS (64) */

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        NIMCP_LOG_ERROR(LOG_MODULE, "Failed to create SNN network");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_snn_create: bridge->snn is NULL");
        return NULL;
    }
    bridge->owns_snn = true;

    /* Add populations for observation and execution pathways (G8: guarded) */
    if (g8_add_lw_pop(bridge->snn, bridge->config.input_dim, NEURON_GENERIC_LIF,
                      "observation", &bridge->observation_pop) != 0
     || g8_add_lw_pop(bridge->snn, bridge->config.input_dim, NEURON_GENERIC_LIF,
                      "execution", &bridge->execution_pop) != 0
     || g8_add_lw_pop(bridge->snn, bridge->config.hidden_dim, NEURON_GENERIC_LIF,
                      "hidden", &bridge->hidden_pop) != 0
     || g8_add_lw_pop(bridge->snn, bridge->config.output_dim, NEURON_GENERIC_LIF,
                      "output", &bridge->output_pop) != 0) {
        NIMCP_LOG_ERROR(LOG_MODULE, "mirror_snn_create: lightweight pop alloc failed");
        snn_network_destroy(bridge->snn);
        nimcp_free(bridge);
        return NULL;
    }

    /* Connect populations */
    snn_network_connect_populations(bridge->snn, bridge->observation_pop,
        bridge->hidden_pop, SNN_TOPO_RANDOM, 0.3f, SYNAPSE_AMPA, 0.5f, 0.1f);
    snn_network_connect_populations(bridge->snn, bridge->execution_pop,
        bridge->hidden_pop, SNN_TOPO_RANDOM, 0.3f, SYNAPSE_AMPA, 0.5f, 0.1f);
    snn_network_connect_populations(bridge->snn, bridge->hidden_pop,
        bridge->output_pop, SNN_TOPO_FEEDFORWARD, 1.0f, SYNAPSE_AMPA, 0.5f, 0.1f);

    /* Add recurrent connections if enabled */
    if (bridge->config.enable_recurrence) {
        snn_network_connect_populations(bridge->snn, bridge->hidden_pop,
            bridge->hidden_pop, SNN_TOPO_RANDOM, 0.1f, SYNAPSE_AMPA, 0.3f, 0.1f);
    }

    /* G8/H1 input shunt: snn_network_create made a dense input_pop at id 0
     * to catch snn_network_set_inputs. Route it to both observation and
     * execution pathways so external input reaches the lightweight graph. */
    snn_network_connect_populations(bridge->snn,
        /*src=dense input_pop*/ 0, bridge->observation_pop,
        SNN_TOPO_RANDOM, 0.5f, SYNAPSE_AMPA, 0.5f, 0.1f);
    snn_network_connect_populations(bridge->snn,
        /*src=dense input_pop*/ 0, bridge->execution_pop,
        SNN_TOPO_RANDOM, 0.5f, SYNAPSE_AMPA, 0.5f, 0.1f);

    /* G8/H1 output shunt: bridge's output_pop → dense output_pop (id 2) so
     * snn_network_get_outputs returns the bridge's result. */
    snn_network_connect_populations(bridge->snn,
        bridge->output_pop, /*dst=dense output_pop*/ 2,
        SNN_TOPO_FEEDFORWARD, 1.0f, SYNAPSE_AMPA, 0.7f, 0.05f);

    /* G8: lightweight CSR populations store synapses in COO form during
     * connect_populations; finalize builds row_ptr for O(1) incoming lookup
     * during forward pass. Skipped for dense pops (no-op). */
    snn_network_finalize_connections(bridge->snn);

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "mirror_snn") != 0) {
        NIMCP_LOG_ERROR(LOG_MODULE, "Failed to initialize bridge base");
        snn_network_destroy(bridge->snn);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mirror_snn_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->state = MIRROR_SNN_STATE_IDLE;
    bridge->last_health = SNN_STATE_HEALTHY;
    bridge->last_update_us = get_time_us();

    /* Initialize action states */
    for (uint32_t i = 0; i < MIRROR_SNN_MAX_ACTIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_SNN_MAX_ACTIONS > 256) {
            mirror_snn_bridge_heartbeat("mirror_snn_b_loop",
                             (float)(i + 1) / (float)MIRROR_SNN_MAX_ACTIONS);
        }

        init_action_state(&bridge->actions[i], i);
    }

    NIMCP_LOG_INFO(LOG_MODULE, "Created mirror-SNN bridge (input=%u, hidden=%u, output=%u)",
        bridge->config.input_dim, bridge->config.hidden_dim, bridge->config.output_dim);

    NIMCP_LOGGING_INFO("Created %s bridge", "mirror_snn");
    return bridge;
}

mirror_snn_bridge_t* mirror_snn_create_with_network(
    const mirror_snn_config_t* config,
    snn_network_t* snn
) {
    if (!snn) {
        NIMCP_LOG_ERROR(LOG_MODULE, "NULL SNN network provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_create_wi", 0.0f);


    mirror_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(mirror_snn_bridge_t));
    if (!bridge) {
        NIMCP_LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    bridge->config = config ? *config : mirror_snn_config_default();
    bridge->snn = snn;
    bridge->owns_snn = false;  /* Do NOT destroy on cleanup */

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "mirror_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mirror_snn_create_with_network: validation failed");
        return NULL;
    }

    bridge->state = MIRROR_SNN_STATE_IDLE;
    bridge->last_health = SNN_STATE_HEALTHY;
    bridge->last_update_us = get_time_us();

    for (uint32_t i = 0; i < MIRROR_SNN_MAX_ACTIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_SNN_MAX_ACTIONS > 256) {
            mirror_snn_bridge_heartbeat("mirror_snn_b_loop",
                             (float)(i + 1) / (float)MIRROR_SNN_MAX_ACTIONS);
        }

        init_action_state(&bridge->actions[i], i);
    }

    return bridge;
}

void mirror_snn_destroy(mirror_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "mirror_snn");

    /* Disconnect integrations */
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_destroy", 0.0f);


    mirror_snn_disconnect_bio_async(bridge);
    mirror_snn_disconnect_immune(bridge);

    /* Destroy SNN only if we own it */
    if (bridge->owns_snn && bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOG_INFO(LOG_MODULE, "Destroyed mirror-SNN bridge");
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int mirror_snn_connect_bio_async(mirror_snn_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->bio_async_connected) return 0;  /* Already connected */

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_connect_b", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Connect SNN to bio-async */
    int ret = snn_network_connect_bio_async(bridge->snn);
    if (ret != SNN_SUCCESS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_LOG_ERROR(LOG_MODULE, "Failed to connect SNN to bio-async");
        return ret;
    }

    /* Register message handlers */
    snn_bio_async_register_handler(bridge->snn, SNN_BIO_MSG_SPIKE_EVENT,
        handle_spike_event, bridge);
    snn_bio_async_register_handler(bridge->snn, SNN_BIO_MSG_STDP_EVENT,
        handle_training_event, bridge);
    snn_bio_async_register_handler(bridge->snn, SNN_BIO_MSG_POPULATION_ACTIVITY,
        handle_population_activity, bridge);

    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_INFO(LOG_MODULE, "Connected to bio-async");
    return 0;
}

int mirror_snn_disconnect_bio_async(mirror_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_disconnect_bio_async: bridge is NULL");
        return 0;
    }
    if (!bridge->bio_async_connected) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_disconnec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    snn_network_disconnect_bio_async(bridge->snn);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_INFO(LOG_MODULE, "Disconnected from bio-async");
    return 0;
}

bool mirror_snn_is_bio_async_connected(const mirror_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_is_bio_as", 0.0f);


    return bridge->bio_async_connected;
}

int mirror_snn_process_messages(mirror_snn_bridge_t* bridge, int timeout_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_process_messages: bridge is NULL");
        return 0;
    }
    if (!bridge->bio_async_connected) return 0;
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_process_m", 0.0f);


    return snn_bio_async_process(bridge->snn, timeout_ms);
}

//=============================================================================
// Immune System Integration
//=============================================================================

int mirror_snn_connect_immune(mirror_snn_bridge_t* bridge, void* immune_system) {
    if (!bridge || !immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_connect_immune: bridge or immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_connect_i", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int ret = snn_network_connect_immune(bridge->snn, immune_system);
    if (ret == SNN_SUCCESS) {
        bridge->immune_system = immune_system;
        bridge->immune_connected = true;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return ret;
}

int mirror_snn_disconnect_immune(mirror_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_disconnect_immune: bridge is NULL");
        return 0;
    }
    if (!bridge->immune_connected) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_disconnec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->immune_system = NULL;
    bridge->immune_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int mirror_snn_apply_immune_modulation(mirror_snn_bridge_t* bridge) {
    if (!bridge || !bridge->immune_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_apply_immune_modulation: bridge is NULL or immune not connected");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_apply_imm", 0.0f);


    return snn_network_apply_immune_modulation(bridge->snn);
}

//=============================================================================
// Mirror --> SNN Pathway (Observation Encoding)
//=============================================================================

int mirror_snn_encode_observation(
    mirror_snn_bridge_t* bridge,
    uint32_t action_id,
    const float* features,
    uint32_t feature_dim,
    float observation_strength
) {
    if (!bridge || !features || feature_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_encode_observation: bridge or features is NULL or feature_dim is 0");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, features, feature_dim * sizeof(float));

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_encode_ob", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = MIRROR_SNN_STATE_ENCODING;

    /* Scale features by observation strength */
    uint32_t dim = feature_dim < bridge->config.input_dim ?
                   feature_dim : bridge->config.input_dim;
    float scaled[MIRROR_SNN_INPUT_DIM];
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            mirror_snn_bridge_heartbeat("mirror_snn_b_loop",
                             (float)(i + 1) / (float)dim);
        }

        scaled[i] = features[i] * observation_strength * bridge->config.encoding_gain;
    }
    for (uint32_t i = dim; i < bridge->config.input_dim; i++) {
        scaled[i] = 0.0f;
    }

    /* Set SNN inputs */
    int ret = snn_network_set_inputs(bridge->snn, scaled, bridge->config.input_dim);
    if (ret != SNN_SUCCESS) {
        bridge->state = MIRROR_SNN_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_encode_observation: validation failed");
        return -1;
    }

    /* Update action state */
    if (action_id < MIRROR_SNN_MAX_ACTIONS) {
        bridge->actions[action_id].status = MIRROR_SNN_ACTION_OBSERVED;
        bridge->actions[action_id].observation_start_us = get_time_us();
    }

    /* Update statistics */
    bridge->stats.total_observations++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return (int)dim;
}

int mirror_snn_encode_execution(
    mirror_snn_bridge_t* bridge,
    uint32_t action_id,
    const float* motor_command,
    uint32_t command_dim,
    float execution_strength
) {
    if (!bridge || !motor_command || command_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_encode_execution: bridge or motor_command is NULL or command_dim is 0");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, motor_command, command_dim * sizeof(float));

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_encode_ex", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = MIRROR_SNN_STATE_ENCODING;

    /* Scale motor command */
    uint32_t dim = command_dim < bridge->config.input_dim ?
                   command_dim : bridge->config.input_dim;
    float scaled[MIRROR_SNN_INPUT_DIM];
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            mirror_snn_bridge_heartbeat("mirror_snn_b_loop",
                             (float)(i + 1) / (float)dim);
        }

        scaled[i] = motor_command[i] * execution_strength * bridge->config.encoding_gain;
    }
    for (uint32_t i = dim; i < bridge->config.input_dim; i++) {
        scaled[i] = 0.0f;
    }

    /* Set SNN inputs (execution pathway) */
    int ret = snn_network_set_inputs(bridge->snn, scaled, bridge->config.input_dim);

    /* Update action state */
    if (action_id < MIRROR_SNN_MAX_ACTIONS) {
        bridge->actions[action_id].status = MIRROR_SNN_ACTION_EXECUTING;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return ret == SNN_SUCCESS ? (int)dim : -1;
}

int mirror_snn_set_input_tensor(
    mirror_snn_bridge_t* bridge,
    const nimcp_tensor_t* input
) {
    if (!bridge || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_set_input_tensor: bridge or input is NULL");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, input, sizeof(void*));
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_set_input", 0.0f);


    return snn_network_set_input_tensor(bridge->snn, input);
}

//=============================================================================
// SNN --> Mirror Pathway (Output Decoding)
//=============================================================================

int mirror_snn_simulate(mirror_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_simulate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = MIRROR_SNN_STATE_SIMULATING;

    uint64_t start = get_time_us();
    int spikes = snn_network_run(bridge->snn, duration_ms);
    uint64_t elapsed = get_time_us() - start;

    /* Update statistics */
    bridge->stats.total_simulation_steps++;
    bridge->stats.total_spikes_generated += spikes > 0 ? spikes : 0;
    bridge->stats.avg_simulation_time_us =
        0.99f * bridge->stats.avg_simulation_time_us + 0.01f * (float)elapsed;

    bridge->state = MIRROR_SNN_STATE_DECODING;
    nimcp_mutex_unlock(bridge->base.mutex);

    return spikes;
}

int mirror_snn_get_recognized_action(
    mirror_snn_bridge_t* bridge,
    uint32_t* action_id,
    float* confidence
) {
    if (!bridge || !action_id || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_get_recognized_action: bridge, action_id, or confidence is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_get_recog", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get SNN outputs */
    float outputs[MIRROR_SNN_OUTPUT_DIM];
    int ret = snn_network_get_outputs(bridge->snn, outputs, bridge->config.output_dim);
    if (ret != SNN_SUCCESS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return ret;
    }

    /* Find maximum output */
    uint32_t best_action = 0;
    float best_conf = outputs[0];
    for (uint32_t i = 1; i < bridge->config.output_dim; i++) {
        if (outputs[i] > best_conf) {
            best_conf = outputs[i];
            best_action = i;
        }
    }

    /* Apply confidence gain and threshold */
    best_conf *= bridge->config.confidence_gain;
    if (best_conf < bridge->config.decoding_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_get_recognized_action: validation failed");
        return -1;  /* No confident recognition */
    }

    *action_id = best_action;
    *confidence = nimcp_clampf(best_conf, 0.0f, 1.0f);

    /* Update action state */
    if (best_action < MIRROR_SNN_MAX_ACTIONS) {
        bridge->actions[best_action].status = MIRROR_SNN_ACTION_RECOGNIZED;
        bridge->actions[best_action].recognition_confidence = best_conf;
    }

    /* Invoke callback */
    if (bridge->recognition_callback) {
        float latency = (float)(get_time_us() -
            bridge->actions[best_action].observation_start_us) / 1000.0f;
        bridge->recognition_callback(
            best_action, best_conf, latency, bridge->recognition_callback_data);
    }

    bridge->stats.total_recognitions++;
    bridge->state = MIRROR_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_snn_get_action_confidences(
    mirror_snn_bridge_t* bridge,
    float* confidences,
    uint32_t num_actions
) {
    if (!bridge || !confidences) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_get_action_confidences: bridge or confidences is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_get_actio", 0.0f);


    uint32_t n = num_actions < bridge->config.output_dim ?
                 num_actions : bridge->config.output_dim;

    int ret = snn_network_get_outputs(bridge->snn, confidences, n);
    if (ret != SNN_SUCCESS) return ret;

    /* Normalize firing rates to [0, 1] confidence
     * SNN outputs are in Hz - typical max firing rates ~100-200 Hz
     * We use sigmoid normalization for smooth scaling */
    const float max_rate = 100.0f;  /* Reference max rate */

    /* Count confident actions and normalize */
    int count = 0;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            mirror_snn_bridge_heartbeat("mirror_snn_b_loop",
                             (float)(i + 1) / (float)n);
        }

        /* Normalize: rate/max_rate then apply gain and clamp */
        float normalized = confidences[i] / max_rate;
        normalized = nimcp_clampf(normalized * bridge->config.confidence_gain, 0.0f, 1.0f);
        confidences[i] = normalized;
        if (confidences[i] > bridge->config.decoding_threshold) count++;
    }

    return count;
}

float mirror_snn_get_population_rate(
    mirror_snn_bridge_t* bridge,
    uint32_t action_id,
    float window_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_get_population_rate: bridge is NULL");
        return -1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_get_popul", 0.0f);


    return snn_network_get_population_rate(bridge->snn, action_id, window_ms);
}

int mirror_snn_get_output_tensor(
    mirror_snn_bridge_t* bridge,
    nimcp_tensor_t* output
) {
    if (!bridge || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_get_output_tensor: bridge or output is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_get_outpu", 0.0f);


    return snn_network_get_output_tensor(bridge->snn, output);
}

//=============================================================================
// Complete Forward Pass
//=============================================================================

int mirror_snn_forward(
    mirror_snn_bridge_t* bridge,
    uint32_t action_id,
    const float* features,
    uint32_t feature_dim,
    float observation_strength,
    uint32_t* recognized_action,
    float* recognition_confidence
) {
    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_forward: bridge or features is NULL");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, features, feature_dim * sizeof(float));

    /* Encode observation */
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_forward", 0.0f);


    int spikes = mirror_snn_encode_observation(
        bridge, action_id, features, feature_dim, observation_strength);
    if (spikes < 0) return spikes;

    /* Simulate */
    spikes = mirror_snn_simulate(bridge, bridge->config.simulation_duration_ms);
    if (spikes < 0) return spikes;

    /* Decode recognition */
    return mirror_snn_get_recognized_action(
        bridge, recognized_action, recognition_confidence);
}

//=============================================================================
// Training API
//=============================================================================

int mirror_snn_set_training(mirror_snn_bridge_t* bridge, bool enable) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_set_train", 0.0f);


    return snn_network_set_training(bridge->snn, enable);
}

int mirror_snn_apply_stdp(mirror_snn_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_apply_std", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = MIRROR_SNN_STATE_TRAINING;

    int updates = snn_network_apply_stdp(bridge->snn);
    bridge->stats.training_iterations++;

    bridge->state = MIRROR_SNN_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);

    return updates;
}

int mirror_snn_apply_reward(mirror_snn_bridge_t* bridge, float reward) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_apply_rew", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = MIRROR_SNN_STATE_TRAINING;

    int updates = snn_network_apply_rstdp(bridge->snn, reward);
    bridge->stats.training_iterations++;

    bridge->state = MIRROR_SNN_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);

    return updates;
}

float mirror_snn_train_step(
    mirror_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    uint32_t target_action
) {
    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_train_step: bridge or features is NULL");
        return -1.0f;
    }
    BRIDGE_BBB_VALIDATE(bridge, features, feature_dim * sizeof(float));

    /* Create target tensor */
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_train_ste", 0.0f);


    float targets[MIRROR_SNN_OUTPUT_DIM] = {0};
    if (target_action < bridge->config.output_dim) {
        targets[target_action] = 1.0f;
    }

    return snn_network_train_step(
        bridge->snn,
        features, feature_dim,
        targets, bridge->config.output_dim,
        bridge->config.simulation_duration_ms
    );
}

//=============================================================================
// Callback Registration
//=============================================================================

int mirror_snn_register_spike_callback(
    mirror_snn_bridge_t* bridge,
    mirror_snn_spike_callback_t callback,
    void* user_data
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_register_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->spike_callback = callback;
    bridge->spike_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_snn_register_recognition_callback(
    mirror_snn_bridge_t* bridge,
    mirror_snn_recognition_callback_t callback,
    void* user_data
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_register_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->recognition_callback = callback;
    bridge->recognition_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_snn_register_training_callback(
    mirror_snn_bridge_t* bridge,
    mirror_snn_training_callback_t callback,
    void* user_data
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_register_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->training_callback = callback;
    bridge->training_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_snn_register_health_callback(
    mirror_snn_bridge_t* bridge,
    mirror_snn_health_callback_t callback,
    void* user_data
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_register_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->health_callback = callback;
    bridge->health_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// State Query API
//=============================================================================

int mirror_snn_get_state(
    const mirror_snn_bridge_t* bridge,
    mirror_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_get_state: bridge or state is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_get_state", 0.0f);


    nimcp_mutex_lock(((mirror_snn_bridge_t*)bridge)->base.mutex);

    state->state = bridge->state;
    state->snn_health = snn_network_check_health(bridge->snn);

    snn_stats_t snn_stats;
    if (snn_network_get_stats(bridge->snn, &snn_stats) == SNN_SUCCESS) {
        state->mean_firing_rate = snn_stats.mean_firing_rate;
        state->sparsity = snn_stats.sparsity;
    }

    /* Count active observations and recognitions */
    state->active_observations = 0;
    state->recognized_actions = 0;
    for (uint32_t i = 0; i < MIRROR_SNN_MAX_ACTIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_SNN_MAX_ACTIONS > 256) {
            mirror_snn_bridge_heartbeat("mirror_snn_b_loop",
                             (float)(i + 1) / (float)MIRROR_SNN_MAX_ACTIONS);
        }

        if (bridge->actions[i].status == MIRROR_SNN_ACTION_OBSERVED) {
            state->active_observations++;
        } else if (bridge->actions[i].status == MIRROR_SNN_ACTION_RECOGNIZED) {
            state->recognized_actions++;
        }
    }

    state->training_active = bridge->config.enable_training;
    state->weight_updates = (uint32_t)bridge->stats.stdp_events;

    nimcp_mutex_unlock(((mirror_snn_bridge_t*)bridge)->base.mutex);
    return 0;
}

int mirror_snn_get_action_state(
    const mirror_snn_bridge_t* bridge,
    uint32_t action_id,
    mirror_snn_action_state_t* state
) {
    if (!bridge || !state || action_id >= MIRROR_SNN_MAX_ACTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_get_action_state: bridge or state is NULL or action_id invalid");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_get_actio", 0.0f);


    nimcp_mutex_lock(((mirror_snn_bridge_t*)bridge)->base.mutex);
    *state = bridge->actions[action_id];
    nimcp_mutex_unlock(((mirror_snn_bridge_t*)bridge)->base.mutex);

    return 0;
}

int mirror_snn_get_stats(
    const mirror_snn_bridge_t* bridge,
    mirror_snn_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_get_stats: bridge or stats is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_get_stats", 0.0f);


    nimcp_mutex_lock(((mirror_snn_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;

    /* Get bio-async stats */
    if (bridge->bio_async_connected) {
        snn_bio_async_get_stats(
            bridge->snn,
            &stats->bio_messages_sent,
            &stats->bio_messages_received,
            &stats->bio_messages_dropped
        );
    }

    nimcp_mutex_unlock(((mirror_snn_bridge_t*)bridge)->base.mutex);
    return 0;
}

void mirror_snn_reset_stats(mirror_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_reset_stats: bridge is NULL");
        return;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_reset_sta", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

snn_state_health_t mirror_snn_check_health(const mirror_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_check_health: bridge is NULL");
        return SNN_STATE_NAN_DETECTED;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_check_hea", 0.0f);


    return snn_network_check_health(bridge->snn);
}

//=============================================================================
// Main Update Loop
//=============================================================================

int mirror_snn_update(mirror_snn_bridge_t* bridge, float dt_ms) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = get_time_us();
    float elapsed_ms = (float)(now - bridge->last_update_us) / 1000.0f;

    /* Rate limiting */
    if (elapsed_ms < bridge->config.update_interval_ms) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Check health status changes */
    snn_state_health_t health = snn_network_check_health(bridge->snn);
    if (health != bridge->last_health && bridge->health_callback) {
        bridge->health_callback(
            bridge->last_health, health, bridge->health_callback_data);
    }
    bridge->last_health = health;

    /* Process bio-async messages */
    if (bridge->bio_async_connected) {
        snn_bio_async_process(bridge->snn, 0);
    }

    /* Apply immune modulation if connected */
    if (bridge->immune_connected) {
        snn_network_apply_immune_modulation(bridge->snn);
    }

    /* Update firing rates for tracked actions */
    for (uint32_t i = 0; i < bridge->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_actions > 256) {
            mirror_snn_bridge_heartbeat("mirror_snn_b_loop",
                             (float)(i + 1) / (float)bridge->num_actions);
        }

        if (bridge->actions[i].status != MIRROR_SNN_ACTION_INACTIVE) {
            bridge->actions[i].firing_rate =
                snn_network_get_population_rate(bridge->snn, i, 100.0f);
        }
    }

    bridge->last_update_us = now;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int mirror_snn_reset(mirror_snn_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    snn_network_reset(bridge->snn);

    for (uint32_t i = 0; i < MIRROR_SNN_MAX_ACTIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_SNN_MAX_ACTIONS > 256) {
            mirror_snn_bridge_heartbeat("mirror_snn_b_loop",
                             (float)(i + 1) / (float)MIRROR_SNN_MAX_ACTIONS);
        }

        init_action_state(&bridge->actions[i], i);
    }

    bridge->state = MIRROR_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Direct SNN Access
//=============================================================================

snn_network_t* mirror_snn_get_network(mirror_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_get_network: bridge is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_get_netwo", 0.0f);


    return bridge->snn;
}

int mirror_snn_get_snn_stats(
    const mirror_snn_bridge_t* bridge,
    snn_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_snn_get_snn_stats: bridge or stats is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_snn_bridge_heartbeat("mirror_snn_b_mirror_snn_get_snn_s", 0.0f);


    return snn_network_get_stats(bridge->snn, stats);
}

//=============================================================================
// Instance Health Agent Setter (B22 Upgrade)
//=============================================================================

void mirror_snn_bridge_set_instance_health_agent(
    mirror_snn_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B22 Upgrade)
//=============================================================================

int mirror_snn_bridge_training_begin(mirror_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_snn_bridge_heartbeat_instance(bridge->health_agent, "mirror_snn_bridge_training_begin", 0.0f);
    return 0;
}

int mirror_snn_bridge_training_end(mirror_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_snn_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_snn_bridge_heartbeat_instance(bridge->health_agent, "mirror_snn_bridge_training_end", 1.0f);
    return 0;
}

int mirror_snn_bridge_training_step(mirror_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_snn_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "mirror_snn_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "mirror_snn_bridge_training_step");
    mirror_snn_bridge_heartbeat_instance(bridge->health_agent, "mirror_snn_bridge_training_step", progress);
    return 0;
}
