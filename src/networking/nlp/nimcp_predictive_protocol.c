/**
 * @file nimcp_predictive_protocol.c
 * @brief Predictive Communication Protocol Implementation
 *
 * WHAT: Core implementation of predictive messaging
 * WHY:  Enable low-latency communication through prediction
 * HOW:  Pattern learning, sequence tracking, prefetch management
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "networking/nlp/nimcp_predictive_protocol.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_async.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(predictive_protocol)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Message sequence pattern node
 *
 * WHAT: Records transitions between message types
 * WHY:  Build Markov chain for prediction
 * HOW:  Count occurrences of msg_type following predecessor
 */
typedef struct pattern_node {
    uint32_t msg_type;              /**< Message type */
    uint32_t source_module;         /**< Source module */
    uint32_t target_module;         /**< Target module */
    uint64_t last_timestamp_ms;     /**< Last occurrence time */
    uint32_t occurrence_count;      /**< Times this pattern occurred */
    struct pattern_node* next;      /**< Next in transition chain */
} pattern_node_t;

/**
 * @brief Message history entry
 *
 * WHAT: Single recorded message in history buffer
 * WHY:  Track temporal sequences for learning
 * HOW:  Circular buffer of message records
 */
typedef struct {
    uint32_t msg_type;
    uint32_t source_module;
    uint32_t target_module;
    uint64_t timestamp_ms;
} message_history_entry_t;

/**
 * @brief Prefetch cache entry
 *
 * WHAT: Cached data for predicted message
 * WHY:  Store prefetched data for fast access
 * HOW:  Hash table keyed by message type
 */
typedef struct prefetch_entry {
    uint32_t msg_type;              /**< Message type */
    void* data;                     /**< Cached data */
    uint32_t size;                  /**< Data size */
    uint64_t prefetch_time_ms;      /**< When prefetched */
    float confidence;               /**< Prediction confidence */
    bool used;                      /**< Whether data was used */
    struct prefetch_entry* next;    /**< Hash chain */
} prefetch_entry_t;

/**
 * @brief Predictive protocol context
 *
 * WHAT: Complete state for predictive protocol
 * WHY:  Encapsulate all learning and prediction machinery
 * HOW:  Internal structure for opaque handle
 */
struct predictive_protocol {
    /* Configuration */
    predictive_protocol_config_t config;

    /* Pattern learning */
    pattern_node_t** pattern_table;     /**< Hash table of patterns */
    uint32_t pattern_table_size;        /**< Size of pattern table */

    /* Message history */
    message_history_entry_t* history;   /**< Circular buffer */
    uint32_t history_head;              /**< Write index */
    uint32_t history_count;             /**< Entries in buffer */

    /* Prefetch cache */
    prefetch_entry_t** prefetch_cache;  /**< Hash table of cached data */
    uint32_t prefetch_cache_size;       /**< Size of cache table */

    /* Statistics */
    predictive_stats_t stats;

    /* Bio-async integration */
    nimcp_bio_router_t* bio_router;
    bio_module_id_t module_id;

    /* Predictive coding integration */
    void* predictive_context;
};

//=============================================================================
// Internal Constants
//=============================================================================

#define PATTERN_TABLE_SIZE 1024         /**< Pattern hash table size */
#define PREFETCH_CACHE_SIZE 256         /**< Prefetch cache size */
#define PREFETCH_TTL_MS 5000            /**< Prefetch data TTL */

//=============================================================================
// Internal Utilities
//=============================================================================

/**
 * @brief Hash function for message patterns
 *
 * WHAT: Generate hash for message type
 * WHY:  Distribute patterns across hash table
 * HOW:  Simple modulo hash
 */
static inline uint32_t hash_message_type(uint32_t msg_type, uint32_t table_size)
{
    return msg_type % table_size;
}

/**
 * @brief Get current timestamp in milliseconds
 *
 * WHAT: Get monotonic time for ordering
 * WHY:  Track temporal patterns
 * HOW:  Use clock_gettime with MONOTONIC clock
 */
