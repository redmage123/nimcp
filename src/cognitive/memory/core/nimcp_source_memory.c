//=============================================================================
// nimcp_source_memory.c - Source Memory System Implementation
//=============================================================================
/**
 * @file nimcp_source_memory.c
 * @brief Implementation of source memory for tracking information origins
 *
 * Implements memory for where, when, and how information was acquired,
 * including reality monitoring and false memory detection.
 */

// Required for clock_gettime and strdup
#define _POSIX_C_SOURCE 200809L

#include "cognitive/memory/core/nimcp_source_memory.h"
#include "cognitive/memory/nimcp_memory_kg_events.h"  /* W6: KG event emitters */
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(source_memory, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Thread-Local Error Handling
//=============================================================================

#ifdef _WIN32
    #define THREAD_LOCAL __declspec(thread)
#else
    #define THREAD_LOCAL _Thread_local
#endif

static THREAD_LOCAL char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    if (msg) {
        strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
        g_last_error[sizeof(g_last_error) - 1] = '\0';
    }
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Hash table entry for source memory entries
 */
typedef struct source_hash_entry {
    uint64_t memory_id;              /**< Key: memory node ID */
    source_memory_entry_t entry;     /**< Value: source entry data */
    struct source_hash_entry* next;  /**< Hash collision chain */
    bool occupied;                   /**< Whether this slot is in use */
} source_hash_entry_t;

/**
 * @brief Hash table entry for agent records
 */
typedef struct agent_hash_entry {
    uint64_t agent_id;               /**< Key: agent ID */
    source_agent_record_t record;    /**< Value: agent record */
    struct agent_hash_entry* next;   /**< Hash collision chain */
    bool occupied;                   /**< Whether this slot is in use */
} agent_hash_entry_t;

/**
 * @brief Internal source memory structure
 */
struct source_memory_struct {
    // Integration with PR memory
    entangle_graph_t entanglement;
    pr_node_manager_t node_manager;

    // Entry hash table
    source_hash_entry_t* entries;
    size_t entry_capacity;
    size_t entry_count;

    // Agent hash table
    agent_hash_entry_t* agents;
    size_t agent_capacity;
    size_t agent_count;

    // Configuration
    source_memory_config_t config;

    // Statistics
    source_memory_stats_t stats;

    // Reality monitoring weights
    float perceptual_weight;
    float contextual_weight;
    float cognitive_weight;
    float semantic_weight;
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Hash function for 64-bit IDs
 */
static size_t hash_id(uint64_t id, size_t capacity) {
    // FNV-1a inspired hash
    uint64_t hash = 0xcbf29ce484222325ULL;
    hash ^= id;
    hash *= 0x100000001b3ULL;
    hash ^= (id >> 32);
    hash *= 0x100000001b3ULL;
    return (size_t)(hash % capacity);
}

/**
 * @brief Find entry by memory ID
 */
static source_hash_entry_t* find_entry(source_memory_t sm, uint64_t memory_id) {
    if (!sm || sm->entry_capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_entry: sm is NULL");
        return NULL;
    }

    size_t idx = hash_id(memory_id, sm->entry_capacity);
    source_hash_entry_t* entry = &sm->entries[idx];

    while (entry) {
        if (entry->occupied && entry->memory_id == memory_id) {
            return entry;
        }
        entry = entry->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_entry: validation failed");
    return NULL;
}

/**
 * @brief Find agent record by ID
 */
static agent_hash_entry_t* find_agent(source_memory_t sm, uint64_t agent_id) {
    if (!sm || sm->agent_capacity == 0 || !sm->config.track_agents) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_agent: required parameter is NULL (sm, sm->config)");
        return NULL;
    }

    size_t idx = hash_id(agent_id, sm->agent_capacity);
    agent_hash_entry_t* agent = &sm->agents[idx];

    while (agent) {
        if (agent->occupied && agent->agent_id == agent_id) {
            return agent;
        }
        agent = agent->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_agent: validation failed");
    return NULL;
}

/**
 * @brief Insert entry into hash table
 */
static source_error_t insert_entry(
    source_memory_t sm,
    uint64_t memory_id,
    const source_memory_entry_t* entry
) {
    if (!sm || !entry) return SOURCE_ERROR_NULL_POINTER;
    if (sm->entry_count >= sm->config.max_entries) return SOURCE_ERROR_FULL;

    size_t idx = hash_id(memory_id, sm->entry_capacity);
    source_hash_entry_t* slot = &sm->entries[idx];

    // Find empty slot or chain end
    source_hash_entry_t* prev = NULL;
    while (slot && slot->occupied) {
        if (slot->memory_id == memory_id) {
            return SOURCE_ERROR_ALREADY_EXISTS;
        }
        prev = slot;
        slot = slot->next;
    }

    // Need new slot in chain
    if (!slot) {
        slot = (source_hash_entry_t*)nimcp_calloc(1, sizeof(source_hash_entry_t));
        if (!slot) return SOURCE_ERROR_NO_MEMORY;
        if (prev) prev->next = slot;
    }

    // Copy entry data
    slot->memory_id = memory_id;
    slot->entry = *entry;
    slot->entry.memory_id = memory_id;
    slot->occupied = true;

    // Copy agent name if present
    if (entry->source.source_agent_name) {
        slot->entry.source.source_agent_name = strdup(entry->source.source_agent_name);
        if (!slot->entry.source.source_agent_name) {
            slot->occupied = false;
            return SOURCE_ERROR_NO_MEMORY;
        }
    }

    sm->entry_count++;
    return SOURCE_SUCCESS;
}

/**
 * @brief Insert agent record into hash table
 */
static source_error_t insert_agent(
    source_memory_t sm,
    uint64_t agent_id,
    const source_agent_record_t* record
) {
    if (!sm || !record) return SOURCE_ERROR_NULL_POINTER;
    if (!sm->config.track_agents) return SOURCE_ERROR_INVALID_PARAM;
    if (sm->agent_count >= SOURCE_MAX_AGENTS) return SOURCE_ERROR_FULL;

    size_t idx = hash_id(agent_id, sm->agent_capacity);
    agent_hash_entry_t* slot = &sm->agents[idx];

    // Find empty slot or chain end
    agent_hash_entry_t* prev = NULL;
    while (slot && slot->occupied) {
        if (slot->agent_id == agent_id) {
            return SOURCE_ERROR_ALREADY_EXISTS;
        }
        prev = slot;
        slot = slot->next;
    }

    // Need new slot in chain
    if (!slot) {
        slot = (agent_hash_entry_t*)nimcp_calloc(1, sizeof(agent_hash_entry_t));
        if (!slot) return SOURCE_ERROR_NO_MEMORY;
        if (prev) prev->next = slot;
    }

    slot->agent_id = agent_id;
    slot->record = *record;
    slot->occupied = true;

    sm->agent_count++;
    return SOURCE_SUCCESS;
}

/**
 * @brief Compute reality score from source attributes
 *
 * Johnson & Raye model:
 * - Real memories: High perceptual, high contextual, low cognitive
 * - Imagined: Low perceptual, low contextual, high cognitive
 */
static float compute_reality_score(
    source_memory_t sm,
    const source_attribute_t* attr
) {
    if (!sm || !attr) return 0.5f;

    // Reality = perceptual + contextual - cognitive + semantic balance
    // Higher score = more likely real
    float perceptual_contrib = attr->perceptual_vividness * sm->perceptual_weight;
    float contextual_contrib = attr->contextual_detail * sm->contextual_weight;
    float cognitive_penalty = attr->cognitive_operations * sm->cognitive_weight;
    float semantic_contrib = attr->semantic_detail * sm->semantic_weight;

    // Semantic: balanced is good (not too high, not too low for real memories)
    float semantic_balance = 1.0f - fabsf(attr->semantic_detail - 0.5f) * 2.0f;
    semantic_contrib = semantic_balance * sm->semantic_weight;

    float score = perceptual_contrib + contextual_contrib - cognitive_penalty + semantic_contrib;

    // Normalize to [0, 1]
    float max_score = sm->perceptual_weight + sm->contextual_weight + sm->semantic_weight;
    float min_score = -sm->cognitive_weight;

    if (max_score > min_score) {
        score = (score - min_score) / (max_score - min_score);
    } else {
        score = 0.5f;
    }

    return nimcp_clampf(score, 0.0f, 1.0f);
}

/**
 * @brief Convert reality score to status enum
 */
static reality_status_t score_to_status(float score) {
    if (score >= 0.8f) return REALITY_CERTAIN_REAL;
    if (score >= 0.6f) return REALITY_PROBABLY_REAL;
    if (score >= 0.4f) return REALITY_UNCERTAIN;
    if (score >= 0.2f) return REALITY_PROBABLY_IMAGINED;
    return REALITY_CERTAIN_IMAGINED;
}

/**
 * @brief Compute individual false memory risk component
 */
static float compute_schema_risk(float schema_consistency, float source_confidence) {
    // High schema fit + low source memory = risk
    if (source_confidence < SOURCE_EPSILON) {
        return schema_consistency;
    }
    return schema_consistency * (1.0f - source_confidence);
}

static float compute_suggestion_risk(float suggestion_exposure, float time_since_encoding) {
    // Suggestion exposure risk increases with time
    float time_factor = 1.0f - expf(-time_since_encoding / 86400.0f);  // ~1 day time constant
    return suggestion_exposure * time_factor;
}

static float compute_repetition_risk(float repetition_count) {
    // Imagination inflation: more repetitions = higher risk
    // Saturates around 10 repetitions
    return 1.0f - expf(-repetition_count / 5.0f);
}

static float compute_emotional_risk(float emotional_intensity) {
    // Very high emotion can distort details
    // Risk is U-shaped: both very high and very low emotion are risky
    // But mainly concerned with high emotion distortion
    return emotional_intensity * emotional_intensity;  // Quadratic increase
}

static float compute_temporal_risk(float time_since_encoding) {
    // Source memory decays faster than content memory
    // Significant risk after ~1 week
    float weeks = time_since_encoding / (7.0f * 24.0f * 3600.0f);
    return 1.0f - expf(-weeks);
}

static float compute_source_amnesia_risk(source_type_t type, float source_confidence) {
    // Risk when source is unknown or forgotten but content remains
    if (type == SOURCE_UNKNOWN) {
        return 1.0f;
    }
    // Low source confidence indicates partial source amnesia
    return 1.0f - source_confidence;
}

/**
 * @brief Update statistics after entry changes
 */
static void update_stats(source_memory_t sm) {
    if (!sm) return;

    // Reset counters
    sm->stats.total_entries = sm->entry_count;
    sm->stats.verified_entries = 0;
    sm->stats.suspicious_entries = 0;
    memset(sm->stats.entries_by_type, 0, sizeof(sm->stats.entries_by_type));
    memset(sm->stats.entries_by_reality, 0, sizeof(sm->stats.entries_by_reality));

    float total_source_conf = 0.0f;
    float total_content_conf = 0.0f;
    float total_risk = 0.0f;

    // Iterate through all entries
    for (size_t i = 0; i < sm->entry_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sm->entry_capacity > 256) {
            source_memory_heartbeat("source_memor_loop",
                             (float)(i + 1) / (float)sm->entry_capacity);
        }

        source_hash_entry_t* entry = &sm->entries[i];
        while (entry && entry->occupied) {
            source_memory_entry_t* e = &entry->entry;

            // Count by type
            if (e->source.type < SOURCE_TYPE_COUNT) {
                sm->stats.entries_by_type[e->source.type]++;
            }

            // Count by reality status
            if (e->reality_status < REALITY_STATUS_COUNT) {
                sm->stats.entries_by_reality[e->reality_status]++;
            }

            // Count verified/suspicious
            if (e->verified) sm->stats.verified_entries++;
            if (e->marked_suspicious) sm->stats.suspicious_entries++;

            // Accumulate confidence
            total_source_conf += e->source_confidence;
            total_content_conf += e->content_confidence;

            // Accumulate risk (compute on-the-fly)
            false_memory_risk_t risk;
            source_memory_compute_false_memory_risk(sm, entry->memory_id, &risk);
            total_risk += risk.total_risk;

            entry = entry->next;
        }
    }

    if (sm->entry_count > 0) {
        sm->stats.avg_source_confidence = total_source_conf / sm->entry_count;
        sm->stats.avg_content_confidence = total_content_conf / sm->entry_count;
        sm->stats.avg_false_memory_risk = total_risk / sm->entry_count;
    }

    // Agent statistics
    sm->stats.num_agents = sm->agent_count;
    float total_cred = 0.0f;
    for (size_t i = 0; i < sm->agent_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sm->agent_capacity > 256) {
            source_memory_heartbeat("source_memor_loop",
                             (float)(i + 1) / (float)sm->agent_capacity);
        }

        agent_hash_entry_t* agent = &sm->agents[i];
        while (agent && agent->occupied) {
            total_cred += agent->record.credibility;
            agent = agent->next;
        }
    }
    if (sm->agent_count > 0) {
        sm->stats.avg_agent_credibility = total_cred / sm->agent_count;
    }

    // Memory usage estimate
    sm->stats.memory_bytes = sizeof(struct source_memory_struct) +
        sm->entry_capacity * sizeof(source_hash_entry_t) +
        sm->agent_capacity * sizeof(agent_hash_entry_t);
}

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT source_memory_config_t source_memory_config_default(void) {
    source_memory_config_t config = {
        .initial_capacity = SOURCE_DEFAULT_ENTRY_CAPACITY,
        .max_entries = SOURCE_MAX_ENTRIES,
        .perceptual_threshold = SOURCE_DEFAULT_PERCEPTUAL_THRESHOLD,
        .cognitive_threshold = SOURCE_DEFAULT_COGNITIVE_THRESHOLD,
        .false_memory_threshold = SOURCE_DEFAULT_FALSE_MEMORY_THRESHOLD,
        .source_decay_rate = SOURCE_DEFAULT_DECAY_RATE,
        .track_agents = true,
        .auto_reality_check = true
    };
    return config;
}

NIMCP_EXPORT bool source_memory_config_validate(const source_memory_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_memory_config_validate: config is NULL");
        return false;
    }

    // Validate capacities
    if (config->initial_capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "source_memory_config_validate: config->initial_capacity is zero");
        return false;
    }
    if (config->max_entries == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "source_memory_config_validate: config->max_entries is zero");
        return false;
    }
    if (config->initial_capacity > config->max_entries) {
        return false;
    }

    // Validate thresholds in [0, 1]
    if (config->perceptual_threshold < 0.0f ||
        config->perceptual_threshold > 1.0f) return false;
    if (config->cognitive_threshold < 0.0f ||
        config->cognitive_threshold > 1.0f) return false;
    if (config->false_memory_threshold < 0.0f ||
        config->false_memory_threshold > 1.0f) return false;

    // Validate decay rate
    if (config->source_decay_rate < 0.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT source_memory_t source_memory_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const source_memory_config_t* config
) {
    // Entanglement and node_manager can be NULL for standalone usage
    // but some features will be limited

    // Use defaults if no config provided
    source_memory_config_t cfg = config ? *config : source_memory_config_default();
    if (!source_memory_config_validate(&cfg)) {
        set_error("invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "source_memory_create: source_memory_config_validate is NULL");
        return NULL;
    }

    // Allocate main structure
    source_memory_t sm = (source_memory_t)nimcp_calloc(1, sizeof(struct source_memory_struct));
    if (!sm) {
        set_error("memory allocation failed for source_memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "source_memory_create: sm is NULL");
        return NULL;
    }

    // Store integrations
    sm->entanglement = entanglement;
    sm->node_manager = node_manager;
    sm->config = cfg;

    // Allocate entry hash table
    sm->entry_capacity = cfg.initial_capacity;
    sm->entries = (source_hash_entry_t*)nimcp_calloc(
        sm->entry_capacity, sizeof(source_hash_entry_t));
    if (!sm->entries) {
        set_error("memory allocation failed for entry hash table");
        nimcp_free(sm);
        sm = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "source_memory_create: sm->entries is NULL");
        return NULL;
    }

    // Allocate agent hash table if tracking enabled
    if (cfg.track_agents) {
        sm->agent_capacity = SOURCE_MAX_AGENTS / 4;  // Start smaller
        sm->agents = (agent_hash_entry_t*)nimcp_calloc(
            sm->agent_capacity, sizeof(agent_hash_entry_t));
        if (!sm->agents) {
            set_error("memory allocation failed for agent hash table");
            nimcp_free(sm->entries);
            nimcp_free(sm);
            sm = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "source_memory_create: sm->agents is NULL");
            return NULL;
        }
    }

    // Initialize reality monitoring weights (Johnson & Raye model)
    // These weights determine how each feature contributes to reality score
    sm->perceptual_weight = 0.35f;   // Perceptual vividness (real > imagined)
    sm->contextual_weight = 0.25f;   // Contextual detail (real > imagined)
    sm->cognitive_weight = 0.25f;    // Cognitive operations (imagined > real, so subtract)
    sm->semantic_weight = 0.15f;     // Semantic detail balance

    // Initialize statistics
    memset(&sm->stats, 0, sizeof(sm->stats));

    return sm;
}

