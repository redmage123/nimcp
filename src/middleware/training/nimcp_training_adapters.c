#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_training_adapters.c - Training Layer Middleware Adapters Implementation
//=============================================================================

#include "middleware/training/nimcp_training_adapters.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"  // Phase MP: Memory pool for hot paths
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "security/nimcp_blood_brain_barrier.h"  // Phase IS-1: BBB perimeter defense
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_adapters)

// Phase IS-1: External declaration of BBB getter (avoid header conflicts)
extern bbb_system_t nimcp_bbb_get_global_system(void);

//=============================================================================
// Memory Pool Configuration (Phase MP)
//=============================================================================

/**
 * @brief Signal feature pool configuration
 *
 * WHAT: Memory pool for learning signal feature arrays
 * WHY:  Signals are allocated frequently in hot training path - O(1) vs O(log n)
 * HOW:  Pre-allocate blocks for typical feature sizes
 */
#define SIGNAL_POOL_BLOCK_SIZE 128   // Fits up to 32 floats per signal
#define SIGNAL_POOL_NUM_BLOCKS 1024  // 1024 signals in flight

static memory_pool_t g_signal_pool = NULL;
static nimcp_mutex_t g_signal_pool_mutex = NIMCP_MUTEX_INITIALIZER;

/**
 * @brief Initialize global signal memory pool
 *
 * WHAT: Lazily initialize memory pool for signal features
 * WHY:  Single pool shared across all adapters for efficiency
 * HOW:  Thread-safe initialization with double-checked locking
 */
static memory_pool_t get_signal_pool(void) {
    if (g_signal_pool == NULL) {
        nimcp_mutex_lock(&g_signal_pool_mutex);
        if (g_signal_pool == NULL) {
            memory_pool_config_t config = memory_pool_default_config(
                SIGNAL_POOL_BLOCK_SIZE, SIGNAL_POOL_NUM_BLOCKS);
            g_signal_pool = memory_pool_create(&config);
            if (g_signal_pool) {
                nimcp_log(LOG_LEVEL_INFO, "Signal memory pool created: %zu blocks x %zu bytes",
                          SIGNAL_POOL_NUM_BLOCKS, SIGNAL_POOL_BLOCK_SIZE);
            }
        }
        nimcp_mutex_unlock(&g_signal_pool_mutex);
    }
    return g_signal_pool;
}

/**
 * @brief Allocate signal features from pool or heap
 *
 * WHAT: Allocate memory for signal feature array
 * WHY:  Use pool for small allocations, fallback to heap for large
 * HOW:  Check size, try pool first, fallback to nimcp_calloc
 */
static float* alloc_signal_features(uint32_t num_features) {
    size_t size = num_features * sizeof(float);

    // Try pool for small allocations
    if (size <= SIGNAL_POOL_BLOCK_SIZE) {
        memory_pool_t pool = get_signal_pool();
        if (pool) {
            float* features = (float*)memory_pool_acquire(pool);
            if (features) {
                memset(features, 0, size);  // Zero-initialize
                return features;
            }
        }
    }

    // Fallback to heap for large allocations or pool exhausted
    return nimcp_calloc(num_features, sizeof(float));
}

/**
 * @brief Free signal features to pool or heap
 *
 * WHAT: Return memory for signal feature array
 * WHY:  Match allocation source for correct deallocation
 * HOW:  Check if from pool, release appropriately
 */
static void free_signal_features(float* features) {
    if (!features) return;

    memory_pool_t pool = get_signal_pool();
    if (pool && memory_pool_owns(pool, features)) {
        memory_pool_release(pool, features);
    } else {
        nimcp_free(features);
    }
}

//=============================================================================
// Training Event Data Extraction
//=============================================================================

/**
 * @brief Extract float from event data
 *
 * WHAT: Safely extract float value from event payload
 * WHY:  Type-safe data extraction from generic event
 * HOW:  Validates size and copies data
 */
static bool extract_float_from_event(const brain_event_t* event, size_t offset, float* out) {
    if (!event || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extract_float_from_event: required parameter is NULL (event, out)");
        return false;
    }
    if (offset + sizeof(float) > event->data.size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "extract_float_from_event: validation failed");
        return false;
    }
    memcpy(out, event->data.data + offset, sizeof(float));
    return true;
}

/**
 * @brief Extract uint32 from event data
 */
static bool extract_uint32_from_event(const brain_event_t* event, size_t offset, uint32_t* out) {
    if (!event || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extract_uint32_from_event: required parameter is NULL (event, out)");
        return false;
    }
    if (offset + sizeof(uint32_t) > event->data.size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "extract_uint32_from_event: validation failed");
        return false;
    }
    memcpy(out, event->data.data + offset, sizeof(uint32_t));
    return true;
}

//=============================================================================
// Learning Signal Adapter Implementation
//=============================================================================

/**
 * @brief Normalization statistics for adaptive normalization
 *
 * WHAT: Running statistics for online normalization
 * WHY:  Track feature statistics without storing history
 * HOW:  Welford's online algorithm for mean/variance
 */
typedef struct {
    float* running_mean;       /**< Running mean per feature */
    float* running_var;        /**< Running variance per feature */
    float* min_values;         /**< Min values per feature */
    float* max_values;         /**< Max values per feature */
    uint64_t sample_count;     /**< Total samples seen */
    uint32_t num_features;     /**< Feature dimensionality */
} normalization_stats_t;

/**
 * @brief Learning signal adapter structure
 */
