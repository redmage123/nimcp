/**
 * @file nimcp_mesh_bio_bridge.c
 * @brief Bio-Async to Mesh Transaction Bridge Implementation
 *
 * WHAT: Bridges bio-async router messages to mesh network transactions
 * WHY:  Enable full integration between bio-router messaging and mesh consensus
 * HOW:  Message category to pattern mapping, channel routing, bidirectional translation
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_bio_bridge.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_integration.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

#define BIO_BRIDGE_MAGIC 0x42494F42  /* "BIOB" */
#define MAX_CHANNEL_MAPPINGS 32
#define MAX_PENDING_TRANSLATIONS 256

/**
 * @brief Channel mapping entry
 */
typedef struct channel_mapping {
    uint32_t bio_category;
    mesh_channel_id_t channel_id;
    bool active;
} channel_mapping_t;

/**
 * @brief Pending translation entry
 */
typedef struct pending_translation {
    mesh_transaction_t* tx;
    uint64_t submit_time_ns;
    bool completed;
} pending_translation_t;

/**
 * @brief Internal bio bridge structure
 */
struct mesh_bio_bridge {
    bridge_base_t base;
    uint32_t magic;
    mesh_bio_bridge_config_t config;

    /* Bootstrap reference */
    mesh_bootstrap_t* bootstrap;
    mesh_integration_t* integration;

    /* Bio-router connection */
    bio_router_t* router;
    bool connected;

    /* Channel mappings */
    channel_mapping_t channel_mappings[MAX_CHANNEL_MAPPINGS];
    size_t mapping_count;

    /* Mesh-to-bio callback */
    mesh_to_bio_callback_t mesh_callback;
    void* mesh_callback_ctx;

    /* Pending translations */
    pending_translation_t pending[MAX_PENDING_TRANSLATIONS];
    size_t pending_count;

    /* Statistics */
    mesh_bio_bridge_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Bio Message Header (for parsing)
 * ============================================================================ */

/**
 * @brief Common bio message header
 *
 * This matches the structure at the start of bio-router messages
 */
typedef struct bio_msg_header {
    uint32_t msg_type;                    /**< Message type (includes category) */
    uint32_t source_module;               /**< Source module ID */
    uint32_t target_module;               /**< Target module ID (0 = broadcast) */
    uint32_t flags;                       /**< Message flags */
    uint64_t timestamp_ns;                /**< Message timestamp */
    uint32_t payload_size;                /**< Payload size after header */
    uint32_t sequence;                    /**< Sequence number */
} bio_msg_header_t;

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_bio_bridge_default_config(mesh_bio_bridge_config_t* config) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));

    config->enable_pattern_routing = true;
    config->enable_channel_mapping = true;
    config->bidirectional = true;

    config->default_timeout_ms = 100.0f;
    config->max_pending_translations = MAX_PENDING_TRANSLATIONS;

    config->pattern_magnitude_threshold = 0.1f;
    config->normalize_patterns = true;

    /* Default channel mappings */
    config->neural_channel = MESH_CHANNEL_SUBCORTICAL;
    config->cognitive_channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    config->motor_channel = MESH_CHANNEL_SUBCORTICAL;
    config->security_channel = MESH_CHANNEL_SYSTEM;

    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get category from message type
 */
static uint32_t get_bio_category(uint32_t msg_type) {
    return msg_type & 0xFF00;  /* Category is in bits 8-15 */
}

/**
 * @brief Get category index for pattern range lookup
 */
static int get_category_index(uint32_t bio_category) {
    switch (bio_category) {
        case MESH_BIO_CAT_NEURAL:     return 0;
        case MESH_BIO_CAT_PLASTICITY: return 1;
        case MESH_BIO_CAT_NEUROMOD:   return 2;
        case MESH_BIO_CAT_PERCEPTION: return 3;
        case MESH_BIO_CAT_COGNITIVE:  return 4;
        case MESH_BIO_CAT_MOTOR:      return 5;
        case MESH_BIO_CAT_SECURITY:   return 6;
        case MESH_BIO_CAT_SYSTEM:     return 7;
        default:                      return -1;
    }
}

