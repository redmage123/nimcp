/**
 * @file nimcp_snn_memory_bridge.c
 * @brief SNN-Working Memory integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_memory_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_memory_bridge)

//=============================================================================
// Bio-Async Module ID
//=============================================================================

#define BIO_MODULE_SNN_MEMORY_BRIDGE 0x0611

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * WHAT: Initialize config with biologically-plausible defaults
 * WHY:  Convenient starting point
 * HOW:  Literature-based parameter values
 */
void snn_memory_config_default(snn_memory_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_config_default: null config pointer");
        return;
    }

    /* Spike-to-memory encoding */
    config->encoding_threshold_rate = 20.0f;     /* 20 Hz min for encoding */
    config->encoding_window_ms = 100.0f;         /* 100ms integration */
    config->population_code_size = 100;          /* 100 neurons per item */
    config->use_population_code = true;

    /* Persistent activity */
    config->persistence_rate_min = 15.0f;        /* 15 Hz sustained */
    config->enable_recurrent_boost = true;
    config->recurrent_weight_scale = 1.3f;       /* 30% boost */

    /* Memory-to-spike retrieval */
    config->retrieval_boost_factor = 2.0f;       /* 2x boost on retrieval */
    config->salience_scaling = 1.5f;             /* Scale salience */
    config->retrieval_duration_ms = 200.0f;      /* 200ms retrieval */

    /* Capacity management */
    config->max_memory_items = 7;                /* Miller's number */
    config->enforce_capacity_limit = true;
    config->lateral_inhibition = 0.2f;           /* 20% inhibition */

    /* Population mapping */
    config->memory_population_ids = NULL;        /* Set by user */
    config->num_memory_populations = 0;

    /* Update timing */
    config->update_interval_ms = 50.0f;          /* 20 Hz update */

    /* Bio-async */
    config->enable_bio_async = false;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * WHAT: Create SNN-memory bridge
 * WHY:  Initialize bidirectional integration
 * HOW:  Allocate, validate, connect components
 */