static uint64_t get_timestamp_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Find pattern in transition chain
 *
 * WHAT: Search for specific pattern in hash chain
 * WHY:  Lookup existing patterns for update
 * HOW:  Linear search through chain
 */
static pattern_node_t* find_pattern(
    pattern_node_t* chain,
    uint32_t msg_type,
    uint32_t source,
    uint32_t target)
{
    if (!chain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chain is NULL");

        return NULL;
    }

    for (pattern_node_t* node = chain; node != NULL; node = node->next) {
        if (node->msg_type == msg_type &&
            node->source_module == source &&
            node->target_module == target) {
            return node;
        }
    }

    /* Not found is normal lookup result, not an error */
    return NULL;
}

/**
 * @brief Calculate prediction confidence
 *
 * WHAT: Compute confidence based on occurrence frequency
 * WHY:  Weight predictions by historical reliability
 * HOW:  Normalize by total occurrences and recency
 */
static float calculate_confidence(
    uint32_t occurrence_count,
    uint32_t total_occurrences,
    uint64_t last_timestamp_ms,
    uint64_t current_timestamp_ms)
{
    if (total_occurrences == 0) {
        return 0.0f;
    }

    /* Frequency component */
    float frequency = (float)occurrence_count / (float)total_occurrences;

    /* Recency component (decay over time) */
    uint64_t age_ms = current_timestamp_ms - last_timestamp_ms;
    float recency = 1.0f / (1.0f + age_ms / 10000.0f); /* 10s half-life */

    /* Combined confidence */
    float confidence = 0.7f * frequency + 0.3f * recency;

    /* Clamp to valid range */
    if (confidence < PRED_PROTO_MIN_CONFIDENCE) {
        confidence = PRED_PROTO_MIN_CONFIDENCE;
    }
    if (confidence > PRED_PROTO_MAX_CONFIDENCE) {
        confidence = PRED_PROTO_MAX_CONFIDENCE;
    }

    return confidence;
}

/**
 * @brief Cleanup old prefetch entries
 *
 * WHAT: Remove expired prefetch data from cache
 * WHY:  Prevent memory leak from unused predictions
 * HOW:  Scan cache and free entries older than TTL
 */