struct learning_signal_adapter_struct {
    learning_signal_adapter_config_t config;
    normalization_stats_t* norm_stats;
    nimcp_mutex_t mutex;

    // Statistics
    uint64_t signals_extracted;
    uint64_t signals_normalized;
    uint64_t signals_dropped;
    float running_avg_magnitude;
    float running_avg_confidence;
};

/**
 * @brief Create normalization statistics
 *
 * WHAT: Allocate and initialize normalization statistics
 * WHY:  Track online statistics for adaptive normalization
 * HOW:  Allocate arrays for mean, variance, min, max
 */
static normalization_stats_t* create_norm_stats(uint32_t num_features) {
    if (num_features == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_norm_stats: num_features is zero");
        return NULL;
    }

    normalization_stats_t* stats = nimcp_calloc(1, sizeof(normalization_stats_t));
    if (!stats) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stats is NULL");

        return NULL;

    }

    stats->running_mean = nimcp_calloc(num_features, sizeof(float));
    stats->running_var = nimcp_calloc(num_features, sizeof(float));
    stats->min_values = nimcp_calloc(num_features, sizeof(float));
    stats->max_values = nimcp_calloc(num_features, sizeof(float));

    if (!stats->running_mean || !stats->running_var ||
        !stats->min_values || !stats->max_values) {
        nimcp_free(stats->running_mean);
        nimcp_free(stats->running_var);
        nimcp_free(stats->min_values);
        nimcp_free(stats->max_values);
        nimcp_free(stats);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_norm_stats: operation failed");
        return NULL;
    }

    // Initialize min/max
    for (uint32_t i = 0; i < num_features; i++) {
        stats->min_values[i] = INFINITY;
        stats->max_values[i] = -INFINITY;
    }

    stats->num_features = num_features;
    stats->sample_count = 0;

    return stats;
}

/**
 * @brief Destroy normalization statistics
 */
static void destroy_norm_stats(normalization_stats_t* stats) {
    if (!stats) return;

    nimcp_free(stats->running_mean);
    nimcp_free(stats->running_var);
    nimcp_free(stats->min_values);
    nimcp_free(stats->max_values);
    nimcp_free(stats);
}

learning_signal_adapter_t learning_signal_adapter_create(
    const learning_signal_adapter_config_t* config) {

    learning_signal_adapter_t adapter = nimcp_calloc(1,
        sizeof(struct learning_signal_adapter_struct));
    if (!adapter) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to allocate learning signal adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "destroy_norm_stats: adapter is NULL");
        return NULL;
    }

    // Use default config if not provided
    if (config) {
        adapter->config = *config;
    } else {
        adapter->config = learning_signal_adapter_default_config();
    }

    // Initialize mutex
    if (nimcp_mutex_init(&adapter->mutex, NULL) != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to initialize adapter mutex");
        nimcp_free(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "destroy_norm_stats: validation failed");
        return NULL;
    }

    nimcp_log(LOG_LEVEL_INFO, "Learning signal adapter created");
    return adapter;
}

void learning_signal_adapter_destroy(learning_signal_adapter_t adapter) {
    if (!adapter) return;

    destroy_norm_stats(adapter->norm_stats);
    nimcp_mutex_destroy(&adapter->mutex);
    nimcp_free(adapter);

    nimcp_log(LOG_LEVEL_INFO, "Learning signal adapter destroyed");
}

/**
 * @brief Extract features from error event
 *
 * WHAT: Convert error detection event to learning signal
 * WHY:  Error signals drive supervised learning
 * HOW:  Extract error magnitude and location as features
 */
static bool extract_error_signal(const brain_event_t* event,
                                  learning_signal_t* signal) {
    if (!event || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "learning_signal_adapter_destroy: required parameter is NULL (event, signal)");
        return false;
    }
    if (event->type != EVENT_ERROR_DETECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "learning_signal_adapter_destroy: validation failed");
        return false;
    }

    // Allocate feature vector from pool (Phase MP)
    signal->num_features = 3;
    signal->features = alloc_signal_features(signal->num_features);
    if (!signal->features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "learning_signal_adapter_destroy: signal->features is NULL");
        return false;
    }

    // Extract features from event data payload
    float expected_value = 0.0F, actual_value = 0.0F, error_magnitude = 0.0F;
    extract_float_from_event(event, 0, &expected_value);
    extract_float_from_event(event, sizeof(float), &actual_value);
    extract_float_from_event(event, 2 * sizeof(float), &error_magnitude);

    signal->features[0] = expected_value;
    signal->features[1] = actual_value;
    signal->features[2] = error_magnitude;

    signal->magnitude = error_magnitude;
    signal->confidence = 0.9F; // High confidence for error signals

    return true;
}

/**
 * @brief Extract features from cognitive events (attention, expectation mismatch)
 *
 * WHAT: Convert cognitive events to learning signals
 * WHY:  Cognitive events indicate important learning opportunities
 * HOW:  Extract event data as signal features
 */
