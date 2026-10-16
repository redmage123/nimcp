/**
 * @file nimcp_mesh_bio_integration.c
 * @brief Bio-Async Router to Mesh Network Integration Implementation
 *
 * WHAT: Wires the bio-async router to use mesh network for message routing
 * WHY:  Enable consensus-based routing for bio-router messages through mesh channels
 * HOW:  Hook into bio_router_send, translate to mesh transactions, route via channels
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_bio_integration.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_bio_bridge.h"
#include "mesh/nimcp_mesh_channel.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define BIO_INTEGRATION_MAGIC 0x42494E54  /* "BINT" */
#define MAX_CATEGORY_MAPPINGS 16
#define MAX_PRIORITY_POLICIES 8

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Pending callback entry
 */
typedef struct pending_callback {
    void* original_msg;               /**< Copy of original message */
    size_t msg_size;                  /**< Message size */
    mesh_tx_id_t tx_id;               /**< Associated transaction ID */
    mesh_bio_commit_callback_t callback;  /**< User callback */
    void* ctx;                        /**< Callback context */
    uint64_t submit_time_ns;          /**< When submitted */
    bool completed;                   /**< Transaction completed */
    mesh_result_t result;             /**< Transaction result */
} pending_callback_t;

/**
 * @brief Category to channel mapping
 */
typedef struct category_mapping {
    uint32_t bio_category;
    mesh_channel_id_t channel_id;
    bool active;
} category_mapping_t;

/**
 * @brief Internal integration structure
 */
struct mesh_bio_integration {
    uint32_t magic;
    mesh_bio_integration_config_t config;

    /* Mesh components */
    mesh_bootstrap_t* bootstrap;
    mesh_integration_t* mesh_integration;
    mesh_bio_bridge_t* bio_bridge;

    /* Bio-router connection */
    bio_router_t router;
    bool connected;
    bool enabled;

    /* Category mappings */
    category_mapping_t category_mappings[MAX_CATEGORY_MAPPINGS];
    size_t mapping_count;

    /* Priority policies */
    mesh_bio_priority_policy_t priority_policies[MAX_PRIORITY_POLICIES];
    size_t policy_count;

    /* Custom routing decision */
    mesh_bio_route_decision_t route_decision;
    void* route_decision_ctx;

    /* Pending callbacks */
    pending_callback_t* pending_callbacks;
    size_t pending_count;
    size_t pending_capacity;