static void cleanup_prefetch_cache(predictive_protocol_t* protocol)
{
    if (!protocol || !protocol->prefetch_cache) {
        return;
    }

    uint64_t now = get_timestamp_ms();

    for (uint32_t i = 0; i < protocol->prefetch_cache_size; i++) {
        prefetch_entry_t** head = &protocol->prefetch_cache[i];
        prefetch_entry_t* prev = NULL;
        prefetch_entry_t* curr = *head;

        while (curr != NULL) {
            uint64_t age = now - curr->prefetch_time_ms;

            if (age > PREFETCH_TTL_MS) {
                /* Entry expired */
                prefetch_entry_t* next = curr->next;

                if (!curr->used) {
                    /* Track wasted prefetch */
                    protocol->stats.prefetch_misses++;
                }

                nimcp_free(curr->data);
                nimcp_free(curr);

                if (prev) {
                    prev->next = next;
                } else {
                    *head = next;
                }

                curr = next;
            } else {
                prev = curr;
                curr = curr->next;
            }
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

predictive_protocol_t* predictive_protocol_create(
    const predictive_protocol_config_t* config)
{
    /* Guard: validate config */
    if (config && config->history_buffer_size == 0) {
        LOG_ERROR("Invalid history buffer size");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "predictive_protocol_create: config->history_buffer_size is zero");
        return NULL;
    }

    /* Allocate context - memset zeroes all pointers for safe cleanup */
    predictive_protocol_t* protocol = nimcp_malloc(sizeof(predictive_protocol_t));
    if (!protocol) {
        LOG_ERROR("Failed to allocate protocol");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_protocol_create: protocol is NULL");
        return NULL;
    }
    memset(protocol, 0, sizeof(predictive_protocol_t));

    /* Set configuration */
    if (config) {
        protocol->config = *config;
    } else {
        /* Default configuration */
        protocol->config.prediction_window_ms = PRED_PROTO_DEFAULT_WINDOW_MS;
        protocol->config.history_buffer_size = PRED_PROTO_DEFAULT_HISTORY_SIZE;
        protocol->config.confidence_threshold = PRED_PROTO_DEFAULT_CONFIDENCE;
        protocol->config.enable_prefetch = true;
        protocol->config.enable_bio_async = false;
    }

    /* Allocate pattern table */
    protocol->pattern_table_size = PATTERN_TABLE_SIZE;
    protocol->pattern_table = nimcp_calloc(
        protocol->pattern_table_size,
        sizeof(pattern_node_t*));
    if (!protocol->pattern_table) goto cleanup;

    /* Allocate history buffer */
    protocol->history = nimcp_calloc(
        protocol->config.history_buffer_size,
        sizeof(message_history_entry_t));
    if (!protocol->history) goto cleanup;

    /* Allocate prefetch cache */
    if (protocol->config.enable_prefetch) {
        protocol->prefetch_cache_size = PREFETCH_CACHE_SIZE;
        protocol->prefetch_cache = nimcp_calloc(
            protocol->prefetch_cache_size,
            sizeof(prefetch_entry_t*));
        if (!protocol->prefetch_cache) goto cleanup;
    }

    /* Initialize bio-async if enabled */
    if (protocol->config.enable_bio_async) {
        protocol->module_id = BIO_MODULE_PROTOCOL;
        protocol->bio_router = nimcp_bio_get_global_router();

        if (protocol->bio_router) {
            nimcp_bio_register_module(
                protocol->bio_router,
                protocol->module_id,
                "PredictiveProtocol");
        }
    }

    LOG_INFO("Created predictive protocol: window=%ums history=%u thresh=%.2f",
        protocol->config.prediction_window_ms,
        protocol->config.history_buffer_size,
        protocol->config.confidence_threshold);

    return protocol;

cleanup:
    LOG_ERROR("Failed to allocate predictive protocol resources");
    nimcp_free(protocol->prefetch_cache);
    nimcp_free(protocol->history);
    nimcp_free(protocol->pattern_table);
    nimcp_free(protocol);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_protocol_create: allocation failed");
    return NULL;
}

void predictive_protocol_destroy(predictive_protocol_t* protocol)
{
    if (!protocol) {
        return;
    }

    /* Free pattern table */
    if (protocol->pattern_table) {
        for (uint32_t i = 0; i < protocol->pattern_table_size; i++) {
            pattern_node_t* node = protocol->pattern_table[i];
            while (node) {
                pattern_node_t* next = node->next;
                nimcp_free(node);
                node = next;
            }
        }
        nimcp_free(protocol->pattern_table);
    }

    /* Free history buffer */
    nimcp_free(protocol->history);

    /* Free prefetch cache */
    if (protocol->prefetch_cache) {
        for (uint32_t i = 0; i < protocol->prefetch_cache_size; i++) {
            prefetch_entry_t* entry = protocol->prefetch_cache[i];
            while (entry) {
                prefetch_entry_t* next = entry->next;
                nimcp_free(entry->data);
                nimcp_free(entry);
                entry = next;
            }
        }
        nimcp_free(protocol->prefetch_cache);
    }

    /* Unregister from bio-async */
    if (protocol->bio_router) {
        nimcp_bio_unregister_module(protocol->bio_router, protocol->module_id);
    }

    LOG_INFO("Destroyed predictive protocol: accuracy=%.2f%% predictions=%llu",
        protocol->stats.prediction_accuracy * 100.0f,
        protocol->stats.predictions_made);

    nimcp_free(protocol);
}

//=============================================================================
// Pattern Learning Functions
//=============================================================================

int predictive_protocol_observe_message(
    predictive_protocol_t* protocol,
    uint32_t msg_type,
    uint32_t source,
    uint32_t target,
    uint64_t timestamp_ms)
{
    /* Guard: validate input */
    if (!protocol) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol is NULL");

        return -1;
    }

    /* Add to history buffer */
    uint32_t idx = protocol->history_head;
    protocol->history[idx].msg_type = msg_type;
    protocol->history[idx].source_module = source;
    protocol->history[idx].target_module = target;
    protocol->history[idx].timestamp_ms = timestamp_ms;

    protocol->history_head = (protocol->history_head + 1) %
        protocol->config.history_buffer_size;

    if (protocol->history_count < protocol->config.history_buffer_size) {
        protocol->history_count++;
    }

    /* Learn pattern if we have a predecessor */
    if (protocol->history_count >= 2) {
        /* Get previous message */
        uint32_t prev_idx = (idx + protocol->config.history_buffer_size - 1) %
            protocol->config.history_buffer_size;
        uint32_t prev_type = protocol->history[prev_idx].msg_type;

        /* Update pattern table */
        uint32_t hash = hash_message_type(prev_type, protocol->pattern_table_size);
        pattern_node_t** head = &protocol->pattern_table[hash];

        /* Find or create pattern node */
        pattern_node_t* node = find_pattern(*head, msg_type, source, target);

        if (node) {
            /* Update existing pattern */
            node->occurrence_count++;
            node->last_timestamp_ms = timestamp_ms;
        } else {
            /* Create new pattern */
            node = nimcp_malloc(sizeof(pattern_node_t));
            if (!node) {
                LOG_ERROR("Failed to allocate pattern node");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_protocol_observe_message: node is NULL");
                return -1;
            }

            node->msg_type = msg_type;
            node->source_module = source;
            node->target_module = target;
            node->last_timestamp_ms = timestamp_ms;
            node->occurrence_count = 1;
            node->next = *head;
            *head = node;
        }
    }

    return 0;
}

int predictive_protocol_learn_sequence(
    predictive_protocol_t* protocol,
    const uint32_t* message_sequence,
    uint32_t seq_len)
{
    /* Guard: validate input */
    if (!protocol || !message_sequence || seq_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_learn_sequence: required parameter is NULL (protocol, message_sequence)");
        return -1;
    }

    if (seq_len > PRED_PROTO_MAX_SEQUENCE_LENGTH) {
        LOG_WARN("Sequence length %u exceeds max %u",
            seq_len, PRED_PROTO_MAX_SEQUENCE_LENGTH);
        seq_len = PRED_PROTO_MAX_SEQUENCE_LENGTH;
    }

    /* Process each message in sequence */
    uint64_t base_time = get_timestamp_ms();

    for (uint32_t i = 0; i < seq_len; i++) {
        uint64_t timestamp = base_time + (i * 100); /* 100ms spacing */

        int ret = predictive_protocol_observe_message(
            protocol,
            message_sequence[i],
            0, /* Unknown source */
            0, /* Unknown target */
            timestamp);

        if (ret != 0) {
            return ret;
        }
    }

    LOG_DEBUG("Learned sequence of %u messages", seq_len);
    return 0;
}

//=============================================================================
// Prediction Functions
//=============================================================================

int predictive_protocol_predict_next(
    predictive_protocol_t* protocol,
    uint32_t current_msg_type,
    predicted_message_t* prediction)
{
    /* Guard: validate input */
    if (!protocol || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_predict_next: required parameter is NULL (protocol, prediction)");
        return -1;
    }

    memset(prediction, 0, sizeof(predicted_message_t));

    /* Look up patterns following current message */
    uint32_t hash = hash_message_type(current_msg_type, protocol->pattern_table_size);
    pattern_node_t* chain = protocol->pattern_table[hash];

    if (!chain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_predict_next: chain is NULL");
        return -1; /* No patterns */
    }

    /* Find most likely next message */
    pattern_node_t* best = NULL;
    uint32_t total_occurrences = 0;

    /* Count total occurrences */
    for (pattern_node_t* node = chain; node != NULL; node = node->next) {
        total_occurrences += node->occurrence_count;
    }

    /* Find best candidate */
    uint64_t now = get_timestamp_ms();
    float best_confidence = 0.0f;

    for (pattern_node_t* node = chain; node != NULL; node = node->next) {
        float confidence = calculate_confidence(
            node->occurrence_count,
            total_occurrences,
            node->last_timestamp_ms,
            now);

        if (confidence > best_confidence) {
            best_confidence = confidence;
            best = node;
        }
    }

    /* Check threshold */
    if (!best || best_confidence < protocol->config.confidence_threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "predictive_protocol_predict_next: best is NULL");
        return -1; /* Below threshold */
    }

    /* Fill prediction */
    prediction->message_type = best->msg_type;
    prediction->source_module = best->source_module;
    prediction->target_module = best->target_module;
    prediction->confidence = best_confidence;
    prediction->predicted_time_ms = now + protocol->config.prediction_window_ms;
    prediction->predicted_payload = NULL;
    prediction->payload_size = 0;

    /* Update statistics */
    protocol->stats.predictions_made++;

    /* Send bio-async notification */
    if (protocol->bio_router) {
        /* BIO_MSG_PREDICTION_MADE would be sent here */
    }

    LOG_DEBUG("Predicted message type=%u conf=%.2f",
        prediction->message_type, prediction->confidence);

    return 0;
}

