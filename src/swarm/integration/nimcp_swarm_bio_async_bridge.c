/**
 * @file nimcp_swarm_bio_async_bridge.c
 * @brief Implementation of Swarm Bio-Async Integration Bridge
 *
 * WHAT: Implements swarm coordination via bio-async messaging
 * WHY:  Enable biological-inspired agent-to-agent communication
 * HOW:  Register with bio-router, handle message dispatch, coordinate consensus
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 */

#include "swarm/integration/nimcp_swarm_bio_async_bridge.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_bio_async_bridge)

/*=============================================================================
 * INTERNAL HELPERS
 *============================================================================*/

/**
 * @brief Find agent by ID (unlocked)
 */
static swarm_agent_info_t* find_agent_unlocked(swarm_bio_bridge_t* bridge, swarm_agent_id_t id) {
    if (!bridge || !bridge->agents) {
        return NULL;
    }
    for (uint32_t i = 0; i < bridge->agent_count; i++) {
        if (bridge->agents[i].id == id) {
            return &bridge->agents[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}

/**
 * @brief Send message via bio-async
 */
static int send_bio_async_message(
    swarm_bio_bridge_t* bridge,
    nimcp_bio_channel_type_t channel,
    swarm_bio_msg_type_t msg_type,
    const void* payload,
    size_t payload_size
) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return SWARM_BIO_ERROR_NOT_CONNECTED;
    }

    /* Create promise on specified channel */
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(channel, payload_size + sizeof(uint32_t));
    if (!promise) {
        bridge->stats.bio_async_errors++;
        return SWARM_BIO_ERROR_SEND_FAILED;
    }

    /* Pack message type + payload */
    uint8_t buffer[SWARM_BIO_BRIDGE_MAX_MSG_SIZE];
    if (payload_size + sizeof(uint32_t) > sizeof(buffer)) {
        nimcp_bio_promise_destroy(promise);
        return SWARM_BIO_ERROR_SEND_FAILED;
    }

    uint32_t type_val = (uint32_t)msg_type;
    memcpy(buffer, &type_val, sizeof(uint32_t));
    if (payload && payload_size > 0) {
        memcpy(buffer + sizeof(uint32_t), payload, payload_size);
    }

    /* Complete promise with data */
    nimcp_error_t err = nimcp_bio_promise_complete(promise, buffer);
    if (err != NIMCP_SUCCESS) {
        nimcp_bio_promise_destroy(promise);
        bridge->stats.bio_async_errors++;
        return SWARM_BIO_ERROR_SEND_FAILED;
    }

    nimcp_bio_promise_destroy(promise);
    return 0;
}

/**
 * @brief Handle incoming agent state message
 */
static void handle_agent_state_message(
    swarm_bio_bridge_t* bridge,
    const swarm_agent_state_msg_t* msg
) {
    if (!bridge || !msg) return;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update or register remote agent */
    swarm_agent_info_t* agent = find_agent_unlocked(bridge, msg->agent_id);
    if (agent) {
        agent->state = msg->state;
        agent->position = msg->position;
        agent->energy = msg->energy_level;
        agent->last_heartbeat = nimcp_platform_time_monotonic_ms();
    }

    bridge->stats.state_messages_received++;
    bridge->stats.last_activity_time = nimcp_platform_time_monotonic_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callback outside lock */
    if (bridge->on_agent_state) {
        bridge->on_agent_state(bridge, msg, bridge->callback_user_data);
    }
}

/**
 * @brief Handle incoming consensus message
 */
static void handle_consensus_message(
    swarm_bio_bridge_t* bridge,
    const swarm_consensus_msg_t* msg
) {
    if (!bridge || !msg) return;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.last_activity_time = nimcp_platform_time_monotonic_ms();
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callback */
    if (bridge->on_consensus) {
        bridge->on_consensus(bridge, msg, bridge->callback_user_data);
    }
}

/**
 * @brief Handle incoming pheromone message
 */
static void handle_pheromone_message(
    swarm_bio_bridge_t* bridge,
    const swarm_pheromone_msg_t* msg
) {
    if (!bridge || !msg) return;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.pheromone_received++;
    bridge->stats.last_activity_time = nimcp_platform_time_monotonic_ms();
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callback */
    if (bridge->on_pheromone) {
        bridge->on_pheromone(bridge, msg, bridge->callback_user_data);
    }
}

/**
 * @brief Handle incoming coordination message
 */
static void handle_coordination_message(
    swarm_bio_bridge_t* bridge,
    const swarm_coordination_msg_t* msg
) {
    if (!bridge || !msg) return;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.coordination_messages++;
    bridge->stats.last_activity_time = nimcp_platform_time_monotonic_ms();
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callback */
    if (bridge->on_coordination) {
        bridge->on_coordination(bridge, msg, bridge->callback_user_data);
    }
}

/**
 * @brief Bio-async message handler callback
 */
static nimcp_error_t bio_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)response_promise;  /* Most messages don't need response */

    swarm_bio_bridge_t* bridge = (swarm_bio_bridge_t*)user_data;
    if (!bridge || !msg || msg_size < sizeof(uint32_t)) {
        return NIMCP_SUCCESS;  /* Ignore malformed messages */
    }

    /* Extract message type */
    uint32_t msg_type;
    memcpy(&msg_type, msg, sizeof(uint32_t));
    const uint8_t* payload = (const uint8_t*)msg + sizeof(uint32_t);
    size_t payload_size = msg_size - sizeof(uint32_t);

    /* Dispatch by message type */
    switch (msg_type) {
        case SWARM_MSG_AGENT_STATE:
        case SWARM_MSG_AGENT_HEARTBEAT:
        case SWARM_MSG_AGENT_POSITION:
            if (payload_size >= sizeof(swarm_agent_state_msg_t)) {
                handle_agent_state_message(bridge, (const swarm_agent_state_msg_t*)payload);
            }
            break;

        case SWARM_MSG_CONSENSUS_REQUEST:
        case SWARM_MSG_CONSENSUS_VOTE:
        case SWARM_MSG_CONSENSUS_RESULT:
            if (payload_size >= sizeof(swarm_consensus_msg_t)) {
                handle_consensus_message(bridge, (const swarm_consensus_msg_t*)payload);
            }
            break;

        case SWARM_MSG_PHEROMONE:
        case SWARM_MSG_PHEROMONE_GRADIENT:
        case SWARM_MSG_STIGMERGY_MARK:
            if (payload_size >= sizeof(swarm_pheromone_msg_t)) {
                handle_pheromone_message(bridge, (const swarm_pheromone_msg_t*)payload);
            }
            break;

        case SWARM_MSG_COORDINATION_REQUEST:
        case SWARM_MSG_COORDINATION_ACK:
        case SWARM_MSG_FORMATION_UPDATE:
        case SWARM_MSG_TASK_DELEGATION:
            if (payload_size >= sizeof(swarm_coordination_msg_t)) {
                handle_coordination_message(bridge, (const swarm_coordination_msg_t*)payload);
            }
            break;

        default:
            /* Unknown message type - ignore */
            break;
    }

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

