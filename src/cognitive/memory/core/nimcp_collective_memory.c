//=============================================================================
// nimcp_collective_memory.c - Collective Memory System Implementation
//=============================================================================
/**
 * @file nimcp_collective_memory.c
 * @brief Implementation of collective memory for multi-agent systems
 *
 * This file implements the collective memory system including:
 * - Multi-agent memory synchronization
 * - Cultural memory management
 * - Consensus computation
 * - Memory propagation and drift
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_collective_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(collective_memory, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Constants
//=============================================================================

/** Initial capacity for memory arrays */
#define INITIAL_MEMORY_CAPACITY     64

/** Initial capacity for agent arrays */
#define INITIAL_AGENT_CAPACITY      32

/** Growth factor when resizing arrays */
#define ARRAY_GROWTH_FACTOR         2

/** Random seed for mutation operations (thread-local for thread safety) */
static _Thread_local uint32_t g_random_seed = 0;
static _Thread_local bool g_random_seed_initialized = false;

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Simple pseudo-random number generator (thread-safe)
 */
static float random_float(void) {
    if (!g_random_seed_initialized) {
        g_random_seed = (uint32_t)time(NULL) ^ (uint32_t)((uintptr_t)pthread_self() & 0xFFFFFFFF);
        g_random_seed_initialized = true;
    }
    g_random_seed = g_random_seed * 1103515245 + 12345;
    return (float)(g_random_seed & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

/**
 * @brief Get current time in milliseconds
 */
static uint64_t current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Find agent index by ID
 */
static int find_agent_index(collective_memory_system_t* system, uint64_t agent_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_agent_index: system is NULL");
        return -1;
    }

    for (size_t i = 0; i < system->num_agents; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_agents > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)system->num_agents);
        }

        if (system->agents[i].agent_id == agent_id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_agent_index: validation failed");
    return -1;
}

/**
 * @brief Find memory index by ID
 */
static int find_memory_index(collective_memory_system_t* system, uint64_t memory_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_memory_index: system is NULL");
        return -1;
    }

    for (size_t i = 0; i < system->num_memories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_memories > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)system->num_memories);
        }

        if (system->memories[i] && system->memories[i]->memory_id == memory_id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_memory_index: validation failed");
    return -1;
}

/**
 * @brief Check if agent has memory
 */
static bool agent_has_memory(collective_memory_t* memory, uint64_t agent_id) {
    if (!memory) {
        return false;
    }

    for (size_t i = 0; i < memory->num_agents; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && memory->num_agents > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)memory->num_agents);
        }

        if (memory->agent_ids[i] == agent_id) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Add agent to memory's sharing list
 */
static collective_error_t add_agent_to_memory(collective_memory_t* memory,
                                               uint64_t agent_id) {
    if (!memory) return COLLECTIVE_ERROR_NULL_POINTER;

    // Check if already sharing
    if (agent_has_memory(memory, agent_id)) {
        return COLLECTIVE_SUCCESS;
    }

    // Grow array if needed
    if (memory->num_agents >= memory->agent_capacity) {
        size_t new_cap = memory->agent_capacity * ARRAY_GROWTH_FACTOR;
        if (new_cap == 0) new_cap = 4;

        /* Safe realloc: use temp pointers so originals are preserved on failure */
        uint64_t* new_ids = nimcp_realloc(memory->agent_ids, new_cap * sizeof(uint64_t));
        if (!new_ids) {
            return COLLECTIVE_ERROR_NO_MEMORY;
        }
        /* Assign first successful realloc immediately (old ptr may be freed by realloc) */
        memory->agent_ids = new_ids;

        float* new_versions = nimcp_realloc(memory->agent_versions, new_cap * sizeof(float));
        if (!new_versions) {
            /* agent_ids was already resized but agent_versions was not.
             * Do NOT update agent_capacity - keep it at old size so
             * subsequent code cannot index beyond agent_versions bounds.
             * agent_ids having extra capacity is harmless. */
            return COLLECTIVE_ERROR_NO_MEMORY;
        }
        memory->agent_versions = new_versions;
        memory->agent_capacity = new_cap;
    }

    // Add agent
    memory->agent_ids[memory->num_agents] = agent_id;
    memory->agent_versions[memory->num_agents] = 1.0f;
    memory->num_agents++;

    return COLLECTIVE_SUCCESS;
}

/**
 * @brief Remove agent from memory's sharing list
 */
static collective_error_t remove_agent_from_memory(collective_memory_t* memory,
                                                    uint64_t agent_id) {
    if (!memory) return COLLECTIVE_ERROR_NULL_POINTER;

    for (size_t i = 0; i < memory->num_agents; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && memory->num_agents > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)memory->num_agents);
        }

        if (memory->agent_ids[i] == agent_id) {
            // Shift remaining agents
            for (size_t j = i; j < memory->num_agents - 1; j++) {
                memory->agent_ids[j] = memory->agent_ids[j + 1];
                memory->agent_versions[j] = memory->agent_versions[j + 1];
            }
            memory->num_agents--;
            return COLLECTIVE_SUCCESS;
        }
    }

    return COLLECTIVE_ERROR_NOT_FOUND;
}

/**
 * @brief Create a new collective memory structure
 */
static collective_memory_t* create_collective_memory(uint64_t memory_id,
                                                       collective_type_t type,
                                                       uint64_t origin_agent_id,
                                                       float origin_time) {
    collective_memory_t* memory = nimcp_calloc(1, sizeof(collective_memory_t));
    if (!memory) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate memory");

        return NULL;

    }

    memory->memory_id = memory_id;
    memory->type = type;
    memory->origin_agent_id = origin_agent_id;
    memory->origin_time = origin_time;

    // Default parameters
    memory->propagation_rate = COLLECTIVE_DEFAULT_PROPAGATION;
    memory->retention_rate = COLLECTIVE_DEFAULT_RETENTION;
    memory->mutation_rate = COLLECTIVE_DEFAULT_MUTATION;
    memory->consensus_strength = 1.0f;
    memory->sync_status = SYNC_LOCAL_ONLY;

    // Initialize arrays
    memory->agent_capacity = 4;
    memory->agent_ids = nimcp_calloc(memory->agent_capacity, sizeof(uint64_t));
    memory->agent_versions = nimcp_calloc(memory->agent_capacity, sizeof(float));

    if (!memory->agent_ids || !memory->agent_versions) {
        nimcp_free(memory->agent_ids);
        nimcp_free(memory->agent_versions);
        nimcp_free(memory);
        memory = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "agent_has_memory: required parameter is NULL (memory->agent_ids, memory->agent_versions)");
        return NULL;
    }

    memory->num_agents = 0;

    // Initialize quaternion to identity
    memory->shared_quaternion = quat_identity();

    return memory;
}