    /* Statistics */
    mesh_bio_integration_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Global State
 * ============================================================================ */

static mesh_bio_integration_t* g_bio_integration = NULL;

/* ============================================================================
 * Bio Message Header (for parsing)
 * ============================================================================ */

/**
 * @brief Common bio message header structure
 */
typedef struct bio_msg_header {
    uint32_t type;                    /**< Message type (includes category) */
    uint32_t sequence_id;             /**< Sequence number */
    uint32_t source_module;           /**< Source module ID */
    uint32_t target_module;           /**< Target module ID (0 = broadcast) */
    uint64_t timestamp_us;            /**< Timestamp */
    uint32_t channel;                 /**< Recommended channel for response */
    uint32_t payload_size;            /**< Payload size */
    uint32_t flags;                   /**< Message flags */
} bio_msg_header_t;

/* Message flag for urgent */
#define BIO_MSG_FLAG_URGENT (1 << 0)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get category from bio message type
 */
static uint32_t get_bio_category(uint32_t msg_type) {
    return msg_type & 0xFF00;
}

/**
 * @brief Initialize default category mappings
 */
static void init_default_category_mappings(mesh_bio_integration_t* integration) {
    integration->category_mappings[0] = (category_mapping_t){
        .bio_category = MESH_BIO_CAT_NEURAL,
        .channel_id = MESH_CHANNEL_SUBCORTICAL,
        .active = true
    };
    integration->category_mappings[1] = (category_mapping_t){
        .bio_category = MESH_BIO_CAT_PLASTICITY,
        .channel_id = MESH_CHANNEL_SUBCORTICAL,
        .active = true
    };
    integration->category_mappings[2] = (category_mapping_t){
        .bio_category = MESH_BIO_CAT_NEUROMOD,
        .channel_id = MESH_CHANNEL_SUBCORTICAL,
        .active = true
    };
    integration->category_mappings[3] = (category_mapping_t){
        .bio_category = MESH_BIO_CAT_PERCEPTION,
        .channel_id = MESH_CHANNEL_RIGHT_HEMISPHERE,
        .active = true
    };
    integration->category_mappings[4] = (category_mapping_t){
        .bio_category = MESH_BIO_CAT_COGNITIVE,
        .channel_id = MESH_CHANNEL_LEFT_HEMISPHERE,
        .active = true
    };
    integration->category_mappings[5] = (category_mapping_t){
        .bio_category = MESH_BIO_CAT_MOTOR,
        .channel_id = MESH_CHANNEL_SUBCORTICAL,
        .active = true
    };
    integration->category_mappings[6] = (category_mapping_t){
        .bio_category = MESH_BIO_CAT_SECURITY,
        .channel_id = MESH_CHANNEL_SYSTEM,
        .active = true
    };
    integration->category_mappings[7] = (category_mapping_t){
        .bio_category = MESH_BIO_CAT_SYSTEM,
        .channel_id = MESH_CHANNEL_SYSTEM,
        .active = true
    };
    integration->category_mappings[8] = (category_mapping_t){
        .bio_category = MESH_BIO_CAT_GLIAL,
        .channel_id = MESH_CHANNEL_SYSTEM,
        .active = true
    };
    integration->category_mappings[9] = (category_mapping_t){
        .bio_category = MESH_BIO_CAT_MEMORY,
        .channel_id = MESH_CHANNEL_SUBCORTICAL,
        .active = true
    };

    integration->mapping_count = 10;
}

/**
 * @brief Initialize default priority policies
 */
static void init_default_priority_policies(mesh_bio_integration_t* integration) {
    integration->priority_policies[0] = (mesh_bio_priority_policy_t){
        .priority = MESH_BIO_PRIORITY_LOW,
        .policy_name = MESH_POLICY_PLASTICITY,
        .min_endorsers = 1,
        .timeout_factor = 2.0f
    };
    integration->priority_policies[1] = (mesh_bio_priority_policy_t){
        .priority = MESH_BIO_PRIORITY_NORMAL,
        .policy_name = MESH_POLICY_COGNITIVE,
        .min_endorsers = 2,
        .timeout_factor = 1.0f
    };
    integration->priority_policies[2] = (mesh_bio_priority_policy_t){
        .priority = MESH_BIO_PRIORITY_HIGH,
        .policy_name = MESH_POLICY_MOTOR_COMMAND,
        .min_endorsers = 2,
        .timeout_factor = 0.5f
    };
    integration->priority_policies[3] = (mesh_bio_priority_policy_t){
        .priority = MESH_BIO_PRIORITY_URGENT,
        .policy_name = MESH_POLICY_EMERGENCY,
        .min_endorsers = 1,
        .timeout_factor = 0.25f
    };
    integration->priority_policies[4] = (mesh_bio_priority_policy_t){
        .priority = MESH_BIO_PRIORITY_CRITICAL,
        .policy_name = MESH_POLICY_EMERGENCY,
        .min_endorsers = 1,
        .timeout_factor = 0.1f
    };

    integration->policy_count = 5;
}

/**
 * @brief Get channel for bio category
 */
static mesh_channel_id_t get_channel_for_category(
    mesh_bio_integration_t* integration,
    uint32_t bio_category
) {
    for (size_t i = 0; i < integration->mapping_count; i++) {
        if (integration->category_mappings[i].active &&
            integration->category_mappings[i].bio_category == bio_category) {
            return integration->category_mappings[i].channel_id;
        }
    }
    return MESH_CHANNEL_SYSTEM;  /* Default fallback */
}

/**
 * @brief Get priority policy
 */
static const mesh_bio_priority_policy_t* get_priority_policy(
    mesh_bio_integration_t* integration,
    mesh_bio_priority_t priority
) {
    for (size_t i = 0; i < integration->policy_count; i++) {
        if (integration->priority_policies[i].priority == priority) {
            return &integration->priority_policies[i];
        }
    }
    /* Policy not set is normal default state, not an error */
    return NULL;
}

/**
 * @brief Determine priority from bio message
 */
static mesh_bio_priority_t determine_priority(
    mesh_bio_integration_t* integration,
    const bio_msg_header_t* header
) {
    if (integration->config.honor_urgent_flag &&
        (header->flags & BIO_MSG_FLAG_URGENT)) {
        return MESH_BIO_PRIORITY_URGENT;
    }

    /* Category-based priority could be added here */
    return integration->config.default_priority;
}

/**
 * @brief Update per-category statistics
 */
static void update_category_stats(
    mesh_bio_integration_stats_t* stats,
    uint32_t bio_category
) {
    switch (bio_category) {
        case MESH_BIO_CAT_NEURAL:     stats->category_neural++; break;
        case MESH_BIO_CAT_PLASTICITY: stats->category_plasticity++; break;
        case MESH_BIO_CAT_NEUROMOD:   stats->category_neuromod++; break;
        case MESH_BIO_CAT_PERCEPTION: stats->category_perception++; break;
        case MESH_BIO_CAT_COGNITIVE:  stats->category_cognitive++; break;
        case MESH_BIO_CAT_MOTOR:      stats->category_motor++; break;
        case MESH_BIO_CAT_SECURITY:   stats->category_security++; break;
        case MESH_BIO_CAT_SYSTEM:     stats->category_system++; break;
    }
}

/**
 * @brief Add pending callback
 */
static nimcp_error_t add_pending_callback(
    mesh_bio_integration_t* integration,
    const void* msg,
    size_t msg_size,
    const mesh_tx_id_t* tx_id,
    mesh_bio_commit_callback_t callback,
    void* ctx
) {
    if (integration->pending_count >= integration->pending_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_bio_integration: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    pending_callback_t* entry = &integration->pending_callbacks[integration->pending_count];

    /* Copy message */
    entry->original_msg = nimcp_malloc(msg_size);
    if (!entry->original_msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY, "mesh_bio_integration: error condition");
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }
    memcpy(entry->original_msg, msg, msg_size);

    entry->msg_size = msg_size;
    entry->tx_id = *tx_id;
    entry->callback = callback;
    entry->ctx = ctx;
    entry->submit_time_ns = nimcp_time_now_ns();
    entry->completed = false;
    memset(&entry->result, 0, sizeof(entry->result));

    integration->pending_count++;
    integration->stats.callbacks_pending++;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_bio_integration_default_config(
    mesh_bio_integration_config_t* config
) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));

    config->enable_mesh_routing = true;
    config->fallback_to_direct = true;
    config->enable_bidirectional = true;

    config->route_broadcasts = false;  /* Broadcasts still use direct routing */
    config->route_kg_dispatch = false; /* KG dispatch uses KG, not mesh */
    config->min_category_for_mesh = 0; /* All categories eligible */

    config->honor_urgent_flag = true;
    config->default_priority = MESH_BIO_PRIORITY_NORMAL;

    config->default_timeout_ms = MESH_BIO_DEFAULT_TIMEOUT_MS;
    config->urgent_timeout_ms = 25.0f;

    config->async_callbacks = true;
    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_bio_integration_t* mesh_bio_integration_create(
    mesh_bootstrap_t* bootstrap,
    const mesh_bio_integration_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create bio integration without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_bio_integration_create: bootstrap is NULL");
        return NULL;
    }

    mesh_bio_integration_config_t default_config;
    if (!config) {
        mesh_bio_integration_default_config(&default_config);
        config = &default_config;
    }

    mesh_bio_integration_t* integration = nimcp_calloc(1, sizeof(*integration));
    if (!integration) {
        LOG_ERROR("Failed to allocate bio integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_bio_integration_create: integration is NULL");
        return NULL;
    }

    integration->magic = BIO_INTEGRATION_MAGIC;
    integration->config = *config;
    integration->bootstrap = bootstrap;
    integration->mesh_integration = mesh_bootstrap_get_integration(bootstrap);
    integration->bio_bridge = mesh_bootstrap_get_bio_bridge(bootstrap);
    integration->enabled = config->enable_mesh_routing;

    /* Create mutex */
    mutex_attr_t attr = {0};
    integration->mutex = nimcp_mutex_create(&attr);
    if (!integration->mutex) {
        LOG_ERROR("Failed to create integration mutex");
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_bio_integration_create: integration->mutex is NULL");
        return NULL;
    }

    /* Initialize category mappings */
    init_default_category_mappings(integration);

    /* Initialize priority policies */
    init_default_priority_policies(integration);

    /* Allocate pending callbacks */
    integration->pending_capacity = MESH_BIO_MAX_PENDING_CALLBACKS;
    integration->pending_callbacks = nimcp_calloc(
        integration->pending_capacity,
        sizeof(pending_callback_t)
    );
    if (!integration->pending_callbacks) {
        LOG_ERROR("Failed to allocate pending callbacks");
        nimcp_mutex_destroy(integration->mutex);
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_bio_integration_create: integration->pending_callbacks is NULL");
        return NULL;
    }

    LOG_DEBUG("Bio-mesh integration created");
    return integration;
}

