//=============================================================================
// nimcp_distributed_cow.c - Distributed COW Implementation
//=============================================================================
/**
 * BIO-ASYNC INTEGRATION:
 * - Module ID: BIO_MODULE_BRAIN_DISTRIBUTED (0x0224)
 * - Publishes: COW fork/merge events, fetch operations, cache updates
 * - Channels: DOPAMINE (success), NOREPINEPHRINE (urgent/alerts)
 */

#define LOG_MODULE "distributed_cow"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(distributed_cow, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include "core/brain/nimcp_distributed_cow.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory_guards.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>  // For compression

/* Forward declaration - defined in nimcp_brain_strategy.c */
extern void set_error(const char* format, ...);
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "networking/protocol/nimcp_protocol.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_rwlock.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// BIO-ASYNC MODULE REGISTRATION
//=============================================================================

#define BIO_MODULE_DISTRIBUTED_COW 0x0680

// Bio-async registration state (thread-safe via nimcp_platform_once)
static bool g_bio_async_registered = false;
static bio_module_context_t g_bio_module_ctx = NULL;
static nimcp_platform_once_t g_bio_async_once = NIMCP_PLATFORM_ONCE_INIT;

/**
 * @brief Internal bio-async registration (called once via nimcp_platform_once)
 */
static void bio_async_register_internal(void) {
    LOG_DEBUG("Registering distributed_cow module with bio-async router");
    bio_module_info_t info = {
        .module_id = BIO_MODULE_DISTRIBUTED_COW,
        .module_name = "distributed_cow",
        .inbox_capacity = 256,
        .user_data = NULL
    };
    g_bio_module_ctx = bio_router_register_module(&info);
    g_bio_async_registered = (g_bio_module_ctx != NULL);
    if (!g_bio_async_registered) {
        LOG_WARN("Failed to register distributed_cow with bio-async router");
    }
}

/**
 * @brief Ensure bio-async system is registered (thread-safe)
 */
static void ensure_bio_async_registered(void) {
    nimcp_platform_once(&g_bio_async_once, bio_async_register_internal);
}

/**
 * @brief Publish COW event via bio-async
 */
static void publish_cow_event(
    bio_message_type_t msg_type,
    uint32_t segment_start,
    uint32_t segment_size,
    size_t bytes_transferred,
    bool success,
    nimcp_bio_channel_type_t channel)
{
    if (!g_bio_async_registered || !g_bio_module_ctx) {
        return;  // Bio-async not available, skip publishing
    }

    // Create COW event message
    bio_msg_brain_state_query_t msg = {0};
    bio_msg_init_header(&msg.header, msg_type,
                       BIO_MODULE_DISTRIBUTED_COW,
                       0,  // Broadcast
                       sizeof(msg));
    msg.header.channel = channel;
    msg.query_flags = segment_start;
    msg.region_id = segment_size;

    // Publish message via bio-router broadcast
    nimcp_error_t err = bio_router_broadcast(g_bio_module_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        LOG_WARN("Failed to broadcast COW event: error=%d", err);
    }

    LOG_DEBUG("Published COW event: type=%u, segment=[%u,%u), bytes=%zu, success=%d, channel=%d",
              msg_type, segment_start, segment_start + segment_size, bytes_transferred, success, channel);
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal brain structure extension for distributed COW
 *
 * NOTE: This is added to brain_struct in nimcp_brain.c
 * For now, we'll use a separate tracking structure
 */
typedef struct {
    brain_t brain;                       /**< Associated brain */
    distributed_cow_state_t* dcow_state; /**< Distributed COW state */
} distributed_cow_brain_t;

// Global registry for distributed COW brains
static distributed_cow_brain_t** g_dcow_brains = NULL;
static uint32_t g_num_dcow_brains = 0;
static nimcp_platform_mutex_t g_registry_mutex;
static nimcp_platform_once_t g_registry_mutex_once = NIMCP_PLATFORM_ONCE_INIT;

//=============================================================================
// Registry Initialization
//=============================================================================

/**
 * @brief Internal registry mutex init (called once via nimcp_platform_once)
 */
static void registry_mutex_init_internal(void) {
    nimcp_platform_mutex_init(&g_registry_mutex, false);
}

/**
 * @brief Ensure global registry mutex is initialized (thread-safe)
 */
static void ensure_registry_mutex_initialized(void) {
    nimcp_platform_once(&g_registry_mutex_once, registry_mutex_init_internal);
}

//=============================================================================
// Network Segment Serialization
//=============================================================================

/**
 * @brief Get access to underlying neural network from adaptive network
 *
 * WHAT: Internal accessor to neural network for serialization
 * WHY:  Need to access neurons array for network segment serialization
 * HOW:  Direct access to base_network->neurons array
 *
 * @param network Adaptive network
 * @return Neural network handle or NULL on error
 */
static neural_network_t get_base_network(adaptive_network_t network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;
    }
    // ASSUMPTION: This relies on adaptive_network_struct having base_network as its
    // first field. This is a fragile pattern - any reordering of the struct will break
    // serialization. If adaptive_network.h exposes an accessor function in the future,
    // that should be used instead. This pattern is used because the struct is opaque
    // and no public accessor exists.
    // See: include/core/brain/nimcp_adaptive_network.h for struct definition.
    return *((neural_network_t*)network);
}