snn_memory_bridge_t* snn_memory_bridge_create(
    const snn_memory_config_t* config,
    snn_network_t* snn,
    working_memory_t* working_memory
) {
    /* Guard: Validate inputs */
    if (!config || !snn || !working_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null parameters to snn_memory_bridge_create: config=%p, snn=%p, wm=%p",
                             (void*)config, (void*)snn, (void*)working_memory);
        return NULL;
    }

    /* Allocate bridge */
    snn_memory_bridge_t* bridge = nimcp_malloc(sizeof(snn_memory_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_memory_bridge_t),
                          "Failed to allocate SNN-memory bridge");
        return NULL;
    }

    /* Initialize structure */
    memset(bridge, 0, sizeof(snn_memory_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_memory") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->working_memory = working_memory;
    bridge->config = *config;

    /* Allocate populations array */
    if (config->num_memory_populations > 0) {
        bridge->memory_pops = nimcp_malloc(
            sizeof(snn_population_t*) * config->num_memory_populations
        );
        if (!bridge->memory_pops) {
            NIMCP_LOGGING_ERROR("Failed to allocate memory populations array");
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_memory_bridge_create: bridge->memory_pops is NULL");
            return NULL;
        }

        /* Get population references */
        for (uint32_t i = 0; i < config->num_memory_populations; i++) {
            bridge->memory_pops[i] = snn_network_get_population(
                snn, config->memory_population_ids[i]
            );
            if (!bridge->memory_pops[i]) {
                NIMCP_LOGGING_WARN("Memory population ID %u not found",
                    config->memory_population_ids[i]);
            }
        }
    }

    /* Allocate patterns array */
    bridge->patterns = nimcp_malloc(
        sizeof(snn_memory_pattern_t) * config->max_memory_items
    );
    if (!bridge->patterns) {
        NIMCP_LOGGING_ERROR("Failed to allocate patterns array");
        nimcp_free(bridge->memory_pops);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_memory_bridge_create: bridge->patterns is NULL");
        return NULL;
    }
    memset(bridge->patterns, 0, sizeof(snn_memory_pattern_t) * config->max_memory_items);

    /* Allocate state arrays */
    bridge->state.item_persistence = nimcp_malloc(sizeof(float) * config->max_memory_items);
    bridge->state.item_timestamps = nimcp_malloc(sizeof(uint64_t) * config->max_memory_items);
    bridge->state.population_rates = nimcp_malloc(sizeof(float) * config->num_memory_populations);
    bridge->state.is_persistent_active = nimcp_malloc(sizeof(bool) * config->num_memory_populations);

    if (!bridge->state.item_persistence || !bridge->state.item_timestamps ||
        !bridge->state.population_rates || !bridge->state.is_persistent_active) {
        NIMCP_LOGGING_ERROR("Failed to allocate state arrays");
        /* Cleanup */
        nimcp_free(bridge->state.item_persistence);
        nimcp_free(bridge->state.item_timestamps);
        nimcp_free(bridge->state.population_rates);
        nimcp_free(bridge->state.is_persistent_active);
        nimcp_free(bridge->patterns);
        nimcp_free(bridge->memory_pops);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_bridge_create: operation failed");
        return NULL;
    }

    /* Initialize state */
    memset(bridge->state.item_persistence, 0, sizeof(float) * config->max_memory_items);
    memset(bridge->state.item_timestamps, 0, sizeof(uint64_t) * config->max_memory_items);
    memset(bridge->state.population_rates, 0, sizeof(float) * config->num_memory_populations);
    memset(bridge->state.is_persistent_active, 0, sizeof(bool) * config->num_memory_populations);

    NIMCP_LOGGING_INFO("Created SNN-memory bridge");
    return bridge;
}

/**
 * WHAT: Destroy bridge and free resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect, free all allocations
 */
void snn_memory_bridge_destroy(snn_memory_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_bridge_destroy: null bridge pointer");
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        snn_memory_bridge_disconnect_bio_async(bridge);
    }

    /* Free patterns and their data */
    if (bridge->patterns) {
        for (uint32_t i = 0; i < bridge->config.max_memory_items; i++) {
            if (bridge->patterns[i].spike_rates) {
                nimcp_free(bridge->patterns[i].spike_rates);
            }
        }
        nimcp_free(bridge->patterns);
    }

    /* Free state arrays */
    nimcp_free(bridge->state.item_persistence);
    nimcp_free(bridge->state.item_timestamps);
    nimcp_free(bridge->state.population_rates);
    nimcp_free(bridge->state.is_persistent_active);

    /* Free populations array */
    nimcp_free(bridge->memory_pops);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-memory bridge");
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Connect to bio-async messaging
 * WHY:  Enable distributed coordination
 * HOW:  Register with router
 */
int snn_memory_bridge_connect_bio_async(snn_memory_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_MEMORY_BRIDGE,
        .module_name = "snn_memory_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_memory_bridge_connect_bio_async: validation failed");
    return -1;
}

/**
 * WHAT: Disconnect from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister from router
 */