void swarm_bio_bridge_default_config(swarm_bio_bridge_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(swarm_bio_bridge_config_t));

    /* Capacity */
    config->max_agents = 64;
    config->inbox_capacity = NIMCP_INBOX_CAPACITY_LARGE;

    /* Bio-async channels */
    config->consensus_channel = BIO_CHANNEL_DOPAMINE;
    config->state_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->pheromone_channel = BIO_CHANNEL_SEROTONIN;
    config->alert_channel = BIO_CHANNEL_NOREPINEPHRINE;

    /* Timing */
    config->heartbeat_interval_ms = 1000;
    config->consensus_timeout_ms = 5000;
    config->pheromone_broadcast_interval_ms = 500;

    /* Features */
    config->enable_stigmergy = true;
    config->enable_consensus = true;
    config->enable_heartbeat = true;
    config->enable_logging = false;
    config->auto_connect_bio_async = true;

    /* Consensus */
    config->quorum_threshold = 0.66f;
    config->consensus_confidence_weight = 0.8f;
}

swarm_bio_bridge_t* swarm_bio_bridge_create(const swarm_bio_bridge_config_t* config) {
    swarm_bio_bridge_t* bridge = (swarm_bio_bridge_t*)nimcp_malloc(sizeof(swarm_bio_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_bio_bridge_create: failed to allocate swarm_bio_bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(swarm_bio_bridge_t));

    /* Initialize base */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SWARM_BIO_ASYNC_BRIDGE, "swarm_bio_async_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "swarm_bio_bridge_create: validation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        swarm_bio_bridge_default_config(&bridge->config);
    }

    /* Validate limits */
    if (bridge->config.max_agents > SWARM_BIO_BRIDGE_MAX_AGENTS) {
        bridge->config.max_agents = SWARM_BIO_BRIDGE_MAX_AGENTS;
    }
    if (bridge->config.max_agents == 0) {
        bridge->config.max_agents = 64;
    }

    /* Allocate agent array */
    bridge->agents = (swarm_agent_info_t*)nimcp_malloc(
        bridge->config.max_agents * sizeof(swarm_agent_info_t)
    );
    if (!bridge->agents) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_bio_bridge_create: failed to allocate agents array");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->agents, 0, bridge->config.max_agents * sizeof(swarm_agent_info_t));

    /* Allocate consensus vote array */
    bridge->consensus_votes = (uint32_t*)nimcp_malloc(bridge->config.max_agents * sizeof(uint32_t));
    if (!bridge->consensus_votes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_bio_bridge_create: failed to allocate consensus_votes array");
        nimcp_free(bridge->agents);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->consensus_votes, 0, bridge->config.max_agents * sizeof(uint32_t));

    /* Initialize state */
    bridge->agent_count = 0;
    bridge->next_agent_id = 1;
    bridge->consensus_in_progress = false;
    bridge->initialized = true;

    /* Auto-connect if requested */
    if (bridge->config.auto_connect_bio_async && bio_router_is_initialized()) {
        swarm_bio_bridge_connect_bio_async(bridge);
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Swarm bio-async bridge created with max_agents=%u",
                         bridge->config.max_agents);
    }

    return bridge;
}