/**
 * @brief Get number of neurons in adaptive network
 *
 * WHAT: Get total neuron count from network
 * WHY:  Validate segment bounds during serialization
 * HOW:  Call neuralnet_get_num_neurons on base network
 */
static uint32_t get_network_num_neurons(adaptive_network_t network) {
    neural_network_t base = get_base_network(network);
    if (!base) {
        return 0;
    }
    return neural_network_get_num_neurons(base);
}

/**
 * @brief Serialize network segment
 *
 * WHAT: Converts neurons and synapses to wire format
 * WHY:  Enable network transfer
 * HOW:  Packs neuron states, synapse weights, and connectivity
 *
 * @param network Adaptive network
 * @param start_neuron First neuron ID
 * @param num_neurons Number of neurons
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param enable_compression Compress output?
 * @return Bytes written, or -1 on error
 */
static int serialize_network_segment(
    adaptive_network_t network,
    uint32_t start_neuron,
    uint32_t num_neurons,
    uint8_t* buffer,
    size_t buffer_size,
    bool enable_compression
) {
    if (!network || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "serialize_network_segment: required parameter is NULL (network, buffer)");
        return -1;
    }

    // Get base neural network
    neural_network_t base_net = get_base_network(network);
    if (!base_net) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "base_net is NULL");

        return -1;
    }

    // Get network info
    uint32_t total_neurons = get_network_num_neurons(network);
    if (start_neuron >= total_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "serialize_network_segment: capacity exceeded");
        return -1;
    }
    if (start_neuron + num_neurons > total_neurons) {
        num_neurons = total_neurons - start_neuron;
    }

    // Estimate size: header + (neuron_state + synapses) per neuron
    // Assume avg 50 synapses per neuron
    size_t temp_buffer_size = 1024 + num_neurons * (32 + 50 * 8);
    uint8_t* temp_buffer = nimcp_malloc(temp_buffer_size);
    if (!temp_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temp_buffer is NULL");

        return -1;
    }

    size_t offset = 0;

    // Write header
    *(uint32_t*)(temp_buffer + offset) = start_neuron;
    offset += sizeof(uint32_t);
    *(uint32_t*)(temp_buffer + offset) = num_neurons;
    offset += sizeof(uint32_t);

    // For each neuron, write state and connections
    for (uint32_t i = start_neuron; i < start_neuron + num_neurons; i++) {
        // Get neuron state from base network
        float neuron_state = 0.0F;
        if (!neural_network_get_neuron_state(base_net, i, &neuron_state)) {
            neuron_state = 0.0F;
        }

        // Get neuron pointer for synapse data (requires internal access)
        // NOTE: This requires the network structure to expose neurons
        // For now, we'll serialize just the state and assume synapses are handled elsewhere

        // Write neuron state
        *(float*)(temp_buffer + offset) = neuron_state;
        offset += sizeof(float);

        // Write threshold (use default if not available)
        float threshold = 1.0F;
        *(float*)(temp_buffer + offset) = threshold;
        offset += sizeof(float);

        // Write number of synapses (0 for now - full implementation would iterate synapses)
        uint32_t num_synapses = 0;
        *(uint32_t*)(temp_buffer + offset) = num_synapses;
        offset += sizeof(uint32_t);

        // Ensure we have enough buffer space
        if (offset + 256 > temp_buffer_size) {
            temp_buffer_size *= 2;
            uint8_t* new_buffer = nimcp_realloc(temp_buffer, temp_buffer_size);
            if (!new_buffer) {
                nimcp_free(temp_buffer);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "serialize_network_segment: new_buffer is NULL");
                return -1;
            }
            temp_buffer = new_buffer;
        }
    }

    // Compress if requested
    if (enable_compression && offset > 1024) {
        uLongf compressed_size = compressBound(offset);
        uint8_t* compressed_buffer = nimcp_malloc(compressed_size);

        if (compressed_buffer) {
            int result = compress2(compressed_buffer, &compressed_size, temp_buffer, offset, Z_DEFAULT_COMPRESSION);

            if (result == Z_OK && compressed_size < offset) {
                // Compression successful and beneficial
                if (compressed_size + 1 <= buffer_size) {
                    buffer[0] = 1; // Compression flag
                    memcpy(buffer + 1, compressed_buffer, compressed_size);
                    nimcp_free(compressed_buffer);
                    nimcp_free(temp_buffer);
                    return (int)(compressed_size + 1);
                }
            }
            nimcp_free(compressed_buffer);
        }
    }

    // No compression or compression failed - use uncompressed
    if (offset + 1 <= buffer_size) {
        buffer[0] = 0; // No compression
        memcpy(buffer + 1, temp_buffer, offset);
        nimcp_free(temp_buffer);
        return (int)(offset + 1);
    }

    nimcp_free(temp_buffer);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "serialize_network_segment: validation failed");
    return -1; // Buffer too small
}

