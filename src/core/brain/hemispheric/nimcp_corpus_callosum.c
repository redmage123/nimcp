//=============================================================================
// nimcp_corpus_callosum.c - Inter-Hemispheric Communication Bridge
//=============================================================================
/**
 * @file nimcp_corpus_callosum.c
 * @brief Implementation of corpus callosum connecting brain hemispheres
 *
 * BIOLOGICAL BASIS:
 * - ~200 million axons connecting hemispheres
 * - Transmission latency: 5-20ms depending on fiber myelination
 * - Bandwidth limited by axon count and conduction velocity
 * - Anterior region: motor/prefrontal
 * - Body: somatosensory/auditory
 * - Splenium: visual areas
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#include "core/brain/hemispheric/nimcp_corpus_callosum.h"
#include "core/brain/hemispheric/nimcp_brain_hemisphere.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE_MESH_ONLY(corpus_callosum, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Constants
//=============================================================================

#define DEFAULT_QUEUE_CAPACITY      64
#define REALISTIC_MAX_MSGS_PER_SEC  200
#define RESTRICTED_MAX_MSGS_PER_SEC 50
#define REALISTIC_MIN_LATENCY_MS    5.0f
#define REALISTIC_MAX_LATENCY_MS    20.0f
#define RESTRICTED_MIN_LATENCY_MS   20.0f
#define RESTRICTED_MAX_LATENCY_MS   50.0f
#define BANDWIDTH_WINDOW_MS         1000  // 1 second window

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void) {
    return nimcp_time_get_us() / 1000;
}

/**
 * @brief Generate random latency within range
 */
static float random_latency(float min_ms, float max_ms) {
    if (min_ms >= max_ms) {
        return min_ms;
    }
    float range = max_ms - min_ms;
    float r = (float)nimcp_tl_rand() / (float)RAND_MAX;
    return min_ms + r * range;
}

/**
 * @brief Initialize a message queue
 */
static int queue_init(callosum_message_queue_t* queue, uint32_t capacity) {
    if (!queue || capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "queue_init: queue is NULL");
        return -1;
    }

    queue->messages = nimcp_calloc(capacity, sizeof(callosum_message_t));
    if (!queue->messages) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "queue_init: queue->messages is NULL");
        return -1;
    }

    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->total_enqueued = 0;
    queue->total_delivered = 0;
    queue->total_dropped = 0;

    queue->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!queue->mutex) {
        nimcp_free(queue->messages);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "queue_init: queue->mutex is NULL");
        return -1;
    }
    nimcp_mutex_init(queue->mutex, NULL);

    return 0;
}

/**
 * @brief Destroy a message queue
 */
static void queue_destroy(callosum_message_queue_t* queue) {
    if (!queue) {
        return;
    }

    // Free any remaining message data (NULL after free to prevent double-free)
    if (queue->messages) {
        for (uint32_t i = 0; i < queue->capacity; i++) {
            if (queue->messages[i].data) {
                nimcp_free(queue->messages[i].data);
                queue->messages[i].data = NULL;
            }
        }
        nimcp_free(queue->messages);
        queue->messages = NULL;
    }

    if (queue->mutex) {
        nimcp_mutex_free(queue->mutex);
    }

    memset(queue, 0, sizeof(*queue));
}

/**
 * @brief Enqueue a message
 */