static bool extract_cognitive_signal(const brain_event_t* event,
                                      learning_signal_t* signal) {
    if (!event || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "learning_signal_adapter_destroy: required parameter is NULL (event, signal)");
        return false;
    }

    // Support various cognitive event types
    if (event->type != EVENT_EXPECTATION_MISMATCH &&
        event->type != EVENT_ATTENTION_SHIFT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "learning_signal_adapter_destroy: required parameter is NULL (event, signal)");
        return false;
    }

    signal->num_features = 2;
    signal->features = alloc_signal_features(signal->num_features);
    if (!signal->features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "learning_signal_adapter_destroy: signal->features is NULL");
        return false;
    }

    // Extract features from event data
    float feature1 = 0.0F, feature2 = 0.0F;
    extract_float_from_event(event, 0, &feature1);
    extract_float_from_event(event, sizeof(float), &feature2);

    signal->features[0] = feature1;
    signal->features[1] = feature2;

    signal->magnitude = fabsf(feature1 - feature2);
    signal->confidence = 0.85F;

    return true;
}

bool learning_signal_adapter_extract(learning_signal_adapter_t adapter,
                                      const brain_event_t* event,
                                      learning_signal_t* signal) {
    if (!adapter || !event || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "learning_signal_adapter_destroy: required parameter is NULL (adapter, event, signal)");
        return false;
    }

    // Phase IS-1: BBB validation for event data
    if (event->data.data && event->data.size > 0) {
        bbb_system_t bbb = nimcp_bbb_get_global_system();
        if (bbb) {
            bbb_validation_result_t result;
            if (!bbb_validate_input(bbb, event->data.data, event->data.size, &result)) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "learning_signal_adapter_destroy: bbb_validate_input is NULL");
                return false;  // BBB rejected the training data
            }
        }
    }

    nimcp_mutex_lock(&adapter->mutex);

    // Initialize signal
    memset(signal, 0, sizeof(learning_signal_t));
    signal->timestamp_us = event->timestamp_us;
    signal->source_id = 0;  // No source ID in brain_event_t

    // Extract features based on event type
    bool extracted = false;
    switch (event->type) {
        case EVENT_ERROR_DETECTED:
        case EVENT_EXPECTATION_MISMATCH:
            signal->type = LEARNING_SIGNAL_ERROR;
            extracted = extract_error_signal(event, signal);
            break;

        case EVENT_ATTENTION_SHIFT:
        case EVENT_ATTENTION_COMPUTED:
            signal->type = LEARNING_SIGNAL_ATTENTION;
            extracted = extract_cognitive_signal(event, signal);
            break;

        case EVENT_EPISODIC_MEMORY_STORED:
        case EVENT_SEMANTIC_MEMORY_STORED:
        case EVENT_MEMORY_RECALLED:
        case EVENT_MEMORY_CONSOLIDATED:
            signal->type = LEARNING_SIGNAL_MEMORY;
            // Memory signals - simple extraction
            signal->num_features = 1;
            signal->features = alloc_signal_features(1);
            if (signal->features) {
                extract_float_from_event(event, 0, &signal->features[0]);
                signal->magnitude = signal->features[0];
                signal->confidence = 0.8F;
                extracted = true;
            }
            break;

        case EVENT_WEIGHT_UPDATE:
        case EVENT_PLASTICITY_UPDATE:
            signal->type = LEARNING_SIGNAL_REWARD;
            // Weight update as reward signal
            signal->num_features = 1;
            signal->features = alloc_signal_features(1);
            if (signal->features) {
                extract_float_from_event(event, 0, &signal->features[0]);
                signal->magnitude = fabsf(signal->features[0]);
                signal->confidence = 0.9F;
                extracted = true;
            }
            break;

        default:
            extracted = false;
            break;
    }

    if (!extracted) {
        adapter->signals_dropped++;
        nimcp_mutex_unlock(&adapter->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "learning_signal_adapter_destroy: extracted is NULL");
        return false;
    }

    // Apply confidence threshold
    if (signal->confidence < adapter->config.min_confidence_threshold) {
        learning_signal_free(signal);
        adapter->signals_dropped++;
        nimcp_mutex_unlock(&adapter->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "learning_signal_adapter_destroy: validation failed");
        return false;
    }

    // Update statistics
    adapter->signals_extracted++;
    adapter->running_avg_magnitude =
        0.95F * adapter->running_avg_magnitude + 0.05F * signal->magnitude;
    adapter->running_avg_confidence =
        0.95F * adapter->running_avg_confidence + 0.05F * signal->confidence;

    nimcp_mutex_unlock(&adapter->mutex);
    return true;
}

/**
 * @brief Update normalization statistics (Welford's algorithm)
 *
 * WHAT: Online update of mean and variance
 * WHY:  Avoid storing full history
 * HOW:  Welford's numerically stable algorithm
 */
static void update_norm_stats(normalization_stats_t* stats,
                               const float* features,
                               uint32_t num_features) {
    if (!stats || !features) return;

    // Initialize stats on first sample
    if (stats->num_features == 0) {
        stats->num_features = num_features;
        stats->running_mean = nimcp_calloc(num_features, sizeof(float));
        stats->running_var = nimcp_calloc(num_features, sizeof(float));
        stats->min_values = nimcp_calloc(num_features, sizeof(float));
        stats->max_values = nimcp_calloc(num_features, sizeof(float));

        for (uint32_t i = 0; i < num_features; i++) {
            stats->min_values[i] = INFINITY;
            stats->max_values[i] = -INFINITY;
        }
    }

    stats->sample_count++;

    for (uint32_t i = 0; i < num_features && i < stats->num_features; i++) {
        float value = features[i];

        // Update min/max
        if (value < stats->min_values[i]) stats->min_values[i] = value;
        if (value > stats->max_values[i]) stats->max_values[i] = value;

        // Welford's online algorithm for mean and variance
        float delta = value - stats->running_mean[i];
        stats->running_mean[i] += delta / (float)stats->sample_count;
        float delta2 = value - stats->running_mean[i];
        stats->running_var[i] += delta * delta2;
    }
}