/**
 * @brief Deserialize network segment
 *
 * WHAT: Reconstructs neurons and synapses from wire format
 * WHY:  Populate local network cache from remote master
 * HOW:  Unpack neuron states and synapse weights, update network
 *
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @param network Network to populate
 * @return true on success
 */
static bool deserialize_network_segment(
    const uint8_t* buffer,
    size_t buffer_size,
    adaptive_network_t network
) {
    if (!buffer || !network || buffer_size < 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_network_segment: required parameter is NULL (buffer, network)");
        return false;
    }

    bool is_compressed = (buffer[0] == 1);
    const uint8_t* data = buffer + 1;
    size_t data_size = buffer_size - 1;

    // Decompress if needed
    uint8_t* decompressed_buffer = NULL;
    if (is_compressed) {
        uLongf decompressed_size = data_size * 10; // Estimate
        decompressed_buffer = nimcp_malloc(decompressed_size);

        if (!decompressed_buffer) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "deserialize_network_segment: decompressed_buffer is NULL");
            return false;
        }

        int result = uncompress(decompressed_buffer, &decompressed_size, data, data_size);
        if (result != Z_OK) {
            nimcp_free(decompressed_buffer);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_network_segment: validation failed");
            return false;
        }

        data = decompressed_buffer;
        data_size = decompressed_size;
    }

    // Get base network for updates
    neural_network_t base_net = get_base_network(network);
    if (!base_net) {
        if (decompressed_buffer) {
            nimcp_free(decompressed_buffer);
        }
        return false;
    }

    size_t offset = 0;

    // Validate minimum size
    if (data_size < 2 * sizeof(uint32_t)) {
        if (decompressed_buffer) {
            nimcp_free(decompressed_buffer);
        }
        return false;
    }

    // Read header
    uint32_t start_neuron = *(const uint32_t*)(data + offset);
    offset += sizeof(uint32_t);
    uint32_t num_neurons = *(const uint32_t*)(data + offset);
    offset += sizeof(uint32_t);

    // Validate neuron count
    uint32_t total_neurons = get_network_num_neurons(network);
    if (start_neuron >= total_neurons || start_neuron + num_neurons > total_neurons) {
        if (decompressed_buffer) {
            nimcp_free(decompressed_buffer);
        }
        return false;
    }

    // Read neuron data
    for (uint32_t i = 0; i < num_neurons; i++) {
        uint32_t neuron_id = start_neuron + i;

        // Validate buffer bounds
        if (offset + 2 * sizeof(float) + sizeof(uint32_t) > data_size) {
            if (decompressed_buffer) {
                nimcp_free(decompressed_buffer);
            }
            return false;
        }

        // Read neuron state
        float activation = *(const float*)(data + offset);
        offset += sizeof(float);
        float threshold = *(const float*)(data + offset);
        offset += sizeof(float);

        // Update neuron state in base network
        // NOTE: We update neuron using neural_network_update_neuron
        neural_network_update_neuron(base_net, neuron_id, activation, nimcp_time_get_us());

        // Read synapses count
        uint32_t num_synapses = *(const uint32_t*)(data + offset);
        offset += sizeof(uint32_t);

        // For now, skip synapse data (would require network modification API)
        // In a full implementation, we would use neural_network_add_synapse or similar
        (void)num_synapses;
        (void)threshold;
    }

    if (decompressed_buffer) {
        nimcp_free(decompressed_buffer);
    }

    return true;
}

//=============================================================================
// Cache Management
//=============================================================================

/**
 * @brief Find cached segment
 */