void mesh_bio_integration_destroy(mesh_bio_integration_t* integration) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) return;

    nimcp_mutex_lock(integration->mutex);

    /* Disconnect from router if connected */
    if (integration->connected) {
        integration->connected = false;
        integration->router = NULL;
    }

    /* Cleanup pending callbacks */
    for (size_t i = 0; i < integration->pending_count; i++) {
        if (integration->pending_callbacks[i].original_msg) {
            nimcp_free(integration->pending_callbacks[i].original_msg);
        }
    }
    nimcp_free(integration->pending_callbacks);

    nimcp_mutex_unlock(integration->mutex);
    nimcp_mutex_destroy(integration->mutex);

    /* Clear global if this is it */
    if (g_bio_integration == integration) {
        g_bio_integration = NULL;
    }

    integration->magic = 0;
    nimcp_free(integration);

    LOG_DEBUG("Bio-mesh integration destroyed");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

nimcp_error_t mesh_bio_integration_connect(
    mesh_bio_integration_t* integration,
    bio_router_t router
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    if (integration->connected) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_WARN("Bio integration already connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_ALREADY_EXISTS, "mesh_bio_integration: error condition");
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    integration->router = router;
    integration->connected = true;

    /* Connect bio-bridge to router if we have one */
    if (integration->bio_bridge && router) {
        mesh_bio_bridge_connect_router(integration->bio_bridge, &router);
    }

    nimcp_mutex_unlock(integration->mutex);

    LOG_INFO("Bio-mesh integration connected to router");
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_bio_integration_disconnect(
    mesh_bio_integration_t* integration
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    if (!integration->connected) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_SUCCESS;
    }

    /* Disconnect bio-bridge */
    if (integration->bio_bridge) {
        mesh_bio_bridge_disconnect_router(integration->bio_bridge);
    }

    integration->router = NULL;
    integration->connected = false;

    nimcp_mutex_unlock(integration->mutex);

    LOG_INFO("Bio-mesh integration disconnected from router");
    return NIMCP_SUCCESS;
}

bool mesh_bio_integration_is_connected(
    const mesh_bio_integration_t* integration
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        return false;
    }
    nimcp_mutex_lock(((mesh_bio_integration_t*)integration)->mutex);
    bool val = integration->connected;
    nimcp_mutex_unlock(((mesh_bio_integration_t*)integration)->mutex);
    return val;
}