static int queue_enqueue(callosum_message_queue_t* queue,
                         const callosum_message_t* msg,
                         bool drop_on_overflow) {
    if (!queue || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "queue_destroy: required parameter is NULL (queue, msg)");
        return -1;
    }

    nimcp_mutex_lock(queue->mutex);

    if (queue->count >= queue->capacity) {
        if (drop_on_overflow) {
            queue->total_dropped++;
            nimcp_mutex_unlock(queue->mutex);
            return 1;  // Dropped
        }
        nimcp_mutex_unlock(queue->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "queue_destroy: validation failed");
        return -1;  // Would block
    }

    // Copy message
    callosum_message_t* slot = &queue->messages[queue->head];

    // Free old data if present
    if (slot->data) {
        nimcp_free(slot->data);
    }

    memcpy(slot, msg, sizeof(callosum_message_t));

    // Copy payload data
    if (msg->data && msg->data_size > 0) {
        slot->data = nimcp_malloc(msg->data_size);
        if (slot->data) {
            memcpy(slot->data, msg->data, msg->data_size);
        }
    }

    queue->head = (queue->head + 1) % queue->capacity;
    queue->count++;
    queue->total_enqueued++;

    nimcp_mutex_unlock(queue->mutex);
    return 0;
}

/**
 * @brief Dequeue messages ready for delivery
 */
static int queue_dequeue_ready(callosum_message_queue_t* queue,
                               callosum_message_t* out_msgs,
                               uint32_t max_msgs,
                               uint64_t current_time) {
    if (!queue || !out_msgs || max_msgs == 0) {
        return 0;
    }

    nimcp_mutex_lock(queue->mutex);

    int count = 0;
    uint32_t checked = 0;
    uint32_t idx = queue->tail;

    while (checked < queue->count && count < (int)max_msgs) {
        callosum_message_t* msg = &queue->messages[idx];

        if (msg->scheduled_delivery <= current_time) {
            // Ready for delivery
            memcpy(&out_msgs[count], msg, sizeof(callosum_message_t));
            out_msgs[count].delivered = true;

            // Clear data pointer (ownership transferred)
            msg->data = NULL;
            msg->data_size = 0;

            // Remove from queue
            if (idx == queue->tail) {
                queue->tail = (queue->tail + 1) % queue->capacity;
            }
            queue->count--;
            queue->total_delivered++;
            count++;
        }

        idx = (idx + 1) % queue->capacity;
        checked++;
    }

    nimcp_mutex_unlock(queue->mutex);
    return count;
}

//=============================================================================
// Channel Helpers
//=============================================================================

/**
 * @brief Initialize a callosum channel
 */
static void channel_init(callosum_channel_t* channel,
                         callosum_channel_type_t type) {
    if (!channel) {
        return;
    }

    memset(channel, 0, sizeof(*channel));
    channel->type = type;
    channel->enabled = true;
    channel->max_msgs_per_second = 0;  // Use global
    channel->min_latency_ms = 0.0f;    // Use global
    channel->max_latency_ms = 0.0f;    // Use global
}

/**
 * @brief Get effective bandwidth limit for channel
 */
static uint32_t channel_get_bandwidth(const corpus_callosum_t* cc,
                                      callosum_channel_type_t channel) {
    if (!cc || channel >= CALLOSUM_CHANNEL_COUNT) {
        return 0;
    }

    uint32_t channel_limit = cc->channels[channel].max_msgs_per_second;
    return (channel_limit > 0) ? channel_limit : cc->max_messages_per_second;
}

/**
 * @brief Get effective latency range for channel
 */
static void channel_get_latency(const corpus_callosum_t* cc,
                                callosum_channel_type_t channel,
                                float* min_ms, float* max_ms) {
    if (!cc || channel >= CALLOSUM_CHANNEL_COUNT) {
        if (min_ms) *min_ms = 0.0f;
        if (max_ms) *max_ms = 0.0f;
        return;
    }

    float ch_min = cc->channels[channel].min_latency_ms;
    float ch_max = cc->channels[channel].max_latency_ms;

    if (min_ms) {
        *min_ms = (ch_min > 0.0f) ? ch_min : cc->min_latency_ms;
    }
    if (max_ms) {
        *max_ms = (ch_max > 0.0f) ? ch_max : cc->max_latency_ms;
    }
}

//=============================================================================
// Configuration
//=============================================================================