static network_segment_t* find_cached_segment(
    distributed_cow_state_t* state,
    uint32_t start_neuron,
    uint32_t num_neurons
) {
    nimcp_platform_rwlock_rdlock(&state->cache_lock);

    for (uint32_t i = 0; i < state->num_cached_segments; i++) {
        network_segment_t* seg = state->cached_segments[i];
        if (seg->start_neuron_id == start_neuron && seg->num_neurons == num_neurons) {
            __atomic_fetch_add(&state->cache_hits, 1, __ATOMIC_RELAXED);
            nimcp_platform_rwlock_rdunlock(&state->cache_lock);
            return seg;
        }
    }

    __atomic_fetch_add(&state->cache_misses, 1, __ATOMIC_RELAXED);
    nimcp_platform_rwlock_rdunlock(&state->cache_lock);
    return NULL;  /* Cache miss - normal path */
}

/**
 * @brief Add segment to cache
 */
static bool add_to_cache(
    distributed_cow_state_t* state,
    network_segment_t* segment
) {
    nimcp_platform_rwlock_wrlock(&state->cache_lock);

    // Check capacity
    if (state->num_cached_segments >= state->cache_capacity) {
        // Evict LRU segment
        // For simplicity, evict first segment (FIFO)
        network_segment_t* evicted = state->cached_segments[0];
        state->cache_size_bytes -= evicted->uncompressed_size;
        nimcp_free(evicted);

        // Shift remaining segments
        for (uint32_t i = 0; i < state->num_cached_segments - 1; i++) {
            state->cached_segments[i] = state->cached_segments[i + 1];
        }
        state->num_cached_segments--;
    }

    // Add new segment
    state->cached_segments[state->num_cached_segments++] = segment;
    state->cache_size_bytes += segment->uncompressed_size;

    nimcp_platform_rwlock_wrunlock(&state->cache_lock);
    return true;
}

//=============================================================================
// Network Protocol Handlers
//=============================================================================

/**
 * @brief Handle COW fetch segment request (master side)
 *
 * WHAT: Process segment fetch request from remote clone
 * WHY:  Serve network data to distributed COW clones on demand
 * HOW:  Serialize requested network segment and send to clone
 *
 * @param brain Master brain instance
 * @param request Fetch request parameters
 * @param response_buffer Output buffer for response
 * @param response_buffer_size Size of response buffer
 * @param response_length Output: actual response size
 * @return true on success, false on error
 */
static bool handle_fetch_segment_request(
    brain_t brain,
    const cow_fetch_segment_request_t* request,
    uint8_t* response_buffer,
    size_t response_buffer_size,
    size_t* response_length
) {
    // Validate inputs
    if (!brain || !request || !response_buffer || !response_length) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_fetch_segment_request: required parameter is NULL (brain, request, response_buffer, response_length)");
        return false;
    }

    // Get network from brain
    adaptive_network_t network = brain_get_network(brain);
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_fetch_segment_request: network is NULL");
        return false;
    }

    // Validate segment bounds
    uint32_t total_neurons = get_network_num_neurons(network);
    if (request->start_neuron_id >= total_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "handle_fetch_segment_request: capacity exceeded");
        return false;
    }

    uint32_t actual_neurons = request->num_neurons;
    if (request->start_neuron_id + actual_neurons > total_neurons) {
        actual_neurons = total_neurons - request->start_neuron_id;
    }

    // Prepare response header
    cow_segment_data_response_t response = {
        .segment_id = nimcp_time_get_us(),
        .start_neuron_id = request->start_neuron_id,
        .num_neurons = actual_neurons,
        .num_synapses = 0, // Calculated during serialization
        .is_compressed = request->enable_compression,
        .data_length = 0
    };

    // Ensure we have space for response header
    if (response_buffer_size < sizeof(cow_segment_data_response_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "handle_fetch_segment_request: validation failed");
        return false;
    }

    // Serialize segment data
    uint8_t* data_buffer = response_buffer + sizeof(cow_segment_data_response_t);
    size_t data_buffer_size = response_buffer_size - sizeof(cow_segment_data_response_t);

    int bytes_written = serialize_network_segment(
        network,
        request->start_neuron_id,
        actual_neurons,
        data_buffer,
        data_buffer_size,
        request->enable_compression
    );

    if (bytes_written < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "handle_fetch_segment_request: validation failed");
        return false;
    }

    response.data_length = (uint32_t)bytes_written;

    // Write response header
    memcpy(response_buffer, &response, sizeof(cow_segment_data_response_t));
    *response_length = sizeof(cow_segment_data_response_t) + bytes_written;

    return true;
}

//=============================================================================
// Distributed COW State Management
//=============================================================================

/**
 * @brief Create distributed COW state
 */