int predictive_protocol_get_predictions(
    predictive_protocol_t* protocol,
    uint64_t time_window_ms,
    predicted_message_t** predictions,
    uint32_t* count)
{
    /* Guard: validate input */
    if (!protocol || !predictions || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_get_predictions: required parameter is NULL (protocol, predictions, count)");
        return -1;
    }

    *predictions = NULL;
    *count = 0;

    /* Allocate prediction array */
    predicted_message_t* preds = nimcp_malloc(
        PRED_PROTO_MAX_PREDICTIONS * sizeof(predicted_message_t));
    if (!preds) {
        LOG_ERROR("Failed to allocate predictions array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_protocol_get_predictions: preds is NULL");
        return -1;
    }

    uint32_t pred_count = 0;
    uint64_t now = get_timestamp_ms();

    /* Scan pattern table for predictions */
    for (uint32_t i = 0; i < protocol->pattern_table_size; i++) {
        pattern_node_t* chain = protocol->pattern_table[i];

        for (pattern_node_t* node = chain; node != NULL; node = node->next) {
            /* Check if pattern is within time window */
            uint64_t age = now - node->last_timestamp_ms;
            if (age > time_window_ms) {
                continue;
            }

            /* Calculate confidence */
            float confidence = calculate_confidence(
                node->occurrence_count,
                node->occurrence_count + 1,
                node->last_timestamp_ms,
                now);

            if (confidence < protocol->config.confidence_threshold) {
                continue;
            }

            /* Add prediction */
            if (pred_count < PRED_PROTO_MAX_PREDICTIONS) {
                preds[pred_count].message_type = node->msg_type;
                preds[pred_count].source_module = node->source_module;
                preds[pred_count].target_module = node->target_module;
                preds[pred_count].confidence = confidence;
                preds[pred_count].predicted_time_ms = now + time_window_ms;
                preds[pred_count].predicted_payload = NULL;
                preds[pred_count].payload_size = 0;
                pred_count++;
            }
        }
    }

    *predictions = preds;
    *count = pred_count;

    LOG_DEBUG("Generated %u predictions for window %llums",
        pred_count, time_window_ms);

    return 0;
}

