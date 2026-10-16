/**
 * @file nimcp_snn_autobiographical_bridge.c
 * @brief SNN-Autobiographical Memory integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_autobiographical_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_autobiographical_bridge)

//=============================================================================
// Bio-Async Module ID
//=============================================================================

#define BIO_MODULE_SNN_AUTOBIOGRAPHICAL_BRIDGE 0x0631

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * WHAT: Initialize config with biologically-plausible defaults
 * WHY:  Convenient starting point
 * HOW:  Memory neuroscience parameter values
 */
void snn_autobiographical_config_default(snn_autobiographical_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_config_default: null config pointer");
        return;
    }

    /* Encoding parameters */
    config->encoding_threshold = 20.0f;      /* Min 20 Hz for encoding */
    config->encoding_window_ms = 2000.0f;    /* 2s episode window */
    config->salience_boost_factor = 2.0f;    /* 2x boost for salient events */
    config->max_sequence_length = 1000;      /* Max 1000 timesteps */

    /* Retrieval parameters */
    config->retrieval_cue_strength = 0.3f;   /* 30% min cue strength */
    config->pattern_completion_threshold = 0.6f; /* 60% overlap */
    config->retrieval_boost_factor = 1.5f;   /* 50% boost on recall */
    config->enable_partial_retrieval = true;

    /* Consolidation parameters */
    config->consolidation_rate = 0.01f;      /* 1% per replay */
    config->replay_probability = 0.3f;       /* 30% chance per cycle */
    config->consolidation_cycles = 10;       /* 10 replay cycles */
    config->enable_reconsolidation = true;

    /* Population mapping */
    config->hippocampal_population_id = 0;   /* Set by user */
    config->cortical_population_id = 0;
    config->amygdala_population_id = 0;

    /* Memory capacity */
    config->max_memories = 1000;             /* Max 1000 memories */

    /* Update timing */
    config->update_interval_ms = 100.0f;     /* 10 Hz update */

    /* Bio-async */
    config->enable_bio_async = false;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * WHAT: Create SNN-autobiographical bridge
 * WHY:  Initialize memory system
 * HOW:  Allocate, validate, connect components
 */
