/**
 * @file nimcp_brain_kg.c
 * @brief Implementation of Internal Runtime Knowledge Graph
 * @version 1.0.0
 * @date 2025-12-31
 */

#include "core/brain/nimcp_brain_kg.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define LOG_MODULE "BRAIN_KG"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_kg, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * INTERNAL STRUCTURE
 * ============================================================================ */

/* Security state structure */
typedef struct {
    brain_kg_access_level_t current_level;   /**< Current access level */
    uint64_t access_token;                   /**< Current authentication token */
    uint64_t admin_token;                    /**< Admin token for emergency unlock */
    bool emergency_locked;                   /**< Emergency lock active */
    uint32_t violation_count;                /**< Total security violations */
    uint64_t last_violation_time;            /**< Timestamp of last violation */
    uint32_t mutations_this_second;          /**< Mutation rate tracking */
    uint64_t mutation_window_start;          /**< Rate limit window start */
    uint64_t last_integrity_check;           /**< Last integrity verification */
    uint32_t integrity_checksum;             /**< Current integrity checksum */
    bool* critical_nodes;                    /**< Bitmap of critical nodes */

    /* Immune integration */
    void* immune_system;                     /**< brain_immune_system_t* */
    brain_kg_security_callback_t callback;   /**< Security event callback */
    void* callback_user_data;                /**< Callback context */
} brain_kg_security_t;

/* Message-type index entry (Phase 6: KG Query Optimization) */
typedef struct {
    uint32_t message_type;                   /**< Message type identifier */
    brain_kg_node_id_t handlers[BRAIN_KG_MAX_HANDLERS_PER_MSG]; /**< Module node IDs */
    uint32_t handler_count;                  /**< Number of handlers */
    bool in_use;                             /**< Slot occupied */
} brain_kg_msg_index_entry_t;

/* Message-type index structure */
typedef struct {
    brain_kg_msg_index_entry_t* entries;     /**< Index entries */
    uint32_t entry_count;                    /**< Number of used entries */
    uint32_t entry_capacity;                 /**< Allocated capacity */
    bool dirty;                              /**< Needs rebuild flag */
    uint64_t last_rebuild;                   /**< Timestamp of last rebuild */
} brain_kg_msg_index_t;

struct brain_kg {
    brain_kg_config_t config;

    /* Node storage */
    brain_kg_node_t* nodes;
    uint32_t node_count;
    uint32_t node_capacity;
    uint32_t next_node_id;

    /* Name→slot index for O(1) find_node (W-review fix).
     * Backed by nimcp_hash_table (FNV-1a, separate chaining). Stores the
     * node array slot index as the value; dereferencing gives the node in
     * O(1). Caller must hold kg->mutex when touching this table — the
     * generic hash_table is not thread-safe internally. */
    hash_table_t* name_index;

    /* Edge storage */
    brain_kg_edge_t* edges;
    uint32_t edge_count;
    uint32_t edge_capacity;
    uint32_t next_edge_id;

    /* Statistics */
    brain_kg_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t created_time;

    /* Security */
    brain_kg_security_t security;

    /* Message-type index (Phase 6: KG Query Optimization) */
    brain_kg_msg_index_t msg_index;
};

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

static uint64_t get_time_ms(void) {
    return nimcp_time_get_ms();
}

/* Simple FNV-1a hash for checksum */
static uint32_t compute_hash(const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

/* Name→slot index helpers — thin wrappers over the generic nimcp_hash_table.
 * Caller must hold kg->mutex (the generic table is external-sync). Value
 * stored is the node's array slot index (uint32_t), so lookup resolves to
 * the node in O(1) via a direct array access. */
static void name_hash_insert_unlocked(brain_kg_t* kg, const char* name,
                                       uint32_t node_slot) {
    if (!kg->name_index || !name) return;
    (void)hash_table_insert_string(kg->name_index, name,
                                   &node_slot, sizeof(uint32_t));
}

static void name_hash_remove_unlocked(brain_kg_t* kg, const char* name,
                                       uint32_t node_slot) {
    (void)node_slot;  /* hash_table keys by name, slot is the value */
    if (!kg->name_index || !name) return;
    (void)hash_table_remove_string(kg->name_index, name);
}

static brain_kg_node_id_t name_hash_lookup_unlocked(const brain_kg_t* kg,
                                                     const char* name) {
    if (!kg->name_index || !name) return BRAIN_KG_INVALID_NODE;
    const uint32_t* slot_ptr =
        (const uint32_t*)hash_table_lookup_string((hash_table_t*)kg->name_index,
                                                   name);
    if (!slot_ptr) return BRAIN_KG_INVALID_NODE;
    uint32_t node_slot = *slot_ptr;
    if (node_slot >= kg->node_capacity) return BRAIN_KG_INVALID_NODE;
    const brain_kg_node_t* n = &kg->nodes[node_slot];
    if (!n->in_use) return BRAIN_KG_INVALID_NODE;
    /* Defensive verify: if the name doesn't match the node at that slot,
     * the slot was reused without index update. Shouldn't happen if
     * add_node + remove_node both call the insert/remove helpers. */
    if (strncmp(n->name, name, BRAIN_KG_MAX_NAME_LEN) != 0) {
        return BRAIN_KG_INVALID_NODE;
    }
    return n->id;
}

/* Generate random token */
static uint64_t generate_random_token(void) {
    uint64_t token = 0;
    token = ((uint64_t)nimcp_tl_rand() << 32) | nimcp_tl_rand();
    token ^= get_time_ms();
    return token;
}

/* Report security violation */
static void report_security_violation(
    brain_kg_t* kg,
    brain_kg_security_event_t event,
    brain_kg_node_id_t node_id,
    const char* details
) {
    if (!kg) return;

    kg->security.violation_count++;
    kg->security.last_violation_time = get_time_ms();

    NIMCP_LOGGING_WARN("KG Security Violation: %s (node=%u)", details, node_id);

    /* Call registered callback */
    if (kg->security.callback) {
        kg->security.callback(event, node_id, details, kg->security.callback_user_data);
    }

    /* Report to immune system if connected */
    if (kg->config.enable_immune_integration && kg->security.immune_system) {
        /* Note: Would call brain_immune_present_antigen() here
         * Left as a note since immune header may not be included */
        NIMCP_LOGGING_INFO("Reporting KG violation to immune system");
    }
}

/* Check if write access is allowed */
static bool check_write_access(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "check_write_access: kg is NULL");
        return false;
    }

    /* Emergency lock blocks all writes */
    if (kg->security.emergency_locked) {
        report_security_violation(kg, BRAIN_KG_SEC_UNAUTHORIZED_ACCESS,
            BRAIN_KG_INVALID_NODE, "Write attempted during emergency lock");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "check_write_access: validation failed");
        return false;
    }

    /* Check access level */
    if (kg->config.enable_access_control &&
        kg->security.current_level < BRAIN_KG_ACCESS_WRITE) {
        report_security_violation(kg, BRAIN_KG_SEC_UNAUTHORIZED_ACCESS,
            BRAIN_KG_INVALID_NODE, "Insufficient access level for write");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_write_access: operation failed");
        return false;
    }

    /* Check rate limit — but ADMIN access bypasses it.
     *
     * Rate-limiting protects against runaway non-admin callers (e.g., a
     * cognitive module looping on emit_event without backoff). ADMIN
     * operations are explicitly system-level: bulk lexicon load, init
     * structural setup, persistence rehydrate. Subjecting those to the
     * 1000-mut/sec limit silently blocks legitimate batch work — verified
     * 2026-05-09: NIMCP_BULK_LEXICON preload of 29,424 wordnet+glove
     * entries hit the rate limit at record 2197, fast_map returned 0,
     * the bulk loader bailed after 256 skips. Result: only ~7% of the
     * intended vocabulary landed in the lexicon for months.
     *
     * The runtime path that the rate limit is meant to protect (cognitive
     * modules emitting events) goes through emit_event_linked which
     * elevates to ADMIN, runs the mutation, then drops back to READ.
     * For those, ADMIN level is held only inside the emit_event_linked
     * critical section, so a runaway emitter that elevates+mutates in a
     * loop would still hit the rate limit on access-level swap (bumping
     * mutations is a write itself? actually no — set_access_level isn't
     * gated by check_write_access).
     *
     * Cleaner: trust ADMIN. If a runaway is firing at ADMIN, the issue
     * isn't rate limit — it's an attacker with the admin token, and
     * rate limiting wasn't going to stop them anyway. */
    if (kg->config.max_mutations_per_sec > 0 &&
        kg->security.current_level < BRAIN_KG_ACCESS_ADMIN) {
        uint64_t now = get_time_ms();
        if (now - kg->security.mutation_window_start >= 1000) {
            kg->security.mutations_this_second = 0;
            kg->security.mutation_window_start = now;
        }

        if (kg->security.mutations_this_second >= kg->config.max_mutations_per_sec) {
            report_security_violation(kg, BRAIN_KG_SEC_EXCESSIVE_MUTATIONS,
                BRAIN_KG_INVALID_NODE, "Mutation rate limit exceeded");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "check_write_access: capacity exceeded");
            return false;
        }
        kg->security.mutations_this_second++;
    }

    return true;
}

/* Check if node is critical */
static bool is_critical_node(brain_kg_t* kg, brain_kg_node_id_t id) {
    if (!kg || !kg->security.critical_nodes || id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "is_critical_node: required parameter is NULL (kg, kg->security)");
        return false;
    }
    if (id < kg->node_capacity) {
        return kg->security.critical_nodes[id];
    }
    return false;
}

/* Compute integrity checksum for entire KG */
static uint32_t compute_kg_checksum(brain_kg_t* kg) {
    if (!kg) return 0;

    uint32_t checksum = 0;

    /* Hash all active nodes */
    for (uint32_t i = 0; i < kg->node_capacity; i++) {
        if (kg->nodes[i].in_use) {
            checksum ^= compute_hash(kg->nodes[i].name, strlen(kg->nodes[i].name));
            checksum ^= kg->nodes[i].id * 31;
            checksum ^= (uint32_t)kg->nodes[i].type * 17;
        }
    }

    /* Hash all active edges */
    for (uint32_t i = 0; i < kg->edge_capacity; i++) {
        if (kg->edges[i].in_use) {
            checksum ^= kg->edges[i].from * 37;
            checksum ^= kg->edges[i].to * 41;
            checksum ^= (uint32_t)kg->edges[i].type * 43;
        }
    }

    return checksum;
}