void swarm_bio_bridge_destroy(swarm_bio_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect from bio-async */
    if (bridge->base.bio_async_enabled) {
        swarm_bio_bridge_disconnect_bio_async(bridge);
    }

    /* Free allocations */
    if (bridge->consensus_votes) {
        nimcp_free(bridge->consensus_votes);
        bridge->consensus_votes = NULL;
    }
    if (bridge->agents) {
        nimcp_free(bridge->agents);
        bridge->agents = NULL;
    }
    if (bridge->active_consensus) {
        nimcp_free(bridge->active_consensus);
        bridge->active_consensus = NULL;
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
}

int swarm_bio_bridge_reset(swarm_bio_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Clear agents */
    if (bridge->agents) {
        memset(bridge->agents, 0, bridge->config.max_agents * sizeof(swarm_agent_info_t));
    }
    bridge->agent_count = 0;
    bridge->next_agent_id = 1;

    /* Clear consensus */
    bridge->consensus_in_progress = false;
    if (bridge->consensus_votes) {
        memset(bridge->consensus_votes, 0, bridge->config.max_agents * sizeof(uint32_t));
    }

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(swarm_bio_bridge_stats_t));

    /* Reset base */
    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * BIO-ASYNC CONNECTION
 *============================================================================*/

int swarm_bio_bridge_connect_bio_async(swarm_bio_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (bridge->base.bio_async_enabled) return 0;  /* Already connected */

    if (!bio_router_is_initialized()) {
        return SWARM_BIO_ERROR_NOT_CONNECTED;
    }

    /* Register module */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SWARM_BIO_ASYNC_BRIDGE,
        .module_name = "swarm_bio_async_bridge",
        .inbox_capacity = bridge->config.inbox_capacity,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (!bridge->base.bio_ctx) {
        return SWARM_BIO_ERROR_NOT_CONNECTED;
    }

    /* Register message handlers for swarm message types */
    bio_router_register_handler(bridge->base.bio_ctx, SWARM_MSG_AGENT_STATE, bio_message_handler);
    bio_router_register_handler(bridge->base.bio_ctx, SWARM_MSG_AGENT_HEARTBEAT, bio_message_handler);
    bio_router_register_handler(bridge->base.bio_ctx, SWARM_MSG_AGENT_POSITION, bio_message_handler);
    bio_router_register_handler(bridge->base.bio_ctx, SWARM_MSG_CONSENSUS_REQUEST, bio_message_handler);
    bio_router_register_handler(bridge->base.bio_ctx, SWARM_MSG_CONSENSUS_VOTE, bio_message_handler);
    bio_router_register_handler(bridge->base.bio_ctx, SWARM_MSG_CONSENSUS_RESULT, bio_message_handler);
    bio_router_register_handler(bridge->base.bio_ctx, SWARM_MSG_PHEROMONE, bio_message_handler);
    bio_router_register_handler(bridge->base.bio_ctx, SWARM_MSG_COORDINATION_REQUEST, bio_message_handler);

    bridge->base.bio_async_enabled = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Swarm bio-async bridge connected to router");
    }

    return 0;
}