//=============================================================================
// Prefetching Functions
//=============================================================================

int predictive_protocol_prefetch_data(
    predictive_protocol_t* protocol,
    const predicted_message_t* prediction)
{
    /* Guard: validate input */
    if (!protocol || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_prefetch_data: required parameter is NULL (protocol, prediction)");
        return -1;
    }

    if (!protocol->config.enable_prefetch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_prefetch_data: protocol->config is NULL");
        return -1;
    }

    /* Check if already cached */
    uint32_t hash = hash_message_type(
        prediction->message_type,
        protocol->prefetch_cache_size);

    prefetch_entry_t* chain = protocol->prefetch_cache[hash];

    for (prefetch_entry_t* entry = chain; entry != NULL; entry = entry->next) {
        if (entry->msg_type == prediction->message_type) {
            /* Already cached */
            return 0;
        }
    }

    /* Create new cache entry */
    prefetch_entry_t* entry = nimcp_malloc(sizeof(prefetch_entry_t));
    if (!entry) {
        LOG_ERROR("Failed to allocate prefetch entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_protocol_prefetch_data: entry is NULL");
        return -1;
    }

    entry->msg_type = prediction->message_type;
    entry->data = NULL; /* Placeholder - actual prefetch would happen here */
    entry->size = 0;
    entry->prefetch_time_ms = get_timestamp_ms();
    entry->confidence = prediction->confidence;
    entry->used = false;
    entry->next = protocol->prefetch_cache[hash];
    protocol->prefetch_cache[hash] = entry;

    LOG_DEBUG("Prefetched data for message type=%u conf=%.2f",
        prediction->message_type, prediction->confidence);

    return 0;
}

