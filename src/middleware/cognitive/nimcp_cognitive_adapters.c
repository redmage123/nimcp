#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_cognitive_adapters.c - Cognitive Layer Middleware Adapters
//=============================================================================

#include "middleware/cognitive/nimcp_cognitive_adapters.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"



#define LOG_MODULE "nimcp_cognitive_adapters"
#define LOG_MODULE_ID 0x0515
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cognitive_adapters)

//=============================================================================
// WORKING MEMORY ADAPTER IMPLEMENTATION
//=============================================================================

/**
 * @brief Working memory adapter internal structure
 *
 * WHAT: Hidden implementation details
 * WHY:  Encapsulation and flexibility
 */
struct wm_adapter {
    wm_adapter_config_t config;     /**< Configuration */
    wm_item_t* items;               /**< Item array */
    sliding_window_t* window;       /**< Sliding window buffer */
    attention_gate_t* gate;         /**< Attention gate */
    wm_adapter_stats_t stats;       /**< Statistics */
    uint64_t last_decay_time_us;    /**< Last decay update */
};

/**
 * @brief Get default WM adapter configuration
 *
 * WHAT: Return sensible defaults
 * WHY:  Quick initialization
 * HOW:  Return struct with default values
 */
wm_adapter_config_t wm_adapter_default_config(void) {
    wm_adapter_config_t config = {
        .mode = WM_MODE_HYBRID,
        .capacity = COGNITIVE_WM_DEFAULT_CAPACITY,
        .window_size = 100,
        .attention_threshold = 0.3F,
        .enable_decay = true,
        .decay_tau_ms = 5000.0F
    };
    return config;
}

/**
 * @brief Create working memory adapter
 */
wm_adapter_t* wm_adapter_create(const wm_adapter_config_t* config) {
    if (!config) {
        wm_adapter_config_t default_config = wm_adapter_default_config();
        config = &default_config;
    }

    wm_adapter_t* adapter = nimcp_calloc(1, sizeof(wm_adapter_t));
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_adapter_create: failed to allocate adapter");
        return NULL;
    }

    adapter->config = *config;

    // Allocate item array
    adapter->items = nimcp_calloc(config->capacity, sizeof(wm_item_t));
    if (!adapter->items) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_adapter_create: failed to allocate items");
        nimcp_free(adapter);
        return NULL;
    }

    // Create sliding window if needed
    if (config->mode == WM_MODE_SLIDING || config->mode == WM_MODE_HYBRID) {
        adapter->window = sliding_window_create(config->window_size, 50);
        if (!adapter->window) {
            nimcp_free(adapter->items);
            nimcp_free(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_adapter_create: adapter->window is NULL");
            return NULL;
        }
    }

    // Create attention gate
    attention_gate_config_t gate_config = attention_gate_default_config();
    gate_config.max_targets = config->capacity;
    gate_config.spotlight_size = COGNITIVE_ATTENTION_SPOTLIGHT_SIZE;
    gate_config.mode = (config->mode == WM_MODE_ATTENTION_GATED) ?
                       ATTENTION_MODE_TOPDOWN : ATTENTION_MODE_MIXED;

    adapter->gate = attention_gate_create(&gate_config);
    if (!adapter->gate) {
        if (adapter->window) sliding_window_destroy(adapter->window);
        nimcp_free(adapter->items);
        nimcp_free(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_adapter_create: validation failed");
        return NULL;
    }

    adapter->last_decay_time_us = cognitive_adapter_get_timestamp_us();

    return adapter;
}

/**
 * @brief Destroy working memory adapter
 */
void wm_adapter_destroy(wm_adapter_t* adapter) {
    if (!adapter) return;

    // Free item data
    for (uint32_t i = 0; i < adapter->config.capacity; i++) {
        if (adapter->items[i].data) {
            nimcp_free(adapter->items[i].data);
        }
    }

    nimcp_free(adapter->items);
    if (adapter->window) sliding_window_destroy(adapter->window);
    if (adapter->gate) attention_gate_destroy(adapter->gate);
    nimcp_free(adapter);
}

/**
 * @brief Find item by ID
 *
 * WHAT: Linear search for item
 * WHY:  Simple, fast for small capacity
 * HOW:  Scan items array
 */
static int32_t wm_find_item(const wm_adapter_t* adapter, uint32_t item_id) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_find_item: adapter is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < adapter->config.capacity; i++) {
        if (adapter->items[i].is_active &&
            adapter->items[i].item_id == item_id) {
            return (int32_t)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_find_item: adapter is NULL");
    return -1;
}

/**
 * @brief Find slot with lowest salience
 *
 * WHAT: Identify eviction candidate
 * WHY:  Evict least important item
 * HOW:  Linear scan for minimum salience
 */
static int32_t wm_find_lowest_salience(const wm_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_find_lowest_salience: adapter is NULL");
        return -1;
    }

    int32_t min_idx = -1;
    float min_salience = 2.0F; // Higher than max possible (1.0)

    for (uint32_t i = 0; i < adapter->config.capacity; i++) {
        if (adapter->items[i].is_active &&
            adapter->items[i].salience < min_salience) {
            min_salience = adapter->items[i].salience;
            min_idx = (int32_t)i;
        }
    }
    return min_idx;
}