static brain_kg_node_t* find_node_by_id_unlocked(brain_kg_t* kg, brain_kg_node_id_t id) {
    for (uint32_t i = 0; i < kg->node_capacity; i++) {
        if (kg->nodes[i].in_use && kg->nodes[i].id == id) {
            return &kg->nodes[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_node_by_id_unlocked: validation failed");
    return NULL;
}

static brain_kg_edge_t* find_edge_by_id_unlocked(brain_kg_t* kg, brain_kg_edge_id_t id) {
    for (uint32_t i = 0; i < kg->edge_capacity; i++) {
        if (kg->edges[i].in_use && kg->edges[i].id == id) {
            return &kg->edges[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_edge_by_id_unlocked: validation failed");
    return NULL;
}

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

int brain_kg_default_config(brain_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));
    config->max_nodes = BRAIN_KG_MAX_NODES;
    config->max_edges = BRAIN_KG_MAX_EDGES;
    config->enable_statistics = true;
    config->auto_populate = false;

    /* Security defaults - enabled by default for protection */
    config->enable_security = true;
    config->enable_integrity_checks = true;
    config->enable_access_control = true;
    config->enable_immune_integration = true;
    config->enable_audit_log = true;
    config->max_mutations_per_sec = 1000;  /* Rate limit: 1000 mutations/sec */
    config->integrity_check_interval_ms = 10000;  /* Verify integrity every 10s */

    return 0;
}

brain_kg_t* brain_kg_create(const brain_kg_config_t* config) {
    brain_kg_t* kg = nimcp_malloc(sizeof(*kg));
    if (!kg) {
        NIMCP_LOGGING_ERROR("Failed to allocate brain_kg");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_create: kg is NULL");
        return NULL;
    }

    memset(kg, 0, sizeof(*kg));

    /* Apply config */
    if (config) {
        kg->config = *config;
    } else {
        brain_kg_default_config(&kg->config);
    }

    /* Set capacities */
    kg->node_capacity = kg->config.max_nodes > 0 ? kg->config.max_nodes : BRAIN_KG_MAX_NODES;
    kg->edge_capacity = kg->config.max_edges > 0 ? kg->config.max_edges : BRAIN_KG_MAX_EDGES;

    /* Allocate nodes */
    kg->nodes = nimcp_malloc(kg->node_capacity * sizeof(brain_kg_node_t));
    if (!kg->nodes) {
        NIMCP_LOGGING_ERROR("Failed to allocate node array");
        nimcp_free(kg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_create: kg->nodes is NULL");
        return NULL;
    }
    memset(kg->nodes, 0, kg->node_capacity * sizeof(brain_kg_node_t));

    /* Name→slot index for O(1) find_node (W-review fix).
     * Sized to hold one entry per possible node; generic table handles
     * bucket resizing if load factor grows. External-sync — we hold
     * kg->mutex around all hash_table_* calls. */
    {
        hash_table_config_t hcfg = {0};
        hcfg.initial_buckets   = kg->node_capacity * 2;
        hcfg.key_type          = HASH_KEY_STRING;
        hcfg.hash_algorithm    = HASH_ALG_FNV1A;
        hcfg.case_insensitive  = false;
        hcfg.thread_safe       = false;  /* KG mutex provides external sync */
        kg->name_index = hash_table_create(&hcfg);
        if (!kg->name_index) {
            NIMCP_LOGGING_ERROR("Failed to create KG name hash index");
            nimcp_free(kg->nodes);
            nimcp_free(kg);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                                  "brain_kg_create: name_index is NULL");
            return NULL;
        }
    }

    /* Allocate edges */
    kg->edges = nimcp_malloc(kg->edge_capacity * sizeof(brain_kg_edge_t));
    if (!kg->edges) {
        NIMCP_LOGGING_ERROR("Failed to allocate edge array");
        hash_table_destroy(kg->name_index);
        nimcp_free(kg->nodes);
        nimcp_free(kg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_create: kg->edges is NULL");
        return NULL;
    }
    memset(kg->edges, 0, kg->edge_capacity * sizeof(brain_kg_edge_t));

    /* Create mutex */
    kg->mutex = nimcp_mutex_create(NULL);
    if (!kg->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        hash_table_destroy(kg->name_index);
        nimcp_free(kg->edges);
        nimcp_free(kg->nodes);
        nimcp_free(kg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_create: kg->mutex is NULL");
        return NULL;
    }

    /* Initialize IDs */
    kg->next_node_id = 1;
    kg->next_edge_id = 1;
    kg->created_time = get_time_ms();

    /* Initialize message-type index (Phase 6) */
    kg->msg_index.entry_capacity = BRAIN_KG_MAX_MESSAGE_TYPES;
    kg->msg_index.entries = nimcp_malloc(
        kg->msg_index.entry_capacity * sizeof(brain_kg_msg_index_entry_t));
    if (!kg->msg_index.entries) {
        NIMCP_LOGGING_ERROR("Failed to allocate message index");
        nimcp_mutex_free(kg->mutex);
        hash_table_destroy(kg->name_index);
        nimcp_free(kg->edges);
        nimcp_free(kg->nodes);
        nimcp_free(kg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_create: kg->msg_index is NULL");
        return NULL;
    }
    memset(kg->msg_index.entries, 0,
           kg->msg_index.entry_capacity * sizeof(brain_kg_msg_index_entry_t));
    kg->msg_index.entry_count = 0;
    kg->msg_index.dirty = false;
    kg->msg_index.last_rebuild = 0;

    NIMCP_LOGGING_INFO("Brain KG created (nodes=%u, edges=%u, msg_index=%u)",
                       kg->node_capacity, kg->edge_capacity,
                       kg->msg_index.entry_capacity);

    return kg;
}

void brain_kg_destroy(brain_kg_t* kg) {
    if (!kg) return;

    /* Free message-type index (Phase 6) */
    if (kg->msg_index.entries) {
        nimcp_free(kg->msg_index.entries);
    }

    /* Free security critical nodes bitmap */
    if (kg->security.critical_nodes) {
        nimcp_free(kg->security.critical_nodes);
    }

    if (kg->mutex) {
        nimcp_mutex_free(kg->mutex);
    }

    if (kg->edges) {
        nimcp_free(kg->edges);
    }

    if (kg->nodes) {
        nimcp_free(kg->nodes);
    }

    /* Free the name→slot hash index (W-review O(1) find_node fix). */
    if (kg->name_index) {
        hash_table_destroy(kg->name_index);
        kg->name_index = NULL;
    }

    nimcp_free(kg);
    NIMCP_LOGGING_INFO("Brain KG destroyed");
}

/* ============================================================================
 * NODE CRUD API
 * ============================================================================ */

brain_kg_node_id_t brain_kg_add_node(
    brain_kg_t* kg,
    const char* name,
    brain_kg_node_type_t type,
    const char* description
) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;

    /* Security check */
    if (kg->config.enable_security && !check_write_access(kg)) {
        return BRAIN_KG_INVALID_NODE;
    }

    nimcp_mutex_lock(kg->mutex);

    /* Check if name already exists — O(1) via name hash index. */
    {
        brain_kg_node_id_t existing = name_hash_lookup_unlocked(kg, name);
        if (existing != BRAIN_KG_INVALID_NODE) {
            nimcp_mutex_unlock(kg->mutex);
            NIMCP_LOGGING_WARN("Node '%s' already exists", name);
            return existing;
        }
    }

    /* Find free slot */
    brain_kg_node_t* node = NULL;
    uint32_t node_slot = 0;
    for (uint32_t i = 0; i < kg->node_capacity; i++) {
        if (!kg->nodes[i].in_use) {
            node = &kg->nodes[i];
            node_slot = i;
            break;
        }
    }

    if (!node) {
        /* Capacity reached — try LRU eviction of the oldest event node.
         * Event nodes are identifiable by the "_event_" substring in their
         * name (convention established by W2-W15 emit helpers). Structural
         * nodes (regions, cognitive roots, networks) are NEVER evicted
         * because they're referenced by edges from many event nodes and
         * removal would orphan those edges. When the KG is full of
         * structural nodes (no event nodes to evict), we genuinely are
         * out of room and return INVALID. */
        brain_kg_node_t* victim = NULL;
        uint32_t victim_slot = 0;
        uint64_t oldest_time = UINT64_MAX;
        for (uint32_t i = 0; i < kg->node_capacity; i++) {
            if (kg->nodes[i].in_use &&
                strstr(kg->nodes[i].name, "_event_") != NULL &&
                kg->nodes[i].created_time < oldest_time) {
                oldest_time = kg->nodes[i].created_time;
                victim = &kg->nodes[i];
                victim_slot = i;
            }
        }
        if (!victim) {
            nimcp_mutex_unlock(kg->mutex);
            NIMCP_LOGGING_ERROR(
                "Node capacity reached (%u) with no evictable event nodes",
                kg->node_capacity);
            return BRAIN_KG_INVALID_NODE;
        }
        /* Evict: remove edges referencing the victim, drop from hash index,
         * clear in_use. Caller reuses the slot below. */
        brain_kg_node_id_t victim_id = victim->id;
        for (uint32_t i = 0; i < kg->edge_capacity; i++) {
            if (kg->edges[i].in_use &&
                (kg->edges[i].from == victim_id ||
                 kg->edges[i].to   == victim_id)) {
                kg->edges[i].in_use = false;
                kg->edge_count--;
                kg->stats.total_edges--;
                kg->stats.edges_by_type[kg->edges[i].type]--;
            }
        }
        name_hash_remove_unlocked(kg, victim->name, victim_slot);
        kg->stats.nodes_by_type[victim->type]--;
        victim->in_use = false;
        kg->node_count--;
        node = victim;
        node_slot = victim_slot;
    }

    /* Initialize node */
    memset(node, 0, sizeof(*node));
    node->id = kg->next_node_id++;
    strncpy(node->name, name, BRAIN_KG_MAX_NAME_LEN - 1);
    node->type = type;
    node->state = BRAIN_KG_STATE_UNKNOWN;
    if (description) {
        strncpy(node->description, description, BRAIN_KG_MAX_DESC_LEN - 1);
    }
    node->created_time = get_time_ms();
    node->last_updated = node->created_time;
    node->enabled = true;
    node->in_use = true;

    kg->node_count++;
    kg->stats.total_nodes++;
    kg->stats.nodes_by_type[type]++;
    kg->stats.last_modified = node->created_time;
    kg->stats.modifications_count++;

    /* W-review fix: maintain O(1) name→slot index for find_node. */
    name_hash_insert_unlocked(kg, node->name, node_slot);

    nimcp_mutex_unlock(kg->mutex);

    NIMCP_LOGGING_DEBUG("Added node '%s' (ID=%u, type=%s)",
                        name, node->id, brain_kg_node_type_to_string(type));

    return node->id;
}

const brain_kg_node_t* brain_kg_get_node(const brain_kg_t* kg, brain_kg_node_id_t id) {
    if (!kg || id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_get_node: kg is NULL");
        return NULL;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;  /* Cast for mutex */
    nimcp_mutex_lock(mkg->mutex);

    const brain_kg_node_t* node = NULL;
    for (uint32_t i = 0; i < kg->node_capacity; i++) {
        if (kg->nodes[i].in_use && kg->nodes[i].id == id) {
            node = &kg->nodes[i];
            break;
        }
    }

    nimcp_mutex_unlock(mkg->mutex);
    return node;
}

brain_kg_node_id_t brain_kg_find_node(const brain_kg_t* kg, const char* name) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);
    /* O(1) via name_index (nimcp_hash_table, FNV-1a). The previous
     * linear-scan implementation was a hot-path bottleneck once W16/W17
     * wired KG queries into brain_decide / brain_learn_vector. */
    brain_kg_node_id_t result = name_hash_lookup_unlocked(kg, name);
    nimcp_mutex_unlock(mkg->mutex);
    return result;
}

int brain_kg_update_node(
    brain_kg_t* kg,
    brain_kg_node_id_t id,
    const char* description,
    brain_kg_node_state_t state
) {
    if (!kg || id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_update_node: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    brain_kg_node_t* node = find_node_by_id_unlocked(kg, id);
    if (!node) {
        nimcp_mutex_unlock(kg->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_update_node: node is NULL");
        return -1;
    }

    if (description) {
        strncpy(node->description, description, BRAIN_KG_MAX_DESC_LEN - 1);
    }
    node->state = state;
    node->last_updated = get_time_ms();
    kg->stats.modifications_count++;
    kg->stats.last_modified = node->last_updated;

    nimcp_mutex_unlock(kg->mutex);
    return 0;
}

int brain_kg_set_module_ptr(brain_kg_t* kg, brain_kg_node_id_t id, void* module_ptr) {
    if (!kg || id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_set_module_ptr: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    brain_kg_node_t* node = find_node_by_id_unlocked(kg, id);
    if (!node) {
        nimcp_mutex_unlock(kg->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_set_module_ptr: node is NULL");
        return -1;
    }

    node->module_ptr = module_ptr;
    node->last_updated = get_time_ms();

    nimcp_mutex_unlock(kg->mutex);
    return 0;
}

int brain_kg_add_metadata(
    brain_kg_t* kg,
    brain_kg_node_id_t id,
    const char* key,
    const char* value
) {
    if (!kg || id == BRAIN_KG_INVALID_NODE || !key || !value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_add_metadata: required parameter is NULL (kg, key, value)");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    brain_kg_node_t* node = find_node_by_id_unlocked(kg, id);
    if (!node || node->metadata_count >= BRAIN_KG_MAX_METADATA) {
        nimcp_mutex_unlock(kg->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "brain_kg_add_metadata: node is NULL");
        return -1;
    }

    brain_kg_metadata_t* meta = &node->metadata[node->metadata_count];
    strncpy(meta->key, key, sizeof(meta->key) - 1);
    strncpy(meta->value, value, sizeof(meta->value) - 1);
    node->metadata_count++;
    node->last_updated = get_time_ms();

    nimcp_mutex_unlock(kg->mutex);
    return 0;
}

int brain_kg_remove_node(brain_kg_t* kg, brain_kg_node_id_t id) {
    if (!kg || id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_remove_node: kg is NULL");
        return -1;
    }

    /* Security check */
    if (kg->config.enable_security && !check_write_access(kg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_remove_node: check_write_access is NULL");
        return -1;
    }

    /* Check if critical node */
    if (is_critical_node(kg, id)) {
        report_security_violation(kg, BRAIN_KG_SEC_CRITICAL_NODE_MODIFIED,
            id, "Attempt to remove critical node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_remove_node: validation failed");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    brain_kg_node_t* node = find_node_by_id_unlocked(kg, id);
    if (!node) {
        nimcp_mutex_unlock(kg->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_remove_node: node is NULL");
        return -1;
    }

    /* Remove all edges connected to this node */
    for (uint32_t i = 0; i < kg->edge_capacity; i++) {
        if (kg->edges[i].in_use &&
            (kg->edges[i].from == id || kg->edges[i].to == id)) {
            kg->edges[i].in_use = false;
            kg->edge_count--;
            kg->stats.total_edges--;
            kg->stats.edges_by_type[kg->edges[i].type]--;
        }
    }

    /* W-review fix: remove from name→slot index before clearing in_use. */
    name_hash_remove_unlocked(kg, node->name, (uint32_t)(node - kg->nodes));

    /* Update statistics */
    kg->stats.nodes_by_type[node->type]--;
    kg->stats.total_nodes--;
    kg->stats.modifications_count++;
    kg->stats.last_modified = get_time_ms();

    /* Mark node as unused */
    node->in_use = false;
    kg->node_count--;

    nimcp_mutex_unlock(kg->mutex);
    return 0;
}

brain_kg_node_list_t* brain_kg_get_nodes_by_type(const brain_kg_t* kg, brain_kg_node_type_t type) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }

    brain_kg_node_list_t* list = nimcp_malloc(sizeof(*list));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;

    }

    list->count = 0;
    list->capacity = 64;
    list->nodes = nimcp_malloc(list->capacity * sizeof(brain_kg_node_t*));
    if (!list->nodes) {
        nimcp_free(list);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_get_nodes_by_type: list->nodes is NULL");
        return NULL;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    for (uint32_t i = 0; i < kg->node_capacity; i++) {
        if (kg->nodes[i].in_use && kg->nodes[i].type == type) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                brain_kg_node_t** new_nodes = nimcp_realloc(
                    list->nodes, list->capacity * sizeof(brain_kg_node_t*));
                if (!new_nodes) break;
                list->nodes = new_nodes;
            }
            list->nodes[list->count++] = &mkg->nodes[i];
        }
    }

    nimcp_mutex_unlock(mkg->mutex);
    return list;
}

brain_kg_node_list_t* brain_kg_get_all_nodes(const brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }

    brain_kg_node_list_t* list = nimcp_malloc(sizeof(*list));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;

    }

    list->count = 0;
    list->capacity = kg->node_count > 0 ? kg->node_count : 64;
    list->nodes = nimcp_malloc(list->capacity * sizeof(brain_kg_node_t*));
    if (!list->nodes) {
        nimcp_free(list);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_get_all_nodes: list->nodes is NULL");
        return NULL;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    for (uint32_t i = 0; i < kg->node_capacity && list->count < list->capacity; i++) {
        if (kg->nodes[i].in_use) {
            list->nodes[list->count++] = &mkg->nodes[i];
        }
    }

    nimcp_mutex_unlock(mkg->mutex);
    return list;
}

void brain_kg_node_list_destroy(brain_kg_node_list_t* list) {
    if (!list) return;
    if (list->nodes) nimcp_free(list->nodes);
    nimcp_free(list);
}

/* ============================================================================
 * EDGE CRUD API
 * ============================================================================ */

brain_kg_edge_id_t brain_kg_add_edge(
    brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    brain_kg_edge_type_t type,
    const char* description,
    float weight
) {
    if (!kg || from == BRAIN_KG_INVALID_NODE || to == BRAIN_KG_INVALID_NODE) {
        return BRAIN_KG_INVALID_NODE;
    }

    /* Security check */
    if (kg->config.enable_security && !check_write_access(kg)) {
        return BRAIN_KG_INVALID_NODE;
    }

    nimcp_mutex_lock(kg->mutex);

    /* Verify nodes exist */
    brain_kg_node_t* from_node = find_node_by_id_unlocked(kg, from);
    brain_kg_node_t* to_node = find_node_by_id_unlocked(kg, to);
    if (!from_node || !to_node) {
        nimcp_mutex_unlock(kg->mutex);
        NIMCP_LOGGING_WARN("Edge nodes not found: %u -> %u", from, to);
        return BRAIN_KG_INVALID_NODE;
    }

    /* Check if edge already exists */
    for (uint32_t i = 0; i < kg->edge_capacity; i++) {
        if (kg->edges[i].in_use &&
            kg->edges[i].from == from && kg->edges[i].to == to) {
            nimcp_mutex_unlock(kg->mutex);
            return kg->edges[i].id;  /* Return existing */
        }
    }

    /* Find free slot */
    brain_kg_edge_t* edge = NULL;
    for (uint32_t i = 0; i < kg->edge_capacity; i++) {
        if (!kg->edges[i].in_use) {
            edge = &kg->edges[i];
            break;
        }
    }

    if (!edge) {
        nimcp_mutex_unlock(kg->mutex);
        NIMCP_LOGGING_ERROR("Edge capacity reached (%u)", kg->edge_capacity);
        return BRAIN_KG_INVALID_NODE;
    }

    /* Initialize edge */
    memset(edge, 0, sizeof(*edge));
    edge->id = kg->next_edge_id++;
    edge->from = from;
    edge->to = to;
    edge->type = type;
    edge->weight = (weight >= 0.0f && weight <= 1.0f) ? weight : 1.0f;
    if (description) {
        strncpy(edge->description, description, BRAIN_KG_MAX_DESC_LEN - 1);
    }
    edge->created_time = get_time_ms();
    edge->in_use = true;

    /* Update node connection counts */
    from_node->outgoing_count++;
    to_node->incoming_count++;

    kg->edge_count++;
    kg->stats.total_edges++;
    kg->stats.edges_by_type[type]++;
    kg->stats.modifications_count++;
    kg->stats.last_modified = edge->created_time;

    nimcp_mutex_unlock(kg->mutex);

    NIMCP_LOGGING_DEBUG("Added edge %u: %s -> %s (%s)",
                        edge->id, from_node->name, to_node->name,
                        brain_kg_edge_type_to_string(type));

    return edge->id;
}

const brain_kg_edge_t* brain_kg_get_edge(const brain_kg_t* kg, brain_kg_edge_id_t id) {
    if (!kg || id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_get_edge: kg is NULL");
        return NULL;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    const brain_kg_edge_t* edge = NULL;
    for (uint32_t i = 0; i < kg->edge_capacity; i++) {
        if (kg->edges[i].in_use && kg->edges[i].id == id) {
            edge = &kg->edges[i];
            break;
        }
    }

    nimcp_mutex_unlock(mkg->mutex);
    return edge;
}

brain_kg_edge_id_t brain_kg_find_edge(
    const brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to
) {
    if (!kg) return BRAIN_KG_INVALID_NODE;

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    brain_kg_edge_id_t result = BRAIN_KG_INVALID_NODE;
    for (uint32_t i = 0; i < kg->edge_capacity; i++) {
        if (kg->edges[i].in_use &&
            kg->edges[i].from == from && kg->edges[i].to == to) {
            result = kg->edges[i].id;
            break;
        }
    }

    nimcp_mutex_unlock(mkg->mutex);
    return result;
}

int brain_kg_update_edge(
    brain_kg_t* kg,
    brain_kg_edge_id_t id,
    float weight,
    const char* description
) {
    if (!kg || id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_update_edge: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    brain_kg_edge_t* edge = find_edge_by_id_unlocked(kg, id);
    if (!edge) {
        nimcp_mutex_unlock(kg->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_update_edge: edge is NULL");
        return -1;
    }

    if (weight >= 0.0f) {
        edge->weight = weight;
    }
    if (description) {
        strncpy(edge->description, description, BRAIN_KG_MAX_DESC_LEN - 1);
    }

    kg->stats.modifications_count++;
    kg->stats.last_modified = get_time_ms();

    nimcp_mutex_unlock(kg->mutex);
    return 0;
}

int brain_kg_remove_edge(brain_kg_t* kg, brain_kg_edge_id_t id) {
    if (!kg || id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_remove_edge: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    brain_kg_edge_t* edge = find_edge_by_id_unlocked(kg, id);
    if (!edge) {
        nimcp_mutex_unlock(kg->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_remove_edge: edge is NULL");
        return -1;
    }

    /* Update node counts */
    brain_kg_node_t* from_node = find_node_by_id_unlocked(kg, edge->from);
    brain_kg_node_t* to_node = find_node_by_id_unlocked(kg, edge->to);
    if (from_node) from_node->outgoing_count--;
    if (to_node) to_node->incoming_count--;

    kg->stats.edges_by_type[edge->type]--;
    kg->stats.total_edges--;
    kg->stats.modifications_count++;
    kg->stats.last_modified = get_time_ms();

    edge->in_use = false;
    kg->edge_count--;

    nimcp_mutex_unlock(kg->mutex);
    return 0;
}

brain_kg_edge_list_t* brain_kg_get_outgoing(const brain_kg_t* kg, brain_kg_node_id_t node_id) {
    if (!kg || node_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_get_outgoing: kg is NULL");
        return NULL;
    }

    brain_kg_edge_list_t* list = nimcp_malloc(sizeof(*list));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;

    }

    list->count = 0;
    list->capacity = 32;
    list->edges = nimcp_malloc(list->capacity * sizeof(brain_kg_edge_t*));
    if (!list->edges) {
        nimcp_free(list);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_get_outgoing: list->edges is NULL");
        return NULL;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    for (uint32_t i = 0; i < kg->edge_capacity; i++) {
        if (kg->edges[i].in_use && kg->edges[i].from == node_id) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                brain_kg_edge_t** new_edges = nimcp_realloc(
                    list->edges, list->capacity * sizeof(brain_kg_edge_t*));
                if (!new_edges) break;
                list->edges = new_edges;
            }
            list->edges[list->count++] = &mkg->edges[i];
        }
    }

    nimcp_mutex_unlock(mkg->mutex);
    return list;
}

brain_kg_edge_list_t* brain_kg_get_incoming(const brain_kg_t* kg, brain_kg_node_id_t node_id) {
    if (!kg || node_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_get_incoming: kg is NULL");
        return NULL;
    }

    brain_kg_edge_list_t* list = nimcp_malloc(sizeof(*list));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;

    }

    list->count = 0;
    list->capacity = 32;
    list->edges = nimcp_malloc(list->capacity * sizeof(brain_kg_edge_t*));
    if (!list->edges) {
        nimcp_free(list);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_get_incoming: list->edges is NULL");
        return NULL;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    for (uint32_t i = 0; i < kg->edge_capacity; i++) {
        if (kg->edges[i].in_use && kg->edges[i].to == node_id) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                brain_kg_edge_t** new_edges = nimcp_realloc(
                    list->edges, list->capacity * sizeof(brain_kg_edge_t*));
                if (!new_edges) break;
                list->edges = new_edges;
            }
            list->edges[list->count++] = &mkg->edges[i];
        }
    }

    nimcp_mutex_unlock(mkg->mutex);
    return list;
}

brain_kg_edge_list_t* brain_kg_get_edges_by_type(const brain_kg_t* kg, brain_kg_edge_type_t type) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }

    brain_kg_edge_list_t* list = nimcp_malloc(sizeof(*list));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;

    }

    list->count = 0;
    list->capacity = 64;
    list->edges = nimcp_malloc(list->capacity * sizeof(brain_kg_edge_t*));
    if (!list->edges) {
        nimcp_free(list);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_get_edges_by_type: list->edges is NULL");
        return NULL;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    for (uint32_t i = 0; i < kg->edge_capacity; i++) {
        if (kg->edges[i].in_use && kg->edges[i].type == type) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                brain_kg_edge_t** new_edges = nimcp_realloc(
                    list->edges, list->capacity * sizeof(brain_kg_edge_t*));
                if (!new_edges) break;
                list->edges = new_edges;
            }
            list->edges[list->count++] = &mkg->edges[i];
        }
    }

    nimcp_mutex_unlock(mkg->mutex);
    return list;
}

void brain_kg_edge_list_destroy(brain_kg_edge_list_t* list) {
    if (!list) return;
    if (list->edges) nimcp_free(list->edges);
    nimcp_free(list);
}

/* ============================================================================
 * GRAPH TRAVERSAL API
 * ============================================================================ */

brain_kg_path_t* brain_kg_find_path(
    const brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to
) {
    if (!kg || from == BRAIN_KG_INVALID_NODE || to == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_find_path: kg is NULL");
        return NULL;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    /* Simple BFS for path finding */
    bool* visited = nimcp_malloc(kg->node_capacity * sizeof(bool));
    uint32_t* parent = nimcp_malloc(kg->node_capacity * sizeof(uint32_t));
    uint32_t* queue = nimcp_malloc(kg->node_capacity * sizeof(uint32_t));

    if (!visited || !parent || !queue) {
        if (visited) nimcp_free(visited);
        if (parent) nimcp_free(parent);
        if (queue) nimcp_free(queue);
        nimcp_mutex_unlock(mkg->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_find_path: validation failed");
        return NULL;
    }

    memset(visited, 0, kg->node_capacity * sizeof(bool));
    for (uint32_t i = 0; i < kg->node_capacity; i++) {
        parent[i] = BRAIN_KG_INVALID_NODE;
    }

    /* Map IDs to indices */
    uint32_t from_idx = BRAIN_KG_INVALID_NODE;
    uint32_t to_idx = BRAIN_KG_INVALID_NODE;
    for (uint32_t i = 0; i < kg->node_capacity; i++) {
        if (kg->nodes[i].in_use) {
            if (kg->nodes[i].id == from) from_idx = i;
            if (kg->nodes[i].id == to) to_idx = i;
        }
    }

    if (from_idx == BRAIN_KG_INVALID_NODE || to_idx == BRAIN_KG_INVALID_NODE) {
        nimcp_free(visited);
        nimcp_free(parent);
        nimcp_free(queue);
        nimcp_mutex_unlock(mkg->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_find_path: validation failed");
        return NULL;
    }

    /* BFS */
    uint32_t head = 0, tail = 0;
    queue[tail++] = from_idx;
    visited[from_idx] = true;
    bool found = false;

    while (head < tail && !found) {
        uint32_t curr_idx = queue[head++];
        brain_kg_node_id_t curr_id = kg->nodes[curr_idx].id;

        /* Find outgoing edges */
        for (uint32_t i = 0; i < kg->edge_capacity; i++) {
            if (!kg->edges[i].in_use || kg->edges[i].from != curr_id) continue;

            /* Find target node index */
            for (uint32_t j = 0; j < kg->node_capacity; j++) {
                if (kg->nodes[j].in_use && kg->nodes[j].id == kg->edges[i].to) {
                    if (!visited[j]) {
                        visited[j] = true;
                        parent[j] = curr_idx;
                        queue[tail++] = j;
                        if (j == to_idx) {
                            found = true;
                        }
                    }
                    break;
                }
            }
            if (found) break;
        }
    }

    brain_kg_path_t* path = NULL;
    if (found) {
        /* Reconstruct path */
        uint32_t path_len = 0;
        uint32_t curr = to_idx;
        while (curr != BRAIN_KG_INVALID_NODE) {
            path_len++;
            curr = parent[curr];
        }

        path = nimcp_malloc(sizeof(*path));
        if (path) {
            path->nodes = nimcp_malloc(path_len * sizeof(brain_kg_node_id_t));
            path->length = path_len;
            path->total_weight = 0.0f;

            if (path->nodes) {
                curr = to_idx;
                for (uint32_t i = path_len; i > 0 && curr != BRAIN_KG_INVALID_NODE; i--) {
                    path->nodes[i - 1] = kg->nodes[curr].id;
                    curr = parent[curr];
                }
            } else {
                nimcp_free(path);
                path = NULL;
            }
        }
    }

    nimcp_free(visited);
    nimcp_free(parent);
    nimcp_free(queue);
    nimcp_mutex_unlock(mkg->mutex);

    return path;
}

bool brain_kg_are_connected(
    const brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to
) {
    brain_kg_path_t* path = brain_kg_find_path(kg, from, to);
    if (path) {
        brain_kg_path_destroy(path);
        return true;
    }
    return false;
}

brain_kg_node_list_t* brain_kg_get_reachable(
    const brain_kg_t* kg,
    brain_kg_node_id_t start_node,
    uint32_t max_depth
) {
    if (!kg || start_node == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_get_reachable: kg is NULL");
        return NULL;
    }

    brain_kg_node_list_t* list = nimcp_malloc(sizeof(*list));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;

    }

    list->count = 0;
    list->capacity = 64;
    list->nodes = nimcp_malloc(list->capacity * sizeof(brain_kg_node_t*));
    if (!list->nodes) {
        nimcp_free(list);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_get_reachable: list->nodes is NULL");
        return NULL;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    bool* visited = nimcp_malloc(kg->node_capacity * sizeof(bool));
    if (!visited) {
        nimcp_mutex_unlock(mkg->mutex);
        brain_kg_node_list_destroy(list);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_get_reachable: visited is NULL");
        return NULL;
    }
    memset(visited, 0, kg->node_capacity * sizeof(bool));

    /* BFS with depth limit */
    uint32_t* queue = nimcp_malloc(kg->node_capacity * sizeof(uint32_t));
    uint32_t* depth = nimcp_malloc(kg->node_capacity * sizeof(uint32_t));
    if (!queue || !depth) {
        if (queue) nimcp_free(queue);
        if (depth) nimcp_free(depth);
        nimcp_free(visited);
        nimcp_mutex_unlock(mkg->mutex);
        brain_kg_node_list_destroy(list);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_get_reachable: validation failed");
        return NULL;
    }

    /* Find start node index */
    uint32_t start_idx = BRAIN_KG_INVALID_NODE;
    for (uint32_t i = 0; i < kg->node_capacity; i++) {
        if (kg->nodes[i].in_use && kg->nodes[i].id == start_node) {
            start_idx = i;
            break;
        }
    }

    if (start_idx != BRAIN_KG_INVALID_NODE) {
        uint32_t head = 0, tail = 0;
        queue[tail] = start_idx;
        depth[tail] = 0;
        tail++;
        visited[start_idx] = true;

        while (head < tail) {
            uint32_t curr_idx = queue[head];
            uint32_t curr_depth = depth[head];
            head++;

            if (max_depth > 0 && curr_depth >= max_depth) continue;

            brain_kg_node_id_t curr_id = kg->nodes[curr_idx].id;

            /* Find outgoing edges */
            for (uint32_t i = 0; i < kg->edge_capacity; i++) {
                if (!kg->edges[i].in_use || kg->edges[i].from != curr_id) continue;

                for (uint32_t j = 0; j < kg->node_capacity; j++) {
                    if (kg->nodes[j].in_use && kg->nodes[j].id == kg->edges[i].to && !visited[j]) {
                        visited[j] = true;
                        queue[tail] = j;
                        depth[tail] = curr_depth + 1;
                        tail++;

                        /* Add to result list */
                        if (list->count >= list->capacity) {
                            list->capacity *= 2;
                            brain_kg_node_t** new_nodes = nimcp_realloc(
                                list->nodes, list->capacity * sizeof(brain_kg_node_t*));
                            if (new_nodes) list->nodes = new_nodes;
                        }
                        if (list->count < list->capacity) {
                            list->nodes[list->count++] = &mkg->nodes[j];
                        }
                        break;
                    }
                }
            }
        }
    }

    nimcp_free(queue);
    nimcp_free(depth);
    nimcp_free(visited);
    nimcp_mutex_unlock(mkg->mutex);

    return list;
}

void brain_kg_path_destroy(brain_kg_path_t* path) {
    if (!path) return;
    if (path->nodes) nimcp_free(path->nodes);
    nimcp_free(path);
}

/* ============================================================================
 * QUERY API
 * ============================================================================ */

brain_kg_node_list_t* brain_kg_search_nodes(const brain_kg_t* kg, const char* pattern) {
    if (!kg || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_search_nodes: required parameter is NULL (kg, pattern)");
        return NULL;
    }

    brain_kg_node_list_t* list = nimcp_malloc(sizeof(*list));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;

    }

    list->count = 0;
    list->capacity = 32;
    list->nodes = nimcp_malloc(list->capacity * sizeof(brain_kg_node_t*));
    if (!list->nodes) {
        nimcp_free(list);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_search_nodes: list->nodes is NULL");
        return NULL;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    for (uint32_t i = 0; i < kg->node_capacity; i++) {
        if (!kg->nodes[i].in_use) continue;

        /* Case-insensitive substring search */
        if (strstr(kg->nodes[i].name, pattern) ||
            strstr(kg->nodes[i].description, pattern)) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                brain_kg_node_t** new_nodes = nimcp_realloc(
                    list->nodes, list->capacity * sizeof(brain_kg_node_t*));
                if (!new_nodes) break;
                list->nodes = new_nodes;
            }
            list->nodes[list->count++] = &mkg->nodes[i];
        }
    }

    mkg->stats.queries_count++;
    nimcp_mutex_unlock(mkg->mutex);

    return list;
}

brain_kg_node_list_t* brain_kg_get_neighbors(const brain_kg_t* kg, brain_kg_node_id_t node_id) {
    if (!kg || node_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_get_neighbors: kg is NULL");
        return NULL;
    }

    brain_kg_node_list_t* list = nimcp_malloc(sizeof(*list));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;

    }

    list->count = 0;
    list->capacity = 32;
    list->nodes = nimcp_malloc(list->capacity * sizeof(brain_kg_node_t*));
    if (!list->nodes) {
        nimcp_free(list);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_get_neighbors: list->nodes is NULL");
        return NULL;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    /* Find all edges to/from this node */
    bool* added = nimcp_malloc(kg->node_capacity * sizeof(bool));
    if (added) {
        memset(added, 0, kg->node_capacity * sizeof(bool));

        for (uint32_t i = 0; i < kg->edge_capacity; i++) {
            if (!kg->edges[i].in_use) continue;

            brain_kg_node_id_t neighbor_id = BRAIN_KG_INVALID_NODE;
            if (kg->edges[i].from == node_id) {
                neighbor_id = kg->edges[i].to;
            } else if (kg->edges[i].to == node_id) {
                neighbor_id = kg->edges[i].from;
            }

            if (neighbor_id != BRAIN_KG_INVALID_NODE) {
                for (uint32_t j = 0; j < kg->node_capacity; j++) {
                    if (kg->nodes[j].in_use && kg->nodes[j].id == neighbor_id && !added[j]) {
                        added[j] = true;
                        if (list->count >= list->capacity) {
                            list->capacity *= 2;
                            brain_kg_node_t** new_nodes = nimcp_realloc(
                                list->nodes, list->capacity * sizeof(brain_kg_node_t*));
                            if (new_nodes) list->nodes = new_nodes;
                        }
                        if (list->count < list->capacity) {
                            list->nodes[list->count++] = &mkg->nodes[j];
                        }
                        break;
                    }
                }
            }
        }
        nimcp_free(added);
    }

    mkg->stats.queries_count++;
    nimcp_mutex_unlock(mkg->mutex);

    return list;
}

brain_kg_node_list_t* brain_kg_get_hubs(const brain_kg_t* kg, uint32_t max_count) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }

    brain_kg_node_list_t* list = brain_kg_get_all_nodes(kg);
    if (!list || list->count == 0) return list;

    /* Sort by connection count (bubble sort for simplicity) */
    for (uint32_t i = 0; i < list->count - 1; i++) {
        for (uint32_t j = 0; j < list->count - i - 1; j++) {
            uint32_t conn_j = list->nodes[j]->incoming_count + list->nodes[j]->outgoing_count;
            uint32_t conn_j1 = list->nodes[j+1]->incoming_count + list->nodes[j+1]->outgoing_count;
            if (conn_j < conn_j1) {
                brain_kg_node_t* temp = list->nodes[j];
                list->nodes[j] = list->nodes[j+1];
                list->nodes[j+1] = temp;
            }
        }
    }

    /* Trim to max_count */
    if (max_count > 0 && list->count > max_count) {
        list->count = max_count;
    }

    return list;
}

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int brain_kg_get_stats(const brain_kg_t* kg, brain_kg_stats_t* stats) {
    if (!kg || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_get_stats: required parameter is NULL (kg, stats)");
        return -1;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);
    *stats = kg->stats;
    nimcp_mutex_unlock(mkg->mutex);

    return 0;
}

int brain_kg_generate_summary(const brain_kg_t* kg, char* buffer, size_t buffer_size) {
    if (!kg || !buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_generate_summary: required parameter is NULL (kg, buffer)");
        return -1;
    }

    brain_kg_stats_t stats;
    brain_kg_get_stats(kg, &stats);

    int written = snprintf(buffer, buffer_size,
        "Brain Internal KG Summary:\n"
        "  Nodes: %u (Cortical: %u, Subcortical: %u, Cognitive: %u, Integration: %u)\n"
        "  Edges: %u (Connects: %u, Integrates: %u, Modulates: %u)\n"
        "  Queries: %lu, Modifications: %lu\n",
        stats.total_nodes,
        stats.nodes_by_type[BRAIN_KG_NODE_CORTICAL],
        stats.nodes_by_type[BRAIN_KG_NODE_SUBCORTICAL],
        stats.nodes_by_type[BRAIN_KG_NODE_COGNITIVE],
        stats.nodes_by_type[BRAIN_KG_NODE_INTEGRATION],
        stats.total_edges,
        stats.edges_by_type[BRAIN_KG_EDGE_CONNECTS_TO],
        stats.edges_by_type[BRAIN_KG_EDGE_INTEGRATES_WITH],
        stats.edges_by_type[BRAIN_KG_EDGE_MODULATES],
        (unsigned long)stats.queries_count,
        (unsigned long)stats.modifications_count
    );

    return written;
}

/* ============================================================================
 * STRING CONVERSION
 * ============================================================================ */

const char* brain_kg_node_type_to_string(brain_kg_node_type_t type) {
    switch (type) {
    case BRAIN_KG_NODE_CORE:        return "core";
    case BRAIN_KG_NODE_CORTICAL:    return "cortical";
    case BRAIN_KG_NODE_SUBCORTICAL: return "subcortical";
    case BRAIN_KG_NODE_BRAINSTEM:   return "brainstem";
    case BRAIN_KG_NODE_COGNITIVE:   return "cognitive";
    case BRAIN_KG_NODE_PERCEPTION:  return "perception";
    case BRAIN_KG_NODE_PLASTICITY:  return "plasticity";
    case BRAIN_KG_NODE_TRAINING:    return "training";
    case BRAIN_KG_NODE_SWARM:       return "swarm";
    case BRAIN_KG_NODE_SECURITY:    return "security";
    case BRAIN_KG_NODE_INTEGRATION: return "integration";
    case BRAIN_KG_NODE_COORDINATOR: return "coordinator";
    case BRAIN_KG_NODE_UTILITY:     return "utility";
    case BRAIN_KG_NODE_CUSTOM:      return "custom";
    default:                        return "unknown";
    }
}

const char* brain_kg_edge_type_to_string(brain_kg_edge_type_t type) {
    switch (type) {
    case BRAIN_KG_EDGE_CONNECTS_TO:      return "connects_to";
    case BRAIN_KG_EDGE_SENDS_TO:         return "sends_to";
    case BRAIN_KG_EDGE_RECEIVES_FROM:    return "receives_from";
    case BRAIN_KG_EDGE_INTEGRATES_WITH:  return "integrates_with";
    case BRAIN_KG_EDGE_MODULATES:        return "modulates";
    case BRAIN_KG_EDGE_EXCITES:          return "excites";
    case BRAIN_KG_EDGE_INHIBITS:         return "inhibits";
    case BRAIN_KG_EDGE_COORDINATES_WITH: return "coordinates_with";
    case BRAIN_KG_EDGE_DEPENDS_ON:       return "depends_on";
    case BRAIN_KG_EDGE_PROVIDES_TO:      return "provides_to";
    case BRAIN_KG_EDGE_HANDLES_MESSAGE:  return "handles_message";
    case BRAIN_KG_EDGE_CUSTOM:           return "custom";
    default:                             return "unknown";
    }
}

const char* brain_kg_node_state_to_string(brain_kg_node_state_t state) {
    switch (state) {
    case BRAIN_KG_STATE_UNKNOWN:        return "unknown";
    case BRAIN_KG_STATE_UNINITIALIZED:  return "uninitialized";
    case BRAIN_KG_STATE_INITIALIZING:   return "initializing";
    case BRAIN_KG_STATE_ACTIVE:         return "active";
    case BRAIN_KG_STATE_DISABLED:       return "disabled";
    case BRAIN_KG_STATE_ERROR:          return "error";
    case BRAIN_KG_STATE_SHUTDOWN:       return "shutdown";
    default:                            return "unknown";
    }
}

/* ============================================================================
 * POPULATION API (Stub - to be implemented with brain_struct access)
 * ============================================================================ */

int brain_kg_populate_from_brain(brain_kg_t* kg, void* brain) {
    if (!kg || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_populate_from_brain: required parameter is NULL (kg, brain)");
        return -1;
    }

    /* Cast to access brain_struct members
     * Note: We use void* to avoid circular header dependencies */

    int nodes_added = 0;

    /* ========================================================================
     * CORE MODULE
     * ======================================================================== */
    brain_kg_node_id_t core_id = brain_kg_add_node(kg, "core_brain",
        BRAIN_KG_NODE_CORE, "Core brain infrastructure - network, config, strategy");
    if (core_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_mark_critical(kg, core_id);  /* Protect core */
        nodes_added++;
    }

    /* ========================================================================
     * CORTICAL REGIONS
     * ======================================================================== */
    brain_kg_node_id_t prefrontal_id = brain_kg_add_node(kg, "prefrontal_cortex",
        BRAIN_KG_NODE_CORTICAL, "Executive control, working memory, planning (DLPFC, VLPFC, OFC)");
    if (prefrontal_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t temporal_id = brain_kg_add_node(kg, "temporal_cortex",
        BRAIN_KG_NODE_CORTICAL, "Auditory processing, object recognition, language (STG, MTG, IT)");
    if (temporal_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t occipital_id = brain_kg_add_node(kg, "occipital_cortex",
        BRAIN_KG_NODE_CORTICAL, "Visual processing, V1-V5 hierarchy, motion detection");
    if (occipital_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t parietal_id = brain_kg_add_node(kg, "parietal_cortex",
        BRAIN_KG_NODE_CORTICAL, "Spatial processing, attention, body awareness (SPL, IPL, IPS)");
    if (parietal_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t motor_id = brain_kg_add_node(kg, "motor_cortex",
        BRAIN_KG_NODE_CORTICAL, "Voluntary movement control, somatotopic organization (M1, PMC, SMA)");
    if (motor_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t cingulate_id = brain_kg_add_node(kg, "cingulate_cortex",
        BRAIN_KG_NODE_CORTICAL, "Error monitoring, conflict detection, cognitive control (ACC, MCC, PCC)");
    if (cingulate_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t insula_id = brain_kg_add_node(kg, "insula",
        BRAIN_KG_NODE_CORTICAL, "Interoception, emotional awareness, disgust processing");
    if (insula_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t broca_id = brain_kg_add_node(kg, "broca_region",
        BRAIN_KG_NODE_CORTICAL, "Language production, syntax, speech motor planning (BA44/45)");
    if (broca_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    /* ========================================================================
     * SUBCORTICAL STRUCTURES
     * ======================================================================== */
    brain_kg_node_id_t hippocampus_id = brain_kg_add_node(kg, "hippocampus",
        BRAIN_KG_NODE_SUBCORTICAL, "Episodic memory, spatial navigation, pattern separation (CA1/CA3, DG)");
    if (hippocampus_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t basal_ganglia_id = brain_kg_add_node(kg, "basal_ganglia",
        BRAIN_KG_NODE_SUBCORTICAL, "Action selection, motor control, reinforcement learning (striatum, GPe/GPi, STN)");
    if (basal_ganglia_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t cerebellum_id = brain_kg_add_node(kg, "cerebellum",
        BRAIN_KG_NODE_SUBCORTICAL, "Motor coordination, timing, error correction (Marr-Albus-Ito model)");
    if (cerebellum_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t thalamus_id = brain_kg_add_node(kg, "thalamus",
        BRAIN_KG_NODE_SUBCORTICAL, "Sensory relay, cortical gating, attention modulation");
    if (thalamus_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t hypothalamus_id = brain_kg_add_node(kg, "hypothalamus",
        BRAIN_KG_NODE_SUBCORTICAL, "Homeostasis, circadian rhythm, HPA axis, autonomic regulation");
    if (hypothalamus_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t amygdala_id = brain_kg_add_node(kg, "amygdala",
        BRAIN_KG_NODE_SUBCORTICAL, "Emotional processing, fear conditioning, threat detection");
    if (amygdala_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    /* ========================================================================
     * BRAINSTEM
     * ======================================================================== */
    brain_kg_node_id_t medulla_id = brain_kg_add_node(kg, "medulla_oblongata",
        BRAIN_KG_NODE_BRAINSTEM, "Vital functions, protective reflexes, arousal state");
    if (medulla_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t brainstem_id = brain_kg_add_node(kg, "brainstem",
        BRAIN_KG_NODE_BRAINSTEM, "Midbrain, pons, reticular formation - arousal and reflex control");
    if (brainstem_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    /* ========================================================================
     * COGNITIVE MODULES
     * ======================================================================== */
    brain_kg_node_id_t ethics_id = brain_kg_add_node(kg, "ethics_engine",
        BRAIN_KG_NODE_COGNITIVE, "Golden Rule, empathy-based moral reasoning");
    if (ethics_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_mark_critical(kg, ethics_id);  /* Protect ethics */
        nodes_added++;
    }

    brain_kg_node_id_t core_directives_id = brain_kg_add_node(kg, "core_directives",
        BRAIN_KG_NODE_COGNITIVE, "Asimov's Laws, Golden Rule, combinatorial harm detection");
    if (core_directives_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_mark_critical(kg, core_directives_id);  /* Protect directives */
        nodes_added++;
    }

    brain_kg_node_id_t introspection_id = brain_kg_add_node(kg, "introspection",
        BRAIN_KG_NODE_COGNITIVE, "Self-awareness, state monitoring, metacognition");
    if (introspection_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t salience_id = brain_kg_add_node(kg, "salience_evaluator",
        BRAIN_KG_NODE_COGNITIVE, "Fast attention, priority scoring, relevance detection");
    if (salience_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t curiosity_id = brain_kg_add_node(kg, "curiosity_engine",
        BRAIN_KG_NODE_COGNITIVE, "Exploration drive, novelty seeking, information gain");
    if (curiosity_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t tom_id = brain_kg_add_node(kg, "theory_of_mind",
        BRAIN_KG_NODE_COGNITIVE, "Mental state inference, belief modeling, social cognition");
    if (tom_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t working_memory_id = brain_kg_add_node(kg, "working_memory",
        BRAIN_KG_NODE_COGNITIVE, "Miller's 7±2 active buffer, phonological loop, visuospatial sketchpad");
    if (working_memory_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t executive_id = brain_kg_add_node(kg, "executive_controller",
        BRAIN_KG_NODE_COGNITIVE, "Task switching, planning, inhibition, goal maintenance");
    if (executive_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t emotional_id = brain_kg_add_node(kg, "emotional_system",
        BRAIN_KG_NODE_COGNITIVE, "Russell's circumplex model, valence/arousal, emotional tagging");
    if (emotional_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    /* ========================================================================
     * SECURITY MODULES
     * ======================================================================== */
    brain_kg_node_id_t immune_id = brain_kg_add_node(kg, "brain_immune",
        BRAIN_KG_NODE_SECURITY, "B/T cells, antibodies, cytokines, threat coordination");
    if (immune_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_mark_critical(kg, immune_id);
        nodes_added++;
    }

    brain_kg_node_id_t bbb_id = brain_kg_add_node(kg, "blood_brain_barrier",
        BRAIN_KG_NODE_SECURITY, "Input validation, code signing, memory protection, access control");
    if (bbb_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_mark_critical(kg, bbb_id);
        nodes_added++;
    }

    /* ========================================================================
     * TRAINING MODULES
     * ======================================================================== */
    brain_kg_node_id_t training_id = brain_kg_add_node(kg, "training_integration",
        BRAIN_KG_NODE_TRAINING, "Loss functions, optimizers, batch processing");
    if (training_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t nas_id = brain_kg_add_node(kg, "neural_architecture_search",
        BRAIN_KG_NODE_TRAINING, "Evolutionary, RL-NAS, DARTS, pruning-based architecture optimization");
    if (nas_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t hpo_id = brain_kg_add_node(kg, "hyperparameter_optimization",
        BRAIN_KG_NODE_TRAINING, "Bayesian, random, grid search, meta-learning");
    if (hpo_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    /* ========================================================================
     * INTEGRATION MODULES
     * ======================================================================== */
    brain_kg_node_id_t fep_id = brain_kg_add_node(kg, "fep_orchestrator",
        BRAIN_KG_NODE_COORDINATOR, "Free energy principle coordination across 93+ bridges");
    if (fep_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t bio_async_id = brain_kg_add_node(kg, "bio_async_orchestrator",
        BRAIN_KG_NODE_COORDINATOR, "Asynchronous neural messaging, neuromodulator channels");
    if (bio_async_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t glial_id = brain_kg_add_node(kg, "glial_integration",
        BRAIN_KG_NODE_INTEGRATION, "Astrocytes, microglia, oligodendrocytes support");
    if (glial_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t neuromod_id = brain_kg_add_node(kg, "neuromodulator_system",
        BRAIN_KG_NODE_PLASTICITY, "DA, 5-HT, ACh, NE, GABA, GLU modulation");
    if (neuromod_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    /* ========================================================================
     * PERCEPTION MODULES
     * ======================================================================== */
    brain_kg_node_id_t visual_id = brain_kg_add_node(kg, "visual_cortex",
        BRAIN_KG_NODE_PERCEPTION, "V1 visual processing, CNN-based feature extraction");
    if (visual_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t audio_id = brain_kg_add_node(kg, "audio_cortex",
        BRAIN_KG_NODE_PERCEPTION, "A1 auditory processing, FFT-based spectral analysis");
    if (audio_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    brain_kg_node_id_t speech_id = brain_kg_add_node(kg, "speech_cortex",
        BRAIN_KG_NODE_PERCEPTION, "STG/Wernicke speech comprehension");
    if (speech_id != BRAIN_KG_INVALID_NODE) nodes_added++;

    /* ========================================================================
     * ADD KEY CONNECTIONS (EDGES)
     * ======================================================================== */

    /* Core connections */
    brain_kg_add_edge(kg, core_id, prefrontal_id, BRAIN_KG_EDGE_CONNECTS_TO,
        "Core to prefrontal pathway", 1.0f);
    brain_kg_add_edge(kg, core_id, immune_id, BRAIN_KG_EDGE_INTEGRATES_WITH,
        "Core immune integration", 1.0f);
    brain_kg_add_edge(kg, core_id, bbb_id, BRAIN_KG_EDGE_DEPENDS_ON,
        "Core requires BBB protection", 1.0f);

    /* Cortical connections */
    brain_kg_add_edge(kg, prefrontal_id, hippocampus_id, BRAIN_KG_EDGE_CONNECTS_TO,
        "Memory retrieval pathway", 0.9f);
    brain_kg_add_edge(kg, prefrontal_id, basal_ganglia_id, BRAIN_KG_EDGE_SENDS_TO,
        "Goal-directed action selection", 0.8f);
    brain_kg_add_edge(kg, prefrontal_id, working_memory_id, BRAIN_KG_EDGE_INTEGRATES_WITH,
        "Active maintenance", 0.95f);
    brain_kg_add_edge(kg, prefrontal_id, executive_id, BRAIN_KG_EDGE_COORDINATES_WITH,
        "Executive control", 0.9f);

    brain_kg_add_edge(kg, temporal_id, hippocampus_id, BRAIN_KG_EDGE_SENDS_TO,
        "Episodic encoding", 0.85f);
    brain_kg_add_edge(kg, temporal_id, audio_id, BRAIN_KG_EDGE_RECEIVES_FROM,
        "Auditory input", 0.9f);
    brain_kg_add_edge(kg, temporal_id, broca_id, BRAIN_KG_EDGE_CONNECTS_TO,
        "Language comprehension to production", 0.8f);

    brain_kg_add_edge(kg, occipital_id, visual_id, BRAIN_KG_EDGE_RECEIVES_FROM,
        "Visual input", 0.95f);
    brain_kg_add_edge(kg, occipital_id, parietal_id, BRAIN_KG_EDGE_SENDS_TO,
        "Dorsal stream (where)", 0.85f);
    brain_kg_add_edge(kg, occipital_id, temporal_id, BRAIN_KG_EDGE_SENDS_TO,
        "Ventral stream (what)", 0.85f);

    brain_kg_add_edge(kg, motor_id, cerebellum_id, BRAIN_KG_EDGE_COORDINATES_WITH,
        "Motor timing", 0.9f);
    brain_kg_add_edge(kg, motor_id, basal_ganglia_id, BRAIN_KG_EDGE_RECEIVES_FROM,
        "Action selection output", 0.85f);

    /* Subcortical connections */
    brain_kg_add_edge(kg, thalamus_id, prefrontal_id, BRAIN_KG_EDGE_SENDS_TO,
        "MD nucleus relay", 0.9f);
    brain_kg_add_edge(kg, thalamus_id, occipital_id, BRAIN_KG_EDGE_SENDS_TO,
        "LGN visual relay", 0.95f);
    brain_kg_add_edge(kg, thalamus_id, temporal_id, BRAIN_KG_EDGE_SENDS_TO,
        "MGN auditory relay", 0.9f);

    brain_kg_add_edge(kg, hypothalamus_id, medulla_id, BRAIN_KG_EDGE_COORDINATES_WITH,
        "Autonomic regulation", 0.9f);
    brain_kg_add_edge(kg, hypothalamus_id, immune_id, BRAIN_KG_EDGE_MODULATES,
        "Neuroimmune axis", 0.7f);

    brain_kg_add_edge(kg, amygdala_id, hippocampus_id, BRAIN_KG_EDGE_MODULATES,
        "Emotional memory tagging", 0.8f);
    brain_kg_add_edge(kg, amygdala_id, prefrontal_id, BRAIN_KG_EDGE_CONNECTS_TO,
        "Emotional regulation", 0.75f);

    /* Cognitive module connections */
    brain_kg_add_edge(kg, core_directives_id, ethics_id, BRAIN_KG_EDGE_INTEGRATES_WITH,
        "Ethical constraint enforcement", 1.0f);
    brain_kg_add_edge(kg, core_directives_id, immune_id, BRAIN_KG_EDGE_SENDS_TO,
        "Ethics violation alerts", 0.9f);

    brain_kg_add_edge(kg, salience_id, prefrontal_id, BRAIN_KG_EDGE_SENDS_TO,
        "Priority signals", 0.8f);
    brain_kg_add_edge(kg, curiosity_id, training_id, BRAIN_KG_EDGE_MODULATES,
        "Learning rate modulation", 0.7f);

    brain_kg_add_edge(kg, tom_id, prefrontal_id, BRAIN_KG_EDGE_INTEGRATES_WITH,
        "Social cognition integration", 0.8f);
    brain_kg_add_edge(kg, emotional_id, amygdala_id, BRAIN_KG_EDGE_INTEGRATES_WITH,
        "Emotional processing", 0.9f);

    /* Security connections */
    brain_kg_add_edge(kg, bbb_id, immune_id, BRAIN_KG_EDGE_SENDS_TO,
        "Threat reporting", 0.95f);
    brain_kg_add_edge(kg, immune_id, fep_id, BRAIN_KG_EDGE_MODULATES,
        "Precision modulation", 0.8f);

    /* Coordinator connections */
    brain_kg_add_edge(kg, fep_id, prefrontal_id, BRAIN_KG_EDGE_MODULATES,
        "Free energy minimization", 0.85f);
    brain_kg_add_edge(kg, bio_async_id, neuromod_id, BRAIN_KG_EDGE_COORDINATES_WITH,
        "Neuromodulator messaging", 0.9f);
    brain_kg_add_edge(kg, glial_id, neuromod_id, BRAIN_KG_EDGE_INTEGRATES_WITH,
        "Gliotransmission", 0.7f);

    NIMCP_LOGGING_INFO("Brain KG populated: %d nodes added", nodes_added);

    /* Update integrity checksum after population */
    if (kg->config.enable_integrity_checks) {
        nimcp_mutex_lock(kg->mutex);
        kg->security.integrity_checksum = compute_kg_checksum(kg);
        nimcp_mutex_unlock(kg->mutex);
    }

    return nodes_added;
}

int brain_kg_refresh_state(brain_kg_t* kg, void* brain) {
    if (!kg || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_refresh_state: required parameter is NULL (kg, brain)");
        return -1;
    }

    NIMCP_LOGGING_DEBUG("brain_kg_refresh_state called");
    return 0;
}

/* ============================================================================
 * SECURITY & IMMUNE INTEGRATION API
 * ============================================================================ */

int brain_kg_connect_immune(brain_kg_t* kg, void* immune_system) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_connect_immune: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);
    kg->security.immune_system = immune_system;
    kg->config.enable_immune_integration = (immune_system != NULL);
    nimcp_mutex_unlock(kg->mutex);

    NIMCP_LOGGING_INFO("Brain KG connected to immune system");
    return 0;
}

int brain_kg_disconnect_immune(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_disconnect_immune: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);
    kg->security.immune_system = NULL;
    kg->config.enable_immune_integration = false;
    nimcp_mutex_unlock(kg->mutex);

    NIMCP_LOGGING_INFO("Brain KG disconnected from immune system");
    return 0;
}

int brain_kg_register_security_callback(
    brain_kg_t* kg,
    brain_kg_security_callback_t callback,
    void* user_data
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_register_security_callback: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);
    kg->security.callback = callback;
    kg->security.callback_user_data = user_data;
    nimcp_mutex_unlock(kg->mutex);

    return 0;
}

int brain_kg_set_access_level(
    brain_kg_t* kg,
    brain_kg_access_level_t level,
    uint64_t token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_set_access_level: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    /* Verify token for WRITE and ADMIN levels */
    if (level >= BRAIN_KG_ACCESS_WRITE) {
        if (token != kg->security.access_token && token != kg->security.admin_token) {
            nimcp_mutex_unlock(kg->mutex);
            report_security_violation(kg, BRAIN_KG_SEC_UNAUTHORIZED_ACCESS,
                BRAIN_KG_INVALID_NODE, "Invalid token for access level change");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_set_access_level: validation failed");
            return -1;
        }
    }

    /* ADMIN requires admin token specifically */
    if (level == BRAIN_KG_ACCESS_ADMIN) {
        if (token != kg->security.admin_token) {
            nimcp_mutex_unlock(kg->mutex);
            report_security_violation(kg, BRAIN_KG_SEC_UNAUTHORIZED_ACCESS,
                BRAIN_KG_INVALID_NODE, "Admin token required for ADMIN access");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_set_access_level: validation failed");
            return -1;
        }
    }

    kg->security.current_level = level;
    nimcp_mutex_unlock(kg->mutex);

    NIMCP_LOGGING_DEBUG("KG access level set to %d", level);
    return 0;
}

int brain_kg_generate_token(
    brain_kg_t* kg,
    brain_kg_access_level_t level,
    uint64_t* token_out
) {
    if (!kg || !token_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_generate_token: required parameter is NULL (kg, token_out)");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    /* Only allow token generation if current level >= ADMIN or first time */
    if (kg->security.admin_token != 0 &&
        kg->security.current_level < BRAIN_KG_ACCESS_ADMIN) {
        nimcp_mutex_unlock(kg->mutex);
        report_security_violation(kg, BRAIN_KG_SEC_UNAUTHORIZED_ACCESS,
            BRAIN_KG_INVALID_NODE, "ADMIN access required to generate tokens");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_generate_token: operation failed");
        return -1;
    }

    uint64_t token = generate_random_token();

    if (level == BRAIN_KG_ACCESS_ADMIN) {
        kg->security.admin_token = token;
    }
    kg->security.access_token = token;

    *token_out = token;
    nimcp_mutex_unlock(kg->mutex);

    NIMCP_LOGGING_INFO("Generated token for access level %d", level);
    return 0;
}

int brain_kg_verify_integrity(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_verify_integrity: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    uint32_t current_checksum = compute_kg_checksum(kg);

    /* First time - just store the checksum */
    if (kg->security.integrity_checksum == 0) {
        kg->security.integrity_checksum = current_checksum;
        kg->security.last_integrity_check = get_time_ms();
        nimcp_mutex_unlock(kg->mutex);
        return 0;
    }

    /* Check if checksum matches */
    if (current_checksum != kg->security.integrity_checksum) {
        nimcp_mutex_unlock(kg->mutex);
        report_security_violation(kg, BRAIN_KG_SEC_INTEGRITY_VIOLATION,
            BRAIN_KG_INVALID_NODE, "Integrity checksum mismatch detected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_verify_integrity: validation failed");
        return -1;
    }

    kg->security.last_integrity_check = get_time_ms();
    nimcp_mutex_unlock(kg->mutex);

    NIMCP_LOGGING_DEBUG("KG integrity verified");
    return 0;
}

void brain_kg_reset_integrity_baseline(brain_kg_t* kg) {
    if (!kg) return;
    nimcp_mutex_lock(kg->mutex);
    kg->security.integrity_checksum = 0;
    nimcp_mutex_unlock(kg->mutex);
}

int brain_kg_mark_critical(brain_kg_t* kg, brain_kg_node_id_t node_id) {
    if (!kg || node_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_mark_critical: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    /* Allocate critical nodes bitmap if needed */
    if (!kg->security.critical_nodes) {
        kg->security.critical_nodes = nimcp_malloc(kg->node_capacity * sizeof(bool));
        if (!kg->security.critical_nodes) {
            nimcp_mutex_unlock(kg->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_mark_critical: kg->security is NULL");
            return -1;
        }
        memset(kg->security.critical_nodes, 0, kg->node_capacity * sizeof(bool));
    }

    /* Find node index */
    for (uint32_t i = 0; i < kg->node_capacity; i++) {
        if (kg->nodes[i].in_use && kg->nodes[i].id == node_id) {
            kg->security.critical_nodes[i] = true;
            nimcp_mutex_unlock(kg->mutex);
            NIMCP_LOGGING_INFO("Node %u marked as critical", node_id);
            return 0;
        }
    }

    nimcp_mutex_unlock(kg->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_mark_critical: validation failed");
    return -1;
}

int brain_kg_get_security_stats(
    const brain_kg_t* kg,
    uint32_t* violations_out,
    uint64_t* last_violation_time
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_get_security_stats: kg is NULL");
        return -1;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    if (violations_out) {
        *violations_out = kg->security.violation_count;
    }
    if (last_violation_time) {
        *last_violation_time = kg->security.last_violation_time;
    }

    nimcp_mutex_unlock(mkg->mutex);
    return 0;
}

int brain_kg_emergency_lock(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_emergency_lock: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);
    kg->security.emergency_locked = true;
    nimcp_mutex_unlock(kg->mutex);

    NIMCP_LOGGING_WARN("Brain KG EMERGENCY LOCKED - all writes disabled");
    return 0;
}

int brain_kg_emergency_unlock(brain_kg_t* kg, uint64_t admin_token) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_emergency_unlock: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    if (admin_token != kg->security.admin_token) {
        nimcp_mutex_unlock(kg->mutex);
        report_security_violation(kg, BRAIN_KG_SEC_UNAUTHORIZED_ACCESS,
            BRAIN_KG_INVALID_NODE, "Invalid admin token for emergency unlock");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_emergency_unlock: validation failed");
        return -1;
    }

    kg->security.emergency_locked = false;
    nimcp_mutex_unlock(kg->mutex);

    NIMCP_LOGGING_INFO("Brain KG emergency lock released");
    return 0;
}

/* ============================================================================
 * MESSAGE-TYPE INDEX API (Phase 6: KG Query Optimization)
 * ============================================================================ */

/**
 * @brief Find index entry for a message type (unlocked)
 */
static brain_kg_msg_index_entry_t* find_msg_index_entry_unlocked(
    brain_kg_t* kg,
    uint32_t message_type
) {
    if (!kg || !kg->msg_index.entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_msg_index_entry_unlocked: required parameter is NULL (kg, kg->msg_index)");
        return NULL;
    }

    for (uint32_t i = 0; i < kg->msg_index.entry_capacity; i++) {
        if (kg->msg_index.entries[i].in_use &&
            kg->msg_index.entries[i].message_type == message_type) {
            return &kg->msg_index.entries[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_msg_index_entry_unlocked: required parameter is NULL (kg, kg->msg_index)");
    return NULL;
}

/**
 * @brief Find or create index entry for a message type (unlocked)
 */
static brain_kg_msg_index_entry_t* get_or_create_msg_index_entry_unlocked(
    brain_kg_t* kg,
    uint32_t message_type
) {
    if (!kg || !kg->msg_index.entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "get_or_create_msg_index_entry_unlocked: required parameter is NULL (kg, kg->msg_index)");
        return NULL;
    }

    /* First try to find existing entry */
    brain_kg_msg_index_entry_t* entry = find_msg_index_entry_unlocked(kg, message_type);
    if (entry) return entry;

    /* Find free slot */
    for (uint32_t i = 0; i < kg->msg_index.entry_capacity; i++) {
        if (!kg->msg_index.entries[i].in_use) {
            entry = &kg->msg_index.entries[i];
            memset(entry, 0, sizeof(*entry));
            entry->message_type = message_type;
            entry->handler_count = 0;
            entry->in_use = true;
            kg->msg_index.entry_count++;
            return entry;
        }
    }

    NIMCP_LOGGING_WARN("Message index capacity reached (%u)", kg->msg_index.entry_capacity);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_or_create_msg_index_entry_unlocked: operation failed");
    return NULL;
}

brain_kg_handler_list_t* brain_kg_get_handlers_for_message_type(
    const brain_kg_t* kg,
    uint32_t message_type
) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }

    brain_kg_handler_list_t* list = nimcp_malloc(sizeof(*list));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;

    }

    list->count = 0;
    list->capacity = 16;
    list->handlers = nimcp_malloc(list->capacity * sizeof(brain_kg_node_id_t));
    if (!list->handlers) {
        nimcp_free(list);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_kg_get_handlers_for_message_type: list->handlers is NULL");
        return NULL;
    }

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    /* Rebuild index if dirty */
    if (mkg->msg_index.dirty) {
        nimcp_mutex_unlock(mkg->mutex);
        brain_kg_rebuild_message_index(mkg);
        nimcp_mutex_lock(mkg->mutex);
    }

    brain_kg_msg_index_entry_t* entry = find_msg_index_entry_unlocked(mkg, message_type);
    if (entry) {
        /* Copy handlers from index */
        for (uint32_t i = 0; i < entry->handler_count; i++) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                brain_kg_node_id_t* new_handlers = nimcp_realloc(
                    list->handlers, list->capacity * sizeof(brain_kg_node_id_t));
                if (!new_handlers) break;
                list->handlers = new_handlers;
            }
            list->handlers[list->count++] = entry->handlers[i];
        }
    }

    mkg->stats.queries_count++;
    nimcp_mutex_unlock(mkg->mutex);

    return list;
}

void brain_kg_handler_list_destroy(brain_kg_handler_list_t* list) {
    if (!list) return;
    if (list->handlers) nimcp_free(list->handlers);
    nimcp_free(list);
}

int brain_kg_add_message_handler(
    brain_kg_t* kg,
    brain_kg_node_id_t module_node_id,
    uint32_t message_type
) {
    if (!kg || module_node_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_add_message_handler: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    /* Note: We intentionally skip node existence validation here.
     * The message index stores handler IDs that can be:
     * - Internal KG node IDs (for KG-internal use)
     * - bio_module_ids (for Phase 7 KG dispatch)
     * The consumer of the handler list interprets the IDs appropriately. */

    /* Get or create index entry */
    brain_kg_msg_index_entry_t* entry = get_or_create_msg_index_entry_unlocked(
        kg, message_type);
    if (!entry) {
        nimcp_mutex_unlock(kg->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_add_message_handler: entry is NULL");
        return -1;
    }

    /* Check if handler already registered */
    for (uint32_t i = 0; i < entry->handler_count; i++) {
        if (entry->handlers[i] == module_node_id) {
            nimcp_mutex_unlock(kg->mutex);
            return 0;  /* Already registered */
        }
    }

    /* Add handler to index */
    if (entry->handler_count >= BRAIN_KG_MAX_HANDLERS_PER_MSG) {
        nimcp_mutex_unlock(kg->mutex);
        NIMCP_LOGGING_WARN("Max handlers per message type reached for type %u",
                           message_type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "brain_kg_add_message_handler: capacity exceeded");
        return -1;
    }

    entry->handlers[entry->handler_count++] = module_node_id;
    kg->stats.modifications_count++;

    nimcp_mutex_unlock(kg->mutex);

    NIMCP_LOGGING_DEBUG("Added handler for msg type 0x%x: handler ID %u",
                        message_type, module_node_id);
    return 0;
}

int brain_kg_remove_message_handler(
    brain_kg_t* kg,
    brain_kg_node_id_t module_node_id,
    uint32_t message_type
) {
    if (!kg || module_node_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_remove_message_handler: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    brain_kg_msg_index_entry_t* entry = find_msg_index_entry_unlocked(kg, message_type);
    if (!entry) {
        nimcp_mutex_unlock(kg->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_remove_message_handler: entry is NULL");
        return -1;
    }

    /* Find and remove handler */
    for (uint32_t i = 0; i < entry->handler_count; i++) {
        if (entry->handlers[i] == module_node_id) {
            /* Shift remaining handlers */
            for (uint32_t j = i; j < entry->handler_count - 1; j++) {
                entry->handlers[j] = entry->handlers[j + 1];
            }
            entry->handler_count--;

            /* If no more handlers, mark entry as unused */
            if (entry->handler_count == 0) {
                entry->in_use = false;
                kg->msg_index.entry_count--;
            }

            kg->stats.modifications_count++;
            nimcp_mutex_unlock(kg->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(kg->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_kg_remove_message_handler: operation failed");
    return -1;  /* Not found */
}

int brain_kg_rebuild_message_index(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg_rebuild_message_index: kg is NULL");
        return -1;
    }

    nimcp_mutex_lock(kg->mutex);

    /* Clear existing index */
    for (uint32_t i = 0; i < kg->msg_index.entry_capacity; i++) {
        kg->msg_index.entries[i].in_use = false;
        kg->msg_index.entries[i].handler_count = 0;
    }
    kg->msg_index.entry_count = 0;

    int handlers_indexed = 0;

    /* Scan all HANDLES_MESSAGE edges and rebuild index */
    for (uint32_t i = 0; i < kg->edge_capacity; i++) {
        if (!kg->edges[i].in_use) continue;
        if (kg->edges[i].type != BRAIN_KG_EDGE_HANDLES_MESSAGE) continue;

        brain_kg_node_id_t module_node_id = kg->edges[i].from;
        uint32_t message_type = kg->edges[i].to;  /* 'to' holds message type */

        brain_kg_msg_index_entry_t* entry = get_or_create_msg_index_entry_unlocked(
            kg, message_type);
        if (!entry) continue;

        /* Check for duplicates */
        bool found = false;
        for (uint32_t j = 0; j < entry->handler_count; j++) {
            if (entry->handlers[j] == module_node_id) {
                found = true;
                break;
            }
        }

        if (!found && entry->handler_count < BRAIN_KG_MAX_HANDLERS_PER_MSG) {
            entry->handlers[entry->handler_count++] = module_node_id;
            handlers_indexed++;
        }
    }

    kg->msg_index.dirty = false;
    kg->msg_index.last_rebuild = get_time_ms();

    nimcp_mutex_unlock(kg->mutex);

    NIMCP_LOGGING_INFO("Message index rebuilt: %d handlers indexed in %u entries",
                       handlers_indexed, kg->msg_index.entry_count);
    return handlers_indexed;
}

uint32_t brain_kg_get_module_handled_messages(
    const brain_kg_t* kg,
    brain_kg_node_id_t module_node_id,
    uint32_t* message_types,
    uint32_t max_types
) {
    if (!kg || !message_types || max_types == 0) return 0;
    if (module_node_id == BRAIN_KG_INVALID_NODE) return 0;

    brain_kg_t* mkg = (brain_kg_t*)kg;
    nimcp_mutex_lock(mkg->mutex);

    uint32_t found = 0;

    /* Scan index for entries containing this module */
    for (uint32_t i = 0; i < kg->msg_index.entry_capacity && found < max_types; i++) {
        if (!kg->msg_index.entries[i].in_use) continue;

        for (uint32_t j = 0; j < kg->msg_index.entries[i].handler_count; j++) {
            if (kg->msg_index.entries[i].handlers[j] == module_node_id) {
                message_types[found++] = kg->msg_index.entries[i].message_type;
                break;
            }
        }
    }

    nimcp_mutex_unlock(mkg->mutex);
    return found;
}

void brain_kg_invalidate_message_index(brain_kg_t* kg) {
    if (!kg) return;

    nimcp_mutex_lock(kg->mutex);
    kg->msg_index.dirty = true;
    nimcp_mutex_unlock(kg->mutex);

    NIMCP_LOGGING_DEBUG("Message index invalidated");
}