bool learning_signal_adapter_normalize(learning_signal_adapter_t adapter,
                                        float* features,
                                        uint32_t num_features) {
    if (!adapter || !features || num_features == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, features)");
        return false;
    }

    nimcp_mutex_lock(&adapter->mutex);

    // Create normalization stats if needed
    if (!adapter->norm_stats) {
        adapter->norm_stats = create_norm_stats(num_features);
        if (!adapter->norm_stats) {
            nimcp_mutex_unlock(&adapter->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter->norm_stats is NULL");
            return false;
        }
    }

    // Update statistics
    update_norm_stats(adapter->norm_stats, features, num_features);

    // Apply normalization strategy
    switch (adapter->config.normalization) {
        case NORMALIZE_NONE:
            // No normalization
            break;

        case NORMALIZE_MIN_MAX:
            // Scale to [0, 1]
            for (uint32_t i = 0; i < num_features; i++) {
                float min = adapter->norm_stats->min_values[i];
                float max = adapter->norm_stats->max_values[i];
                float range = max - min;
                if (range > 1e-8F) {
                    features[i] = (features[i] - min) / range;
                }
            }
            break;

        case NORMALIZE_Z_SCORE:
            // Zero mean, unit variance
            if (adapter->norm_stats->sample_count > 1) {
                for (uint32_t i = 0; i < num_features; i++) {
                    float mean = adapter->norm_stats->running_mean[i];
                    float var = adapter->norm_stats->running_var[i] /
                               (adapter->norm_stats->sample_count - 1);
                    float std = sqrtf(var + 1e-8F);
                    features[i] = (features[i] - mean) / std;
                }
            }
            break;

        case NORMALIZE_L2:
            // Unit L2 norm
            float norm = 0.0F;
            for (uint32_t i = 0; i < num_features; i++) {
                norm += features[i] * features[i];
            }
            norm = sqrtf(norm + 1e-8F);
            for (uint32_t i = 0; i < num_features; i++) {
                features[i] /= norm;
            }
            break;

        case NORMALIZE_ADAPTIVE:
            // Adaptive normalization using running statistics
            if (adapter->norm_stats->sample_count > 10) {
                for (uint32_t i = 0; i < num_features; i++) {
                    float mean = adapter->norm_stats->running_mean[i];
                    float var = adapter->norm_stats->running_var[i] /
                               adapter->norm_stats->sample_count;
                    float std = sqrtf(var + 1e-8F);
                    features[i] = (features[i] - mean) / (std + 0.1F);
                }
            }
            break;
    }

    adapter->signals_normalized++;
    nimcp_mutex_unlock(&adapter->mutex);
    return true;
}

bool learning_signal_adapter_apply_attention(learning_signal_adapter_t adapter,
                                              learning_signal_t* signal,
                                              float attention_weight) {
    if (!adapter || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, signal)");
        return false;
    }
    if (!adapter->config.enable_attention_weighting) return true;

    // Guard clauses
    if (attention_weight < 0.0F) attention_weight = 0.0F;
    if (attention_weight > 1.0F) attention_weight = 1.0F;

    nimcp_mutex_lock(&adapter->mutex);

    // Scale features by attention weight
    for (uint32_t i = 0; i < signal->num_features; i++) {
        signal->features[i] *= attention_weight;
    }

    // Scale magnitude and confidence
    signal->magnitude *= attention_weight;
    signal->confidence *= (0.5F + 0.5F * attention_weight);

    nimcp_mutex_unlock(&adapter->mutex);
    return true;
}

void learning_signal_free(learning_signal_t* signal) {
    if (!signal) return;
    free_signal_features(signal->features);  // Phase MP: Return to pool if applicable
    signal->features = NULL;
    signal->num_features = 0;
}

bool learning_signal_adapter_get_stats(learning_signal_adapter_t adapter,
                                        learning_signal_adapter_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "learning_signal_free: required parameter is NULL (adapter, stats)");
        return false;
    }

    nimcp_mutex_lock(&adapter->mutex);

    stats->signals_extracted = adapter->signals_extracted;
    stats->signals_normalized = adapter->signals_normalized;
    stats->signals_dropped = adapter->signals_dropped;
    stats->avg_magnitude = adapter->running_avg_magnitude;
    stats->avg_confidence = adapter->running_avg_confidence;

    nimcp_mutex_unlock(&adapter->mutex);
    return true;
}

learning_signal_adapter_config_t learning_signal_adapter_default_config(void) {
    learning_signal_adapter_config_t config = {
        .normalization = NORMALIZE_ADAPTIVE,
        .learning_rate_scale = 1.0F,
        .enable_attention_weighting = true,
        .enable_novelty_boost = true,
        .novelty_boost_factor = 1.5F,
        .history_window_size = 100,
        .min_confidence_threshold = 0.1F
    };
    return config;
}

//=============================================================================
// Weight Update Router Implementation
//=============================================================================

/**
 * @brief Weight update router structure
 */
struct weight_update_router_struct {
    weight_update_router_config_t config;
    routing_table_t* routing_table;
    event_bus_t event_bus;
    bool owns_event_bus;
    nimcp_mutex_t mutex;

    // Statistics
    uint64_t updates_routed;
    uint64_t updates_dropped;
    uint64_t batch_operations;
};