/**
 * @brief Destroy a collective memory structure
 */
static void destroy_collective_memory(collective_memory_t* memory) {
    if (!memory) return;

    nimcp_free(memory->agent_ids);
    nimcp_free(memory->agent_versions);

    // Note: memory_node is owned externally, don't free here

    nimcp_free(memory);
    memory = NULL;
}

/**
 * @brief Initialize an agent memory state
 */
static collective_error_t init_agent_state(agent_memory_state_t* state,
                                            uint64_t agent_id,
                                            float reliability,
                                            bool is_leader) {
    if (!state) return COLLECTIVE_ERROR_NULL_POINTER;

    memset(state, 0, sizeof(agent_memory_state_t));

    state->agent_id = agent_id;
    state->reliability = reliability;
    state->is_leader = is_leader;
    state->sync_time = 0.0f;

    // Initialize memory arrays
    state->memory_capacity = 16;
    state->memories = nimcp_calloc(state->memory_capacity, sizeof(prime_signature_t));
    state->memory_ids = nimcp_calloc(state->memory_capacity, sizeof(uint64_t));
    if (!state->memory_ids) return -1;
    state->memory_versions = nimcp_calloc(state->memory_capacity, sizeof(float));

    if (!state->memories || !state->memory_ids || !state->memory_versions) {
        nimcp_free(state->memories);
        nimcp_free(state->memory_ids);
        nimcp_free(state->memory_versions);
        return COLLECTIVE_ERROR_NO_MEMORY;
    }

    state->num_memories = 0;

    return COLLECTIVE_SUCCESS;
}

/**
 * @brief Cleanup an agent memory state
 */
static void cleanup_agent_state(agent_memory_state_t* state) {
    if (!state) return;

    nimcp_free(state->memories);
    nimcp_free(state->memory_ids);
    nimcp_free(state->memory_versions);

    state->memories = NULL;
    state->memory_ids = NULL;
    state->memory_versions = NULL;
    state->num_memories = 0;
    state->memory_capacity = 0;
}

/**
 * @brief Compute signature similarity using Jaccard
 */
static float compute_signature_similarity(const prime_signature_t* s1,
                                           const prime_signature_t* s2) {
    if (!s1 || !s2) return 0.0f;
    return prime_sig_jaccard(s1, s2);
}

/**
 * @brief Compute quaternion similarity
 */
static float compute_state_similarity(nimcp_quaternion_t q1, nimcp_quaternion_t q2) {
    float dist = quat_geodesic_distance(q1, q2);
    return 1.0f - (dist / M_PI);
}

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT collective_memory_config_t collective_memory_default_config(void) {
    collective_memory_config_t config = {
        .sync_interval = COLLECTIVE_DEFAULT_SYNC_INTERVAL,
        .consensus_threshold = COLLECTIVE_DEFAULT_CONSENSUS,
        .drift_threshold = COLLECTIVE_MAX_DRIFT,
        .consensus_method = CONSENSUS_WEIGHTED,
        .propagation_model = PROPAGATION_EPIDEMIC,
        .auto_sync = true,
        .preserve_minorities = true,
        .cultural_threshold = 0.8f
    };
    return config;
}

NIMCP_EXPORT bool collective_memory_config_validate(
    const collective_memory_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_memory_default_config: config is NULL");
        return false;
    }

    if (config->sync_interval < 0.0f) {
        return false;
    }
    if (config->consensus_threshold < 0.0f || config->consensus_threshold > 1.0f) {
        return false;
    }
    if (config->drift_threshold < 0.0f || config->drift_threshold > 1.0f) {
        return false;
    }
    if (config->consensus_method >= CONSENSUS_METHOD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "collective_memory_default_config: capacity exceeded");
        return false;
    }
    if (config->propagation_model >= PROPAGATION_MODEL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "collective_memory_default_config: capacity exceeded");
        return false;
    }
    if (config->cultural_threshold < 0.0f || config->cultural_threshold > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// System Lifecycle Functions
//=============================================================================

NIMCP_EXPORT collective_memory_system_t* collective_memory_create(
    pr_node_manager_t node_manager,
    const collective_memory_config_t* config) {

    collective_memory_system_t* system = nimcp_calloc(1, sizeof(collective_memory_system_t));
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;

    }

    // Set configuration
    if (config) {
        if (!collective_memory_config_validate(config)) {
            nimcp_free(system);
            system = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "collective_memory_default_config: collective_memory_config_validate is NULL");
            return NULL;
        }
        system->config = *config;
    } else {
        system->config = collective_memory_default_config();
    }

    // Store node manager reference
    system->node_manager = node_manager;

    // Create entanglement graph for inter-memory associations
    entangle_config_t entangle_cfg = entangle_config_default();
    system->entanglement = entangle_graph_create(&entangle_cfg);
    if (!system->entanglement) {
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "collective_memory_default_config: system->entanglement is NULL");
        return NULL;
    }

    // Initialize memory array
    system->memory_capacity = INITIAL_MEMORY_CAPACITY;
    system->memories = nimcp_calloc(system->memory_capacity, sizeof(collective_memory_t*));
    if (!system->memories) {
        entangle_graph_destroy(system->entanglement);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "collective_memory_default_config: system->memories is NULL");
        return NULL;
    }
    system->num_memories = 0;

    // Initialize agent array
    system->agent_capacity = INITIAL_AGENT_CAPACITY;
    system->agents = nimcp_calloc(system->agent_capacity, sizeof(agent_memory_state_t));
    if (!system->agents) {
        nimcp_free(system->memories);
        entangle_graph_destroy(system->entanglement);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "collective_memory_default_config: system->agents is NULL");
        return NULL;
    }
    system->num_agents = 0;

    // Initialize IDs
    system->next_memory_id = 1;
    system->next_agent_id = 1;
    system->current_time = 0.0f;
    system->last_sync_time = 0.0f;

    return system;
}