/**
 * @brief Add item to working memory
 */
bool wm_adapter_add_item(wm_adapter_t* adapter,
                          uint32_t item_id,
                          const float* data,
                          size_t data_size,
                          float salience) {
    if (!adapter || !data || data_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_find_lowest_salience: required parameter is NULL (adapter, data)");
        return false;
    }

    // Check salience threshold
    if (salience < adapter->config.attention_threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wm_find_lowest_salience: validation failed");
        return false;
    }

    // Find existing item or free slot
    int32_t idx = wm_find_item(adapter, item_id);
    if (idx < 0) {
        // Find free slot
        for (uint32_t i = 0; i < adapter->config.capacity; i++) {
            if (!adapter->items[i].is_active) {
                idx = (int32_t)i;
                break;
            }
        }

        // If still no slot, evict lowest salience
        if (idx < 0) {
            idx = wm_find_lowest_salience(adapter);
            if (idx >= 0) {
                // Free evicted item data
                if (adapter->items[idx].data) {
                    nimcp_free(adapter->items[idx].data);
                }
                adapter->stats.total_evicted++;
            }
        }
    }

    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wm_find_lowest_salience: validation failed");
        return false;
    }

    // Allocate and copy data
    float* item_data = nimcp_malloc(data_size * sizeof(float));
    if (!item_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_find_lowest_salience: item_data is NULL");
        return false;
    }
    memcpy(item_data, data, data_size * sizeof(float));

    // Initialize item
    adapter->items[idx].item_id = item_id;
    adapter->items[idx].data = item_data;
    adapter->items[idx].data_size = data_size;
    adapter->items[idx].salience = salience;
    adapter->items[idx].timestamp_us = cognitive_adapter_get_timestamp_us();
    adapter->items[idx].is_active = true;

    // Update attention gate
    attention_gate_set_weight(adapter->gate, 0, item_id, salience);

    // Update statistics
    adapter->stats.total_added++;

    return true;
}

/**
 * @brief Update item attention weight
 */
bool wm_adapter_set_attention(wm_adapter_t* adapter,
                                uint32_t item_id,
                                float attention_weight) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_find_lowest_salience: adapter is NULL");
        return false;
    }

    int32_t idx = wm_find_item(adapter, item_id);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wm_find_lowest_salience: validation failed");
        return false;
    }

    // Update attention gate
    if (!attention_gate_set_weight(adapter->gate, 0, item_id, attention_weight)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wm_find_lowest_salience: attention_gate_set_weight is NULL");
        return false;
    }

    // Get combined weight
    float combined_weight = 0.0F;
    attention_gate_get_weight(adapter->gate, 0, item_id, &combined_weight);

    adapter->items[idx].salience = combined_weight;

    return true;
}

/**
 * @brief Get item from working memory
 */