weight_update_router_t weight_update_router_create(
    const weight_update_router_config_t* config,
    event_bus_t event_bus) {

    weight_update_router_t router = nimcp_calloc(1,
        sizeof(struct weight_update_router_struct));
    if (!router) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to allocate weight update router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "learning_signal_adapter_default_config: router is NULL");
        return NULL;
    }

    // Use default config if not provided
    if (config) {
        router->config = *config;
    } else {
        router->config = weight_update_router_default_config();
    }

    // Create or use provided event bus
    if (event_bus) {
        router->event_bus = event_bus;
        router->owns_event_bus = false;
    } else {
        // Create event bus for routing notifications
        router->event_bus = event_bus_create("weight_update_router", EVENT_DELIVERY_IMMEDIATE);
        router->owns_event_bus = true;

        if (!router->event_bus) {
            nimcp_log(LOG_LEVEL_ERROR, "Failed to create event bus");
            nimcp_free(router);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "learning_signal_adapter_default_config: router->event_bus is NULL");
            return NULL;
        }
    }

    // Create routing table
    routing_table_config_t table_config = routing_table_default_config();
    table_config.max_routes = router->config.routing_table_capacity;
    table_config.enable_learning = router->config.enable_dynamic_routing;
    table_config.learning_rate = router->config.route_learning_rate;

    router->routing_table = routing_table_create(&table_config);
    if (!router->routing_table) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to create routing table");
        if (router->owns_event_bus) {
            event_bus_destroy(router->event_bus);
        }
        nimcp_free(router);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "learning_signal_adapter_default_config: validation failed");
        return NULL;
    }

    // Initialize mutex
    if (nimcp_mutex_init(&router->mutex, NULL) != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to initialize router mutex");
        routing_table_destroy(router->routing_table);
        if (router->owns_event_bus) {
            event_bus_destroy(router->event_bus);
        }
        nimcp_free(router);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "learning_signal_adapter_default_config: validation failed");
        return NULL;
    }

    nimcp_log(LOG_LEVEL_INFO, "Weight update router created");
    return router;
}

void weight_update_router_destroy(weight_update_router_t router) {
    if (!router) return;

    routing_table_destroy(router->routing_table);

    if (router->owns_event_bus) {
        event_bus_destroy(router->event_bus);
    }

    nimcp_mutex_destroy(&router->mutex);
    nimcp_free(router);

    nimcp_log(LOG_LEVEL_INFO, "Weight update router destroyed");
}

/**
 * @brief Convert weight update to brain event
 *
 * WHAT: Package weight update as brain event
 * WHY:  Enable event bus delivery
 * HOW:  Create brain event with weight update payload
 */
static brain_event_t weight_update_to_event(const weight_update_t* update) {
    brain_event_t event = event_create(EVENT_WEIGHT_UPDATE, EVENT_PRIORITY_HIGH, "weight_router");
    event.timestamp_us = update->timestamp_us;

    // Package weight delta as event data
    if (event.data.size < sizeof(float)) {
        event_set_data(&event, &update->weight_delta, sizeof(float));
    }

    return event;
}

bool weight_update_router_route(weight_update_router_t router,
                                 const weight_update_t* update) {
    if (!router || !update) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "weight_update_to_event: required parameter is NULL (router, update)");
        return false;
    }

    nimcp_mutex_lock(&router->mutex);

    // Query routing table for target
    uint32_t source_id = (uint32_t)update->target_type;
    route_query_t query = {0};

    bool has_route = routing_table_query_routes(router->routing_table,
                                                 source_id, &query);

    if (!has_route || query.num_dests == 0) {
        router->updates_dropped++;
        nimcp_mutex_unlock(&router->mutex);
        routing_table_free_query(&query);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "weight_update_to_event: has_route is NULL");
        return false;
    }

    // Convert to brain event and publish
    brain_event_t event = weight_update_to_event(update);
    bool published = event_bus_publish(router->event_bus, &event);

    if (published) {
        router->updates_routed++;

        // Strengthen route (Hebbian learning)
        if (router->config.enable_dynamic_routing) {
            routing_table_use_route(router->routing_table,
                                   source_id, query.dest_ids[0]);
        }
    } else {
        router->updates_dropped++;
    }

    routing_table_free_query(&query);
    nimcp_mutex_unlock(&router->mutex);
    return published;
}

uint32_t weight_update_router_route_batch(weight_update_router_t router,
                                           const weight_update_t* updates,
                                           uint32_t num_updates) {
    if (!router || !updates || num_updates == 0) return 0;

    nimcp_mutex_lock(&router->mutex);

    uint32_t routed = 0;
    uint32_t batch_size = router->config.max_batch_size;

    for (uint32_t i = 0; i < num_updates; i += batch_size) {
        uint32_t end = (i + batch_size < num_updates) ?
                       i + batch_size : num_updates;

        for (uint32_t j = i; j < end; j++) {
            brain_event_t event = weight_update_to_event(&updates[j]);
            if (event_bus_publish(router->event_bus, &event)) {
                routed++;
            }
        }
    }

    router->updates_routed += routed;
    router->updates_dropped += (num_updates - routed);
    router->batch_operations++;

    nimcp_mutex_unlock(&router->mutex);
    return routed;
}