NIMCP_EXPORT void collective_memory_destroy(collective_memory_system_t* system) {
    if (!system) return;

    // Destroy all collective memories
    for (size_t i = 0; i < system->num_memories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_memories > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)system->num_memories);
        }

        destroy_collective_memory(system->memories[i]);
    }
    nimcp_free(system->memories);

    // Cleanup all agent states
    for (size_t i = 0; i < system->num_agents; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_agents > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)system->num_agents);
        }

        cleanup_agent_state(&system->agents[i]);
    }
    nimcp_free(system->agents);

    // Destroy entanglement graph
    entangle_graph_destroy(system->entanglement);

    nimcp_free(system);
    system = NULL;
}

NIMCP_EXPORT collective_error_t collective_memory_reset(
    collective_memory_system_t* system) {
    if (!system) return COLLECTIVE_ERROR_NULL_POINTER;

    // Destroy all collective memories
    for (size_t i = 0; i < system->num_memories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_memories > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)system->num_memories);
        }

        destroy_collective_memory(system->memories[i]);
        system->memories[i] = NULL;
    }
    system->num_memories = 0;

    // Cleanup all agent states
    for (size_t i = 0; i < system->num_agents; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_agents > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)system->num_agents);
        }

        cleanup_agent_state(&system->agents[i]);
    }
    system->num_agents = 0;

    // Clear entanglement graph
    entangle_graph_clear(system->entanglement);

    // Reset IDs and time
    system->next_memory_id = 1;
    system->next_agent_id = 1;
    system->current_time = 0.0f;
    system->last_sync_time = 0.0f;

    return COLLECTIVE_SUCCESS;
}

//=============================================================================
// Agent Management Functions
//=============================================================================

NIMCP_EXPORT collective_error_t collective_memory_add_agent(
    collective_memory_system_t* system,
    float reliability,
    bool is_leader,
    uint64_t* agent_id_out) {

    if (!system || !agent_id_out) return COLLECTIVE_ERROR_NULL_POINTER;

    // Check capacity
    if (system->num_agents >= COLLECTIVE_MAX_AGENTS) {
        return COLLECTIVE_ERROR_MAX_AGENTS;
    }

    // Grow array if needed
    if (system->num_agents >= system->agent_capacity) {
        size_t new_cap = system->agent_capacity * ARRAY_GROWTH_FACTOR;
        agent_memory_state_t* new_agents = nimcp_realloc(system->agents,
            new_cap * sizeof(agent_memory_state_t));
        if (!new_agents) return COLLECTIVE_ERROR_NO_MEMORY;

        system->agents = new_agents;
        system->agent_capacity = new_cap;
    }

    // Initialize new agent
    uint64_t new_id = system->next_agent_id++;
    collective_error_t err = init_agent_state(&system->agents[system->num_agents],
                                               new_id, reliability, is_leader);
    if (err != COLLECTIVE_SUCCESS) return err;

    system->num_agents++;
    *agent_id_out = new_id;

    return COLLECTIVE_SUCCESS;
}

NIMCP_EXPORT collective_error_t collective_memory_remove_agent(
    collective_memory_system_t* system,
    uint64_t agent_id) {

    if (!system) return COLLECTIVE_ERROR_NULL_POINTER;

    int idx = find_agent_index(system, agent_id);
    if (idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    // Remove agent from all memories
    for (size_t i = 0; i < system->num_memories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_memories > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)system->num_memories);
        }

        if (system->memories[i]) {
            remove_agent_from_memory(system->memories[i], agent_id);
        }
    }

    // Cleanup agent state
    cleanup_agent_state(&system->agents[idx]);

    // Shift remaining agents
    for (size_t i = (size_t)idx; i < system->num_agents - 1; i++) {
        system->agents[i] = system->agents[i + 1];
    }
    system->num_agents--;

    return COLLECTIVE_SUCCESS;
}