callosum_config_t callosum_default_config(void) {
    callosum_config_t config = {
        .bandwidth_mode = CALLOSUM_BW_REALISTIC,
        .custom_max_msgs_per_second = 200,
        .custom_min_latency_ms = 5.0f,
        .custom_max_latency_ms = 20.0f,

        // Per-channel defaults (0 = use global)
        .channel_bandwidth = {0},
        .channel_min_latency = {0.0f},
        .channel_max_latency = {0.0f},

        .queue_capacity = DEFAULT_QUEUE_CAPACITY,
        .drop_on_overflow = true,

        .initial_connection_strength = 1.0f,
        .enable_bio_async = true
    };
    return config;
}

bool callosum_validate_config(const callosum_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "callosum_validate_config: config is NULL");
        return false;
    }

    if (config->queue_capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "callosum_validate_config: config->queue_capacity is zero");
        return false;
    }

    if (config->initial_connection_strength < 0.0f ||
        config->initial_connection_strength > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "callosum_validate_config: config->queue_capacity is zero");
        return false;
    }

    if (config->custom_min_latency_ms > config->custom_max_latency_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "callosum_validate_config: validation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle
//=============================================================================

corpus_callosum_t* callosum_create(const callosum_config_t* config) {
    callosum_config_t cfg = config ? *config : callosum_default_config();

    if (!callosum_validate_config(&cfg)) {
        NIMCP_LOGGING_ERROR("Invalid callosum configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "callosum_create: invalid configuration");
        return NULL;
    }

    corpus_callosum_t* cc = nimcp_calloc(1, sizeof(corpus_callosum_t));
    if (!cc) {
        NIMCP_LOGGING_ERROR("Failed to allocate corpus callosum");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "callosum_create: failed to allocate corpus callosum");
        return NULL;
    }

    // Initialize channels
    for (int i = 0; i < CALLOSUM_CHANNEL_COUNT; i++) {
        channel_init(&cc->channels[i], (callosum_channel_type_t)i);
        cc->channels[i].max_msgs_per_second = cfg.channel_bandwidth[i];
        cc->channels[i].min_latency_ms = cfg.channel_min_latency[i];
        cc->channels[i].max_latency_ms = cfg.channel_max_latency[i];
    }

    // Set bandwidth mode
    cc->bandwidth_mode = cfg.bandwidth_mode;
    switch (cfg.bandwidth_mode) {
        case CALLOSUM_BW_UNLIMITED:
            cc->max_messages_per_second = 0;
            cc->enable_latency_simulation = false;
            cc->min_latency_ms = 0.0f;
            cc->max_latency_ms = 0.0f;
            break;
        case CALLOSUM_BW_REALISTIC:
            cc->max_messages_per_second = REALISTIC_MAX_MSGS_PER_SEC;
            cc->enable_latency_simulation = true;
            cc->min_latency_ms = REALISTIC_MIN_LATENCY_MS;
            cc->max_latency_ms = REALISTIC_MAX_LATENCY_MS;
            break;
        case CALLOSUM_BW_RESTRICTED:
            cc->max_messages_per_second = RESTRICTED_MAX_MSGS_PER_SEC;
            cc->enable_latency_simulation = true;
            cc->min_latency_ms = RESTRICTED_MIN_LATENCY_MS;
            cc->max_latency_ms = RESTRICTED_MAX_LATENCY_MS;
            break;
        case CALLOSUM_BW_CUSTOM:
            cc->max_messages_per_second = cfg.custom_max_msgs_per_second;
            cc->enable_latency_simulation = (cfg.custom_max_latency_ms > 0.0f);
            cc->min_latency_ms = cfg.custom_min_latency_ms;
            cc->max_latency_ms = cfg.custom_max_latency_ms;
            break;
    }

    // Initialize message queues
    if (queue_init(&cc->left_to_right, cfg.queue_capacity) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "callosum_create: failed to initialize left_to_right queue");
        nimcp_free(cc);
        return NULL;
    }
    if (queue_init(&cc->right_to_left, cfg.queue_capacity) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "callosum_create: failed to initialize right_to_left queue");
        queue_destroy(&cc->left_to_right);
        nimcp_free(cc);
        return NULL;
    }

    // Connection state
    cc->is_connected = true;
    cc->connection_strength = cfg.initial_connection_strength;
    cc->state = CALLOSUM_STATE_HEALTHY;

    // Mutex
    cc->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!cc->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "callosum_create: failed to allocate mutex");
        queue_destroy(&cc->left_to_right);
        queue_destroy(&cc->right_to_left);
        nimcp_free(cc);
        return NULL;
    }
    nimcp_mutex_init(cc->mutex, NULL);

    // Bio-async
    cc->bio_async_enabled = false;  // Connected later

    // Reset bandwidth window
    cc->bandwidth_window_start = get_time_ms();
    cc->current_message_count = 0;

    NIMCP_LOGGING_INFO("Corpus callosum created with %s bandwidth mode",
                       callosum_bandwidth_mode_name(cfg.bandwidth_mode));

    return cc;
}