bool weight_update_router_add_route(weight_update_router_t router,
                                     learning_signal_type_t source_type,
                                     weight_target_type_t target_type,
                                     uint32_t priority) {
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "weight_update_to_event: router is NULL");
        return false;
    }

    nimcp_mutex_lock(&router->mutex);

    bool added = routing_table_add_route(router->routing_table,
                                        (uint32_t)source_type,
                                        (uint32_t)target_type,
                                        0.8F); // Initial strength

    if (added && router->config.enable_priority_routing) {
        routing_table_set_priority(router->routing_table,
                                   (uint32_t)source_type,
                                   (uint32_t)target_type,
                                   priority);
    }

    nimcp_mutex_unlock(&router->mutex);
    return added;
}

bool weight_update_router_remove_route(weight_update_router_t router,
                                        learning_signal_type_t source_type,
                                        weight_target_type_t target_type) {
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "weight_update_to_event: router is NULL");
        return false;
    }

    nimcp_mutex_lock(&router->mutex);

    bool removed = routing_table_remove_route(router->routing_table,
                                              (uint32_t)source_type,
                                              (uint32_t)target_type);

    nimcp_mutex_unlock(&router->mutex);
    return removed;
}

bool weight_update_router_strengthen_route(weight_update_router_t router,
                                            learning_signal_type_t source_type,
                                            weight_target_type_t target_type) {
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "weight_update_to_event: router is NULL");
        return false;
    }

    nimcp_mutex_lock(&router->mutex);

    bool strengthened = routing_table_use_route(router->routing_table,
                                               (uint32_t)source_type,
                                               (uint32_t)target_type);

    nimcp_mutex_unlock(&router->mutex);
    return strengthened;
}

bool weight_update_router_get_stats(weight_update_router_t router,
                                     weight_update_router_stats_t* stats) {
    if (!router || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "weight_update_to_event: required parameter is NULL (router, stats)");
        return false;
    }

    nimcp_mutex_lock(&router->mutex);

    stats->updates_routed = router->updates_routed;
    stats->updates_dropped = router->updates_dropped;
    stats->batch_operations = router->batch_operations;

    uint32_t num_routes = 0;
    routing_table_get_stats(router->routing_table, &num_routes, NULL, NULL);
    stats->active_routes = num_routes;

    stats->avg_routing_time_us = 10.0F; // Placeholder

    nimcp_mutex_unlock(&router->mutex);
    return true;
}

weight_update_router_config_t weight_update_router_default_config(void) {
    weight_update_router_config_t config = {
        .routing_table_capacity = 1000,
        .enable_dynamic_routing = true,
        .enable_priority_routing = true,
        .route_learning_rate = 0.01F,
        .max_batch_size = NIMCP_DEFAULT_BATCH_SIZE,
        .enable_update_coalescing = false
    };
    return config;
}

//=============================================================================
// Training Event Manager Implementation
//=============================================================================

/**
 * @brief Training event subscriber
 */
typedef struct training_subscriber {
    training_event_callback_fn callback;
    void* user_context;
    event_subscription_handle_t handle;
    struct training_subscriber* next;
} training_subscriber_t;

/**
 * @brief Training event manager structure
 */
struct training_event_manager_struct {
    training_event_manager_config_t config;
    event_bus_t event_bus;
    bool owns_event_bus;
    nimcp_mutex_t mutex;

    // Simple subscriber list
    training_subscriber_t* subscribers;
    uint32_t subscriber_count;

    // Statistics
    uint64_t events_published;
    uint64_t events_delivered;
    uint64_t events_dropped;
};

training_event_manager_t training_event_manager_create(
    const training_event_manager_config_t* config,
    event_bus_t event_bus) {

    training_event_manager_t manager = nimcp_calloc(1,
        sizeof(struct training_event_manager_struct));
    if (!manager) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to allocate training event manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "weight_update_router_default_config: manager is NULL");
        return NULL;
    }

    // Use default config if not provided
    if (config) {
        manager->config = *config;
    } else {
        manager->config = training_event_manager_default_config();
    }

    // Create or use provided event bus
    if (event_bus) {
        manager->event_bus = event_bus;
        manager->owns_event_bus = false;
    } else {
        // Create event bus with async delivery based on config
        event_delivery_mode_t delivery_mode = manager->config.enable_async_delivery ?
            EVENT_DELIVERY_ASYNC : EVENT_DELIVERY_IMMEDIATE;
        manager->event_bus = event_bus_create("training_event_manager", delivery_mode);
        manager->owns_event_bus = true;

        if (!manager->event_bus) {
            nimcp_log(LOG_LEVEL_ERROR, "Failed to create event bus");
            nimcp_free(manager);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "weight_update_router_default_config: manager->event_bus is NULL");
            return NULL;
        }
    }

    // Initialize subscriber list
    manager->subscribers = NULL;
    manager->subscriber_count = 0;

    // Initialize mutex
    if (nimcp_mutex_init(&manager->mutex, NULL) != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_ERROR, "Failed to initialize manager mutex");
        if (manager->owns_event_bus) {
            event_bus_destroy(manager->event_bus);
        }
        nimcp_free(manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "weight_update_router_default_config: validation failed");
        return NULL;
    }

    nimcp_log(LOG_LEVEL_INFO, "Training event manager created");
    return manager;
}

void training_event_manager_destroy(training_event_manager_t manager) {
    if (!manager) return;

    // Free subscriber list
    training_subscriber_t* sub = manager->subscribers;
    while (sub) {
        training_subscriber_t* next = sub->next;
        // Unsubscribe from event bus
        if (sub->handle != INVALID_SUBSCRIPTION_HANDLE) {
            event_bus_unsubscribe(manager->event_bus, sub->handle);
        }
        nimcp_free(sub);
        sub = next;
    }

    if (manager->owns_event_bus) {
        event_bus_destroy(manager->event_bus);
    }

    nimcp_mutex_destroy(&manager->mutex);
    nimcp_free(manager);

    nimcp_log(LOG_LEVEL_INFO, "Training event manager destroyed");
}