int predictive_protocol_check_prefetch(
    predictive_protocol_t* protocol,
    uint32_t msg_type,
    void** data,
    uint32_t* size)
{
    /* Guard: validate input */
    if (!protocol || !data || !size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_check_prefetch: required parameter is NULL (protocol, data, size)");
        return -1;
    }

    if (!protocol->config.enable_prefetch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_check_prefetch: protocol->config is NULL");
        return -1;
    }

    *data = NULL;
    *size = 0;

    /* Cleanup old entries first */
    cleanup_prefetch_cache(protocol);

    /* Look up in cache */
    uint32_t hash = hash_message_type(msg_type, protocol->prefetch_cache_size);
    prefetch_entry_t* chain = protocol->prefetch_cache[hash];

    for (prefetch_entry_t* entry = chain; entry != NULL; entry = entry->next) {
        if (entry->msg_type == msg_type) {
            /* Found cached data */
            *data = entry->data;
            *size = entry->size;
            entry->used = true;

            /* Update statistics */
            protocol->stats.prefetch_hits++;

            LOG_DEBUG("Prefetch hit for message type=%u", msg_type);
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "predictive_protocol_check_prefetch: operation failed");
    return -1; /* Not cached */
}

//=============================================================================
// Integration Functions
//=============================================================================

int predictive_protocol_connect_predictive_coding(
    predictive_protocol_t* protocol,
    void* predictive_context)
{
    /* Guard: validate input */
    if (!protocol) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol is NULL");

        return -1;
    }

    protocol->predictive_context = predictive_context;

    LOG_INFO("Connected predictive coding context");
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int predictive_protocol_process_inbox(predictive_protocol_t* protocol)
{
    /* Guard: validate input */
    if (!protocol || !protocol->bio_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_process_inbox: required parameter is NULL (protocol, protocol->bio_router)");
        return -1;
    }

    int processed = 0;

    /* Process all pending messages */
    while (true) {
        bio_message_header_t header;
        uint8_t payload[1024];

        int ret = nimcp_bio_receive_message(
            protocol->bio_router,
            protocol->module_id,
            &header,
            payload,
            sizeof(payload));

        if (ret <= 0) {
            break; /* No more messages */
        }

        /* Handle message based on type */
        switch (header.type) {
            case BIO_MSG_BRAIN_STATE_QUERY:
                /* Example: could trigger predictions */
                LOG_DEBUG("Received brain state query");
                break;

            default:
                LOG_DEBUG("Unhandled message type: %u", header.type);
                break;
        }

        processed++;
    }

    return processed;
}

//=============================================================================
// Statistics Functions
//=============================================================================

predictive_stats_t predictive_protocol_get_stats(
    predictive_protocol_t* protocol)
{
    predictive_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    /* Guard: validate input */
    if (!protocol) {
        return stats;
    }

    stats = protocol->stats;

    /* Calculate accuracy */
    if (stats.predictions_made > 0) {
        stats.prediction_accuracy = (float)stats.predictions_correct /
            (float)stats.predictions_made;
    }

    /* Calculate average lead time */
    stats.avg_prediction_lead_time_ms = protocol->config.prediction_window_ms;

    return stats;
}