const wm_item_t* wm_adapter_get_item(const wm_adapter_t* adapter,
                                      uint32_t item_id) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_find_lowest_salience: adapter is NULL");
        return NULL;
    }

    int32_t idx = wm_find_item(adapter, item_id);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_find_lowest_salience: validation failed");
        return NULL;
    }

    return &adapter->items[idx];
}

/**
 * @brief Get all active items
 */
uint32_t wm_adapter_get_all_items(const wm_adapter_t* adapter,
                                   const wm_item_t** items,
                                   uint32_t max_items) {
    if (!adapter || !items) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < adapter->config.capacity && count < max_items; i++) {
        if (adapter->items[i].is_active) {
            items[count++] = &adapter->items[i];
        }
    }

    return count;
}

/**
 * @brief Remove item from working memory
 */
bool wm_adapter_remove_item(wm_adapter_t* adapter, uint32_t item_id) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_adapter_remove_item: adapter is NULL");
        return false;
    }

    int32_t idx = wm_find_item(adapter, item_id);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wm_adapter_remove_item: validation failed");
        return false;
    }

    if (adapter->items[idx].data) {
        nimcp_free(adapter->items[idx].data);
        adapter->items[idx].data = NULL;
    }

    adapter->items[idx].is_active = false;

    return true;
}

/**
 * @brief Clear all items
 */
void wm_adapter_clear(wm_adapter_t* adapter) {
    if (!adapter) return;

    for (uint32_t i = 0; i < adapter->config.capacity; i++) {
        if (adapter->items[i].data) {
            nimcp_free(adapter->items[i].data);
            adapter->items[i].data = NULL;
        }
        adapter->items[i].is_active = false;
    }

    if (adapter->gate) {
        attention_gate_reset(adapter->gate);
    }
}

/**
 * @brief Update temporal decay
 */
void wm_adapter_update_decay(wm_adapter_t* adapter, uint64_t dt) {
    if (!adapter || !adapter->config.enable_decay) return;

    float decay_factor = expf(-(float)dt / (adapter->config.decay_tau_ms * 1000.0F));

    for (uint32_t i = 0; i < adapter->config.capacity; i++) {
        if (adapter->items[i].is_active) {
            adapter->items[i].salience *= decay_factor;

            // Remove if below threshold
            if (adapter->items[i].salience < adapter->config.attention_threshold) {
                wm_adapter_remove_item(adapter, adapter->items[i].item_id);
            }
        }
    }
}

/**
 * @brief Get adapter statistics
 */
bool wm_adapter_get_stats(const wm_adapter_t* adapter,
                           wm_adapter_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_adapter_update_decay: required parameter is NULL (adapter, stats)");
        return false;
    }

    uint32_t active_count = 0;
    float total_salience = 0.0F;

    for (uint32_t i = 0; i < adapter->config.capacity; i++) {
        if (adapter->items[i].is_active) {
            active_count++;
            total_salience += adapter->items[i].salience;
        }
    }

    stats->current_items = active_count;
    stats->total_added = adapter->stats.total_added;
    stats->total_evicted = adapter->stats.total_evicted;
    stats->avg_salience = (active_count > 0) ?
                          (total_salience / active_count) : 0.0F;
    stats->capacity_utilization = (float)active_count / adapter->config.capacity;

    return true;
}

//=============================================================================
// CONSOLIDATION ADAPTER IMPLEMENTATION
//=============================================================================

/**
 * @brief Consolidation adapter internal structure
 */
struct consol_adapter {
    consol_adapter_config_t config;     /**< Configuration */
    integration_buffer_t* buffer;       /**< Multi-timescale buffer */
    temporal_accumulator_t* accumulator; /**< Temporal accumulator */
    consol_adapter_stats_t stats;       /**< Statistics */
};

/**
 * @brief Get default consolidation adapter configuration
 */
consol_adapter_config_t consol_adapter_default_config(void) {
    consol_adapter_config_t config = {
        .strategy = CONSOL_STRATEGY_WEIGHTED,
        .fast_size = 100,
        .medium_size = 50,
        .slow_size = 20,
        .num_channels = 1,
        .alpha = 0.1F,
        .enable_normalization = true
    };
    return config;
}

/**
 * @brief Create consolidation adapter
 */