int swarm_bio_bridge_disconnect_bio_async(swarm_bio_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    return 0;
}

bool swarm_bio_bridge_is_bio_async_connected(const swarm_bio_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}

/*=============================================================================
 * AGENT MANAGEMENT
 *============================================================================*/

swarm_agent_id_t swarm_bio_bridge_register_agent(
    swarm_bio_bridge_t* bridge,
    const char* name
) {
    return swarm_bio_bridge_register_agent_ex(bridge, name, NULL);
}

swarm_agent_id_t swarm_bio_bridge_register_agent_ex(
    swarm_bio_bridge_t* bridge,
    const char* name,
    void* user_data
) {
    if (!bridge || !name) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->agent_count >= bridge->config.max_agents) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Assign ID and populate info */
    swarm_agent_id_t id = bridge->next_agent_id++;
    swarm_agent_info_t* agent = &bridge->agents[bridge->agent_count];

    agent->id = id;
    strncpy(agent->name, name, SWARM_BIO_BRIDGE_AGENT_NAME_MAX - 1);
    agent->name[SWARM_BIO_BRIDGE_AGENT_NAME_MAX - 1] = '\0';
    agent->state = SWARM_AGENT_STATE_IDLE;
    agent->position = (swarm_bio_position_t){0.0f, 0.0f, 0.0f};
    agent->energy = 1.0f;
    agent->last_heartbeat = nimcp_platform_time_monotonic_ms();
    agent->is_local = true;
    agent->user_data = user_data;

    bridge->agent_count++;
    bridge->stats.total_agents_registered++;
    bridge->stats.active_agents = bridge->agent_count;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast join message */
    if (bridge->base.bio_async_enabled) {
        swarm_agent_state_msg_t msg = {
            .agent_id = id,
            .state = SWARM_AGENT_STATE_IDLE,
            .position = {0.0f, 0.0f, 0.0f},
            .energy_level = 1.0f,
            .confidence = 1.0f,
            .timestamp = nimcp_platform_time_monotonic_ms(),
            .sequence = 0
        };
        send_bio_async_message(bridge, bridge->config.state_channel,
                              SWARM_MSG_AGENT_JOINED, &msg, sizeof(msg));
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Registered agent '%s' with ID %u", name, id);
    }

    return id;
}

int swarm_bio_bridge_unregister_agent(
    swarm_bio_bridge_t* bridge,
    swarm_agent_id_t agent_id
) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find and remove agent */
    int found_idx = -1;
    for (uint32_t i = 0; i < bridge->agent_count; i++) {
        if (bridge->agents[i].id == agent_id) {
            found_idx = (int)i;
            break;
        }
    }

    if (found_idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return SWARM_BIO_ERROR_AGENT_NOT_FOUND;
    }

    /* Broadcast leave message before removing */
    if (bridge->base.bio_async_enabled) {
        swarm_agent_state_msg_t msg = {
            .agent_id = agent_id,
            .state = SWARM_AGENT_STATE_DISCONNECTED,
            .timestamp = nimcp_platform_time_monotonic_ms()
        };
        send_bio_async_message(bridge, bridge->config.state_channel,
                              SWARM_MSG_AGENT_LEFT, &msg, sizeof(msg));
    }

    /* Shift remaining agents */
    for (uint32_t i = (uint32_t)found_idx; i < bridge->agent_count - 1; i++) {
        bridge->agents[i] = bridge->agents[i + 1];
    }
    bridge->agent_count--;
    bridge->stats.active_agents = bridge->agent_count;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int swarm_bio_bridge_get_agent_info(
    const swarm_bio_bridge_t* bridge,
    swarm_agent_id_t agent_id,
    swarm_agent_info_t* out_info
) {
    if (!bridge || !out_info) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);

    const swarm_agent_info_t* agent = find_agent_unlocked((swarm_bio_bridge_t*)bridge, agent_id);
    if (!agent) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return SWARM_BIO_ERROR_AGENT_NOT_FOUND;
    }

    *out_info = *agent;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_bio_bridge_update_agent_state(
    swarm_bio_bridge_t* bridge,
    swarm_agent_id_t agent_id,
    swarm_agent_state_t state
) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);

    swarm_agent_info_t* agent = find_agent_unlocked(bridge, agent_id);
    if (!agent) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return SWARM_BIO_ERROR_AGENT_NOT_FOUND;
    }

    agent->state = state;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast state change */
    return swarm_bio_bridge_broadcast_state(bridge, agent_id);
}