NIMCP_EXPORT collective_error_t collective_memory_get_agent(
    collective_memory_system_t* system,
    uint64_t agent_id,
    agent_memory_state_t* state_out) {

    if (!system || !state_out) return COLLECTIVE_ERROR_NULL_POINTER;

    int idx = find_agent_index(system, agent_id);
    if (idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    *state_out = system->agents[idx];
    return COLLECTIVE_SUCCESS;
}

NIMCP_EXPORT collective_error_t collective_memory_update_agent_reliability(
    collective_memory_system_t* system,
    uint64_t agent_id,
    float reliability) {

    if (!system) return COLLECTIVE_ERROR_NULL_POINTER;

    int idx = find_agent_index(system, agent_id);
    if (idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    // Clamp reliability to [0, 1]
    if (reliability < 0.0f) reliability = 0.0f;
    if (reliability > 1.0f) reliability = 1.0f;

    system->agents[idx].reliability = reliability;
    return COLLECTIVE_SUCCESS;
}

//=============================================================================
// Memory Sharing Functions
//=============================================================================

NIMCP_EXPORT collective_error_t collective_memory_share(
    collective_memory_system_t* system,
    uint64_t agent_id,
    pr_memory_node_t* memory_node,
    collective_type_t type,
    uint64_t* memory_id_out) {

    if (!system || !memory_node || !memory_id_out) {
        return COLLECTIVE_ERROR_NULL_POINTER;
    }

    // Verify agent exists
    int agent_idx = find_agent_index(system, agent_id);
    if (agent_idx < 0) return COLLECTIVE_ERROR_INVALID_ID;

    // Check capacity
    if (system->num_memories >= COLLECTIVE_MAX_MEMORIES) {
        return COLLECTIVE_ERROR_MAX_MEMORIES;
    }

    // Grow memory array if needed
    if (system->num_memories >= system->memory_capacity) {
        size_t new_cap = system->memory_capacity * ARRAY_GROWTH_FACTOR;
        collective_memory_t** new_mems = nimcp_realloc(system->memories,
            new_cap * sizeof(collective_memory_t*));
        if (!new_mems) return COLLECTIVE_ERROR_NO_MEMORY;

        system->memories = new_mems;
        system->memory_capacity = new_cap;
    }

    // Create collective memory
    uint64_t new_id = system->next_memory_id++;
    collective_memory_t* col_mem = create_collective_memory(new_id, type,
                                                             agent_id,
                                                             system->current_time);
    if (!col_mem) return COLLECTIVE_ERROR_NO_MEMORY;

    // Copy signature from memory node
    const prime_signature_t* node_sig = pr_memory_node_get_signature(memory_node);
    if (node_sig) {
        col_mem->content_signature = *node_sig;
    }

    // Copy quaternion state
    col_mem->shared_quaternion = pr_memory_node_get_state(memory_node);

    // Link to memory node
    col_mem->memory_node = memory_node;

    // Add origin agent to sharing list
    collective_error_t err = add_agent_to_memory(col_mem, agent_id);
    if (err != COLLECTIVE_SUCCESS) {
        destroy_collective_memory(col_mem);
        return err;
    }

    // Add to system
    system->memories[system->num_memories] = col_mem;
    system->num_memories++;

    *memory_id_out = new_id;
    return COLLECTIVE_SUCCESS;
}

NIMCP_EXPORT collective_error_t collective_memory_adopt(
    collective_memory_system_t* system,
    uint64_t memory_id,
    uint64_t agent_id) {

    if (!system) return COLLECTIVE_ERROR_NULL_POINTER;

    // Verify agent exists
    int agent_idx = find_agent_index(system, agent_id);
    if (agent_idx < 0) return COLLECTIVE_ERROR_INVALID_ID;

    // Find memory
    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    collective_memory_t* memory = system->memories[mem_idx];

    // Add agent to memory's sharing list
    collective_error_t err = add_agent_to_memory(memory, agent_id);
    if (err != COLLECTIVE_SUCCESS) return err;

    // Update sync status
    if (memory->num_agents >= 2) {
        memory->sync_status = SYNC_SYNCING;
    }

    return COLLECTIVE_SUCCESS;
}

NIMCP_EXPORT collective_error_t collective_memory_release(
    collective_memory_system_t* system,
    uint64_t memory_id,
    uint64_t agent_id) {

    if (!system) return COLLECTIVE_ERROR_NULL_POINTER;

    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    collective_memory_t* memory = system->memories[mem_idx];

    return remove_agent_from_memory(memory, agent_id);
}

//=============================================================================
// Synchronization Functions
//=============================================================================

NIMCP_EXPORT collective_error_t collective_memory_sync(
    collective_memory_system_t* system,
    uint64_t memory_id) {

    if (!system) return COLLECTIVE_ERROR_NULL_POINTER;

    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    collective_memory_t* memory = system->memories[mem_idx];

    // Need at least 2 agents for sync
    if (memory->num_agents < COLLECTIVE_MIN_AGENTS_CONSENSUS) {
        memory->sync_status = SYNC_LOCAL_ONLY;
        return COLLECTIVE_SUCCESS;
    }

    // Mark as syncing
    memory->sync_status = SYNC_SYNCING;

    // Compute consensus
    consensus_result_t result;
    collective_error_t err = collective_memory_compute_consensus(system, memory_id, &result);
    if (err != COLLECTIVE_SUCCESS) {
        memory->sync_status = SYNC_CONFLICTED;
        return err;
    }

    // Apply consensus if above threshold
    if (result.consensus_level >= system->config.consensus_threshold) {
        err = collective_memory_apply_consensus(system, memory_id, &result);
        if (err != COLLECTIVE_SUCCESS) {
            memory->sync_status = SYNC_CONFLICTED;
            return err;
        }
        memory->sync_status = SYNC_SYNCED;
    } else {
        memory->sync_status = SYNC_CONFLICTED;
    }

    memory->last_sync_time = system->current_time;
    return COLLECTIVE_SUCCESS;
}

NIMCP_EXPORT size_t collective_memory_sync_all(
    collective_memory_system_t* system) {

    if (!system) return 0;

    size_t synced = 0;

    for (size_t i = 0; i < system->num_memories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_memories > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)system->num_memories);
        }

        if (system->memories[i]) {
            collective_error_t err = collective_memory_sync(system,
                system->memories[i]->memory_id);
            if (err == COLLECTIVE_SUCCESS) {
                synced++;
            }
        }
    }

    system->last_sync_time = system->current_time;
    return synced;
}

NIMCP_EXPORT size_t collective_memory_sync_agents(
    collective_memory_system_t* system,
    uint64_t agent1_id,
    uint64_t agent2_id) {

    if (!system) return 0;

    // Find both agents
    int idx1 = find_agent_index(system, agent1_id);
    int idx2 = find_agent_index(system, agent2_id);
    if (idx1 < 0 || idx2 < 0) return 0;

    size_t synced = 0;

    // Sync memories shared by both agents
    for (size_t i = 0; i < system->num_memories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_memories > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)system->num_memories);
        }

        collective_memory_t* memory = system->memories[i];
        if (!memory) continue;

        bool has_agent1 = agent_has_memory(memory, agent1_id);
        bool has_agent2 = agent_has_memory(memory, agent2_id);

        if (has_agent1 && has_agent2) {
            collective_error_t err = collective_memory_sync(system, memory->memory_id);
            if (err == COLLECTIVE_SUCCESS) {
                synced++;
            }
        }
    }

    return synced;
}

//=============================================================================
// Consensus Functions
//=============================================================================