consol_adapter_t* consol_adapter_create(const consol_adapter_config_t* config) {
    if (!config) {
        consol_adapter_config_t default_config = consol_adapter_default_config();
        config = &default_config;
    }

    consol_adapter_t* adapter = nimcp_calloc(1, sizeof(consol_adapter_t));
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "consol_adapter_create: adapter is NULL");
        return NULL;
    }

    adapter->config = *config;

    // Create integration buffer
    adapter->buffer = integration_buffer_create(
        config->fast_size,
        config->medium_size,
        config->slow_size,
        config->num_channels
    );

    if (!adapter->buffer) {
        nimcp_free(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consol_adapter_create: adapter->buffer is NULL");
        return NULL;
    }

    // Create temporal accumulator
    adapter->accumulator = temporal_accumulator_create(
        config->num_channels,
        config->alpha,
        INTEGRATION_EMA
    );

    if (!adapter->accumulator) {
        integration_buffer_destroy(adapter->buffer);
        nimcp_free(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consol_adapter_create: adapter->accumulator is NULL");
        return NULL;
    }

    return adapter;
}

/**
 * @brief Destroy consolidation adapter
 */
void consol_adapter_destroy(consol_adapter_t* adapter) {
    if (!adapter) return;

    if (adapter->buffer) {
        integration_buffer_destroy(adapter->buffer);
    }
    if (adapter->accumulator) {
        temporal_accumulator_destroy(adapter->accumulator);
    }

    nimcp_free(adapter);
}

/**
 * @brief Update consolidation with new data
 */
bool consol_adapter_update(consol_adapter_t* adapter,
                            size_t channel,
                            float value,
                            uint64_t timestamp) {
    if (!adapter || channel >= adapter->config.num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consol_adapter_destroy: adapter is NULL");
        return false;
    }

    // Add to integration buffer
    if (!integration_buffer_add(adapter->buffer, channel, value, timestamp)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consol_adapter_destroy: integration_buffer_add is NULL");
        return false;
    }

    // Update temporal accumulator
    if (!temporal_accumulator_update(adapter->accumulator, channel, value, 1.0F)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consol_adapter_destroy: temporal_accumulator_update is NULL");
        return false;
    }

    adapter->stats.total_updates++;

    return true;
}

/**
 * @brief Get consolidated value at timescale
 */
float consol_adapter_get_value(const consol_adapter_t* adapter,
                                 timescale_level_t level,
                                 size_t channel) {
    if (!adapter || channel >= adapter->config.num_channels) {
        return 0.0F;
    }

    return integration_buffer_get_latest(adapter->buffer, level, channel);
}

/**
 * @brief Apply consolidation strategy
 *
 * WHAT: Combine timescales using strategy
 * WHY:  Different tasks need different integration
 * HOW:  Switch on strategy type
 */
static float apply_consolidation_strategy(const consol_adapter_t* adapter,
                                           float fast, float medium, float slow) {
    switch (adapter->config.strategy) {
        case CONSOL_STRATEGY_AVERAGE:
            return (fast + medium + slow) / 3.0F;

        case CONSOL_STRATEGY_WEIGHTED:
            // Weight recent more heavily
            return 0.5F * fast + 0.3F * medium + 0.2F * slow;

        case CONSOL_STRATEGY_THRESHOLD:
            // Use fast if variance low, else average
            if (fabsf(fast - medium) < 0.1F) {
                return fast;
            }
            return (fast + medium + slow) / 3.0F;

        case CONSOL_STRATEGY_ADAPTIVE:
            // Adapt based on variance
            {
                float variance = (fast - medium) * (fast - medium) +
                               (medium - slow) * (medium - slow);
                float weight_fast = expf(-variance);
                return weight_fast * fast + (1.0F - weight_fast) * medium;
            }

        default:
            return fast;
    }
}

/**
 * @brief Get consolidation across all timescales
 */
float consol_adapter_get_consolidated(const consol_adapter_t* adapter,
                                        size_t channel) {
    if (!adapter || channel >= adapter->config.num_channels) {
        return 0.0F;
    }

    float fast = integration_buffer_get_latest(adapter->buffer,
                                                TIMESCALE_FAST, channel);
    float medium = integration_buffer_get_latest(adapter->buffer,
                                                  TIMESCALE_MEDIUM, channel);
    float slow = integration_buffer_get_latest(adapter->buffer,
                                                TIMESCALE_SLOW, channel);

    return apply_consolidation_strategy(adapter, fast, medium, slow);
}

/**
 * @brief Get temporal trend
 */
float consol_adapter_get_trend(const consol_adapter_t* adapter,
                                 size_t channel) {
    if (!adapter || channel >= adapter->config.num_channels) {
        return 0.0F;
    }

    return integration_buffer_trend(adapter->buffer, channel);
}

/**
 * @brief Normalize channel data
 */
float consol_adapter_normalize(const consol_adapter_t* adapter,
                                 size_t channel) {
    if (!adapter || channel >= adapter->config.num_channels) {
        return 0.0F;
    }

    float mean = integration_buffer_mean(adapter->buffer,
                                          TIMESCALE_FAST, channel);
    float variance = integration_buffer_variance(adapter->buffer,
                                                  TIMESCALE_FAST, channel);
    float value = integration_buffer_get_latest(adapter->buffer,
                                                 TIMESCALE_FAST, channel);

    if (variance < 1e-6F) return 0.0F;

    return (value - mean) / sqrtf(variance);
}

/**
 * @brief Clear consolidation buffers
 */
void consol_adapter_clear(consol_adapter_t* adapter) {
    if (!adapter) return;

    if (adapter->buffer) {
        integration_buffer_clear(adapter->buffer);
    }
    if (adapter->accumulator) {
        temporal_accumulator_reset_all(adapter->accumulator);
    }
}

/**
 * @brief Get adapter statistics
 */
bool consol_adapter_get_stats(const consol_adapter_t* adapter,
                               consol_adapter_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consol_adapter_clear: required parameter is NULL (adapter, stats)");
        return false;
    }

    stats->total_updates = adapter->stats.total_updates;

    // Calculate activity levels
    float fast_sum = 0.0F, medium_sum = 0.0F, slow_sum = 0.0F;
    for (size_t ch = 0; ch < adapter->config.num_channels; ch++) {
        fast_sum += fabsf(integration_buffer_get_latest(
            adapter->buffer, TIMESCALE_FAST, ch));
        medium_sum += fabsf(integration_buffer_get_latest(
            adapter->buffer, TIMESCALE_MEDIUM, ch));
        slow_sum += fabsf(integration_buffer_get_latest(
            adapter->buffer, TIMESCALE_SLOW, ch));
    }

    float n = (float)adapter->config.num_channels;
    stats->fast_activity = fast_sum / n;
    stats->medium_activity = medium_sum / n;
    stats->slow_activity = slow_sum / n;

    // Integration quality: how consistent are timescales?
    float consistency = 1.0F - fabsf(stats->fast_activity - stats->slow_activity)
                              / (stats->fast_activity + stats->slow_activity + 1e-6F);
    stats->integration_quality = fmaxf(0.0F, fminf(1.0F, consistency));

    return true;
}