int swarm_bio_bridge_update_agent_position(
    swarm_bio_bridge_t* bridge,
    swarm_agent_id_t agent_id,
    const swarm_bio_position_t* position
) {
    if (!bridge || !position) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);

    swarm_agent_info_t* agent = find_agent_unlocked(bridge, agent_id);
    if (!agent) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return SWARM_BIO_ERROR_AGENT_NOT_FOUND;
    }

    agent->position = *position;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

uint32_t swarm_bio_bridge_get_agent_count(const swarm_bio_bridge_t* bridge) {
    if (!bridge) return 0;
    return bridge->agent_count;
}

/*=============================================================================
 * AGENT STATE MESSAGING
 *============================================================================*/

int swarm_bio_bridge_broadcast_state(
    swarm_bio_bridge_t* bridge,
    swarm_agent_id_t agent_id
) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);

    swarm_agent_info_t* agent = find_agent_unlocked(bridge, agent_id);
    if (!agent) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return SWARM_BIO_ERROR_AGENT_NOT_FOUND;
    }

    swarm_agent_state_msg_t msg = {
        .agent_id = agent->id,
        .state = agent->state,
        .position = agent->position,
        .energy_level = agent->energy,
        .confidence = 1.0f,
        .timestamp = nimcp_platform_time_monotonic_ms(),
        .sequence = 0
    };

    bridge->stats.state_messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return send_bio_async_message(bridge, bridge->config.state_channel,
                                  SWARM_MSG_AGENT_STATE, &msg, sizeof(msg));
}

int swarm_bio_bridge_send_heartbeat(
    swarm_bio_bridge_t* bridge,
    swarm_agent_id_t agent_id
) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);

    swarm_agent_info_t* agent = find_agent_unlocked(bridge, agent_id);
    if (!agent) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return SWARM_BIO_ERROR_AGENT_NOT_FOUND;
    }

    swarm_agent_state_msg_t msg = {
        .agent_id = agent->id,
        .state = agent->state,
        .position = agent->position,
        .energy_level = agent->energy,
        .confidence = 1.0f,
        .timestamp = nimcp_platform_time_monotonic_ms(),
        .sequence = 0
    };

    agent->last_heartbeat = msg.timestamp;

    nimcp_mutex_unlock(bridge->base.mutex);

    return send_bio_async_message(bridge, bridge->config.state_channel,
                                  SWARM_MSG_AGENT_HEARTBEAT, &msg, sizeof(msg));
}

/*=============================================================================
 * CONSENSUS COORDINATION
 *============================================================================*/

uint32_t swarm_bio_bridge_initiate_consensus(
    swarm_bio_bridge_t* bridge,
    swarm_consensus_topic_t topic,
    swarm_agent_id_t initiator,
    uint32_t options,
    uint32_t timeout_ms
) {
    if (!bridge || !bridge->config.enable_consensus) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->consensus_in_progress) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Already have active consensus */
    }

    /* Generate consensus ID */
    static uint32_t s_consensus_id = 1;
    uint32_t consensus_id = s_consensus_id++;

    /* Create consensus message */
    swarm_consensus_msg_t msg = {
        .consensus_id = consensus_id,
        .topic = topic,
        .initiator = initiator,
        .confidence = 1.0f,
        .option = options,
        .deadline_ms = timeout_ms > 0 ? timeout_ms : bridge->config.consensus_timeout_ms,
        .payload_size = 0
    };

    /* Store active consensus */
    if (!bridge->active_consensus) {
        bridge->active_consensus = (swarm_consensus_msg_t*)nimcp_malloc(sizeof(swarm_consensus_msg_t));
    }
    if (bridge->active_consensus) {
        *bridge->active_consensus = msg;
        bridge->consensus_in_progress = true;

        /* Reset vote tallies */
        memset(bridge->consensus_votes, 0, bridge->config.max_agents * sizeof(uint32_t));
    }

    bridge->stats.consensus_initiated++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast request */
    send_bio_async_message(bridge, bridge->config.consensus_channel,
                          SWARM_MSG_CONSENSUS_REQUEST, &msg, sizeof(msg));

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Initiated consensus %u on topic %d", consensus_id, (int)topic);
    }

    return consensus_id;
}