int snn_memory_bridge_disconnect_bio_async(snn_memory_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

/**
 * WHAT: Check bio-async connection status
 * WHY:  Query before sending messages
 * HOW:  Return flag
 */
bool snn_memory_bridge_is_bio_async_connected(const snn_memory_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * WHAT: Process spike patterns for memory encoding
 * WHY:  Main processing pipeline
 * HOW:  Detect patterns, encode, maintain
 */
int snn_memory_bridge_process(
    snn_memory_bridge_t* bridge,
    const float* input,
    float* output
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_bridge_process: null bridge pointer");
        return -1;
    }

    /* Update bridge state */
    int ret = snn_memory_bridge_update(bridge, bridge->config.update_interval_ms);
    if (ret != 0) {
        return ret;
    }

    /* If output buffer provided, fill with memory state */
    if (output) {
        output[0] = (float)bridge->state.num_encoded_items;
        output[1] = bridge->state.avg_persistence_rate;
    }

    return 0;
}

/**
 * WHAT: Update bridge state
 * WHY:  Synchronize SNN and memory
 * HOW:  Update persistence, check for encoding
 */
int snn_memory_bridge_update(snn_memory_bridge_t* bridge, float dt) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_bridge_update: null bridge pointer");
        return -1;
    }

    /* Check if update needed based on interval */
    if (bridge->last_update_time > 0 &&
        (dt < bridge->config.update_interval_ms)) {
        return 0;  /* Skip update, too soon */
    }

    /* Update population rates and check for persistence */
    float total_persistence = 0.0f;
    for (uint32_t i = 0; i < bridge->config.num_memory_populations; i++) {
        if (!bridge->memory_pops[i]) continue;

        /* Get population rate */
        bridge->state.population_rates[i] = snn_network_get_population_rate(
            bridge->snn,
            bridge->config.memory_population_ids[i],
            bridge->config.encoding_window_ms
        );

        /* Check persistence */
        bridge->state.is_persistent_active[i] = snn_memory_is_persistent(
            bridge, bridge->config.memory_population_ids[i]
        );

        if (bridge->state.is_persistent_active[i]) {
            total_persistence += bridge->state.population_rates[i];
        }
    }

    /* Update average persistence rate */
    if (bridge->config.num_memory_populations > 0) {
        bridge->state.avg_persistence_rate = total_persistence / bridge->config.num_memory_populations;
    }

    /* Check for new encodings (populations with high activity) */
    for (uint32_t i = 0; i < bridge->config.num_memory_populations; i++) {
        if (bridge->state.population_rates[i] >= bridge->config.encoding_threshold_rate) {
            /* Encode this population */
            /* Normalize salience using threshold rate (max expected is ~2x threshold) */
            float max_rate = bridge->config.encoding_threshold_rate * 2.0f;
            float salience = bridge->state.population_rates[i] / max_rate;
            if (salience > 1.0f) salience = 1.0f;
            snn_memory_encode_item(bridge, bridge->config.memory_population_ids[i], salience);
        }
    }

    bridge->last_update_time += dt;
    return 0;
}

//=============================================================================
// Spike-to-Memory Encoding
//=============================================================================

/**
 * WHAT: Encode spike pattern to memory item
 * WHY:  Store active representations
 * HOW:  Extract pattern, add to working memory
 */
int snn_memory_encode_item(
    snn_memory_bridge_t* bridge,
    uint32_t population_id,
    float salience
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_encode_item: null bridge pointer");
        return -1;
    }

    /* Get population */
    snn_population_t* pop = snn_network_get_population(bridge->snn, population_id);
    if (!pop) {
        NIMCP_LOGGING_ERROR("Population %u not found", population_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_encode_item: pop is NULL");
        return -1;
    }

    /* Check capacity */
    if (bridge->state.num_encoded_items >= bridge->config.max_memory_items &&
        bridge->config.enforce_capacity_limit) {
        /* Would need to evict - for now just log */
        NIMCP_LOGGING_WARN("Memory capacity reached, skipping encoding");
        bridge->state.capacity_evictions++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_memory_encode_item: operation failed");
        return -1;
    }

    /* Extract spike pattern */
    snn_memory_pattern_t pattern;
    int ret = snn_memory_extract_pattern(bridge, pop, &pattern);
    if (ret != 0) {
        return ret;
    }

    /* Add to working memory */
    bool success = working_memory_add(
        bridge->working_memory,
        pattern.spike_rates,
        pattern.num_neurons,
        salience
    );

    if (success) {
        /* Store pattern */
        uint32_t idx = bridge->state.num_encoded_items;
        if (idx < bridge->config.max_memory_items) {
            bridge->patterns[idx] = pattern;
            bridge->state.item_persistence[idx] = 1.0f;
            bridge->state.item_timestamps[idx] = pattern.timestamp_us;
            bridge->state.num_encoded_items++;
            bridge->state.total_encodings++;
        }
    } else {
        /* Free pattern if not stored */
        nimcp_free(pattern.spike_rates);
    }

    return success ? 0 : -1;
}