void callosum_destroy(corpus_callosum_t* cc) {
    if (!cc) {
        return;
    }

    // Disconnect bio-async
    if (cc->bio_async_enabled) {
        callosum_disconnect_bio_async(cc);
    }

    // Destroy queues
    queue_destroy(&cc->left_to_right);
    queue_destroy(&cc->right_to_left);

    // Destroy mutex
    if (cc->mutex) {
        nimcp_mutex_free(cc->mutex);
    }

    nimcp_free(cc);
}

int callosum_connect_hemispheres(
    corpus_callosum_t* cc,
    brain_hemisphere_t* left,
    brain_hemisphere_t* right
) {
    if (!cc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cc is NULL");

        return -1;
    }

    nimcp_mutex_lock(cc->mutex);

    cc->left = left;
    cc->right = right;

    // Set callosum reference in hemispheres
    if (left) {
        hemisphere_set_callosum(left, cc);
    }
    if (right) {
        hemisphere_set_callosum(right, cc);
    }

    nimcp_mutex_unlock(cc->mutex);
    return 0;
}

//=============================================================================
// Messaging
//=============================================================================

int callosum_send(
    corpus_callosum_t* cc,
    hemisphere_id_t from,
    callosum_channel_type_t channel,
    callosum_priority_t priority,
    uint32_t message_type,
    const void* data,
    size_t size
) {
    if (!cc || channel >= CALLOSUM_CHANNEL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "callosum_send: cc is NULL or invalid channel");
        return -1;
    }

    nimcp_mutex_lock(cc->mutex);

    // Check connection
    if (!cc->is_connected) {
        cc->stats.messages_dropped++;
        nimcp_mutex_unlock(cc->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "callosum_send: cc->is_connected is NULL");
        return -1;
    }

    // Check channel enabled
    if (!cc->channels[channel].enabled) {
        cc->stats.messages_dropped++;
        nimcp_mutex_unlock(cc->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "callosum_send: cc->channels is NULL");
        return -1;
    }

    // Check bandwidth limit
    uint64_t now = get_time_ms();
    if (now - cc->bandwidth_window_start >= BANDWIDTH_WINDOW_MS) {
        // Reset window
        cc->bandwidth_window_start = now;
        cc->current_message_count = 0;
    }

    uint32_t limit = channel_get_bandwidth(cc, channel);
    if (limit > 0 && cc->current_message_count >= limit) {
        cc->stats.messages_dropped++;
        cc->stats.messages_delayed++;
        nimcp_mutex_unlock(cc->mutex);
        return 1;  // Bandwidth limited
    }

    // Apply connection strength (probabilistic drop)
    if (cc->connection_strength < 1.0f) {
        float r = (float)nimcp_tl_rand() / (float)RAND_MAX;
        if (r > cc->connection_strength) {
            cc->stats.messages_dropped++;
            nimcp_mutex_unlock(cc->mutex);
            return 1;  // Dropped due to degradation
        }
    }

    // Create message
    // Note: Use explicit masking to prevent uint64_t overflow when casting to uint32_t
    // Check for potential overflow before adding (defensive - unlikely in practice)
    uint64_t left_count = cc->stats.total_messages_left_to_right;
    uint64_t right_count = cc->stats.total_messages_right_to_left;
    uint64_t total_msgs;
    if (left_count > UINT64_MAX - right_count) {
        // Overflow detected - wrap around safely
        total_msgs = (left_count - (UINT64_MAX - right_count) - 1);
    } else {
        total_msgs = left_count + right_count;
    }
    callosum_message_t msg = {
        .source = from,
        .destination = (from == HEMISPHERE_LEFT) ? HEMISPHERE_RIGHT : HEMISPHERE_LEFT,
        .channel = channel,
        .priority = priority,
        .timestamp = now,
        .sequence_num = (uint32_t)(total_msgs & 0xFFFFFFFF),
        .message_type = message_type,
        .data = (void*)data,
        .data_size = size,
        .delivered = false
    };

    // Calculate delivery time
    if (cc->enable_latency_simulation) {
        float min_lat, max_lat;
        channel_get_latency(cc, channel, &min_lat, &max_lat);
        float latency = random_latency(min_lat, max_lat);
        msg.scheduled_delivery = now + (uint64_t)latency;
    } else {
        msg.scheduled_delivery = now;
    }

    // Enqueue
    callosum_message_queue_t* queue = (from == HEMISPHERE_LEFT)
        ? &cc->left_to_right
        : &cc->right_to_left;

    int result = queue_enqueue(queue, &msg, true);

    if (result == 0) {
        cc->current_message_count++;
        cc->channels[channel].messages_sent++;
        cc->channels[channel].bytes_transferred += size;

        if (from == HEMISPHERE_LEFT) {
            cc->stats.total_messages_left_to_right++;
        } else {
            cc->stats.total_messages_right_to_left++;
        }
        cc->stats.total_bytes_transferred += size;
        cc->stats.channel_messages[channel]++;
    } else if (result == 1) {
        /* Queue overflow - message was dropped */
        cc->stats.messages_dropped++;
    }

    nimcp_mutex_unlock(cc->mutex);
    return result;
}