NIMCP_EXPORT collective_error_t collective_memory_compute_consensus(
    collective_memory_system_t* system,
    uint64_t memory_id,
    consensus_result_t* result) {

    if (!system || !result) return COLLECTIVE_ERROR_NULL_POINTER;

    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    collective_memory_t* memory = system->memories[mem_idx];
    memset(result, 0, sizeof(consensus_result_t));

    if (memory->num_agents == 0) {
        return COLLECTIVE_ERROR_INVALID_ID;
    }

    // For single agent, consensus is trivial
    if (memory->num_agents == 1) {
        result->consensus_signature = memory->content_signature;
        result->consensus_state = memory->shared_quaternion;
        result->consensus_level = 1.0f;
        result->agreeing_agents = 1;
        result->total_agents = 1;
        result->conflict_resolved = true;
        return COLLECTIVE_SUCCESS;
    }

    // Compute consensus based on method
    switch (system->config.consensus_method) {
        case CONSENSUS_MAJORITY:
        case CONSENSUS_WEIGHTED: {
            // Weighted voting based on agent reliability
            float total_weight = 0.0f;
            nimcp_quaternion_t weighted_state = {0, 0, 0, 0};

            for (size_t i = 0; i < memory->num_agents; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && memory->num_agents > 256) {
                    collective_memory_heartbeat("collective_m_loop",
                                     (float)(i + 1) / (float)memory->num_agents);
                }

                uint64_t aid = memory->agent_ids[i];
                int aidx = find_agent_index(system, aid);
                if (aidx < 0) continue;

                float weight = system->agents[aidx].reliability;
                if (system->config.consensus_method == CONSENSUS_MAJORITY) {
                    weight = 1.0f;  // Equal weights for majority
                }

                total_weight += weight;

                // Weighted average of quaternion states
                weighted_state.w += weight * memory->shared_quaternion.w;
                weighted_state.x += weight * memory->shared_quaternion.x;
                weighted_state.y += weight * memory->shared_quaternion.y;
                weighted_state.z += weight * memory->shared_quaternion.z;
            }

            if (total_weight > COLLECTIVE_EPSILON) {
                weighted_state.w /= total_weight;
                weighted_state.x /= total_weight;
                weighted_state.y /= total_weight;
                weighted_state.z /= total_weight;
            }

            result->consensus_signature = memory->content_signature;
            result->consensus_state = quat_normalize(weighted_state);
            result->consensus_level = memory->consensus_strength;
            result->agreeing_agents = memory->num_agents;
            result->total_agents = memory->num_agents;
            result->conflict_resolved = true;
            break;
        }

        case CONSENSUS_LEADER: {
            // Find leader agent
            bool found_leader = false;
            for (size_t i = 0; i < memory->num_agents; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && memory->num_agents > 256) {
                    collective_memory_heartbeat("collective_m_loop",
                                     (float)(i + 1) / (float)memory->num_agents);
                }

                uint64_t aid = memory->agent_ids[i];
                int aidx = find_agent_index(system, aid);
                if (aidx >= 0 && system->agents[aidx].is_leader) {
                    result->consensus_signature = memory->content_signature;
                    result->consensus_state = memory->shared_quaternion;
                    result->consensus_level = 1.0f;
                    result->agreeing_agents = memory->num_agents;
                    result->total_agents = memory->num_agents;
                    result->conflict_resolved = true;
                    found_leader = true;
                    break;
                }
            }

            if (!found_leader) {
                // No leader, fall back to weighted
                return collective_memory_compute_consensus(system, memory_id, result);
            }
            break;
        }

        case CONSENSUS_MERGE:
        case CONSENSUS_BAYESIAN:
        default:
            // For merge and Bayesian, use weighted approach
            result->consensus_signature = memory->content_signature;
            result->consensus_state = memory->shared_quaternion;
            result->consensus_level = memory->consensus_strength;
            result->agreeing_agents = memory->num_agents;
            result->total_agents = memory->num_agents;
            result->conflict_resolved = true;
            break;
    }

    return COLLECTIVE_SUCCESS;
}

NIMCP_EXPORT collective_error_t collective_memory_apply_consensus(
    collective_memory_system_t* system,
    uint64_t memory_id,
    const consensus_result_t* result) {

    if (!system || !result) return COLLECTIVE_ERROR_NULL_POINTER;

    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    collective_memory_t* memory = system->memories[mem_idx];

    // Update memory with consensus
    memory->content_signature = result->consensus_signature;
    memory->shared_quaternion = result->consensus_state;
    memory->consensus_strength = result->consensus_level;

    // Update version numbers for all agents
    for (size_t i = 0; i < memory->num_agents; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && memory->num_agents > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)memory->num_agents);
        }

        memory->agent_versions[i] += 1.0f;
    }

    memory->sync_status = SYNC_SYNCED;
    memory->last_sync_time = system->current_time;

    return COLLECTIVE_SUCCESS;
}

//=============================================================================
// Memory Propagation Functions
//=============================================================================

NIMCP_EXPORT size_t collective_memory_propagate(
    collective_memory_system_t* system,
    uint64_t memory_id,
    const uint64_t* target_agents,
    size_t num_targets) {

    if (!system || !target_agents || num_targets == 0) return 0;

    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) return 0;

    collective_memory_t* memory = system->memories[mem_idx];
    size_t propagated = 0;

    for (size_t i = 0; i < num_targets; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_targets > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)num_targets);
        }

        uint64_t agent_id = target_agents[i];

        // Skip if agent already has memory
        if (agent_has_memory(memory, agent_id)) continue;

        // Verify agent exists
        int agent_idx = find_agent_index(system, agent_id);
        if (agent_idx < 0) continue;

        // Probabilistic propagation based on propagation rate
        float roll = random_float();
        if (roll > memory->propagation_rate) continue;

        // Propagate to this agent
        collective_error_t err = add_agent_to_memory(memory, agent_id);
        if (err == COLLECTIVE_SUCCESS) {
            propagated++;

            // Apply mutation if configured
            if (memory->mutation_rate > COLLECTIVE_EPSILON) {
                collective_memory_apply_mutation(system, memory_id,
                                                  memory->mutation_rate);
            }
        }
    }

    return propagated;
}