//=============================================================================
// ATTENTION ADAPTER IMPLEMENTATION
//=============================================================================

/**
 * @brief Attention adapter internal structure
 */
struct attention_adapter {
    attention_adapter_config_t config;  /**< Configuration */
    attention_gate_t* gate;             /**< Attention gate */
    attention_adapter_stats_t stats;    /**< Statistics */
    uint32_t pattern_count;             /**< Patterns detected */
};

/**
 * @brief Get default attention adapter configuration
 */
attention_adapter_config_t attention_adapter_default_config(void) {
    attention_adapter_config_t config = {
        .mode = ATTENTION_CONTROL_MIXED,
        .max_targets = COGNITIVE_MAX_CHANNELS,
        .spotlight_size = COGNITIVE_ATTENTION_SPOTLIGHT_SIZE,
        .enable_wta = false,
        .enable_pattern_detection = true,
        .pattern_threshold = COGNITIVE_PATTERN_THRESHOLD
    };
    return config;
}

/**
 * @brief Create attention adapter
 */
attention_adapter_t* attention_adapter_create(
    const attention_adapter_config_t* config) {

    if (!config) {
        attention_adapter_config_t default_config = attention_adapter_default_config();
        config = &default_config;
    }

    attention_adapter_t* adapter = nimcp_calloc(1, sizeof(attention_adapter_t));
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_adapter_default_config: adapter is NULL");
        return NULL;
    }

    adapter->config = *config;

    // Create attention gate
    attention_gate_config_t gate_config = attention_gate_default_config();
    gate_config.max_targets = config->max_targets;
    gate_config.spotlight_size = config->spotlight_size;
    gate_config.enable_winner_take_all = config->enable_wta;
    gate_config.enable_shift_detection = true;

    switch (config->mode) {
        case ATTENTION_CONTROL_TOPDOWN:
            gate_config.mode = ATTENTION_MODE_TOPDOWN;
            break;
        case ATTENTION_CONTROL_BOTTOMUP:
            gate_config.mode = ATTENTION_MODE_BOTTOMUP;
            break;
        default:
            gate_config.mode = ATTENTION_MODE_MIXED;
            // For cognitive adapter, we want weights to pass through unchanged
            // User sets explicit weights that should be used as-is
            gate_config.topdown_weight = 1.0F;
            gate_config.bottomup_weight = 1.0F;
            break;
    }

    adapter->gate = attention_gate_create(&gate_config);
    if (!adapter->gate) {
        nimcp_free(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_adapter_default_config: adapter->gate is NULL");
        return NULL;
    }

    return adapter;
}