int callosum_receive(
    corpus_callosum_t* cc,
    hemisphere_id_t hemisphere,
    callosum_message_t* messages,
    uint32_t max_messages
) {
    if (!cc || !messages || max_messages == 0) {
        return 0;
    }

    uint64_t now = get_time_ms();

    callosum_message_queue_t* queue = (hemisphere == HEMISPHERE_LEFT)
        ? &cc->right_to_left
        : &cc->left_to_right;

    return queue_dequeue_ready(queue, messages, max_messages, now);
}

int callosum_process_queues(corpus_callosum_t* cc) {
    if (!cc) {
        return 0;
    }

    // This function is called to allow messages to be delivered
    // based on their scheduled delivery time. Actual delivery
    // happens via callosum_receive().

    // For now, just return count of pending messages
    nimcp_mutex_lock(cc->mutex);
    int count = cc->left_to_right.count + cc->right_to_left.count;
    nimcp_mutex_unlock(cc->mutex);

    return count;
}

int callosum_flush(corpus_callosum_t* cc) {
    if (!cc) {
        return 0;
    }

    nimcp_mutex_lock(cc->mutex);

    // Set all scheduled deliveries to now
    uint64_t now = get_time_ms();

    for (uint32_t i = 0; i < cc->left_to_right.capacity; i++) {
        cc->left_to_right.messages[i].scheduled_delivery = now;
    }
    for (uint32_t i = 0; i < cc->right_to_left.capacity; i++) {
        cc->right_to_left.messages[i].scheduled_delivery = now;
    }

    int count = cc->left_to_right.count + cc->right_to_left.count;
    nimcp_mutex_unlock(cc->mutex);

    return count;
}