/**
 * @brief Convert training event to brain event
 *
 * WHAT: Package training event as brain event
 * WHY:  Enable event bus delivery
 * HOW:  Map training event type to brain event type
 */
static brain_event_type_t training_type_to_brain_type(training_event_type_t type) {
    switch (type) {
        case TRAINING_EVENT_EPOCH_START:
        case TRAINING_EVENT_EPOCH_END:
            return EVENT_TRAINING_EPOCH_COMPLETE;
        case TRAINING_EVENT_BATCH_START:
        case TRAINING_EVENT_BATCH_END:
            return EVENT_TRAINING_BATCH_COMPLETE;
        case TRAINING_EVENT_LOSS_UPDATE:
            return EVENT_LOSS_THRESHOLD_CROSSED;
        case TRAINING_EVENT_LR_UPDATE:
            return EVENT_LEARNING_RATE_CHANGE;
        case TRAINING_EVENT_CONVERGENCE:
            return EVENT_TRAINING_CONVERGED;
        case TRAINING_EVENT_DIVERGENCE:
            return EVENT_TRAINING_DIVERGED;
        case TRAINING_EVENT_CHECKPOINT:
            return EVENT_CHECKPOINT_CREATED;
        default:
            return EVENT_CUSTOM_USER;
    }
}

bool training_event_manager_publish(training_event_manager_t manager,
                                     const training_event_data_t* event) {
    if (!manager || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_type_to_brain_type: required parameter is NULL (manager, event)");
        return false;
    }

    nimcp_mutex_lock(&manager->mutex);

    // Convert to brain event
    brain_event_type_t brain_type = training_type_to_brain_type(event->type);
    brain_event_t brain_event = event_create(brain_type, EVENT_PRIORITY_NORMAL, "training_manager");
    brain_event.timestamp_us = event->timestamp_us;

    // Pack training data into event payload
    struct {
        uint32_t epoch;
        uint32_t batch;
        float loss;
        float learning_rate;
    } payload = {
        .epoch = event->epoch,
        .batch = event->batch,
        .loss = event->loss,
        .learning_rate = event->learning_rate
    };
    event_set_data(&brain_event, &payload, sizeof(payload));

    // Publish directly to event bus
    bool published = event_bus_publish(manager->event_bus, &brain_event);

    if (published) {
        manager->events_published++;
    } else {
        manager->events_dropped++;
    }

    nimcp_mutex_unlock(&manager->mutex);
    return published;
}

/**
 * @brief Training event callback wrapper
 *
 * WHAT: Adapter between brain event callback and training callback
 * WHY:  Allow training-specific callback signature
 * HOW:  Extract training data and call user callback
 */
static void training_event_callback_wrapper(const brain_event_t* event, void* context) {
    if (!event || !context) return;

    training_subscriber_t* sub = (training_subscriber_t*)context;

    // Reconstruct training event from brain event data
    training_event_data_t train_event = {0};
    train_event.timestamp_us = event->timestamp_us;

    // Extract training data from payload
    if (event->data.size >= sizeof(uint32_t) * 2 + sizeof(float) * 2) {
        memcpy(&train_event.epoch, event->data.data, sizeof(uint32_t));
        memcpy(&train_event.batch, event->data.data + sizeof(uint32_t), sizeof(uint32_t));
        memcpy(&train_event.loss, event->data.data + sizeof(uint32_t) * 2, sizeof(float));
        memcpy(&train_event.learning_rate, event->data.data + sizeof(uint32_t) * 2 + sizeof(float), sizeof(float));
    }

    // Map brain event type back to training event type
    switch (event->type) {
        case EVENT_TRAINING_EPOCH_COMPLETE:
            train_event.type = TRAINING_EVENT_EPOCH_END;
            break;
        case EVENT_TRAINING_BATCH_COMPLETE:
            train_event.type = TRAINING_EVENT_BATCH_END;
            break;
        case EVENT_LOSS_THRESHOLD_CROSSED:
            train_event.type = TRAINING_EVENT_LOSS_UPDATE;
            break;
        case EVENT_LEARNING_RATE_CHANGE:
            train_event.type = TRAINING_EVENT_LR_UPDATE;
            break;
        case EVENT_TRAINING_CONVERGED:
            train_event.type = TRAINING_EVENT_CONVERGENCE;
            break;
        case EVENT_TRAINING_DIVERGED:
            train_event.type = TRAINING_EVENT_DIVERGENCE;
            break;
        case EVENT_CHECKPOINT_CREATED:
            train_event.type = TRAINING_EVENT_CHECKPOINT;
            break;
        default:
            return; // Unknown event type
    }

    sub->callback(&train_event, sub->user_context);
}