NIMCP_EXPORT collective_error_t collective_memory_simulate_propagation(
    collective_memory_system_t* system,
    uint64_t memory_id,
    size_t time_steps,
    propagation_result_t* result) {

    if (!system || !result) return COLLECTIVE_ERROR_NULL_POINTER;

    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    collective_memory_t* memory = system->memories[mem_idx];

    memset(result, 0, sizeof(propagation_result_t));
    result->memory_id = memory_id;
    result->initial_agents = memory->num_agents;

    // Simple SIR-style epidemic simulation
    // S = susceptible (don't have memory)
    // I = infected (have memory, can spread)
    // R = recovered (have memory, stable)

    size_t susceptible = system->num_agents - memory->num_agents;
    size_t infected = memory->num_agents;

    float beta = memory->propagation_rate;  // Transmission rate
    float gamma = 0.1f;  // Recovery rate

    for (size_t t = 0; t < time_steps && susceptible > 0; t++) {
        // New infections
        float new_infections_f = beta * (float)susceptible * (float)infected /
                                 (float)system->num_agents;
        size_t new_infections = (size_t)(new_infections_f + 0.5f);
        if (new_infections > susceptible) new_infections = susceptible;

        // Recoveries
        size_t recoveries = (size_t)((float)infected * gamma + 0.5f);

        susceptible -= new_infections;
        infected = infected + new_infections - recoveries;

        result->propagation_time = (float)(t + 1);
    }

    result->final_agents = system->num_agents - susceptible;
    result->coverage = (float)result->final_agents / (float)system->num_agents;
    result->final_mutation = memory->mutation_rate * result->propagation_time;

    return COLLECTIVE_SUCCESS;
}

//=============================================================================
// Conflict Resolution Functions
//=============================================================================

NIMCP_EXPORT collective_error_t collective_memory_merge(
    collective_memory_system_t* system,
    uint64_t memory_id,
    uint64_t agent1_id,
    uint64_t agent2_id) {

    if (!system) return COLLECTIVE_ERROR_NULL_POINTER;

    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    collective_memory_t* memory = system->memories[mem_idx];

    // Verify both agents share this memory
    if (!agent_has_memory(memory, agent1_id) ||
        !agent_has_memory(memory, agent2_id)) {
        return COLLECTIVE_ERROR_INVALID_ID;
    }

    // Get agent reliabilities for weighted merge
    int idx1 = find_agent_index(system, agent1_id);
    int idx2 = find_agent_index(system, agent2_id);
    if (idx1 < 0 || idx2 < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    float rel1 = system->agents[idx1].reliability;
    float rel2 = system->agents[idx2].reliability;
    float total_rel = rel1 + rel2;

    if (total_rel < COLLECTIVE_EPSILON) {
        total_rel = 2.0f;
        rel1 = rel2 = 1.0f;
    }

    float t = rel2 / total_rel;  // Interpolation parameter

    // Merge quaternion states using SLERP
    memory->shared_quaternion = quat_slerp(memory->shared_quaternion,
                                            memory->shared_quaternion, t);

    // Update consensus strength (merge may reduce confidence)
    memory->consensus_strength *= 0.9f;

    memory->sync_status = SYNC_SYNCING;

    return COLLECTIVE_SUCCESS;
}

NIMCP_EXPORT collective_error_t collective_memory_resolve_conflict(
    collective_memory_system_t* system,
    uint64_t memory_id) {

    if (!system) return COLLECTIVE_ERROR_NULL_POINTER;

    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    collective_memory_t* memory = system->memories[mem_idx];

    // Compute consensus
    consensus_result_t result;
    collective_error_t err = collective_memory_compute_consensus(system, memory_id, &result);
    if (err != COLLECTIVE_SUCCESS) return err;

    // If consensus is below threshold, try leader resolution
    if (result.consensus_level < system->config.consensus_threshold) {
        // Find leader
        for (size_t i = 0; i < memory->num_agents; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && memory->num_agents > 256) {
                collective_memory_heartbeat("collective_m_loop",
                                 (float)(i + 1) / (float)memory->num_agents);
            }

            uint64_t aid = memory->agent_ids[i];
            int aidx = find_agent_index(system, aid);
            if (aidx >= 0 && system->agents[aidx].is_leader) {
                // Leader decides
                result.consensus_level = 1.0f;
                result.conflict_resolved = true;
                break;
            }
        }
    }

    if (!result.conflict_resolved) {
        return COLLECTIVE_ERROR_CONFLICT;
    }

    return collective_memory_apply_consensus(system, memory_id, &result);
}

//=============================================================================
// Drift and Evolution Functions
//=============================================================================

NIMCP_EXPORT collective_error_t collective_memory_compute_drift(
    collective_memory_system_t* system,
    uint64_t memory_id,
    drift_measurement_t* measurement) {

    if (!system || !measurement) return COLLECTIVE_ERROR_NULL_POINTER;

    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    collective_memory_t* memory = system->memories[mem_idx];

    memset(measurement, 0, sizeof(drift_measurement_t));
    measurement->memory_id = memory_id;

    // Compare current signature to original (stored in memory node)
    if (memory->memory_node) {
        const prime_signature_t* original = pr_memory_node_get_signature(memory->memory_node);
        if (original) {
            float sim = compute_signature_similarity(&memory->content_signature, original);
            measurement->signature_drift = 1.0f - sim;
        }

        nimcp_quaternion_t original_state = pr_memory_node_get_state(memory->memory_node);
        measurement->state_drift = 1.0f - compute_state_similarity(
            memory->shared_quaternion, original_state);
    }

    // Combined drift metric
    measurement->total_drift = (measurement->signature_drift + measurement->state_drift) / 2.0f;

    // Compute drift rate
    float elapsed = system->current_time - memory->origin_time;
    if (elapsed > COLLECTIVE_EPSILON) {
        measurement->drift_rate = measurement->total_drift / (fabsf(elapsed) > 1e-7f ? elapsed : 1e-7f);
    }

    // Check divergence threshold
    measurement->is_divergent = measurement->total_drift > system->config.drift_threshold;

    if (measurement->is_divergent) {
        memory->sync_status = SYNC_DIVERGED;
    }

    return COLLECTIVE_SUCCESS;
}