//=============================================================================
// Bandwidth Control
//=============================================================================

int callosum_set_bandwidth_mode(
    corpus_callosum_t* cc,
    callosum_bandwidth_mode_t mode
) {
    if (!cc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cc is NULL");

        return -1;
    }

    nimcp_mutex_lock(cc->mutex);

    cc->bandwidth_mode = mode;

    switch (mode) {
        case CALLOSUM_BW_UNLIMITED:
            cc->max_messages_per_second = 0;
            cc->enable_latency_simulation = false;
            cc->min_latency_ms = 0.0f;
            cc->max_latency_ms = 0.0f;
            break;
        case CALLOSUM_BW_REALISTIC:
            cc->max_messages_per_second = REALISTIC_MAX_MSGS_PER_SEC;
            cc->enable_latency_simulation = true;
            cc->min_latency_ms = REALISTIC_MIN_LATENCY_MS;
            cc->max_latency_ms = REALISTIC_MAX_LATENCY_MS;
            break;
        case CALLOSUM_BW_RESTRICTED:
            cc->max_messages_per_second = RESTRICTED_MAX_MSGS_PER_SEC;
            cc->enable_latency_simulation = true;
            cc->min_latency_ms = RESTRICTED_MIN_LATENCY_MS;
            cc->max_latency_ms = RESTRICTED_MAX_LATENCY_MS;
            break;
        case CALLOSUM_BW_CUSTOM:
            // Keep current custom values
            break;
    }

    nimcp_mutex_unlock(cc->mutex);
    return 0;
}

int callosum_set_bandwidth_limit(corpus_callosum_t* cc, uint32_t msgs_per_second) {
    if (!cc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cc is NULL");

        return -1;
    }

    nimcp_mutex_lock(cc->mutex);
    cc->max_messages_per_second = msgs_per_second;
    cc->bandwidth_mode = CALLOSUM_BW_CUSTOM;
    nimcp_mutex_unlock(cc->mutex);

    return 0;
}

uint32_t corpus_callosum_get_base_bandwidth(const corpus_callosum_t* cc) {
    if (!cc) {
        return 200;  // Default realistic bandwidth
    }

    return cc->max_messages_per_second;
}

int callosum_set_channel_bandwidth(
    corpus_callosum_t* cc,
    callosum_channel_type_t channel,
    uint32_t msgs_per_second
) {
    if (!cc || channel >= CALLOSUM_CHANNEL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "callosum_set_channel_bandwidth: cc is NULL or invalid channel");
        return -1;
    }

    nimcp_mutex_lock(cc->mutex);
    cc->channels[channel].max_msgs_per_second = msgs_per_second;
    nimcp_mutex_unlock(cc->mutex);

    return 0;
}

float callosum_get_bandwidth_utilization(const corpus_callosum_t* cc) {
    if (!cc || cc->max_messages_per_second == 0) {
        return 0.0f;
    }

    return (float)cc->current_message_count / (float)cc->max_messages_per_second;
}

//=============================================================================
// Latency Control
//=============================================================================

int callosum_set_latency(corpus_callosum_t* cc, float min_ms, float max_ms) {
    if (!cc || min_ms > max_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "callosum_set_latency: cc is NULL or invalid latency range");
        return -1;
    }

    nimcp_mutex_lock(cc->mutex);
    cc->min_latency_ms = min_ms;
    cc->max_latency_ms = max_ms;
    cc->enable_latency_simulation = (max_ms > 0.0f);
    cc->bandwidth_mode = CALLOSUM_BW_CUSTOM;
    nimcp_mutex_unlock(cc->mutex);

    return 0;
}

int callosum_set_channel_latency(
    corpus_callosum_t* cc,
    callosum_channel_type_t channel,
    float min_ms,
    float max_ms
) {
    if (!cc || channel >= CALLOSUM_CHANNEL_COUNT || min_ms > max_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "callosum_set_channel_latency: cc is NULL, invalid channel, or invalid range");
        return -1;
    }

    nimcp_mutex_lock(cc->mutex);
    cc->channels[channel].min_latency_ms = min_ms;
    cc->channels[channel].max_latency_ms = max_ms;
    nimcp_mutex_unlock(cc->mutex);

    return 0;
}