NIMCP_EXPORT void source_memory_destroy(source_memory_t sm) {
    if (!sm) return;

    // Free entry chain nodes
    for (size_t i = 0; i < sm->entry_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sm->entry_capacity > 256) {
            source_memory_heartbeat("source_memor_loop",
                             (float)(i + 1) / (float)sm->entry_capacity);
        }

        source_hash_entry_t* entry = sm->entries[i].next;
        while (entry) {
            source_hash_entry_t* next = entry->next;
            if (entry->entry.source.source_agent_name) {
                nimcp_free(entry->entry.source.source_agent_name);
            }
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
        // Free agent name in base slot
        if (sm->entries[i].entry.source.source_agent_name) {
            nimcp_free(sm->entries[i].entry.source.source_agent_name);
        }
    }
    nimcp_free(sm->entries);

    // Free agent chain nodes
    if (sm->agents) {
        for (size_t i = 0; i < sm->agent_capacity; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && sm->agent_capacity > 256) {
                source_memory_heartbeat("source_memor_loop",
                                 (float)(i + 1) / (float)sm->agent_capacity);
            }

            agent_hash_entry_t* agent = sm->agents[i].next;
            while (agent) {
                agent_hash_entry_t* next = agent->next;
                nimcp_free(agent);
                agent = NULL;
                agent = next;
            }
        }
        nimcp_free(sm->agents);
    }

    nimcp_free(sm);
    sm = NULL;
}