int swarm_bio_bridge_cast_vote(
    swarm_bio_bridge_t* bridge,
    uint32_t consensus_id,
    swarm_agent_id_t agent_id,
    uint32_t option,
    float confidence
) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->consensus_in_progress ||
        !bridge->active_consensus ||
        bridge->active_consensus->consensus_id != consensus_id) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return SWARM_BIO_ERROR_AGENT_NOT_FOUND;
    }

    /* Record vote locally */
    if (option < bridge->config.max_agents && bridge->consensus_votes) {
        bridge->consensus_votes[option]++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast vote */
    swarm_consensus_msg_t vote_msg = {
        .consensus_id = consensus_id,
        .topic = bridge->active_consensus ? bridge->active_consensus->topic : SWARM_CONSENSUS_CUSTOM,
        .initiator = agent_id,
        .confidence = confidence,
        .option = option,
        .deadline_ms = 0,
        .payload_size = 0
    };

    return send_bio_async_message(bridge, bridge->config.consensus_channel,
                                  SWARM_MSG_CONSENSUS_VOTE, &vote_msg, sizeof(vote_msg));
}

int swarm_bio_bridge_get_consensus_result(
    const swarm_bio_bridge_t* bridge,
    uint32_t consensus_id,
    swarm_consensus_result_t* out_result
) {
    if (!bridge || !out_result) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->active_consensus ||
        bridge->active_consensus->consensus_id != consensus_id) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return SWARM_BIO_ERROR_AGENT_NOT_FOUND;
    }

    /* Calculate result */
    out_result->consensus_id = consensus_id;
    out_result->topic = bridge->active_consensus->topic;

    uint32_t total_votes = 0;
    uint32_t winning_option = 0;
    uint32_t max_votes = 0;

    for (uint32_t i = 0; i < bridge->config.max_agents && bridge->consensus_votes; i++) {
        total_votes += bridge->consensus_votes[i];
        if (bridge->consensus_votes[i] > max_votes) {
            max_votes = bridge->consensus_votes[i];
            winning_option = i;
        }
    }

    out_result->votes_total = total_votes;
    out_result->winning_option = winning_option;
    out_result->votes_for = max_votes;
    out_result->votes_against = total_votes - max_votes;

    /* Check quorum */
    float quorum_ratio = (bridge->agent_count > 0) ?
        (float)total_votes / (float)bridge->agent_count : 0.0f;
    out_result->achieved = (quorum_ratio >= bridge->config.quorum_threshold);
    out_result->confidence = quorum_ratio;
    out_result->duration_ms = 0;  /* Would need timing tracking */

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int swarm_bio_bridge_abort_consensus(
    swarm_bio_bridge_t* bridge,
    uint32_t consensus_id
) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->consensus_in_progress ||
        !bridge->active_consensus ||
        bridge->active_consensus->consensus_id != consensus_id) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Nothing to abort */
    }

    bridge->consensus_in_progress = false;
    bridge->stats.consensus_failed++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast abort */
    swarm_consensus_msg_t abort_msg = {
        .consensus_id = consensus_id,
        .topic = SWARM_CONSENSUS_CUSTOM,
        .initiator = 0,
        .confidence = 0.0f,
        .option = 0,
        .deadline_ms = 0,
        .payload_size = 0
    };

    return send_bio_async_message(bridge, bridge->config.consensus_channel,
                                  SWARM_MSG_CONSENSUS_ABORT, &abort_msg, sizeof(abort_msg));
}

/*=============================================================================
 * PHEROMONE/STIGMERGY
 *============================================================================*/

int swarm_bio_bridge_broadcast_pheromone(
    swarm_bio_bridge_t* bridge,
    swarm_agent_id_t depositor,
    const swarm_pheromone_msg_t* pheromone
) {
    if (!bridge || !pheromone) return NIMCP_ERROR_NULL_POINTER;
    if (!bridge->config.enable_stigmergy) return 0;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.pheromone_broadcasts++;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create message with depositor */
    swarm_pheromone_msg_t msg = *pheromone;
    msg.depositor = depositor;
    msg.timestamp = nimcp_platform_time_monotonic_ms();

    return send_bio_async_message(bridge, bridge->config.pheromone_channel,
                                  SWARM_MSG_PHEROMONE, &msg, sizeof(msg));
}