int callosum_enable_latency_simulation(corpus_callosum_t* cc, bool enable) {
    if (!cc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cc is NULL");

        return -1;
    }

    nimcp_mutex_lock(cc->mutex);
    cc->enable_latency_simulation = enable;
    nimcp_mutex_unlock(cc->mutex);

    return 0;
}

float callosum_get_avg_latency(const corpus_callosum_t* cc) {
    if (!cc) {
        return 0.0f;
    }
    return cc->stats.avg_latency_ms;
}

//=============================================================================
// Connection Control
//=============================================================================

int callosum_disconnect(corpus_callosum_t* cc) {
    if (!cc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cc is NULL");

        return -1;
    }

    nimcp_mutex_lock(cc->mutex);

    cc->is_connected = false;
    cc->state = CALLOSUM_STATE_DISCONNECTED;
    cc->stats.disconnection_events++;
    cc->stats.state_changes++;

    NIMCP_LOGGING_INFO("Corpus callosum disconnected (split-brain mode)");

    nimcp_mutex_unlock(cc->mutex);
    return 0;
}

int callosum_reconnect(corpus_callosum_t* cc) {
    if (!cc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cc is NULL");

        return -1;
    }

    nimcp_mutex_lock(cc->mutex);

    cc->is_connected = true;
    cc->state = (cc->connection_strength >= 0.9f)
        ? CALLOSUM_STATE_HEALTHY
        : (cc->connection_strength >= 0.5f)
            ? CALLOSUM_STATE_DEGRADED
            : CALLOSUM_STATE_IMPAIRED;
    cc->stats.reconnection_events++;
    cc->stats.state_changes++;

    NIMCP_LOGGING_INFO("Corpus callosum reconnected (strength: %.2f)",
                       cc->connection_strength);

    nimcp_mutex_unlock(cc->mutex);
    return 0;
}

bool callosum_is_connected(const corpus_callosum_t* cc) {
    return cc ? cc->is_connected : false;
}

int callosum_set_connection_strength(corpus_callosum_t* cc, float strength) {
    if (!cc || strength < 0.0f || strength > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "callosum_set_connection_strength: cc is NULL or strength out of range");
        return -1;
    }

    nimcp_mutex_lock(cc->mutex);

    cc->connection_strength = strength;

    // Update state based on strength
    if (!cc->is_connected) {
        cc->state = CALLOSUM_STATE_DISCONNECTED;
    } else if (strength >= 0.9f) {
        cc->state = CALLOSUM_STATE_HEALTHY;
    } else if (strength >= 0.5f) {
        cc->state = CALLOSUM_STATE_DEGRADED;
    } else {
        cc->state = CALLOSUM_STATE_IMPAIRED;
    }

    nimcp_mutex_unlock(cc->mutex);
    return 0;
}

float callosum_get_connection_strength(const corpus_callosum_t* cc) {
    return cc ? cc->connection_strength : 0.0f;
}

callosum_state_t callosum_get_state(const corpus_callosum_t* cc) {
    return cc ? cc->state : CALLOSUM_STATE_DISCONNECTED;
}

//=============================================================================
// Channel Control
//=============================================================================

int callosum_set_channel_enabled(
    corpus_callosum_t* cc,
    callosum_channel_type_t channel,
    bool enable
) {
    if (!cc || channel >= CALLOSUM_CHANNEL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "callosum_set_channel_enabled: cc is NULL or invalid channel");
        return -1;
    }

    nimcp_mutex_lock(cc->mutex);
    cc->channels[channel].enabled = enable;
    nimcp_mutex_unlock(cc->mutex);

    return 0;
}