/**
 * @brief Destroy attention adapter
 */
void attention_adapter_destroy(attention_adapter_t* adapter) {
    if (!adapter) return;

    if (adapter->gate) {
        attention_gate_destroy(adapter->gate);
    }

    nimcp_free(adapter);
}

/**
 * @brief Set attention weight
 */
bool attention_adapter_set_weight(attention_adapter_t* adapter,
                                   uint32_t source_id,
                                   uint32_t target_id,
                                   float weight) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_adapter_destroy: adapter is NULL");
        return false;
    }

    return attention_gate_set_weight(adapter->gate, source_id, target_id, weight);
}

/**
 * @brief Update bottom-up salience
 */
bool attention_adapter_update_salience(attention_adapter_t* adapter,
                                        uint32_t target_id,
                                        float salience) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_adapter_destroy: adapter is NULL");
        return false;
    }

    return attention_gate_update_salience(adapter->gate, target_id, salience);
}

/**
 * @brief Apply winner-take-all
 */
bool attention_adapter_apply_wta(attention_adapter_t* adapter,
                                  uint32_t* winner_id) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_adapter_destroy: adapter is NULL");
        return false;
    }

    return attention_gate_apply_wta(adapter->gate, winner_id);
}

/**
 * @brief Update attention spotlight
 */
bool attention_adapter_update_spotlight(attention_adapter_t* adapter,
                                         uint32_t* spotlight_ids,
                                         uint32_t* num_in_spotlight) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_adapter_destroy: adapter is NULL");
        return false;
    }

    return attention_gate_update_spotlight(adapter->gate,
                                            spotlight_ids,
                                            num_in_spotlight);
}

/**
 * @brief Route signal with attention
 */
bool attention_adapter_route_signal(attention_adapter_t* adapter,
                                     uint32_t target_id,
                                     const float* signal_in,
                                     float* signal_out,
                                     size_t signal_size) {
    if (!adapter || !signal_in || !signal_out || signal_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_adapter_destroy: required parameter is NULL (adapter, signal_in, signal_out)");
        return false;
    }

    // Get attention weight
    float weight = 0.0F;
    if (!attention_gate_get_weight(adapter->gate, 0, target_id, &weight)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_adapter_destroy: attention_gate_get_weight is NULL");
        return false;
    }

    // Modulate signal
    for (size_t i = 0; i < signal_size; i++) {
        signal_out[i] = signal_in[i] * weight;
    }

    return true;
}

/**
 * @brief Detect attention pattern
 */