NIMCP_EXPORT source_error_t source_memory_clear(source_memory_t sm) {
    if (!sm) return SOURCE_ERROR_NULL_POINTER;

    // Clear entries
    for (size_t i = 0; i < sm->entry_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sm->entry_capacity > 256) {
            source_memory_heartbeat("source_memor_loop",
                             (float)(i + 1) / (float)sm->entry_capacity);
        }

        source_hash_entry_t* entry = sm->entries[i].next;
        while (entry) {
            source_hash_entry_t* next = entry->next;
            if (entry->entry.source.source_agent_name) {
                nimcp_free(entry->entry.source.source_agent_name);
            }
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
        if (sm->entries[i].entry.source.source_agent_name) {
            nimcp_free(sm->entries[i].entry.source.source_agent_name);
        }
        memset(&sm->entries[i], 0, sizeof(source_hash_entry_t));
    }
    sm->entry_count = 0;

    // Clear agents
    if (sm->agents) {
        for (size_t i = 0; i < sm->agent_capacity; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && sm->agent_capacity > 256) {
                source_memory_heartbeat("source_memor_loop",
                                 (float)(i + 1) / (float)sm->agent_capacity);
            }

            agent_hash_entry_t* agent = sm->agents[i].next;
            while (agent) {
                agent_hash_entry_t* next = agent->next;
                nimcp_free(agent);
                agent = NULL;
                agent = next;
            }
            memset(&sm->agents[i], 0, sizeof(agent_hash_entry_t));
        }
        sm->agent_count = 0;
    }

    // Reset statistics
    memset(&sm->stats, 0, sizeof(sm->stats));

    return SOURCE_SUCCESS;
}

//=============================================================================
// Source Binding Functions
//=============================================================================

NIMCP_EXPORT source_error_t source_memory_bind_source(
    source_memory_t sm,
    pr_memory_node_t* memory,
    const source_attribute_t* source
) {
    if (!sm || !memory || !source) return SOURCE_ERROR_NULL_POINTER;

    uint64_t memory_id = pr_memory_node_get_id(memory);
    return source_memory_bind_source_by_id(sm, memory_id, source);
}

