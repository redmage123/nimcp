//=============================================================================
// nimcp_pr_kg_bridge.c - KG Bridge Implementation for Prime Resonant Memory
//=============================================================================
/**
 * @file nimcp_pr_kg_bridge.c
 * @brief Implementation of PR Memory <-> Brain KG bidirectional bridge
 *
 * WHAT: Bidirectional bridge connecting PR Memory nodes to Brain Knowledge Graph
 * WHY:  Enable self-aware memory system by integrating episodic/semantic memories
 *       with the brain's internal knowledge representation
 * HOW:  Maintains mapping table, syncs nodes/edges, provides query interfaces
 *
 * INTERNAL ARCHITECTURE:
 *
 *   Bridge Internal Structure:
 *   +-----------------------------------------------------------------------+
 *   |  pr_kg_bridge_struct                                                  |
 *   |-----------------------------------------------------------------------|
 *   |  config: pr_kg_bridge_config_t                                        |
 *   |  brain_kg: brain_kg_t* -----> [External Brain KG]                     |
 *   |                                                                        |
 *   |  Mapping Table (Hash-based):                                          |
 *   |  +----------------------------+                                        |
 *   |  | pr_to_kg_hash[PR_ID]       | -> pr_kg_mapping_t                    |
 *   |  +----------------------------+                                        |
 *   |  | kg_to_pr_hash[KG_ID]       | -> index into mappings array          |
 *   |  +----------------------------+                                        |
 *   |                                                                        |
 *   |  mappings[]: pr_kg_mapping_t array                                     |
 *   |  mapping_count: active mappings                                        |
 *   |                                                                        |
 *   |  stats: pr_kg_bridge_stats_t                                          |
 *   |  mutex: thread safety                                                 |
 *   +-----------------------------------------------------------------------+
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/memory/core/nimcp_pr_kg_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pr_kg_bridge, MESH_ADAPTER_CATEGORY_MEMORY)


/** @brief Send heartbeat from pr_kg_bridge module (instance-level) */
static inline void pr_kg_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_kg_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_kg_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_kg_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_KG_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Platform-specific includes
//=============================================================================

#ifdef _WIN32
    #include <windows.h>
    #define MUTEX_T CRITICAL_SECTION
    #define MUTEX_INIT(m) InitializeCriticalSection(&(m))
    #define MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
    #define MUTEX_LOCK(m) EnterCriticalSection(&(m))
    #define MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))
#else
    #include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
    #define MUTEX_T nimcp_mutex_t*
    #define MUTEX_INIT(m) nimcp_mutex_init((m), NULL)
    #define MUTEX_DESTROY(m) nimcp_mutex_destroy((m))
    #define MUTEX_LOCK(m) nimcp_mutex_lock((m))
    #define MUTEX_UNLOCK(m) nimcp_mutex_unlock((m))
#endif

//=============================================================================
// Constants
//=============================================================================

/** Hash table size (prime for better distribution) */
#define HASH_TABLE_SIZE 65537

/** Invalid hash index sentinel */
#define HASH_INVALID UINT32_MAX

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * @brief Hash bucket entry for PR->KG lookup
 */
typedef struct pr_hash_entry {
    uint64_t pr_node_id;
    uint32_t mapping_index;
    struct pr_hash_entry* next;
} pr_hash_entry_t;

/**
 * @brief Hash bucket entry for KG->PR lookup
 */
typedef struct kg_hash_entry {
    brain_kg_node_id_t kg_node_id;
    uint32_t mapping_index;
    struct kg_hash_entry* next;
} kg_hash_entry_t;

/**
 * @brief Internal bridge structure
 */
struct pr_kg_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    pr_kg_bridge_config_t config;

    /* Connected brain KG */
    brain_kg_t* brain_kg;

    /* Mapping storage */
    pr_kg_mapping_t* mappings;
    uint32_t mapping_count;
    uint32_t mapping_capacity;

    /* Hash tables for fast lookup */
    pr_hash_entry_t** pr_to_kg_hash;    /* PR ID -> mapping index */
    kg_hash_entry_t** kg_to_pr_hash;    /* KG ID -> mapping index */

    /* Statistics */
    pr_kg_bridge_stats_t stats;

    /* Thread safety */
    MUTEX_T mutex;

    /* State flags */
    bool initialized;
    bool connected;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_kg_bridge, struct pr_kg_bridge_struct)

//=============================================================================
// Static Variables
//=============================================================================

/** Thread-local error message buffer */
static _Thread_local char s_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Set the last error message
 */
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(s_last_error, sizeof(s_last_error), format, args);
    va_end(args);
}

/**
 * @brief Clear the last error message
 */
static void clear_error(void) {
    s_last_error[0] = '\0';
}

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
#ifdef _WIN32
    /* Windows implementation */
    LARGE_INTEGER count, freq;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&freq);
    return (uint64_t)(count.QuadPart * 1000 / freq.QuadPart);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

/**
 * @brief Hash function for 64-bit PR node IDs
 */
static uint32_t hash_pr_id(uint64_t pr_id) {
    /* FNV-1a hash variant for 64-bit */
    uint64_t hash = 14695981039346656037ULL;
    hash ^= pr_id;
    hash *= 1099511628211ULL;
    hash ^= (pr_id >> 32);
    hash *= 1099511628211ULL;
    return (uint32_t)(hash % HASH_TABLE_SIZE);
}

/**
 * @brief Hash function for 32-bit KG node IDs
 */
static uint32_t hash_kg_id(brain_kg_node_id_t kg_id) {
    /* Simple multiplication hash for 32-bit */
    uint32_t hash = kg_id;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = (hash >> 16) ^ hash;
    return hash % HASH_TABLE_SIZE;
}

/**
 * @brief Insert entry into PR->KG hash table
 */
static int hash_insert_pr(struct pr_kg_bridge_struct* bridge,
                          uint64_t pr_id, uint32_t mapping_idx) {
    uint32_t bucket = hash_pr_id(pr_id);

    pr_hash_entry_t* entry = (pr_hash_entry_t*)nimcp_malloc(sizeof(pr_hash_entry_t));
    if (!entry) {
        set_error("Failed to allocate PR hash entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hash_kg_id: entry is NULL");
        return -1;
    }

    entry->pr_node_id = pr_id;
    entry->mapping_index = mapping_idx;
    entry->next = bridge->pr_to_kg_hash[bucket];
    bridge->pr_to_kg_hash[bucket] = entry;

    return 0;
}

/**
 * @brief Insert entry into KG->PR hash table
 */
static int hash_insert_kg(struct pr_kg_bridge_struct* bridge,
                          brain_kg_node_id_t kg_id, uint32_t mapping_idx) {
    uint32_t bucket = hash_kg_id(kg_id);

    kg_hash_entry_t* entry = (kg_hash_entry_t*)nimcp_malloc(sizeof(kg_hash_entry_t));
    if (!entry) {
        set_error("Failed to allocate KG hash entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hash_kg_id: entry is NULL");
        return -1;
    }

    entry->kg_node_id = kg_id;
    entry->mapping_index = mapping_idx;
    entry->next = bridge->kg_to_pr_hash[bucket];
    bridge->kg_to_pr_hash[bucket] = entry;

    return 0;
}