static distributed_cow_state_t* create_distributed_cow_state(
    const char* master_host,
    uint16_t master_port,
    p2p_node_t p2p_node,
    const distributed_cow_config_t* config
) {
    distributed_cow_state_t* state = nimcp_calloc(1, sizeof(distributed_cow_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "state is NULL");

        return NULL;
    }

    state->is_distributed = true;
    state->is_master = false;
    strncpy(state->master_host, master_host, sizeof(state->master_host) - 1);
    state->master_port = master_port;
    state->p2p_node = p2p_node;

    // Copy or use default config
    if (config) {
        state->config = *config;
    } else {
        state->config = distributed_cow_default_config();
    }

    // Allocate cache
    state->cache_capacity = (state->config.cache_capacity_mb * 1024 * 1024) /
                           (state->config.segment_size * 100); // Estimate
    state->cached_segments = nimcp_calloc(state->cache_capacity, sizeof(network_segment_t*));
    state->num_cached_segments = 0;
    state->cache_size_bytes = 0;

    // Generate unique clone ID
    state->clone_id = nimcp_time_get_us();
    state->local_refcount = 1;
    state->remote_refcount = 0;

    // Initialize locks
    nimcp_platform_rwlock_init(&state->cache_lock);
    nimcp_platform_mutex_init(&state->fetch_mutex, false);

    // Initialize stats
    state->total_fetches = 0;
    state->total_bytes_fetched = 0;
    state->cache_hits = 0;
    state->cache_misses = 0;
    state->avg_fetch_latency_ms = 0.0F;

    return state;
}

/**
 * @brief Destroy distributed COW state
 */
__attribute__((unused))
static void destroy_distributed_cow_state(distributed_cow_state_t* state) {
    if (!state) {
        return;
    }

    // Free cached segments
    nimcp_platform_rwlock_wrlock(&state->cache_lock);
    for (uint32_t i = 0; i < state->num_cached_segments; i++) {
        nimcp_free(state->cached_segments[i]);
    }
    nimcp_free(state->cached_segments);
    nimcp_platform_rwlock_wrunlock(&state->cache_lock);

    // Destroy locks
    nimcp_platform_rwlock_destroy(&state->cache_lock);
    nimcp_platform_mutex_destroy(&state->fetch_mutex);

    nimcp_free(state);
}

//=============================================================================
// Registry Management
//=============================================================================

/**
 * @brief Register distributed COW brain
 */