int swarm_bio_bridge_query_pheromone_gradient(
    swarm_bio_bridge_t* bridge,
    const swarm_bio_position_t* position,
    swarm_bio_pheromone_type_t type,
    swarm_bio_position_t* out_direction,
    float* out_intensity
) {
    if (!bridge || !position) return NIMCP_ERROR_NULL_POINTER;

    /* This would typically query a spatial data structure.
     * For now, return zero gradient as placeholder. */
    if (out_direction) {
        *out_direction = (swarm_bio_position_t){0.0f, 0.0f, 0.0f};
    }
    if (out_intensity) {
        *out_intensity = 0.0f;
    }

    (void)type;
    return 0;
}

/*=============================================================================
 * COORDINATION
 *============================================================================*/

int swarm_bio_bridge_send_coordination(
    swarm_bio_bridge_t* bridge,
    const swarm_coordination_msg_t* msg
) {
    if (!bridge || !msg) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.coordination_messages++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return send_bio_async_message(bridge, bridge->config.state_channel,
                                  SWARM_MSG_COORDINATION_REQUEST, msg, sizeof(*msg));
}

int swarm_bio_bridge_broadcast_alert(
    swarm_bio_bridge_t* bridge,
    uint32_t alert_type,
    const swarm_bio_position_t* position,
    float intensity
) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    swarm_coordination_msg_t msg = {
        .coordination_id = 0,
        .target_agent = 0,  /* Broadcast */
        .action_type = alert_type,
        .target_position = position ? *position : (swarm_bio_position_t){0, 0, 0},
        .priority = intensity,
        .data_size = 0
    };

    /* Use norepinephrine for high-priority alerts */
    return send_bio_async_message(bridge, bridge->config.alert_channel,
                                  SWARM_MSG_COORDINATION_REQUEST, &msg, sizeof(msg));
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *============================================================================*/

int swarm_bio_bridge_on_agent_state(
    swarm_bio_bridge_t* bridge,
    swarm_agent_state_callback_t callback,
    void* user_data
) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->on_agent_state = callback;
    bridge->callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int swarm_bio_bridge_on_consensus(
    swarm_bio_bridge_t* bridge,
    swarm_consensus_callback_t callback,
    void* user_data
) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->on_consensus = callback;
    bridge->callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int swarm_bio_bridge_on_pheromone(
    swarm_bio_bridge_t* bridge,
    swarm_pheromone_callback_t callback,
    void* user_data
) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->on_pheromone = callback;
    bridge->callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int swarm_bio_bridge_on_coordination(
    swarm_bio_bridge_t* bridge,
    swarm_coordination_callback_t callback,
    void* user_data
) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->on_coordination = callback;
    bridge->callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * UPDATE AND PROCESSING
 *============================================================================*/

int swarm_bio_bridge_process_messages(
    swarm_bio_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    /* Message processing is handled by bio_message_handler callback
     * when registered with the router. This function exists for
     * explicit polling if needed. */
    (void)max_messages;

    return 0;
}

int swarm_bio_bridge_update(
    swarm_bio_bridge_t* bridge,
    uint32_t dt_ms
) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    uint64_t now = nimcp_platform_time_monotonic_ms();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for stale agents */
    uint32_t heartbeat_timeout = bridge->config.heartbeat_interval_ms * 3;
    for (uint32_t i = 0; i < bridge->agent_count; i++) {
        if (!bridge->agents[i].is_local) {
            uint64_t age = now - bridge->agents[i].last_heartbeat;
            if (age > heartbeat_timeout) {
                bridge->agents[i].state = SWARM_AGENT_STATE_DISCONNECTED;
                bridge->stats.heartbeats_missed++;
            }
        }
    }

    /* Check consensus timeout */
    if (bridge->consensus_in_progress && bridge->active_consensus) {
        /* Would check deadline_ms here if tracking start time */
    }

    /* Update base */
    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    (void)dt_ms;
    return 0;
}

/*=============================================================================
 * STATISTICS AND DEBUG
 *============================================================================*/