/**
 * @brief Initialize default channel mappings
 */
static void init_default_mappings(mesh_bio_bridge_t* bridge) {
    const mesh_bio_bridge_config_t* cfg = &bridge->config;

    bridge->channel_mappings[0] = (channel_mapping_t){
        .bio_category = MESH_BIO_CAT_NEURAL,
        .channel_id = cfg->neural_channel,
        .active = true
    };
    bridge->channel_mappings[1] = (channel_mapping_t){
        .bio_category = MESH_BIO_CAT_PLASTICITY,
        .channel_id = MESH_CHANNEL_SUBCORTICAL,
        .active = true
    };
    bridge->channel_mappings[2] = (channel_mapping_t){
        .bio_category = MESH_BIO_CAT_NEUROMOD,
        .channel_id = MESH_CHANNEL_SUBCORTICAL,
        .active = true
    };
    bridge->channel_mappings[3] = (channel_mapping_t){
        .bio_category = MESH_BIO_CAT_PERCEPTION,
        .channel_id = MESH_CHANNEL_RIGHT_HEMISPHERE,
        .active = true
    };
    bridge->channel_mappings[4] = (channel_mapping_t){
        .bio_category = MESH_BIO_CAT_COGNITIVE,
        .channel_id = cfg->cognitive_channel,
        .active = true
    };
    bridge->channel_mappings[5] = (channel_mapping_t){
        .bio_category = MESH_BIO_CAT_MOTOR,
        .channel_id = cfg->motor_channel,
        .active = true
    };
    bridge->channel_mappings[6] = (channel_mapping_t){
        .bio_category = MESH_BIO_CAT_SECURITY,
        .channel_id = cfg->security_channel,
        .active = true
    };
    bridge->channel_mappings[7] = (channel_mapping_t){
        .bio_category = MESH_BIO_CAT_SYSTEM,
        .channel_id = MESH_CHANNEL_SYSTEM,
        .active = true
    };
    bridge->channel_mappings[8] = (channel_mapping_t){
        .bio_category = MESH_BIO_CAT_GLIAL,
        .channel_id = MESH_CHANNEL_SYSTEM,
        .active = true
    };
    bridge->channel_mappings[9] = (channel_mapping_t){
        .bio_category = MESH_BIO_CAT_MEMORY,
        .channel_id = MESH_CHANNEL_SUBCORTICAL,
        .active = true
    };

    bridge->mapping_count = 10;
}

/**
 * @brief Update per-category statistics
 */