/**
 * WHAT: Extract spike pattern from population
 * WHY:  Capture population code
 * HOW:  Sample spike rates
 */
int snn_memory_extract_pattern(
    snn_memory_bridge_t* bridge,
    snn_population_t* population,
    snn_memory_pattern_t* pattern
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_extract_pattern: null bridge pointer");
        return -1;
    }
    if (!population) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_extract_pattern: null population pointer");
        return -1;
    }
    if (!pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_extract_pattern: null pattern pointer");
        return -1;
    }

    /* Allocate spike rates array */
    pattern->num_neurons = population->n_neurons;
    pattern->spike_rates = nimcp_malloc(sizeof(float) * pattern->num_neurons);
    if (!pattern->spike_rates) {
        NIMCP_LOGGING_ERROR("Failed to allocate spike rates");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_memory_extract_pattern: pattern->spike_rates is NULL");
        return -1;
    }

    /* Extract rates per neuron */
    float total_rate = 0.0f;
    for (uint32_t i = 0; i < pattern->num_neurons; i++) {
        /* Get firing rate for this neuron */
        pattern->spike_rates[i] = snn_network_get_firing_rate(
            bridge->snn,
            population->id,
            i,
            bridge->config.encoding_window_ms
        );
        total_rate += pattern->spike_rates[i];
    }

    /* Compute pattern strength */
    pattern->pattern_strength = total_rate / pattern->num_neurons;
    pattern->is_persistent = (pattern->pattern_strength >= bridge->config.persistence_rate_min);

    /* Timestamp */
    pattern->timestamp_us = 0;  /* Would need simulation time */

    return 0;
}

/**
 * WHAT: Check for persistent activity
 * WHY:  Detect active maintenance
 * HOW:  Check sustained rate
 */
bool snn_memory_is_persistent(
    snn_memory_bridge_t* bridge,
    uint32_t population_id
) {
    if (!bridge) {
        return false;
    }

    /* Find population index */
    for (uint32_t i = 0; i < bridge->config.num_memory_populations; i++) {
        if (bridge->config.memory_population_ids[i] == population_id) {
            return (bridge->state.population_rates[i] >= bridge->config.persistence_rate_min);
        }
    }

    return false;
}

/**
 * WHAT: Boost recurrent connections
 * WHY:  Sustain persistent activity
 * HOW:  Scale recurrent weights
 */
int snn_memory_boost_recurrence(
    snn_memory_bridge_t* bridge,
    uint32_t population_id
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_boost_recurrence: null bridge pointer");
        return -1;
    }

    if (!bridge->config.enable_recurrent_boost) {
        return 0;  /* Disabled */
    }

    /* Note: Actual implementation would modify recurrent synapses */
    /* This requires access to synapse structures */
    /* Placeholder for demonstration */

    return 0;
}

//=============================================================================
// Memory-to-Spike Retrieval
//=============================================================================

/**
 * WHAT: Retrieve memory item and activate population
 * WHY:  Memory recall drives activity
 * HOW:  Reactivate stored pattern
 */
int snn_memory_retrieve_item(
    snn_memory_bridge_t* bridge,
    uint32_t item_index
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_retrieve_item: null bridge pointer");
        return -1;
    }

    if (item_index >= bridge->state.num_encoded_items) {
        NIMCP_LOGGING_ERROR("Item index %u out of range", item_index);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "snn_memory_retrieve_item: capacity exceeded");
        return -1;
    }

    /* Get pattern */
    snn_memory_pattern_t* pattern = &bridge->patterns[item_index];

    /* Get corresponding population */
    if (item_index >= bridge->config.num_memory_populations) {
        NIMCP_LOGGING_ERROR("No population for item %u", item_index);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_memory_retrieve_item: capacity exceeded");
        return -1;
    }

    snn_population_t* pop = bridge->memory_pops[item_index];
    if (!pop) {
        NIMCP_LOGGING_ERROR("Population for item %u not found", item_index);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_retrieve_item: pop is NULL");
        return -1;
    }

    /* Activate pattern */
    int ret = snn_memory_activate_pattern(bridge, pop, pattern);
    if (ret == 0) {
        bridge->state.total_retrievals++;
        bridge->state.num_active_retrievals++;
    }

    return ret;
}