bool mesh_bio_integration_mesh_available(
    const mesh_bio_integration_t* integration
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration_mesh_available: integration is NULL");
        return false;
    }
    nimcp_mutex_lock(((mesh_bio_integration_t*)integration)->mutex);
    bool val = integration->mesh_integration != NULL && integration->enabled;
    nimcp_mutex_unlock(((mesh_bio_integration_t*)integration)->mutex);
    return val;
}

/* ============================================================================
 * Routing API
 * ============================================================================ */

nimcp_error_t mesh_bio_integration_route_message(
    mesh_bio_integration_t* integration,
    const void* msg,
    size_t msg_size,
    mesh_bio_commit_callback_t callback,
    void* ctx
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!msg || msg_size < sizeof(bio_msg_header_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    integration->stats.messages_intercepted++;

    /* Check if mesh routing is available */
    if (!integration->enabled || !integration->mesh_integration) {
        integration->stats.direct_fallback++;
        nimcp_mutex_unlock(integration->mutex);

        if (integration->config.fallback_to_direct) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_bio_integration: error condition");
            return NIMCP_ERROR_NOT_FOUND;  /* Signal to use direct routing */
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_bio_integration: not initialized");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    const bio_msg_header_t* header = (const bio_msg_header_t*)msg;

    /* Check custom routing decision */
    if (integration->route_decision) {
        nimcp_mutex_unlock(integration->mutex);
        bool should_route = integration->route_decision(
            msg, msg_size, integration->route_decision_ctx);
        nimcp_mutex_lock(integration->mutex);

        if (!should_route) {
            integration->stats.direct_fallback++;
            nimcp_mutex_unlock(integration->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_bio_integration: error condition");
            return NIMCP_ERROR_NOT_FOUND;
        }
    }

    /* Check if broadcasts should be routed */
    if (header->target_module == 0 && !integration->config.route_broadcasts) {
        integration->stats.direct_fallback++;
        nimcp_mutex_unlock(integration->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_bio_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Determine priority */
    mesh_bio_priority_t priority = determine_priority(integration, header);

    /* Update priority stats */
    switch (priority) {
        case MESH_BIO_PRIORITY_LOW:
            integration->stats.low_messages++;
            break;
        case MESH_BIO_PRIORITY_URGENT:
        case MESH_BIO_PRIORITY_CRITICAL:
            integration->stats.urgent_messages++;
            break;
        default:
            integration->stats.normal_messages++;
            break;
    }

    /* Get channel for category */
    uint32_t category = get_bio_category(header->type);
    mesh_channel_id_t channel = get_channel_for_category(integration, category);

    /* Update category stats */
    update_category_stats(&integration->stats, category);

    /* Route through bio-bridge if available */
    nimcp_error_t result = NIMCP_SUCCESS;

    if (integration->bio_bridge) {
        result = mesh_bio_bridge_route_bio_message(
            integration->bio_bridge, msg, msg_size);
    } else if (integration->mesh_integration) {
        /* Direct routing through mesh integration */
        mesh_transaction_t* tx = mesh_transaction_create(
            MESH_TX_BELIEF_UPDATE,
            mesh_make_participant_id(channel, MESH_PARTICIPANT_MODULE,
                                     header->source_module),
            channel
        );

        if (tx) {
            /* Set payload */
            if (header->payload_size > 0 && msg_size > sizeof(bio_msg_header_t)) {
                tx->payload = nimcp_malloc(msg_size - sizeof(bio_msg_header_t));
                if (tx->payload) {
                    memcpy(tx->payload,
                           (const uint8_t*)msg + sizeof(bio_msg_header_t),
                           msg_size - sizeof(bio_msg_header_t));
                    tx->payload_size = msg_size - sizeof(bio_msg_header_t);
                }
            }

            /* Submit transaction */
            result = mesh_integration_submit_transaction(
                integration->mesh_integration, tx);

            if (result == NIMCP_SUCCESS && callback) {
                /* Add pending callback */
                add_pending_callback(integration, msg, msg_size,
                                   &tx->id, callback, ctx);
            }

            mesh_transaction_destroy(tx);
        } else {
            result = NIMCP_ERROR_OUT_OF_MEMORY;
        }
    }

    if (result == NIMCP_SUCCESS) {
        integration->stats.routed_through_mesh++;
        integration->stats.transactions_created++;

        if (integration->config.verbose_logging) {
            LOG_DEBUG("Routed bio msg type=0x%04X to mesh channel %u",
                     header->type, channel);
        }
    } else {
        integration->stats.routing_failures++;

        if (integration->config.fallback_to_direct) {
            integration->stats.direct_fallback++;
            nimcp_mutex_unlock(integration->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_bio_integration: error condition");
            return NIMCP_ERROR_NOT_FOUND;
        }
    }

    nimcp_mutex_unlock(integration->mutex);
    return result;
}

nimcp_error_t mesh_bio_integration_route_priority(
    mesh_bio_integration_t* integration,
    const void* msg,
    size_t msg_size,
    mesh_bio_priority_t priority,
    mesh_bio_commit_callback_t callback,
    void* ctx
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Get policy for priority */
    const mesh_bio_priority_policy_t* policy =
        get_priority_policy(integration, priority);

    if (!policy) {
        return mesh_bio_integration_route_message(
            integration, msg, msg_size, callback, ctx);
    }

    /* Apply policy settings could be done here */
    /* For now, just route normally */
    return mesh_bio_integration_route_message(
        integration, msg, msg_size, callback, ctx);
}

nimcp_error_t mesh_bio_integration_route_to_channel(
    mesh_bio_integration_t* integration,
    const void* msg,
    size_t msg_size,
    mesh_channel_id_t channel
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!msg || msg_size < sizeof(bio_msg_header_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    if (!integration->enabled || !integration->mesh_integration) {
        nimcp_mutex_unlock(integration->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_bio_integration: not initialized");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    const bio_msg_header_t* header = (const bio_msg_header_t*)msg;

    /* Create transaction for specific channel */
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_BELIEF_UPDATE,
        mesh_make_participant_id(channel, MESH_PARTICIPANT_MODULE,
                                 header->source_module),
        channel
    );

    nimcp_error_t result = NIMCP_ERROR_OUT_OF_MEMORY;

    if (tx) {
        if (header->payload_size > 0 && msg_size > sizeof(bio_msg_header_t)) {
            tx->payload = nimcp_malloc(msg_size - sizeof(bio_msg_header_t));
            if (tx->payload) {
                memcpy(tx->payload,
                       (const uint8_t*)msg + sizeof(bio_msg_header_t),
                       msg_size - sizeof(bio_msg_header_t));
                tx->payload_size = msg_size - sizeof(bio_msg_header_t);
            }
        }

        result = mesh_integration_submit_transaction(
            integration->mesh_integration, tx);

        mesh_transaction_destroy(tx);
    }

    if (result == NIMCP_SUCCESS) {
        integration->stats.routed_through_mesh++;
    }

    nimcp_mutex_unlock(integration->mutex);
    return result;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

nimcp_error_t mesh_bio_integration_set_route_decision(
    mesh_bio_integration_t* integration,
    mesh_bio_route_decision_t callback,
    void* ctx
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->route_decision = callback;
    integration->route_decision_ctx = ctx;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_bio_integration_set_priority_policy(
    mesh_bio_integration_t* integration,
    const mesh_bio_priority_policy_t* policy
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!policy) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(integration->mutex);

    /* Find existing or add new */
    for (size_t i = 0; i < integration->policy_count; i++) {
        if (integration->priority_policies[i].priority == policy->priority) {
            integration->priority_policies[i] = *policy;
            nimcp_mutex_unlock(integration->mutex);
            return NIMCP_SUCCESS;
        }
    }

    if (integration->policy_count >= MAX_PRIORITY_POLICIES) {
        nimcp_mutex_unlock(integration->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_bio_integration: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    integration->priority_policies[integration->policy_count++] = *policy;

    nimcp_mutex_unlock(integration->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_bio_integration_set_category_channel(
    mesh_bio_integration_t* integration,
    uint32_t bio_category,
    mesh_channel_id_t channel_id
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Find existing or add new */
    for (size_t i = 0; i < integration->mapping_count; i++) {
        if (integration->category_mappings[i].bio_category == bio_category) {
            integration->category_mappings[i].channel_id = channel_id;
            integration->category_mappings[i].active = true;
            nimcp_mutex_unlock(integration->mutex);
            return NIMCP_SUCCESS;
        }
    }

    if (integration->mapping_count >= MAX_CATEGORY_MAPPINGS) {
        nimcp_mutex_unlock(integration->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_bio_integration: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    integration->category_mappings[integration->mapping_count++] =
        (category_mapping_t){
            .bio_category = bio_category,
            .channel_id = channel_id,
            .active = true
        };

    nimcp_mutex_unlock(integration->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_bio_integration_set_enabled(
    mesh_bio_integration_t* integration,
    bool enabled
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->enabled = enabled;
    nimcp_mutex_unlock(integration->mutex);

    LOG_INFO("Bio-mesh integration %s", enabled ? "enabled" : "disabled");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Callback Management API
 * ============================================================================ */

uint32_t mesh_bio_integration_process_callbacks(
    mesh_bio_integration_t* integration,
    uint32_t max_callbacks
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) return 0;

    nimcp_mutex_lock(integration->mutex);

    uint32_t processed = 0;
    uint32_t limit = (max_callbacks == 0) ?
                     (uint32_t)integration->pending_count : max_callbacks;

    for (size_t i = 0; i < integration->pending_count && processed < limit; ) {
        pending_callback_t* entry = &integration->pending_callbacks[i];

        if (entry->completed && entry->callback) {
            /* Invoke callback */
            nimcp_mutex_unlock(integration->mutex);

            entry->callback(entry->original_msg, entry->msg_size,
                          &entry->result, entry->ctx);

            nimcp_mutex_lock(integration->mutex);

            /* Cleanup and remove entry */
            nimcp_free(entry->original_msg);

            /* Shift remaining entries */
            if (i < integration->pending_count - 1) {
                memmove(&integration->pending_callbacks[i],
                       &integration->pending_callbacks[i + 1],
                       (integration->pending_count - i - 1) *
                       sizeof(pending_callback_t));
            }
            integration->pending_count--;
            integration->stats.callbacks_completed++;
            integration->stats.callbacks_pending--;
            processed++;
        } else {
            i++;
        }
    }

    nimcp_mutex_unlock(integration->mutex);
    return processed;
}

uint32_t mesh_bio_integration_pending_callbacks(
    const mesh_bio_integration_t* integration
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) return 0;
    nimcp_mutex_lock(((mesh_bio_integration_t*)integration)->mutex);
    uint32_t val = (uint32_t)integration->pending_count;
    nimcp_mutex_unlock(((mesh_bio_integration_t*)integration)->mutex);
    return val;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_bio_integration_get_stats(
    const mesh_bio_integration_t* integration,
    mesh_bio_integration_stats_t* stats
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_bio_integration_t*)integration)->mutex);
    *stats = integration->stats;
    nimcp_mutex_unlock(((mesh_bio_integration_t*)integration)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_bio_integration_reset_stats(
    mesh_bio_integration_t* integration
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    memset(&integration->stats, 0, sizeof(integration->stats));
    integration->stats.callbacks_pending = integration->pending_count;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Hook Implementation
 * ============================================================================ */

/**
 * @brief Internal routing hook called by bio-router
 */
static nimcp_error_t mesh_bio_routing_hook_impl(
    const void* msg,
    size_t msg_size,
    void* ctx
) {
    mesh_bio_integration_t* integration = (mesh_bio_integration_t*)ctx;

    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_bio_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;  /* Continue to next handler */
    }

    if (!integration->enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_bio_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    return mesh_bio_integration_route_message(integration, msg, msg_size,
                                              NULL, NULL);
}

mesh_bio_routing_hook_t mesh_bio_integration_get_hook(
    mesh_bio_integration_t* integration
) {
    if (!integration || integration->magic != BIO_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bio_integration_get_hook: integration is NULL");
        return NULL;
    }
    return mesh_bio_routing_hook_impl;
}

/* ============================================================================
 * Global Access API
 * ============================================================================ */

nimcp_error_t mesh_bio_integration_set_global(
    mesh_bio_integration_t* integration
) {
    g_bio_integration = integration;
    return NIMCP_SUCCESS;
}

mesh_bio_integration_t* mesh_bio_integration_get_global(void) {
    return g_bio_integration;
}

bool mesh_bio_integration_global_available(void) {
    return g_bio_integration != NULL &&
           g_bio_integration->magic == BIO_INTEGRATION_MAGIC &&
           g_bio_integration->enabled &&
           g_bio_integration->mesh_integration != NULL;
}