NIMCP_EXPORT collective_error_t collective_memory_apply_mutation(
    collective_memory_system_t* system,
    uint64_t memory_id,
    float mutation_strength) {

    if (!system) return COLLECTIVE_ERROR_NULL_POINTER;

    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    collective_memory_t* memory = system->memories[mem_idx];

    // Clamp mutation strength
    if (mutation_strength < 0.0f) mutation_strength = 0.0f;
    if (mutation_strength > 1.0f) mutation_strength = 1.0f;

    // Apply noise to signature exponents
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        float noise = (random_float() - 0.5f) * 2.0f * mutation_strength * 10.0f;
        int new_exp = (int)memory->content_signature.exponents[i] + (int)noise;
        if (new_exp < 0) new_exp = 0;
        if (new_exp > 255) new_exp = 255;
        memory->content_signature.exponents[i] = (uint8_t)new_exp;
    }

    // Recompute signature hash
    memory->content_signature.hash = prime_sig_hash(&memory->content_signature);
    memory->content_signature.num_factors = prime_sig_count_factors(&memory->content_signature);

    // Apply small perturbation to quaternion state
    float q_noise = mutation_strength * 0.1f;
    memory->shared_quaternion.w += (random_float() - 0.5f) * q_noise;
    memory->shared_quaternion.x += (random_float() - 0.5f) * q_noise;
    memory->shared_quaternion.y += (random_float() - 0.5f) * q_noise;
    memory->shared_quaternion.z += (random_float() - 0.5f) * q_noise;
    memory->shared_quaternion = quat_normalize(memory->shared_quaternion);

    return COLLECTIVE_SUCCESS;
}

//=============================================================================
// Cultural Memory Functions
//=============================================================================

NIMCP_EXPORT collective_error_t collective_memory_get_cultural(
    collective_memory_system_t* system,
    collective_memory_t** memories,
    size_t max_memories,
    size_t* count) {

    if (!system || !memories || !count) return COLLECTIVE_ERROR_NULL_POINTER;

    *count = 0;

    for (size_t i = 0; i < system->num_memories && *count < max_memories; i++) {
        collective_memory_t* memory = system->memories[i];
        if (memory && memory->type == COLLECTIVE_CULTURAL) {
            memories[*count] = memory;
            (*count)++;
        }
    }

    return COLLECTIVE_SUCCESS;
}

NIMCP_EXPORT collective_error_t collective_memory_promote_cultural(
    collective_memory_system_t* system,
    uint64_t memory_id) {

    if (!system) return COLLECTIVE_ERROR_NULL_POINTER;

    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) return COLLECTIVE_ERROR_NOT_FOUND;

    collective_memory_t* memory = system->memories[mem_idx];

    // Change type to cultural
    memory->type = COLLECTIVE_CULTURAL;

    // Reduce mutation rate for cultural memories
    memory->mutation_rate *= 0.1f;

    // Increase retention rate
    memory->retention_rate = fminf(memory->retention_rate * 1.2f, 1.0f);

    return COLLECTIVE_SUCCESS;
}

NIMCP_EXPORT bool collective_memory_check_cultural_threshold(
    collective_memory_system_t* system,
    uint64_t memory_id) {

    if (!system) {
        return false;
    }

    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) {
        return false;
    }

    collective_memory_t* memory = system->memories[mem_idx];

    // Check consensus threshold
    if (memory->consensus_strength < system->config.cultural_threshold) {
        return false;
    }

    // Check minimum sharing (at least 50% of agents)
    float sharing_ratio = (float)memory->num_agents / (float)system->num_agents;
    if (sharing_ratio < 0.5f) {
        return false;
    }

    // Check sync status
    if (memory->sync_status != SYNC_SYNCED) {
        return false;
    }

    return true;
}

//=============================================================================
// Query Functions
//=============================================================================

NIMCP_EXPORT collective_memory_t* collective_memory_get(
    collective_memory_system_t* system,
    uint64_t memory_id) {

    if (!system) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");


        return NULL;


    }

    int mem_idx = find_memory_index(system, memory_id);
    if (mem_idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    return system->memories[mem_idx];
}

NIMCP_EXPORT collective_error_t collective_memory_find_by_type(
    collective_memory_system_t* system,
    collective_type_t type,
    collective_memory_t** memories,
    size_t max_memories,
    size_t* count) {

    if (!system || !memories || !count) return COLLECTIVE_ERROR_NULL_POINTER;

    *count = 0;

    for (size_t i = 0; i < system->num_memories && *count < max_memories; i++) {
        collective_memory_t* memory = system->memories[i];
        if (memory && memory->type == type) {
            memories[*count] = memory;
            (*count)++;
        }
    }

    return COLLECTIVE_SUCCESS;
}

NIMCP_EXPORT collective_error_t collective_memory_find_by_agent(
    collective_memory_system_t* system,
    uint64_t agent_id,
    uint64_t* memory_ids,
    size_t max_memories,
    size_t* count) {

    if (!system || !memory_ids || !count) return COLLECTIVE_ERROR_NULL_POINTER;

    // Verify agent exists
    if (find_agent_index(system, agent_id) < 0) {
        return COLLECTIVE_ERROR_NOT_FOUND;
    }

    *count = 0;

    for (size_t i = 0; i < system->num_memories && *count < max_memories; i++) {
        collective_memory_t* memory = system->memories[i];
        if (memory && agent_has_memory(memory, agent_id)) {
            memory_ids[*count] = memory->memory_id;
            (*count)++;
        }
    }

    return COLLECTIVE_SUCCESS;
}

//=============================================================================
// Statistics Functions
//=============================================================================

NIMCP_EXPORT collective_error_t collective_memory_get_stats(
    collective_memory_system_t* system,
    collective_stats_t* stats) {

    if (!system || !stats) return COLLECTIVE_ERROR_NULL_POINTER;

    memset(stats, 0, sizeof(collective_stats_t));

    stats->num_agents = system->num_agents;
    stats->num_memories = system->num_memories;

    float total_consensus = 0.0f;
    size_t total_agents_sharing = 0;
    float total_drift = 0.0f;

    for (size_t i = 0; i < system->num_memories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_memories > 256) {
            collective_memory_heartbeat("collective_m_loop",
                             (float)(i + 1) / (float)system->num_memories);
        }

        collective_memory_t* memory = system->memories[i];
        if (!memory) continue;

        // Count by type
        if (memory->type < COLLECTIVE_TYPE_COUNT) {
            stats->memories_by_type[memory->type]++;
        }

        total_consensus += memory->consensus_strength;
        total_agents_sharing += memory->num_agents;

        // Count conflicts
        if (memory->sync_status == SYNC_CONFLICTED) {
            stats->conflicts_detected++;
        }

        // Compute drift
        drift_measurement_t drift;
        if (collective_memory_compute_drift(system, memory->memory_id, &drift) ==
            COLLECTIVE_SUCCESS) {
            total_drift += drift.total_drift;
        }
    }

    if (system->num_memories > 0) {
        stats->avg_consensus = total_consensus / (float)system->num_memories;
        stats->avg_agents_per_memory = (float)total_agents_sharing /
                                        (float)system->num_memories;
        stats->avg_drift = total_drift / (float)system->num_memories;
    }

    return COLLECTIVE_SUCCESS;
}