/**
 * @brief Find mapping index by PR node ID
 */
static uint32_t hash_find_pr(const struct pr_kg_bridge_struct* bridge,
                             uint64_t pr_id) {
    uint32_t bucket = hash_pr_id(pr_id);
    pr_hash_entry_t* entry = bridge->pr_to_kg_hash[bucket];

    while (entry) {
        if (entry->pr_node_id == pr_id) {
            return entry->mapping_index;
        }
        entry = entry->next;
    }

    return HASH_INVALID;
}

/**
 * @brief Find mapping index by KG node ID
 */
static uint32_t hash_find_kg(const struct pr_kg_bridge_struct* bridge,
                             brain_kg_node_id_t kg_id) {
    uint32_t bucket = hash_kg_id(kg_id);
    kg_hash_entry_t* entry = bridge->kg_to_pr_hash[bucket];

    while (entry) {
        if (entry->kg_node_id == kg_id) {
            return entry->mapping_index;
        }
        entry = entry->next;
    }

    return HASH_INVALID;
}

/**
 * @brief Remove entry from PR->KG hash table
 */
static int hash_remove_pr(struct pr_kg_bridge_struct* bridge, uint64_t pr_id) {
    uint32_t bucket = hash_pr_id(pr_id);
    pr_hash_entry_t** pp = &bridge->pr_to_kg_hash[bucket];

    while (*pp) {
        if ((*pp)->pr_node_id == pr_id) {
            pr_hash_entry_t* to_free = *pp;
            *pp = (*pp)->next;
            nimcp_free(to_free);
            to_free = NULL;
            return 0;
        }
        pp = &(*pp)->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hash_remove_pr: validation failed");
    return -1;
}

/**
 * @brief Remove entry from KG->PR hash table
 */
static int hash_remove_kg(struct pr_kg_bridge_struct* bridge,
                          brain_kg_node_id_t kg_id) {
    uint32_t bucket = hash_kg_id(kg_id);
    kg_hash_entry_t** pp = &bridge->kg_to_pr_hash[bucket];

    while (*pp) {
        if ((*pp)->kg_node_id == kg_id) {
            kg_hash_entry_t* to_free = *pp;
            *pp = (*pp)->next;
            nimcp_free(to_free);
            to_free = NULL;
            return 0;
        }
        pp = &(*pp)->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hash_remove_pr: validation failed");
    return -1;
}

/**
 * @brief Free all hash table entries
 */
static void hash_clear_all(struct pr_kg_bridge_struct* bridge) {
    /* Clear PR hash table */
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && HASH_TABLE_SIZE > 256) {
            pr_kg_bridge_heartbeat("pr_kg_bridge_loop",
                             (float)(i + 1) / (float)HASH_TABLE_SIZE);
        }

        pr_hash_entry_t* entry = bridge->pr_to_kg_hash[i];
        while (entry) {
            pr_hash_entry_t* next = entry->next;
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
        bridge->pr_to_kg_hash[i] = NULL;
    }

    /* Clear KG hash table */
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && HASH_TABLE_SIZE > 256) {
            pr_kg_bridge_heartbeat("pr_kg_bridge_loop",
                             (float)(i + 1) / (float)HASH_TABLE_SIZE);
        }

        kg_hash_entry_t* entry = bridge->kg_to_pr_hash[i];
        while (entry) {
            kg_hash_entry_t* next = entry->next;
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
        bridge->kg_to_pr_hash[i] = NULL;
    }
}

/**
 * @brief Map entanglement edge type to KG edge type
 */
static brain_kg_edge_type_t map_edge_type(pr_kg_entangle_edge_type_t etype) {
    switch (etype) {
        case ENTANGLE_EDGE_SEMANTIC:
            return BRAIN_KG_EDGE_CONNECTS_TO;
        case ENTANGLE_EDGE_CAUSAL:
            return BRAIN_KG_EDGE_SENDS_TO;
        case ENTANGLE_EDGE_ASSOCIATIVE:
            return BRAIN_KG_EDGE_MODULATES;
        case ENTANGLE_EDGE_EMOTIONAL:
            return BRAIN_KG_EDGE_MODULATES;
        case ENTANGLE_EDGE_TEMPORAL:
            return BRAIN_KG_EDGE_COORDINATES_WITH;
        case ENTANGLE_EDGE_CONTEXTUAL:
            return BRAIN_KG_EDGE_INTEGRATES_WITH;
        default:
            return BRAIN_KG_EDGE_CONNECTS_TO;
    }
}

/**
 * @brief Allocate and add a new mapping slot
 */
static uint32_t allocate_mapping(struct pr_kg_bridge_struct* bridge) {
    /* Check capacity */
    if (bridge->mapping_count >= bridge->mapping_capacity) {
        /* Expand mapping array */
        uint32_t new_capacity = bridge->mapping_capacity * 2;
        if (new_capacity > PR_KG_MAX_MAPPINGS) {
            new_capacity = PR_KG_MAX_MAPPINGS;
        }
        if (new_capacity <= bridge->mapping_capacity) {
            set_error("Maximum mapping capacity reached");
            return HASH_INVALID;
        }

        pr_kg_mapping_t* new_mappings = (pr_kg_mapping_t*)nimcp_realloc(
            bridge->mappings, new_capacity * sizeof(pr_kg_mapping_t));
        if (!new_mappings) {
            set_error("Failed to expand mapping array");
            return HASH_INVALID;
        }

        /* Zero new entries */
        memset(&new_mappings[bridge->mapping_capacity], 0,
               (new_capacity - bridge->mapping_capacity) * sizeof(pr_kg_mapping_t));

        bridge->mappings = new_mappings;
        bridge->mapping_capacity = new_capacity;
    }

    /* Find first free slot or use next index */
    for (uint32_t i = 0; i < bridge->mapping_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->mapping_capacity > 256) {
            pr_kg_bridge_heartbeat("pr_kg_bridge_loop",
                             (float)(i + 1) / (float)bridge->mapping_capacity);
        }

        if (bridge->mappings[i].state == PR_KG_MAPPING_STATE_INVALID) {
            return i;
        }
    }

    return bridge->mapping_count++;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pr_kg_bridge_config_t pr_kg_bridge_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_config_default", 0.0f);


    pr_kg_bridge_config_t config = {
        .brain_kg = NULL,
        .auto_register_memories = true,
        .sync_on_update = true,
        .sync_entanglements = true,
        .edge_weight_scale = PR_KG_DEFAULT_WEIGHT_SCALE,
        .stale_threshold_ms = PR_KG_STALE_THRESHOLD_MS,
        .enable_statistics = true
    };
    return config;
}