static void update_category_stats(
    mesh_bio_bridge_stats_t* stats,
    uint32_t bio_category
) {
    switch (bio_category) {
        case MESH_BIO_CAT_NEURAL:     stats->neural_translations++; break;
        case MESH_BIO_CAT_PLASTICITY: stats->plasticity_translations++; break;
        case MESH_BIO_CAT_NEUROMOD:   stats->neuromod_translations++; break;
        case MESH_BIO_CAT_PERCEPTION: stats->perception_translations++; break;
        case MESH_BIO_CAT_COGNITIVE:  stats->cognitive_translations++; break;
        case MESH_BIO_CAT_MOTOR:      stats->motor_translations++; break;
        case MESH_BIO_CAT_SECURITY:   stats->security_translations++; break;
        case MESH_BIO_CAT_SYSTEM:     stats->system_translations++; break;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_bio_bridge_t* mesh_bio_bridge_create(
    mesh_bootstrap_t* bootstrap,
    const mesh_bio_bridge_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create bio bridge without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_bio_bridge_create: bootstrap is NULL");
        return NULL;
    }

    mesh_bio_bridge_config_t default_config;
    if (!config) {
        mesh_bio_bridge_default_config(&default_config);
        config = &default_config;
    }

    mesh_bio_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        LOG_ERROR("Failed to allocate bio bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_bio_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->magic = BIO_BRIDGE_MAGIC;
    bridge->config = *config;
    bridge->bootstrap = bootstrap;
    bridge->integration = mesh_bootstrap_get_integration(bootstrap);

    /* Create mutex */
    mutex_attr_t attr = {0};
    bridge->mutex = nimcp_mutex_create(&attr);
    if (!bridge->mutex) {
        LOG_ERROR("Failed to create bridge mutex");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_bio_bridge_create: bridge->mutex is NULL");
        return NULL;
    }

    /* Initialize channel mappings */
    init_default_mappings(bridge);

    bridge_base_init(&bridge->base, 0, "mesh_bio_bridge");

    LOG_DEBUG("Bio-mesh bridge created");
    return bridge;
}

void mesh_bio_bridge_destroy(mesh_bio_bridge_t* bridge) {
    if (!bridge || bridge->magic != BIO_BRIDGE_MAGIC) return;

    nimcp_mutex_lock(bridge->mutex);

    /* Cleanup pending translations */
    for (size_t i = 0; i < bridge->pending_count; i++) {
        if (bridge->pending[i].tx) {
            /* Transaction cleanup would go here */
        }
    }

    /* Disconnect from router if connected */
    if (bridge->connected && bridge->router) {
        /* Would uninstall hooks here */
        bridge->connected = false;
    }

    nimcp_mutex_unlock(bridge->mutex);
    nimcp_mutex_destroy(bridge->mutex);
    bridge_base_cleanup(&bridge->base);

    bridge->magic = 0;
    nimcp_free(bridge);

    LOG_DEBUG("Bio-mesh bridge destroyed");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

nimcp_error_t mesh_bio_bridge_connect_router(
    mesh_bio_bridge_t* bridge,
    bio_router_t* router
) {
    if (!bridge || bridge->magic != BIO_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!router) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->mutex);

    if (bridge->connected) {
        nimcp_mutex_unlock(bridge->mutex);
        LOG_WARN("Bio bridge already connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_ALREADY_EXISTS, "mesh_bio_bridge: error condition");
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    bridge->router = router;
    bridge->connected = true;

    /* In production, would register message interception hooks here:
     * bio_router_register_handler(router, 0, bridge_message_handler, bridge);
     */

    nimcp_mutex_unlock(bridge->mutex);

    LOG_INFO("Bio bridge connected to router");
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_bio_bridge_disconnect_router(mesh_bio_bridge_t* bridge) {
    if (!bridge || bridge->magic != BIO_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_bridge_disconnect_router: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->mutex);
        return NIMCP_SUCCESS;  /* Already disconnected */
    }

    /* Would unregister hooks here */
    bridge->router = NULL;
    bridge->connected = false;

    nimcp_mutex_unlock(bridge->mutex);

    LOG_INFO("Bio bridge disconnected from router");
    return NIMCP_SUCCESS;
}

bool mesh_bio_bridge_is_connected(const mesh_bio_bridge_t* bridge) {
    if (!bridge || bridge->magic != BIO_BRIDGE_MAGIC) {
        return false;
    }
    return bridge->connected;
}

/* ============================================================================
 * Pattern Extraction
 * ============================================================================ */

nimcp_error_t mesh_bio_bridge_extract_pattern(
    mesh_bio_bridge_t* bridge,
    const void* bio_msg,
    size_t msg_size,
    mesh_pattern_t* pattern_out
) {
    if (!bridge || bridge->magic != BIO_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!bio_msg || !pattern_out) return NIMCP_ERROR_NULL_POINTER;
    if (msg_size < sizeof(bio_msg_header_t)) return NIMCP_ERROR_INVALID_PARAM;

    const bio_msg_header_t* header = (const bio_msg_header_t*)bio_msg;

    /* Initialize pattern to zero */
    memset(pattern_out, 0, sizeof(*pattern_out));

    /* Get category and determine dimension range */
    uint32_t category = get_bio_category(header->msg_type);
    int cat_idx = get_category_index(category);

    if (cat_idx < 0) {
        /* Unknown category - use full pattern space */
        pattern_out->vector[0] = 1.0f;  /* Mark as unknown */
        pattern_out->magnitude = 1.0f;
        pattern_out->active_dims = 1;
        return NIMCP_SUCCESS;
    }

    const mesh_pattern_dim_range_t* range = &MESH_BIO_PATTERN_RANGES[cat_idx];
    size_t dim_count = range->end - range->start;

    /* Extract features based on message content */
    /* Sub-type (lower 8 bits) determines primary dimension */
    uint32_t sub_type = header->msg_type & 0x00FF;
    if (sub_type < dim_count) {
        pattern_out->vector[range->start + sub_type] = 1.0f;
        pattern_out->active_dims++;
    }

    /* Use source module to add secondary features */
    if (header->source_module != 0) {
        size_t mod_dim = range->start + (header->source_module % dim_count);
        pattern_out->vector[mod_dim] += 0.5f;
        pattern_out->active_dims++;
    }

    /* Extract additional features from payload if present */
    if (header->payload_size > 0 && msg_size > sizeof(bio_msg_header_t)) {
        const uint8_t* payload = (const uint8_t*)bio_msg + sizeof(bio_msg_header_t);
        size_t payload_len = msg_size - sizeof(bio_msg_header_t);
        payload_len = (payload_len < header->payload_size) ?
                      payload_len : header->payload_size;

        /* Simple feature extraction: first few bytes influence pattern */
        for (size_t i = 0; i < payload_len && i < 4; i++) {
            size_t dim = range->start + (i % dim_count);
            pattern_out->vector[dim] += (float)payload[i] / 255.0f * 0.25f;
        }
    }

    /* Calculate magnitude */
    float mag_sq = 0.0f;
    for (size_t i = range->start; i < range->end; i++) {
        mag_sq += pattern_out->vector[i] * pattern_out->vector[i];
    }
    pattern_out->magnitude = sqrtf(mag_sq);

    /* Normalize if configured */
    if (bridge->config.normalize_patterns && pattern_out->magnitude > 0.001f) {
        for (size_t i = range->start; i < range->end; i++) {
            pattern_out->vector[i] /= pattern_out->magnitude;
        }
        pattern_out->magnitude = 1.0f;
    }

    bridge->stats.pattern_extractions++;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Translation API: Bio -> Mesh
 * ============================================================================ */

nimcp_error_t mesh_bio_bridge_translate_to_mesh(
    mesh_bio_bridge_t* bridge,
    const void* bio_msg,
    size_t msg_size,
    mesh_transaction_t** tx_out
) {
    if (!bridge || bridge->magic != BIO_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!bio_msg || !tx_out) return NIMCP_ERROR_NULL_POINTER;
    if (msg_size < sizeof(bio_msg_header_t)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(bridge->mutex);

    bridge->stats.bio_messages_received++;

    const bio_msg_header_t* header = (const bio_msg_header_t*)bio_msg;
    uint32_t category = get_bio_category(header->msg_type);

    /* Determine target channel */
    mesh_channel_id_t channel = mesh_bio_bridge_get_channel(bridge, category);

    /* Create transaction - using mesh_transaction_manager_create would be better
     * but we need to create a minimal transaction for now */
    mesh_transaction_t* tx = nimcp_calloc(1, sizeof(mesh_transaction_t));
    if (!tx) {
        bridge->stats.translation_failures++;
        nimcp_mutex_unlock(bridge->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY, "mesh_bio_bridge: error condition");
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    /* Set transaction fields */
    tx->id.channel = channel;
    tx->id.proposer = (mesh_participant_id_t)header->source_module;
    tx->id.timestamp_ns = nimcp_time_now_ns();
    tx->id.sequence = header->sequence;
    tx->type = MESH_TX_BELIEF_UPDATE;  /* Bio messages update beliefs */
    tx->status = MESH_TX_STATUS_PROPOSED;
    tx->proposer_id = (mesh_participant_id_t)header->source_module;

    /* Copy payload */
    if (header->payload_size > 0 && msg_size > sizeof(bio_msg_header_t)) {
        size_t payload_len = msg_size - sizeof(bio_msg_header_t);
        payload_len = (payload_len < header->payload_size) ?
                      payload_len : header->payload_size;

        tx->payload = nimcp_malloc(payload_len);
        if (tx->payload) {
            memcpy(tx->payload, (const uint8_t*)bio_msg + sizeof(bio_msg_header_t),
                   payload_len);
            tx->payload_size = payload_len;
        }
    }

    /* Extract pattern for routing */
    if (bridge->config.enable_pattern_routing) {
        mesh_pattern_t pattern;
        if (mesh_bio_bridge_extract_pattern(bridge, bio_msg, msg_size, &pattern)
            == NIMCP_SUCCESS) {
            /* Pattern would be attached to transaction for routing */
            /* In production, tx would have a pattern field */
        }
    }

    *tx_out = tx;
    bridge->stats.mesh_transactions_created++;
    update_category_stats(&bridge->stats, category);

    if (bridge->config.verbose_logging) {
        LOG_DEBUG("Translated bio msg type=0x%04X to mesh tx on channel %u",
                 header->msg_type, channel);
    }

    nimcp_mutex_unlock(bridge->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_bio_bridge_route_bio_message(
    mesh_bio_bridge_t* bridge,
    const void* bio_msg,
    size_t msg_size
) {
    if (!bridge || bridge->magic != BIO_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Check if connected to a router or has mesh integration context */
    if (!bridge->connected && !bridge->integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_bio_bridge: not connected to router");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    mesh_transaction_t* tx = NULL;
    nimcp_error_t err = mesh_bio_bridge_translate_to_mesh(
        bridge, bio_msg, msg_size, &tx);

    if (err != NIMCP_SUCCESS) return err;

    /* Submit to mesh integration */
    if (bridge->integration) {
        /* Would call mesh_integration_submit_transaction() here */
    }

    /* Cleanup - in production, transaction manager owns it */
    if (tx) {
        nimcp_free(tx->payload);
        nimcp_free(tx);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Translation API: Mesh -> Bio
 * ============================================================================ */

nimcp_error_t mesh_bio_bridge_register_mesh_callback(
    mesh_bio_bridge_t* bridge,
    mesh_to_bio_callback_t callback,
    void* ctx
) {
    if (!bridge || bridge->magic != BIO_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);
    bridge->mesh_callback = callback;
    bridge->mesh_callback_ctx = ctx;
    nimcp_mutex_unlock(bridge->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_bio_bridge_translate_to_bio(
    mesh_bio_bridge_t* bridge,
    const mesh_transaction_t* tx,
    void* bio_msg_out,
    size_t* msg_size_out,
    size_t max_size
) {
    if (!bridge || bridge->magic != BIO_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx || !bio_msg_out || !msg_size_out) return NIMCP_ERROR_NULL_POINTER;
    if (max_size < sizeof(bio_msg_header_t)) return NIMCP_ERROR_CAPACITY_EXCEEDED;

    nimcp_mutex_lock(bridge->mutex);

    bridge->stats.mesh_events_received++;

    /* Use callback if registered */
    if (bridge->mesh_callback) {
        nimcp_error_t err = bridge->mesh_callback(
            tx, bio_msg_out, msg_size_out, bridge->mesh_callback_ctx);
        if (err == NIMCP_SUCCESS) {
            bridge->stats.bio_messages_sent++;
        } else {
            bridge->stats.reverse_translation_failures++;
        }
        nimcp_mutex_unlock(bridge->mutex);
        return err;
    }

    /* Default translation */
    bio_msg_header_t* header = (bio_msg_header_t*)bio_msg_out;

    /* Map transaction type back to bio category */
    uint32_t bio_type = MESH_BIO_CAT_SYSTEM;  /* Default */
    switch (tx->id.channel) {
        case MESH_CHANNEL_LEFT_HEMISPHERE:
            bio_type = MESH_BIO_CAT_COGNITIVE;
            break;
        case MESH_CHANNEL_RIGHT_HEMISPHERE:
            bio_type = MESH_BIO_CAT_PERCEPTION;
            break;
        case MESH_CHANNEL_SUBCORTICAL:
            bio_type = MESH_BIO_CAT_NEURAL;
            break;
        case MESH_CHANNEL_GPU_COMPUTE:
            bio_type = MESH_BIO_CAT_SYSTEM;
            break;
    }

    header->msg_type = bio_type | (tx->type & 0xFF);
    header->source_module = 0;  /* Mesh source */
    header->target_module = 0;  /* Broadcast */
    header->flags = 0;
    header->timestamp_ns = tx->id.timestamp_ns;
    header->sequence = tx->id.sequence;

    /* Copy payload if space allows */
    size_t total_size = sizeof(bio_msg_header_t);
    if (tx->payload && tx->payload_size > 0) {
        size_t copy_size = tx->payload_size;
        if (total_size + copy_size > max_size) {
            copy_size = max_size - total_size;
        }
        if (copy_size > 0) {
            memcpy((uint8_t*)bio_msg_out + sizeof(bio_msg_header_t),
                   tx->payload, copy_size);
            header->payload_size = (uint32_t)copy_size;
            total_size += copy_size;
        }
    } else {
        header->payload_size = 0;
    }

    *msg_size_out = total_size;
    bridge->stats.bio_messages_sent++;

    nimcp_mutex_unlock(bridge->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Channel Mapping API
 * ============================================================================ */

mesh_channel_id_t mesh_bio_bridge_get_channel(
    const mesh_bio_bridge_t* bridge,
    uint32_t bio_category
) {
    if (!bridge || bridge->magic != BIO_BRIDGE_MAGIC) {
        return MESH_CHANNEL_SYSTEM;
    }

    for (size_t i = 0; i < bridge->mapping_count; i++) {
        if (bridge->channel_mappings[i].active &&
            bridge->channel_mappings[i].bio_category == bio_category) {
            return bridge->channel_mappings[i].channel_id;
        }
    }

    return MESH_CHANNEL_SYSTEM;  /* Default fallback */
}

nimcp_error_t mesh_bio_bridge_set_channel_mapping(
    mesh_bio_bridge_t* bridge,
    uint32_t bio_category,
    mesh_channel_id_t channel_id
) {
    if (!bridge || bridge->magic != BIO_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Find existing or add new */
    for (size_t i = 0; i < bridge->mapping_count; i++) {
        if (bridge->channel_mappings[i].bio_category == bio_category) {
            bridge->channel_mappings[i].channel_id = channel_id;
            bridge->channel_mappings[i].active = true;
            nimcp_mutex_unlock(bridge->mutex);
            return NIMCP_SUCCESS;
        }
    }

    /* Add new mapping */
    if (bridge->mapping_count >= MAX_CHANNEL_MAPPINGS) {
        nimcp_mutex_unlock(bridge->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_bio_bridge: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    bridge->channel_mappings[bridge->mapping_count++] = (channel_mapping_t){
        .bio_category = bio_category,
        .channel_id = channel_id,
        .active = true
    };

    nimcp_mutex_unlock(bridge->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_bio_bridge_get_pattern_range(
    uint32_t bio_category,
    mesh_pattern_dim_range_t* range_out
) {
    if (!range_out) return NIMCP_ERROR_NULL_POINTER;

    int idx = get_category_index(bio_category);
    if (idx < 0) return NIMCP_ERROR_NOT_FOUND;

    *range_out = MESH_BIO_PATTERN_RANGES[idx];
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_bio_bridge_get_stats(
    const mesh_bio_bridge_t* bridge,
    mesh_bio_bridge_stats_t* stats
) {
    if (!bridge || bridge->magic != BIO_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_bio_bridge_t*)bridge)->mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((mesh_bio_bridge_t*)bridge)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_bio_bridge_reset_stats(mesh_bio_bridge_t* bridge) {
    if (!bridge || bridge->magic != BIO_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bio_bridge_reset_stats: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->mutex);

    return NIMCP_SUCCESS;
}