bool attention_adapter_detect_pattern(attention_adapter_t* adapter,
                                       attention_pattern_t* pattern) {
    if (!adapter || !adapter->config.enable_pattern_detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_adapter_destroy: required parameter is NULL (adapter, adapter->config)");
        return false;
    }

    // Get current spotlight
    uint32_t spotlight_ids[COGNITIVE_ATTENTION_SPOTLIGHT_SIZE];
    uint32_t num_in_spotlight = 0;

    if (!attention_gate_update_spotlight(adapter->gate,
                                          spotlight_ids,
                                          &num_in_spotlight)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_adapter_destroy: operation failed");
        return false;
    }

    if (num_in_spotlight < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_adapter_destroy: validation failed");
        return false;
    }

    // Simple pattern: if spotlight stable, it's a pattern
    if (pattern) {
        pattern->pattern_id = adapter->pattern_count++;
        pattern->target_ids = nimcp_malloc(num_in_spotlight * sizeof(uint32_t));
        if (pattern->target_ids) {
            memcpy(pattern->target_ids, spotlight_ids,
                   num_in_spotlight * sizeof(uint32_t));
            pattern->num_targets = num_in_spotlight;
            pattern->coherence = adapter->config.pattern_threshold;
            pattern->timestamp_us = cognitive_adapter_get_timestamp_us();
        }
    }

    adapter->stats.patterns_detected++;

    return true;
}

/**
 * @brief Get recent attention shifts
 */
bool attention_adapter_get_shifts(const attention_adapter_t* adapter,
                                   attention_shift_t* shifts,
                                   uint32_t max_shifts,
                                   uint32_t* num_shifts) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_adapter_destroy: adapter is NULL");
        return false;
    }

    return attention_gate_get_shifts(adapter->gate, shifts,
                                      max_shifts, num_shifts);
}

/**
 * @brief Reset attention state
 */
void attention_adapter_reset(attention_adapter_t* adapter) {
    if (!adapter) return;

    if (adapter->gate) {
        attention_gate_reset(adapter->gate);
    }
}

/**
 * @brief Get adapter statistics
 */
bool attention_adapter_get_stats(const attention_adapter_t* adapter,
                                  attention_adapter_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_adapter_reset: required parameter is NULL (adapter, stats)");
        return false;
    }

    uint32_t num_targets = 0;
    uint32_t num_in_spotlight = 0;
    uint64_t total_shifts = 0;

    attention_gate_get_stats(adapter->gate, &num_targets,
                              &num_in_spotlight, &total_shifts);

    stats->active_targets = num_targets;
    stats->total_shifts = (uint32_t)total_shifts;
    stats->patterns_detected = adapter->stats.patterns_detected;
    stats->avg_spotlight_size = (float)num_in_spotlight;

    // Stability: fewer shifts = more stable
    stats->attention_stability = (total_shifts > 0) ?
                                 1.0F / (1.0F + logf((float)total_shifts)) :
                                 1.0F;

    return true;
}

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Get current timestamp in microseconds
 */
uint64_t cognitive_adapter_get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Calculate salience from signal properties
 */
float cognitive_adapter_calculate_salience(const float* signal,
                                             size_t signal_size,
                                             float baseline) {
    if (!signal || signal_size == 0) return 0.0F;

    // Calculate mean
    float mean = 0.0F;
    for (size_t i = 0; i < signal_size; i++) {
        mean += signal[i];
    }
    mean /= signal_size;

    // Calculate variance
    float variance = 0.0F;
    for (size_t i = 0; i < signal_size; i++) {
        float diff = signal[i] - mean;
        variance += diff * diff;
    }
    variance /= signal_size;

    // Novelty: distance from baseline
    float novelty = fabsf(mean - baseline);

    // Intensity: absolute magnitude
    float intensity = fabsf(mean);

    // Variability: standard deviation
    float variability = sqrtf(variance);

    // Combine factors
    float salience = 0.4F * novelty + 0.3F * intensity + 0.3F * variability;

    // Clamp to [0, 1]
    return fmaxf(0.0F, fminf(1.0F, salience));
}