event_subscription_handle_t training_event_manager_subscribe(
    training_event_manager_t manager,
    training_event_callback_fn callback,
    void* context,
    const training_event_type_t* event_types,
    uint32_t num_types) {

    if (!manager || !callback) return INVALID_SUBSCRIPTION_HANDLE;

    nimcp_mutex_lock(&manager->mutex);

    // Create subscriber
    training_subscriber_t* sub = nimcp_calloc(1, sizeof(training_subscriber_t));
    if (!sub) {
        nimcp_mutex_unlock(&manager->mutex);
        return INVALID_SUBSCRIPTION_HANDLE;
    }

    sub->callback = callback;
    sub->user_context = context;

    // Subscribe to all training events on event bus
    // (we filter by type in the wrapper)
    sub->handle = event_bus_subscribe(
        manager->event_bus,
        EVENT_ALL,  // Subscribe to all, filter in wrapper
        training_event_callback_wrapper,
        sub);

    if (sub->handle == INVALID_SUBSCRIPTION_HANDLE) {
        nimcp_free(sub);
        nimcp_mutex_unlock(&manager->mutex);
        return INVALID_SUBSCRIPTION_HANDLE;
    }

    // Add to subscriber list
    sub->next = manager->subscribers;
    manager->subscribers = sub;
    manager->subscriber_count++;

    nimcp_mutex_unlock(&manager->mutex);
    return sub->handle;
}

bool training_event_manager_unsubscribe(training_event_manager_t manager,
                                         event_subscription_handle_t handle) {
    if (!manager || handle == INVALID_SUBSCRIPTION_HANDLE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_event_callback_wrapper: manager is NULL");
        return false;
    }

    nimcp_mutex_lock(&manager->mutex);

    // Find and remove subscriber
    training_subscriber_t* prev = NULL;
    training_subscriber_t* sub = manager->subscribers;

    while (sub) {
        if (sub->handle == handle) {
            // Unsubscribe from event bus
            event_bus_unsubscribe(manager->event_bus, handle);

            // Remove from list
            if (prev) {
                prev->next = sub->next;
            } else {
                manager->subscribers = sub->next;
            }

            nimcp_free(sub);
            manager->subscriber_count--;

            nimcp_mutex_unlock(&manager->mutex);
            return true;
        }
        prev = sub;
        sub = sub->next;
    }

    nimcp_mutex_unlock(&manager->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_event_callback_wrapper: operation failed");
    return false;
}

uint32_t training_event_manager_process_events(training_event_manager_t manager,
                                                uint32_t max_events) {
    if (!manager || max_events == 0) return 0;

    // With core event bus, events are delivered immediately or via async thread
    // This function is now a no-op for compatibility
    // Could flush the event bus if needed
    nimcp_mutex_lock(&manager->mutex);
    uint32_t flushed = event_bus_flush(manager->event_bus);
    manager->events_delivered += flushed;
    nimcp_mutex_unlock(&manager->mutex);

    return flushed;
}

bool training_event_manager_get_stats(training_event_manager_t manager,
                                       training_event_manager_stats_t* stats) {
    if (!manager || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_event_callback_wrapper: required parameter is NULL (manager, stats)");
        return false;
    }

    nimcp_mutex_lock(&manager->mutex);

    stats->events_published = manager->events_published;
    stats->events_delivered = manager->events_delivered;
    stats->events_dropped = manager->events_dropped;
    stats->active_subscribers = manager->subscriber_count;
    stats->queue_size = event_bus_get_pending_count(manager->event_bus);

    nimcp_mutex_unlock(&manager->mutex);
    return true;
}

training_event_manager_config_t training_event_manager_default_config(void) {
    training_event_manager_config_t config = {
        .event_queue_capacity = 1024,
        .enable_async_delivery = true,
        .enable_event_batching = false,
        .batch_timeout_ms = 10,
        .enable_priority_scheduling = true
    };
    return config;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* learning_signal_type_name(learning_signal_type_t type) {
    switch (type) {
        case LEARNING_SIGNAL_ERROR:     return "Error";
        case LEARNING_SIGNAL_REWARD:    return "Reward";
        case LEARNING_SIGNAL_SURPRISE:  return "Surprise";
        case LEARNING_SIGNAL_ATTENTION: return "Attention";
        case LEARNING_SIGNAL_MEMORY:    return "Memory";
        case LEARNING_SIGNAL_CUSTOM:    return "Custom";
        default:                        return "Unknown";
    }
}

const char* weight_target_type_name(weight_target_type_t type) {
    switch (type) {
        case WEIGHT_TARGET_CORTICAL:     return "Cortical";
        case WEIGHT_TARGET_SUBCORTICAL:  return "Subcortical";
        case WEIGHT_TARGET_HIPPOCAMPAL:  return "Hippocampal";
        case WEIGHT_TARGET_STRIATAL:     return "Striatal";
        case WEIGHT_TARGET_THALAMIC:     return "Thalamic";
        case WEIGHT_TARGET_CEREBELLAR:   return "Cerebellar";
        case WEIGHT_TARGET_CUSTOM:       return "Custom";
        default:                         return "Unknown";
    }
}

const char* training_event_type_name(training_event_type_t type) {
    switch (type) {
        case TRAINING_EVENT_EPOCH_START:  return "Epoch Start";
        case TRAINING_EVENT_EPOCH_END:    return "Epoch End";
        case TRAINING_EVENT_BATCH_START:  return "Batch Start";
        case TRAINING_EVENT_BATCH_END:    return "Batch End";
        case TRAINING_EVENT_LOSS_UPDATE:  return "Loss Update";
        case TRAINING_EVENT_LR_UPDATE:    return "LR Update";
        case TRAINING_EVENT_CONVERGENCE:  return "Convergence";
        case TRAINING_EVENT_DIVERGENCE:   return "Divergence";
        case TRAINING_EVENT_CHECKPOINT:   return "Checkpoint";
        default:                          return "Unknown";
    }
}