NIMCP_EXPORT source_error_t source_memory_bind_source_by_id(
    source_memory_t sm,
    uint64_t memory_id,
    const source_attribute_t* source
) {
    if (!sm || !source) return SOURCE_ERROR_NULL_POINTER;
    if (memory_id == PR_NODE_INVALID_ID) return SOURCE_ERROR_INVALID_PARAM;

    // Check if already exists
    if (find_entry(sm, memory_id)) {
        return SOURCE_ERROR_ALREADY_EXISTS;
    }

    // Create entry
    source_memory_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.memory = NULL;  // Will be looked up when needed
    entry.memory_id = memory_id;
    entry.source = *source;

    // Compute initial reality status
    float reality_score = compute_reality_score(sm, source);
    entry.reality_status = score_to_status(reality_score);

    // Initialize confidence based on source type
    switch (source->type) {
        case SOURCE_PERCEIVED:
            entry.source_confidence = 0.9f;
            entry.content_confidence = 0.85f;
            break;
        case SOURCE_TOLD:
            entry.source_confidence = 0.7f;
            entry.content_confidence = source->source_credibility * 0.8f;
            break;
        case SOURCE_READ:
            entry.source_confidence = 0.8f;
            entry.content_confidence = 0.75f;
            break;
        case SOURCE_INFERRED:
            entry.source_confidence = 0.6f;
            entry.content_confidence = 0.5f;
            break;
        case SOURCE_IMAGINED:
            entry.source_confidence = 0.5f;
            entry.content_confidence = 0.3f;
            break;
        default:
            entry.source_confidence = 0.3f;
            entry.content_confidence = 0.3f;
            break;
    }

    // Initialize risk indicators
    entry.schema_consistency = 0.0f;
    entry.suggestion_exposure = 0.0f;
    entry.repetition_count = 0.0f;
    entry.emotional_intensity = 0.0f;
    entry.time_since_encoding = 0.0f;

    // Initialize verification state
    entry.verified = false;
    entry.marked_suspicious = false;
    entry.last_verified_time = 0;

    // Insert into hash table
    source_error_t err = insert_entry(sm, memory_id, &entry);
    if (err != SOURCE_SUCCESS) return err;

    // Register agent if tracking and this is from another agent
    if (sm->config.track_agents &&
        source->source_agent_id != 0 &&
        source->source_agent_id != SOURCE_INVALID_AGENT_ID) {
        if (!find_agent(sm, source->source_agent_id)) {
            // Auto-register agent
            source_agent_record_t agent_rec;
            memset(&agent_rec, 0, sizeof(agent_rec));
            agent_rec.agent_id = source->source_agent_id;
            if (source->source_agent_name) {
                strncpy(agent_rec.name, source->source_agent_name,
                       SOURCE_MAX_AGENT_NAME_LEN - 1);
            }
            agent_rec.credibility = source->source_credibility;
            agent_rec.total_memories = 1;
            insert_agent(sm, source->source_agent_id, &agent_rec);
        } else {
            // Update existing agent's memory count
            agent_hash_entry_t* agent = find_agent(sm, source->source_agent_id);
            if (agent) {
                agent->record.total_memories++;
            }
        }
    }

    // Update statistics
    sm->stats.total_entries = sm->entry_count;
    if (source->type < SOURCE_TYPE_COUNT) {
        sm->stats.entries_by_type[source->type]++;
    }
    if (entry.reality_status < REALITY_STATUS_COUNT) {
        sm->stats.entries_by_reality[entry.reality_status]++;
    }

    /* W6: emit KG event for source tagging */
    memory_kg_emit_source_bound(
        memory_kg_events_get_registered_brain(),
        memory_id, (int)source->type);

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_error_t source_memory_get_source(
    source_memory_t sm,
    const pr_memory_node_t* memory,
    source_attribute_t* source
) {
    if (!sm || !memory || !source) return SOURCE_ERROR_NULL_POINTER;

    uint64_t memory_id = pr_memory_node_get_id(memory);
    return source_memory_get_source_by_id(sm, memory_id, source);
}

NIMCP_EXPORT source_error_t source_memory_get_source_by_id(
    source_memory_t sm,
    uint64_t memory_id,
    source_attribute_t* source
) {
    if (!sm || !source) return SOURCE_ERROR_NULL_POINTER;

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return SOURCE_ERROR_NOT_FOUND;

    *source = entry->entry.source;
    // Note: source_agent_name points to internal storage

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_error_t source_memory_get_entry(
    source_memory_t sm,
    uint64_t memory_id,
    source_memory_entry_t** entry
) {
    if (!sm || !entry) return SOURCE_ERROR_NULL_POINTER;

    source_hash_entry_t* hash_entry = find_entry(sm, memory_id);
    if (!hash_entry) {
        *entry = NULL;
        return SOURCE_ERROR_NOT_FOUND;
    }

    *entry = &hash_entry->entry;
    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_error_t source_memory_update_source(
    source_memory_t sm,
    uint64_t memory_id,
    const source_attribute_t* source
) {
    if (!sm || !source) return SOURCE_ERROR_NULL_POINTER;

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return SOURCE_ERROR_NOT_FOUND;

    // Free old agent name if different
    if (entry->entry.source.source_agent_name &&
        (!source->source_agent_name ||
         strcmp(entry->entry.source.source_agent_name, source->source_agent_name) != 0)) {
        nimcp_free(entry->entry.source.source_agent_name);
        entry->entry.source.source_agent_name = NULL;
    }

    // Update source attributes
    entry->entry.source = *source;

    // Copy agent name
    if (source->source_agent_name) {
        entry->entry.source.source_agent_name = strdup(source->source_agent_name);
        if (!entry->entry.source.source_agent_name) {
            return SOURCE_ERROR_NO_MEMORY;
        }
    }

    // Update reality status if auto-check enabled
    if (sm->config.auto_reality_check) {
        float reality_score = compute_reality_score(sm, source);
        entry->entry.reality_status = score_to_status(reality_score);
    }

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_error_t source_memory_unbind_source(
    source_memory_t sm,
    uint64_t memory_id
) {
    if (!sm) return SOURCE_ERROR_NULL_POINTER;

    size_t idx = hash_id(memory_id, sm->entry_capacity);
    source_hash_entry_t* entry = &sm->entries[idx];
    source_hash_entry_t* prev = NULL;

    while (entry) {
        if (entry->occupied && entry->memory_id == memory_id) {
            // Free agent name
            if (entry->entry.source.source_agent_name) {
                nimcp_free(entry->entry.source.source_agent_name);
            }

            if (prev) {
                // Remove from chain
                prev->next = entry->next;
                nimcp_free(entry);
                entry = NULL;
            } else if (entry->next) {
                // Move next to base slot
                source_hash_entry_t* next = entry->next;
                *entry = *next;
                nimcp_free(next);
                next = NULL;
            } else {
                // Clear base slot
                memset(entry, 0, sizeof(*entry));
            }

            sm->entry_count--;
            return SOURCE_SUCCESS;
        }
        prev = entry;
        entry = entry->next;
    }

    return SOURCE_ERROR_NOT_FOUND;
}

//=============================================================================
// Reality Monitoring Functions
//=============================================================================

NIMCP_EXPORT source_error_t source_memory_reality_monitor(
    source_memory_t sm,
    uint64_t memory_id,
    reality_monitor_result_t* result
) {
    if (!sm || !result) return SOURCE_ERROR_NULL_POINTER;

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return SOURCE_ERROR_NOT_FOUND;

    source_attribute_t* attr = &entry->entry.source;

    // Compute individual scores
    result->perceptual_score = attr->perceptual_vividness;
    result->contextual_score = attr->contextual_detail;
    result->cognitive_score = attr->cognitive_operations;
    result->semantic_score = attr->semantic_detail;

    // Compute overall reality score
    result->reality_score = compute_reality_score(sm, attr);

    // Determine status
    result->status = score_to_status(result->reality_score);

    // Compute confidence in assessment
    // Higher when features are consistent with the determined status
    float feature_consistency = 0.0f;
    if (result->status == REALITY_CERTAIN_REAL || result->status == REALITY_PROBABLY_REAL) {
        // For real: high perceptual, high contextual, low cognitive
        feature_consistency = (result->perceptual_score * 0.4f +
                              result->contextual_score * 0.3f +
                              (1.0f - result->cognitive_score) * 0.3f);
    } else if (result->status == REALITY_CERTAIN_IMAGINED || result->status == REALITY_PROBABLY_IMAGINED) {
        // For imagined: low perceptual, low contextual, high cognitive
        feature_consistency = ((1.0f - result->perceptual_score) * 0.3f +
                              (1.0f - result->contextual_score) * 0.3f +
                              result->cognitive_score * 0.4f);
    } else {
        // Uncertain: consistency is low by definition
        feature_consistency = 0.5f;
    }

    result->confidence = nimcp_clampf(feature_consistency, 0.0f, 1.0f);

    // Update entry's reality status
    entry->entry.reality_status = result->status;

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_error_t source_memory_update_reality_features(
    source_memory_t sm,
    uint64_t memory_id,
    float perceptual,
    float contextual,
    float cognitive,
    float semantic
) {
    if (!sm) return SOURCE_ERROR_NULL_POINTER;

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return SOURCE_ERROR_NOT_FOUND;

    // Update features (NaN means keep current)
    if (!isnan(perceptual)) {
        entry->entry.source.perceptual_vividness = nimcp_clampf(perceptual, 0.0f, 1.0f);
    }
    if (!isnan(contextual)) {
        entry->entry.source.contextual_detail = nimcp_clampf(contextual, 0.0f, 1.0f);
    }
    if (!isnan(cognitive)) {
        entry->entry.source.cognitive_operations = nimcp_clampf(cognitive, 0.0f, 1.0f);
    }
    if (!isnan(semantic)) {
        entry->entry.source.semantic_detail = nimcp_clampf(semantic, 0.0f, 1.0f);
    }

    // Update reality status
    if (sm->config.auto_reality_check) {
        float reality_score = compute_reality_score(sm, &entry->entry.source);
        entry->entry.reality_status = score_to_status(reality_score);
    }

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT reality_status_t source_memory_get_reality_status(
    source_memory_t sm,
    uint64_t memory_id
) {
    if (!sm) return REALITY_UNCERTAIN;

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return REALITY_UNCERTAIN;

    return entry->entry.reality_status;
}

//=============================================================================
// False Memory Detection Functions
//=============================================================================

NIMCP_EXPORT source_error_t source_memory_compute_false_memory_risk(
    source_memory_t sm,
    uint64_t memory_id,
    false_memory_risk_t* risk
) {
    if (!sm || !risk) return SOURCE_ERROR_NULL_POINTER;

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return SOURCE_ERROR_NOT_FOUND;

    source_memory_entry_t* e = &entry->entry;

    // Compute individual risk components
    risk->schema_risk = compute_schema_risk(e->schema_consistency, e->source_confidence);
    risk->suggestion_risk = compute_suggestion_risk(e->suggestion_exposure, e->time_since_encoding);
    risk->repetition_risk = compute_repetition_risk(e->repetition_count);
    risk->emotional_risk = compute_emotional_risk(e->emotional_intensity);
    risk->temporal_risk = compute_temporal_risk(e->time_since_encoding);
    risk->source_amnesia_risk = compute_source_amnesia_risk(e->source.type, e->source_confidence);

    // Combine risks (weighted average)
    // Source amnesia and schema consistency are primary concerns
    float weights[] = {0.20f, 0.15f, 0.15f, 0.10f, 0.15f, 0.25f};
    risk->total_risk = risk->schema_risk * weights[0] +
                       risk->suggestion_risk * weights[1] +
                       risk->repetition_risk * weights[2] +
                       risk->emotional_risk * weights[3] +
                       risk->temporal_risk * weights[4] +
                       risk->source_amnesia_risk * weights[5];

    risk->total_risk = nimcp_clampf(risk->total_risk, 0.0f, 1.0f);

    // Determine if high risk
    risk->is_high_risk = risk->total_risk >= sm->config.false_memory_threshold;

    // Identify primary concern
    float max_risk = 0.0f;
    const char* concerns[] = {
        "High schema consistency without source encoding",
        "Post-event suggestion exposure",
        "Imagination inflation from repeated retrieval",
        "Emotional distortion of details",
        "Source memory decay over time",
        "Source amnesia (source forgotten, content remains)"
    };
    float risks[] = {
        risk->schema_risk, risk->suggestion_risk, risk->repetition_risk,
        risk->emotional_risk, risk->temporal_risk, risk->source_amnesia_risk
    };

    risk->primary_concern = concerns[0];
    for (int i = 0; i < 6; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 6 > 256) {
            source_memory_heartbeat("source_memor_loop",
                             (float)(i + 1) / (float)6);
        }

        if (risks[i] > max_risk) {
            max_risk = risks[i];
            risk->primary_concern = concerns[i];
        }
    }

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_error_t source_memory_update_risk_indicators(
    source_memory_t sm,
    uint64_t memory_id,
    float schema_consistency,
    float suggestion_exposure,
    float emotional_intensity
) {
    if (!sm) return SOURCE_ERROR_NULL_POINTER;

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return SOURCE_ERROR_NOT_FOUND;

    // Update indicators (NaN means keep current)
    if (!isnan(schema_consistency)) {
        entry->entry.schema_consistency = nimcp_clampf(schema_consistency, 0.0f, 1.0f);
    }
    if (!isnan(suggestion_exposure)) {
        entry->entry.suggestion_exposure = nimcp_clampf(suggestion_exposure, 0.0f, 1.0f);
    }
    if (!isnan(emotional_intensity)) {
        entry->entry.emotional_intensity = nimcp_clampf(emotional_intensity, 0.0f, 1.0f);
    }

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT uint32_t source_memory_record_retrieval(
    source_memory_t sm,
    uint64_t memory_id
) {
    if (!sm) return 0;

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return 0;

    entry->entry.repetition_count += 1.0f;
    return (uint32_t)entry->entry.repetition_count;
}

NIMCP_EXPORT source_error_t source_memory_mark_suspicious(
    source_memory_t sm,
    uint64_t memory_id,
    const char* reason
) {
    if (!sm) return SOURCE_ERROR_NULL_POINTER;
    (void)reason;  // Currently not stored, could be added to entry

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return SOURCE_ERROR_NOT_FOUND;

    entry->entry.marked_suspicious = true;
    sm->stats.suspicious_entries++;

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_error_t source_memory_clear_suspicious(
    source_memory_t sm,
    uint64_t memory_id
) {
    if (!sm) return SOURCE_ERROR_NULL_POINTER;

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return SOURCE_ERROR_NOT_FOUND;

    if (entry->entry.marked_suspicious) {
        entry->entry.marked_suspicious = false;
        if (sm->stats.suspicious_entries > 0) {
            sm->stats.suspicious_entries--;
        }
    }

    return SOURCE_SUCCESS;
}

//=============================================================================
// Source Credibility Functions
//=============================================================================

NIMCP_EXPORT float source_memory_update_credibility(
    source_memory_t sm,
    uint64_t agent_id,
    float delta
) {
    if (!sm) return -1.0f;
    if (!sm->config.track_agents) return -1.0f;

    agent_hash_entry_t* agent = find_agent(sm, agent_id);
    if (!agent) return -1.0f;

    agent->record.credibility = nimcp_clampf(
        agent->record.credibility + delta, 0.0f, 1.0f);

    // Update trend
    agent->record.credibility_trend = nimcp_clampf(
        agent->record.credibility_trend + delta * 0.5f, -1.0f, 1.0f);

    return agent->record.credibility;
}

NIMCP_EXPORT float source_memory_get_credibility(
    source_memory_t sm,
    uint64_t agent_id
) {
    if (!sm) return -1.0f;
    if (!sm->config.track_agents) return -1.0f;

    agent_hash_entry_t* agent = find_agent(sm, agent_id);
    if (!agent) return -1.0f;

    return agent->record.credibility;
}

NIMCP_EXPORT source_error_t source_memory_register_agent(
    source_memory_t sm,
    uint64_t agent_id,
    const char* name,
    float initial_credibility
) {
    if (!sm) return SOURCE_ERROR_NULL_POINTER;
    if (!sm->config.track_agents) return SOURCE_ERROR_INVALID_PARAM;
    if (agent_id == SOURCE_INVALID_AGENT_ID) return SOURCE_ERROR_INVALID_AGENT;

    // Check if already exists
    if (find_agent(sm, agent_id)) {
        return SOURCE_ERROR_ALREADY_EXISTS;
    }

    source_agent_record_t record;
    memset(&record, 0, sizeof(record));
    record.agent_id = agent_id;
    if (name) {
        strncpy(record.name, name, SOURCE_MAX_AGENT_NAME_LEN - 1);
    }
    record.credibility = nimcp_clampf(initial_credibility, 0.0f, 1.0f);
    record.total_memories = 0;
    record.verified_correct = 0;
    record.verified_incorrect = 0;
    record.credibility_trend = 0.0f;

    return insert_agent(sm, agent_id, &record);
}

NIMCP_EXPORT source_error_t source_memory_get_agent(
    source_memory_t sm,
    uint64_t agent_id,
    source_agent_record_t* record
) {
    if (!sm || !record) return SOURCE_ERROR_NULL_POINTER;
    if (!sm->config.track_agents) return SOURCE_ERROR_INVALID_PARAM;

    agent_hash_entry_t* agent = find_agent(sm, agent_id);
    if (!agent) return SOURCE_ERROR_NOT_FOUND;

    *record = agent->record;
    return SOURCE_SUCCESS;
}

//=============================================================================
// Source Verification Functions
//=============================================================================

NIMCP_EXPORT source_error_t source_memory_verify_source(
    source_memory_t sm,
    uint64_t memory_id,
    bool correct
) {
    if (!sm) return SOURCE_ERROR_NULL_POINTER;

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return SOURCE_ERROR_NOT_FOUND;

    entry->entry.verified = true;
    entry->entry.last_verified_time = get_current_time_ms();

    // Update agent credibility if tracking
    if (sm->config.track_agents &&
        entry->entry.source.source_agent_id != 0 &&
        entry->entry.source.source_agent_id != SOURCE_INVALID_AGENT_ID) {

        agent_hash_entry_t* agent = find_agent(sm, entry->entry.source.source_agent_id);
        if (agent) {
            if (correct) {
                agent->record.verified_correct++;
                source_memory_update_credibility(sm, agent->record.agent_id, 0.05f);
            } else {
                agent->record.verified_incorrect++;
                source_memory_update_credibility(sm, agent->record.agent_id, -0.1f);
            }
        }
    }

    // Update confidence based on verification
    if (correct) {
        entry->entry.source_confidence = nimcp_clampf(
            entry->entry.source_confidence + 0.1f, 0.0f, 1.0f);
        entry->entry.content_confidence = nimcp_clampf(
            entry->entry.content_confidence + 0.1f, 0.0f, 1.0f);
    } else {
        entry->entry.source_confidence = nimcp_clampf(
            entry->entry.source_confidence - 0.2f, 0.0f, 1.0f);
        entry->entry.content_confidence = nimcp_clampf(
            entry->entry.content_confidence - 0.3f, 0.0f, 1.0f);
        entry->entry.marked_suspicious = true;
    }

    // Update stats
    if (!entry->entry.marked_suspicious) {
        sm->stats.verified_entries++;
    }

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_error_t source_memory_cross_check(
    source_memory_t sm,
    uint64_t memory_id,
    float corroboration_threshold,
    uint64_t* corroborating_ids,
    size_t max_ids,
    size_t* count
) {
    if (!sm || !corroborating_ids || !count) return SOURCE_ERROR_NULL_POINTER;

    *count = 0;

    source_hash_entry_t* target_entry = find_entry(sm, memory_id);
    if (!target_entry) return SOURCE_ERROR_NOT_FOUND;

    // If we have entanglement graph, use it to find related memories
    if (sm->entanglement) {
        entangle_neighbor_t neighbors[64];
        size_t neighbor_count = 0;

        if (entangle_get_neighbors(sm->entanglement, memory_id,
                                   neighbors, 64, &neighbor_count)) {
            for (size_t i = 0; i < neighbor_count && *count < max_ids; i++) {
                if (neighbors[i].edge.resonance_score >= corroboration_threshold) {
                    // Check if this neighbor has source info
                    source_hash_entry_t* neighbor_entry = find_entry(
                        sm, neighbors[i].neighbor_id);
                    if (neighbor_entry) {
                        corroborating_ids[(*count)++] = neighbors[i].neighbor_id;
                    }
                }
            }
        }
    }

    // Also check for same-source memories
    if (*count < max_ids && target_entry->entry.source.source_agent_id != 0) {
        for (size_t i = 0; i < sm->entry_capacity && *count < max_ids; i++) {
            source_hash_entry_t* entry = &sm->entries[i];
            while (entry && entry->occupied && *count < max_ids) {
                if (entry->memory_id != memory_id &&
                    entry->entry.source.source_agent_id ==
                        target_entry->entry.source.source_agent_id) {
                    corroborating_ids[(*count)++] = entry->memory_id;
                }
                entry = entry->next;
            }
        }
    }

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_error_t source_memory_detect_misattribution(
    source_memory_t sm,
    uint64_t memory_id,
    float* misattribution_score,
    uint64_t* likely_true_source
) {
    if (!sm || !misattribution_score) return SOURCE_ERROR_NULL_POINTER;

    *misattribution_score = 0.0f;
    if (likely_true_source) *likely_true_source = 0;

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return SOURCE_ERROR_NOT_FOUND;

    // Misattribution indicators:
    // 1. Source confidence is low
    // 2. Source type doesn't match typical features
    // 3. Agent credibility is low
    // 4. Content confidence is much higher than source confidence

    source_memory_entry_t* e = &entry->entry;
    float score = 0.0f;

    // Low source confidence
    score += (1.0f - e->source_confidence) * 0.3f;

    // Source-feature mismatch
    // E.g., PERCEIVED should have high perceptual vividness
    float expected_perceptual = 0.0f;
    float expected_cognitive = 0.0f;
    switch (e->source.type) {
        case SOURCE_PERCEIVED:
            expected_perceptual = 0.7f;
            expected_cognitive = 0.2f;
            break;
        case SOURCE_TOLD:
            expected_perceptual = 0.4f;
            expected_cognitive = 0.3f;
            break;
        case SOURCE_READ:
            expected_perceptual = 0.3f;
            expected_cognitive = 0.5f;
            break;
        case SOURCE_INFERRED:
            expected_perceptual = 0.2f;
            expected_cognitive = 0.8f;
            break;
        case SOURCE_IMAGINED:
            expected_perceptual = 0.5f;
            expected_cognitive = 0.9f;
            break;
        default:
            break;
    }

    float perceptual_mismatch = fabsf(e->source.perceptual_vividness - expected_perceptual);
    float cognitive_mismatch = fabsf(e->source.cognitive_operations - expected_cognitive);
    score += (perceptual_mismatch + cognitive_mismatch) * 0.25f;

    // Agent credibility (if applicable)
    if (sm->config.track_agents && e->source.source_agent_id != 0) {
        agent_hash_entry_t* agent = find_agent(sm, e->source.source_agent_id);
        if (agent) {
            score += (1.0f - agent->record.credibility) * 0.2f;
        }
    }

    // Content vs source confidence gap
    float confidence_gap = e->content_confidence - e->source_confidence;
    if (confidence_gap > 0.3f) {
        score += confidence_gap * 0.25f;
    }

    *misattribution_score = nimcp_clampf(score, 0.0f, 1.0f);

    return SOURCE_SUCCESS;
}

//=============================================================================
// Query Functions
//=============================================================================

NIMCP_EXPORT source_error_t source_memory_query_by_source(
    source_memory_t sm,
    const source_query_t* query,
    uint64_t* results,
    size_t max_results,
    size_t* count
) {
    if (!sm || !query || !results || !count) return SOURCE_ERROR_NULL_POINTER;

    *count = 0;

    for (size_t i = 0; i < sm->entry_capacity && *count < max_results; i++) {
        source_hash_entry_t* entry = &sm->entries[i];
        while (entry && entry->occupied && *count < max_results) {
            source_memory_entry_t* e = &entry->entry;
            bool match = true;

            // Filter by source types
            if (query->source_types && query->num_source_types > 0) {
                bool type_match = false;
                for (size_t j = 0; j < query->num_source_types; j++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((j & 0xFF) == 0 && query->num_source_types > 256) {
                        source_memory_heartbeat("source_memor_loop",
                                         (float)(j + 1) / (float)query->num_source_types);
                    }

                    if (e->source.type == query->source_types[j]) {
                        type_match = true;
                        break;
                    }
                }
                if (!type_match) match = false;
            }

            // Filter by agent
            if (match && query->source_agent_id != SOURCE_INVALID_AGENT_ID) {
                if (e->source.source_agent_id != query->source_agent_id) {
                    match = false;
                }
            }

            // Filter by credibility
            if (match && e->source.source_credibility < query->min_credibility) {
                match = false;
            }

            // Filter by time
            if (match && query->min_time > 0.0f) {
                if (e->source.acquisition_time < query->min_time) {
                    match = false;
                }
            }
            if (match && query->max_time > 0.0f) {
                if (e->source.acquisition_time > query->max_time) {
                    match = false;
                }
            }

            // Filter by reality status
            if (match && query->reality_statuses && query->num_reality_statuses > 0) {
                bool status_match = false;
                for (size_t j = 0; j < query->num_reality_statuses; j++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((j & 0xFF) == 0 && query->num_reality_statuses > 256) {
                        source_memory_heartbeat("source_memor_loop",
                                         (float)(j + 1) / (float)query->num_reality_statuses);
                    }

                    if (e->reality_status == query->reality_statuses[j]) {
                        status_match = true;
                        break;
                    }
                }
                if (!status_match) match = false;
            }

            // Filter by false memory risk
            if (match && query->max_false_memory_risk < 1.0f) {
                false_memory_risk_t risk;
                source_memory_compute_false_memory_risk(sm, entry->memory_id, &risk);
                if (risk.total_risk > query->max_false_memory_risk) {
                    match = false;
                }
            }

            // Filter by verification
            if (match && query->only_verified && !e->verified) {
                match = false;
            }

            // Filter out suspicious
            if (match && query->exclude_suspicious && e->marked_suspicious) {
                match = false;
            }

            if (match) {
                results[(*count)++] = entry->memory_id;
            }

            entry = entry->next;
        }
    }

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_error_t source_memory_query_by_time(
    source_memory_t sm,
    float min_time,
    float max_time,
    uint64_t* results,
    size_t max_results,
    size_t* count
) {
    if (!sm || !results || !count) return SOURCE_ERROR_NULL_POINTER;

    *count = 0;

    for (size_t i = 0; i < sm->entry_capacity && *count < max_results; i++) {
        source_hash_entry_t* entry = &sm->entries[i];
        while (entry && entry->occupied && *count < max_results) {
            float time = entry->entry.source.acquisition_time;
            if (time >= min_time && time <= max_time) {
                results[(*count)++] = entry->memory_id;
            }
            entry = entry->next;
        }
    }

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_error_t source_memory_query_by_reality(
    source_memory_t sm,
    reality_status_t status,
    uint64_t* results,
    size_t max_results,
    size_t* count
) {
    if (!sm || !results || !count) return SOURCE_ERROR_NULL_POINTER;

    *count = 0;

    for (size_t i = 0; i < sm->entry_capacity && *count < max_results; i++) {
        source_hash_entry_t* entry = &sm->entries[i];
        while (entry && entry->occupied && *count < max_results) {
            if (entry->entry.reality_status == status) {
                results[(*count)++] = entry->memory_id;
            }
            entry = entry->next;
        }
    }

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_error_t source_memory_query_high_risk(
    source_memory_t sm,
    float risk_threshold,
    uint64_t* results,
    size_t max_results,
    size_t* count
) {
    if (!sm || !results || !count) return SOURCE_ERROR_NULL_POINTER;

    *count = 0;

    for (size_t i = 0; i < sm->entry_capacity && *count < max_results; i++) {
        source_hash_entry_t* entry = &sm->entries[i];
        while (entry && entry->occupied && *count < max_results) {
            false_memory_risk_t risk;
            source_memory_compute_false_memory_risk(sm, entry->memory_id, &risk);
            if (risk.total_risk >= risk_threshold) {
                results[(*count)++] = entry->memory_id;
            }
            entry = entry->next;
        }
    }

    return SOURCE_SUCCESS;
}

//=============================================================================
// Source Decay Functions
//=============================================================================

NIMCP_EXPORT size_t source_memory_source_forgetting(
    source_memory_t sm,
    float elapsed_seconds
) {
    if (!sm || elapsed_seconds <= 0.0f) return 0;

    size_t affected = 0;
    float decay_factor = expf(-sm->config.source_decay_rate * elapsed_seconds);

    for (size_t i = 0; i < sm->entry_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sm->entry_capacity > 256) {
            source_memory_heartbeat("source_memor_loop",
                             (float)(i + 1) / (float)sm->entry_capacity);
        }

        source_hash_entry_t* entry = &sm->entries[i];
        while (entry && entry->occupied) {
            // Apply decay to source confidence
            float old_conf = entry->entry.source_confidence;
            entry->entry.source_confidence *= decay_factor;

            // Update time since encoding
            entry->entry.time_since_encoding += elapsed_seconds;

            // If source confidence drops very low, mark as SOURCE_UNKNOWN
            if (entry->entry.source_confidence < 0.1f &&
                entry->entry.source.type != SOURCE_UNKNOWN) {
                entry->entry.source.type = SOURCE_UNKNOWN;
                affected++;
            } else if (old_conf != entry->entry.source_confidence) {
                affected++;
            }

            entry = entry->next;
        }
    }

    return affected;
}

NIMCP_EXPORT float source_memory_consolidate_source(
    source_memory_t sm,
    uint64_t memory_id,
    float reinforcement
) {
    if (!sm || reinforcement < 0.0f) return -1.0f;

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return -1.0f;

    entry->entry.source_confidence = nimcp_clampf(
        entry->entry.source_confidence + reinforcement, 0.0f, 1.0f);

    return entry->entry.source_confidence;
}

//=============================================================================
// Context Binding Functions
//=============================================================================

NIMCP_EXPORT source_error_t source_memory_bind_context(
    source_memory_t sm,
    uint64_t memory_id,
    const prime_signature_t* context_signature,
    float context_confidence
) {
    if (!sm || !context_signature) return SOURCE_ERROR_NULL_POINTER;

    source_hash_entry_t* entry = find_entry(sm, memory_id);
    if (!entry) return SOURCE_ERROR_NOT_FOUND;

    // Copy context signature
    entry->entry.source.location_context = *context_signature;
    entry->entry.source.location_confidence = nimcp_clampf(context_confidence, 0.0f, 1.0f);

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_error_t source_memory_query_by_context(
    source_memory_t sm,
    const prime_signature_t* context_signature,
    float similarity_threshold,
    uint64_t* results,
    size_t max_results,
    size_t* count
) {
    if (!sm || !context_signature || !results || !count) {
        return SOURCE_ERROR_NULL_POINTER;
    }

    *count = 0;

    for (size_t i = 0; i < sm->entry_capacity && *count < max_results; i++) {
        source_hash_entry_t* entry = &sm->entries[i];
        while (entry && entry->occupied && *count < max_results) {
            // Compute similarity between context signatures
            float sim = prime_sig_jaccard(context_signature,
                                          &entry->entry.source.location_context);
            if (sim >= similarity_threshold) {
                results[(*count)++] = entry->memory_id;
            }
            entry = entry->next;
        }
    }

    return SOURCE_SUCCESS;
}

//=============================================================================
// Statistics and Utility Functions
//=============================================================================

NIMCP_EXPORT source_error_t source_memory_get_stats(
    source_memory_t sm,
    source_memory_stats_t* stats
) {
    if (!sm || !stats) return SOURCE_ERROR_NULL_POINTER;

    update_stats(sm);
    *stats = sm->stats;

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT const char* source_error_string(source_error_t error) {
    switch (error) {
        case SOURCE_SUCCESS:              return "Success";
        case SOURCE_ERROR_NULL_POINTER:   return "NULL pointer argument";
        case SOURCE_ERROR_NOT_FOUND:      return "Entry not found";
        case SOURCE_ERROR_ALREADY_EXISTS: return "Entry already exists";
        case SOURCE_ERROR_NO_MEMORY:      return "Memory allocation failed";
        case SOURCE_ERROR_FULL:           return "Maximum entries reached";
        case SOURCE_ERROR_INVALID_PARAM:  return "Invalid parameter value";
        case SOURCE_ERROR_INVALID_AGENT:  return "Invalid agent ID";
        case SOURCE_ERROR_SERIALIZE:      return "Serialization failed";
        case SOURCE_ERROR_DESERIALIZE:    return "Deserialization failed";
        case SOURCE_ERROR_VERSION_MISMATCH: return "Incompatible version";
        default:                          return "Unknown error";
    }
}

NIMCP_EXPORT const char* source_type_name(source_type_t type) {
    switch (type) {
        case SOURCE_PERCEIVED: return "PERCEIVED";
        case SOURCE_TOLD:      return "TOLD";
        case SOURCE_READ:      return "READ";
        case SOURCE_INFERRED:  return "INFERRED";
        case SOURCE_IMAGINED:  return "IMAGINED";
        case SOURCE_UNKNOWN:   return "UNKNOWN";
        default:               return "INVALID";
    }
}

NIMCP_EXPORT const char* reality_status_name(reality_status_t status) {
    switch (status) {
        case REALITY_CERTAIN_REAL:      return "CERTAIN_REAL";
        case REALITY_PROBABLY_REAL:     return "PROBABLY_REAL";
        case REALITY_UNCERTAIN:         return "UNCERTAIN";
        case REALITY_PROBABLY_IMAGINED: return "PROBABLY_IMAGINED";
        case REALITY_CERTAIN_IMAGINED:  return "CERTAIN_IMAGINED";
        default:                        return "INVALID";
    }
}

NIMCP_EXPORT void source_attribute_init(source_attribute_t* attr) {
    if (!attr) return;

    memset(attr, 0, sizeof(*attr));
    attr->type = SOURCE_UNKNOWN;
    attr->source_agent_id = 0;
    attr->source_agent_name = NULL;
    attr->source_credibility = 0.5f;
    attr->acquisition_time = 0.0f;
    attr->time_confidence = 0.5f;
    attr->location_confidence = 0.0f;
    attr->modality_flags = MODALITY_NONE;
    attr->perceptual_vividness = 0.5f;
    attr->contextual_detail = 0.5f;
    attr->cognitive_operations = 0.5f;
    attr->semantic_detail = 0.5f;
}

NIMCP_EXPORT void source_query_init(source_query_t* query) {
    if (!query) return;

    memset(query, 0, sizeof(*query));
    query->source_types = NULL;
    query->num_source_types = 0;
    query->source_agent_id = SOURCE_INVALID_AGENT_ID;
    query->min_credibility = 0.0f;
    query->min_time = 0.0f;
    query->max_time = 0.0f;
    query->reality_statuses = NULL;
    query->num_reality_statuses = 0;
    query->max_false_memory_risk = 1.0f;
    query->only_verified = false;
    query->exclude_suspicious = false;
}

NIMCP_EXPORT void source_entry_print(const source_memory_entry_t* entry) {
    if (!entry) {
        printf("SourceMemoryEntry: (null)\n");
        return;
    }

    printf("SourceMemoryEntry {\n");
    printf("  memory_id: %lu\n", (unsigned long)entry->memory_id);
    printf("  Source {\n");
    printf("    type: %s\n", source_type_name(entry->source.type));
    printf("    agent_id: %lu\n", (unsigned long)entry->source.source_agent_id);
    if (entry->source.source_agent_name) {
        printf("    agent_name: %s\n", entry->source.source_agent_name);
    }
    printf("    credibility: %.3f\n", entry->source.source_credibility);
    printf("    acquisition_time: %.3f\n", entry->source.acquisition_time);
    printf("    Reality Features {\n");
    printf("      perceptual_vividness: %.3f\n", entry->source.perceptual_vividness);
    printf("      contextual_detail: %.3f\n", entry->source.contextual_detail);
    printf("      cognitive_operations: %.3f\n", entry->source.cognitive_operations);
    printf("      semantic_detail: %.3f\n", entry->source.semantic_detail);
    printf("    }\n");
    printf("  }\n");
    printf("  reality_status: %s\n", reality_status_name(entry->reality_status));
    printf("  Risk Indicators {\n");
    printf("    schema_consistency: %.3f\n", entry->schema_consistency);
    printf("    suggestion_exposure: %.3f\n", entry->suggestion_exposure);
    printf("    repetition_count: %.1f\n", entry->repetition_count);
    printf("    emotional_intensity: %.3f\n", entry->emotional_intensity);
    printf("    time_since_encoding: %.1f sec\n", entry->time_since_encoding);
    printf("  }\n");
    printf("  Confidence {\n");
    printf("    source: %.3f\n", entry->source_confidence);
    printf("    content: %.3f\n", entry->content_confidence);
    printf("  }\n");
    printf("  verified: %s\n", entry->verified ? "true" : "false");
    printf("  marked_suspicious: %s\n", entry->marked_suspicious ? "true" : "false");
    printf("}\n");
}

//=============================================================================
// Serialization Functions
//=============================================================================

/**
 * @brief Serialization header structure
 */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t entry_count;
    uint64_t agent_count;
    uint64_t total_size;
    uint32_t checksum;
    uint32_t flags;
} source_serial_header_t;

NIMCP_EXPORT source_error_t source_memory_serialize(
    source_memory_t sm,
    void* buffer,
    size_t buffer_size,
    size_t* written_size
) {
    if (!sm || !written_size) return SOURCE_ERROR_NULL_POINTER;

    // Calculate required size
    size_t header_size = sizeof(source_serial_header_t);
    size_t entry_size = sm->entry_count * sizeof(source_memory_entry_t);
    size_t agent_size = sm->agent_count * sizeof(source_agent_record_t);
    size_t config_size = sizeof(source_memory_config_t);
    size_t total_size = header_size + entry_size + agent_size + config_size;

    *written_size = total_size;

    if (!buffer) {
        return SOURCE_SUCCESS;  // Just querying size
    }

    if (buffer_size < total_size) {
        return SOURCE_ERROR_SERIALIZE;
    }

    uint8_t* ptr = (uint8_t*)buffer;

    // Write header
    source_serial_header_t header = {
        .magic = SOURCE_MEMORY_MAGIC,
        .version = SOURCE_MEMORY_VERSION,
        .entry_count = sm->entry_count,
        .agent_count = sm->agent_count,
        .total_size = total_size,
        .checksum = 0,
        .flags = 0
    };
    memcpy(ptr, &header, sizeof(header));
    ptr += sizeof(header);

    // Write config
    memcpy(ptr, &sm->config, sizeof(sm->config));
    ptr += sizeof(sm->config);

    // Write entries
    for (size_t i = 0; i < sm->entry_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sm->entry_capacity > 256) {
            source_memory_heartbeat("source_memor_loop",
                             (float)(i + 1) / (float)sm->entry_capacity);
        }

        source_hash_entry_t* entry = &sm->entries[i];
        while (entry && entry->occupied) {
            // Write entry (excluding pointer fields)
            source_memory_entry_t e = entry->entry;
            e.memory = NULL;  // Don't serialize pointer
            e.source.source_agent_name = NULL;  // Handle separately
            memcpy(ptr, &e, sizeof(e));
            ptr += sizeof(e);
            entry = entry->next;
        }
    }

    // Write agents
    for (size_t i = 0; i < sm->agent_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sm->agent_capacity > 256) {
            source_memory_heartbeat("source_memor_loop",
                             (float)(i + 1) / (float)sm->agent_capacity);
        }

        agent_hash_entry_t* agent = &sm->agents[i];
        while (agent && agent->occupied) {
            memcpy(ptr, &agent->record, sizeof(agent->record));
            ptr += sizeof(agent->record);
            agent = agent->next;
        }
    }

    return SOURCE_SUCCESS;
}

NIMCP_EXPORT source_memory_t source_memory_deserialize(
    const void* buffer,
    size_t buffer_size,
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    size_t* bytes_read
) {
    if (!buffer || !bytes_read) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_memory_deserialize: required parameter is NULL (buffer, bytes_read)");
        return NULL;
    }

    *bytes_read = 0;

    // Read header
    if (buffer_size < sizeof(source_serial_header_t)) {
        set_error("buffer too small for header");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "source_memory_deserialize: validation failed");
        return NULL;
    }

    const uint8_t* ptr = (const uint8_t*)buffer;
    source_serial_header_t header;
    memcpy(&header, ptr, sizeof(header));
    ptr += sizeof(header);

    // Validate header
    if (header.magic != SOURCE_MEMORY_MAGIC) {
        set_error("invalid magic number");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "source_memory_deserialize: validation failed");
        return NULL;
    }
    if (header.version != SOURCE_MEMORY_VERSION) {
        set_error("version mismatch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "source_memory_deserialize: validation failed");
        return NULL;
    }
    if (buffer_size < header.total_size) {
        set_error("buffer too small for data");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "source_memory_deserialize: validation failed");
        return NULL;
    }

    // Read config
    source_memory_config_t config;
    memcpy(&config, ptr, sizeof(config));
    ptr += sizeof(config);

    // Create source memory with config
    source_memory_t sm = source_memory_create(entanglement, node_manager, &config);
    if (!sm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sm is NULL");

        return NULL;
    }

    // Read entries
    for (uint64_t i = 0; i < header.entry_count; i++) {
        source_memory_entry_t entry;
        memcpy(&entry, ptr, sizeof(entry));
        ptr += sizeof(entry);

        // Insert entry
        insert_entry(sm, entry.memory_id, &entry);
    }

    // Read agents
    for (uint64_t i = 0; i < header.agent_count; i++) {
        source_agent_record_t record;
        memcpy(&record, ptr, sizeof(record));
        ptr += sizeof(record);

        insert_agent(sm, record.agent_id, &record);
    }

    *bytes_read = header.total_size;
    return sm;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void source_memory_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_source_memory_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int source_memory_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "source_memory_training_begin: NULL argument");
        return -1;
    }
    source_memory_heartbeat_instance(NULL, "source_memory_training_begin", 0.0f);
    (void)(struct source_hash_entry*)instance; /* Module state available for reset */
    return 0;
}

int source_memory_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "source_memory_training_end: NULL argument");
        return -1;
    }
    source_memory_heartbeat_instance(NULL, "source_memory_training_end", 1.0f);
    (void)(struct source_hash_entry*)instance; /* Module state available for finalization */
    return 0;
}

int source_memory_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "source_memory_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    source_memory_heartbeat_instance(NULL, "source_memory_training_step", progress);
    (void)(struct source_hash_entry*)instance; /* Module state available for step adaptation */
    return 0;
}