int swarm_bio_bridge_get_stats(
    const swarm_bio_bridge_t* bridge,
    swarm_bio_bridge_stats_t* out_stats
) {
    if (!bridge || !out_stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->base.mutex);
    *out_stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

void swarm_bio_bridge_reset_stats(swarm_bio_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(swarm_bio_bridge_stats_t));
    bridge->stats.active_agents = bridge->agent_count;
    nimcp_mutex_unlock(bridge->base.mutex);
}

const char* swarm_bio_msg_type_name(swarm_bio_msg_type_t msg_type) {
    switch (msg_type) {
        case SWARM_MSG_AGENT_STATE: return "AGENT_STATE";
        case SWARM_MSG_AGENT_HEARTBEAT: return "AGENT_HEARTBEAT";
        case SWARM_MSG_AGENT_CAPABILITY: return "AGENT_CAPABILITY";
        case SWARM_MSG_AGENT_JOINED: return "AGENT_JOINED";
        case SWARM_MSG_AGENT_LEFT: return "AGENT_LEFT";
        case SWARM_MSG_AGENT_POSITION: return "AGENT_POSITION";
        case SWARM_MSG_CONSENSUS_REQUEST: return "CONSENSUS_REQUEST";
        case SWARM_MSG_CONSENSUS_VOTE: return "CONSENSUS_VOTE";
        case SWARM_MSG_CONSENSUS_RESULT: return "CONSENSUS_RESULT";
        case SWARM_MSG_CONSENSUS_ABORT: return "CONSENSUS_ABORT";
        case SWARM_MSG_CONSENSUS_QUORUM_CHECK: return "CONSENSUS_QUORUM_CHECK";
        case SWARM_MSG_PHEROMONE: return "PHEROMONE";
        case SWARM_MSG_PHEROMONE_GRADIENT: return "PHEROMONE_GRADIENT";
        case SWARM_MSG_PHEROMONE_DECAY: return "PHEROMONE_DECAY";
        case SWARM_MSG_STIGMERGY_MARK: return "STIGMERGY_MARK";
        case SWARM_MSG_STIGMERGY_QUERY: return "STIGMERGY_QUERY";
        case SWARM_MSG_COORDINATION_REQUEST: return "COORDINATION_REQUEST";
        case SWARM_MSG_COORDINATION_ACK: return "COORDINATION_ACK";
        case SWARM_MSG_FORMATION_UPDATE: return "FORMATION_UPDATE";
        case SWARM_MSG_LEADER_ANNOUNCE: return "LEADER_ANNOUNCE";
        case SWARM_MSG_TASK_DELEGATION: return "TASK_DELEGATION";
        default: return "UNKNOWN";
    }
}

const char* swarm_consensus_topic_name(swarm_consensus_topic_t topic) {
    switch (topic) {
        case SWARM_CONSENSUS_FORMATION: return "FORMATION";
        case SWARM_CONSENSUS_TARGET: return "TARGET";
        case SWARM_CONSENSUS_RETREAT: return "RETREAT";
        case SWARM_CONSENSUS_RESOURCE: return "RESOURCE";
        case SWARM_CONSENSUS_LEADER: return "LEADER";
        case SWARM_CONSENSUS_TASK: return "TASK";
        case SWARM_CONSENSUS_EMERGENCY: return "EMERGENCY";
        case SWARM_CONSENSUS_CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

const char* swarm_bio_pheromone_type_name(swarm_bio_pheromone_type_t type) {
    switch (type) {
        case SWARM_PHEROMONE_PATH: return "PATH";
        case SWARM_PHEROMONE_RESOURCE: return "RESOURCE";
        case SWARM_PHEROMONE_DANGER: return "DANGER";
        case SWARM_PHEROMONE_RALLY: return "RALLY";
        case SWARM_PHEROMONE_TERRITORY: return "TERRITORY";
        case SWARM_PHEROMONE_SIGNAL: return "SIGNAL";
        default: return "UNKNOWN";
    }
}

const char* swarm_agent_state_name(swarm_agent_state_t state) {
    switch (state) {
        case SWARM_AGENT_STATE_IDLE: return "IDLE";
        case SWARM_AGENT_STATE_ACTIVE: return "ACTIVE";
        case SWARM_AGENT_STATE_BUSY: return "BUSY";
        case SWARM_AGENT_STATE_COORDINATING: return "COORDINATING";
        case SWARM_AGENT_STATE_WAITING: return "WAITING";
        case SWARM_AGENT_STATE_ERROR: return "ERROR";
        case SWARM_AGENT_STATE_DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}