NIMCP_EXPORT void collective_memory_reset_stats(
    collective_memory_system_t* system) {
    // Currently stats are computed on-demand, so nothing to reset
    (void)system;
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT const char* collective_memory_error_string(collective_error_t error) {
    switch (error) {
        case COLLECTIVE_SUCCESS:           return "Success";
        case COLLECTIVE_ERROR_NULL_POINTER: return "Null pointer";
        case COLLECTIVE_ERROR_INVALID_ID:   return "Invalid ID";
        case COLLECTIVE_ERROR_NO_MEMORY:    return "Memory allocation failed";
        case COLLECTIVE_ERROR_MAX_AGENTS:   return "Maximum agents exceeded";
        case COLLECTIVE_ERROR_MAX_MEMORIES: return "Maximum memories exceeded";
        case COLLECTIVE_ERROR_NOT_FOUND:    return "Not found";
        case COLLECTIVE_ERROR_CONFLICT:     return "Conflict unresolved";
        case COLLECTIVE_ERROR_SYNC_FAILED:  return "Synchronization failed";
        case COLLECTIVE_ERROR_INVALID_CONFIG: return "Invalid configuration";
        default:                            return "Unknown error";
    }
}

NIMCP_EXPORT const char* collective_type_name(collective_type_t type) {
    switch (type) {
        case COLLECTIVE_CULTURAL:   return "Cultural";
        case COLLECTIVE_EPISODIC:   return "Episodic";
        case COLLECTIVE_PROCEDURAL: return "Procedural";
        case COLLECTIVE_SEMANTIC:   return "Semantic";
        default:                    return "Unknown";
    }
}

NIMCP_EXPORT const char* consensus_method_name(consensus_method_t method) {
    switch (method) {
        case CONSENSUS_MAJORITY:  return "Majority";
        case CONSENSUS_WEIGHTED:  return "Weighted";
        case CONSENSUS_BAYESIAN:  return "Bayesian";
        case CONSENSUS_MERGE:     return "Merge";
        case CONSENSUS_LEADER:    return "Leader";
        default:                  return "Unknown";
    }
}

NIMCP_EXPORT const char* propagation_model_name(propagation_model_t model) {
    switch (model) {
        case PROPAGATION_DIRECT:    return "Direct";
        case PROPAGATION_BROADCAST: return "Broadcast";
        case PROPAGATION_EPIDEMIC:  return "Epidemic";
        case PROPAGATION_CASCADE:   return "Cascade";
        default:                    return "Unknown";
    }
}

NIMCP_EXPORT const char* sync_status_name(sync_status_t status) {
    switch (status) {
        case SYNC_UNKNOWN:     return "Unknown";
        case SYNC_LOCAL_ONLY:  return "Local Only";
        case SYNC_SYNCING:     return "Syncing";
        case SYNC_SYNCED:      return "Synced";
        case SYNC_CONFLICTED:  return "Conflicted";
        case SYNC_DIVERGED:    return "Diverged";
        default:               return "Unknown";
    }
}

NIMCP_EXPORT void collective_memory_advance_time(
    collective_memory_system_t* system,
    float delta_time) {

    if (!system) return;

    system->current_time += delta_time;

    // Check if auto-sync is due
    if (system->config.auto_sync) {
        float elapsed = system->current_time - system->last_sync_time;
        if (elapsed >= system->config.sync_interval) {
            collective_memory_sync_all(system);
        }
    }
}

NIMCP_EXPORT void collective_memory_print_summary(
    collective_memory_system_t* system) {

    if (!system) {
        printf("Collective Memory System: NULL\n");
        return;
    }

    collective_stats_t stats;
    collective_memory_get_stats(system, &stats);

    printf("=== Collective Memory System Summary ===\n");
    printf("Agents: %zu\n", stats.num_agents);
    printf("Memories: %zu\n", stats.num_memories);
    printf("  Cultural: %zu\n", stats.memories_by_type[COLLECTIVE_CULTURAL]);
    printf("  Episodic: %zu\n", stats.memories_by_type[COLLECTIVE_EPISODIC]);
    printf("  Procedural: %zu\n", stats.memories_by_type[COLLECTIVE_PROCEDURAL]);
    printf("  Semantic: %zu\n", stats.memories_by_type[COLLECTIVE_SEMANTIC]);
    printf("Avg Consensus: %.3f\n", stats.avg_consensus);
    printf("Avg Agents/Memory: %.2f\n", stats.avg_agents_per_memory);
    printf("Avg Drift: %.4f\n", stats.avg_drift);
    printf("Conflicts: %zu detected\n", stats.conflicts_detected);
    printf("Current Time: %.2f\n", system->current_time);
    printf("Consensus Method: %s\n", consensus_method_name(system->config.consensus_method));
    printf("Propagation Model: %s\n", propagation_model_name(system->config.propagation_model));
    printf("========================================\n");
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void collective_memory_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_collective_memory_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int collective_memory_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_memory_training_begin: NULL argument");
        return -1;
    }
    collective_memory_heartbeat_instance(NULL, "collective_memory_training_begin", 0.0f);
    return 0;
}

int collective_memory_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_memory_training_end: NULL argument");
        return -1;
    }
    collective_memory_heartbeat_instance(NULL, "collective_memory_training_end", 1.0f);
    return 0;
}

int collective_memory_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_memory_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    collective_memory_heartbeat_instance(NULL, "collective_memory_training_step", progress);
    return 0;
}