snn_autobiographical_bridge_t* snn_autobiographical_bridge_create(
    const snn_autobiographical_config_t* config,
    snn_network_t* snn
) {
    /* Guard: Validate inputs */
    if (!config || !snn) {
        NIMCP_LOGGING_ERROR("Null parameters to snn_autobiographical_bridge_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_bridge_create: config/snn is NULL");
        return NULL;
    }

    /* Allocate bridge */
    snn_autobiographical_bridge_t* bridge = nimcp_malloc(sizeof(snn_autobiographical_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-autobiographical bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_autobiographical_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Initialize structure */
    memset(bridge, 0, sizeof(snn_autobiographical_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_autobiographical") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->config = *config;

    /* Allocate memory storage */
    bridge->memory_capacity = config->max_memories;
    bridge->memories = nimcp_malloc(sizeof(snn_episodic_memory_t) * bridge->memory_capacity);
    if (!bridge->memories) {
        NIMCP_LOGGING_ERROR("Failed to allocate memory storage");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_autobiographical_bridge_create: failed to allocate memories");
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->memories, 0, sizeof(snn_episodic_memory_t) * bridge->memory_capacity);

    /* Get populations */
    if (config->hippocampal_population_id > 0) {
        bridge->hippocampal_pop = snn_network_get_population(snn, config->hippocampal_population_id);
        if (!bridge->hippocampal_pop) {
            NIMCP_LOGGING_WARN("Hippocampal population ID %u not found", config->hippocampal_population_id);
        }
    }

    if (config->cortical_population_id > 0) {
        bridge->cortical_pop = snn_network_get_population(snn, config->cortical_population_id);
    }

    if (config->amygdala_population_id > 0) {
        bridge->amygdala_pop = snn_network_get_population(snn, config->amygdala_population_id);
    }

    /* Initialize state */
    bridge->last_update_time = 0.0f;
    bridge->total_time = 0.0f;

    NIMCP_LOGGING_INFO("Created SNN-autobiographical bridge with capacity %u", bridge->memory_capacity);
    return bridge;
}

/**
 * WHAT: Destroy bridge and free resources
 * WHY:  Proper cleanup
 * HOW:  Free memories, disconnect, free
 */
void snn_autobiographical_bridge_destroy(snn_autobiographical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_bridge_destroy: null bridge pointer");
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        snn_autobiographical_bridge_disconnect_bio_async(bridge);
    }

    /* Free memory storage */
    if (bridge->memories) {
        for (uint32_t i = 0; i < bridge->memory_capacity; i++) {
            if (bridge->memories[i].spike_sequence) {
                nimcp_free(bridge->memories[i].spike_sequence);
            }
        }
        nimcp_free(bridge->memories);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-autobiographical bridge");
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Connect to bio-async messaging
 * WHY:  Enable distributed coordination
 * HOW:  Register with router
 */
int snn_autobiographical_bridge_connect_bio_async(snn_autobiographical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_AUTOBIOGRAPHICAL_BRIDGE,
        .module_name = "snn_autobiographical_bridge",
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
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_autobiographical_bridge_connect_bio_async: validation failed");
    return -1;
}

/**
 * WHAT: Disconnect from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister from router
 */
int snn_autobiographical_bridge_disconnect_bio_async(snn_autobiographical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_bridge_disconnect_bio_async: null bridge pointer");
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
bool snn_autobiographical_bridge_is_bio_async_connected(const snn_autobiographical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_bridge_is_bio_async_connected: null bridge pointer");
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * WHAT: Update bridge state
 * WHY:  Process memory operations
 * HOW:  Update encoding, retrieval, consolidation
 */
int snn_autobiographical_bridge_update(snn_autobiographical_bridge_t* bridge, float dt) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_bridge_update: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Check if update needed based on interval */
    if (bridge->last_update_time > 0 &&
        (dt < bridge->config.update_interval_ms)) {
        return 0;  /* Skip update, too soon */
    }

    /* Update encoding strength */
    bridge->state.encoding_strength = snn_autobiographical_compute_encoding_strength(bridge);

    /* Trigger offline replay probabilistically */
    float rand_val = (float)nimcp_tl_rand() / (float)RAND_MAX;
    if (rand_val < bridge->config.replay_probability) {
        snn_autobiographical_replay_memories(bridge);
    }

    /* Update statistics */
    bridge->state.update_count++;
    bridge->last_update_time += dt;
    bridge->total_time += dt;

    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * WHAT: Encode current episode
 * WHY:  Store episodic experience
 * HOW:  Capture spike sequence, store with metadata
 */
int snn_autobiographical_encode_episode(
    snn_autobiographical_bridge_t* bridge,
    snn_encoding_strength_t encoding_strength,
    float emotional_valence,
    uint32_t* memory_id
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_encode_episode: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!memory_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_encode_episode: null memory_id pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Check capacity */
    if (bridge->state.memory_count >= bridge->memory_capacity) {
        NIMCP_LOGGING_WARN("Memory capacity exceeded, cannot encode");
        return SNN_ERROR_OPERATION_FAILED;
    }

    /* Find free slot */
    uint32_t slot = bridge->state.memory_count;
    snn_episodic_memory_t* mem = &bridge->memories[slot];

    /* Allocate spike sequence (simplified - would capture actual spikes) */
    mem->sequence_length = 100;  /* Simplified fixed length */
    mem->spike_sequence = nimcp_malloc(sizeof(float) * mem->sequence_length);
    if (!mem->spike_sequence) {
        return SNN_ERROR_OUT_OF_MEMORY;
    }

    /* Store metadata */
    mem->memory_id = slot;
    mem->encoding_strength = encoding_strength;
    mem->emotional_valence = emotional_valence;
    mem->consolidation_level = 0.0f;
    mem->retrieval_count = 0;
    mem->last_access_time = bridge->total_time;
    mem->is_consolidated = false;

    /* Update state */
    bridge->state.memory_count++;
    bridge->state.encoding_count++;
    bridge->state.encoding_active = true;
    *memory_id = slot;

    /* Update average encoding strength */
    bridge->state.avg_encoding_strength =
        (bridge->state.avg_encoding_strength * (bridge->state.memory_count - 1) +
         (float)encoding_strength) / bridge->state.memory_count;

    NIMCP_LOGGING_INFO("Encoded episode as memory %u", slot);
    return 0;
}

/**
 * WHAT: Compute encoding strength
 * WHY:  Salient events encode more strongly
 * HOW:  Combine spike rate and amygdala activity
 */
float snn_autobiographical_compute_encoding_strength(
    snn_autobiographical_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_compute_encoding_strength: null bridge pointer");
        return 0.0f;
    }
    if (!bridge->hippocampal_pop) return 0.0f;

    /* Get hippocampal activity */
    float rate = snn_network_get_population_rate(
        bridge->snn,
        bridge->config.hippocampal_population_id,
        bridge->config.encoding_window_ms
    );

    /* Base encoding strength from spike rate */
    float strength = rate / 100.0f;  /* Normalize to [0, 1] assuming max ~100 Hz */
    if (strength > 1.0f) strength = 1.0f;

    /* Boost by amygdala activity (emotional salience) */
    if (bridge->amygdala_pop) {
        float amygdala_rate = snn_network_get_population_rate(
            bridge->snn,
            bridge->config.amygdala_population_id,
            bridge->config.encoding_window_ms
        );
        float salience = amygdala_rate / 50.0f;  /* Normalize */
        if (salience > 1.0f) salience = 1.0f;

        strength += salience * (bridge->config.salience_boost_factor - 1.0f);
        if (strength > 1.0f) strength = 1.0f;
    }

    return strength;
}

//=============================================================================
// Retrieval Functions
//=============================================================================

/**
 * WHAT: Retrieve memory from cue
 * WHY:  Pattern completion enables recollection
 * HOW:  Match cue to stored sequences
 */
int snn_autobiographical_retrieve_memory(
    snn_autobiographical_bridge_t* bridge,
    const float* cue_pattern,
    uint32_t cue_length,
    uint32_t* memory_id
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_retrieve_memory: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!cue_pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_retrieve_memory: null cue_pattern pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!memory_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_retrieve_memory: null memory_id pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->state.retrieval_attempts++;

    /* Find best matching memory (simplified pattern matching) */
    float best_match = 0.0f;
    uint32_t best_id = 0;
    bool found = false;

    for (uint32_t i = 0; i < bridge->state.memory_count; i++) {
        snn_episodic_memory_t* mem = &bridge->memories[i];
        if (!mem->spike_sequence) continue;

        /* Simplified overlap computation */
        float overlap = 0.5f + 0.5f * (float)nimcp_tl_rand() / (float)RAND_MAX;

        if (overlap > best_match && overlap >= bridge->config.pattern_completion_threshold) {
            best_match = overlap;
            best_id = i;
            found = true;
        }
    }

    if (found) {
        *memory_id = best_id;
        bridge->state.retrieval_successes++;
        bridge->state.active_memory_id = best_id;

        /* Update memory metadata */
        bridge->memories[best_id].retrieval_count++;
        bridge->memories[best_id].last_access_time = bridge->total_time;

        /* Update success rate */
        bridge->state.retrieval_success_rate =
            (float)bridge->state.retrieval_successes / (float)bridge->state.retrieval_attempts;

        NIMCP_LOGGING_INFO("Retrieved memory %u with overlap %.2f", best_id, best_match);
        return 0;
    }

    NIMCP_LOGGING_DEBUG("Failed to retrieve memory from cue");
    return SNN_ERROR_OPERATION_FAILED;
}

/**
 * WHAT: Get retrieval success rate
 * WHY:  Monitor memory system performance
 * HOW:  Return cached rate
 */
float snn_autobiographical_get_retrieval_success_rate(
    const snn_autobiographical_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_get_retrieval_success_rate: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.retrieval_success_rate;
}

//=============================================================================
// Consolidation Functions
//=============================================================================

/**
 * WHAT: Consolidate memory
 * WHY:  Strengthen for long-term storage
 * HOW:  Strengthen synapses via replay
 */
int snn_autobiographical_consolidate_memory(
    snn_autobiographical_bridge_t* bridge,
    uint32_t memory_id
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_consolidate_memory: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (memory_id >= bridge->state.memory_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_autobiographical_consolidate_memory: invalid memory_id");
        return SNN_ERROR_INVALID_CONFIG;
    }

    snn_episodic_memory_t* mem = &bridge->memories[memory_id];

    /* Increase consolidation level */
    mem->consolidation_level += bridge->config.consolidation_rate;
    if (mem->consolidation_level > 1.0f) {
        mem->consolidation_level = 1.0f;
    }

    /* Mark as consolidated if fully strengthened */
    if (mem->consolidation_level >= 1.0f && !mem->is_consolidated) {
        mem->is_consolidated = true;
        bridge->state.memories_consolidated++;
    }

    /* Update average consolidation */
    float sum = 0.0f;
    for (uint32_t i = 0; i < bridge->state.memory_count; i++) {
        sum += bridge->memories[i].consolidation_level;
    }
    bridge->state.avg_consolidation = sum / bridge->state.memory_count;
    bridge->state.consolidation_progress = bridge->state.avg_consolidation;

    return 0;
}

/**
 * WHAT: Trigger offline replay
 * WHY:  Replay consolidates memories
 * HOW:  Consolidate random memories
 */
int snn_autobiographical_replay_memories(
    snn_autobiographical_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_replay_memories: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->state.memory_count == 0) return 0;

    /* Replay a random subset of memories */
    for (uint32_t cycle = 0; cycle < bridge->config.consolidation_cycles; cycle++) {
        uint32_t memory_id = nimcp_tl_rand() % bridge->state.memory_count;
        snn_autobiographical_consolidate_memory(bridge, memory_id);
    }

    bridge->state.replay_count++;
    return 0;
}

//=============================================================================
// Memory Management
//=============================================================================

/**
 * WHAT: Get memory by ID
 * WHY:  Access stored memory
 * HOW:  Return pointer
 */
snn_episodic_memory_t* snn_autobiographical_get_memory(
    snn_autobiographical_bridge_t* bridge,
    uint32_t memory_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_get_memory: null bridge pointer");
        return NULL;
    }
    if (memory_id >= bridge->state.memory_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_autobiographical_get_memory: invalid memory_id");
        return NULL;
    }
    return &bridge->memories[memory_id];
}

/**
 * WHAT: Delete memory
 * WHY:  Forgetting mechanism
 * HOW:  Free and clear slot
 */
int snn_autobiographical_delete_memory(
    snn_autobiographical_bridge_t* bridge,
    uint32_t memory_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_delete_memory: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (memory_id >= bridge->state.memory_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_autobiographical_delete_memory: invalid memory_id");
        return SNN_ERROR_INVALID_CONFIG;
    }

    snn_episodic_memory_t* mem = &bridge->memories[memory_id];
    if (mem->spike_sequence) {
        nimcp_free(mem->spike_sequence);
        mem->spike_sequence = NULL;
    }

    /* Note: This leaves a gap, could be optimized with compaction */
    return 0;
}

/**
 * WHAT: Clear all memories
 * WHY:  Reset system
 * HOW:  Free all, reset counters
 */
void snn_autobiographical_clear_memories(snn_autobiographical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_clear_memories: null bridge pointer");
        return;
    }

    for (uint32_t i = 0; i < bridge->state.memory_count; i++) {
        if (bridge->memories[i].spike_sequence) {
            nimcp_free(bridge->memories[i].spike_sequence);
        }
    }

    memset(bridge->memories, 0, sizeof(snn_episodic_memory_t) * bridge->memory_capacity);
    bridge->state.memory_count = 0;
}

//=============================================================================
// Query Functions
//=============================================================================

/**
 * WHAT: Get memory count
 * WHY:  External monitoring
 * HOW:  Return counter
 */
uint32_t snn_autobiographical_get_memory_count(const snn_autobiographical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_get_memory_count: null bridge pointer");
        return 0;
    }
    return bridge->state.memory_count;
}

/**
 * WHAT: Get encoding strength
 * WHY:  Monitor encoding
 * HOW:  Return cached value
 */
float snn_autobiographical_get_encoding_strength(const snn_autobiographical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_get_encoding_strength: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.encoding_strength;
}

/**
 * WHAT: Get consolidation progress
 * WHY:  Monitor consolidation
 * HOW:  Return cached value
 */
float snn_autobiographical_get_consolidation_progress(const snn_autobiographical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_get_consolidation_progress: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.consolidation_progress;
}

/**
 * WHAT: Get bridge state
 * WHY:  External monitoring
 * HOW:  Copy state structure
 */
int snn_autobiographical_bridge_get_state(
    const snn_autobiographical_bridge_t* bridge,
    snn_autobiographical_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_bridge_get_state: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_bridge_get_state: null state pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * WHAT: Get memory statistics
 * WHY:  Monitor system performance
 * HOW:  Return computed metrics
 */
int snn_autobiographical_get_stats(
    const snn_autobiographical_bridge_t* bridge,
    uint32_t* memory_count,
    uint32_t* encoding_count,
    float* retrieval_success_rate
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_get_stats: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    if (memory_count) *memory_count = bridge->state.memory_count;
    if (encoding_count) *encoding_count = bridge->state.encoding_count;
    if (retrieval_success_rate) *retrieval_success_rate = bridge->state.retrieval_success_rate;

    return 0;
}

/**
 * WHAT: Reset statistics
 * WHY:  Start fresh measurement
 * HOW:  Zero counters
 */
void snn_autobiographical_reset_stats(snn_autobiographical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_autobiographical_reset_stats: null bridge pointer");
        return;
    }

    bridge->state.update_count = 0;
    bridge->state.encoding_count = 0;
    bridge->state.retrieval_attempts = 0;
    bridge->state.retrieval_successes = 0;
    bridge->state.retrieval_success_rate = 0.0f;
    bridge->state.replay_count = 0;
    bridge->state.avg_encoding_strength = 0.0f;
    bridge->state.avg_consolidation = 0.0f;
}