pr_kg_bridge_t pr_kg_bridge_create(const pr_kg_bridge_config_t* config) {
    if (!config) {
        set_error("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_kg_bridge_create: config is NULL");
        return NULL;
    }

    if (!config->brain_kg) {
        set_error("brain_kg must be non-NULL in config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_bridge_create: config->brain_kg is NULL");
        return NULL;
    }

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_create", 0.0f);


    struct pr_kg_bridge_struct* bridge =
        (struct pr_kg_bridge_struct*)nimcp_calloc(1, sizeof(struct pr_kg_bridge_struct));
    if (!bridge) {
        set_error("Failed to allocate bridge structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_kg_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Copy configuration */
    bridge->config = *config;
    bridge->brain_kg = config->brain_kg;

    /* Initialize mapping array */
    bridge->mapping_capacity = 1024;  /* Initial capacity */
    bridge->mappings = (pr_kg_mapping_t*)nimcp_calloc(
        bridge->mapping_capacity, sizeof(pr_kg_mapping_t));
    if (!bridge->mappings) {
        set_error("Failed to allocate mapping array");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_kg_bridge_create: bridge->mappings is NULL");
        return NULL;
    }

    /* Initialize hash tables */
    bridge->pr_to_kg_hash = (pr_hash_entry_t**)nimcp_calloc(
        HASH_TABLE_SIZE, sizeof(pr_hash_entry_t*));
    bridge->kg_to_pr_hash = (kg_hash_entry_t**)nimcp_calloc(
        HASH_TABLE_SIZE, sizeof(kg_hash_entry_t*));

    if (!bridge->pr_to_kg_hash || !bridge->kg_to_pr_hash) {
        set_error("Failed to allocate hash tables");
        nimcp_free(bridge->mappings);
        nimcp_free(bridge->pr_to_kg_hash);
        nimcp_free(bridge->kg_to_pr_hash);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_kg_bridge_create: required parameter is NULL (bridge->pr_to_kg_hash, bridge->kg_to_pr_hash)");
        return NULL;
    }

    /* Initialize mutex */
    MUTEX_INIT(bridge->base.mutex);

    /* Set state flags */
    bridge->initialized = true;
    bridge->connected = true;
    bridge->mapping_count = 0;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    clear_error();
    NIMCP_LOGGING_INFO("Created %s bridge", "pr_kg");
    return bridge;
}

void pr_kg_bridge_destroy(pr_kg_bridge_t bridge) {
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_kg");
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_destroy", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    /* Clear hash tables */
    hash_clear_all(bridge);

    /* Free hash table arrays */
    nimcp_free(bridge->pr_to_kg_hash);
    nimcp_free(bridge->kg_to_pr_hash);

    /* Free mappings array */
    nimcp_free(bridge->mappings);

    bridge->initialized = false;
    bridge->connected = false;

    MUTEX_UNLOCK(bridge->base.mutex);
    MUTEX_DESTROY(bridge->base.mutex);

    nimcp_free(bridge);
    bridge = NULL;
}

int pr_kg_bridge_connect(pr_kg_bridge_t bridge, brain_kg_t* brain_kg) {
    if (!bridge) {
        set_error("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_bridge_connect: bridge is NULL");
        return -1;
    }

    if (!brain_kg) {
        set_error("NULL brain_kg pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_bridge_connect: brain_kg is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_connect", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    /* Clear existing mappings */
    hash_clear_all(bridge);

    /* Reset mapping array */
    memset(bridge->mappings, 0,
           bridge->mapping_capacity * sizeof(pr_kg_mapping_t));
    bridge->mapping_count = 0;

    /* Update KG pointer */
    bridge->brain_kg = brain_kg;
    bridge->config.brain_kg = brain_kg;
    bridge->connected = true;

    /* Update stats */
    bridge->stats.active_mappings = 0;

    MUTEX_UNLOCK(bridge->base.mutex);

    clear_error();
    return 0;
}

bool pr_kg_bridge_is_connected(const pr_kg_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_is_connected", 0.0f);


    return bridge->initialized && bridge->connected && bridge->brain_kg != NULL;
}

//=============================================================================
// Memory -> KG Sync Functions
//=============================================================================

brain_kg_node_id_t pr_kg_register_memory(
    pr_kg_bridge_t bridge,
    uint64_t pr_node_id,
    const char* content_desc,
    uint32_t module_id)
{
    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_register_memor", 0.0f);


    return pr_kg_register_memory_full(bridge, pr_node_id, content_desc,
                                      module_id, NULL, 0.5f);
}

brain_kg_node_id_t pr_kg_register_memory_full(
    pr_kg_bridge_t bridge,
    uint64_t pr_node_id,
    const char* content_desc,
    uint32_t module_id,
    const prime_signature_t* signature,
    float importance)
{
    if (!bridge || !bridge->connected) {
        set_error("Bridge not connected");
        return BRAIN_KG_INVALID_NODE;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_register_memor", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    /* Check if already registered */
    uint32_t existing = hash_find_pr(bridge, pr_node_id);
    if (existing != HASH_INVALID) {
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("PR node %lu already registered", (unsigned long)pr_node_id);
        return bridge->mappings[existing].kg_node_id;
    }

    /* Generate KG node name */
    char node_name[BRAIN_KG_MAX_NAME_LEN];
    snprintf(node_name, sizeof(node_name), "pr_memory_%lu",
             (unsigned long)pr_node_id);

    /* Generate description */
    char description[BRAIN_KG_MAX_DESC_LEN];
    if (content_desc && content_desc[0]) {
        snprintf(description, sizeof(description), "%s", content_desc);
    } else {
        snprintf(description, sizeof(description),
                 "PR Memory Node %lu (module %u)",
                 (unsigned long)pr_node_id, module_id);
    }

    /* Create KG node */
    brain_kg_node_id_t kg_id = brain_kg_add_node(
        bridge->brain_kg,
        node_name,
        BRAIN_KG_NODE_COGNITIVE,
        description
    );

    if (kg_id == BRAIN_KG_INVALID_NODE) {
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("Failed to create KG node for PR memory %lu",
                  (unsigned long)pr_node_id);
        return BRAIN_KG_INVALID_NODE;
    }

    /* Add metadata to KG node */
    char meta_value[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(meta_value, sizeof(meta_value), "%lu", (unsigned long)pr_node_id);
    brain_kg_add_metadata(bridge->brain_kg, kg_id, "pr_node_id", meta_value);

    snprintf(meta_value, sizeof(meta_value), "%u", module_id);
    brain_kg_add_metadata(bridge->brain_kg, kg_id, "module_id", meta_value);

    snprintf(meta_value, sizeof(meta_value), "%.4f", importance);
    brain_kg_add_metadata(bridge->brain_kg, kg_id, "importance", meta_value);

    /* Add signature hash if provided */
    if (signature) {
        snprintf(meta_value, sizeof(meta_value), "0x%016lx",
                 (unsigned long)signature->hash);
        brain_kg_add_metadata(bridge->brain_kg, kg_id, "sig_hash", meta_value);

        snprintf(meta_value, sizeof(meta_value), "%u", signature->num_factors);
        brain_kg_add_metadata(bridge->brain_kg, kg_id, "sig_factors", meta_value);
    }

    /* Allocate mapping slot */
    uint32_t map_idx = allocate_mapping(bridge);
    if (map_idx == HASH_INVALID) {
        /* Failed to allocate, remove KG node */
        brain_kg_remove_node(bridge->brain_kg, kg_id);
        MUTEX_UNLOCK(bridge->base.mutex);
        return BRAIN_KG_INVALID_NODE;
    }

    /* Set up mapping */
    uint64_t now = get_time_ms();
    bridge->mappings[map_idx].pr_node_id = pr_node_id;
    bridge->mappings[map_idx].kg_node_id = kg_id;
    bridge->mappings[map_idx].created_time_ms = now;
    bridge->mappings[map_idx].sync_time_ms = now;
    bridge->mappings[map_idx].state = PR_KG_MAPPING_STATE_SYNCED;
    bridge->mappings[map_idx].sync_count = 1;
    bridge->mappings[map_idx].module_id = module_id;

    /* Add to hash tables */
    if (hash_insert_pr(bridge, pr_node_id, map_idx) < 0 ||
        hash_insert_kg(bridge, kg_id, map_idx) < 0) {
        /* Hash insertion failed, cleanup */
        bridge->mappings[map_idx].state = PR_KG_MAPPING_STATE_INVALID;
        brain_kg_remove_node(bridge->brain_kg, kg_id);
        MUTEX_UNLOCK(bridge->base.mutex);
        return BRAIN_KG_INVALID_NODE;
    }

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        bridge->stats.total_registrations++;
        bridge->stats.active_mappings++;
        bridge->stats.last_sync_time_ms = now;
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return kg_id;
}

int pr_kg_unregister_memory(pr_kg_bridge_t bridge, uint64_t pr_node_id) {
    if (!bridge || !bridge->connected) {
        set_error("Bridge not connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_unregister_memory: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_unregister_mem", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    /* Find mapping */
    uint32_t map_idx = hash_find_pr(bridge, pr_node_id);
    if (map_idx == HASH_INVALID) {
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("PR node %lu not found", (unsigned long)pr_node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_kg_unregister_memory: validation failed");
        return -1;
    }

    pr_kg_mapping_t* mapping = &bridge->mappings[map_idx];
    brain_kg_node_id_t kg_id = mapping->kg_node_id;

    /* Remove KG node (this also removes connected edges) */
    if (kg_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_remove_node(bridge->brain_kg, kg_id);
    }

    /* Remove from hash tables */
    hash_remove_pr(bridge, pr_node_id);
    hash_remove_kg(bridge, kg_id);

    /* Clear mapping slot */
    memset(mapping, 0, sizeof(*mapping));
    mapping->state = PR_KG_MAPPING_STATE_INVALID;

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        bridge->stats.total_unregistrations++;
        if (bridge->stats.active_mappings > 0) {
            bridge->stats.active_mappings--;
        }
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return 0;
}

int pr_kg_sync_memory(
    pr_kg_bridge_t bridge,
    uint64_t pr_node_id,
    const char* new_desc,
    brain_kg_node_state_t new_state)
{
    if (!bridge || !bridge->connected) {
        set_error("Bridge not connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_sync_memory: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_sync_memory", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    /* Find mapping */
    uint32_t map_idx = hash_find_pr(bridge, pr_node_id);
    if (map_idx == HASH_INVALID) {
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("PR node %lu not found", (unsigned long)pr_node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_kg_sync_memory: validation failed");
        return -1;
    }

    pr_kg_mapping_t* mapping = &bridge->mappings[map_idx];

    /* Update KG node */
    int result = brain_kg_update_node(
        bridge->brain_kg,
        mapping->kg_node_id,
        new_desc,
        new_state
    );

    if (result < 0) {
        mapping->state = PR_KG_MAPPING_STATE_ERROR;
        bridge->stats.error_count++;
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("Failed to update KG node for PR memory %lu",
                  (unsigned long)pr_node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_kg_sync_memory: validation failed");
        return -1;
    }

    /* Update mapping */
    uint64_t now = get_time_ms();
    mapping->sync_time_ms = now;
    mapping->sync_count++;
    mapping->state = PR_KG_MAPPING_STATE_SYNCED;

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        bridge->stats.total_syncs++;
        bridge->stats.last_sync_time_ms = now;
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return 0;
}

brain_kg_edge_id_t pr_kg_sync_entanglement(
    pr_kg_bridge_t bridge,
    uint64_t from_pr_id,
    uint64_t to_pr_id,
    pr_kg_entangle_edge_type_t edge_type,
    float resonance_strength)
{
    if (!bridge || !bridge->connected) {
        set_error("Bridge not connected");
        return BRAIN_KG_INVALID_NODE;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_sync_entanglem", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    /* Find mappings for both nodes */
    uint32_t from_idx = hash_find_pr(bridge, from_pr_id);
    uint32_t to_idx = hash_find_pr(bridge, to_pr_id);

    if (from_idx == HASH_INVALID || to_idx == HASH_INVALID) {
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("One or both PR nodes not registered (from=%lu, to=%lu)",
                  (unsigned long)from_pr_id, (unsigned long)to_pr_id);
        return BRAIN_KG_INVALID_NODE;
    }

    brain_kg_node_id_t from_kg = bridge->mappings[from_idx].kg_node_id;
    brain_kg_node_id_t to_kg = bridge->mappings[to_idx].kg_node_id;

    /* Map edge type */
    brain_kg_edge_type_t kg_edge_type = map_edge_type(edge_type);

    /* Calculate weight with scale factor */
    float weight = resonance_strength * bridge->config.edge_weight_scale;
    if (weight > 1.0f) weight = 1.0f;
    if (weight < 0.0f) weight = 0.0f;

    /* Generate edge description */
    char edge_desc[BRAIN_KG_MAX_DESC_LEN];
    static const char* edge_type_names[] = {
        "semantic", "causal", "associative", "emotional", "temporal", "contextual"
    };
    const char* type_name = (edge_type < ENTANGLE_EDGE_TYPE_COUNT) ?
                            edge_type_names[edge_type] : "unknown";
    snprintf(edge_desc, sizeof(edge_desc),
             "Entanglement: %s (resonance=%.3f)", type_name, resonance_strength);

    /* Check if edge already exists */
    brain_kg_edge_id_t existing = brain_kg_find_edge(bridge->brain_kg,
                                                      from_kg, to_kg);

    if (existing != BRAIN_KG_INVALID_NODE) {
        /* Update existing edge */
        brain_kg_update_edge(bridge->brain_kg, existing, weight, edge_desc);

        /* Update statistics */
        if (bridge->config.enable_statistics) {
            bridge->stats.total_edge_syncs++;
            bridge->stats.last_sync_time_ms = get_time_ms();
        }

        MUTEX_UNLOCK(bridge->base.mutex);
        clear_error();
        return existing;
    }

    /* Create new edge */
    brain_kg_edge_id_t edge_id = brain_kg_add_edge(
        bridge->brain_kg,
        from_kg,
        to_kg,
        kg_edge_type,
        edge_desc,
        weight
    );

    if (edge_id == BRAIN_KG_INVALID_NODE) {
        bridge->stats.error_count++;
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("Failed to create KG edge from %lu to %lu",
                  (unsigned long)from_pr_id, (unsigned long)to_pr_id);
        return BRAIN_KG_INVALID_NODE;
    }

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        bridge->stats.total_edge_syncs++;
        bridge->stats.last_sync_time_ms = get_time_ms();
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return edge_id;
}

int pr_kg_remove_entanglement(
    pr_kg_bridge_t bridge,
    uint64_t from_pr_id,
    uint64_t to_pr_id)
{
    if (!bridge || !bridge->connected) {
        set_error("Bridge not connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_remove_entanglement: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_remove_entangl", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    /* Find mappings */
    uint32_t from_idx = hash_find_pr(bridge, from_pr_id);
    uint32_t to_idx = hash_find_pr(bridge, to_pr_id);

    if (from_idx == HASH_INVALID || to_idx == HASH_INVALID) {
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("One or both PR nodes not registered");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_kg_remove_entanglement: validation failed");
        return -1;
    }

    brain_kg_node_id_t from_kg = bridge->mappings[from_idx].kg_node_id;
    brain_kg_node_id_t to_kg = bridge->mappings[to_idx].kg_node_id;

    /* Find and remove edge */
    brain_kg_edge_id_t edge_id = brain_kg_find_edge(bridge->brain_kg,
                                                     from_kg, to_kg);

    if (edge_id == BRAIN_KG_INVALID_NODE) {
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("Edge not found between PR nodes %lu and %lu",
                  (unsigned long)from_pr_id, (unsigned long)to_pr_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_kg_remove_entanglement: validation failed");
        return -1;
    }

    int result = brain_kg_remove_edge(bridge->brain_kg, edge_id);

    MUTEX_UNLOCK(bridge->base.mutex);

    if (result < 0) {
        set_error("Failed to remove KG edge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_kg_remove_entanglement: validation failed");
        return -1;
    }

    clear_error();
    return 0;
}

//=============================================================================
// KG -> Memory Query Functions
//=============================================================================

int pr_kg_query_by_module(
    pr_kg_bridge_t bridge,
    uint32_t module_id,
    pr_kg_query_result_t* result,
    uint32_t max_results)
{
    if (!bridge || !bridge->connected || !result) {
        set_error("Invalid parameters to query_by_module");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_query_by_module: required parameter is NULL (bridge, bridge->connected, result)");
        return -1;
    }

    if (!result->mappings || result->capacity == 0) {
        set_error("Query result not properly allocated");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_kg_query_by_module: result->mappings is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_query_by_modul", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    result->count = 0;
    uint32_t limit = (max_results < result->capacity) ? max_results : result->capacity;

    /* Scan all mappings for matching module_id */
    for (uint32_t i = 0; i < bridge->mapping_capacity && result->count < limit; i++) {
        if (bridge->mappings[i].state == PR_KG_MAPPING_STATE_SYNCED &&
            bridge->mappings[i].module_id == module_id) {
            result->mappings[result->count++] = bridge->mappings[i];
        }
    }

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        bridge->stats.total_queries++;
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return (int)result->count;
}

int pr_kg_query_by_path(
    pr_kg_bridge_t bridge,
    const brain_kg_path_t* path,
    uint64_t* pr_node_ids,
    uint32_t max_ids)
{
    if (!bridge || !bridge->connected || !path || !pr_node_ids) {
        set_error("Invalid parameters to query_by_path");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_query_by_path: required parameter is NULL (bridge, bridge->connected, path, pr_node_ids)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_query_by_path", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    uint32_t found = 0;

    for (uint32_t i = 0; i < path->length && found < max_ids; i++) {
        uint32_t map_idx = hash_find_kg(bridge, path->nodes[i]);
        if (map_idx != HASH_INVALID) {
            pr_node_ids[found++] = bridge->mappings[map_idx].pr_node_id;
        }
    }

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        bridge->stats.total_queries++;
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return (int)found;
}

int pr_kg_get_context(
    pr_kg_bridge_t bridge,
    brain_kg_node_id_t kg_node_id,
    pr_kg_context_t* context)
{
    if (!bridge || !bridge->connected || !context) {
        set_error("Invalid parameters to get_context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_get_context: required parameter is NULL (bridge, bridge->connected, context)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_get_context", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    /* Get KG node info */
    const brain_kg_node_t* kg_node = brain_kg_get_node(bridge->brain_kg,
                                                        kg_node_id);
    if (!kg_node) {
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("KG node %u not found", kg_node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_get_context: kg_node is NULL");
        return -1;
    }

    /* Initialize context */
    context->kg_node_id = kg_node_id;
    context->node_name = kg_node->name;
    context->node_type = kg_node->type;

    /* Count memories linked to this node */
    brain_kg_edge_list_t* outgoing = brain_kg_get_outgoing(bridge->brain_kg,
                                                            kg_node_id);
    brain_kg_edge_list_t* incoming = brain_kg_get_incoming(bridge->brain_kg,
                                                            kg_node_id);

    context->neighbor_count = 0;
    context->avg_edge_weight = 0.0f;
    float weight_sum = 0.0f;
    int edge_count = 0;

    if (outgoing) {
        context->neighbor_count += outgoing->count;
        for (uint32_t i = 0; i < outgoing->count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && outgoing->count > 256) {
                pr_kg_bridge_heartbeat("pr_kg_bridge_loop",
                                 (float)(i + 1) / (float)outgoing->count);
            }

            weight_sum += outgoing->edges[i]->weight;
            edge_count++;
        }
        brain_kg_edge_list_destroy(outgoing);
    }

    if (incoming) {
        context->neighbor_count += incoming->count;
        for (uint32_t i = 0; i < incoming->count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && incoming->count > 256) {
                pr_kg_bridge_heartbeat("pr_kg_bridge_loop",
                                 (float)(i + 1) / (float)incoming->count);
            }

            weight_sum += incoming->edges[i]->weight;
            edge_count++;
        }
        brain_kg_edge_list_destroy(incoming);
    }

    if (edge_count > 0) {
        context->avg_edge_weight = weight_sum / (float)edge_count;
    }

    /* Find linked memories */
    uint32_t map_idx = hash_find_kg(bridge, kg_node_id);
    if (map_idx != HASH_INVALID) {
        context->memory_count = 1;
        context->memory_ids = (uint64_t*)nimcp_malloc(sizeof(uint64_t));
        if (context->memory_ids) {
            context->memory_ids[0] = bridge->mappings[map_idx].pr_node_id;
        }
    } else {
        context->memory_count = 0;
        context->memory_ids = NULL;
    }

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        bridge->stats.total_queries++;
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return 0;
}

void pr_kg_context_destroy(pr_kg_context_t* context) {
    if (!context) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_context_destro", 0.0f);


    if (context->memory_ids) {
        nimcp_free(context->memory_ids);
        context->memory_ids = NULL;
    }

    context->memory_count = 0;
}

int pr_kg_query_by_kg_node(
    pr_kg_bridge_t bridge,
    brain_kg_node_id_t kg_node_id,
    uint64_t* pr_node_ids,
    uint32_t max_ids)
{
    if (!bridge || !bridge->connected || !pr_node_ids) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_query_by_kg_node: required parameter is NULL (bridge, bridge->connected, pr_node_ids)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_query_by_kg_no", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    uint32_t found = 0;

    /* Get neighbors of the KG node */
    brain_kg_node_list_t* neighbors = brain_kg_get_neighbors(bridge->brain_kg,
                                                              kg_node_id);

    if (neighbors) {
        for (uint32_t i = 0; i < neighbors->count && found < max_ids; i++) {
            uint32_t map_idx = hash_find_kg(bridge, neighbors->nodes[i]->id);
            if (map_idx != HASH_INVALID) {
                pr_node_ids[found++] = bridge->mappings[map_idx].pr_node_id;
            }
        }
        brain_kg_node_list_destroy(neighbors);
    }

    /* Also check if the node itself is a memory */
    uint32_t self_idx = hash_find_kg(bridge, kg_node_id);
    if (self_idx != HASH_INVALID && found < max_ids) {
        /* Check if not already added */
        bool already_added = false;
        uint64_t self_pr = bridge->mappings[self_idx].pr_node_id;
        for (uint32_t i = 0; i < found; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && found > 256) {
                pr_kg_bridge_heartbeat("pr_kg_bridge_loop",
                                 (float)(i + 1) / (float)found);
            }

            if (pr_node_ids[i] == self_pr) {
                already_added = true;
                break;
            }
        }
        if (!already_added) {
            pr_node_ids[found++] = self_pr;
        }
    }

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        bridge->stats.total_queries++;
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return (int)found;
}

//=============================================================================
// Mapping Management Functions
//=============================================================================

brain_kg_node_id_t pr_kg_get_kg_node(pr_kg_bridge_t bridge, uint64_t pr_node_id) {
    if (!bridge || !bridge->connected) {
        return BRAIN_KG_INVALID_NODE;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_get_kg_node", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    uint32_t map_idx = hash_find_pr(bridge, pr_node_id);
    brain_kg_node_id_t result = BRAIN_KG_INVALID_NODE;

    if (map_idx != HASH_INVALID) {
        result = bridge->mappings[map_idx].kg_node_id;
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    return result;
}

uint64_t pr_kg_get_pr_node(pr_kg_bridge_t bridge, brain_kg_node_id_t kg_node_id) {
    if (!bridge || !bridge->connected) {
        return PR_KG_INVALID_PR_ID;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_get_pr_node", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    uint32_t map_idx = hash_find_kg(bridge, kg_node_id);
    uint64_t result = PR_KG_INVALID_PR_ID;

    if (map_idx != HASH_INVALID) {
        result = bridge->mappings[map_idx].pr_node_id;
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    return result;
}

int pr_kg_get_mapping(
    pr_kg_bridge_t bridge,
    uint64_t pr_node_id,
    pr_kg_mapping_t* mapping)
{
    if (!bridge || !bridge->connected || !mapping) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_get_mapping: required parameter is NULL (bridge, bridge->connected, mapping)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_get_mapping", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    uint32_t map_idx = hash_find_pr(bridge, pr_node_id);
    if (map_idx == HASH_INVALID) {
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("PR node %lu not found", (unsigned long)pr_node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_kg_get_mapping: validation failed");
        return -1;
    }

    *mapping = bridge->mappings[map_idx];

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return 0;
}

int pr_kg_list_mappings(
    pr_kg_bridge_t bridge,
    pr_kg_query_result_t* result,
    uint32_t max_results)
{
    if (!bridge || !result || !result->mappings) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_list_mappings: required parameter is NULL (bridge, result, result->mappings)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_list_mappings", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    result->count = 0;
    uint32_t limit = (max_results < result->capacity) ? max_results : result->capacity;

    for (uint32_t i = 0; i < bridge->mapping_capacity && result->count < limit; i++) {
        if (bridge->mappings[i].state != PR_KG_MAPPING_STATE_INVALID) {
            result->mappings[result->count++] = bridge->mappings[i];
        }
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return (int)result->count;
}

const char* pr_kg_mapping_state_to_string(pr_kg_mapping_state_t state) {
    switch (state) {
        case PR_KG_MAPPING_STATE_INVALID:  return "INVALID";
        case PR_KG_MAPPING_STATE_PENDING:  return "PENDING";
        case PR_KG_MAPPING_STATE_SYNCED:   return "SYNCED";
        case PR_KG_MAPPING_STATE_DIRTY:    return "DIRTY";
        case PR_KG_MAPPING_STATE_ORPHANED: return "ORPHANED";
        case PR_KG_MAPPING_STATE_ERROR:    return "ERROR";
        default:                           return "UNKNOWN";
    }
}

//=============================================================================
// Batch Operations
//=============================================================================

int pr_kg_batch_register(
    pr_kg_bridge_t bridge,
    const uint64_t* pr_node_ids,
    const char** descriptions,
    uint32_t module_id,
    size_t count)
{
    if (!bridge || !bridge->connected || !pr_node_ids) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_batch_register: required parameter is NULL (bridge, bridge->connected, pr_node_ids)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_batch_register", 0.0f);


    if (count == 0) {
        return 0;
    }

    int success_count = 0;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_kg_bridge_heartbeat("pr_kg_bridge_loop",
                             (float)(i + 1) / (float)count);
        }

        const char* desc = (descriptions && descriptions[i]) ? descriptions[i] : NULL;
        brain_kg_node_id_t kg_id = pr_kg_register_memory(bridge, pr_node_ids[i],
                                                          desc, module_id);
        if (kg_id != BRAIN_KG_INVALID_NODE) {
            success_count++;
        }
    }

    clear_error();
    return success_count;
}

int pr_kg_batch_sync(pr_kg_bridge_t bridge) {
    if (!bridge || !bridge->connected) {
        set_error("Bridge not connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_batch_sync: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_batch_sync", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    int sync_count = 0;
    uint64_t now = get_time_ms();

    for (uint32_t i = 0; i < bridge->mapping_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->mapping_capacity > 256) {
            pr_kg_bridge_heartbeat("pr_kg_bridge_loop",
                             (float)(i + 1) / (float)bridge->mapping_capacity);
        }

        pr_kg_mapping_t* mapping = &bridge->mappings[i];

        if (mapping->state == PR_KG_MAPPING_STATE_SYNCED ||
            mapping->state == PR_KG_MAPPING_STATE_DIRTY) {

            /* Update KG node state to ACTIVE */
            brain_kg_update_node(bridge->brain_kg, mapping->kg_node_id,
                                 NULL, BRAIN_KG_STATE_ACTIVE);

            mapping->sync_time_ms = now;
            mapping->sync_count++;
            mapping->state = PR_KG_MAPPING_STATE_SYNCED;
            sync_count++;
        }
    }

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        bridge->stats.total_syncs += sync_count;
        bridge->stats.last_sync_time_ms = now;
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return sync_count;
}

int pr_kg_cleanup_stale(pr_kg_bridge_t bridge, uint64_t threshold_ms) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_cleanup_stale: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_cleanup_stale", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    if (threshold_ms == 0) {
        threshold_ms = bridge->config.stale_threshold_ms;
    }

    uint64_t now = get_time_ms();
    int removed_count = 0;

    for (uint32_t i = 0; i < bridge->mapping_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->mapping_capacity > 256) {
            pr_kg_bridge_heartbeat("pr_kg_bridge_loop",
                             (float)(i + 1) / (float)bridge->mapping_capacity);
        }

        pr_kg_mapping_t* mapping = &bridge->mappings[i];

        if (mapping->state != PR_KG_MAPPING_STATE_INVALID &&
            (now - mapping->sync_time_ms) > threshold_ms) {

            /* Remove from hash tables */
            hash_remove_pr(bridge, mapping->pr_node_id);
            hash_remove_kg(bridge, mapping->kg_node_id);

            /* Remove KG node */
            brain_kg_remove_node(bridge->brain_kg, mapping->kg_node_id);

            /* Clear mapping */
            memset(mapping, 0, sizeof(*mapping));
            mapping->state = PR_KG_MAPPING_STATE_INVALID;

            removed_count++;
        }
    }

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        bridge->stats.active_mappings -= removed_count;
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return removed_count;
}

int pr_kg_mark_dirty(pr_kg_bridge_t bridge, uint64_t pr_node_id) {
    if (!bridge || !bridge->connected) {
        set_error("Bridge not connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_mark_dirty: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_mark_dirty", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    uint32_t map_idx = hash_find_pr(bridge, pr_node_id);
    if (map_idx == HASH_INVALID) {
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("PR node not found");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_kg_mark_dirty: validation failed");
        return -1;
    }

    bridge->mappings[map_idx].state = PR_KG_MAPPING_STATE_DIRTY;

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return 0;
}

//=============================================================================
// Prime Signature Integration
//=============================================================================

prime_signature_t* pr_kg_signature_from_node(
    pr_kg_bridge_t bridge,
    brain_kg_node_id_t kg_node_id)
{
    if (!bridge || !bridge->connected) {
        set_error("Bridge not connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_signature_from_node: required parameter is NULL (bridge, bridge->connected)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_signature_from", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    /* Get KG node */
    const brain_kg_node_t* kg_node = brain_kg_get_node(bridge->brain_kg,
                                                        kg_node_id);
    if (!kg_node) {
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("KG node not found");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_signature_from_node: kg_node is NULL");
        return NULL;
    }

    /* Create empty signature */
    prime_signature_t* sig = prime_sig_create();
    if (!sig) {
        MUTEX_UNLOCK(bridge->base.mutex);
        set_error("Failed to create signature");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_kg_signature_from_node: sig is NULL");
        return NULL;
    }

    /* Try to recover signature hash from metadata */
    for (uint32_t i = 0; i < kg_node->metadata_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && kg_node->metadata_count > 256) {
            pr_kg_bridge_heartbeat("pr_kg_bridge_loop",
                             (float)(i + 1) / (float)kg_node->metadata_count);
        }

        if (strcmp(kg_node->metadata[i].key, "sig_hash") == 0) {
            /* Parse hex hash value */
            sig->hash = strtoull(kg_node->metadata[i].value, NULL, 16);
        }
        if (strcmp(kg_node->metadata[i].key, "sig_factors") == 0) {
            // P1-2 fix: Use strtol instead of atoi for safe conversion
            char* endptr;
            long val = strtol(kg_node->metadata[i].value, &endptr, 10);
            if (endptr != kg_node->metadata[i].value && val >= 0 && val <= UINT32_MAX) {
                sig->num_factors = (uint32_t)val;
            }
        }
    }

    /* Generate partial signature from node name if no stored hash */
    if (sig->hash == 0 && kg_node->name[0]) {
        prime_signature_t* name_sig = prime_sig_from_text(kg_node->name);
        if (name_sig) {
            *sig = *name_sig;
            prime_sig_destroy(name_sig);
        }
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return sig;
}

int pr_kg_find_similar_memories(
    pr_kg_bridge_t bridge,
    brain_kg_node_id_t kg_node_id,
    float similarity_threshold,
    uint64_t* pr_node_ids,
    uint32_t max_ids)
{
    if (!bridge || !bridge->connected || !pr_node_ids) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_find_similar_memories: required parameter is NULL (bridge, bridge->connected, pr_node_ids)");
        return -1;
    }

    /* Get reference signature from KG node */
    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_find_similar_m", 0.0f);


    prime_signature_t* ref_sig = pr_kg_signature_from_node(bridge, kg_node_id);
    if (!ref_sig) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ref_sig is NULL");

        return -1;
    }

    int result = pr_kg_find_similar_by_signature(bridge, ref_sig,
                                                  similarity_threshold,
                                                  pr_node_ids, max_ids);

    prime_sig_destroy(ref_sig);
    return result;
}

int pr_kg_find_similar_by_signature(
    pr_kg_bridge_t bridge,
    const prime_signature_t* signature,
    float similarity_threshold,
    uint64_t* pr_node_ids,
    uint32_t max_ids)
{
    if (!bridge || !bridge->connected || !signature || !pr_node_ids) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_find_similar_by_signature: required parameter is NULL (bridge, bridge->connected, signature, pr_node_ids)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_find_similar_b", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    uint32_t found = 0;

    /* Iterate all mappings and check similarity */
    for (uint32_t i = 0; i < bridge->mapping_capacity && found < max_ids; i++) {
        pr_kg_mapping_t* mapping = &bridge->mappings[i];

        if (mapping->state != PR_KG_MAPPING_STATE_SYNCED) {
            continue;
        }

        /* Get signature for this mapped memory from KG metadata */
        const brain_kg_node_t* kg_node = brain_kg_get_node(bridge->brain_kg,
                                                            mapping->kg_node_id);
        if (!kg_node) {
            continue;
        }

        /* Try to get stored hash for comparison */
        uint64_t stored_hash = 0;
        for (uint32_t j = 0; j < kg_node->metadata_count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && kg_node->metadata_count > 256) {
                pr_kg_bridge_heartbeat("pr_kg_bridge_loop",
                                 (float)(j + 1) / (float)kg_node->metadata_count);
            }

            if (strcmp(kg_node->metadata[j].key, "sig_hash") == 0) {
                stored_hash = strtoull(kg_node->metadata[j].value, NULL, 16);
                break;
            }
        }

        /* Fast hash comparison first */
        if (stored_hash != 0 && stored_hash == signature->hash) {
            /* Hash match - likely identical */
            pr_node_ids[found++] = mapping->pr_node_id;
            continue;
        }

        /* For more detailed comparison, we would need the full signature
         * stored or access to the original memory. For now, we skip if
         * no hash match (full implementation would query PR Memory). */
    }

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        bridge->stats.total_queries++;
    }

    MUTEX_UNLOCK(bridge->base.mutex);
    clear_error();
    return (int)found;
}

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

int pr_kg_get_stats(pr_kg_bridge_t bridge, pr_kg_bridge_stats_t* stats) {
    if (!bridge || !stats) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_kg_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_get_stats", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);
    *stats = bridge->stats;
    MUTEX_UNLOCK(bridge->base.mutex);

    return 0;
}

void pr_kg_reset_stats(pr_kg_bridge_t bridge) {
    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_reset_stats", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    /* Preserve active_mappings count */
    uint64_t active = bridge->stats.active_mappings;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.active_mappings = active;

    MUTEX_UNLOCK(bridge->base.mutex);
}

size_t pr_kg_generate_summary(pr_kg_bridge_t bridge, char* buf, size_t size) {
    if (!bridge || !buf || size == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_generate_summa", 0.0f);


    MUTEX_LOCK(bridge->base.mutex);

    pr_kg_bridge_stats_t* s = &bridge->stats;

    int written = snprintf(buf, size,
        "PR-KG Bridge Summary:\n"
        "---------------------\n"
        "Connected: %s\n"
        "Active Mappings: %lu\n"
        "Total Registrations: %lu\n"
        "Total Unregistrations: %lu\n"
        "Total Syncs: %lu\n"
        "Total Edge Syncs: %lu\n"
        "Total Queries: %lu\n"
        "Errors: %lu\n"
        "Last Sync: %lu ms ago\n"
        "Config:\n"
        "  - Auto Register: %s\n"
        "  - Sync on Update: %s\n"
        "  - Edge Weight Scale: %.2f\n",
        bridge->connected ? "Yes" : "No",
        (unsigned long)s->active_mappings,
        (unsigned long)s->total_registrations,
        (unsigned long)s->total_unregistrations,
        (unsigned long)s->total_syncs,
        (unsigned long)s->total_edge_syncs,
        (unsigned long)s->total_queries,
        (unsigned long)s->error_count,
        s->last_sync_time_ms ? (unsigned long)(get_time_ms() - s->last_sync_time_ms) : 0,
        bridge->config.auto_register_memories ? "Yes" : "No",
        bridge->config.sync_on_update ? "Yes" : "No",
        bridge->config.edge_weight_scale
    );

    MUTEX_UNLOCK(bridge->base.mutex);

    return (written > 0) ? (size_t)written : 0;
}

void pr_kg_print_state(pr_kg_bridge_t bridge) {
    if (!bridge) {
        printf("PR-KG Bridge: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_print_state", 0.0f);


    char buf[NIMCP_DEBUG_BUFFER_SIZE];
    pr_kg_generate_summary(bridge, buf, sizeof(buf));
    printf("%s", buf);

    /* Print mapping details */
    printf("\nMappings:\n");

    MUTEX_LOCK(bridge->base.mutex);

    int printed = 0;
    for (uint32_t i = 0; i < bridge->mapping_capacity && printed < 10; i++) {
        pr_kg_mapping_t* m = &bridge->mappings[i];
        if (m->state != PR_KG_MAPPING_STATE_INVALID) {
            printf("  [%u] PR:%lu -> KG:%u (%s) module=%u syncs=%u\n",
                   i,
                   (unsigned long)m->pr_node_id,
                   m->kg_node_id,
                   pr_kg_mapping_state_to_string(m->state),
                   m->module_id,
                   m->sync_count);
            printed++;
        }
    }

    if (bridge->stats.active_mappings > 10) {
        printf("  ... and %lu more mappings\n",
               (unsigned long)(bridge->stats.active_mappings - 10));
    }

    MUTEX_UNLOCK(bridge->base.mutex);
}

const char* pr_kg_get_last_error(void) {
    return s_last_error[0] ? s_last_error : NULL;
}

//=============================================================================
// Query Result Management
//=============================================================================

pr_kg_query_result_t* pr_kg_query_result_create(uint32_t capacity) {
    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_query_result_c", 0.0f);


    if (capacity == 0) {
        capacity = 64;  /* Default capacity */
    }

    if (capacity > PR_KG_MAX_QUERY_RESULTS) {
        capacity = PR_KG_MAX_QUERY_RESULTS;
    }

    pr_kg_query_result_t* result = (pr_kg_query_result_t*)nimcp_calloc(
        1, sizeof(pr_kg_query_result_t));
    if (!result) {
        set_error("Failed to allocate query result");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_kg_query_result_create: result is NULL");
        return NULL;
    }

    result->mappings = (pr_kg_mapping_t*)nimcp_calloc(capacity, sizeof(pr_kg_mapping_t));
    if (!result->mappings) {
        nimcp_free(result);
        result = NULL;
        set_error("Failed to allocate mapping array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_kg_query_result_create: result->mappings is NULL");
        return NULL;
    }

    result->capacity = capacity;
    result->count = 0;

    return result;
}

void pr_kg_query_result_destroy(pr_kg_query_result_t* result) {
    if (!result) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_query_result_d", 0.0f);


    if (result->mappings) {
        nimcp_free(result->mappings);
    }

    nimcp_free(result);
    result = NULL;
}

void pr_kg_query_result_clear(pr_kg_query_result_t* result) {
    if (!result) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_kg_bridge_heartbeat("pr_kg_bridge_pr_kg_query_result_c", 0.0f);


    if (result->mappings) {
        memset(result->mappings, 0, result->capacity * sizeof(pr_kg_mapping_t));
    }

    result->count = 0;
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_kg_bridge_set_instance_health_agent(
    pr_kg_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_kg_bridge_training_begin(pr_kg_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_kg_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_kg_bridge_heartbeat_instance(bridge->health_agent, "pr_kg_bridge_training_begin", 0.0f);
    return 0;
}

int pr_kg_bridge_training_end(pr_kg_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_kg_bridge_training_end: NULL argument");
        return -1;
    }
    pr_kg_bridge_heartbeat_instance(bridge->health_agent, "pr_kg_bridge_training_end", 1.0f);
    return 0;
}

int pr_kg_bridge_training_step(pr_kg_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_kg_bridge_training_step: NULL argument");
        return -1;
    }
    pr_kg_bridge_heartbeat_instance(bridge->health_agent, "pr_kg_bridge_training_step", progress);
    return 0;
}