bool callosum_is_channel_enabled(
    const corpus_callosum_t* cc,
    callosum_channel_type_t channel
) {
    if (!cc || channel >= CALLOSUM_CHANNEL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "callosum_is_channel_enabled: cc is NULL");
        return false;
    }
    return cc->channels[channel].enabled;
}

//=============================================================================
// Statistics
//=============================================================================

int callosum_get_stats(const corpus_callosum_t* cc, callosum_stats_t* stats) {
    if (!cc || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "callosum_get_stats: cc or stats is NULL");
        return -1;
    }

    // Note: Must cast away const to lock mutex for thread-safe read
    // This is safe because we only read stats, not modify callosum state
    nimcp_mutex_lock((nimcp_mutex_t*)cc->mutex);
    memcpy(stats, &cc->stats, sizeof(callosum_stats_t));
    stats->current_bandwidth_utilization = callosum_get_bandwidth_utilization(cc);
    stats->current_state = cc->state;
    nimcp_mutex_unlock((nimcp_mutex_t*)cc->mutex);

    return 0;
}

int callosum_reset_stats(corpus_callosum_t* cc) {
    if (!cc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cc is NULL");

        return -1;
    }

    nimcp_mutex_lock(cc->mutex);
    memset(&cc->stats, 0, sizeof(callosum_stats_t));
    cc->stats.current_state = cc->state;
    nimcp_mutex_unlock(cc->mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int callosum_connect_bio_async(corpus_callosum_t* cc) {
    if (!cc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cc is NULL");

        return -1;
    }

    if (cc->bio_async_enabled) {
        return 0;  // Already connected
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_CORPUS_CALLOSUM,
        .module_name = "corpus_callosum",
        .inbox_capacity = 32,
        .user_data = cc
    };

    cc->bio_ctx = bio_router_register_module(&info);
    if (cc->bio_ctx) {
        cc->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Corpus callosum connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available for corpus callosum");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "callosum_connect_bio_async: validation failed");
    return -1;
}

int callosum_disconnect_bio_async(corpus_callosum_t* cc) {
    if (!cc || !cc->bio_async_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "callosum_disconnect_bio_async: cc is NULL or not connected");
        return -1;
    }

    bio_router_unregister_module(cc->bio_ctx);
    cc->bio_ctx = NULL;
    cc->bio_async_enabled = false;

    return 0;
}

bool callosum_is_bio_async_connected(const corpus_callosum_t* cc) {
    return cc ? cc->bio_async_enabled : false;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* callosum_channel_name(callosum_channel_type_t channel) {
    switch (channel) {
        case CALLOSUM_CHANNEL_MOTOR:
            return "Motor";
        case CALLOSUM_CHANNEL_SENSORY:
            return "Sensory";
        case CALLOSUM_CHANNEL_COGNITIVE:
            return "Cognitive";
        case CALLOSUM_CHANNEL_EMOTIONAL:
            return "Emotional";
        case CALLOSUM_CHANNEL_INHIBITORY:
            return "Inhibitory";
        default:
            return "Unknown";
    }
}

const char* callosum_bandwidth_mode_name(callosum_bandwidth_mode_t mode) {
    switch (mode) {
        case CALLOSUM_BW_UNLIMITED:
            return "Unlimited";
        case CALLOSUM_BW_REALISTIC:
            return "Realistic";
        case CALLOSUM_BW_RESTRICTED:
            return "Restricted";
        case CALLOSUM_BW_CUSTOM:
            return "Custom";
        default:
            return "Unknown";
    }
}

const char* callosum_state_name(callosum_state_t state) {
    switch (state) {
        case CALLOSUM_STATE_DISCONNECTED:
            return "Disconnected";
        case CALLOSUM_STATE_IMPAIRED:
            return "Impaired";
        case CALLOSUM_STATE_DEGRADED:
            return "Degraded";
        case CALLOSUM_STATE_HEALTHY:
            return "Healthy";
        default:
            return "Unknown";
    }
}