/**
 * WHAT: Activate population from pattern
 * WHY:  Reactivation is recall
 * HOW:  Inject currents
 */
int snn_memory_activate_pattern(
    snn_memory_bridge_t* bridge,
    snn_population_t* population,
    const snn_memory_pattern_t* pattern
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_activate_pattern: null bridge pointer");
        return -1;
    }
    if (!population) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_activate_pattern: null population pointer");
        return -1;
    }
    if (!pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_activate_pattern: null pattern pointer");
        return -1;
    }

    /* Note: Actual implementation would inject currents to neurons */
    /* This requires access to neuron_t structures and input current API */
    /* Placeholder for demonstration */

    return 0;
}

/**
 * WHAT: Modulate population by salience
 * WHY:  Important items have stronger recall
 * HOW:  Scale excitability
 */
int snn_memory_modulate_by_salience(
    snn_memory_bridge_t* bridge,
    uint32_t population_id,
    float salience
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_modulate_by_salience: null bridge pointer");
        return -1;
    }

    float boost = bridge->config.salience_scaling * salience;

    /* Note: Actual implementation would modulate population excitability */
    /* Placeholder for demonstration */

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

/**
 * WHAT: Get current bridge state
 * WHY:  External monitoring
 * HOW:  Copy state structure
 */
int snn_memory_bridge_get_state(
    const snn_memory_bridge_t* bridge,
    snn_memory_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_bridge_get_state: null bridge pointer");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_bridge_get_state: null state pointer");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

/**
 * WHAT: Get number of encoded items
 * WHY:  Query capacity usage
 * HOW:  Return counter
 */
uint32_t snn_memory_get_num_items(const snn_memory_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_get_num_items: null bridge pointer");
        return 0;
    }
    return bridge->state.num_encoded_items;
}

/**
 * WHAT: Check persistent activity
 * WHY:  Query maintenance state
 * HOW:  Return flag
 */
bool snn_memory_has_persistent_activity(
    const snn_memory_bridge_t* bridge,
    uint32_t population_id
) {
    if (!bridge) {
        return false;
    }

    for (uint32_t i = 0; i < bridge->config.num_memory_populations; i++) {
        if (bridge->config.memory_population_ids[i] == population_id) {
            return bridge->state.is_persistent_active[i];
        }
    }

    return false;
}

/**
 * WHAT: Get persistence strength
 * WHY:  Query maintenance quality
 * HOW:  Return strength value
 */
float snn_memory_get_persistence_strength(
    const snn_memory_bridge_t* bridge,
    uint32_t item_index
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_get_persistence_strength: null bridge pointer");
        return 0.0f;
    }
    if (item_index >= bridge->state.num_encoded_items) {
        return 0.0f;
    }

    return bridge->state.item_persistence[item_index];
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * WHAT: Get bridge statistics
 * WHY:  Monitor performance
 * HOW:  Return computed metrics
 */
int snn_memory_get_stats(
    const snn_memory_bridge_t* bridge,
    uint32_t* encodings,
    uint32_t* retrievals,
    uint32_t* evictions
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_get_stats: null bridge pointer");
        return -1;
    }

    if (encodings) *encodings = bridge->state.total_encodings;
    if (retrievals) *retrievals = bridge->state.total_retrievals;
    if (evictions) *evictions = bridge->state.capacity_evictions;

    return 0;
}

/**
 * WHAT: Reset statistics
 * WHY:  Start fresh measurement
 * HOW:  Zero counters
 */
void snn_memory_reset_stats(snn_memory_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_memory_reset_stats: null bridge pointer");
        return;
    }

    bridge->state.total_encodings = 0;
    bridge->state.total_retrievals = 0;
    bridge->state.capacity_evictions = 0;
    bridge->state.avg_persistence_rate = 0.0f;
}