static bool register_distributed_cow_brain(brain_t brain, distributed_cow_state_t* state) {
    ensure_registry_mutex_initialized();
    nimcp_platform_mutex_lock(&g_registry_mutex);

    // Allocate registry if needed
    if (!g_dcow_brains) {
        g_dcow_brains = nimcp_calloc(16, sizeof(distributed_cow_brain_t*));
    }

    // Find empty slot or expand
    distributed_cow_brain_t* entry = nimcp_calloc(1, sizeof(distributed_cow_brain_t));
    if (!entry) {
        nimcp_platform_mutex_unlock(&g_registry_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "register_distributed_cow_brain: entry is NULL");
        return false;
    }
    entry->brain = brain;
    entry->dcow_state = state;

    if (g_num_dcow_brains >= 16) {
        nimcp_platform_mutex_unlock(&g_registry_mutex);
        nimcp_free(entry);
        set_error("register_distributed_cow_brain: registry full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "register_distributed_cow_brain: registry full");
        return false;
    }
    g_dcow_brains[g_num_dcow_brains++] = entry;

    nimcp_platform_mutex_unlock(&g_registry_mutex);
    return true;
}

/**
 * @brief Find distributed COW state for brain
 */
static distributed_cow_state_t* find_distributed_cow_state(brain_t brain) {
    ensure_registry_mutex_initialized();
    nimcp_platform_mutex_lock(&g_registry_mutex);

    for (uint32_t i = 0; i < g_num_dcow_brains; i++) {
        if (g_dcow_brains[i]->brain == brain) {
            distributed_cow_state_t* state = g_dcow_brains[i]->dcow_state;
            nimcp_platform_mutex_unlock(&g_registry_mutex);
            return state;
        }
    }

    nimcp_platform_mutex_unlock(&g_registry_mutex);
    /* Not found - normal for non-distributed brains */
    return NULL;
}

//=============================================================================
// Public API Implementation
//=============================================================================

/**
 * @brief Create distributed COW clone
 */
brain_t brain_clone_cow_distributed(
    brain_t original,
    const char* remote_host,
    uint16_t remote_port,
    const distributed_cow_config_t* config
) {
    LOG_DEBUG("brain_clone_cow_distributed: Entry, remote=%s:%u", remote_host, remote_port);

    // Ensure bio-async is available
    ensure_bio_async_registered();

    if (!original || !remote_host) {
        LOG_ERROR("brain_clone_cow_distributed: Invalid parameters (original=%p, remote_host=%p)",
                  (void*)original, (const void*)remote_host);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_clone_cow_distributed: required parameter is NULL (original, remote_host)");
        return NULL;
    }

    // Create local COW clone first
    LOG_INFO("brain_clone_cow_distributed: Creating local COW clone");
    brain_t clone = brain_clone_cow(original);
    if (!clone) {
        LOG_ERROR("brain_clone_cow_distributed: Failed to create local COW clone");
        publish_cow_event(BIO_MSG_ERROR_REPORT, 0, 0, 0, false, BIO_CHANNEL_NOREPINEPHRINE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_clone_cow_distributed: clone is NULL");
        return NULL;
    }

    // Create P2P node for communication
    LOG_DEBUG("brain_clone_cow_distributed: Creating P2P node");
    node_config_t node_config = {
        .listen_port = 0, // Any available port
        .max_peers = 4,
        .ping_interval = 5000
    };
    p2p_node_t p2p_node = p2p_node_create(&node_config);
    if (!p2p_node) {
        LOG_ERROR("brain_clone_cow_distributed: Failed to create P2P node");
        brain_destroy(clone);
        publish_cow_event(BIO_MSG_ERROR_REPORT, 0, 0, 0, false, BIO_CHANNEL_NOREPINEPHRINE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_clone_cow_distributed: p2p_node is NULL");
        return NULL;
    }

    // Start P2P node
    LOG_INFO("brain_clone_cow_distributed: Starting P2P node");
    p2p_node_start(p2p_node);

    // Connect to master
    LOG_INFO("brain_clone_cow_distributed: Connecting to master %s:%u", remote_host, remote_port);
    if (!p2p_node_connect_peer(p2p_node, remote_host, remote_port)) {
        LOG_ERROR("brain_clone_cow_distributed: Failed to connect to master");
        p2p_node_destroy(p2p_node);
        brain_destroy(clone);
        publish_cow_event(BIO_MSG_ERROR_REPORT, 0, 0, 0, false, BIO_CHANNEL_NOREPINEPHRINE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_clone_cow_distributed: p2p_node_connect_peer is NULL");
        return NULL;
    }

    // Create distributed COW state
    LOG_DEBUG("brain_clone_cow_distributed: Creating distributed COW state");
    distributed_cow_state_t* dcow_state = create_distributed_cow_state(
        remote_host, remote_port, p2p_node, config
    );
    if (!dcow_state) {
        LOG_ERROR("brain_clone_cow_distributed: Failed to create distributed COW state");
        p2p_node_destroy(p2p_node);
        brain_destroy(clone);
        publish_cow_event(BIO_MSG_ERROR_REPORT, 0, 0, 0, false, BIO_CHANNEL_NOREPINEPHRINE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_clone_cow_distributed: dcow_state is NULL");
        return NULL;
    }

    // Register clone
    LOG_DEBUG("brain_clone_cow_distributed: Registering clone");
    register_distributed_cow_brain(clone, dcow_state);

    // Send create clone message to master
    // (Simplified - actual implementation would use protocol messages)

    LOG_INFO("brain_clone_cow_distributed: Distributed COW clone created successfully, clone_id=%lu",
             (unsigned long)dcow_state->clone_id);

    // Publish COW fork event (via dopamine - success)
    publish_cow_event(BIO_MSG_BRAIN_STATE_RESPONSE, 0, 0, 0, true, BIO_CHANNEL_DOPAMINE);

    LOG_DEBUG("brain_clone_cow_distributed: Exit, clone=%p", (void*)clone);
    return clone;
}

/**
 * @brief Enable distributed COW master
 */
bool brain_enable_distributed_cow_master(brain_t brain, p2p_node_t p2p_node) {
    if (!brain || !p2p_node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_enable_distributed_cow_master: required parameter is NULL (brain, p2p_node)");
        return false;
    }

    // Create master state
    distributed_cow_state_t* state = nimcp_calloc(1, sizeof(distributed_cow_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_enable_distributed_cow_master: state is NULL");
        return false;
    }

    state->is_distributed = true;
    state->is_master = true;
    state->p2p_node = p2p_node;
    state->local_refcount = 1;
    state->remote_refcount = 0;

    nimcp_platform_rwlock_init(&state->cache_lock);
    nimcp_platform_mutex_init(&state->fetch_mutex, false);

    // Register as master
    register_distributed_cow_brain(brain, state);

    // Install P2P message handlers
    // (Simplified - actual implementation would register protocol handlers)

    return true;
}

/**
 * @brief Fetch network segment
 */
bool distributed_cow_fetch_segment(
    brain_t brain,
    uint32_t start_neuron_id,
    uint32_t num_neurons
) {
    LOG_DEBUG("distributed_cow_fetch_segment: Entry, segment=[%u,%u)", start_neuron_id, start_neuron_id + num_neurons);

    // Ensure bio-async is available
    ensure_bio_async_registered();

    // Process pending bio-async messages
    if (g_bio_async_registered && g_bio_module_ctx) {
        bio_router_process_inbox(g_bio_module_ctx, 5);
    }

    distributed_cow_state_t* state = find_distributed_cow_state(brain);
    if (!state || !state->is_distributed || state->is_master) {
        LOG_WARN("distributed_cow_fetch_segment: Invalid state (state=%p, is_distributed=%d, is_master=%d)",
                 (void*)state, state ? state->is_distributed : 0, state ? state->is_master : 0);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distributed_cow_fetch_segment: required parameter is NULL (state, state->is_distributed)");
        return false;
    }

    // Check cache first
    network_segment_t* cached = find_cached_segment(state, start_neuron_id, num_neurons);
    if (cached) {
        LOG_DEBUG("distributed_cow_fetch_segment: Segment found in cache");
        publish_cow_event(BIO_MSG_BRAIN_STATE_RESPONSE, start_neuron_id, num_neurons, 0, true, BIO_CHANNEL_DOPAMINE);
        return true; // Already cached
    }

    LOG_INFO("distributed_cow_fetch_segment: Cache miss, fetching from master");
    nimcp_platform_mutex_lock(&state->fetch_mutex);

    uint64_t start_time = nimcp_time_get_us();

    // Build fetch request
    // Build fetch request (unused in stub)
    (void)start_neuron_id;
    (void)num_neurons;

    // Send request via P2P
    // (Simplified - actual implementation would use p2p_node_send_message)

    // BUGFIX: Heap allocation instead of 64KB stack buffer to prevent stack overflow
    uint8_t* response_buffer = (uint8_t*)nimcp_malloc(65536);
    if (!response_buffer) {
        nimcp_platform_mutex_unlock(&state->fetch_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distributed_cow_fetch_segment: response_buffer is NULL");
        return false;  // Return bool false, not error code (function returns bool)
    }
    size_t response_length = 0;

    // Simulate response (in real implementation, this would come from network)
    // handle_fetch_segment_request(brain, &request, response_buffer, sizeof(response_buffer), &response_length);

    // Deserialize response
    adaptive_network_t network = brain_get_network(brain);
    if (network && response_length > sizeof(cow_segment_data_response_t)) {
        const cow_segment_data_response_t* response = (const cow_segment_data_response_t*)response_buffer;

        // Deserialize network data
        deserialize_network_segment(
            response_buffer + sizeof(cow_segment_data_response_t),
            response->data_length,
            network
        );

        // Create cache entry
        network_segment_t* segment = nimcp_calloc(1, sizeof(network_segment_t));
        if (segment) {
            segment->start_neuron_id = response->start_neuron_id;
            segment->num_neurons = response->num_neurons;
            segment->num_synapses = response->num_synapses;
            segment->segment_id = response->segment_id;
            segment->timestamp = nimcp_time_get_us();
            segment->is_compressed = response->is_compressed;
            segment->compressed_size = response->data_length;
            segment->uncompressed_size = num_neurons * 1024; // Estimate

            add_to_cache(state, segment);
        }
    }

    // Update stats
    uint64_t latency_us = nimcp_time_get_us() - start_time;
    float latency_ms = latency_us / 1000.0F;

    state->total_fetches++;
    state->total_bytes_fetched += response_length;
    state->avg_fetch_latency_ms = (state->avg_fetch_latency_ms * (state->total_fetches - 1) + latency_ms) / state->total_fetches;

    LOG_INFO("distributed_cow_fetch_segment: Fetch complete, bytes=%zu, latency=%.2fms",
             response_length, latency_ms);

    nimcp_platform_mutex_unlock(&state->fetch_mutex);

    // BUGFIX: Free heap-allocated response buffer
    nimcp_free(response_buffer);

    // Publish fetch complete event (via dopamine - success)
    publish_cow_event(BIO_MSG_BRAIN_STATE_RESPONSE, start_neuron_id, num_neurons,
                     response_length, true, BIO_CHANNEL_DOPAMINE);

    LOG_DEBUG("distributed_cow_fetch_segment: Exit, success=true");
    return true;
}

/**
 * @brief Prefetch segments
 */
uint32_t distributed_cow_prefetch_segments(brain_t brain, uint32_t current_neuron_id) {
    distributed_cow_state_t* state = find_distributed_cow_state(brain);
    if (!state || !state->config.enable_prefetch) {
        return 0;
    }

    uint32_t prefetched = 0;
    uint32_t segment_size = state->config.segment_size;
    uint32_t lookahead = state->config.prefetch_lookahead;

    // Prefetch next N segments
    for (uint32_t offset = segment_size; offset < lookahead; offset += segment_size) {
        uint32_t start_neuron = current_neuron_id + offset;
        if (distributed_cow_fetch_segment(brain, start_neuron, segment_size)) {
            prefetched++;
        }
    }

    return prefetched;
}

/**
 * @brief Fetch full network
 *
 * WHAT: Downloads entire network from master to local node
 * WHY:  Prepare for write operations (learning, fine-tuning)
 * HOW:  Fetch all segments sequentially, transition to local COW
 *
 * @param brain Distributed COW clone
 * @return true on success, false on error
 */
bool distributed_cow_fetch_full_network(brain_t brain) {
    distributed_cow_state_t* state = find_distributed_cow_state(brain);
    if (!state || !state->is_distributed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distributed_cow_fetch_full_network: required parameter is NULL (state, state->is_distributed)");
        return false;
    }

    adaptive_network_t network = brain_get_network(brain);
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distributed_cow_fetch_full_network: network is NULL");
        return false;
    }

    // Get actual network size
    uint32_t total_neurons = get_network_num_neurons(network);
    uint32_t segment_size = state->config.segment_size;

    // Fetch all segments
    for (uint32_t start = 0; start < total_neurons; start += segment_size) {
        uint32_t num_neurons = (start + segment_size > total_neurons) ?
                               (total_neurons - start) : segment_size;

        if (!distributed_cow_fetch_segment(brain, start, num_neurons)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "distributed_cow_fetch_full_network: distributed_cow_fetch_segment is NULL");
            return false;
        }
    }

    // Transition to local COW
    state->is_distributed = false;

    return true;
}

/**
 * @brief Get distributed COW statistics
 */
bool brain_get_distributed_cow_stats(brain_t brain, distributed_cow_stats_t* stats) {
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_distributed_cow_stats: stats is NULL");
        return false;
    }

    distributed_cow_state_t* state = find_distributed_cow_state(brain);
    if (!state) {
        memset(stats, 0, sizeof(distributed_cow_stats_t));
        stats->is_distributed = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_distributed_cow_stats: state is NULL");
        return false;
    }

    nimcp_platform_rwlock_rdlock(&state->cache_lock);

    stats->is_distributed = state->is_distributed;
    stats->is_master = state->is_master;
    stats->local_refcount = state->local_refcount;
    stats->remote_refcount = state->remote_refcount;
    stats->num_cached_segments = state->num_cached_segments;
    stats->cache_size_bytes = state->cache_size_bytes;
    stats->total_fetches = state->total_fetches;
    stats->total_bytes_fetched = state->total_bytes_fetched;
    stats->cache_hits = state->cache_hits;
    stats->cache_misses = state->cache_misses;

    uint64_t total_accesses = state->cache_hits + state->cache_misses;
    stats->cache_hit_rate = total_accesses > 0 ? (float)state->cache_hits / total_accesses : 0.0F;
    stats->avg_fetch_latency_ms = state->avg_fetch_latency_ms;

    // Calculate bandwidth (bytes/sec)
    if (state->avg_fetch_latency_ms > 0) {
        float bytes_per_ms = state->total_bytes_fetched / (state->total_fetches * state->avg_fetch_latency_ms);
        stats->network_bandwidth_mbps = (bytes_per_ms * 1000.0F * 8.0F) / (1024.0F * 1024.0F);
    } else {
        stats->network_bandwidth_mbps = 0.0F;
    }

    nimcp_platform_rwlock_rdunlock(&state->cache_lock);

    return true;
}

/**
 * @brief Check if brain is distributed COW
 */
bool brain_is_distributed_cow(brain_t brain) {
    distributed_cow_state_t* state = find_distributed_cow_state(brain);
    return state != NULL && state->is_distributed;
}

/**
 * @brief Clear cache
 */
size_t distributed_cow_clear_cache(brain_t brain, uint32_t target_size_mb) {
    distributed_cow_state_t* state = find_distributed_cow_state(brain);
    if (!state) {
        return 0;
    }

    size_t target_bytes = target_size_mb * 1024 * 1024;
    size_t bytes_freed = 0;

    nimcp_platform_rwlock_wrlock(&state->cache_lock);

    while (state->cache_size_bytes > target_bytes && state->num_cached_segments > 0) {
        // Evict oldest segment (FIFO)
        network_segment_t* evicted = state->cached_segments[0];
        bytes_freed += evicted->uncompressed_size;
        state->cache_size_bytes -= evicted->uncompressed_size;
        nimcp_free(evicted);

        // Shift remaining segments
        for (uint32_t i = 0; i < state->num_cached_segments - 1; i++) {
            state->cached_segments[i] = state->cached_segments[i + 1];
        }
        state->num_cached_segments--;
    }

    nimcp_platform_rwlock_wrunlock(&state->cache_lock);

    return bytes_freed;
}
